/*
 *  Copyright (C) 2017 Intel Corporation.
 *  Zhu YiXin <Yixin.zhu@intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  The full GNU General Public License is included in this distribution in
 *  the file called "COPYING".
 */

#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>
#include <dt-bindings/clock/intel,falconmx-clk.h>
#include "clk_api.h"

static DEFINE_SPINLOCK(pll0a_lock);
static DEFINE_SPINLOCK(pll0b_lock);
static DEFINE_SPINLOCK(pll1_lock);
static DEFINE_SPINLOCK(pll3_lock);
static DEFINE_SPINLOCK(pll4_lock);
static DEFINE_SPINLOCK(pll5_lock);
static DEFINE_SPINLOCK(ifclk_lock);
static DEFINE_SPINLOCK(pcmclk_lock);

static const struct clk_div_table pll_div[] = {
	{1,	2},
	{2,	3},
	{3,	4},
	{4,	5},
	{5,	6},
	{6,	8},
	{7,	10},
	{8,	12},
	{9,	16},
	{10,	20},
	{11,	24},
	{12,	32},
	{13,	40},
	{14,	48},
	{15,	64},
	{0,	0},
};

static const struct clk_div_table dcl_div[] = {
	{0,	6},
	{1,	12},
	{2,	24},
	{3,	36},
	{4,	48},
	{5,	96},
	{0,	0},
};

static const struct clk_div_table ljpll_div[] = {
	{0,	1},
	{1,	2},
	{2,	3},
	{3,	4},
	{4,	5},
	{5,	6},
	{6,	7},
	{7,	8},
};

enum {
	PLL5_CMOS_MODE = 0,
	PLL5_CML_MODE = 1
};

static const struct mdiv_table pcie_div_table[] = {
	{PCIE_CLK_SHIFT1, PCIE_CLK_WIDTH1},
	{PCIE_CLK_SHIFT2, PCIE_CLK_WIDTH2},
};

static const struct factor_clk_data pcie_factor = {
	.shift = PCIE_CLK_SHIFT3,
	.width = PCIE_CLK_WIDTH3,
	.mult = 2,
	.div = 5,
};

static const struct clk_ljpll_div_data pcie_clk_data __initconst = {
	.mdiv_tbl = pcie_div_table,
	.mdiv_tblsz = ARRAY_SIZE(pcie_div_table),
	.opt_factor = &pcie_factor,
	.table = ljpll_div,
	.lock = &pll4_lock,
};

static void __init pcie_clk_setup(struct device_node *node)
{
	clk_ljpll_div_setup(node, &pcie_clk_data);
}

static const struct mdiv_table ptp_div_table[] = {
	{PTP_CLK_SHIFT1, PTP_CLK_WIDTH1},
	{PTP_CLK_SHIFT2, PTP_CLK_WIDTH2},
};

static const struct clk_ljpll_div_data ptp_clk_data __initconst = {
	.mdiv_tbl = ptp_div_table,
	.mdiv_tblsz = ARRAY_SIZE(ptp_div_table),
	.table = ljpll_div,
	.lock = &pll3_lock,
};

static void __init ptp_clk_setup(struct device_node *node)
{
	clk_ljpll_div_setup(node, &ptp_clk_data);
}

static const struct mdiv_table sync_div_table[] = {
	{SYNC_CLK_SHIFT1, SYNC_CLK_WIDTH1},
	{SYNC_CLK_SHIFT2, SYNC_CLK_WIDTH2},
};

static const struct clk_ljpll_div_data sync_clk_data __initconst = {
	.mdiv_tbl = sync_div_table,
	.mdiv_tblsz = ARRAY_SIZE(sync_div_table),
	.table = ljpll_div,
	.lock = &pll3_lock,
};

static void __init sync_clk_setup(struct device_node *node)
{
	clk_ljpll_div_setup(node, &sync_clk_data);
}

static const struct mdiv_table serdes_div_table[] = {
	{SERDES_CLK_SHIFT1, SERDES_CLK_WIDTH1},
	{SERDES_CLK_SHIFT2, SERDES_CLK_WIDTH2},
};

static const struct clk_ljpll_div_data serdes_clk_data __initconst = {
	.mdiv_tbl = serdes_div_table,
	.mdiv_tblsz = ARRAY_SIZE(serdes_div_table),
	.table = ljpll_div,
	.lock = &pll3_lock,
};

static void __init serdes_clk_setup(struct device_node *node)
{
	clk_ljpll_div_setup(node, &serdes_clk_data);
}

static const struct mdiv_table gphy_div_table[] = {
	{GPHY_CLK_SHIFT1, GPHY_CLK_WIDTH1},
	{GPHY_CLK_SHIFT2, GPHY_CLK_WIDTH2},
};

static const struct factor_clk_data gphy_factor = {
	.shift = GPHY_CLK_SHIFT3,
	.width = GPHY_CLK_WIDTH3,
	.mult = 2,
	.div = 5,
};

static const struct clk_ljpll_div_data gphy_clk_data __initconst = {
	.mdiv_tbl = gphy_div_table,
	.mdiv_tblsz = ARRAY_SIZE(gphy_div_table),
	.opt_factor = &gphy_factor,
	.table = ljpll_div,
	.lock = &pll3_lock,
};

static void __init gphy_clk_setup(struct device_node *node)
{
	clk_ljpll_div_setup(node, &gphy_clk_data);
}

static const struct mux_clk_data gpc2_clk_data __initconst = {
	.shift = GPC2_CLK_SHIFT,
	.width = GPC2_CLK_WIDTH,
	.lock = &ifclk_lock,
};

static void __init gpc2_clk_setup(struct device_node *node)
{
	mux_clk_setup(node, &gpc2_clk_data);
}

static const struct mux_clk_data gpc1_clk_data __initconst = {
	.shift = GPC1_CLK_SHIFT,
	.width = GPC1_CLK_WIDTH,
	.lock = &ifclk_lock,
};

static void __init gpc1_clk_setup(struct device_node *node)
{
	mux_clk_setup(node, &gpc1_clk_data);
}

static const struct mux_clk_data sync1_clk_data __initconst = {
	.shift = SYNC1_CLK_SHIFT,
	.width = SYNC1_CLK_WIDTH,
	.lock = &ifclk_lock,
};

static void __init sync1_clk_setup(struct device_node *node)
{
	mux_clk_setup(node, &sync1_clk_data);
}

static const struct mux_clk_data sync0_clk_data __initconst = {
	.shift = SYNC0_CLK_SHIFT,
	.width = SYNC0_CLK_WIDTH,
	.lock = &ifclk_lock,
};

static void __init sync0_clk_setup(struct device_node *node)
{
	mux_clk_setup(node, &sync0_clk_data);
}

static const struct clk_div_data dcl_clk_data __initconst = {
	.shift = DCL_CLK_SHIFT,
	.width = DCL_CLK_WIDTH,
	.table = dcl_div,
	.lock = &pcmclk_lock,
};

static void __init dcl_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &dcl_clk_data);
}

static const struct clk_fixed_factor_data slic_clk_data __initconst = {
	.shift = SLIC_CLK_SHIFT,
	.width = SLIC_CLK_WIDTH,
	.set_val = SLIC_SET_VAL,
	.lock = &ifclk_lock,
};

static void __init slic_clk_setup(struct device_node *node)
{
	clk_fixed_factor_setup(node, &slic_clk_data);
}

static const struct clk_div_data vif_clk_data __initconst = {
	.shift = VIF_CLK_SHIFT,
	.width = VIF_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll1_lock,
};

static void __init vif_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &vif_clk_data);
}

static const struct clk_div_data vdsp_clk_data __initconst = {
	.shift = VDSP_CLK_SHIFT,
	.width = VDSP_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll1_lock,
};

static void __init vdsp_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &vdsp_clk_data);
}

static const struct clk_div_data ppv4_clk_data __initconst = {
	.shift = PPV4_CLK_SHIFT,
	.width = PPV4_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll0b_lock,
};

static void __init ppv4_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &ppv4_clk_data);
}

static const struct clk_div_data ngi_clk_data __initconst = {
	.shift = NGI_CLK_SHIFT,
	.width = NGI_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll0b_lock,
};

static void __init ngi_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &ngi_clk_data);
}

static const struct clk_div_data ssx4_clk_data __initconst = {
	.shift = SSX4_CLK_SHIFT,
	.width = SSX4_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll0b_lock,
};

static void __init ssx4_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &ssx4_clk_data);
}

static const struct clk_div_data sw_clk_data __initconst = {
	.shift = SWITCH_CLK_SHIFT,
	.width = SWITCH_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll0b_lock,
};

static void __init sw_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &sw_clk_data);
}

static const struct clk_div_data qspi_clk_data __initconst = {
	.shift = QSPI_CLK_SHIFT,
	.width = QSPI_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll0a_lock,
};

static void __init qspi_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &qspi_clk_data);
}

static const struct clk_div_data cpu_clk_data __initconst = {
	.shift = CPU_CLK_SHIFT,
	.width = CPU_CLK_WIDTH,
	.table = pll_div,
	.lock = &pll0a_lock,
};

static void __init cpu_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &cpu_clk_data);
}

static const struct clk_div_data ddr_clk_data __initconst = {
	.shift = DDR_CLK_SHIFT,
	.width = DDR_CLK_WIDTH,
	.table = pll_div,
};

static void __init ddr_clk_setup(struct device_node *node)
{
	clk_div_setup(node, &ddr_clk_data);
}

static const struct mux_clk_data wan_clk_data __initconst = {
	.shift = WAN_CLK_SHIFT,
	.width = WAN_CLK_WIDTH,
	.lock = &ifclk_lock,
};

static void __init wan_clk_setup(struct device_node *node)
{
	mux_clk_setup(node, &wan_clk_data);
}

static const struct mux_clk_data pll5pr_clk_data __initconst = {
	.shift = PLL5PR_CLK_SHIFT,
	.width = PLL5PR_CLK_WIDTH,
	.lock = &pll5_lock,
	.flags = CLK_SET_RATE_PARENT,
	.clk_flags = CLK_SET_RATE_PARENT,
	.def_val = PLL5_CML_MODE,
	.mux_flags = CLK_INIT_DEF_CFG_REQ,
};

static void __init pll5pr_clk_setup(struct device_node *node)
{
	mux_clk_setup(node, &pll5pr_clk_data);
}

static const struct mux_clk_data p2_clk_data __initconst = {
	.shift = P2_CLK_SHIFT,
	.width = P2_CLK_WIDTH,
	.lock = &ifclk_lock,
	.flags = CLK_SET_RATE_PARENT,
};

static void __init p2_clk_setup(struct device_node *node)
{
	mux_clk_setup(node, &p2_clk_data);
}

static const struct mdiv_table pon_div_table[] = {
	{PON_CLK_SHIFT1, PON_CLK_WIDTH1, 7},
	{PON_CLK_SHIFT2, PON_CLK_WIDTH2, 0},
};

static const struct factor_clk_data pon_factor = {
	.shift = PON_CLK_SHIFT3,
	.width = PON_CLK_WIDTH3,
	.mult = 2,
	.div = 5,
	.def_val = 0,
};

static const struct clk_ljpll_div_data pon_clk_data __initconst = {
	.mdiv_tbl = pon_div_table,
	.mdiv_tblsz = ARRAY_SIZE(pon_div_table),
	.opt_factor = &pon_factor,
	.table = ljpll_div,
	.lock = &pll5_lock,
	.flags = CLK_INIT_DEF_CFG_REQ,
	.clk_flags = CLK_SET_RATE_PARENT,
};

static void __init pon_clk_setup(struct device_node *node)
{
	clk_ljpll_div_setup(node, &pon_clk_data);
}

static const struct mdiv_table ponpll_div_table[] = {
	{PONPLL_CLK_SHIFT1, PONPLL_CLK_WIDTH1, 1},
	{PONPLL_CLK_SHIFT2, PONPLL_CLK_WIDTH2, 0},
};

static const struct clk_ljpll_div_data ponpll_clk_data __initconst = {
	.mdiv_tbl = ponpll_div_table,
	.mdiv_tblsz = ARRAY_SIZE(ponpll_div_table),
	.table = ljpll_div,
	.lock = &pll5_lock,
	.flags = CLK_INIT_DEF_CFG_REQ,
};

static void __init ponpll_clk_setup(struct device_node *node)
{
	clk_ljpll_div_setup(node, &ponpll_clk_data);
}

static const struct factor_frac_tbl pll5_ff_table[] = {
	{1200000UL,		150000000UL,  125,  1, 0},
	{1200000UL,		149299200UL,  124,  1, 0x6A7EFA},
	{4665600UL,		149299200UL,  256,  8, 0},
	{18662400UL,		149299200UL,  256, 32, 0},
	{2343750UL,		150000000UL,  256,  4, 0},
	{9375000UL,		150000000UL,  256, 16, 0},
	{4687500UL,		150000000UL,  256,  8, 0},
};

static const struct clk_factor_frac_data pll5_clk_data __initconst = {
	.ff_tbl = pll5_ff_table,
	.ff_tblsz = ARRAY_SIZE(pll5_ff_table),
	.def_idx = 0,
	.frac_div = BIT(24),
	.lock = &pll5_lock,
	.flags = CLK_INIT_DEF_CFG_REQ,
};

static void __init pll5_clk_setup(struct device_node *node)
{
	pll_ff_clk_setup(node, &pll5_clk_data);
}

static const struct gate_clk_data gate2_clk_data __initconst = {
	.mask = GATE2_CLK_MASK,
	.reg_size = 32,
};

static void __init gate2_clk_setup(struct device_node *node)
{
	gate_clk_setup(node, &gate2_clk_data);
}

static const struct gate_clk_data gate1_clk_data __initconst = {
	.mask = GATE1_CLK_MASK,
	.reg_size = 32,
};

static void __init gate1_clk_setup(struct device_node *node)
{
	gate_clk_setup(node, &gate1_clk_data);
}

static const struct gate_clk_data gate0_clk_data __initconst = {
	.mask = GATE0_CLK_MASK,
	.reg_size = 32,
};

static void __init gate0_clk_setup(struct device_node *node)
{
	gate_clk_setup(node, &gate0_clk_data);
}

static const struct gate_clk_data gate3_clk_data __initconst = {
	.mask = GATE3_CLK_MASK,
	.reg_size = 32,
	.flags = V_GATE_CLK,
};

static void __init gate3_clk_setup(struct device_node *node)
{
	gate_clk_setup(node, &gate3_clk_data);
}

CLK_OF_DECLARE(gate3clk, FALCONMX_GATE3_CLK, gate3_clk_setup);
CLK_OF_DECLARE(pll5clk, FALCONMX_PLL5_CLK, pll5_clk_setup);
CLK_OF_DECLARE(ponpllclk, FALCONMX_PONPLL_CLK, ponpll_clk_setup);
CLK_OF_DECLARE(ponclk, FALCONMX_PON_CLK, pon_clk_setup);
CLK_OF_DECLARE(wanclk, FALCONMX_WAN_CLK, wan_clk_setup);
CLK_OF_DECLARE(pll5prclk, FALCONMX_PLL5PR_CLK, pll5pr_clk_setup);
CLK_OF_DECLARE(p2clk, FALCONMX_P2_CLK, p2_clk_setup);
CLK_OF_DECLARE(gate2clk, FALCONMX_GATE2_CLK, gate2_clk_setup);
CLK_OF_DECLARE(gate1clk, FALCONMX_GATE1_CLK, gate1_clk_setup);
CLK_OF_DECLARE(gate0clk, FALCONMX_GATE0_CLK, gate0_clk_setup);
CLK_OF_DECLARE(pcieclk, FALCONMX_PCIE_CLK, pcie_clk_setup);
CLK_OF_DECLARE(ptpclk, FALCONMX_PTP_CLK, ptp_clk_setup);
CLK_OF_DECLARE(syncclk, FALCONMX_SYNC_CLK, sync_clk_setup);
CLK_OF_DECLARE(serdesclk, FALCONMX_SERDES_CLK, serdes_clk_setup);
CLK_OF_DECLARE(gphyclk, FALCONMX_GPHY_CLK, gphy_clk_setup);
CLK_OF_DECLARE(sync0clk, FALCONMX_SYNC0_CLK, sync0_clk_setup);
CLK_OF_DECLARE(sync1clk, FALCONMX_SYNC1_CLK, sync1_clk_setup);
CLK_OF_DECLARE(gpc1clk, FALCONMX_GPC1_CLK, gpc1_clk_setup);
CLK_OF_DECLARE(gpc2clk, FALCONMX_GPC2_CLK, gpc2_clk_setup);
CLK_OF_DECLARE(dclclk, FALCONMX_DCL_CLK, dcl_clk_setup);
CLK_OF_DECLARE(slicclk, FALCONMX_SLIC_CLK, slic_clk_setup);
CLK_OF_DECLARE(vifclk, FALCONMX_VIF_CLK, vif_clk_setup);
CLK_OF_DECLARE(vdspclk, FALCONMX_VDSP_CLK, vdsp_clk_setup);
CLK_OF_DECLARE(ppv4clk, FALCONMX_PPV4_CLK, ppv4_clk_setup);
CLK_OF_DECLARE(ngiclk, FALCONMX_NGI_CLK, ngi_clk_setup);
CLK_OF_DECLARE(ssx4clk, FALCONMX_SSX4_CLK, ssx4_clk_setup);
CLK_OF_DECLARE(swclk, FALCONMX_SWITCH_CLK, sw_clk_setup);
CLK_OF_DECLARE(qspiclk, FALCONMX_QSPI_CLK, qspi_clk_setup);
CLK_OF_DECLARE(cpuclk, FALCONMX_CPU_CLK, cpu_clk_setup);
CLK_OF_DECLARE(ddrclk, FALCONMX_DDR_CLK, ddr_clk_setup);
