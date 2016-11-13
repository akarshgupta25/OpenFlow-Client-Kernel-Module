/********************************************************************
*
* Filename: ofc_std.h
*
* Description: This file contains OpenFlow standard enums
*
*******************************************************************/

#ifndef __OFC_STD_H__
#define __OFC_STD_H__

/* Reserved ports */
enum
{
    OFPP_IN_PORT    = 0xfffffff8, /* Deprecated. */
    OFPP_TABLE      = 0xfffffff9, /* OpenFlow Extensible Match */
    OFPP_NORMAL     = 0xfffffffa,
    OFPP_FLOOD      = 0xfffffffb,
    OFPP_ALL        = 0xfffffffc,
    OFPP_CONTROLLER = 0xfffffffd,
    OFPP_LOCAL      = 0xfffffffe,
    OFPP_ANY        = 0xffffffff,
    OFPP_NUM        = 8
};

enum 
{
    OFCXMT_OFB_IN_PORT         = 0,   /* Switch input port */
    OFCXMT_OFB_IN_PHY_PORT     = 1,   /* Switch physical input port */
    OFCXMT_OFB_METADATA        = 2,   /* Metadata passed between tables */
    OFCXMT_OFB_ETH_DST         = 3,   /* Ethernet Destination Address */
    OFCXMT_OFB_ETH_SRC         = 4,   /* Ethernet Source Address */
    OFCXMT_OFB_ETH_TYPE        = 5,   /* Ethernet Type */
    OFCXMT_OFB_VLAN_VID        = 6,   /* VLAN ID */
    OFCXMT_OFB_VLAN_PCP        = 7,   /* VLAN Priority */
    OFCXMT_OFB_IP_DSCP         = 8,   /* IP DSCP (6 bits in ToS field) */
    OFCXMT_OFB_IP_ECN          = 9,   /* IP ECN (2 bits in ToS field) */
    OFCXMT_OFB_IP_PROTO        = 10,  /* IPv4 Protocol */
    OFCXMT_OFB_IPV4_SRC        = 11,  /* IPv4 Source Address */
    OFCXMT_OFB_IPV4_DST        = 12,  /* IPv4 Destination Address */
    OFCXMT_OFB_TCP_SRC         = 13,  /* TCP Source Port */
    OFCXMT_OFB_TCP_DST         = 14,  /* TCP Destination Port */
    OFCXMT_OFB_UDP_SRC         = 15,  /* UDP Source Port */
    OFCXMT_OFB_UDP_DST         = 16,  /* UDP Destination Port */
    OFCXMT_OFB_SCTP_SRC        = 17,  /* SCTP Source Port */
    OFCXMT_OFB_SCTP_DST        = 18,  /* SCTP Destination Port */
    OFCXMT_OFB_ICMPV4_TYPE     = 19,  /* ICMP Type */
    OFCXMT_OFB_ICMPV4_CODE     = 20,  /* ICMP Code */
    OFCXMT_OFB_ARP_OP          = 21,  /* ARP OpCode */
    OFCXMT_OFB_ARP_SPA         = 22,  /* ARP Source IPv4 Address */
    OFCXMT_OFB_ARP_TPA         = 23,  /* ARP Target IPv4 Address */
    OFCXMT_OFB_ARP_SHA         = 24,  /* ARP Source Hardware Address */
    OFCXMT_OFB_ARP_THA         = 25,  /* ARP Destination Hardware Address */
    OFCXMT_OFB_IPV6_SRC        = 26,  /* IPv6 Source Address */
    OFCXMT_OFB_IPV6_DST        = 27,  /* IPv6 Destination Address */
    OFCXMT_OFB_IPV6_FLABEL     = 28,  /* IPv6 Flow Label */
    OFCXMT_OFB_ICMPV6_TYPE     = 29,  /* ICMPv6 Type */
    OFCXMT_OFB_ICMPV6_CODE     = 30,  /* ICMPv6 Code */
    OFCXMT_OFB_IPV6_ND_TARGET  = 31,  /* Target Address for ND */
    OFCXMT_OFB_IPV6_ND_SLL     = 32,  /* Source Link Layer for ND */
    OFCXMT_OFB_IPV6_ND_TLL     = 33,  /* Target Link Layer for ND */
    OFCXMT_OFB_MPLS_LABEL      = 34,  /* MPLS Label */
    OFCXMT_OFB_MPLS_TC         = 35,  /* MPLS TC */
    OFCXMT_OFB_MPLS_BOS        = 36,  /* MPLS BoS bit */
    OFCXMT_OFB_PBB_ISID        = 37,  /* PBB I-SID */
    OFCXMT_OFB_TUNNEL_ID       = 38,  /* Logical Port Metadata */
    OFCXMT_OFB_IPV6_EXTHDR     = 39,  /* IPv6 Extension Header pseudo-field */
    OFCXMT_OFB_MAX             = 40
};

enum 
{
    OFCIT_GOTO_TABLE      = 1,      /* Setup next table in the lookup */
    OFCIT_WRITE_METADATA  = 2,      /* Setup metadata field for later use */
    OFCIT_WRITE_ACTIONS   = 3,      /* Write the action(s) onto the action set */
    OFCIT_APPLY_ACTIONS   = 4,      /* Applies the action(s) immediately */
    OFCIT_CLEAR_ACTIONS   = 5,      /* Clears all actions from the action set */
    OFCIT_METER           = 6,      /* Apply meter (rate limiter) */
    OFCIT_EXPERIMENTER    = 0xFFFF  /* Experimenter instruction */
};

enum 
{
    OFCAT_OUTPUT        = 0,      /* Output to switch port. */
    OFCAT_COPY_TTL_OUT  = 11,     /* Copy TTL "outwards" from next-to-outermost
                                     to outermost */
    OFCAT_COPY_TTL_IN   = 12,     /* Copy TTL "inwards"  from outermost to
                                     next-to-outermost */
    OFCAT_SET_MPLS_TTL  = 15,     /* MPLS TTL */
    OFCAT_DEC_MPLS_TTL  = 16,     /* Decrement MPLS TTL */
    OFCAT_PUSH_VLAN     = 17,     /* Push a new VLAN tag */
    OFCAT_POP_VLAN      = 18,     /* Pop the outer VLAN tag */
    OFCAT_PUSH_MPLS     = 19,     /* Push a new MPLS tag */
    OFCAT_POP_MPLS      = 20,     /* Pop the outer MPLS tag */
    OFCAT_SET_QUEUE     = 21,     /* Set queue id when outputting to a port */
    OFCAT_GROUP         = 22,     /* Apply group. */
    OFCAT_SET_NW_TTL    = 23,     /* IP TTL. */
    OFCAT_DEC_NW_TTL    = 24,     /* Decrement IP TTL */
    OFCAT_SET_FIELD     = 25,     /* Set a header field using OXM TLV format */
    OFCAT_PUSH_PBB      = 26,     /* Push a new PBB service tag (I-TAG) */
    OFCAT_POP_PBB       = 27,     /* Pop the outer PBB service tag (I-TAG) */
    OFCAT_EXPERIMENTER  = 0xFFFF
};

enum 
{
    OFCR_NO_MATCH, /* No matching flow. */
    OFCR_ACTION    /* Action explicitly output to controller. */
};

enum 
{
    OFPMT_STANDARD = 0, /* Deprecated. */
    OFPMT_OXM = 1 /* OpenFlow Extensible Match */
};

enum 
{
    OFPXMC_NXM_0 = 0x0000,
    OFPXMC_NXM_1 = 0x0001,
    OFPXMC_OPENFLOW_BASIC = 0x8000, 
    OFPXMC_EXPERIMENTER = 0xFFFF
};

enum 
{
    OFPT_HELLO, 
    OFPT_ERROR, 
    OFPT_ECHO_REQUEST, 
    OFPT_ECHO_REPLY, 
    OFPT_EXPERIMENTER, 
    OFPT_FEATURES_REQUEST, 
    OFPT_FEATURES_REPLY, 
    OFPT_GET_CONFIG_REQUEST, 
    OFPT_GET_CONFIG_REPLY,
    OFPT_SET_CONFIG,
    OFPT_PACKET_IN,
    OFPT_FLOW_REMOVED,
    OFPT_PORT_STATUS,
    OFPT_PACKET_OUT,
    OFPT_FLOW_MOD,
    OFPT_GROUP_MOD,
    OFPT_PORT_MOD,
    OFPT_TABLE_MOD,
    OFPT_MULTIPART_REQUEST,
    OFPT_MULTIPART_REPLY,
    OFPT_BARRIER_REQUEST,
    OFPT_BARRIER_REPLY,
    OFPT_QUEUE_GET_CONFIG_REQUEST,
    OFPT_QUEUE_GET_CONFIG_REPLY,
    OFPT_ROLE_REQUEST,
    OFPT_ROLE_REPLY,
    OFPT_GET_ASYNC_CONFIG_REQUEST,
    OFPT_GET_ASYNC_CONFIG_REPLY,
    OFPT_SET_ASYNC,
    OFPT_METER_MOD,
    OFPT_MAX_PKT_TYPE
};

enum
{
    OFPC_FLOW_STATS   = 1 << 0,
    OFPC_TABLE_STATS  = 1 << 1,
    OFPC_PORT_STATS   = 1 << 2,
    OFPC_GROUP_STATS  = 1 << 3,
    OFPC_IP_REASM     = 1 << 5,
    OFPC_QUEUE_STATS  = 1 << 6,
    OFPC_PORT_BLOCKED = 1 << 8 
};

// Types expected in the Multipart messages.
enum
{
    OFPMP_DESC            =  0,
    OFPMP_FLOW            =  1,
    OFPMP_AGGREGATE       =  2,
    OFPMP_TABLE           =  3,
    OFPMP_PORT_STATS      =  4,
    OFPMP_QUEUE           =  5,
    OFPMP_GROUP           =  6,
    OFPMP_GROUP_DESC      =  7,
    OFPMP_GROUP_FEATURES  =  8,
    OFPMP_METER           =  9,
    OFPMP_METER_CONFIG    =  10,
    OFPMP_METER_FEATURES  =  11,
    OFPMP_TABLE_FEATURES  =  12,
    OFPMP_PORT_DESC       =  13,
    OFPMP_EXPERIMENTER    =  0xffff
};

#endif /* __OFC_STD_H__ */
