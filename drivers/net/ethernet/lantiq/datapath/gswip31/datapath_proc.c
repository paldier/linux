/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/module.h>
#include <net/datapath_proc_api.h>	/*for proc api */
#include <net/datapath_api.h>
#include <linux/list.h>

#include "../datapath.h"
#include "datapath_misc.h"

#include "../datapath_swdev.h"

#define PROC_PARSER "parser"
#define DP_PROC_CBMLOOKUP "lookup"

struct list_head fdb_tbl_list;

static void proc_parser_read(struct seq_file *s);
static ssize_t proc_parser_write(struct file *, const char *, size_t,
				 loff_t *);

static void proc_parser_read(struct seq_file *s)
{
	s8 cpu, mpe1, mpe2, mpe3;

	dp_get_gsw_parser_31(&cpu, &mpe1, &mpe2, &mpe3);
	seq_printf(s, "cpu : %s with parser size =%d bytes\n",
		   parser_flag_str(cpu), parser_size_via_index(0));
	seq_printf(s, "mpe1: %s with parser size =%d bytes\n",
		   parser_flag_str(mpe1), parser_size_via_index(1));
	seq_printf(s, "mpe2: %s with parser size =%d bytes\n",
		   parser_flag_str(mpe2), parser_size_via_index(2));
	seq_printf(s, "mpe3: %s with parser size =%d bytes\n",
		   parser_flag_str(mpe3), parser_size_via_index(3));
}

ssize_t proc_parser_write(struct file *file, const char *buf,
			  size_t count, loff_t *ppos)
{
	int len;
	char str[64];
	int num, i;
	char *param_list[20];
	s8 cpu = 0, mpe1 = 0, mpe2 = 0, mpe3 = 0, flag = 0;
	int pce_rule_id;
	static GSW_PCE_rule_t pce;
	int inst = 0;
	struct core_ops *gsw_handle;

	memset(&pce, 0, sizeof(pce));
	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (dp_strncmpi(param_list[0],
			"enable",
			strlen("enable")) == 0) {
		for (i = 1; i < num; i++) {
			if (dp_strncmpi(param_list[i],
					"cpu",
					strlen("cpu")) == 0) {
				flag |= 0x1;
				cpu = 2;
			}

			if (dp_strncmpi(param_list[i],
					"mpe1",
					strlen("mpe1")) == 0) {
				flag |= 0x2;
				mpe1 = 2;
			}

			if (dp_strncmpi(param_list[i],
					"mpe2",
					strlen("mpe2")) == 0) {
				flag |= 0x4;
				mpe2 = 2;
			}

			if (dp_strncmpi(param_list[i],
					"mpe3",
					strlen("mpe3")) == 0) {
				flag |= 0x8;
				mpe3 = 2;
			}
		}

		if (!flag) {
			flag = 0x1 | 0x2 | 0x4 | 0x8;
			cpu = 2;
			mpe1 = 2;
			mpe2 = 2;
			mpe3 = 2;
		}

		DP_DEBUG(DP_DBG_FLAG_DBG,
			 "flag=0x%x mpe3/2/1/cpu=%d/%d/%d/%d\n", flag, mpe3,
			 mpe2, mpe1, cpu);
		dp_set_gsw_parser_31(flag, cpu, mpe1, mpe2, mpe3);
	} else if (dp_strncmpi(param_list[0],
				"disable",
				strlen("disable")) == 0) {
		for (i = 1; i < num; i++) {
			if (dp_strncmpi(param_list[i],
					"cpu",
					strlen("cpu")) == 0) {
				flag |= 0x1;
				cpu = 0;
			}

			if (dp_strncmpi(param_list[i],
					"mpe1",
					strlen("mpe1")) == 0) {
				flag |= 0x2;
				mpe1 = 0;
			}

			if (dp_strncmpi(param_list[i],
					"mpe2",
					strlen("mpe2")) == 0) {
				flag |= 0x4;
				mpe2 = 0;
			}

			if (dp_strncmpi(param_list[i],
					"mpe3",
					strlen("mpe3")) == 0) {
				flag |= 0x8;
				mpe3 = 0;
			}
		}

		if (!flag) {
			flag = 0x1 | 0x2 | 0x4 | 0x8;
			cpu = 0;
			mpe1 = 0;
			mpe2 = 0;
			mpe3 = 0;
		}

		DP_DEBUG(DP_DBG_FLAG_DBG,
			 "flag=0x%x mpe3/2/1/cpu=%d/%d/%d/%d\n", flag, mpe3,
			 mpe2, mpe1, cpu);
		dp_set_gsw_parser_31(flag, cpu, mpe1, mpe2, mpe3);
	} else if (dp_strncmpi(param_list[0],
				"refresh",
				strlen("refresh")) == 0) {
		dp_get_gsw_parser_31(NULL, NULL, NULL, NULL);
		PR_INFO("value:cpu=%d mpe1=%d mpe2=%d mpe3=%d\n", pinfo[0].v,
			pinfo[1].v, pinfo[2].v, pinfo[3].v);
		PR_INFO("size :cpu=%d mpe1=%d mpe2=%d mpe3=%d\n",
			pinfo[0].size, pinfo[1].size, pinfo[2].size,
			pinfo[3].size);
		return count;
	} else if (dp_strncmpi(param_list[0], "mark", strlen("mark")) == 0) {
		int flag = dp_atoi(param_list[1]);

		pce_rule_id = dp_atoi(param_list[2]);

		if (flag < 0)
			flag = 0;
		else if (flag > 3)
			flag = 3;
		PR_INFO("eProcessPath_Action set to %d\n", flag);
		/*: All packets set to same mpe flag as specified */
		memset(&pce, 0, sizeof(pce));
		pce.pattern.nIndex = pce_rule_id;
		pce.pattern.bEnable = 1;

		pce.pattern.bParserFlagMSB_Enable = 1;
		/* rule.pce.pattern.nParserFlagMSB = 0x0021; */
		pce.pattern.nParserFlagMSB_Mask = 0xffff;
		pce.pattern.bParserFlagLSB_Enable = 1;
		/* rule.pce.pattern.nParserFlagLSB = 0x0000; */
		pce.pattern.nParserFlagLSB_Mask = 0xffff;
		/* rule.pce.pattern.eDstIP_Select = 2; */

		pce.pattern.nDstIP_Mask = 0xffffffff;
		pce.pattern.bDstIP_Exclude = 0;

		pce.action.bRtDstPortMaskCmp_Action = 1;
		pce.action.bRtSrcPortMaskCmp_Action = 1;
		pce.action.bRtDstIpMaskCmp_Action = 1;
		pce.action.bRtSrcIpMaskCmp_Action = 1;

		pce.action.bRoutExtId_Action = 1;
		pce.action.nRoutExtId = 0; /*RT_EXTID_UDP; */
		pce.action.bRtAccelEna_Action = 1;
		pce.action.bRtCtrlEna_Action = 1;
		pce.action.eProcessPath_Action = flag;
		pce.action.bRMON_Action = 1;
		pce.action.nRMON_Id = 0;	/*RMON_UDP_CNTR; */

		if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_tflow_ops
				 .TFLOW_PceRuleWrite, gsw_handle, &pce)) {
			PR_ERR("PCE rule add fail: GSW_PCE_RULE_WRITE\n");
			return count;
		}

	} else if (dp_strncmpi(param_list[0], "unmark", 6) == 0) {
		/*: All packets set to same mpe flag as specified */
		memset(&pce, 0, sizeof(pce));
		pce_rule_id = dp_atoi(param_list[1]);
		pce.pattern.nIndex = pce_rule_id;
		pce.pattern.bEnable = 0;
		if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_tflow_ops
				 .TFLOW_PceRuleWrite, gsw_handle, &pce)) {
			PR_ERR("PCE rule add fail:GSW_PCE_RULE_WRITE\n");
			return count;
		}
	} else {
		PR_INFO("Usage: echo %s [cpu] [mpe1] [mpe2] [mpe3] > parser\n",
			"<enable/disable>");
		PR_INFO("Usage: echo <refresh> parser\n");

		PR_INFO("Usage: echo %s > parser\n",
			"mark eProcessPath_Action_value(0~3) pce_rule_id");
		PR_INFO("Usage: echo unmark pce_rule_id > parser\n");
		return count;
	}

	return count;
}

char *get_bp_member_string(int inst, u16 bp, char *buf)
{
	GSW_BRIDGE_portConfig_t bp_cfg;
	int i, ret;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[GSWIP_L];
	if (!buf)
		return NULL;
	buf[0] = 0;
	bp_cfg.nBridgePortId = bp;
	bp_cfg.eMask = GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP |
		GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID;
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_brdgport_ops
			   .BridgePort_ConfigGet, gsw_handle, &bp_cfg);
	if (ret != GSW_statusOk) {
		PR_ERR("Failed to get bridge port's member for bridgeport=%d\n",
		       bp_cfg.nBridgePortId);
		return buf;
	}
	for (i = 0; i < MAX_BP_NUM; i++)
		if (GET_BP_MAP(bp_cfg.nBridgePortMap, i))
			sprintf(buf + strlen(buf), "%d ", i);
	sprintf(buf + strlen(buf), " Fid=%d ", bp_cfg.nBridgeId);
	return buf;
}
/* proc_print_ctp_bp_info is an callback API, not a standalone proc API */
int proc_print_ctp_bp_info(struct seq_file *s, int inst,
			   struct pmac_port_info *port,
			   int subif_index, u32 flag)
{
	struct logic_dev *tmp;
	int bp = port->subif_info[subif_index].bp;
	unsigned char *buf = kmalloc(MAX_BP_NUM * 5 + 1, GFP_KERNEL);

	seq_printf(s, "          : bp=%d(member:%s)\n", bp,
		   get_bp_member_string(inst, bp, buf));
	list_for_each_entry(tmp, &port->subif_info[subif_index].logic_dev,
			    list) {
		seq_printf(s, "             %s: bp=%d(member:%s\n",
			   tmp->dev->name, tmp->bp,
			   get_bp_member_string(inst, tmp->bp, buf));
	}

	kfree(buf);
	return 0;
}
static struct dp_proc_entry dp_proc_entries[] = {
	/*name single_callback_t multi_callback_t/_start write_callback_t */
	{PROC_PARSER, proc_parser_read, NULL, NULL, proc_parser_write},
	{DP_PROC_CBMLOOKUP, NULL, lookup_dump31, lookup_start31,
		proc_get_qid_via_index31},

	/*the last place holder */
	{NULL, NULL, NULL, NULL, NULL}
};

int dp_sub_proc_install_31(void)
{
	int i;

	if (!dp_proc_node) {
		PR_ERR("dp_sub_proc_install failed\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(dp_proc_entries); i++)
		dp_proc_entry_create(dp_proc_node, &dp_proc_entries[i]);
	return 0;
}

#define INVALID_DMA_CH 255
char *get_dma_flags_str31(u32 epn, char *buf, int buf_len)
{
	char tmp[30]; /*must be static */
	u32 flags;
	u32 tx_ch, k;
	u8 f_found;
	int i;
	int inst = 0;

	if (!buf || (buf_len < 1))
		return NULL;
	tx_ch = 0;
	flags = 0;
	tmp[0] = '\0';
	f_found = 0;
	for (i = 0; i < ARRAY_SIZE(dp_port_info); i++) {
		if ((dp_port_info[inst][i].flag_other &
		    CBM_PORT_DMA_CHAN_SET) &&
		    (dp_port_info[inst][i].deq_port_base == epn)) {
			tx_ch = dp_port_info[inst][i].dma_chan;
			break;
		}
	}
	if (i >= ARRAY_SIZE(dp_port_info))
		goto EXIT;
	sprintf(tmp, "--");
	if (tx_ch != INVALID_DMA_CH) {
		if (!(flags & DP_F_FAST_DSL))/*DSLupstrem no DMA1/2 TX CHannel*/
			sprintf(tmp + strlen(tmp), "CH%02d ", tx_ch);
		else
			sprintf(tmp + strlen(tmp), "CHXX ");
	} else {
		sprintf(tmp + strlen(tmp), "CHXX ");
	}
	if (flags == 0) {
		if (epn < 4)
			sprintf(tmp + strlen(tmp), "CPU");
		else
			sprintf(tmp + strlen(tmp), "Flag0");

		goto EXIT;
	}
	for (k = 0; k < get_dp_port_type_str_size(); k++) {
		if (flags & dp_port_flag[k]) {
			sprintf(tmp + strlen(tmp), "%s ",
				dp_port_type_str[k]);
			f_found = 1;
		}
	}
	if ((f_found == 1) &&
	    (flags == DP_F_FAST_ETH_LAN)) { /*try to find its ep */
		u32 i, num, j;
		cbm_tmu_res_t *res;
		struct pmac_port_info *port;

		for (i = 3; i <= 4; i++) {	/*2 LAN port */
			num = 0;
			port = get_port_info(inst, i);
			if (!port)
				continue;
			if (cbm_dp_port_resources_get
			    (&i, &num, &res, port->alloc_flags))
				continue;
			for (j = 0; j < num; j++) {
				if (res[j].tmu_port != epn)/* not match */
					continue;
				sprintf(tmp + strlen(tmp), "%d", i);
			}
			kfree(res);
		}
	}
	if (!f_found)
		sprintf(tmp + strlen(tmp), "Unknown[0x%x]\n", flags);

 EXIT:
	strncpy(buf, tmp, buf_len);
	return buf;
}
EXPORT_SYMBOL(get_dma_flags_str31);
