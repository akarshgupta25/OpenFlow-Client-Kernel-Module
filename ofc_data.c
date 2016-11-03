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
    __u8          *pDataPkt = NULL;
    __u32         pktLen = 0;
    __u8          dataIfNum = 0;

    down_interruptible (&gOfcDpGlobals.dataPktQSemId);

    while ((pMsgQ = OfcDpRecvFromDataPktQ()) != NULL)
    {
        dataIfNum = pMsgQ->dataIfNum;
        if (OfcDpRcvDataPktFromSock (dataIfNum, &pDataPkt, &pktLen)
            != OFC_SUCCESS)
        {
            continue;
        }

        /* Process packet using OpenFlow Pipeline */
        OfcDpProcessPktOpenFlowPipeline (pDataPkt, pktLen, dataIfNum);

#if 0
        OfcDumpPacket (pDataPkt, pktLen);
        /* Send packet to control path task */
        OfcDpSendToCpQ (pDataPkt, pktLen);
        OfcCpSendEvent (OFC_DP_TO_CP_EVENT);
#endif

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
    tOfcInstrList   *pInstr = NULL;
    tOfcActionList  *pActions = NULL;
    __u8            outPortList[gNumOpenFlowIf];
    __u8            tableId = 0;

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
        if (OfcExecFlowInstr (pPkt, pktLen, inPort, 
                              &pMatchFlow->instrList, 
                              &tableId, outPortList) 
            != OFC_SUCCESS)
        {
            printk (KERN_CRIT "Failed to execute flow instruction\r\n ");
            return OFC_FAILURE;
        }
    }

    return OFC_SUCCESS;
}
/******************************************************************                                                                          
* Function: OfcExecFlowInstr
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
          pOutPortList - Pointer to output port list
*
* Returns: OFC_SUCCESS/OFC_FAILURE
*
*******************************************************************/
int OfcExecFlowInstr (__u8 *pPkt, __u32 pktLen, __u8 inPort,
                      struct list_head *pInstrList, __u8 *pTableId, 
                      __u8 *pOutPortList)
{
    tOfcInstrList    *pInstr = NULL;
    tOfcActionList   *pActions = NULL;
    struct list_head *pList = NULL;

    list_for_each (pList, pInstrList)
    {
        pInstr = (tOfcInstrList *) pList;
        switch (pInstr->instrType)
        {
            case OFCIT_APPLY_ACTIONS:
                pActions = &pInstr->u.actionList;
                OfcApplyInstrActions (pPkt, pktLen, inPort,
                                      pOutPortList);
                break;

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
