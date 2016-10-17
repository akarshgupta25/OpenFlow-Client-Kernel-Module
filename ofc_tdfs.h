/********************************************************************
*
* Filename: ofc_tdfs.h
*
* Description: This file contains structures and enums used in
*              OpenFlow client kernel module.
*
*******************************************************************/

#ifndef __OFC_TDFS_H__
#define __OFC_TDFS_H__

/* Common structures */
typedef struct
{
    struct task_struct *pOfcDpThread;
    struct task_struct *pOfcCpThread;
    struct nf_hook_ops netFilterOps;
    int                aDataIfSockFd[OFC_MAX_OF_IF_NUM];
    char               aDataIfName[OFC_MAX_IFNAME_LEN];
    char               aCntrlIfName[OFC_MAX_IFNAME_LEN];
    int                events;
} tOfcGlobals;

/* Data path structures */
typedef struct
{
    struct list_head list;
    int              dataIfNum;
} tDataPktRxIfQ;

typedef struct
{
    struct socket    *aDataSocket[OFC_MAX_OF_IF_NUM];
    struct semaphore semId;
    struct semaphore dataPktQSemId;
    struct list_head pktRxIfListHead; /* Queue for interfaces on which
                                       * data packet is received */
    int              events;
} tOfcDpGlobals;

/* Control path structures */
typedef struct
{
    struct socket    *pCntrlSocket;
    struct semaphore semId;
    struct list_head dpMsgListHead; /* Queue for messages rx from
                                     * data packet */
    int              events;
    int              isModInit;
} tOfcCpGlobals;

/* Function Declarations */
int OfcDpReceiveEvent (int events, int *pRxEvents);
int OfcDpSendEvent (int events);
void OfcDumpPacket (char *au1Packet, int len);
struct net_device *OfcGetNetDevByName (char *pIfName);
int OfcConvertStringToIp (char *pString, unsigned int *pIpAddr);

int OfcDpMainTask (void *args);
int OfcDpCreateSocketsForDataPkts (void);
int OfcDpRxDataPacket (void);
int OfcDpSendToDataPktQ (int dataIfNum);
tDataPktRxIfQ *OfcDpRecvFromDataPktQ (void);

int OfcCpMainTask (void *args);
int OfcCpReceiveEvent (int events, int *pRxEvents);
int OfcCpSendEvent (int events);
int OfcCpCreateCntrlSocket (void);
int OfcCpRxControlPacket (void);

#endif /* __OFC_TDFS_H__ */
