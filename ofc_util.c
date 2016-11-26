/********************************************************************
*
* Filename: ofc_util.c
*
* Description: This file contains the utility functions used by
*              OpenFlow data path and control path sub modules.
*
*******************************************************************/

#include "ofc_hdrs.h"

extern struct net init_net;
extern tOfcDpGlobals gOfcDpGlobals;
extern tOfcCpGlobals gOfcCpGlobals;
extern char *gpOpenFlowIf[OFC_MAX_OF_IF_NUM];
extern int gNumOpenFlowIf;
extern unsigned int gCntrlIpAddr;
extern unsigned short gCntrlPortNo;

/* Event wait queue for data path task */
DECLARE_WAIT_QUEUE_HEAD (gOfcDpWaitQueue);
/* Event wait queue for control path task */
DECLARE_WAIT_QUEUE_HEAD (gOfcCpWaitQueue);

/******************************************************************                                                                          
* Function: OfcDpReceiveEvent
*
* Description: This function causes data path task to wait for
*              events, and receives events from kernel or control
*              path task
*
* Input: events - BitList of possible events
*
* Output: pRxEvents - Event that has occurred
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpReceiveEvent (int events, int *pRxEvents)
{
    printk (KERN_INFO "Waiting for event...\r\n");
    wait_event_interruptible (gOfcDpWaitQueue, gOfcDpGlobals.events);
    printk (KERN_INFO "Event Rx: %u\r\n", gOfcDpGlobals.events);
    if (gOfcDpGlobals.events & events)
    {
        down_interruptible (&gOfcDpGlobals.semId);
        *pRxEvents = gOfcDpGlobals.events & events;
        gOfcDpGlobals.events &= ~(*pRxEvents);
        up (&gOfcDpGlobals.semId);
        return OFC_SUCCESS;
    }

    return OFC_FAILURE;
}

/******************************************************************                                                                          
* Function: OfcDpSendEvent
*
* Description: This function posts events to data path task
*
* Input: events - Event that has occurred
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpSendEvent (int events)
{
    printk (KERN_INFO "Sending event:%u\r\n", events);
    down_interruptible (&gOfcDpGlobals.semId);
    gOfcDpGlobals.events |= events;
    up (&gOfcDpGlobals.semId);
    wake_up_interruptible (&gOfcDpWaitQueue);
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpReceiveEvent
*
* Description: This function waits and receives events from
*              from kernel and data path task
*
* Input: events - BitList of possible events
*
* Output: pRxEvents - Event that has occurred
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpReceiveEvent (int events, int *pRxEvents)
{
    printk (KERN_INFO "Control path Waiting for event...\r\n");
    wait_event_interruptible (gOfcCpWaitQueue, gOfcCpGlobals.events);
    printk (KERN_INFO "Control path Event Rx: %u\r\n", gOfcCpGlobals.events);
    if (gOfcCpGlobals.events & events)
    {
        down_interruptible (&gOfcCpGlobals.semId);
        *pRxEvents = gOfcCpGlobals.events & events;
        gOfcCpGlobals.events &= ~(*pRxEvents);
        up (&gOfcCpGlobals.semId);
        return OFC_SUCCESS;
    }

    return OFC_FAILURE;
}

/******************************************************************                                                                          
* Function: OfcCpSendEvent
*
* Description: This function posts events to control path task
*
* Input: events - Event that has occurred
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpSendEvent (int events)
{
    printk (KERN_INFO "Control path sending event:%u\r\n", events);
    down_interruptible (&gOfcCpGlobals.semId);
    gOfcCpGlobals.events |= events;
    up (&gOfcCpGlobals.semId);
    wake_up_interruptible (&gOfcCpWaitQueue);
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpRecvFromDataPktQ
*
* Description: This function extracts messages from data packet
*              queue of data path task
*
* Input: None
*
* Output: None
*
* Returns: Pointer to extracted message
*
*******************************************************************/
tDataPktRxIfQ *OfcDpRecvFromDataPktQ (void)
{
    struct list_head *pMsgQ = NULL;
    struct list_head *pListHead = NULL;

    pListHead = &gOfcDpGlobals.pktRxIfListHead;
    list_for_each (pMsgQ, pListHead)
    {
        /* Queue is not empty, remove first element */
        list_del_init (pMsgQ);
        return (tDataPktRxIfQ *) pMsgQ;
    }

    return NULL;
}

/******************************************************************                                                                          
* Function: OfcDpSendToDataPktQ
*
* Description: This function sends messages to data packet queue of 
*              data path task
*
* Input: dataIfNum - OpenFlow interface
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpSendToDataPktQ (int dataIfNum, __u8 *pDataPkt, 
                         __u16 dataPktLen)
{
    tDataPktRxIfQ *pMsgQ = NULL;

    pMsgQ = (tDataPktRxIfQ *) kmalloc (sizeof(tDataPktRxIfQ), 
                                       GFP_KERNEL);
    if (pMsgQ == NULL)
    {
        printk (KERN_CRIT "Queue message allocation failed!!\r\n");
        return OFC_FAILURE;
    }

    memset (pMsgQ, 0, sizeof (tDataPktRxIfQ));
    INIT_LIST_HEAD (&pMsgQ->list);
    pMsgQ->pDataPkt = pDataPkt;
    pMsgQ->dataPktLen = dataPktLen;
    pMsgQ->dataIfNum = dataIfNum;
    list_add_tail (&pMsgQ->list, &gOfcDpGlobals.pktRxIfListHead);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpRecvFromCpMsgQ
*
* Description: This function extracts messages from control path
*              message queue in data path task
*
* Input: None
*
* Output: None
*
* Returns: Pointer to extracted message
*
*******************************************************************/
tDpCpMsgQ *OfcDpRecvFromCpMsgQ (void)
{
    struct list_head *pMsgQ = NULL;
    struct list_head *pListHead = NULL;

    pListHead = &gOfcDpGlobals.cpMsgListHead;
    list_for_each (pMsgQ, pListHead)
    {
        /* Queue is not empty, remove first element */
        list_del_init (pMsgQ);
        return (tDpCpMsgQ *) pMsgQ;
    }

    return NULL;
}

/******************************************************************                                                                          
* Function: OfcDpSendToCpQ
*
* Description: This function sends messages from data path task
*              to control path task queue
*
* Input: pPkt - Data packet
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpSendToCpQ (tDpCpMsgQ *pMsgParam)
{
    tDpCpMsgQ *pMsgQ = NULL;

    pMsgQ = (tDpCpMsgQ *) kmalloc (sizeof(tDpCpMsgQ),
                                   GFP_KERNEL);
    if (pMsgQ == NULL)
    {
        printk (KERN_CRIT "Queue message allocation failed!!\r\n");
        return OFC_FAILURE;
    }

    down_interruptible (&gOfcCpGlobals.dpMsgQSemId);

    memset (pMsgQ, 0, sizeof(tDpCpMsgQ));
    memcpy (pMsgQ, pMsgParam, sizeof(tDpCpMsgQ));
    INIT_LIST_HEAD (&pMsgQ->list);
    list_add_tail (&pMsgQ->list, &gOfcCpGlobals.dpMsgListHead);

    up (&gOfcCpGlobals.dpMsgQSemId);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpSendToDpQ
*
* Description: This function sends messages from control path task
*              to data path task queue
*
* Input: pPkt - Data packet
*             - Flow entry
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpSendToDpQ (tDpCpMsgQ *pMsgParam)
{
    tDpCpMsgQ *pMsgQ = NULL;

    pMsgQ = (tDpCpMsgQ *) kmalloc (sizeof(tDpCpMsgQ),
                                   GFP_KERNEL);
    if (pMsgQ == NULL)
    {
        printk (KERN_CRIT "Queue message allocation failed!!\r\n");
        return OFC_FAILURE;
    }

    down_interruptible (&gOfcDpGlobals.cpMsgQSemId);

    memset (pMsgQ, 0, sizeof(tDpCpMsgQ));
    memcpy (pMsgQ, pMsgParam, sizeof (tDpCpMsgQ));
    INIT_LIST_HEAD (&pMsgQ->list);
    list_add_tail (&pMsgQ->list, &gOfcDpGlobals.cpMsgListHead);

    up (&gOfcDpGlobals.cpMsgQSemId);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpRecvFromDpMsgQ
*
* Description: This function extracts messages from data path
*              message queue in control path task
*
* Input: None
*
* Output: None
*
* Returns: Pointer to extracted message
*
*******************************************************************/
tDpCpMsgQ *OfcCpRecvFromDpMsgQ (void)
{
    struct list_head *pMsgQ = NULL;
    struct list_head *pListHead = NULL;

    pListHead = &gOfcCpGlobals.dpMsgListHead;
    list_for_each (pMsgQ, pListHead)
    {
        /* Queue is not empty, remove first element */
        list_del_init (pMsgQ);
        return (tDpCpMsgQ *) pMsgQ;
    }

    return NULL;
}

/******************************************************************                                                                          
* Function: OfcGetNetDevByName
*
* Description: This function returns the net device pointer
*              corresponding to interface name
*
* Input: pIfName - pointer to interface name
*
* Output: None
*
* Returns: Pointer to network device
*
*******************************************************************/
struct net_device *OfcGetNetDevByName (char *pIfName)
{
    struct net_device *pTempDev = NULL;

    pTempDev = first_net_device (&init_net);
    while (pTempDev)
    {
        if (!strcmp (pTempDev->name, pIfName))
        {
            return pTempDev;
        }
        pTempDev = next_net_device (pTempDev);
    }

    return NULL;
}

/******************************************************************                                                                          
* Function: OfcGetNetDevByIp
*
* Description: This function returns the net device pointer
*              corresponding to the ip address massed.
*
* Input: ipAddr - IP address that belongs in the subnet of the net
*                 device.
*
* Output: None
*
* Returns: Pointer to network device
*
*******************************************************************/
struct net_device *OfcGetNetDevByIp (unsigned int ipAddr)
{
    struct net_device *pTempDev = NULL;

    pTempDev = first_net_device (&init_net);
    while (pTempDev)
    {
        for_ifa(pTempDev->ip_ptr) 
        {
            if ((ifa->ifa_mask & ifa->ifa_address) == 
                (ifa->ifa_mask & htonl(ipAddr)))
            {
                return pTempDev;
            }
        }
        endfor_ifa(indev)
        pTempDev = next_net_device (pTempDev);
    }

    return NULL;
}

/******************************************************************
* Function: OfcDumpPacket
*
* Description: Debug function to dump packets
*
* Input: au1Packet - Packet
*        len - Packet length 
*
* Output: None
*
* Returns: None
*
*******************************************************************/
void OfcDumpPacket (char *au1Packet, int len)
{   
    unsigned int u4ByteCount = 0;
    unsigned int u4Length = len;
    
    for (u4ByteCount = 0; u4ByteCount < u4Length; u4ByteCount++)
    {   
        if ((u4ByteCount % 16) == 0)
        {   
            printk (KERN_INFO "\n");
        }
        if (au1Packet[u4ByteCount] <= 0xF)
        {   
            printk (KERN_INFO "0%x ", au1Packet[u4ByteCount]);
        }
        else
        {   
            printk (KERN_INFO "%x ", au1Packet[u4ByteCount]);
        }
    }
    printk (KERN_INFO "\n");
    
    return;
}

/* Debug function to flows in flow table */
void OfcDumpFlows (__u8 tableId)
{
    tOfcFlowEntry    *pFlowEntry = NULL;
    tOfcFlowTable    *pFlowTable = NULL;
    struct list_head *pList = NULL;
    struct list_head *pList2 = NULL;
    struct list_head *pList3 = NULL;
    tMatchListEntry  *pMatchList = NULL;
    tOfcInstrList    *pInstrList = NULL;
    tOfcActionList   *pActionList = NULL;
    __u8             aNullMacAddr[OFC_MAC_ADDR_LEN];
    __u8             flowNum = 1;
    __u8             index = 0;

    memset (aNullMacAddr, 0, sizeof(aNullMacAddr));

    pFlowTable = OfcDpGetFlowTableEntry (tableId);
    if (pFlowTable == NULL)
    {
        printk (KERN_CRIT "[%s]: Invalid flow table Id\r\n",
                           __func__);
        return;
    }

    list_for_each (pList, &pFlowTable->flowEntryList)
    {
        pFlowEntry = (tOfcFlowEntry *) pList;
        printk (KERN_INFO "\nFlow Entry (%d)\r\n", flowNum++);
        printk (KERN_INFO "cookie.hi:0x%x, cookie.lo:0x%x\r\n", 
                pFlowEntry->cookie.hi, pFlowEntry->cookie.lo);
        printk (KERN_INFO "tableId:%d\r\n", pFlowEntry->tableId);
        printk (KERN_INFO "idleTimeout:%d, hardTimeout:%d\r\n",
                pFlowEntry->idleTimeout, pFlowEntry->hardTimeout);
        printk (KERN_INFO "priority:%d\r\n", pFlowEntry->priority);
        printk (KERN_INFO "bufId:0x%x\r\n", pFlowEntry->bufId);
        printk (KERN_INFO "outPort:0x%x, outGrp:0x%x, flags:%d\r\n", 
                pFlowEntry->outPort, pFlowEntry->outGrp, 
                pFlowEntry->flags);

        printk (KERN_INFO "MatchList:\r\n");
        list_for_each (pList2, &pFlowEntry->matchList)
        {
            pMatchList = (tMatchListEntry *) pList2;
            printk (KERN_INFO "field:%d, length:%d\r\n",
                    pMatchList->field, pMatchList->length);
            for (index = 0; index < pMatchList->length;
                 index++)
            {
                printk (KERN_INFO "value[%d]:0x%x\r\n",  
                        index, pMatchList->aValue[index]);
            }
        }

        printk (KERN_INFO "Match Fields:\r\n");
        if (memcmp (pFlowEntry->matchFields.aDstMacAddr,
                    aNullMacAddr, OFC_MAC_ADDR_LEN))
        {
            for (index = 0; index < OFC_MAC_ADDR_LEN; index++)
            {
                printk (KERN_INFO "dst[%d]:0x%x\r\n", index,
                        pFlowEntry->matchFields.aDstMacAddr[index]);
            }
        }
        if (memcmp (pFlowEntry->matchFields.aSrcMacAddr,
                    aNullMacAddr, OFC_MAC_ADDR_LEN))
        {
            for (index = 0; index < OFC_MAC_ADDR_LEN; index++)
            {
                printk (KERN_INFO "src[%d]:0x%x\r\n", index,
                        pFlowEntry->matchFields.aSrcMacAddr[index]);
            }
        }
        if (pFlowEntry->matchFields.vlanId != 0)
        {
            printk (KERN_INFO "vlanId:%d\r\n",
                    pFlowEntry->matchFields.vlanId);
        }
        if (pFlowEntry->matchFields.etherType != 0)
        {
            printk (KERN_INFO "etherType:0x%x\r\n",
                    pFlowEntry->matchFields.etherType);
        }
        if (pFlowEntry->matchFields.srcIpAddr != 0)
        {
            printk (KERN_INFO "srcIpAddr:%u\r\n",
                    pFlowEntry->matchFields.srcIpAddr);
        }
        if (pFlowEntry->matchFields.dstIpAddr != 0)
        {
            printk (KERN_INFO "dstIpAddr:%u\r\n",
                    pFlowEntry->matchFields.dstIpAddr);
        }
        if (pFlowEntry->matchFields.protocolType != 0)
        {
            printk (KERN_INFO "protocolType:0x%x\r\n",
                    pFlowEntry->matchFields.protocolType);
        }
        if (pFlowEntry->matchFields.srcPortNum != 0)
        {
            printk (KERN_INFO "srcPortNum:%d\r\n",
                    pFlowEntry->matchFields.srcPortNum);
        }
        if (pFlowEntry->matchFields.dstPortNum != 0)
        {
            printk (KERN_INFO "dstPortNum:%d\r\n",
                    pFlowEntry->matchFields.dstPortNum);
        }
        if (pFlowEntry->matchFields.l4HeaderType != 0)
        {
            printk (KERN_INFO "l4HeaderType:%d\r\n",
                    pFlowEntry->matchFields.l4HeaderType);
        }
        if (pFlowEntry->matchFields.arpFlds.targetIpAddr 
            != 0)
        {
            printk (KERN_INFO "arpFlds.targetIpAddr:%u\r\n",
                    pFlowEntry->matchFields.arpFlds.targetIpAddr);
        }

        printk (KERN_INFO "Instruction List:\r\n");
        list_for_each (pList2, &pFlowEntry->instrList)
        {
            pInstrList = (tOfcInstrList *) pList2;
            printk (KERN_INFO "type:%d\r\n", pInstrList->instrType);
            if (pInstrList->instrType == OFCIT_GOTO_TABLE)
            {
                printk (KERN_INFO "GotoTableId:%d\r\n", 
                        pInstrList->u.tableId);
            }
            if ((pInstrList->instrType == OFCIT_WRITE_ACTIONS) ||
                (pInstrList->instrType == OFCIT_APPLY_ACTIONS))
            {
                printk (KERN_INFO "Action List:\r\n");
                list_for_each (pList3, &pInstrList->u.actionList)
                {
                    pActionList = (tOfcActionList *) pList3;
                    printk (KERN_INFO "type:%d\r\n",
                            pActionList->actionType);
                    if (pActionList->actionType == OFCAT_OUTPUT)
                    {
                        printk (KERN_INFO "outputPort:0x%x\r\n",
                                pActionList->u.outPort);
                    }
                }
            }
        }
    }

    return;
}

int OfcDumpFlowFields (tOfcFlowEntry *pFlowEntry)
{
    struct list_head *pList2 = NULL;
    struct list_head *pList3 = NULL;
    tMatchListEntry  *pMatchList = NULL;
    tOfcInstrList    *pInstrList = NULL;
    tOfcActionList   *pActionList = NULL;
    __u8             aNullMacAddr[OFC_MAC_ADDR_LEN];
    __u8             index = 0;

    if (pFlowEntry == NULL)
    {
        printk (KERN_CRIT "[%s]: pFlowEntry NULL\r\n", __func__);
        return OFC_FAILURE;
    }

    printk (KERN_INFO "cookie.hi:0x%x, cookie.lo:0x%x\r\n", 
            pFlowEntry->cookie.hi, pFlowEntry->cookie.lo);
    printk (KERN_INFO "tableId:%d\r\n", pFlowEntry->tableId);
    printk (KERN_INFO "idleTimeout:%d, hardTimeout:%d\r\n",
            pFlowEntry->idleTimeout, pFlowEntry->hardTimeout);
    printk (KERN_INFO "priority:%d\r\n", pFlowEntry->priority);
    printk (KERN_INFO "bufId:0x%x\r\n", pFlowEntry->bufId);
    printk (KERN_INFO "outPort:0x%x, outGrp:0x%x, flags:%d\r\n", 
            pFlowEntry->outPort, pFlowEntry->outGrp, 
            pFlowEntry->flags);

    printk (KERN_INFO "MatchList:\r\n");
    list_for_each (pList2, &pFlowEntry->matchList)
    {
        pMatchList = (tMatchListEntry *) pList2;
        printk (KERN_INFO "field:%d, length:%d\r\n",
                pMatchList->field, pMatchList->length);
        for (index = 0; index < pMatchList->length;
             index++)
        {
            printk (KERN_INFO "value[%d]:0x%x\r\n",  
                    index, pMatchList->aValue[index]);
        }
    }

    printk (KERN_INFO "Match Fields:\r\n");
    if (memcmp (pFlowEntry->matchFields.aDstMacAddr,
                aNullMacAddr, OFC_MAC_ADDR_LEN))
    {
        for (index = 0; index < OFC_MAC_ADDR_LEN; index++)
        {
            printk (KERN_INFO "dst[%d]:0x%x\r\n", index,
                    pFlowEntry->matchFields.aDstMacAddr[index]);
        }
    }
    if (memcmp (pFlowEntry->matchFields.aSrcMacAddr,
                aNullMacAddr, OFC_MAC_ADDR_LEN))
    {
        for (index = 0; index < OFC_MAC_ADDR_LEN; index++)
        {
            printk (KERN_INFO "src[%d]:0x%x\r\n", index,
                    pFlowEntry->matchFields.aSrcMacAddr[index]);
        }
    }
    if (pFlowEntry->matchFields.vlanId != 0)
    {
        printk (KERN_INFO "vlanId:%d\r\n",
                pFlowEntry->matchFields.vlanId);
    }
    if (pFlowEntry->matchFields.etherType != 0)
    {
        printk (KERN_INFO "etherType:0x%x\r\n",
                pFlowEntry->matchFields.etherType);
    }
    if (pFlowEntry->matchFields.srcIpAddr != 0)
    {
        printk (KERN_INFO "srcIpAddr:%u\r\n",
                pFlowEntry->matchFields.srcIpAddr);
    }
    if (pFlowEntry->matchFields.dstIpAddr != 0)
    {
        printk (KERN_INFO "dstIpAddr:%u\r\n",
                pFlowEntry->matchFields.dstIpAddr);
    }
    if (pFlowEntry->matchFields.protocolType != 0)
    {
        printk (KERN_INFO "protocolType:0x%x\r\n",
                pFlowEntry->matchFields.protocolType);
    }
    if (pFlowEntry->matchFields.srcPortNum != 0)
    {
        printk (KERN_INFO "srcPortNum:%d\r\n",
                pFlowEntry->matchFields.srcPortNum);
    }
    if (pFlowEntry->matchFields.dstPortNum != 0)
    {
        printk (KERN_INFO "dstPortNum:%d\r\n",
                pFlowEntry->matchFields.dstPortNum);
    }
    if (pFlowEntry->matchFields.l4HeaderType != 0)
    {
        printk (KERN_INFO "l4HeaderType:%d\r\n",
                pFlowEntry->matchFields.l4HeaderType);
    }
    if (pFlowEntry->matchFields.arpFlds.targetIpAddr 
        != 0)
    {
        printk (KERN_INFO "arpFlds.targetIpAddr:%u\r\n",
                pFlowEntry->matchFields.arpFlds.targetIpAddr);
    }

    printk (KERN_INFO "Instruction List:\r\n");
    list_for_each (pList2, &pFlowEntry->instrList)
    {
        pInstrList = (tOfcInstrList *) pList2;
        printk (KERN_INFO "type:%d\r\n", pInstrList->instrType);
        if (pInstrList->instrType == OFCIT_GOTO_TABLE)
        {
            printk (KERN_INFO "GotoTableId:%d\r\n", 
                    pInstrList->u.tableId);
        }
        if ((pInstrList->instrType == OFCIT_WRITE_ACTIONS) ||
            (pInstrList->instrType == OFCIT_APPLY_ACTIONS))
        {
            printk (KERN_INFO "Action List:\r\n");
            list_for_each (pList3, &pInstrList->u.actionList)
            {
                pActionList = (tOfcActionList *) pList3;
                printk (KERN_INFO "type:%d\r\n",
                        pActionList->actionType);
                if (pActionList->actionType == OFCAT_OUTPUT)
                {
                    printk (KERN_INFO "outputPort:0x%x\r\n",
                            pActionList->u.outPort);
                }
            }
        }
    }
}

/******************************************************************
* Function: OfcDpCreateSocketsForDataPkts
*
* Description: This function creates raw sockets on OpenFlow
*              interfaces to transmit and receive data packets
*
* Input: None 
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpCreateSocketsForDataPkts (void)
{
    struct net_device    *dev = NULL;
    struct sockaddr_ll   socketBindAddr;
    struct socket        *socket = NULL;
    int                  dataIfNum = 0;

    for (dataIfNum = 0; dataIfNum < gNumOpenFlowIf; dataIfNum++)
    {
        if ((sock_create (AF_PACKET, SOCK_RAW, htons(ETH_P_ALL), 
             &socket)) < 0)
        {
            printk (KERN_CRIT "Failed to open data socket!!\r\n");
            return OFC_FAILURE;
        }

        dev = OfcGetNetDevByName (gpOpenFlowIf[dataIfNum]);
        if (dev == NULL)
        {
            printk (KERN_CRIT "Device not found!!\r\n");
            return OFC_FAILURE;
        }
        printk (KERN_INFO "dev->ifindex:%d\r\n", dev->ifindex);

        memset (&socketBindAddr, 0, sizeof(socketBindAddr));
        socketBindAddr.sll_family = AF_PACKET;
        socketBindAddr.sll_protocol = htons(ETH_P_ALL);
        socketBindAddr.sll_ifindex = dev->ifindex;

        if (socket->ops->bind (socket, (struct sockaddr *) &socketBindAddr, 
                               sizeof(socketBindAddr)) < 0)
        {
            printk (KERN_CRIT "Failed to bind data socket!!\r\n");
            return OFC_FAILURE;
        }

        gOfcDpGlobals.aDataSocket[dataIfNum] = socket;
        dev = NULL;
        socket = NULL;
    }

    return OFC_SUCCESS;
}

/******************************************************************
* Function: OfcDpCreateThreadsForRxDataPkts
*
* Description: This function creates threads for receiving data
*              packets on raw sockets 
*
* Input: None 
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpCreateThreadsForRxDataPkts (void)
{
    struct task_struct *pThread = NULL;
    char               threadName[OFC_MAX_THREAD_NAME_LEN];
    int                dataIfNum = 0;
    
    for (dataIfNum = 0; dataIfNum < gNumOpenFlowIf; dataIfNum++)
    {
        memset (threadName, 0, sizeof (threadName));
        sprintf (threadName, "%s%d", OFC_RX_DATA_PKT_TH_NAME, 
                                     dataIfNum + 1);
        pThread = kthread_run (OfcDpRxDataPktThread, 
                               (void *) &dataIfNum, threadName);
        if (!pThread)
        {
            printk (KERN_CRIT "Failed to create Rx Data Packet "
                    "thread for dataIfNum:%d\r\n", dataIfNum);
            return OFC_FAILURE;
        }

        gOfcDpGlobals.aDataPktRxThread[dataIfNum] = pThread;
        pThread = NULL;
        msleep (OFC_TASK_SPAWN_GAP);
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcConvertStringToIp
*
* Description: This function converts IP address in string format
*              into 4 byte integer format
*
* Input: pString - IP address in string format
*
* Output: pIpAddr - IP address in integer format
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcConvertStringToIp (char *pString, unsigned int *pIpAddr)
{
    char          octet[4];
    int           index = 0;
    int           stringIndex = 0;
    int           tempIpAddr = 0;
    unsigned int  ipAddr = 0;
    int           dotCount = 0;

    *pIpAddr = 0;
    memset (octet, 0, sizeof(octet));

    /* Parse through the string and when '.' is encountered,
     * convert the characters before the '.' into integers.
     * Subsequently, add this integer in the corresponding octet of
     * the integer representation */
    for (stringIndex = 0; stringIndex <= strlen(pString); stringIndex++)
    {
        if ((pString[stringIndex] != '.') && 
            (pString[stringIndex] != '\0'))
        {
            octet[index++] = pString[stringIndex];
            continue;
        }

        dotCount++;
        kstrtol (octet, 10, (long int *) &tempIpAddr);
        if ((tempIpAddr < 0) || (tempIpAddr > 0xFF))
        {
            return OFC_FAILURE;
        }
        ipAddr |= (tempIpAddr & 0xFF) << (32 - (dotCount * 8));
        memset (octet, 0, sizeof(octet)); 
        index = 0;
        tempIpAddr = 0;
    }

    if (dotCount != 4) /* '\0' is also increasing dotCount */
    {
        return OFC_FAILURE;
    }

    *pIpAddr = ipAddr;
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpCreateCntrlSocket
*
* Description: This function creates TCP socket to communicate
*              with SDN controller
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpCreateCntrlSocket (void)
{
    struct sockaddr_in   serverAddr;
    struct socket        *socket = NULL;

    if ((sock_create (AF_INET, SOCK_STREAM, 0, &socket)) < 0)
    {
        printk (KERN_CRIT "Failed to open TCP socket!!\r\n");
        return OFC_FAILURE;
    }

    memset (&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl (gCntrlIpAddr);
    if (gCntrlPortNo == 0)
    {
        /* set default OpenFlow port number */
        gCntrlPortNo = OFC_DEF_CNTRL_PORT_NUM;
    }
    serverAddr.sin_port = htons (gCntrlPortNo);

    if (socket->ops->connect (socket, (struct sockaddr *) &serverAddr, 
                              sizeof(serverAddr), 0) < 0)
    {
        printk (KERN_CRIT "Failed to connect to controller!!\r\n");
        return OFC_FAILURE;
    }

    gOfcCpGlobals.pCntrlSocket = socket;

#if 0
    /* Send Hello packet to controller */
    OfcCpSendHelloPacket (htonl (OFC_INIT_TRANSACTION_ID));
#endif

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpRcvDataPktFromSock
*
* Description: This function receives data packets from OpenFlow
*              interfaces raw socket in data path task
*
* Input: dataIfNum - OpenFlow interface number
*
* Output: ppPkt - Pointer to data packet
*         pPktLen - Length of data packet
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
__u32 OfcDpRcvDataPktFromSock (__u8 dataIfNum, __u8 **ppPkt, 
                               __u32 *pPktLen)
{
    struct msghdr msg;
    struct iovec  iov;
    mm_segment_t  old_fs;
    __u32         msgLen = 0;
    __u8          *pDataPkt = NULL;

    pDataPkt = (__u8 *) kmalloc (OFC_MAX_PKT_SIZE, GFP_KERNEL);
    if (pDataPkt == NULL)
    {
        printk (KERN_CRIT "Failed to allocate memory to data " 
                          "packet!!\r\n");
        return OFC_FAILURE;
    }

    memset (&msg, 0, sizeof(msg));
    memset (&iov, 0, sizeof(iov));
    memset (pDataPkt, 0, OFC_MAX_PKT_SIZE);
    iov.iov_base = pDataPkt;
    iov.iov_len = OFC_MAX_PKT_SIZE;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    msgLen = sock_recvmsg (gOfcDpGlobals.aDataSocket[dataIfNum], 
                           &msg, OFC_MAX_PKT_SIZE, 0);
    set_fs(old_fs);
    if (msgLen == 0)
    {
        printk (KERN_CRIT "Failed to receive message from data " 
                          "socket!!\r\n");
        kfree (pDataPkt);
        pDataPkt = NULL;
        return OFC_FAILURE;
    }

    *ppPkt = pDataPkt;
    *pPktLen = msgLen;

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpSendDataPktOnSock
*
* Description: This function sends data packets on OpenFlow
*              interfaces raw socket in data path task
*
* Input: dataIfNum - OpenFlow interface number
*        pPkt - Pointer to data packet
*        pktLen - Length of data packet
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpSendDataPktOnSock (__u8 dataIfNum, __u8 *pPkt, 
                            __u32 pktLen)
{
    struct msghdr msg;
    struct iovec  iov;
    mm_segment_t  old_fs;
    __u32         msgLen = 0;

    memset (&msg, 0, sizeof(msg));
    memset (&iov, 0, sizeof(iov));
    iov.iov_base = pPkt;
    iov.iov_len = pktLen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    msgLen = sock_sendmsg (gOfcDpGlobals.aDataSocket[dataIfNum],
                           &msg, pktLen);
    set_fs(old_fs);
    if (msgLen == 0)
    {
        printk (KERN_CRIT "Failed to send message from data "
                          "socket!!\r\n");
        return OFC_FAILURE;
    }

    printk (KERN_INFO "Packet Tx from data socket " 
            "(dataIfNum:%d)\r\n", dataIfNum);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpRecvCntrlPktOnSock
*
* Description: This function receives OpenFlow control packets
*              on tcp socket from controller in control path task
*
* Input: None
*
* Output: ppPkt - Pointer to control packet
*         pPktLen - Pointer to length of control packet
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpRecvCntrlPktOnSock (__u8 **ppPkt, __u16 *pPktLen)
{
    struct msghdr msg;
    struct iovec  iov;
    mm_segment_t  old_fs;
    __u8          *pCntrlPkt = NULL;
    int           msgLen = 0;

    pCntrlPkt = (__u8 *) kmalloc (OFC_MTU_SIZE, GFP_KERNEL);
    if (pCntrlPkt == NULL)
    {
        printk (KERN_CRIT "Failed to allocate memory to " 
                          "control packet\r\n");
        return OFC_FAILURE;
    }
    memset (&msg, 0, sizeof(msg));
    memset (&iov, 0, sizeof(iov));
    memset (pCntrlPkt, 0, OFC_MTU_SIZE);

    iov.iov_base = pCntrlPkt;
    iov.iov_len = OFC_MTU_SIZE;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    msgLen = sock_recvmsg (gOfcCpGlobals.pCntrlSocket, &msg,
                           OFC_MTU_SIZE, 0);
    set_fs(old_fs);

    if (msgLen < OFC_OPENFLOW_HDR_LEN)
    {
        printk (KERN_CRIT "Failed to receive control packet\r\n");
        kfree (pCntrlPkt);
        pCntrlPkt = NULL;
        return OFC_FAILURE;
    }

    *ppPkt = pCntrlPkt;
    *pPktLen = msgLen;
    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCpSendCntrlPktFromSock
*
* Description: This function sends OpenFlow control packets
*              on tcp socket to controller in control path task
*
* Input: pPkt - Pointer to control packet
*        pktLen - Length of control packet
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCpSendCntrlPktFromSock (__u8 *pPkt, __u32 pktLen)
{
    struct msghdr msg;
    struct iovec  iov;
    mm_segment_t  old_fs;
    int           msgLen = 0;

    memset (&msg, 0, sizeof(msg));
    memset (&iov, 0, sizeof(iov));
    iov.iov_base = pPkt;
    iov.iov_len = pktLen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    msgLen = sock_sendmsg (gOfcCpGlobals.pCntrlSocket, &msg,
                           pktLen);
    set_fs(old_fs);

    if (msgLen == 0)
    {
        printk (KERN_CRIT "Failed to send message from control "
                          "socket!!\r\n");
        return OFC_FAILURE;
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpGetFlowTableEntry
*
* Description: This function fetches flow table structure from
*              the list of flow tables
*
* Input: tableId - Flow table ID
*
* Output: None
*
* Returns: Pointer to flow table or NULL
*
*******************************************************************/
tOfcFlowTable *OfcDpGetFlowTableEntry (__u8 tableId)
{
    tOfcFlowTable     *pFlowTable = NULL;
    struct list_head  *pListHead = NULL;
    struct list_head  *pList = NULL;

    pListHead = &gOfcDpGlobals.flowTableListHead;
    list_for_each (pList, pListHead)
    {
        pFlowTable = (tOfcFlowTable *) pList;
        if (pFlowTable->tableId == tableId)
        {
            return pFlowTable;
        }
    }

    return NULL;
}

/******************************************************************                                                                          
* Function: OfcDpExtractPktHdrs
*
* Description: This function extracts packet headers for matching
*              flow table entries
*
* Input: pPkt - Pointer to packet
*        pktLen - Length of packet
*        inPort - Ingress port
*
* Output: pktMatchFields - Extracted packet headers
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpExtractPktHdrs (__u8 *pPkt, __u32 pktLen, __u8 inPort, 
                         tOfcMatchFields *pPktMatchFields)
{
    __u32   pktOffset = 0;
    __u16   vlanTpid = 0;
    __u16   vlanId = 0;
    __u8    ipHdrLen = 0;

    /* Port n in switch corresponds to port n+1 for controller */
    pPktMatchFields->inPort = inPort + 1;

    /* Extract destination MAC address */
    memcpy (pPktMatchFields->aDstMacAddr, pPkt, OFC_MAC_ADDR_LEN);
    pktOffset += OFC_MAC_ADDR_LEN;

    /* Extract source MAC address */
    memcpy (pPktMatchFields->aSrcMacAddr, pPkt + pktOffset,
            OFC_MAC_ADDR_LEN);
    pktOffset += OFC_MAC_ADDR_LEN;

    /* Extract Vlan Id (if present) */
    vlanTpid = htons (OFC_VLAN_TPID);
    if (!memcmp ((pPkt + pktOffset), &vlanTpid, sizeof(vlanTpid)))
    {
        pktOffset += sizeof(vlanTpid);
        memcpy (&vlanId, pPkt + pktOffset, sizeof(vlanId));
        pPktMatchFields->vlanId = ntohs (vlanId) & 0xFFF;
        pktOffset += sizeof(vlanId);
    }

    /* Extract EthType */
    memcpy (&pPktMatchFields->etherType, pPkt + pktOffset, 
            sizeof (pPktMatchFields->etherType));
    pPktMatchFields->etherType = ntohs (pPktMatchFields->etherType);
    pktOffset += sizeof (pPktMatchFields->etherType);

    if (pPktMatchFields->etherType == OFC_ARP_ETHTYPE)
    {
        /* Extract ARP specific fields */
        /* Extract target IP address */
        memcpy (&pPktMatchFields->arpFlds.targetIpAddr,
                pPkt + pktOffset + OFC_ARP_TRGT_IP_ADDR_OFFSET, 
                sizeof (__u32));
        pPktMatchFields->arpFlds.targetIpAddr =
            ntohl (pPktMatchFields->arpFlds.targetIpAddr);

        return OFC_SUCCESS;
    }

    if (pPktMatchFields->etherType != OFC_IP_ETHTYPE)
    {
        return OFC_SUCCESS;
    }

    /* Fetch IP header length */
    memcpy (&ipHdrLen, pPkt + pktOffset, sizeof (ipHdrLen));
    ipHdrLen = (ipHdrLen & 0xF) * 4;

    /* Extract protocol type */
    memcpy (&pPktMatchFields->protocolType, 
            pPkt + pktOffset + OFC_IP_PROT_TYPE_OFFSET,
            sizeof (pPktMatchFields->protocolType));
#if 0
    pPktMatchFields->protocolType = ntohs (pPktMatchFields->protocolType);
#endif
    
    /* Extract source IP address */
    memcpy (&pPktMatchFields->srcIpAddr,
            pPkt + pktOffset + OFC_IP_SRC_IP_OFFSET,
            sizeof (pPktMatchFields->srcIpAddr));
    pPktMatchFields->srcIpAddr = ntohl (pPktMatchFields->srcIpAddr);

    /* Extract destination IP address */
    memcpy (&pPktMatchFields->dstIpAddr, 
            pPkt + pktOffset + OFC_IP_DST_IP_OFFSET,
            sizeof (pPktMatchFields->dstIpAddr));
    pPktMatchFields->dstIpAddr = ntohl (pPktMatchFields->dstIpAddr);
    pktOffset += ipHdrLen;

    if ((pPktMatchFields->protocolType != OFC_TCP_PROT_TYPE) &&
        (pPktMatchFields->protocolType != OFC_UDP_PROT_TYPE))
    {
        return OFC_SUCCESS;
    }

    /* Extract L4 source port number */
    memcpy (&pPktMatchFields->srcPortNum, pPkt + pktOffset,
            sizeof(pPktMatchFields->srcPortNum));
    pPktMatchFields->srcPortNum = ntohs (pPktMatchFields->srcPortNum);
    pktOffset += sizeof(pPktMatchFields->srcPortNum);
  
    /* Extract L4 destination port number */
    memcpy (&pPktMatchFields->dstPortNum, pPkt + pktOffset,
            sizeof(pPktMatchFields->dstPortNum));
    pPktMatchFields->dstPortNum = ntohs (pPktMatchFields->dstPortNum);
    pktOffset += sizeof(pPktMatchFields->dstPortNum);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDeleteList
*
* Description: This function deletes the linked list and frees
*              memory of each member in the list
*
* Input: pListHead - Head of linked list
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDeleteList (struct list_head *pListHead)
{
    struct list_head  *pList = NULL;
 
    if (pListHead == NULL)
    {
        return OFC_FAILURE;
    }

    list_for_each (pList, pListHead)
    {
        list_del_init (pList);
        kfree (pList);
        pList = pListHead;
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcCalcHdrOffset
*
* Description: This function calculates header field offset
*              from the packet.
*
* Input: pPkt - Pointer to packet
*        pktLen - Length of packet
*        hdrField - Header field for packet offset
*
* Output: pPktOffset - Pointer to packet header offset
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcCalcHdrOffset (__u8 *pPkt, __u16 pktLen, __u8 hdrField, 
                      __u16 *pPktOffset)
{
    __u16   parsedLen = 0;
    __u16   etherType = 0;
    __u8    ipHdrLen = 0;
    __u8    ipProtType = 0;
    
    /* Destination MAC address offset */
    if (hdrField == OFCXMT_OFB_ETH_DST)
    {
        *pPktOffset = 0;
        return OFC_SUCCESS;
    }
    parsedLen += OFC_MAC_ADDR_LEN;

    /* Source MAC address offset */
    if (hdrField == OFCXMT_OFB_ETH_SRC)
    {
        *pPktOffset = parsedLen;
        return OFC_SUCCESS;
    }
    parsedLen += OFC_MAC_ADDR_LEN;

    memcpy (&etherType, pPkt + parsedLen, sizeof (etherType));
    etherType = ntohs (etherType);
    parsedLen += sizeof (etherType);

    if (hdrField == OFCXMT_OFB_VLAN_VID)
    {
        /* VLAN tag not present in packet */
        if (etherType != OFC_VLAN_TPID)
        {
            return OFC_FAILURE;
        }

        *pPktOffset = parsedLen;
        return OFC_SUCCESS;
    }

    if (etherType == OFC_VLAN_TPID)
    {
        parsedLen += sizeof (etherType);
        memcpy (&etherType, pPkt + parsedLen, sizeof (etherType));
        etherType = ntohs (etherType);
        parsedLen += sizeof (etherType);
    }

    /* Check for various EtherTypes */

    /* All non-IP fields must have been checked before this
     * Now check only IP and higher layer related fields */
    if (etherType != OFC_IP_ETHTYPE)
    {
        return OFC_FAILURE;
    }

    if ((hdrField == OFCXMT_OFB_IPV4_SRC) ||
        (hdrField == OFCXMT_OFB_IPV4_DST))
    {
        *pPktOffset = parsedLen;
        return OFC_SUCCESS;
    }

    /* Fetch IP header length */
    memcpy (&ipHdrLen, pPkt + parsedLen, sizeof (ipHdrLen));
    ipHdrLen = (ipHdrLen & 0xF) * 4;

    /* Fetch protocol type */
    memcpy (&ipProtType,
            pPkt + parsedLen + OFC_IP_PROT_TYPE_OFFSET,
            sizeof (ipProtType));

    parsedLen += ipHdrLen;
    /* Check for various L3 protocols */

    /* All non-layer 4 protocols must have been checked
     * before this */
    if ((hdrField == OFCXMT_OFB_TCP_SRC) ||
        (hdrField == OFCXMT_OFB_TCP_DST) ||
        (hdrField == OFCXMT_OFB_UDP_SRC) ||
        (hdrField == OFCXMT_OFB_UDP_DST))
    {
        if ((ipProtType != OFC_TCP_PROT_TYPE) &&
            (ipProtType != OFC_UDP_PROT_TYPE))
        {
            return OFC_FAILURE;
        }

        *pPktOffset = parsedLen;
        return OFC_SUCCESS;
    }

    return OFC_FAILURE;
}
