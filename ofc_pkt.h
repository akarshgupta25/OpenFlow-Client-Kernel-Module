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
    __u16  type;
    __u16  length;
} tOfcInstrTlv;

typedef struct
{
    __u16  type;
    __u16  length;
} tOfcActionTlv;

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
} tOfcFeatReply;

typedef struct 
{
    tOfcEightByte cookie;
    tOfcEightByte cookieMask;
    __u8          tableId;
    __u8          command;
    __u16         idleTimeout;
    __u16         hardTimeout;
    __u16         priority;
    __u32         bufId;
    __u32         outPort;
    __u32         outGrp;
    __u16         flags;
    __u8          aPad[2];
    tOfcMatchTlv  OfpMatch;
    tOfcInstrTlv  OfpInstr[0];
} tOfcFlowModHdr;

#endif /* __OFC_PKT_H__ */
