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
#include <net/datapath_api_vlan.h>

#ifdef CONFIG_LTQ_VMB
#include <asm/ltq_vmb.h>	/*vmb */
#include <asm/ltq_itc.h>	/*mips itc */
#endif
#include <linux/list.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include "datapath.h"
#include "datapath_instance.h"

/*meter alloc,add macros*/
#define DP_METER_ALLOC(inst, id, flag) \
	dp_port_prop[(inst)].info.dp_meter_alloc(inst, &(id), (flag))
#define DP_METER_CFGAPI(inst, func, dev, meter, flag, mtr_subif) \
	dp_port_prop[(inst)].info.func((dev), &(meter), (flag), (mtr_subif))

#define DP_PROC_NAME       "dp"
#define DP_PROC_BASE       "/proc/" DP_PROC_NAME "/"
#define DP_PROC_PARENT     ""

#define PROC_DBG   "dbg"
#define PROC_PORT   "port"
#define PROC_MEM "mem"
#define DP_PROC_PRINT_MODE "print_mode"
#define PROC_DT  "dt"
#define PROC_LOGICAL_DEV "logic"
#define PROC_INST_DEV "inst_dev"
#define PROC_INST_MOD "inst_mod"
#define PROC_INST_HAL "inst_hal"
#define PROC_INST "inst"
#define PROC_TX_PKT "tx"
#define PROC_QOS  "qos"
#define PROC_ASYM_VLAN  "asym_vlan"
#define PROC_METER "meter"
#define PROC_ALLOC_PARAM "-p <port-id> -t <inst> -o <owner>"
#define PROC_ALLOC_PARAM_FLAGS "-f <flags/(ETH_LAN/ETH_WAN/" \
			       "FAST_WLAN/DSL/Tunne_loop/" \
			       "DirectPath/GPON/EPON/GINT/.. >"
#define PROC_FREE_PARAM "-p <port-id> -t <inst>"

static int tmp_inst;
static ssize_t proc_port_write(struct file *file, const char *buf,
			       size_t count, loff_t *ppos);
static struct module owner[DP_MAX_INST][MAX_DP_PORTS] = {0};
#if defined(CONFIG_LTQ_DATAPATH_DBG) && CONFIG_LTQ_DATAPATH_DBG
static void proc_dbg_read(struct seq_file *s);
static ssize_t proc_dbg_write(struct file *, const char *, size_t, loff_t *);
#endif
static int proc_port_dump(struct seq_file *s, int pos);
static int proc_write_mem(struct file *, const char *, size_t, loff_t *);
static int proc_asym_vlan(struct file *, const char *, size_t, loff_t *);
static void print_device_tree_node(struct device_node *node, int depth);
static ssize_t proc_dt_write(struct file *file, const char *buf,
			     size_t count, loff_t *ppos);
static ssize_t proc_logical_dev_write(struct file *file, const char *buf,
				      size_t count, loff_t *ppos);
static ssize_t proc_meter_write(struct file *file, const char *buf,
				size_t count, loff_t *ppos);
static void meter_create_help(void);

static int alloc_port(char *param_list[], int num);
static int free_port(char *param_list[], int num);
static int32_t dp_cb_rx(struct net_device *rxif,
			struct net_device *txif,
			struct sk_buff *skb, int32_t len);
static int proc_port_init(void);

int proc_port_init(void)
{
	tmp_inst = 0;
	return 0;
}

int proc_port_dump(struct seq_file *s, int pos)
{
	int i, j;
	int cqm_p;
	int (*print_ctp_bp)(struct seq_file *s, int inst,
			    struct pmac_port_info *port,
			    int subif_index, u32 flag);
	struct pmac_port_info *port = get_port_info(tmp_inst, pos);
	u16 start = 0;

	if (!port) {
		PR_ERR("Why port is NULL\n");
		return -1;
	}
	print_ctp_bp = DP_CB(tmp_inst, proc_print_ctp_bp_info);
	DP_CB(tmp_inst, get_itf_start_end)(port->itf_info, &start, NULL);

	if (port->status == PORT_FREE) {
		if (pos == 0) {
			seq_printf(s,
				   "Reserved Port: rx_err_drop=0x%08x  tx_err_drop=0x%08x\n",
				   STATS_GET(port->rx_err_drop),
				   STATS_GET(port->tx_err_drop));
			if (print_ctp_bp) /*just to print bridge
					   *port zero's member
					   */
				print_ctp_bp(s, tmp_inst, port, 0, 0);
			i = 0;
			seq_printf(s, "          : qid/node:    %d/%d\n",
				   port->subif_info[i].qid,
				   port->subif_info[i].q_node);
			seq_printf(s, "          : port/node:    %d/%d\n",
				   port->subif_info[i].cqm_deq_port,
				   port->subif_info[i].qos_deq_port);

		} else
			seq_printf(s,
				   "%02d: rx_err_drop=0x%08x  tx_err_drop=0x%08x\n",
				   pos,
				   STATS_GET(port->rx_err_drop),
				   STATS_GET(port->tx_err_drop));

		goto EXIT;
	}

	seq_printf(s,
		   "%02d:%s=0x0x%0x(%s:%8s) %s=%02d %s=%02d %s=%d(%s) %s=%d\n",
		   pos,
		   "module",
		   (u32)port->owner,
		   "name",
		   port->owner->name,
		   "dev_port",
		   port->dev_port,
		   "dp_port",
		   port->port_id,
		   "itf_base",
		   start,
		   port->itf_info ? "Enabled" : "Not Enabled",
		   "ctp_max",
		   port->ctp_max);
	seq_printf(s, "    status:            %s\n",
		   dp_port_status_str[port->status]);
	seq_puts(s, "    allocate_flags:    ");
	for (i = 0; i < get_dp_port_type_str_size(); i++) {
		if (port->alloc_flags & dp_port_flag[i])
			seq_printf(s, "%s ", dp_port_type_str[i]);
	}
	seq_puts(s, "\n");
	seq_printf(s, "    mode:              %d\n", port->cqe_lu_mode);
	seq_printf(s, "    LCT:               %d\n", port->lct_idx);
	seq_printf(s, "    cb->rx_fn:         0x%0x\n", (u32)port->cb.rx_fn);
	seq_printf(s, "    cb->restart_fn:    0x%0x\n",
		   (u32)port->cb.restart_fn);
	seq_printf(s, "    cb->stop_fn:       0x%0x\n",
		   (u32)port->cb.stop_fn);
	seq_printf(s, "    cb->get_subifid_fn:0x%0x\n",
		   (u32)port->cb.get_subifid_fn);
	seq_printf(s, "    num_subif:         %d\n", port->num_subif);
	seq_printf(s, "    vap_offset/mask:   %d/0x%x\n", port->vap_offset,
		   port->vap_mask);
	seq_printf(s, "    flag_other:        0x%x\n", port->flag_other);
	seq_printf(s, "    deq_port_base:     %d\n", port->deq_port_base);
	seq_printf(s, "    deq_port_num:      %d\n", port->deq_port_num);
	seq_printf(s, "    dma_chan:          0x%x\n", port->dma_chan);
	seq_printf(s, "    tx_pkt_credit:     %d\n", port->tx_pkt_credit);
	seq_printf(s, "    tx_b_credit:       %02d\n", port->tx_b_credit);
	seq_printf(s, "    tx_ring_addr:      0x%x\n", port->tx_ring_addr);
	seq_printf(s, "    tx_ring_size:      %d\n", port->tx_ring_size);
	seq_printf(s, "    tx_ring_offset:    %d(to next dequeue port)\n",
		   port->tx_ring_offset);
	for (i = 0; i < port->ctp_max; i++) {
		if (!port->subif_info[i].flags)
			continue;
		seq_printf(s,
			   "      [%02d]:%s=0x%04x %s=0x%0x(%s=%s),%s=%s\n",
			i,
			"subif",
			port->subif_info[i].subif,
			"netif",
			(u32)port->subif_info[i].netif,
			"netif",
			port->subif_info[i].netif ?
			port->subif_info[i].netif->name : "NULL/DSL",
			"device_name",
			port->subif_info[i].device_name);
		seq_puts(s, "          : subif_flag = ");
		for (j = 0; j < get_dp_port_type_str_size(); j++) {
			if (!port->subif_info[i].subif_flag) {
				seq_printf(s, "%s ", "NULL");
				break;
			}
			if (port->subif_info[i].subif_flag & dp_port_flag[j])
				seq_printf(s, "%s ", dp_port_type_str[j]);
		}
		seq_puts(s, "\n");
		seq_printf(s, "          : rx_fn_rxif_pkt =0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.rx_fn_rxif_pkt));
		seq_printf(s, "          : rx_fn_txif_pkt =0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.rx_fn_txif_pkt));
		seq_printf(s, "          : rx_fn_dropped  =0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.rx_fn_dropped));
		seq_printf(s, "          : tx_cbm_pkt     =0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.tx_cbm_pkt));
		seq_printf(s, "          : tx_tso_pkt     =0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.tx_tso_pkt));
		seq_printf(s, "          : tx_pkt_dropped =0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.tx_pkt_dropped));
		seq_printf(s, "          : tx_clone_pkt   =0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.tx_clone_pkt));
		seq_printf(s, "          : tx_hdr_room_pkt=0x%08x\n",
			   STATS_GET(port->subif_info[i].mib.tx_hdr_room_pkt));
		if (print_ctp_bp)
			print_ctp_bp(s, tmp_inst, port, i, 0);
		seq_printf(s, "          : qid/node:    %d/%d\n",
			   port->subif_info[i].qid,
			   port->subif_info[i].q_node);
		cqm_p = port->subif_info[i].cqm_deq_port;
		seq_printf(s, "          : port/node:    %d/%d(ref=%d)\n",
			   cqm_p,
			   port->subif_info[i].qos_deq_port,
			   dp_deq_port_tbl[tmp_inst][cqm_p].ref_cnt);
		if (port->subif_info[i].ctp_dev &&
		    port->subif_info[i].ctp_dev->name)
			seq_printf(s, "          : ctp_dev = %s\n",
				   port->subif_info[i].ctp_dev->name);
		else
			seq_puts(s, "          : ctp_dev = NULL\n");
	}
	seq_printf(s, "    rx_err_drop=0x%08x  tx_err_drop=0x%08x\n",
		   STATS_GET(port->rx_err_drop),
		   STATS_GET(port->tx_err_drop));
EXIT:
	if (!seq_has_overflowed(s))
		pos++;
	if (pos >= MAX_DP_PORTS) {
		tmp_inst++;
		pos = 0;
	}
	if (tmp_inst >= dp_inst_num)
		pos = -1;	/*end of the loop */
	return pos;
}

int display_port_info(int inst, u8 pos, int start_vap, int end_vap, u32 flag)
{
	int i;
	int ret;
	struct pmac_port_info *port = get_port_info(inst, pos);
	u16 start = 0;

	if (!port) {
		PR_ERR("Why port is NULL\n");
		return -1;
	}

	if (port->status == PORT_FREE) {
		if (pos == 0) {
			PR_INFO("%s:rx_err_drop=0x%08x  tx_err_drop=0x%08x\n",
				"Reserved Port",
				STATS_GET(port->rx_err_drop),
				STATS_GET(port->tx_err_drop));

		} else
			PR_INFO("%02d:rx_err_drop=0x%08x  tx_err_drop=0x%08x\n",
				pos,
				STATS_GET(port->rx_err_drop),
				STATS_GET(port->tx_err_drop));

		goto EXIT;
	}

	DP_CB(tmp_inst, get_itf_start_end)(port->itf_info, &start, NULL);

	PR_INFO("%02d: %s=0x0x%0x(name:%8s) %s=%02d %s=%02d itf_base=%d(%s)\n",
		pos,
		"module",
		(u32)port->owner, port->owner->name,
		"dev_port",
		port->dev_port,
		"dp_port",
		port->port_id,
		start,
		port->itf_info ? "Enabled" : "Not Enabled");
	PR_INFO("    status:            %s\n",
		dp_port_status_str[port->status]);
	PR_INFO("    allocate_flags:    ");

	for (i = 0; i < get_dp_port_type_str_size(); i++) {
		if (port->alloc_flags & dp_port_flag[i])
			PR_INFO("%s ", dp_port_type_str[i]);
	}

	PR_INFO("\n");

	if (!flag) {
		PR_INFO("    cb->rx_fn:         0x%0x\n",
			(u32)port->cb.rx_fn);
		PR_INFO("    cb->restart_fn:    0x%0x\n",
			(u32)port->cb.restart_fn);
		PR_INFO("    cb->stop_fn:       0x%0x\n",
			(u32)port->cb.stop_fn);
		PR_INFO("    cb->get_subifid_fn:0x%0x\n",
			(u32)port->cb.get_subifid_fn);
		PR_INFO("    num_subif:         %02d\n", port->num_subif);
	}

	for (i = start_vap; i < end_vap; i++) {
		if (port->subif_info[i].flags) {
			PR_INFO
			    ("      [%02d]:%s=0x%04x %s=0x%0x(%s=%s),%s=%s\n",
			     i,
			     "subif",
			     port->subif_info[i].subif,
			     "netif",
			     (u32)port->subif_info[i].netif,
			     "device_name",
			     port->subif_info[i].netif ? port->subif_info[i].
			     netif->name : "NULL/DSL",
			     "name",
			     port->subif_info[i].device_name);
			PR_INFO("          : rx_fn_rxif_pkt =0x%08x\n",
				STATS_GET(port->subif_info[i].mib.
				rx_fn_rxif_pkt));
			PR_INFO("          : rx_fn_txif_pkt =0x%08x\n",
				STATS_GET(port->subif_info[i].mib.
				rx_fn_txif_pkt));
			PR_INFO("          : rx_fn_dropped  =0x%08x\n",
				STATS_GET(port->subif_info[i].mib.
				rx_fn_dropped));
			PR_INFO("          : tx_cbm_pkt     =0x%08x\n",
				STATS_GET(port->subif_info[i].mib.tx_cbm_pkt));
			PR_INFO("          : tx_tso_pkt     =0x%08x\n",
				STATS_GET(port->subif_info[i].mib.tx_tso_pkt));
			PR_INFO("          : tx_pkt_dropped =0x%08x\n",
				STATS_GET(port->subif_info[i].mib.
				tx_pkt_dropped));
			PR_INFO("          : tx_clone_pkt   =0x%08x\n",
				STATS_GET(port->subif_info[i].mib.
				tx_clone_pkt));
			PR_INFO("          : tx_hdr_room_pkt=0x%08x\n",
				STATS_GET(port->subif_info[i].mib.
				tx_hdr_room_pkt));
		}
	}

	ret = PR_INFO("    rx_err_drop=0x%08x  tx_err_drop=0x%08x\n",
		      STATS_GET(port->rx_err_drop),
		      STATS_GET(port->tx_err_drop));
EXIT:
	return 0;
}

ssize_t proc_port_write(struct file *file, const char *buf, size_t count,
			loff_t *ppos)
{
	int len;
	char str[64];
	int num, i;
	u8 index_start = 0;
	u8 index_end = MAX_DP_PORTS;
	int vap_start = 0;
	int vap_end = MAX_SUBIFS;
	char *param_list[10];
	int inst;

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (num <= 1)
		goto help;
	if (param_list[1]) {
		index_start = dp_atoi(param_list[1]);
		index_end = index_start + 1;
	}

	if (param_list[2]) {
		vap_start = dp_atoi(param_list[2]);
		vap_end = vap_start + 1;
	}

	if (index_start >= MAX_DP_PORTS) {
		PR_ERR("wrong index: 0 ~ 15\n");
		return count;
	}

	if (vap_start >= MAX_SUBIFS) {
		PR_ERR("wrong VAP: 0 ~ 15\n");
		return count;
	}

	if (dp_strncmpi(param_list[0], "mib", strlen("mib")) == 0) {
		for (inst = 0; inst < dp_inst_num; inst++)
		for (i = index_start; i < index_end; i++)
			display_port_info(inst, i, vap_start, vap_end, 1);

	} else if (dp_strncmpi(param_list[0], "port", strlen("port")) == 0) {
		for (inst = 0; inst < dp_inst_num; inst++)
		for (i = index_start; i < index_end; i++)
			display_port_info(inst, i, vap_start, vap_end, 0);

	} else {
		goto help;
	}

	return count;
 help:
	PR_INFO("usage:\n");
	PR_INFO("  echo mib  [ep][vap] > /prooc/dp/port\n");
	PR_INFO("  echo port [ep][vap] > /prooc/dp/port\n");
	return count;
}

#if defined(CONFIG_LTQ_DATAPATH_DBG) && CONFIG_LTQ_DATAPATH_DBG
void proc_dbg_read(struct seq_file *s)
{
	int i;

	seq_printf(s, "dp_dbg_flag=0x%08x\n", dp_dbg_flag);

	seq_printf(s, "Supported Flags =%d\n", get_dp_dbg_flag_str_size());
	seq_printf(s, "Enabled Flags(0x%0x):", dp_dbg_flag);

	for (i = 0; i < get_dp_dbg_flag_str_size(); i++)
		if ((dp_dbg_flag & dp_dbg_flag_list[i]) ==
		    dp_dbg_flag_list[i])
			seq_printf(s, "%s ", dp_dbg_flag_str[i]);

	seq_puts(s, "\n\n");

	seq_printf(s, "dp_drop_all_tcp_err=%d @ 0x%p\n", dp_drop_all_tcp_err,
		   &dp_drop_all_tcp_err);
	seq_printf(s, "dp_pkt_size_check=%d @ 0x%p\n", dp_pkt_size_check,
		   &dp_pkt_size_check);

	seq_printf(s, "dp_rx_test_mode=%d @ 0x%p\n", dp_rx_test_mode,
		   &dp_rx_test_mode);
	seq_printf(s, "dp_dbg_err(flat to print error or not)=%d @ 0x%p\n",
		   dp_dbg_err,
		   &dp_dbg_err);
	print_parser_status(s);
}

ssize_t proc_dbg_write(struct file *file, const char *buf, size_t count,
		       loff_t *ppos)
{
	int len, i, j;
	char str[64];
	int num;
	char *param_list[20];
	int f_enable;
	char *tmp_buf;
	#define BUF_SIZE_TMP 2000

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (dp_strncmpi(param_list[0], "enable", strlen("enable")) == 0)
		f_enable = 1;
	else if (dp_strncmpi(param_list[0], "disable", strlen("disable")) == 0)
		f_enable = -1;
	else
		goto help;

	if (!param_list[1]) {	/*no parameter after enable or disable */
		set_ltq_dbg_flag(dp_dbg_flag, f_enable, -1);
		goto EXIT;
	}

	for (i = 1; i < num; i++) {
		for (j = 0; j < get_dp_dbg_flag_str_size(); j++)
			if (dp_strncmpi(param_list[i], dp_dbg_flag_str[j], strlen(dp_dbg_flag_str[j])) ==
			    0) {
				set_ltq_dbg_flag(dp_dbg_flag, f_enable,
						 dp_dbg_flag_list[j]);
				break;
			}
	}

EXIT:
	return count;
help:
	tmp_buf = kmalloc(BUF_SIZE_TMP, GFP_KERNEL);
	if (!tmp_buf)
		return count;
	num = 0;
	num = snprintf(tmp_buf + num, BUF_SIZE_TMP - num - 1,
		       "echo <enable/disable> ");
	for (i = 0; i < get_dp_dbg_flag_str_size(); i++)
		num += snprintf(tmp_buf + num, BUF_SIZE_TMP - num - 1,
				"%s ", dp_dbg_flag_str[i]);

	num += snprintf(tmp_buf + num, BUF_SIZE_TMP - num - 1,
			" > /proc/dp/dbg\n");
	num += snprintf(tmp_buf + num, BUF_SIZE_TMP - num - 1,
			" display command: cat /proc/dp/cmd\n");
	PR_INFO("%s", tmp_buf);
	kfree(tmp_buf);
	return count;
}
#endif

/**
 * \brief directly read memory address with 4 bytes alignment.
 * \param  reg_addr memory address (it must be 4 bytes alignment)
 * \param  shift to the expected bits (its value is from 0 ~ 31)
 * \param  size the bits number (its value is from 1 ~ 32).
 *  Note: shift + size <= 32
 * \param  buffer destionation
 * \return on Success return 0
 */
int32_t dp_mem_read(u32 reg_addr, u32 shift, u32 size,
		    u32 *buffer, u32 base)
{
	u32 v;
	u32 mask = 0;
	int i;

	/*generate mask */
	for (i = 0; i < size; i++)
		mask |= 1 << i;

	mask = mask << shift;

	/*read data from specified address */
	if (base == 4)
		v = *(u32 *)reg_addr;
	else if (base == 2)
		v = *(u16 *)reg_addr;
	else
		v = *(u8 *)reg_addr;

	v = dp_get_val(v, mask, shift);

	*buffer = v;
	return 0;
}

/**
 * \brief directly write memory address with
 * \param  reg_addr memory address (it must be 4 bytes alignment)
 * \param  shift to the expected bits (its value is from 0 ~ 31)
 * \param  size the bits number (its value is from 1 ~ 32)
 * \param  value value writen to
 * \return on Success return 0
 */
int32_t dp_mem_write(u32 reg_addr, u32 shift, u32 size,
		     u32 value, u32 base)
{
	u32 tmp = 0;
	u32 mask = 0;
	int i;

	/*generate mask */
	for (i = 0; i < size; i++)
		mask |= 1 << i;

	mask = mask << shift;

	/*read data from specified address */
	if (base == 4) {
		tmp = *(u32 *)reg_addr;
	} else if (base == 2) {
		tmp = *(u16 *)reg_addr;
	} else if (base == 1) {
		tmp = *(u8 *)reg_addr;
	} else {
		PR_ERR("wrong base in dp_mem_write\n");
		return 0;
	}

	dp_set_val(tmp, value, mask, shift);

	if (base == 4)
		*(u32 *)reg_addr = tmp;
	else if (base == 2)
		*(u16 *)reg_addr = tmp;
	else
		*(u8 *)reg_addr = tmp;

	return 0;
}

#define MODE_ACCESS_BYTE  1
#define MODE_ACCESS_SHORT 2
#define MODE_ACCESS_DWORD 4

#define ACT_READ   1
#define ACT_WRITE  2
static int proc_write_mem(struct file *file, const char *buf, size_t count,
			  loff_t *ppos)
{
	char str[100];
	int num;
	char *param_list[20] = { NULL };
	int i, k, len;
	u32 line_max_num = 32;	/* per line number printed */
	u32 addr = 0;
	u32 v = 0;
	u32 act = ACT_READ;
	u32 bit_offset = 0;
	u32 bit_num = 32;
	u32 repeat = 1;
	u32 mode = MODE_ACCESS_DWORD;
	int v_flag = 0;
	char *tmp_buf;

	len = sizeof(str) < count ? sizeof(str) - 1 : count;
	len = len - copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (num < 2)
		goto proc_help;

	if (dp_strncmpi(param_list[0], "w", 1) == 0)
		act = ACT_WRITE;
	else if (dp_strncmpi(param_list[0], "r", 1) == 0)
		act = ACT_READ;
	else
		goto proc_help;

	if (num < 3)
		goto proc_help;

	k = 1;

	while (k < num) {
		if (dp_strncmpi(param_list[k], "-s", strlen("-s")) == 0) {
			addr = dp_atoi(param_list[k + 1]);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "-o", strlen("-o")) == 0) {
			bit_offset = dp_atoi(param_list[k + 1]);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "-n", strlen("-n")) == 0) {
			bit_num = dp_atoi(param_list[k + 1]);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "-r", strlen("-r")) == 0) {
			repeat = dp_atoi(param_list[k + 1]);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "-v", strlen("-v")) == 0) {
			v = dp_atoi(param_list[k + 1]);
			k += 2;
			v_flag = 1;
		} else if (dp_strncmpi(param_list[k], "-b", strlen("-b")) == 0) {
			mode = MODE_ACCESS_BYTE;
			k += 1;
		} else if (dp_strncmpi(param_list[k], "-w", strlen("-w")) == 0) {
			mode = MODE_ACCESS_SHORT;
			k += 1;
		} else if (dp_strncmpi(param_list[k], "-d", strlen("-d")) == 0) {
			mode = MODE_ACCESS_DWORD;
			k += 1;
		} else {
			PR_INFO("unknown command option: %s\n",
				param_list[k]);
			break;
		}
	}

	if (bit_num > mode * 8)
		bit_num = mode * 8;

	if (repeat == 0)
		repeat = 1;

	if (!addr) {
		PR_ERR("addr cannot be zero\n");
		goto EXIT;
	}

	if ((mode != MODE_ACCESS_DWORD) && (mode != MODE_ACCESS_SHORT) &&
	    (mode != MODE_ACCESS_BYTE)) {
		PR_ERR("wrong access mode: %d bytes\n", mode);
		goto EXIT;
	}

	if ((act == ACT_WRITE) && !v_flag) {
		PR_ERR("For write command it needs to provide -v\n");
		goto EXIT;
	}

	if (bit_offset > mode * 8 - 1) {
		PR_ERR("valid bit_offset range:0 ~ %d\n", mode * 8 - 1);
		goto EXIT;
	}

	if ((bit_num > mode * 8) || (bit_num < 1)) {
		PR_ERR("valid bit_num range:0 ~ %d. Current bit_num=%d\n",
		       mode * 8, bit_num);
		goto EXIT;
	}

	if ((bit_offset + bit_num) > mode * 8) {
		PR_ERR("valid bit_offset+bit_num range:0 ~ %d\n", mode * 8);
		goto EXIT;
	}

	if ((addr % mode) != 0) {	/*access alignment */
		PR_ERR("Cannot access 0x%08x in %d bytes\n", addr, mode);
		goto EXIT;
	}

	line_max_num /= mode;

	if (act == ACT_WRITE)
		PR_INFO
		    ("act=%s addr=0x%08x mode=%s %s=%d %s=%d v=0x%08x\n",
		     "write", addr,
		     (mode ==
		      MODE_ACCESS_DWORD) ? "dword" : ((mode ==
						       MODE_ACCESS_SHORT) ?
						      "short" : "DWORD"),
		     "bit_offset",
		     bit_offset,
		     "bit_num",
		     bit_num,
		     v);
	else if (act == ACT_READ)
		PR_INFO
		    ("act=%s addr=0x%08x mode=%s bit_offset=%d bit_num=%d\n",
		     "Read", addr,
		     (mode ==
		      MODE_ACCESS_DWORD) ? "dword" : ((mode ==
						       MODE_ACCESS_SHORT) ?
						      "short" : "DWORD"),
		     bit_offset, bit_num);

	if (act == ACT_WRITE)
		for (i = 0; i < repeat; i++)
			dp_mem_write(addr + mode * i, bit_offset, bit_num, v,
				     mode);
	else {
		#define CH_PER_NUM  11 /*like 0X12345678*/
		int offset = 0;

		tmp_buf = kmalloc((line_max_num + 1) * CH_PER_NUM, GFP_KERNEL);
		if (!tmp_buf)
			return count;
		for (i = 0; i < repeat; i++) {
			v = 0;
			dp_mem_read(addr + mode * i, bit_offset, bit_num, &v,
				    mode);

			/*print format control */
			if ((i % line_max_num) == 0)
				offset += sprintf(tmp_buf + offset,
						  "0x%08x: ", addr + mode * i);

			if (mode == MODE_ACCESS_DWORD)
				offset += sprintf(tmp_buf + offset, "0x%08x ",
						  v);
			else if (mode == MODE_ACCESS_SHORT)
				offset += sprintf(tmp_buf + offset, "0x%04x ",
						  v);
			else
				offset += sprintf(tmp_buf + offset, "0x%02x ",
						  v);

			if ((i % line_max_num) == (line_max_num - 1)) {
				offset += sprintf(tmp_buf + offset, "\n");
				PR_INFO("%s", tmp_buf);
				offset = 0;
				}
		}
		if (offset) {
			offset += sprintf(tmp_buf + offset, "\n");
			PR_INFO("%s", tmp_buf);
		}
		kfree(tmp_buf);
	}
	PR_INFO("\n");
EXIT:
	return count;

proc_help:
	PR_INFO("echo <write/w> %s -s %s [-r %s]-v <value> [-o %s] [-n %s]\n",
		"[-d/w/b]",
		"<start_v_addr>",
		"<repeat_times>",
		"<bit_offset>",
		"<bit_num>");
	PR_INFO("echo <read/r>  %s -s %s [-r %s] [-o %s] [-n %s]\n",
		"[-d/w/b]",
		"<start_v_addr>",
		"<repeat_times>",
		"<bit_offset>",
		"<bit_num>");
	PR_INFO("\t -d: default read/write in dwords, ie 4 bytes\n");
	PR_INFO("\t -w: read/write in short, ie 2 bytes\n");
	PR_INFO("\t -b: read/write in bytes, ie 1 bytes\n");

	return count;
}

static void set_dev(struct dp_tc_vlan *vlan, struct net_device *dev, int
		    def_apply, int dir, int n_vlan0, int n_vlan1, int n_vlan2,
		    int mcast)
{
	vlan->dev = dev;
	vlan->def_apply = def_apply;
	vlan->mcast_flag = mcast;
	vlan->dir = dir;
	vlan->n_vlan0 = n_vlan0;
	vlan->n_vlan1 = n_vlan1;
	vlan->n_vlan2 = n_vlan2;
}

static void set_pattern(struct dp_pattern_vlan *patt,
			int prio, int vid, int tpid,
			int dei, int proto)
{
	patt->prio = prio;
	patt->vid = vid;
	patt->tpid = tpid;
	patt->dei = dei;
	patt->proto = proto;
}

static void set_action(struct dp_act_vlan *act, int action, int pop_n, int
		       push_n)
{
	act->act = action;
	act->pop_n = pop_n;
	act->push_n = push_n;
}

static void set_tag(struct dp_act_vlan *act, int copy,  int index, int prio,
		    int vid, int tpid, int dei)
{
	if ((copy == CP_FROM_INNER) || (copy == CP_FROM_OUTER)) {
		act->prio[index] = copy;
		act->vid[index] = copy;
		act->tpid[index] = copy;
		act->dei[index] = copy;
	} else {
		act->prio[index] = prio;
		act->vid[index] = vid;
		act->tpid[index] = tpid;
		act->dei[index] = dei;
	}
}

static int proc_asym_vlan(struct file *file, const char *buf, size_t count,
			  loff_t *ppos)
{
	char str[256];
	int num;
	char *param_list[20] = { NULL };
	int  k, len;
	char dev_name[16];
	int test_num = 0;
	int ctp = 0;
	int mcast = 0;
	int dir = 0;
	struct dp_tc_vlan vlan = {0};
	struct dp_vlan0 vlan0_list[1] = {0};
	struct dp_vlan1 vlan1_list[4] = {0};
	struct dp_vlan2 vlan2_list[3] = {0};
	struct net_device *dev;

#define TEST_VID 10
	len = sizeof(str) < count ? sizeof(str) - 1 : count;
	len = len - copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));

	if (num < 2)
		goto proc_help;

	k = 0;

	while (k < num) {
		if (dp_strncmpi(param_list[k], "dev", strlen("dev")) == 0) {
			dev_name[0] = '\0';
			strlcat(dev_name,  param_list[k + 1], 16);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "-tnum", strlen("-tnum")) == 0) {
			test_num = dp_atoi(param_list[k + 1]);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "CTP", strlen("CTP")) == 0) {
			ctp = dp_atoi(param_list[k + 1]);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "mcast", strlen("mcast")) == 0) {
			mcast = dp_atoi(param_list[k + 1]);
			k += 2;
		} else if (dp_strncmpi(param_list[k], "dir", strlen("dir")) == 0) {
			dir = dp_atoi(param_list[k + 1]);
			k += 2;
		} else {
			PR_INFO("unknown command option: %s\n",
				param_list[k]);
			break;
		}
	}
	dev = dev_get_by_name(&init_net, dev_name);
	if (!dev) {
		PR_INFO("unknown device");
		return -1;
	}

	switch (test_num) {
	case 1:
		PR_INFO("Input:IP packet with VID 10\n");
		PR_INFO("Desc:pattern match = FALSE\n");
		PR_INFO("Output:Enqueued packet is received without change\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		/*random proto for failing the pattern match*/
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, 0x100);
		vlan1_list[0].act.act = DP_VLAN_ACT_DROP;
		dp_vlan_set(&vlan, 0);

	break;
	case 2:
		PR_INFO("Input:IP packet without VLAN tag\n");
		PR_INFO("Desc:pattern match = FALSE\n");
		PR_INFO("Output:Enqueued packet is received without change\n");
		set_dev(&vlan, dev, ctp, dir, 1, 0, 0, mcast);
		vlan.vlan0_list = vlan0_list;
		/*random proto for failing the pattern match*/
		set_pattern(&vlan0_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, 0x100);
		vlan0_list[0].act.act = DP_VLAN_ACT_DROP;
		dp_vlan_set(&vlan, 0);

	break;
	case 3:
		PR_INFO("Input:IP packet without VLAN tag\n");
		PR_INFO("Desc:pattern match = TRUE\n");
		PR_INFO("Output:Enqueued packet is not received\n");
		set_dev(&vlan, dev, ctp, dir, 1, 0, 0, mcast);
		vlan.vlan0_list = vlan0_list;
		/*random proto for failing the pattern match*/
		set_pattern(&vlan0_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		vlan0_list[0].act.act = DP_VLAN_ACT_DROP;
		dp_vlan_set(&vlan, 0);

	break;
	case 4:
		PR_INFO("This test is same as testnum 1, so skipping\n");
	break;
	case 5:
		PR_INFO("Input:IP packet with VID 10\n");
		PR_INFO("Desc:pattern match = TRUE and POP action\n");
		PR_INFO("Output:Enqueued packet is received without vlantag\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    TEST_VID, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		vlan1_list[0].act.act = DP_VLAN_ACT_POP;
		vlan1_list[0].act.pop_n = 1;
		dp_vlan_set(&vlan, 0);

	break;
	case 6:
		PR_INFO("Input:IP packet with double tag VID 10 and vid	100\n");
		PR_INFO("Desc:pattern match = TRUE and POP action\n");
		PR_INFO("Output:Enqueued packet is received without vlantag\n");
		set_dev(&vlan, dev, ctp, dir, 0, 0, 1, mcast);
		vlan.vlan2_list = vlan2_list;
		set_pattern(&vlan2_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    TEST_VID, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_pattern(&vlan2_list[0].inner, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		vlan2_list[0].act.act = DP_VLAN_ACT_POP;
		vlan2_list[0].act.pop_n = 2;
		dp_vlan_set(&vlan, 0);

	break;
	case 7:
		PR_INFO("Input:IP packet without vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with vlan tag");
		PR_INFO("that is pushed\n");
		set_dev(&vlan, dev, ctp, dir, 1, 0, 0, mcast);
		vlan.vlan0_list = vlan0_list;
		set_pattern(&vlan0_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan0_list[0].act, DP_VLAN_ACT_PUSH, 0, 1);
		set_tag(&vlan0_list[0].act, 0, 0, 0, 10, 0x8100, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 8:
		PR_INFO("Input:IP packet without vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with 2 vlan tags");
		PR_INFO("that are pushed\n");
		set_dev(&vlan, dev, ctp, 0, 1, 0, 0, mcast);
		vlan.vlan0_list = vlan0_list;
		set_pattern(&vlan0_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan0_list[0].act, DP_VLAN_ACT_PUSH, 0, 2);
		set_tag(&vlan0_list[0].act, 0, 0, 0, 10, 0x8100, 0);
		set_tag(&vlan0_list[0].act, 0, 1, 1, 100, 0x8100, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 9:
		PR_INFO("Input:IP packet without vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with vlan tag");
		PR_INFO("that is pushed\n");
		set_dev(&vlan, dev, ctp, dir, 1, 0, 0, mcast);
		vlan.vlan0_list = vlan0_list;
		set_pattern(&vlan0_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan0_list[0].act, DP_VLAN_ACT_PUSH, 0, 1);
		set_tag(&vlan0_list[0].act, 0, 0, 0, 10, 0x8100, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 10:
		PR_INFO("Input:IP packet without vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with 2 vlan tags");
		PR_INFO("that are pushed\n");
		set_dev(&vlan, dev, ctp, dir, 1, 0, 0, mcast);
		vlan.vlan0_list = vlan0_list;
		set_pattern(&vlan0_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan0_list[0].act, DP_VLAN_ACT_PUSH, 0, 2);
		set_tag(&vlan0_list[0].act, 0, 0, 0, 10, 0x8100, 0);
		set_tag(&vlan0_list[0].act, 0, 1, 1, 100, 0x8100, 0);
		dp_vlan_set(&vlan, 0);
	break;
	case 11:
		PR_INFO("Input:IP packet single vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with 2 vlan tags,");
		PR_INFO("the original and the pushed one\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_PUSH, 0, 1);
		set_tag(&vlan1_list[0].act, CP_FROM_OUTER, 0, 0, 0, 0, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 12:
		PR_INFO("Input:IP packet single vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with 3 vlan tags,");
		PR_INFO("the original and 2 pushed ones\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_PUSH, 0, 2);
		set_tag(&vlan1_list[0].act, CP_FROM_OUTER, 0, 0, 0, 0, 0);
		set_tag(&vlan1_list[0].act, CP_FROM_OUTER, 1, 0, 0, 0, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 13:
		PR_INFO("Input:IP packet single vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with 2 vlan tags,");
		PR_INFO("the original and the pushed one\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_PUSH, 0, 1);
		set_tag(&vlan1_list[0].act, CP_FROM_OUTER, 0, 0, 0, 0, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 14:
		PR_INFO("Input:IP packet single vlan tag\n");
		PR_INFO("Desc:pattern match = TRUE and PUSH action\n");
		PR_INFO("Output:Enqueued packet is received with 3 vlan tags");
		PR_INFO("the original and 2 pushed ones\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_PUSH, 0, 2);
		set_tag(&vlan1_list[0].act, CP_FROM_OUTER, 0, 0, 0, 0, 0);
		set_tag(&vlan1_list[0].act, CP_FROM_OUTER, 1, 0, 0, 0, 0);
		dp_vlan_set(&vlan, 0);
	break;
	case 15:
		PR_INFO("Input:IP packet single vlan tag and vid 10\n");
		PR_INFO("Desc:pattern match = FALSE and DROP action\n");
		PR_INFO("Output:Enqueued packet is received unaltered\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    100, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 16:
		PR_INFO("Input:IP packet single vlan tag and vid 10\n");
		PR_INFO("Desc:pattern match = TRUE and DROP action\n");
		PR_INFO("Output:Enqueued packet is dropped\n");
		set_dev(&vlan, dev, ctp, dir, 0, 1, 0, mcast);
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    10, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 17:
		PR_INFO("Input:IP packet double vlan tag, vid 10 and vid 20\n");
		PR_INFO("Desc:pattern match = FALSE and DROP action\n");
		PR_INFO("Output:Enqueued packet is received unaltered\n");
		set_dev(&vlan, dev, ctp, dir, 0, 0, 1, mcast);
		vlan.vlan2_list = vlan2_list;
		set_pattern(&vlan2_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    100, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_pattern(&vlan2_list[0].inner, DP_VLAN_PATTERN_NOT_CARE,
			    200, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan2_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		dp_vlan_set(&vlan, 0);

	break;
	case 18:
		PR_INFO("Input:IP packet double vlan tag, vid 10 and vid 20\n");
		PR_INFO("Desc:pattern match = TRUE and DROP action\n");
		PR_INFO("Output:Enqueued packet is not received\n");
		set_dev(&vlan, dev, ctp, dir, 0, 0, 1, mcast);
		vlan.vlan2_list = vlan2_list;
		set_pattern(&vlan2_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    10, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_pattern(&vlan2_list[0].inner, DP_VLAN_PATTERN_NOT_CARE,
			    20, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan2_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		dp_vlan_set(&vlan, 0);

	break;

	case 19:
	{
		struct dp_vlan1 vlan1_list[4] = {0};

		PR_INFO("Input:IP packet multiple streams single vlan tag,");
		PR_INFO("with vid 5,6,7,8\n");
		PR_INFO("Desc:pattern match = TRUE (vid match) , with action");
		PR_INFO("PUSH for vid 5,6 POP for vid 7 and forward vid 8\n");
		PR_INFO("Output:Enqueued packet received\n");
		set_dev(&vlan, dev, ctp, dir, 0, 4, 0, mcast);
		vlan1_list[0].def = 0;
		vlan1_list[1].def = 0;
		vlan1_list[2].def = 0;
		vlan1_list[3].def = 0;
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    5, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_pattern(&vlan1_list[1].outer, DP_VLAN_PATTERN_NOT_CARE,
			    6, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_pattern(&vlan1_list[2].outer, DP_VLAN_PATTERN_NOT_CARE,
			    7, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_pattern(&vlan1_list[3].outer, DP_VLAN_PATTERN_NOT_CARE,
			    8, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_PROTO_IP4);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_PUSH, 0, 1);
		set_tag(&vlan1_list[0].act, 0, 0, 0, 0, 0x8100, 0);

		set_action(&vlan1_list[1].act, DP_VLAN_ACT_PUSH, 0, 2);
		set_tag(&vlan1_list[1].act, CP_FROM_OUTER, 0, 0, 0, 0x8100, 0);
		set_tag(&vlan1_list[1].act, CP_FROM_OUTER, 1, 0, 0, 0x8100, 0);

		set_action(&vlan1_list[2].act, DP_VLAN_ACT_POP, 1, 0);
		set_action(&vlan1_list[3].act, DP_VLAN_ACT_FWD, 0, 0);

		dp_vlan_set(&vlan, 0);
	}
	break;

	case 20:
		PR_INFO("This test to del rule\n");
		set_dev(&vlan, dev, ctp, dir, 0, 0, 0, mcast);
		vlan.vlan0_list = NULL;
		vlan.vlan1_list = NULL;
		vlan.vlan2_list = NULL;
		dp_vlan_set(&vlan, 0);
	break;
	case 21:
	{
		PR_INFO("Input:IP packet single or double vlan tag,");
		PR_INFO("with outer vid 74\n");
		PR_INFO("Desc:pattern match = TRUE (vid match) , with action");
		PR_INFO("forward vid 74 and drop other VLAN tag\n");
		PR_INFO("Output:Enqueued pkt recv for vid 74 other vid drop\n");
		set_dev(&vlan, dev, ctp, dir, 0, 2, 2, mcast);
		vlan1_list[0].def = 1; /* default rule */
		vlan1_list[1].def = 0;
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		set_pattern(&vlan1_list[1].outer, DP_VLAN_PATTERN_NOT_CARE,
			    74, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan1_list[1].act, DP_VLAN_ACT_FWD, 0, 0);
		vlan2_list[0].def = 1;
		vlan2_list[1].def = 0;
		vlan.vlan2_list = vlan2_list;
		set_pattern(&vlan2_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_pattern(&vlan2_list[0].inner, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan2_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		set_pattern(&vlan2_list[1].outer, DP_VLAN_PATTERN_NOT_CARE,
			    74, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_pattern(&vlan2_list[1].inner, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan2_list[1].act, DP_VLAN_ACT_FWD, 0, 0);
		dp_vlan_set(&vlan, 0);
	}
	break;
	case 22:
	{
		PR_INFO("Input:IP packet single or double vlan tag,");
		PR_INFO("with outer vid 100 or 200\n");
		PR_INFO("Desc:pattern match = TRUE (vid match) , with action");
		PR_INFO("forward vid 100,200 and drop other VLAN tag\n");
		PR_INFO("Output:Enq pkt recv for vid 100,200 other vid drop\n");
		set_dev(&vlan, dev, ctp, dir, 0, 3, 2, mcast);
		vlan1_list[0].def = 1; /* default rule */
		vlan1_list[1].def = 0;
		vlan1_list[2].def = 0;
		vlan.vlan1_list = vlan1_list;
		set_pattern(&vlan1_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan1_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		set_pattern(&vlan1_list[1].outer, 1,
			    100, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan1_list[1].act, DP_VLAN_ACT_FWD, 0, 0);
		set_pattern(&vlan1_list[2].outer, 2,
			    300, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan1_list[2].act, DP_VLAN_ACT_DROP, 0, 0);
		vlan2_list[0].def = 1;
		vlan2_list[1].def = 0;
		vlan.vlan2_list = vlan2_list;
		set_pattern(&vlan2_list[0].outer, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_pattern(&vlan2_list[0].inner, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan2_list[0].act, DP_VLAN_ACT_DROP, 0, 0);
		set_pattern(&vlan2_list[1].outer, 2,
			    200, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_pattern(&vlan2_list[1].inner, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE,
			    DP_VLAN_PATTERN_NOT_CARE, DP_VLAN_PATTERN_NOT_CARE);
		set_action(&vlan2_list[1].act, DP_VLAN_ACT_FWD, 0, 0);
		dp_vlan_set(&vlan, 0);
	}
	break;

	default:
		PR_INFO("unknown test case\n");
	break;
	}

	PR_INFO("\n");
	return count;

proc_help:
	PR_INFO("echo <dev> %s [-tnum %s] %s\n", "<device name>", "test_number",
		"CTP <0/1> mcast <0/1> dir <0/1>");
	return count;
}

ssize_t proc_dt_write(struct file *file, const char *buf,
		      size_t count, loff_t *ppos)
{
	u16 len;
	char str[64];
	char *param_list[20 * 2];
	unsigned int num;
	struct device_node *node;

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if (!param_list[0] || (strlen(param_list[0]) <= 1))
		param_list[0] = "/";
	node = of_find_node_by_path(param_list[0]);
	if (!node)
		node = of_find_node_by_name(NULL, param_list[0]);
	if (!node) {
		PR_INFO("Cannot find node for %s\n", param_list[0]);
		return count;
	}
	print_device_tree_node(node, 0);
	return count;
}

struct proc_vap_test {
	struct net_device *dev;
};

struct vdev_priv {
	int vap;
	struct rtnl_link_stats64 stats;
};

static int vdev_open(struct net_device *dev)
{
	netif_start_queue(dev);
	return 0;
}

static int vdev_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

static int vdev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	dev_kfree_skb(skb);
	return 0;
}

static int vdev_ioctl(struct net_device *dev,
		      struct ifreq *ifr, int cmd)
{
	return -EOPNOTSUPP;
}

static int vdev_dev_init(struct net_device *dev)
{
	return 0;
}

static int vdev_set_mac_address(struct net_device *dev,
				void *p)
{
	struct sockaddr *addr = p;

	if (netif_running(dev))
		return -EBUSY;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EINVAL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	return 0;
}

static int vdev_change_mtu(struct net_device *dev,
			   int new_mtu)
{
	dev->mtu = new_mtu;
	return 0;
}

static struct rtnl_link_stats64 *vdev_get_stats(
		struct net_device *dev,
		struct rtnl_link_stats64 *storage)
{
	struct vdev_priv *priv = netdev_priv(dev);
	*storage = priv->stats;
	return storage;
}

static struct net_device_ops vdev_ops = {
	.ndo_init         = vdev_dev_init,
	.ndo_open         = vdev_open,
	.ndo_stop         = vdev_close,
	.ndo_start_xmit   = vdev_xmit,
	.ndo_do_ioctl     = vdev_ioctl,
	.ndo_set_mac_address = vdev_set_mac_address,
	.ndo_change_mtu   = vdev_change_mtu,
	.ndo_get_stats64	  = vdev_get_stats,
};

static void vdev_setup(struct net_device *dev)
{
	ether_setup(dev);
	dev->netdev_ops = &vdev_ops;
}

#define PROC_DP_MAX_VAP 256
static struct proc_vap_test vaps[PROC_DP_MAX_VAP];
int free_new_vap_dev(struct net_device *dev)
{
	int i;
	dp_subif_t subif = {0};

	if (!dev)
		return 0;

	/* Don't free it and DP may still using it for pmapper case */
	if (dp_get_port_subitf_via_dev(dev, &subif) == DP_SUCCESS) {
		PR_INFO("Don't free dev %s yet\n", dev->name);
		return 0;
	}
	PR_INFO("Free dev %s\n", dev->name);
	for (i = 0; i < ARRAY_SIZE(vaps); i++) {
		if (vaps[i].dev != dev)
			continue;
		unregister_netdev(vaps[i].dev);
		free_netdev(vaps[i].dev);
		vaps[i].dev = NULL;
		return i;
	}
	return -1;
}

struct net_device *create_new_vap_dev(char *name)
{
	int i;

	if (!name)
		return NULL;
	for (i = 0; i < ARRAY_SIZE(vaps); i++) {
		if (vaps[i].dev)
			continue;
		vaps[i].dev = alloc_netdev(sizeof(struct vdev_priv),
					   name,
					   NET_NAME_ENUM,
					   vdev_setup);
		strncpy(vaps[i].dev->name, name, sizeof(vaps[i].dev->name) - 1);
		memset(vaps[i].dev->dev_addr, 0, sizeof(vaps[i].dev->dev_addr));
		vaps[i].dev->dev_addr[5] = (u8)(i + 1);
		if (register_netdev(vaps[i].dev)) {
			free_netdev(vaps[i].dev);
			vaps[i].dev = NULL;
			PR_ERR("register device fail:%s?\n", name);
		}
		return vaps[i].dev;
	}

	return NULL;
}

int del_vap(char *param_list[], int num)
{
	u8 vap;
	int dp_port;
	dp_subif_t subif = {0};
	struct pmac_port_info *port_info;
	struct net_device *dev = NULL, *ctp_dev = NULL;
	int inst = 0;
	struct dp_subif_data data = {0};

	if (num < 4) {
		PR_ERR("Not enough parameters\n");
		return -1;
	}
	/*dp_port */
	dp_port = dp_atoi(param_list[1]);
	if ((dp_port < 0) && (dp_port >= MAX_DP_PORTS)) {
		PR_ERR("Wrong dp_port=%d\n", dp_port);
		return -1;
	}
	if (!dp_port_info[inst][dp_port].status) {
		PR_ERR("dp_port status=%d: expect non-zero\n",
		       dp_port_info[0][dp_port].status);
		return -1;
	}
	if (!dp_port_info[inst][dp_port].num_subif) {
		PR_ERR("dp_port num_subif=%d: expect non-zero\n",
		       dp_port_info[0][dp_port].num_subif);
		return -1;
	}
	port_info = get_port_info_via_dp_port(inst, dp_port);
	if (!port_info) {
		PR_ERR("port_info NULL\n");
		return -1;
	}
	/*vap */
	vap = dp_atoi(param_list[2]);
	if ((vap < 0) ||
	    (vap >= port_info->ctp_max)) {
		PR_ERR("Wrong vap=%d: expect 0 ~ %d\n",
		       vap, port_info->ctp_max - 1);
		return -1;
	}
	if (port_info->subif_info[vap].flags == 0) {
		PR_ERR("VAP=%d not registered yet\n", vap);
		return -1;
	}
	subif.subif = SET_VAP(vap,
			      port_info->vap_offset,
			      port_info->vap_mask);
	dev = port_info->subif_info[vap].netif;
	ctp_dev = port_info->subif_info[vap].ctp_dev;
	/*de-register */
	subif.inst = inst;
	subif.port_id = dp_port;
	if (dp_strncmpi(param_list[3], "lct", 3) == 0) {/*lct */
		data.flag_ops |= DP_F_DATA_LCT_SUBIF;
	}
	if (dp_register_subif_ext(inst, port_info->owner, dev,
				  dev->name, &subif, &data,
				  DP_F_DEREGISTER)) {
		PR_INFO("de-register_subif fail: %s\n", dev->name);
		return -1;
	}
	if (ctp_dev && ctp_dev->name)
		PR_INFO("\nDe-Register subif for %s ok: subif=0x%x(%d) %s:%s\n",
			dev->name, subif.subif, vap,
			"ctp_dev:", ctp_dev->name);
	else
		PR_INFO("\nDe-Register subif for %s ok: subif=0x%x(%d)\n",
			dev->name, subif.subif, vap);

	free_new_vap_dev(dev);
	free_new_vap_dev(ctp_dev);
	return 0;
}

static int32_t dp_cb_rx(struct net_device *rxif, struct net_device *txif,
			struct sk_buff *skb, int32_t len)
{
	if (skb)
		skb_pull(skb, sizeof(struct pmac_rx_hdr));
	else
		return -1;

	if (rxif) {
		if (netif_running(rxif)) {
			skb->dev = rxif;
			return netif_rx(skb);
		}
	} else {
		PR_ERR("Tx fails\n");
		dev_kfree_skb_any(skb);
	}
	return 0;
}

int free_port(char *param_list[], int num)
{
	int inst = 0, res, port_id = 1, c;
	int opt_offset = 1;
	char *optstring = "p:t:";
	char *optarg = 0;
	dp_cb_t cb = {0};

	if (num < 2) {
		PR_ERR("Not enough parameters\n");
		goto help;
	}
	while ((c = dp_getopt(param_list, num, &opt_offset,
			      &optarg, optstring)) > 0) {
		if (optstring)
			PR_INFO("opt_offset=%d optarg=%s.\n",
				opt_offset, optarg);
		switch (c) {
		case 'p':
			port_id = dp_atoi(optarg);
			PR_INFO("port_id=%d\n", port_id);
			if (port_id < 1 || port_id > MAX_DP_PORTS) {
				PR_ERR("Invalid Port_id\n");
				return -1;
			}
			break;
		case 't':
			inst = dp_atoi(optarg);
			PR_INFO("inst=%d\n", inst);
			if (inst < 0) {
				PR_ERR("wrong inst for  with ep=%d\n",
				       port_id);
				return -1;
			}
			break;
		default:
			PR_INFO("wrong command");
			goto help;
		}
	}
	if (c < 0) {
		PR_INFO("Wrong command\n");
		goto help;
	}
	cb.rx_fn = (dp_rx_fn_t)dp_cb_rx;
	res = dp_register_dev_ext(inst, &owner[inst][port_id], port_id,
				  &cb, NULL, DP_F_DEREGISTER);
	if (res < 0) {
		PR_ERR("dp_register_dev_ext failed\n");
		return -1;
	}
	res = dp_alloc_port_ext(inst, &owner[inst][port_id], NULL, 0,
				port_id, NULL, NULL, DP_F_DEREGISTER);
	if (res < 0) {
		PR_ERR("dp_alloc_port_ext de-reg failed\n");
		return -1;
	}
	PR_INFO("DP port free successfully\n");
	return 0;
help:
	PR_INFO("echo free %s > /sys/kernel/debug/dp/%s\n",
		PROC_FREE_PARAM, PROC_LOGICAL_DEV);
	return 0;
}

int alloc_port(char *param_list[], int num)
{
	int inst = 0, res, port_id = 1, flags = 0, c, j;
	int opt_offset = 1;
	char *optstring = "p:t:o:f:";
	char *optarg = 0;
	dp_cb_t cb = {0};

	if (num < 2) {
		PR_ERR("Not enough parameters\n");
		goto help;
	}
	while ((c = dp_getopt(param_list, num, &opt_offset,
			      &optarg, optstring)) > 0) {
		if (optstring)
			PR_INFO("opt_offset=%d optarg=%s.\n",
				opt_offset, optarg);
		PR_INFO("\ndp_getopt :%c\n", c);
		switch (c) {
		case 'p':
			PR_INFO("port_id=%s\n", optarg);
			port_id = dp_atoi(optarg);
			if (port_id < 1 || port_id > MAX_DP_PORTS) {
				PR_ERR("Invalid Port_id\n");
				return -1;
			}
			break;
		case 't':
			inst = dp_atoi(optarg);
			PR_INFO("inst=%d\n", inst);
			break;
		case 'o':
			strncpy(owner[inst][port_id].name, optarg,
				sizeof(owner[inst][port_id].name) - 1);
			PR_INFO("owner name=%s\n", optarg);
			break;
		case 'f':
			for (j = 0; j < get_dp_port_type_str_size(); j++) {
				if (dp_strncmpi(optarg,
						dp_port_type_str[j],
						strlen(dp_port_type_str[j]))
						== 0) {
					flags |= dp_port_flag[j];
					PR_INFO("flag =%d\n", flags);
					break;
				}
			}
			break;
		default:
			PR_INFO("wrong command");
			goto help;
		}
	}
	if (c < 0) {
		PR_INFO("Wrong command\n");
		goto help;
	}
	res = dp_alloc_port_ext(inst, &owner[inst][port_id], NULL, 0,
				port_id, NULL, NULL, flags);
	if (res < 0) {
		PR_ERR("dp_alloc_port_ext failed\n");
		return -1;
	}
	cb.rx_fn = (dp_rx_fn_t)dp_cb_rx;
	res = dp_register_dev_ext(inst, &owner[inst][port_id], port_id,
				  &cb, NULL, flags);
	if (res < 0) {
		dp_alloc_port_ext(inst, &owner[inst][port_id], NULL, 0,
				  port_id, NULL, NULL, DP_F_DEREGISTER);
		PR_ERR("dp_register_dev_ext failed\n");
		return -1;
	}
	PR_INFO("DP port allocated successfully\n");
	return 0;

help:
	PR_INFO("echo alloc %s %s > /sys/kernel/debug/dp/%s\n",
		PROC_ALLOC_PARAM, PROC_ALLOC_PARAM_FLAGS,
		PROC_LOGICAL_DEV);
	return 0;
}

#define DP_PROC_Q_AUTO_SHARE 0
#define DP_PROC_Q_NEW_QUEUE  -1
/* param_list[]: parameter list
 * num: parameter list size
 */
int add_vap(char *param_list[], int num)
{
	struct dp_subif_data data = {0};
	u8 vap;
	int dp_port;
	dp_subif_t subif = {0};
	struct pmac_port_info *port_info;
	char name[IFNAMSIZ] = {0};
	struct net_device *dev = NULL;
	int inst = 0;

	if (num < 6) {
		PR_ERR("Not enough parameters\n");
		return -1;
	}
	/*dp_port */
	dp_port = dp_atoi(param_list[1]);
	if ((dp_port < 0) && (dp_port >= MAX_DP_PORTS)) {
		PR_ERR("Wrong dp_port=%d\n", dp_port);
		return -1;
	}
	if (!dp_port_info[inst][dp_port].status) {
		PR_ERR("dp_port status=%d: expect non-zero\n",
		       dp_port_info[0][dp_port].status);
		return -1;
	}
	if (!dp_port_info[inst][dp_port].num_subif) {
		PR_ERR("dp_port num_subif=%d: expect non-zero\n",
		       dp_port_info[0][dp_port].num_subif);
		return -1;
	}
	port_info = get_port_info_via_dp_port(inst, dp_port);
	if (!port_info) {
		PR_ERR("port_info NULL\n");
		return -1;
	}

	/*vap */
	vap = dp_atoi(param_list[2]);
	if ((vap < 0) ||
	    (vap >= port_info->ctp_max)) {
		PR_ERR("Wrong vap=%d: expect 0 ~ %d\n",
		       vap, port_info->ctp_max - 1);
		return -1;
	}
	if (port_info->subif_info[vap].flags != 0) {
		PR_ERR("VAP=%d already exist\n", vap);
		return -1;
	}
	subif.subif = SET_VAP(vap,
			      port_info->vap_offset,
			      port_info->vap_mask);

	/*tcont*/
	data.deq_port_idx = dp_atoi(param_list[3]);
	if ((data.deq_port_idx < 0) ||
	    (data.deq_port_idx >= port_info->deq_port_num)) {
		PR_ERR("Wrong tcont=%d: expect 0 ~ %d\n",
		       data.deq_port_idx, port_info->deq_port_num - 1);
		return -1;
	}

	/*qid */
	if (dp_strncmpi(param_list[4], "def", 3) == 0) {/*sharing queue*/
		data.flag_ops = 0;
	} else if (dp_strncmpi(param_list[4], "new", 3) == 0) {
		data.flag_ops = DP_SUBIF_AUTO_NEW_Q; /*alloc new queue */
	} else {
		data.q_id = dp_atoi(param_list[4]);
		if (data.q_id > 0)
			data.flag_ops = DP_SUBIF_SPECIFIC_Q; /*specify queue*/
		else {
			PR_ERR("Wrong Queue ID:%d\n", data.q_id);
			return -1;
		}
	}
	if (dp_strncmpi(param_list[5], "lct", 3) == 0) {/*lct */
		data.flag_ops |= DP_F_DATA_LCT_SUBIF;
	}
	/*ctp-dev*/
	if (param_list[6]) {
		char ctp_name[IFNAMSIZ] = {0};
		/* create ctp dev */
		strncpy(name, param_list[6], sizeof(name) - 1);
		snprintf(ctp_name, sizeof(ctp_name), "ctp%d_%d", dp_port, vap);
		data.ctp_dev = create_new_vap_dev(ctp_name);
		if (!data.ctp_dev) {
			PR_ERR("failed to create dev %s\n", ctp_name);
			goto ERR_EXIT;
		}
		/* parent dev */
		rtnl_lock();
		dev = dev_get_by_name(&init_net, param_list[5]);
		if (!dev) {
			rtnl_unlock();
			snprintf(name, sizeof(name), "%s", param_list[5]);
			/* need to create ctp parent later */
			goto CREATE_DEV;
		}
		rtnl_unlock();
		dev_put(dev);
	}
CREATE_DEV:
	if (!dev) {
		if (!name[0])
			snprintf(name, sizeof(name), "vap%d_%x", dp_port, vap);
		dev = create_new_vap_dev(name);
		if (!dev) {
			PR_ERR("failed to create dev %s\n", name);
			goto ERR_EXIT;
		}
	}
	/*register */
	subif.inst = inst;
	subif.port_id = dp_port;
	if (dp_register_subif_ext(inst, port_info->owner, dev,
				  dev->name, &subif, &data, 0)) {
		PR_INFO("register_subif fail: %s 0x%x(vap=%d)\n",
			dev->name, subif.subif, vap);
		goto ERR_EXIT;
	}
	PR_INFO("\nRegister subif for %s ok: subif=0x%x(%d) %s\n",
		dev->name,
		subif.subif, vap,
		data.ctp_dev ? data.ctp_dev->name : "");
	return 0;
ERR_EXIT:
	if (dev) {
		free_new_vap_dev(dev);
		dev = NULL;
	}
	if (data.ctp_dev) {
		free_new_vap_dev(data.ctp_dev);
		data.ctp_dev = NULL;
	}
	return -1;
}

ssize_t proc_logical_dev_write(struct file *file, const char *buf,
			       size_t count, loff_t *ppos)
{
	u16 len;
	char str[64];
	static char *param_list[20 * 4];
	unsigned int num;
	struct net_device *dev = NULL, *base = NULL;
	dp_subif_t *subif = NULL;

	struct pmac_port_info *port_info;
	static struct vlan_prop vlan_prop;

	memset(param_list, 0, sizeof(*param_list));
	memset(&vlan_prop, 0, sizeof(vlan_prop));
	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;
	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if (num <= 1)
		goto HELP;
	subif = kmalloc(sizeof(*subif), GFP_KERNEL);
	if (!subif) {
		PR_INFO("kmalloc failed for %d bytes\n", sizeof(*subif));
		goto EXIT1;
	}
	if (dp_strncmpi(param_list[0], "check", strlen("check")) == 0) {
		if (num != 2)
			goto HELP;
		dev = dev_get_by_name(&init_net, param_list[1]);
		if (!dev) {
			PR_INFO("No such device:%s\n", param_list[1]);
			goto EXIT1;
		}
		get_vlan_via_dev(dev, &vlan_prop);
		if (!vlan_prop.num) {
			PR_INFO("No VLAN interface %s\n", param_list[1]);
			goto EXIT1;
		}
		if (!vlan_prop.base) {
			PR_INFO("No base found for %s\n", param_list[1]);
			goto EXIT1;
		}
		if (vlan_prop.num > 0)
			PR_INFO("outer VLAN proto=%x, vid=%d\n",
				vlan_prop.out_proto, vlan_prop.out_vid);
		if (vlan_prop.num == 2)
			PR_INFO("Inner VLAN proto=%x, vid=%d\n",
				vlan_prop.in_proto, vlan_prop.in_vid);
		PR_INFO("base of %s: %s\n",
			param_list[0], vlan_prop.base->name);
		goto EXIT1;
	} else if (dp_strncmpi(param_list[0], "set", strlen("set")) == 0) {
		u32 flag = DP_F_SUBIF_LOGICAL;

		if ((num != 2) && (num != 3))
			goto HELP;
		dev = dev_get_by_name(&init_net, param_list[1]);
		if (!dev) {
			PR_INFO("No such device:%s\n", param_list[1]);
			goto EXIT1;
		}
		if (dp_strncmpi(param_list[2], "explicit", strlen("explicit")) == 0)
			flag |= DP_F_ALLOC_EXPLICIT_SUBIFID;
		base = get_base_dev(dev, -1);
		if (!base) { /*not logical dev */
			base = dev;
			flag = 0;
		}
		if (dp_get_netif_subifid(base, NULL, NULL, NULL, subif, 0)) {
			PR_INFO("dp_get_netif_subifid fail:%s\n", base->name);
			goto EXIT1;
		}
		port_info = get_port_info_via_dp_port(0, subif->port_id);
		if (!port_info) {
			PR_INFO("get_port_info_via_dp_port fail: port_id:%d\n",
				subif->port_id);
			goto EXIT1;
		}
		subif->subif = -1;
		if (dp_register_subif(port_info->owner, dev,
				      dev->name, subif, flag)) {
			PR_INFO("dp_register_subif fail: %s\n",
				dev->name);
			goto EXIT1;
		}
		PR_INFO("\nRegister subif for %s ok with %s\n",
			param_list[1],
			flag & DP_F_ALLOC_EXPLICIT_SUBIFID ?
			"explicit" : "its base's subif/ctp");
	} else if (dp_strncmpi(param_list[0], "unset", strlen("unset")) == 0) {
		u32 flag = DP_F_DEREGISTER;

		if (num != 2)
			goto HELP;
		dev = dev_get_by_name(&init_net, param_list[1]);
		if (!dev) {
			PR_INFO("No such device:%s\n", param_list[1]);
			goto EXIT1;
		}
		if (dp_get_netif_subifid(dev, NULL, NULL, NULL, subif, 0)) {
			PR_INFO("dp_get_netif_subifid fail:%s\n", dev->name);
			goto EXIT1;
		}
		port_info = get_port_info_via_dp_port(0, subif->port_id);
		if (!port_info) {
			PR_INFO("get_port_info_via_dp_port fail: port_id:%d\n",
				subif->port_id);
			goto EXIT1;
		}
		if (dp_register_subif(port_info->owner, dev,
				      dev->name, subif, flag)) {
			PR_INFO("dp_deregister_subif fail: %s\n",
				dev->name);
			goto EXIT1;
		}
		PR_INFO("Deregister subif for %s ok with subif=%d\n",
			param_list[1],
			subif->subif);
	} else if (dp_strncmpi(param_list[0], "get", strlen("get")) == 0) {
		u32 flag = 0;
		int i;

		if (num < 2)
			goto HELP;
		dev = dev_get_by_name(&init_net, param_list[1]);
		if (!dev) {
			PR_INFO("No such device:%s\n", param_list[1]);
			goto EXIT1;
		}
		subif->port_id = -1;
		if (dp_get_netif_subifid(dev, NULL, NULL, NULL, subif,
					 flag)) {
			PR_INFO("dp_get_netif_subifid fail: %s\n",
				dev->name);
			goto EXIT1;
		}
		PR_INFO("dev %s subif info:\n", dev->name);
		PR_INFO("  inst=%d ep=%d bp=%d flag_bp=%u\n",
			subif->inst, subif->port_id, subif->bport,
			subif->flag_bp);
		for (i = 0; i < subif->subif_num; i++)
			PR_INFO("  subif[%d]=%d\n",
				i, subif->subif_list[i]);
		PR_INFO("\n");
		goto EXIT1;
	} else if (dp_strncmpi(param_list[0], "add_v", strlen("add_v")) == 0) {
		add_vap(param_list, num);
		goto EXIT1;
	} else if (dp_strncmpi(param_list[0], "del_v", strlen("del_v")) == 0) {
		del_vap(param_list, num);
		goto EXIT1;
	} else if (dp_strncmpi(param_list[0], "alloc",
		   strlen("alloc") + 1) == 0) {
		alloc_port(param_list, num);
	} else if (dp_strncmpi(param_list[0], "free",
		   strlen("free") + 1) == 0) {
		free_port(param_list, num);
	} else {
		PR_ERR("Wrong cmd:%s\n", param_list[0]);
		goto EXIT1;
	}
EXIT1:
	if (dev)
		dev_put(dev);

	kfree(subif);
	return count;

HELP:
	PR_INFO("Check Base Dev:echo check logic_dev_name > /proc/dp/%s\n",
		PROC_LOGICAL_DEV);
	PR_INFO("Register Logical dev:echo set logic_dev [explicit] > %s/%s\n",
		DP_PROC_BASE, PROC_LOGICAL_DEV);
	PR_INFO("DeRegister Logical dev:echo unset logic_dev_name > %s/%s\n",
		DP_PROC_BASE, PROC_LOGICAL_DEV);
	PR_INFO("Get dp_subif info:echo get dev_name > /proc/dp/%s\n",
		PROC_LOGICAL_DEV);
	PR_INFO("Add vap:echo add_v <dp_port> <vap> <tcont> <qid> %s\n",
		"lct <ctp_parent_dev> > " DP_PROC_BASE PROC_LOGICAL_DEV);
	PR_INFO("   Note of qid:\n");
	PR_INFO("     default  : auto share queue(default handling)\n");
	PR_INFO("     new_queue: auto alloc new queue\n");
	PR_INFO("     value(>0): specified queue id by caller.\n");
	PR_INFO("   Note of vap: 0, 1, 2, 3,....\n");
	PR_INFO("   Note of ctp_dev: for PON pmapper case\n");
	PR_INFO("   Note of lct: for LCT device registration\n");
	PR_INFO("Del vap:echo del_v <dp_port> <vap> lct %s/%s\n",
		DP_PROC_BASE, PROC_LOGICAL_DEV);
	PR_INFO("echo alloc %s %s > /sys/kernel/debug/dp/%s\n",
		PROC_ALLOC_PARAM, PROC_ALLOC_PARAM_FLAGS,
		PROC_LOGICAL_DEV);
	PR_INFO("echo free %s > /sys/kernel/debug/dp/%s\n",
		PROC_FREE_PARAM, PROC_LOGICAL_DEV);
	if (dev)
		dev_put(dev);

	kfree(subif);
	return count;
}

struct property_info {
	char *name;
	int type;
};

enum PROPERTY_TYPE {
	PROP_UNKNOWN = 0,
	PROP_STRING,
	PROP_REG,
	PROP_RANGER,
	PROP_U32_OCT,
	PROP_U32_HEX,
	PROP_HANDLE,
	PROP_REFERENCE
};

struct property_info prop_info[] = {
	{"compatible", PROP_STRING},
	{"status", PROP_STRING},
	{"name", PROP_STRING},
	{"label", PROP_STRING},
	{"model", PROP_STRING},
	{"reg-names", PROP_STRING},
	{"reg", PROP_REG},
	{"interrupts", PROP_U32_OCT},
	{"ranges", PROP_RANGER},
	{"dma-ranges", PROP_RANGER},
	{"phandle", PROP_HANDLE},
	{"interrupt-parent", PROP_HANDLE}

};

int get_property_info(char *name)
{
	int i;

	if (!name)
		return PROP_UNKNOWN;
	for (i = 0; i < ARRAY_SIZE(prop_info); i++) {
		if (dp_strncmpi(name, prop_info[i].name, strlen(prop_info[i].name)) == 0)
			return prop_info[i].type;
	}

	return PROP_UNKNOWN;
}

/*0--not string
 *1--is string
 */
/*#define LOCAL_STRING_PARSE*/
int is_print_string(char *p, int len)
{
	int i;

	if (!p || !len)
		return 0;
	if (p[len - 1] != 0)
		return 0;
	for (i = 0; i < len - 1; i++) {
#ifdef LOCAL_STRING_PARSE
		if (!(((p[i] >= 'a') && (p[i] <= 'z')) ||
		      ((p[i] >= 'A') && (p[i] <= 'Z')) ||
		      ((p[i] >= '0') && (p[i] <= '9')) ||
		      (p[i] == '.') ||
		      (p[i] == '/')))
#else
		if (!isprint(p[i]) && p[i] != 0) /*string list */
#endif
			return 0;
	}
	if (p[0] == 0)
		return 0;

	return 1;
}

#define INDENT_BASE 3 /*3 Space */
void print_property(struct device_node *node, struct property *p, char *indent)
{
	int type;
	int k, times, i;

	if (!p || !node)
		return;
	type = get_property_info(p->name);
	if (type == PROP_UNKNOWN) {
		if (is_print_string(p->value, p->length))
			type = PROP_STRING;
		else if ((p->length % 4) == 0)
			type = PROP_U32_OCT;
	}
	if (type == PROP_STRING) {
		char *s = (char *)p->value;
		int k = 0;

		PR_INFO("%s  %s=", indent, p->name);
		do {
			PR_INFO("\"%s\"", s);
			k += strlen(s) + 1;
			if (k < p->length) {
				s += strlen(s) + 1;
				PR_INFO(",");
				continue;
			}
			PR_INFO("\n");
			break;
		} while (1);
	} else if (type == PROP_U32_OCT) { /*each item is 4 bytes*/
		PR_INFO("%s  %s=<", indent, p->name);
		times = p->length / 4;
		if (times) {
			for (k = 0; k < times; k++)
				PR_INFO("%d ", *(int *)(p->value + k * 4));
		}
		PR_INFO(">\n");
	} else if (type == PROP_U32_HEX) { /*each item is 4 bytes*/
		PR_INFO("%s  %s=<", indent, p->name);
		times = p->length / 4;
		if (times) {
			for (k = 0; k < times; k++)
				PR_INFO("0x%x ", *(int *)(p->value + k * 4));
		}
		PR_INFO(">\n");
	} else if (type == PROP_REG) {/*two tuple: address and size */
		int n = (of_n_addr_cells(node) + of_n_size_cells(node));
		int j;

		PR_INFO("%s  %s=<", indent, p->name);
		times = p->length / (4 * n);
		if (times) {
			for (k = 0; k < times; k++) {
				if (k)
					PR_INFO("%s    ", indent);
				for (j = 0; j < n; j++)
					PR_INFO("0x%x ",
						*(int *)(p->value + k * 8 +
						4 * j));
				if (k != (times - 1))
					PR_INFO("\n");
			}
		}
		PR_INFO(">\n");
	} else if (type == PROP_RANGER) {
		/*triple: child-bus-address, parent-bus-address, length */
		PR_INFO("%s  %s=<", indent, p->name);
		times = p->length / (4 * 3);
		if (times) {
			for (k = 0; k < times; k++) {
				if (!k)
					PR_INFO("0x%x 0x%x 0x%x",
						*(int *)(p->value + k * 8),
						*(int *)(p->value + k * 8 + 4),
						*(int *)(p->value + k * 8 + 8));
				else
					PR_INFO("%s    0x%x 0x%x 0x%x", indent,
						*(int *)(p->value + k * 8),
						*(int *)(p->value + k * 8 + 4),
						*(int *)(p->value + k * 8 + 8));
				if (k != (times - 1))
					PR_INFO("\n");
			}
		}
		PR_INFO(">\n");
	} else if (type == PROP_HANDLE) {
		struct device_node *tmp = of_find_node_by_phandle(
			be32_to_cpup((u32 *)p->value));
		int offset = 0;

		if (tmp) {
			PR_INFO("%s  %s=<&%s", indent, p->name, tmp->name);
			offset = 1;
		} else {
			PR_INFO("%s  %s=<", indent, p->name);
		}
		if (p->length >= 4) {
			int times = p->length / 4;

			if (times) {
				for (k = offset; k < times; k++)
					PR_INFO("%d ",
						*(int *)(p->value + k * 4));
			}
		}
		PR_INFO(">\n");
	} else {
		PR_INFO("%s  %s length=%d\n", indent, p->name, p->length);
		if (p->length) {
			char *s = (unsigned char *)p->value;

			PR_INFO("%s   ", indent);
			for (i = 0; i < p->length; i++)
				PR_INFO("0x%02x ", s[i]);
			PR_INFO("\n");
		}
	}
}

void print_device_tree_node(struct device_node *node, int depth)
{
	int i = 0, len;
	struct device_node *child;
	struct property *p;
	char *indent;

	if (!node)
		return;
	len = (depth + 1) * 3;
	indent = kmalloc(len, GFP_KERNEL);
	if (!indent)
		return;
	for (i = 0; i < depth * 3; i++)
		indent[i] = ' ';
	indent[i] = '\0';
	++depth;
	PR_INFO("%s{%s(%s)_cell addr/size=%d/%d\n",
		indent, node->name, node->full_name,
		of_n_addr_cells(node), of_n_size_cells(node));
	for_each_property_of_node(node, p)
		print_property(node, p, indent);
	for_each_child_of_node(node, child)
		print_device_tree_node(child, depth);

	PR_INFO("%s}\n", indent);
	kfree(indent);
}

static u8 ipv4_plain_udp[] = {
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01, /*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x08, 0x00,	/*type */
	0x45, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x11, /*ip hdr*/
	0x3A, 0x56, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x7A, 0x41, 0x00, 0x00, /*udp hdr*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

static u8 ipv4_plain_tcp[1514] = {
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01, /*mac */
	0x00, 0x10, 0x94, 0x00, 0x00, 0x02,
	0x08, 0x00,	/*type */
	0x45, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x06, /*ip hdr*/
	0x3A, 0x61, 0xC0, 0x55, 0x01, 0x02, 0xC0, 0x00, 0x00, 0x01,
	0x04, 0x00, 0x04, 0x00, 0x00, 0x01, 0xE2, 0x40, 0x00, 0x03, /*tcp hdr*/
	0x94, 0x47, 0x50, 0x10, 0x10, 0x00, 0x9F, 0xD9, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*data */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

/*flag: for dp_xmit
 *pool: 1-CPU
 *discard: discard bit in DMA descriptor, CQM HW will drop the packet
 *priority: for skb->priority
 */
static int dp_send_packet(u8 *pdata, int len, char *devname, u32 flag,
			  u32 pool, u32 discard, int priority, int color)
{
	struct sk_buff *skb;
	dp_subif_t subif = { 0 };
	#define PAD  32

	if (pool == 1) {
		len += PAD;
		if (len < ETH_ZLEN)
			len = ETH_ZLEN;
		skb = alloc_skb(len, GFP_ATOMIC);
		if (skb)
			skb_reserve(skb, PAD);
		PR_INFO_ONCE("Allocate CPU buffer\n");
	} else {
		skb = cbm_alloc_skb(len + 8, GFP_ATOMIC);
		PR_INFO_ONCE("Allocate bm buffer\n");
	}

	if (unlikely(!skb)) {
		PR_ERR("allocate cbm buffer fail\n");
		return -1;
	}

	skb->DW0 = 0;
	skb->DW1 = 0;
	skb->DW2 = 0;
	skb->DW3 = 0;
	if (discard) {
		((struct dma_tx_desc_3 *)&skb->DW3)->field.dic = 1;
		PR_INFO_ONCE("discard bit set in DW3\n");
	}
	((struct dma_tx_desc_1 *)&skb->DW1)->field.color = color;
	memcpy(skb->data, pdata, len);
	skb->data[5] = (char)priority;
	skb->len = 0;
	skb_put(skb, len);
	skb->dev = dev_get_by_name(&init_net, devname);
	skb->priority = priority;

	if (dp_get_netif_subifid(skb->dev, skb, NULL, skb->data, &subif, 0)) {
		PR_ERR("dp_get_netif_subifid failed for %s\n",
		       skb->dev->name);
		dev_kfree_skb_any(skb);
		return -1;
	}

	((struct dma_tx_desc_1 *)&skb->DW1)->field.ep = subif.port_id;
	((struct dma_tx_desc_0 *)&skb->DW0)->field.dest_sub_if_id =
		subif.subif;

	if (dp_xmit(skb->dev, &subif, skb, skb->len, flag))
		return -1;

	return 0;
}

#define OPT_TX_DEV "[-i <dev_name>]"
#define OPT_TX_NUM "[-n <pkt_num>]"
#define OPT_TX_PROT "[-t <types: udp/tcp/raw>]"
#define OPT_TX_POOL "[-p <pool:cpu/bm>]"
#define OPT_TX_F_DISC "[-d]"
#define OPT_TX_PRIO "[-c <class/prio range>]"
#define OPT_TX_COLOR "[-o <color>]"
#define OPT_TX1 (OPT_TX_DEV OPT_TX_NUM OPT_TX_PROT)
#define OPT_TX2 (OPT_TX_POOL OPT_TX_F_DISC OPT_TX_PRIO OPT_TX_COLOR)
#define OPT_TX (OPT_TX1 OPT_TX2)
char l2_hdr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		 0x01, 0x01, 0x01, 0x01, 0x01, 0x11,
		 0x12, 0x34};
ssize_t proc_tx_pkt(struct file *file, const char *buf,
		    size_t count, loff_t *ppos)
{
	int len = 0, c;
	char data[100];
	char *param_list[10];
	int num, pkt_num = 1, times, pool = 0, disc = 0;
	char *p = ipv4_plain_udp;
	int size = sizeof(ipv4_plain_udp);
	short prio_range = 1, color = 0;
	int opt_offset;
	char *optstring = "i:n:t:p:dc:o:";
	char *optarg = 0;
	char *dev_name = "eth1";

	len = (count >= sizeof(data)) ? (sizeof(data) - 1) : count;
	DP_DEBUG(DP_DBG_FLAG_DBG, "len=%d\n", len);

	if (len <= 0) {
		PR_ERR("Wrong len value (%d)\n", len);
		return count;
	}
	if (copy_from_user(data, buf, len)) {
		PR_ERR("copy_from_user fail");
		return count;
	}
	data[len - 1] = 0; /* Make string */
	num = dp_split_buffer(data, param_list, ARRAY_SIZE(param_list));
	if (num <= 1)
		goto help;
	opt_offset = 0;
	while ((c = dp_getopt(param_list, num,
			      &opt_offset, &optarg, optstring)) > 0) {
		if (optstring)
			DP_DEBUG(DP_DBG_FLAG_DBG, "opt_offset=%d optarg=%s.\n",
				 opt_offset, optarg);
		switch (c) {
		case 'i':
			dev_name = optarg;
			DP_DEBUG(DP_DBG_FLAG_DBG, "dev_name=%s\n", dev_name);
			break;
		case 'n':
			pkt_num = dp_atoi(optarg);
			PR_INFO("pkt_num=%d\n", pkt_num);
			break;
		case 't':
			if (dp_strncmpi(optarg, "tcp", strlen("tcp")) == 0) {
				p = ipv4_plain_tcp;
				size = sizeof(ipv4_plain_tcp);
			} else if (dp_strncmpi(optarg, "udp", strlen("udp")) == 0) {
				p = ipv4_plain_udp;
				size = sizeof(ipv4_plain_udp);
			} else  {
				PR_INFO("Wrong procol selected\n");
				return count;
			}
			break;
		case 'p':
			if (dp_strncmpi(optarg, "cpu", strlen("cpu")) == 0) {
				pool = 1;
			} else if (dp_strncmpi(optarg, "bm", strlen("bm")) == 0) {
				pool = 0;
			} else  {
				PR_INFO("Wrong procol selected\n");
				return count;
			}
			break;
		case 'd':
			disc = 1;
			break;
		case 'c':
			prio_range = dp_atoi(optarg);
			if (prio_range <= 0)
				prio_range = 1;
			PR_INFO("prio_range=%d\n", prio_range);
			break;
		case 'o':
			color = dp_atoi(optarg);
			if (color < 0)
				color = 0;
			else if (color > 3)
				color = 3;
			PR_INFO("color=%d\n", color);
			break;
		default:
			PR_INFO("Wrong command\n");
			goto help;
		}
	}
	if (c < 0) { /*c == 0 means reach end of list */
		PR_INFO("Wrong command\n");
		goto help;
	}

	if (!dev_name) { /*c == 0 means reach end of list */
		PR_INFO("Must provide dev_name\n");
		return count;
	}

	times = 0;
	while (pkt_num--) {
		if (dp_send_packet(p, size, dev_name, 0, pool, disc,
				   times % prio_range, color))
			break;
		times++;
	}
	PR_INFO("sent %d packet already\n", times);
	return count;
help:   /*                        [0]         [1]         [2]     [3] [4]*/
	PR_INFO("tx packet: echo %s\n", OPT_TX1);
	PR_INFO("                %s\n", OPT_TX2);
	return count;
}

void meter_create_help(void)
{
	PR_INFO("METER ADD/DELETE: echo meter <dev> %s",
		"<alloc/dealloc/add/delete>\n");
	PR_INFO("<port_flag> <trfic_dir> <trfic_type> <cir> <pir> <cbs> %s",
		"<pbs> <type> > /sys/kernel/debug/dp/qos");
	PR_INFO("     dev: CTP/BP/Bridge device name\n");
	PR_INFO("     alloc/deallc/add/del: meter operation\n");
	PR_INFO("     port_flag: opt flag for CTP/BP/br/clrMrk/CPUtrfic\n");
	PR_INFO("     trfic_dir: opt ingress or egress data\n");
	PR_INFO("     trfic_type: traffic flow type(unicast,multicast,..\n");
	PR_INFO("     cir: opt committed information rate in bit/s\n");
	PR_INFO("     pir: opt Peak information rate in bit/s\n");
	PR_INFO("     cbs: opt committed burst size in bytes\n");
	PR_INFO("     pbs: opt peak burst size in bytes\n");
	PR_INFO("     type:opt type single/dual rate(strTCM,trTCM\n");
}

ssize_t proc_meter_write(struct file *file, const char *buf, size_t count,
			 loff_t *ppos)
{
	int len;
	char str[100];
	char *param_list[16] = { 0 };
	unsigned int level = 0, num = 0;

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;

	if (!len)
		return count;

	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	level = num - 3;

	if ((num <= 1 || num > ARRAY_SIZE(param_list)) ||
	    (dp_strncmpi(param_list[0], "help", strlen("help")) == 0))
		meter_create_help();
	else if (dp_strncmpi(param_list[0], "meter",
			     strlen("meter")) == 0) {
		int inst = 0;
		struct dp_meter_cfg meter = {
			.type = srTCM,
			.cir = 5000000,
			.pir = 5000000,
			.cbs = 1023,
			.pbs = 1023,
			.col_mode = 0,
			.dir = DP_DIR_EGRESS,
			.mode = DP_PCP_8P0D,
			.dp_pce.flow = DP_UKNOWN_UNICAST,
			.dp_pce.pce_idx = 0
		};
		struct net_device *dev;
		int ret;
		int meter_flag = DP_METER_ATTACH_CTP, meterid = -1;
		struct dp_meter_subif mtr_subif = {0};
		
		mtr_subif.inst = inst;
		dev = dev_get_by_name(&init_net, param_list[1]);
		if (!dev) {
			PR_ERR(" dev NULL\n");
			return count;
		}
		ret = dp_get_netif_subifid(dev, NULL, NULL, NULL,
					   &mtr_subif.subif, 0);
		if ( ret < 0) {
			PR_ERR("subif fails\n");
			return count;
		}
		if (dp_strncmpi(param_list[2], "dealloc",
				strlen("dealloc") + 1) == 0) {
			meterid = dp_atoi(param_list[3]);
			ret = DP_METER_ALLOC(inst, meterid, DP_F_DEREGISTER);
			if (ret < 0) {
				PR_ERR("Fail to get meter dealloc\n");
				return count;
			}
		PR_INFO("Meter dealloc succes, MeterId:=%d\n",
			meterid);
		} else if (dp_strncmpi(param_list[2], "alloc",
				       strlen("alloc") + 1) == 0) {
			ret = DP_METER_ALLOC(inst, meterid, 0);
			if (ret < 0) {
				PR_ERR("Fail to get meter alloc\n");
				return count;
			}
			PR_INFO("Meter alloc succes, MeterId:=%d\n", meterid);
		} else if ((dp_strncmpi(param_list[2], "del",
					strlen("del") + 1) == 0) ||
					(dp_strncmpi(param_list[2], "add",
					strlen("add")+ 1) == 0)) {
			int param_val;

			if (!param_list[3]) {
				PR_ERR("meterid NULLL\n");
				return count;
			}
			param_val = dp_atoi(param_list[3]);
			if (param_val < 0) {
				PR_ERR("meterid less then 0");
				return count;
			}
			meter.meter_id = param_val;
			if (param_list[4])
				meter_flag = dp_atoi(param_list[4]);
			if (param_list[5])
				meter.dir = dp_atoi(param_list[5]);
			if (param_list[6])
				meter.dp_pce.flow = dp_atoi(param_list[6]);
			if (param_list[7])
				meter.cir = dp_atoi(param_list[7]);
			if (param_list[8])
				meter.pir = dp_atoi(param_list[8]);
			if (param_list[9])
				meter.cbs = dp_atoi(param_list[9]);
			if (param_list[10])
				meter.pbs = dp_atoi(param_list[10]);
			if (param_list[11])
				meter.type = dp_atoi(param_list[11]);
			meter.mode = DP_PCP_8P0D;
			if (dp_strncmpi(param_list[2], "add",
					strlen("add")) == 0)
				ret = DP_METER_CFGAPI(inst, dp_meter_add, dev,
						      meter, meter_flag, &mtr_subif);
			else
				ret = DP_METER_CFGAPI(inst, dp_meter_del, dev,
						      meter, meter_flag, &mtr_subif);
			if (ret < 0) {
				PR_ERR("meter %s failed\n",
				       param_list[2]);
				return count;
			}
			PR_INFO("meterid:=%d %s success\n",
				meter.meter_id, param_list[2]);
		}
	} else {
		PR_INFO("Wrong Paramters\n");
		meter_create_help();
	}
	return count;
}

static struct dp_proc_entry dp_proc_entries[] = {
	/*name single_callback_t multi_callback_t/_start write_callback_t */
#if defined(CONFIG_LTQ_DATAPATH_DBG) && CONFIG_LTQ_DATAPATH_DBG
	{PROC_DBG, proc_dbg_read, NULL, NULL, proc_dbg_write},
#endif
	{PROC_PORT, NULL, proc_port_dump, proc_port_init, proc_port_write},
	{PROC_MEM, NULL, NULL, NULL, proc_write_mem},
	{PROC_DT, NULL, NULL, NULL, proc_dt_write},
	{PROC_LOGICAL_DEV, NULL, NULL, NULL, proc_logical_dev_write},
	{PROC_INST_DEV, NULL, proc_inst_dev_dump, proc_inst_dev_start, NULL},
	{PROC_INST_MOD, NULL, proc_inst_mod_dump, proc_inst_mod_start, NULL},
	{PROC_INST_HAL, NULL, proc_inst_hal_dump, NULL, NULL},
	{PROC_INST, NULL, proc_inst_dump, NULL, NULL},
	{PROC_TX_PKT, NULL, NULL, NULL, proc_tx_pkt},
	{PROC_QOS, NULL, qos_dump, qos_dump_start, proc_qos_write},
	{PROC_ASYM_VLAN, NULL, NULL, NULL, proc_asym_vlan},
	{PROC_METER, NULL, NULL, NULL, proc_meter_write},

	/*the last place holder */
	{NULL, NULL, NULL, NULL, NULL}
};

struct dentry *dp_proc_node;
EXPORT_SYMBOL(dp_proc_node);

struct dentry *dp_proc_install(void)
{
	dp_proc_node = debugfs_create_dir(DP_PROC_PARENT DP_PROC_NAME, NULL);

	if (dp_proc_node) {
		int i;

		for (i = 0; i < ARRAY_SIZE(dp_proc_entries); i++)
			dp_proc_entry_create(dp_proc_node,
					     &dp_proc_entries[i]);
	} else {
		PR_ERR("datapath cannot create proc entry");
		return NULL;
	}

	return dp_proc_node;
}

