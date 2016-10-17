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
    /* init_MUTEX (&gOfcDpGlobals.semId); */
    sema_init (&gOfcDpGlobals.semId, 1);
    sema_init (&gOfcDpGlobals.dataPktQSemId, 1);

    /* Initialize queue for interfaces on which data packet is Rx */
    INIT_LIST_HEAD (&gOfcDpGlobals.pktRxIfListHead);

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
    struct msghdr msg;
    struct iovec  iov;
    mm_segment_t  old_fs;
    char          dataPkt[OFC_MTU_SIZE];
    int           msgLen = 0;
    int           dataIfNum = 0;

    down_interruptible (&gOfcDpGlobals.dataPktQSemId);

    while ((pMsgQ = OfcDpRecvFromDataPktQ()) != NULL)
    {
        dataIfNum = pMsgQ->dataIfNum;
        printk (KERN_INFO "pMsgQ->dataIfNum:%d\r\n", pMsgQ->dataIfNum);
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
        OfcDumpPacket (dataPkt, msgLen);

        /* Release message */
        kfree (pMsgQ);
        pMsgQ = NULL;
    }

    up (&gOfcDpGlobals.dataPktQSemId);

    return OFC_SUCCESS;
}
