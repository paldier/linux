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

#define DP_PROC_FILE_GSWIP_BP "bp"
#define DP_PROC_FILE_SWDEV_BR "brctl"
#define DP_PROC_SWDEV_FDB "fdb"
#define DP_PROC_SWDEV_MAC "sw_mac"
#define PROC_PARSER "parser"
#define PROC_RMON_PORTS  "rmon"
#define PROC_MIB_TIMER "mib_timer"
#define PROC_MIB_INSIDE "mib_inside"
#define PROC_MIBPORT "mib_port"
#define PROC_MIBVAP "mib_vap"
#define PROC_COMMON_CMD "cmd"
#define PROC_COC "coc"
#define PROC_PCE  "pce"
#define PROC_PMAC  "pmac"
#define PROC_EP "ep" /*EP/port ID info */
#define DP_PROC_CBMLOOKUP "lookup"

struct list_head fdb_tbl_list;

static int proc_gsw_bp_dump(struct seq_file *s, int pos);
static int proc_common_start(void);
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
static int proc_swdev_brctl_dump(struct seq_file *s, int pos);
static int proc_swdev_brctl_start(void);
static ssize_t proc_swdev_brctl_write(struct file *,
				      const char *, size_t, loff_t *);
static int print_bridge(int fid, int inst);
static ssize_t proc_swdev_fdb_write(struct file *,
				    const char *, size_t, loff_t *);
static void proc_swdev_fdb_read(struct seq_file *s);
#endif
static int proc_gsw_pce_dump(struct seq_file *s, int pos);
static int proc_gsw_pce_start(void);

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
void print_dash_line(struct seq_file *s)
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

static void proc_parser_read(struct seq_file *s);
static ssize_t proc_parser_write(struct file *, const char *, size_t,
				 loff_t *);
static int proc_gsw_pce_dump(struct seq_file *s, int pos);
static int proc_gsw_pce_start(void);
static ssize_t proc_gsw_pmac_write(struct file *file, const char *buf,
				   size_t count, loff_t *ppos);
static int rmon_display_port_full;

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
	GSW_return_t ret = 0;
	struct core_ops *gsw_handle;
	//char flag_buf[20];

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
	} else {
		goto NEXT;
	}
 NEXT:
	pos++;

	if (pos >= RMON_MAX)
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

	if (dp_strncmpi(param_list[0], "clear", 5) == 0 ||
	    dp_strncmpi(param_list[0], "c", 1) == 0 ||
	    dp_strncmpi(param_list[0], "rest", 4) == 0 ||
	    dp_strncmpi(param_list[0], "r", 1) == 0) {
		dp_sys_mib_reset_31(0);
		goto EXIT_OK;
	}
	if (dp_strncmpi(param_list[0], "RMON", 4) == 0) {
		if (dp_strncmpi(param_list[1], "Full", 4) == 0) {
			rmon_display_port_full = 1;
			goto EXIT_OK;
		} else if (dp_strncmpi(param_list[1], "Basic", 5) == 0) {
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
	PR_INFO("usage: echo RMON Full > /proc/dp/rmon\n");
	PR_INFO("usage: echo RMON Basic > /proc/dp/rmon\n");
	return count;
}

/*in bytes*/
int get_q_qocc(int inst, int qid, u32 *c)
{
	if (c)
		*c = readl((void *)(0xb8820E00 + qid * 4));
	return 0;
}

/*in packet
 *total accept(green/yellow ?)
 */
int get_q_mib(int inst, int qid,
	      u32 *total_accept,
	      u32 *total_drop,
	      u32 *red_drop)
{
	/*indirect access */
	while (readl((void *)(0xb88200c4)) & 1)/*busy and wait*/
		;
	writel(qid, (void *)0xb88200bc);

	while (readl((void *)(0xb88200c4)) & 1) /*busy wait*/
		;
	writel(0, (void *)0xb88200c0);

	while (readl((void *)(0xb88200c4)) & 1) /*busy wait*/
		;
	if (total_accept)
		*total_accept = readl((void *)(0xb88200ec));
	if (total_drop)
		*total_drop = readl((void *)(0xb88200ec + 4));
	if (red_drop)
		*red_drop = readl((void *)(0xb88200ec + 8));
	return 0;
}

/*in bytes*/
int get_p_mib(int inst, int pid,
	      u32 *green /*green bytes*/,
	      u32 *yellow/*yellow bytes*/)
{
	if (green)
		*green = readl((void *)0xb8820400 + pid * 4);
	if (yellow)
		*yellow = readl((void *)0xb8820200 + pid * 4);
	return 0;
}

#define PRINT_Q_MIB(i, c, x, y, z)\
	PR_INFO("Q[%03d]:0x%08x 0x%08x 0x%08x 0x%08x\n",\
		 i, c, x, y, z)\

static int proc_gsw_pce_dump(struct seq_file *s, int pos)
{
	struct core_ops *gsw_handle;
	GSW_PCE_rule_t *rule;
	int ret = 0, i;

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

int proc_gsw_pce_start(void)
{
	return 0;
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
				PR_INFO("%s:%s %s\n",
					"Wrong format",
					"should be like",
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
	int id;

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
	if (dp_strncmpi(param_list[0], "set", strlen("set")) != 0) {
		PR_INFO("wrong command: %s\n", param_list[0]);
		goto help;
	}
	if (dp_strncmpi(param_list[1], "0", 1) == 0) {
		id = 0;
	} else if (dp_strncmpi(param_list[1], "1", 1) == 0) {
		id = 1;
	} else {
		PR_INFO("wrong param:should provide L/R\n");
		goto exit;
	}
	gsw_handle = dp_port_prop[0].ops[0];
	memset(&pmac, 0, sizeof(pmac));
	start_param = 3;

	if (dp_strncmpi(param_list[start_param - 1], "EG", 2) == 0) {
		/*egress pmac */
		pmac.eg.nPmacId = id;
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops
				   .Pmac_Eg_CfgGet, gsw_handle, &pmac);
		for (i = start_param; i < num; i += 2) {
			if (dp_strncmpi(param_list[i],
					"Class",
					strlen("Class")) == 0) {
				char *p = param_list[i + 1];
				char *tail = p + strlen(p);
				char *tmp;

				tmp = strstr(p, ":");
				if (!tmp || (tmp >= tail)) {
					PR_INFO("%s:it should be like xx:xx\n",
						"Wrong format for Class");
					goto exit;
				}
				*tmp = 0;
				class_s = dp_atoi(p);
				class_e = dp_atoi(tmp + 1);
			} else if (dp_strncmpi(param_list[i],
					"FlowID",
					strlen("FlowID")) == 0) {
				char *p = param_list[i + 1];
				char *tail = p + strlen(p);
				char *tmp;

				tmp = strstr(p, ":");
				if (!tmp || (tmp >= tail)) {
					PR_INFO("%s: it should be like xx:xx\n",
						"Wrong format for FlowID");
					goto exit;
				}
				*tmp = 0;
				flow_s = dp_atoi(p);
				flow_e = dp_atoi(tmp + 1);
			} else if (dp_strncmpi(param_list[i],
					"DestPort",
					strlen("DestPort")) == 0) {
				PMAC_EG_SET(nDestPortId, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					"RxDmaCH",
					strlen("RxDmaCH")) == 0) {
				PMAC_EG_SET(nRxDmaChanId, param_list[i + 1]);
			}
#ifdef xxxxx
			/*below global flag cannot be editted here*/
			else if (dp_strncmpi(param_list[i],
					     "MPE1",
						 strlen("MPE1")) == 0)
				PMAC_EG_SET(bMpe1Flag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "MPE2",
						 strlen("MPE2")) == 0)
				PMAC_EG_SET(bMpe2Flag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "DEC",
						 strlen("DEC")) == 0)
				PMAC_EG_SET(bDecFlag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "ENC",
						 strlen("ENC")) == 0)
				PMAC_EG_SET(bEncFlag, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "ProcFlag",
						 strlen("ProcFlag")) == 0)
				PMAC_EG_SET(bProcFlagsSelect,
					    param_list[i + 1]);
#endif
			else if (dp_strncmpi(param_list[i],
					     "RemL2Hdr",
						 strlen("RemL2Hdr")) == 0)
				PMAC_EG_SET(bRemL2Hdr, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "RemNum",
						 strlen("RemNum")) == 0)
				PMAC_EG_SET(numBytesRem, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "FCS",
						 strlen("FCS")) == 0)
				PMAC_EG_SET(bFcsEna, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "PmacEna",
						 strlen("PmacEna")) == 0)
				PMAC_EG_SET(bPmacEna, param_list[i + 1]);
			else if (dp_strncmpi(param_list[i],
					     "TcEnable",
						 strlen("TcEnable")) == 0)
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
				pmac.eg.nFlowIDMsb = k;
				ret = gsw_core_api(DP_PMAC_OPS(gsw_handle,
							       Pmac_Eg_CfgGet),
						   gsw_handle, &pmac);
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
				ret = gsw_core_api(DP_PMAC_OPS(gsw_handle,
							       Pmac_Eg_CfgSet),
						   gsw_handle, &pmac);
			}
		}
		if (ret < GSW_statusOk) {
			PR_INFO("GSW_PMAC_EG_CFG_SET returned failure\n");
			goto exit;
		}
	} else if (dp_strncmpi(param_list[start_param - 1], "IG", 2) == 0) {
		/*ingress pmac1 */
		pmac.ig.nPmacId = id;
		ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_pmac_ops
				   .Pmac_Ig_CfgGet, gsw_handle, &pmac);
		for (i = start_param; i < num; i += 2) {
			if (dp_strncmpi(param_list[i],
					"TxDmaCH",
					strlen("TxDmaCH")) == 0) {
				PMAC_IG_SET(nTxDmaChanId, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					"ErrDrop",
					strlen("ErrDrop")) == 0) {
				PMAC_IG_SET(bErrPktsDisc, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					"ClassEna",
					strlen("ClassEna")) == 0) {
				PMAC_IG_SET(bClassEna, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					"ClassDefault",
					strlen("ClassDefault")) == 0) {
				PMAC_IG_SET(bClassDefault, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					"PmacEna",
					strlen("PmacEna")) == 0) {
				PMAC_IG_SET(bPmapEna, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					"PmacDefault",
					strlen("PmacEna")) == 0) {
				PMAC_IG_SET(bPmapDefault, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					"SubIdDefault",
					strlen("SubIdDefault")) == 0) {
				 /*changed from bSubIdDefault in GSWIP3.1 */
				//PMAC_IG_SET(bSubIdDefault, param_list[i + 1]);
				PMAC_IG_SET(eSubId, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					      "SpIdDefault",
						  strlen("SpIdDefault")) == 0) {
				PMAC_IG_SET(bSpIdDefault, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
					      "PmacPresent",
						  strlen("PmacPresent")) == 0) {
				PMAC_IG_SET(bPmacPresent, param_list[i + 1]);
			} else if (dp_strncmpi(param_list[i],
				"DefaultPmacHdr",
				strlen("DefaultPmacHdr")) == 0) {
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
			   "reset", strlen("reset")) == 0) {
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
	PR_INFO("  echo set <0/1> EG\n");
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
	PR_INFO("  echo set <0/1> IG [TxDmaCH] [TX-DMA-CH-value]\n");
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
	gsw_handle = dp_port_prop[0].ops[GSWIP_L];
	seq_puts(s, "\nGSWIP-L Ingress PMAC Configure\n");
	seq_printf(s, "%15s %15s %15s %15s %15s %15s %15s %15s %15s %15s\n",
		   "nTxDmaChanId", "bErrPktsDisc", "bPmapDefault", "bPmapEna",
		   "bClassDefault", "bClassEna", "bSubIdDefault",
		   "bSpIdDefault", "bPmacPresent", "defPmacHdr");
	for (i = 0; i <= 15; i++)  {
		memset(&igCfg, 0x00, sizeof(igCfg));
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
			seq_printf(s, "%02x\n", igCfg.defPmacHdr[j]);
		seq_puts(s, "\n");
	}

	seq_puts(s, "GSWIP-L Egress PMAC Configure\n");
	for (i = 0; i <= 3; i++) {
		memset(&egCfg, 0x00, sizeof(egCfg));

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

	gsw_handle = dp_port_prop[0].ops[GSWIP_R];
	seq_puts(s, "\n\n\nGSWIP-R Ingress PMAC Configure\n");
		seq_printf(s, "\n%15s %15s %15s %15s %15s %15s %15s %15s %15s %15s\n",
			   "nTxDmaChanId", "bErrPktsDisc", "bPmapDefault",
			   "bPmapEna", "bClassDefault", "bClassEna",
			   "bSubIdDefault", "bSpIdDefault", "bPmacPresent",
			   "defPmacHdr");
	for (i = 0; i <= 15; i++) {
		memset(&igCfg, 0x00, sizeof(igCfg));
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

int proc_common_start(void)
{
	return 0;
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

static int proc_gsw_bp_dump(struct seq_file *s, int pos)
{
	pos = -1;
	return pos;
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

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
static u32 br_hash_index;
static struct br_info *brdev_info;
static int proc_swdev_brctl_dump(struct seq_file *s, int pos)
{
	struct bridge_member_port *tmp = NULL;

	while (!brdev_info) {
		br_hash_index++;
		pos = 0;
		if (br_hash_index == BR_ID_ENTRY_HASH_TABLE_SIZE) {
			seq_puts(s, "end\n");
			return -1;
		}
		brdev_info = hlist_entry_safe
			     ((&g_bridge_id_entry_hash_table
			      [0][br_hash_index])->first,
			      struct br_info, br_hlist);
	}
	seq_printf(s, "Hash=%u pos=%d dev=%s inst= fid=%d\n",
		   br_hash_index,
		   pos,
		   brdev_info->br_device_name ? brdev_info->br_device_name
		   : NULL,
		   brdev_info->fid);
	seq_puts(s, "  bp_list=");
	list_for_each_entry(tmp, &brdev_info->bp_list, list) {
		seq_printf(s, "%u ", tmp->portid);
	}
	seq_puts(s, "\n");
	brdev_info = hlist_entry_safe((brdev_info)->br_hlist.next,
				      struct br_info, br_hlist);
	if (!seq_has_overflowed(s))
		pos++;
	return pos;
}

static int proc_swdev_brctl_start(void)
{
	br_hash_index = 0;

	brdev_info = hlist_entry_safe((
			&g_bridge_id_entry_hash_table[0][br_hash_index])->first,
				struct br_info, br_hlist);
	return 0;
}

static int print_bridge(int fid, int inst)
{
	int ret;
	GSW_BRIDGE_config_t brcfg;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[0];
	memset(&brcfg, 0, sizeof(brcfg));
	brcfg.nBridgeId = fid;
	brcfg.eMask = GSW_BRIDGE_CONFIG_MASK_FORWARDING_MODE;
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_brdg_ops
			   .Bridge_ConfigGet, gsw_handle, &brcfg);
	if (ret != GSW_statusOk) {
		PR_ERR("Failed to get bridge id(%d)\n", brcfg.nBridgeId);
		return -1;
	}
	PR_INFO("eForwardBroadcast=:%d\r\n", brcfg.eForwardBroadcast);
	PR_INFO("eForwardUnknownMulticastNonIp=:%d\r\n",
		brcfg.eForwardUnknownMulticastNonIp);
	PR_INFO("eForwardUnknownUnicast=:%d\r\n", brcfg.eForwardUnknownUnicast);
	return 0;
}

static ssize_t proc_swdev_brctl_write(struct file *file,
				      const char *buf,
				      size_t count,
				      loff_t *ppos)
{
	int len;
	char str[64];
	int num, ret, k;
	char *param_list[10];
	struct br_info *br_info = NULL;
	struct bridge_member_port *temp_list = NULL;
	GSW_BRIDGE_portConfig_t brportcfg;
	unsigned char *buf1;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[0].ops[0];
	memset(&brportcfg, 0, sizeof(GSW_BRIDGE_portConfig_t));
	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if ((num != 2) ||
	    (dp_strncmpi(param_list[0], "help", strlen("help")) == 0))
		goto HELP;

	buf1 = kmalloc(MAX_BP_NUM + 1, GFP_KERNEL);
	if (!buf1)
		return -1;
	buf1[0] = 0;

	if (dp_strncmpi(param_list[0], "brctl", strlen("brctl")) == 0) {
		br_info = dp_swdev_bridge_entry_lookup(param_list[1],
						       0);
		if (br_info) {
			print_bridge(br_info->fid, br_info->inst);
			list_for_each_entry(temp_list,  &br_info->bp_list,
					    list) {
				PR_INFO("stored BP=:%d\r\n", temp_list->portid);
				brportcfg.nBridgePortId = temp_list->portid;
				brportcfg.eMask =
				GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID |
				GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP;
				ret = gsw_core_api((dp_gsw_cb)gsw_handle
						   ->gsw_brdgport_ops
						   .BridgePort_ConfigGet,
						   gsw_handle, &brportcfg);
				if (ret != GSW_statusOk) {
					PR_ERR("fail to get bport configed\n");
					goto EXIT;
				}
				/*for (i = 0; i < MAX_BP_NUM; i++) {
				 *	if (GET_BP_MAP(brportcfg.nBridgePortMap,
				 *	    i)) {
				 *		sprintf(buf1 + strlen(buf1),
				 *			"%d ", i);
				 *	}
				 *}
				 */
				for (k = 0; k < 16; k++) {
					PR_INFO("  nBridgePortMap[%d] = %x\n",
						k, brportcfg.nBridgePortMap[k]);
				}
				sprintf(buf1 + strlen(buf1), " Fid=%d ",
					brportcfg.nBridgeId);
				PR_INFO("          : bp=%d(%s)\n",
					brportcfg.nBridgePortId, buf1);
				buf1[0] = 0;
				}
			}
		}
EXIT:
	kfree(buf1);
	return count;
HELP:
	PR_INFO("Provide brname: echo brctl brdev_name > /proc/dp/brctl\n");
	return count;
}

static int fdb_cnt;
static void proc_swdev_fdb_read(struct seq_file *s)
{
	struct fdb_tbl *tmp = NULL;

	seq_puts(s, "dev_name		MAC\n");
	seq_puts(s, "---------------------------------\n");
	list_for_each_entry(tmp, &fdb_tbl_list, fdb_list) {
		if (tmp) {
			seq_printf
			(s, "%s		%02x:%02x:%02x:%02x:%02x:%02x\n",
			 tmp->port_dev->name, tmp->addr[0],
			 tmp->addr[1], tmp->addr[2], tmp->addr[3],
			 tmp->addr[4], tmp->addr[5]);
		} else {
			break;
		}
	}
}

static ssize_t proc_swdev_fdb_write(struct file *file, const char *buf,
				    size_t count,
				    loff_t *ppos)
{
	int len;
	char str[64];
	int num, flag = 0;
	char *param_list[10];
	struct fdb_tbl *tmp = NULL;
	struct net_device *dev;
	u8 b[6];

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if ((num != 5) ||
	    (dp_strncmpi(param_list[0], "help", strlen("help")) == 0))
		goto HELP;

	if (dp_strncmpi(param_list[0], "fdb", strlen("fdb")) == 0) {
		if (dp_strncmpi(param_list[1], "add", strlen("add")) == 0) {
			if (dp_strncmpi(param_list[2], "dev",
					strlen("dev")) == 0) {
				/*fdb add <mac> dev <port>*/
				tmp =
				kmalloc(sizeof(struct fdb_tbl *), GFP_KERNEL);
				if (!tmp)
					goto exit;
				INIT_LIST_HEAD(&tmp->fdb_list);
				tmp->port_dev =
				dev_get_by_name(&init_net, param_list[3]);
				mac_stob(param_list[4], tmp->addr);
				flag = 1;
			}
		}
		if (dp_strncmpi(param_list[1], "del", strlen("del")) == 0) {
			if (dp_strncmpi(param_list[2], "dev",
					strlen("dev")) == 0) {
				/*fdb add dev <port> <mac>*/
				dev = dev_get_by_name(&init_net, param_list[3]);
				mac_stob(param_list[4], b);
				flag = 2;
			}
		}
	}
	if (!fdb_cnt)
		INIT_LIST_HEAD(&fdb_tbl_list);
	if (list_empty(&fdb_tbl_list) && (fdb_cnt != 0))
		goto exit;
	if (fdb_cnt < 0)
		goto exit;
	if (flag == 1) {
		list_add_tail(&tmp->fdb_list, &fdb_tbl_list);
		fdb_cnt++;
	}
	if (flag == 2) {
		list_for_each_entry(tmp, &fdb_tbl_list, fdb_list) {
			if (memcmp(&tmp->addr, &b, 6) == 0) {
				list_del(&tmp->fdb_list);
				kfree(tmp);
				fdb_cnt--;
				break;
			}
		}
	}
	return count;
exit:
	PR_ERR("list add / count error %d\n", fdb_cnt);
	return count;
HELP:
	PR_INFO("echo fdb add dev <dev_name> <mac_addr> > /proc/dp/fdb\n");
	PR_INFO("echo fdb del dev <dev_name> <mac_addr> > /proc/dp/fdb\n");
	return count;
}

static void proc_swdev_mac_read(struct seq_file *s)
{
	int ret, i;
	//GSW_MAC_tableRead_t macread;
	GSW_MAC_tableQuery_t macread;
	struct core_ops *gsw_handle;
	static int bp = 1;

	gsw_handle = dp_port_prop[0].ops[0];
	seq_puts(s, "FID    BP    static	MAC\n");
	seq_puts(s, "---------------------------------\n");
	for (i = 0; i < 10; i++) {
	memset(&macread, 0, sizeof(macread));
	macread.nFId = 1;
	macread.nPortId = bp;
	ret = gsw_core_api((dp_gsw_cb)gsw_handle->gsw_swmac_ops.
			   //MAC_TableEntryRead, gsw_handle, &macread);
			   MAC_TableEntryQuery, gsw_handle, &macread);
	if (ret != GSW_statusOk) {
		PR_ERR("Failed to get MAC entry\n");
		//return -1;
	}

	seq_printf(s, "%d    %d    %d	%02x:%02x:%02x:%02x:%02x:%02x\n",
		   macread.nFId, macread.nPortId, macread.bStaticEntry,
		   macread.nMAC[0],
		   macread.nMAC[1], macread.nMAC[2], macread.nMAC[3],
		   macread.nMAC[4], macread.nMAC[5]);
	bp += 1;
	}
}
#endif

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
					PR_INFO("egress ep %s configed ok\n",
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
				PR_INFO("egress pmac ep %s configured ok\n",
					egress_entries[j].name);
				break;
			}
		}
	}
}

static ssize_t ep_port_write(struct file *file, const char *buf, size_t count,
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
				if (dp_strncmpi
				    (param_list[i],
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

		ret = dp_pmac_set_31(inst, port, &pmac_cfg);

		if (ret != 0) {
			PR_INFO("pmac set configuration failed\n");
			return -1;
		}
	} else if (dp_strncmpi(param_list[0], "egress",
	strlen("egress")) == 0) {
		port = dp_atoi(param_list[1]);

		pmac_eg_cfg(param_list, num, &pmac_cfg);
		ret = dp_pmac_set_31(inst, port, &pmac_cfg);

		if (ret != 0) {
			PR_INFO("pmac set configuration failed\n");
			return -1;
		}
	} else {
		PR_INFO("wrong command\n");
		goto help;
	}

	return count;
 help:
	PR_INFO("echo %s > /proc/dp/ep\n",
		"ingress/egress [ep_port] ['ingress/egress fields'] [value]");
	PR_INFO("(eg) echo ingress 1 pmac 1 > /proc/dp/ep\n");
	PR_INFO("(eg) echo egress 1 rm_l2hdr 2 > /proc/dp/ep\n");
	PR_INFO("echo %s %s",
		"ingress [ep_port]",
		"['errdisc/pmac/pmac_pmap/pmac_en_pmap/pmac_tc");
	PR_INFO("                         %s [value] > /proc/dp/ep\n",
		"/pmac_en_tc/pmac_subifid/pmac_srcport']");
	PR_INFO("echo %s %s > /proc/dp/ep\n",
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
	{PROC_EP, NULL, NULL, NULL, ep_port_write},
	{PROC_PCE, NULL, proc_gsw_pce_dump, proc_gsw_pce_start, NULL},
	{PROC_PMAC, NULL, proc_gsw_pmac_dump, proc_gsw_pmac_start,
	 proc_gsw_pmac_write},
	{DP_PROC_FILE_GSWIP_BP, NULL, proc_gsw_bp_dump,
		proc_common_start, NULL},
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
	{DP_PROC_FILE_SWDEV_BR, NULL, proc_swdev_brctl_dump,
		proc_swdev_brctl_start, proc_swdev_brctl_write},
	{DP_PROC_SWDEV_FDB, proc_swdev_fdb_read, NULL, NULL,
		proc_swdev_fdb_write},
	{DP_PROC_SWDEV_MAC, proc_swdev_mac_read, NULL, NULL,
		NULL},
#endif
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
	PR_INFO("dp_sub_proc_install ok\n");
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
