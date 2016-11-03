/********************************************************************
*
* Filename: ofc_data.c
*
* Description: This file contains the OpenFlow data path sub module.
*              It receives packets from OpenFlow interfaces, and
*              forwards these packets according to flow table
*              programmed by Flow Classification application.
*              It also interfaces with OpenFlow control path task
*              to interact with Flow Classification application.
*
*******************************************************************/

#include "ofc_hdrs.h"

tOfcDpGlobals gOfcDpGlobals;

/******************************************************************                                                                          
* Function: OfcDpMainInit
*
* Description: This function performs the initialization tasks of
*              the data path sub module
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpMainInit (void)
{
    memset (&gOfcDpGlobals, 0, sizeof (gOfcDpGlobals));

    /* Initialize semaphore */
    sema_init (&gOfcDpGlobals.semId, 1);
    sema_init (&gOfcDpGlobals.dataPktQSemId, 1);
    sema_init (&gOfcDpGlobals.cpMsgQSemId, 1);

    /* Initialize lists and queues */
    INIT_LIST_HEAD (&gOfcDpGlobals.pktRxIfListHead);
	INIT_LIST_HEAD (&gOfcDpGlobals.cpMsgListHead);

    if (OfcDpCreateFlowTables() != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Flow table creation failed!!\r\n");
        return OFC_FAILURE;
    }

    /* Create raw sockets to transmit and receive data packets from
       OpenFlow interfaces  */
    if (OfcDpCreateSocketsForDataPkts() != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Data socket creation failed!!\r\n");
        return OFC_FAILURE;
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpMainTask
*
* Description: Main function for OpenFlow data path task
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpMainTask (void *args)
{
    int event = 0;

    /* Initialize memory and structures */
    if (OfcDpMainInit() != OFC_SUCCESS)
    {
        /* Check if thread needs to be killed */
        printk (KERN_CRIT "OpenFlow data path task intialization " 
                          "failed!!\r\n");
        return OFC_FAILURE;
    }

    while (1)
    {
        if (OfcDpReceiveEvent (OFC_PKT_RX_EVENT | OFC_CP_TO_DP_EVENT, 
            &event) == OFC_SUCCESS)
        {
            if (event & OFC_PKT_RX_EVENT)
            {
                /* Receive packet from raw socket */
                OfcDpRxDataPacket();
            }

            if (event & OFC_CP_TO_DP_EVENT)
            {
                /* Process information sent by control path task */
            }
        }
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpRxDataPacket
*
* Description: This function receives data packet from OpenFlow
*              interfaces via raw socket
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpRxDataPacket (void)
{
    tDataPktRxIfQ *pMsgQ = NULL;
#if 0
    struct msghdr msg;
    struct iovec  iov;
    mm_segment_t  old_fs;
    char          dataPkt[OFC_MTU_SIZE];
    int           msgLen = 0;
#endif
    __u8          *pDataPkt = NULL;
    __u32         pktLen = 0;
    __u8          dataIfNum = 0;

    down_interruptible (&gOfcDpGlobals.dataPktQSemId);

    while ((pMsgQ = OfcDpRecvFromDataPktQ()) != NULL)
    {
        dataIfNum = pMsgQ->dataIfNum;
        printk (KERN_INFO "pMsgQ->dataIfNum:%d\r\n", pMsgQ->dataIfNum);
#if 0
        /* Receive data packet from raw socket */
        memset (&msg, 0, sizeof(msg));
        memset (&iov, 0, sizeof(iov));
        memset (dataPkt, 0, sizeof(dataPkt));
        iov.iov_base = dataPkt;
        iov.iov_len = sizeof(dataPkt);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        old_fs = get_fs();
        set_fs(KERNEL_DS);
        msgLen = sock_recvmsg (gOfcDpGlobals.aDataSocket[dataIfNum], 
                               &msg, sizeof(dataPkt), 0);
        set_fs(old_fs);
#endif
        if (OfcDpRcvDataPktFromSock (dataIfNum, &pDataPkt, &pktLen)
            != OFC_SUCCESS)
        {
            continue;
        }
        OfcDumpPacket (pDataPkt, pktLen);

        /* Send packet to control path task */
        OfcDpSendToCpQ (dataPkt, msgLen);
        OfcCpSendEvent (OFC_DP_TO_CP_EVENT);

        /* Release message */
        kfree (pMsgQ);
        pMsgQ = NULL;
    }

    up (&gOfcDpGlobals.dataPktQSemId);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpCreateFlowTables
*
* Description: This function creates flow tables during data path
*              task init
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpCreateFlowTables (void)
{
    tOfcFlowTable *pFlowTable = NULL;
    int           flowTableNum = 0;

    INIT_LIST_HEAD (&gOfcDpGlobals.flowTableListHead);
    for (flowTableNum = 0; flowTableNum < OFC_MAX_FLOW_TABLES;
         flowTableNum++)
    {
        pFlowTable = (tOfcFlowTable *) kmalloc (sizeof(tOfcFlowTable *),
                                                GFP_KERNEL);
        if (pFlowTable == NULL)
        {
            printk (KERN_CRIT "Failed to allocate memory to " 
                              "flow tables!!\r\n");
            return OFC_FAILURE;
        }

        /* Initialize flow tables */
        memset (pFlowTable, 0, sizeof(tOfcFlowTable));
        INIT_LIST_HEAD (&pFlowTable->flowEntryListHead);
        pFlowTable->tableId = flowTableNum;
        pFlowTable->maxEntries = OFC_MAX_FLOW_ENTRIES;

        /* Add to flow table list */
        INIT_LIST_HEAD (&pFlowTable->list);
        list_add_tail (&pFlowTable->list, 
                       &gOfcDpGlobals.flowTableListHead);
    }

    return OFC_SUCCESS;
}

