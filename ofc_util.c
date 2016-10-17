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
int OfcDpSendToDataPktQ (int dataIfNum)
{
    tDataPktRxIfQ *pMsgQ = NULL;

    pMsgQ = (tDataPktRxIfQ *) kmalloc (sizeof(tDataPktRxIfQ), 
                                       GFP_KERNEL);
    if (pMsgQ == NULL)
    {
        printk (KERN_CRIT "Queue message allocation failed!!\r\n");
        return OFC_FAILURE;
    }

    INIT_LIST_HEAD (&pMsgQ->list);
    pMsgQ->dataIfNum = dataIfNum;
    list_add_tail (&pMsgQ->list, &gOfcDpGlobals.pktRxIfListHead);

    return OFC_SUCCESS;
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
            printk (KERN_INFO "Device found\r\n");
            return pTempDev;
        }
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
    
    return;
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
            printk (KERN_INFO "Device not found!!\r\n");
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
    return OFC_SUCCESS;
}
