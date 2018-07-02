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
 * =========================================================================
 */

#include <xgmac.h>
#include <gswss_mac_api.h>
#include <xgmac_mdio.h>
#include <mac_cfg.h>
#include <lmac_api.h>

static void xgmac_menu(void);
static void set_data(u8 *argv[], int i, u32 *start_arg, u32 idx);
static void set_pdata(u8 *arg, u32 idx, u32 *data, u32 *start_arg);
static void set_all_pdata(u8 *arg, u32 *data, u32 *start_arg);

struct _xgmac_cfg {
	char name[256];
	void (*set_func)(void *);
	int (*get_func)(void *);
	u32 args;
	u32 *data1;
	u32 *data2;
	u32 *data3;
	u32 *data4;
	char help[1024];
};

struct mac_prv_data prv_data[10];
struct mac_prv_data pdata;

struct _xgmac_cfg xgmac_cfg_table[] = {
	/* Name  WritePtr ReadPtr  Args,Data1,Data2,Data3,Data4	Help	*/
	{
		"init            ",
		cli_init,
		0,
		0, &pdata.init_val, 0, 0, 0,
		"<Initialize the module based on the Index Set>"
	},
	{
		"reset           ",
		cli_reset,
		0,
		0, 0, 0, 0, 0,
		"<XGMAC Reset 1/0 Reset/No reset>"
	},
	{
		"regtest         ",
		cli_test_all_reg,
		0,
		0, 0, 0, 0, 0,
		"<Test all registers>"
	},
	/* MTL REGISTER SETTINGS */
	{
		"mtl_tx          ",
		cli_set_mtl_tx,
		xgmac_get_mtl_tx,
		0, &pdata.tx_sf_mode, &pdata.tx_threshold, 0, 0,
		"<args 2: 1/0: store_forward/threshold arg 2 0:64, 1:res, 2:96, 3:128, 4:192, 5:256, 6:384, 7:512>"
	},
	{
		"mtl_rx          ",
		cli_set_mtl_rx,
		xgmac_get_mtl_rx,
		0, &pdata.rx_sf_mode, &pdata.rx_threshold, 0, 0,
		"<args 2: 1/0: store_forward/threshold args 2: 0:64, 1:res, 2:96, 3:128>"
	},
	{
		"flow_ctrl_thresh",
		cli_flow_ctrl_thresh,
		xgmac_get_mtl_rx_flow_ctl,
		0, &pdata.rfa, &pdata.rfd, 0, 0,
		"<args 2: Thresh to act Flow Ctrl, Thresh to de-act Flow Ctrl>"
	},
	{
		"ts_addend       ",
		cli_set_tstamp_addend,
		0,
		0, &pdata.tstamp_addend, 0, 0, 0,
		"<args 1: Timestamd addend val>"
	},
	{
		"systime         ",
		0,
		xgmac_print_system_time,
		0, 0, 0, 0, 0,
		"Print System time"
	},
	{
		"ts_enable       ",
		cli_set_tstamp_enable,
		0,
		0, 0, 0, 0, 0,
		"Tx Timestamp Enable"
	},
	{
		"ts_disable      ",
		cli_set_tstamp_disable,
		0,
		0, 0, 0, 0, 0,
		"Disable TX and RX timestamp"
	},
	{
		"ptp_tx_mode     ",
		cli_set_txtstamp_mode,
		xgmac_get_txtstamp_mode,
		0, &pdata.snaptype, &pdata.tsmstrena, &pdata.tsevntena, 0,
		"<1/0: SnaptypeSel, 1/0: tsmstrena, 1/0: tseventena>"
	},
	{
		"hwtstamp        ",
		cli_set_hwtstamp_settings,
		xgmac_get_tstamp_settings,
		0, &pdata.tstamp_config.tx_type, &pdata.tstamp_config.rx_filter, 0, 0,
		"<args 2: 1/0 TX timestamp ON/OFF, FILTER_TYPE: 0-14/ None/ALL/Some/V1L4EVENT/V1L4SYNC/V1L4DELAY_REQ/V2L4EVENT/V2L4SYNC/V2L4DELAYREQ/V2L2EVENT/V2L2SYNC/V2L2DELAYREQ/V2EVENT/V2SYNC/V2DELAYREQ>"
	},
	{
		"flush_tx_q      ",
		cli_flush_tx_queues,
		0,
		0, 0, 0, 0, 0,
		"flush MTL transmit Q>"
	},
	{
		"debug_en        ",
		cli_set_debug_ctl,
		xgmac_get_debug_sts,
		0, &pdata.dbg_en, &pdata.dbg_mode, 0, 0,
		"<args 2: 1/0 DBG_EN 1/0 DBGMODE/SLAVE MODE>"
	},
	{
		"debug_tx        ",
		cli_set_tx_debug_data,
		0,
		0, &pdata.dbg_pktstate, 0, 0, 0,
		"<Pktstate 0/1/2/3 PKT_DATA/Ctrl_Word/SOP/EOP"
	},
	{
		"debug_rx        ",
		cli_set_rx_debug_data,
		0,
		0, 0, 0, 0, 0,
		"<Pktstate 0/1/2/3 PKT_DATA/Normal Sts/Last Sts/EOP>"
	},
	{
		"debug_data      ",
		cli_set_debug_data,
		0,
		0, &pdata.dbg_data, 0, 0, 0,
		"<args 1: debug_data pointer>"
	},
	{
		"debug_int_en    ",
		cli_set_rx_debugctrl_int,
		0,
		0, &pdata.dbg_pktie, 0, 0, 0,
		"<args 1: 1/0 PKT INT EN>"
	},
	{
		"debug_reset     ",
		cli_set_fifo_reset,
		0,
		0, &pdata.dbg_rst_sel, &pdata.dbg_rst_all, 0, 0,
		"<args 2: 1/0 RESET SEL FIFO, 1/0 RESET ALL FIFO>"
	},
	{
		"dbg_rx          ",
		cli_rx_packet_dbgmode,
		0,
		0, 0, 0, 0, 0,
		"<Rx Packet in DBGMODE>"
	},
	{
		"dbg_rx_slave    ",
		cli_rx_packet_slavemode,
		0,
		0, 0, 0, 0, 0,
		"<Rx Packet in SLAVEMODE>"
	},
	{
		"error_pkt_fwd   ",
		cli_set_fup_fep_pkt,
		xgmac_get_fup_fep_setting,
		0, &pdata.fup, &pdata.fef, 0, 0,
		"<args 2: <1/0 enable/disable FUP, 1/0 enable/disable FEF>"
	},
	{
		"int_en          ",
		cli_set_int,
		xgmac_dbg_int_sts,
		0, &pdata.enable_mtl_int, &pdata.enable_mac_int, 0, 0,
		"<MTL and MAC Interrupt Enable and get status>"
	},
	/* MAC REGISTER SETTINGS */
	{
		"mac_enable      ",
		cli_set_mac_enable,
		xgmac_get_mac_settings,
		0, &pdata.mac_en, 0, 0, 0,
		"<args 1: 1/0: MAC enable/disable>"
	},
	{
		"mac_addr        ",
		cli_set_mac_address,
		xgmac_get_mac_addr,
		0, (u32 *)&pdata.mac_addr, 0, 0, 0,
		"<args 1: mac_addr>"
	},
	{
		"mac_rx_mode     ",
		cli_set_mac_rx_mode,
		xgmac_get_mac_rx_mode,
		0, &pdata.promisc_mode, &pdata.all_mcast_mode, 0, 0,
		"<args 2: <1/0 enable/disable promisc, 1/0 accept/deny allmcast>"
	},
	{
		"mtu             ",
		cli_set_mtu,
		mac_get_mtu,
		0, &pdata.mtu, 0, 0, 0,
		"<args 1: MTU value>"
	},
	{
		"pause_time      ",
		0,
		0,
		0, &pdata.pause_time, 0, 0, 0,
		"<args 1: pause_time>"
	},
	{
		"pause_frame_ctrl",
		cli_set_pause_frame_ctrl,
		xgmac_get_pause_frame_ctl,
		0, &pdata.pause_frm_enable, &pdata.pause_time, 0, 0,
		"<args 1: 1/0: enable pause frame/disable pause frame>"
	},
	{
		"pause_filter    ",
		cli_set_pause_frame_filter,
		0,
		0, &pdata.pause_filter, 0, 0, 0,
		"<args 1: 1/0: enable pause frame filter/disable pause frame filter>"
	},
	{
		"pause_tx        ",
		cli_initiate_pause_tx,
		0,
		0, 0, 0, 0, 0,
		"<Initiate PAUSE packet transmit>"
	},
	{
		"speed           ",
		cli_set_mac_speed,
		mac_get_speed,
		0, &pdata.phy_speed, 0, 0, 0,
		"<args 1: 0/1/2/3:LMAC 10/100/200/1G  4/5/6/7/8/9:XGMAC 10/100/1G/2G5/5G/10G 10: 2G5-GMII>"
	},
	{
		"duplex          ",
		cli_set_duplex_mode,
		0,
		0, &pdata.duplex_mode, 0, 0, 0,
		"<args 1: 0/1/2:Full/Half/Auto>"
	},
	{
		"rx_csum_offload ",
		cli_set_csum_offload,
		xgmac_get_checksum_offload,
		0, &pdata.rx_checksum_offload, 0, 0, 0,
		"<args 1: 1/0: enable/disable rx checksum offload>"
	},
	{
		"loopback        ",
		cli_set_loopback,
		xgmac_get_mac_loopback_mode,
		0, &pdata.loopback, 0, 0, 0,
		"<args 1: 1/0: loopback enable/disable>"
	},
	{
		"eee             ",
		cli_set_eee_mode,
		xgmac_get_eee_settings,
		0, &pdata.eee_enable, 0, 0, 0,
		"<args 1: 1/0: enable/disable eee mode>"
	},
	{
		"crc_strip_type  ",
		cli_set_crc_strip_type,
		xgmac_get_crc_settings,
		0, &pdata.crc_strip_type, 0, 0, 0,
		"<args 1: 1/0: enable/disable crc strip>"
	},
	{
		"crc_strip_acs   ",
		cli_set_crc_strip_acs,
		xgmac_get_crc_settings,
		0, &pdata.padcrc_strip, 0, 0, 0,
		"<args 1: 1/0: enable/disable crc strip>"
	},
	{
		"ipg             ",
		cli_set_ipg,
		xgmac_get_ipg,
		0, &pdata.ipg, 0, 0, 0,
		"<args 1 IPG val 0 - 4, default val 0>"
	},
	{
		"magic_pmt       ",
		cli_set_pmt_magic,
		xgmac_dbg_pmt,
		0, &pdata.magic_pkt_en, 0, 0, 0,
		"<args 1: 1/0 MAGIC packet enable/disable for PMT>"
	},
	{
		"rwk_pmt         ",
		cli_set_pmt_rwk,
		xgmac_dbg_pmt,
		0, &pdata.rwk_pkt_en, 0, 0, 0,
		"<args 1: 1/0 Remote Wake Up packet enable/disable for PMT>"
	},
	{
		"gucast_pmt      ",
		cli_set_pmt_gucast,
		xgmac_dbg_pmt,
		0, &pdata.gucast, 0, 0, 0,
		"<args 1: 1/0 PMT Global Unicast ENABLE/DISABLE>"
	},
	{
		"extcfg          ",
		cli_set_extcfg,
		xgmac_get_extcfg,
		0, &pdata.extcfg, 0, 0, 0,
		"<args 1: 1/0 ExtCfg SBDIOEN>"
	},
	{
		"rxtx            ",
		cli_set_macrxtxcfg,
		xgmac_get_mac_rxtx_sts,
		0, &pdata.jd, &pdata.wd, 0, 0,
		"<args 2: 1/0 Jabber Disable 1/0 Watchdog Disable>"
	},
	{
		"miss_ovf_pkt_cnt",
		0,
		xgmac_get_mtl_missed_pkt_cnt,
		0, 0, 0, 0, 0,
		"<Missed Overflow packet count>"
	},
	{
		"uflow_pkt_cnt   ",
		0,
		xgmac_get_mtl_underflow_pkt_cnt,
		0, 0, 0, 0, 0,
		"<Underflow packet count>"
	},
	{
		"tstamp_sts      ",
		0,
		xgmac_get_tstamp_status,
		0, 0, 0, 0, 0,
		"<Get timestamp status>"
	},
	{
		"txtstamp_cnt      ",
		0,
		xgmac_get_txtstamp_cnt,
		0, 0, 0, 0, 0,
		"<Get txtstamp count>"
	},
	{
		"txtstamp_pktid      ",
		0,
		xgmac_get_txtstamp_pktid,
		0, 0, 0, 0, 0,
		"<Get txtstamp pktid>"
	},
	{
		"linksts         ",
		cli_set_linksts,
		0,
		0, &pdata.linksts, 0, 0, 0,
		"<0/1/2 UP/DOWN/AUTO>"
	},
	{
		"lpitx           ",
		cli_set_lpitx,
		0,
		0, &pdata.lpitxa, 0, 0, 0,
		"<args 1: 1/0: enable/disable>"
	},
	{
		"mdio_cl         ",
		cli_set_mdio_cl,
		0,
		0, &pdata.mdio_cl, &pdata.phy_id, 0, 0,
		"<args 2: 1/0: CL22/CL45, phy_id>"
	},
	{
		"mdio_rd         ",
		cli_mdio_rd,
		0,
		0, &pdata.dev_adr, &pdata.phy_id, &pdata.phy_reg, 0,
		"<args 3: dev_adr, phy_id, phy_reg>"
	},
	{
		"mdio_rd_cont    ",
		cli_mdio_rd_cont,
		0,
		0, &pdata.dev_adr, &pdata.phy_id, &pdata.phy_reg_st, &pdata.phy_reg_end,
		"<args 4: dev_adr, phy_id, phy_reg_st, phy_reg_end>"
	},
	{
		"mdio_wr         ",
		cli_mdio_wr,
		0,
		0, &pdata.dev_adr, &pdata.phy_id, &pdata.phy_reg, &pdata.phy_data,
		"<args 4: dev_adr, phy_id, phy_reg, phy_data>"
	},
	{
		"mdio_int        ",
		cli_set_mdio_int,
		xgmac_mdio_get_int_sts,
		0, &pdata.mdio_int, 0, 0, 0,
		"<args 1: <mdio_int>"
	},
	{
		"fcs_gen         ",
		cli_set_fcsgen,
		0,
		0, &pdata.fcsgen, 0, 0, 0,
		"<args 1: 1/0 fcs gen ENABLE/DISABLE>"
	},
	{
		"gint             ",
		cli_set_gint,
		0,
		0, &pdata.val, 0, 0, 0,
		"<args 1: 1/0 G.INT ENABLE/DISABLE>"
	},
	{
		"rx_crc             ",
		cli_set_rxcrc,
		0,
		0, &pdata.val, 0, 0, 0,
		"<args 1: 1/0 Rx Crc check DISABLE/ENABLE>"
	},
	{
		"fifo             ",
		0,
		cli_get_fifo,
		0, 0, 0, 0, 0,
		"<Get Tx Fifo>"
	},
	{
		"fifo_add             ",
		cli_add_fifo,
		0,
		0, 0, 0, 0, 0,
		"<Add Tx Fifo>"
	},
	{
		"fifo_del             ",
		cli_del_fifo,
		0,
		0, &pdata.rec_id, 0, 0, 0,
		"<Del Tx Fifo>"
	},
	{
		"ts_src_sel             ",
		cli_set_extsrc,
		0,
		0, &pdata.val, 0, 0, 0,
		"<REF: 0/1 - Internal/External>"
	},
	/* OTHERS */
	{
		"rmon            ",
		0,
		cli_get_rmon,
		0, 0, 0, 0, 0,
		"<args 1: 1: reset 0: no reset>"
	},
	{
		"clear_rmon      ",
		cli_clear_rmon_all,
		0,
		0, 0, 0, 0, 0,
		"<clear rmon>"
	},
	{
		"rmon_cfg        ",
		0,
		xgmac_get_counters_cfg,
		0, 0, 0, 0, 0,
		"<RMON config>"
	},
	{
		"priv_data       ",
		0,
		xgmac_get_priv_data,
		0, 0, 0, 0, 0,
		"<read private data>"
	},
	{
		"hw_feat         ",
		0,
		xgmac_print_hw_cap,
		0, 0, 0, 0, 0,
		"<get all hw features>"
	},
	{
		"all             ",
		0,
		xgmac_get_all_hw_settings,
		0, 0, 0, 0, 0,
		"<Get all HW settings>"
	},
};

u32 wakeup_filter_config[] = {
	/* for filter 0 CRC is computed on 0 - 7 bytes from offset */
	0x000000ff,
	/* for filter 1 CRC is computed on 0 - 7 bytes from offset */
	0x000000ff,
	/* for filter 2 CRC is computed on 0 - 7 bytes from offset */
	0x000000ff,
	/* for filter 3 CRC is computed on 0 - 7 bytes from offset */
	0x000000ff,
	/* filter 0, 1 independently enabled and would apply for
	 * unicast packet only filter 3, 2 combined as,
	 * "Filter-3 pattern AND NOT Filter-2 pattern"
	 */
	0x01010101,
	/* filter 3, 2, 1 and 0 offset is 14, 22, 30, 38 bytes
	 * from start
	 */
	0x0e161e26,
	/* pattern for filter 1 and 0, "0x55", "11", repeated 8 times */
	0x0B400B40,
	/* pattern for filter 3 and 2, "0x44", "33", repeated 8 times */
	0x0B400B40,
};

int populate_filter_frames(void *pdev)
{
	int i;
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	for (i = 0; i < 8; i++)
		pdata->rwk_filter_values[i] = wakeup_filter_config[i];

	pdata->rwk_filter_length = 8;

	return 0;
}

void removeSpace(char *str)
{
	char *p1 = str, *p2 = str;

	do
		while (*p2 == ' ')
			p2++;

	while ((*p1++ = *p2++));
}

void xgmac_menu(void)
{
	int i = 0;

	int num_of_elem = (sizeof(xgmac_cfg_table) / sizeof(struct _xgmac_cfg));

	mac_printf("\n MAC SET API's\n\n");

	for (i = 0; i < num_of_elem; i++) {
		if (xgmac_cfg_table[i].set_func)
			mac_printf("switch_cli xgmac <0/1/2/* MacIdx> %15s \t %s \n",
				   xgmac_cfg_table[i].name,
				   xgmac_cfg_table[i].help);
	}

	mac_printf("\n MAC GET API's\n\n");

	for (i = 0; i < num_of_elem; i++) {
		if (xgmac_cfg_table[i].get_func)
			mac_printf("switch_cli xgmac <0/1/2/ MacIdx> get %s\n",
				   xgmac_cfg_table[i].name);
	}
}

void xgmac_wr_reg(void *pdev, u16 reg_off, u32 reg_val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	mac_printf("\tREG offset: 0x%04x\n\tData: %08X\n", reg_off,
		   reg_val);

	XGMAC_RGWR(pdata, reg_off, reg_val);
}

u32 xgmac_rd_reg(void *pdev, u16 reg_off)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	pdata->reg_val = XGMAC_RGRD(pdata, reg_off);

	mac_printf("\tREG offset: 0x%04x\n\tData: %08X\n", reg_off,
		   pdata->reg_val);

	return pdata->reg_val;
}

u8 mac_addr[6] = {0x00, 0x00, 0x94, 0x00, 0x00, 0x08};

void xgmac_init_pdata(struct mac_prv_data *pdata, int idx)
{
#ifdef __KERNEL__
	memset(pdata, 0, sizeof(struct mac_prv_data));
#endif

	if (idx == -1)
		pdata->mac_idx = (pdata - &prv_data[0]);
	else
		pdata->mac_idx = idx;

	pdata->xgmac_ctrl_reg = XGMAC_CTRL_REG(pdata->mac_idx);
	pdata->xgmac_data1_reg = XGMAC_DATA1_REG(pdata->mac_idx);
	pdata->xgmac_data0_reg = XGMAC_DATA0_REG(pdata->mac_idx);

	pdata->tx_q_count		= 1;
	pdata->rx_q_count		= 1;
	pdata->tx_sf_mode		= 0;
	pdata->tx_threshold		= MTL_TX_THRESHOLD_64;
	pdata->rx_sf_mode		= 0;
	pdata->rx_threshold		= MTL_RX_THRESHOLD_64;

	mac_addr[5] = pdata->mac_idx + 1;
	memcpy(pdata->mac_addr,	mac_addr, 6);

	pdata->tstamp_addend		= 0;
	pdata->tx_tstamp		= 0;
	pdata->phy_speed		= SPEED_XGMAC_10G;
	pdata->promisc_mode		= 1;
	pdata->all_mcast_mode		= 1;
	pdata->rfa			= 2;
	pdata->rfd			= 4;
	pdata->tx_mtl_alg		= MTL_ETSALG_WRR;
	pdata->rx_mtl_alg		= MTL_RAA_SP;
	pdata->mtu			= FALCON_MAX_MTU;
	pdata->pause_time		= 0xFFFF;
	pdata->rx_checksum_offload	= 1;
	pdata->pause_frm_enable		= 1;
	pdata->rmon_reset		= 1;
	pdata->loopback			= 0;
	pdata->eee_enable		= 1;
	pdata->lst			= 1000;
	pdata->twt			= 0;
	pdata->lpitxa			= 0;
	pdata->crc_strip		= 0;
	pdata->crc_strip_type		= 0;
	pdata->padcrc_strip		= 0;
	pdata->rmon_reset		= 1;
	pdata->fup			= 1;
	pdata->fef			= 1;
	pdata->mac_en			= 1;
	pdata->ipg			= 0;
	pdata->enable_mac_int		= XGMAC_ALL_EVNT;
	pdata->enable_mtl_int		= MASK(MTL_Q_IER, TXUIE) |
					  MASK(MTL_Q_IER, ABPSIE) |
					  MASK(MTL_Q_IER, RXOIE);
	/* Calc as (2^32 * 50Mhz)/ 500Mhz */
	pdata->def_addend		= 0x19999999;
	pdata->sec			= 0;
	pdata->nsec			= 0;
	pdata->ptp_clk			= PTP_CLK;
	pdata->one_nsec_accuracy	= 0;
	pdata->ss_addr_base		= adap_priv_data.ss_addr_base;
	pdata->lmac_addr_base		= LEGACY_MAC_BASE;
#if defined(PC_UTILITY) || defined(CHIPTEST)
	pdata->max_mac			= MAX_MAC;
#endif
}

int xgmac_main(u32 argc, u8 *argv[])
{
	u32 i = 0, found = 0;
	u32 start_arg = 0;
	int idx = 0;
	u32 max_mac;
	u32 nanosec;
	u32 sec, min, hr, days;

	int num_of_elem =
		(sizeof(xgmac_cfg_table) / sizeof(struct _xgmac_cfg));
	struct mac_ops *ops = NULL;
	struct mac_prv_data *prv_data_k = NULL;

	prv_data[0].set_all = 0;
	prv_data[1].set_all = 0;
	prv_data[2].set_all = 0;

	start_arg++;
	start_arg++;

	if (!strcmp(argv[start_arg], "-help")) {
		found = 1;
		xgmac_menu();

		goto end;
	}

	if (found)
		goto end;

	if (!strcmp(argv[start_arg], "*")) {
		start_arg++;
		max_mac = gsw_get_mac_subifcnt(0);

		for (i = 0; i < max_mac; i++) {
			prv_data[i].set_all = 1;
			ops = gsw_get_mac_ops(0, i);
			prv_data_k = GET_MAC_PDATA(ops);
			prv_data_k->set_all = 1;
		}

	} else {
		idx = mac_nstrtoul(argv[start_arg],
				   mac_nstrlen(argv[start_arg]), &start_arg);

		max_mac = gsw_get_mac_subifcnt(0);

		if ((idx > (max_mac - 1)) || (idx < 0)) {
			mac_printf("Give valid xgmac index 0/1/2/*\n");
			return -1;
		}

		ops = gsw_get_mac_ops(0, idx);

		if (!ops)
			return -1;

		prv_data_k = GET_MAC_PDATA(ops);

		if (!prv_data_k)
			return -1;

		prv_data_k->set_all = 0;
	}

	if (!strcmp(argv[start_arg], "uptime")) {
		found = 1;

		sec = XGMAC_RGRD(prv_data_k, MAC_SYS_TIME_SEC);
		nanosec = XGMAC_RGRD(prv_data_k, MAC_SYS_TIME_NSEC);

		if (sec >= 60) {
			min = sec / 60;
			sec = sec - (min * 60);
		} else {
			min = 0;
		}

		if (min >= 60) {
			hr = min / 60;
			min = min - (hr * 60);
		} else {
			hr = 0;
		}

		if (hr >= 24) {
			days = hr / 24;
			hr = hr - (days * 24);
		} else {
			days = 0;
		}

		mac_printf("Uptime(d:h:m:s): %02d:%02d:%02d:%02d\n",
			   days, hr, min, sec);

		goto end;
	}

	if (!strcmp(argv[start_arg], "r")) {
		start_arg++;
		found = 1;
#if defined(PC_UTILITY) || defined(__KERNEL__)

		if ((strstr(argv[start_arg], "0x")) ||
		    (strstr(argv[start_arg], "0X")))
			mac_printf("matches with 0x\n");
		else
			mac_printf("Please give the address with "
				   "0x firmat\n");

#endif

		if (prv_data_k) {
			prv_data_k->reg_off = mac_nstrtoul(argv[start_arg],
							   mac_nstrlen(argv[start_arg]),
							   &start_arg);
			xgmac_rd_reg(ops, prv_data_k->reg_off);
		}

		goto end;
	}

	if (!strcmp(argv[start_arg], "w")) {
		start_arg++;
		found = 1;

#if defined(PC_UTILITY) || defined(__KERNEL__)

		if ((strstr(argv[start_arg], "0x")) ||
		    (strstr(argv[start_arg], "0X")))
			mac_printf("matches with 0x\n");
		else
			mac_printf("Please give the address with "
				   "0x format\n");

#endif

		if (prv_data_k) {
			prv_data_k->reg_off = mac_nstrtoul(argv[start_arg],
							   mac_nstrlen(argv[start_arg]),
							   &start_arg);
			prv_data_k->reg_val = mac_nstrtoul(argv[start_arg],
							   mac_nstrlen(argv[start_arg]),
							   &start_arg);
			xgmac_wr_reg(ops,
				     prv_data_k->reg_off,
				     prv_data_k->reg_val);
		}

		goto end;
	}

	if (!strcmp(argv[start_arg], "get")) {
		start_arg++;

		for (i = 0; i < num_of_elem; i++) {
			removeSpace(xgmac_cfg_table[i].name);

			if (!strcmp(xgmac_cfg_table[i].name,
				    argv[start_arg])) {
				if (xgmac_cfg_table[i].get_func)
					xgmac_cfg_table[i].get_func(ops);

				found = 1;
				break;
			}
		}
	}

	if (found)
		goto end;

	for (i = 0; i < num_of_elem; i++) {
		removeSpace(argv[start_arg]);

		if (!strcmp(xgmac_cfg_table[i].name, argv[start_arg])) {
			start_arg++;

			if (argc != xgmac_cfg_table[i].args) {
				mac_printf("[USAGE:]\n");
				mac_printf("xgmac <idx> %s %s\n",
					   xgmac_cfg_table[i].name,
					   xgmac_cfg_table[i].help);
				return 0;
			}

			set_data(argv, i, &start_arg, idx);

			if (xgmac_cfg_table[i].set_func)
				xgmac_cfg_table[i].set_func(ops);

			found = 1;
			break;
		}
	}

end:

	if (found == 0)
		mac_printf("command entered is invalid, use help to "
			   "display the supported cmds\n");

	return 0;
}

void set_data(u8 *argv[], int i, u32 *start_arg, u32 idx)
{
	if (prv_data[0].set_all == 1) {
		set_all_pdata(argv[*start_arg],
			      xgmac_cfg_table[i].data1, start_arg);
		set_all_pdata(argv[*start_arg],
			      xgmac_cfg_table[i].data2, start_arg);
		set_all_pdata(argv[*start_arg],
			      xgmac_cfg_table[i].data3, start_arg);
		set_all_pdata(argv[*start_arg],
			      xgmac_cfg_table[i].data4, start_arg);
	} else {
		set_pdata(argv[*start_arg], idx,
			  xgmac_cfg_table[i].data1, start_arg);
		set_pdata(argv[*start_arg], idx,
			  xgmac_cfg_table[i].data2, start_arg);
		set_pdata(argv[*start_arg], idx,
			  xgmac_cfg_table[i].data3, start_arg);
		set_pdata(argv[*start_arg], idx,
			  xgmac_cfg_table[i].data4, start_arg);
	}
}

void set_all_pdata(u8 *arg, u32 *data, u32 *start_arg)
{
	int j = 0, offset;
	u32 val;
	struct mac_ops *ops;
	struct mac_prv_data *mac_priv_data;
	u32 max_mac = gsw_get_mac_subifcnt(0);

	if (data) {
		offset = (u8 *)(data) - (u8 *)&pdata;
		val = mac_nstrtoul(arg, mac_nstrlen(arg), start_arg);

		*data = val;

		for (j = 0; j < max_mac; j++) {
#ifdef __KERNEL__
			ops  = gsw_get_mac_ops(0, j);

			if (ops) {
				mac_priv_data = GET_MAC_PDATA(ops);
				*(u32 *)(((u8 *)mac_priv_data + offset)) =
					val;
			}

#else
			*((u32 *)(((u8 *)&prv_data[j]) + offset)) = val;
#endif
		}
	}
}

void set_pdata(u8 *arg, u32 idx, u32 *data, u32 *start_arg)
{
	int offset;
	u32 val;
	struct mac_ops *ops;
	struct mac_prv_data *mac_priv_data;

	if (data) {
		offset = (u8 *)(data) - (u8 *)&pdata;
		val =  mac_nstrtoul(arg, mac_nstrlen(arg), start_arg);

		*data = val;

#ifdef __KERNEL__
		ops  = gsw_get_mac_ops(0, idx);

		if (ops) {
			mac_priv_data = GET_MAC_PDATA(ops);
			*(u32 *)(((u8 *)mac_priv_data + offset)) = val;
		}

#else
		*((u32 *)(((u8 *)&prv_data[idx]) + offset)) = val;
#endif
	}
}

void xgmac_cli_init(void)
{
	int i = 0;
	int num_of_elem = (sizeof(xgmac_cfg_table) / sizeof(struct _xgmac_cfg));

	for (i = 0; i < num_of_elem; i++) {
		removeSpace(xgmac_cfg_table[i].name);

		xgmac_cfg_table[i].args = 4;

		if (xgmac_cfg_table[i].data1)
			xgmac_cfg_table[i].args += 1;

		if (xgmac_cfg_table[i].data2)
			xgmac_cfg_table[i].args += 1;

		if (xgmac_cfg_table[i].data3)
			xgmac_cfg_table[i].args += 1;

		if (xgmac_cfg_table[i].data4)
			xgmac_cfg_table[i].args += 1;
	}
}

#if defined(PC_UTILITY) || defined(CHIPTEST)
struct mac_ops *gsw_get_mac_ops(u32 devid, u32 mac_idx)
{
	return &prv_data[mac_idx].ops;
}

u32 gsw_get_mac_subifcnt(u32 devid)
{
	return MAX_MAC;
}
#endif
