/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef DATAPATH_H
#define DATAPATH_H

#include <linux/klogging.h>
#include <linux/skbuff.h>	/*skb */
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <net/lantiq_cbm_api.h>

//#define CONFIG_LTQ_DATAPATH_DUMMY_QOS
//#define DUMMY_PPV4_QOS_API_OLD

#ifdef DUMMY_PPV4_QOS_API_OLD
/*TODO:currently need to include both header file */
#include <net/pp_qos_drv_slim.h>
#include <net/pp_qos_drv.h>
#else
#include <net/pp_qos_drv.h>
#endif
#include <net/datapath_api_qos.h>
#ifdef CONFIG_NET_SWITCHDEV
#include <net/switchdev.h>
#endif
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
#include "datapath_swdev.h"
#endif
#include <net/datapath_inst.h>

#define MAX_SUBIFS 256
#define MAX_DP_PORTS  16
#define PMAC_SIZE 8
#define PMAC_CPU_ID  0
#define DP_MAX_BP_NUM 128
#define DP_MAX_QUEUE_NUM 256
#define DP_MAX_SCHED_NUM 2048  /*In fact it should use max number of node*/
#define DP_MAX_CQM_DEQ 128 /*CQM dequeue port*/

#ifdef LOGF_KLOG_ERROR
#define PR_ERR  LOGF_KLOG_ERROR
#else
#define PR_ERR printk
#endif

#ifdef LOGF_KLOG_INFO
#undef PR_INFO
#define PR_INFO LOGF_KLOG_ERROR
#else
#undef PR_INFO
#define PR_INFO printk
#endif

#ifdef LOGF_KLOG_INFO_ONCE
#define PR_INFO_ONCE    LOGF_KLOG_INFO_ONCE
#else
#define PR_INFO_ONCE printk_once
#endif

#ifdef LOGF_KLOG_RATELIMITED
#define PR_RATELIMITED LOGF_KLOG_RATELIMITED
#else
#define PR_RATELIMITED printk_ratelimited
#endif

#define DP_PLATFORM_INIT    1
#define DP_PLATFORM_DE_INIT 2

#define UP_STATS(atomic) atomic_add(1, &(atomic))

#define STATS_GET(atomic) atomic_read(&(atomic))
#define STATS_SET(atomic, val) atomic_set(&(atomic), val)
#define DP_CB(i, x) dp_port_prop[i].info.x

#define PORT(inst, ep) &dp_port_info[inst][ep]
#define PORT_INFO(inst, ep, x) dp_port_info[inst][ep].x
#define PORT_SUBIF(inst, ep, ix, x) dp_port_info[inst][ep].subif_info[ix].x
#define PORT_VAP_MIB(i, ep, vap, x) dp_port_info[i][ep].subif_info[vap].mib.x
#define PORT_VAP(i, ep, vap, x) dp_port_info[i][ep].subif_info[vap].x

#define dp_set_val(reg, val, mask, offset) do {\
	(reg) &= ~(mask);\
	(reg) |= (((val) << (offset)) & (mask));\
} while (0)

#define dp_get_val(val, mask, offset) (((val) & (mask)) >> (offset))

#define DP_DEBUG_ASSERT(expr, fmt, arg...)  do { if (expr) \
	PR_ERR(fmt, ##arg); \
} while (0)

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DBG)
#define DP_DEBUG(flags, fmt, arg...)  do { \
	if (unlikely((dp_dbg_flag & (flags)) && \
		     (((dp_print_num_en) && \
		      (dp_max_print_num)) || (!dp_print_num_en)))) {\
	PR_INFO(fmt, ##arg); \
	if ((dp_print_num_en) && \
	    (dp_max_print_num)) \
		dp_max_print_num--; \
	} \
} while (0)

#define DP_ASSERT_SCOPE __func__

#else
#define DP_DEBUG(flags, fmt, arg...)
#endif				/* end of CONFIG_LTQ_DATAPATH_DBG */

#define IFNAMSIZ 16
#define DP_MAX_HW_CAP 4

#if (!IS_ENABLED(CONFIG_FALCONMX_CQM))
#define DP_SPIN_LOCK 
#endif
#ifdef DP_SPIN_LOCK
#define DP_LIB_LOCK    spin_lock_bh
#define DP_LIB_UNLOCK  spin_unlock_bh
#else
#define DP_LIB_LOCK    mutex_lock
#define DP_LIB_UNLOCK  mutex_unlock
#endif

#define PARSER_FLAG_SIZE   40
#define PARSER_OFFSET_SIZE 8
#define DP_PMAC_OPS(gsw, cmd) (dp_gsw_cb)(gsw)->gsw_pmac_ops.cmd

#define PKT_PASER_FLAG_OFFSET   0
#define PKT_PASER_OFFSET_OFFSET (PARSER_FLAG_SIZE)
#define PKT_PMAC_OFFSET         ((PARSER_FLAG_SIZE) + (PARSER_OFFSET_SIZE))
#define PKT_DATA_OFFSET         ((PKT_PMAC_OFFSET) + (PMAC_SIZE))

#define CHECK_BIT(var, pos) (((var) & (1 << (pos))) ? 1 : 0)

#define PASAR_OFFSETS_NUM 40	/*40 bytes offset */
#define PASAR_FLAGS_NUM 8	/*8 bytes */

#define GET_VAP(subif, bit_shift, mask) (((subif) >> (bit_shift)) & (mask))
#define SET_VAP(vap, bit_shift, mask) ((((u32)vap) & (mask)) << (bit_shift))

enum dp_xmit_errors {
	DP_XMIT_ERR_DEFAULT = 0,
	DP_XMIT_ERR_NOT_INIT,
	DP_XMIT_ERR_IN_IRQ,
	DP_XMIT_ERR_NULL_SUBIF,
	DP_XMIT_ERR_PORT_TOO_BIG,
	DP_XMIT_ERR_NULL_SKB,
	DP_XMIT_ERR_NULL_IF,
	DP_XMIT_ERR_REALLOC_SKB,
	DP_XMIT_ERR_EP_ZERO,
	DP_XMIT_ERR_GSO_NOHEADROOM,
	DP_XMIT_ERR_CSM_NO_SUPPORT,
	DP_XMIT_PTP_ERR,
};

enum PARSER_FLAGS {
	PASER_FLAGS_NO = 0,
	PASER_FLAGS_END,
	PASER_FLAGS_CAPWAP,
	PASER_FLAGS_GRE,
	PASER_FLAGS_LEN,
	PASER_FLAGS_GREK,
	PASER_FLAGS_NN1,
	PASER_FLAGS_NN2,

	PASER_FLAGS_ITAG,
	PASER_FLAGS_1VLAN,
	PASER_FLAGS_2VLAN,
	PASER_FLAGS_3VLAN,
	PASER_FLAGS_4VLAN,
	PASER_FLAGS_SNAP,
	PASER_FLAGS_PPPOES,
	PASER_FLAGS_1IPV4,

	PASER_FLAGS_1IPV6,
	PASER_FLAGS_2IPV4,
	PASER_FLAGS_2IPV6,
	PASER_FLAGS_ROUTEXP,
	PASER_FLAGS_TCP,
	PASER_FLAGS_1UDP,
	PASER_FLAGS_IGMP,
	PASER_FLAGS_IPV4OPT,

	PASER_FLAGS_IPV6EXT,
	PASER_FLAGS_TCPACK,
	PASER_FLAGS_IPFRAG,
	PASER_FLAGS_EAPOL,
	PASER_FLAGS_2IPV6EXT,
	PASER_FLAGS_2UDP,
	PASER_FLAGS_L2TPNEXP,
	PASER_FLAGS_LROEXP,

	PASER_FLAGS_L2TP,
	PASER_FLAGS_GRE_VLAN1,
	PASER_FLAGS_GRE_VLAN2,
	PASER_FLAGS_GRE_PPPOE,
	PASER_FLAGS_BYTE4_BIT4,
	PASER_FLAGS_BYTE4_BIT5,
	PASER_FLAGS_BYTE4_BIT6,
	PASER_FLAGS_BYTE4_BIT7,

	PASER_FLAGS_BYTE5_BIT0,
	PASER_FLAGS_BYTE5_BIT1,
	PASER_FLAGS_BYTE5_BIT2,
	PASER_FLAGS_BYTE5_BIT3,
	PASER_FLAGS_BYTE5_BIT4,
	PASER_FLAGS_BYTE5_BIT5,
	PASER_FLAGS_BYTE5_BIT6,
	PASER_FLAGS_BYTE5_BIT7,

	PASER_FLAGS_BYTE6_BIT0,
	PASER_FLAGS_BYTE6_BIT1,
	PASER_FLAGS_BYTE6_BIT2,
	PASER_FLAGS_BYTE6_BIT3,
	PASER_FLAGS_BYTE6_BIT4,
	PASER_FLAGS_BYTE6_BIT5,
	PASER_FLAGS_BYTE6_BIT6,
	PASER_FLAGS_BYTE6_BIT7,

	PASER_FLAGS_BYTE7_BIT0,
	PASER_FLAGS_BYTE7_BIT1,
	PASER_FLAGS_BYTE7_BIT2,
	PASER_FLAGS_BYTE7_BIT3,
	PASER_FLAGS_BYTE7_BIT4,
	PASER_FLAGS_BYTE7_BIT5,
	PASER_FLAGS_BYTE7_BIT6,
	PASER_FLAGS_BYTE7_BIT7,

	/*Must be put at the end of the enum */
	PASER_FLAGS_MAX
};

/*! PMAC port flag */
enum PORT_FLAG {
	PORT_FREE = 0,		/*! The port is free */
	PORT_ALLOCATED,		/*! the port is already allocated to others,
				 * but not registered or no need to register.\n
				 * eg, LRO/CAPWA, only need to allocate,
				 * but no need to register
				 */
	PORT_DEV_REGISTERED,	/*! dev Registered already. */
	PORT_SUBIF_REGISTERED,	/*! subif Registered already. */

	PORT_FLAG_NO_VALID	/*! Not valid flag */
};

#define DP_DBG_ENUM_OR_STRING(name, value, short_name) {name = value}

enum PMAC_TCP_TYPE {
	TCP_OVER_IPV4 = 0,
	UDP_OVER_IPV4,
	TCP_OVER_IPV6,
	UDP_OVER_IPV6,
	TCP_OVER_IPV6_IPV4,
	UDP_OVER_IPV6_IPV4,
	TCP_OVER_IPV4_IPV6,
	UDP_OVER_IPV4_IPV6
};

enum DP_DBG_FLAG {
	DP_DBG_FLAG_DBG = BIT(0),
	DP_DBG_FLAG_DUMP_RX_DATA = BIT(1),
	DP_DBG_FLAG_DUMP_RX_DESCRIPTOR = BIT(2),
	DP_DBG_FLAG_DUMP_RX_PASER = BIT(3),
	DP_DBG_FLAG_DUMP_RX_PMAC = BIT(4),
	DP_DBG_FLAG_DUMP_RX = (BIT(1) | BIT(2) | BIT(3) | BIT(4)),
	DP_DBG_FLAG_DUMP_TX_DATA = BIT(5),
	DP_DBG_FLAG_DUMP_TX_DESCRIPTOR = BIT(6),
	DP_DBG_FLAG_DUMP_TX_PMAC = BIT(7),
	DP_DBG_FLAG_DUMP_TX_SUM = BIT(8),
	DP_DBG_FLAG_DUMP_TX = (BIT(5) | BIT(6) | BIT(7) | BIT(8)),
	DP_DBG_FLAG_COC = BIT(9),
	DP_DBG_FLAG_MIB = BIT(10),
	DP_DBG_FLAG_MIB_ALGO = BIT(11),
	DP_DBG_FLAG_CBM_BUF = BIT(12),
	DP_DBG_FLAG_PAE = BIT(13),
	DP_DBG_FLAG_INST = BIT(14),
	DP_DBG_FLAG_SWDEV = BIT(15),
	DP_DBG_FLAG_NOTIFY = BIT(16),
	DP_DBG_FLAG_LOGIC = BIT(17),
	DP_DBG_FLAG_GSWIP_API = BIT(18),
	DP_DBG_FLAG_QOS = BIT(19),
	DP_DBG_FLAG_QOS_DETAIL = BIT(20),
	DP_DBG_FLAG_LOOKUP = BIT(21),
	DP_DBG_FLAG_REG = BIT(22),

	/*Note, once add a new entry here int the enum,
	 *need to add new item in below macro DP_F_FLAG_LIST
	 */
	DP_DBG_FLAG_MAX = BIT(31)
};

/*Note: per bit one variable */
#define DP_DBG_FLAG_LIST {\
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DBG, "dbg"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_RX_DATA, "rx_data"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_RX_DESCRIPTOR, "rx_desc"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_RX_PASER, "rx_parse"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_RX_PMAC, "rx_pmac"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_RX, "rx"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_TX_DATA, "tx_data"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_TX_DESCRIPTOR, "tx_desc"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_TX_PMAC, "tx_pmac"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_TX_SUM, "tx_sum"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_DUMP_TX, "tx"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_COC, "coc"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_MIB, "mib"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_MIB_ALGO, "mib_algo"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_CBM_BUF, "cbm_buf"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_PAE, "pae"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_INST, "inst"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_SWDEV, "swdev"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_NOTIFY, "notify"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_LOGIC, "logic"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_GSWIP_API, "gswip"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_QOS, "qos"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_QOS_DETAIL, "qos2"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_LOOKUP, "lookup"), \
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_REG, "register"), \
	\
	\
	/*must be last one */\
	DP_DBG_ENUM_OR_STRING(DP_DBG_FLAG_MAX, "")\
}

enum {
	NODE_LINK_ADD = 0, /*add a link node */
	NODE_LINK_GET,     /*get a link node */
	NODE_LINK_EN_GET,  /*Get link status: enable/disable */
	NODE_LINK_EN_SET,  /*Set link status: enable/disable */
	NODE_UNLINK,       /*unlink a node: in fact, it is just flush now*/
	LINK_ADD,          /*add a link with multiple link nodes */
	LINK_GET,          /*get a link may with multiple link nodes */
	LINK_PRIO_SET,     /*set arbitrate/priority */
	LINK_PRIO_GET,     /*get arbitrate/priority */
	QUEUE_CFG_SET,     /*set queue configuration */
	QUEUE_CFG_GET,     /*get queue configuration */
	SHAPER_SET,        /*set shaper/bandwidth*/
	SHAPER_GET,        /*get shaper/bandwidth*/
	NODE_ALLOC,        /*allocate a node */
	NODE_FREE,         /*free a node */
	NODE_CHILDREN_FREE,  /*free all children under one specified parent:
			      *   scheduler/port
			      */
	DEQ_PORT_RES_GET,  /*get all full links under one specified port */
	COUNTER_MODE_SET,  /*set counter mode: may only for TMU now so far*/
	COUNTER_MODE_GET,  /*get counter mode: may only for TMU now so far*/
	QUEUE_MAP_GET,     /*get lookup entries based on the specified qid*/
	QUEUE_MAP_SET,     /*set lookup entries to the specified qid*/
	NODE_CHILDREN_GET, /*get direct children list of node*/
	QOS_LEVEL_GET,     /* get Max Scheduler level for Node */
};

struct dev_mib {
	atomic_t rx_fn_rxif_pkt; /*! received packet counter */
	atomic_t rx_fn_txif_pkt; /*! transmitted packet counter */
	atomic_t rx_fn_dropped; /*! transmitted packet counter */
	atomic_t tx_cbm_pkt; /*! transmitted packet counter */
	atomic_t tx_clone_pkt; /*! duplicate unicast packet for cloned flag */
	atomic_t tx_hdr_room_pkt; /*! duplicate pkt for no enough headerroom*/
	atomic_t tx_tso_pkt;	/*! transmitted packet counter */
	atomic_t tx_pkt_dropped;	/*! dropped packet counter */
};

struct logic_dev {
	struct list_head list;
	struct net_device *dev;
	u16 bp; /*bridge port */
	u16 ep;
	u16 ctp;
	u32 subif_flag; /*save the flag used during dp_register_subif*/
};

/*! Sub interface detail information */
struct dp_subif_info {
	s32 flags;
	u32 subif:15;
	struct net_device *netif; /*! pointer to  net_device */
	char device_name[IFNAMSIZ]; /*! devide name, like wlan0, */
	struct dev_mib mib; /*! mib */
	struct net_device *ctp_dev; /*CTP dev for PON pmapper case*/
	u16 bp; /*bridge port */
	u16 fid; /* switch bridge id */
	struct list_head logic_dev; /*unexplicit logical dev*/
	struct net_device_ops *old_dev_ops;
	struct net_device_ops new_dev_ops;
#ifdef CONFIG_NET_SWITCHDEV
	struct switchdev_ops *old_swdev_ops;
	struct switchdev_ops new_swdev_ops;
	void *swdev_priv; /*to store ext vlan info*/
#endif
	s16 qid;    /* physical queue id */
	s16 sched_id; /* can be physical scheduler id or logical node id */
	s16 q_node; /* logical queue node Id if applicable */
	s16 qos_deq_port; /* qos port id */
	s16 cqm_deq_port; /* CQM physical dequeue port ID (absolute) */
	s16 cqm_port_idx; /* CQM relative dequeue port index, like tconf id */
	u32 subif_flag; /* To store original flag from caller during
			 * dp_register_subif
			 */
};

struct vlan_info {
	u16 out_proto;
	u16 out_vid;
	u16 in_proto;
	u16 in_vid;
	int cnt;
};

enum DP_TEMP_DMA_PMAC {
	TEMPL_NORMAL = 0,
	TEMPL_CHECKSUM,
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
	TEMPL_PTP,
#endif
	TEMPL_OTHERS,
	MAX_TEMPLATE
};

enum DP_PRIV_F {
	DP_PRIV_PER_CTP_QUEUE = BIT(0), /*Manage Queue per CTP/subif */
};

struct pmac_port_info {
	enum PORT_FLAG status;	/*! port status */
	int alloc_flags;	/* the flags saved when calling dp_port_alloc */
	struct dp_cb cb;	/*! Callback Pointer to DIRECTPATH_CB */
	struct module *owner;
	struct net_device *dev;
	u32 dev_port;
	u32 num_subif;
	s32 port_id;
	struct dp_subif_info subif_info[MAX_SUBIFS];
	atomic_t tx_err_drop;
	atomic_t rx_err_drop;
	struct gsw_itf *itf_info;  /*point to switch interface configuration */
	int ctp_max; /*maximum ctp */
	u32 vap_offset; /*shift bits to get vap value */
	u32 vap_mask; /*get final vap after bit shift */
	u8  cqe_lu_mode; /*cqe lookup mode */
	u32 gsw_mode; /*gswip mode for subif */
	u32 flag_other; /*save flag from cbm_dp_port_alloc */
	u32 deq_port_base; /*CQE Dequeue Port */
	u32 deq_port_num;  /*for PON IP: 64 ports, others: 1 */
	u32 dma_chan; /*associated dma tx CH,-1 means no DMA CH*/
	u32 tx_pkt_credit;  /*PP port tx bytes credit */
	u32 tx_b_credit;  /*PP port tx bytes credit */
	u32 tx_ring_addr;  /*PP port ring address. should follow HW definition*/
	u32 tx_ring_size; /*PP ring size */
	u32 tx_ring_offset;  /*PP: next tx_ring_addr=
			      *   current tx_ring_addr + tx_ring_offset
			      */
	u32 lct_idx; /* LCT subif register flag */
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
	u32 f_ptp:1; /* PTP1588 support enablement */
#endif
};

struct pmac_port_info2 {
	/*only valid for 1st dp instanace which need dp_xmit/dp_rx*/
	/*[0] for non-checksum case,
	 *[1] for checksum offload
	 *[2] two cases:
	 * a: only traffic directly to MPE DL FW
	 * b: DSL bonding FCS case
	 */
	struct pmac_tx_hdr pmac_template[MAX_TEMPLATE];
	struct dma_tx_desc_0 dma0_template[MAX_TEMPLATE];
	struct dma_tx_desc_0 dma0_mask_template[MAX_TEMPLATE];
	struct dma_tx_desc_1 dma1_template[MAX_TEMPLATE];
	struct dma_tx_desc_1 dma1_mask_template[MAX_TEMPLATE];
};

/*bridge port with pmapper supported dev structure */
struct bp_pmapper_dev {
	int flag;/*0-FREE, 1-Used*/
	struct net_device *dev; /*bridge port device pointer */
	int pcp[DP_PMAP_PCP_NUM];  /*PCP table */
	int dscp[DP_PMAP_DSCP_NUM]; /*DSCP table*/
	int def_ctp; /*Untag & nonip*/
	int mode; /*mode*/
	int ref_cnt; /*reference counter */
};

/*queue struct */
struct q_info {
	int flag;  /*0-FREE, 1-Used*/
	int need_free; /*if this queue is allocated by dp_register_subif,
			*   it needs free during de-register.
			*Otherwise, no free
			*/
	int q_node_id;
	int ref_cnt; /*subif_counter*/
	int cqm_dequeue_port; /*CQM dequeue port */
};

/*scheduler struct */
struct sched_info {
	int flag;  /*0-FREE, 1-Used*/
	int ref_cnt; /*subif_counter*/
	int cqm_dequeue_port; /*CQM dequeue port */
};

struct cqm_port_info {
	int f_first_qid : 1; /*0 not valid */
	u32 ref_cnt; /*reference counter: the number of CTP attached to it*/
	u32 tx_pkt_credit;  /*PP port tx bytes credit */
	u32 tx_ring_addr;  /*PP port ring address. should follow HW definition*/
	u32 tx_ring_size; /*PP port ring size */
	int qos_port; /*qos port id*/
	int first_qid; /*in order to auto sharing queue, 1st queue allocated by
			*dp_register_subif_ext for that cqm_dequeue_port will be
			*stored here. later it will be shared by other subif via
			*dp_register_subif_ext
			*/
	int q_node; /*first_qid's logical node id*/
	int dp_port; /* dp_port info */
};

struct parser_info {
	u8 v;
	s8 size;
};

struct subif_platform_data {
	struct net_device *dev;
	struct dp_subif_data *subif_data;  /*from dp_register_subif_ex */
#define TRIGGER_CQE_DP_ENABLE  1
	int act; /*Set by HAL subif_platform_set and used by DP lib */
};

struct vlan_info1 {
	/* Changed this TPID field to GSWIP API enum type.
	 * We do not have flexible for any TPID, only following are supported:
	 * 1. ignore (don't care)
	 * 2. 0x8100
	 * 3. Value configured int VTE Type register
	 */
	GSW_ExtendedVlanFilterTpid_t tpid;  /* TPID like 0x8100, 0x8800 */
	u16 vid ;  /*VLAN ID*/
	/*note: user priority/CFI both don't care */
	/* DSCP to Priority value mapping is possible */
};

struct vlan1 {
	int bp;  /*assigned bp for this single VLAN dev */
	struct vlan_info1 outer_vlan; /*out vlan info */
	/* Add Ethernet type with GSWIP API enum type.
	 * Following types are supported:
	 * 1. ignore	(don't care)
	 * 2. IPoE	(0x0800)
	 * 3. PPPoE	(0x8863 or 0x8864)
	 * 4. ARP	(0x0806)
	 * 5. IPv6 IPoE	(0x86DD)
	 */
	GSW_ExtendedVlanFilterEthertype_t ether_type;
};

struct vlan2 {
	int bp;  /*assigned bp for this double VLAN dev */
	struct vlan_info1 outer_vlan;  /*out vlan info */
	struct vlan_info1 inner_vlan;   /*in vlan info */
	/* Add Ethernet type with GSWIP API enum type.
	 * Following types are supported:
	 * 1. ignore	(don't care)
	 * 2. IPoE	(0x0800)
	 * 3. PPPoE	(0x8863 or 0x8864)
	 * 4. ARP	(0x0806)
	 * 5. IPv6 IPoE	(0x86DD)
	 */
	GSW_ExtendedVlanFilterEthertype_t ether_type;
};

struct ext_vlan_info {
	int subif_grp, logic_port; /* base subif group and logical port.
				    * In DP it is subif
				    */
	int bp; /*default bp for this ctp */
	int n_vlan1, n_vlan2; /*size of vlan1/2_list*/
	int n_vlan1_drop, n_vlan2_drop; /*size of vlan1/2_drop_list */
	struct vlan1 *vlan1_list; /*allow single vlan dev info list auto
				   * bp is for egress VLAN setting
				   */
	struct vlan2 *vlan2_list; /* allow double vlan dev info list auto
				   * bp is for egress VLAN setting
				   */
	struct vlan1 *vlan1_drop_list; /* drop single vlan list - manual
					*  bp no use
					*/
	struct vlan2 *vlan2_drop_list; /* drop double vlan list - manual
					* bp no use
					*/
	/* Need add other input / output information for deletion. ?? */
	/* private data stored by function set_gswip_ext_vlan */
	void *priv;
};

struct dp_tc_vlan_info {
	int dev_type; /* bit 0 - 1: apply VLAN to bp
		       *         0: apply VLAN to subix (subif group)
		       * bit 1 - 1: multicast session
		       *         0: normal
		       *
		       */
	int subix;  /*similar like GSWIP subif group*/
	int bp;  /*bridge port id */
	int dp_port; /*logical port */
	int inst;  /*DP instance */
};

/*port 0 is reserved*/
extern int dp_inst_num;
extern struct inst_property dp_port_prop[DP_MAX_INST];
extern struct pmac_port_info dp_port_info[DP_MAX_INST][MAX_DP_PORTS];
extern struct pmac_port_info2 dp_port_info2[DP_MAX_INST][MAX_DP_PORTS];
extern struct q_info dp_q_tbl[DP_MAX_INST][DP_MAX_QUEUE_NUM];
extern struct sched_info dp_sched_tbl[DP_MAX_INST][DP_MAX_SCHED_NUM];
extern struct cqm_port_info dp_deq_port_tbl[DP_MAX_INST][DP_MAX_CQM_DEQ];
extern struct bp_pmapper_dev dp_bp_dev_tbl[DP_MAX_INST][DP_MAX_BP_NUM];

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DBG)
extern u32 dp_dbg_flag;
extern unsigned int dp_dbg_err;
extern unsigned int dp_max_print_num;
extern unsigned int dp_print_num_en;
#endif

int dp_loop_eth_dev_init(struct dentry *parent);
void dp_loop_eth_dev_exit(void);
struct dentry *dp_proc_install(void);
extern char *dp_dbg_flag_str[];
extern unsigned int dp_dbg_flag_list[];
extern u32 dp_port_flag[];
extern char *dp_port_type_str[];
extern char *dp_port_status_str[];
extern struct parser_info pinfo[];
//extern GSW_API_HANDLE gswr_r;
enum TEST_MODE {
	DP_RX_MODE_NORMAL = 0,
	DP_RX_MODE_LAN_WAN_BRIDGE,
	DPR_RX_MODE_MAX
};

extern u32 dp_rx_test_mode;
extern struct dma_rx_desc_1 dma_rx_desc_mask1;
extern struct dma_rx_desc_3 dma_rx_desc_mask3;
extern struct dma_rx_desc_0 dma_tx_desc_mask0;
extern struct dma_rx_desc_1 dma_tx_desc_mask1;
ssize_t proc_print_mode_write(struct file *file, const char *buf,
			      size_t count, loff_t *ppos);
void proc_print_mode_read(struct seq_file *s);
int parser_size_via_index(u8 index);
struct pmac_port_info *get_port_info_via_dp_name(struct net_device *dev);
void dp_clear_mib(dp_subif_t *subif, uint32_t flag);
extern u32 dp_drop_all_tcp_err;
extern u32 dp_pkt_size_check;
void dp_mib_exit(void);
void print_parser_status(struct seq_file *s);
void proc_mib_timer_read(struct seq_file *s);
int mpe_fh_netfiler_install(void);
int dp_coc_cpufreq_exit(void);
int dp_coc_cpufreq_init(void);
int qos_dump_start(void);
int qos_dump(struct seq_file *s, int pos);
ssize_t proc_qos_write(struct file *file, const char *buf,
		       size_t count, loff_t *ppos);
int update_coc_up_sub_module(enum ltq_cpufreq_state new_state,
			     enum ltq_cpufreq_state old_state, uint32_t flag);
void proc_coc_read(struct seq_file *s);
ssize_t proc_coc_write(struct file *file, const char *buf, size_t count,
		       loff_t *ppos);
void dump_parser_flag(char *buf);

//int dp_reset_sys_mib(u32 flag);
void dp_clear_all_mib_inside(uint32_t flag);

struct sk_buff *dp_create_new_skb(struct sk_buff *skb);
extern int ip_offset_hw_adjust;
int register_notifier(u32 flag);
int unregister_notifier(u32 flag);
//int supported_logic_dev(int inst, struct net_device *dev, char *subif_name);
struct net_device *get_base_dev(struct net_device *dev, int level);
int add_logic_dev(int inst, int port_id, struct net_device *dev,
		  dp_subif_t *subif_id, u32 flags);
int del_logic_dev(int inst, struct list_head *head, struct net_device *dev,
		  u32 flags);
int dp_get_port_subitf_via_dev_private(struct net_device *dev,
				       dp_subif_t *subif);
int get_vlan_via_dev(struct net_device *dev, struct vlan_prop *vlan_prop);
void dp_parser_info_refresh(u32 cpu, u32 mpe1, u32 mpe2, u32 mpe3, u32 verify);
int dp_inst_init(u32 flag);
int request_dp(u32 flag);
/*static __init */ int dp_init_module(void);
/*static __exit*/ void dp_cleanup_module(void);
int dp_probe(struct platform_device *pdev);
#define NS_INT16SZ	 2
#define NS_INADDRSZ	 4
#define NS_IN6ADDRSZ	16

int inet_pton4(const char *src, u_char *dst);
int pton(const char *src, void *dst);
int inet_pton4(const char *src, u_char *dst);
int inet_pton6(const char *src, u_char *dst);
int low_10dec(u64 x);
int high_10dec(u64 x);
int dp_atoi(unsigned char *str);
int mac_stob(const char *mac, u8 bytes[6]);
int get_offset_clear_chksum(struct sk_buff *skb, u32 *ip_offset,
			    u32 *tcp_h_offset,
			    u32 *tcp_type);
int get_vlan_info(struct net_device *dev, struct vlan_info *vinfo);
int dp_basic_proc(void);

struct pmac_port_info *get_port_info(int inst, int index);
struct pmac_port_info *get_port_info_via_dp_port(int inst, int dp_port);

void set_dp_dbg_flag(uint32_t flags);
uint32_t get_dp_dbg_flag(void);
void dp_dump_raw_data(char *buf, int len, char *prefix_str);

#ifdef CONFIG_LTQ_TOE_DRIVER
/*! @brief  ltq_tso_xmit
 *@param[in] skb  pointer to packet buffer like sk_buff
 *@param[in] hdr  point to packet header, like pmac header
 *@param[in] len  packet header
 *@param[in] flags :Reserved
 *@return 0 if OK  / -1 if error
 *@note
 */
int ltq_tso_xmit(struct sk_buff *skb, void *hdr, int len, int flags);
#endif

int dp_set_meter_rate(enum ltq_cpufreq_state stat, unsigned int rate);
char *dp_skb_csum_str(struct sk_buff *skb);
extern struct dentry *dp_proc_node;
int get_dp_dbg_flag_str_size(void);
int get_dp_port_status_str_size(void);
int get_dp_port_type_str_size(void);
char *get_dp_port_type_str(int k);

u32 *get_port_flag(int inst, int index);
int dp_request_inst(struct dp_inst_info *info, u32 flag);
int register_dp_cap(u32 flag);
int print_symbol_name(unsigned long addr);
typedef GSW_return_t(*dp_gsw_cb)(void *, void *);
void falcon_test(void); /*defined in Pp qos driver */
int bp_pmapper_dev_get(int inst, struct net_device *dev);

extern int32_t (*qos_mgr_hook_setup_tc)(struct net_device *dev, u32 handle,
					__be16 protocol,
					struct tc_to_netdev *tc);

#define DP_SUBIF_LIST_HASH_SHIFT 8
#define DP_SUBIF_LIST_HASH_BIT_LENGTH 10
#define DP_SUBIF_LIST_HASH_SIZE ((1 << DP_SUBIF_LIST_HASH_BIT_LENGTH) - 1)

extern struct hlist_head dp_subif_list[DP_SUBIF_LIST_HASH_SIZE];
int32_t dp_sync_subifid(struct net_device *dev, char *subif_name,
			dp_subif_t *subif_id, struct dp_subif_data *data,
			u32 flags);
int32_t	dp_update_subif(struct net_device *netif, void *data, dp_subif_t *subif,
			char *subif_name, u32 flags);
int32_t	dp_del_subif(struct net_device *netif, void *data, dp_subif_t *subif,
		     char *subif_name, u32 flags);
struct dp_subif_cache *dp_subif_lookup(struct hlist_head *head,
				       struct net_device *dev,
				       void *data);
int dp_subif_list_init(void);
u32 dp_subif_hash(struct net_device *dev);
int32_t dp_get_netif_subifid_priv(struct net_device *netif,
				  struct sk_buff *skb, void *subif_data,
				  u8 dst_mac[DP_MAX_ETH_ALEN],
				  dp_subif_t *subif, uint32_t flags);
#endif /*DATAPATH_H */

