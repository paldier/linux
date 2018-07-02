/******************************************************************************
 *                Copyright (c) 2016, 2017 Intel Corporation
 *
 *
 * For licensing information, see the file 'LICENSE' in the root folder of
 * this software module.
 *
 ******************************************************************************/

#include <gswss_mac_api.h>

struct adap_prv_data adap_priv_data = {
	.flags = 0,
	.ss_addr_base = GSWIP_SS_TOP_BASE,
};

struct _gswss_cfg {
	char cmdname[256];
	u8 args;
	char help[1024];
};

struct _gswss_cfg gswss_mac_cfg[] = {
	{
		"reset",
		3,
		"<0/1/2: MacIdx> "
		"<0/1/2/3/4 XGMAC/LMAC/ADAP> <1/0: Reset/No reset>"
	},
	{
		"macen",
		2,
		"<0/1/2: MacIdx> "
		"<1/0: Enable/Disable>"
	},
	{
		"macif",
		4,
		"<0/1/2: MacIdx> <0/1/2: 1G/FE/2.5G> "
		"<0/1/2/3 MII/GMII/XGMAC_GMII/XGMAC_XGMII>"
	},
	{
		"macop",
		4,
		"<0/1/2: MacIdx> <0/1 RX/TX> "
		"<0/1/2 FCS/SPTAG/TIME> <0/1/2/3 Mode0/Mode1/Mode2/Mode3>"
	},
	{
		"mtu",
		2,
		"<0/1/2: MacIdx> "
		"<mtu size>"
	},
	{
		"txtstamp_fifo",
		8,
		"<0/1/2: MacIdx> "
		"<ttse ostc ost_avail cic sec nsec rec_id>"
	},
	{
		"w",
		2,
		"<reg_off> <reg_val>"
	},
	{
		"r",
		1,
		"<reg_off>"
	},
};

struct _gswss_cfg gswss_adap_cfg[] = {
	{
		"int_en",
		3,
		"<0/1/2 MacIdx> "
		"<0/1/2/3 XGMAC/LMAC/ADAP/MACSEC> <1/0: Enable/Disable>"
	},
	{
		"macsec",
		2,
		"<0/1/2 MacIdx> "
		"<1/0: Enable/Disable>"
	},
	{
		"cfg_1588",
		4,
		"<ref_time <0/1/2/3/4/5 PON/PCIE0/PCIE1/XGMAC2/XGMAC3/XGMAC4>"
		"<dig_time <0/1/2/3/4/5 PON/PCIE0/PCIE1/XGMAC2/XGMAC3/XGMAC4>"
		"<bin_time <0/1/2/3/4/5 PON/PCIE0/PCIE1/XGMAC2/XGMAC3/XGMAC4>"
		"<pps_sel <0/1/2/3/4/5/6/7 PON/PCIE0/PCIE1/XGMAC2/XGMAC3/XGMAC4/PON100US/SW>"
	},
	{
		"aux_trig",
		3,
		"<trig0_sel <0/1/2/3/4/5/6/8/9/10 PON/PCIE0/PCIE1/XGMAC2/XGMAC3/XGMAC4/PON100US/EXTPPS0/EXTPPS1/SW>"
		"<trig1_sel <0/1/2/3/4/5/6/8/9/10 PON/PCIE0/PCIE1/XGMAC2/XGMAC3/XGMAC4/PON100US/EXTPPS0/EXTPPS1/SW>"
		"<sw_trig <0/1>"
	},
	{
		"clk_md",
		1,
		"<0/1/2/3 666/450/Auto/Auto Mhz >"
	},
	{
		"nco",
		2,
		"<val idx>"
	},
	{
		"macsec_rst",
		1,
		"<1/0: Reset/No-Reset>"
	},
	{
		"ss_rst",
		0,
		"<Reset Switch Core>"
	},
	{
		"corese",
		1,
		"<1/0: enable/disable>"
	},
};

void gswss_help(void)
{
	int i = 0;
	int num_of_elem = (sizeof(gswss_mac_cfg) / sizeof(struct _gswss_cfg));

	mac_printf("\n----GSWSS MAC Commands----\n\n");

	for (i = 0; i < num_of_elem; i++) {
		if (gswss_mac_cfg[i].help) {
#if defined(CHIPTEST) && CHIPTEST
			mac_printf("gsw gswss %15s \t %s\n",
				   gswss_mac_cfg[i].cmdname,
				   gswss_mac_cfg[i].help);
#else
			mac_printf("switch_cli gswss %15s \t %s\n",
				   gswss_mac_cfg[i].cmdname,
				   gswss_mac_cfg[i].help);
#endif
		}
	}

	mac_printf("\n");

	for (i = 0; i < num_of_elem; i++) {
		if (gswss_mac_cfg[i].cmdname) {
#if defined(CHIPTEST) && CHIPTEST

			if (!strcmp(gswss_mac_cfg[i].cmdname,
				    "txtstamp_fifo")) {
				mac_printf("gsw gswss get %11s \t "
					   "<0/1/2: MacIdx> <rec_id>\n",
					   gswss_mac_cfg[i].cmdname);
				continue;
			}

			if (!strcmp(gswss_mac_cfg[i].cmdname, "w") ||
			    (!strcmp(gswss_mac_cfg[i].cmdname, "r")))
				continue;

			mac_printf("gsw gswss get %11s \t <0/1/2: MacIdx> \n",
				   gswss_mac_cfg[i].cmdname);
#else

			if (!strcmp(gswss_mac_cfg[i].cmdname,
				    "txtstamp_fifo")) {
				mac_printf("switch_cli gswss get %11s \t "
					   "<0/1/2: MacIdx> <rec_id>\n",
					   gswss_mac_cfg[i].cmdname);
				continue;
			}

			if (!strcmp(gswss_mac_cfg[i].cmdname, "w") ||
			    (!strcmp(gswss_mac_cfg[i].cmdname, "r")))
				continue;

			mac_printf("switch_cli gswss get %11s \t "
				   "<0/1/2: MacIdx> \n",
				   gswss_mac_cfg[i].cmdname);
#endif
		}
	}

	num_of_elem = (sizeof(gswss_adap_cfg) / sizeof(struct _gswss_cfg));

	mac_printf("\n----GSWSS Adaption Commands----\n\n");

	for (i = 0; i < num_of_elem; i++) {
		if (gswss_adap_cfg[i].help) {
#if defined(CHIPTEST) && CHIPTEST
			mac_printf("gsw gswss %15s \t %s\n",
				   gswss_adap_cfg[i].cmdname,
				   gswss_adap_cfg[i].help);
#else
			mac_printf("switch_cli gswss %15s \t %s\n",
				   gswss_adap_cfg[i].cmdname,
				   gswss_adap_cfg[i].help);
#endif
		}
	}

	mac_printf("\n");

	for (i = 0; i < num_of_elem; i++) {
		if (gswss_adap_cfg[i].cmdname) {
#if defined(CHIPTEST) && CHIPTEST
			mac_printf("gsw gswss get %11s \t \n",
				   gswss_adap_cfg[i].cmdname);
#else
			mac_printf("switch_cli gswss get %11s \t \n",
				   gswss_adap_cfg[i].cmdname);
#endif
		}
	}

	return;
}

int gswss_mac_check_args(int argc, char *argv)
{
	int i = 0;
	int num_of_elem = (sizeof(gswss_mac_cfg) / sizeof(struct _gswss_cfg));

	for (i = 0; i < num_of_elem; i++) {
		if (!strcmp(argv, gswss_mac_cfg[i].cmdname)) {
			if (argc != (gswss_mac_cfg[i].args + 3)) {
				mac_printf("\n--WRONG Command--\n");
				mac_printf("switch_cli gswss %s %s\n",
					   gswss_mac_cfg[i].cmdname,
					   gswss_mac_cfg[i].help);
				mac_printf("switch_cli gswss get %s "
					   "<0/1/2: MacIdx> \n",
					   gswss_mac_cfg[i].cmdname);
				return -1;
			}
		}
	}

	return 0;
}

struct mac_ops *gswss_get_mac_ops(char *cmd, char *argv, u32 *start_arg)
{
	u32 idx, i = 0;
	int num_of_elem = (sizeof(gswss_mac_cfg) / sizeof(struct _gswss_cfg));
	struct mac_ops *ops = NULL;
	u32 max_mac = 0;

	for (i = 0; i < num_of_elem; i++) {
		if (!strcmp(cmd, gswss_mac_cfg[i].cmdname))
			break;
	}

	max_mac = gsw_get_mac_subifcnt(0);

	idx = mac_nstrtoul(argv,
			   mac_nstrlen(argv),
			   start_arg);

	if ((idx < 0) || (idx > max_mac)) {
		gswss_help();
		return NULL;
	}

	ops = gsw_get_mac_ops(0, idx);
	return ops;
}

int gswss_adap_check_args(int argc, char *argv)
{
	int i = 0;
	int num_of_elem = (sizeof(gswss_adap_cfg) / sizeof(struct _gswss_cfg));

	for (i = 0; i < num_of_elem; i++) {
		if (!strcmp(argv, gswss_adap_cfg[i].cmdname)) {
			if (argc != (gswss_adap_cfg[i].args + 3)) {
				mac_printf("\n--WRONG Command--\n");
				mac_printf("switch_cli %s\n",
					   gswss_adap_cfg[i].help);
				mac_printf("switch_cli gswss get %s\n",
					   gswss_adap_cfg[i].cmdname);
				return -1;
			}
		}
	}

	return 0;
}

void gswss_wr_reg(void *pdev, u32 reg_off, u32 reg_val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	mac_printf("\tREG offset: 0x%04x\n\tData: %08X\n", reg_off,
		   reg_val);

	GSWSS_MAC_RGWR(pdata, reg_off, reg_val);
}

u32 gswss_rd_reg(void *pdev, u32 reg_off)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	pdata->reg_val = GSWSS_MAC_RGRD(pdata, reg_off);

	mac_printf("\tREG offset: 0x%04x\n\tData: %08X\n", reg_off,
		   pdata->reg_val);

	return pdata->reg_val;
}

int gswss_main(u32 argc, u8 *argv[])
{
	u32 start_arg = 0;
	u32 idx;
	u32 mod, val;
	u32 mac_idx;
	u32 speed;
	u32 dir, oper;
	u32 mtu;
	u32 enable, rec_id;
	u32 ref_time, dig_time, bin_time, pps_sel;
	struct mac_ops *ops = NULL;
	struct adap_ops *adap_ops = gsw_get_adap_ops(0);
	u32 reg_off, reg_val;
	struct mac_fifo_entry f_entry;
	u32 trig1_sel, trig0_sel, sw_trig;

	start_arg++;
	start_arg++;

	if (argc <= 2) {
		gswss_help();
		goto end;
	}

	if (!strcmp(argv[start_arg], "-help")) {
		gswss_help();
		goto end;
	}

	if (!strcmp(argv[start_arg], "w")) {
		start_arg++;

		ops = gsw_get_mac_ops(0, 0);

		reg_off = mac_nstrtoul(argv[start_arg],
				       mac_nstrlen(argv[start_arg]),
				       &start_arg);
		reg_val = mac_nstrtoul(argv[start_arg],
				       mac_nstrlen(argv[start_arg]),
				       &start_arg);
		gswss_wr_reg(ops, reg_off, reg_val);

		goto end;
	}

	if (!strcmp(argv[start_arg], "r")) {
		start_arg++;

		ops = gsw_get_mac_ops(0, 0);

		reg_off = mac_nstrtoul(argv[start_arg],
				       mac_nstrlen(argv[start_arg]),
				       &start_arg);

		gswss_rd_reg(ops, reg_off);

		goto end;
	}

	if (!strcmp(argv[start_arg], "get")) {
		start_arg++;

		if (!strcmp(argv[start_arg], "reset")) {
			start_arg++;

			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			gswss_get_mac_reset(ops);
		} else if (!strcmp(argv[start_arg], "macen")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			gswss_get_mac_en(ops);
		} else if (!strcmp(argv[start_arg], "int_en")) {
			start_arg++;
			gswss_get_int_en_sts(adap_ops);
		} else if (!strcmp(argv[start_arg], "macif")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			gswss_get_macif(ops);
		} else if (!strcmp(argv[start_arg], "macop")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			gswss_get_macop(ops);
		} else if (!strcmp(argv[start_arg], "mtu")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			gswss_get_mtu(ops);
		} else if (!strcmp(argv[start_arg], "txtstamp_fifo")) {
			start_arg++;

#ifdef __KERNEL__

			if (argc != 6) {
				mac_printf("switch_cli gswss get txtstamp_fifo"
					   "<0/1/2: MacIdx> rec_id\n");
				return -1;
			}

#endif
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			rec_id = mac_nstrtoul(argv[start_arg],
					      mac_nstrlen(argv[start_arg]),
					      &start_arg);

			gswss_get_txtstamp_fifo(ops, rec_id, &f_entry);
		} else if (!strcmp(argv[start_arg], "phymode")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			gswss_get_phy2mode(ops);
		} else if (!strcmp(argv[start_arg], "macsec")) {
			gswss_dbg_macsec_to_mac(adap_ops);
		} else if (!strcmp(argv[start_arg], "cfg_1588")) {
			gswss_get_cfg0_1588(adap_ops,
					    &ref_time,
					    &dig_time,
					    &bin_time,
					    &pps_sel);
		} else if (!strcmp(argv[start_arg], "clk_md")) {
			gswss_get_clkmode(adap_ops);
		} else if (!strcmp(argv[start_arg], "nco")) {
			gswss_get_nco(adap_ops, 0);
		} else if (!strcmp(argv[start_arg], "macsec_rst")) {
			gswss_get_macsec_reset(adap_ops);
		} else if (!strcmp(argv[start_arg], "ss_rst")) {
			gswss_get_switch_ss_reset(adap_ops);
		} else if (!strcmp(argv[start_arg], "corese")) {
			enable = gswss_get_corese(adap_ops);
			mac_printf("Switch Core: %s\n", enable ? "ENABLED" :
				   "DISABLED");
		} else {
			gswss_help();
		}
	} else {
		if (gswss_mac_check_args(argc, argv[start_arg]) < 0)
			return -1;

		if (gswss_adap_check_args(argc, argv[start_arg]) < 0)
			return -1;

		if (!strcmp(argv[start_arg], "reset")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			mod = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);

			if (mod == XGMAC)
				gswss_xgmac_reset(ops, val);
			else if (mod == LMAC)
				gswss_lmac_reset(ops, val);
			else if (mod == ADAP)
				gswss_adap_reset(ops, val);
		} else if (!strcmp(argv[start_arg], "macsec_rst")) {
			start_arg++;
			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			gswss_macsec_reset(adap_ops, val);
		} else if (!strcmp(argv[start_arg], "ss_rst")) {
			start_arg++;
			gswss_switch_ss_reset(adap_ops);
		} else if (!strcmp(argv[start_arg], "macen")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			gswss_mac_enable(ops, val);
		} else if (!strcmp(argv[start_arg], "int_en")) {
			start_arg++;

			mod = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			idx = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			gswss_set_interrupt(adap_ops, mod, idx, val);

		} else if (!strcmp(argv[start_arg], "macif")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			speed = mac_nstrtoul(argv[start_arg],
					     mac_nstrlen(argv[start_arg]),
					     &start_arg);
			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);

			if (speed == 0)
				gswss_set_1g_intf(ops, val);
			else if (speed == 1)
				gswss_set_fe_intf(ops, val);
			else if (speed == 2)
				gswss_set_2G5_intf(ops, val);
		} else if (!strcmp(argv[start_arg], "macop")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			dir = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			oper = mac_nstrtoul(argv[start_arg],
					    mac_nstrlen(argv[start_arg]),
					    &start_arg);
			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);

			if (dir == RX) {
				if (oper == 0)
					gswss_set_mac_rxfcs_op(ops, val);
				else if (oper == 1)
					gswss_set_mac_rxsptag_op(ops, val);
				else if (oper == 2)
					gswss_set_mac_rxtime_op(ops, val);
			} else if (dir == TX) {
				if (oper == 0) {
					if (val == 0 || val == 1)
						gswss_set_mac_txfcs_ins_op(ops,
									   val);
					else if (val == 2 || val == 3)
						gswss_set_mac_txfcs_rm_op(ops,
									  val - 2);
				} else if (oper == 1) {
					gswss_set_mac_txsptag_op(ops, val);
				}
			}
		} else if (!strcmp(argv[start_arg], "mtu")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			mtu = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			gswss_set_mtu(ops, mtu);
		} else if (!strcmp(argv[start_arg], "txtstamp_fifo")) {
			start_arg++;
			ops = gswss_get_mac_ops(argv[start_arg - 1],
						argv[start_arg],
						&start_arg);

			if (!ops)
				return -1;

			f_entry.ttse = mac_nstrtoul(argv[start_arg],
						    mac_nstrlen(argv[start_arg]),
						    &start_arg);
			f_entry.ostc = mac_nstrtoul(argv[start_arg],
						    mac_nstrlen(argv[start_arg]),
						    &start_arg);
			f_entry.ostpa = mac_nstrtoul(argv[start_arg],
						     mac_nstrlen(argv[start_arg]),
						     &start_arg);
			f_entry.cic = mac_nstrtoul(argv[start_arg],
						   mac_nstrlen(argv[start_arg]),
						   &start_arg);
			f_entry.ttsl = mac_nstrtoul(argv[start_arg],
						    mac_nstrlen(argv[start_arg]),
						    &start_arg);
			f_entry.ttsh = mac_nstrtoul(argv[start_arg],
						    mac_nstrlen(argv[start_arg]),
						    &start_arg);
			f_entry.rec_id = mac_nstrtoul(argv[start_arg],
						      mac_nstrlen(argv[start_arg]),
						      &start_arg);
			gswss_set_txtstamp_fifo(ops, &f_entry);
		} else if (!strcmp(argv[start_arg], "nco")) {
			start_arg++;

			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);
			idx = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);

			gswss_set_nco(adap_ops, val, idx);
		} else if (!strcmp(argv[start_arg], "macsec")) {
			start_arg++;

			mac_idx = mac_nstrtoul(argv[start_arg],
					       mac_nstrlen(argv[start_arg]),
					       &start_arg);
			enable = mac_nstrtoul(argv[start_arg],
					      mac_nstrlen(argv[start_arg]),
					      &start_arg);

			gswss_set_macsec_to_mac(adap_ops,
						mac_idx, enable);
		} else if (!strcmp(argv[start_arg], "corese")) {
			start_arg++;

			enable = mac_nstrtoul(argv[start_arg],
					      mac_nstrlen(argv[start_arg]),
					      &start_arg);

			gswss_set_corese(adap_ops, enable);
		} else if (!strcmp(argv[start_arg], "cfg_1588")) {
			start_arg++;

			ref_time = mac_nstrtoul(argv[start_arg],
						mac_nstrlen(argv[start_arg]),
						&start_arg);
			dig_time = mac_nstrtoul(argv[start_arg],
						mac_nstrlen(argv[start_arg]),
						&start_arg);
			bin_time = mac_nstrtoul(argv[start_arg],
						mac_nstrlen(argv[start_arg]),
						&start_arg);
			pps_sel = mac_nstrtoul(argv[start_arg],
					       mac_nstrlen(argv[start_arg]),
					       &start_arg);

			gswss_cfg0_1588(adap_ops,
					ref_time, dig_time, bin_time, pps_sel);
		} else if (!strcmp(argv[start_arg], "clk_md")) {
			start_arg++;

			val = mac_nstrtoul(argv[start_arg],
					   mac_nstrlen(argv[start_arg]),
					   &start_arg);

			gswss_set_clkmode(adap_ops, val);
		} else if (!strcmp(argv[start_arg], "aux_trig")) {
			start_arg++;

			trig0_sel = mac_nstrtoul(argv[start_arg],
						 mac_nstrlen(argv[start_arg]),
						 &start_arg);
			trig1_sel = mac_nstrtoul(argv[start_arg],
						 mac_nstrlen(argv[start_arg]),
						 &start_arg);
			sw_trig = mac_nstrtoul(argv[start_arg],
					       mac_nstrlen(argv[start_arg]),
					       &start_arg);
			gswss_cfg1_1588(adap_ops,
					trig1_sel, trig0_sel, sw_trig);
		} else {
			gswss_help();
		}
	}

end:
	return 0;
}

int gswss_xgmac_reset(void *pdev, u32 reset)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, XGMAC_RES, reset);

	mac_printf("GSWSS: Resetting XGMAC %d Module\n", pdata->mac_idx);

	GSWSS_MAC_RGWR(pdata, MAC_IF_CFG(pdata->mac_idx), mac_if_cfg);

	return 0;
}

int gswss_lmac_reset(void *pdev, u32 reset)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, LMAC_RES, reset);

	mac_printf("GSWSS: Resetting LMAC %d Module\n", pdata->mac_idx);

	GSWSS_MAC_RGWR(pdata, MAC_IF_CFG(pdata->mac_idx), mac_if_cfg);

	return 0;
}

int gswss_adap_reset(void *pdev, u32 reset)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, ADAP_RES, reset);

	mac_printf("GSWSS: Resetting ADAP %d Module\n", pdata->mac_idx);

	GSWSS_MAC_RGWR(pdata, MAC_IF_CFG(pdata->mac_idx), mac_if_cfg);

	return 0;
}

int gswss_mac_enable(void *pdev, u32 enable)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, MAC_EN, enable);

	mac_printf("GSWSS: MAC %d: %s\n", pdata->mac_idx,
		   enable ? "ENABLED" : "DISABLED");

	GSWSS_MAC_RGWR(pdata, MAC_IF_CFG(pdata->mac_idx), mac_if_cfg);

	return 0;
}

int gswss_get_mac_reset(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg, reset;
	int ret = 0, i = 0;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));
	reset = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, XGMAC_RES);
	mac_printf("\tXGMAC %d Reset Bit: %s\n", (i + 2),
		   reset ? "ENABLED" : "DISABLED");
	reset = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, LMAC_RES);
	mac_printf("\tXGMAC %d Reset Bit: %s\n", (i + 2),
		   reset ? "ENABLED" : "DISABLED");
	reset = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, ADAP_RES);
	mac_printf("\tXGMAC %d Reset Bit: %s\n", (i + 2),
		   reset ? "ENABLED" : "DISABLED");

	return ret;
}

int gswss_get_mac_en(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg, enable;
	int ret = 0, i = 0;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));
	enable = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, MAC_EN);
	mac_printf("GSWSS: MAC %d: %s\n", i, enable ? "ENABLED" : "DISABLED");

	return ret;
}

int gswss_set_1g_intf(void *pdev, u32 macif)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg;
	int ret = 0;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	if (macif == LMAC_GMII) {
		mac_printf("GSWSS: MACIF Set to CFG1G with LMAC_GMII\n");
		MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, CFG1G, 0);
	} else if (macif == XGMAC_GMII) {
		mac_printf("GSWSS: MACIF Set to CFG1G with XGMAC_GMII\n");
		MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, CFG1G, 1);
	} else {
		mac_printf("GSWSS: MACIF Set to 1G Wrong Value\n");
	}

	GSWSS_MAC_RGWR(pdata, MAC_IF_CFG(pdata->mac_idx), mac_if_cfg);

	return ret;
}

u32 gswss_get_1g_intf(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg, macif;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	macif = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, CFG1G);

	if (macif == 0) {
		mac_printf("GSWSS: MACIF got for CFG1G is LMAC_GMII\n");
		macif = LMAC_GMII;
	} else if (macif == 1) {
		mac_printf("GSWSS: MACIF got for CFG1G is XGMAC_GMII\n");
		macif = XGMAC_GMII;
	} else {
		mac_printf("GSWSS: MACIF got for CFG1G is Wrong Value\n");
	}

	return macif;
}

int gswss_set_fe_intf(void *pdev, u32 macif)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg;
	int ret = 0;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	if (macif == LMAC_MII) {
		mac_printf("GSWSS: MACIF Set to CFGFE with LMAC_MII\n");
		MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, CFGFE, 0);
	} else if (macif == XGMAC_GMII) {
		mac_printf("GSWSS: MACIF Set to CFGFE with XGMAC_GMII\n");
		MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, CFGFE, 1);
	} else {
		mac_printf("GSWSS: MACIF Set to CFGFE Wrong Value\n");
	}

	GSWSS_MAC_RGWR(pdata, MAC_IF_CFG(pdata->mac_idx), mac_if_cfg);

	return ret;
}

u32 gswss_get_fe_intf(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg, macif;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	macif = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, CFGFE);

	if (macif == 0) {
		mac_printf("GSWSS: MACIF got for CFGFE is LMAC_MII\n");
		macif = LMAC_MII;
	} else if (macif == 1) {
		mac_printf("GSWSS: MACIF got for CFGFE is XGMAC_GMII\n");
		macif = XGMAC_GMII;
	} else {
		mac_printf("GSWSS: MACIF got for CFGFE is Wrong Value\n");
	}

	return macif;
}

int gswss_set_2G5_intf(void *pdev, u32 macif)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg;
	int ret = 0;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	if (macif == XGMAC_GMII) {
		mac_printf("GSWSS: MACIF Set to 2.5G with XGMAC_GMII\n");
		MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, CFG2G5, 0);
	} else if (macif == XGMAC_XGMII) {
		mac_printf("GSWSS: MACIF Set to 2.5G with XGMAC_XGMII\n");
		MAC_SET_VAL(mac_if_cfg, MAC_IF_CFG, CFG2G5, 1);
	} else {
		mac_printf("GSWSS: MACIF Set to 2.5G Wrong Value\n");
	}

	GSWSS_MAC_RGWR(pdata, MAC_IF_CFG(pdata->mac_idx), mac_if_cfg);

	return ret;
}

u32 gswss_get_2G5_intf(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg, macif;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));

	macif = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, CFG2G5);

	if (macif == 0) {
		mac_printf("GSWSS: MACIF Got for 2.5G with XGMAC_GMII\n");
		macif = XGMAC_GMII;
	} else if (macif == 1) {
		mac_printf("GSWSS: MACIF Got for 2.5G with XGMAC_XGMII\n");
		macif = XGMAC_XGMII;
	} else {
		mac_printf("GSWSS: MACIF Got for 2.5G Wrong Value\n");
	}

	return macif;
}

void gswss_get_macif(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_if_cfg, val;

	mac_if_cfg = GSWSS_MAC_RGRD(pdata, MAC_IF_CFG(pdata->mac_idx));
	mac_printf("GSWSS: MAC%d IF_CFG %d\n", (pdata->mac_idx + 2),
		   mac_if_cfg);
	val = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, CFG1G);
	mac_printf("\t1G:            %s\n",
		   val ?
		   "XGMAC GMII interface mode is used" :
		   "Legacy GMAC GMII interface mode is used");
	val = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, CFGFE);
	mac_printf("\tFast Ethernet: %s\n",
		   val ?
		   "XGMAC GMII interface mode is used" :
		   "Legacy GMAC MII interface mode is used");
	val = MAC_GET_VAL(mac_if_cfg, MAC_IF_CFG, CFG2G5);
	mac_printf("\t2.5G:          %s\n",
		   val ?
		   "XGMAC XGMII interface mode is used" :
		   "XGMAC GMII interface mode is used");
}

int gswss_set_mac_txfcs_ins_op(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_op_cfg;

	mac_op_cfg = GSWSS_MAC_RGRD(pdata, MAC_OP_CFG(pdata->mac_idx));

	if (MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, TXFCS_INS) != val) {
		mac_printf("GSWSS: TX FCS INS operation changing to MODE%d\n",
			   val);
		MAC_SET_VAL(mac_op_cfg, MAC_OP_CFG, TXFCS_INS, val);
		GSWSS_MAC_RGWR(pdata, MAC_OP_CFG(pdata->mac_idx), mac_op_cfg);
	}

	return 0;
}

int gswss_set_mac_txfcs_rm_op(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_op_cfg;

	mac_op_cfg = GSWSS_MAC_RGRD(pdata, MAC_OP_CFG(pdata->mac_idx));

	if (MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, TXFCS_RM) != val) {
		mac_printf("GSWSS: TX FCS RM operation changing to MODE%d\n",
			   val);
		MAC_SET_VAL(mac_op_cfg, MAC_OP_CFG, TXFCS_RM, val);
		GSWSS_MAC_RGWR(pdata, MAC_OP_CFG(pdata->mac_idx), mac_op_cfg);
	}

	return 0;
}

int gswss_set_mac_rxfcs_op(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_op_cfg;

	mac_op_cfg = GSWSS_MAC_RGRD(pdata, MAC_OP_CFG(pdata->mac_idx));

	if (MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, RXFCS) != val) {
		mac_printf("GSWSS: RX FCS operation changing to MODE%d\n",
			   val);
		MAC_SET_VAL(mac_op_cfg, MAC_OP_CFG, RXFCS, val);
		GSWSS_MAC_RGWR(pdata, MAC_OP_CFG(pdata->mac_idx), mac_op_cfg);
	}

	return 0;
}

int gswss_set_mac_rxsptag_op(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_op_cfg;

	mac_op_cfg = GSWSS_MAC_RGRD(pdata, MAC_OP_CFG(pdata->mac_idx));

	if (MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, RXSPTAG) != val) {
		mac_printf("GSWSS: RX SPTAG operation changing to MODE%d\n",
			   val);
		MAC_SET_VAL(mac_op_cfg, MAC_OP_CFG, RXSPTAG, val);
		GSWSS_MAC_RGWR(pdata, MAC_OP_CFG(pdata->mac_idx), mac_op_cfg);
	}

	return 0;
}

int gswss_set_mac_txsptag_op(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_op_cfg;

	mac_op_cfg = GSWSS_MAC_RGRD(pdata, MAC_OP_CFG(pdata->mac_idx));

	if (MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, TXSPTAG) != val) {
		mac_printf("GSWSS: TX SPTAG operation changing to MODE%d\n",
			   val);
		MAC_SET_VAL(mac_op_cfg, MAC_OP_CFG, TXSPTAG, val);
		GSWSS_MAC_RGWR(pdata, MAC_OP_CFG(pdata->mac_idx), mac_op_cfg);
	}

	return 0;
}

int gswss_set_mac_rxtime_op(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_op_cfg;

	mac_op_cfg = GSWSS_MAC_RGRD(pdata, MAC_OP_CFG(pdata->mac_idx));

	if (MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, RXTIME) != val) {
		mac_printf("GSWSS: RX TIME operation changing to MODE%d\n",
			   val);
		MAC_SET_VAL(mac_op_cfg, MAC_OP_CFG, RXTIME, val);
		GSWSS_MAC_RGWR(pdata, MAC_OP_CFG(pdata->mac_idx), mac_op_cfg);
	}

	return 0;
}

void gswss_get_macop(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int i = 0;
	u32 mac_op_cfg, val;

	mac_op_cfg = GSWSS_MAC_RGRD(pdata, MAC_OP_CFG(pdata->mac_idx));
	mac_printf("GSWSS: MAC%d OP_CFG %d\n", (i + 2), mac_op_cfg);
	val = MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, TXFCS_INS);
	mac_printf("TX direction FCS\n");

	if (val == MODE0)
		mac_printf("\tPacket does not have FCS and FCS "
			   "is not inserted\n");
	else if (val == MODE1)
		mac_printf("\tPacket does not have FCS and FCS "
			   "is inserted\n");
	else if (val == MODE2)
		mac_printf("\tPacket has FCS and FCS is not inserted\n");
	else if (val == MODE3)
		mac_printf("\tReserved\n");

	val = MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, RXFCS);
	mac_printf("RX direction FCS\n");

	if (val == MODE0)
		mac_printf("\tPacket does not have FCS and FCS "
			   "is not removed\n");
	else if (val == MODE1)
		mac_printf("\tReserved\n");
	else if (val == MODE2)
		mac_printf("\tPacket has FCS and FCS is not removed\n");
	else if (val == MODE3)
		mac_printf("\tPacket has FCS and FCS is removed\n");

	val = MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, TXSPTAG);
	mac_printf("TX Special Tag\n");

	if (val == MODE0)
		mac_printf("\tPacket does not have Special Tag and "
			   "Special Tag is not removed\n");
	else if (val == MODE1)
		mac_printf("\tPacket has Special Tag and "
			   "Special Tag is replaced\n");
	else if (val == MODE2)
		mac_printf("\tPacket has Special Tag and "
			   "Special Tag is not removed\n");
	else if (val == MODE3)
		mac_printf("\tPacket has Special Tag and "
			   "Special Tag is removed\n");

	val = MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, RXSPTAG);
	mac_printf("RX Special Tag\n");

	if (val == MODE0)
		mac_printf("\tPacket does not have Special Tag and "
			   "Special Tag is not inserted\n");
	else if (val == MODE1)
		mac_printf("\tPacket does not have Special Tag and "
			   "Special Tag is inserted\n");
	else if (val == MODE2)
		mac_printf("\tPacket has Special Tag and "
			   "Special Tag is not inserted\n");
	else if (val == MODE3)
		mac_printf("\tReserved\n");

	val = MAC_GET_VAL(mac_op_cfg, MAC_OP_CFG, RXTIME);
	mac_printf("RX Direction Timestamp\n");

	if (val == MODE0)
		mac_printf("\tPacket does not have time stamp and "
			   "time stamp is not inserted.\n");
	else if (val == MODE1)
		mac_printf("\tPacket doe not have time stamp and "
			   "time stamp is inserted\n");
	else if (val == MODE2)
		mac_printf("\tPacket has time stamp and "
			   "time stamp is not inserted\n");
	else if (val == MODE3)
		mac_printf("\tReserved\n");
}

int gswss_set_mtu(void *pdev, u32 mtu)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int ret = 0;

	if (GSWSS_MAC_RGRD(pdata, MAC_MTU_CFG(pdata->mac_idx)) != mtu) {
		mac_printf("GSWSS: MTU set to %d\n", mtu);
		GSWSS_MAC_RGWR(pdata, MAC_MTU_CFG(pdata->mac_idx), mtu);
	}

	return ret;
}

int gswss_get_mtu(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mtu = GSWSS_MAC_RGRD(pdata, MAC_MTU_CFG(pdata->mac_idx));

	mac_printf("GSWSS: MAC%d MTU %d\n", (pdata->mac_idx + 2), mtu);

	return mtu;
}

int gswss_set_txtstamp_fifo(void *pdev,
			    struct mac_fifo_entry *f_entry)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int ret = 0;
	u16 val = 0;
	u32 mac_txtstamp;

	val = ((f_entry->ttse << 4) | (f_entry->ostc << 3) |
	       (f_entry->ostpa << 2) | f_entry->cic);
	GSWSS_MAC_RGWR(pdata, MAC_TXTS_CIC(pdata->mac_idx), val);

	mac_txtstamp = MAC_TXTS_0(pdata->mac_idx);
	val = GET_N_BITS(f_entry->ttsh, 0, 16);
	GSWSS_MAC_RGWR(pdata, mac_txtstamp, val);

	mac_txtstamp = MAC_TXTS_1(pdata->mac_idx);
	val = GET_N_BITS(f_entry->ttsh, 16, 16);
	GSWSS_MAC_RGWR(pdata, mac_txtstamp, val);

	mac_txtstamp = MAC_TXTS_2(pdata->mac_idx);
	val = GET_N_BITS(f_entry->ttsl, 0, 16);
	GSWSS_MAC_RGWR(pdata, mac_txtstamp, val);

	mac_txtstamp = MAC_TXTS_3(pdata->mac_idx);
	val = GET_N_BITS(f_entry->ttsl, 16, 16);
	GSWSS_MAC_RGWR(pdata, mac_txtstamp, val);

	mac_dbg("MAC%d: TxTstamp Fifo Record ID %d written\n",
		(pdata->mac_idx + 2), f_entry->rec_id);

	/* Write the entries into the 64 array record_id */
	gswss_set_txtstamp_access(pdev, 1, f_entry->rec_id);

	return ret;
}

void gswss_get_txtstamp_fifo(void *pdev,
			     u32 record_id, struct mac_fifo_entry *f_entry)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_txtstamp;

	gswss_set_txtstamp_access(pdev, 0, record_id);

	mac_dbg("\nMAC%d: TxTstamp Fifo Record ID %d:\n",
		(pdata->mac_idx + 2),
		record_id);

	mac_txtstamp = GSWSS_MAC_RGRD(pdata, MAC_TXTS_CIC(pdata->mac_idx));

	f_entry->ttse = GET_N_BITS(mac_txtstamp, 4, 1);
	f_entry->ostc = GET_N_BITS(mac_txtstamp, 3, 1);
	f_entry->ostpa = GET_N_BITS(mac_txtstamp, 2, 1);
	f_entry->cic = GET_N_BITS(mac_txtstamp, 0, 2);

	f_entry->ttsl =
		((GSWSS_MAC_RGRD(pdata, MAC_TXTS_1(pdata->mac_idx)) << 16) |
		 GSWSS_MAC_RGRD(pdata, MAC_TXTS_0(pdata->mac_idx)));
	f_entry->ttsh =
		((GSWSS_MAC_RGRD(pdata, MAC_TXTS_3(pdata->mac_idx)) << 16) |
		 GSWSS_MAC_RGRD(pdata, MAC_TXTS_2(pdata->mac_idx)));

	f_entry->rec_id = record_id;


	mac_dbg("\tTTSE: \t%s\n",
		f_entry->ttse ? "ENABLED" : "DISABLED");
	mac_dbg("\tOSTC: \t%s\n",
		f_entry->ostc ? "ENABLED" : "DISABLED");
	mac_dbg("\tOSTPA: \t%s\n",
		f_entry->ostpa ? "ENABLED" : "DISABLED");

	if (f_entry->cic == 0)
		mac_dbg("\tCIC: \t"
			"DISABLED\n");

	if (f_entry->cic == 1)
		mac_dbg("\tCIC: \tTime stamp IP Checksum update\n");

	if (f_entry->cic == 2)
		mac_dbg("\tCIC: \tTime stamp IP and "
			"Payload Checksum update\n");

	if (f_entry->cic == 3)
		mac_dbg("\tCIC: \tTime stamp IP, Payload checksum and "
			"Pseudo header update\n");

	mac_dbg("\tTTSL: \t%d\n", f_entry->ttsl);
	mac_dbg("\tTTSH: \t%d\n", f_entry->ttsh);
}

int gswss_set_txtstamp_access(void *pdev, u32 op_mode, u32 addr)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int ret = 0;
	u16 tstamp_acc = 0;
	u32 busy_bit;

	MAC_SET_VAL(tstamp_acc, MAC_TXTS_ACC, BAS, 1);
	MAC_SET_VAL(tstamp_acc, MAC_TXTS_ACC, OPMOD, op_mode);
	MAC_SET_VAL(tstamp_acc, MAC_TXTS_ACC, ADDR, addr);

	GSWSS_MAC_RGWR(pdata, MAC_TXTS_ACC(pdata->mac_idx), tstamp_acc);

	while (1) {
		tstamp_acc = GSWSS_MAC_RGRD(pdata,
					    MAC_TXTS_ACC(pdata->mac_idx));
		busy_bit = MAC_GET_VAL(tstamp_acc, MAC_TXTS_ACC, BAS);

		if (busy_bit == 0)
			break;
	}

	return ret;
}

int gswss_set_duplex_mode(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	if (MAC_GET_VAL(phy_mode, PHY_MODE, FDUP) != val) {
		if (val == MAC_AUTO_DPLX)
			mac_printf("\tGSWSS: Duplex mode set: Auto Mode\n");
		else if (val == MAC_FULL_DPLX)
			mac_printf("\tGSWSS: Duplex mode set: Full Duplex\n");
		else if (val == MAC_RES_DPLX)
			mac_printf("\tGSWSS: Duplex mode set: Reserved\n");
		else if (val == MAC_HALF_DPLX)
			mac_printf("\tGSWSS: Duplex mode set: Half Duplex\n");

		MAC_SET_VAL(phy_mode, PHY_MODE, FDUP, val);
		GSWSS_MAC_RGWR(pdata, PHY_MODE(pdata->mac_idx), phy_mode);
	}

	return 0;
}

int gswss_get_duplex_mode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;
	u32 val;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));
	val = MAC_GET_VAL(phy_mode, PHY_MODE, FDUP);

	if (val == MAC_AUTO_DPLX)
		mac_printf("\tGSWSS: Duplex mode got: Auto Mode\n");
	else if (val == MAC_FULL_DPLX)
		mac_printf("\tGSWSS: Duplex mode got: Full Duplex\n");
	else if (val == MAC_RES_DPLX)
		mac_printf("\tGSWSS: Duplex mode got: Reserved\n");
	else if (val == MAC_HALF_DPLX)
		mac_printf("\tGSWSS: Duplex mode got: Half Duplex\n");

	return val;
}

int gswss_set_speed(void *pdev, u8 speed)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;
	u8 speed_msb = 0, speed_lsb = 0;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	speed_msb = GET_N_BITS(speed, 2, 1);
	speed_lsb = GET_N_BITS(speed, 0, 2);

	if (speed == SPEED_10M)
		mac_printf("\tGSWSS: SPEED    10 Mbps\n");
	else if (speed == SPEED_100M)
		mac_printf("\tGSWSS: SPEED    100 Mbps\n");
	else if (speed == SPEED_1G)
		mac_printf("\tGSWSS: SPEED    1 Gbps\n");
	else if (speed == SPEED_10G)
		mac_printf("\tGSWSS: SPEED    10 Gbps\n");
	else if (speed == SPEED_2G5)
		mac_printf("\tGSWSS: SPEED    2.5 Gbps\n");
	else if (speed == SPEED_5G)
		mac_printf("\tGSWSS: SPEED    5 Gbps\n");
	else if (speed == SPEED_FLEX)
		mac_printf("\tGSWSS: SPEED    RESERVED\n");
	else if (speed == SPEED_AUTO)
		mac_printf("\tGSWSS: SPEED    Auto Mode\n");

	MAC_SET_VAL(phy_mode, PHY_MODE, SPEEDMSB, speed_msb);
	MAC_SET_VAL(phy_mode, PHY_MODE, SPEEDLSB, speed_lsb);

	GSWSS_MAC_RGWR(pdata, PHY_MODE(pdata->mac_idx), phy_mode);

	return 0;
}

u8 gswss_get_speed(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;
	u8 speed_msb, speed_lsb, speed;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	speed_msb = MAC_GET_VAL(phy_mode, PHY_MODE, SPEEDMSB);
	speed_lsb = MAC_GET_VAL(phy_mode, PHY_MODE, SPEEDLSB);

	speed = ((speed_msb << 2) | speed_lsb);

	if (speed == SPEED_10M)
		mac_printf("\tGSWSS: SPEED Got   10 Mbps\n");
	else if (speed == SPEED_100M)
		mac_printf("\tGSWSS: SPEED Got   100 Mbps\n");
	else if (speed == SPEED_1G)
		mac_printf("\tGSWSS: SPEED Got   1 Gbps\n");
	else if (speed == SPEED_10G)
		mac_printf("\tGSWSS: SPEED Got   10 Gbps\n");
	else if (speed == SPEED_2G5)
		mac_printf("\tGSWSS: SPEED Got   2.5 Gbps\n");
	else if (speed == SPEED_5G)
		mac_printf("\tGSWSS: SPEED Got   5 Gbps\n");
	else if (speed == SPEED_FLEX)
		mac_printf("\tGSWSS: SPEED Got   RESERVED\n");
	else if (speed == SPEED_AUTO)
		mac_printf("\tGSWSS: SPEED Got   Auto Mode\n");

	return speed;
}

int gswss_set_linkstatus(void *pdev, u8 linkst)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	if (MAC_GET_VAL(phy_mode, PHY_MODE, LINKST) != linkst) {
		if (linkst == 0)
			mac_printf("\tGSWSS: LINK STS: Auto Mode\n");
		else if (linkst == 1)
			mac_printf("\tGSWSS: LINK STS: Forced UP\n");
		else if (linkst == 2)
			mac_printf("\tGSWSS: LINK STS: Forced DOWN\n");
		else if (linkst == 3)
			mac_printf("\tGSWSS: LINK STS: Reserved\n");

		MAC_SET_VAL(phy_mode, PHY_MODE, LINKST, linkst);
		GSWSS_MAC_RGWR(pdata, PHY_MODE(pdata->mac_idx), phy_mode);
	}

	return 0;
}

int gswss_get_linkstatus(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;
	int linkst;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	linkst = MAC_GET_VAL(phy_mode, PHY_MODE, LINKST);

	if (linkst == 0)
		mac_printf("\tGSWSS: LINK STS Got: Auto Mode\n");
	else if (linkst == 1)
		mac_printf("\tGSWSS: LINK STS Got: Forced UP\n");
	else if (linkst == 2)
		mac_printf("\tGSWSS: LINK STS Got: Forced DOWN\n");
	else if (linkst == 3)
		mac_printf("\tGSWSS: LINK STS Got: Reserved\n");

	return linkst;
}

int gswss_set_flowctrl_tx(void *pdev, u8 flow_ctrl_tx)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int ret = 0;
	u16 phy_mode = 0;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	if (MAC_GET_VAL(phy_mode, PHY_MODE, FCONTX) != flow_ctrl_tx) {
		if (flow_ctrl_tx == 0)
			mac_printf("\tGSWSS: Flow Ctrl Mode TX: Auto Mode\n");
		else if (flow_ctrl_tx == 1)
			mac_printf("\tGSWSS: Flow Ctrl Mode TX: ENABLED\n");
		else if (flow_ctrl_tx == 2)
			mac_printf("\tGSWSS: Flow Ctrl Mode TX: Reserved\n");
		else if (flow_ctrl_tx == 3)
			mac_printf("\tGSWSS: Flow Ctrl Mode TX: DISABLED\n");

		MAC_SET_VAL(phy_mode, PHY_MODE, FCONTX, flow_ctrl_tx);
		GSWSS_MAC_RGWR(pdata, PHY_MODE(pdata->mac_idx), phy_mode);
	}

	return ret;
}

u32 gswss_get_flowctrl_tx(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;
	u32 flow_ctrl_tx;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	flow_ctrl_tx = MAC_GET_VAL(phy_mode, PHY_MODE, FCONTX);

	if (flow_ctrl_tx == 0)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: Auto Mode\n");
	else if (flow_ctrl_tx == 1)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: ENABLED\n");
	else if (flow_ctrl_tx == 2)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: Reserved\n");
	else if (flow_ctrl_tx == 3)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: DISABLED\n");

	return flow_ctrl_tx;
}

int gswss_set_flowctrl_rx(void *pdev, u8 flow_ctrl_rx)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int ret = 0;
	u16 phy_mode = 0;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	if (MAC_GET_VAL(phy_mode, PHY_MODE, FCONRX) != flow_ctrl_rx) {
		if (flow_ctrl_rx == 0)
			mac_printf("\tGSWSS: Flow Ctrl Mode RX: Auto Mode\n");
		else if (flow_ctrl_rx == 1)
			mac_printf("\tGSWSS: Flow Ctrl Mode RX: ENABLED\n");
		else if (flow_ctrl_rx == 2)
			mac_printf("\tGSWSS: Flow Ctrl Mode RX: Reserved\n");
		else if (flow_ctrl_rx == 3)
			mac_printf("\tGSWSS: Flow Ctrl Mode RX: DISABLED\n");

		MAC_SET_VAL(phy_mode, PHY_MODE, FCONRX, flow_ctrl_rx);
		GSWSS_MAC_RGWR(pdata, PHY_MODE(pdata->mac_idx), phy_mode);
	}

	return ret;
}

u32 gswss_get_flowctrl_rx(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 phy_mode = 0;
	u32 flow_ctrl_rx;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));

	flow_ctrl_rx = MAC_GET_VAL(phy_mode, PHY_MODE, FCONRX);

	if (flow_ctrl_rx == 0)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX: Auto Mode\n");
	else if (flow_ctrl_rx == 1)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX:  ENABLED\n");
	else if (flow_ctrl_rx == 2)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX:  Reserved\n");
	else if (flow_ctrl_rx == 3)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX:  DISABLED\n");

	return flow_ctrl_rx;
}

void gswss_get_phy2mode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int i = 0;
	u32 phy_mode;
	u8 speed_msb, speed_lsb, speed;
	u8 linkst, fdup, flow_ctrl_tx, flow_ctrl_rx;

	phy_mode = GSWSS_MAC_RGRD(pdata, PHY_MODE(pdata->mac_idx));
	mac_printf("phy_mode%d %d\n", (i + 2), phy_mode);
	speed_msb = GET_N_BITS(phy_mode, 15, 1);
	speed_lsb = GET_N_BITS(phy_mode, 11, 2);
	speed = (speed_msb << 2) | speed_lsb;

	if (speed == 0)
		mac_printf("\tGSWSS: SPEED    10 Mbps\n");
	else if (speed == 1)
		mac_printf("\tGSWSS: SPEED    100 Mbps\n");
	else if (speed == 2)
		mac_printf("\tGSWSS: SPEED    1 Gbps\n");
	else if (speed == 3)
		mac_printf("\tGSWSS: SPEED    10 Gbps\n");
	else if (speed == 4)
		mac_printf("\tGSWSS: SPEED    2.5 Gbps\n");
	else if (speed == 5)
		mac_printf("\tGSWSS: SPEED    5 Gbps\n");
	else if (speed == 6)
		mac_printf("\tGSWSS: SPEED    RESERVED\n");
	else if (speed == 7)
		mac_printf("\tGSWSS: SPEED    Auto Mode\n");

	linkst = GET_N_BITS(phy_mode, 13, 2);

	if (linkst == 0)
		mac_printf("\tGSWSS: LINK STS: Auto Mode\n");
	else if (linkst == 1)
		mac_printf("\tGSWSS: LINK STS: Forced up\n");
	else if (linkst == 2)
		mac_printf("\tGSWSS: LINK STS: Forced down\n");
	else if (linkst == 3)
		mac_printf("\tGSWSS: LINK STS: Reserved\n");

	fdup = GET_N_BITS(phy_mode, 9, 2);

	if (fdup == 0)
		mac_printf("\tGSWSS: Duplex mode set: Auto Mode\n");
	else if (fdup == 1)
		mac_printf("\tGSWSS: Duplex mode set: Full Duplex Mode\n");
	else if (fdup == 2)
		mac_printf("\tGSWSS: Duplex mode set: Reserved\n");
	else if (fdup == 3)
		mac_printf("\tGSWSS: Duplex mode set: Half Duplex Mode\n");

	flow_ctrl_tx = GET_N_BITS(phy_mode, 7, 2);

	if (flow_ctrl_tx == 0)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: Auto Mode\n");
	else if (flow_ctrl_tx == 1)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: ENABLED\n");
	else if (flow_ctrl_tx == 2)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: Reserved\n");
	else if (flow_ctrl_tx == 3)
		mac_printf("\tGSWSS: Flow Ctrl Mode TX: DISABLED\n");

	flow_ctrl_rx = GET_N_BITS(phy_mode, 7, 2);

	if (flow_ctrl_rx == 0)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX: Auto Mode\n");
	else if (flow_ctrl_rx == 1)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX: ENABLED\n");
	else if (flow_ctrl_rx == 2)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX: Reserved\n");
	else if (flow_ctrl_rx == 3)
		mac_printf("\tGSWSS: Flow Ctrl Mode RX: DISABLED\n");
}

int gswss_set_xgmac_tx_disable(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 xgmac_ctrl = 0;

	xgmac_ctrl = GSWSS_MAC_RGRD(pdata, XGMAC_CTRL(pdata->mac_idx));

	if (MAC_GET_VAL(xgmac_ctrl, XGMAC_CTRL, DISTX) != val) {
		mac_printf("\tGSWSS: XGMAC %d TX %s\n", pdata->mac_idx,
			   val ? "DISABLED" : "NOT DISABLED");
		MAC_SET_VAL(xgmac_ctrl, XGMAC_CTRL, DISTX, val);
		GSWSS_MAC_RGWR(pdata, XGMAC_CTRL(pdata->mac_idx), xgmac_ctrl);
	}

	return 0;
}

int gswss_set_xgmac_rx_disable(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 xgmac_ctrl = 0;

	xgmac_ctrl = GSWSS_MAC_RGRD(pdata, XGMAC_CTRL(pdata->mac_idx));

	if (MAC_GET_VAL(xgmac_ctrl, XGMAC_CTRL, DISRX) != val) {
		mac_printf("\tGSWSS: XGMAC %d RX %s\n", pdata->mac_idx,
			   val ? "DISABLED" : "NOT DISABLED");
		MAC_SET_VAL(xgmac_ctrl, XGMAC_CTRL, DISRX, val);
		GSWSS_MAC_RGWR(pdata, XGMAC_CTRL(pdata->mac_idx), xgmac_ctrl);
	}

	return 0;
}

int gswss_set_xgmac_crc_ctrl(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 xgmac_ctrl = 0;

	xgmac_ctrl = GSWSS_MAC_RGRD(pdata, XGMAC_CTRL(pdata->mac_idx));

	if (MAC_GET_VAL(xgmac_ctrl, XGMAC_CTRL, CPC) != val) {
		if (val == 0)
			mac_printf("GSWSS: CRC and PAD insertion are "
				   "enabled.\n");
		else if (val == 1)
			mac_printf("GSWSS: CRC insert enable,PAD insert "
				   "disable\n");
		else if (val == 2)
			mac_printf("GSWSS: CRC,PAD not inserted not "
				   "replaced.\n");

		MAC_SET_VAL(xgmac_ctrl, XGMAC_CTRL, CPC, val);
		GSWSS_MAC_RGWR(pdata, XGMAC_CTRL(pdata->mac_idx), xgmac_ctrl);
	}

	return 0;
}

int gswss_set_eee_cap(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 aneg_eee = 0;

	aneg_eee = GSWSS_MAC_RGRD(pdata, ANEG_EEE(pdata->mac_idx));
	MAC_SET_VAL(aneg_eee, ANEG_EEE, EEE_CAPABLE, val);
	GSWSS_MAC_RGWR(pdata, ANEG_EEE(pdata->mac_idx), aneg_eee);

	return 0;
}

int gswss_get_xgmac_crc_ctrl(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 xgmac_ctrl = 0, val = 0;

	xgmac_ctrl = GSWSS_MAC_RGRD(pdata, XGMAC_CTRL(pdata->mac_idx));

	val = MAC_GET_VAL(xgmac_ctrl, XGMAC_CTRL, CPC);

	if (val == 0)
		mac_printf("GSWSS: CRC and PAD insertion are enabled.\n");
	else if (val == 1)
		mac_printf("GSWSS: CRC insert enable,PAD insert disable\n");
	else if (val == 2)
		mac_printf("GSWSS: CRC,PAD not inserted not replaced.\n");

	return val;
}

void gswss_get_xgmac_ctrl(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 xgmac_ctrl = 0, distx, disrx, crc_ctrl;
	u32 mac_idx = 0;

	xgmac_ctrl = GSWSS_MAC_RGRD(pdata, XGMAC_CTRL(pdata->mac_idx));
	mac_printf("GSWSS: XGMAC CTRL %d %x\n", mac_idx, xgmac_ctrl);
	distx = MAC_GET_VAL(xgmac_ctrl, XGMAC_CTRL, DISTX);
	disrx = MAC_GET_VAL(xgmac_ctrl, XGMAC_CTRL, DISRX);
	crc_ctrl = MAC_GET_VAL(xgmac_ctrl, XGMAC_CTRL, CPC);

	mac_printf("\tGSWSS: XGMAC %d TX %s\n", mac_idx,
		   distx ? "DISABLED" : "NOT DISABLED");
	mac_printf("\tGSWSS: XGMAC %d RX %s\n", mac_idx,
		   disrx ? "DISABLED" : "NOT DISABLED");

	if (crc_ctrl == 0)
		mac_printf("\tGSWSS: CRC and PAD insertion are enabled.\n");
	else if (crc_ctrl == 1)
		mac_printf("\tGSWSS: CRC insert enable,PAD insert disable\n");
	else if (crc_ctrl == 2)
		mac_printf("\tGSWSS: CRC,PAD not inserted not replaced.\n");
}

void gswss_test_all_reg(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	gswss_check_reg(pdev, GSWIPSS_IER0, "GSWIPSS_IER0", 0, 0x8C8C, 0);
	gswss_check_reg(pdev, GSWIPSS_ISR0, "GSWIPSS_ISR0", 0, 0x8C8C, 0);
	gswss_check_reg(pdev, GSWIPSS_IER1, "GSWIPSS_IER1", 0, 0x8C, 0);
	gswss_check_reg(pdev, GSWIPSS_ISR1, "GSWIPSS_ISR1", 0, 0x8C, 0);
	gswss_check_reg(pdev, CFG0_1588, "CFG0_1588", 0, 0x7777, 0);
	gswss_check_reg(pdev, CFG1_1588, "CFG1_1588", 0, 0xFF80, 0);

	gswss_check_reg(pdev, MAC_IF_CFG(pdata->mac_idx),
			"MAC_IF_CFG", pdata->mac_idx, 0x7, 0);
	gswss_check_reg(pdev, MAC_OP_CFG(pdata->mac_idx),
			"MAC_OP_CFG", pdata->mac_idx, 0x1FF, 0);
	gswss_check_reg(pdev, MAC_MTU_CFG(pdata->mac_idx),
			"MAC_MTU_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_CFG(pdata->mac_idx),
			"MAC_GINT_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_HD0_CFG(pdata->mac_idx),
			"MAC_GINT_HD0_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_HD1_CFG(pdata->mac_idx),
			"MAC_GINT_HD1_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_HD2_CFG(pdata->mac_idx),
			"MAC_GINT_HD2_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_HD3_CFG(pdata->mac_idx),
			"MAC_GINT_HD3_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_HD4_CFG(pdata->mac_idx),
			"MAC_GINT_HD4_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_HD5_CFG(pdata->mac_idx),
			"MAC_GINT_HD5_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_GINT_HD6_CFG(pdata->mac_idx),
			"MAC_GINT_HD6_CFG", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_TXTS_0(pdata->mac_idx),
			"MAC_TXTS_0", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_TXTS_1(pdata->mac_idx),
			"MAC_TXTS_1", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_TXTS_2(pdata->mac_idx),
			"MAC_TXTS_2", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_TXTS_3(pdata->mac_idx),
			"MAC_TXTS_3", pdata->mac_idx, 0xFFFF, 0);
	gswss_check_reg(pdev, MAC_TXTS_CIC(pdata->mac_idx),
			"MAC_TXTS_CIC", pdata->mac_idx, 0x1F, 0);
	gswss_check_reg(pdev, MAC_TXTS_ACC(pdata->mac_idx),
			"MAC_TXTS_ACC", pdata->mac_idx, 0x403F, 0);
	gswss_check_reg(pdev, PHY_MODE(pdata->mac_idx),
			"PHY_MODE", pdata->mac_idx, 0xFFE0, 0);
	gswss_check_reg(pdev, ANEG_EEE(pdata->mac_idx),
			"ANEG_EEE", pdata->mac_idx, 0xF, 0);
	gswss_check_reg(pdev, XGMAC_CTRL(pdata->mac_idx),
			"XGMAC_CTRL", pdata->mac_idx, 0xF, 0);
}

void gswss_check_reg(void *pdev, u32 reg, char *name, int idx,
		     u16 set_val, u16 clr_val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 val;

	GSWSS_MAC_RGWR(pdata, reg, set_val);
	val = GSWSS_MAC_RGRD(pdata, reg);

	if (val != set_val)
		mac_printf("Setting reg %s: %d with %x FAILED\n",
			   name, idx, set_val);

	GSWSS_MAC_RGWR(pdata, reg, clr_val);

	if (val != clr_val)
		mac_printf("Setting reg %s: %d with %x FAILED\n",
			   name, idx, clr_val);
}

#if defined(PC_UTILITY) || defined(CHIPTEST)
struct adap_ops *gsw_get_adap_ops(u32 devid)
{
	return &(adap_priv_data.ops);
}
#endif
