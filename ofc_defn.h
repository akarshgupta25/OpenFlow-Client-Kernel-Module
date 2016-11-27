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

#define OFC_DP_TASK_NAME        "OpenFlowDataPath"
#define OFC_CP_TASK_NAME        "OpenFlowControlPath"
#define OFC_MAX_OF_IF_NUM       10
#define OFC_MAX_IFNAME_LEN      16
#define OFC_MAX_THREAD_NAME_LEN 35
#define OFC_RX_DATA_PKT_TH_NAME "OpenFlowRxDataPktThread"
#define OFC_MAX_DATA_SOCK       10
#define OFC_MTU_SIZE            1500
#define OFC_L2_HDR_LEN          18
#define OFC_MAX_PKT_SIZE        (OFC_MTU_SIZE + OFC_L2_HDR_LEN) /* Check if CRC is required */
#define OFC_DEF_CNTRL_PORT_NUM  6633
#define OFC_TASK_SPAWN_GAP      1000 /* 1 second */

/* NOTE: If events are added or removed, update OFC_MAX_EVENTS */
#define OFC_PKT_RX_EVENT   0x00001
#define OFC_CTRL_PKT_EVENT 0x00002
#define OFC_DP_TO_CP_EVENT 0X00004
#define OFC_CP_TO_DP_EVENT 0x00008
#define OFC_MAX_EVENTS     4

#define OFC_MAX_FLOW_TABLES   2
#define OFC_MAX_FLOW_ENTRIES  20
#define OFC_MIN_FLOW_PRIORITY 0
#define OFC_FIRST_TABLE_INDEX 0

#define OFC_MAC_ADDR_LEN             6
#define OFC_IP_PROT_TYPE_OFFSET      9
#define OFC_IP_SRC_IP_OFFSET         12
#define OFC_IP_DST_IP_OFFSET         16
#define OFC_MATCH_TLV_OFFSET         40
#define OFC_HEADER_OFFSET_FROM_IP    52
#define OFC_ARP_SRC_IP_ADDR_OFFSET   14
#define OFC_ARP_TRGT_IP_ADDR_OFFSET  24

#define OFC_ARP_ETHTYPE    0x0806
#define OFC_IP_ETHTYPE     0x0800
#define OFC_VLAN_TPID      0x8100
#define OFC_TCP_PROT_TYPE  0x06
#define OFC_UDP_PROT_TYPE  0x11

#define OFC_OPENFLOW_HDR_LEN     8
#define OFC_VERSION              0x04
#define OFC_INIT_TRANSACTION_ID  0x01
#define OFC_NO_BUFFER            0xffffffff
#define OFC_MAX_PKT_BUFFER       0x111
#define OFC_CTRL_MAIN_CONNECTION 0x0

#define OFC_DESCR_STRING_LEN     256
#define OFC_SERIAL_NUM_LEN       32
#define OFC_HW_DESC              "Test Hardware"
#define OFC_SW_DESC              "OpenFlow 1.3 Version"
#define OFC_MNF_DESC             "Test Manufacturer"
#define OFC_SERIAL_NUM           "11 11 11 11 11 11"
#define OFC_DATAPATH_DESC        "Test OpenFlow Switch"

#endif /* __OFC_DEFN_H__ */
