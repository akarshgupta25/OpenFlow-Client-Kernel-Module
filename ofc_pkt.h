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
    __u32                  bufId;
    __u16                 totLength;
    __u8                  reason;
    __u8                  tableId;
    tOfcEightByte cookie;
    tOfcMatchTlv          matchTlv;
} tOfcPktInHdr;

#endif /* __OFC_PKT_H__ */
