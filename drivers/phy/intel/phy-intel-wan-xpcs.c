/*
 * Intel WAN XPCS PHY driver
 *
 * Copyright (C) 2018 Intel, Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

/* chiptop aon/pon config; this is platform specific */
#define CHIP_TOP_IFMUX_CFG 0x120
#define WAN_MUX_AON        0x1
#define WAN_MUX_MASK       0x1

enum {
	PHY_RST,
	WANSS_RST,
	MAX_RST
};

static const char *rst_name[MAX_RST] = {
	"phy", "wanss"
};

struct intel_wan_xpcs_phy {
	struct phy             *phy;
	struct platform_device *pdev;
	struct device          *dev;
	struct clk             *clk;
	u32                    clk_freq;
	struct regmap          *syscfg;
	struct reset_control   *resets[MAX_RST];
};

static int intel_wan_xpcs_phy_init(struct phy *phy)
{
	struct intel_wan_xpcs_phy *priv = phy_get_drvdata(phy);

	dev_dbg(priv->dev, "Initializing intel wan xpcs phy\n");

	/* set WAN_MUX to AON mode */
	regmap_update_bits(priv->syscfg, CHIP_TOP_IFMUX_CFG, WAN_MUX_MASK,
			   WAN_MUX_AON);
	return 0;
}

static int intel_wan_xpcs_phy_power_on(struct phy *phy)
{
	int ret;
	struct intel_wan_xpcs_phy *priv = phy_get_drvdata(phy);
	struct device *dev = priv->dev;

	dev_dbg(priv->dev, "Power on intel wan xpcs phy\n");

	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(dev, "Failed to enable PHY gate clock\n");
		return ret;
	}

	if (priv->clk_freq)
		clk_set_rate(priv->clk, priv->clk_freq);

	ret = reset_control_deassert(priv->resets[PHY_RST]);
	if (ret) {
		dev_err(dev, "Failed to deassert phy reset\n");
		return ret;
	}

	ret = reset_control_deassert(priv->resets[WANSS_RST]);
	if (ret) {
		dev_err(dev, "Failed to deassert wanss reset\n");
		return ret;
	}
	udelay(2);

	return 0;
}

static int intel_wan_xpcs_phy_power_off(struct phy *phy)
{
	int ret;
	struct intel_wan_xpcs_phy *priv = phy_get_drvdata(phy);
	struct device *dev = priv->dev;

	dev_dbg(priv->dev, "Power off intel xpcs phy\n");

	ret = reset_control_assert(priv->resets[PHY_RST]);
	if (ret) {
		dev_err(dev, "Failed to assert phy reset\n");
		return ret;
	}

	ret = reset_control_assert(priv->resets[WANSS_RST]);
	if (ret) {
		dev_err(dev, "Failed to assert WANSS reset\n");
		return ret;
	}
	clk_disable_unprepare(priv->clk);

	return 0;
}

static int intel_wan_xpcs_phy_dt_parse(struct intel_wan_xpcs_phy *priv)
{
	struct device *dev = priv->dev;
	struct device_node *np = dev->of_node;
	int i;

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "Failed to retrieve clk\n");
		return PTR_ERR(priv->clk);
	}

	if (device_property_read_u32(dev, "clock-frequency", &priv->clk_freq))
		priv->clk_freq = 0;

	for (i = 0; i < MAX_RST; i++) {
		priv->resets[i] = devm_reset_control_get(dev, rst_name[i]);
		if (IS_ERR(priv->resets[i])) {
			dev_err(dev, "Failed to retrieve rst controller\n");
			return PTR_ERR(priv->resets[i]);
		}
	}

	/* get chiptop regmap */
	priv->syscfg =
		syscon_regmap_lookup_by_phandle(np,
						"intel,syscon");
	if (IS_ERR(priv->syscfg)) {
		dev_err(dev, "No phandle specified for xpcs-phy syscon\n");
		return PTR_ERR(priv->syscfg);
	}

	return 0;
}

static const struct phy_ops ops = {
	.init		= intel_wan_xpcs_phy_init,
	.power_on	= intel_wan_xpcs_phy_power_on,
	.power_off	= intel_wan_xpcs_phy_power_off,
	.owner		= THIS_MODULE,
};

static int intel_wan_xpcs_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct intel_wan_xpcs_phy *priv;
	struct phy_provider *phy_provider;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->pdev = pdev;
	platform_set_drvdata(pdev, priv);

	ret = intel_wan_xpcs_phy_dt_parse(priv);
	if (ret)
		return ret;

	priv->phy = devm_phy_create(dev, dev->of_node, &ops);
	if (IS_ERR(priv->phy)) {
		dev_err(dev, "Failed to create PHY\n");
		return PTR_ERR(priv->phy);
	}

	phy_set_drvdata(priv->phy, priv);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev, "Failed to register phy provider!\n");
		return PTR_ERR(phy_provider);
	}

	return 0;
}

static const struct of_device_id of_intel_wan_xpcs_phy_match[] = {
	{ .compatible = "intel,wanxpcsphy-prx300" },
	{}
};
MODULE_DEVICE_TABLE(of, of_intel_wan_xpcs_phy_match);

static struct platform_driver intel_wan_xpcs_phy_driver = {
	.probe = intel_wan_xpcs_phy_probe,
	.driver = {
		.name = "intel-wan-xpcs-phy",
		.of_match_table = of_match_ptr(of_intel_wan_xpcs_phy_match),
	}
};

builtin_platform_driver(intel_wan_xpcs_phy_driver);

MODULE_AUTHOR("Peter Harliman Liem <peter.harliman.liem@intel.com>");
MODULE_DESCRIPTION("Intel WAN XPCS PHY driver");
MODULE_LICENSE("GPL v2");
