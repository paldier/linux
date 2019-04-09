// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2018 Intel Corporation.
 *  Zhu YiXin <Yixin.zhu@intel.com>
 */

//----define register and macro start
#define DP_UMT_NOT_SENDING_ZERO_COUNT	BIT(0)
#define DP_UMT_SENDING_RX_COUNT_ONLY	BIT(1)
#define DP_UMT_SUSPEND_SENDING_COUNT	BIT(2)
#define DP_UMT_ENABLE			BIT(3)

//----define register and macro end

//----define enum start
enum dp_umt_rx_src {
	DP_UMT_RX_FROM_CQEM,
	DP_UMT_RX_FROM_DMA
};

enum dp_umt_msg_mode {
	DP_UMT_SELFCNT_MODE = 0,
	DP_UMG_USER_MSG_MODE = 1,
};

enum dp_umt_sw_msg { /* for DP_UMG_USER_MSG_MODE only */
	DP_UMT_MSG0_ONLY = 0,
	DP_UMT_MSG1_ONLY,
	DP_UMT_MSG0_MSG1,
};

//----define enum end

//----define structure here start
/**
 * struct dp_umt_param
 * id: umt HW ID. (0 - 7)
 * rx_src: indicate rx msg source.
 * dma_id: it contains DMA controller ID, DMA port ID and base DMA channel ID.
 * dma_ch_num: number of dma channels used by this UMT port.
 * cqm_enq_pid: cqm enqueue port ID.
 * cqm_dq_pid: cqm dequeue port ID.
 * daddr: UMT message destination address.
 * msg_mode: UMT message mode.
 * period: UMT message interval period.
 * sw_msg: software message mode.
 * flag:  UMT message control flag.
 */
struct dp_umt_param {
	u8			id; /* [in/out] 0xff -- auto assign,
	 				othe value: caller provide
	 			     */
	enum dp_umt_rx_src	rx_src; /* [in] */
	u32			dma_id;  /* [in] */
	u8			dma_ch_num; /* [in] */
	u8			cqm_enq_pid; /* [in] */
	u8			cqm_dq_pid; /* [in] */
	u32			daddr;  /* [in] */
	enum dp_umt_msg_mode	msg_mode; /* [in] */
	u32			period; /* [in] */
	enum dp_umt_sw_msg	sw_msg; /* [in] */
	unsigned long		flag; /* [in] control flag*/
};

/**
 * struct dp_umt_priv
 * dev: platform device.
 * membase: UMT register base address.
 * umt_num: number of UMT entries.
 * umts: umt entry list.
 */
struct dp_umt_priv {
	struct device		*dev;
	void __iomem		*membase;
	u8			umt_num;
	struct dp_umt_entry	*umts;
};

/**
 * struct dp_umt_entry
 * param: umt configure parameters.
 * alloced: umt entry status.
 * enabled: umt entry status.
 * halted:  umt control status.
 * not_snd_zero_cnt: umt control status.
 * snd_rx_only: umt control status.
 * max_dam_ch_num: support max DMA channel numbers.
 * debugfs: debugfs proc.
 */
struct dp_umt_entry {
	struct dp_umt_param	param;
	int			alloced:1;
	int			enabled:1;
	int			halted:1;
	int			not_snd_zero_cnt:1;
	int			snd_rx_only:1;
	int			max_dma_ch_num;
	struct dentry		*debugfs;
};

//----define structure here end

//----declare APIs start
int dp_umt_request(struct dp_umt_param *umt, unsigned long flag);

/* set flag for period  */
#define DP_UMT_SET_Period BIT(0)
int dp_umt_set(struct dp_umt_param *umt, unsigned long flag);

int dp_umt_enable(struct dp_umt_param *umt, unsigned long flag, int enable);
int dp_umt_suspend_sending(struct dp_umt_param *umt,
			   unsigned long flag, int halt);

//----declare APIs end

/**
 * debug proc should support:
 * 1. register content dump
 * 2. RX/TX trigger
 * 3. UMT status
 */
