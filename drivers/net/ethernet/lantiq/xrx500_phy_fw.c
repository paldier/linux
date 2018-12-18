/*
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  Copyright (C) 2012 John Crispin <blogic@openwrt.org>
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/of_platform.h>
#include <linux/reset.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define XRX500_GPHY_NUM 5 /* phy2-5 + phyf */
struct xrx500_reset_control {
	struct reset_control *phy[XRX500_GPHY_NUM];
};

struct prx300_reset_control {
	struct reset_control *gphy;
	struct reset_control *gphy_cdb;
	struct reset_control *gphy_pwr_down;
};

struct xway_gphy_data {
	struct device *dev;
	struct regmap *syscfg, *cgu_syscfg, *chipid_syscfg, *aspa_syscfg;
	void __iomem *base, *fcsi_base;

	dma_addr_t dma_addr;

	/* Number of resets and names are SoC specific. Hence we place it as
	 * union here.
	 */
	union {
		struct xrx500_reset_control xrx500;
		struct prx300_reset_control prx300;
	} rst;

	/* SoC specific functions */
	const struct xway_gphy_soc_data {
		int (*boot_func)(struct xway_gphy_data *);
		int (*dt_parse_func)(struct xway_gphy_data *);
		int align;
	} *soc_data;
};

/* GPHY related */
static int g_xway_gphy_fw_loaded;

/* xRX500 gphy (GSW-L) registers */
#define GPHY2_LBADR_XRX500     0x0228
#define GPHY2_MBADR_XRX500     0x022C
#define GPHY3_LBADR_XRX500     0x0238
#define GPHY3_MBADR_XRX500     0x023c
#define GPHY4_LBADR_XRX500     0x0248
#define GPHY4_MBADR_XRX500     0x024C
#define GPHY5_LBADR_XRX500     0x0258
#define GPHY5_MBADR_XRX500     0x025C
#define GPHYF_LBADR_XRX500     0x0268
#define GPHYF_MBADR_XRX500     0x026C

/* reset / boot a gphy */
static u32 xrx500_gphy[] = {
	GPHY2_LBADR_XRX500,
	GPHY3_LBADR_XRX500,
	GPHY4_LBADR_XRX500,
	GPHY5_LBADR_XRX500,
	GPHYF_LBADR_XRX500,
};

/* prx300 register definition */
/* CGU0 */
#define PRX300_GPHY_FCR 0x800
#define PRX300_GPHY0_GPS0 0x804
#define PRX300_GPHY0_GPS1 0x808
/* ASPA CFG */
#define PRX300_ASPA_CFG_CTRL 0x0
#define PRX300_ASPA_BUS_BLOCK 0x8
/* GPHY CDB FCSI */
#define PRX300_GPHY_CDB_FCSI_PLL_CFG1 0x0
#define PRX300_GPHY_CDB_FCSI_PLL_CFG2 0x4
#define PRX300_GPHY_CDB_FCSI_PLL_RCMSTAT 0x28
#define PRX300_GPHY_CDB_FCSI_PLL_RCMCFG 0x2c
/* GPHY CDB PDI */
#define PRX300_GPHY_CDB_PDI_PLL_CFG0 0x0
#define PRX300_GPHY_CDB_PDI_PLL_CFG2 0x8
#define PRX300_GPHY_CDB_PDI_PLL_MISC 0xc
#define PRX300_PLL_FBDIV 0x145
#define PRX300_PLL_LOCK_RST 0xb
#define PRX300_PLL_REFDIV 0x4
#define PRX300_GPHY_FORCE_LATCH 1
#define PRX300_GPHY_CLEAR_STICKY 1
/* Chiptop */
#define PRX300_IFMUX_CFG 0x120
#define PRX300_LAN_MUX_MASK 0x2
#define PRX300_LAN_MUX_GPHY 0x0
/* Chipid */
#define PRX300_FUSE_REDUND_1 0x74
/* RCM calculation */
#define GPHYCDB_RCM_C 188
#define GPHYCDB_RCM_D 1
#define GPHYCDB_RCM_M 4110

static u32 gsw_reg_r32(void __iomem *base, u32 reg_off)
{
	return __raw_readl(base + reg_off);
}

static void gsw_reg_w32(void __iomem *base, u32 val, u32 reg_off)
{
	__raw_writel(val, base + reg_off);
}

static void gsw_reg_w32_mask(void __iomem *base, u32 clear, u32 set,
			     u32 reg_off)
{
	u32 val = gsw_reg_r32(base, reg_off);

	gsw_reg_w32(base, (val & (~clear)) | set, reg_off);
}

/* xrx500 specific boot sequence */
static int xrx500_gphy_boot(struct xway_gphy_data *priv)
{
	int i;
	struct xrx500_reset_control *rst = &priv->rst.xrx500;

	for (i = 0; i < XRX500_GPHY_NUM; i++) {
		if (!rst->phy[i])
			continue;

		reset_control_assert(rst->phy[i]);
		gsw_reg_w32(priv->base, (priv->dma_addr & 0xFFFF),
			    xrx500_gphy[i]);
		gsw_reg_w32(priv->base, ((priv->dma_addr >> 16) & 0xFFFF),
			    (xrx500_gphy[i] + 4));
		reset_control_deassert(rst->phy[i]);
		dev_info(priv->dev, "booting GPHY%u firmware for GRX500\n", i);
	}

	return 0;
}

static int xrx500_dt_parse(struct xway_gphy_data *priv)
{
	char phy_str[8];
	int i;
	struct xrx500_reset_control *rst = &priv->rst.xrx500;
	struct resource *res;
	struct platform_device *pdev = container_of(priv->dev,
						    struct platform_device,
						    dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(priv->dev, "no resources\n");
		return -ENODEV;
	}

	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (!priv->base)
		return -ENOMEM;

	for (i = 0; i < XRX500_GPHY_NUM; i++) {
		snprintf(phy_str, sizeof(phy_str), "phy%d", i);

		rst->phy[i] = devm_reset_control_get_optional(priv->dev,
							      phy_str);
		if (IS_ERR(rst->phy[i])) {
			dev_err(priv->dev, "fail to get %s prop\n", phy_str);
			return PTR_ERR(rst->phy[i]);
		}
	}

	return 0;
}

/* prx300 rcal/rc_count value calculation.
 * Calculation routines are very much hardware specific,
 * and derived from verification team recommendation.
 */
static u32 prx300_gphy_config_rcal_rcm(struct xway_gphy_data *priv)
{
	int rcal = 0x08, rc_count = 0x10;
	int rc_count_pol_0 = 0, rc_count_pol_1 = 0, rc_count_avg;
	u32 val;
	int retry;

	/* get rcal from fused register (upper 4-bits) */
	regmap_read(priv->chipid_syscfg, PRX300_FUSE_REDUND_1,
		    &val);
	val = val >> 28;

	/* no fused values, simply use default settings */
	if (!val)
		goto exit;

	/* rcm calculation routine */

	/* enable block access */
	regmap_update_bits(priv->aspa_syscfg, PRX300_ASPA_CFG_CTRL,
			   PRX300_ASPA_BUS_BLOCK, PRX300_ASPA_BUS_BLOCK);

	/* configure rcm clock and FOUT0-3 enable bit */
	gsw_reg_w32_mask(priv->fcsi_base, 0xF << 8, 0x6 << 8,
			 PRX300_GPHY_CDB_FCSI_PLL_CFG2);
	gsw_reg_w32_mask(priv->fcsi_base, 0xF << 2, 0x4 << 2,
			 PRX300_GPHY_CDB_FCSI_PLL_CFG1);

	/* get rc_count with polarity 0 */
	gsw_reg_w32(priv->fcsi_base, 0x3, PRX300_GPHY_CDB_FCSI_PLL_RCMCFG);
	usleep_range(1, 2);
	gsw_reg_w32(priv->fcsi_base, 0x1, PRX300_GPHY_CDB_FCSI_PLL_RCMCFG);
	usleep_range(1, 2);
	gsw_reg_w32(priv->fcsi_base, 0x5, PRX300_GPHY_CDB_FCSI_PLL_RCMCFG);

	retry = 10;
	while (--retry) {
		if (gsw_reg_r32(priv->fcsi_base,
				PRX300_GPHY_CDB_FCSI_PLL_RCMSTAT) & 0x400)
			break;
		usleep_range(1, 2);
	}
	if (!retry) {
		dev_err(priv->dev, "Timedout at rcal calculation\n");
		goto exit;
	}

	rc_count_pol_0 = gsw_reg_r32(priv->fcsi_base,
				     PRX300_GPHY_CDB_FCSI_PLL_RCMSTAT) & 0x3FF;

	/* get rc_count with polarity 1 */
	gsw_reg_w32(priv->fcsi_base, 0xb, PRX300_GPHY_CDB_FCSI_PLL_RCMCFG);
	usleep_range(1, 2);
	gsw_reg_w32(priv->fcsi_base, 0x1, PRX300_GPHY_CDB_FCSI_PLL_RCMCFG);
	usleep_range(1, 2);
	gsw_reg_w32(priv->fcsi_base, 0x5, PRX300_GPHY_CDB_FCSI_PLL_RCMCFG);

	retry = 10;
	while (--retry) {
		if (gsw_reg_r32(priv->fcsi_base,
				PRX300_GPHY_CDB_FCSI_PLL_RCMSTAT) & 0x400)
			break;
		usleep_range(1, 2);
	}
	if (!retry) {
		dev_err(priv->dev, "Timedout at rcal calculation\n");
		goto exit;
	}

	rc_count_pol_1 = gsw_reg_r32(priv->fcsi_base,
				     PRX300_GPHY_CDB_FCSI_PLL_RCMSTAT) & 0x3FF;

	/* Polarity swap tests: if one result is zero we skip averaging */
	rc_count_avg = rc_count_pol_0 + rc_count_pol_1;
	if (rc_count_pol_0 && rc_count_pol_1)
		rc_count_avg = rc_count_avg / 2;

	/* check extreme values */
	if (rc_count_avg < GPHYCDB_RCM_C + GPHYCDB_RCM_D) {
		rc_count = 0;
	} else {
		rc_count = GPHYCDB_RCM_M * (rc_count_avg - GPHYCDB_RCM_C -
					    GPHYCDB_RCM_D);
		rc_count = rc_count >> 15;
		if (rc_count > 0x1f)
			rc_count = 0x1f;
	}

	/* disable RCMCFG and disable PLLCFG1 */
	gsw_reg_w32(priv->fcsi_base, 0x0, PRX300_GPHY_CDB_FCSI_PLL_RCMCFG);
	gsw_reg_w32_mask(priv->fcsi_base, 0xF << 2, 0x0 << 2,
			 PRX300_GPHY_CDB_FCSI_PLL_CFG1);

exit:
	dev_dbg(priv->dev, "rcal %x rc_count %x rccal 0x%x:0x%x\n",
		rcal, rc_count, rc_count_pol_0, rc_count_pol_1);
	return (rc_count << 4) | rcal;
}

/* prx300 specific boot sequence */
static int prx300_gphy_boot(struct xway_gphy_data *priv)
{
	struct prx300_reset_control *rst = &priv->rst.prx300;
	u32 pin_strap_lo, pin_strap_hi;

	/* set LAN interface to GPHY */
	regmap_update_bits(priv->syscfg, PRX300_IFMUX_CFG, PRX300_LAN_MUX_MASK,
			   PRX300_LAN_MUX_GPHY);

	/* GPHY reset */
	reset_control_assert(rst->gphy);
	usleep_range(500, 1000);

	/* CDB and Power Down */
	reset_control_assert(rst->gphy_cdb);
	reset_control_assert(rst->gphy_pwr_down);
	usleep_range(400, 1000);

	/* release CDB reset */
	reset_control_deassert(rst->gphy_cdb);

	/* Set divider and misc config, must be done before rcm calculation */
	gsw_reg_w32(priv->base, (PRX300_PLL_FBDIV << 4) | PRX300_PLL_LOCK_RST,
		    PRX300_GPHY_CDB_PDI_PLL_CFG0);
	gsw_reg_w32(priv->base, (PRX300_PLL_REFDIV << 8),
		    PRX300_GPHY_CDB_PDI_PLL_CFG2);
	gsw_reg_w32(priv->base, (PRX300_GPHY_FORCE_LATCH << 13) |
		    (PRX300_GPHY_CLEAR_STICKY << 14),
		    PRX300_GPHY_CDB_PDI_PLL_MISC);

	/* GPHY FW address */
	regmap_update_bits(priv->cgu_syscfg, PRX300_GPHY_FCR, ~0,
			   priv->dma_addr);

	pin_strap_lo = 0x4000; /* base freq deviation */
	pin_strap_lo |= 0x1f << 24; /* MDIO address */
	pin_strap_lo |= 0x1 << 29; /* interrupt polarity */
	pin_strap_hi = prx300_gphy_config_rcal_rcm(priv);

	regmap_update_bits(priv->cgu_syscfg, PRX300_GPHY0_GPS0, ~0,
			   pin_strap_lo);
	regmap_update_bits(priv->cgu_syscfg, PRX300_GPHY0_GPS1, ~0,
			   pin_strap_hi);

	/* release GPHY reset */
	reset_control_deassert(rst->gphy);
	usleep_range(500, 1000);

	/* GPHY Power on */
	reset_control_deassert(rst->gphy_pwr_down);

	/* delay to wait until firmware boots up */
	msleep(100);

	dev_info(priv->dev, "booting GPHY firmware for PRX300\n");
	return 0;
}

static int prx300_dt_parse(struct xway_gphy_data *priv)
{
	struct prx300_reset_control *rst = &priv->rst.prx300;
	struct resource *res;
	struct platform_device *pdev = container_of(priv->dev,
						    struct platform_device,
						    dev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "gphy_cdb_pdi");
	if (!res) {
		dev_err(&pdev->dev, "no cdb_pdi resources\n");
		return -ENODEV;
	}
	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (!priv->base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "gphy_cdb_fcsi");
	if (!res) {
		dev_err(&pdev->dev, "no cdb_fcsi resources\n");
		return -ENODEV;
	}
	priv->fcsi_base = devm_ioremap_resource(&pdev->dev, res);
	if (!priv->fcsi_base)
		return -ENOMEM;

	/* get chiptop regmap */
	priv->syscfg = syscon_regmap_lookup_by_phandle(priv->dev->of_node,
						       "intel,syscon");
	if (IS_ERR(priv->syscfg)) {
		dev_err(priv->dev, "No phandle for intel,syscon\n");
		return PTR_ERR(priv->syscfg);
	}

	/* get CGU regmap */
	priv->cgu_syscfg = syscon_regmap_lookup_by_phandle(priv->dev->of_node,
							   "intel,cgu-syscon");
	if (IS_ERR(priv->cgu_syscfg)) {
		dev_err(priv->dev, "No phandle for intel,cgu-syscon\n");
		return PTR_ERR(priv->cgu_syscfg);
	}

	/* get chipid regmap */
	priv->chipid_syscfg = syscon_regmap_lookup_by_phandle(priv->dev->of_node,
							      "intel,chipid-syscon");
	if (IS_ERR(priv->chipid_syscfg)) {
		dev_err(priv->dev, "No phandle for intel,chipid-syscon\n");
		return PTR_ERR(priv->chipid_syscfg);
	}

	/* get aspa_cfg regmap */
	priv->aspa_syscfg = syscon_regmap_lookup_by_phandle(priv->dev->of_node,
							    "intel,aspa-syscon");
	if (IS_ERR(priv->aspa_syscfg)) {
		dev_err(priv->dev, "No phandle for intel,aspa-syscon\n");
		return PTR_ERR(priv->aspa_syscfg);
	}

	rst->gphy = devm_reset_control_get(priv->dev, "gphy");
	if (IS_ERR(rst->gphy)) {
		dev_err(priv->dev, "fail to get gphy prop\n");
		return PTR_ERR(rst->gphy);
	}

	rst->gphy_cdb = devm_reset_control_get(priv->dev, "gphy_cdb");
	if (IS_ERR(rst->gphy_cdb)) {
		dev_err(priv->dev, "fail to get gphy_cdb prop\n");
		return PTR_ERR(rst->gphy_cdb);
	}

	rst->gphy_pwr_down = devm_reset_control_get(priv->dev, "gphy_pwr_down");
	if (IS_ERR(rst->gphy_pwr_down)) {
		dev_err(priv->dev, "fail to get gphy_pwr_down prop\n");
		return PTR_ERR(rst->gphy_pwr_down);
	}

	return 0;
}

static int xway_gphy_load(struct xway_gphy_data *priv)
{
	const struct firmware *fw;
	const char *fw_name;
	void *virt_addr;
	size_t size;

	if (of_property_read_string(priv->dev->of_node, "firmware", &fw_name)) {
		dev_err(priv->dev, "failed to load firmware filename\n");
		return -EINVAL;
	}

	dev_info(priv->dev, "requesting %s\n", fw_name);
	if (request_firmware(&fw, fw_name, priv->dev)) {
		dev_err(priv->dev, "failed to load firmware: %s\n", fw_name);
		return -EIO;
	}

	/**
	 * GPHY cores need the firmware code in a persistent and contiguous
	 * memory area with a boundary aligned start address.
	 */
	size = fw->size + priv->soc_data->align;
	virt_addr = dmam_alloc_coherent(priv->dev, size, &priv->dma_addr,
					GFP_KERNEL);
	if (!virt_addr) {
		dev_err(priv->dev, "failed to alloc firmware memory\n");
		release_firmware(fw);
		return -ENOMEM;
	}

	virt_addr = PTR_ALIGN(virt_addr, priv->soc_data->align);
	priv->dma_addr = ALIGN(priv->dma_addr, priv->soc_data->align);
	memcpy(virt_addr, fw->data, fw->size);

	release_firmware(fw);
	return 0;
}

static int xway_phy_fw_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct xway_gphy_data *priv;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "can't allocate private data\n");
		return -ENOMEM;
	}

	priv->soc_data = of_device_get_match_data(&pdev->dev);
	if (!priv->soc_data) {
		dev_err(&pdev->dev, "Failed to find soc data!\n");
		return -ENODEV;
	}

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	ret = priv->soc_data->dt_parse_func(priv);
	if (ret)
		return -EINVAL;

	ret = xway_gphy_load(priv);
	if (ret)
		return -EPROBE_DEFER;

	ret = priv->soc_data->boot_func(priv);
	if (!ret)
		msleep(100);

	g_xway_gphy_fw_loaded = 1;
	return ret;
}

bool is_xway_gphy_fw_loaded(void)
{
	if (g_xway_gphy_fw_loaded)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_xway_gphy_fw_loaded);

static struct xway_gphy_soc_data xrx500_gphy_data = {
	.boot_func = &xrx500_gphy_boot,
	.dt_parse_func = &xrx500_dt_parse,
	.align = 16 * 1024,
};

static struct xway_gphy_soc_data prx300_gphy_data = {
	.boot_func = &prx300_gphy_boot,
	.dt_parse_func = &prx300_dt_parse,
	.align = 128 * 1024,
};

static const struct of_device_id xway_phy_match[] = {
	{
		.compatible = "lantiq,phy-xrx500",
		.data = &xrx500_gphy_data,
	},
	{
		.compatible = "intel,phy-prx300",
		.data = &prx300_gphy_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, xway_phy_match);

static struct platform_driver xway_phy_driver = {
	.probe = xway_phy_fw_probe,
	.driver = {
		.name = "phy-xrx500",
		.owner = THIS_MODULE,
		.of_match_table = xway_phy_match,
	},
};

int __init xrx500_phy_fw_init(void)
{
	return platform_driver_register(&xway_phy_driver);
}
module_init(xrx500_phy_fw_init);

MODULE_AUTHOR("John Crispin <blogic@openwrt.org>");
MODULE_DESCRIPTION("Lantiq XRX200 PHY Firmware Loader");
MODULE_LICENSE("GPL");
