#ifndef __INTEL_FALCONMX_CLK_H_
#define __INTEL_FALCONMX_CLK_H_

/* Intel Falconmx CGU device tree "compatible" strings */
#define FALCONMX_DDR_CLK		"intel,falconmx-ddrclk"
#define FALCONMX_CPU_CLK		"intel,falconmx-cpuclk"
#define FALCONMX_QSPI_CLK		"intel,falconmx-qspiclk"
#define FALCONMX_SWITCH_CLK		"intel,falconmx-swclk"
#define FALCONMX_SSX4_CLK		"intel,falconmx-ssx4clk"
#define FALCONMX_NGI_CLK		"intel,falconmx-ngiclk"
#define FALCONMX_PPV4_CLK		"intel,falconmx-ppv4clk"
#define FALCONMX_VDSP_CLK		"intel,falconmx-vdspclk"
#define FALCONMX_VIF_CLK		"intel,falconmx-vifclk"
#define FALCONMX_SLIC_CLK		"intel,falconmx-slicclk"
#define FALCONMX_DCL_CLK		"intel,falconmx-dclclk"
#define FALCONMX_SYNC0_CLK		"intel,falconmx-sync0clk"
#define FALCONMX_SYNC1_CLK		"intel,falconmx-sync1clk"
#define FALCONMX_GPC1_CLK		"intel,falconmx-gpc1clk"
#define FALCONMX_GPC2_CLK		"intel,falconmx-gpc2clk"
#define FALCONMX_GPHY_CLK		"intel,falconmx-gphyclk"
#define FALCONMX_SERDES_CLK		"intel,falconmx-serdesclk"
#define FALCONMX_SYNC_CLK		"intel,falconmx-syncclk"
#define FALCONMX_PTP_CLK		"intel,falconmx-ptpclk"
#define FALCONMX_PCIE_CLK		"intel,falconmx-pcieclk"
#define FALCONMX_GATE0_CLK		"intel,falconmx-gate0clk"
#define FALCONMX_GATE1_CLK		"intel,falconmx-gate1clk"
#define FALCONMX_GATE2_CLK		"intel,falconmx-gate2clk"
#define FALCONMX_GATE3_CLK		"intel,falconmx-gate3clk"
#define FALCONMX_P2_CLK			"intel,falconmx-p2clk"
#define FALCONMX_PLL5PR_CLK		"intel,falconmx-pll5prclk"
#define FALCONMX_PLL5_CLK		"intel,falconmx-pll5clk"
#define FALCONMX_PON_CLK		"intel,falconmx-ponclk"
#define FALCONMX_PONPLL_CLK		"intel,falconmx-ponpllclk"
#define FALCONMX_WAN_CLK		"intel,falconmx-wanclk"

/* clock shift and width */
/* PLL2 CLK */
#define DDR_CLK_SHIFT		0
#define DDR_CLK_WIDTH		4
/* PLL0A CLK */
#define CPU_CLK_SHIFT		0
#define CPU_CLK_WIDTH		4
#define QSPI_CLK_SHIFT		12
#define QSPI_CLK_WIDTH		4
/* PLLOB CLK */
#define SWITCH_CLK_SHIFT	0
#define SWITCH_CLK_WIDTH	4
#define SSX4_CLK_SHIFT		4
#define SSX4_CLK_WIDTH		4
#define NGI_CLK_SHIFT		8
#define NGI_CLK_WIDTH		4
#define PPV4_CLK_SHIFT		12
#define PPV4_CLK_WIDTH		4
/* PLL1 CLK */
#define VDSP_CLK_SHIFT		0
#define VDSP_CLK_WIDTH		4
#define VIF_CLK_SHIFT		4
#define VIF_CLK_WIDTH		4
#define SLIC_CLK_SHIFT		14
#define SLIC_CLK_WIDTH		2
#define SLIC_SET_VAL		2
#define DCL_CLK_SHIFT		25
#define DCL_CLK_WIDTH		3
/* PLL3 CLK */
#define GPHY_CLK_SHIFT1		0
#define GPHY_CLK_WIDTH1		3
#define GPHY_CLK_SHIFT2		3
#define GPHY_CLK_WIDTH2		3
#define GPHY_CLK_SHIFT3		29
#define GPHY_CLK_WIDTH3		1
#define SERDES_CLK_SHIFT1	6
#define SERDES_CLK_WIDTH1	3
#define SERDES_CLK_SHIFT2	9
#define SERDES_CLK_WIDTH2	3
#define SYNC_CLK_SHIFT1		12
#define SYNC_CLK_WIDTH1		3
#define SYNC_CLK_SHIFT2		15
#define SYNC_CLK_WIDTH2		3
#define PTP_CLK_SHIFT1		18
#define PTP_CLK_WIDTH1		3
#define PTP_CLK_SHIFT2		21
#define PTP_CLK_WIDTH2		3
#define SYNC0_CLK_SHIFT		16
#define SYNC0_CLK_WIDTH		1
#define SYNC1_CLK_SHIFT		17
#define SYNC1_CLK_WIDTH		2
#define GPC1_CLK_SHIFT		12
#define GPC1_CLK_WIDTH		2
#define GPC2_CLK_SHIFT		10
#define GPC2_CLK_WIDTH		2
/* PLL4 CLK */
#define PCIE_CLK_SHIFT1		0
#define PCIE_CLK_WIDTH1		3
#define PCIE_CLK_SHIFT2		3
#define PCIE_CLK_WIDTH2		3
#define PCIE_CLK_SHIFT3		29
#define PCIE_CLK_WIDTH3		1

/* PLL5 CLK */
#define PON_CLK_SHIFT1		0
#define PON_CLK_WIDTH1		3
#define PON_CLK_SHIFT2		3
#define PON_CLK_WIDTH2		3
#define PON_CLK_SHIFT3		29
#define PON_CLK_WIDTH3		1
#define PONPLL_CLK_SHIFT1	6
#define PONPLL_CLK_WIDTH1	3
#define PONPLL_CLK_SHIFT2	9
#define PONPLL_CLK_WIDTH2	3

/* P2 CLK */
#define P2_CLK_SHIFT		3
#define P2_CLK_WIDTH		1

/* PLL5PR CLK */
#define PLL5PR_CLK_SHIFT	28
#define PLL5PR_CLK_WIDTH	1

/* WAN CLK */
#define WAN_CLK_SHIFT		0
#define WAN_CLK_WIDTH		1

/* clocks under gate0-clk */
#define GATE_XBAR0_CLK		0
#define GATE_XBAR1_CLK		1
#define GATE_XBAR2_CLK		2
#define GATE_XBAR7_CLK		7

/* clocks under gate1-clk */
#define GATE_V_CODEC_CLK	0
#define GATE_DMA0_CLK		1
#define GATE_I2C0_CLK		2
#define GATE_I2C1_CLK		3
#define GATE_I2C2_CLK		4
#define GATE_SPI1_CLK		5
#define GATE_SPI0_CLK		6
#define GATE_QSPI_CLK		7
#define GATE_CQEM_CLK		8
#define GATE_SSO_CLK		9
#define GATE_GPTC0_CLK		10
#define GATE_GPTC1_CLK		11
#define GATE_GPTC2_CLK		12
#define GATE_URT0_CLK		13
#define GATE_URT1_CLK		14
#define GATE_EIP123_CLK		15
#define GATE_SCPU_CLK		16
#define GATE_MPE_CLK		17
#define GATE_TDM_CLK		18
#define GATE_PP_CLK		19
#define GATE_DMA3_CLK		20
#define GATE_SWITCH_CLK		21
#define GATE_PON_CLK		22
#define GATE_AON_CLK		23
#define GATE_DDR_CLK		24

/* clocks under gate2-clk */
#define GATE_CTRL0_CLK		0
#define GATE_MSI0_CLK		1
#define GATE_CTRL1_CLK		2
#define GATE_MSI1_CLK		3

/* clocks under gate3-clk */
#define GATE_SW_REF		0
#define GATE_SERDES0		1
#define GATE_SERDES1		2

/* Gate clock mask */
#define GATE0_CLK_MASK		0x87
#define GATE1_CLK_MASK		0xFEE67FFC
#define GATE2_CLK_MASK		0x60006
#define GATE3_CLK_MASK		0x3000010

/* Gate clock bits */
#define GATE_XBAR0		0
#define GATE_XBAR1		1
#define GATE_XBAR2		2
#define GATE_XBAR7		7

#define GATE_V_CODEC		2
#define GATE_DMA0		3
#define GATE_I2C0		4
#define GATE_I2C1		5
#define GATE_I2C2		6
#define GATE_SPI1		7
#define GATE_SPI0		8
#define GATE_QSPI		9
#define GATE_CQEM		10
#define GATE_SSO		11
#define GATE_GPTC0		12
#define GATE_GPTC1		13
#define GATE_GPTC2		14
#define GATE_UART0		17
#define GATE_UART1		18
#define GATE_EIP123		21
#define GATE_SCPU		22
#define GATE_MPE		23
#define GATE_TDM		25
#define GATE_PP			26
#define GATE_DMA3		27
#define GATE_SWITCH		28
#define GATE_PON		29
#define GATE_AON		30
#define GATE_DDR		31

#define GATE_PCIE0_CTRL		1
#define GATE_PCIE0_MSI		2
#define GATE_PCIE1_CTRL		17
#define GATE_PCIE1_MSI		18

#endif /* __INTEL_FALCONMX_CLK_H_ */
