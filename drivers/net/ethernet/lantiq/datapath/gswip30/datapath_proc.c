/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
#include <linux/module.h>
#include <net/datapath_proc_api.h>	/*for proc api*/
#include <net/datapath_api.h>
#include <net/drv_tmu_ll.h>
#include <linux/list.h>
#include <net/lantiq_cbm_api.h>
#include "../../cqm/grx500/reg/fsqm.h" /* hardcoded path*/

#include <linux/list.h>

#include "../datapath.h"
#include "datapath_misc.h"

#define PROC_PARSER "parser"
#define PROC_RMON_PORTS  "rmon"
#define PROC_MIB_TIMER "mib_timer"
#define PROC_MIB_INSIDE "mib_inside"
#define PROC_MIBPORT "mib_port"
#define PROC_CHECKSUM "checksum"
#define PROC_MIBVAP "mib_vap"
#define PROC_COMMON_CMD "cmd"
#define PROC_COC "coc"
#define PROC_CBM_BUF_TEST   "cbm_buf"
#define PROC_PCE  "pce"
#define PROC_ROUTE  "route"
#define PROC_PMAC  "pmac"
#define PROC_EP "ep"	/*EP/port ID info */
#define PROC_DPORT "dport"	/*TMU dequeue port info */
#define DP_PROC_CBMLOOKUP "lookup"

#define MAX_GSW_L_PMAC_PORT  7
#define MAX_GSW_R_PMAC_PORT  16
static GSW_RMON_Port_cnt_t gsw_l_rmon_mib[MAX_GSW_L_PMAC_PORT];
static GSW_RMON_Port_cnt_t gsw_r_rmon_mib[MAX_GSW_R_PMAC_PORT];
static GSW_RMON_Redirect_cnt_t gswr_rmon_redirect;

enum RMON_MIB_TYPE {
	RX_GOOD_PKTS = 0,
	RX_FILTER_PKTS,
	RX_DROP_PKTS,
	RX_OTHERS,

	TX_GOOD_PKTS,
	TX_ACM_PKTS,
	TX_DROP_PKTS,
	TX_OTHERS,

	REDIRECT_MIB,
	DP_DRV_MIB,

	/*last entry */
	RMON_MAX
};

static char f_q_mib_title_proc;
static int tmp_inst;

#define RMON_DASH_LINE 130
static void print_dash_line(struct seq_file *s)
{
	char buf[RMON_DASH_LINE + 4];

	memset(buf, '-', RMON_DASH_LINE);
	sprintf(buf + RMON_DASH_LINE, "\n");
	seq_puts(s, buf);
}

#define GSW_PORT_RMON_PRINT(title, var)  do { \
	seq_printf(s, \
		   "%-14s%10s %12u %12u %12u %12u %12u %12u %12u\n", \
		   title, "L(0-6)", \
		   gsw_l_rmon_mib[0].var, gsw_l_rmon_mib[1].var, \
		   gsw_l_rmon_mib[2].var, gsw_l_rmon_mib[3].var, \
		   gsw_l_rmon_mib[4].var, gsw_l_rmon_mib[5].var, \
		   gsw_l_rmon_mib[6].var); \
	seq_printf(s, \
		   "%-14s%10s %12u %12u %12u %12u %12u %12u %12u %12u\n", \
		   title, "R(0-6,15)", \
		   gsw_r_rmon_mib[0].var, gsw_r_rmon_mib[1].var, \
		   gsw_r_rmon_mib[2].var, gsw_r_rmon_mib[3].var, \
		   gsw_r_rmon_mib[4].var, gsw_r_rmon_mib[5].var, \
		   gsw_r_rmon_mib[6].var, gsw_r_rmon_mib[15].var); \
	seq_printf(s, \
		   "%-14s%10s %12u %12u %12u %12u %12u %12u %12u %12u\n", \
		   title, "R(7-14)", \
		   gsw_r_rmon_mib[7].var, gsw_r_rmon_mib[8].var, \
		   gsw_r_rmon_mib[9].var, gsw_r_rmon_mib[10].var, \
		   gsw_r_rmon_mib[11].var, gsw_r_rmon_mib[12].var, \
		   gsw_r_rmon_mib[13].var, gsw_r_rmon_mib[14].var); \
	print_dash_line(s); \
} while (0)

static void proc_checksum_read(struct seq_file *s);
static void proc_parser_read(struct seq_file *s);
static ssize_t proc_parser_write(struct file *, const char *, size_t,
				 loff_t *);
static ssize_t proc_checksum_write(struct file *file, const char *buf,
				   size_t count, loff_t *ppos);
static ssize_t proc_cbm_buf_write(struct file *file, const char *buf,
				  size_t count, loff_t *ppos);
static void proc_cbm_buf_read(struct seq_file *s);
static int proc_gsw_pce_dump(struct seq_file *s, int pos);
static int proc_gsw_pce_start(void);
static ssize_t proc_gsw_route_write(struct file *file, const char *buf,
				    size_t count, loff_t *ppos);
static ssize_t proc_gsw_pmac_write(struct file *file, const char *buf,
				   size_t count, loff_t *ppos);
static int proc_ep_dump(struct seq_file *s, int pos);
static ssize_t ep_port_write(struct file *, const char *, size_t, loff_t *);
static int proc_dport_dump(struct seq_file *s, int pos);
static int proc_gsw_route_dump(struct seq_file *seq, int pos);
static int rmon_display_tmu_mib = 1;
static int rmon_display_port_full;

#ifndef CONFIG_LTQ_TMU
void tmu_qoct_read(const u32 qid,
		   u32 *wq,
		   u32 *qrth, u32 *qocc, u32 *qavg)
{
}

void tmu_qdct_read(const u32 qid, u32 *qdc)
{
}

char *get_dma_flags_str(u32 epn, char *buf, int buf_len)
{
	return NULL;
}

void tmu_equeue_link_get(const u32 qid, struct tmu_equeue_link *link)
{
}

u32 get_enq_counter(u32 index)
{
	return 0;
}

u32 get_deq_counter(u32 index)
{
	return 0;
}

#endif

static void proc_parser_read(struct seq_file *s)
{
	s8 cpu, mpe1, mpe2, mpe3;

	dp_get_gsw_parser_30(&cpu, &mpe1, &mpe2, &mpe3);
	seq_printf(s, "cpu : %s with parser size =%d bytes\n",
		   parser_flag_str(cpu), parser_size_via_index(0));
	seq_printf(s, "mpe1: %s with parser size =%d bytes\n",
		   parser_flag_str(mpe1), parser_size_via_index(1));
	seq_printf(s, "mpe2: %s with parser size =%d bytes\n",
		   parser_flag_str(mpe2), parser_size_via_index(2));
	seq_printf(s, "mpe3: %s with parser size =%d bytes\n",
		   parser_flag_str(mpe3), parser_size_via_index(3));
}

static ssize_t proc_parser_write(struct file *file, const char *buf,
				 size_t count, loff_t *ppos)
{
	int len;
	char str[64];
	int num, i;
	char *param_list[20];
	s8 cpu = 0, mpe1 = 0, mpe2 = 0, mpe3 = 0, flag = 0;
	static int pce_rule_id;
	static GSW_PCE_rule_t pce;
	int inst = 0;
	struct core_ops *gsw_handle;

	memset(&pce, 0, sizeof(pce));
	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (dp_strncmpi(param_list[0], "enable", strlen("enable")) == 0) {
		for (i = 1; i < num; i++) {
			if (dp_strncmpi(param_list[i],
					"cpu",
					strlen("cpu"))
					== 0) {
				flag |= 0x1;
				cpu = 2;
			}

			if (dp_strncmpi(param_list[i],
					"mpe1",
					strlen("mpe1"))
					== 0) {
				flag |= 0x2;
				mpe1 = 2;
			}

			if (dp_strncmpi(param_list[i],
					"mpe2",
					strlen("mpe2"))
					== 0) {
				flag |= 0x4;
				mpe2 = 2;
			}

			if (dp_strncmpi(param_list[i],
					"mpe3",
					strlen("mpe3"))
					== 0) {
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
		dp_set_gsw_parser_30(flag, cpu, mpe1, mpe2, mpe3);
	} else if (dp_strncmpi(param_list[0],
				"disable",
				strlen("disable"))
				== 0) {
		for (i = 1; i < num; i++) {
			if (dp_strncmpi(param_list[i],
					"cpu",
					strlen("cpu"))
					== 0) {
				flag |= 0x1;
				cpu = 0;
			}

			if (dp_strncmpi(param_list[i],
					"mpe1",
					strlen("mpe1"))
					== 0) {
				flag |= 0x2;
				mpe1 = 0;
			}

			if (dp_strncmpi(param_list[i],
					"mpe2",
					strlen("mpe2"))
					== 0) {
				flag |= 0x4;
				mpe2 = 0;
			}

			if (dp_strncmpi(param_list[i],
					"mpe3",
					strlen("mpe3"))
					== 0) {
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
		dp_set_gsw_parser_30(flag, cpu, mpe1, mpe2, mpe3);
	} else if (dp_strncmpi(param_list[0],
				     "refresh",
					 strlen("refresh"))
					 == 0) {
		dp_get_gsw_parser_30(NULL, NULL, NULL, NULL);
		PR_INFO("value:cpu=%d mpe1=%d mpe2=%d mpe3=%d\n", pinfo[0].v,
			pinfo[1].v, pinfo[2].v, pinfo[3].v);
		PR_INFO("size :cpu=%d mpe1=%d mpe2=%d mpe3=%d\n",
			pinfo[0].size, pinfo[1].size, pinfo[2].size,
			pinfo[3].size);
		return count;
	} else if (dp_strncmpi(param_list[0], "mark", strlen("mark")) == 0) {
		int flag = dp_atoi(param_list[1]);

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
		pce.pattern.nParserFlagMSB_Mask = 0xffff;
		pce.pattern.bParserFlagLSB_Enable = 1;
		pce.pattern.nParserFlagLSB_Mask = 0xffff;
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
			PR_ERR("PCE rule add fail for GSW_PCE_RULE_WRITE\n");
			return count;
		}

	} else if (dp_strncmpi(param_list[0],
				     "unmark",
					 strlen("unmark"))
					 == 0) {
		/*: All packets set to same mpe flag as specified */
		memset(&pce, 0, sizeof(pce));
		pce.pattern.nIndex = pce_rule_id;
		pce.pattern.bEnable = 0;
		if (gsw_core_api((dp_gsw_cb)gsw_handle->gsw_tflow_ops
				 .TFLOW_PceRuleWrite, gsw_handle, &pce)) {
			PR_ERR("PCE rule add fail for GSW_PCE_RULE_WRITE\n");
			return count;
		}
	} else {
		PR_INFO("Usage: echo %s > parser\n",
			"<enable/disable> [cpu] [mpe1] [mpe2] [mpe3]");
		PR_INFO("Usage: echo <refresh> parser\n");

		PR_INFO("Usage: echo %s > parser\n",
			" mark eProcessPath_Action_value(0~3)");
		PR_INFO("Usage: echo unmark > parser\n");
		return count;
	}

	return count;
}

#define GSW_PORT_RMON64_PRINT(title, var)  do { \
	seq_printf(s, "%-14s%10s %12u %12u %12u %12u %12u %12u %12u\n", \
		   title "(H)", "L(0-6)", \
		   high_10dec(gsw_l_rmon_mib[0].var), \
		   high_10dec(gsw_l_rmon_mib[1].var), \
		   high_10dec(gsw_l_rmon_mib[2].var),  \
		   high_10dec(gsw_l_rmon_mib[3].var), \
		   high_10dec(gsw_l_rmon_mib[4].var),  \
		   high_10dec(gsw_l_rmon_mib[5].var), \
		   high_10dec(gsw_l_rmon_mib[6].var)); \
	seq_printf(s, "%-14s%10s %12u %12u %12u %12u %12u %12u %12u\n", \
		   title "(L)", "L(0-6)", \
		   low_10dec(gsw_l_rmon_mib[0].var), \
		   low_10dec(gsw_l_rmon_mib[1].var), \
		   low_10dec(gsw_l_rmon_mib[2].var), \
		   low_10dec(gsw_l_rmon_mib[3].var), \
		   low_10dec(gsw_l_rmon_mib[4].var), \
		   low_10dec(gsw_l_rmon_mib[5].var), \
		   low_10dec(gsw_l_rmon_mib[6].var)); \
	seq_printf(s, "%-14s%10s %12u %12u %12u %12u %12u %12u %12u %12u\n", \
		   title "(H)", "R(0-6,15)", \
		   high_10dec(gsw_r_rmon_mib[0].var), \
		   high_10dec(gsw_r_rmon_mib[1].var), \
		   high_10dec(gsw_r_rmon_mib[2].var), \
		   high_10dec(gsw_r_rmon_mib[3].var), \
		   high_10dec(gsw_r_rmon_mib[4].var), \
		   high_10dec(gsw_r_rmon_mib[5].var), \
		   high_10dec(gsw_r_rmon_mib[6].var), \
		   high_10dec(gsw_r_rmon_mib[15].var)); \
	seq_printf(s, "%-14s%10s %12u %12u %12u %12u %12u %12u %12u %12u\n", \
		   title "(L)", "R(0-6,15)", \
		   low_10dec(gsw_r_rmon_mib[0].var), \
		   low_10dec(gsw_r_rmon_mib[1].var), \
		   low_10dec(gsw_r_rmon_mib[2].var), \
		   low_10dec(gsw_r_rmon_mib[3].var), \
		   low_10dec(gsw_r_rmon_mib[4].var), \
		   low_10dec(gsw_r_rmon_mib[5].var), \
		   low_10dec(gsw_r_rmon_mib[6].var), \
		   low_10dec(gsw_r_rmon_mib[15].var)); \
	seq_printf(s, "%-14s%10s %12u %12u %12u %12u %12u %12u %12u %12u\n", \
		   title "(H)", "R(7-14)", \
		   high_10dec(gsw_r_rmon_mib[7].var), \
		   high_10dec(gsw_r_rmon_mib[8].var), \
		   high_10dec(gsw_r_rmon_mib[9].var),  \
		   high_10dec(gsw_r_rmon_mib[10].var), \
		   high_10dec(gsw_r_rmon_mib[11].var), \
		   high_10dec(gsw_r_rmon_mib[12].var), \
		   high_10dec(gsw_r_rmon_mib[13].var), \
		   high_10dec(gsw_r_rmon_mib[14].var)); \
	seq_printf(s, \
		   "%-14s%10s %12u %12u %12u %12u %12u %12u %12u %12u\n", \
		   title "(L)", "R(7-14)", \
		   low_10dec(gsw_r_rmon_mib[7].var), \
		   low_10dec(gsw_r_rmon_mib[8].var), \
		   low_10dec(gsw_r_rmon_mib[9].var), \
		   low_10dec(gsw_r_rmon_mib[10].var), \
		   low_10dec(gsw_r_rmon_mib[11].var), \
		   low_10dec(gsw_r_rmon_mib[12].var), \
		   low_10dec(gsw_r_rmon_mib[13].var), \
		   low_10dec(gsw_r_rmon_mib[14].var)); \
	print_dash_line(s); \
	} while (0)

typedef int (*ingress_pmac_set_callback_t) (dp_pmac_cfg_t *pmac_cfg,
					    u32 value);
typedef int (*egress_pmac_set_callback_t) (dp_pmac_cfg_t *pmac_cfg,
					   u32 value);
struct ingress_pmac_entry {
	char *name;
	ingress_pmac_set_callback_t ingress_callback;
};

struct egress_pmac_entry {
	char *name;
	egress_pmac_set_callback_t egress_callback;
};

static int ingress_err_disc_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.err_disc = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_ERR_DISC;
	return 0;
}

static int ingress_pmac_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.pmac = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PRESENT;
	return 0;
}

static int ingress_pmac_pmap_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.def_pmac_pmap = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMAP;
	return 0;
}

static int ingress_pmac_en_pmap_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.def_pmac_en_pmap = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMAPENA;
	return 0;
}

static int ingress_pmac_tc_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.def_pmac_tc = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_CLASS;
	return 0;
}

static int ingress_pmac_en_tc_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.def_pmac_en_tc = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_CLASSENA;
	return 0;
}

static int ingress_pmac_subifid_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.def_pmac_subifid = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_SUBIF;
	return 0;
}

static int ingress_pmac_srcport_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->ig_pmac.def_pmac_src_port = value;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_SPID;
	return 0;
}

static int ingress_pmac_hdr1_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[0] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR1;
	return 0;
}

static int ingress_pmac_hdr2_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[1] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR2;
	return 0;
}

static int ingress_pmac_hdr3_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[2] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR3;
	return 0;
}

static int ingress_pmac_hdr4_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[3] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR4;
	return 0;
}

static int ingress_pmac_hdr5_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[4] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR5;
	return 0;
}

static int ingress_pmac_hdr6_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[5] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR6;
	return 0;
}

static int ingress_pmac_hdr7_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[6] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR7;
	return 0;
}

static int ingress_pmac_hdr8_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	u8 hdr;

	hdr = (u8)value;
	pmac_cfg->ig_pmac.def_pmac_hdr[7] = hdr;
	pmac_cfg->ig_pmac_flags = IG_PMAC_F_PMACHDR8;
	return 0;
}

static int egress_fcs_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.fcs = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_FCS;
	return 0;
}

static int egress_l2hdr_bytes_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.num_l2hdr_bytes_rm = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_L2HDR_RM;
	return 0;
}

static int egress_rx_dma_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.rx_dma_chan = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_RXID;
	return 0;
}

static int egress_pmac_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.pmac = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_PMAC;
	return 0;
}

static int egress_res_dw_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.res_dw1 = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_RESDW1;
	return 0;
}

static int egress_res1_dw_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.res1_dw0 = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_RES1DW0;
	return 0;
}

static int egress_res2_dw_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.res2_dw0 = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_RES2DW0;
	return 0;
}

static int egress_tc_ena_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.tc_enable = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_TCENA;
	return 0;
}

static int egress_dec_flag_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.dec_flag = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_DECFLG;
	return 0;
}

static int egress_enc_flag_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.enc_flag = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_ENCFLG;
	return 0;
}

static int egress_mpe1_flag_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.mpe1_flag = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_MPE1FLG;
	return 0;
}

static int egress_mpe2_flag_set(dp_pmac_cfg_t *pmac_cfg, u32 value)
{
	pmac_cfg->eg_pmac.mpe2_flag = value;
	pmac_cfg->eg_pmac_flags = EG_PMAC_F_MPE2FLG;
	return 0;
}

static struct ingress_pmac_entry ingress_entries[] = {
	{"errdisc", ingress_err_disc_set},
	{"pmac", ingress_pmac_set},
	{"pmac_pmap", ingress_pmac_pmap_set},
	{"pmac_en_pmap", ingress_pmac_en_pmap_set},
	{"pmac_tc", ingress_pmac_tc_set},
	{"pmac_en_tc", ingress_pmac_en_tc_set},
	{"pmac_subifid", ingress_pmac_subifid_set},
	{"pmac_srcport", ingress_pmac_srcport_set},
	{"pmac_hdr1", ingress_pmac_hdr1_set},
	{"pmac_hdr2", ingress_pmac_hdr2_set},
	{"pmac_hdr3", ingress_pmac_hdr3_set},
	{"pmac_hdr4", ingress_pmac_hdr4_set},
	{"pmac_hdr5", ingress_pmac_hdr5_set},
	{"pmac_hdr6", ingress_pmac_hdr6_set},
	{"pmac_hdr7", ingress_pmac_hdr7_set},
	{"pmac_hdr8", ingress_pmac_hdr8_set},
	{NULL, NULL}
};

static struct egress_pmac_entry egress_entries[] = {
	{"rx_dmachan", egress_rx_dma_set},
	{"rm_l2hdr", egress_l2hdr_bytes_set},
	{"fcs", egress_fcs_set},
	{"pmac", egress_pmac_set},
	{"res_dw1", egress_res_dw_set},
	{"res1_dw0", egress_res1_dw_set},
	{"res2_dw0", egress_res2_dw_set},
	{"tc_enable", egress_tc_ena_set},
	{"dec_flag", egress_dec_flag_set},
	{"enc_flag", egress_enc_flag_set},
	{"mpe1_flag", egress_mpe1_flag_set},
	{"mpe2_flag", egress_mpe2_flag_set},
	{NULL, NULL}
};

static int proc_gsw_port_rmon_dump(struct seq_file *s, int pos)
{
	int i;
	int ret = 0;
	struct core_ops *gsw_handle;
	char flag_buf[20];

	if (pos == 0) {
		memset(gsw_r_rmon_mib, 0, sizeof(gsw_r_rmon_mib));
		memset(gsw_l_rmon_mib, 0, sizeof(gsw_l_rmon_mib));

		/*read gswip-r rmon counter */
		gsw_handle = dp_port_prop[0].ops[GSWIP_R];

		for (i = 0; i < ARRAY_SIZE(gsw_r_rmon_mib); i++) {
			gsw_r_rmon_mib[i].nPortId = i;
			ret = gsw_core_api((dp_gsw_cb)gsw_handle
					   ->gsw_rmon_ops.RMON_Port_Get,
					   gsw_handle, &gsw_r_rmon_mib[i]);

			if (ret != GSW_statusOk) {
				PR_ERR("RMON_PORT_GET fail for Port %d\n", i);
				return -1;
			}
		}

		/*read pmac rmon redirect mib */
		memset(&gswr_rmon_redirect, 0, sizeof(gswr_rmon_redirect));
		ret = gsw_core_api((dp_gsw_cb)gsw_handle
				   ->gsw_rmon_ops.RMON_Redirect_Get, gsw_handle,
				   &gswr_rmon_redirect);

		if (ret != GSW_statusOk) {
			PR_ERR("GSW_RMON_REDIRECT_GET fail for Port %d\n", i);
			return -1;
		}

		/*read gswip-l rmon counter */
		gsw_handle = dp_port_prop[0].ops[GSWIP_L];
		for (i = 0; i < ARRAY_SIZE(gsw_l_rmon_mib); i++) {
			gsw_l_rmon_mib[i].nPortId = i;
			ret = gsw_core_api((dp_gsw_cb)gsw_handle
					   ->gsw_rmon_ops.RMON_Port_Get,
					   gsw_handle,
					   &gsw_l_rmon_mib[i]);
			if (ret != GSW_statusOk) {
				PR_ERR("RMON_PORT_GET fail for Port %d\n", i);
				return -1;
			}
		}

		seq_printf(s, "%-24s %12u %12u %12u %12u %12u %12u %12u\n",
			   "GSWIP-L", 0, 1, 2, 3, 4, 5, 6);
		seq_printf(s, "%-24s %12u %12u %12u %12u %12u %12u %12u %12u\n",
			   "GSWIP-R(Fixed)", 0, 1, 2, 3, 4, 5, 6, 15);
		seq_printf(s, "%-24s %12u %12u %12u %12u %12u %12u %12u %12u\n",
			   "GSWIP-R(Dynamic)", 7, 8, 9, 10, 11, 12, 13, 14);
		print_dash_line(s);
	}
	if (pos == RX_GOOD_PKTS) {
		GSW_PORT_RMON_PRINT("RX_Good", nRxGoodPkts);
	} else if (pos == RX_FILTER_PKTS) {
		GSW_PORT_RMON_PRINT("RX_FILTER", nRxFilteredPkts);
	} else if (pos == RX_DROP_PKTS) {
		GSW_PORT_RMON_PRINT("RX_DROP", nRxDroppedPkts);
	} else if (pos == RX_OTHERS) {
		if (!rmon_display_port_full)
			goto NEXT;

		GSW_PORT_RMON_PRINT("RX_UNICAST", nRxUnicastPkts);
		GSW_PORT_RMON_PRINT("RX_BROADCAST", nRxBroadcastPkts);
		GSW_PORT_RMON_PRINT("RX_MULTICAST", nRxMulticastPkts);
		GSW_PORT_RMON_PRINT("RX_FCS_ERR", nRxFCSErrorPkts);
		GSW_PORT_RMON_PRINT("RX_UNDER_GOOD",
				    nRxUnderSizeGoodPkts);
		GSW_PORT_RMON_PRINT("RX_OVER_GOOD", nRxOversizeGoodPkts);
		GSW_PORT_RMON_PRINT("RX_UNDER_ERR",
				    nRxUnderSizeErrorPkts);
		GSW_PORT_RMON_PRINT("RX_OVER_ERR", nRxOversizeErrorPkts);
		GSW_PORT_RMON_PRINT("RX_ALIGN_ERR", nRxAlignErrorPkts);
		GSW_PORT_RMON_PRINT("RX_64B", nRx64BytePkts);
		GSW_PORT_RMON_PRINT("RX_127B", nRx127BytePkts);
		GSW_PORT_RMON_PRINT("RX_255B", nRx255BytePkts);
		GSW_PORT_RMON_PRINT("RX_511B", nRx511BytePkts);
		GSW_PORT_RMON_PRINT("RX_1023B", nRx1023BytePkts);
		GSW_PORT_RMON_PRINT("RX_MAXB", nRxMaxBytePkts);
		GSW_PORT_RMON64_PRINT("RX_BAD_b", nRxBadBytes);
	} else if (pos == TX_GOOD_PKTS) {
		GSW_PORT_RMON_PRINT("TX_Good", nTxGoodPkts);
	} else if (pos == TX_ACM_PKTS) {
		GSW_PORT_RMON_PRINT("TX_ACM_DROP", nTxAcmDroppedPkts);
	} else if (pos == TX_DROP_PKTS) {
		GSW_PORT_RMON_PRINT("TX_Drop", nTxDroppedPkts);
	} else if (pos == TX_OTHERS) {
		if (!rmon_display_port_full)
			goto NEXT;

		GSW_PORT_RMON_PRINT("TX_UNICAST", nTxUnicastPkts);
		GSW_PORT_RMON_PRINT("TX_BROADAST", nTxBroadcastPkts);
		GSW_PORT_RMON_PRINT("TX_MULTICAST", nTxMulticastPkts);
		GSW_PORT_RMON_PRINT("TX_SINGLE_COLL",
				    nTxSingleCollCount);
		GSW_PORT_RMON_PRINT("TX_MULT_COLL", nTxMultCollCount);
		GSW_PORT_RMON_PRINT("TX_LATE_COLL", nTxLateCollCount);
		GSW_PORT_RMON_PRINT("TX_EXCESS_COLL",
				    nTxExcessCollCount);
		GSW_PORT_RMON_PRINT("TX_COLL", nTxCollCount);
		GSW_PORT_RMON_PRINT("TX_PAUSET", nTxPauseCount);
		GSW_PORT_RMON_PRINT("TX_64B", nTx64BytePkts);
		GSW_PORT_RMON_PRINT("TX_127B", nTx127BytePkts);
		GSW_PORT_RMON_PRINT("TX_255B", nTx255BytePkts);
		GSW_PORT_RMON_PRINT("TX_511B", nTx511BytePkts);
		GSW_PORT_RMON_PRINT("TX_1023B", nTx1023BytePkts);
		GSW_PORT_RMON_PRINT("TX_MAX_B", nTxMaxBytePkts);
		GSW_PORT_RMON_PRINT("TX_UNICAST", nTxUnicastPkts);
		GSW_PORT_RMON_PRINT("TX_UNICAST", nTxUnicastPkts);
		GSW_PORT_RMON_PRINT("TX_UNICAST", nTxUnicastPkts);
		GSW_PORT_RMON64_PRINT("TX_GOOD_b", nTxGoodBytes);

	} else if (pos == REDIRECT_MIB) {
		/*GSWIP-R PMAC Redirect conter */
		seq_printf(s, "%-24s %12s %12s %12s %12s\n",
			   "GSW-R Redirect", "Rx_Pkts", "Tx_Pkts",
			   "Rx_DropsPkts", "Tx_DropsPkts");

		seq_printf(s, "%-24s %12d %12d %12d %12d\n", "",
			   gswr_rmon_redirect.nRxPktsCount,
			   gswr_rmon_redirect.nTxPktsCount,
			   gswr_rmon_redirect.nRxDiscPktsCount,
			   gswr_rmon_redirect.nTxDiscPktsCount);
		print_dash_line(s);
	} else if (pos == DP_DRV_MIB) {
		u64 eth0_rx = 0, eth0_tx = 0;
		u64 eth1_rx = 0, eth1_tx = 0;
		u64 dsl_rx = 0, dsl_tx = 0;
		u64 other_rx = 0, other_tx = 0;
		int i, j;
		struct pmac_port_info *port;

		for (i = 1; i < PMAC_MAX_NUM; i++) {
			port = get_port_info(tmp_inst, i);

			if (!port)
				continue;

			if (i < 6) {
				for (j = 0; j < 16; j++) {
					eth0_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_rxif_pkt);
					eth0_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_txif_pkt);
					eth0_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_cbm_pkt);
					eth0_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_tso_pkt);
				}
			} else if (i == 15) {
				for (j = 0; j < 16; j++) {
					eth1_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_rxif_pkt);
					eth1_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_txif_pkt);
					eth1_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_cbm_pkt);
					eth1_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_tso_pkt);
				}
			} else if (port->alloc_flags & DP_F_FAST_DSL) {
				for (j = 0; j < 16; j++) {
					dsl_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_rxif_pkt);
					dsl_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_txif_pkt);
					dsl_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_cbm_pkt);
					dsl_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_tso_pkt);
				}
			} else {
				for (j = 0; j < 16; j++) {
					other_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_rxif_pkt);
					other_rx +=
					    STATS_GET(port->subif_info[j].mib.
					    rx_fn_txif_pkt);
					other_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_cbm_pkt);
					other_tx +=
					    STATS_GET(port->subif_info[j].mib.
					    tx_tso_pkt);
				}
			}
		}

		seq_printf(s, "%-15s %22s %22s %22s %22s\n", "DP Drv Mib",
			   "ETH_LAN", "ETH_WAN", "DSL", "Others");

		seq_printf(s, "%15s %22llu %22llu %22llu %22llu\n",
			   "Rx_Pkts", eth0_rx, eth1_rx, dsl_rx, other_rx);
		seq_printf(s, "%15s %22llu %22llu %22llu %22llu\n",
			   "Tx_Pkts", eth0_tx, eth1_tx, dsl_tx, other_tx);
		print_dash_line(s);
	} else if ((pos >= RMON_MAX) &&
		   (pos < (RMON_MAX + EGRESS_QUEUE_ID_MAX))) {
		u32 qdc[4], enq_c, deq_c, index;
		u32 wq, qrth, qocc, qavg;
		struct tmu_equeue_link equeue_link;
		char *flag_s;

		if (!rmon_display_tmu_mib)
			goto NEXT;

		index = pos - RMON_MAX;
		enq_c = get_enq_counter(index);
		deq_c = get_deq_counter(index);
		tmu_qdct_read(index, qdc);
		tmu_qoct_read(index, &wq, &qrth, &qocc, &qavg);
		tmu_equeue_link_get(index, &equeue_link);
		flag_s =
		    get_dma_flags_str(equeue_link.epn, flag_buf,
				      sizeof(flag_buf));

		if ((enq_c || deq_c || (qdc[0] + qdc[1] + qdc[2] + qdc[3])) ||
		    qocc || qavg) {
			if (!f_q_mib_title_proc) {
				seq_printf(s, "%-15s %10s %10s %10s (%10s %10s %10s %10s) %10s %10s %10s\n",
					   "TMU MIB     QID", "enq",
					   "deq", "drop", "No-Color",
					   "Green", "Yellow", "Red",
					   "qocc", "qavg", "  DMA  Flag");
				f_q_mib_title_proc = 1;
			}

			seq_printf(s, "%15d %10u %10u %10u (%10u %10u %10u %10u) %10u %10u %10s\n",
				   index, enq_c, deq_c,
				   (qdc[0] + qdc[1] + qdc[2] + qdc[3]),
				   qdc[0], qdc[1], qdc[2], qdc[3], qocc,
				   (qavg >> 8), flag_s ? flag_s : "");

		} else {
			goto NEXT;
		}
	} else {
		goto NEXT;
	}
NEXT:
	pos++;

	if (pos - RMON_MAX + 1 >= EGRESS_QUEUE_ID_MAX)
		return -1;

	return pos;
}

static int proc_gsw_rmon_port_start(void)
{
	f_q_mib_title_proc = 0;
	tmp_inst = 0;
	return 0;
}

static ssize_t proc_gsw_rmon_write(struct file *file, const char *buf,
				   size_t count, loff_t *ppos)
{
	int len;
	char str[64];
	int num;
	char *param_list[10];

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (num < 1)
		goto help;

	if (dp_strncmpi(param_list[0], "clear", strlen("clear")) == 0 ||
	    dp_strncmpi(param_list[0], "c", 1) == 0 ||
	    dp_strncmpi(param_list[0], "rest", strlen("rest")) == 0 ||
	    dp_strncmpi(param_list[0], "r", 1) == 0) {
		dp_sys_mib_reset_30(0);
		goto EXIT_OK;
	}

	if (dp_strncmpi(param_list[0], "TMU", strlen("TMU")) == 0) {
		if (dp_strncmpi(param_list[1], "on", 2) == 0) {
			rmon_display_tmu_mib = 1;
			goto EXIT_OK;
		} else if (dp_strncmpi(param_list[1], "off", 3) == 0) {
			rmon_display_tmu_mib = 0;
			goto EXIT_OK;
		}
	}

	if (dp_strncmpi(param_list[0], "RMON", strlen("RMON")) == 0) {
		if (dp_strncmpi(param_list[1], "Full", strlen("Full")) == 0) {
			rmon_display_port_full = 1;
			goto EXIT_OK;
		} else if (dp_strncmpi(param_list[1],
				     "Basic",
					 strlen("Basic"))
					 == 0) {
			rmon_display_port_full = 0;
			goto EXIT_OK;
		}
	}

	/*unknown command */
	goto help;

EXIT_OK:
	return count;

help:
	PR_INFO("usage: echo clear > /proc/dp/rmon\n");
	PR_INFO("usage: echo TMU on > /proc/dp/rmon\n");
	PR_INFO("usage: echo TMU off > /proc/dp/rmon\n");
	PR_INFO("usage: echo RMON Full > /proc/dp/rmon\n");
	PR_INFO("usage: echo RMON Basic > /proc/dp/rmon\n");
	return count;
}

static void dp_send_packet(u8 *pdata, int len, char *devname, u32 flag)
{
	struct sk_buff *skb;
	dp_subif_t subif = { 0 };

	skb = cbm_alloc_skb(len + 8, GFP_ATOMIC);

	if (unlikely(!skb)) {
		PR_ERR("allocate cbm buffer fail\n");
		return;
	}

	skb->DW0 = 0;
	skb->DW1 = 0;
	skb->DW2 = 0;
	skb->DW3 = 0;
	memcpy(skb->data, pdata, len);
	skb->len = 0;
	skb_put(skb, len);
	skb->dev = dev_get_by_name(&init_net, devname);

	if (dp_get_netif_subifid(skb->dev, skb, NULL, skb->data, &subif, 0)) {
		PR_ERR("dp_get_netif_subifid failed for %s\n",
		       skb->dev->name);
		dev_kfree_skb_any(skb);
		return;
	}

	((struct dma_tx_desc_1 *)&skb->DW1)->field.ep = subif.port_id;
	((struct dma_tx_desc_0 *)&skb->DW0)->field.dest_sub_if_id =
		subif.subif;

	dp_xmit(skb->dev, &subif, skb, skb->len, flag);
}

static u8 ipv4_plain_udp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x08, 0x00,		/*type */
	0x45, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x11, /*ip hdr*/
	0x3A, 0x56, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x7A, 0x41, 0x00, 0x00, /*udp hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

static u8 ipv4_plain_tcp[1514] = {
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x08, 0x00,		/*type */
	0x45, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x06, /*ip hdr*/
	0x3A, 0x61, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x04, 0x00, 0x00, 0x01, 0xE2, 0x40, 0x00, 0x03, /*tcp hdr*/
	0x94, 0x47, 0x50, 0x10, 0x10, 0x00, 0x9F, 0xD9, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*data */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

static u8 ipv6_plain_udp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x86, 0xDD,		/*type */
	0x60, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x11, 0xFF, 0x20, 0x00, /*ip hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x00, 0x00, 0x00, 0x3E, 0xBB, 0x6F, 0x00, 0x00, /*udp hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

static u8 ipv6_plain_tcp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x86, 0xDD,		/*type */
	0x60, 0x00, 0x00, 0x00, 0x00, 0x46, 0x06, 0xFF, 0x20, 0x00, /*ip hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x04, 0x00, 0x00, 0x01, 0xE2, 0x40, 0x00, 0x03, /*tcp hdr*/
	0x94, 0x47, 0x50, 0x10, 0x10, 0x00, 0xE1, 0x13, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*data */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 ipv6_extensions_udp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x86, 0xDD,		/*type */
	0x60, 0x00, 0x00, 0x00, 0x00, 0x8E, 0x00, 0xFF, 0x20, 0x00, /*ip hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x3C, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00,	/*next extension:hop */
	0x2B, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00,	/*next extension:Dst */
	0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/*next extension:Rout*/
	0x04, 0x00, 0x00, 0x00, 0x00, 0x76, 0xBA, 0xFF, 0x00, 0x00,/*udp hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 ipv6_extensions_tcp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x86, 0xDD,		/*type */
	0x60, 0x00, 0x00, 0x00, 0x00, 0x8E, 0x00, 0xFF, 0x20, 0x00,/*ip hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x3C, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, /*next extension:hop */
	0x2B, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, /*next extension:dst*/
	0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*next extension:Rout*/
	0x04, 0x00, 0x04, 0x00, 0x00, 0x01, 0xE2, 0x40, 0x00, 0x03,/*tcp hdr*/
	0x94, 0x47, 0x50, 0x10, 0x10, 0x00, 0xE0, 0xE3, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 rd6_ip4_ip6_udp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x08, 0x00,		/*type */
	0x45, 0x00, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x29,/*ip4 hdr */
	0x3A, 0x0E, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x32, 0x11, 0xFF, 0x20, 0x00,/*ip6 hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x00, 0x00, 0x00, 0x32, 0xBB, 0x87, 0x00, 0x00,/*udp hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 rd6_ip4_ip6_tcp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x08, 0x00,		/*type */
	0x45, 0x00, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x29, /*ip4 hdr*/
	0x3A, 0x0E, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x32, 0x06, 0xFF, 0x20, 0x00, /*ip6 hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x04, 0x00, 0x00, 0x01, 0xE2, 0x40, 0x00, 0x03, /*tcp hdr*/
	0x94, 0x47, 0x50, 0x10, 0x10, 0x00, 0xE1, 0x27, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 dslite_ip6_ip4_udp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x86, 0xDD,		/*type */
	0x60, 0x00, 0x00, 0x00, 0x00, 0x46, 0x04, 0xFF, 0x20, 0x00, /*ip6 hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x45, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x11, /*ip4 hdr*/
	0x3A, 0x4E, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x00, 0x00, 0x00, 0x32, 0x7A, 0x31, 0x00, 0x00, /*udp hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 dslite_ip6_ip4_tcp[] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x01,	/*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x86, 0xDD,		/*type */
	0x60, 0x00, 0x00, 0x00, 0x00, 0x46, 0x04, 0xFF, 0x20, 0x00,/*ip6 hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x45, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x06,/*ip4 hdr*/
	0x3A, 0x59, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x04, 0x00, 0x00, 0x01, 0xE2, 0x40, 0x00, 0x03,/*tcp hdr*/
	0x94, 0x47, 0x50, 0x10, 0x10, 0x00, 0x9F, 0xD1, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int checksm_mode = 2;
void proc_checksum_read(struct seq_file *s)
{
	char *devname = "eth0_4";

	if (!checksm_mode) {
		seq_printf(s,
			   "\nsend pmac checksum ipv4_plain_udp new via %s\n",
			   devname);
		dp_send_packet(ipv4_plain_udp, sizeof(ipv4_plain_udp),
			       devname, DP_TX_CAL_CHKSUM);

		seq_printf(s,
			   "\nsend pmac checksum ipv4_plain_tcp new via %s\n",
			   devname);
		dp_send_packet(ipv4_plain_tcp, sizeof(ipv4_plain_tcp),
			       devname, DP_TX_CAL_CHKSUM);

		seq_printf(s,
			   "\nsend pmac checksum ipv6_plain_udp new via %s\n",
			   devname);
		dp_send_packet(ipv6_plain_udp, sizeof(ipv6_plain_udp),
			       devname, DP_TX_CAL_CHKSUM);

		seq_printf(s,
			   "\nsend pmac checksum ipv6_plain_tcp new via %s\n",
			   devname);
		dp_send_packet(ipv6_plain_tcp, sizeof(ipv6_plain_tcp),
			       devname, DP_TX_CAL_CHKSUM);

		seq_printf(s,
			   "\nsend pmac checksum ipv6_extensions_udp new via %s\n",
			   devname);
		dp_send_packet(ipv6_extensions_udp,
			       sizeof(ipv6_extensions_udp), devname,
			       DP_TX_CAL_CHKSUM);

		seq_printf(s,
			   "\nsend pmac checksum ipv6_extensions_tcp via %s\n",
			   devname);
		dp_send_packet(ipv6_extensions_tcp,
			       sizeof(ipv6_extensions_tcp), devname,
			       DP_TX_CAL_CHKSUM);

		seq_printf(s, "\nsend pmac checksum rd6_ip4_ip6_udp via %s\n",
			   devname);
		dp_send_packet(rd6_ip4_ip6_udp, sizeof(rd6_ip4_ip6_udp),
			       devname, DP_TX_CAL_CHKSUM);

		seq_printf(s, "\nsend pmac checksum rd6_ip4_ip6_tcp via %s\n",
			   devname);
		dp_send_packet(rd6_ip4_ip6_tcp, sizeof(rd6_ip4_ip6_tcp),
			       devname, DP_TX_CAL_CHKSUM);

		seq_printf(s,
			   "\nsend pmac checksum dslite_ip6_ip4_udp via %s\n",
			   devname);
		dp_send_packet(dslite_ip6_ip4_udp, sizeof(dslite_ip6_ip4_udp),
			       devname, DP_TX_CAL_CHKSUM);

		seq_printf(s,
			   "\nsend pmac checksum dslite_ip6_ip4_tcp via %s\n",
			   devname);
		dp_send_packet(dslite_ip6_ip4_tcp, sizeof(dslite_ip6_ip4_tcp),
			       devname, DP_TX_CAL_CHKSUM);
	} else if (checksm_mode == 1) {
#define MOD_V  32
		int offset = 14 /*mac */ + 20 /*ip */ + 20 /*tcp */;
#define IP_LEN_OFFSET 16
		int i;
		int numbytes = jiffies % 1515;

		if (numbytes < 64)
			numbytes = 64;
		else if (numbytes >= 1514)
			numbytes = 1514;

		for (i = 0; i < sizeof(ipv4_plain_tcp) - offset; i++) {
			if (i < (numbytes - offset))
				ipv4_plain_tcp[offset + i] = (i % MOD_V) + 1;
			else
				ipv4_plain_tcp[offset + i] = 0;
		}
		*(unsigned short *)&ipv4_plain_tcp[IP_LEN_OFFSET] =
		    numbytes - 14 /*MAC HDR */;

		dp_send_packet(ipv4_plain_tcp, numbytes, devname,
			       DP_TX_CAL_CHKSUM);
	} else if (checksm_mode == 2) {
#define MOD_V  32
		int offset = 14 /*mac */  + 20 /*ip */  + 20 /*tcp */;
		int i;
		int numbytes = offset + 2 /*2 bytes payload */;

		for (i = 0; i < sizeof(ipv4_plain_tcp) - offset; i++) {
			if (i < (numbytes - offset))
				ipv4_plain_tcp[offset + i] = (i % MOD_V) + 1;
			else
				ipv4_plain_tcp[offset + i] = 0;
		}
		*(unsigned short *)&ipv4_plain_tcp[IP_LEN_OFFSET] =
		    numbytes - 14 /*MAC HDR */;
		dp_send_packet(ipv4_plain_tcp, numbytes, devname,
			       DP_TX_CAL_CHKSUM);
	}
}

static ssize_t proc_checksum_write(struct file *file, const char *buf,
				   size_t count, loff_t *ppos)
{
	int len;
	char str[64];
	int num;
	char *param_list[2];

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if ((dp_strncmpi(param_list[0], "help", strlen("help")) == 0) ||
	    (dp_strncmpi(param_list[0], "h", 1) == 0)) {
		goto help;
	} else {
		checksm_mode = dp_atoi(param_list[0]);
	}

	PR_INFO("new checksm_mode=%d\n", checksm_mode);
	return count;

help:
	PR_INFO("checksm_mode usage: current value=%d\n", checksm_mode);
	PR_INFO(" 0: common protocol test with HW checksum\n");
	PR_INFO(" 1: TCP random size with HW checksum\n");
	PR_INFO(" 2: 2 Bytes TCP packet with HW checksum\n");
	PR_INFO(" others: not supported value\n");

	return count;
}

static int proc_dport_dump(struct seq_file *s, int pos)
{
	int i;
	cbm_dq_port_res_t res;
	u32 flag = 0;

	memset(&res, 0, sizeof(cbm_dq_port_res_t));
	if (cbm_dequeue_port_resources_get(pos, &res, flag) == 0) {
		seq_printf(s, "Dequeue port=%d free_base=0x%x\n", pos,
			   (u32)res.cbm_buf_free_base);

		for (i = 0; i < res.num_deq_ports; i++) {
			seq_printf(s,
				   "%d:deq_port_base=0x%x num_desc=%d port = %d tx chan %d\n",
				   i, (u32)res.deq_info[i].cbm_dq_port_base,
				   res.deq_info[i].num_desc,
				   res.deq_info[i].port_no,
				   res.deq_info[i].dma_tx_chan);
		}

		kfree(res.deq_info);
	}

	pos++;

	if (pos >= PMAC_MAX_NUM)
		pos = -1;	/*end of the loop */

	return pos;
}

struct dp_skb_info {
	struct list_head list;
	struct sk_buff *skb;
};

static int cbm_skb_num;	/* for cbm buffer testing purpose */
static struct dp_skb_info skb_list;
#define get_val(val, mask, offset) (((val) & (mask)) >> (offset))
static int cbm_get_free_buf(int fsqm_index, u32 *free_num, u32 *head,
			    u32 *tail)
{
	unsigned char *base;

	if (!fsqm_index)
		base = (unsigned char *)(FSQM0_MODULE_BASE + 0xa0000000);
	else
		base = (unsigned char *)(FSQM1_MODULE_BASE + 0xa0000000);
	if (free_num)
		*free_num =
		    get_val(*(u32 *)(base + OFSC), OFSC_FSC_MASK,
			    OFSC_FSC_POS);
	if (head)
		*head =
		    get_val(*(u32 *)(base + OFSQ), OFSQ_HEAD_MASK,
			    OFSQ_HEAD_POS);
	if (tail)
		*tail =
		    get_val(*(u32 *)(base + OFSQ), OFSQ_TAIL_MASK,
			    OFSQ_TAIL_POS);

	return 0;
}

static void proc_cbm_buf_read(struct seq_file *s)
{
	u32 free_fsqm_num[2], fsqm_head[2], fsqm_tail[2];
	int i;

	for (i = 0; i < 2; i++)
		cbm_get_free_buf(i, &free_fsqm_num[i], &fsqm_head[i],
				 &fsqm_tail[i]);
	for (i = 0; i < 2; i++)
		seq_printf(s,
			   "FSQM%d: free buffer-%04d, head-%04d, tail-%04d\n",
			   i, free_fsqm_num[i], fsqm_head[i], fsqm_tail[i]);
	if (cbm_skb_num)
		seq_printf(s,
			   "Overall %d CBM buffer allocated for test only!\n",
			   cbm_skb_num);
	seq_printf(s,
		   "Note: %s: echo help > /proc/dp/%s\n",
		   "use echo to display other commands",
		   PROC_CBM_BUF_TEST);
}

static ssize_t proc_cbm_buf_write(struct file *file, const char *buf,
				  size_t count, loff_t *ppos)
{
	int len;
	char str[64];
	char *param_list[2] = { 0 };
	unsigned int num;
	struct dp_skb_info *tmp = NULL;
	u32 free_fsqm_num[2], fsqm_head[2], fsqm_tail[2], *check_list;
	int i;
	u32 idx, buf_ptr, val, head, tail, bits;
	const u32 fsqm_buf_len[] = {9216, 1024};
	void __iomem *base;

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;

	if (dp_split_buffer(str, param_list, ARRAY_SIZE(param_list)) < 2)
		goto help;
	if (!cbm_skb_num)
		INIT_LIST_HEAD(&skb_list.list);
	if (list_empty(&skb_list.list) && (cbm_skb_num != 0)) {
		PR_ERR("%s but recorded value of cbm_skb_num=%d\n",
		       "Why skb_list is empty",
		       cbm_skb_num);
		goto exit;
	}
	if (cbm_skb_num < 0) {
		PR_ERR("Why cbm_skb_num(%d) less than zero\n", cbm_skb_num);
		goto exit;
	}
	num = dp_atoi(param_list[1]);
	if (dp_strncmpi(param_list[0], "alloc", strlen("alloc")) == 0) {
		if (num == 0)
			goto exit;
			do {
				tmp = kmalloc(sizeof(*tmp),
					      GFP_KERNEL);
				if (!tmp)
					goto exit;
				INIT_LIST_HEAD(&tmp->list);
				tmp->skb = cbm_alloc_skb(1000, 0);
				if (!tmp->skb) {
					kfree(tmp);
					goto exit;
				}
				DP_DEBUG(DP_DBG_FLAG_CBM_BUF,
					 "%s: %p (node=%p buffer=%p)\n",
					 "cbm_alloc_skb ok",
					 tmp->skb, &tmp->list, tmp);
				list_add(&tmp->list, &skb_list.list);
				num--;
				cbm_skb_num++;
			} while (num);
	} else if (dp_strncmpi(param_list[0], "free", strlen("free")) == 0) {
		struct list_head *pos, *n;
		struct dp_skb_info *p;

		if (cbm_skb_num == 0 || num == 0)
			goto exit;
		list_for_each_safe(pos, n, &skb_list.list) {
			p = list_entry(pos, struct dp_skb_info, list);
			if (p->skb) {
				if (!check_ptr_validation
				    ((uint32_t)p->skb->data))
					PR_ERR("%s %p(node=%p buffer=%p) %s\n",
					       "Wrong Free skb",
					       p->skb, pos, p,
					       "not CBM bffer");
				else
					DP_DEBUG(DP_DBG_FLAG_CBM_BUF,
						 "%s %p(node=%p buffer=%p)\n",
						 "Free skb",
						 p->skb, pos, p);
				dev_kfree_skb(p->skb);

				p->skb = NULL;
			} else {
				PR_ERR("why p->skb NULL ???\n");
			}
			list_del(pos);
			kfree(p);
			num--;
			cbm_skb_num--;
			if (!num)
				break;
		}
	} else if (dp_strncmpi(param_list[0], "check", strlen("check")) == 0) {
		int good = 1;

		idx = dp_atoi(param_list[1]);
		if (idx >= 2) {
			PR_INFO("FSQM idx must be 0 or 1\n");
			return count;
		}
		num = idx;
		PR_INFO("\%s!\n",
			"nCBM link list check can only work with NO traffic");
		check_list = kmalloc(fsqm_buf_len[idx] >> 3, GFP_KERNEL);
		if (!check_list) {
			PR_ERR("Failed to allocate check list buffer\n");
			return count;
		}
		memset(check_list, 0, fsqm_buf_len[idx] >> 3);
		if (idx == 0) /* Fixme: Hardcoded mapping address */
			base = (void __iomem *)FSQM0_MODULE_BASE + 0xa0000000;
		else
			base = (void __iomem *)FSQM1_MODULE_BASE + 0xa0000000;
		val = readl(base + OFSQ);
		head = val & 0x7FFF;
		tail = (val >> 16) & 0x7FFF;
		PR_INFO("FSQM Head: 0x%x, Tail: 0x%x\n", head, tail);
		for (i = 0, buf_ptr = head;
			i < fsqm_buf_len[num] && buf_ptr != tail;
			i++) {
			idx = buf_ptr / 32;
			bits = buf_ptr % 32;
			if (!(check_list[idx] & BIT(bits))) {
				check_list[idx] |= BIT(bits);
			} else {
				PR_INFO("FSQM[%d] ERROR: PTR:[0x%4x] dupcate\n",
					num, buf_ptr);
				good = 0;
			}
			buf_ptr = readl(base + RAM + (buf_ptr << 2));
		}
		PR_INFO("%s: 0x%x, free buffer cnt in CBM OFSC REG: 0x%x--%s\n",
			"Total freed buffers in link list",
			i + 1, readl(base + OFSC),
			good ? "In Good State" : "In Wrong State");
		kfree(check_list);
		return count;
	}

	goto help;
exit:
	for (i = 0; i < 2; i++)
		cbm_get_free_buf(i, &free_fsqm_num[i], &fsqm_head[i],
				 &fsqm_tail[i]);
	if (cbm_skb_num)
		PR_INFO("Overall %d CBM buffer allocated for testing purpose\n",
			cbm_skb_num);
	else
		PR_INFO("All buffer already returned to CBM\n");
	for (i = 0; i < 2; i++)
		PR_INFO("FSQM%d: free buffer-%04d, head-%04d, tail-%04d\n", i,
			free_fsqm_num[i], fsqm_head[i], fsqm_tail[i]);
	return count;

help:
	PR_INFO("usage: echo alloc [cbm buffer num] > /proc/dp/%s\n",
		PROC_CBM_BUF_TEST);
	PR_INFO("usage: echo free  [cbm buffer num] > /proc/dp/%s\n",
		PROC_CBM_BUF_TEST);
	PR_INFO("Check CBM buffer list:echo check <fqsm_idx> > /proc/dp/%s\n",
		PROC_CBM_BUF_TEST);
	return count;
}

static int proc_gsw_pce_dump(struct seq_file *s, int pos)
{
	struct core_ops *gsw_handle;
	GSW_PCE_rule_t *rule;
	int i;
	GSW_return_t ret;

	rule = kmalloc(sizeof(GSW_PCE_rule_t) + 1,
		       GFP_KERNEL);
	if (!rule) {
		pos = -1;
		return pos;
	}

	/*read gswip-r rmon counter */
	gsw_handle = dp_port_prop[0].ops[1];
	rule->pattern.nIndex = pos;
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_tflow_ops
			   .TFLOW_PceRuleRead, gsw_handle, rule);
	if (ret != GSW_statusOk) {
		pos = -1;
		return pos;
	}
	if (!rule->pattern.bEnable)
		goto EXIT;

	seq_printf(s, "Pattern[%d]:-----\n", rule->pattern.nIndex);
	if (rule->pattern.bPortIdEnable) {
		seq_printf(s, "  bPortIdEnable           =   %d\n",
			   rule->pattern.bPortIdEnable);
		seq_printf(s, "  nPortId                 =   %d\n",
			   rule->pattern.nPortId);
		seq_printf(s, "  bPortId_Exclude         =   %d\n",
			   rule->pattern.bPortId_Exclude);
	}
	if (rule->pattern.bSubIfIdEnable) {
		seq_printf(s, "  bSubIfIdEnable          =   %d\n",
			   rule->pattern.bSubIfIdEnable);
		seq_printf(s, "  nSubIfId                =   %d\n",
			   rule->pattern.nSubIfId);
		seq_printf(s, "  bSubIfId_Exclude        =   %d\n",
			   rule->pattern.bSubIfId_Exclude);
	}
	if (rule->pattern.bDSCP_Enable) {
		seq_printf(s, "  bDSCP_Enable            =   %d\n",
			   rule->pattern.bDSCP_Enable);
		seq_printf(s, "  nDSCP                   =   %d\n",
			   rule->pattern.nDSCP);
		seq_printf(s, "  bDSCP_Exclude           =   %d\n",
			   rule->pattern.bDSCP_Exclude);
	}
	if (rule->pattern.bInner_DSCP_Enable) {
		seq_printf(s, "  bInner_DSCP_Enable      =   %d\n",
			   rule->pattern.bInner_DSCP_Enable);
		seq_printf(s, "  nInnerDSCP              =   %d\n",
			   rule->pattern.nInnerDSCP);
		seq_printf(s, "  bInnerDSCP_Exclude      =   %d\n",
			   rule->pattern.bInnerDSCP_Exclude);
	}
	if (rule->pattern.bPCP_Enable) {
		seq_printf(s, "  bPCP_Enable             =   %d\n",
			   rule->pattern.bPCP_Enable);
		seq_printf(s, "  nPCP                    =   %d\n",
			   rule->pattern.nPCP);
		seq_printf(s, "  bCTAG_PCP_DEI_Exclude   =   %d\n",
			   rule->pattern.bCTAG_PCP_DEI_Exclude);
	}
	if (rule->pattern.bSTAG_PCP_DEI_Enable) {
		seq_printf(s, "  bSTAG_PCP_DEI_Enable    =   %d\n",
			   rule->pattern.bSTAG_PCP_DEI_Enable);
		seq_printf(s, "  nSTAG_PCP_DEI           =   %d\n",
			   rule->pattern.nSTAG_PCP_DEI);
		seq_printf(s, "  bSTAG_PCP_DEI_Exclude   =   %d\n",
			   rule->pattern.bSTAG_PCP_DEI_Exclude);
	}
	if (rule->pattern.bPktLngEnable) {
		seq_printf(s, "  bPktLngEnable           =   %d\n",
			   rule->pattern.bPktLngEnable);
		seq_printf(s, "  nPktLng                 =   %d\n",
			   rule->pattern.nPktLng);
		seq_printf(s, "  nPktLngRange            =   %d\n",
			   rule->pattern.nPktLngRange);
		seq_printf(s, "  bPktLng_Exclude         =   %d\n",
			   rule->pattern.bPktLng_Exclude);
	}
	if (rule->pattern.bMAC_DstEnable) {
		seq_printf(s, "  bMAC_DstEnable          =   %d\n",
			   rule->pattern.bMAC_DstEnable);
		seq_printf(s,
			   "  nMAC_Dst                =   %02x:%2x:%2x:%2x:%2x:%2x\n",
			   rule->pattern.nMAC_Dst[0],
			   rule->pattern.nMAC_Dst[1],
			   rule->pattern.nMAC_Dst[2],
			   rule->pattern.nMAC_Dst[3],
			   rule->pattern.nMAC_Dst[4],
			   rule->pattern.nMAC_Dst[5]);
		seq_printf(s, "  nMAC_DstMask            =   %x\n",
			   rule->pattern.nMAC_DstMask);
		seq_printf(s, "  bDstMAC_Exclude         =   %d\n",
			   rule->pattern.bDstMAC_Exclude);
	}
	if (rule->pattern.bMAC_SrcEnable) {
		seq_printf(s, "  bMAC_SrcEnable          =   %d\n",
			   rule->pattern.bMAC_SrcEnable);
		seq_printf(s,
			   "  nMAC_Src                =   %02x:%2x:%2x:%2x:%2x:%2x\n",
			   rule->pattern.nMAC_Src[0],
			   rule->pattern.nMAC_Src[1],
			   rule->pattern.nMAC_Src[2],
			   rule->pattern.nMAC_Src[3],
			   rule->pattern.nMAC_Src[4],
			   rule->pattern.nMAC_Src[5]);
		seq_printf(s, "  nMAC_SrcMask            =   %x\n",
			   rule->pattern.nMAC_SrcMask);
		seq_printf(s, "  bSrcMAC_Exclude         =   %d\n",
			   rule->pattern.bSrcMAC_Exclude);
	}
	if (rule->pattern.bAppDataMSB_Enable) {
		seq_printf(s, "  bAppDataMSB_Enable      =   %d\n",
			   rule->pattern.bAppDataMSB_Enable);
		seq_printf(s, "  nAppDataMSB             =   %x\n",
			   rule->pattern.nAppDataMSB);
		seq_printf(s, "  bAppMaskRangeMSB_Select =   %d\n",
			   rule->pattern.bAppMaskRangeMSB_Select);
		seq_printf(s, "  nAppMaskRangeMSB        =   %x\n",
			   rule->pattern.nAppMaskRangeMSB);
		seq_printf(s, "  bAppMSB_Exclude         =   %d\n",
			   rule->pattern.bAppMSB_Exclude);
	}
	if (rule->pattern.bAppDataLSB_Enable) {
		seq_printf(s, "  bAppDataLSB_Enable      =   %d\n",
			   rule->pattern.bAppDataLSB_Enable);
		seq_printf(s, "  nAppDataLSB             =   %x\n",
			   rule->pattern.nAppDataLSB);
		seq_printf(s, "  bAppMaskRangeLSB_Select =   %d\n",
			   rule->pattern.bAppMaskRangeLSB_Select);
		seq_printf(s, "  nAppMaskRangeLSB        =   %x\n",
			   rule->pattern.nAppMaskRangeLSB);
		seq_printf(s, "  bAppLSB_Exclude         =   %d\n",
			   rule->pattern.bAppLSB_Exclude);
	}
	if (rule->pattern.eDstIP_Select) {
		seq_printf(s, "  eDstIP_Select           =   %d\n",
			   rule->pattern.eDstIP_Select);
		seq_printf(s, "  nDstIP                  =   %08x ",
			   rule->pattern.nDstIP.nIPv4);
		if (rule->pattern.eDstIP_Select == 2)
			for (i = 2; i < 8; i++)
				seq_printf(s, "%04x ",
					   rule->pattern.nDstIP.nIPv6[i]);
		seq_puts(s, "\n");
		seq_printf(s, "  nDstIP_Mask             =   %x\n",
			   rule->pattern.nDstIP_Mask);
		seq_printf(s, "  bDstIP_Exclude          =   %d\n",
			   rule->pattern.bDstIP_Exclude);
	}
	if (rule->pattern.eInnerDstIP_Select) {
		seq_printf(s, "  eInnerDstIP_Select      =   %d\n",
			   rule->pattern.eInnerDstIP_Select);
		seq_printf(s, "  nInnerDstIP             =   %x\n",
			   rule->pattern.nInnerDstIP.nIPv4);
		seq_printf(s, "  nInnerDstIP_Mask        =   %x\n",
			   rule->pattern.nInnerDstIP_Mask);
		seq_printf(s, "  bInnerDstIP_Exclude     =   %d\n",
			   rule->pattern.bInnerDstIP_Exclude);
	}
	if (rule->pattern.eSrcIP_Select) {
		seq_printf(s, "  eSrcIP_Select           =   %d\n",
			   rule->pattern.eSrcIP_Select);
		seq_printf(s, "  nSrcIP                  =   %x\n",
			   rule->pattern.nSrcIP.nIPv4);
		seq_printf(s, "  nSrcIP_Mask             =   %x\n",
			   rule->pattern.nSrcIP_Mask);
		seq_printf(s, "  bSrcIP_Exclude          =   %d\n",
			   rule->pattern.bSrcIP_Exclude);
	}
	if (rule->pattern.eInnerSrcIP_Select) {
		seq_printf(s, "  eInnerSrcIP_Select      =   %d\n",
			   rule->pattern.eInnerSrcIP_Select);
		seq_printf(s, "  nInnerSrcIP             =   %x\n",
			   rule->pattern.nInnerSrcIP.nIPv4);
		seq_printf(s, "  nInnerSrcIP_Mask        =   %x\n",
			   rule->pattern.nInnerSrcIP_Mask);
		seq_printf(s, "  bInnerSrcIP_Exclude     =   %d\n",
			   rule->pattern.bInnerSrcIP_Exclude);
	}
	if (rule->pattern.bEtherTypeEnable) {
		seq_printf(s, "  bEtherTypeEnable        =   %d\n",
			   rule->pattern.bEtherTypeEnable);
		seq_printf(s, "  nEtherType              =   %x\n",
			   rule->pattern.nEtherType);
		seq_printf(s, "  nEtherTypeMask          =   %x\n",
			   rule->pattern.nEtherTypeMask);
		seq_printf(s, "  bEtherType_Exclude      =   %d\n",
			   rule->pattern.bEtherType_Exclude);
	}
	if (rule->pattern.bProtocolEnable) {
		seq_printf(s, "  bProtocolEnable         =   %d\n",
			   rule->pattern.bProtocolEnable);
		seq_printf(s, "  nProtocol               =   %x\n",
			   rule->pattern.nProtocol);
		seq_printf(s, "  nProtocolMask           =   %x\n",
			   rule->pattern.nProtocolMask);
		seq_printf(s, "  bProtocol_Exclude       =   %d\n",
			   rule->pattern.bProtocol_Exclude);
	}
	if (rule->pattern.bInnerProtocolEnable) {
		seq_printf(s, "  bInnerProtocolEnable    =   %d\n",
			   rule->pattern.bInnerProtocolEnable);
		seq_printf(s, "  nInnerProtocol          =   %x\n",
			   rule->pattern.nInnerProtocol);
		seq_printf(s, "  nInnerProtocolMask      =   %x\n",
			   rule->pattern.nInnerProtocolMask);
		seq_printf(s, "  bInnerProtocol_Exclude  =   %d\n",
			   rule->pattern.bInnerProtocol_Exclude);
	}
	if (rule->pattern.bSessionIdEnable) {
		seq_printf(s, "  bSessionIdEnable        =   %d\n",
			   rule->pattern.bSessionIdEnable);
		seq_printf(s, "  nSessionId              =   %x\n",
			   rule->pattern.nSessionId);
		seq_printf(s, "  bSessionId_Exclude      =   %d\n",
			   rule->pattern.bSessionId_Exclude);
	}
	if (rule->pattern.bPPP_ProtocolEnable) {
		seq_printf(s, "  bPPP_ProtocolEnable     =   %d\n",
			   rule->pattern.bPPP_ProtocolEnable);
		seq_printf(s, "  nPPP_Protocol           =   %x\n",
			   rule->pattern.nPPP_Protocol);
		seq_printf(s, "  nPPP_ProtocolMask       =   %x\n",
			   rule->pattern.nPPP_ProtocolMask);
		seq_printf(s, "  bPPP_Protocol_Exclude   =   %d\n",
			   rule->pattern.bPPP_Protocol_Exclude);
	}
	if (rule->pattern.bVid) {
		seq_printf(s, "  bVid                    =   %d\n",
			   rule->pattern.bVid);
		seq_printf(s, "  nVid                    =   %d\n",
			   rule->pattern.nVid);
		seq_printf(s, "  bVid_Exclude            =   %d\n",
			   rule->pattern.bVid_Exclude);
	}
	if (rule->pattern.bSLAN_Vid) {
		seq_printf(s, "  bSLAN_Vid               =    %d\n",
			   rule->pattern.bSLAN_Vid);
		seq_printf(s, "  nSLAN_Vid               =    %d\n",
			   rule->pattern.nSLAN_Vid);
		seq_printf(s, "  bSLANVid_Exclude        =    %d\n",
			   rule->pattern.bSLANVid_Exclude);
	}
	if (rule->pattern.bPayload1_SrcEnable) {
		seq_printf(s, "  bPayload1_SrcEnable     =   %d\n",
			   rule->pattern.bPayload1_SrcEnable);
		seq_printf(s, "  nPayload1               =   %x\n",
			   rule->pattern.nPayload1);
		seq_printf(s, "  nPayload1_Mask          =   %x\n",
			   rule->pattern.nPayload1_Mask);
		seq_printf(s, "  bPayload1_Exclude       =   %d\n",
			   rule->pattern.bPayload1_Exclude);
	}
	if (rule->pattern.bPayload2_SrcEnable) {
		seq_printf(s, "  bPayload2_SrcEnable     =   %d\n",
			   rule->pattern.bPayload2_SrcEnable);
		seq_printf(s, "  nPayload2               =   %x\n",
			   rule->pattern.nPayload2);
		seq_printf(s, "  nPayload2_Mask          =   %x\n",
			   rule->pattern.nPayload2_Mask);
		seq_printf(s, "  bPayload2_Exclude       =   %d\n",
			   rule->pattern.bPayload2_Exclude);
	}
	if (rule->pattern.bParserFlagLSB_Enable) {
		seq_printf(s, "  bParserFlagLSB_Enable   =   %d\n",
			   rule->pattern.bParserFlagLSB_Enable);
		seq_printf(s, "  nParserFlagLSB          =   %x\n",
			   rule->pattern.nParserFlagLSB);
		seq_printf(s, "  nParserFlagLSB_Mask     =   %x\n",
			   rule->pattern.nParserFlagLSB_Mask);
		seq_printf(s, "  bParserFlagLSB_Exclude  =   %d\n",
			   rule->pattern.bParserFlagLSB_Exclude);
	}
	if (rule->pattern.bParserFlagMSB_Enable) {
		seq_printf(s, "  bParserFlagMSB_Enable   =   %d\n",
			   rule->pattern.bParserFlagMSB_Enable);
		seq_printf(s, "  nParserFlagMSB          =   %x\n",
			   rule->pattern.nParserFlagMSB);
		seq_printf(s, "  nParserFlagMSB_Mask     =   %x\n",
			   rule->pattern.nParserFlagMSB_Mask);
		seq_printf(s, "  bParserFlagMSB_Exclude  =   %d\n",
			   rule->pattern.bParserFlagMSB_Exclude);
	}

	seq_puts(s, "Action:\n");
	if (rule->action.eTrafficClassAction) {
		seq_printf(s, "  eTrafficClassAction      =   %d\n",
			   rule->action.eTrafficClassAction);
		seq_printf(s, "  nTrafficClassAlternate   =   %d\n",
			   rule->action.nTrafficClassAlternate);
	}
	if (rule->action.eSnoopingTypeAction)
		seq_printf(s, "  eSnoopingTypeAction      =   %d\n",
			   rule->action.eSnoopingTypeAction);
	if (rule->action.eLearningAction)
		seq_printf(s, "  eLearningAction          =   %d\n",
			   rule->action.eLearningAction);
	if (rule->action.eIrqAction)
		seq_printf(s, "  eIrqAction               =   %d\n",
			   rule->action.eIrqAction);
	if (rule->action.eCrossStateAction)
		seq_printf(s, "  eCrossStateAction        =   %d\n",
			   rule->action.eCrossStateAction);
	if (rule->action.eCritFrameAction)
		seq_printf(s, "  eCritFrameAction         =   %d\n",
			   rule->action.eCritFrameAction);
	if (rule->action.eTimestampAction) {
		seq_printf(s, "  eTimestampAction         =   %d\n",
			   rule->action.eTimestampAction);
	}
	if (rule->action.ePortMapAction) {
		seq_printf(s, "  ePortMapAction           =   %d\n",
			   rule->action.ePortMapAction);
		seq_printf(s, "  nForwardSubIfId          =   %d\n",
			   rule->action.nForwardSubIfId);
	}
	if (rule->action.bRemarkAction)
		seq_printf(s, "  bRemarkAction            =   %d\n",
			   rule->action.bRemarkAction);
	if (rule->action.bRemarkPCP)
		seq_printf(s, "  bRemarkPCP               =   %d\n",
			   rule->action.bRemarkPCP);
	if (rule->action.bRemarkSTAG_PCP)
		seq_printf(s, "  bRemarkSTAG_PCP          =   %d\n",
			   rule->action.bRemarkSTAG_PCP);
	if (rule->action.bRemarkSTAG_DEI)
		seq_printf(s, "  bRemarkSTAG_DEI          =   %d\n",
			   rule->action.bRemarkSTAG_DEI);
	if (rule->action.bRemarkDSCP)
		seq_printf(s, "  bRemarkDSCP              =   %d\n",
			   rule->action.bRemarkDSCP);
	if (rule->action.bRemarkClass) {
		seq_printf(s, "  bRemarkClass             =   %d\n",
			   rule->action.bRemarkClass);
	}
	if (rule->action.eMeterAction) {
		seq_printf(s, "  eMeterAction             =   %d\n",
			   rule->action.eMeterAction);
		seq_printf(s, "  nMeterId                 =   %d\n",
			   rule->action.nMeterId);
	}
	if (rule->action.bRMON_Action) {
		seq_printf(s, "  bRMON_Action             =   %d\n",
			   rule->action.bRMON_Action);
		seq_printf(s, "  nRMON_Id                 =   %d\n",
			   rule->action.nRMON_Id);
	}
	if (rule->action.eVLAN_Action) {
		seq_printf(s, "  eVLAN_Action             =   %d\n",
			   rule->action.eVLAN_Action);
		seq_printf(s, "  nVLAN_Id                 =   %d\n",
			   rule->action.nVLAN_Id);
	}
	if (rule->action.eSVLAN_Action) {
		seq_printf(s, "  eSVLAN_Action            =   %d\n",
			   rule->action.eSVLAN_Action);
		seq_printf(s, "  nSVLAN_Id                =   %d\n",
			   rule->action.nSVLAN_Id);
	}
	if (rule->action.eVLAN_CrossAction)
		seq_printf(s, "  eVLAN_CrossAction        =   %d\n",
			   rule->action.eVLAN_CrossAction);
	if (rule->action.nFId)
		seq_printf(s, "  nFId                     =   %d\n",
			   rule->action.nFId);
	if (rule->action.bPortBitMapMuxControl)
		seq_printf(s, "  bPortBitMapMuxControl    =   %d\n",
			   rule->action.bPortBitMapMuxControl);
	if (rule->action.bPortTrunkAction)
		seq_printf(s, "  bPortTrunkAction         =   %d\n",
			   rule->action.bPortTrunkAction);
	if (rule->action.bPortLinkSelection)
		seq_printf(s, "  bPortLinkSelection       =   %d\n",
			   rule->action.bPortLinkSelection);
	if (rule->action.bCVLAN_Ignore_Control)
		seq_printf(s, "  bCVLAN_Ignore_Control    =   %d\n",
			   rule->action.bCVLAN_Ignore_Control);
	if (rule->action.bFlowID_Action) {
		seq_printf(s, "  bFlowID_Action           =   %d\n",
			   rule->action.bFlowID_Action);
		seq_printf(s, "  nFlowID                  =   %d\n",
			   rule->action.nFlowID);
	}
	if (rule->action.bRoutExtId_Action) {
		seq_printf(s, "  bRoutExtId_Action        =   %d\n",
			   rule->action.bRoutExtId_Action);
		seq_printf(s, "  nRoutExtId               =   %d\n",
			   rule->action.nRoutExtId);
	}
	if (rule->action.bRtDstPortMaskCmp_Action)
		seq_printf(s, "  bRtDstPortMaskCmp_Action =   %d\n",
			   rule->action.bRtDstPortMaskCmp_Action);
	if (rule->action.bRtSrcPortMaskCmp_Action)
		seq_printf(s, "  bRtSrcPortMaskCmp_Action =   %d\n",
			   rule->action.bRtSrcPortMaskCmp_Action);
	if (rule->action.bRtDstIpMaskCmp_Action)
		seq_printf(s, "  bRtDstIpMaskCmp_Action   =   %d\n",
			   rule->action.bRtDstIpMaskCmp_Action);
	if (rule->action.bRtSrcIpMaskCmp_Action)
		seq_printf(s, "  bRtSrcIpMaskCmp_Action   =   %d\n",
			   rule->action.bRtSrcIpMaskCmp_Action);
	if (rule->action.bRtInnerIPasKey_Action)
		seq_printf(s, "  bRtInnerIPasKey_Action   =   %d\n",
			   rule->action.bRtInnerIPasKey_Action);
	if (rule->action.bRtAccelEna_Action)
		seq_printf(s, "  bRtAccelEna_Action       =   %d\n",
			   rule->action.bRtAccelEna_Action);
	if (rule->action.bRtCtrlEna_Action)
		seq_printf(s, "  bRtCtrlEna_Action        =   %d\n",
			   rule->action.bRtCtrlEna_Action);
	if (rule->action.eProcessPath_Action)
		seq_printf(s, "  eProcessPath_Action      =   %d\n",
			   rule->action.eProcessPath_Action);
	if (rule->action.ePortFilterType_Action)
		seq_printf(s, "  ePortFilterType_Action   =   %d\n",
			   rule->action.ePortFilterType_Action);
	seq_puts(s, "\n");
 EXIT:
	kfree(rule);
	pos++;

	return pos;
}

static int proc_gsw_pce_start(void)
{
	return 0;
}

static char *get_pae_ip_type(int type)
{
	if (type == GSW_RT_IP_V4)
		return "IPV4";
	if (type == GSW_RT_IP_V6)
		return "IPV6";
	return "Unknown";
}

static char *get_pae_tunnel_type(int type)
{
	if (type == GSW_ROUTE_TUNL_NULL)
		return "NULL";
	if (type == GSW_ROUTE_TUNL_6RD)
		return "6RD";
	if (type == GSW_ROUTE_TUNL_DSLITE)
		return "Dslite";
	if (type == GSW_ROUTE_TUNL_L2TP)
		return "L2TP";
	if (type == GSW_ROUTE_TUNL_IPSEC)
		return "IPSEC";
	return "Unknown";
}

static char *get_pae_ext_type(int type)
{
	if (type == 100)
		return "UDP";
	if (type == 0)
		return "TCP";
	return "Unknown";
}

static char *get_pae_dir_type(int type)
{
	if (type == GSW_ROUTE_DIRECTION_DNSTREAM)
		return "DownStream";
	if (type == GSW_ROUTE_DIRECTION_UPSTREAM)
		return "UpStream";
	return "Unknown";
}

static char *pae_pppoe_mode_s(int type)
{
	if (type == 0)
		return "Transparent";
	if (type == GSW_ROUTE_DIRECTION_UPSTREAM)
		return "Termination";
	return "Unknown";
}

static char *pae_rout_mode_s(int type)
{
	if (type == GSW_ROUTE_MODE_NULL)
		return "NULL";
	if (type == GSW_ROUTE_MODE_ROUTING)
		return "Basic Routing";
	if (type == GSW_ROUTE_MODE_NAT)
		return "NAT";
	if (type == GSW_ROUTE_MODE_NAPT)
		return "NAPT";
	return "Unknown";
}

static char *get_pae_out_dscp_type(int type)
{
	if (type == GSW_ROUTE_OUT_DSCP_NULL)
		return "No Outer DSCP Marking";
	if (type == GSW_ROUTE_OUT_DSCP_INNER)
		return "Outer DSCP from Inner IP header";
	if (type == GSW_ROUTE_OUT_DSCP_SESSION)
		return "Outer DSCP from Session action";
	return "Unknown";
}

/* For proc only, no protection */

static char *get_pae_port_list(u32 port)
{
	int i, k;
	static char list[PMAC_MAX_NUM * 2 + 1];

	k = 0;
	list[0] = 0;
	for (i = 0; i < PMAC_MAX_NUM; i++) {
		if (port & (1 << i)) {
			if (k)
				sprintf(list + strlen(list), "/");
			sprintf(list + strlen(list), "%d", i);
			k++;
		}
	}
	return list;
}

/* return 0 -- ok */
static int dp_route_dump_seq(struct seq_file *seq, GSW_ROUTE_Entry_t *rt_entry)
{
	seq_printf(seq, "Index[%04d] Hash=%u: %s(%u)\n",
		   rt_entry->nRtIndex, rt_entry->nHashVal,
		   (rt_entry->routeEntry.pattern.bValid ==
			   LTQ_TRUE) ? "Valid" : "Not Valid",
		   rt_entry->routeEntry.pattern.bValid);
	seq_puts(seq, " Compare:\n");
	seq_printf(seq, "   IP Type         = %d (%s)\n",
		   rt_entry->routeEntry.pattern.eIpType,
		   get_pae_ip_type(rt_entry->routeEntry.pattern.eIpType));
	if (rt_entry->routeEntry.action.eIpType == GSW_RT_IP_V6)
		seq_printf(seq, "   Src IP          = %pI6\n",
			   rt_entry->routeEntry.pattern.nSrcIP.nIPv6);
	else
		seq_printf(seq, "   Src IP          = %pI4\n",
			   &rt_entry->routeEntry.pattern.nSrcIP.nIPv4);

	if (rt_entry->routeEntry.pattern.eIpType == GSW_RT_IP_V6)
		seq_printf(seq, "   Dest IP         = %pI6\n",
			   rt_entry->routeEntry.pattern.nDstIP.nIPv6);
	else
		seq_printf(seq, "   Dest IP         = %pI4\n",
			   &rt_entry->routeEntry.pattern.nDstIP.nIPv4);

	seq_printf(seq, "   Src Port        = %d\n",
		   rt_entry->routeEntry.pattern.nSrcPort);
	seq_printf(seq, "   Dest Port       = %d\n",
		   rt_entry->routeEntry.pattern.nDstPort);
	seq_printf(seq, "   Extn Id         = %d (%s)\n",
		   rt_entry->routeEntry.pattern.nRoutExtId,
		   get_pae_ext_type(rt_entry->routeEntry.pattern.
					   nRoutExtId));
	seq_puts(seq, " Action:\n");
	seq_printf(seq, "   Dst PMAC List   = 0x%0x (%s)\n",
		   rt_entry->routeEntry.action.nDstPortMap,
		   get_pae_port_list(rt_entry->routeEntry.action.
					    nDstPortMap));
	seq_printf(seq, "   Subif           = 0x%0x\n",
		   rt_entry->routeEntry.action.nDstSubIfId);
	seq_printf(seq, "   IP Type         = %d (%s)\n",
		   rt_entry->routeEntry.action.eIpType,
		   get_pae_ip_type(rt_entry->routeEntry.action.eIpType));
	if (rt_entry->routeEntry.action.eIpType == GSW_RT_IP_V6)
		seq_printf(seq, "   NAT IP          = %pI6\n",
			   rt_entry->routeEntry.action.nNATIPaddr.
				  nIPv6);
	else
		seq_printf(seq, "   NAT IP          = %pI4\n",
			   &rt_entry->routeEntry.action.nNATIPaddr.
				  nIPv4);
	seq_printf(seq, "   NAT Port        = %d\n",
		   rt_entry->routeEntry.action.nTcpUdpPort);
	seq_printf(seq, "   MTU             = %d\n",
		   rt_entry->routeEntry.action.nMTUvalue);
	seq_printf(seq, "   Src MAC         = %pM (%s)\n",
		   rt_entry->routeEntry.action.nSrcMAC,
		   rt_entry->routeEntry.action.
			  bMAC_SrcEnable ? "Enabled" : "Disabled");
	seq_printf(seq, "   Dst MAC         = %pM (%s)\n",
		   rt_entry->routeEntry.action.nDstMAC,
		   rt_entry->routeEntry.action.
			  bMAC_DstEnable ? "Enabled" : "Disabled");
	seq_printf(seq, "   PPPoE Mode      = %u (%s)\n",
		   rt_entry->routeEntry.action.bPPPoEmode,
		   pae_pppoe_mode_s(rt_entry->routeEntry.action.
						  bPPPoEmode));
	seq_printf(seq, "   PPPoE SessID    = %u\n",
		   rt_entry->routeEntry.action.nPPPoESessId);
	seq_printf(seq, "   Dir             = %u (%s)\n",
		   rt_entry->routeEntry.action.eSessDirection,
		   get_pae_dir_type(rt_entry->routeEntry.action.
					   eSessDirection));
	seq_printf(seq, "   Class           = %u (%s)\n",
		   rt_entry->routeEntry.action.nTrafficClass,
		   rt_entry->routeEntry.action.
			  bTCremarking ? "Enabled" : "Disabled");
	seq_printf(seq, "   Routing Mode    = %u (%s)\n",
		   rt_entry->routeEntry.action.eSessRoutingMode,
		   pae_rout_mode_s(rt_entry->routeEntry.action.
			  eSessRoutingMode));
	seq_printf(seq, "   Tunnel Type     = %u (%s: %s\n",
		   rt_entry->routeEntry.action.eTunType,
		   get_pae_tunnel_type(rt_entry->routeEntry.action.
					      eTunType),
		   rt_entry->routeEntry.action.
			  bTunnel_Enable ? "Enabled" : "Disabled");
	seq_printf(seq, "   Tunnel Index    = %u\n",
		   rt_entry->routeEntry.action.nTunnelIndex);
	seq_printf(seq, "   MeterID         = %u (%s)\n",
		   rt_entry->routeEntry.action.nMeterId,
		   rt_entry->routeEntry.action.
			  bMeterAssign ? "Enabled" : "Disabled");
	seq_printf(seq, "   TTL  Decrease   = %u (%s)\n",
		   rt_entry->routeEntry.action.bTTLDecrement,
		   rt_entry->routeEntry.action.
			  bTTLDecrement ? "Enabled" : "Disabled");
	seq_printf(seq, "   OutDSCP         = %u (%s)\n",
		   rt_entry->routeEntry.action.eOutDSCPAction,
		   get_pae_out_dscp_type(rt_entry->routeEntry.
			action.eOutDSCPAction));
	seq_printf(seq, "   InDSCP          = %u (%s)\n",
		   rt_entry->routeEntry.action.bInnerDSCPRemark,
		   rt_entry->routeEntry.action.
		   bInnerDSCPRemark ? "Enabled" : "Disabled");
	seq_printf(seq, "   DSCP            = %u\n",
		   rt_entry->routeEntry.action.nDSCP);
	seq_printf(seq, "   RTP             = %s (seq=%u roll=%u)\n",
		   rt_entry->routeEntry.action.
			  bRTPMeasEna ? "Enabled" : "Disabled",
		   rt_entry->routeEntry.action.nRTPSeqNumber,
		   rt_entry->routeEntry.action.nRTPSessionPktCnt);
	seq_printf(seq, "   FID             = %u\n",
		   rt_entry->routeEntry.action.nFID);
	seq_printf(seq, "   Flow ID         = %u\n",
		   rt_entry->routeEntry.action.nFlowId);
	seq_printf(seq, "   Hit Status      = %u\n",
		   rt_entry->routeEntry.action.bHitStatus);
	seq_printf(seq, "   Session Counters= %u\n",
		   rt_entry->routeEntry.action.nSessionCtrs);
	seq_puts(seq, "\n");
	return 0;
}

static int dp_route_dump_pr(GSW_ROUTE_Entry_t *rt_entry)
{
	PR_INFO("Index[%04d] Hash=%u: %s(%u)\n",
		rt_entry->nRtIndex, rt_entry->nHashVal,
		(rt_entry->routeEntry.pattern.bValid == LTQ_TRUE) ?
		"Valid" : "Not Valid",
		rt_entry->routeEntry.pattern.bValid);
	PR_INFO(" Compare:\n");
	PR_INFO("   IP Type         = %d (%s)\n",
		rt_entry->routeEntry.pattern.eIpType,
		get_pae_ip_type(rt_entry->routeEntry.pattern.eIpType));
	if (rt_entry->routeEntry.action.eIpType == GSW_RT_IP_V6)
		PR_INFO("   Src IP          = %pI6\n",
			rt_entry->routeEntry.pattern.nSrcIP.nIPv6);
	else
		PR_INFO("   Src IP          = %pI4\n",
			&rt_entry->routeEntry.pattern.nSrcIP.nIPv4);

	if (rt_entry->routeEntry.pattern.eIpType == GSW_RT_IP_V6)
		PR_INFO("   Dest IP         = %pI6\n",
			rt_entry->routeEntry.pattern.nDstIP.nIPv6);
	else
		PR_INFO("   Dest IP         = %pI4\n",
			&rt_entry->routeEntry.pattern.nDstIP.nIPv4);

	PR_INFO("   Src Port        = %d\n",
		rt_entry->routeEntry.pattern.nSrcPort);
	PR_INFO("   Dest Port       = %d\n",
		rt_entry->routeEntry.pattern.nDstPort);
	PR_INFO("   Extn Id         = %d (%s)\n",
		rt_entry->routeEntry.pattern.nRoutExtId,
		get_pae_ext_type(rt_entry->routeEntry.pattern.nRoutExtId));
	PR_INFO(" Action:\n");
	PR_INFO("   Dst PMAC List   = 0x%0x (%s)\n",
		rt_entry->routeEntry.action.nDstPortMap,
		get_pae_port_list(rt_entry->routeEntry.action.nDstPortMap));
	PR_INFO("   Subif           = 0x%0x\n",
		rt_entry->routeEntry.action.nDstSubIfId);
	PR_INFO("   IP Type         = %d (%s)\n",
		rt_entry->routeEntry.action.eIpType,
		get_pae_ip_type(rt_entry->routeEntry.action.eIpType));
	if (rt_entry->routeEntry.action.eIpType == GSW_RT_IP_V6)
		PR_INFO("   NAT IP          = %pI6\n",
			rt_entry->routeEntry.action.nNATIPaddr.nIPv6);
	else
		PR_INFO("   NAT IP          = %pI4\n",
			&rt_entry->routeEntry.action.nNATIPaddr.nIPv4);
	PR_INFO("   NAT Port        = %d\n",
		rt_entry->routeEntry.action.nTcpUdpPort);
	PR_INFO("   MTU             = %d\n",
		rt_entry->routeEntry.action.nMTUvalue);
	PR_INFO("   Src MAC         = %pM (%s)\n",
		rt_entry->routeEntry.action.nSrcMAC,
		rt_entry->routeEntry.action.bMAC_SrcEnable ?
		"Enabled" : "Disabled");
	PR_INFO("   Dst MAC         = %pM (%s)\n",
		rt_entry->routeEntry.action.nDstMAC,
		rt_entry->routeEntry.action.bMAC_DstEnable ?
		"Enabled" : "Disabled");
	PR_INFO("   PPPoE Mode      = %u (%s)\n",
		rt_entry->routeEntry.action.bPPPoEmode,
		pae_pppoe_mode_s(rt_entry->routeEntry.action.bPPPoEmode));
	PR_INFO("   PPPoE SessID    = %u\n",
		rt_entry->routeEntry.action.nPPPoESessId);
	PR_INFO("   Dir             = %u (%s)\n",
		rt_entry->routeEntry.action.eSessDirection,
		get_pae_dir_type(rt_entry->routeEntry.action.eSessDirection));
	PR_INFO("   Class           = %u (%s)\n",
		rt_entry->routeEntry.action.nTrafficClass,
		rt_entry->routeEntry.action.bTCremarking ?
		"Enabled" : "Disabled");
	PR_INFO("   Routing Mode    = %u (%s)\n",
		rt_entry->routeEntry.action.eSessRoutingMode,
		pae_rout_mode_s(rt_entry->routeEntry.action.eSessRoutingMode));
	PR_INFO("   Tunnel Type     = %u (%s: %s\n",
		rt_entry->routeEntry.action.eTunType,
		get_pae_tunnel_type(rt_entry->routeEntry.action.eTunType),
		rt_entry->routeEntry.action.bTunnel_Enable ?
		"Enabled" : "Disabled");
	PR_INFO("   Tunnel Index    = %u\n",
		rt_entry->routeEntry.action.nTunnelIndex);
	PR_INFO("   MeterID         = %u (%s)\n",
		rt_entry->routeEntry.action.nMeterId,
		rt_entry->routeEntry.action.
		bMeterAssign ? "Enabled" : "Disabled");
	PR_INFO("   TTL  Decrease   = %u (%s)\n",
		rt_entry->routeEntry.action.bTTLDecrement,
		rt_entry->routeEntry.action.
		bTTLDecrement ? "Enabled" : "Disabled");
	PR_INFO("   OutDSCP         = %u (%s)\n",
		rt_entry->routeEntry.action.eOutDSCPAction,
		get_pae_out_dscp_type(rt_entry->routeEntry.
	       action.eOutDSCPAction));
	PR_INFO("   InDSCP          = %u (%s)\n",
		rt_entry->routeEntry.action.bInnerDSCPRemark,
		rt_entry->routeEntry.action.
	       bInnerDSCPRemark ? "Enabled" : "Disabled");
	PR_INFO("   DSCP            = %u\n",
		rt_entry->routeEntry.action.nDSCP);
	PR_INFO("   RTP             = %s (seq=%u roll=%u)\n",
		rt_entry->routeEntry.action.bRTPMeasEna ? "Enable" : "Disable",
		rt_entry->routeEntry.action.nRTPSeqNumber,
		rt_entry->routeEntry.action.nRTPSessionPktCnt);
	PR_INFO("   FID             = %u\n",
		rt_entry->routeEntry.action.nFID);
	PR_INFO("   Flow ID         = %u\n",
		rt_entry->routeEntry.action.nFlowId);
	PR_INFO("   Hit Status      = %u\n",
		rt_entry->routeEntry.action.bHitStatus);
	PR_INFO("   Session Counters= %u\n",
		rt_entry->routeEntry.action.nSessionCtrs);
	PR_INFO("\n");
	return 0;
}

ssize_t proc_gsw_route_write(struct file *file, const char *buf,
			     size_t count, loff_t *ppos)
{
	u16 len, i, tmp, start_param;
	GSW_return_t ret = 0;
	char *str = NULL;
	char *param_list[30 * 2];
	unsigned int num;
	GSW_ROUTE_Entry_t *rt_entry;
	struct core_ops *gsw_handle;
	u8 dscp_f = 0;

	gsw_handle = dp_port_prop[0].ops[1];
	str = kmalloc(count + 1, GFP_KERNEL);
	if (!str)
		return count;
	rt_entry = kmalloc(sizeof(GSW_ROUTE_Entry_t) + 1,
			   GFP_KERNEL);
	if (!rt_entry) {
		kfree(str);
		return count;
	}

	len = count;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;

	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if (num < 2) {
		PR_INFO("parameter %d not enough. count=%d\n", num, count);
		goto help;
	}
	if (dp_strncmpi(param_list[0],
			"help", strlen("help")) == 0)	/* help */
		goto help;

	/* delete an entry */
	if (dp_strncmpi(param_list[0], "del", strlen("del")) == 0) {
		rt_entry->nRtIndex = dp_atoi(param_list[1]);
		DP_DEBUG(DP_DBG_FLAG_PAE, "Delete pae entry %d\n",
			 rt_entry->nRtIndex);
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pae_ops
				   .ROUTE_SessionEntryDel, gsw_handle,
				   rt_entry);
		if (ret != GSW_statusOk) {
			PR_ERR("GSW_ROUTE_ENTRY_DELETE returned failure\n");
			goto exit;
		}
		kfree(str);
		kfree(rt_entry);
		return count;
	}

	/* dump an entry */
	if (dp_strncmpi(param_list[0], "dump", strlen("dump")) == 0) {
		rt_entry->nRtIndex = dp_atoi(param_list[1]);
		DP_DEBUG(DP_DBG_FLAG_PAE, "Dump pae entry %d\n",
			 rt_entry->nRtIndex);
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pae_ops
				   .ROUTE_SessionEntryRead, gsw_handle,
				   rt_entry);
		if (ret != GSW_statusOk) {
			PR_ERR("GSW_ROUTE_ENTRY_DELETE returned failure\n");
			goto exit;
		}
		dp_route_dump_pr(rt_entry);
		kfree(str);
		kfree(rt_entry);
		return count;
	}

	/* Modify an entry */
	if (dp_strncmpi(param_list[0], "modify", strlen("modify")) == 0) {
		rt_entry->nRtIndex = dp_atoi(param_list[1]);
		/*read back before delete and add it new */
		DP_DEBUG(DP_DBG_FLAG_PAE, "Dump pae entry %d\n",
			 rt_entry->nRtIndex);
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pae_ops
				   .ROUTE_SessionEntryRead, gsw_handle,
				   rt_entry);
		if (ret != GSW_statusOk) {
			PR_ERR("GSW_ROUTE_ENTRY_DELETE returned failure\n");
			goto exit;
		}
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pae_ops
				   .ROUTE_SessionEntryDel, gsw_handle,
				   rt_entry);
		if (ret != GSW_statusOk) {
			PR_ERR("GSW_ROUTE_ENTRY_DELETE returned failure\n");
			goto exit;
		}
		rt_entry->nHashVal = -1; /*since GSWAPI no modify support,
					  *here switch to add command
					  */
		start_param = 2;
		goto ADD_MODIFY_BOTH;
	}

	/* add a new entry */
	if (dp_strncmpi(param_list[0], "add", strlen("add")) != 0) {
		PR_INFO("wrong command: %s\n", param_list[0]);
		goto help;
	}
	memset(rt_entry, 0, sizeof(*rt_entry));
	rt_entry->nHashVal = -1;
	rt_entry->bPrio = 1;
	rt_entry->routeEntry.action.nMTUvalue = 1501;
	rt_entry->routeEntry.pattern.bValid = LTQ_TRUE;
	start_param = 1;
 ADD_MODIFY_BOTH:
	for (i = start_param; i < num; i += 2) {
		/*compare table */
		if (dp_strncmpi(param_list[i], "SrcIP", strlen("SrcIP")) == 0) {
			tmp =
			    pton(param_list[i + 1],
				 &rt_entry->routeEntry.pattern.nSrcIP);
			if (tmp == 4)
				rt_entry->routeEntry.pattern.eIpType =
				    GSW_RT_IP_V4;
			else if (tmp == 6)
				rt_entry->routeEntry.pattern.eIpType =
				    GSW_RT_IP_V6;
			else {
				PR_INFO("Wong IP format for SrcIP\n");
				goto exit;
			}
		} else if (dp_strncmpi(param_list[i],
				     "DstIP",
					 strlen("DstIP"))
					 == 0) {
			tmp =
			    pton(param_list[i + 1],
				 &rt_entry->routeEntry.pattern.nDstIP);
			if (tmp == 4)
				rt_entry->routeEntry.pattern.eIpType =
				    GSW_RT_IP_V4;
			else if (tmp == 6)
				rt_entry->routeEntry.pattern.eIpType =
				    GSW_RT_IP_V6;
			else {
				PR_INFO("Wong IP format for DstIP\n");
				goto exit;
			}
		} else if (dp_strncmpi(param_list[i],
				     "SrcPort",
					 strlen("SrcPort"))
					 == 0)
			rt_entry->routeEntry.pattern.nSrcPort =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "DstPort",
					 strlen("DstPort"))
					 == 0)
			rt_entry->routeEntry.pattern.nDstPort =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "ExtId",
					 strlen("ExtId"))
					 == 0)
			rt_entry->routeEntry.pattern.nRoutExtId =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "SrcMac",
					 strlen("SrcMac"))
					 == 0) {
			rt_entry->routeEntry.action.bMAC_SrcEnable = 1;
			mac_stob(param_list[i + 1],
				 rt_entry->routeEntry.action.nSrcMAC);

			if (rt_entry->routeEntry.action.eSessRoutingMode <
			    GSW_ROUTE_MODE_ROUTING) /*normally only
						     *routing mode will
						     *change mac
						     */
				rt_entry->routeEntry.action.eSessRoutingMode =
				    GSW_ROUTE_MODE_ROUTING;
		} /*below is all actions */
		else if (dp_strncmpi(param_list[i],
				     "DstMac",
					 strlen("DstMac"))
					 == 0) {
			rt_entry->routeEntry.action.bMAC_DstEnable = 1;
			mac_stob(param_list[i + 1],
				 rt_entry->routeEntry.action.nDstMAC);

			if (rt_entry->routeEntry.action.eSessRoutingMode <
			    GSW_ROUTE_MODE_ROUTING) /*normally only routing
						     *mode will change mac
						     */
				rt_entry->routeEntry.action.eSessRoutingMode =
				    GSW_ROUTE_MODE_ROUTING;
		} else if (dp_strncmpi(param_list[i],
				     "NatIP",
					 strlen("NatIP"))
					 == 0) {
			tmp =
			    pton(param_list[i + 1],
				 &rt_entry->routeEntry.action.nNATIPaddr);
			if (tmp == 4)
				rt_entry->routeEntry.action.eIpType =
				    GSW_RT_IP_V4;
			else if (tmp == 6)
				rt_entry->routeEntry.action.eIpType =
				    GSW_RT_IP_V6;
			else {
				PR_INFO("Wong IP format for NatIP\n");
				goto exit;
			}
			if (rt_entry->routeEntry.action.eSessRoutingMode <
			    GSW_ROUTE_MODE_NAT)
				rt_entry->routeEntry.action.eSessRoutingMode =
				GSW_ROUTE_MODE_NAT;	/* NAT */
		} else if (dp_strncmpi(param_list[i],
				     "NatPort",
					 strlen("NatPort"))
					 == 0) {
			rt_entry->routeEntry.action.nTcpUdpPort =
			    dp_atoi(param_list[i + 1]);
			if (rt_entry->routeEntry.action.eSessRoutingMode <
			    GSW_ROUTE_MODE_NAPT)
				rt_entry->routeEntry.action.eSessRoutingMode =
				GSW_ROUTE_MODE_NAPT;/* NAPT */
		} else if (dp_strncmpi(param_list[i],
				     "MTU",
					 strlen("MTU"))
					 == 0)
			rt_entry->routeEntry.action.nMTUvalue =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "PPPoEmode",
					 strlen("PPPoEmode"))
					 == 0)
			rt_entry->routeEntry.action.bPPPoEmode =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "PPPoEId",
					 strlen("PPPoEId"))
					 == 0)
			rt_entry->routeEntry.action.nPPPoESessId =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "TunType",
					 strlen("TunType"))
					 == 0) {
			rt_entry->routeEntry.action.bTunnel_Enable = 1;
			rt_entry->routeEntry.action.eTunType =
			    dp_atoi(param_list[i + 1]);
		} else if (dp_strncmpi(param_list[i],
				     "TunIndex",
					 strlen("TunIndex"))
					 == 0) {
			rt_entry->routeEntry.action.bTunnel_Enable = 1;
			rt_entry->routeEntry.action.eTunType =
			    dp_atoi(param_list[i + 1]);

		} else if (dp_strncmpi(param_list[i],
				     "MeterId",
					 strlen("MeterId"))
					 == 0) {
			rt_entry->routeEntry.action.bMeterAssign = 1;
			rt_entry->routeEntry.action.nMeterId =
			    dp_atoi(param_list[i + 1]);

		} else if (dp_strncmpi(param_list[i],
				     "FID",
					 strlen("FID"))
					 == 0)
			rt_entry->routeEntry.action.nFID =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "FlowId",
					 strlen("FlowId"))
					 == 0)
			rt_entry->routeEntry.action.nFlowId =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "OutDscp",
					 strlen("OutDscp"))
					 == 0)
			rt_entry->routeEntry.action.eOutDSCPAction =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "InDscp",
					 strlen("InDscp"))
					 == 0)
			rt_entry->routeEntry.action.bInnerDSCPRemark =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "Dscp",
					 strlen("Dscp"))
					 == 0) {
			rt_entry->routeEntry.action.nDSCP =
			    dp_atoi(param_list[i + 1]);
			dscp_f = 1;
		} else if (dp_strncmpi(param_list[i],
				     "class",
					 strlen("class"))
					 == 0) {
			rt_entry->routeEntry.action.bTCremarking = 1;
			rt_entry->routeEntry.action.nTrafficClass =
			    dp_atoi(param_list[i + 1]);
		} else if (dp_strncmpi(param_list[i],
				     "ttl",
					 strlen("ttl"))
					 == 0)
			rt_entry->routeEntry.action.bTTLDecrement =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "dir",
					 strlen("dir"))
					 == 0)
			rt_entry->routeEntry.action.eSessDirection =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "DstPmac",
					 strlen("DstPmac"))
					 == 0)
			rt_entry->routeEntry.action.nDstPortMap =
			    dp_atoi(param_list[i + 1]);
		else if (dp_strncmpi(param_list[i],
				     "Subif",
					 strlen("Subif"))
					 == 0)
			rt_entry->routeEntry.action.nDstSubIfId =
			    dp_atoi(param_list[i + 1]);
		else {
			PR_INFO("wrong parameter[%d]: %s\n", i, param_list[i]);
			goto exit;
		}

		if (!rt_entry->routeEntry.action.bTTLDecrement &&
		    (rt_entry->routeEntry.action.eSessRoutingMode >
		     GSW_ROUTE_MODE_NULL))
			rt_entry->routeEntry.action.bTTLDecrement = 1;

		/* if key in dscp but no inner/outer dscp action enabled,
		 * then auto enable indscp action
		 */
		if (dscp_f &&
		    !rt_entry->routeEntry.action.eOutDSCPAction &&
		    !rt_entry->routeEntry.action.bInnerDSCPRemark)
			rt_entry->routeEntry.action.bInnerDSCPRemark = 1;
		/*nSessionCtrs ??*/
	}
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pae_ops
				   .ROUTE_SessionEntryAdd, gsw_handle,
				   rt_entry);
	if (ret < GSW_statusOk) {
		PR_ERR("GSW_ROUTE_ENTRY_ADD returned failure\n");
		goto exit;
	}
	DP_DEBUG(DP_DBG_FLAG_PAE, "pae entry %d updated\n",
		 rt_entry->nRtIndex);

	dp_route_dump_pr(rt_entry);

 exit:
	kfree(str);
	kfree(rt_entry);
	return count;

 help:
	PR_INFO("usage:\n");
	PR_INFO("  echo del	<entry-index> > /prooc/dp/%s\n",
		PROC_ROUTE);
	PR_INFO("  echo show <entry-index> > /prooc/dp/%s\n",
		PROC_ROUTE);
	PR_INFO("  echo add %s %s\n",
		"[SrcIP] [IP-value] [DstIP] [IP-value] [SrcPort] [Port-value]",
		"[DstPort] [Port-value] [ExtId] [ExtId-value]");
	PR_INFO("           %s [NatIP] [IP-value] [NatPort] [Port-value]\n",
		"[dir] [dir-value] [SrcMAC] [MAC-value] [DstMAC] [MAC-value]");
	PR_INFO("           %s [PPPoEId-value] [TunType] [Tunnel-value]\n",
		"[MTU] [MTU-value] [PPPoEmode] [PPPoEmode-value] [PPPoEId]");
	PR_INFO("           %s [FID] [FID-value] [FlowId] [FlowId-value]\n",
		"[TunIndex] [Tunnel-index-value] [MeterId] [MeterId-value]");
	PR_INFO("           %s [OutDscp-value] [class] [class-value]\n",
		"[InDscp] [InDscp-value] [Dscp] [Dscp-value] [OutDscp]");
	PR_INFO("           [DstPmac] [DstPmac-value] [Subif] [Subif-value]\n");
	PR_INFO("		> /prooc/dp/%s\n", PROC_ROUTE);
	PR_INFO("  echo modify <entry-index> %s > /prooc/dp/%s\n",
		"[followed by paramers as add command]",
		PROC_ROUTE);

	PR_INFO(" Take note:\n");
	PR_INFO("     Only MAC address learned session is accelerated by HW\n");
	PR_INFO("     After modify entry, its entry index maybe changed\n");
	PR_INFO("     ExtId: %d(%s)/%d(%s)\n", 0, get_pae_ext_type(0), 100,
		get_pae_ext_type(100));
	PR_INFO("     Dir: %d(%s)/%d(%s)\n", GSW_ROUTE_DIRECTION_DNSTREAM,
		get_pae_dir_type(GSW_ROUTE_DIRECTION_DNSTREAM),
		GSW_ROUTE_DIRECTION_UPSTREAM,
		get_pae_dir_type(GSW_ROUTE_DIRECTION_UPSTREAM));
	PR_INFO("     OutDscp: %d(%s)/%d(%s)/%d(%s)\n",
		GSW_ROUTE_OUT_DSCP_NULL,
		get_pae_dir_type(GSW_ROUTE_OUT_DSCP_NULL),
		GSW_ROUTE_OUT_DSCP_INNER,
		get_pae_out_dscp_type(GSW_ROUTE_OUT_DSCP_INNER),
		GSW_ROUTE_OUT_DSCP_SESSION,
		get_pae_out_dscp_type(GSW_ROUTE_OUT_DSCP_SESSION));
	PR_INFO("     Tunnel: %d(%s)/%d(%s)/%d(%s)/%d(%s)/%d(%s)\n",
		GSW_ROUTE_TUNL_NULL, get_pae_tunnel_type(GSW_ROUTE_TUNL_NULL),
		GSW_ROUTE_TUNL_6RD, get_pae_tunnel_type(GSW_ROUTE_TUNL_6RD),
		GSW_ROUTE_TUNL_DSLITE,
		get_pae_tunnel_type(GSW_ROUTE_TUNL_DSLITE),
		GSW_ROUTE_TUNL_L2TP, get_pae_tunnel_type(GSW_ROUTE_TUNL_L2TP),
		GSW_ROUTE_TUNL_IPSEC,
		get_pae_tunnel_type(GSW_ROUTE_TUNL_IPSEC));
	PR_INFO("     PPPoEmode: %d(%s)/%d(%s)\n", 0,
		pae_pppoe_mode_s(0), 1, pae_pppoe_mode_s(1));
	PR_INFO("     TTL/Route Mode/IPV4/6 auto handled inside the proc\n");
	PR_INFO("     DstPmac:bit 0 for pmac port 0, 1 for pmac port 1....\n");
	PR_INFO("     Subif(ATM bit format): %s\n",
		"ATM-QID[6:3] Mpoa_pt[2] Mpoa_mode[1:0]");
	PR_INFO("     ext(up)  : echo add %s %s %s %s %s > /proc/dp/%s\n",
		"SrcIP 192.168.1.100 DstIP 192.168.0.100",
		"SrcPort 1024 DstPort 1024 ExtId 100",
		"SrcMac 11:11:11:11:11:11 DstMac 11:11:11:11:11:22",
		"NatIP 192.168.0.1   NatPort 3000 MTU 1500",
		"DstPmac 0x8000 subif 0 dir 1",
		PROC_ROUTE);
	PR_INFO("     ext(down): echo add %s %s %s %s %s > /proc/dp/%s\n",
		"SrcIP 192.168.0.100 DstIP 192.168.0.1",
		"SrcPort 1024 DstPort 3000 ExtId 100",
		"SrcMac 11:11:11:11:11:33 DstMac 11:11:11:11:11:44",
		"NatIP 192.168.1.100",
		"NatPort 1024 MTU 1500 DstPmac 0x2 subif 0 dir 0",
		PROC_ROUTE);

	goto exit;
}

int proc_gsw_route_dump(struct seq_file *seq, int pos)
{
	struct core_ops *gsw_handle;
	GSW_ROUTE_Entry_t *rt_entry;
	GSW_return_t  ret = 0;

	rt_entry = kmalloc(sizeof(GSW_ROUTE_Entry_t) + 1,
			   GFP_KERNEL);
	if (!rt_entry)
		return -1;
	/* read gswip-r rmon counter */
	gsw_handle = dp_port_prop[0].ops[1];
	memset(rt_entry, 0, sizeof(*rt_entry));
	rt_entry->nRtIndex = pos;
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pae_ops
			   .ROUTE_SessionEntryRead, gsw_handle, rt_entry);
	if (ret != GSW_statusOk) {
		PR_ERR("GSW_ROUTE_ENTRY_READ returned Failure for index=%d\n",
		       rt_entry->nRtIndex);
		pos = -1;
		kfree(rt_entry);
		return pos;
	}
	if (rt_entry->routeEntry.pattern.bValid != LTQ_TRUE)
		goto EXIT;

	if (dp_route_dump_seq(seq, rt_entry))
		return pos;	/*need report*/

 EXIT:
	pos++;
	kfree(rt_entry);
	if (pos >= 4096) /*GSWIP API does not check the maximum
			  *entry and it will hang
			  */
		pos = -1;
	return pos;
}

#define PMAC_EG_SET(x, y) (pmac.eg.x = dp_atoi(y))
#define PMAC_IG_SET(x, y) (pmac.ig.x = dp_atoi(y))

static int set_pmac_ig_v(char *p, char *tail, GSW_PMAC_Ig_Cfg_t *ig)
{
	char *tmp;
	int k;

	for (k = 0; k < 8; k++) {
		if (k < (8 - 1)) {
			tmp = strstr(p, ":");
			if (!tmp ||
			    ((u32)tmp >= (u32)tail)) {
				PR_INFO("%s: should be like %s\n",
					"Wrong format",
					"xx:xx:xx:xx:xx:xx:xx:xx");
				return -1;
			}
			*tmp = 0; /*replace:with zero*/
		}
		ig->defPmacHdr[k] = dp_atoi(p);

		p = tmp + 1; /* move to next value */
	}
	return 0;
}

static ssize_t proc_gsw_pmac_write(struct file *file, const char *buf,
				   size_t count, loff_t *ppos)
{
	u16 len, i, k, start_param;
	GSW_return_t ret = 0;
	char *str = NULL;
	char *param_list[20 * 2];
	unsigned int num;
	union {
		GSW_PMAC_Eg_Cfg_t eg;
		GSW_PMAC_Ig_Cfg_t ig;
	} pmac;
	#define MAX_GSWIP_CALSS 15
	#define MAX_GSWIP_FLOW  3
	struct core_ops *gsw_handle;
	int class_s = 0, class_e = MAX_GSWIP_CALSS;
	int flow_s = 0, flow_e = MAX_GSWIP_FLOW;

	str = kmalloc(count + 1, GFP_KERNEL);
	if (!str)
		return count;
	len = count;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;

	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if ((num < 2) || (num >= ARRAY_SIZE(param_list))) {
		PR_INFO("parameter %d not enough/more. count=%d\n", num, count);
		goto help;
	}
	if (dp_strncmpi(param_list[0],
			"help", strlen("help")) == 0)	/* help */
		goto help;
	/* set pmac */
	if (dp_strncmpi(param_list[0],
			"set",
			strlen("set")) != 0) {
		PR_INFO("wrong command: %s\n", param_list[0]);
		goto help;
	}
	if (dp_strncmpi(param_list[1], "L", 1) == 0) {
		gsw_handle = dp_port_prop[0].ops[GSWIP_L];
	} else if (dp_strncmpi(param_list[1], "R", 1) == 0) {
		gsw_handle = dp_port_prop[0].ops[GSWIP_R];
	} else {
		PR_INFO("wrong param:should provide L/R\n");
		goto exit;
	}
	memset(&pmac, 0, sizeof(pmac));
	start_param = 3;

	if (dp_strncmpi(param_list[start_param - 1], "EG", 2) == 0) {
		/*ingress pmac */
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops
				   .Pmac_Eg_CfgGet, gsw_handle, &pmac);
		for (i = start_param; i < num; i += 2) {
			if (dp_strncmpi(param_list[i],
					"Class", strlen("Class")) == 0) {
				char *p = param_list[i + 1];

				char *tail = p + strlen(p);
				char *tmp;

				tmp = strstr(p, ":");
				if (!tmp || (tmp >= tail)) {
					PR_INFO("%s: should be like xx:xx\n",
						"Wrong format for Class");
					goto exit;
				}
				*tmp = 0;
				class_s = dp_atoi(p);
				class_e = dp_atoi(tmp + 1);
			} else if (dp_strncmpi(param_list[i],
					     "FlowID",
						 strlen("FlowID"))
						 == 0) {
				char *p = param_list[i + 1];
				char *tail = p + strlen(p);
				char *tmp;

				tmp = strstr(p, ":");
				if (!tmp || (tmp >= tail)) {
					PR_INFO("%s:should be like xx:xx\n",
						"Wrong format for FlowID");
					goto exit;
				}
				*tmp = 0;
				flow_s = dp_atoi(p);
				flow_e = dp_atoi(tmp + 1);
			} else if (dp_strncmpi(param_list[i],
					     "DestPort",
						 strlen("DestPort"))
						 == 0) {
				PMAC_EG_SET(nDestPortId, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					     "RxDmaCH",
						 strlen("RxDmaCH"))
						 == 0) {
				PMAC_EG_SET(nRxDmaChanId, param_list[i + 1]);
			}
#ifdef xxxxx
			/*below global flag cannot be editted here*/
			else if (dp_strncmpi(param_list[i],
					     "MPE1",
						 strlen("MPE1"))
						 == 0)
				PMAC_EG_SET(bMpe1Flag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "MPE2",
						 strlen("MPE2"))
						 == 0)
				PMAC_EG_SET(bMpe2Flag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "DEC",
						 strlen("DEC"))
						 == 0)
				PMAC_EG_SET(bDecFlag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "ENC",
						 strlen("ENC"))
						 == 0)
				PMAC_EG_SET(bEncFlag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "ProcFlag",
						 strlen("ProcFlag"))
						 == 0)
				PMAC_EG_SET(bProcFlagsSelect,
					    param_list[i + 1]);
#endif
			else if (dp_strncmpi(param_list[i],
					     "RemL2Hdr",
						 strlen("RemL2Hdr"))
						 == 0)
				PMAC_EG_SET(bRemL2Hdr, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "RemNum",
						 strlen("RemNum"))
						 == 0)
				PMAC_EG_SET(numBytesRem, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "FCS",
						 strlen("FCS"))
						 == 0)
				PMAC_EG_SET(bFcsEna, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "PmacEna",
						 strlen("PmacEna"))
						 == 0)
				PMAC_EG_SET(bPmacEna, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "TcEnable",
						 strlen("TcEnable"))
						 == 0)
				PMAC_EG_SET(bTCEnable, param_list[i + 1]);
			else {
				PR_INFO("wrong parameter[%d]: %s\n",
					i, param_list[i]);
				goto exit;
			}
		}
		if (class_e > MAX_GSWIP_CALSS)
			class_e = MAX_GSWIP_CALSS;
		if (flow_e > MAX_GSWIP_FLOW)
			flow_e = MAX_GSWIP_FLOW;
		if (class_s > class_e) {
			PR_INFO("wrong param:class_s=%d should < class_e=%d\n",
				class_s, class_e);
			goto exit;
		}
		if (flow_s > flow_e) {
			PR_INFO("wrong param:flow_s=%d should < flow_e=%d\n",
				flow_s, flow_e);
			goto exit;
		}
		PR_INFO("Set EG PMAC for class %d:%d flow %d:%d\n",
			class_s, class_e, flow_s, flow_e);
		for (i = class_s; i <= class_e; i++) {
			for (k = flow_s; k <= flow_e; k++) {
				pmac.eg.nTrafficClass = i;
				/*Note: bProcFlagsSelect zero,
				 *just nTrafficClass,
				 *else use MPE1/2/ENC/DEC flag instead
				 */
				pmac.eg.bMpe1Flag = (pmac.eg.nTrafficClass >>
					0) & 1;
				pmac.eg.bMpe2Flag = (pmac.eg.nTrafficClass >>
					1) & 1;
				pmac.eg.bEncFlag = (pmac.eg.nTrafficClass >>
					2) & 1;
				pmac.eg.bDecFlag = (pmac.eg.nTrafficClass >>
					3) & 1;
				pmac.eg.nFlowIDMsb = k;
				ret = gsw_core_api((dp_gsw_cb)gsw_handle->
						   gsw_pmac_ops.Pmac_Eg_CfgSet,
						   gsw_handle, &pmac);
			}
		}
		if (ret < GSW_statusOk) {
			PR_ERR("GSW_PMAC_EG_CFG_SET returned failure\n");
			goto exit;
		}
	} else if (dp_strncmpi(param_list[start_param - 1], "IG", 2) == 0) {
		/*ingress pmac1 */
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops
				   .Pmac_Ig_CfgGet, gsw_handle, &pmac);
		for (i = start_param; i < num; i += 2) {
			if (dp_strncmpi(param_list[i],
					"TxDmaCH",
					strlen("TxDmaCH"))
					== 0) {
				PMAC_IG_SET(nTxDmaChanId, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"ErrDrop",
						strlen("ErrDrop"))
						== 0) {
				PMAC_IG_SET(bErrPktsDisc, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"ClassEna",
						strlen("ClassEna"))
						== 0) {
				PMAC_IG_SET(bClassEna, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"ClassDefault",
						strlen("ClassDefault"))
						== 0) {
				PMAC_IG_SET(bClassDefault, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"PmacEna",
						strlen("PmacEna"))
						== 0) {
				PMAC_IG_SET(bPmapEna, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"PmacDefault",
						strlen("PmacDefault"))
						== 0) {
				PMAC_IG_SET(bPmapDefault, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"SubIdDefault",
						strlen("SubIdDefault"))
						== 0) {
				 /*changed from bSubIdDefault in GSWIP3.1 */
				//PMAC_IG_SET(bSubIdDefault, param_list[i + 1]);
				PMAC_IG_SET(eSubId, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"SpIdDefault",
						strlen("SpIdDefault"))
						== 0) {
				PMAC_IG_SET(bSpIdDefault, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"PmacPresent",
						strlen("PmacPresent"))
						== 0) {
				PMAC_IG_SET(bPmacPresent, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
						"DefaultPmacHdr",
						strlen("DefaultPmacHdr"))
						== 0) {
				char *p = param_list[i + 1];
				char *tail = p + strlen(p);

				if (set_pmac_ig_v(p, tail, &pmac.ig))
					goto exit;
			} else {
				PR_INFO("wrong parameter[%d]: %s\n", i,
					param_list[i]);
				goto exit;
			}
		}
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops
				   .Pmac_Ig_CfgSet, gsw_handle, &pmac);
		if (ret < GSW_statusOk) {
			PR_ERR("GSW_PMAC_IG_CFG_SET returned failure\n");
			goto exit;
		}
	} else if (dp_strncmpi(param_list[start_param - 1],
				"reset",
				strlen("reset")) == 0) {
		GSW_reset_t reset;

		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.Reset,
			     gsw_handle, &reset);
	} else {
		PR_INFO("wrong parameter not supported: %s\n",
			param_list[start_param - 1]);
		goto exit;
	}
exit:
	kfree(str);
	return count;

help:
	PR_INFO("usage:\n");
	PR_INFO("  echo set <L/R> EG\n");
	PR_INFO("    [DestPort] [Dst-PMAC-Port-value]\n");
	PR_INFO("    [Class] [Class-start:end](0~15)\n");
	PR_INFO("    [FlowID] [FlowID-start:end](0~3)\n");
	PR_INFO("\n");
	PR_INFO("    [PmacEna] [Enable PMAC HDR (1) or not(0)]\n");
	PR_INFO("    [RxDmaCH] [RxDmaCH-value]\n");
	PR_INFO("    [TcEnable] [TcEnable-value(0/1)] [FCS] [FCS-value(0/1]\n");
	PR_INFO("    %s [RemL2Hdr-value(0/1)] [RemNum] [RemNum-value]\n",
		"[RemL2Hdr]");
	PR_INFO("     > /prooc/dp/%s\n", PROC_PMAC);
	PR_INFO("  echo set <L/R> IG [TxDmaCH] [TX-DMA-CH-value]\n");
	PR_INFO("\n");
	PR_INFO("    [ErrDrop] [Error-Drop-value(0/1)]\n");
	PR_INFO("    %s PMAC header(1) or incoming PMAC header(0)]\n",
		"[ClassEna] [Class Enable info from default");
	PR_INFO("    %s %s %s\n",
		"[ClassDefault]",
		"[Class Default info from default PMAC header(1)",
		"or incoming PMAC header(0)]");
	PR_INFO("    %s %s %s\n",
		"[PmacEna]",
		"[Port Map Enable info from default PMAC header(1)",
		"or incoming PMAC header(0)]");
	PR_INFO("    %s %s or incoming PMAC header(0)]\n",
		"[PmacDefault]",
		"[Port Map info from default PMAC header(1)");
	PR_INFO("    %s %s or in packet descriptor (0)]\n",
		"[SubIdDefault]",
		"[Sub_Interface Id Info from default PMAC header(1)");
	PR_INFO("    %s %s or incoming PMAC header (False)]\n",
		"[SpIdDefault]",
		"[Source port id from default PMAC header(1)");
	PR_INFO("    %s %s or not (0)]\n",
		"[PmacPresent]",
		"[Packet PMAC header is present (1)");
	PR_INFO("    %s [Default PMAC HDR(8 bytes: xx:xx:xx:xx:xx:xx:xx:xx]\n",
		"[DefaultPmacHdr]");
	PR_INFO("     > /prooc/dp/%s\n", PROC_PMAC);
	PR_INFO("  echo set <0/1> reset\n");
	PR_INFO("  ext1: echo %s %s %s %s > /proc/dp/pmac\n",
		"set R IG TxDmaCH 1 ErrDrop 0 PmacDefault 0 PmacEna 0",
		"ClassEna 1 ClassDefault 1 SubIdDefault 1 SpIdDefault 1",
		"PmacPresent 0",
		"DefaultPmacHdr 0x11:0x22:0x33:0x44:0x55:0x66:0x77:0x88");
	PR_INFO("  ext2: %s %s > /proc/dp/pmac\n",
		"echo set R EG DestPort 15 class 0:15 FlowID 0:3",
		"RxDmaCH 4 TcEnable 1 RemL2Hdr 1 RemNum 8 PmacEna 0 FCS 1");
	goto exit;
}

static int proc_gsw_pmac_start(void)
{
	return 0;
}

static int proc_gsw_pmac_dump(struct seq_file *s, int pos)
{
	GSW_PMAC_Ig_Cfg_t igCfg;
	GSW_PMAC_Eg_Cfg_t egCfg;
	struct core_ops *gsw_handle;
	u8 i = 0, j = 0;

	/* Do the GSW-L configuration */
	gsw_handle = dp_port_prop[0].ops[0];
	seq_puts(s, "\nGSWIP PMAC0 Ingress PMAC Configure\n");
	seq_printf(s, "%15s %15s %15s %15s %15s %15s %15s %15s %15s %15s\n",
		   "nTxDmaChanId", "bErrPktsDisc", "bPmapDefault", "bPmapEna",
		   "bClassDefault", "bClassEna", "bSubIdDefault",
		   "bSpIdDefault", "bPmacPresent", "defPmacHdr");
	for (i = 0; i <= 15; i++) {
		memset(&igCfg, 0x00, sizeof(igCfg));
		igCfg.nPmacId = 0;
		igCfg.nTxDmaChanId = i;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops.Pmac_Ig_CfgGet,
			     gsw_handle, &igCfg);
		seq_printf(s, "%15d %15d %15d %15d %15d %15d %15d %15d %15d ",
			   igCfg.nTxDmaChanId, igCfg.bErrPktsDisc,
			   igCfg.bPmapDefault, igCfg.bPmapEna,
			   igCfg.bClassDefault, igCfg.bClassEna,
			   igCfg.eSubId, igCfg.bSpIdDefault,
			   igCfg.bPmacPresent);
		for (j = 0; j < 8; j++)
			seq_printf(s, "%02x", igCfg.defPmacHdr[j]);
		seq_puts(s, "\n");
	}

	seq_puts(s, "GSWIP PMAC0 Egress PMAC Configure\n");
	for (i = 0; i <= 3; i++) {
		memset(&egCfg, 0x00, sizeof(egCfg));
		egCfg.nPmacId = 0;
		egCfg.nDestPortId	= i;
		egCfg.nTrafficClass	= 0;
		egCfg.nFlowIDMsb	= 0;
		egCfg.bDecFlag		= 0;
		egCfg.bEncFlag		= 0;
		egCfg.bMpe1Flag		= 0;
		egCfg.bMpe2Flag		= 0;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops.Pmac_Eg_CfgGet,
			     gsw_handle, &egCfg);
		seq_printf(s, " nRxDmaChanId  = %d\n", egCfg.nRxDmaChanId);
		seq_printf(s, " bRemL2Hdr     = %d\n", egCfg.bRemL2Hdr);
		seq_printf(s, " numBytesRem   = %d\n", egCfg.numBytesRem);
		seq_printf(s, " bFcsEna       = %d\n", egCfg.bFcsEna);
		seq_printf(s, " bPmacEna      = %d\n", egCfg.bPmacEna);
		seq_printf(s, " nResDW1       = %d\n", egCfg.nResDW1);
		seq_printf(s, " nRes1DW0      = %d\n", egCfg.nRes1DW0);
		seq_printf(s, " nRes2DW0      = %d\n", egCfg.nRes2DW0);
		seq_printf(s, " nDestPortId   = %d\n", egCfg.nDestPortId);
		seq_printf(s, " nTrafficClass = %d\n", egCfg.nTrafficClass);
		seq_printf(s, " nFlowIDMsb    = %d\n", egCfg.nFlowIDMsb);
		seq_printf(s, " bDecFlag      = %d\n", egCfg.bDecFlag);
		seq_printf(s, " bEncFlag      = %d\n", egCfg.bEncFlag);
		seq_printf(s, " bMpe1Flag     = %d\n", egCfg.bMpe1Flag);
		seq_printf(s, " bMpe2Flag     = %d\n", egCfg.bMpe2Flag);
		seq_puts(s, "\n");
	}

	seq_puts(s, "\n\nGSWIP PMAC Ingress PMAC Configure\n");
		seq_printf(s, "\n%15s %15s %15s %15s %15s %15s %15s %15s %15s %15s\n",
			   "nTxDmaChanId", "bErrPktsDisc", "bPmapDefault",
			   "bPmapEna", "bClassDefault", "bClassEna",
			   "bSubIdDefault", "bSpIdDefault", "bPmacPresent",
			   "defPmacHdr");
	for (i = 0; i <= 15; i++) {
		memset(&igCfg, 0x00, sizeof(igCfg));
		igCfg.nPmacId = 1;
		igCfg.nTxDmaChanId = i;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops.Pmac_Ig_CfgGet,
			     gsw_handle, &igCfg);
		seq_printf(s, "%15d %15d %15d %15d %15d %15d %15d %15d %15d ",
			   igCfg.nTxDmaChanId, igCfg.bErrPktsDisc,
			   igCfg.bPmapDefault, igCfg.bPmapEna,
			   igCfg.bClassDefault, igCfg.bClassEna,
			   igCfg.eSubId, igCfg.bSpIdDefault,
			   igCfg.bPmacPresent);
		for (j = 0; j < 8; j++)
			seq_printf(s, "%02x", igCfg.defPmacHdr[j]);
		seq_puts(s, "\n");
	}
	seq_puts(s, "\n\n\nGSWIP-R Egress PMAC Configure\n");
	for (i = 0; i <= 15; i++) {
		memset(&egCfg, 0x00, sizeof(egCfg));
		egCfg.nPmacId = 1;
		egCfg.nDestPortId	= i;
		egCfg.nTrafficClass	= 0;
		egCfg.nFlowIDMsb	= 0;
		egCfg.bDecFlag		= 0;
		egCfg.bEncFlag		= 0;
		egCfg.bMpe1Flag		= 0;
		egCfg.bMpe2Flag		= 0;

		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops.Pmac_Eg_CfgGet,
			     gsw_handle, &egCfg);

		seq_printf(s, " nRxDmaChanId  = %d\n", egCfg.nRxDmaChanId);
		seq_printf(s, " bRemL2Hdr     = %d\n", egCfg.bRemL2Hdr);
		seq_printf(s, " numBytesRem   = %d\n", egCfg.numBytesRem);
		seq_printf(s, " bFcsEna       = %d\n", egCfg.bFcsEna);
		seq_printf(s, " bPmacEna      = %d\n", egCfg.bPmacEna);
		seq_printf(s, " nResDW1       = %d\n", egCfg.nResDW1);
		seq_printf(s, " nRes1DW0      = %d\n", egCfg.nRes1DW0);
		seq_printf(s, " nRes2DW0      = %d\n", egCfg.nRes2DW0);
		seq_printf(s, " nDestPortId   = %d\n", egCfg.nDestPortId);
		seq_printf(s, " nTrafficClass = %d\n", egCfg.nTrafficClass);
		seq_printf(s, " nFlowIDMsb    = %d\n", egCfg.nFlowIDMsb);
		seq_printf(s, " bDecFlag      = %d\n", egCfg.bDecFlag);
		seq_printf(s, " bEncFlag      = %d\n", egCfg.bEncFlag);
		seq_printf(s, " bMpe1Flag     = %d\n", egCfg.bMpe1Flag);
		seq_printf(s, " bMpe2Flag     = %d\n", egCfg.bMpe2Flag);
		seq_puts(s, "\n");
	}
	if (!seq_has_overflowed(s))
		pos++;
	if (pos == 1)
		return -1;

	return pos;
}

int proc_ep_dump(struct seq_file *s, int pos)
{
#if defined(NEW_CBM_API) && NEW_CBM_API
	u32 num;
	cbm_tmu_res_t *res = NULL;
	u32 flag = 0;
	int i;
	struct pmac_port_info *port = get_port_info(0, pos);

	if (cbm_dp_port_resources_get
	    (&pos, &num, &res, port ? port->alloc_flags : flag) == 0) {
		for (i = 0; i < num; i++) {
			seq_printf(s, "ep=%d tmu_port=%d queue=%d sid=%d\n",
				   pos, res[i].tmu_port, res[i].tmu_q,
				   res[i].tmu_sched);
		}

		kfree(res);
	}
#endif
	pos++;

	if (pos >= PMAC_MAX_NUM)
		pos = -1;	/*end of the loop */

	return pos;
}

static void pmac_eg_cfg(char *param_list[], int num, dp_pmac_cfg_t *pmac_cfg)
{
	int i, j;
	u32 value;

	for (i = 2; i < num; i += 2) {
		for (j = 0; j < ARRAY_SIZE(egress_entries); j++) {
			if (dp_strncmpi(param_list[i],
					egress_entries[j].name,
					strlen(egress_entries[j].name)))
				continue;
			if (dp_strncmpi(egress_entries[j].name,
					"rm_l2hdr",
					strlen("rm_l2hdr")) == 0) {
				if (dp_atoi(param_list[i + 1]) > 0) {
					pmac_cfg->eg_pmac.rm_l2hdr = 1;
					value = dp_atoi(param_list[i + 1]);
					egress_entries[j].
					   egress_callback(pmac_cfg,
							   value);
					PR_INFO("egress pmac ep %s config ok\n",
						egress_entries
					     [j].name);
					break;
				}
				pmac_cfg->eg_pmac.rm_l2hdr =
				    dp_atoi(param_list[i + 1]);
			} else {
				value = dp_atoi(param_list[i + 1]);
				egress_entries[j].
				    egress_callback(pmac_cfg,
						    value);
				PR_INFO("egress pmac ep %s configu ok\n",
					egress_entries[j].name);
				break;
			}
		}
	}
}

ssize_t ep_port_write(struct file *file, const char *buf, size_t count,
		      loff_t *ppos)
{
	int len;
	char str[64];
	int num, i, j, ret;
	u32 value;
	u32 port;
	char *param_list[10];
	dp_pmac_cfg_t pmac_cfg;
	int inst = 0;

	memset(&pmac_cfg, 0, sizeof(dp_pmac_cfg_t));
	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (dp_strncmpi(param_list[0], "ingress", strlen("ingress")) == 0) {
		port = dp_atoi(param_list[1]);

		for (i = 2; i < num; i += 2) {
			for (j = 0; j < ARRAY_SIZE(ingress_entries); j++) {
				if (dp_strncmpi(param_list[i],
						ingress_entries[j].name,
						strlen(ingress_entries[j].name))
						== 0) {
					value = dp_atoi(param_list[i + 1]);
					ingress_entries[j].
					    ingress_callback(&pmac_cfg,
							     value);
					PR_INFO("ingress pmac ep %s configed\n",
						ingress_entries[j].name);
					break;
				}
			}
		}

		ret = dp_pmac_set_30(inst, port, &pmac_cfg);

		if (ret != 0) {
			PR_ERR("pmac set configuration failed\n");
			return -1;
		}
	} else if (dp_strncmpi(param_list[0], "egress", 6) == 0) {
		port = dp_atoi(param_list[1]);

		pmac_eg_cfg(param_list, num, &pmac_cfg);
		ret = dp_pmac_set_30(inst, port, &pmac_cfg);

		if (ret != 0) {
			PR_ERR("pmac set configuration failed\n");
			return -1;
		}
	} else {
		PR_INFO("wrong command\n");
		goto help;
	}

	return count;
help:
	PR_INFO("echo ingress/egress [ep_port] %s > /proc/dp/ep\n",
		"['ingress/egress fields'] [value]");
	PR_INFO("(eg) echo ingress 1 pmac 1 > /proc/dp/ep\n");
	PR_INFO("(eg) echo egress 1 rm_l2hdr 2 > /proc/dp/ep\n");
	PR_INFO("echo %s ['errdisc/pmac/pmac_pmap/pmac_en_pmap/pmac_tc",
		"ingress [ep_port] ");
	PR_INFO("                         %s > /proc/dp/ep\n",
		"/pmac_en_tc/pmac_subifid/pmac_srcport'] [value]");
	PR_INFO("echo  %s %s> /proc/dp/ep\n",
		"egress [ep_port]",
		"['rx_dmachan/fcs/pmac/res_dw1/res1_dw0/res2_dw0] [value]");
	PR_INFO("echo egress [ep_port] ['rm_l2hdr'] [value] > /proc/dp/ep\n");
	return count;
}

static struct dp_proc_entry dp_proc_entries[] = {
	/*name single_callback_t multi_callback_t/_start write_callback_t */
	{PROC_PARSER, proc_parser_read, NULL, NULL, proc_parser_write},
	{PROC_RMON_PORTS, NULL, proc_gsw_port_rmon_dump,
	 proc_gsw_rmon_port_start, proc_gsw_rmon_write},
	{PROC_CHECKSUM, proc_checksum_read, NULL, NULL,
		proc_checksum_write},
#ifdef CONFIG_LTQ_DATAPATH_MIB
	{PROC_MIB_TIMER, proc_mib_timer_read, NULL, NULL,
		proc_mib_timer_write},
	{PROC_MIB_INSIDE, NULL, proc_mib_inside_dump,
		proc_mib_inside_start, proc_mib_inside_write},
	{PROC_MIBPORT, NULL, proc_mib_port_dump,
		proc_mib_port_start, proc_mib_port_write},
	{PROC_MIBVAP, NULL, proc_mib_vap_dump, proc_mib_vap_start,
		proc_mib_vap_write},
#endif
#ifdef CONFIG_LTQ_DATAPATH_CPUFREQ
	{PROC_COC, proc_coc_read, NULL, NULL, proc_coc_write},
#endif
	{PROC_CBM_BUF_TEST, proc_cbm_buf_read, NULL, NULL,
	 proc_cbm_buf_write},
	{PROC_EP, NULL, proc_ep_dump, NULL, ep_port_write},
	{PROC_PCE, NULL, proc_gsw_pce_dump, proc_gsw_pce_start, NULL},
	{PROC_ROUTE, NULL, proc_gsw_route_dump, NULL, proc_gsw_route_write},
	{PROC_DPORT, NULL, proc_dport_dump, NULL, NULL},
	{PROC_PMAC, NULL, proc_gsw_pmac_dump, proc_gsw_pmac_start,
	 proc_gsw_pmac_write},
	{DP_PROC_CBMLOOKUP, NULL, lookup_dump30, lookup_start30,
	 proc_get_qid_via_index30},
	/*the last place holder */
	{NULL, NULL, NULL, NULL, NULL}
};

int dp_sub_proc_install_30(void)
{
	int i;

	if (!dp_proc_node) {
		PR_INFO("dp_sub_proc_install failed\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(dp_proc_entries); i++)
		dp_proc_entry_create(dp_proc_node, &dp_proc_entries[i]);
	PR_INFO("dp_sub_proc_install ok\n");
	return 0;
}

