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
    __u16   u2Type;              
    __u16   u2Length;
    __u8    aOxmFields[4];
} tOfcMatch;

#endif /* __OFC_PKT_H__ */
