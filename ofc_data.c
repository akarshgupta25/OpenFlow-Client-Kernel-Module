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
extern int  gNumOpenFlowIf;

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

    /* Create threads for receiving data packets from raw sockets
     * and posting them to data path task */
    if (OfcDpCreateThreadsForRxDataPkts() != OFC_SUCCESS)
    {
        printk (KERN_CRIT "Data packet thread failed!!\r\n");
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
                OfcDpRxControlPathMsg();
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
    __u8          *pDataPkt = NULL;
    __u32         pktLen = 0;
    __u8          dataIfNum = 0;

    down_interruptible (&gOfcDpGlobals.dataPktQSemId);

    while ((pMsgQ = OfcDpRecvFromDataPktQ()) != NULL)
    {
        dataIfNum = pMsgQ->dataIfNum;
        pDataPkt = pMsgQ->pDataPkt;
        pktLen = pMsgQ->dataPktLen;

        if (pDataPkt == NULL)
        {
            printk (KERN_CRIT "NULL packet received by data path"
                              " task\r\n");
            kfree (pMsgQ);
            pMsgQ = NULL;
            continue;
        }
 
        /* Process packet using OpenFlow Pipeline */
        OfcDpProcessPktOpenFlowPipeline (pDataPkt, pktLen, dataIfNum);

        /* Release message */
        kfree (pDataPkt);
        pDataPkt = NULL;
        kfree (pMsgQ);
        pMsgQ = NULL;
    }

    up (&gOfcDpGlobals.dataPktQSemId);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpRxControlPathMsg
*
* Description: This function receives messages sent by control
*              path task
*
* Input: None
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpRxControlPathMsg (void)
{
    tDpCpMsgQ      *pMsgQ = NULL;
    tOfcFlowEntry  *pFlowEntry = NULL;

    down_interruptible (&gOfcDpGlobals.cpMsgQSemId);

    while ((pMsgQ = OfcDpRecvFromCpMsgQ()) != NULL)
    {
        switch (pMsgQ->msgType)
        {
            case OFC_FLOW_MOD_ADD:
                pFlowEntry = pMsgQ->pFlowEntry;
                if (pFlowEntry == NULL)
                {
                    printk (KERN_CRIT "Data path did not receive flow " 
                                      "entry from control path\r\n");
                    kfree (pMsgQ);
                    pMsgQ = NULL;
                    continue;
                }

                OfcDpInsertFlowEntry (pFlowEntry);
                OfcDumpFlows(0);
                break;

            case OFC_FLOW_MOD_DEL:
                pFlowEntry = pMsgQ->pFlowEntry;
                if (pFlowEntry == NULL)
                {   
                    printk (KERN_CRIT "Data path did not receive flow "
                                      "entry from control path\r\n");
                    kfree (pMsgQ);
                    pMsgQ = NULL;
                    continue;
                }

                OfcDpDeleteFlowEntry (pFlowEntry);
                OfcDumpFlows(0);
                break;

            case OFC_PACKET_OUT:
                OfcDpExecPktOutActions (pMsgQ->pPkt, pMsgQ->pktLen,
                                        pMsgQ->pActionListHead);
                break;

            default:
                printk (KERN_CRIT "Invalid message received from "
                                  "control path task\r\n");
                break;
        }

        /* Release message */
        kfree (pMsgQ);
        pMsgQ = NULL;
    }

    up (&gOfcDpGlobals.cpMsgQSemId);

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpRxDataPktThread
*
* Description: This function receives data packets from raw
*              sockets and posts event to data path task.
*
* Input: args - Pointer to OpenFlow interface number
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpRxDataPktThread (void *args)
{
    __u8    *pDataPkt = NULL;
    __u32   pktLen = 0;
    int     dataIfNum = 0;

    dataIfNum = *((int *) args);
    printk (KERN_INFO "dataIfNum:%d, spawned\r\n", dataIfNum);

    while (1)
    {

        if (OfcDpRcvDataPktFromSock (dataIfNum, &pDataPkt, &pktLen)
            != OFC_SUCCESS)
        {
            pDataPkt = NULL;
            pktLen = 0;
            continue;
        }

        printk (KERN_INFO "dataIfNum:%d, Data Packet Rx\r\n", 
                dataIfNum);

        down_interruptible (&gOfcDpGlobals.dataPktQSemId);
        OfcDpSendToDataPktQ (dataIfNum, pDataPkt, pktLen);
        up (&gOfcDpGlobals.dataPktQSemId);
        OfcDpSendEvent (OFC_PKT_RX_EVENT);

        pDataPkt = NULL;
        pktLen = 0;
    }

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
    tOfcFlowTable  *pFlowTable = NULL;
    tOfcFlowEntry  *pTableMissFlow = NULL;
    tOfcInstrList  *pInstr = NULL;
    tOfcActionList *pActions = NULL;
    int           flowTableNum = 0;

    INIT_LIST_HEAD (&gOfcDpGlobals.flowTableListHead);
    for (flowTableNum = OFC_FIRST_TABLE_INDEX; 
         flowTableNum < OFC_MAX_FLOW_TABLES; flowTableNum++)
    {
        pFlowTable = (tOfcFlowTable *) kmalloc (sizeof(tOfcFlowTable),
                                                GFP_KERNEL);
        if (pFlowTable == NULL)
        {
            printk (KERN_CRIT "Failed to allocate memory to " 
                              "flow tables!!\r\n");
            return OFC_FAILURE;
        }

        /* Initialize flow tables */
        memset (pFlowTable, 0, sizeof(tOfcFlowTable));
        INIT_LIST_HEAD (&pFlowTable->flowEntryList);
        pFlowTable->tableId = flowTableNum;
        pFlowTable->maxEntries = OFC_MAX_FLOW_ENTRIES;

        /* Add to flow table list */
        INIT_LIST_HEAD (&pFlowTable->list);
        list_add_tail (&pFlowTable->list, 
                       &gOfcDpGlobals.flowTableListHead);

        /* Add table-miss flow */
        pTableMissFlow = 
            (tOfcFlowEntry *) kmalloc (sizeof(tOfcFlowEntry), GFP_KERNEL);
        if (pTableMissFlow == NULL)
        {
            printk (KERN_CRIT "Failed to allocate memory to "
                              "table-miss flow!!\r\n");
            return OFC_FAILURE;
        }
        memset (pTableMissFlow, 0, sizeof(tOfcFlowEntry));
        INIT_LIST_HEAD (&pTableMissFlow->list);
        pTableMissFlow->priority = OFC_MIN_FLOW_PRIORITY;
        pTableMissFlow->tableId = pFlowTable->tableId;
        pTableMissFlow->bufId = OFC_NO_BUFFER;
        INIT_LIST_HEAD (&pTableMissFlow->matchList);
        INIT_LIST_HEAD (&pTableMissFlow->instrList);

        /* No match list, therefore all packets would match */
        /* Instruction list: Apply Action */
        pInstr = (tOfcInstrList *) kmalloc (sizeof(tOfcInstrList),
                                            GFP_KERNEL);
        if (pInstr == NULL)
        {
            printk (KERN_CRIT "Failed to allocate memory to "
                              "instruction list!!\r\n");
            return OFC_FAILURE;
        }
        memset (pInstr, 0, sizeof(tOfcInstrList));
        INIT_LIST_HEAD (&pInstr->list);
        INIT_LIST_HEAD (&pInstr->u.actionList);
        pInstr->instrType = OFCIT_APPLY_ACTIONS;
        list_add_tail (&pInstr->list, &pTableMissFlow->instrList);
        
        /* Action: output port:controller */
        pActions = 
            (tOfcActionList *) kmalloc (sizeof(tOfcActionList), 
                                        GFP_KERNEL);
        if (pActions == NULL)
        {
            printk (KERN_CRIT "Failed to allocate memory to "
                              "action list!!\r\n");
            return OFC_FAILURE;
        }
        memset (pActions, 0, sizeof(tOfcActionList));
        INIT_LIST_HEAD (&pActions->list);
        pActions->actionType = OFCAT_OUTPUT;
        pActions->u.outPort = OFPP_CONTROLLER;
        list_add_tail (&pActions->list, &pInstr->u.actionList);

        list_add_tail (&pTableMissFlow->list,
                       &pFlowTable->flowEntryList);
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpProcessPktOpenFlowPipeline
*
* Description: This function processes packet via OpenFlow
*              processing pipeline
*
* Input: pPkt - Pointer to data packet
*        pktLen - Packet length
*        inPort - Input port
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpProcessPktOpenFlowPipeline (__u8 *pPkt, __u32 pktLen, 
                                     __u8 inPort)
{
    tOfcFlowTable   *pFlowTable = NULL;
    tOfcFlowEntry   *pMatchFlow = NULL;
    tDpCpMsgQ       msgQ;
    tOfcMatchFields pktMatchFields;
    __u8            *pDataPkt = NULL;
    __u8            maxOutPorts = OFC_MAX_FLOW_TABLES * 
                                  (gNumOpenFlowIf + OFPP_NUM);
    __u32           aOutPortList[maxOutPorts];
    __u32           outPort = 0;
    __u8            numOutPorts = 0;
    __u8            portIndex = 0;
    __u8            tableId = 0;
    __u8            isTableMiss = OFC_FALSE;
    __u8            dataIfNum = 0;

    printk (KERN_INFO "Processing data packet...\r\n");

    memset (aOutPortList, 0, sizeof(aOutPortList));
    memset (&pktMatchFields, 0, sizeof(pktMatchFields));

    /* Extract packet headers to match flow */
    OfcDpExtractPktHdrs (pPkt, pktLen, inPort, &pktMatchFields);

    /* Start processing with flows in table 0 */
    tableId = OFC_FIRST_TABLE_INDEX;
    while (tableId < OFC_MAX_FLOW_TABLES)
    {
        pFlowTable = OfcDpGetFlowTableEntry (tableId);
        if (pFlowTable == NULL)
        {
            printk (KERN_CRIT "Failed to fetch first flow table\r\n");
            return OFC_FAILURE;
        }

        /* Update flow table lookup count for statistics */
        pFlowTable->lookupCount++;

        /* Get best match flow */
        pMatchFlow = OfcDpGetBestMatchFlow (pktMatchFields,
                                            &pFlowTable->flowEntryList,
                                            &isTableMiss);
        if (pMatchFlow == NULL)
        {
            printk (KERN_CRIT "Failed to fetch best match " 
                              "flow entry\r\n");
            return OFC_FAILURE;
        }

        /* Update flow statistics */
        pFlowTable->matchCount++;
        pMatchFlow->pktMatchCount++;
        pMatchFlow->byteMatchCount += pktLen;

        /* This will be updated during instruction execution */
        /* If instruction is not GOTO_TABLE then loop will terminate */
        tableId = OFC_MAX_FLOW_TABLES;

        /* Execute flow instruction */
        if (OfcDpExecuteFlowInstr (pPkt, pktLen, inPort, 
                                   &pMatchFlow->instrList, 
                                   &tableId, aOutPortList, &numOutPorts) 
            != OFC_SUCCESS)
        {
            printk (KERN_CRIT "Failed to execute flow instruction\r\n ");
            return OFC_FAILURE;
        }
    }

    /* Apply action list (TODO) */

    /* Send packet to output ports */
    for (portIndex = 0; portIndex < numOutPorts; portIndex++)
    {
        outPort = aOutPortList[portIndex];

        /* TODO: Support OFPP_NORMAL || OFPP_LOCAL?? */
        /* TODO: Support OFPP_FLOOD */
        if ((outPort == OFPP_NORMAL) || 
            (outPort == OFPP_LOCAL) ||
            (outPort == OFPP_FLOOD))
        {
            printk (KERN_INFO "[%s]: OutPort:0x%x not supported\r\n",
                    __func__, outPort);
            continue;
        }

        if (outPort == OFPP_CONTROLLER)
        {
            /* Send packet-in to controller */
            /* This is done by sending the packet to control
             * path task */
            pDataPkt = (__u8 *) kmalloc (pktLen, GFP_KERNEL);
            if (pDataPkt == NULL)
            {
                printk (KERN_CRIT "[%s]: Failed to allocate memory to "
                                  "data packet\r\n", __func__);
                continue;
            }
            memset (pDataPkt, 0, pktLen);
            memcpy (pDataPkt, pPkt, pktLen);
            memset (&msgQ, 0, sizeof(msgQ));

            msgQ.pPkt = pDataPkt;
            msgQ.pktLen = pktLen;
            /* Port n in switch corresponds to port n+1 for controller */
            msgQ.inPort = inPort + 1;
            msgQ.msgType = (isTableMiss == OFC_TRUE) ? OFCR_NO_MATCH :
                                                       OFCR_ACTION;
            msgQ.tableId = pMatchFlow->tableId;
            msgQ.pFlowEntry = pMatchFlow;
            if (isTableMiss == OFC_TRUE)
            {
                msgQ.cookie.hi = 0xFFFFFFFF;
                msgQ.cookie.lo = 0xFFFFFFFF;
            }
            else
            {
                memcpy (&msgQ.cookie, &pMatchFlow->cookie, 
                        sizeof(msgQ.cookie));
            }

            OfcDpSendToCpQ (&msgQ);
            OfcCpSendEvent (OFC_DP_TO_CP_EVENT);
        }
        else if (outPort == OFPP_ALL)
        {
            /* Send packet through all OpenFlow ports */
            for (dataIfNum = 0; dataIfNum < gNumOpenFlowIf; 
                 dataIfNum++)
            {
                if (dataIfNum == inPort)
                {
                    /* Do not send packet through input port */
                    continue;
                }
                OfcDpSendDataPktOnSock (dataIfNum, pPkt, pktLen);
            }
        }
        else if (outPort == OFPP_IN_PORT)
        {
            OfcDpSendDataPktOnSock (inPort, pPkt, pktLen);
        }
        else
        {
            /* Output port n corresponds to dataIfNum n-1 */
            OfcDpSendDataPktOnSock (outPort - 1, pPkt, pktLen);
        }
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpGetBestMatchFlow
*
* Description: This function matches flow table entries and returns
*              the best matching flow entry or table-miss flow
*
* Input: pktMatchFields - Packet header fields
*        pFlowEntryList - Pointer to flow table entry list head
*
* Output: pIsTableMiss - Whether table-miss occurred
*
* Returns: Pointer to best match flow entry
*
*******************************************************************/
tOfcFlowEntry *OfcDpGetBestMatchFlow (tOfcMatchFields pktMatchFields,
                                      struct list_head *pFlowEntryList,
                                      __u8 *pIsTableMiss)
{
    struct list_head *pList = NULL;
    tOfcFlowEntry    *pFlowEntry = NULL;
    tOfcFlowEntry    *pBestMatchFlow = NULL;
    tOfcMatchFields  tableMissEntry;
    __u8             aNullMacAddr[OFC_MAC_ADDR_LEN];

    memset (aNullMacAddr, 0, sizeof(aNullMacAddr));
    memset (&tableMissEntry, 0, sizeof(tableMissEntry));
    *pIsTableMiss = OFC_FALSE;

    list_for_each (pList, pFlowEntryList)
    {
        pFlowEntry = (tOfcFlowEntry *) pList;
        /* Match fields for this flow entry */

        if (memcmp (pFlowEntry->matchFields.aDstMacAddr, 
                    aNullMacAddr, OFC_MAC_ADDR_LEN))
        {
            /* Match destination MAC address */
            if (memcmp (pFlowEntry->matchFields.aDstMacAddr,
                        pktMatchFields.aDstMacAddr, OFC_MAC_ADDR_LEN))
            {
                continue;
            }
        }

        if (memcmp (pFlowEntry->matchFields.aSrcMacAddr,
                    aNullMacAddr, OFC_MAC_ADDR_LEN))
        {
            /* Match source MAC address */
            if (memcmp (pFlowEntry->matchFields.aSrcMacAddr,
                        pktMatchFields.aSrcMacAddr, OFC_MAC_ADDR_LEN))
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.vlanId != 0)
        {
            /* Match VLAN ID */
            if (pFlowEntry->matchFields.vlanId != 
                pktMatchFields.vlanId)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.etherType != 0)
        {
            /* Match EtherType */
            if (pFlowEntry->matchFields.etherType !=
                pktMatchFields.etherType)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.srcIpAddr != 0)
        {
            /* Match source IP address */
            if (pFlowEntry->matchFields.srcIpAddr !=
                pktMatchFields.srcIpAddr)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.dstIpAddr != 0)
        {
            /* Match destination IP address */
            if (pFlowEntry->matchFields.dstIpAddr !=
                pktMatchFields.dstIpAddr)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.protocolType != 0)
        {
            /* Match IP protocol type */
            if (pFlowEntry->matchFields.protocolType !=
                pktMatchFields.protocolType)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.srcPortNum != 0)
        {
            /* Check for L4 protocol type */
            if (pFlowEntry->matchFields.l4HeaderType != 
                pktMatchFields.protocolType)
            {
                continue;
            }
            /* Match L4 source port number */
            if (pFlowEntry->matchFields.srcPortNum !=
                pktMatchFields.srcPortNum)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.dstPortNum != 0)
        {
            /* Check for L4 protocol type */
            if (pFlowEntry->matchFields.l4HeaderType != 
                pktMatchFields.protocolType)
            {
                continue;
            }
            /* Match L4 destination port number */
            if (pFlowEntry->matchFields.dstPortNum !=
                pktMatchFields.dstPortNum)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.arpFlds.targetIpAddr != 0)
        {
            /* Match ARP target IP address */
            if (pFlowEntry->matchFields.arpFlds.targetIpAddr !=
                pktMatchFields.arpFlds.targetIpAddr)
            {
                continue;
            }
        }

        if (pFlowEntry->matchFields.inPort != 0)
        {
            /* Match packet input port */
            if (pFlowEntry->matchFields.inPort !=
                pktMatchFields.inPort)
            {
                continue;
            }
        }

#if 0
        /* Flow matched!! Determine whether this is best match */
        if ((pBestMatchFlow == NULL) ||
            (pFlowEntry->priority > pBestMatchFlow->priority))
        {
            pBestMatchFlow = pFlowEntry;
        }
#endif
        /* Flow matched!! This is the highest priority matched 
         * flow!! */
        pBestMatchFlow = pFlowEntry;
        break;
    }

    /* Check whether the best match flow is table-miss flow */
    if (!memcmp (&pBestMatchFlow->matchFields, &tableMissEntry, 
                 sizeof(tOfcMatchFields)))
    {
        /* Table-miss occurred */
        *pIsTableMiss = OFC_TRUE;
    }

    return pBestMatchFlow;
}

/******************************************************************                                                                          
* Function: OfcDpExecuteFlowInstr
*
* Description: This function processes instruction of matching
*              flow entry
*
* Input: pPkt - Pointer to data packet
*        pktLen - Packet length
*        inPort - Input port
*        pInstr - Pointer to instruction list
*
* Output: pTableId - Pointer to new table Id
*         pOutPortList - Pointer to output port list
*         pNumOutPorts - Number of ports in output port list
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpExecuteFlowInstr (__u8 *pPkt, __u32 pktLen, __u8 inPort,
                           struct list_head *pInstrList, __u8 *pTableId, 
                           __u32 *pOutPortList, __u8 *pNumOutPorts)
{
    tOfcInstrList    *pInstr = NULL;
    struct list_head *pActions = NULL;
    struct list_head *pList = NULL;

    list_for_each (pList, pInstrList)
    {
        pInstr = (tOfcInstrList *) pList;
        switch (pInstr->instrType)
        {
            case OFCIT_APPLY_ACTIONS:
                pActions = &pInstr->u.actionList;
                OfcDpApplyInstrActions (pPkt, pktLen, inPort,
                                        pActions, pOutPortList,
                                        pNumOutPorts);
                break;

            /* TODO: These instructions */
            case OFCIT_CLEAR_ACTIONS:
                break;

            case OFCIT_WRITE_ACTIONS:
                break;

            case OFCIT_GOTO_TABLE:
                *pTableId = pInstr->u.tableId;
                break;

            default:
                return OFC_FAILURE;
        }
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpApplyInstrActions
*
* Description: This function applies the actions list specified in
*              flow entry instructions
*
* Input: pPkt - Pointer to data packet
*        pktLen - Packet length
*        inPort - Input port
*        pActions - Pointer to action list
*
* Output: pOutPortList - Pointer to output port list
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpApplyInstrActions (__u8 *pPkt, __u32 pktLen, __u8 inPort,
                            struct list_head *pActionsList,
                            __u32 *pOutPortList, __u8 *pNumOutPorts)
{
    tOfcActionList   *pActions = NULL;
    struct list_head *pList = NULL;
    __u8             numPorts = *pNumOutPorts;

    list_for_each (pList, pActionsList)
    {
        pActions = (tOfcActionList *) pList;
        switch (pActions->actionType)
        {
            case OFCAT_OUTPUT:
                pOutPortList[numPorts++] = pActions->u.outPort;
                *pNumOutPorts = numPorts;
                break;

            /* TODO: Set Fields action */
            default:
                printk (KERN_CRIT "Unsupported action!!\r\n");
                return OFC_FAILURE;
        }
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpInsertFlowEntry
*
* Description: This function inserts flow entry in flow table
*
* Input: pFlowEntry - Pointer to flow entry
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpInsertFlowEntry (tOfcFlowEntry *pFlowEntry)
{
    tOfcFlowTable     *pFlowTable = NULL;
    tOfcFlowEntry     *pFlowEntryParser = NULL;
    struct list_head  *pList = NULL;
    __u8              tableId = 0;

    tableId = pFlowEntry->tableId;
    pFlowTable = OfcDpGetFlowTableEntry (tableId);
    if (pFlowTable == NULL)
    {
        printk (KERN_CRIT "Invalid flow table Id in flow entry\r\n");
        kfree (pFlowEntry);
        pFlowEntry = NULL;
        return OFC_FAILURE;
    }

    /* Entries are inserted in decreasing order of priorities i.e.
     * flow entries with greater priorities are inserted ahead of
     * flow entries with lower priorities. If two flows have
     * equal priority then the new flow is inserted ahead of the
     * older flow */
    list_for_each (pList, &pFlowTable->flowEntryList)
    {
        pFlowEntryParser = (tOfcFlowEntry *) pList;

        if (pFlowEntry->priority < pFlowEntryParser->priority)
        {
            continue;
        }

        printk (KERN_INFO "Inserting entry in flow table\r\n");
        /* Insert new flow before this flow entry */
        INIT_LIST_HEAD (&pFlowEntry->list);
        list_add_tail (&pFlowEntry->list, &pFlowEntryParser->list);
        break;
    }

    return OFC_SUCCESS;
}

/******************************************************************                                                                          
* Function: OfcDpDeleteFlowEntry
*
* Description: This function deletes flow entry from flow table
*
* Input: pFlowEntry - Pointer to flow entry
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpDeleteFlowEntry (tOfcFlowEntry *pFlowEntry)
{
    tOfcFlowTable     *pFlowTable = NULL;
    tOfcFlowEntry     *pFlowEntryParser = NULL;
    tOfcInstrList     *pInstrList = NULL;
    struct list_head  *pList = NULL;
    struct list_head  *pList2 = NULL;
    __u8              tableId = 0;

    tableId = pFlowEntry->tableId;
    pFlowTable = OfcDpGetFlowTableEntry (tableId);
    if (pFlowTable == NULL)
    {
        printk (KERN_CRIT "Invalid flow table Id in flow entry\r\n");
        list_for_each (pList2, &pFlowEntry->instrList)
        {
            pInstrList = (tOfcInstrList *) pList2;
            OfcDeleteList (&pInstrList->u.actionList);
        }  
        OfcDeleteList (&pFlowEntry->instrList);
        OfcDeleteList (&pFlowEntry->matchList);
        kfree (pFlowEntry);
        pFlowEntry = NULL;
        return OFC_FAILURE;
    }

#if 0
    printk (KERN_INFO "Dumping pFlowEntry:\r\n");
    OfcDumpFlowFields (pFlowEntry);
    printk (KERN_INFO "\r\n");
#endif
    list_for_each (pList, &pFlowTable->flowEntryList)
    {
        pFlowEntryParser = (tOfcFlowEntry *) pList;

#if 0
        printk (KERN_INFO "Dumping pFlowEntryParser:\r\n");
        OfcDumpFlowFields (pFlowEntryParser);
        printk (KERN_INFO "\r\n");
#endif

        if (pFlowEntryParser->hardTimeout != 
            pFlowEntry->hardTimeout)
        {
            continue;
        }
        if (pFlowEntryParser->idleTimeout !=
            pFlowEntry->idleTimeout)
        {
            continue;
        }

        if (memcmp (&pFlowEntryParser->cookie, &pFlowEntry->cookie,
                    sizeof (pFlowEntry->cookie)))
        {
            continue;
        }
        if (memcmp (&pFlowEntryParser->cookieMask,
                    &pFlowEntry->cookieMask,
                    sizeof (pFlowEntry->cookieMask)))
        {
            continue;
        }

        if (pFlowEntryParser->flags != pFlowEntry->flags)
        {
            continue;
        }

        if (pFlowEntryParser->priority != pFlowEntry->priority)
        {
            continue;
        }

        if (pFlowEntryParser->bufId != pFlowEntry->bufId)
        {
            continue;
        }
        if (pFlowEntryParser->outPort != pFlowEntry->outPort)
        {
            continue;
        }
        if (pFlowEntryParser->outGrp != pFlowEntry->outGrp)
        {
            continue;
        }

        if (pFlowEntryParser->tableId != pFlowEntry->tableId)
        {
            continue;
        }

        if (memcmp (&pFlowEntryParser->matchFields,
                    &pFlowEntry->matchFields,
                    sizeof (pFlowEntry->matchFields)))
        {
            continue;
        }

        /* TODO: Match instruction list for deletion as well */

        printk (KERN_INFO "Deleting entry from flow table\r\n");

        /* Flow entry found, delete it */
        list_del_init (pList);
        list_for_each (pList2, 
                       &pFlowEntryParser->instrList)
        {
            pInstrList = (tOfcInstrList *) pList2;
            OfcDeleteList (&pInstrList->u.actionList);
        }
        OfcDeleteList (&pFlowEntryParser->instrList);
        OfcDeleteList (&pFlowEntryParser->matchList);
        kfree (pFlowEntryParser);
        pFlowEntryParser = NULL;
        break;
    }

    list_for_each (pList2, &pFlowEntry->instrList)
    {
        pInstrList = (tOfcInstrList *) pList2;
        OfcDeleteList (&pInstrList->u.actionList);
    }   
    OfcDeleteList (&pFlowEntry->instrList);
    OfcDeleteList (&pFlowEntry->matchList);
    kfree (pFlowEntry);
    pFlowEntry = NULL;

    return OFC_SUCCESS;
}
/******************************************************************                                                                          
* Function: OfcDpExecPktOutActions
*
* Description: This function executes actions specified in the
*              packet-out message
*
* Input: pPkt - Pointer to data packet
*        pktLen - Length of packet
*        pActionsListHead - Actions list head
*
* Output: None
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcDpExecPktOutActions (__u8 *pPkt, __u16 pktLen,
                            struct list_head *pActionsListHead)
{
    __u8    maxOutPorts = OFC_MAX_FLOW_TABLES *
                          (gNumOpenFlowIf + OFPP_NUM);
    __u32   outPort = 0;
    __u32   aOutPortList[maxOutPorts];
    __u8    numOutPorts = 0;
    __u8    portIndex = 0;
    __u8    dataIfNum = 0;

    printk (KERN_INFO "Packet-Out Rx from control path " 
                      "task\r\n");

    if ((pPkt == NULL) || (pActionsListHead == NULL))
    {
        printk (KERN_CRIT "[%s]: Invalid inputs\r\n", __func__);
        return OFC_FAILURE;
    }

    memset (aOutPortList, 0, sizeof (aOutPortList));
    if (OfcDpApplyInstrActions (pPkt, pktLen, 0,
                                pActionsListHead, aOutPortList,
                                &numOutPorts)
        != OFC_SUCCESS)
    {
        OfcDeleteList (pActionsListHead);
        kfree (pActionsListHead);
        pActionsListHead = NULL;
        kfree (pPkt);
        pPkt = NULL;
        return OFC_FAILURE;
    }

    for (portIndex = 0; portIndex < numOutPorts; portIndex++)
    {
        outPort = aOutPortList[portIndex];
        printk (KERN_INFO "outPort:0x%x\r\n", outPort);
        if ((outPort == OFPP_CONTROLLER) || 
            (outPort == OFPP_NORMAL) || 
            (outPort == OFPP_LOCAL) ||
            (outPort == OFPP_FLOOD) ||
            (outPort == OFPP_IN_PORT))
        {
            continue;
        }

        if (outPort == OFPP_ALL)
        {
            /* Send packet through all OpenFlow ports */
            for (dataIfNum = 0; dataIfNum < gNumOpenFlowIf; 
                 dataIfNum++)
            {
                OfcDpSendDataPktOnSock (dataIfNum, pPkt, pktLen);
            }
        }
        else
        {
            /* Output port n corresponds to dataIfNum n-1 */
            OfcDpSendDataPktOnSock (outPort - 1, pPkt, pktLen);
        }
    }
    
    OfcDeleteList (pActionsListHead);
    kfree (pActionsListHead);
    pActionsListHead = NULL;
    kfree (pPkt);
    pPkt = NULL;
    return OFC_SUCCESS;
}
