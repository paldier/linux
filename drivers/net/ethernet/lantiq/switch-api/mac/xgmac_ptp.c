/******************************************************************************
 *                Copyright (c) 2016, 2017 Intel Corporation
 *
 *
 * For licensing information, see the file 'LICENSE' in the root folder of
 * this software module.
 *
 ******************************************************************************/
/* =========================================================================
 * This file incorporates work covered by the following copyright and
 * permission notice:
 * The Synopsys DWC ETHER XGMAC Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================= */


#include <xgmac.h>
#include <xgmac_ptp.h>

static int xgmac_adj_freq(struct ptp_clock_info *ptp, s32 ppb);
static int xgmac_adj_time(struct ptp_clock_info *ptp, s64 delta);
static int xgmac_get_time(struct ptp_clock_info *ptp,
			  struct timespec64 *ts);
static int xgmac_set_time(struct ptp_clock_info *ptp,
			  const struct timespec64 *ts);
static void *parse_ptp_packet(struct sk_buff *skb, u16 *eth_type);
static int xgmac_check_tx_tstamp(void *pdev, struct sk_buff *skb);
static void xgmac_get_rx_tstamp(void *pdev, struct sk_buff *skb);
static int xgmac_ptp_register(void *pdev);

// TODO: Get the ptp clock in Hz from device tree
void xgmac_config_timer_reg(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	struct timespec now;
	struct mac_ops *hw_if = &pdata->ops;
	u64 temp;

	/* program Sub Second Increment Reg */
	hw_if->config_subsec_inc(pdata, pdata->ptp_clk);

	/* Calculate the def addend:
	 * addend = 2^32 / (PTP ref clock / 50Mhz)
	 *        = (2^32 * 50Mhz) / PTP ref clock
	 */
	temp = ((u64)(MHZ_TO_HZ(50)) << 32);
	pdata->def_addend = div_u64(temp, pdata->ptp_clk);

	hw_if->config_addend(pdev, pdata->def_addend);

	/* initialize system time */
	getnstimeofday(&now);
	hw_if->init_systime(pdev, now.tv_sec, now.tv_nsec);
}

/* API to adjust the frequency of hardware clock.
 * delta - desired period change.
 */
static int xgmac_adj_freq(struct ptp_clock_info *ptp, s32 ppb)
{
	struct mac_prv_data *pdata =
		container_of(ptp, struct mac_prv_data, ptp_clk_info);
	struct mac_ops *hw_if = &pdata->ops;

	u64 adj;
	u32 diff, addend;
	int neg_adj = 0;

	pr_info("Calling adjust_freq: %d\n", ppb);

	if (ppb < 0) {
		neg_adj = 1;
		ppb = -ppb;
	}

	addend = pdata->def_addend;
	adj = addend;
	adj *= ppb;
	/* div_u64 will divided the "adj" by "NSEC_TO_SEC"
	 * and return the quotient.
	 */
	diff = div_u64(adj, NSEC_TO_SEC);
	addend = neg_adj ? (addend - diff) : (addend + diff);

	spin_lock_bh(&pdata->ptp_lock);

	hw_if->config_addend(pdata, addend);

	spin_unlock_bh(&pdata->ptp_lock);

	return 0;
}

/* This function is used to shift/adjust the time of the
 * hardware clock.
 * delta - desired change in nanoseconds.
 */
static int xgmac_adj_time(struct ptp_clock_info *ptp, s64 delta)
{
	struct mac_prv_data *pdata =
		container_of(ptp, struct mac_prv_data, ptp_clk_info);
	struct mac_ops *hw_if = &pdata->ops;

	u32 sec, nsec;
	u32 quotient, reminder;
	int neg_adj = 0;

	pr_info("Calling adjust_time: %lld\n", delta);

	if (delta < 0) {
		neg_adj = 1;
		delta = -delta;
	}

	quotient = div_u64_rem(delta, NSEC_TO_SEC, &reminder);
	sec = quotient;
	nsec = reminder;

	spin_lock_bh(&pdata->ptp_lock);

	hw_if->adjust_systime(pdata, sec, nsec, neg_adj,
			      pdata->one_nsec_accuracy);

	spin_unlock_bh(&pdata->ptp_lock);

	return 0;
}

/* This function is used to read the current time from the
 * hardware clock.
 */
static int xgmac_get_time(struct ptp_clock_info *ptp,
			  struct timespec64 *ts)
{
	struct mac_prv_data *pdata =
		container_of(ptp, struct mac_prv_data, ptp_clk_info);
	struct mac_ops *hw_if = &pdata->ops;
	u64 ns;
	u32 reminder;


	spin_lock_bh(&pdata->ptp_lock);

	ns = hw_if->get_systime(pdata);

	spin_unlock_bh(&pdata->ptp_lock);

	ts->tv_sec = div_u64_rem(ns, NSEC_TO_SEC, &reminder);
	ts->tv_nsec = reminder;

	pr_info("get_time: ts->tv_sec  = %ld,ts->tv_nsec = %ld\n",
		(long int)ts->tv_sec, (long int)ts->tv_nsec);

	return 0;
}

/* This function is used to set the current time on the
 * hardware clock.
 */
static int xgmac_set_time(struct ptp_clock_info *ptp,
			  const struct timespec64 *ts)
{
	struct mac_prv_data *pdata =
		container_of(ptp, struct mac_prv_data, ptp_clk_info);
	struct mac_ops *hw_if = &pdata->ops;


	pr_info("set_time: ts->tv_sec  = %lld, ts->tv_nsec = %lld\n",
		(u64)ts->tv_sec, (u64)ts->tv_nsec);

	spin_lock_bh(&pdata->ptp_lock);

	hw_if->init_systime(pdata, ts->tv_sec, ts->tv_nsec);

	spin_unlock_bh(&pdata->ptp_lock);

	return 0;
}

/*  Parse PTP packet to find out whether flag is 1 step or 2 step.
 *  The PTP header can be found in an IPv4, IPv6 or in an IEEE802.3
 *  ethernet frame. The function returns the position of the PTP packet
 *  or NULL, if no PTP found.
 */
static void *parse_ptp_packet(struct sk_buff *skb, u16 *eth_type)
{
	struct iphdr *iph;
	struct udphdr *udph;
	struct ipv6hdr *ipv6h;
	void *pos = skb->data + ETH_ALEN + ETH_ALEN;
	u8 *ptp_loc = NULL;
	u8 msg_type;

	*eth_type = *((u16 *)pos);

	/* Check if any VLAN tag is there */
	while (1) {
		if (*eth_type == ETH_P_8021Q) {
			pos += 4;
			*eth_type = *((u16 *)pos);
		} else {
			break;
		}
	}

	/* set pos after ethertype */
	pos += 2;

	switch (*eth_type) {
	/* Transport of PTP over Ethernet */
	case ETH_P_1588:
		ptp_loc = pos;

		msg_type = *((u8 *)(ptp_loc + PTP_OFFS_MSG_TYPE)) & 0xf;

		if ((msg_type == PTP_MSGTYPE_SYNC) ||
		    (msg_type == PTP_MSGTYPE_DELREQ) ||
		    (msg_type == PTP_MSGTYPE_PDELREQ) ||
		    (msg_type == PTP_MSGTYPE_PDELRESP)) {
			pr_info("Transport of PTP over Ethernet header\n");
			ptp_loc = pos;
		} else {
			pr_info("Error: Transport of PTP over Ethernet header\n");
			ptp_loc = NULL;
		}

		break;

	/* Transport of PTP over IPv4 */
	case ETH_P_IP:
		iph = (struct iphdr *)pos;

		if (ntohs(iph->protocol) != IPPROTO_UDP)
			ptp_loc = NULL;

		pos += iph->ihl * 4;
		udph = (struct udphdr *)pos;

		/* check the destination port address
		 * ( 319 (0x013F) = PTP event port )
		 */
		if (ntohs(udph->dest) != 319) {
			pr_info(" Transport of PTP over IPv4 Error\n");
			ptp_loc = NULL;
		} else {
			pr_info("Transport of PTP over IPv4 header\n");
			ptp_loc = pos + sizeof(struct udphdr);
		}

		break;

	/* Transport of PTP over IPv6 */
	case ETH_P_IPV6:
		ipv6h = (struct ipv6hdr *)pos;

		if (ntohs(ipv6h->nexthdr) != IPPROTO_UDP)
			ptp_loc = NULL;

		pos += sizeof(struct ipv6hdr);
		udph = (struct udphdr *)pos;

		/* check the destination port address
		 * ( 319 (0x013F) = PTP event port )
		 */
		if (ntohs(udph->dest) != 319) {
			pr_info(" Transport of PTP over IPv6 Error\n");
			ptp_loc = NULL;
		} else {
			pr_info("Transport of PTP over IPv6 header\n");
			ptp_loc = pos + sizeof(struct udphdr);
		}

		break;

	default:
		ptp_loc = NULL;
		break;
	}

	if (!ptp_loc)
		pr_info("PTP header is not found in the packet, Please check\n");

	return ptp_loc;
}

/* =========================== TX TIMESTAMP =========================== */

/*  This API will be called by Datapath library when transmitting a packet to HW
 *  ptpd sends down 1-Step/2-Step sync packet on the event socket (i.e, one
 *  for which SO_TIMESTAMPING socket option is set).
 *  Network stack sets the SKBTX_HW_TSTAMP in skb since socket is
 *  marked for SO_TIMESTAMPING.
 */
/*  TODO: From Pmac Header OneStep bit indicates oneStep or TwoStep
 *  If 2 step use record id to enable TTSE/OSTC/OSTC_AVAIL
 *  to store the transmit timestamp
 */
int xgmac_tx_hwts(void *pdev, struct sk_buff *skb)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	int ret = 0;

	/* check for hw tstamping */
	if (pdata->hw_feat.ts_src && pdata->ptp_flgs.ptp_tx_en) {
		if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) &&
		    !(pdata->ptp_tx_skb)) {
			/* Check 1-step or 2-step tstamp */
			if (xgmac_check_tx_tstamp(pdata, skb) < 0)
				return ret;

			/* declare that device is doing timestamping */
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
			pdata->ptp_tx_skb = skb_get(skb);
			pr_info("Got PTP pkt to transmit\n");
		}
	}

	return ret;
}

/*  This API will determine whether the TX packet need to do 1 Step or 2 Step
 *  For one-step clock, the value of TWO_STEP shall be FALSE.
 *  For two-step clock, the value of TWO_STEP shall be TRUE.
 */
static int xgmac_check_tx_tstamp(void *pdev, struct sk_buff *skb)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	u16 flags;
	u16 ethtype;
	u8 *ptp_loc;

	/* Extract the packet Sync or Delay Resp Flags Field
	 *  Parsing here will not cause performance drop since only
	 *  SKBTX_HW_TSTAMP flagged packet will be parsed
	 */

	ptp_loc = parse_ptp_packet(skb, &ethtype);

	if (!ptp_loc)
		return -1;

	flags =  *((u16 *)(ptp_loc + PTP_OFFS_FLAGS)) & 0xFFFF;

	/* Check the TWO_STEP bit in the flag, Bit 1 in Flags is the 2 step flag
	 */
	if ((flags >> 8) & 0x02) {
		/* 2 Step timestamp flag enabled */
		pdata->two_step = 1;

		if (pdata->ptp_tx_init == 0) {
			INIT_WORK(&pdata->ptp_tx_work,
				  (work_func_t)xgmac_ptp_tx_work);
			pdata->ptp_tx_init = 1;
		}
	} else {
		/* 1 Step timestamp flag enabled */
		pdata->two_step = 0;

		if (pdata->ptp_tx_init) {
			cancel_work_sync(&pdata->ptp_tx_work);
			pdata->ptp_tx_init = 0;
		}
	}

	return 0;
}

/*  This API will get executed by workqueue only for 2 step timestamp
 *  Get the TX'ted Timestamp and pass it to upper app
 *  and free the skb
 */
int xgmac_ptp_tx_work(struct work_struct *work)
{
	struct mac_prv_data *pdata =
		container_of(work, struct mac_prv_data, ptp_tx_work);
	u64 tstamp;
	struct skb_shared_hwtstamps shhwtstamp;

	if (!pdata->ptp_tx_skb) {
		pr_info("No PTP packet transferred out from device\n");
		return -1;
	}

	/* check tx tstamp status, if have, read the timestamp and sent to
	 * stack, otherwise reschedule later
	 */
	tstamp = pdata->ops.get_tx_tstamp(pdata);

	if (!tstamp) {
		pr_info("Tx tstamp is not captured or ignored for this pkt\n");
		goto schedule_later;
	}

	memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp.hwtstamp = ns_to_ktime(tstamp);

	/* pass tstamp from HW to network stack fr 2 step */
	skb_tstamp_tx(pdata->ptp_tx_skb, &shhwtstamp);

	pr_info("Tx timestamp to stack\n");

	/* skb_tstamp_tx() clones the original skb and
	 * adds the timestamps, therefore the original skb
	 * has to be freed now and return.
	 */

	if (tstamp) {
		dev_kfree_skb_any(pdata->ptp_tx_skb);
		pdata->ptp_tx_skb = NULL;
		return 0;
	}

schedule_later:

	/* If One step timestamp no need to schedule work */
	if (pdata->two_step == 1) {
		/* reschedule to check later */
		schedule_work(&pdata->ptp_tx_work);
	}

	return 1;
}

/* =========================== RX TIMESTAMP =========================== */
/* This API will be called by Datapath library before calling netif_rx()
 */
int xgmac_rx_hwts(void *pdev, struct sk_buff *skb)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	/* No Rx packet will have timestamp added to it */
	if (pdata->ptp_flgs.ptp_rx_en == 0)
		return -1;

	/* All packets have timestamp appended to it, retrieve all
	 * timestamp and send to App
	 */
	if (pdata->ptp_flgs.ptp_rx_en & PTP_RX_EN_ALL)
		xgmac_get_rx_tstamp(pdev, skb);

	/* No other Rx filter currently supported in driver */
	return 0;
}

/* Update the received timestamp in skbhwtstamp
 * which will be used by PTP app
 */
static void xgmac_get_rx_tstamp(void *pdev, struct sk_buff *skb)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	__le64 regval;
	u32 ts_hdr_len = 8;
	struct skb_shared_hwtstamps *shhwtstamp = NULL;
	u64 ns;

	/* TODO: Below code to get hdr len is wrong, Need to find the
	 * correct register
	 */
	/* External clk_src only have 8 bytes as tstamp header
	 * Internal clk_src have 10 bytes as tstamp header
	 * (8 bytes tstamp + 2 bytes Year)
	 * Get the register setting for clk src
	 */
	if (pdata->hw_feat.ts_src == 2)
		ts_hdr_len = 8;
	else if (pdata->hw_feat.ts_src == 1 || pdata->hw_feat.ts_src == 3)
		ts_hdr_len = 10;

	/* copy the bits out of the skb, and then trim the skb length */
	skb_copy_bits(skb, skb->len - ts_hdr_len, &regval, ts_hdr_len);
	__pskb_trim(skb, skb->len - ts_hdr_len);

	/* The timestamp is recorded in little endian format, and is stored at
	 * the end of the packet.
	 *
	 * DWORD: N              N + 1      N + 2
	 * Field: End of Packet  SYSTIMH    SYSTIML
	 */
	ns = le64_to_cpu(regval);

	shhwtstamp = skb_hwtstamps(skb);
	memset(shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp->hwtstamp = ns_to_ktime(ns);
}

/* Configuring the HW Timestamping feature
 * This api will be called from the IOCTL SIOCSHWTSTAMP
 */
int xgmac_config_hwts(void *pdev, struct ifreq *ifr)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	struct hwtstamp_config config;
	int err;
	struct mac_ops *hw_if = &pdata->ops;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	err = hw_if->config_hw_time_stamping(pdev,
					     config.rx_filter, config.tx_type);

	if (err)
		return err;

	/* save these settings for future reference */
	memcpy(&pdata->tstamp_config, &config, sizeof(pdata->tstamp_config));

	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ?
	       -EFAULT : 0;
}

int xgmac_ptp_isr_hdlr(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 txtsc;
	struct mac_ops *hw_if = &pdata->ops;
	u64 tstamp;
	int ret = -1;

	/* Clear/Acknowledge interrupt by reading TXTSC */
	txtsc = XGMAC_RGRD_BITS(pdata, MAC_TSTAMP_STSR, TXTSC);

	if (txtsc) {
		/* If One step timestamp no need to schedule work */
		if (pdata->two_step == 1) {
			schedule_work(&pdata->ptp_tx_work);
		} else {
			/* Read the TxTimestamp Seconds register
			 * to clear the TXTSC bit
			 */
			tstamp = hw_if->get_tx_tstamp(pdata);
		}

		ret = 0;
	}

	return ret;
}

/* This API performs the required steps for enabling PTP support.
 * This api will be called by MAC driver when initializing MAC
 */
int xgmac_ptp_init(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	int ret = 0;

	if (!pdata->hw_feat.ts_src || !pdata->hw_feat.osten) {
		ret = -1;
		pdata->ptp_clock = NULL;
		pr_info("No PTP suppor in HW Aborting PTP clk drv register\n");
		return ret;
	}

	/* Initialize the spin lock first since we can't control when a user
	 * will call the entry functions once we have initialized the clock
	 * device
	 */

	spin_lock_init(&pdata->ptp_lock);

	/* Initialize the work queue, this is needed for 2 step timestamp
	 * process
	 */
	INIT_WORK(&pdata->ptp_tx_work, (work_func_t)xgmac_ptp_tx_work);

	pdata->ptp_tx_init = 1;

	pdata->one_nsec_accuracy = 1;

	ret = xgmac_ptp_register(pdev);

	if (ret == -1)
		pr_info("Already have a PTP clock device\n");

	return ret;
}

static int xgmac_ptp_register(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	struct ptp_clock_info *info = &pdata->ptp_clk_info;
	int ret = 0;

	/* do nothing if we already have a clock device */
	if (pdata->ptp_clock)
		return -1;

	snprintf(info->name, sizeof(info->name),
		 "%s", "xgmac_clk");

	info->owner = THIS_MODULE;
	info->adjfreq = xgmac_adj_freq;
	info->adjtime = xgmac_adj_time;
	info->gettime64 = xgmac_get_time;
	info->settime64 = xgmac_set_time;
#ifdef CONFIG_PTP_1588_CLOCK
	pdata->ptp_clock = ptp_clock_register(info, pdata->dev);
#endif

	if (IS_ERR(pdata->ptp_clock)) {
		pdata->ptp_clock = NULL;
		pr_err("ptp_clock_register() failed\n");
		return -1;
	}

	pr_info("Added PTP HW clock successfully\n");

	/* Disable all timestamping to start */
	pdata->tstamp_config.tx_type = HWTSTAMP_TX_OFF;
	pdata->tstamp_config.rx_filter = HWTSTAMP_FILTER_NONE;

	return ret;
}

void xgmac_ptp_remove(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	if (pdata->ptp_tx_init) {
		cancel_work_sync(&pdata->ptp_tx_work);
		pdata->ptp_tx_init = 0;
	}

	if (pdata->ptp_clock) {
#ifdef CONFIG_PTP_1588_CLOCK
		ptp_clock_unregister(pdata->ptp_clock);
		pr_info("Removed PTP HW clock successfully\n");
#endif
	}
}

