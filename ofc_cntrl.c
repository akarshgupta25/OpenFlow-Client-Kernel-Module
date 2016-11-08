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
    __u8   *pCntrlPkt = NULL;
    __u32  pktLen = 0;

    if (OfcCpRecvCntrlPktOnSock (&pCntrlPkt, &pktLen) 
        != OFC_SUCCESS)
    {
        return OFC_FAILURE;
    }

    printk (KERN_INFO "Control packet recevied\r\n"); 

    kfree (pCntrlPkt);
    return OFC_SUCCESS;
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
            return;
        }
        pktLen = ntohs (((tOfcOfHdr *) pOpenFlowPkt)->length);

        OfcCpSendCntrlPktFromSock (pOpenFlowPkt, pktLen);

        printk (KERN_INFO "OpenFlow Packet:\r\n");
        OfcDumpPacket (pOpenFlowPkt, pktLen);

        kfree (pOpenFlowPkt);

        /* Release message */
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
    pOfHdr->xid = htonl (xid);

    if (pPktHdr != NULL)
    {
        memcpy (((__u8 *) pOfHdr) + OFC_OPENFLOW_HDR_LEN, 
                pPktHdr, pktHdrLen);
    }

    *ppOfPkt = (__u8 *) pOfHdr;
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
    fourByteField = matchFields.inPort;
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
    kfree (pMatchTlv);
    return OFC_SUCCESS;
}
