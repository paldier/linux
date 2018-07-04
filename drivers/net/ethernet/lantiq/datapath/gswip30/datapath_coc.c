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
#include <net/datapath_api.h>
#include <net/datapath_proc_api.h>
#include "../datapath.h"

#define DP_MODULE  LTQ_CPUFREQ_MODULE_DP
#define DP_ID 0	 /* this ID should represent the Datapath interface No. */
static struct timer_list dp_coc_timer;
static u32 polling_period;	/*seconds */
static int rmon_timer_en;
static spinlock_t dp_coc_lock;

/* threshold data for D0:D3 */
struct ltq_cpufreq_threshold rmon_threshold = { 0, 30, 20, 10 }; /*packets */

/* driver is busy and needs highest performance */
static int dp_coc_init_stat;	/*DP COC is Initialized or not */
static int dp_coc_ena;		/*DP COC is enabled or not */
enum ltq_cpufreq_state dp_coc_ps_curr = LTQ_CPUFREQ_PS_UNDEF;/*current state*/
/*new statue wanted to switch to */
enum ltq_cpufreq_state dp_coc_ps_new = LTQ_CPUFREQ_PS_UNDEF;

static GSW_RMON_Port_cnt_t rmon_last[PMAC_MAX_NUM];
static u64 last_rmon_rx;

/*meter */
#define PCE_OVERHD 20
static u32 meter_id;
/*3 ~ 4 packet size */
static u32 meter_ncbs = 0x8000 + (1514 + PCE_OVERHD) * 3 + 200;
/*1 ~ 2 packet size */
static u32 meter_nebs = 0x8000 + (1514 + PCE_OVERHD) * 1 + 200;
/*k bits */
static u32 meter_nrate[4] = { 0/*D0 */, 700/*D1*/, 600/*D2*/, 500/*D3*/};

static int dp_coc_cpufreq_notifier(struct notifier_block *nb,
				   unsigned long val, void *data);
static int dp_coc_stateget(enum ltq_cpufreq_state *state);
static int dp_coc_fss_ena(int ena);
static int apply_meter_rate(u32 rate, enum ltq_cpufreq_state new_state);
static int enable_meter_interrupt(void);
static int clear_meter_interrupt(void);
int dp_set_meter_rate(enum ltq_cpufreq_state stat, unsigned int rate)
{/*set the rate for upscaling to D0 from specified stat */
	if (stat == LTQ_CPUFREQ_PS_D1)
		meter_nrate[1] = rate;
	else if (stat == LTQ_CPUFREQ_PS_D2)
		meter_nrate[2] = rate;
	else if (stat == LTQ_CPUFREQ_PS_D3)
		meter_nrate[3] = rate;
	else
		return -1;
	if (dp_coc_ps_curr == stat)
		apply_meter_rate(-1, stat);
	return 0;
}
EXPORT_SYMBOL(dp_set_meter_rate);

static struct notifier_block dp_coc_cpufreq_notifier_block = {
	.notifier_call = dp_coc_cpufreq_notifier
};

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

struct ltq_cpufreq_module_info dp_coc_feature_fss = {
	.name = "Datapath FSS",
	.pmcuModule = DP_MODULE,
	.pmcuModuleNr = DP_ID,
	.powerFeatureStat = 1,
	.ltq_cpufreq_state_get = dp_coc_stateget,
	.ltq_cpufreq_pwr_feature_switch = dp_coc_fss_ena,
};

#if defined(CONFIG_LTQ_DATAPATH_DBG) && CONFIG_LTQ_DATAPATH_DBG
static char *get_sub_module_str(uint32_t flag)
{
	if (flag == DP_COC_REQ_DP)
		return "DP";
	else if (flag == DP_COC_REQ_ETHERNET)
		return "Ethernet";
	else if (flag == DP_COC_REQ_VRX318)
		return "VRX318";
	else
		return "Unknown";
}
#endif

static char *get_coc_stat_string(enum ltq_cpufreq_state stat)
{
	if (stat == LTQ_CPUFREQ_PS_D0)
		return "D0";
	else if (stat == LTQ_CPUFREQ_PS_D1)
		return "D1";
	else if (stat == LTQ_CPUFREQ_PS_D2)
		return "D2";
	else if (stat == LTQ_CPUFREQ_PS_D3)
		return "D3";
	else if (stat == LTQ_CPUFREQ_PS_D0D3)
		return "D0D3-NotCare";
	else if (stat == LTQ_CPUFREQ_PS_BOOST)
		return "Boost";
	else
		return "Undef";
}

static void dp_rmon_polling(unsigned long data)
{
	GSW_RMON_Port_cnt_t curr;
	int i;
	u64 rx = 0;

	for (i = 0; i < PMAC_MAX_NUM; i++) {
		memset(&curr, 0, sizeof(curr));
		get_gsw_port_rmon(i, GSWIP_R, &curr);

		coc_lock();
		/*wrapround handling */
		if (curr.nRxGoodPkts >= rmon_last[i].nRxGoodPkts)
			rx += curr.nRxGoodPkts - rmon_last[i].nRxGoodPkts;
		else
			rx +=
			    (u64)0xFFFFFFFF + (u64)curr.nRxGoodPkts -
			    rmon_last[i].nRxGoodPkts;

		if (curr.nRxDroppedPkts >= rmon_last[i].nRxDroppedPkts)
			rx +=
			    curr.nRxDroppedPkts - rmon_last[i].nRxDroppedPkts;
		else
			rx +=
			    (u64)0xFFFFFFFF + (u64)curr.nRxDroppedPkts -
			    rmon_last[i].nRxDroppedPkts;

		memcpy(&rmon_last[i], &curr, sizeof(curr));
		coc_unlock();
	}
	last_rmon_rx = rx;
	if (dp_coc_ps_curr != LTQ_CPUFREQ_PS_UNDEF) {
		if (rx < rmon_threshold.th_d3) {
			if (dp_coc_new_stat_req
			    (LTQ_CPUFREQ_PS_D3, DP_COC_REQ_DP) == 0) {
				coc_lock();
				rmon_timer_en = 0;
				coc_unlock();
				DP_DEBUG(DP_DBG_FLAG_COC,
					 "Request to D3:rx (%u) < th_d3 %d\n",
					 (unsigned int)rx,
					 rmon_threshold.th_d3);
			} else
				DP_DEBUG(DP_DBG_FLAG_COC,
					 "req to D3 fail for dp_coc_new_stat_req\n");
		} else if (rx < rmon_threshold.th_d2) {
			if (dp_coc_new_stat_req
			    (LTQ_CPUFREQ_PS_D2, DP_COC_REQ_DP) == 0) {
				DP_DEBUG(DP_DBG_FLAG_COC,
					 "req to D2: rx (%u) < th_d2 %d\n",
					 (unsigned int)rx,
					 rmon_threshold.th_d2);
			} else
				DP_DEBUG(DP_DBG_FLAG_COC,
					 "req to D2 fail for dp_coc_new_stat_req\n");
		} else if (rx < rmon_threshold.th_d1) {
			if (dp_coc_new_stat_req
			    (LTQ_CPUFREQ_PS_D1, DP_COC_REQ_DP) == 0) {
				DP_DEBUG(DP_DBG_FLAG_COC,
					 "req to D1 since rx (%u) < th_d1 %d\n",
					 (unsigned int)rx,
					 rmon_threshold.th_d1);
			} else
				DP_DEBUG(DP_DBG_FLAG_COC,
					 "req to D1 fail: dp_coc_new_stat_req\n");
		} else
			DP_DEBUG(DP_DBG_FLAG_COC,
				 "Stat no change:rx(%u)>=any thresholds %d_%d_%d\n",
				 (unsigned int)rx, rmon_threshold.th_d3,
				 rmon_threshold.th_d2, rmon_threshold.th_d1);
	} else
		DP_DEBUG(DP_DBG_FLAG_COC,
			 "DP COC not get its initial power state yet\n");

	coc_lock();

	if (rmon_timer_en)
		mod_timer(&dp_coc_timer,
			  jiffies + msecs_to_jiffies(polling_period * 1000));
	else
		last_rmon_rx = 0;

	coc_unlock();
}

static int dp_coc_stateget(enum ltq_cpufreq_state *state)
{
	DP_DEBUG(DP_DBG_FLAG_COC, "dp_coc_stateget with %d(%s)\n",
		 dp_coc_ps_curr, get_coc_stat_string(dp_coc_ps_curr));
	coc_lock();
	*state = dp_coc_ps_curr;
	coc_unlock();
	return LTQ_CPUFREQ_RETURN_SUCCESS;
}

static int dp_coc_fss_ena(int ena)
{
	DP_DEBUG(DP_DBG_FLAG_COC,
		 "dp_coc_fss_ena: %d(%s frequency scaling)\n", ena,
		 ena ? "enable" : "disable");
	coc_lock();
	dp_coc_ena = ena;
	coc_unlock();
	return LTQ_CPUFREQ_RETURN_SUCCESS;
}

void update_rmon_last(void)
{
	int i;

	for (i = 0; i < PMAC_MAX_NUM; i++)
		get_gsw_port_rmon(i, GSWIP_R, &rmon_last[i]);
}

int update_coc_rmon_timer(enum ltq_cpufreq_state new_state, uint32_t flag)
{
	if (new_state == LTQ_CPUFREQ_PS_D0) {
		/*enable rmon timer */
		if (!rmon_timer_en)
			update_rmon_last();
		mod_timer(&dp_coc_timer,
			  jiffies + msecs_to_jiffies(polling_period * 1000));
		rmon_timer_en = 1;

		/*disable meter */
		apply_meter_rate(0, 0);
	} else if (new_state == LTQ_CPUFREQ_PS_D1 ||
		   new_state == LTQ_CPUFREQ_PS_D2) {
		/*enable rmon timer */
		if (!rmon_timer_en)
			update_rmon_last();
		mod_timer(&dp_coc_timer,
			  jiffies + msecs_to_jiffies(polling_period * 1000));
		rmon_timer_en = 1;

		/*enable meter, but first disable to fix red color issue
		 * if last already triggered
		 */
		apply_meter_rate(0, 0);
		apply_meter_rate(-1, new_state);	/*enable again */
	} else if (new_state == LTQ_CPUFREQ_PS_D3) {
		/*disable rmon timer */
		del_timer(&dp_coc_timer);
		rmon_timer_en = 0;
		last_rmon_rx = 0;

		/*enable meter */
		/*enable meter, but first disable to fix red color issue
		 * if last already triggered
		 */
		apply_meter_rate(0, 0);
		apply_meter_rate(-1, new_state);	/*enable again */
	}

	return 0;
}

static int update_coc_cfg(enum ltq_cpufreq_state new_state,
			  enum ltq_cpufreq_state old_state, uint32_t flag)
{
	update_coc_up_sub_module(new_state, old_state, flag);
	update_coc_rmon_timer(new_state, flag);
	return 0;
}

static int dp_coc_prechange(enum ltq_cpufreq_module module,
			    enum ltq_cpufreq_state new_state,
			    enum ltq_cpufreq_state old_state, u8 flags)
{
	int res = -1;

	/*check whether can be switched or not */
	if (!dp_coc_init_stat || !dp_coc_ena) {
		res = 0;
	} else if ((flags & CPUFREQ_PM_NO_DENY) ||
		   (dp_coc_ps_curr == LTQ_CPUFREQ_PS_UNDEF) ||
		   (dp_coc_ps_new == LTQ_CPUFREQ_PS_UNDEF) ||
		   (dp_coc_ps_new == LTQ_CPUFREQ_PS_D0D3)) { /*accept */
		res = 0;
	} else if (dp_coc_ps_new >= new_state) {
		/* Accept any upscale request */
		res = 0;
	}

	DP_DEBUG(DP_DBG_FLAG_COC,
		 "dp_coc_prechange:%s to switch from %s to %s\n",
		 res ? "Deny" : "Accept", get_coc_stat_string(old_state),
		 get_coc_stat_string(new_state));

	return res;
}

static int dp_coc_postchange(enum ltq_cpufreq_module module,
			     enum ltq_cpufreq_state new_state,
			     enum ltq_cpufreq_state old_state, u8 flags)
{
	if (!dp_coc_init_stat || !dp_coc_ena)
		return 0;

	DP_DEBUG(DP_DBG_FLAG_COC, "dp_coc_postchange:switch from %s to %s\n",
		 get_coc_stat_string(old_state),
		 get_coc_stat_string(new_state));

	coc_lock();
	dp_coc_ps_curr = new_state;
	dp_coc_ps_new = LTQ_CPUFREQ_PS_UNDEF;
	update_coc_cfg(new_state, old_state, flags);/*don't lock before it */
	coc_unlock();

	return 0;
}

/* keep track of frequency transitions */
static int dp_coc_cpufreq_notifier(struct notifier_block *nb,
				   unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	enum ltq_cpufreq_state new_state, old_state;

	new_state = ltq_cpufreq_get_ps_from_khz(freq->new);

	if (new_state == LTQ_CPUFREQ_PS_UNDEF) {
		DP_DEBUG(DP_DBG_FLAG_COC,
			 "dp_coc_cpufreq_notifier new_state=UNDEF with val=%u\n",
			 (unsigned int)val);
		return NOTIFY_STOP_MASK | (DP_MODULE << 4);
	}

	old_state = ltq_cpufreq_get_ps_from_khz(freq->old);

	if (old_state == LTQ_CPUFREQ_PS_UNDEF) {
		DP_DEBUG(DP_DBG_FLAG_COC,
			 "new_state=%s old_state=UNDEF with val=%u\n",
			 get_coc_stat_string(new_state), (unsigned int)val);
		return NOTIFY_STOP_MASK | (DP_MODULE << 4);
	}

	if (val == CPUFREQ_PRECHANGE) {
		DP_DEBUG(DP_DBG_FLAG_COC,
			 "dp_coc_cpufreq_notifier prechange from %s to %s\n",
			 get_coc_stat_string(old_state),
			 get_coc_stat_string(new_state));

		if (dp_coc_prechange
		    (DP_MODULE, new_state, old_state, freq->flags))
			return NOTIFY_STOP_MASK | (DP_MODULE << 4);
	} else if (val == CPUFREQ_POSTCHANGE) {
		DP_DEBUG(DP_DBG_FLAG_COC,
			 "dp_coc_cpufreq_notifier postchange from %s to %s\n",
			 get_coc_stat_string(old_state),
			 get_coc_stat_string(new_state));

		if (dp_coc_postchange
		    (DP_MODULE, new_state, old_state, freq->flags))
			return NOTIFY_STOP_MASK | (DP_MODULE << 4);
	}

	return NOTIFY_OK | (DP_MODULE << 4);
}

void proc_coc_read(struct seq_file *s)
{
	GSW_register_t reg;
	GSW_QoS_meterCfg_t meter_cfg;
	struct core_ops *gsw_handle;

	gsw_handle = dp_port_prop[inst].ops[GSWIP_R];
	seq_puts(s, "  Basic DP COC Info:\n");
	seq_printf(s, "    dp_coc_ena=%d @ 0x%p (DP %s)\n", dp_coc_ena,
		   &dp_coc_ena, dp_coc_ena ? "COC Enabled" : "COC Disabled");
	seq_printf(s, "    Rmon timer interval: %u sec (Timer %s)\n",
		   (unsigned int)polling_period,
		   rmon_timer_en ? "enabled" : "disabled");
	/*seq_printf(s, "    RMON D0 Threshold: %d\n", rmon_threshold.th_d0); */
	seq_printf(s, "    RMON D1 Threshold: %d\n", rmon_threshold.th_d1);
	seq_printf(s, "    RMON D2 Threshold: %d\n", rmon_threshold.th_d2);
	seq_printf(s, "    RMON D3 Threshold: %d\n", rmon_threshold.th_d3);
	seq_printf(s, "    dp_coc_init_stat=%d @ %p (%s)\n", dp_coc_init_stat,
		   &dp_coc_init_stat,
		   dp_coc_init_stat ? "initialized ok" : "Not initialized");
	seq_printf(s, "    dp_coc_ps_curr=%d (%s) @ 0x%p\n", dp_coc_ps_curr,
		   get_coc_stat_string(dp_coc_ps_curr), &dp_coc_ps_curr);
	seq_printf(s, "    dp_coc_ps_new=%d (%s)@ 0x%p\n", dp_coc_ps_new,
		   get_coc_stat_string(dp_coc_ps_new), &dp_coc_ps_new);
	seq_printf(s, "    last_rmon_rx=%llu pkts@ 0x%p (%s)\n", last_rmon_rx,
		   &last_rmon_rx, rmon_timer_en ? "Valid" : "Not valid");

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
	seq_printf(s, "    D1 nRate=%u\n", meter_nrate[1]);
	seq_printf(s, "    D2 nRate=%u\n", meter_nrate[2]);
	seq_printf(s, "    D3 nRate=%u\n", meter_nrate[3]);

	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0x489 + meter_id * 10;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	seq_printf(s, "    PCE_PISR(0x%x)=%u(0x%x)-interrupt %s\n",
		   reg.nRegAddr, reg.nData, reg.nData,
		   (reg.nData & 0x100) ? "on" : "off");

	memset(&reg, 0, sizeof(reg));
	reg.nRegAddr = 0xE11;
	gsw_core_api((dp_gsw_cb)gsw_handle->gsw_common_ops.RegisterGet,
		     gsw_handle, &reg);
	seq_printf(s, "    GSW_PCE_TCM_STAT(0x%x)=%u(0x%x)-backpress %s\n",
		   reg.nRegAddr, reg.nData, reg.nData,
		   (reg.nData & 1) ? "on" : "off");
}

int dp_set_rmon_threshold(struct ltq_cpufreq_threshold *threshold,
			  uint32_t flags)
{
	if (!threshold)
		return -1;

	memcpy((void *)&rmon_threshold, (void *)threshold,
	       sizeof(rmon_threshold));
	return 0;
}
EXPORT_SYMBOL(dp_set_rmon_threshold);

ssize_t proc_coc_write(struct file *file, const char *buf, size_t count,
		       loff_t *ppos)
{
	int len, num;
	char str[64];
	char *param_list[3];
#define MIN_POLL_TIME 1

	len = (sizeof(str) > count) ? count : sizeof(str) - 1;
	len -= copy_from_user(str, buf, len);
	str[len] = 0;

	num = dp_split_buffer(str, param_list, ARRAY_SIZE(param_list));
	if (dp_strncmpi(param_list[0], "timer", strlen("timer")) == 0) {
		polling_period = dp_atoi(param_list[1]);

		if (polling_period < MIN_POLL_TIME)
			polling_period = MIN_POLL_TIME;

		coc_lock();

		if (rmon_timer_en) {
			mod_timer(&dp_coc_timer,
				  jiffies +
				  msecs_to_jiffies(polling_period * 1000));
		}

		coc_unlock();
	} else if (dp_strncmpi(param_list[0],
			"threshold0", strlen("threshold0")) == 0) {
		coc_lock();
		rmon_threshold.th_d0 = dp_atoi(param_list[1]);

		if (!rmon_threshold.th_d0)
			rmon_threshold.th_d0 = 1;

		coc_unlock();
	} else if (dp_strncmpi(param_list[0],
			"threshold1", strlen("threshold1")) == 0) {
		coc_lock();
		rmon_threshold.th_d1 = dp_atoi(param_list[1]);

		if (!rmon_threshold.th_d1)
			rmon_threshold.th_d1 = 1;

		coc_unlock();
	} else if (dp_strncmpi(param_list[0],
			"threshold2", strlen("threshold2")) == 0) {
		coc_lock();
		rmon_threshold.th_d2 = dp_atoi(param_list[1]);

		if (!rmon_threshold.th_d2)
			rmon_threshold.th_d2 = 1;

		coc_unlock();
	} else if (dp_strncmpi(param_list[0],
			"threshold3", strlen("threshold3")) == 0) {
		coc_lock();
		rmon_threshold.th_d3 = dp_atoi(param_list[1]);

		if (!rmon_threshold.th_d3)
			rmon_threshold.th_d3 = 1;

		coc_unlock();
	} else if (dp_strncmpi(param_list[0], "D0", 2) == 0) {
		dp_coc_new_stat_req(LTQ_CPUFREQ_PS_D0, DP_COC_REQ_DP);
	} else if (dp_strncmpi(param_list[0], "D1", 2) == 0) {
		dp_coc_new_stat_req(LTQ_CPUFREQ_PS_D1, DP_COC_REQ_DP);
	} else if (dp_strncmpi(param_list[0], "D2", 2) == 0) {
		dp_coc_new_stat_req(LTQ_CPUFREQ_PS_D2, DP_COC_REQ_DP);
	} else if (dp_strncmpi(param_list[0], "D3", 2) == 0) {
		dp_coc_new_stat_req(LTQ_CPUFREQ_PS_D3, DP_COC_REQ_DP);
	} else if (dp_strncmpi(param_list[0], "rate", strlen("rate")) == 0) {
		/*meter rate */
		u32 rate = dp_atoi(param_list[1]);

		if (!rate) {
			PR_INFO("rate should not be zero\n");
			return count;
		}
		if (dp_strncmpi(param_list[0], "rate1", strlen("rate1")) == 0) {
			dp_set_meter_rate(LTQ_CPUFREQ_PS_D1, rate);

		} else if (dp_strncmpi(param_list[0],
				"rate2",
				strlen("rate2")) == 0) {
			dp_set_meter_rate(LTQ_CPUFREQ_PS_D2, rate);
		} else if ((dp_strncmpi(param_list[0],
				"rate3",
				strlen("rate3")) == 0) ||
			   (dp_strncmpi(param_list[0],
			   "rate",
			   strlen("rate")) == 0)) { /*back-compatiable */
			dp_set_meter_rate(LTQ_CPUFREQ_PS_D3, rate);
		} else {
			PR_INFO
			    ("Wrong COC state, it should be D1/D2/D3 only\n");
		}
	} else if (dp_strncmpi(param_list[0],
			"interrupt", strlen("interrupt")) == 0) {/*meter */
		enable_meter_interrupt();
		PR_INFO("Enabled meter interurpt\n");
	} else if (dp_strncmpi(param_list[0],
			"clear", strlen("clear")) == 0) {	/*meter */
		clear_meter_interrupt();
		PR_INFO("Clear meter interurpt src\n");
	} else {
		goto help;
	}
	return count;

 help:
	PR_INFO("Datapath COC Proc Usage:\n");
	PR_INFO("  echo timer polling_interval_in_seconds > /proc/dp/coc\n");
	PR_INFO("  echo <thresholdx> its_threshold_value > /proc/dp/coc\n");
	PR_INFO("       Note:Valid x of ranage: 1 2 3\n");
	PR_INFO
	    ("            For downscale to D<x> if rmon<threshold<x>'s cfg\n");
	PR_INFO("            threshold1's >= threshold'2 > threshold'3\n");
	PR_INFO("  echo <ratex> <meter_rate_in_knps> /proc/dp/coc\n");
	PR_INFO("       Note:Valid x of ranage: 1 2 3\n");
	PR_INFO
	  ("            For upscale to D0 from D<x> if rmon>=rate<x>'s cfg\n");
	PR_INFO("            Rate1's >= Rate2's > D3's threshold\n");
	PR_INFO
	  ("  echo interrupt > /proc/dp/coc:enable/disable meter interrupt\n");
	PR_INFO("  echo clear > /proc/dp/coc  :clear meter interrupt\n");
	return count;
}

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

/* rate      0: disable meter
 * -1: enable meter
 * others: really change rate.
 */
int apply_meter_rate(u32 rate, enum ltq_cpufreq_state new_state)
{
	GSW_QoS_meterCfg_t meter_cfg;
	struct core_ops *gsw_handle;

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
		if (new_state == LTQ_CPUFREQ_PS_D1) {
			meter_cfg.nRate = meter_nrate[1];
		} else if (new_state == LTQ_CPUFREQ_PS_D2) {
			meter_cfg.nRate = meter_nrate[2];
		} else if (new_state == LTQ_CPUFREQ_PS_D3) {
			meter_cfg.nRate = meter_nrate[3];
		} else {
			DP_DEBUG(DP_DBG_FLAG_COC,
				 "Why still try to enable meter with status %s\n",
				 get_coc_stat_string(dp_coc_ps_curr));

			return -1;
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
	meter_cfg.nRate = meter_nrate[3];
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
			     &tmp);
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

/* For Datapth's sub-module to request power state change, esp for
 *  Ethernet/VRX318 driver to call it if there is state change needed.
 *   The flag can be:
 *     DP_COC_REQ_DP
 *     DP_COC_REQ_ETHERNET
 *     DP_COC_REQ_VRX318
 */
int dp_coc_new_stat_req(enum ltq_cpufreq_state new_state, uint32_t flag)
{
	int ret;

	DP_DEBUG(DP_DBG_FLAG_COC,
		 "SubModular [%s] requesting to switch from %s to %s\n",
		 get_sub_module_str(flag),
		 get_coc_stat_string(dp_coc_ps_curr),
		 get_coc_stat_string(new_state));

	if (unlikely(in_irq())) {
		PR_ERR
		    ("Not allowed to cal dp_coc_new_stat_req in_irq mode\n");
		return -1;
	}

	if (!dp_coc_init_stat) {
		PR_ERR("COC is not initialized yet in DP\n");
		return -1;
	}

	if (!dp_coc_ena) {
		PR_ERR("COC is not enabled in DP yet\n");
		return -1;
	}

	coc_lock();

	if (dp_coc_ps_curr == new_state) {
		/*Workaround: if no change but this API still is called,
		 *it means need to update interrupt enable/disable status
		 */
		DP_DEBUG(DP_DBG_FLAG_COC, "No change\n");
		update_coc_cfg(new_state, new_state, flag);
		coc_unlock();
		return 0;
	}

	dp_coc_ps_new = new_state;
	coc_unlock();

	DP_DEBUG(DP_DBG_FLAG_COC, "ltq_cpufreq_state_req(%d,%d,%d)\n",
		 DP_MODULE, DP_ID, new_state);
	ret = ltq_cpufreq_state_req(DP_MODULE, DP_ID, new_state);

	if (ret != LTQ_CPUFREQ_RETURN_SUCCESS) {
		DP_DEBUG(DP_DBG_FLAG_COC,
			 "Power stat req to %d(%s) fail via ltq_cpufreq_state_req?\n",
			 new_state, get_coc_stat_string(new_state));
		DP_DEBUG(DP_DBG_FLAG_COC, "return value: %d ??\n", ret);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(dp_coc_new_stat_req);

int dp_coc_cpufreq_init(void)
{
	struct ltq_cpufreq *dp_coc_cpufreq_p;
	struct ltq_cpufreq_threshold *th_data;

	pr_debug("enter dp_coc_cpufreq_init\n");
	spin_lock_init(&dp_coc_lock);
	dp_coc_init_stat = 0;
	dp_coc_ena = 0;
	dp_coc_cpufreq_p = ltq_cpufreq_get();

	if (!dp_coc_cpufreq_p) {
		PR_ERR("dp_coc_cpufreq_init failed:ltq_cpufreq_get failed?\n");
		return -1;
	}

	if (cpufreq_register_notifier
	    (&dp_coc_cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER)) {
		PR_ERR("cpufreq_register_notifier failed?\n");
		return -1;
	}

	ltq_cpufreq_mod_list(&dp_coc_feature_fss.list, LTQ_CPUFREQ_LIST_ADD);

	th_data = ltq_cpufreq_get_threshold(DP_MODULE, DP_ID);
	if (th_data) {		/*copy threshold to local */
		rmon_threshold.th_d0 = th_data->th_d0;
		rmon_threshold.th_d1 = th_data->th_d1;
		rmon_threshold.th_d2 = th_data->th_d2;
		rmon_threshold.th_d3 = th_data->th_d3;
	}
	/*santity check */
	if (rmon_threshold.th_d2 < rmon_threshold.th_d3)
		rmon_threshold.th_d2 = rmon_threshold.th_d3 + 100;
	if (rmon_threshold.th_d1 < rmon_threshold.th_d2)
		rmon_threshold.th_d1 = rmon_threshold.th_d2 + 100;

	polling_period = ltq_cpufreq_get_poll_period(DP_MODULE, DP_ID);

	if (!polling_period)
		polling_period = 2;

	meter_set_default();
	/*Set to D0 Stage from the beginning */
	dp_coc_new_stat_req(LTQ_CPUFREQ_PS_D0, 0);
	init_timer_on_stack(&dp_coc_timer);
	dp_coc_timer.data = 0;
	dp_coc_timer.function = dp_rmon_polling;
	dp_coc_init_stat = 1;
	dp_coc_ena = 1;
	pr_debug("Register DP to CPUFREQ successfully.\n");
	return 0;
}

int dp_coc_cpufreq_exit(void)
{
	if (dp_coc_init_stat) {
		int ret;

		coc_lock();
		ret = del_timer(&dp_coc_timer);

		if (ret)
			PR_ERR("The timer is still in use...\n");

		dp_coc_new_stat_req(LTQ_CPUFREQ_PS_D0D3, 0);/*dont care mode */

		if (cpufreq_unregister_notifier
		    (&dp_coc_cpufreq_notifier_block,
		     CPUFREQ_TRANSITION_NOTIFIER))
			PR_ERR("CPUFREQ unregistration failed.");

		ltq_cpufreq_mod_list(&dp_coc_feature_fss.list,
				     LTQ_CPUFREQ_LIST_DEL);
		dp_coc_init_stat = 0;
		dp_coc_ena = 0;
		coc_unlock();
	}

	return 0;
}
