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

    /* Create TCP socket to interact with controller */ 
    if (OfcCpCreateCntrlSocket() != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Data socket creation failed!!\r\n");
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
                printk (KERN_INFO "Call OfcCpRxControlPacket\r\n");
                OfcCpRxControlPacket();
            }

            if (event & OFC_DP_TO_CP_EVENT)
            {
                /* Process information sent by data path task */
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
    struct msghdr msg;
    struct iovec  iov;
    mm_segment_t  old_fs;
    char          cntrlPkt[OFC_MTU_SIZE];
    int           msgLen = 0;

    memset (&msg, 0, sizeof(msg));
    memset (&iov, 0, sizeof(iov));
    memset (cntrlPkt, 0, sizeof(cntrlPkt));
    iov.iov_base = cntrlPkt;
    iov.iov_len = sizeof(cntrlPkt);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    msgLen = sock_recvmsg (gOfcCpGlobals.pCntrlSocket, &msg, 
                           sizeof(cntrlPkt), 0);
    set_fs(old_fs);
    OfcDumpPacket (cntrlPkt, msgLen);

    return OFC_SUCCESS;
}
