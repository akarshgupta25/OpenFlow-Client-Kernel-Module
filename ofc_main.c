/********************************************************************
*
* Filename: ofc_main.c
*
* Description: This file contains the module intialization code
*              that is executed when OpenFlow client kernel module
*              is inserted or removed.
*
*******************************************************************/

#include "ofc_hdrs.h"

/* Global variable used by OpenFlow kernel module */
tOfcGlobals gOfcGlobals;

/* Command line arguments */
/* OpenFlow interfaces */
char *gpOpenFlowIf[OFC_MAX_OF_IF_NUM];
int  gNumOpenFlowIf = 0;
module_param_array (gpOpenFlowIf, charp, &gNumOpenFlowIf, 0);

/* SDN Controller IP address */
char *gpServerIpAddr = NULL;
module_param (gpServerIpAddr, charp, 0);

/* SDN Controller port number (optional) */
unsigned short gCntrlPortNo = 0;
module_param (gCntrlPortNo, short, 0);

/* SDN Controller IP address in integer format */
unsigned int gCntrlIpAddr = 0;

extern tOfcDpGlobals gOfcDpGlobals;
extern tOfcCpGlobals gOfcCpGlobals;


unsigned int OfcNetFilterPreRouteHook (const struct nf_hook_ops *ops,
                                       struct sk_buff *skb,
                                       const struct net_device *in,
                                       const struct net_device *out,
                                       int (*okfn)(struct sk_buff*))
{
    struct iphdr  *iph = NULL;
    int           dataIfNum = 0;

    /* Check whether data packet has been received */
    for (dataIfNum = 0; dataIfNum < gNumOpenFlowIf; dataIfNum++)
    {
        if (!strcmp (in->name, gpOpenFlowIf[dataIfNum]))
        {
            /* Packet rx on OpenFlow interface, notify kernel 
             * that the packet need not be processed */
            return NF_STOLEN;
        }
    }

    /* Check whether control packet has been recevied  */
    if (gOfcCpGlobals.isModInit == OFC_TRUE)
    {
        iph = ip_hdr (skb);
        if ((iph != NULL) && 
            (gCntrlIpAddr == (ntohl (iph->saddr))))
        {
            /* Packet received from controller. Send event
             * to control path task only if it is an
             * OpenFlow packet i.e. TCP packet consisting
             * of some payload. The control path task
             * will validate whether payload is OpenFlow
             * packet or not */
            if (ntohs (iph->tot_len) != 
                OFC_HEADER_OFFSET_FROM_IP)
            {
                OfcCpSendEvent (OFC_CTRL_PKT_EVENT);
            }
        }
    }
    
    return NF_ACCEPT;
}

static int OfcMainInit (void)
{
    /* Register pre-routing hook with netfilter */
    gOfcGlobals.netFilterOps.hook     = OfcNetFilterPreRouteHook;
    gOfcGlobals.netFilterOps.pf       = PF_INET;        
    gOfcGlobals.netFilterOps.hooknum  = NF_INET_PRE_ROUTING;
    gOfcGlobals.netFilterOps.priority = NF_IP_PRI_FIRST;
    nf_register_hook (&gOfcGlobals.netFilterOps);

    return OFC_SUCCESS;
}

static int __init OpenFlowClientStart (void)
{
    /* Command line arguments missing */
    if ((gpServerIpAddr == NULL) || (gNumOpenFlowIf == 0))
    {
        printk (KERN_CRIT "Missing command line arguments!!\r\n");
        return OFC_FAILURE;
    }
    if (gNumOpenFlowIf > OFC_MAX_OF_IF_NUM)
    {
        printk (KERN_CRIT "Too many OpenFlow interfaces!!\r\n");
        return OFC_FAILURE;
    }
    if (OfcConvertStringToIp (gpServerIpAddr, &gCntrlIpAddr) 
        != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Invalid controller IP address!!\r\n");
        return OFC_FAILURE;
    }

    /* Initialize kernel module */
    if (OfcMainInit() != OFC_SUCCESS)
    {   
        printk (KERN_CRIT "Kernel module initialization failed!!\r\n");
        return OFC_FAILURE;
    }

    /* Spawn OpenFlow data path task */
    gOfcGlobals.pOfcDpThread = kthread_run (OfcDpMainTask, NULL, 
                                            OFC_DP_TASK_NAME);
    if (!gOfcGlobals.pOfcDpThread)
    {
        printk (KERN_CRIT "Error creating OpenFlow data " 
                          "path task!!\r\n");
        kthread_stop (gOfcGlobals.pOfcCpThread);
	    return OFC_FAILURE;
    }

    /* Wait for data path task to be spawned */
    msleep ((gNumOpenFlowIf - 1) * OFC_TASK_SPAWN_GAP);

    /* Spawn OpenFlow control path task */
    gOfcGlobals.pOfcCpThread = kthread_run (OfcCpMainTask, NULL,
                                            OFC_CP_TASK_NAME);
    if (!gOfcGlobals.pOfcCpThread)
    {
        printk (KERN_CRIT "Error creating OpenFlow control" 
                          " path task!!\r\n");
        return OFC_FAILURE;
    }

    return OFC_SUCCESS;
}

static void __exit OpenFlowClientStop (void)
{
    /* TODO: Release all data sockets */
    /* TODO: Stop all data pkt rx threads */
    kthread_stop (gOfcGlobals.pOfcDpThread);
    sock_release (gOfcCpGlobals.pCntrlSocket);
    kthread_stop (gOfcGlobals.pOfcCpThread);
    nf_unregister_hook (&gOfcGlobals.netFilterOps);
    printk (KERN_INFO "Openflow Client Stopped!!\r\n");
}

module_init (OpenFlowClientStart);
module_exit (OpenFlowClientStop);

MODULE_LICENSE ("Proprietary");
MODULE_AUTHOR (MOD_AUTHOR);
MODULE_DESCRIPTION (MOD_DESC);
