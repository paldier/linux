/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
#ifndef DATAPATH_VLAN_H
#define DATAPATH_VLAN_H
#include <linux/if_ether.h>

#define DP_VLAN_PATTERN_NOT_CARE -1  /*Don't care the specified pattern field,
				      *ie, match whatever value supported
				      */
#define DP_VLAN_NUM 2  /*Maximum number of vlan tag supported */
#define DP_MAX_EXT_VLAN_RULES 64 /* Maximum number of VLAN rules
				  * per dev per direction.
				  * It is mainly for performance purpose
				  */
struct dp_pattern_vlan {
	int prio; /* match exact VLAN tag priority: 0-7
		   * DP_VLAN_PATTERN_PRIO_NOT_CARE: don't care
		   */
	int vid; /* match exact VID: 0 - 4095
		  * DP_VLAN_PATTERN_NOT_CARE: don't care
		  */
	int tpid; /* match exact TPID: 0x8100 or another configured TPID
		   * via FDMA_VTETYPE
		   * Can only match two TPID
		   * DP_VLAN_PATTERN_NOT_CARE: don't care
		   */
	int dei; /* match exact DEI: 0 or 1
		  * DP_VLAN_PATTERN_NOT_CARE: don't care
		  */
#define DP_PROTO_IP4    ETH_P_IP /*IP packet 0x0800*/
#define DP_PROTO_PPPOE  ETH_P_PPP_DISC /*PPPoE packet: 0x8863 & 0x8864) */
#define DP_PROTO_ARP    ETH_P_ARP /*ARP 0x0806*/
#define DP_PROTO_IP6    ETH_P_IPV6 /*IPv6 packet 0x86DD*/
#define DP_PROTO_EAPOL  ETH_P_PAE /*EAPOL packet 0x888E */

	int proto;/* match exact ether type (protocol) as defined
		   * or
		   * DP_VLAN_PATTERN_NOT_CARE: match all proto/ether type
		   */
};

struct dp_act_vlan {
#define DP_VLAN_ACT_FWD    BIT(0)  /*forward packet without editing */
#define DP_VLAN_ACT_DROP   BIT(1)  /*drop packet */
#define DP_VLAN_ACT_POP    BIT(2)  /*pop/remove VLAN */
#define DP_VLAN_ACT_PUSH   BIT(3)  /*push/insert VLAN */
	int act;  /* if act == DP_VLAN_ACT_FWD, forward packet without editing
		   * if act == DP_VLAN_ACT_DROP, drop the packet
		   * if act == DP_VLAN_ACT_POP, remove VLAN
		   * if act == DP_VLAN_ACT_PUSH, insert VLAN
		   */
	int pop_n;  /*the number of VLAN tag to pop
		     *the valid number: 1 or 2
		     */
	int push_n;  /*the number of VLAN tag to push
		      *the valid number: 1 or 2
		      */
#define CP_FROM_INNER		-1 /*copy from inner VLAN header*/
#define CP_FROM_OUTER		-2 /*copy from inner VLAN header*/
#define DERIVE_FROM_DSCP	-3 /* prio is derived from DSCP */
	int tpid[DP_VLAN_NUM]; /* the tpid of VLAN to push:
				*  support two TPID 0x8100 and
					  another programmable TPID
				* or
				*  copy from recv pkt's inner tag(CP_FROM_INNER)
				*  copy from recv pkt's outer tag(CP_FROM_OUTER)
				*/
	int vid[DP_VLAN_NUM];  /* the VID of VLAN to push:
				*  support range: 0 - 4095
				* or
				*  copy from recv pkt's inner tag(CP_FROM_INNER)
				*  copy from recv pkt's outer tag(CP_FROM_OUTER)
				*/
	int dei[DP_VLAN_NUM];  /* the DEI of VLAN to push:
				*  support range: 0 - 1
				* or
				*  copy from recv pkt's inner tag(CP_FROM_INNER)
				*  copy from recv pkt's outer tag(CP_FROM_OUTER)
				*  keep existing value
				*/
	int prio[DP_VLAN_NUM]; /* the prority of VLAN to push:
				*  support range: 0 - 7
				* or
				*  copy from recv pkt's inner tag(CP_FROM_INNER)
				*  copy from recv pkt's outer tag(CP_FROM_OUTER)
				*/
};

#define DP_VLAN_DEF_RULE 1
struct dp_vlan0 {
	int def;		/* default rule for untagged packet */
	struct dp_pattern_vlan outer;	/* match pattern.
					 * only proto is valid for this case
					 */
	struct dp_act_vlan act; /*action once matched */
};

struct dp_vlan1 {
	int def;		/* default rule for 1-tag packet */
	struct dp_pattern_vlan outer;	/*outer VLAN match pattern */
	struct dp_act_vlan act; /*action once matched */
};

struct dp_vlan2 {
	int def;		/* default rule for 2-tag packet */
	struct dp_pattern_vlan outer;	/*outer VLAN match pattern */
	struct dp_pattern_vlan inner;	/*inner VLAN match pattern */
	struct dp_act_vlan act; /*action once matched */
};

struct dp_tc_vlan {
	struct net_device *dev;  /*bridge port device or its CTP device */

#define DP_VLAN_APPLY_CTP 1  /*apply to BP for CTP device */
	int def_apply; /* By default, ie, def_apply == 0,
			*  Apply rule to bridge port if it is a bridge port dev
			*  Apply rule to ctp if it is CTP device
			* But for UNI dev, normally only bridge port dev and not
			* its CTP dev, in this case, caller need to specify flag
			* DP_VLAN_APPLY_CTP to apply VLAN to its CTP port.
			*/

#define DP_DIR_INGRESS 0
#define DP_DIR_EGRESS  1
	int dir; /* DP_DIR_INGRESS(0) and DP_DIR_EGRESS(1) */
#define DP_MULTICAST_SESSION 1  /*IGMP Multicast session */
	int mcast_flag; /*normal or multicast session */

	int n_vlan0, n_vlan1, n_vlan2; /*size of vlan0/vlan1/2_list*/
	struct dp_vlan0 *vlan0_list; /* non-vlan matching rules,
				      * ie, ether type matching only
				      */
	struct dp_vlan1 *vlan1_list; /* single vlan matching rules */
	struct dp_vlan2 *vlan2_list; /* double vlan matching rules */

};

/* API dp_vlan_set: Asymmetric VLAN handling via TC command
 * This API is used to set VLAN on CTP/Bridge port in specified direction:
 * ingress or egress
 * It will automatically apply VLAN on vlan->dev in the specified direction
 * Note: every time to call this API, caller need to collect all VLAN
 *       information for this CTP or brige port based on specified direction
 * return:
 *    DP_FAILURE: no resource or not find such dev
 *    DP_SUCCESS: succeed
 *
 * Treatment outer TPID/DEI example: (3 bits)
 * 000 Copy TPID (and DEI, if present) from the inner tag of the received frame.
 *     tpid=CP_FROM_INNER   dei=CP_FROM_INNER
 * 001 Copy TPID (and DEI, if present) from the outer tag of the received frame.
 *     tpid= CP_FROM_OUTER	dei=CP_FROM_OUTER
 * 010 Set TPID = output TPID attribute value, copy DEI bit from the inner tag
 *     of the received frame
 *      tpid=specified_tpid   dei=CP_FROM_INNER
 * 011 Set TPID = output TPID, copy DEI from the outer tag of the received frame
 *     tpid=specified_tpid   dei=CP_FROM_OUTER
 * 100 Set TPID = 0x8100
 *     tpid=0x8100  dei=0
 * 101 Reserved
 * 110 Set TPID = output TPID, DEI = 0
 *     tpid=specified_tpid   dei=0
 * 111 Set TPID = output TPID, DEI = 1
 *     tpid=specified_tpid   dei=1
 */
int dp_vlan_set(struct dp_tc_vlan *vlan, int flag);
#endif /*DATAPATH_VLAN_H*/
