/********************************************************************
*
* Filename: ofc_cntrl.c
*
* Description: This file contains the OpenFlow control path sub
*              module. It interacts with SDN controller and
*              exchanges OpenFlow control packets with the controller.
*              It also interfaces with OpenFlow control path task
*              to interact with Flow Classification application.
*
*******************************************************************/

#include "ofc_hdrs.h"

tOfcCpGlobals gOfcCpGlobals;
extern unsigned int gCntrlIpAddr;

/******************************************************************                                                                          
* Function: OfcCpMainInit
*
* Description: This function performs the initialization tasks of
*              the control path sub module
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpMainInit (void)
{
    memset (&gOfcCpGlobals, 0, sizeof (gOfcCpGlobals));

    /* Initialize semaphore */
    sema_init (&gOfcCpGlobals.semId, 1);
    sema_init (&gOfcCpGlobals.dpMsgQSemId, 1);

    /* Initialize queues */
    INIT_LIST_HEAD (&gOfcCpGlobals.dpMsgListHead);

    /* Create TCP socket to interact with controller */ 
    if (OfcCpCreateCntrlSocket() != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Control socket creation failed!!\r\n");
        return OFC_FAILURE;
    }

    gOfcCpGlobals.isModInit = OFC_TRUE;

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpMainTask
*
* Description: Main function for OpenFlow control path task
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpMainTask (void *args)
{
    int event = 0;

    /* Initialize memory and structures */
    if (OfcCpMainInit() != OFC_SUCCESS)
    {
        /* Check if thread needs to be killed */
        printk (KERN_CRIT "OpenFlow control path task intialization"
                          " failed!!\r\n");
        return OFC_FAILURE;
    }

    while (1)
    {
        if (OfcCpReceiveEvent (OFC_CTRL_PKT_EVENT | OFC_DP_TO_CP_EVENT,
            &event) == OFC_SUCCESS)
        {
            if (event & OFC_CTRL_PKT_EVENT)
            {
                /* Receive packet from controller */
                OfcCpRxControlPacket();
            }

            if (event & OFC_DP_TO_CP_EVENT)
            {
                /* Process information sent by data path task */
                OfcCpRxDataPathMsg();
            }
        }
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpRxControlPacket
*
* Description: This function receives OpenFlow control packets
*              from SDN controller and processes these packets
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpRxControlPacket (void)
{
    tOfcOfHdr  *pOfHdr = NULL;
    __u8       *pCntrlPkt = NULL;
    __u8       *pPkt = NULL;
    __u16      pktLen = 0;
    __u16      cntrlPktLen = 0;
    __u16      bytesProcessed = 0;
    int        retVal = OFC_SUCCESS;

    printk (KERN_INFO "Before socket receive\r\n"); 

    if (OfcCpRecvCntrlPktOnSock (&pPkt, &pktLen) 
        != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Failed to read control packet from"
                          " socket\r\n");
        return OFC_FAILURE;
    }

    printk (KERN_INFO "Control packet received (pktLen:%d)\r\n", pktLen);

    /* On each read call, socket returns all the control messages
     * present in the socket queue. Therefore, the packets need
     * need to be separated and processed sequentially */
    pCntrlPkt = pPkt;
    while (bytesProcessed < pktLen)
    {
        pOfHdr = (tOfcOfHdr *) ((void *) pCntrlPkt);
        cntrlPktLen = ntohs (pOfHdr->length);
        printk (KERN_INFO "Processing a packet (bytesProcessed:%d, cntrlPktLen:%d)\r\n", bytesProcessed, cntrlPktLen);

        /* Validate OpenFlow version */
        if (pOfHdr->version != OFC_VERSION)
        {
            printk (KERN_CRIT "OpenFlow version mismatch!!\r\n");
            /* TODO: Send error message and handle for higher versions */
            bytesProcessed += cntrlPktLen;
            pCntrlPkt += cntrlPktLen;
            retVal = OFC_FAILURE;
            continue;
        }

        /* Validate packet type */
        if (pOfHdr->type >= OFPT_MAX_PKT_TYPE)
        {
            printk (KERN_CRIT "Invalid OpenFlow packet type!!\r\n");
            /* TODO: Send error message */
            bytesProcessed += cntrlPktLen;
            pCntrlPkt += cntrlPktLen;
            retVal = OFC_FAILURE;
            continue;
        }

        switch (pOfHdr->type)
        {
            case OFPT_HELLO:
                OfcCpSendHelloPacket (pOfHdr->xid);
                break;

            case OFPT_ECHO_REQUEST:
                break;

            case OFPT_ECHO_REPLY:
                break;

            case OFPT_FEATURES_REQUEST:
                OfcCpSendFeatureReply (pCntrlPkt);
                break;

            case OFPT_GET_CONFIG_REQUEST:
                break;

            case OFPT_SET_CONFIG:
                break;

            case OFPT_PACKET_OUT:
                OfcCpProcessPktOut (pCntrlPkt, cntrlPktLen);
                break;

            case OFPT_FLOW_MOD:
                OfcCpProcessFlowMod (pCntrlPkt, cntrlPktLen);
                break;

            case OFPT_PORT_MOD:
                break;

            case OFPT_TABLE_MOD:
                break;

            case OFPT_MULTIPART_REQUEST:
                break;

            case OFPT_BARRIER_REQUEST:
                break;

            default:
                printk (KERN_CRIT "Packet not currently supported\r\n");
                break; 
        }

        bytesProcessed += cntrlPktLen;
        pCntrlPkt += cntrlPktLen;
    }

    /* Release processed control packet */
    kfree (pPkt);
    pPkt = NULL;

    return retVal;
}

/******************************************************************                                                                          
* Function: OfcCpRxDataPathMsg
*
* Description: This function dequeues messages sent by data path
*              task and forwards them to the controller.
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
void OfcCpRxDataPathMsg (void)
{
    tDpCpMsgQ *pMsgQ = NULL;
    __u8      *pOpenFlowPkt = NULL;
    __u16     pktLen = 0;

    down_interruptible (&gOfcCpGlobals.dpMsgQSemId);

    while ((pMsgQ = OfcCpRecvFromDpMsgQ()) != NULL)
    {
        /* Send packet as packet-in to controller */
        OfcCpConstructPacketIn (pMsgQ->pPkt, pMsgQ->pktLen, 
                                pMsgQ->inPort, pMsgQ->msgType,
                                pMsgQ->tableId, pMsgQ->cookie,
                                pMsgQ->pFlowEntry->matchFields,
                                &pOpenFlowPkt);
        if (pOpenFlowPkt == NULL)
        {
            printk (KERN_INFO "Failed to construct Packet-in "
                              "message\r\n");
            kfree (pMsgQ);
            pMsgQ = NULL;
            continue;
        }

        pktLen = ntohs (((tOfcOfHdr *) pOpenFlowPkt)->length);
        OfcCpSendCntrlPktFromSock (pOpenFlowPkt, pktLen);

        printk (KERN_INFO "OpenFlow Packet:\r\n");
        OfcDumpPacket (pOpenFlowPkt, pktLen);

        /* Release message */
        kfree (pOpenFlowPkt);
        pOpenFlowPkt = NULL;
        kfree (pMsgQ);
        pMsgQ = NULL;
    }

    up (&gOfcCpGlobals.dpMsgQSemId);

    return;
}

/******************************************************************                                                                          
* Function: OfcCpAddOpenFlowHdr
*
* Description: This function adds OpenFlow standard header to
*              an OpenFlow packet type
*
* Input: 
*
* Output: ppOpenFlowPkt - Pointer to OpenFlow packet
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpAddOpenFlowHdr (__u8 *pPktHdr, __u16 pktHdrLen,
                         __u8 msgType, __u32 xid, 
                         __u8 **ppOfPkt)
{
    tOfcOfHdr *pOfHdr = NULL;
    __u16     ofHdrLen = 0;
    
    ofHdrLen = OFC_OPENFLOW_HDR_LEN + pktHdrLen;
    pOfHdr = (tOfcOfHdr *) kmalloc (ofHdrLen, GFP_KERNEL);
    if (pOfHdr == NULL)
    {
        printk (KERN_CRIT "Failed to allocate memory to OpenFlow "
                          "header\r\n");
        return OFC_FAILURE;
    }
    
    memset (pOfHdr, 0, ofHdrLen);
    pOfHdr->version = OFC_VERSION;
    pOfHdr->type = msgType;
    pOfHdr->length = htons (ofHdrLen);
    /* xid same as that of controller, hence already in 
     * network byte order */
    pOfHdr->xid = xid; 

    if (pPktHdr != NULL)
    {
        memcpy (((__u8 *) pOfHdr) + OFC_OPENFLOW_HDR_LEN, 
                pPktHdr, pktHdrLen);
    }

    *ppOfPkt = (__u8 *) pOfHdr;
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpSendHelloPacket
*
* Description: This function is invoked to create the HELLO packet.
*              It would invoke the OfcCpSendCntrlPktFromSock 
*              function to send it out to controller
*
* Input: xid - Transaction ID.
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpSendHelloPacket (__u32 xid)
{
    __u8  *pHelloPkt = NULL;

    if (OfcCpAddOpenFlowHdr (NULL, 0, OFPT_HELLO, xid, &pHelloPkt) 
        != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Failed to construct Hello packet\r\n");
        return OFC_FAILURE;
    }

    if (OfcCpSendCntrlPktFromSock (pHelloPkt, OFC_OPENFLOW_HDR_LEN)
        != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Failed to send Hello packet\r\n");
        kfree(pHelloPkt);
        pHelloPkt = NULL;
        return OFC_FAILURE;
    }

    kfree(pHelloPkt);
    pHelloPkt = NULL;
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpSendFeatureReply
*
* Description: This function is invoked to reply to FEATURE_REQUEST
*              message from the controller.
*
* Input: pCntrlPkt - The packet received from the controller.
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpSendFeatureReply (__u8 *pCntrlPkt)
{
    tOfcFeatReply     *pResponseMsg = NULL;
    struct net_device *dev = NULL;
    __u8              *pOpenFlowPkt = NULL;
    __u16             featReplyLen = 0;

    pResponseMsg = (tOfcFeatReply *) kmalloc (sizeof(tOfcFeatReply),
                                              GFP_KERNEL);
    if (!pResponseMsg)
    {
        printk (KERN_CRIT "Failed to allocate memory to Feature "
                          "Response packet\r\n");
        return OFC_FAILURE;
    }

    dev = OfcGetNetDevByIp (gCntrlIpAddr);
    if (!dev)
    {
        printk (KERN_CRIT "Failed to retrieve interface for "
                          "controller IP\r\n");
        kfree (pResponseMsg);
        pResponseMsg = NULL;
        return OFC_FAILURE;
    }

    memset (pResponseMsg, 0, sizeof (tOfcFeatReply));
    memcpy (pResponseMsg->macDatapathId, dev->dev_addr, 
            OFC_MAC_ADDR_LEN);
    pResponseMsg->maxBuffers = htonl (OFC_MAX_PKT_BUFFER);
    pResponseMsg->maxTables  = OFC_MAX_FLOW_TABLES;
    pResponseMsg->auxilaryId = OFC_CTRL_MAIN_CONNECTION;
    pResponseMsg->capabilities = 
        htonl (OFPC_FLOW_STATS | OFPC_TABLE_STATS);

    featReplyLen = sizeof (pResponseMsg->impDatapathId) +
                   sizeof (pResponseMsg->macDatapathId) +
                   sizeof (pResponseMsg->maxBuffers) +
                   sizeof (pResponseMsg->maxTables) +
                   sizeof (pResponseMsg->auxilaryId) +
                   sizeof (pResponseMsg->pad) +
                   sizeof (pResponseMsg->capabilities) +
                   sizeof (pResponseMsg->reserved);

    if (OfcCpAddOpenFlowHdr ((__u8 *) pResponseMsg, featReplyLen,
        OFPT_FEATURES_REPLY, ((tOfcOfHdr *) pCntrlPkt)->xid, 
        &pOpenFlowPkt) != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Failed to add OpenFlow header in "
                          "Feature Reply message\r\n");
        kfree (pResponseMsg);
        pResponseMsg = NULL;
        return OFC_FAILURE;
    }

    if (OfcCpSendCntrlPktFromSock (pOpenFlowPkt, 
        ntohs (((tOfcOfHdr *) pOpenFlowPkt)->length)) != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Failed to send Feature Reply message\r\n");
        kfree (pResponseMsg);
        pResponseMsg = NULL;
        kfree (pOpenFlowPkt);
        pOpenFlowPkt = NULL;
        return OFC_FAILURE;
    }

    kfree(pResponseMsg);
    pResponseMsg = NULL;
    kfree(pOpenFlowPkt);
    pOpenFlowPkt = NULL;
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpConstructPacketIn
*
* Description: This function constructs packet-in message to be
*              sent to the controller
*
* Input: 
*
* Output: ppOpenFlowPkt - Pointer to OpenFlow packet
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpConstructPacketIn (__u8 *pPkt, __u32 pktLen, __u8 inPort, 
                            __u8 msgType, __u8 tableId, 
                            tOfcEightByte cookie, 
                            tOfcMatchFields matchFields,
                            __u8 **ppOpenFlowPkt)
{
    tOfcPktInHdr     *pPktInHdr = NULL;
    tOfcMatchTlv     *pMatchTlv = NULL;
    tOfcMatchOxmTlv  *pOxmTlv = NULL;
    __u32            fourByteField = 0;
    __u16            twoByteField = 0;
    __u16            pktInLen = 0;
    __u16            pktInHdrLen = 0;
    __u16            matchTlvLen = 0;
    __u8             oxmTlvLen = 0;
    __u8             padBytes = 0;
    __u8             aNullMacAddr[OFC_MAC_ADDR_LEN];
    
    /* Construct match field TLV  */
    pMatchTlv = (tOfcMatchTlv *) kmalloc (OFC_MTU_SIZE, GFP_KERNEL);
    if (pMatchTlv == NULL)
    {
        printk (KERN_CRIT "Failed to allocate memory to match "
                          "fields\r\n");
        return OFC_FAILURE;
    }
    
    memset (pMatchTlv, 0, OFC_MTU_SIZE);
    memset (aNullMacAddr, 0, sizeof(aNullMacAddr));

    pMatchTlv->type = htons (OFPMT_OXM);
    pOxmTlv = (tOfcMatchOxmTlv *) (void *) (((__u8 *) pMatchTlv) + 
                                            sizeof(pMatchTlv->type) + 
                                            sizeof(pMatchTlv->length));
    oxmTlvLen = sizeof(pOxmTlv->Class) + sizeof(pOxmTlv->field) +
                sizeof(pOxmTlv->length);
    printk (KERN_INFO "oxmTlvLen: %u\r\n", oxmTlvLen);

    /* TODO: Not supporting field mask option */
    /* Add OXM match field TLVs */
    if (memcmp (matchFields.aDstMacAddr, aNullMacAddr, 
                OFC_MAC_ADDR_LEN))
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        pOxmTlv->field = OFCXMT_OFB_ETH_DST << 1;
        pOxmTlv->length = OFC_MAC_ADDR_LEN;
        memcpy (pOxmTlv->aValue, matchFields.aDstMacAddr, 
                OFC_MAC_ADDR_LEN);
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (memcmp (matchFields.aSrcMacAddr, aNullMacAddr, 
                OFC_MAC_ADDR_LEN))
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        pOxmTlv->field = OFCXMT_OFB_ETH_SRC << 1;
        pOxmTlv->length = OFC_MAC_ADDR_LEN;
        memcpy (pOxmTlv->aValue, matchFields.aSrcMacAddr, 
                OFC_MAC_ADDR_LEN);
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (matchFields.vlanId != 0)
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        pOxmTlv->field = OFCXMT_OFB_VLAN_VID << 1;
        pOxmTlv->length = sizeof (matchFields.vlanId);
        twoByteField = htons (matchFields.vlanId);
        memcpy (pOxmTlv->aValue, &twoByteField,
                sizeof (matchFields.vlanId));
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (matchFields.etherType != 0)
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        pOxmTlv->field = OFCXMT_OFB_ETH_TYPE << 1;
        pOxmTlv->length = sizeof (matchFields.etherType);
        twoByteField = htons (matchFields.etherType);
        memcpy (pOxmTlv->aValue, &twoByteField,
                sizeof (matchFields.etherType));
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (matchFields.srcIpAddr != 0)
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        pOxmTlv->field = OFCXMT_OFB_IPV4_SRC << 1;
        pOxmTlv->length = sizeof (matchFields.srcIpAddr);
        fourByteField = htonl (matchFields.srcIpAddr);
        memcpy (pOxmTlv->aValue, &fourByteField,
                sizeof (matchFields.srcIpAddr));
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (matchFields.dstIpAddr != 0)
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        pOxmTlv->field = OFCXMT_OFB_IPV4_DST << 1;
        pOxmTlv->length = sizeof (matchFields.dstIpAddr);
        fourByteField = htonl (matchFields.dstIpAddr);
        memcpy (pOxmTlv->aValue, &fourByteField,
                sizeof (matchFields.dstIpAddr));
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (matchFields.protocolType != 0)
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        pOxmTlv->field = OFCXMT_OFB_IP_PROTO << 1;
        pOxmTlv->length = sizeof (matchFields.protocolType);
        memcpy (pOxmTlv->aValue, &matchFields.protocolType,
                sizeof (matchFields.protocolType));
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (matchFields.srcPortNum != 0)
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        switch (matchFields.l4HeaderType)
        {
            case OFC_TCP_PROT_TYPE:
                pOxmTlv->field = OFCXMT_OFB_TCP_SRC << 1;
                break;

            case OFC_UDP_PROT_TYPE:
                pOxmTlv->field = OFCXMT_OFB_UDP_SRC << 1;
                break;
        }
        pOxmTlv->length = sizeof (matchFields.srcPortNum);
        twoByteField = htons (matchFields.srcPortNum);
        memcpy (pOxmTlv->aValue, &twoByteField,
                sizeof (matchFields.srcPortNum));
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    if (matchFields.dstPortNum != 0)
    {
        pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
        switch (matchFields.l4HeaderType)
        {
            case OFC_TCP_PROT_TYPE:
                pOxmTlv->field = OFCXMT_OFB_TCP_DST << 1;
                break;

            case OFC_UDP_PROT_TYPE:
                pOxmTlv->field = OFCXMT_OFB_UDP_DST << 1;
                break;
        }
        pOxmTlv->length = sizeof (matchFields.dstPortNum);
        twoByteField = htons (matchFields.dstPortNum);
        memcpy (pOxmTlv->aValue, &twoByteField,
                sizeof (matchFields.dstPortNum));
        matchTlvLen += oxmTlvLen + pOxmTlv->length;
        pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                  (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);
    }

    /* Add input port in match field TLV OXM fields */
    pOxmTlv->Class = htons (OFPXMC_OPENFLOW_BASIC);
    pOxmTlv->field = OFCXMT_OFB_IN_PORT << 1;
    pOxmTlv->length = sizeof (__u32);
    fourByteField = inPort;
    fourByteField = htonl (fourByteField);
    memcpy (pOxmTlv->aValue, &fourByteField, sizeof(fourByteField));
    matchTlvLen += oxmTlvLen + pOxmTlv->length;
    pOxmTlv = (tOfcMatchOxmTlv *) (void *) 
               (((__u8 *) pOxmTlv) + oxmTlvLen + pOxmTlv->length);

    matchTlvLen += sizeof (pMatchTlv->type) + 
                   sizeof (pMatchTlv->length);
    pMatchTlv->length = htons (matchTlvLen);

    /* Add match padding bytes */
    if (matchTlvLen % 8)
    {
        matchTlvLen = (matchTlvLen + 8) - (matchTlvLen % 8);
    }

    printk (KERN_INFO "Match TLV:\r\n");
    OfcDumpPacket ((__u8 *) pMatchTlv, matchTlvLen);

    /* Construct packet-in message */
    pktInLen = sizeof (((tOfcPktInHdr *) 0)->bufId) +
               sizeof (((tOfcPktInHdr *) 0)->totLength) +
               sizeof (((tOfcPktInHdr *) 0)->reason) +
               sizeof (((tOfcPktInHdr *) 0)->tableId) +
               sizeof (((tOfcPktInHdr *) 0)->cookie);

    pktInHdrLen = pktInLen + matchTlvLen + pktLen;
    printk (KERN_INFO "matchTlvLen: %u\r\n", matchTlvLen);
    printk (KERN_INFO "pktInLen: %u\r\n", pktInLen);
    printk (KERN_INFO "pktLen: %u\r\n", pktLen);

#if 0
    /* Add padding if required */
    if (pktInHdrLen % 8)
    {
        padBytes = 8 - (pktInHdrLen % 8);
    }
#endif
    /* Add packet-in padding bytes */
    padBytes += 2;
    pktInHdrLen += padBytes;
    printk (KERN_INFO "pktInHdrLen: %u\r\n", pktInHdrLen);

    pPktInHdr = (tOfcPktInHdr *) kmalloc (pktInHdrLen, GFP_KERNEL);
    if (pPktInHdr == NULL)
    {
        printk (KERN_CRIT "Failed to allocate memory to packet-in"
                          " message\r\n");
        kfree (pMatchTlv);
        pMatchTlv = NULL;
        return OFC_FAILURE;
    }

    memset (pPktInHdr, 0, pktInHdrLen);
    pPktInHdr->bufId = htonl (OFC_NO_BUFFER);
    pPktInHdr->totLength = htons (pktLen);
    pPktInHdr->reason = msgType;
    pPktInHdr->tableId = tableId;
    pPktInHdr->cookie.lo = cookie.lo;
    pPktInHdr->cookie.hi = cookie.hi;
    memcpy (((__u8 *) pPktInHdr) + pktInLen, pMatchTlv, matchTlvLen);
    memcpy (((__u8 *) pPktInHdr) + pktInLen + matchTlvLen + padBytes, 
            pPkt, pktLen);

    printk (KERN_INFO "Packet-In:\r\n");
    OfcDumpPacket ((__u8 *) pPktInHdr, pktInHdrLen);

    /* Add standard OpenFlow header */
    OfcCpAddOpenFlowHdr ((__u8 *) pPktInHdr, pktInHdrLen,
                         OFPT_PACKET_IN, 0, ppOpenFlowPkt);
    
    kfree (pPktInHdr);
    pPktInHdr = NULL;
    kfree (pMatchTlv);
    pMatchTlv = NULL;
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpProcessPktOut
*
* Description: This function processes packet out  messages
*              received from the controller and takes actions
*              as mentioned in the message
*
* Input: pPkt - Pointer to control packet
*        pktLen - Length of control packet
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpProcessPktOut (__u8 *pPkt, __u16 pktLen)
{
    struct list_head actionList;
    tOfcActionList   *pActionList = NULL;
    tOfcActionTlv    *pActionTlv = NULL;
    tOfcPktOutHdr    *pPktOut = NULL;
    __u16            actionListLen = 0;
 
    pPktOut = 
        (tOfcPktOutHdr *) (void *) (pPkt + OFC_OPENFLOW_HDR_LEN);
    actionListLen = ntohs (pPktOut->actionsLen);

    pActionTlv = (tOfcActionTlv *) (void *) (((__u8 *) pPktOut) +
                                           sizeof (tOfcPktOutHdr));

    return OFC_SUCCESS;   
}

/******************************************************************                                                                          
* Function: OfcCpProcessFlowMod
*
* Description: This function processes flow mod messages received
*              from the controller and installs or deletes flows
*              in the flow table
*
* Input: pPkt - Pointer to control packet
*        pktLen - Length of control packet
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpProcessFlowMod (__u8 *pPkt, __u16 pktLen)
{
    tOfcFlowModHdr  *pFlowMod = NULL;
    tOfcFlowEntry   *pFlowEntry = NULL;
    tDpCpMsgQ       msgQ;
    __u16           flowModLen = 0;
 
    pFlowMod =
        (tOfcFlowModHdr *) (void *) (pPkt + OFC_OPENFLOW_HDR_LEN);
    flowModLen = pktLen - OFC_OPENFLOW_HDR_LEN;

    printk (KERN_INFO "pktLen:%d, flowModLen:%d\r\n", pktLen, flowModLen);
    OfcDumpPacket ((__u8 *) pFlowMod, flowModLen);

    switch (pFlowMod->command)
    {
        case OFPFC_ADD:
        /* Intentional fall through */
        case OFPFC_DELETE:
            pFlowEntry = OfcCpExtractFlow (pFlowMod, flowModLen);
            if (pFlowEntry == NULL)
            {
                printk (KERN_CRIT "Failed to extract flow from Flow"
                                  " Mod message\r\n");
                return OFC_FAILURE;
            }
            break;

        default:
            printk (KERN_CRIT "Flow Mod command not supported!!\r\n");
            return OFC_FAILURE;
    }

    /* Send the extracted flow to data path task for insertion or
     * deletion in flow table */
    memset (&msgQ, 0, sizeof (msgQ));
    msgQ.pFlowEntry = pFlowEntry;
    msgQ.msgType = (pFlowMod->command == OFPFC_ADD) ? 
                    OFC_FLOW_MOD_ADD : OFC_FLOW_MOD_DEL;
    OfcCpSendToDpQ (&msgQ);
    OfcDpSendEvent (OFC_CP_TO_DP_EVENT);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpExtractFlow
*
* Description: This function extracts a flow entry from Flow Mod
*              message
*
* Input: pFlowMod - Pointer to flow mod header
*        flowModLen - Length of flow mod packet
*
* Output: None
*
* Returns: Pointer to flow entry
*
*******************************************************************/
tOfcFlowEntry *OfcCpExtractFlow (tOfcFlowModHdr *pFlowMod,
                                 __u16 flowModLen)
{
    tOfcFlowEntry    *pFlowEntry = NULL;

    pFlowEntry = (tOfcFlowEntry *) kmalloc (sizeof (tOfcFlowEntry),
                                            GFP_KERNEL);
    if (pFlowEntry == NULL)
    {
        printk (KERN_CRIT "Failed to allocate memory for " 
                          "new flow\r\n");
        return NULL;
    }

    /* Populate flow entry fields */
    memset (pFlowEntry, 0, sizeof (tOfcFlowEntry));
    pFlowEntry->cookie.hi = pFlowMod->cookie.hi;
    pFlowEntry->cookie.lo = pFlowMod->cookie.lo;
    pFlowEntry->tableId = pFlowMod->tableId;
    pFlowEntry->idleTimeout = ntohs (pFlowMod->idleTimeout);
    pFlowEntry->hardTimeout = ntohs (pFlowMod->hardTimeout);
    pFlowEntry->priority = ntohs (pFlowMod->priority);
    pFlowEntry->bufId = ntohl (pFlowMod->bufId);
    pFlowEntry->outPort = ntohl (pFlowMod->outPort);
    pFlowEntry->outGrp = ntohl (pFlowMod->outGrp);
    pFlowEntry->flags = ntohs (pFlowMod->flags);
    INIT_LIST_HEAD (&pFlowEntry->matchList);
    INIT_LIST_HEAD (&pFlowEntry->instrList);
    
    /* Add match fields list in flow entry */
    if (OfcCpAddMatchFieldsInFlow (pFlowMod, flowModLen, pFlowEntry)
        != OFC_SUCCESS)
    {
        kfree (pFlowEntry);
        pFlowEntry = NULL;
        return NULL;
    }

    /* Add instruction list in flow entry */
    if (OfcCpAddInstrListInFlow (pFlowMod, flowModLen, pFlowEntry)
        != OFC_SUCCESS)
    {
        kfree (pFlowEntry);
        pFlowEntry = NULL;
        return NULL;
    }

    return pFlowEntry;
}

/******************************************************************                                                                          
* Function: OfcCpAddMatchFieldsInFlow
*
* Description: This function adds list of match fields in flow
*              entry
*
* Input: pFlowMod - Pointer to flow mod header
*        flowModLen - Length of flow mod packet
*        pFlowEntry - Pointer to flow entry
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpAddMatchFieldsInFlow (tOfcFlowModHdr *pFlowMod, 
                               __u16 flowModLen,
                               tOfcFlowEntry *pFlowEntry)
{
    tOfcMatchTlv     *pMatchTlv = NULL;
    tOfcMatchOxmTlv  *pOfcMatchOxmTlv = NULL;
    tMatchListEntry  *pMatchList = NULL;
    __u16            matchTlvLen = 0;
    __u8             oxmTlvLen = 0;
    __u32            inPort = 0;

    if (flowModLen < OFC_MATCH_TLV_OFFSET)
    {
        printk (KERN_CRIT "Invalid flow mod message length\r\n");
        return OFC_FAILURE;
    }

    pMatchTlv = (tOfcMatchTlv *) (void *) (((__u8 *) pFlowMod) + 
                                           OFC_MATCH_TLV_OFFSET);
    if (ntohs (pMatchTlv->type) != OFPMT_OXM)
    {
        printk (KERN_CRIT "Match field type mismatch!!\r\n");
        return OFC_FAILURE;
    }

    matchTlvLen = ntohs (pMatchTlv->length);
    printk (KERN_INFO "matchTlvLen(before): %d\r\n", matchTlvLen);
    OfcDumpPacket ((__u8 *) pMatchTlv, matchTlvLen);
    /* Discern length of match Oxm TLV */
    matchTlvLen = matchTlvLen - (sizeof (pMatchTlv->type) + 
                                 sizeof (pMatchTlv->length));
    printk (KERN_INFO "matchTlvLen(after): %d\r\n", matchTlvLen);

    pOfcMatchOxmTlv = (tOfcMatchOxmTlv *) (void *) 
                      (((__u8 *) pMatchTlv) + sizeof (pMatchTlv->type) +
                         sizeof (pMatchTlv->length));
    oxmTlvLen = sizeof(pOfcMatchOxmTlv->Class) + 
                sizeof(pOfcMatchOxmTlv->field) +
                sizeof(pOfcMatchOxmTlv->length);

    /* Add each Oxm match to flow entry match list */
    while (matchTlvLen > 0)
    {
        printk (KERN_INFO "Parsing match list\r\n");
        if (ntohs (pOfcMatchOxmTlv->Class) != OFPXMC_OPENFLOW_BASIC)
        {
            matchTlvLen = matchTlvLen - (oxmTlvLen +  
                                         pOfcMatchOxmTlv->length);
            pOfcMatchOxmTlv =
                (tOfcMatchOxmTlv *) (void *) (((__u8 *) pOfcMatchOxmTlv) + 
                                               oxmTlvLen +
                                               pOfcMatchOxmTlv->length);
            continue;
        }

        if ((pOfcMatchOxmTlv->field >> 1) >= OFCXMT_OFB_MAX)
        {
            matchTlvLen = matchTlvLen - (oxmTlvLen +  
                                         pOfcMatchOxmTlv->length);
            pOfcMatchOxmTlv =
                (tOfcMatchOxmTlv *) (void *) (((__u8 *) pOfcMatchOxmTlv) + 
                                               oxmTlvLen +
                                               pOfcMatchOxmTlv->length);
            continue;
        }

        pMatchList = (tMatchListEntry *) kmalloc (sizeof(tMatchListEntry),
                                                  GFP_KERNEL);
        if (pMatchList == NULL)
        {
            printk (KERN_CRIT "Failed to allocate memory to match "
                              "list entry\r\n");
            /* TODO: Delete each match list entry */
            return OFC_FAILURE;
        }

        memset (pMatchList, 0, sizeof (tMatchListEntry));
        pMatchList->field = pOfcMatchOxmTlv->field >> 1;
        pMatchList->length = pOfcMatchOxmTlv->length;
        memcpy (pMatchList->aValue, pOfcMatchOxmTlv->aValue,
                pMatchList->length);

        INIT_LIST_HEAD (&pMatchList->list);
        list_add_tail (&pMatchList->list, &pFlowEntry->matchList);

        /* Update match fields structure */
        switch (pMatchList->field)
        {
            case OFCXMT_OFB_IN_PORT:
                memcpy (&inPort, pMatchList->aValue, 
                        pMatchList->length);
                inPort = ntohl (inPort);
                pFlowEntry->matchFields.inPort = inPort;
                break;

            case OFCXMT_OFB_ETH_DST:
                memcpy (pFlowEntry->matchFields.aDstMacAddr,
                        pMatchList->aValue, pMatchList->length);
                break;

            case OFCXMT_OFB_ETH_SRC:
                memcpy (pFlowEntry->matchFields.aSrcMacAddr,
                        pMatchList->aValue, pMatchList->length);
                break;

            case OFCXMT_OFB_VLAN_VID:
                memcpy (&pFlowEntry->matchFields.vlanId,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.vlanId =
                    ntohs (pFlowEntry->matchFields.vlanId);
                break;

            case OFCXMT_OFB_ETH_TYPE:
                memcpy (&pFlowEntry->matchFields.etherType,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.etherType =
                    ntohs (pFlowEntry->matchFields.etherType);
                break;

            case OFCXMT_OFB_IP_PROTO:
                memcpy (&pFlowEntry->matchFields.protocolType,
                        pMatchList->aValue, pMatchList->length);
                break;

            case OFCXMT_OFB_IPV4_SRC:
                memcpy (&pFlowEntry->matchFields.srcIpAddr,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.srcIpAddr =
                    ntohl (pFlowEntry->matchFields.srcIpAddr);
                break;

            case OFCXMT_OFB_IPV4_DST:
                memcpy (&pFlowEntry->matchFields.dstIpAddr,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.dstIpAddr =
                    ntohl (pFlowEntry->matchFields.dstIpAddr);
                break;

            case OFCXMT_OFB_TCP_SRC:
                memcpy (&pFlowEntry->matchFields.srcPortNum,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.srcPortNum =
                    ntohs (pFlowEntry->matchFields.srcPortNum);
                pFlowEntry->matchFields.l4HeaderType = OFC_TCP_PROT_TYPE;
                break;

            case OFCXMT_OFB_TCP_DST:
                memcpy (&pFlowEntry->matchFields.dstPortNum,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.dstPortNum =
                    ntohs (pFlowEntry->matchFields.dstPortNum);
                pFlowEntry->matchFields.l4HeaderType = OFC_TCP_PROT_TYPE;
                break;

            case OFCXMT_OFB_UDP_SRC:
                memcpy (&pFlowEntry->matchFields.srcPortNum,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.srcPortNum =
                    ntohs (pFlowEntry->matchFields.srcPortNum);
                pFlowEntry->matchFields.l4HeaderType = OFC_UDP_PROT_TYPE;
                break;

            case OFCXMT_OFB_UDP_DST:
                memcpy (&pFlowEntry->matchFields.dstPortNum,
                        pMatchList->aValue, pMatchList->length);
                pFlowEntry->matchFields.dstPortNum =
                    ntohs (pFlowEntry->matchFields.dstPortNum);
                pFlowEntry->matchFields.l4HeaderType = OFC_UDP_PROT_TYPE;
                break;

            default:
                break;
        }

        matchTlvLen = matchTlvLen - (oxmTlvLen +
                                     pOfcMatchOxmTlv->length);
        pOfcMatchOxmTlv =
            (tOfcMatchOxmTlv *) (void *) (((__u8 *) pOfcMatchOxmTlv) + 
                                           oxmTlvLen +
                                           pOfcMatchOxmTlv->length);
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpAddInstrListInFlow
*
* Description: This function adds list of instructions in flow
*              entry
*
* Input: pFlowMod - Pointer to flow mod header
*        flowModLen - Length of flow mod packet
*        pFlowEntry - Pointer to flow entry
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpAddInstrListInFlow (tOfcFlowModHdr *pFlowMod, 
                             __u16 flowModLen,
                             tOfcFlowEntry *pFlowEntry)
{
    tOfcInstrTlv    *pInstrTlv = NULL;
    tOfcInstrList   *pInstrList = NULL;
    tOfcMatchTlv    *pMatchTlv = NULL;
    tOfcActionTlv   *pActionTlv = NULL;
    __u8            *pPktParser = NULL;
    __u16           matchTlvLen = 0;
    __u16           instrTlvLen = 0;
    __u16           actionTlvLen = 0;
 
    /* Compute instruction list offset */
    pMatchTlv = (tOfcMatchTlv *) (void *) (((__u8 *) pFlowMod) +
                                           OFC_MATCH_TLV_OFFSET);
    matchTlvLen = ntohs (pMatchTlv->length);
    if (matchTlvLen % 8)
    {
        matchTlvLen = (matchTlvLen + 8) - (matchTlvLen % 8);
    }
    printk (KERN_INFO "[%s]: matchTlvLen: %d\r\n", __func__, matchTlvLen);

    pInstrTlv = (tOfcInstrTlv *) (void *) (((__u8 *) pMatchTlv) +
                                            matchTlvLen);
    instrTlvLen = flowModLen - (OFC_MATCH_TLV_OFFSET + matchTlvLen);
    printk (KERN_INFO "[%s]: instrTlvLen: %d\r\n", __func__, instrTlvLen);
    OfcDumpPacket ((__u8 *) pInstrTlv, instrTlvLen);

    while (instrTlvLen > 0)
    {
        printk (KERN_INFO "Parsing Instruction List\r\n");
        pInstrList = (tOfcInstrList *) kmalloc (sizeof(tOfcInstrList),
                                                GFP_KERNEL);
        if (pInstrList == NULL)
        {
            printk (KERN_CRIT "Failed to allocated memory to "
                              "instruction list\r\n");
            /* TODO: Delete instruction list */
            return OFC_FAILURE;
        }

        memset (pInstrList, 0, sizeof (tOfcInstrList));
        pInstrList->instrType = ntohs (pInstrTlv->type);
        INIT_LIST_HEAD (&pInstrList->list);
        INIT_LIST_HEAD (&pInstrList->u.actionList);
     
        switch (pInstrList->instrType)
        {
            case OFCIT_GOTO_TABLE:
                pPktParser = 
                    (__u8 *) (void *) (((__u8 *) pInstrTlv) + 
                                       sizeof (pInstrTlv->type) +
                                       sizeof (pInstrTlv->length));
                memcpy (&pInstrList->u.tableId, pPktParser,
                        sizeof (pInstrList->u.tableId));
                if ((pInstrList->u.tableId <= pFlowEntry->tableId)
                    || (pInstrList->u.tableId >= OFC_MAX_FLOW_TABLES))
                {
                    printk (KERN_CRIT "Invalid table Id in GOTO"
                                      " instruction\r\n");
                    /* TODO: Delete instruction list */
                    return OFC_FAILURE;
                }

                list_add_tail (&pInstrList->list, 
                               &pFlowEntry->instrList);
                break;

            case OFCIT_WRITE_ACTIONS:
            case OFCIT_APPLY_ACTIONS:
                pActionTlv = (tOfcActionTlv *) (void *)
                              (((__u8 *) pInstrTlv) + 
                               sizeof (pInstrTlv->type) +
                               sizeof (pInstrTlv->length) + 4);
                actionTlvLen = ntohs (pInstrTlv->length) -
                               (sizeof (pInstrTlv->type) +
                                sizeof (pInstrTlv->length) + 4);
                if (OfcCpAddActionListToInstr (pActionTlv,
                                               actionTlvLen, 
                                               pInstrList) 
                    != OFC_SUCCESS)
                {
                    printk (KERN_CRIT "Failed to add action list in"
                                      " instruction\r\n");
                    /* TODO: Delete instruction list */
                    return OFC_FAILURE;
                }

                list_add_tail (&pInstrList->list,
                               &pFlowEntry->instrList);
                break;
                
        }

        instrTlvLen -= (ntohs (pInstrTlv->length));
        pInstrTlv = (tOfcInstrTlv *) (void *) 
                     (((__u8 *) pInstrTlv) + ntohs (pInstrTlv->length));
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpAddInstrListInFlow
*
* Description: This function adds list of actions in flow
*              entry instruction list
*
* Input: pActionTlv - Pointer to action TLV
*        actionTlvLen - Length of action list TLVs
*
* Output: pInstrList - Pointer to instruction list
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpAddActionListToInstr (tOfcActionTlv *pActionTlv,
                               __u16 actionTlvLen,
                               tOfcInstrList *pInstrList)
{
    tOfcActionList  *pActionList = NULL;
    __u8            *pPktParser = NULL;
    
    printk (KERN_INFO "[%s]: actionTlvLen:%d\r\n", __func__, actionTlvLen);
    OfcDumpPacket ((__u8 *) pActionTlv, actionTlvLen);

    while (actionTlvLen > 0)
    {
        printk (KERN_INFO "[%s]: Parsing Action List\r\n", __func__);
        pActionList = (tOfcActionList *) kmalloc (sizeof(tOfcActionList),
                                                  GFP_KERNEL);
        if (pActionList == NULL)
        {
            printk (KERN_CRIT "Failed to allocate memory to action "
                              "list\r\n");
            /* TODO: Delete action list */
            return OFC_FAILURE;
        }

        memset (pActionList, 0, sizeof (tOfcActionList));
        pActionList->actionType = ntohs (pActionTlv->type);
        INIT_LIST_HEAD (&pActionList->list);

        switch (pActionList->actionType)
        {
            case OFCAT_OUTPUT:
                pPktParser = (__u8 *) (void *)
                              (((__u8 *) pActionTlv) +
                               sizeof (pActionTlv->type) +
                               sizeof (pActionTlv->length));
                memcpy (&pActionList->u.outPort, pPktParser,
                        sizeof (pActionList->u.outPort));
                pActionList->u.outPort =
                    ntohl (pActionList->u.outPort);
                printk (KERN_INFO "[%s]: pActionList->u.outPort: 0x%x\r\n", __func__, pActionList->u.outPort);

                list_add_tail (&pActionList->list,
                               &pInstrList->u.actionList);
                break;

            default:
                /* TODO: Support other actions? */
                printk (KERN_CRIT "Action not supported presently\r\n");
                return OFC_FAILURE;
        }

        actionTlvLen -= (ntohs (pActionTlv->length));
        pActionTlv = (tOfcActionTlv *) (void *) 
                      (((__u8 *) pActionTlv) + 
                       (ntohs (pActionTlv->length)));
    }

    return OFC_SUCCESS;
}
