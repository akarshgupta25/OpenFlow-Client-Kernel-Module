/********************************************************************
*
* Filename: ofc_pkt.h
*
* Description: This file contains structures for OpenFlow control
*              packet headers
*
*******************************************************************/

#ifndef __OFC_PKT_H__
#define __OFC_PKT_H__

typedef struct
{    
    __u32   hi;     
    __u32   lo;                    
} tOfcEightByte;

typedef struct
{
    tOfcEightByte datapathId;
    __u32         numBuffers;
    __u8          numTables;
    __u8          auxId;
    __u8          aPad[2];
    __u32         flowStats:1;
    __u32         tableStats:1;
    __u32         portStats:1;
    __u32         grpStats:1;
    __u32         notSupp1:1;
    __u32         ipReasm:1;
    __u32         queueStats:1;
    __u32         notSupp2:1;
    __u32         portBlocked:1;
    __u32         reserved;
} tOfcFeatRply;

typedef struct
{
    __u8    version; 
    __u8    type;
    __u16   length;
    __u32   xid;
} tOfcOfHdr;

typedef struct 
{
    __u16   type;              
    __u16   length;
    __u8    aPad[4];
} tOfcMatchTlv;

typedef struct
{
    __u16   Class;
    __u8    field;
    __u8    length;
    __u8    aValue[0];
} tOfcMatchOxmTlv;

typedef struct
{
    __u32         bufId;
    __u16         totLength;
    __u8          reason;
    __u8          tableId;
    tOfcEightByte cookie;
    tOfcMatchTlv  matchTlv;
} tOfcPktInHdr;

typedef struct
{
    __u8       impDatapathId[2];
    __u8       macDatapathId[OFC_MAC_ADDR_LEN];
    __u32      maxBuffers;
    __u8       maxTables;
    __u8       auxilaryId;
    __u8       pad[2];
    __u32      capabilities;
    __u32      reserved;
} tOfcCpFeatReply;

typedef struct
{
    __u16  type;
    __u16  flags;
    __u8   pad[4];
} tOfcCpMultipartHeader;

typedef struct
{
    char mfcDesc[OFC_DESCR_STRING_LEN];
    char hwDesc[OFC_DESCR_STRING_LEN];
    char swDesc[OFC_DESCR_STRING_LEN];
    char serialNum[OFC_SERIAL_NUM_LEN];
    char dpDesc[OFC_DESCR_STRING_LEN];
} tOfcMultipartDesc;

typedef struct 
{
    uint16_t type;
    uint16_t length;
} tOfcMatch;

typedef struct
{
    uint8_t table_id;
    uint8_t pad[3];
    uint32_t out_port;
    uint32_t out_group;
    uint8_t pad2[4];
    uint64_t cookie;
    uint64_t cookie_mask;
    tOfcMatch match;
} tOfcMultipartFlowStats;

typedef struct
{
    uint16_t length;
    uint8_t table_id;
    uint8_t pad;
    uint32_t duration_sec;
    uint32_t duration_nsec;
    uint16_t priority;
    uint16_t idle_timeout;
    uint16_t hard_timeout;
    uint16_t flags;
    uint8_t pad2[4];
    uint64_t cookie;
    uint64_t packet_count;
    uint64_t byte_count;
} tOfcMultiPartFlowStatsReply;

#endif /* __OFC_PKT_H__ */
