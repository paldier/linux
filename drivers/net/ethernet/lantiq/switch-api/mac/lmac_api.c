/******************************************************************************
 *                Copyright (c) 2016, 2017 Intel Corporation
 *
 *
 * For licensing information, see the file 'LICENSE' in the root folder of
 * this software module.
 *
 ******************************************************************************/

#include <lmac_api.h>

static u32 read_lmac_cnt(void *pdev);
static u32 write_lmac_cnt(void *pdev, u32 val);

struct _lmac_cfg {
	char cmdname[256];
	u8 args;
	char help[1024];
};

struct _lmac_cfg lmac_cfg[] = {
	{
		"rmon",
		0,
		"lmac <0/1/2: Mac Idx> get rmon"
	},
	{
		"all",
		0,
		"lmac <0/1/2: Mac Idx> get all"
	},
	{
		"w",
		2,
		"lmac <0/1/2: Mac Idx> w <reg_off> <reg_val>"
	},
	{
		"r",
		1,
		"lmac <0/1/2: Mac Idx> r <reg_off>"
	},
};

void lmac_help(void)
{
	int i = 0;
	int num_of_elem = (sizeof(lmac_cfg) / sizeof(struct _lmac_cfg));

	mac_printf("\n----Legacy MAC Commands----\n\n");

	for (i = 0; i < num_of_elem; i++) {
		if (lmac_cfg[i].help) {

#if defined(CHIPTEST) && CHIPTEST
			mac_printf("gsw %s\n",
				   lmac_cfg[i].help);
#else
			mac_printf("switch_cli %s\n",
				   lmac_cfg[i].help);
#endif
		}
	}
}

int lmac_check_args(int argc, char *argv)
{
	int i = 0;
	int num_of_elem = (sizeof(lmac_cfg) / sizeof(struct _lmac_cfg));

	for (i = 0; i < num_of_elem; i++) {
		if (!strcmp(argv, lmac_cfg[i].cmdname)) {

			if (argc != (lmac_cfg[i].args + 4)) {
				mac_printf("\n--WRONG Command--\n");
				mac_printf("switch_cli %s\n",
					   lmac_cfg[i].help);
				return -1;
			}
		}
	}

	return 0;
}

void lmac_wr_reg(void *pdev, u32 reg_off, u32 reg_val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	mac_printf("\tREG offset: 0x%04x\n\tData: %08X\n", reg_off,
		   reg_val);

	LMAC_RGWR(pdata, reg_off, reg_val);
}

u32 lmac_rd_reg(void *pdev, u32 reg_off)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	pdata->reg_val = LMAC_RGRD(pdata, reg_off);

	mac_printf("\tREG offset: 0x%04x\n\tData: %08X\n", reg_off,
		   pdata->reg_val);

	return pdata->reg_val;
}

int lmac_main(u32 argc, u8 *argv[])
{
	int idx = 0;
	u32 start_arg = 0;
	u32 mode_en, lpi_waitg, lpi_waitm;
	struct mac_ops *ops;
	u32 reg_off, reg_val;


	start_arg++;
	start_arg++;


	if (argc <= 2) {
		lmac_help();
		goto end;
	}

	if (!strcmp(argv[start_arg], "-help")) {
		lmac_help();

		goto end;
	}

	idx = mac_nstrtoul(argv[start_arg],
			   mac_nstrlen(argv[start_arg]),
			   &start_arg);

	if (idx > 2 || idx < 0) {
		mac_printf("Give valid lmac index 0/1/2/\n");
		return -1;
	}

	ops = gsw_get_mac_ops(0, idx);

	if (!strcmp(argv[start_arg], "r")) {
		if (0 != lmac_check_args(argc, (char *)argv[start_arg])) {
			return -1;
		}

		start_arg++;
#if defined(PC_UTILITY) || defined(__KERNEL__)

		if ((strstr(argv[start_arg], "0x")) ||
		    (strstr(argv[start_arg], "0X")))
			mac_printf("matches with 0x\n");
		else
			mac_printf("Please give the address with "
				   "0x firmat\n");

#endif
		reg_off = mac_nstrtoul(argv[start_arg],
				       mac_nstrlen(argv[start_arg]),
				       &start_arg);
		lmac_rd_reg(ops, reg_off);

		goto end;
	}

	if (!strcmp(argv[start_arg], "w")) {
		if (0 != lmac_check_args(argc, (char *)argv[start_arg])) {
			return -1;
		}

		start_arg++;

#if defined(PC_UTILITY) || defined(__KERNEL__)

		if ((strstr(argv[start_arg], "0x")) ||
		    (strstr(argv[start_arg], "0X")))
			mac_printf("matches with 0x\n");
		else
			mac_printf("Please give the address with "
				   "0x format\n");

#endif
		reg_off = mac_nstrtoul(argv[start_arg],
				       mac_nstrlen(argv[start_arg]),
				       &start_arg);
		reg_val = mac_nstrtoul(argv[start_arg],
				       mac_nstrlen(argv[start_arg]),
				       &start_arg);
		lmac_wr_reg(ops, reg_off, reg_val);

		goto end;
	}

	if (!strcmp(argv[start_arg], "get")) {
		start_arg++;

		if (!strcmp(argv[start_arg], "rmon")) {
			lmac_get_rmon();
			goto end;
		} else if (!strcmp(argv[start_arg], "all")) {

			lmac_get_intf_mode(ops);
			lmac_get_duplex_mode(ops);
			lmac_get_txfcs(ops);
			lmac_get_flowcon_mode(ops);
			lmac_get_ipg(ops);
			lmac_get_preamble(ops);
			lmac_get_defermode(ops);
			lmac_get_jps(ops);
			lmac_get_loopback(ops);
			lmac_get_txer(ops);
			lmac_get_lpimonitor_mode(ops);
			lmac_get_lpi(ops, &mode_en,
				     &lpi_waitg, &lpi_waitm);
			lmac_get_mac_pstat(ops);
			lmac_get_mac_pisr(ops);

			lmac_get_pauseframe_samode(gsw_get_mac_ops(0, 0));
		} else {
			lmac_help();
		}
	} else {
		lmac_help();
	}

end:
	return 0;
}

int lmac_set_intf_mode(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, GMII) != val) {
		if (val == 0)
			mac_printf("LMAC %d Intf: AUTO\n",
				   pdata->mac_idx);
		else if (val == 1)
			mac_printf("LMAC %d Intf: MII(10/100/200 Mbps)\n",
				   pdata->mac_idx);
		else if (val == 2)
			mac_printf("LMAC %d Intf: GMII (1000 Mbps)\n",
				   pdata->mac_idx);
		else if (val == 3)
			mac_printf("LMAC %d Intf: GMII_2G (2000 Mbps)\n",
				   pdata->mac_idx);

		MAC_SET_VAL(mac_ctrl0, MAC_CTRL0, GMII, val);

		LMAC_RGWR(pdata, MAC_CTRL0(pdata->mac_idx), mac_ctrl0);
	}

	return 0;
}

int lmac_get_intf_mode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));
	u32 val = 0;

	mac_printf("LMAC %d INTF MODE %08x\n", pdata->mac_idx, mac_ctrl0);
	val = MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, GMII);

	if (val == 0)
		mac_printf("\tIntf mode set to : AUTO\n");
	else if (val == 1)
		mac_printf("\tIntf mode set to : MII (10/100/200 Mbps)\n");
	else if (val == 2)
		mac_printf("\tIntf mode set to : GMII (1000 Mbps)\n");
	else if (val == 3)
		mac_printf("\tIntf mode set to : GMII_2G (2000 Mbps)\n");

	return 0;
}

int lmac_set_duplex_mode(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, FDUP) != val) {
		if (val == 0)
			mac_printf("LMAC %d FDUP set to: AUTO\n",
				   pdata->mac_idx);
		else if (val == 1)
			mac_printf("LMAC %d FDUP set to: Full Duplex Mode\n",
				   pdata->mac_idx);
		else if (val == 2)
			mac_printf("LMAC %d FDUP set to: Reserved\n",
				   pdata->mac_idx);
		else if (val == 3)
			mac_printf("LMAC %d FDUP set to: Half Duplex Mode\n",
				   pdata->mac_idx);

		MAC_SET_VAL(mac_ctrl0, MAC_CTRL0, FDUP, val);

		LMAC_RGWR(pdata, MAC_CTRL0(pdata->mac_idx), mac_ctrl0);
	}

	return 0;
}

int lmac_get_duplex_mode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d DUPLEX MODE %08x\n", pdata->mac_idx, mac_ctrl0);

	val = MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, FDUP);

	if (val == 0)
		mac_printf("\tFDUP mode set to : AUTO\n");
	else if (val == 1)
		mac_printf("\tFDUP mode set to : Full Duplex Mode\n");
	else if (val == 2)
		mac_printf("\tFDUP mode set to : Reserved\n");
	else if (val == 3)
		mac_printf("\tFDUP mode set to : Half Duplex Mode\n");

	return val;
}

int lmac_set_flowcon_mode(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, FCON) != val) {
		if (val == 0)
			mac_printf("LMAC %d FCON mode set to : AUTO\n",
				   pdata->mac_idx);
		else if (val == 1)
			mac_printf("LMAC %d FCON mode set to : RX only\n",
				   pdata->mac_idx);
		else if (val == 2)
			mac_printf("LMAC %d FCON mode set to : TX only\n",
				   pdata->mac_idx);
		else if (val == 3)
			mac_printf("LMAC %d FCON mode set to : RXTX \n",
				   pdata->mac_idx);
		else if (val == 4)
			mac_printf("LMAC %d FCON mode set to : DISABLED\n",
				   pdata->mac_idx);

		MAC_SET_VAL(mac_ctrl0, MAC_CTRL0, FCON, val);

		LMAC_RGWR(pdata, MAC_CTRL0(pdata->mac_idx), mac_ctrl0);
	}

	return 0;
}

u32 lmac_get_flowcon_mode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d FLOWCONTROL MODE %08x\n",
		   pdata->mac_idx, mac_ctrl0);

	val = MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, FCON);

	if (val == 0)
		mac_printf("\tFCON mode set to : AUTO\n");
	else if (val == 1)
		mac_printf("\tFCON mode set to : Receive only\n");
	else if (val == 2)
		mac_printf("\tFCON mode set to : transmit only\n");
	else if (val == 3)
		mac_printf("\tFCON mode set to : RXTX\n");
	else if (val == 4)
		mac_printf("\tFCON mode set to : DISABLED\n");

	return val;
}

int lmac_set_txfcs(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, FCS) != val) {
		mac_printf("LMAC %d FCS generation : %s\n", pdata->mac_idx,
			   val ? "ENABLED" : "DISABLED");
		MAC_SET_VAL(mac_ctrl0, MAC_CTRL0, FCS, val);

		LMAC_RGWR(pdata, MAC_CTRL0(pdata->mac_idx), mac_ctrl0);
	}

	return 0;
}

int lmac_get_txfcs(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl0 = LMAC_RGRD(pdata, MAC_CTRL0(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d FCS %08x\n", pdata->mac_idx, mac_ctrl0);

	val = MAC_GET_VAL(mac_ctrl0, MAC_CTRL0, FCS);
	mac_printf("\tFCS generation : %s\n", val ? "ENABLED" : "DISABLED");

	return val;
}

int lmac_set_int(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 lmac_ier = LMAC_RGRD(pdata, LMAC_IER);

	pdata->mac_idx += LMAC_IER_MAC2_POS;
	SET_N_BITS(lmac_ier, pdata->mac_idx, LMAC_IER_MAC2_WIDTH, val);

	//mac_printf("LMAC %d Interrupt : %s\n", pdata->mac_idx,
	//	   val ? "ENABLED" : "DISABLED");
	LMAC_RGWR(pdata, LMAC_IER, lmac_ier);

	return 0;
}

int lmac_set_event_int(void *pdev, u32 evnt, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 lmac_pier = LMAC_RGRD(pdata, MAC_PIER(pdata->mac_idx));

	switch (evnt) {
	case LMAC_PHYERR_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, PHYERR, val);
		break;

	case LMAC_ALIGN_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, ALIGN, val);
		break;

	case LMAC_SPEED_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, SPEED, val);
		break;

	case LMAC_FDUP_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, FDUP, val);
		break;

	case LMAC_RXPAUEN_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, RXPAUEN, val);
		break;

	case LMAC_TXPAUEN_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, TXPAUEN, val);
		break;

	case LMAC_LPIOFF_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, LPIOFF, val);
		break;

	case LMAC_LPION_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, LPION, val);
		break;

	case LMAC_JAM_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, JAM, val);
		break;

	case LMAC_FCSERR_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, FCSERR, val);
		break;

	case LMAC_TXPAU_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, TXPAUSE, val);
		break;

	case LMAC_RXPAU_EVNT:
		MAC_SET_VAL(lmac_pier, MAC_PIER, RXPAUSE, val);
		break;

	case LMAC_ALL_EVNT:
		if (val)
			lmac_pier = 0xFFFFFFFF;
		else
			lmac_pier = 0;

		break;

	default:
		return -1;
	}

	mac_printf("LMAC %d PIER Interrupt Value: %d\n", pdata->mac_idx,
		   lmac_pier);
	LMAC_RGWR(pdata, MAC_PIER(pdata->mac_idx), lmac_pier);

	return 0;
}

int lmac_get_int(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 lmac_isr = LMAC_RGRD(pdata, LMAC_ISR);
	u32 val = 0, mac_idx = 0;

	mac_idx = pdata->mac_idx + LMAC_ISR_MAC2_POS;
	val = GET_N_BITS(lmac_isr, mac_idx, LMAC_ISR_MAC2_WIDTH);

#if 0
	mac_printf("LMAC %d Interrupt Stats : %s\n", pdata->mac_idx,
		   val ? "ENABLED" : "DISABLED");
#endif
	return val;
}

/* Clear all the LMAC interrupts, Write 1 to Clear */
int lmac_clear_int(void *pdev, u32 event)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_isr = 0;

	/* Clear all the interrupts which are set */
	mac_isr = LMAC_RGRD(pdata, MAC_PISR(pdata->mac_idx));

	if ((event & LMAC_PHYERR_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, PHYERR))) {
		mac_printf("LMAC %d Clearing PHYERR Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, PHYERR, 1);
	}

	if ((event & LMAC_ALIGN_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, ALIGN))) {
		mac_printf("LMAC %d Clearing ALIGN Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, ALIGN, 1);
	}

	if ((event & LMAC_SPEED_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, SPEED))) {
		mac_printf("LMAC %d Clearing SPEED Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, SPEED, 1);
	}

	if ((event & LMAC_FDUP_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, FDUP))) {
		mac_printf("LMAC %d Clearing FDUP Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, FDUP, 1);
	}

	if ((event & LMAC_RXPAUEN_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, RXPAUEN))) {
		mac_printf("LMAC %d Clearing RXPAUEN Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, RXPAUEN, 1);
	}

	if ((event & LMAC_TXPAUEN_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, TXPAUEN))) {
		mac_printf("LMAC %d Clearing TXPAUEN Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, TXPAUEN, 1);
	}

	if ((event & LMAC_LPIOFF_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, LPIOFF))) {
		mac_printf("LMAC %d Clearing LPIOFF Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, LPIOFF, 1);
	}

	if ((event & LMAC_LPION_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, LPION))) {
		mac_printf("LMAC %d Clearing LPION Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, LPION, 1);
	}

	if ((event & LMAC_JAM_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, JAM))) {
		mac_printf("LMAC %d Clearing JAM Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, JAM, 1);
	}

	if ((event & LMAC_FCSERR_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, FCSERR))) {
		mac_printf("LMAC %d Clearing FCSERR Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, FCSERR, 1);
	}

	if ((event & LMAC_TXPAU_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, TXPAUSE))) {
		mac_printf("LMAC %d Clearing TXPAUSE Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, TXPAUSE, 1);
	}

	if ((event & LMAC_RXPAU_EVNT) &&
	    (MAC_GET_VAL(mac_isr, MAC_PISR, RXPAUSE))) {
		mac_printf("LMAC %d Clearing RXPAUSE Interrupt Status\n",
			   pdata->mac_idx);
		MAC_SET_VAL(mac_isr, MAC_PISR, RXPAUSE, 1);
	}

	LMAC_RGWR(pdata, MAC_PISR(pdata->mac_idx), mac_isr);

	return 0;
}

int lmac_set_ipg(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl1 = LMAC_RGRD(pdata, MAC_CTRL1(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl1, MAC_CTRL1, IPG) != val) {
		mac_printf("LMAC %d IPG set to : %d bytes\n",
			   pdata->mac_idx, val);
		MAC_SET_VAL(mac_ctrl1, MAC_CTRL1, IPG, val);

		LMAC_RGWR(pdata, MAC_CTRL1(pdata->mac_idx), mac_ctrl1);
	}

	return 0;
}

int lmac_get_ipg(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl1 = LMAC_RGRD(pdata, MAC_CTRL1(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d IPG %08x\n", pdata->mac_idx, mac_ctrl1);

	val = MAC_GET_VAL(mac_ctrl1, MAC_CTRL1, IPG);
	mac_printf("\tIPG set to %d : bytes\n", val);

	return val;
}

int lmac_set_preamble(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl1 = LMAC_RGRD(pdata, MAC_CTRL1(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl1, MAC_CTRL1, SHORTPRE) != val) {
		mac_printf("LMAC %d Preamble is : %s\n", pdata->mac_idx,
			   val ? "0 byte" : "7 byte");
		MAC_SET_VAL(mac_ctrl1, MAC_CTRL1, SHORTPRE, val);

		LMAC_RGWR(pdata, MAC_CTRL1(pdata->mac_idx), mac_ctrl1);
	}

	return 0;
}

int lmac_get_preamble(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl1 = LMAC_RGRD(pdata, MAC_CTRL1(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d PREAMBLE %08x\n", pdata->mac_idx, mac_ctrl1);

	val = MAC_GET_VAL(mac_ctrl1, MAC_CTRL1, SHORTPRE);
	mac_printf("\tPreamble is : %s\n", val ? "0 byte" : "7 byte");

	return val;
}

int lmac_set_defermode(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl1 = LMAC_RGRD(pdata, MAC_CTRL1(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl1, MAC_CTRL1, DEFERMODE) != val) {
		mac_printf("LMAC %d CRS backpressure : %s\n", pdata->mac_idx,
			   val ?
			   "Enabled in Full Duplex mode" :
			   "Enabled in Half Duplex mode");
		MAC_SET_VAL(mac_ctrl1, MAC_CTRL1, DEFERMODE, val);

		LMAC_RGWR(pdata, MAC_CTRL1(pdata->mac_idx), mac_ctrl1);
	}

	return 0;
}

int lmac_get_defermode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl1 = LMAC_RGRD(pdata, MAC_CTRL1(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d DEFERMODE %08x\n", pdata->mac_idx, mac_ctrl1);

	val = MAC_GET_VAL(mac_ctrl1, MAC_CTRL1, DEFERMODE);
	mac_printf("\tCRS backpressure : %s\n",
		   val ?
		   "Enabled in Full Duplex mode" :
		   "Enabled in Half Duplex mode");

	return val;
}

int lmac_set_lpi(void *pdev, u32 mode_en, u32 lpi_waitg, u32 lpi_waitm)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl4 = LMAC_RGRD(pdata, MAC_CTRL4(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl4, MAC_CTRL4, LPIEN) != mode_en) {
		mac_printf("LMAC %d LPI Mode : %s\n", pdata->mac_idx,
			   mode_en ? "ENABLED" : "DISABLED");
		MAC_SET_VAL(mac_ctrl4, MAC_CTRL4, LPIEN, mode_en);
	}

	if (MAC_GET_VAL(mac_ctrl4, MAC_CTRL4, WAIT) != lpi_waitm) {
		mac_printf("LMAC %d LPI Wait time for 100M : %d usec\n",
			   pdata->mac_idx, lpi_waitm);
		MAC_SET_VAL(mac_ctrl4, MAC_CTRL4, WAIT, lpi_waitm);
	}

	if (MAC_GET_VAL(mac_ctrl4, MAC_CTRL4, GWAIT) != lpi_waitg) {
		mac_printf("LMAC %d LPI Wait time for 1G : %d usec\n",
			   pdata->mac_idx, lpi_waitg);
		MAC_SET_VAL(mac_ctrl4, MAC_CTRL4, GWAIT, lpi_waitg);
	}

	LMAC_RGWR(pdata, MAC_CTRL4(pdata->mac_idx), mac_ctrl4);

	return 0;
}

int lmac_get_lpi(void *pdev, u32 *mode_en, u32 *lpi_waitg, u32 *lpi_waitm)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl4 = LMAC_RGRD(pdata, MAC_CTRL4(pdata->mac_idx));

	mac_printf("LMAC %d LPI %08x\n", pdata->mac_idx, mac_ctrl4);

	*mode_en = MAC_GET_VAL(mac_ctrl4, MAC_CTRL4, LPIEN);
	mac_printf("\tLPI Mode : %s\n", *mode_en ? "ENABLED" : "DISABLED");

	*lpi_waitm = MAC_GET_VAL(mac_ctrl4, MAC_CTRL4, WAIT);
	mac_printf("\tLPI Wait time for 100M : %d usec\n", *lpi_waitm);

	*lpi_waitg = MAC_GET_VAL(mac_ctrl4, MAC_CTRL4, GWAIT);
	mac_printf("\tLPI Wait time for 1G : %d usec\n", *lpi_waitg);

	return 0;
}

int lmac_set_jps(void *pdev, u32 pjps_bp, u32 pjps_nobp)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl5 = LMAC_RGRD(pdata, MAC_CTRL5(pdata->mac_idx));

	if (MAC_GET_VAL(mac_ctrl5, MAC_CTRL5, PJPS_BP) != pjps_bp) {
		mac_printf("LMAC %d Prolong Jam Pattern Size during "
			   "backpressure : %s\n",
			   pdata->mac_idx, pjps_bp ?
			   "64 bit jam pattern" :
			   "32 bit jam pattern");
		MAC_SET_VAL(mac_ctrl5, MAC_CTRL5, PJPS_BP, pjps_bp);
	}

	if (MAC_GET_VAL(mac_ctrl5, MAC_CTRL5, PJPS_NOBP) != pjps_nobp) {
		mac_printf("LMAC %d Prolong Jam Pattern Size during "
			   "no-backpressure : %s\n",
			   pdata->mac_idx, pjps_nobp ?
			   "64 bit jam pattern" :
			   "32 bit jam pattern");
		MAC_SET_VAL(mac_ctrl5, MAC_CTRL5, PJPS_NOBP, pjps_nobp);
	}

	LMAC_RGWR(pdata, MAC_CTRL5(pdata->mac_idx), mac_ctrl5);

	return 0;
}

int lmac_get_jps(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_ctrl5 = LMAC_RGRD(pdata, MAC_CTRL5(pdata->mac_idx));
	u32 pjps_bp, pjps_nobp;

	mac_printf("LMAC %d JPS %08x\n", pdata->mac_idx, mac_ctrl5);

	pjps_bp = MAC_GET_VAL(mac_ctrl5, MAC_CTRL5, PJPS_BP);
	mac_printf("\tProlong Jam Pattern Size during backpressure : %s\n",
		   pjps_bp ?
		   "64 bit jam pattern" :
		   "32 bit jam pattern");

	pjps_nobp = MAC_GET_VAL(mac_ctrl5, MAC_CTRL5, PJPS_NOBP);
	mac_printf("\tProlong Jam Pattern Size during no-backpressure : %s\n",
		   pjps_nobp ?
		   "64 bit jam pattern" :
		   "32 bit jam pattern");

	return 0;
}

int lmac_set_loopback(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_testen = LMAC_RGRD(pdata, MAC_TESTEN(pdata->mac_idx));

	if (MAC_GET_VAL(mac_testen, MAC_TESTEN, LOOP) != val) {
		mac_printf("LMAC %d Loopback : %s\n", pdata->mac_idx,
			   val ? "ENABLED" : "DISABLED");
		MAC_SET_VAL(mac_testen, MAC_TESTEN, LOOP, val);

		LMAC_RGWR(pdata, MAC_TESTEN(pdata->mac_idx), mac_testen);
	}

	return 0;
}

int lmac_get_loopback(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_testen = LMAC_RGRD(pdata, MAC_TESTEN(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d Loopback: %08x\n", pdata->mac_idx, mac_testen);

	val = MAC_GET_VAL(mac_testen, MAC_TESTEN, LOOP);
	mac_printf("\tLMAC: Loopback : %s\n", val ? "ENABLED" : "DISABLED");

	return val;
}

int lmac_set_txer(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	u32 mac_testen = LMAC_RGRD(pdata, MAC_TESTEN(pdata->mac_idx));

	if (MAC_GET_VAL(mac_testen, MAC_TESTEN, TXER) != val) {
		mac_printf("LMAC %d Inject transmit error : %s\n",
			   pdata->mac_idx, val ? "ENABLED" : "DISABLED");
		MAC_SET_VAL(mac_testen, MAC_TESTEN, TXER, val);

		LMAC_RGWR(pdata, MAC_TESTEN(pdata->mac_idx), mac_testen);
	}

	return 0;
}

int lmac_get_txer(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_testen = LMAC_RGRD(pdata, MAC_TESTEN(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d TXER %08x\n", pdata->mac_idx, mac_testen);

	val = MAC_GET_VAL(mac_testen, MAC_TESTEN, TXER);
	mac_printf("\tInject transmit error : %s\n",
		   val ? "ENABLED" : "DISABLED");

	return val;
}

int lmac_set_lpimonitor_mode(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_testen = LMAC_RGRD(pdata, MAC_TESTEN(pdata->mac_idx));

	if (MAC_GET_VAL(mac_testen, MAC_TESTEN, LPITM) != val) {
		if (val == 0)
			mac_printf("LMAC %d LPI to be monitored in "
				   "time recording : TX\n", pdata->mac_idx);
		else if (val == 1)
			mac_printf("LMAC %d LPI to be monitored in "
				   "time recording : RX\n", pdata->mac_idx);
		else if (val == 2)
			mac_printf("LMAC %d LPI to be monitored in "
				   "time recording : TXRX\n", pdata->mac_idx);

		MAC_SET_VAL(mac_testen, MAC_TESTEN, LPITM, val);

		LMAC_RGWR(pdata, MAC_TESTEN(pdata->mac_idx), mac_testen);
	}

	return 0;
}

int lmac_get_lpimonitor_mode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_testen = LMAC_RGRD(pdata, MAC_TESTEN(pdata->mac_idx));
	u32 val;

	mac_printf("LMAC %d LPI MONITORING MODE %08x\n",
		   pdata->mac_idx, mac_testen);

	val = MAC_GET_VAL(mac_testen, MAC_TESTEN, LPITM);

	if (val == 0)
		mac_printf("\tLPI to be monitored in time recording : TX\n");
	else if (val == 1)
		mac_printf("\tLPI to be monitored in time recording : RX\n");
	else if (val == 2)
		mac_printf("\tLPI to be monitored in time recording : TXRX\n");

	return val;
}

int lmac_set_pauseframe_samode(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_pfad = LMAC_RGRD(pdata, MAC_PFADCFG);

	if (MAC_GET_VAL(mac_pfad, MAC_PFADCFG, SAMOD) != val) {
		mac_printf("LMAC: Pause frame use : %s\n",
			   val ?
			   "PORT specific MAC source address" :
			   "COMMON MAC source address");
		MAC_SET_VAL(mac_pfad, MAC_PFADCFG, SAMOD, val);

		LMAC_RGWR(pdata, MAC_PFADCFG, mac_pfad);
	}

	return 0;
}

int lmac_get_pauseframe_samode(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_pfad = LMAC_RGRD(pdata, MAC_PFADCFG);
	u32 val;

	mac_printf("LMAC: PAUSE FRAME SAMODE %08x\n", mac_pfad);

	val = MAC_GET_VAL(mac_pfad, MAC_PFADCFG, SAMOD);
	mac_printf("\tPause frame use : %s\n",
		   val ?
		   "PORT specific MAC source address" :
		   "COMMON MAC source address");

	return val;
}

int lmac_set_pauseframe_addr(void *pdev, u8 *mac_addr)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 mac_addr_0 = 0, mac_addr_1 = 0, mac_addr_2;

	mac_addr_2 = (mac_addr[5] <<  8) | (mac_addr[4] <<  0);
	mac_addr_1 = (mac_addr[3] << 24) | (mac_addr[2] << 16);
	mac_addr_0 = (mac_addr[1] <<  8) | (mac_addr[0] <<  0);

	if (LMAC_RGRD(pdata, MAC_PFSA_0) != mac_addr_0) {
		mac_printf("Setting mac_addr_0 as %08x\n", mac_addr_0);
		LMAC_RGWR(pdata, MAC_PFSA_0, mac_addr_0);
	}

	if (LMAC_RGRD(pdata, MAC_PFSA_1) != mac_addr_1) {
		mac_printf("Setting mac_addr_1 as %08x\n", mac_addr_1);
		LMAC_RGWR(pdata, MAC_PFSA_1, mac_addr_1);
	}

	if (LMAC_RGRD(pdata, MAC_PFSA_2) != mac_addr_2) {
		mac_printf("Setting mac_addr_2 as %08x\n", mac_addr_2);
		LMAC_RGWR(pdata, MAC_PFSA_2, mac_addr_2);
	}

	return 0;
}

int lmac_get_pauseframe_addr(void *pdev, u8 *mac_addr)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u16 mac_addr_0 = 0, mac_addr_1 = 0, mac_addr_2;

	mac_addr_0 = LMAC_RGRD(pdata, MAC_PFSA_0);
	mac_addr[1] = ((mac_addr_0 & 0xFF00) >> 8);
	mac_addr[0] = (mac_addr_0 & 0x00FF);

	mac_addr_1 = LMAC_RGRD(pdata, MAC_PFSA_1);
	mac_addr[3] = ((mac_addr_1 & 0xFF00) >> 8);
	mac_addr[2] = (mac_addr_1 & 0x00FF);

	mac_addr_2 = LMAC_RGRD(pdata, MAC_PFSA_2);
	mac_addr[5] = ((mac_addr_2 & 0xFF00) >> 8);
	mac_addr[4] = (mac_addr_2 & 0x00FF);

	return 0;
}

int lmac_get_mac_pstat(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_pstat = LMAC_RGRD(pdata, MAC_PSTAT(pdata->mac_idx));

	mac_printf("LMAC %d PORT STAT: %08x\n", pdata->mac_idx, mac_pstat);

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, RXLPI))
		mac_printf("\tReceive Low Power Idle Status : "
			   "LPI Low power idle state\n");
	else
		mac_printf("\tReceive Low Power Idle Status : "
			   "Normal Power state\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, TXLPI))
		mac_printf("\tTransmit Low Power Idle Status : "
			   "LPI Low power idle state\n");
	else
		mac_printf("\tTransmit Low Power Idle Status : "
			   "Normal Power state\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, CRS))
		mac_printf("\tCarrier Detected\n");
	else
		mac_printf("\tNo Carrier Detected\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, LSTAT))
		mac_printf("\tLink is : UP\n");
	else
		mac_printf("\tLink is : DOWN\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, TXPAUEN))
		mac_printf("\tLink Partner accepts Pause frames\n");
	else
		mac_printf("\tLink Partner doesnot accepts Pause frames\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, RXPAUEN))
		mac_printf("\tLink Partner sends Pause frames\n");
	else
		mac_printf("\tLink Partner doesnot sends Pause frames\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, TXPAU))
		mac_printf("\tTransmit Pause status is active\n");
	else
		mac_printf("\tNormal transmit operation\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, RXPAU))
		mac_printf("\tReceive Pause status is active\n");
	else
		mac_printf("\tNormal Receive operation\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, FDUP))
		mac_printf("\tFull duplex Mode\n");
	else
		mac_printf("\thalf Duplex mode\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, MBIT))
		mac_printf("\tAttached PHY runs at a data rate of "
			   "100 Mbps\n");
	else
		mac_printf("\tAttached PHY runs at a data rate of "
			   "10 Mbps\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, GBIT))
		mac_printf("\tAttached PHY runs at a data rate of "
			   "1000 or 2000 Mbps\n");
	else
		mac_printf("\tAttached PHY runs at a data rate of "
			   "10 or 100 Mbps\n");

	if (MAC_GET_VAL(mac_pstat, MAC_PSTAT, PACT))
		mac_printf("\tPHY is active and responds to MDIO accesses\n");
	else
		mac_printf("\tPHY is inactive or not present\n");

	return 0;
}

int lmac_get_mac_pisr(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mac_pisr = LMAC_RGRD(pdata, MAC_PISR(pdata->mac_idx));

	mac_printf("LMAC %d PORT INTERRUPT STATUS: %08x\n",
		   pdata->mac_idx, mac_pisr);

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, RXPAUSE))
		mac_printf("\tAtleast 1 pause frame has been Received\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, TXPAUSE))
		mac_printf("\tAtleast 1 pause frame has been transmitted\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, FCSERR))
		mac_printf("\tFrame checksum Error Detected\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, LENERR))
		mac_printf("\tLength mismatch Error Detected\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, TOOLONG))
		mac_printf("\tToo Long frame Error Detected\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, TOOSHORT))
		mac_printf("\tToo Short frame Error Detected\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, JAM))
		mac_printf("\tJam status detected\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, LPION))
		mac_printf("\tReceive low power idle mode is entered\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, LPIOFF))
		mac_printf("\tReceive low power idle mode is left\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, TXPAUEN))
		mac_printf("\tA change of Transmit Pause Enable Status\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, RXPAUEN))
		mac_printf("\tA change of Receive Pause Enable Status\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, FDUP))
		mac_printf("\tA change of half- or full-duplex mode\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, SPEED))
		mac_printf("\tA change of speed mode\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, PACT))
		mac_printf("\tA change of link activity\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, ALIGN))
		mac_printf("\tA frame has been received which an "
			   "alignment error\n");

	if (MAC_GET_VAL(mac_pisr, MAC_PISR, PHYERR))
		mac_printf("\tA frame has been received which has an "
			   "active rx_err signal\n");

	return 0;
}

void lmac_test_all_reg(void *pdev)
{
	int i = 0;

	lmac_check_reg(pdev, MAC_TEST, "MAC_TEST", 0, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_PFADCFG, "MAC_PFADCFG", 0, 0x1, 0);
	lmac_check_reg(pdev, MAC_PFSA_0, "MAC_PFSA_0", 0, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_PFSA_1, "MAC_PFSA_1", 0, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_PFSA_2, "MAC_PFSA_2", 0, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_VLAN_ETYPE_0, "MAC_VLAN_ETYPE_0",
		       0, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_VLAN_ETYPE_1, "MAC_VLAN_ETYPE_1",
		       0, 0xFFFF, 0);
	lmac_check_reg(pdev, REG_LMAC_CNT_LSB, "REG_LMAC_CNT_LSB",
		       0, 0xFFFF, 0);
	lmac_check_reg(pdev, REG_LMAC_CNT_MSB, "REG_LMAC_CNT_MSB",
		       0, 0xFFFF, 0);
	lmac_check_reg(pdev, REG_LMAC_CNT_ACC, "REG_LMAC_CNT_ACC",
		       0, 0x6F1F, 0);

	lmac_check_reg(pdev, MAC_PISR(i), "MAC_PISR", i, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_PIER(i), "MAC_PIER", i, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_CTRL0(i), "MAC_CTRL0", i, 0xFFF, 0);
	lmac_check_reg(pdev, MAC_CTRL1(i), "MAC_CTRL1", i, 0x810F, 0);
	lmac_check_reg(pdev, MAC_CTRL2(i), "MAC_CTRL2", i, 0xF, 0);
	lmac_check_reg(pdev, MAC_CTRL3(i), "MAC_CTRL3", i, 0xF, 0);
	lmac_check_reg(pdev, MAC_CTRL4(i), "MAC_CTRL4", i, 0x7FFF, 0);
	lmac_check_reg(pdev, MAC_CTRL5(i), "MAC_CTRL5", i, 0x3, 0);
	lmac_check_reg(pdev, MAC_TESTEN(i), "MAC_TESTEN", i, 0x307, 0);
	lmac_check_reg(pdev, MAC_LPITIMER0(i), "MAC_LPITIMER0",
		       i, 0xFFFF, 0);
	lmac_check_reg(pdev, MAC_LPITIMER1(i), "MAC_LPITIMER1",
		       i, 0xFFFF, 0);
}

void lmac_check_reg(void *pdev, u32 reg, char *name, int idx, u16 set_val,
		    u16 clr_val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 val;

	LMAC_RGWR(pdata, reg, set_val);
	val = LMAC_RGRD(pdata, reg);

	if (val != set_val)
		mac_printf("Setting reg %s: %d with %x FAILED\n",
			   name, idx, set_val);

	LMAC_RGWR(pdata, reg, clr_val);

	if (val != clr_val)
		mac_printf("Setting reg %s: %d with %x FAILED\n",
			   name, idx, clr_val);
}

static u32 read_lmac_cnt(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 cnt = 0;

	cnt = (((LMAC_RGRD(pdata, REG_LMAC_CNT_MSB) & 0xFFFF) << 16) |
	       (LMAC_RGRD(pdata, REG_LMAC_CNT_LSB)));

	return cnt;
}

static u32 write_lmac_cnt(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 cnt_msb = 0, cnt_lsb;

	cnt_msb = ((val & 0xFFFF0000) >> 16);
	cnt_lsb = (val & 0xFFFF);

	LMAC_RGWR(pdata, REG_LMAC_CNT_MSB, cnt_msb);
	LMAC_RGWR(pdata, REG_LMAC_CNT_LSB, cnt_lsb);

	return 0;
}

void lmac_rmon_rd(void *pdev, struct lmac_rmon_cnt *lmac_cnt)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 lmac_cnt_acc = 0;
	int i = 0;

	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, OPMOD, LMAC_RMON_RD);
	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, MAC, pdata->mac_idx + 2);

	for (i = 0; i < 6; i++) {
		MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, BAS, 1);
		MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, ADDR, i);

		LMAC_RGWR(pdata, REG_LMAC_CNT_ACC, lmac_cnt_acc);

		while (1) {
			if ((LMAC_RGRD(pdata, REG_LMAC_CNT_ACC) & 0x8000) == 0)
				break;
		}

		switch (i) {
		case SGLE_COLN_CNT:
			lmac_cnt->sing_coln_cnt = read_lmac_cnt(pdev);
			break;

		case MPLE_COLN_CNT:
			lmac_cnt->mple_coln_cnt = read_lmac_cnt(pdev);
			break;

		case LATE_COLN_CNT:
			lmac_cnt->late_coln_cnt = read_lmac_cnt(pdev);
			break;

		case EXCS_COLN_CNT:
			lmac_cnt->excs_coln_cnt = read_lmac_cnt(pdev);
			break;

		case RXPA_FRAM_CNT:
			lmac_cnt->rx_pause_cnt  = read_lmac_cnt(pdev);
			break;

		case TXPA_FRAM_CNT:
			lmac_cnt->tx_pause_cnt  = read_lmac_cnt(pdev);
			break;

		default:
			break;
		}
	}
}

void lmac_rmon_wr(void *pdev, struct lmac_rmon_cnt *lmac_cnt)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 lmac_cnt_acc = 0;
	int i = 0;

	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, OPMOD, LMAC_RMON_WR);
	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, MAC, pdata->mac_idx + 2);

	for (i = 0; i < 6; i++) {
		MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, BAS, 1);
		MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, ADDR, i);

		LMAC_RGWR(pdata, REG_LMAC_CNT_ACC, lmac_cnt_acc);

		while (1) {
			if ((LMAC_RGRD(pdata, REG_LMAC_CNT_ACC) & 0x8000) == 0)
				break;
		}

		switch (i) {
		case SGLE_COLN_CNT:
			write_lmac_cnt(pdev, lmac_cnt->sing_coln_cnt);
			break;

		case MPLE_COLN_CNT:
			write_lmac_cnt(pdev, lmac_cnt->mple_coln_cnt);
			break;

		case LATE_COLN_CNT:
			write_lmac_cnt(pdev, lmac_cnt->late_coln_cnt);
			break;

		case EXCS_COLN_CNT:
			write_lmac_cnt(pdev, lmac_cnt->excs_coln_cnt);
			break;

		case RXPA_FRAM_CNT:
			write_lmac_cnt(pdev, lmac_cnt->rx_pause_cnt);
			break;

		case TXPA_FRAM_CNT:
			write_lmac_cnt(pdev, lmac_cnt->tx_pause_cnt);
			break;

		default:
			break;
		}
	}
}

void lmac_rmon_clr(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 lmac_cnt_acc = 0;

	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, OPMOD, LMAC_RMON_CLR);
	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, MAC, pdata->mac_idx + 2);

	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, BAS, 1);
	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, ADDR, 0); // ignored

	LMAC_RGWR(pdata, REG_LMAC_CNT_ACC, lmac_cnt_acc);

	while (1) {
		if ((LMAC_RGRD(pdata, REG_LMAC_CNT_ACC) & 0x8000) == 0)
			break;
	}
}

void lmac_rmon_clr_allmac(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 lmac_cnt_acc = 0;

	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, OPMOD, LMAC_RMON_CLRALL);
	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, MAC, 0); // ignored

	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, BAS, 1);
	MAC_SET_VAL(lmac_cnt_acc, LMAC_CNT_ACC, ADDR, 0); // ignored

	LMAC_RGWR(pdata, REG_LMAC_CNT_ACC, lmac_cnt_acc);

	while (1) {
		if ((LMAC_RGRD(pdata, REG_LMAC_CNT_ACC) & 0x8000) == 0)
			break;
	}
}

void lmac_get_rmon(void)
{
	struct lmac_rmon_cnt lmac_cnt[3];
	int i = 0;
	struct mac_ops *ops;
	u32 max_mac = gsw_get_mac_subifcnt(0);

	for (i = 0; i < max_mac; i++) {
		ops = gsw_get_mac_ops(0, i);
		lmac_rmon_rd(ops, &lmac_cnt[i]);
	}

	mac_printf("\nTYPE                            %11s %11s %11s\n", "       LMAC 2", "     LMAC 3", "     LMAC 4\n");

	mac_printf("Single Collision Cnt            = ");

	mac_printf("%11u %11u %11u", lmac_cnt[0].sing_coln_cnt,
		   lmac_cnt[1].sing_coln_cnt, lmac_cnt[2].sing_coln_cnt);

	mac_printf("Multiple Collision Cnt          = ");

	mac_printf("%11u %11u %11u", lmac_cnt[0].mple_coln_cnt,
		   lmac_cnt[1].mple_coln_cnt, lmac_cnt[2].mple_coln_cnt);

	mac_printf("Late Collision Cnt              = ");

	mac_printf("%11u %11u %11u", lmac_cnt[0].late_coln_cnt,
		   lmac_cnt[1].late_coln_cnt, lmac_cnt[2].late_coln_cnt);

	mac_printf("Excess Collision Cnt            = ");

	mac_printf("%11u %11u %11u", lmac_cnt[0].excs_coln_cnt,
		   lmac_cnt[1].excs_coln_cnt, lmac_cnt[2].excs_coln_cnt);

	mac_printf("Rx Pause Cnt                    = ");

	mac_printf("%11u %11u %11u", lmac_cnt[0].rx_pause_cnt,
		   lmac_cnt[1].rx_pause_cnt, lmac_cnt[2].rx_pause_cnt);

	mac_printf("Tx Pause Cnt                    = ");

	mac_printf("%11u %11u %11u", lmac_cnt[0].tx_pause_cnt,
		   lmac_cnt[1].tx_pause_cnt, lmac_cnt[2].tx_pause_cnt);

	mac_printf("\n");
}
