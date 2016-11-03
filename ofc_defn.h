/********************************************************************
*
* Filename: ofc_defn.h
*
* Description: This file contains the macro definitions that are
*              used in OpenFlow client kernel module.
*
*******************************************************************/

#ifndef __OFC_DEFN_H__
#define __OFC_DEFN_H__

#define MOD_AUTHOR "SDN Flow Classification in OpenFlow Client Kernel Module"
#define MOD_DESC "This module implements OpenFlow protocol in linux kernel"

#define OFC_SUCCESS 0
#define OFC_FAILURE 1
#define OFC_FALSE   0
#define OFC_TRUE    1

#define OFC_DP_TASK_NAME       "OpenFlowDataPath"
#define OFC_CP_TASK_NAME       "OpenFlowControlPath"
#define OFC_MAX_OF_IF_NUM      10
#define OFC_MAX_IFNAME_LEN     10
#define OFC_MAX_DATA_SOCK      10
#define OFC_MTU_SIZE           1500
#define OFC_L2_HDR_LEN         18
#define OFC_MAX_PKT_SIZE       (OFC_MTU_SIZE + OFC_L2_HDR_LEN) /* Check if CRC is required */
#define OFC_DEF_CNTRL_PORT_NUM 6633

/* NOTE: If events are added or removed, update OFC_MAX_EVENTS */
#define OFC_PKT_RX_EVENT   0x00001
#define OFC_CTRL_PKT_EVENT 0x00002
#define OFC_DP_TO_CP_EVENT 0X00004
#define OFC_CP_TO_DP_EVENT 0x00008
#define OFC_MAX_EVENTS     4

#define OFC_MAX_FLOW_TABLES   2
#define OFC_MAX_FLOW_ENTRIES  20
#define OFC_MIN_FLOW_PRIORITY 1
#define OFC_FIRST_TABLE_INDEX 0

#define OFC_MAC_ADDR_LEN    6

#endif /* __OFC_DEFN_H__ */
