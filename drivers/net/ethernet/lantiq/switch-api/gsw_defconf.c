/******************************************************************************
 *                Copyright (c) 2016, 2017 Intel Corporation
 *
 *
 * For licensing information, see the file 'LICENSE' in the root folder of
 * this software module.
 *
 ******************************************************************************/

#include <gsw_init.h>
#include <gsw_defconf.h>

#define PMAC0_TX_DMACHID_START	0
#define PMAC0_TX_DMACHID_END	16
#define PMAC1_TX_DMACHID_START	0
#define PMAC1_TX_DMACHID_END	0

#define PMAC0_DST_PRT_START	0
#define PMAC0_DST_PRT_END	11
#define PMAC1_DST_PRT_START	2
#define PMAC1_DST_PRT_END	2

/* Default GSWIP PCE Path Q-Map */
static struct _gsw_pce_path gsw_pce_path[] = {
	/*EG_LPID	EXT	TC_FR	TC_TO   QID	RDR_LPID */
	{ LOG_2,	X,	0,	0,	28,	PMAC_1},
	{ LOG_2,	X,	1,	1,	29,	PMAC_1},
	{ LOG_2,	X,	2,	2,	30,	PMAC_1},
	{ LOG_2,	X,	3,	15,	31,	PMAC_1},
	/* QID 24 */
	{ LOG_3,	X,	0,	0,	24,	PMAC_0},
	{ LOG_4,	X,	0,	0,	24,	PMAC_0},
	{ LOG_5,	X,	0,	0,	24,	PMAC_0},
	{ LOG_6,	X,	0,	0,	24,	PMAC_0},
	{ LOG_7,	X,	0,	0,	24,	PMAC_0},
	{ LOG_8,	X,	0,	0,	24,	PMAC_0},
	{ LOG_9,	X,	0,	0,	24,	PMAC_0},
	{ LOG_10,	X,	0,	0,	24,	PMAC_0},
	{ LOG_11,	X,	0,	0,	24,	PMAC_0},
	{ LOG_0,	X,	0,	0,	24,	PMAC_0},
	/* QID 25 */
	{ LOG_3,	X,	1,	1,	25,	PMAC_0},
	{ LOG_4,	X,	1,	1,	25,	PMAC_0},
	{ LOG_5,	X,	1,	1,	25,	PMAC_0},
	{ LOG_6,	X,	1,	1,	25,	PMAC_0},
	{ LOG_7,	X,	1,	1,	25,	PMAC_0},
	{ LOG_8,	X,	1,	1,	25,	PMAC_0},
	{ LOG_9,	X,	1,	1,	25,	PMAC_0},
	{ LOG_10,	X,	1,	1,	25,	PMAC_0},
	{ LOG_11,	X,	1,	1,	25,	PMAC_0},
	{ LOG_0,	X,	1,	1,	25,	PMAC_0},
	/* QID 26 */
	{ LOG_3,	X,	2,	2,	26,	PMAC_0},
	{ LOG_4,	X,	2,	2,	26,	PMAC_0},
	{ LOG_5,	X,	2,	2,	26,	PMAC_0},
	{ LOG_6,	X,	2,	2,	26,	PMAC_0},
	{ LOG_7,	X,	2,	2,	26,	PMAC_0},
	{ LOG_8,	X,	2,	2,	26,	PMAC_0},
	{ LOG_9,	X,	2,	2,	26,	PMAC_0},
	{ LOG_10,	X,	2,	2,	26,	PMAC_0},
	{ LOG_11,	X,	2,	2,	26,	PMAC_0},
	{ LOG_0,	X,	2,	2,	26,	PMAC_0},
	/* QID 27 */
	{ LOG_3,	X,	3,	15,	27,	PMAC_0},
	{ LOG_4,	X,	3,	15,	27,	PMAC_0},
	{ LOG_5,	X,	3,	15,	27,	PMAC_0},
	{ LOG_6,	X,	3,	15,	27,	PMAC_0},
	{ LOG_7,	X,	3,	15,	27,	PMAC_0},
	{ LOG_8,	X,	3,	15,	27,	PMAC_0},
	{ LOG_9,	X,	3,	15,	27,	PMAC_0},
	{ LOG_10,	X,	3,	15,	27,	PMAC_0},
	{ LOG_11,	X,	3,	15,	27,	PMAC_0},
	{ LOG_0,	X,	3,	15,	27,	PMAC_0},
};

/* Do the GSWIP PCE Q-MAP configuration */
int gsw_set_def_pce_qmap(struct core_ops *ops)
{
	int i = 0, j = 0;
	GSW_QoS_queuePort_t q_map;
	int num_of_elem =
		(sizeof(gsw_pce_path) / sizeof(struct _gsw_pce_path));

	for (j = 0; j < num_of_elem; j++) {
		for (i = gsw_pce_path[j].tc_from;
		     i <= gsw_pce_path[j].tc_to; i++) {
			memset(&q_map, 0, sizeof(GSW_QoS_queuePort_t));
			q_map.nPortId = gsw_pce_path[j].eg_lpid;

			if (gsw_pce_path[j].ext != X)
				q_map.bExtrationEnable = gsw_pce_path[j].ext;

			q_map.nTrafficClassId = i;
			q_map.nQueueId = gsw_pce_path[j].qid;
			q_map.nRedirectPortId = gsw_pce_path[j].redir_lpid;

			ops->gsw_qos_ops.QoS_QueuePortSet(ops, &q_map);
		}
	}

	return 0;
}

int gsw_get_def_pce_qmap(struct core_ops *ops)
{
	int i = 0, j = 0;
	GSW_QoS_queuePort_t q_map;
	int num_of_elem =
		(sizeof(gsw_pce_path) / sizeof(struct _gsw_pce_path));

	if (!ops) {
		pr_err("%s: Open SWAPI device FAILED!\n", __func__);
		return -EIO;
	}

	printk("\nGSWIP Default PCE-Q-MAP\n");
	printk("%15s %15s %15s %15s %15s %15s\n",
	       "EgLpid", "Ext", "Traf_Cls", "Q_Map_Mode", "Qid", "Redir_Lpid");

	for (j = 0; j < num_of_elem; j++) {
		for (i = gsw_pce_path[j].tc_from;
		     i <= gsw_pce_path[j].tc_to; i++) {
			memset(&q_map, 0, sizeof(GSW_QoS_queuePort_t));
			q_map.nTrafficClassId = i;
			q_map.nPortId = gsw_pce_path[j].eg_lpid;
			ops->gsw_qos_ops.QoS_QueuePortGet(ops, &q_map);
			printk("%15d %15d %15d %15d %15d %15d ",
			       q_map.nPortId, q_map.bExtrationEnable,
			       q_map.nTrafficClassId, q_map.eQMapMode,
			       q_map.nQueueId, q_map.nRedirectPortId);
			printk("\n");

		}
	}

	return 0;
}

/* Default GSWIP PCE-Bypass Path Q-Map */
static struct _gsw_bypass_path gsw_bypass_path[] = {
	/*EG_MPID	EXT	SUBIF	QID	RDR_LPID */
	{ MAC_2,	0,	X,	16,	MAC_2},
	{ MAC_2,	1,	X,	27,	PMAC_0},
	{ MAC_3,	0,	0,	0,	MAC_3},
	{ MAC_4,	0,	8,	8,	MAC_4},
	{ MAC_3,	1,	X,	27,	PMAC_0},
	{ MAC_4,	1,	X,	27,	PMAC_0},
};

/* Do the GSWIP Bypass Q-MAP configuration */
int gsw_set_def_bypass_qmap(struct core_ops *ops,
			    GSW_QoS_qMapMode_t q_map_mode)
{
	int j = 0;
	GSW_QoS_queuePort_t q_map;
	int num_of_elem =
		(sizeof(gsw_bypass_path) / sizeof(struct _gsw_bypass_path));

	for (j = 0; j < num_of_elem; j++) {
		memset(&q_map, 0, sizeof(GSW_QoS_queuePort_t));
		q_map.nPortId = gsw_bypass_path[j].eg_pid;

		if (gsw_bypass_path[j].ext != X)
			q_map.bExtrationEnable = gsw_bypass_path[j].ext;

		q_map.bRedirectionBypass = 1;

		q_map.eQMapMode = q_map_mode;
		q_map.nQueueId = gsw_bypass_path[j].qid;
		q_map.nRedirectPortId = gsw_bypass_path[j].redir_pid;

		ops->gsw_qos_ops.QoS_QueuePortSet(ops, &q_map);
	}

	return 0;
}

int gsw_get_def_bypass_qmap(struct core_ops *ops)
{
	int j = 0;
	GSW_QoS_queuePort_t q_map;
	int num_of_elem =
		(sizeof(gsw_bypass_path) / sizeof(struct _gsw_bypass_path));

	if (!ops) {
		pr_err("%s: Open SWAPI device FAILED!\n", __func__);
		return -EIO;
	}

	printk("\nGSWIP Default PCE Bypass Q-MAP\n");
	printk("%15s %15s %15s %15s %15s %15s\n",
	       "EgMpid", "Ext", "Traf_Cls", "Q_Map_Mode", "Qid", "Redir_Lpid");

	for (j = 0; j < num_of_elem; j++) {
		memset(&q_map, 0, sizeof(GSW_QoS_queuePort_t));
		q_map.nPortId = gsw_bypass_path[j].eg_pid;
		q_map.bRedirectionBypass = 1;
		q_map.bExtrationEnable = gsw_bypass_path[j].ext;
		ops->gsw_qos_ops.QoS_QueuePortGet(ops, &q_map);
		printk("%15d %15d %15d %15d %15d %15d ",
		       q_map.nPortId, q_map.bExtrationEnable,
		       q_map.nTrafficClassId, q_map.eQMapMode,
		       q_map.nQueueId, q_map.nRedirectPortId);
		printk("\n");
	}

	return 0;
}

/* Default Qos WRED Config in switch */
int gsw_qos_def_config(struct core_ops *ops)
{
	GSW_QoS_WRED_PortCfg_t sVar;
	GSW_QoS_WRED_QueueCfg_t qcfg;
	int j = 0;

	for (j = 0; j < 5; j++) {
		memset(&sVar, 0x00, sizeof(sVar));

		sVar.nPortId = j;
		sVar.nGreen_Min = 240;
		sVar.nGreen_Max = 256;

		ops->gsw_qos_ops.QoS_WredPortCfgSet(ops, &sVar);
	}

	for (j = 0; j < 31; j++) {
		memset(&qcfg, 0x00, sizeof(qcfg));

		qcfg.nQueueId = j;
		qcfg.nGreen_Min = 240;
		qcfg.nGreen_Max = 256;

		ops->gsw_qos_ops.QoS_WredQueueCfgSet(ops, &qcfg);
	}

	return 0;
}

/* Pmac Ingress table has 17 entries,
 * 17 channels
 * Non - DPU
 * Pmac 0
 * Address: (i = 0 and i = 8), PCE Bypass traffic to P3 and P4
 * Address: (i = 16), Traffic from CPU and Voice
 * Address: (i = others), Traffic from HGU, WiFi, 5G
 * Pmac 1
 * Address: (i = 0), PCE bypass traffic  to MAC2
 * DPU:
 * Pmac 0
 * Address: (i from 0 to 15), PCE bypass traffic to MAC3 and MAC4
 * Address: (i = 16), Traffic from CPU
 */
static int pmac_ig_cfg(struct core_ops *ops, u8 pmacid, u8 dpu)
{
	int i = 0;
	u8 addr_from = 0, addr_to = 0;
	GSW_PMAC_Ig_Cfg_t ig_cfg;

	/* Do the GSWIP PMAC IG configuration */

	if (pmacid == 0) {
		addr_from = PMAC0_TX_DMACHID_START;
		addr_to   = PMAC0_TX_DMACHID_END;
	} else if (pmacid == 1) {
		addr_from = PMAC1_TX_DMACHID_START;
		addr_to   = PMAC1_TX_DMACHID_END;
	}

	for (i = addr_from; i <= addr_to; i++) {
		memset((void *)&ig_cfg, 0x00, sizeof(ig_cfg));

		ig_cfg.nPmacId		= pmacid;
		ig_cfg.nTxDmaChanId	= i;
		/* Discard Packet with Error Flag Set */
		ig_cfg.bErrPktsDisc	= 1;

		/* CLASS_EN from default PMAC header,
		 * TC (Class) enable is fixed to 0.
		 */
		ig_cfg.bClassEna	= 1;

		if (dpu == NON_DPU) {
			if (i == 0 || i == 8) {
				/* CLASS from default PMAC header,
				 * TC (class) is fixed to 0.
				 */
				ig_cfg.bClassDefault	= 1;
				/* Source SUBID is from PMAC header. */
				ig_cfg.eSubId		= 1;
				/* SPPID from default PMAC header */
				ig_cfg.bSpIdDefault	= 1;
			}

			/* The packets has PMAC header for 0, 8 & 16 */
			if ((i % 8) == 0)
				ig_cfg.bPmacPresent  = 1;
		} else if (dpu == DPU) {
			/* The packets has PMAC header for all channels */
			ig_cfg.bPmacPresent  	= 1;

			if (i != 16) {
				/* CLASS from default PMAC header,
				 * TC (class) is fixed to 0.
				 */
				ig_cfg.bClassDefault	= 1;
				/* Source SUBID is from PMAC header. */
				ig_cfg.eSubId		= 1;
				/* SPPID from default PMAC header */
				ig_cfg.bSpIdDefault	= 1;
			}
		}

		/* Set IGP = 1 for PMAC 1 */
		if (pmacid == 1)
			ig_cfg.defPmacHdr[2] = 0x10;

		ops->gsw_pmac_ops.Pmac_Ig_CfgSet(ops, &ig_cfg);
	}

	pr_info("PMAC_IG_CFG_SET for PMAC %d %s\n", pmacid,
		(dpu == NON_DPU) ? "Non-DPU" : "DPU");
	return 0;
}

int pmac_get_ig_cfg(struct core_ops *ops, u8 pmacid)
{
	int i = 0;
	GSW_PMAC_Ig_Cfg_t ig_cfg;

	/* Do the GSWIP PMAC IG configuration */

	if (!ops) {
		pr_err("%s: Open SWAPI device FAILED!\n", __func__);
		return -EIO;
	}

	printk("\nGSWIP PMAC IG CFG\n");
	printk("%10s %10s %10s %10s %10s %10s %10s %10s %10s\n",
	       "PmacId", "TxDmaChId", "ErrPktDisc", "ClassEn",
	       "ClassDef", "eSubId", "bSpIdDef", "bPmacPr", "DefPmacHdr");

	for (i = PMAC0_TX_DMACHID_START; i <= PMAC0_TX_DMACHID_END; i++) {
		memset((void *)&ig_cfg, 0x00, sizeof(ig_cfg));

		ig_cfg.nPmacId		= pmacid;
		ig_cfg.nTxDmaChanId	= i;

		ops->gsw_pmac_ops.Pmac_Ig_CfgGet(ops, &ig_cfg);
		printk("%10d %10d %10d %10d %10d %10d %10d %10d %10x:%x:%x:%x:%x:%x:%x:%x",
		       ig_cfg.nPmacId, ig_cfg.nTxDmaChanId,
		       ig_cfg.bErrPktsDisc, ig_cfg.bClassEna,
		       ig_cfg.bClassDefault, ig_cfg.eSubId,
		       ig_cfg.bSpIdDefault, ig_cfg.bPmacPresent,
		       ig_cfg.defPmacHdr[0], ig_cfg.defPmacHdr[1],
		       ig_cfg.defPmacHdr[2], ig_cfg.defPmacHdr[3],
		       ig_cfg.defPmacHdr[4], ig_cfg.defPmacHdr[5],
		       ig_cfg.defPmacHdr[6], ig_cfg.defPmacHdr[7]);
		printk("\n");
	}

	return 0;
}

/* Pmac Egress table has 1024 entries,
 * i * j * k * 16 channels
 */
static int pmac_eg_cfg(struct core_ops *ops, u8 pmacid, u8 dpu)
{
	GSW_PMAC_Eg_Cfg_t eg_cfg;
	int i = 0, j = 0, k = 0, m = 0;
	u8 dst_start = 0, dst_end = 0;

	/* Do the GSWIP PMAC configuration */

	if (pmacid == 0) {
		dst_start = PMAC0_DST_PRT_START;
		dst_end   = PMAC0_DST_PRT_END;
	} else if (pmacid == 1) {
		dst_start = PMAC1_DST_PRT_START;
		dst_end   = PMAC1_DST_PRT_END;
	}

	/* m = Dest port
	 * k = flow_id
	 * i = traffic class
	 * j = mpe flag
	 */
	for (m = dst_start; m <= dst_end; m++) {
		for (k = 0; k <= 3; k++) {
			for (i = 0; i <= 3; i++) {
				for (j = 0; j <= 3; j++) {
					memset((void *)&eg_cfg, 0x00,
					       sizeof(eg_cfg));

					/* Select Pmac ID */
					eg_cfg.nPmacId		= pmacid;
					/* Egress ch for Pmac0/1 is always 0 */
					eg_cfg.nRxDmaChanId = 0;
					/* Packet traffic class */
					eg_cfg.nBslTrafficClass = i;

					/* Traffic to CPU and voice */
					if (m == 0 || m == 11) {
						/* Every Pkt has Pmac header */
						eg_cfg.bPmacEna = 1;
						/* Pkt cannot be segmented. */
						eg_cfg.bBslSegmentDisable = 1;
					}
					/* Traffic to HGU, WiFi and LTE */
					else if (m >= 5 && m <= 10) {
						/* Every pkt has NO PMAC hdr. */
						eg_cfg.bPmacEna = 0;
						/* Pkt cannot be segmented. */
						eg_cfg.bBslSegmentDisable = 1;
					}
					/* PCE-Bypass Traf to MAC3 & MAC4 DPU */
					else if ((dpu == DPU) &&
						 (m >= 3 && m <= 4)) {
						/* Every Pkt has Pmac header */
						eg_cfg.bPmacEna = 1;
						/* Packet can be segmented. */
						eg_cfg.bBslSegmentDisable = 0;
						/* Pkt to GSWIP PCE bypass. */
						eg_cfg.bRedirEnable = 1;
					}
					/* Pmac1, PCE bypass traffic to MAC2*/
					/* Pmac0, PCE-Bypass Traf to MAC3-MAC4*/
					else {
						/* Every Pkt has Pmac header */
						eg_cfg.bPmacEna = 1;
						/* Pkt can be segmented. */
						eg_cfg.bBslSegmentDisable = 0;
						/* Pkt to GSWIP PCE-Bypass. */
						eg_cfg.bRedirEnable = 1;
					}

					eg_cfg.bProcFlagsSelect = 1;
					eg_cfg.nDestPortId	= m;
					eg_cfg.nTrafficClass	= i;
					eg_cfg.bMpe2Flag	= ((j & 3) >> 1);
					eg_cfg.bMpe1Flag	= (j & 1);
					eg_cfg.nFlowIDMsb	= k;
					eg_cfg.bFcsEna		= 1;

					/* All other fields set to 0. */
					ops->gsw_pmac_ops.Pmac_Eg_CfgSet(ops,
									 &eg_cfg);
				}
			}
		}
	}

	pr_info("PMAC_EG_CFG_SET for PMAC %d %s\n", pmacid,
		(dpu == NON_DPU) ? "Non-DPU" : "DPU");

	return 0;
}

int pmac_get_eg_cfg(struct core_ops *ops, u8 pmacid, u8 dst_port)
{
	int i = 0, k = 0, j = 0;
	GSW_PMAC_Eg_Cfg_t eg_cfg;

	/* Do the GSWIP PMAC IG configuration */

	if (!ops) {
		pr_err("%s: Open SWAPI device FAILED!\n", __func__);
		return -EIO;
	}

	printk("\nGSWIP PMAC EG CFG\n");

	printk("\n\nDestination portId = %d\n\n", dst_port);
	printk("%10s %10s %10s %10s %10s %10s %10s %10s %10s %10s %10s %10s\n",
	       "PmacId", "RxDmaChId", "BslTrafCls", "BslSegDis",
	       "PmacEna", "RedirEna", "DestPortId", "TrafCls", "Mpe1",
	       "Mpe2", "FlowId", "FcsEn");

	for (k = 0; k <= 3; k++) {
		for (i = 0; i <= 3; i++) {
			for (j = 0; j <= 3; j++) {

				memset((void *)&eg_cfg, 0x00,
				       sizeof(eg_cfg));

				eg_cfg.nPmacId		= pmacid;
				eg_cfg.nDestPortId	= dst_port;

				eg_cfg.bProcFlagsSelect = 1;
				eg_cfg.nTrafficClass	= i;
				eg_cfg.bMpe2Flag	= ((j & 3) >> 1);
				eg_cfg.bMpe1Flag	= (j & 1);
				eg_cfg.nFlowIDMsb	= k;

				ops->gsw_pmac_ops.Pmac_Eg_CfgGet(ops,
								 &eg_cfg);
				printk("%10d %10d %10d %10d %10d %10d %10d %10d %10d %10d %10d %10d",
				       eg_cfg.nPmacId,
				       eg_cfg.nRxDmaChanId,
				       eg_cfg.nBslTrafficClass,
				       eg_cfg.bBslSegmentDisable,
				       eg_cfg.bPmacEna,
				       eg_cfg.bRedirEnable,
				       eg_cfg.nDestPortId,
				       eg_cfg.nTrafficClass,
				       eg_cfg.bMpe1Flag,
				       eg_cfg.bMpe2Flag,
				       eg_cfg.nFlowIDMsb,
				       eg_cfg.bFcsEna);
				printk("\n");
			}
		}
	}

	return 0;
}

static int pmac_glbl_cfg(struct core_ops *ops, u8 pmacid)
{
	GSW_PMAC_Glbl_Cfg_t glbl_cfg;

	/* Do the GSWIP PMAC configuration */

	memset((void *)&glbl_cfg, 0x00, sizeof(glbl_cfg));

	glbl_cfg.nPmacId = pmacid;
	glbl_cfg.bJumboEna = 1;
	glbl_cfg.nMaxJumboLen = 10000;
	glbl_cfg.bTxFCSDis = 0;
	glbl_cfg.bRxFCSDis = 1;
	glbl_cfg.eShortFrmChkType = GSW_PMAC_SHORT_LEN_ENA_UNTAG;
	glbl_cfg.bLongFrmChkDis = 1;
	glbl_cfg.bProcFlagsEgCfgEna = 1;
	glbl_cfg.eProcFlagsEgCfg = GSW_PMAC_PROC_FLAGS_FLAG;

	ops->gsw_pmac_ops.Pmac_Gbl_CfgSet(ops, &glbl_cfg);

	pr_info("PMAC_GLBL_CFG_SET for PMAC %d\n", pmacid);

	return 0;
}

/* Pmac Backpressure table has 17 entries,
 * 17 channels
 * TxQMask - Which Q backpressure is enabled 32 bit denotes 32 Q
 * RxPortMask - Which ingress port flow control is enabled
 * (Configurable upto 16 ports)
 */
GSW_PMAC_BM_Cfg_t bm_cfg_nondpu[] = {
	/*	PmacId	TxDmaChId TxQMask	RxPortMask */
	{0, 	0,	  1,		0},/* PCE Bypass traf to MAC3+MAC4 */
	{0, 	1,	  0,		0},
	{0, 	2,	  0,		0},
	{0, 	3,	  0,		0},
	{0, 	4,	  0,		0},
	{0, 	5,	  0,		0x20},/* Traffic from HGU and LTE */
	{0, 	6,	  0,		0x40},/* Traffic from HGU and LTE */
	{0, 	7,	  0,		0x180},/* Traffic from Wifi */
	{0, 	8,	  0x100,	0},/* PCE Bypass traf to MAC3+MAC4 */
	{0, 	9,	  0,		0x600},/* Traffic from Wifi */
	{0, 	10,	  0,		0},
	{0, 	11,	  0,		0},
	{0, 	12,	  0,		0},
	{0, 	13,	  0,		0},
	{0, 	14,	  0,		0},
	{0, 	15,	  0,		0},
	{0, 	16,	  0,		0x801},/* Traffic from CPU or voice */
	{1, 	0,	  0x10000,	0},/* PCE bypass traffic to MAC2 */
};

GSW_PMAC_BM_Cfg_t bm_cfg_dpu[] = {
	/*	PmacId	TxDmaChId TxQMask	RxPortMask */
	{0, 	0,	  1,		0},
	{0, 	1,	  2,		0},
	{0, 	2,	  4,		0},
	{0, 	3,	  8,		0},
	{0, 	4,	  0x10,		0},
	{0, 	5,	  0x20,		0},
	{0, 	6,	  0x40,		0},
	{0, 	7,	  0x80,		0},
	{0, 	8,	  0x100,	0},
	{0, 	9,	  0x200,	0},
	{0, 	10,	  0x400,	0},
	{0, 	11,	  0x800,	0},
	{0, 	12,	  0x1000,	0},
	{0, 	13,	  0x2000,	0},
	{0, 	14,	  0x4000,	0},
	{0, 	15,	  0x8000,	0},
	/* Port 0 flow control is enabled.DMA channel for traffic from CPU.*/
	{0, 	16,	  0x10000,	0x0001},
	{1, 	0,	  0x10000,	0},
};

static int pmac_bp_cfg(struct core_ops *ops, u8 dpu)
{
	GSW_PMAC_BM_Cfg_t bm_cfg;
	int m = 0;
	int num_of_elem;

	/* Do the GSWIP PMAC BM table configuration */

	if (dpu == NON_DPU) {
		num_of_elem =
			(sizeof(bm_cfg_nondpu) / sizeof(GSW_PMAC_BM_Cfg_t));
	} else if (dpu == DPU) {
		num_of_elem =
			(sizeof(bm_cfg_dpu) / sizeof(GSW_PMAC_BM_Cfg_t));
	}

	for (m = 0; m < num_of_elem; m++) {
		memset((void *)&bm_cfg, 0x00, sizeof(bm_cfg));

		if (dpu == NON_DPU) {
			bm_cfg.nPmacId 		= bm_cfg_nondpu[m].nPmacId;
			bm_cfg.nTxDmaChanId 	= bm_cfg_nondpu[m].nTxDmaChanId;
			bm_cfg.txQMask 		= bm_cfg_nondpu[m].txQMask;
			bm_cfg.rxPortMask	= bm_cfg_nondpu[m].rxPortMask;
		} else if (dpu == DPU) {
			bm_cfg.nPmacId 		= bm_cfg_dpu[m].nPmacId;
			bm_cfg.nTxDmaChanId 	= bm_cfg_dpu[m].nTxDmaChanId;
			bm_cfg.txQMask 		= bm_cfg_dpu[m].txQMask;
			bm_cfg.rxPortMask	= bm_cfg_dpu[m].rxPortMask;
		}

		ops->gsw_pmac_ops.Pmac_Bm_CfgSet(ops, &bm_cfg);
	}

	return 0;
}

int pmac_get_bp_cfg(struct core_ops *ops, u8 pmacid)
{
	GSW_PMAC_BM_Cfg_t bm_cfg;
	int m = 0;

	if (!ops) {
		pr_err("%s: Open SWAPI device FAILED!\n", __func__);
		return -EIO;
	}

	/* Do the GSWIP PMAC BM table configuration */
	printk("\nGSWIP PMAC BP CFG\n");
	printk("%10s %10s %10s %10s\n",
	       "PmacId", "TxDmaChId", "TxQMask", "RxPortMask");

	for (m = PMAC0_TX_DMACHID_START; m <= PMAC0_TX_DMACHID_END; m++) {
		memset((void *)&bm_cfg, 0x00, sizeof(bm_cfg));
		bm_cfg.nPmacId		= pmacid;
		bm_cfg.nTxDmaChanId	= m;

		ops->gsw_pmac_ops.Pmac_Bm_CfgGet(ops, &bm_cfg);

		printk("%10d %10d %10x %10x",
		       bm_cfg.nPmacId, bm_cfg.nTxDmaChanId,
		       bm_cfg.txQMask, bm_cfg.rxPortMask);
		printk("\n");
	}

	return 0;
}

int gsw_pmac_init_nondpu(void)
{
	struct core_ops *ops = gsw_get_swcore_ops(0);

	if (!ops) {
		pr_err("%s: Open SWAPI device FAILED!\n", __func__);
		return -EIO;
	}

	pmac_glbl_cfg(ops, 0);
	pmac_glbl_cfg(ops, 1);
	pmac_ig_cfg(ops, 0, NON_DPU);
	pmac_ig_cfg(ops, 1, NON_DPU);
	pmac_eg_cfg(ops, 0, NON_DPU);
	pmac_eg_cfg(ops, 1, NON_DPU);
	pmac_bp_cfg(ops, NON_DPU);

	pr_info("\n\t GSW PMAC Init Done!!!\n");
	return 0;
}

int gsw_pmac_init_dpu(void)
{
	struct core_ops *ops = gsw_get_swcore_ops(0);

	if (!ops) {
		pr_err("%s: Open SWAPI device FAILED!\n", __func__);
		return -EIO;
	}

	pmac_glbl_cfg(ops, 0);
	pmac_glbl_cfg(ops, 1);
	pmac_ig_cfg(ops, 0, DPU);
	pmac_ig_cfg(ops, 1, NON_DPU);
	pmac_eg_cfg(ops, 0, DPU);
	pmac_eg_cfg(ops, 1, NON_DPU);
	pmac_bp_cfg(ops, DPU);

	pr_info("\n\t GSW PMAC Init Done!!!\n");
	return 0;
}

