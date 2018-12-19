/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/types.h>	/* size_t */
#include <linux/timer.h>
#include <linux/cpufreq.h>
#include <net/datapath_api.h>
#include <net/datapath_proc_api.h>
#include "../datapath.h"
#include "datapath_misc.h"

#define DP_MODULE  LTQ_CPUFREQ_MODULE_DP
#define DP_ID 0	 /* this ID should represent the Datapath interface No. */
#define LTQ_CPUFREQ_PS_D0 0
#define LTQ_CPUFREQ_PS_D3 3
static spinlock_t dp_coc_lock;

/* driver is busy and needs highest performance */
static int dp_coc_init_stat;	/*DP COC is Initialized or not */
static int dp_coc_ena;		/*DP COC is enabled or not */
int dp_coc_ps_curr = -1;/*current state*/

int inst = 0;
/*meter */
#define PCE_OVERHD 20
static u32 meter_id;
/*3 ~ 4 packet size */
static u32 meter_ncbs = 0x8000 + (1514 + PCE_OVERHD) * 3 + 200;
/*1 ~ 2 packet size */
static u32 meter_nebs = 0x8000 + (1514 + PCE_OVERHD) * 1 + 200;
/*k bits */
static u32 meter_nrate[2] = { 0/*D0 */, 500/*D3*/};

static int apply_meter_rate(u32 rate, unsigned int new_state);

int dp_set_meter_rate(int stat, unsigned int rate)
{/*set the rate for upscaling to D0 from specified stat */
	if (stat == LTQ_CPUFREQ_PS_D3)
		meter_nrate[1] = rate;
	else
		return -1;
	//if (dp_coc_ps_curr == stat)
		apply_meter_rate(-1, stat);
	return 0;
}

static inline void coc_lock(void)
{
	if (unlikely(in_irq())) {
		PR_ERR("Not allowed to call COC API in_irq mode\n");
		return;
	}
	spin_lock_bh(&dp_coc_lock);
}

static inline void coc_unlock(void)
{
	spin_unlock_bh(&dp_coc_lock);
}

static char *get_coc_stat_string(int stat)
{
	if (stat == LTQ_CPUFREQ_PS_D0)
		return "D0";
	else if (stat == LTQ_CPUFREQ_PS_D3)
		return "D3";
	else
		return "Undef";
}

void proc_coc_read(struct seq_file *s)
{
	GSW_register_t reg;
	GSW_QoS_meterCfg_t meter_cfg;
	struct core_ops *gsw_handle;

	if (!capable(CAP_NET_ADMIN))
		return;
	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	seq_puts(s, "  Basic DP COC Info:\n");
	seq_printf(s, "    dp_coc_ena=%d @ 0x%p (DP %s)\n", dp_coc_ena,
		   &dp_coc_ena, dp_coc_ena ? "COC Enabled" : "COC Disabled");
	seq_printf(s, "    dp_coc_init_stat=%d @ %p (%s)\n", dp_coc_init_stat,
		   &dp_coc_init_stat,
		   dp_coc_init_stat ? "initialized ok" : "Not initialized");
	seq_printf(s, "    dp_coc_ps_curr=%d (%s) @ 0x%p\n", dp_coc_ps_curr,
		   get_coc_stat_string(dp_coc_ps_curr), &dp_coc_ps_curr);

	seq_puts(s, "    Metering Info:\n");
	/*PCE_OVERHD */
	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x46C;	/*PCE_OVERHD */
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	seq_printf(s, "    PCE_OVERHD=%d\n", reg.nData);

	meter_cfg.nMeterId = meter_id;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops.QoS_MeterCfgGet,
		     gsw_handle, &meter_cfg);
	seq_printf(s, "    meter id=%u (%s)\n", meter_id,
		   meter_cfg.bEnable ? "Enabled" : "Disabled");
	seq_printf(s, "    meter nCbs=%u\n", meter_cfg.nCbs);
	seq_printf(s, "    meter nEbs=%u\n", meter_cfg.nEbs);
	seq_printf(s, "    meter nRate=%u\n", meter_cfg.nRate);
	seq_printf(s, "    meter nPiRate=%u\n", meter_cfg.nPiRate);
	seq_printf(s, "    meter eMtrType=%u\n", (int)meter_cfg.eMtrType);
	seq_printf(s, "    D3 nRate=%u\n", meter_nrate[1]);

	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x489 + meter_id * 10;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	seq_printf(s, "    PCE_PISR(0x%x)=%u(0x%x)-interrupt %s\n",
		   reg.nRegAddr, reg.nData, reg.nData,
		   (reg.nData & 0x100) ? "on" : "off");

}

ssize_t proc_coc_write(struct file *file, const char *buf, size_t count,
		       loff_t *ppos)
{
	int len, num;
	char str[64];
	char *param_list[3];

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;
	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;

	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if (dp_strncmpi(param_list[0], "rate", strlen("rate") + 1) == 0) {
		/*meter rate */
		u32 rate = dp_atoi(param_list[1]);

		if (!rate) {
			PR_INFO("rate should not be zero\n");
			return count;
		}
		if (dp_strncmpi(param_list[0], "rate3", strlen("rate3") + 1) 
									== 0) {
			dp_set_meter_rate(LTQ_CPUFREQ_PS_D3, rate);

		} else {
			PR_INFO
			    ("Wrong COC state, it should be D3 only\n");
		}
	} else {
		goto help;
	}
	return count;
 help:
	PR_INFO("Datapath COC Proc Usage:\n");
	PR_INFO("  echo <ratex> <meter_rate_in_knps> /proc/dp/coc\n");
	PR_INFO("       Note:Valid x of range: 3\n");
	return count;
}

#if defined(DATAPATH_ENABLE_CPU)
int clear_meter_interrupt(void)
{
	GSW_register_t reg;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x489 + meter_id * 10;
	reg.nData = -1;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterSet,
		     gsw_handle, &reg);
	return 0;
}

int enable_meter_interrupt(void)
{
	GSW_register_t reg;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	/*#Enable PCE Interrupt
	 *  switch_cli GSW_REGISTER_SET nRegAddr=0x14 nData=0x2 dev=1
	 */
	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x14;	/*ETHSW_IER */
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	reg.nRegAddr |= 1 << 1; /*Enable PCEIE */
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterSet,
		     gsw_handle, &reg);

	/*#Enable PCE Port Interrupt
	 *  switch_cli GSW_REGISTER_SET nRegAddr=0x465 nData=0x1 dev=1
	 */
	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x465;	/*PCE_IER_0 */
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	reg.nRegAddr |= 1 << 0; /*Enable PCE Port 0 */
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterSet,
		     gsw_handle, &reg);

	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x488;	/*PCE_PIER */
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	/*Enable Metering Based Backpressure Status Change Interrupt Enable */
	reg.nRegAddr |= 1 << 8;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterSet,
		     gsw_handle, &reg);

	return 0;
}
#endif

/* rate      0: disable meter
 * -1: enable meter
 * others: really change rate.
 */
int apply_meter_rate(u32 rate, unsigned int new_state)
{
	GSW_QoS_meterCfg_t meter_cfg;
	struct core_ops *gsw_handle;

	DP_DEBUG(DP_DBG_FLAG_COC,
		 "rate=%d new state=%d\n", rate, new_state);
	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	memset(&meter_cfg, 0, sizeof(meter_cfg));
	meter_cfg.nMeterId = meter_id;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops.QoS_MeterCfgGet,
		     gsw_handle, &meter_cfg);
	if (rate == 0) {		/*only need to disable the meter */
		meter_cfg.bEnable = 0;
	} else if (rate == -1) {
		meter_cfg.bEnable = 1;
		/*set PAE metering */
		if (new_state == LTQ_CPUFREQ_PS_D3) {
			meter_cfg.nRate = meter_nrate[1];
		} else {
			meter_cfg.nRate = new_state;
		}
	} else {
		return -1;
	}

	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops.QoS_MeterCfgSet,
		     gsw_handle, &meter_cfg);

	return 0;
}

int meter_set_default(void)
{
	int i;
	GSW_QoS_WRED_PortCfg_t wred_p;
	GSW_QoS_WRED_QueueCfg_t wred_q;
	GSW_QoS_WRED_Cfg_t wred_cfg;
	GSW_QoS_meterCfg_t meter_cfg;
	GSW_QoS_meterPort_t meter_port;
	GSW_register_t reg;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	/*#currently directly change global setting, later should use
	 * GSW_QOS_WRED_PORT_CFG_SET instead
	 * switch_cli dev=1 GSW_REGISTER_SET nRegAddr=0x4a nData=0x518
	 */
	memset(&wred_p, 0, sizeof(wred_p));

	for (i = 0; i < PMAC_MAX_NUM; i++) {/*cp green setting to yellow/red*/
		wred_p.nPortId = i;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops
			     .QoS_WredPortCfgGet, gsw_handle,
			     &wred_p);
		wred_p.nYellow_Min = wred_p.nGreen_Min;
		wred_p.nYellow_Max = wred_p.nGreen_Max;
		wred_p.nRed_Min = wred_p.nGreen_Min;
		wred_p.nRed_Max = wred_p.nGreen_Max;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops
			     .QoS_WredPortCfgSet, gsw_handle,
			     &wred_p);
	}

	/*#Enable Meter 0, configure the rate
	 * switch_cli GSW_QOS_METER_CFG_SET bEnable=1
	 *	nMeterId=0 nCbs=0xA000 nEbs=0x9000 nRate=500 dev=1
	 */
	memset(&meter_cfg, 0, sizeof(meter_cfg));
	/*tmp.meter_cfg.bEnable = 1; */
	meter_cfg.nMeterId = meter_id;
	meter_cfg.nCbs = meter_ncbs;
	meter_cfg.nEbs = meter_nebs;
	meter_cfg.nRate = meter_nrate[1];
	meter_cfg.nPiRate = 0xFFFFFF; /* try to set maximum */
	meter_cfg.eMtrType = GSW_QOS_Meter_trTCM;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops.QoS_MeterCfgSet,
		     gsw_handle, &meter_cfg);

	/*#Assign Port0 ingress to Meter 0
	 * switch_cli GSW_QOS_METER_PORT_ASSIGN nMeterId=0 eDir=1
	 *	nPortIngressId=0 dev=1
	 */
	memset(&meter_port, 0, sizeof(meter_port));

	for (i = 0; i < PMAC_MAX_NUM; i++) {
		meter_port.nMeterId = meter_id;
		meter_port.eDir = 1;
		meter_port.nPortIngressId = i;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops
			     .QoS_MeterPortAssign, gsw_handle, &meter_port);
	}

	/*#Enable Port 0 Meter Based Flow control (Bit 2 MFCEN)
	 *  switch_cli GSW_REGISTER_SET nRegAddr=0xBC0 nData=0x5107 dev=1
	 */
	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0xBC0;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	reg.nData |= 1 << 2;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterSet,
		     gsw_handle, &reg);

	/*#Configure Red and Yellow watermark for each queue
	 *  (Yellow and Red shall not be 0 in CoC case in order to avoid
	 *  packet drop)
	 *  i=0
	 *  while [$i -lt 32]
	 *  do
	 *  switch_cli GSW_QOS_WRED_QUEUE_CFG_SET nQueueId=$i nRed_Min=0x40
	 *  nRed_Max=0x40 nYellow_Min=0x40 nYellow_Max=0x40 nGreen_Min=0x40
	 *  nGreen_Max=0x40 dev=1
	 *  i=$(($i+1))
	 *  done
	 */
	memset(&wred_q, 0, sizeof(wred_q));
	for (i = 0; i < 32; i++) {	/*copy green setting to yellow/red */
		wred_q.nQueueId = i;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops
			     .QoS_WredQueueCfgGet, gsw_handle,
			     &wred_q);

		wred_q.nYellow_Min = wred_q.nGreen_Min;
		wred_q.nYellow_Max = wred_q.nGreen_Max;
		wred_q.nRed_Min = wred_q.nGreen_Min;
		wred_q.nRed_Max = wred_q.nGreen_Max;
		gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops
			     .QoS_WredQueueCfgSet, gsw_handle,
			     &wred_q);
	}

	/*Configure Red and Yellow watermark for each queue (Yellow and Red
	 * shall not be 0 in CoC case in order to avoid packet drop)
	 * switch_cli GSW_QOS_WRED_CFG_SET nRed_Min=255 nRed_Max=255
	 * nYellow_Min=255 nYellow_Max=255  nGreen_Min=255 nGreen_Max=255 dev=1
	 */
	memset(&wred_cfg, 0, sizeof(wred_cfg));
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops.QoS_WredCfgGet,
		     gsw_handle, &wred_cfg);
	wred_cfg.nYellow_Min = wred_cfg.nGreen_Min;
	wred_cfg.nYellow_Max = wred_cfg.nGreen_Max;
	wred_cfg.nRed_Min = wred_cfg.nGreen_Min;
	wred_cfg.nRed_Max = wred_cfg.nGreen_Max;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_qos_ops.QoS_WredCfgSet,
		     gsw_handle, &wred_cfg);

	/*#Configure OVERHEAD */
	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x46C;	/*PCE_OVERHD */
	reg.nData = PCE_OVERHD;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterSet,
		     gsw_handle, &reg);

	return 0;
}

static int dp_coc_prechange(struct cpufreq_freqs *freq)
{
	int res = -1;

	/*check whether can be switched or not
	 * and accept the request 
	 */
	if (!dp_coc_init_stat || !dp_coc_ena) {
		res = 0;
	}
	DP_DEBUG(DP_DBG_FLAG_COC,
	         "dp_coc_prechange:to switch from %d to %d\n",
		 freq->old, freq->new);

	return res;
}

static int dp_coc_postchange(struct cpufreq_freqs *freq)
{
	if (!dp_coc_init_stat || !dp_coc_ena)
		return 0;

	coc_lock();
	if (dp_coc_ps_curr > freq->new) {/*changing to low freq, enable meter*/
		DP_DEBUG(DP_DBG_FLAG_COC, "enable meter new_freq=%d\n",
			 freq->new);
		apply_meter_rate(-1, freq->new);
	} else if (dp_coc_ps_curr < freq->new) { /*changing to high freq, disable meter*/
		DP_DEBUG(DP_DBG_FLAG_COC, "disable meter new_freq=%d\n",
			 freq->new);
		apply_meter_rate(0, 0);
	} 
	dp_coc_ps_curr = freq->new;
	coc_unlock();

	return 0;
}

static int dp_coc_policy_notify(struct cpufreq_policy *policy)
{
	if (dp_coc_ps_curr == -1) {
		dp_coc_ps_curr = policy->max;
	} else if (dp_coc_ps_curr <= policy->min) {
		/*No down scaling allowed, limit the frequency to max */
		cpufreq_verify_within_limits(policy, policy->max, policy->max);
	} else {
		return NOTIFY_OK;
	}

	return NOTIFY_OK;
}

int dp_handle_cpufreq_event_30(int event_id, void *cfg)
{
	int res = DP_FAILURE;

	if (!cfg)
		return res;

	switch (event_id) {
	case PRE_CHANGE:
		res = dp_coc_prechange((struct cpufreq_freqs *)cfg);
		break;
	case POST_CHANGE:
		res = dp_coc_postchange((struct cpufreq_freqs *)cfg);
		break;
	case POLICY_NOTIFY:
		res = dp_coc_policy_notify((struct cpufreq_policy *)cfg);
		break;
	default:
		PR_ERR("no support for %d\n", event_id);
		break;
	}
	return res;
}

void dp_meter_interrupt_cb(void *param)
{
	DP_DEBUG(DP_DBG_FLAG_COC, "triggered meter intr\n");

	cpufreq_update_policy(0);
	return;
}

int dp_coc_cpufreq_init(void)
{
	GSW_Irq_Op_t irq;
	struct core_ops *gsw_handle;
	pr_debug("enter dp_coc_cpufreq_init\n");

	spin_lock_init(&dp_coc_lock);
	dp_coc_init_stat = 0;
	dp_coc_ena = 0;
        gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	cpufreq_update_policy(0);
	meter_set_default();
	irq.blk = PCE;
	irq.event = PCE_METER_EVENT;
	irq.portid = 0; // logical port id
	irq.call_back = dp_meter_interrupt_cb;// Callback API
	irq.param = NULL; // Callback  API parameter
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_irq_ops.IRQ_Register,
		     gsw_handle, &irq);
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_irq_ops.IRQ_Enable,
		     gsw_handle, &irq);

	dp_coc_init_stat = 1;
	dp_coc_ena = 1;
	pr_debug("Register DP to CPUFREQ successfully.\n");
	return 0;
}

int dp_coc_cpufreq_exit(void)
{
	if (dp_coc_init_stat) {

		coc_lock();
		dp_coc_init_stat = 0;
		dp_coc_ena = 0;
		coc_unlock();
	}

	return 0;
}
