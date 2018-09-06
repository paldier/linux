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

struct xway_gphy_data {
	struct device *dev;
	struct reset_control *phy_rst[3];

	dma_addr_t dma_addr; /* DMA address */
	void *virt_addr; /* virtual address */
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
static int xrx500_gphy_boot(struct xway_gphy_data *priv, unsigned int id)
{
	if (id > 4) {
		dev_info(priv->dev, "%u is an invalid gphy id\n", id);
		return -EINVAL;
	}
	reset_control_assert(priv->phy_rst[0]);
	gsw_reg_w32(GSW_L_TOP_BASE, (priv->dma_addr & 0xFFFF),
		    xrx500_gphy[id]);
	gsw_reg_w32(GSW_L_TOP_BASE, ((priv->dma_addr >> 16) & 0xFFFF),
		    (xrx500_gphy[id] + 4));
	reset_control_deassert(priv->phy_rst[0]);
	dev_info(priv->dev, "booting GPHY%u firmware at %X for GRX500\n",
		 id, priv->dma_addr);

	return 0;
}

/* prx300 specific boot sequence */
static int prx300_gphy_boot(struct xway_gphy_data *priv, unsigned int id)
{
	/* set LAN interface to GPHY */
	gsw_reg_w32_mask(PRX300_CHIP_TOP, 0x2, 0x0, PRX300_IFMUX_CFG);

	/* GPHY reset */
	reset_control_assert(priv->phy_rst[0]);
	udelay(500);

	/* CDB and Power Down */
	reset_control_assert(priv->phy_rst[1]);
	reset_control_assert(priv->phy_rst[2]);
	udelay(400);

	/* release CDB reset */
	reset_control_deassert(priv->phy_rst[1]);

	/* GPHY FW address and pin strapping */
	gsw_reg_w32(PRX300_GPHY_CGU_BASE, priv->dma_addr, PRX300_GPHY_FCR);
	gsw_reg_w32(PRX300_GPHY_CGU_BASE, PRX300_GPHY0_GPS0_LO,
		    PRX300_GPHY0_GPS0);
	gsw_reg_w32(PRX300_GPHY_CGU_BASE, PRX300_GPHY0_GPS0_HI,
		    PRX300_GPHY0_GPS1);

	/* release GPHY reset */
	reset_control_deassert(priv->phy_rst[0]);
	udelay(500);

	/* GPHY Power on */
	reset_control_deassert(priv->phy_rst[2]);

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

	dev_info(priv->dev, "booting GPHY%u firmware at %X for PRX300\n",
		 id, priv->dma_addr);

	devm_iounmap(priv->dev, priv->virt_addr);
	return 0;
}

static int xway_gphy_load(struct xway_gphy_data *priv)
{
	const struct firmware *fw;
	const char *fw_name;
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
	 * memory area with a 16 kB boundary aligned start address
	 */
	size = fw->size + XRX200_GPHY_FW_ALIGN;

	if (of_device_is_compatible(priv->dev->of_node, "intel,phy-prx300")) {
		/* Temporary workaround for PRX300 GPHY issue.
		 * Firmware loading in PRX300 GPHY does not work if address
		 * is in DDR. For this reason we hardcode the address to SRAM
		 * location (which is assumed to be free at the beginning of
		 * boot).
		 */
		priv->dma_addr = (dma_addr_t)PRX300_FW_LOAD_ADDR;
		priv->virt_addr = devm_ioremap_nocache(priv->dev,
						       PRX300_FW_LOAD_ADDR,
						       size);
		dev_info(priv->dev, "Temporary use SRAM for firmware %p:%x\n",
			 priv->virt_addr, priv->dma_addr);
	} else {
		priv->virt_addr = dma_alloc_coherent(priv->dev, size,
						     &priv->dma_addr,
						     GFP_KERNEL);
	}

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
	struct property *pp;
	unsigned char *phyids;
	int i, ret = 0;
	char phy_str[16];
	struct xway_gphy_data *priv;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "can't allocate private data\n");
		return -ENOMEM;
	}

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	ret = xway_gphy_load(priv);
	if (ret)
		return -EPROBE_DEFER;

	pp = of_find_property(pdev->dev.of_node, "phy_id", NULL);
	if (!pp)
		return -ENOENT;

	phyids = pp->value;
	for (i = 0; i < pp->length && !ret; i++) {
		sprintf(phy_str, "phy%d", phyids[i]);
		priv->phy_rst[0] = devm_reset_control_get(&pdev->dev, phy_str);
		if (IS_ERR(priv->phy_rst[0])) {
			dev_err(&pdev->dev, "fail to get %s prop\n", phy_str);
			return PTR_ERR(priv->phy_rst[0]);
		}

		if (of_device_is_compatible(pdev->dev.of_node,
					    "lantiq,phy-xrx500")) {
			ret = xrx500_gphy_boot(priv, phyids[i]);
		} else if (of_device_is_compatible(pdev->dev.of_node,
						   "intel,phy-prx300")) {
			/* phy cdb reset */
			sprintf(phy_str, "phy_cdb%d", phyids[i]);
			priv->phy_rst[1] = devm_reset_control_get(&pdev->dev,
								  phy_str);
			if (IS_ERR(priv->phy_rst[1])) {
				dev_err(&pdev->dev, "fail to get %s prop\n",
					phy_str);
				return PTR_ERR(priv->phy_rst[1]);
			}

			/* phy power down */
			sprintf(phy_str, "phy_pwr_down%d", phyids[i]);
			priv->phy_rst[2] = devm_reset_control_get(&pdev->dev,
								  phy_str);
			if (IS_ERR(priv->phy_rst[2])) {
				dev_err(&pdev->dev, "fail to get %s prop\n",
					phy_str);
				return PTR_ERR(priv->phy_rst[2]);
			}

			ret = prx300_gphy_boot(priv, phyids[i]);
		}
	}
	if (!ret)
		mdelay(100);

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

static const struct of_device_id xway_phy_match[] = {
	{ .compatible = "lantiq,phy-xrx500" },
	{ .compatible = "intel,phy-prx300" },
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
