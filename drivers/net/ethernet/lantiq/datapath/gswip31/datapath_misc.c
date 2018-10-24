/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if_ether.h>
#include <linux/ethtool.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

#include <linux/clk.h>
#include <linux/ip.h>
#include <net/ip.h>

#include <lantiq.h>
#include <lantiq_soc.h>
#include <net/lantiq_cbm_api.h>
#define DATAPATH_HAL_LAYER   /*must put before include datapath_api.h in
			      *order to avoid include another platform's
			      *DMA descriptor and pmac header files
			      */
#include <net/lantiq_cbm_api.h>
#include <net/datapath_api.h>
#include <net/datapath_api_gswip31.h>
#include "../datapath.h"
#include "../datapath_instance.h"
#include "datapath_proc.h"
#include "datapath_ppv4.h"
#include "datapath_misc.h"

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
#include "datapath_switchdev.h"
#endif

static void init_dma_desc_mask(void)
{
	/*mask 0: to remove the bit, 1 -- keep the bit */
	dma_rx_desc_mask1.all = 0xFFFFFFFF;
	dma_rx_desc_mask3.all = 0xFFFFFFFF;
	dma_rx_desc_mask3.field.own = 0; /*remove owner bit */
	dma_rx_desc_mask3.field.c = 0;
	dma_rx_desc_mask3.field.sop = 0;
	dma_rx_desc_mask3.field.eop = 0;
	dma_rx_desc_mask3.field.dic = 0;
	dma_rx_desc_mask3.field.byte_offset = 0;
	dma_rx_desc_mask1.field.dec = 0;
	dma_rx_desc_mask1.field.enc = 0;
	dma_rx_desc_mask1.field.mpe2 = 0;
	dma_rx_desc_mask1.field.mpe1 = 0;
	/*mask to keep some value via 1 set by
	 * top application all others set to 0
	 */
	dma_tx_desc_mask0.all = 0;
	dma_tx_desc_mask1.all = 0;
	dma_tx_desc_mask0.field.flow_id = 0xFF;
	dma_tx_desc_mask0.field.dest_sub_if_id = 0x7FFF;
	dma_tx_desc_mask1.field.mpe1 = 0x1;
	dma_tx_desc_mask1.field.color = 0x3;
	dma_tx_desc_mask1.field.ep = 0xF;
}

static void init_dma_pmac_template(int portid, u32 flags)
{
	int i;
	struct pmac_port_info2 *dp_info = &dp_port_info2[0][portid];

	/*Note:
	 * final tx_dma0 = (tx_dma0 & dma0_mask_template) | dma0_template
	 * final tx_dma1 = (tx_dma1 & dma1_mask_template) | dma1_template
	 * final tx_pmac = pmac_template
	 */
	memset(dp_info->pmac_template, 0, sizeof(dp_info->pmac_template));
	memset(dp_info->dma0_template, 0, sizeof(dp_info->dma0_template));
	memset(dp_info->dma1_template, 0, sizeof(dp_info->dma1_template));
	for (i = 0; i < MAX_TEMPLATE; i++) {
		dp_info->dma0_mask_template[i].all = 0xFFFFFFFF;
		dp_info->dma1_mask_template[i].all = 0xFFFFFFFF;
	}
	if ((flags & DP_F_FAST_ETH_LAN) || (flags & DP_F_FAST_ETH_WAN) ||
	    (flags & DP_F_GPON) || (flags & DP_F_EPON)) {
		/*always with pmac */
		for (i = 0; i < MAX_TEMPLATE; i++) {
			dp_info->pmac_template[i].class_en = 1;
			SET_PMAC_IGP_EGP(&dp_info->pmac_template[i], portid);
			//TODO changed redir to '0' for HAPS Local testing
			dp_info->dma0_template[i].field.redir = 1;
			dp_info->dma0_mask_template[i].field.redir = 0;
		}
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
		dp_info->pmac_template[TEMPL_PTP].ptp = 1;
#endif
	} else if (flags & DP_F_DIRECTLINK) { /*always with pmac*/
		/*normal dirctpath without checksum support
		 *but with pmac to Switch for accelerate
		 */
		dp_info->pmac_template[TEMPL_NORMAL].igp_msb = portid;
		dp_info->pmac_template[TEMPL_NORMAL].class_en = 1;
		SET_PMAC_IGP_EGP(&dp_info->pmac_template[TEMPL_NORMAL], portid);

		dp_info->dma1_template[TEMPL_CHECKSUM].field.enc = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.dec = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.mpe2 = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.mpe2 = 0;
		/*dirctpath with checksum support */
		dp_info->pmac_template[TEMPL_CHECKSUM].igp_msb = portid;
		dp_info->dma0_template[TEMPL_CHECKSUM].field.redir = 1;
		dp_info->dma0_mask_template[TEMPL_CHECKSUM].field.redir = 0;
		dp_info->pmac_template[TEMPL_CHECKSUM].tcp_chksum = 1;
		dp_info->pmac_template[TEMPL_CHECKSUM].class_en = 1;
		SET_PMAC_IGP_EGP(&dp_info->pmac_template[TEMPL_CHECKSUM],
				 portid);
		dp_info->dma1_template[TEMPL_CHECKSUM].field.enc = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.dec = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.mpe2 = 1;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.mpe2 = 0;

		/*dirctpath without checksum support but send packet to MPE
		 * DL FW
		 */
		dp_info->pmac_template[TEMPL_OTHERS].igp_msb = portid;
		dp_info->dma0_template[TEMPL_OTHERS].field.redir = 1;
		dp_info->dma0_mask_template[TEMPL_OTHERS].field.redir = 0;
		dp_info->pmac_template[TEMPL_OTHERS].class_en = 1;
		SET_PMAC_IGP_EGP(&dp_info->pmac_template[TEMPL_OTHERS], portid);
#if defined(CONFIG_ACCL_11AC_CS2) || defined(CONFIG_ACCL_11AC_CS2_MODULE)
			/* CPU traffic to PAE via cbm to apply PCE rule */
		dp_info->dma1_template[TEMPL_OTHERS].field.enc = 1;
		dp_info->dma1_template[TEMPL_OTHERS].field.dec = 1;
		dp_info->dma1_template[TEMPL_OTHERS].field.mpe2 = 0;
		dp_info->dma1_mask_template[TEMPL_OTHERS].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_OTHERS].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_OTHERS].field.mpe2 = 0;
#else
		/* No need since already set to zero by default
		 *dp_info->dma1_template[TEMPL_OTHERS].field.enc = 0;
		 *dp_info->dma1_template[TEMPL_OTHERS].field.dec = 0;
		 *dp_info->dma1_template[TEMPL_OTHERS].field.mpe2 = 0;
		 *dp_info->dma1_mask_template[TEMPL_OTHERS].field.enc = 0;
		 *dp_info->dma1_mask_template[TEMPL_OTHERS].field.dec = 0;
		 *dp_info->dma1_mask_template[TEMPL_OTHERS].field.mpe2 = 0;
		 */
#endif
	} else if (flags & DP_F_FAST_DSL) { /*sometimes with pmac*/
		/* For normal single DSL upstream, there is no pmac at all*/
		dp_info->dma1_template[TEMPL_NORMAL].field.dec = 1;
		dp_info->dma1_template[TEMPL_NORMAL].field.mpe2 = 1;
		dp_info->dma1_mask_template[TEMPL_NORMAL].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_NORMAL].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_NORMAL].field.mpe2 = 0;

		/*DSL  with checksum support */
		dp_info->pmac_template[TEMPL_CHECKSUM].igp_msb = portid;
		dp_info->dma0_template[TEMPL_CHECKSUM].field.redir = 1;
		dp_info->dma0_mask_template[TEMPL_CHECKSUM].field.redir = 0;
		/*checksum*/
		dp_info->pmac_template[TEMPL_CHECKSUM].tcp_chksum = 1;
		dp_info->pmac_template[TEMPL_CHECKSUM].class_en = 1;
		SET_PMAC_IGP_EGP(&dp_info->pmac_template[TEMPL_CHECKSUM],
				 portid);
		dp_info->dma1_template[TEMPL_CHECKSUM].field.enc = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.dec = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.mpe2 = 1;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.mpe2 = 0;

		/*Bonding DSL  FCS Support via GSWIP */
		dp_info->pmac_template[TEMPL_OTHERS].igp_msb = portid;
		dp_info->dma0_template[TEMPL_OTHERS].field.redir = 1;
		dp_info->dma0_mask_template[TEMPL_OTHERS].field.redir = 0;
		/*dp_info->pmac_template[TEMPL_OTHERS].tcp_chksum = 1; */
		dp_info->pmac_template[TEMPL_OTHERS].class_en = 1;
		SET_PMAC_IGP_EGP(&dp_info->pmac_template[TEMPL_CHECKSUM],
				 portid);
		dp_info->dma1_template[TEMPL_OTHERS].field.enc = 1;
		dp_info->dma1_template[TEMPL_OTHERS].field.dec = 1;
		dp_info->dma1_template[TEMPL_OTHERS].field.mpe2 = 1;
		dp_info->dma1_mask_template[TEMPL_OTHERS].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_OTHERS].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_OTHERS].field.mpe2 = 0;
	} else if (flags & DP_F_FAST_WLAN) {
		/* WLAN block must put after DSL/DIRECTLINK block
		 * since all ACA device in GRX500 is using WLAN flag wrongly
		 * and here must make sure still be back-compatible for it.
		 * normal fast_wlan without pmac except checksum offload support
		 * So no need to configure pmac_tmplate[0]
		 *
		 * Need to add real stuff later
		 */
	} else /*if(flags & DP_F_DIRECT ) */{/*always with pmac*/
		/*normal dirctpath without checksum support */
		dp_info->pmac_template[TEMPL_NORMAL].igp_msb = portid;
		dp_info->pmac_template[TEMPL_NORMAL].class_en = 1;
		SET_PMAC_IGP_EGP(&dp_info->pmac_template[TEMPL_NORMAL], portid);
		dp_info->dma1_template[TEMPL_CHECKSUM].field.enc = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.dec = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.mpe2 = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.mpe2 = 0;

		/*dirctpath with checksum support */
		dp_info->pmac_template[TEMPL_CHECKSUM].igp_msb = PMAC_CPU_ID;
		dp_info->dma0_template[TEMPL_CHECKSUM].field.redir = 1;
		dp_info->dma0_mask_template[TEMPL_CHECKSUM].field.redir = 0;
		dp_info->pmac_template[TEMPL_CHECKSUM].tcp_chksum = 1;
		dp_info->pmac_template[TEMPL_CHECKSUM].class_en = 1;
		SET_PMAC_IGP_EGP(&dp_info->pmac_template[TEMPL_CHECKSUM],
				 portid);
		dp_info->dma1_template[TEMPL_CHECKSUM].field.enc = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.dec = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.mpe2 = 1;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.mpe2 = 0;
	}
}

void dump_rx_dma_desc(struct dma_rx_desc_0 *desc_0,
		      struct dma_rx_desc_1 *desc_1,
		      struct dma_rx_desc_2 *desc_2,
		      struct dma_rx_desc_3 *desc_3)
{
	if (!desc_0 || !desc_1 || !desc_2 || !desc_3) {
		PR_ERR("rx desc_0/1/2/3 NULL\n");
		return;
	}

	PR_INFO(" DMA Descripotr:D0=0x%08x D1=0x%08x D2=0x%08x D3=0x%08x\n",
		*(u32 *)desc_0, *(u32 *)desc_1,
		*(u32 *)desc_2, *(u32 *)desc_3);
	PR_INFO("  DW0:redir=%d res=%d tunl=%d flow=%d ether=%d %s=0x%04x\n",
		desc_0->field.redir,
		desc_0->field.resv, desc_0->field.tunnel_id,
		desc_0->field.flow_id, desc_0->field.eth_type,
		"subif", desc_0->field.dest_sub_if_id);
	PR_INFO("  DW1:%s=%d tcp_err=%d nat=%d dec=%d enc=%d mpe2/1=%d/%d\n",
		"sess/src_subif", desc_1->field.session_id,
		desc_1->field.tcp_err,
		desc_1->field.nat, desc_1->field.dec, desc_1->field.enc,
		desc_1->field.mpe2, desc_1->field.mpe1);
	PR_INFO("      color=%02d ep=%02d ip=%02d classid=%02d\n",
		desc_1->field.color, desc_1->field.ep, desc_1->field.ip,
		desc_1->field.classid);
	PR_INFO("  DW2:data_ptr=0x%08x\n", desc_2->field.data_ptr);
	PR_INFO("  DW3:own=%d c=%d sop=%d eop=%d dic=%d pdu_type=%d\n",
		desc_3->field.own, desc_3->field.c, desc_3->field.sop,
		desc_3->field.eop, desc_3->field.dic, desc_3->field.pdu_type);
	PR_INFO("      offset=%d policy=%d res=%d pool=%d len=%d\n",
		desc_3->field.byte_offset, desc_3->field.policy,
		desc_3->field.res, desc_3->field.pool,
		desc_3->field.data_len);
}

void dump_tx_dma_desc(struct dma_tx_desc_0 *desc_0,
		      struct dma_tx_desc_1 *desc_1,
		      struct dma_tx_desc_2 *desc_2,
		      struct dma_tx_desc_3 *desc_3)
{
	int lookup;
	int inst = 0;
	int dp_port;

	if (!desc_0 || !desc_1 || !desc_2 || !desc_3) {
		PR_ERR("tx desc_0/1/2/3 NULL\n");
		return;
	}
	PR_INFO(" DMA Descripotr:D0=0x%08x D1=0x%08x D2=0x%08x D3=0x%08x\n",
		*(u32 *)desc_0, *(u32 *)desc_1,
		*(u32 *)desc_2, *(u32 *)desc_3);
	PR_INFO("  DW0:redir=%d res=%d tunl=%d flow=%d ether=%d subif=0x%04x\n",
		desc_0->field.redir,
		desc_0->field.resv, desc_0->field.tunnel_id,
		desc_0->field.flow_id, desc_0->field.eth_type,
		desc_0->field.dest_sub_if_id);
	PR_INFO("  DW1:sess=%d tcp_err=%d nat=%d dec=%d enc=%d mpe2/1=%d/%d\n",
		desc_1->field.session_id, desc_1->field.tcp_err,
		desc_1->field.nat, desc_1->field.dec, desc_1->field.enc,
		desc_1->field.mpe2, desc_1->field.mpe1);
	PR_INFO("  color=%02d ep=%02d ip=%02d class=%d\n",
		desc_1->field.color, desc_1->field.ep,
		desc_1->field.ip, desc_1->field.classid);
	PR_INFO("  DW2:data_ptr=0x%08x\n", desc_2->field.data_ptr);
	PR_INFO("  DW3:own=%d c=%d sop=%d eop=%d dic=%d pdu_type=%d\n",
		desc_3->field.own, desc_3->field.c, desc_3->field.sop,
		desc_3->field.eop, desc_3->field.dic, desc_3->field.pdu_type);
	PR_INFO("  offset=%d policy=%d res=%d pool=%d data_len=%d\n",
		desc_3->field.byte_offset,
		desc_3->field.policy, desc_3->field.res,
		desc_3->field.pool, desc_3->field.data_len);
	dp_port = desc_1->field.ep;
	if (dp_port_info[inst][dp_port].cqe_lu_mode == CQE_LU_MODE0)
		/*Flow[7:6] DEC ENC MPE2 MPE1 EP Class */
		lookup = ((desc_0->field.flow_id >> 6) << 12) |
			 ((desc_1->field.dec) << 11) |
			 ((desc_1->field.enc) << 10) |
			 ((desc_1->field.mpe2) << 9) |
			 ((desc_1->field.mpe1) << 8) |
			 ((desc_1->field.ep) << 4) |
			 desc_1->field.classid;
	else if (dp_port_info[inst][dp_port].cqe_lu_mode == CQE_LU_MODE1)
		/*Subif[7:4] MPE2 MPE1 EP Subif[3:0] */
		lookup = ((desc_0->field.dest_sub_if_id >> 4) << 10) |
			 ((desc_1->field.mpe2) << 9) |
			 ((desc_1->field.mpe1) << 8) |
			 ((desc_1->field.ep) << 4) |
			 (desc_0->field.dest_sub_if_id & 0xf);
	else if (dp_port_info[inst][dp_port].cqe_lu_mode == CQE_LU_MODE2)
		/*Subif[7:4] MPE2 MPE1 EP Class */
		lookup = ((desc_0->field.dest_sub_if_id >> 4) << 10) |
			 ((desc_1->field.mpe2) << 9) |
			 ((desc_1->field.mpe1) << 8) |
			 ((desc_1->field.ep) << 4) |
			 desc_1->field.classid;
	else /*mode3*/
		/*Subif[4:1] MPE2 MPE1 EP Subif[0:0] Class[2:0] */
		lookup = (((desc_0->field.dest_sub_if_id >> 1) & 0xf) << 10) |
			 ((desc_1->field.mpe2) << 9) |
			 ((desc_1->field.mpe1) << 8) |
			 ((desc_1->field.ep) << 4) |
			 ((desc_0->field.dest_sub_if_id & 0x1) << 3) |
			 (desc_1->field.classid & 7); /*lower 3 bits*/
	PR_INFO("  lookup index=0x%x qid=%d\n", lookup,
		get_lookup_qid_via_index(lookup));
}

static void dump_rx_pmac(struct pmac_rx_hdr *pmac)
{
	int i, l;
	unsigned char *p = (char *)pmac;
	unsigned char buf[100];

	if (!pmac) {
		PR_ERR(" pmac NULL ??\n");
		return;
	}

	l = sprintf(buf, "PMAC at 0x%p: ", p);
	for (i = 0; i < 8; i++)
		l += sprintf(buf + l, "0x%02x ", p[i]);
	l += sprintf(buf + l, "\n");
	PR_INFO("%s", buf);

	/*byte 0 */
	PR_INFO("  byte 0:res=%d ver_done=%d ip_offset=%d\n", pmac->res0,
		pmac->ver_done, pmac->ip_offset);
	/*byte 1 */
	PR_INFO("  byte 1:tcp_h_offset=%d tcp_type=%d\n", pmac->tcp_h_offset,
		pmac->tcp_type);
	/*byte 2 */
	PR_INFO("  byte 2:class=%d res=%d\n", pmac->class, pmac->res2);
	/*byte 3 */
	PR_INFO("  byte 3:pkt_type=%d ext=%d ins=%d res31=%d oam=%d res32=%d\n",
		pmac->pkt_type, pmac->ext, pmac->ins, pmac->res31,
		pmac->oam, pmac->res32);
	/*byte 4 */
	PR_INFO("  byte 4:res=%d ptp=%d one_step=%d src_dst_subif_id_msb=%d\n",
		pmac->res4, pmac->ptp, pmac->one_step,
		pmac->src_dst_subif_id_msb);
	/*byte 5 */
	PR_INFO("  byte 5:src_sub_inf_id2=%d\n", pmac->src_dst_subif_id_lsb);
	/*byte 6 */
	PR_INFO("  byte 6:record_id_msb=%d\n", pmac->record_id_msb);
	/*byte 7 */
	PR_INFO("  byte 7:record_id_lsb=%d igp_egp=%d\n",
		pmac->record_id_lsb, pmac->igp_egp);
}

static void dump_tx_pmac(struct pmac_tx_hdr *pmac)
{
	int i, l;
	unsigned char *p = (char *)pmac;
	unsigned char buf[100];

	if (!pmac) {
		PR_ERR("dump_tx_pmac pmac NULL ??\n");
		return;
	}

	l = sprintf(buf, "PMAC at 0x%p: ", p);
	for (i = 0; i < 8; i++)
		l += sprintf(buf + l, "0x%02x ", p[i]);
	sprintf(buf + l, "\n");
	PR_INFO("%s", buf);

	/*byte 0 */
	PR_INFO("  byte 0:res=%d tcp_chksum=%d ip_offset=%d\n", pmac->res1,
		pmac->tcp_chksum, pmac->ip_offset);
	/*byte 1 */
	PR_INFO("  byte 1:tcp_h_offset=%d tcp_type=%d\n", pmac->tcp_h_offset,
		pmac->tcp_type);
	/*byte 2 */
	PR_INFO("  byte 2:igp_msb=%d res=%d\n", pmac->igp_msb, pmac->res2);
	/*byte 3 */
	PR_INFO("  byte 3:%s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d\n",
		"pkt_type", pmac->pkt_type,
		"ext", pmac->ext,
		"ins", pmac->ins,
		"res3", pmac->res3,
		"oam", pmac->oam,
		"lrnmd", pmac->lrnmd,
		"class_en", pmac->class_en);
	/*byte 4 */
	PR_INFO("  byte 4:%s=%d ptp=%d one_step=%d src_dst_subif_id_msb=%d\n",
		"fcs_ins_dis", pmac->fcs_ins_dis,
		pmac->ptp, pmac->one_step,
		pmac->src_dst_subif_id_msb);
	/*byte 5 */
	PR_INFO("  byte 5:src_dst_subif_id_lsb=%d\n",
		pmac->src_dst_subif_id_lsb);
	/*byte 6 */
	PR_INFO("  byte 6:record_id_msb=%d\n", pmac->record_id_msb);
	/*byte 7 */
	PR_INFO("  byte 7:record_id_lsb=%d igp_egp=%d\n", pmac->record_id_lsb,
		pmac->igp_egp);
}

static void mib_init(u32 flag)
{
#ifdef CONFIG_LTQ_DATAPATH_MIB
	dp_mib_init(0);
#endif
	gsw_mib_reset_31(0, 0); /* GSW O */
}

void dp_sys_mib_reset_31(u32 flag)
{
#ifdef CONFIG_LTQ_DATAPATH_MIB
	dp_reset_sys_mib(0);
#else
	gsw_mib_reset_31(0, 0); /* GSW L */
	gsw_mib_reset_31(1, 0); /* GSW R */
	dp_clear_all_mib_inside(0);
#endif
}

#ifndef CONFIG_LTQ_DATAPATH_QOS_HAL
int alloc_q_to_port(struct ppv4_q_sch_port *info, u32 flag)
{
	struct ppv4_queue q;
	struct ppv4_port port;
	int inst = info->inst;
	struct hal_priv *priv = HAL(inst);

	if (!priv) {
		PR_ERR("why priv NULL ???\n");
		return -1;
	}

	if (priv->deq_port_stat[info->cqe_deq].flag == PP_NODE_FREE) {
		port.cqm_deq_port = info->cqe_deq;
		port.tx_pkt_credit = info->tx_pkt_credit;
		port.tx_ring_addr = info->tx_ring_addr;
		port.tx_ring_size = info->tx_ring_size;
		port.inst = inst;
		port.dp_port = info->dp_port;
		if (dp_pp_alloc_port(&port)) {
			PR_ERR("%s fail for deq_port=%d qos_deq_port=%d\n",
			       "dp_pp_alloc_port",
			       port.cqm_deq_port, port.qos_deq_port);
			return -1;
		}
		//priv->deq_port_stat[info->cqe_deq].node_id = port.node_id;
		//priv->deq_port_stat[info->cqe_deq].flag = PP_NODE_ALLOC;
		info->port_node = port.node_id;
		info->f_deq_port_en = 1;
	} else {
		port.node_id = priv->deq_port_stat[info->cqe_deq].node_id;
	}
	q.qid = -1;
	q.parent = port.node_id;
	q.inst = inst;
#ifdef CONFIG_LTQ_DATAPATH_DUMMY_QOS
	q.dq_port = info->cqe_deq; /*for qos slim driver only */
#endif
	if (dp_pp_alloc_queue(&q)) {
		PR_ERR("%s fail\n",
		       "dp_pp_alloc_queue");
		return -1;
	}
	PORT_VAP(info->inst, info->dp_port, info->ctp, qid) = q.qid;
	PORT_VAP(info->inst, info->dp_port, info->ctp, q_node) = q.node_id;
	info->qid = q.qid;
	info->q_node = q.node_id;
	priv->qos_queue_stat[q.qid].deq_port = info->cqe_deq;
	priv->qos_queue_stat[q.qid].node_id = q.node_id;
	priv->qos_queue_stat[q.qid].flag = PP_NODE_ALLOC;
	DP_DEBUG(DP_DBG_FLAG_QOS, "alloc_q_to_port done\n");
	return 0;
}
#else
int alloc_q_to_port(struct ppv4_q_sch_port *info, u32 flag)
{
	struct dp_node_link link = {0};

	link.cqm_deq_port.cqm_deq_port = info->cqe_deq;
	link.dp_port = info->dp_port; /*in case for qos node alloc resv pool*/
	link.inst = info->inst;
	link.node_id.q_id = DP_NODE_AUTO_ID;
	link.node_type = DP_NODE_QUEUE;
	link.p_node_id.cqm_deq_port = info->cqe_deq;
	link.p_node_type = DP_NODE_PORT;
	link.arbi = ARBITRATION_WRR;
	link.prio_wfq = 0;

	if (dp_node_link_add(&link, 0)) {
		PR_ERR("dp_node_link_add_31 fail: cqm_deq_port=%d\n",
		       info->cqe_deq);
		return -1;
	}
	//info->f_deq_port_en = 1;
	info->qid = link.node_id.q_id;
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "qid=%d p_node_id=%d for cqm port=%d\n",
		 link.node_id.q_id,
		 link.p_node_id.cqm_deq_port, info->cqe_deq);
	return 0;
}
#endif /*CONFIG_LTQ_DATAPATH_QOS_HAL*/

int dp_platform_queue_set(int inst, u32 flag)
{
	int ret, i;
	u32 mode;
	struct ppv4_queue q = {0};
	cbm_queue_map_entry_t lookup = {0};
	struct cbm_cpu_port_data cpu_data = {0};
	struct ppv4_q_sch_port q_port = {0};
	u8 f_cpu_q = 0;
	struct cbm_dp_en_data en_data = {0};
	struct hal_priv *priv = (struct hal_priv *)dp_port_prop[inst].priv_hal;
	struct pmac_port_info *port_info;

	port_info = &dp_port_info[inst][0]; /*CPU*/
	if ((flag & DP_PLATFORM_DE_INIT) == DP_PLATFORM_DE_INIT) {
		PR_ERR("Need to free resoruce in the future\n");
		return 0;
	}

	/*Allocate a drop queue*/
	if (priv->ppv4_drop_q < 0) {
		q.parent = 0;
		q.inst = inst;
		if (dp_pp_alloc_queue(&q)) {
			PR_ERR("%s fail to alloc a drop queue ??\n",
			       "dp_pp_alloc_queue");
			return -1;
		}
		priv->ppv4_drop_q = q.qid;
	} else {
		PR_INFO("drop queue: %d\n", priv->ppv4_drop_q);
	}
	/*Map all lookup entry to drop queue at the beginning*/
	cbm_queue_map_set(dp_port_prop[inst].cbm_inst, priv->ppv4_drop_q,
			  &lookup,
			  CBM_QUEUE_MAP_F_FLOWID_L_DONTCARE |
			  CBM_QUEUE_MAP_F_FLOWID_H_DONTCARE |
			  CBM_QUEUE_MAP_F_SUBIF_DONTCARE |
			  CBM_QUEUE_MAP_F_EN_DONTCARE |
			  CBM_QUEUE_MAP_F_DE_DONTCARE |
			  CBM_QUEUE_MAP_F_MPE1_DONTCARE |
			  CBM_QUEUE_MAP_F_MPE2_DONTCARE |
			  CBM_QUEUE_MAP_F_EP_DONTCARE |
			  CBM_QUEUE_MAP_F_TC_DONTCARE);

	/*Set CPU port to Mode0 only*/
	dp_port_info[inst][0].cqe_lu_mode = CQE_LU_MODE0;
	mode = CQE_LU_MODE0;
	lookup.ep = PMAC_CPU_ID;
	cqm_mode_table_set(dp_port_prop[inst].cbm_inst, &lookup,
			   mode,
			   CBM_QUEUE_MAP_F_MPE1_DONTCARE |
			   CBM_QUEUE_MAP_F_MPE2_DONTCARE);

	/*Alloc queue/scheduler/port per CPU port */
	cpu_data.dp_inst = inst;
	cpu_data.cbm_inst = dp_port_prop[inst].cbm_inst;
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DDR_SIMULATE_GSWIP31)
	cpu_data.dq_tx_push_info[0].deq_port = 0;
	cpu_data.dq_tx_push_info[1].deq_port = -1;
	cpu_data.dq_tx_push_info[2].deq_port = -1;
	cpu_data.dq_tx_push_info[3].deq_port = -1;
#else
	ret = cbm_cpu_port_get(&cpu_data, 0);
#endif
	if (ret == -1) {
		PR_ERR("%s fail for CPU Port. Why ???\n",
		       "cbm_cpu_port_get");
		return -1;
	}
	port_info->deq_port_base = 0;
	port_info->deq_port_num = 4;  /*need improve later*/
	for (i = 0; i < ARRAY_SIZE(cpu_data.dq_tx_push_info); i++) {
		if (cpu_data.dq_tx_push_info[i].deq_port == (u32)-1)
			continue;
		DP_DEBUG(DP_DBG_FLAG_QOS, "cpu(%d) deq_port=%d",
			 i, cpu_data.dq_tx_push_info[i].deq_port);
		q_port.cqe_deq = cpu_data.dq_tx_push_info[i].deq_port;
		q_port.tx_pkt_credit = cpu_data.dq_tx_push_info[i].
							tx_pkt_credit;
		q_port.tx_ring_addr = cpu_data.dq_tx_push_info[i].tx_ring_addr;
		q_port.tx_ring_size = cpu_data.dq_tx_push_info[i].tx_ring_size;

		/*Sotre Ring Info */
		dp_deq_port_tbl[inst][q_port.cqe_deq].tx_pkt_credit =
			cpu_data.dq_tx_push_info[i].tx_pkt_credit;
		dp_deq_port_tbl[inst][q_port.cqe_deq].tx_ring_addr =
			cpu_data.dq_tx_push_info[i].tx_ring_addr;
		dp_deq_port_tbl[inst][q_port.cqe_deq].tx_ring_size =
			cpu_data.dq_tx_push_info[i].tx_ring_size;
		dp_deq_port_tbl[inst][q_port.cqe_deq].dp_port = 0;/* CPU */
		DP_DEBUG(DP_DBG_FLAG_QOS, "Store CPU ring info\n");
		DP_DEBUG(DP_DBG_FLAG_QOS, "  ring_address[%d]=0x%x\n",
			 q_port.cqe_deq,
			 dp_deq_port_tbl[inst][q_port.cqe_deq].tx_ring_addr);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  ring_size[%d]=%d\n",
			 q_port.cqe_deq,
			 dp_deq_port_tbl[inst][q_port.cqe_deq].tx_ring_size);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  credit[%d]=%d\n",
			 q_port.cqe_deq,
			 dp_deq_port_tbl[inst][q_port.cqe_deq].tx_pkt_credit);
		q_port.inst = inst;
		q_port.dp_port = PMAC_CPU_ID;
		DP_DEBUG(DP_DBG_FLAG_QOS, "CPU[%d] ring addr=%x\n", i,
			 cpu_data.dq_tx_push_info[i].tx_ring_addr);
		q_port.ctp = i; /*fake CTP for CPU port to store its qid*/
		DP_DEBUG(DP_DBG_FLAG_QOS, "alloc_q_to_port...\n");
		if (alloc_q_to_port(&q_port, 0)) {
			PR_ERR("alloc_q_to_port fail for dp_port=%d\n",
			       q_port.dp_port);
			return -1;
		}
		port_info->deq_port_num++;
		port_info->subif_info[i].qid = q_port.qid;
		port_info->subif_info[i].q_node = q_port.q_node;
		port_info->subif_info[i].qos_deq_port = q_port.port_node;
		port_info->subif_info[i].cqm_deq_port = q_port.cqe_deq;
		if (!f_cpu_q) {
			f_cpu_q = 1;
			/*Map all CPU port's lookup to its 1st queue only */
			lookup.ep = PMAC_CPU_ID;
			cbm_queue_map_set(dp_port_prop[inst].cbm_inst,
					  q_port.qid,
					  &lookup,
					  CBM_QUEUE_MAP_F_FLOWID_L_DONTCARE |
					  CBM_QUEUE_MAP_F_FLOWID_H_DONTCARE |
					  CBM_QUEUE_MAP_F_SUBIF_DONTCARE |
					  CBM_QUEUE_MAP_F_EN_DONTCARE |
					  CBM_QUEUE_MAP_F_DE_DONTCARE |
					  CBM_QUEUE_MAP_F_MPE1_DONTCARE |
					  CBM_QUEUE_MAP_F_MPE2_DONTCARE |
					  CBM_QUEUE_MAP_F_TC_DONTCARE);
		}
		/*Note:
		 *    CPU port no DMA and don't set en_data.dma_chnl_init to 1
		 */
		en_data.cbm_inst = dp_port_prop[inst].cbm_inst;
		en_data.dp_inst = inst;
		en_data.deq_port = cpu_data.dq_tx_push_info[i].deq_port;
		if (cbm_dp_enable(NULL, PMAC_CPU_ID, &en_data, 0, 0)) {
			PR_ERR("Fail to enable CPU[%d]\n", en_data.deq_port);
			return -1;
		}
	}
	return 0;
}

static int dp_platform_set(int inst, u32 flag)
{
	GSW_QoS_portRemarkingCfg_t port_remark;
	struct core_ops *gsw_handle;
	struct hal_priv *priv;

	/* For initialize */
	if ((flag & DP_PLATFORM_INIT) == DP_PLATFORM_INIT) {
		dp_port_prop[inst].priv_hal =
			kzalloc(sizeof(*priv), GFP_KERNEL);
		if (!dp_port_prop[inst].priv_hal) {
			PR_ERR("kmalloc failed: %d bytes\n",
			       sizeof(struct hal_priv));
			return -1;
		}
		priv = (struct hal_priv *)dp_port_prop[inst].priv_hal;
		priv->ppv4_drop_q = MAX_QUEUE - 1; /*Need change later */
		gsw_handle = dp_port_prop[inst].ops[0];
		if (!inst)/*only inst zero need DMA descriptor */
			init_dma_desc_mask();
		if (!dp_port_prop[inst].ops[0] ||
		    !dp_port_prop[inst].ops[1]) {
			PR_ERR("Why GSWIP handle Zero\n");
			return -1;
		}
		if (!inst)
			dp_sub_proc_install_31();
		init_qos_fn();
		/*just for debugging purpose */
		dp_port_info[inst][0].subif_info[0].bp = CPU_BP;
		INIT_LIST_HEAD(&dp_port_info[inst][0].subif_info[0].logic_dev);

		priv->bp_def = alloc_bridge_port(inst, CPU_PORT, CPU_SUBIF,
						 CPU_FID, CPU_BP);
		DP_DEBUG(DP_DBG_FLAG_DBG, "bp_def[%d]=%d\n",
			 inst, priv->bp_def);

		if (!inst) /*only inst zero will support mib feature */
			mib_init(0);
		dp_get_gsw_parser_31(NULL, NULL, NULL, NULL);
#ifdef CONFIG_LTQ_DATAPATH_CPUFREQ
	if (!inst)
		dp_coc_cpufreq_init();
#endif
		/*disable egress VLAN modification for CPU port*/
		port_remark.nPortId = 0;
		if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops
				 .QoS_PortRemarkingCfgGet,
				 gsw_handle, &port_remark)) {
			PR_ERR("GSW_QOS_PORT_REMARKING_CFG_GET failed\n");
			return -1;
		}
		port_remark.bPCP_EgressRemarkingEnable = 0;
		if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops
				 .QoS_PortRemarkingCfgGet,
				 gsw_handle, &port_remark)) {
			PR_ERR("GSW_QOS_PORT_REMARKING_CFG_GET failed\n");
			return -1;
		}

		if (init_ppv4_qos(inst, flag)) {
			PR_ERR("init_ppv4_qos fail\n");
			return -1;
		}
		if (dp_platform_queue_set(inst, flag)) {
			PR_ERR("dp_platform_queue_set fail\n");

			return -1;
		}
		if (cpu_vlan_mod_dis(inst)) {
			PR_ERR("cpu_vlan_mod_dis fail\n");
			return -1;
		}
		return 0;
	}

	/* De-initialize */
	priv = (struct hal_priv *)dp_port_prop[inst].priv_hal;
	if (priv->bp_def)
		free_bridge_port(inst, priv->bp_def);
	init_ppv4_qos(inst, flag); /*de-initialize qos */
	kfree(priv);
	dp_port_prop[inst].priv_hal = NULL;
	return 0;
}

#define DP_GSWIP_CRC_DISABLE 1
#define DP_GSWIP_FLOW_CTL_DISABLE 4
static int pon_config(int inst, int ep, struct dp_port_data *data, u32 flags)
{
	struct core_ops *gsw_handle;
	GSW_return_t ret;
	struct mac_ops *mac_ops;
	GSW_CPU_PortCfg_t cpu_port_cfg;

	mac_ops = dp_port_prop[inst].mac_ops[ep];
	gsw_handle = dp_port_prop[inst].ops[GSWIP_L];
	memset((void *)&cpu_port_cfg, 0x00, sizeof(cpu_port_cfg));
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.CPU_PortCfgGet,
			   gsw_handle, &cpu_port_cfg);
	if (ret != GSW_statusOk) {
		PR_ERR("fail in getting CPU port config\r\n");
		return -1;
	}
	/* Enable the Egress and Ingress Special Tag */
	cpu_port_cfg.nPortId = ep;
	cpu_port_cfg.bSpecialTagIngress = 1;
	cpu_port_cfg.bSpecialTagEgress = 1;
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.CPU_PortCfgSet,
			   gsw_handle, &cpu_port_cfg);
	if (ret != GSW_statusOk) {
		PR_ERR("Fail in configuring CPU port\n");
		return -1;
	}

	/* Disable Rx CRC check. Value '0'-enable, '1'-disable */
	mac_ops->set_rx_crccheck(mac_ops, DP_GSWIP_CRC_DISABLE);

	/* TX FCS generation disable, Padding Insertion disable. */
	if (data->flag_ops & DP_F_DATA_FCS_DISABLE)
		mac_ops->set_fcsgen(mac_ops, GSW_CRC_PAD_INS_DIS);

	/* Disables RX/TX Flow control */
	mac_ops->set_flow_ctl(mac_ops, DP_GSWIP_FLOW_CTL_DISABLE);

	/* Replace Tx Special Tag Byte 2 & Byte 3 with packet length */
	mac_ops->mac_op_cfg(mac_ops, TX_SPTAG_REPLACE);

	/* Indicate GSWIP that packet coming from PON have timestamp
	 * In acceleration path, GSWIP can remove the timestamp
	 */
	mac_ops->mac_op_cfg(mac_ops, RX_TIME_NO_INSERT);

	return 0;
}

static int dp_port_spl_cfg(int inst, int ep, struct dp_port_data *data,
			   u32 flags)
{
	if (flags & (DP_F_GPON | DP_F_EPON))
		pon_config(inst, ep, data, flags);
	return 0;
}

static int port_platform_set(int inst, u8 ep, struct dp_port_data *data,
			     u32 flags)
{
	int idx, i;
	u32 mode;
	cbm_queue_map_entry_t lookup = {0};
	struct hal_priv *priv = (struct hal_priv *)dp_port_prop[inst].priv_hal;
	struct gsw_itf *itf;
	struct pmac_port_info *port_info = &dp_port_info[inst][ep];

	if (!priv) {
		PR_ERR("priv is NULL\n");
		return DP_FAILURE;
	}
	itf = ctp_port_assign(inst, ep, priv->bp_def, flags);
	/*reset_gsw_itf(ep); */
	dp_port_info[inst][ep].itf_info = itf;
	if (flags & DP_F_DEREGISTER) {
		dp_node_reserve(inst, ep, NULL, flags);
		return 0;
	}

	DP_DEBUG(DP_DBG_FLAG_QOS, "priv=%p deq_port_stat=%p qdev=%p\n",
		 priv,
		 priv ? priv->deq_port_stat : NULL,
		 priv ? priv->qdev : NULL);
	idx = port_info->deq_port_base;

	for (i = 0; i < port_info->deq_port_num; i++) {
		dp_deq_port_tbl[inst][i + idx].tx_ring_addr =
			port_info->tx_ring_addr +
			(port_info->tx_ring_offset * i);
		dp_deq_port_tbl[inst][i + idx].tx_ring_size =
			port_info->tx_ring_size;
		dp_deq_port_tbl[inst][i + idx].tx_pkt_credit =
			port_info->tx_pkt_credit;
		dp_deq_port_tbl[inst][i + idx].dp_port = ep;
	}
	mode = dp_port_info[inst][ep].cqe_lu_mode;
	lookup.ep = ep;
	/*Set all mode based on MPE1/2 to same single mode as specified */
	cqm_mode_table_set(dp_port_prop[inst].cbm_inst, &lookup,
			   mode,
			   CBM_QUEUE_MAP_F_MPE1_DONTCARE |
			   CBM_QUEUE_MAP_F_MPE2_DONTCARE);

	dp_node_reserve(inst, ep, data, flags);
	dp_port_spl_cfg(inst, ep, data, flags);
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DBG)
	if (DP_DBG_FLAG_QOS & dp_dbg_flag) {
		for (i = 0; i < port_info->deq_port_num; i++) {
			PR_INFO("cqm[%d]: addr=%x credit=%d size==%d\n",
				i + idx,
				dp_deq_port_tbl[inst][i + idx].tx_ring_addr,
				dp_deq_port_tbl[inst][i + idx].tx_pkt_credit,
				dp_deq_port_tbl[inst][i + idx].tx_ring_size);
		}
	}
#endif
	return 0;
}

static int set_ctp_bp(int inst, int ctp, int portid, int bp)
{
	GSW_CTP_portConfig_t tmp;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[GSWIP_L];
	memset(&tmp, 0, sizeof(tmp));
	tmp.nLogicalPortId = portid;
	tmp.nSubIfIdGroup = ctp;
	tmp.eMask = GSW_CTP_PORT_CONFIG_MASK_BRIDGE_PORT_ID;
	tmp.nBridgePortId = bp;
	if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_ctp_ops.CTP_PortConfigSet,
			 gsw_handle, &tmp) != 0) {
		PR_ERR("Failed to CTP(%d)'s bridge port=%d for ep=%d\n",
		       ctp, bp, portid);
		return -1;
	}

	return 0;
}

static int reset_ctp_bp(int inst, int ctp, int portid, int bp)
{
	GSW_CTP_portConfig_t tmp;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[GSWIP_L];
	memset(&tmp, 0, sizeof(tmp));
	tmp.nLogicalPortId = portid;
	tmp.nSubIfIdGroup = ctp;
	tmp.nBridgePortId = bp;
	if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_ctp_ops.CTP_PortConfigReset,
			 gsw_handle, &tmp) != 0) {
		PR_ERR("Failed to reset CTP(%d)'s bridge port=%d for ep=%d\n",
		       ctp, bp, portid);
		return -1;
	}

	return 0;
}

static char *q_flag_str(int q_flag)
{
	if (q_flag == DP_SUBIF_AUTO_NEW_Q)
		return "auto_new_queue";
	if (q_flag == DP_SUBIF_SPECIFIC_Q)
		return "specified_queue";
	return "sharing_queue";
}

static int subif_hw_set(int inst, int portid, int subif_ix,
			struct subif_platform_data *data, u32 flags)
{
	struct ppv4_q_sch_port q_port = {0};
	static cbm_queue_map_entry_t lookup = {0};
	u32 lookup_f = CBM_QUEUE_MAP_F_FLOWID_L_DONTCARE |
		CBM_QUEUE_MAP_F_FLOWID_H_DONTCARE |
		CBM_QUEUE_MAP_F_EN_DONTCARE |
		CBM_QUEUE_MAP_F_DE_DONTCARE |
		CBM_QUEUE_MAP_F_MPE1_DONTCARE |
		CBM_QUEUE_MAP_F_MPE2_DONTCARE |
		CBM_QUEUE_MAP_F_TC_DONTCARE;
	int subif, deq_port_idx = 0, bp = -1;
	struct pmac_port_info *port_info;
	struct hal_priv *priv = HAL(inst);
	int q_flag = 0;

	if (!data || !data->subif_data) {
		PR_ERR("data NULL or subif_data NULL\n");
		return -1;
	}
	port_info = &dp_port_info[inst][portid];
	subif = SET_VAP(subif_ix, port_info->vap_offset,
			port_info->vap_mask);

	if (data->subif_data->ctp_dev) /* for pmapper later */
		bp = bp_pmapper_dev_get(inst, data->dev);
	if (bp >= 0) {
		DP_DEBUG(DP_DBG_FLAG_DBG, "%s Reuse BP(%d) ctp_dev=%s\n",
			 data->dev ? data->dev->name : "NULL",
			 bp,
			 data->subif_data->ctp_dev->name);
	} else {
		DP_DEBUG(DP_DBG_FLAG_DBG, "need alloc BP for %s ctp_dev=%s\n",
			 data->dev ? data->dev->name : "NULL",
			 data->subif_data->ctp_dev ?
				data->subif_data->ctp_dev->name : "NULL");
		bp = alloc_bridge_port(inst, portid,
				       subif_ix, CPU_FID, CPU_BP);
		if (bp < 0) {
			PR_ERR("Fail to alloc bridge port\n");
			return -1;
		}
	}
	port_info->subif_info[subif_ix].bp = bp;
	set_ctp_bp(inst, subif_ix, portid,
		   port_info->subif_info[subif_ix].bp);
	data->act = 0;
	if (flags & DP_F_SUBIF_LOGICAL) {
		PR_ERR("need more for logical dev??\n");
		return 0;
	}
	if (data->subif_data->ctp_dev) {
		DP_DEBUG(DP_DBG_FLAG_DBG,
			 "dp_bp_dev_tbl[%d][%d]=%s current reg_cnt=%d\n",
			 inst, bp, data->dev->name,
			 dp_bp_dev_tbl[inst][bp].ref_cnt);
		dp_bp_dev_tbl[inst][bp].dev = data->dev;
		dp_bp_dev_tbl[inst][bp].ref_cnt++;
		dp_bp_dev_tbl[inst][bp].flag = 1;
		port_info->subif_info[subif_ix].ctp_dev =
			data->subif_data->ctp_dev;
	}
	DP_DEBUG(DP_DBG_FLAG_DBG,
		 "inst=%d portid=%d dp numsubif=%d subif_ix=%d pmapper.cnt=%d\n",
		 inst, portid,
		 port_info->num_subif, subif_ix,
		 dp_bp_dev_tbl[inst][bp].ref_cnt);
	if (data->subif_data)
		deq_port_idx = data->subif_data->deq_port_idx;
	if (port_info->deq_port_num < deq_port_idx + 1) {
		PR_ERR("Wrong deq_port_idx(%d), should < %d\n",
		       deq_port_idx, port_info->deq_port_num);
		return -1;
	}
	/*QUEUE_CFG if needed */
	q_port.cqe_deq = port_info->deq_port_base + deq_port_idx;
	if (!priv) {
		PR_ERR("priv NULL\n");
		return -1;
	}
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DBG)
	if (unlikely(dp_dbg_flag & DP_DBG_FLAG_QOS)) {
		DP_DEBUG(DP_DBG_FLAG_QOS, "cqe_deq=%d\n", q_port.cqe_deq);
		DP_DEBUG(DP_DBG_FLAG_QOS, "priv=%p deq_port_stat=%p qdev=%p\n",
			 priv,
			 priv ? priv->deq_port_stat : NULL,
			 priv ? priv->qdev : NULL);
		DP_DEBUG(DP_DBG_FLAG_QOS, "cqe_deq=%d inst=%d\n",
			 q_port.cqe_deq, inst);
	}
#endif
	q_port.tx_pkt_credit =
		dp_deq_port_tbl[inst][q_port.cqe_deq].tx_pkt_credit;
	q_port.tx_ring_addr =
		dp_deq_port_tbl[inst][q_port.cqe_deq].tx_ring_addr;
	q_port.tx_ring_size =
		dp_deq_port_tbl[inst][q_port.cqe_deq].tx_ring_size;
	q_port.inst = inst;
	q_port.dp_port = portid;
	q_port.ctp = subif_ix;

	if (data->subif_data->flag_ops & DP_SUBIF_SPECIFIC_Q) {
		q_flag = DP_SUBIF_SPECIFIC_Q;
	} else if (data->subif_data->flag_ops & DP_SUBIF_AUTO_NEW_Q) {
		q_flag = DP_SUBIF_AUTO_NEW_Q;
	}  else { /*sharing mode (default)*/
		if (!dp_deq_port_tbl[inst][q_port.cqe_deq].f_first_qid)
			q_flag = DP_SUBIF_AUTO_NEW_Q; /*no queue created yet*/
	}
	DP_DEBUG(DP_DBG_FLAG_QOS, "Queue decision:%s\n", q_flag_str(q_flag));
	if (q_flag == DP_SUBIF_AUTO_NEW_Q) {
		int cqe_deq;

		if (alloc_q_to_port(&q_port, 0)) {
			PR_ERR("alloc_q_to_port fail for dp_port=%d\n",
			       q_port.dp_port);
			return -1;
		}
		if (dp_q_tbl[inst][q_port.qid].flag) {
			PR_ERR("Why dp_q_tbl[%d][%d].flag =%d:expect 0?\n",
			       inst, q_port.qid,
			       dp_q_tbl[inst][q_port.qid].flag);
			return -1;
		}
		if (dp_q_tbl[inst][q_port.qid].ref_cnt) {
			PR_ERR("Why dp_q_tbl[%d][%d].ref_cnt =%d:expect 0?\n",
			       inst, q_port.qid,
			       dp_q_tbl[inst][q_port.qid].ref_cnt);
			return -1;
		}
		/*update queue table */
		dp_q_tbl[inst][q_port.qid].flag = 1;
		dp_q_tbl[inst][q_port.qid].need_free = 1;
		dp_q_tbl[inst][q_port.qid].ref_cnt = 1;
		dp_q_tbl[inst][q_port.qid].q_node_id = q_port.q_node;
		dp_q_tbl[inst][q_port.qid].cqm_dequeue_port = q_port.cqe_deq;

		/*update port table */
		cqe_deq = q_port.cqe_deq;
		dp_deq_port_tbl[inst][cqe_deq].ref_cnt++;
		dp_deq_port_tbl[inst][cqe_deq].qos_port = q_port.port_node;
		if (!dp_deq_port_tbl[inst][cqe_deq].f_first_qid) {
			dp_deq_port_tbl[inst][cqe_deq].first_qid = q_port.qid;
			dp_deq_port_tbl[inst][cqe_deq].f_first_qid = 1;
			DP_DEBUG(DP_DBG_FLAG_QOS,
				 "dp_deq_port_tbl[%d][%d].first_qid=%d\n",
				 inst, q_port.cqe_deq,
				 dp_deq_port_tbl[inst][cqe_deq].first_qid);
		}
		/*update scheduler table later */

	} else if (q_flag == DP_SUBIF_SPECIFIC_Q) { /*specified queue */
		if (!dp_q_tbl[inst][q_port.qid].flag) {
			/*1st time to use it
			 *In this case, normally this queue is created by caller
			 */
			dp_q_tbl[inst][q_port.qid].flag = 1;
			dp_q_tbl[inst][q_port.qid].need_free = 0; /*caller q*/
			dp_q_tbl[inst][q_port.qid].ref_cnt = 1;

			/*update port table
			 *Note: since this queue is created by caller itself
			 *      we need find way to get cqm_dequeue_port
			 *      and qos_port later
			 */
			/* need set cqm_dequeue_port/qos_port since not fully
			 * tested
			 */
			dp_q_tbl[inst][q_port.qid].cqm_dequeue_port =
				q_port.cqe_deq;
			dp_deq_port_tbl[inst][q_port.cqe_deq].qos_port = -1;
			dp_deq_port_tbl[inst][q_port.cqe_deq].ref_cnt++;
		} else {
			/*note: don't change need_free in this case */
			dp_q_tbl[inst][q_port.cqe_deq].ref_cnt++;
			dp_deq_port_tbl[inst][q_port.cqe_deq].ref_cnt++;
		}

		/*get already stored q_node_id/qos_port id to q_port
		 */
		q_port.q_node = dp_q_tbl[inst][q_port.qid].q_node_id;
		q_port.port_node =
			dp_deq_port_tbl[inst][q_port.cqe_deq].qos_port;

		/* need to further set q_port.q_node/port_node
		 * via special internal QOS HAL API to get it
		 * since it is created by caller itself\n");
		 */

	} else { /*auto sharing queue: if go to here, it means sharing queue
		  *is ready and it is created by previous dp_register_subif_ext
		  */

		/*get already stored q_node_id/qos_port id to q_port
		 */
		q_port.qid = dp_deq_port_tbl[inst][q_port.cqe_deq].first_qid;
		q_port.q_node = dp_deq_port_tbl[inst][q_port.cqe_deq].q_node;
		q_port.port_node =
			dp_deq_port_tbl[inst][q_port.cqe_deq].qos_port;
		dp_q_tbl[inst][q_port.qid].ref_cnt++;
		dp_deq_port_tbl[inst][q_port.cqe_deq].ref_cnt++;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "%s:%s=%d %s=%d q[%d].cnt=%d cqm_p[%d].cnt=%d\n",
		 "subif_hw_set",
		 "dp_port", portid,
		 "vap", subif_ix,
		 q_port.qid, dp_q_tbl[inst][q_port.qid].ref_cnt,
		 q_port.cqe_deq, dp_deq_port_tbl[inst][q_port.cqe_deq].ref_cnt);
#ifdef CONFIG_LTQ_DATAPATH_QOS_HAL
	if (dp_deq_port_tbl[inst][q_port.cqe_deq].ref_cnt == 1) /*first CTP*/
		data->act = TRIGGER_CQE_DP_ENABLE;
#else
	if (q_port.f_deq_port_en)
		data->act = TRIGGER_CQE_DP_ENABLE;
#endif
	/* update caller dp_subif_data.q_id with allocated queue number */
	data->subif_data->q_id = q_port.qid;
	/*update subif table */
	port_info->subif_info[subif_ix].qid = q_port.qid;
	port_info->subif_info[subif_ix].q_node = q_port.q_node;
	port_info->subif_info[subif_ix].qos_deq_port = q_port.port_node;
	port_info->subif_info[subif_ix].cqm_deq_port = q_port.cqe_deq;
	port_info->subif_info[subif_ix].cqm_port_idx = deq_port_idx;

	/* Map this port's lookup to its 1st queue only */
	//lookup.mode = dp_port_info[inst][portid].cqe_lu_mode; /*no need */
	lookup.ep = portid;
	lookup.sub_if_id = subif; /* Note:CQM API need full subif(15bits) */
	/* For 1st subif and mode 0, use CBM_QUEUE_MAP_F_SUBIF_DONTCARE,
	 * otherwise, don't use this flag
	 */
	if (!dp_port_info[inst][portid].num_subif &&
	    (dp_port_info[inst][portid].cqe_lu_mode == CQE_LU_MODE0))
		lookup_f |= CBM_QUEUE_MAP_F_SUBIF_DONTCARE;
	cbm_queue_map_set(dp_port_prop[inst].cbm_inst,
			  q_port.qid,
			  &lookup,
			  lookup_f);
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "%s %s=%d %s=%d %s=%d %s=%d %s=0x%x %s=0x%x(%d)\n",
		 "cbm_queue_map_set",
		 "qid", q_port.qid,
		 "for dp_port", lookup.ep,
		 "num_subif", dp_port_info[inst][portid].num_subif,
		 "lu_mode", dp_port_info[inst][portid].cqe_lu_mode,
		 "flag", lookup_f,
		 "subif", subif, subif_ix);
	return 0;
}

static int subif_hw_reset(int inst, int portid, int subif_ix,
			  struct subif_platform_data *data, u32 flags)
{
	int qid;
	int cqm_deq_port;
	struct pmac_port_info *port_info = &dp_port_info[inst][portid];
	struct dp_node_alloc node;
	int bp = port_info->subif_info[subif_ix].bp;

	qid = port_info->subif_info[subif_ix].qid;
	cqm_deq_port = port_info->subif_info[subif_ix].cqm_deq_port;
	bp = port_info->subif_info[subif_ix].bp;
	/* santity check table */
	if (!dp_q_tbl[inst][qid].ref_cnt) {
		PR_ERR("Why dp_q_tbl[%d][%d].ref_cnt Zero: expect > 0\n",
		       inst, qid);
		return DP_FAILURE;
	}
	if (!dp_deq_port_tbl[inst][cqm_deq_port].ref_cnt) {
		PR_ERR("Why dp_deq_port_tbl[%d][%d].ref_cnt Zero\n",
		       inst, cqm_deq_port);
		return DP_FAILURE;
	}
	if ((port_info->subif_info[subif_ix].ctp_dev) &&
	    !dp_bp_dev_tbl[inst][bp].ref_cnt) {
		PR_ERR("Why dp_bp_dev_tbl[%d][%d].ref_cnt =%d\n",
		       inst, bp, dp_bp_dev_tbl[inst][bp].ref_cnt);
		return DP_FAILURE;
	}
	/* update queue/port/sched/bp_pmapper table's ref_cnt */
	dp_q_tbl[inst][qid].ref_cnt--;
	dp_deq_port_tbl[inst][cqm_deq_port].ref_cnt--;
	if (port_info->subif_info[subif_ix].ctp_dev) { /* pmapper */
		port_info->subif_info[subif_ix].ctp_dev = NULL;
		dp_bp_dev_tbl[inst][bp].ref_cnt--;
		if (!dp_bp_dev_tbl[inst][bp].ref_cnt) {
			dp_bp_dev_tbl[inst][bp].dev = NULL;
			dp_bp_dev_tbl[inst][bp].flag = 0;
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "ctp ref_cnt becomes zero:%s\n",
				 port_info->subif_info[subif_ix].device_name);
		}
	}

	reset_ctp_bp(inst, subif_ix, portid, bp);
	if (!dp_bp_dev_tbl[inst][bp].dev) /*NULL already, then free it */ {
		DP_DEBUG(DP_DBG_FLAG_PAE, "Free BP[%d] vap=%d\n",
			 bp, subif_ix);
		free_bridge_port(inst, bp);
	}
#ifdef CONFIG_LTQ_DATAPATH_QOS_HAL
	qid = port_info->subif_info[subif_ix].qid;
	cqm_deq_port = dp_q_tbl[inst][qid].cqm_dequeue_port;

	if (dp_q_tbl[inst][qid].flag &&
	    !dp_q_tbl[inst][qid].ref_cnt &&/*no one is using */
	    dp_q_tbl[inst][qid].need_free) {
		DP_DEBUG(DP_DBG_FLAG_QOS, "Free qid %d\n", qid);
		node.id.q_id = qid;
		/*if no subif using this queue, need to delete it*/
		node.inst = inst;
		node.dp_port = portid;
		node.type = DP_NODE_QUEUE;
		dp_node_free(&node, 0);

		/*update dp_q_tbl*/
		dp_q_tbl[inst][qid].flag = 0;
		dp_q_tbl[inst][qid].need_free = 0;
		if (dp_deq_port_tbl[inst][cqm_deq_port].f_first_qid &&
		    (dp_deq_port_tbl[inst][cqm_deq_port].first_qid
		     == qid)) {
			dp_deq_port_tbl[inst][cqm_deq_port].f_first_qid = 0;
			dp_deq_port_tbl[inst][cqm_deq_port].first_qid = 0;
			DP_DEBUG(DP_DBG_FLAG_QOS, "q_id[%d] is freed\n", qid);
			DP_DEBUG(DP_DBG_FLAG_QOS,
				 "dp_deq_port_tbl[%d][%d].f_first_qid reset\n",
				 inst, cqm_deq_port);
		}
	} else {
		DP_DEBUG(DP_DBG_FLAG_QOS, "q_id[%d] dont need freed\n", qid);
	}
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "%s:%s=%d %s=%d q[%d].cnt=%d cqm_p[%d].cnt=%d\n",
		 "subif_hw_reset",
		 "dp_port", portid,
		 "vap", subif_ix,
		 qid, dp_q_tbl[inst][qid].ref_cnt,
		 cqm_deq_port, dp_deq_port_tbl[inst][cqm_deq_port].ref_cnt);
#else
	qos_queue_flush(priv->qdev, port_info->subif_info[subif_ix].q_node);
	qos_queue_remove(priv->qdev, port_info->subif_info[subif_ix].q_node);
	qos_port_remove(priv->qdev,
			port_info->subif_info[subif_ix].qos_deq_port);
	priv->deq_port_stat[port_info->subif_info[subif_ix].cqm_deq_port].flag =
		PP_NODE_FREE;
#endif /* CONFIG_LTQ_DATAPATH_QOS_HAL */

	if (!port_info->num_subif &&
	    dp_deq_port_tbl[inst][cqm_deq_port].ref_cnt) {
		PR_ERR("num_subif(%d) not match dp_deq_port[%d][%d].ref_cnt\n",
		       port_info->num_subif,
		       inst, cqm_deq_port);
		return DP_FAILURE;
	}

	return DP_SUCCESS;
}

/*Set basic BP/CTP */
static int subif_platform_set(int inst, int portid, int subif_ix,
			      struct subif_platform_data *data, u32 flags)
{
	if (flags & DP_F_DEREGISTER)
		return subif_hw_reset(inst, portid, subif_ix, data, flags);
	return subif_hw_set(inst, portid, subif_ix, data, flags);
}

static int supported_logic_dev(int inst, struct net_device *dev,
			       char *subif_name)
{
	return is_vlan_dev(dev);
}

static int subif_platform_set_unexplicit(int inst, int port_id,
					 struct logic_dev *logic_dev,
					 u32 flags)
{
	if (flags & DP_F_DEREGISTER) {
		free_bridge_port(inst, logic_dev->bp);
	} else {
		logic_dev->bp =
			alloc_bridge_port(inst, port_id, logic_dev->ctp,
					  CPU_FID, CPU_BP);
	}

	return 0;
}

static int dp_ctp_tc_map_set_31(struct dp_tc_cfg *tc, int flag,
				struct dp_meter_subif *mtr_subif)
{
	struct core_ops *gsw_handle;
	GSW_CTP_portConfig_t ctp_tc_cfg;

	memset(&ctp_tc_cfg, 0, sizeof(ctp_tc_cfg));

	if (!mtr_subif) {
		PR_ERR("mtr_subif struct NULL\n");
		return -1;
	}
	if (mtr_subif->subif.flag_pmapper) {
		PR_ERR("Cannot support ctp tc set for pmmapper dev(%s)\n",
		       tc->dev ? tc->dev->name : "NULL");
		return -1;
	}
	gsw_handle = dp_port_prop[mtr_subif->inst].ops[GSWIP_L];
	ctp_tc_cfg.nLogicalPortId = mtr_subif->subif.port_id;
	ctp_tc_cfg.nSubIfIdGroup = mtr_subif->subif.subif;
	if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_ctp_ops.CTP_PortConfigGet,
			 gsw_handle, &ctp_tc_cfg) != 0) {
		PR_ERR("Failed to get CTP info for ep=%d subif=%d\n",
		       mtr_subif->subif.port_id, mtr_subif->subif.subif);
		return -1;
	}
	ctp_tc_cfg.eMask = GSW_CTP_PORT_CONFIG_MASK_FORCE_TRAFFIC_CLASS;
	ctp_tc_cfg.nDefaultTrafficClass = tc->tc;
	if (tc->force)
		ctp_tc_cfg.bForcedTrafficClass = tc->force;
	else
		ctp_tc_cfg.bForcedTrafficClass = 0;

	if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_ctp_ops.CTP_PortConfigSet,
			 gsw_handle, &ctp_tc_cfg) != 0) {
		PR_ERR("CTP tc set fail for ep=%d subif=%d tc=%d force=%d\n",
		       mtr_subif->subif.port_id, mtr_subif->subif.subif,
		       tc->tc, tc->force);
		return -1;
	}
	return 0;
}

static int not_valid_rx_ep(int ep)
{
	return (((ep >= 3) && (ep <= 6)) || (ep == 2) || (ep > 15));
}

static void set_pmac_subif(struct pmac_tx_hdr *pmac, int32_t subif)
{
	pmac->src_dst_subif_id_lsb = subif & 0xff;
	pmac->src_dst_subif_id_msb =  (subif >> 8) & 0x1f;
}

static void update_port_vap(int inst, u32 *ep, int *vap,
			    struct sk_buff *skb,
			    struct pmac_rx_hdr *pmac, char *decryp)
{
	//*ep = pmac->igp_egp; /*get the port_id from pmac's sppid */
	*ep = (skb->DW1 >> 4) & 0xF; /*get the port_id from pmac's sppid */
	if (dp_port_info[inst][*ep].alloc_flags & DP_F_LOOPBACK) {
		/*get the real source port from VAP for ipsec */
		/* related tunnel decap case */
		*ep = GET_VAP((u32)pmac->src_dst_subif_id_lsb +
			(u32)(pmac->src_dst_subif_id_msb << 8),
			PORT_INFO(inst, *ep, vap_offset),
			PORT_INFO(inst, *ep, vap_mask));
		*vap = 0;
		*decryp = 1;
	} else {
		struct dma_rx_desc_1 *desc_1;

		desc_1 = (struct dma_rx_desc_1 *)&skb->DW1;
		*vap = desc_1->field.session_id;
	}
}

static void get_dma_pmac_templ(int index, struct pmac_tx_hdr *pmac,
			       struct dma_tx_desc_0 *desc_0,
			       struct dma_tx_desc_1 *desc_1,
			       struct pmac_port_info2 *dp_info)
{
	if (likely(pmac))
		memcpy(pmac, &dp_info->pmac_template[index], sizeof(*pmac));
	desc_0->all = (desc_0->all & dp_info->dma0_mask_template[index].all) |
				dp_info->dma0_template[index].all;
	desc_1->all = (desc_1->all & dp_info->dma1_mask_template[index].all) |
				dp_info->dma1_template[index].all;
}

static int check_csum_cap(void)
{
	return 0;
}

static int get_itf_start_end(struct gsw_itf *itf_info, u16 *start, u16 *end)
{
	if (!itf_info)
		return -1;
	if (start)
		*start = itf_info->start;
	if (end)
		*end = itf_info->end;

	return 0;
}

int register_dp_cap_gswip31(int flag)
{
	struct dp_hw_cap cap;

	memset(&cap, 0, sizeof(cap));
	cap.info.type = GSWIP31_TYPE;
	cap.info.ver = GSWIP31_VER;

	cap.info.dp_platform_set = dp_platform_set;
	cap.info.port_platform_set = port_platform_set;
	cap.info.subif_platform_set_unexplicit = subif_platform_set_unexplicit;
	cap.info.proc_print_ctp_bp_info = proc_print_ctp_bp_info;
	cap.info.init_dma_pmac_template = init_dma_pmac_template;
	//dp.info.port_platform_set_unexplicit = port_platform_set_unexplicit;
	cap.info.subif_platform_set = subif_platform_set;
	cap.info.init_dma_pmac_template = init_dma_pmac_template;
	cap.info.not_valid_rx_ep = not_valid_rx_ep;
	cap.info.set_pmac_subif = set_pmac_subif;
	cap.info.update_port_vap = update_port_vap;
	cap.info.check_csum_cap = check_csum_cap;
	cap.info.get_dma_pmac_templ = get_dma_pmac_templ;
	cap.info.get_itf_start_end = get_itf_start_end;
	cap.info.dump_rx_dma_desc = dump_rx_dma_desc;
	cap.info.dump_tx_dma_desc = dump_tx_dma_desc;
	cap.info.dump_rx_pmac = dump_rx_pmac;
	cap.info.dump_tx_pmac = dump_tx_pmac;
	cap.info.supported_logic_dev = supported_logic_dev;
	cap.info.dp_pmac_set = dp_pmac_set_31;
	cap.info.dp_set_gsw_parser = dp_set_gsw_parser_31;
	cap.info.dp_get_gsw_parser = dp_get_gsw_parser_31;
	cap.info.dp_qos_platform_set = qos_platform_set;
	cap.info.dp_set_gsw_pmapper = dp_set_gsw_pmapper_31;
	cap.info.dp_get_gsw_pmapper = dp_get_gsw_pmapper_31;
	cap.info.dp_ctp_tc_map_set = dp_ctp_tc_map_set_31;
	cap.info.dp_meter_alloc = dp_meter_alloc_31;
	cap.info.dp_meter_add = dp_meter_add_31;
	cap.info.dp_meter_del = dp_meter_del_31;
#ifdef CONFIG_LTQ_DATAPATH_HAL_GSWIP31_MIB
	cap.info.dp_get_port_vap_mib = dp_get_port_vap_mib_31;
	cap.info.dp_clear_netif_mib = dp_clear_netif_mib_31;
#endif

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
	cap.info.swdev_flag = 1;
	cap.info.swdev_alloc_bridge_id = dp_swdev_alloc_bridge_id;
	cap.info.swdev_free_brcfg = dp_swdev_free_brcfg;
	cap.info.swdev_bridge_cfg_set = dp_swdev_bridge_cfg_set;
	cap.info.swdev_bridge_port_cfg_reset = dp_swdev_bridge_port_cfg_reset;
	cap.info.swdev_bridge_port_cfg_set = dp_swdev_bridge_port_cfg_set;
	cap.info.dp_mac_set = dp_gswip_mac_entry_add;
	cap.info.dp_mac_reset = dp_gswip_mac_entry_del;
	cap.info.dp_cfg_vlan = dp_gswip_ext_vlan; /*for symmetric VLAN */
	cap.info.dp_tc_vlan_set = tc_vlan_set_31;
#endif
	cap.info.cap.tx_hw_chksum = 0;
	cap.info.cap.rx_hw_chksum = 0;
	cap.info.cap.hw_tso = 0;
	cap.info.cap.hw_gso = 0;
	cap.info.cap.hw_ptp = 1;
	strncpy(cap.info.cap.qos_eng_name, "ppv4",
		sizeof(cap.info.cap.qos_eng_name));
	strncpy(cap.info.cap.pkt_eng_name, "mpe",
		sizeof(cap.info.cap.pkt_eng_name));
	cap.info.cap.max_num_queues = MAX_QUEUE;
	cap.info.cap.max_num_scheds = MAX_SCHD;
	cap.info.cap.max_num_deq_ports = MAX_CQM_DEQ;
	cap.info.cap.max_num_qos_ports = MAX_PPV4_PORT;
	cap.info.cap.max_num_dp_ports = PMAC_MAX_NUM;
	cap.info.cap.max_num_subif_per_port = MAX_SUBIF_PER_PORT;
	cap.info.cap.max_num_subif = 288;
	cap.info.cap.max_num_bridge_port = 128;

	if (register_dp_hw_cap(&cap, flag)) {
		PR_ERR("Why register_dp_hw_cap fail\n");
		return -1;
	}

	return 0;
}
