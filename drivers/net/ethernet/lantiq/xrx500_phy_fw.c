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

#include <lantiq_soc.h>

#define XRX200_GPHY_FW_ALIGN	(16 * 1024)

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

	size_t fw_size;
	dma_addr_t dma_addr;
	void *virt_addr;

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

#define GSW_L_TOP_BASE 0xBC003C00

/* prx300 gphy register definition */
#define PRX300_GPHY_CGU_BASE 0xb6200000
#define PRX300_GPHY_FCR 0x800
#define PRX300_GPHY0_GPS0 0x804
#define PRX300_GPHY0_GPS1 0x808
#define PRX300_GPHY0_GPS0_LO 0x3f004000
#define PRX300_GPHY0_GPS0_HI 0
#define PRX300_FW_LOAD_ADDR 0x18da0000

#define PRX300_GPHY_CDB_PDI_BASE 0xb6210100
#define PRX300_GPHY_CDB_PDI_PLL_CFG0 0x0
#define PRX300_GPHY_CDB_PDI_PLL_CFG2 0x8
#define PRX300_GPHY_CDB_PDI_PLL_MISC 0xc
#define PRX300_PLL_FBDIV 0x145
#define PRX300_PLL_REFDIV 0x4
#define PRX300_GPHY_FORCE_LATCH 1
#define PRX300_GPHY_CLEAR_STICKY 1

#define PRX300_CHIP_TOP 0xb6180000
#define PRX300_IFMUX_CFG 0x120

static void gsw_reg_w32(u32 base, u32 val, u32 reg_off)
{
	ltq_w32(val, (void __iomem *)(base + reg_off));
}

static void gsw_reg_w32_mask(u32 base, u32 clear, u32 val, u32 reg_off)
{
	ltq_w32_mask(clear, val, (void __iomem *)(base + reg_off));
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
		gsw_reg_w32(GSW_L_TOP_BASE, (priv->dma_addr & 0xFFFF),
			    xrx500_gphy[i]);
		gsw_reg_w32(GSW_L_TOP_BASE, ((priv->dma_addr >> 16) & 0xFFFF),
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

/* prx300 specific boot sequence */
static int prx300_gphy_boot(struct xway_gphy_data *priv)
{
	struct prx300_reset_control *rst = &priv->rst.prx300;
	void *virt_addr;

	/* Temporary workaround for PRX300 GPHY issue.
	 * Firmware loading in PRX300 GPHY does not work if address
	 * is in DDR. For this reason we hardcode the address to SRAM
	 * location (which is assumed to be free at the beginning of
	 * boot).
	 */
	virt_addr = devm_ioremap_nocache(priv->dev, PRX300_FW_LOAD_ADDR,
					 priv->fw_size);
	memcpy(virt_addr, priv->virt_addr, priv->fw_size);
	dma_free_coherent(priv->dev, priv->fw_size, priv->virt_addr,
			  priv->dma_addr);
	priv->dma_addr = (dma_addr_t)PRX300_FW_LOAD_ADDR;
	priv->virt_addr = virt_addr;
	dev_info(priv->dev, "Temporary use SRAM for firmware %p:%x\n",
		 priv->virt_addr, priv->dma_addr);

	/* set LAN interface to GPHY */
	gsw_reg_w32_mask(PRX300_CHIP_TOP, 0x2, 0x0, PRX300_IFMUX_CFG);

	/* GPHY reset */
	reset_control_assert(rst->gphy);
	usleep_range(500, 1000);

	/* CDB and Power Down */
	reset_control_assert(rst->gphy_cdb);
	reset_control_assert(rst->gphy_pwr_down);
	usleep_range(400, 1000);

	/* release CDB reset */
	reset_control_deassert(rst->gphy_cdb);

	/* GPHY FW address and pin strapping */
	gsw_reg_w32(PRX300_GPHY_CGU_BASE, priv->dma_addr, PRX300_GPHY_FCR);
	gsw_reg_w32(PRX300_GPHY_CGU_BASE, PRX300_GPHY0_GPS0_LO,
		    PRX300_GPHY0_GPS0);
	gsw_reg_w32(PRX300_GPHY_CGU_BASE, PRX300_GPHY0_GPS0_HI,
		    PRX300_GPHY0_GPS1);

	/* release GPHY reset */
	reset_control_deassert(rst->gphy);
	usleep_range(500, 1000);

	/* GPHY Power on */
	reset_control_deassert(rst->gphy_pwr_down);

	/* Set divider and misc config */
	gsw_reg_w32_mask(PRX300_GPHY_CDB_PDI_BASE, 0xFFF0,
			 (PRX300_PLL_FBDIV << 4), PRX300_GPHY_CDB_PDI_PLL_CFG0);
	gsw_reg_w32(PRX300_GPHY_CDB_PDI_BASE, (PRX300_PLL_REFDIV << 8),
		    PRX300_GPHY_CDB_PDI_PLL_CFG2);
	gsw_reg_w32_mask(PRX300_GPHY_CDB_PDI_BASE,
			 (PRX300_GPHY_FORCE_LATCH << 13) |
			  (PRX300_GPHY_CLEAR_STICKY << 14),
			 (PRX300_GPHY_FORCE_LATCH << 13) |
			  (PRX300_GPHY_CLEAR_STICKY << 14),
			 PRX300_GPHY_CDB_PDI_PLL_MISC);

	/* delay to wait until firmware boots up */
	msleep(100);

	dev_info(priv->dev, "booting GPHY firmware for PRX300\n");

	devm_iounmap(priv->dev, priv->virt_addr);
	return 0;
}

static int prx300_dt_parse(struct xway_gphy_data *priv)
{
	struct prx300_reset_control *rst = &priv->rst.prx300;

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
	 * memory area with a 16 kB boundary aligned start address
	 */
	priv->fw_size = fw->size + XRX200_GPHY_FW_ALIGN;
	priv->virt_addr = dma_alloc_coherent(priv->dev, priv->fw_size,
					     &priv->dma_addr, GFP_KERNEL);
	if (!priv->virt_addr) {
		dev_err(priv->dev, "failed to alloc firmware memory\n");
		release_firmware(fw);
		return -ENOMEM;
	}

	priv->virt_addr = PTR_ALIGN(priv->virt_addr, XRX200_GPHY_FW_ALIGN);
	priv->dma_addr = ALIGN(priv->dma_addr, XRX200_GPHY_FW_ALIGN);
	memcpy(priv->virt_addr, fw->data, fw->size);

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
};

static struct xway_gphy_soc_data prx300_gphy_data = {
	.boot_func = &prx300_gphy_boot,
	.dt_parse_func = &prx300_dt_parse,
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
