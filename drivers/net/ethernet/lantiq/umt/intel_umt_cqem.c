// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2015 Zhu YiXin <yixin.zhu@lantiq.com>
 * Copyright (C) 2016~2018 Intel Corporation.
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <linux/dma/lantiq_dmax.h>
#include <linux/intel_umt_cqem.h>
#include "intel_umt_cqem_reg.h"

static const u32 g_dma_ctrl = DMA1TX;
static void __iomem *g_umt_addr_base;

/* 0x17AC == 20us default translation according to register document*/
#define UMT_US_TO_CNT_DEFAULT_VALUE (0x17AC / 20)

#define umt_r32(x)	readl(g_umt_addr_base + (x))
#define umt_w32(x, y)	writel((x), g_umt_addr_base + (y))
#define umt_w32_mask(x, y, z) \
		do { \
			void __iomem *z_ = g_umt_addr_base + (z); \
			writel((readl(z_) & ~(x)) | (y), z_); \
		} while (0)

#define UMT_MSG(x)		(UMT_MSG0 + (x) * 4)

#define UMT_X_SW_MODE		(UMT_SW_MODE_CH1 - UMT_MSG1_0)
#define UMT_X_PERIOD		(UMT_PERIOD_CH1 - UMT_MSG1_0)
#define UMT_X_DEST		(UMT_DEST_1 - UMT_MSG1_0)
#define UMT_X_MSG(x)		((x) * 4)

#define UMT_X_ADDR(x, off)	(UMT_MSG1_0 + \
				((x) - 1) * (UMT_MSG2_0 - UMT_MSG1_0) + (off))

#define UMT_PORTS_NUM		3
#define MIN_UMT_PRD		20
#define UMT_DEF_DMACID		13

struct umt_port {
	u32 umt_pid;
	u32 ep_id;
	enum umt_mode umt_mode;
	enum umt_msg_mode msg_mode;
	enum umt_status status;
	u32 umt_period;
	u32 umt_dst;
	u32 cbm_pid; /* CBM WLAN ID (0 - 3) */
	u32 dma_cid; /* DMA Chan ID */
	enum umt_status suspend;
	spinlock_t umt_port_lock; /* protect the umt per port resource */
};

struct umt_info {
	struct umt_port ports[UMT_PORTS_NUM];
	u32 dma_ctrlid;
	enum umt_status status;
	struct dentry *debugfs;
	spinlock_t umt_lock; /* protect the umt global resource */
	struct clk *clk;
};

static struct umt_info sg_umt;

static inline void umt_set_mode(u32 umt_id, enum umt_mode umt_mode)
{
	u32 val, off;

	if (!umt_id) {
		umt_w32_mask(0x2, ((u32)umt_mode) << 1, UMT_GCTRL);
	} else {
		off = 16 + (umt_id - 1) * 3;
		val = umt_r32(UMT_GCTRL) & ~(BIT(off));
		umt_w32(val | (((u32)umt_mode) << off), UMT_GCTRL);
	}
}

static inline void umt_set_msgmode(u32 umt_id, enum umt_msg_mode msg_mode)
{
	if (!umt_id)
		umt_w32((u32)msg_mode, UMT_SW_MODE);
	else
		umt_w32((u32)msg_mode, UMT_X_ADDR(umt_id, UMT_X_SW_MODE));
}

/* input in term of microseconds */
static inline u32 umt_us_to_cnt(u32 usec)
{
	unsigned long cpuclk;
	struct clk *clk;
	unsigned long long usec_tmp;

	clk = sg_umt.clk;
	if (!clk)
		return usec * UMT_US_TO_CNT_DEFAULT_VALUE;

	cpuclk = clk_get_rate(clk);

	pr_debug("umt_us_to_cnt the cpuclk is %lu.\n", cpuclk);

	usec_tmp = (unsigned long long)usec *
			((unsigned long)cpuclk / 1000000ULL);
	if (usec_tmp > 0xFFFFFFFFULL) {
		pr_info("UMT period exceeds max value! set to 0xFFFFFFFF.\n");
		return 0xFFFFFFFF;
	}
	return (u32)usec_tmp;
}

static inline void umt_set_period(u32 umt_id, u32 umt_period)
{
	umt_period = umt_us_to_cnt(umt_period);

	if (!umt_id)
		umt_w32(umt_period, UMT_PERD);
	else
		umt_w32(umt_period, UMT_X_ADDR(umt_id, UMT_X_PERIOD));
}

static inline void umt_set_dst(u32 umt_id, u32 umt_dst)
{
	if (!umt_id)
		umt_w32(umt_dst, UMT_DEST);
	else
		umt_w32(umt_dst, UMT_X_ADDR(umt_id, UMT_X_DEST));
}

static inline void umt_set_mux(u32 umt_id, u32 cbm_pid, u32 dma_cid)
{
	u32 mux_sel;

	cbm_pid = cbm_pid & 0xF;
	dma_cid = dma_cid & 0xF;
	mux_sel = umt_r32(UMT_TRG_MUX) &
			(~((0xF000F) << (umt_id * 4)));
	mux_sel |= (dma_cid << (umt_id * 4)) |
			(cbm_pid << (16 + (umt_id * 4)));
	umt_w32(mux_sel, UMT_TRG_MUX);
}

static inline void umt_set_endian(int dw_swp, int byte_swp)
{
	u32 val;

	val = umt_r32(UMT_GCTRL);
	if (byte_swp)
		val |= UMT_GCTRL_OCP_UMT_ENDI_B_MASK;
	else
		val &= ~(UMT_GCTRL_OCP_UMT_ENDI_B_MASK);

	if (dw_swp)
		val |= UMT_GCTRL_OCP_UMT_ENDI_W_MASK;
	else
		val &= ~(UMT_GCTRL_OCP_UMT_ENDI_W_MASK);

	umt_w32(val, UMT_GCTRL);
}

static inline void umt_en_endian_mode(void)
{
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		umt_set_endian(1, 0);
	else
		umt_set_endian(1, 1);
}

static inline void umt_enable(u32 umt_id, enum umt_status status)
{
	u32 val, off;

	if (!umt_id) {
		umt_w32_mask(0x4, ((u32)status) << 2, UMT_GCTRL);
	} else {
		off = 17 + (umt_id - 1) * 3;
		val = (umt_r32(UMT_GCTRL) & ~BIT(off))
				| (((u32)status) << off);
		umt_w32(val, UMT_GCTRL);
	}
}

static inline void umt_suspend(u32 umt_id, enum umt_status status)
{
	u32 val;

	if (status)
		val = umt_r32(UMT_COUNTER_CTRL) | BIT(umt_id);
	else
		val = umt_r32(UMT_COUNTER_CTRL) & (~(BIT(umt_id)));

	umt_w32(val, UMT_COUNTER_CTRL);
}

/*This function will disable umt */
static inline void umt_reset_umt(u32 umt_id)
{
	u32 mode;

	umt_enable(umt_id, UMT_DISABLE);

	/* Bit 1 for UMT0
	 * Bit 16 for UMT1
	 * Bit 19 for UMT2
	 */
	if (!umt_id)
		mode = (umt_r32(UMT_GCTRL) & BIT(1));
	else
		mode = (umt_r32(UMT_GCTRL) & BIT(16 + 3 * (umt_id - 1)));

	umt_set_mode(umt_id, !mode);
	umt_set_mode(umt_id, mode);
}

/**
 * intput:
 * @umt_id: UMT port id, (0 - 3)
 * @ep_id:  Aligned with datapath lib ep_id
 * @period: measured in microseconds.
 * ret:  Fail < 0 / Success: 0
 */
int intel_umt_cqem_set_period(u32 umt_id, u32 ep_id, u32 period)
{
	struct umt_port *port;

	if (period < MIN_UMT_PRD) {
		pr_err("Period (%d) is below min requirement!\n", period);
		return -EINVAL;
	}

	if (umt_id >= UMT_PORTS_NUM) {
		pr_err("umit id (%d) is out of range !\n", umt_id);
		return -EINVAL;
	}

	if (sg_umt.status != UMT_ENABLE) {
		pr_err("UMT is not initialized!\n");
		return -EINVAL;
	}

	port = &sg_umt.ports[umt_id];

	spin_lock_bh(&port->umt_port_lock);
	if (port->ep_id != ep_id) {
		spin_unlock_bh(&port->umt_port_lock);
		pr_err("umt_id: %d, ep_id: %d, period: %d\n",
		       umt_id, ep_id, period);
		return -EINVAL;
	}

	if (port->umt_period != period) {
		port->umt_period = period;
		umt_set_period(umt_id, port->umt_period);
	}
	spin_unlock_bh(&port->umt_port_lock);

	return 0;
}
EXPORT_SYMBOL(intel_umt_cqem_set_period);

/**
 * API to configure the UMT port.
 * input:
 * @umt_id: (0 - 3)
 * @ep_id: aligned with datapath lib EP
 * @umt_mode:  0-self-counting mode, 1-user mode.
 * @msg_mode:  0-No MSG, 1-MSG0 Only, 2-MSG1 Only, 3-MSG0 & MSG1.
 * @dst:  Destination PHY address.
 * @period(ms): only applicable when set to self-counting mode.
 *              self-counting interval time. if 0, use the original setting.
 * @enable: 1-Enable/0-Disable
 * @ret:  Fail < 0 , SUCCESS:0
 */
int intel_umt_cqem_set_mode(u32 umt_id, u32 ep_id,
			    struct umt_set_mode *p_umt_mode)
{
	struct umt_port *port;
	u32 msg_mode;
	u32 umt_mode;
	u32 phy_dst;
	u32 period;
	u32 enable;
	u32 umt_ep_dst;

	if (!p_umt_mode) {
		pr_err("UMT mode is NULL!\n");
		return -EINVAL;
	}

	if (sg_umt.status != UMT_ENABLE) {
		pr_err("UMT is not initialized!!\n");
		return -ENODEV;
	}

	umt_mode = p_umt_mode->umt_mode;
	msg_mode = p_umt_mode->msg_mode;
	phy_dst = p_umt_mode->phy_dst;
	period = p_umt_mode->period;
	enable = p_umt_mode->enable;
	umt_ep_dst = p_umt_mode->umt_ep_dst;

	if ((umt_mode >= (u32)UMT_MODE_MAX) || (msg_mode >= (u32)UMT_MSG_MAX) ||
	    (enable >= (u32)UMT_STATUS_MAX) || (umt_ep_dst == 0) ||
	    (phy_dst == 0) || (period == 0) || (umt_id >= UMT_PORTS_NUM)) {
		pr_err("umt_id: %d, umt_mode: %d, msg_mode: %d.\n",
		       umt_id, umt_mode, msg_mode);
		pr_err("enable: %d, phy_dst: %d, umt_ep_dst: %d.\n",
		       enable, phy_dst, umt_ep_dst);
		return -EINVAL;
	}

	port = &sg_umt.ports[umt_id];

	spin_lock_bh(&port->umt_port_lock);
	if (port->ep_id != ep_id) {
		spin_unlock_bh(&port->umt_port_lock);
		pr_err("input ep_id: %d, port ep_id: %d\n", ep_id, port->ep_id);
		return -EINVAL;
	}

	umt_reset_umt(umt_id);

	port->umt_mode = (enum umt_mode)umt_mode;
	port->msg_mode = (enum umt_msg_mode)msg_mode;
	port->umt_dst = phy_dst;
	port->umt_period = period;
	port->status = (enum umt_status)enable;

	umt_set_mode(umt_id, port->umt_mode);
	umt_set_msgmode(umt_id, port->msg_mode);
	umt_set_dst(umt_id, port->umt_dst);
	umt_set_period(umt_id, port->umt_period);
	umt_enable(umt_id, port->status);
	/* setup the CBM/DMA mapping */
	spin_unlock_bh(&port->umt_port_lock);

	return 0;
}
EXPORT_SYMBOL(intel_umt_cqem_set_mode);

/**
 * API to enable/disable umt port
 * input:
 * @umt_id (0 - 3)
 * @ep_id: aligned with datapath lib EP
 * @enable: Enable: 1 / Disable: 0
 * ret:  Fail < 0, Success: 0
 */
int intel_umt_cqem_enable(u32 umt_id, u32 ep_id, u32 enable)
{
	struct umt_port *port;

	if (umt_id >= UMT_PORTS_NUM)
		return -EINVAL;
	if (enable >= (u32)UMT_STATUS_MAX || sg_umt.status != UMT_ENABLE)
		return -ENODEV;

	port = &sg_umt.ports[umt_id];

	spin_lock_bh(&port->umt_port_lock);
	if (port->ep_id != ep_id || port->umt_dst == 0 || port->ep_id == 0) {
		spin_unlock_bh(&port->umt_port_lock);
		pr_err("input ep_id: %d, umt port ep_id: %d, umt_dst: 0x%x\n",
		       ep_id, port->ep_id, port->umt_dst);
		return -EINVAL;
	}

	if (port->status != enable) {
		port->status = (enum umt_status)enable;
		umt_enable(umt_id, port->status);
	}
	spin_unlock_bh(&port->umt_port_lock);

	return 0;
}
EXPORT_SYMBOL(intel_umt_cqem_enable);

/**
 * API to suspend/resume umt US/DS counter
 * input:
 * @umt_id (0 - 3)
 * @ep_id: aligned with datapath lib EP
 * @enable: suspend: 1 / resume: 0
 * ret:  Fail < 0, Success: 0
 */
int intel_umt_cqem_suspend(u32 umt_id, u32 ep_id, u32 enable)
{
	struct umt_port *port;

	if (umt_id >= UMT_PORTS_NUM)
		return -EINVAL;
	if (enable >= (u32)UMT_STATUS_MAX || sg_umt.status != UMT_ENABLE)
		return -ENODEV;

	port = &sg_umt.ports[umt_id];

	spin_lock_bh(&port->umt_port_lock);
	if (port->ep_id != ep_id || port->umt_dst == 0 || port->ep_id == 0) {
		spin_unlock_bh(&port->umt_port_lock);
		pr_err("input ep_id: %d, umt port ep_id: %d, umt_dst: 0x%x\n",
		       ep_id, port->ep_id, port->umt_dst);
		return -EINVAL;
	}

	if (port->suspend != enable) {
		port->suspend = (enum umt_status)enable;
		umt_enable(umt_id, port->status);
		umt_suspend(umt_id, port->suspend);
	}
	spin_unlock_bh(&port->umt_port_lock);

	return 0;
}
EXPORT_SYMBOL(intel_umt_cqem_suspend);

/**
 * API to request and allocate UMT port
 * input:
 * @ep_id: aligned with datapath lib EP.
 * @cbm_pid: CBM Port ID(0-3), 0 - CBM port 4, 1 - CBM port 5,
 * 2 - CBM port 6
 * output:
 * @dma_ctrlid: DMA controller ID. aligned with DMA driver DMA controller ID
 * @dma_cid: DMA channel ID.
 * @umt_id: (0 - 3)
 * ret: Fail: < 0,  Success: 0
 */
int intel_umt_cqem_request(u32 ep_id, u32 cbm_pid,
			   u32 *dma_ctrlid, u32 *dma_cid, u32 *umt_id)
{
	int i, pid;
	struct umt_port *port;

	if (!dma_ctrlid || !dma_cid || !umt_id) {
		pr_err("Output pointer is NULL!\n");
		return -EINVAL;
	}

	if (sg_umt.status != UMT_ENABLE) {
		pr_err("UMT not initialized!\n");
		return -EINVAL;
	}
	if (!ep_id) {
		pr_err("%s: ep_id cannot be zero!\n", __func__);
		return -EINVAL;
	}

	if (cbm_pid >= UMT_PORTS_NUM) {
		pr_err("%s: cbm pid must be in ranage(0 - %d)\n",
		       __func__, UMT_PORTS_NUM);
		return -EINVAL;
	}

	pid = -1;
	spin_lock_bh(&sg_umt.umt_lock);
	for (i = 0; i < UMT_PORTS_NUM; i++) {
		port = &sg_umt.ports[i];
		spin_lock_bh(&port->umt_port_lock);
		if (port->ep_id == ep_id && port->cbm_pid == cbm_pid) {
			pid = i;
			spin_unlock_bh(&port->umt_port_lock);
			break;
		} else if (port->ep_id == 0 && pid == -1) {
			pid = i;
		}
		spin_unlock_bh(&port->umt_port_lock);
	}
	spin_unlock_bh(&sg_umt.umt_lock);

	if (pid < 0) {
		pr_err("No free UMT port!\n");
		return -ENODEV;
	}

	port = &sg_umt.ports[pid];
	spin_lock_bh(&port->umt_port_lock);
	port->ep_id = ep_id;
	port->cbm_pid = cbm_pid;
	umt_set_mux(port->umt_pid, port->cbm_pid, port->dma_cid);
	*dma_ctrlid = sg_umt.dma_ctrlid;
	*dma_cid = port->dma_cid;
	*umt_id = port->umt_pid;
	spin_unlock_bh(&port->umt_port_lock);

	return 0;
}
EXPORT_SYMBOL(intel_umt_cqem_request);

/**
 * API to release umt port
 * input:
 * @umt_id (0 - 3)
 * @ep_id: aligned with datapath lib EP
 *
 * ret:  Fail < 0, Success: 0
 */
int intel_umt_cqem_release(u32 umt_id, u32 ep_id)
{
	struct umt_port *port;

	if (umt_id >= UMT_PORTS_NUM)
		return -ENODEV;

	if (sg_umt.status != UMT_ENABLE) {
		pr_err("UMT is not initialized!\n");
		return -ENODEV;
	}

	port = &sg_umt.ports[umt_id];

	spin_lock_bh(&port->umt_port_lock);
	if (port->ep_id != ep_id) {
		spin_unlock_bh(&port->umt_port_lock);
		pr_err("input ep_id: %d, UMT port ep_id: %d\n",
		       ep_id, port->ep_id);
		return -ENODEV;
	}

	port->ep_id = 0;
	port->cbm_pid = 0;
	port->umt_dst = 0;
	port->umt_period = 0;
	port->status = UMT_DISABLE;
	umt_enable(port->umt_pid, UMT_DISABLE);

	spin_unlock_bh(&port->umt_port_lock);

	return 0;
}
EXPORT_SYMBOL(intel_umt_cqem_release);

static void umt_port_init(struct device_node *node, int pid)
{
	char res_cid[32];
	int cid;
	struct umt_port *port;

	port = &sg_umt.ports[pid];
	snprintf(res_cid, sizeof(res_cid), "lantiq,umt%d-dmacid", pid);
	if (of_property_read_u32(node, res_cid, &cid) < 0) {
		cid = UMT_DEF_DMACID + pid;
		pr_info("umt dma channel id not found in device tree!\n");
		pr_info("umt dma channel id set to default %d!\n", cid);
	}

	port->umt_pid = pid;
	port->dma_cid = cid;
	port->ep_id = 0;
	port->status = UMT_DISABLE;
	spin_lock_init(&port->umt_port_lock);
}

#ifdef CONFIG_DEBUG_FS
static void *umt_port_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= UMT_PORTS_NUM)
		return NULL;
	return &sg_umt.ports[*pos];
}

static void *umt_port_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	if (++*pos >= UMT_PORTS_NUM)
		return NULL;
	return &sg_umt.ports[*pos];
}

static void umt_port_seq_stop(struct seq_file *s, void *v)
{
}

static int umt_port_seq_show(struct seq_file *s, void *v)
{
	struct umt_port *port = v;
	int pid = port->umt_pid;
	u32 val;

	seq_printf(s, "\nUMT port %d configuration\n", pid);
	seq_puts(s, "-----------------------------------------\n");
	seq_printf(s, "UMT port ep_id: %d\n", port->ep_id);
	seq_printf(s, "UMT Mode: \t%s\n",
		   port->umt_mode == UMT_SELFCNT_MODE ?
		   "UMT SelfCounting Mode" : "UMT User Mode");
	switch (port->msg_mode) {
	case UMT_NO_MSG:
		seq_puts(s, "UMT MSG Mode: \tUMT NO MSG\n");
		break;
	case UMT_MSG0_ONLY:
		seq_puts(s, "UMT MSG Mode: \tUMT MSG0 Only\n");
		break;
	case UMT_MSG1_ONLY:
		seq_puts(s, "UMT MSG Mode: \tUMT MSG1 Only\n");
		break;
	case UMT_MSG0_MSG1:
		seq_puts(s, "UMT MSG Mode: \tUMT_MSG0_And_MSG1\n");
		break;
	default:
		seq_printf(s, "UMT MSG Mode Error! Msg_mode: %d\n",
			   port->msg_mode);
	}
	seq_printf(s, "UMT DST: \t0x%x\n", port->umt_dst);
	if (port->umt_mode == UMT_SELFCNT_MODE)
		seq_printf(s, "UMT Period: \t%d(us)\n", port->umt_period);
	seq_printf(s, "UMT Status: \t%s\n",
		   port->status == UMT_ENABLE ? "Enable" :
		   port->status == UMT_DISABLE ? "Disable" : "Init Fail");
	seq_printf(s, "UMT DMA CID: \t%d\n", port->dma_cid);
	seq_printf(s, "UMT CBM PID: \t%d\n", port->cbm_pid);

	seq_printf(s, "++++Register dump of umt port: %d++++\n", pid);
	if (pid == 0) {
		seq_printf(s, "UMT Status: \t%s\n",
			   (umt_r32(UMT_GCTRL) & BIT(2)) != 0 ?
			   "Enable" : "Disable");
		seq_printf(s, "UMT Mode: \t%s\n",
			   (umt_r32(UMT_GCTRL) & BIT(1)) != 0 ?
			   "UMT User MSG mode" : "UMT SelfCounting mode");
		seq_printf(s, "UMT MSG Mode: \t%d\n",
			   umt_r32(UMT_SW_MODE));
		seq_printf(s, "UMT Dst: \t0x%x\n",
			   umt_r32(UMT_DEST));
		seq_printf(s, "UMT Period: \t0x%x\n",
			   umt_r32(UMT_PERD));
		seq_printf(s, "UMT MSG0: \t0x%x\n",
			   umt_r32(UMT_MSG(0)));
		seq_printf(s, "UMT MSG1: \t0x%x\n",
			   umt_r32(UMT_MSG(1)));
	} else {
		seq_printf(s, "UMT Status: \t%s\n",
			   (umt_r32(UMT_GCTRL) &
			   BIT(17 + 3 * (pid - 1))) != 0 ?
			   "Enable" : "Disable");
		seq_printf(s, "UMT Mode: \t%s\n",
			   (umt_r32(UMT_GCTRL) &
			   BIT(16 + 3 * (pid - 1))) != 0 ?
			   "UMT User MSG mode" : "UMT SelfCounting mode");
		seq_printf(s, "UMT MSG Mode: \t%d\n",
			   umt_r32(UMT_X_ADDR(pid, UMT_X_SW_MODE)));
		seq_printf(s, "UMT Dst: \t0x%x\n",
			   umt_r32(UMT_X_ADDR(pid, UMT_X_DEST)));
		seq_printf(s, "UMT Period: \t0x%x\n",
			   umt_r32(UMT_X_ADDR(pid, UMT_X_PERIOD)));
		seq_printf(s, "UMT MSG0: \t0x%x\n",
			   umt_r32(UMT_X_ADDR(pid, UMT_X_MSG(0))));
		seq_printf(s, "UMT MSG1: \t0x%x\n",
			   umt_r32(UMT_X_ADDR(pid, UMT_X_MSG(1))));
	}

	val = umt_r32(UMT_TRG_MUX);
	seq_printf(s, "DMA CID: \t%d\n",
		   (val & ((0xF) << (pid * 4))) >> (pid * 4));
	seq_printf(s, "CBM PID: \t%d\n",
		   (val & ((0xF) << (16 + pid * 4))) >> (16 + pid * 4));

	return 0;
}

static const struct seq_operations umt_port_seq_ops = {
	.start = umt_port_seq_start,
	.next = umt_port_seq_next,
	.stop = umt_port_seq_stop,
	.show = umt_port_seq_show,
};

static int umt_cfg_read_debugfs_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &umt_port_seq_ops);
}

static const struct file_operations umt_debugfs_fops = {
	.open		= umt_cfg_read_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int umt_debugfs_init(void)
{
	struct dentry *entry;

	sg_umt.debugfs = debugfs_create_dir("umt", NULL);
	if (!sg_umt.debugfs)
		return -ENOMEM;

	entry = debugfs_create_file("umt_info", 0644, sg_umt.debugfs,
				    NULL, &umt_debugfs_fops);
	if (!entry)
		goto err1;

	return 0;
err1:
	debugfs_remove_recursive(sg_umt.debugfs);
	sg_umt.debugfs = NULL;
	pr_err("UMT debugfs create fail!\n");
	return -ENOMEM;
}
#endif

static int intel_umt_drv_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int i;
	struct resource *res;

	sg_umt.dma_ctrlid = g_dma_ctrl;
	sg_umt.clk = of_clk_get_by_name(node, "freq");

	if (IS_ERR_VALUE(sg_umt.clk)) {
		pr_err("the clock for umt is missing, error no %d.\n",
		       (int)sg_umt.clk);
		sg_umt.clk = NULL;
	}

	spin_lock_init(&sg_umt.umt_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("failed to get the umt resource!\n");
		return -ENODEV;
	}

	g_umt_addr_base = devm_ioremap_resource(&pdev->dev, res);
	if (!g_umt_addr_base) {
		pr_err("failed to request amd map the umt io range!\n");
		return -ENODEV;
	}
	pr_debug("the base addr of UMT is 0x%p\n", g_umt_addr_base);

	umt_en_endian_mode();

	for (i = 0; i < UMT_PORTS_NUM; i++)
		umt_port_init(node, i);
#ifdef CONFIG_DEBUG_FS
	umt_debugfs_init();
#endif
	sg_umt.status = UMT_ENABLE;

	pr_info("UMT initialize success on processor: %d!\n",
		smp_processor_id());

	return 0;
}

static int intel_umt_drv_remove(struct platform_device *pdev)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(sg_umt.debugfs);
	sg_umt.debugfs = NULL;
#endif
	sg_umt.status = UMT_DISABLE;
	pr_info("Intel UMT CQEM driver remove!\n");
	return 0;
}

static const struct of_device_id intel_umt_drv_match[] = {
	{ .compatible = "intel,umt-cqem" },
	{},
};
MODULE_DEVICE_TABLE(of, intel_umt__drv_match);

static struct platform_driver intel_umt_driver = {
	.probe = intel_umt_drv_probe,
	.remove = intel_umt_drv_remove,
	.driver = {
		.name = "intel,umt-cqem",
		.of_match_table = intel_umt_drv_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(intel_umt_driver);

MODULE_LICENSE("GPL v2");
