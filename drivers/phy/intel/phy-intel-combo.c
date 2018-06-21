// SPDX-License-Identifier: GPL-2.0
/*
 * Intel Combo-PHY driver
 *
 * Copyright (C) 2017 Intel Corporation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>

#define CLK_100MHZ	100000000	/* 100MHZ */
#define CLK_156MHZ	156250000	/* 156.25Mhz */
#define CLK_78MHZ	78125000	/* 78.125Mhz */

#define PCIE_PHY_GEN_CTRL		0x00
#define PCIE_PHY_CLK_PAD		17
#define IFMUX_CFG			0x120
#define PCIE_RCLK(x)			(22 + (x))
#define PCIE_SEL(x)			(((x) == 0) ? 20 : 19)
#define PCIE_PHY_MPLLA_CTRL		0x10
#define PCIE_PHY_MPLLB_CTRL		0x14

#define COMBO_PHY_ID(x)	((x)->parent->id)
#define PHY_ID(x)	((x)->id)

static const char * const intel_phy_names[] = {"pcie", "xpcs", "sata"};

static int intel_combo_phy_init(struct phy *phy);
static int intel_combo_phy_exit(struct phy *phy);
static int intel_combo_phy_power_on(struct phy *phy);
static int intel_combo_phy_power_off(struct phy *phy);

enum {
	PHY_0 = 0,
	PHY_1 = 1,
	PHY_MAX_NUM = 2,
};

enum intel_phy_mode {
	PHY_PCIE_MODE = 0,
	PHY_XPCS_MODE,
	PHY_SATA_MODE,
	PHY_MAX_MODE
};

enum combo_phy_mode {
	PCIE0_PCIE1_MODE = 0,
	PCIE_DL_MODE, /* PCIe dual lane */
	RXAUI_MODE,   /* XPCS dual lane */
	XPCS0_XPCS1_MODE,
	XPCS0_PCIE1_MODE,
	PCIE0_XPCS1_MODE,
	SATA0_XPCS1_MODE,
	SATA0_PCIE1_MODE,
	COMBO_PHY_MODE_MAX
};

enum {
	PHY_PCIE_CAP = BIT(PHY_PCIE_MODE),
	PHY_XPCS_CAP = BIT(PHY_XPCS_MODE),
	PHY_SATA_CAP = BIT(PHY_SATA_MODE),
};

enum aggregated_mode {
	PHY_SL_MODE = 1, /* Single Lane mode */
	PHY_DL_MODE,	/* Dual Lane mode */
};

enum intel_phy_role {
	PHY_INDIVIDUAL = 0,	/* Not aggr phy mode */
	PHY_MASTER,		/* DL mode, PHY 0 */
	PHY_SLAVE,		/* DL mode, PHY 1 */
};

struct intel_combo_phy;

struct phy_ctx {
	u32 id; /* Internal PHY idx 0 - 1 */
	bool enable;
	struct intel_combo_phy *parent;
	enum intel_phy_mode phy_mode;
	bool power_en;
	enum intel_phy_role phy_role;
	struct phy *phy;
	void __iomem *cr_base;
	void __iomem *pcie_base;
	struct clk *phy_freq_clk;
	struct clk *phy_gate_clk;
	unsigned long phy_clk_rate[PHY_MAX_MODE];
	struct reset_control *phy_rst;
	struct reset_control *core_rst[PHY_MAX_MODE];
	struct device	*dev;
	struct platform_device *pdev;
	struct device_node *np;
};

struct intel_cbphy_soc_data {
	char *name;
	unsigned long (*get_clk_rate)(enum intel_phy_mode mode);
	u32 (*get_phy_cap)(unsigned int id);
	int (*reset_ctrl)(struct phy_ctx *iphy, enum intel_phy_mode mode);
	int (*phy_cr_cfg)(struct phy_ctx *iphy, enum intel_phy_mode mode);
	void (*combo_phy_mode_set)(struct intel_combo_phy *priv);
};

struct intel_combo_phy {
	u32 id; /* Physical COMBO PHY Index */
	u32 phy_cap; /* phy capability, depends on SoC and PHY ID */
	enum combo_phy_mode cb_phy_mode;
	enum aggregated_mode aggr_mode;
	bool enable[PHY_MAX_NUM];
	enum intel_phy_mode phy_mode[PHY_MAX_NUM];
	struct platform_device *pdev;
	struct device	*dev;
	struct device_node *np;
	struct regmap	*syscfg;
	struct reset_control *phy_global_rst;

	const struct intel_cbphy_soc_data *soc_data;
	struct phy_ctx phy[PHY_MAX_NUM];
};

static const struct phy_ops intel_cbphy_ops = {
	.init = intel_combo_phy_init,
	.exit = intel_combo_phy_exit,
	.power_on = intel_combo_phy_power_on,
	.power_off = intel_combo_phy_power_off,
};

static ssize_t
combo_phy_info_show(struct device *dev,
		    struct device_attribute *attr, char *buf)
{
	struct intel_combo_phy *priv;
	int i, off;

	priv = dev_get_drvdata(dev);

	/* combo phy mode */
	off = sprintf(buf, "mode: %u\n", priv->cb_phy_mode);

	/* aggr mode */
	off += sprintf(buf + off, "aggr mode: %s\n",
		      priv->aggr_mode == PHY_DL_MODE ? "Yes" : "No");

	/* combo phy capability */
	off += sprintf(buf + off, "capability: ");
	for (i = PHY_PCIE_MODE; i < PHY_MAX_MODE; i++) {
		if (BIT(i) & priv->phy_cap)
			off += sprintf(buf + off, "%s ", intel_phy_names[i]);
	}
	off += sprintf(buf + off, "\n");

	/* individual phy mode */
	for (i = 0; i < PHY_MAX_NUM; i++) {
		off += sprintf(buf + off, "PHY%d mode: %s, enable: %s\n",
			       i, intel_phy_names[priv->phy_mode[i]],
			       priv->enable[i] ? "Yes" : "No");
	}

	off += sprintf(buf + off, "SoC: %s\n", priv->soc_data->name);

	return off;
}

static DEVICE_ATTR_RO(combo_phy_info);

static struct attribute *combo_phy_attrs[] = {
	&dev_attr_combo_phy_info.attr,
	NULL,
};

ATTRIBUTE_GROUPS(combo_phy);

/* driver functions */
static inline u32 combo_phy_r32(void __iomem *base, u32 reg)
{
	return readl(base + reg);
}

static inline void combo_phy_w32(void __iomem *base, u32 val, unsigned int reg)
{
	writel(val, base + reg);
}

static inline void combo_phy_w32_off_mask(void __iomem *base, u32 off,
					  u32 mask, u32 set, unsigned int reg)
{
	u32 val;

	val = combo_phy_r32(base, reg) & (~(mask << off));
	val |= (set & mask) << off;
	combo_phy_w32(base, val, reg);
}

static inline void combo_phy_w32_mask(void __iomem *base, u32 clr,
				      u32 set, unsigned int reg)
{
	combo_phy_w32(base, (combo_phy_r32(base, reg) & ~(clr)) | set, reg);
}

static inline void combo_phy_reg_bit_set(void __iomem *base, u32 off,
					 unsigned int reg)
{
	combo_phy_w32_off_mask(base, off, 1, 1, reg);
}

static inline void combo_phy_reg_bit_clr(void __iomem *base, u32 off,
					 unsigned int reg)
{
	combo_phy_w32_off_mask(base, off, 1, 0, reg);
}

static struct phy_ctx *intel_get_combo_slave(struct phy_ctx *iphy)
{
	struct intel_combo_phy *priv = iphy->parent;
	struct phy_ctx *phy_slave;

	phy_slave = &priv->phy[PHY_1];

	WARN_ON_ONCE(phy_slave->phy_role != PHY_SLAVE);
	WARN_ON_ONCE(phy_slave == iphy);

	return phy_slave;
}

static int
intel_cbphy_cfg(struct phy_ctx *iphy, bool aggr,
		int (*phy_cfg)(struct phy_ctx *))
{
	struct phy_ctx *phy_sl;
	int ret;

	ret = phy_cfg(iphy);
	if (ret)
		return ret;

	if (aggr) {
		phy_sl = intel_get_combo_slave(iphy);
		if (!phy_sl)
			return -ENODEV;
		return phy_cfg(phy_sl);
	}

	return 0;
}

static int intel_phy_rst_assert(struct phy_ctx *iphy)
{
	if (iphy->phy_rst)
		if (reset_control_assert(iphy->phy_rst))
			return -EINVAL;

	return 0;
}

static int intel_phy_rst_deassert(struct phy_ctx *iphy)
{
	if (iphy->phy_rst) {
		if (reset_control_deassert(iphy->phy_rst))
			return -EINVAL;
	}
	udelay(1);

	return 0;
}

static int intel_core_rst_assert(struct phy_ctx *iphy)
{
	enum intel_phy_mode phy_mode = iphy->phy_mode;

	if (iphy->core_rst[phy_mode])
		if (reset_control_assert(iphy->core_rst[phy_mode]))
			return -EINVAL;

	udelay(1);

	return 0;
}

static int intel_core_rst_deassert(struct phy_ctx *iphy)
{
	enum intel_phy_mode phy_mode = iphy->phy_mode;

	if (iphy->core_rst[phy_mode]) {
		if (reset_control_deassert(iphy->core_rst[phy_mode]))
			return -EINVAL;
	}

	udelay(2);

	return 0;
}

static int intel_phy_gate_clk_enable(struct phy_ctx *iphy)
{
	if (iphy->phy_gate_clk)
		if (clk_prepare_enable(iphy->phy_gate_clk))
			return -EINVAL;

	return 0;
}

static void intel_phy_gate_clk_disable(struct phy_ctx *iphy)
{
	if (iphy->phy_gate_clk)
		clk_disable_unprepare(iphy->phy_gate_clk);
}

static int intel_phy_freq_clk_enable(struct phy_ctx *iphy)
{
	if (iphy->phy_freq_clk)
		if (clk_prepare_enable(iphy->phy_freq_clk))
			return -EINVAL;

	return 0;
}

static void intel_phy_freq_clk_disable(struct phy_ctx *iphy)
{
	if (iphy->phy_freq_clk)
		clk_disable_unprepare(iphy->phy_freq_clk);
}

static int intel_phy_clk_freq_set(struct phy_ctx *iphy)
{
	enum intel_phy_mode phy_mode = iphy->phy_mode;

	if (iphy->phy_freq_clk && iphy->phy_clk_rate[phy_mode]) {
		if (clk_set_rate(iphy->phy_freq_clk,
				 iphy->phy_clk_rate[phy_mode]))
			return -EINVAL;
	}

	return 0;
}

static int intel_pcie_set_clk_src(struct phy_ctx *iphy)
{
	combo_phy_reg_bit_clr(iphy->pcie_base,
			      PCIE_PHY_CLK_PAD, PCIE_PHY_GEN_CTRL);
	/*
	 * NB, no way to identify gen1/2/3/4 for mppla and mpplb, just delay
	 * for stable plla(gen1/gen2) or pllb(gen3/gen4)
	 */
	usleep_range(50, 100);

	dev_dbg(iphy->dev, "%s PHY%d MPLLA %x MPLLB %x\n",
		__func__, iphy->id,
		combo_phy_r32(iphy->pcie_base, PCIE_PHY_MPLLA_CTRL),
		combo_phy_r32(iphy->pcie_base, PCIE_PHY_MPLLB_CTRL));
	return 0;
}

static int intel_phy_power_on(struct phy_ctx *iphy)
{
	struct device *dev = iphy->dev;
	int ret;

	ret = intel_phy_gate_clk_enable(iphy);
	if (ret) {
		dev_err(dev, "PHY(%u:%u) gate clock enable failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return ret;
	}

	ret = intel_phy_rst_deassert(iphy);
	if (ret) {
		dev_err(dev, "PHY(%u:%u) phy deassert failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return ret;
	}

	ret = intel_phy_freq_clk_enable(iphy);
	if (ret) {
		dev_err(dev, "PHY(%u:%u) freq clock enable failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return ret;
	}

	ret = intel_phy_clk_freq_set(iphy);
	if (ret) {
		dev_err(dev, "PHY(%u:%u) clock frequency set to %lu failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy),
			iphy->phy_clk_rate[iphy->phy_mode]);
		return ret;
	}

	iphy->power_en = true;

	return 0;
}

static int intel_phy_power_off(struct phy_ctx *iphy)
{
	struct device *dev = iphy->dev;
	int ret;

	ret = intel_phy_rst_assert(iphy);
	if (ret) {
		dev_err(dev, "PHY(%u:%u) phy assert failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return ret;
	}

	ret = intel_core_rst_assert(iphy);
	if (ret) {
		dev_err(dev, "PHY(%u:%u) core assert failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return ret;
	}

	intel_phy_gate_clk_disable(iphy);
	intel_phy_freq_clk_disable(iphy);
	iphy->power_en = false;

	return 0;
}

static int intel_phy_cr_config(struct phy_ctx *iphy)
{
	struct intel_combo_phy *priv;
	enum intel_phy_mode phy_mode = iphy->phy_mode;

	priv = iphy->parent;

	if (priv->soc_data->phy_cr_cfg)
		priv->soc_data->phy_cr_cfg(iphy, phy_mode);

	return 0;
}

static int intel_combo_phy_start(struct intel_combo_phy *priv)
{
	struct phy_ctx *iphy;
	int i;

	for (i = 0; i < PHY_MAX_NUM; i++) {
		iphy = &priv->phy[i];

		if (!iphy->enable)
			continue;

		if (intel_phy_rst_deassert(iphy))
			return -EINVAL;
	}

	return 0;
}

static int intel_combo_phy_init(struct phy *phy)
{
	struct phy_ctx *iphy;
	struct device *dev;
	bool aggr;

	iphy = phy_get_drvdata(phy);
	dev = iphy->dev;

	if (iphy->phy_role == PHY_MASTER) {
		aggr = true;
	} else if (iphy->phy_role == PHY_INDIVIDUAL) {
		aggr = false;
	} else {
		dev_err(dev, "PHY(%u:%u) dl mode error: %d\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy), iphy->phy_role);
		return -EINVAL;
	}

	if (intel_cbphy_cfg(iphy, aggr, intel_core_rst_assert))
		return -EINVAL;

	if (intel_cbphy_cfg(iphy, aggr, intel_core_rst_deassert))
		return -EINVAL;

	if (iphy->phy_mode == PHY_PCIE_MODE)
		intel_cbphy_cfg(iphy, aggr, intel_pcie_set_clk_src);

	if (intel_cbphy_cfg(iphy, aggr, intel_phy_power_on))
		return -ENOTSUPP;

	intel_cbphy_cfg(iphy, aggr, intel_phy_cr_config);

	return 0;
}

static int intel_combo_phy_exit(struct phy *phy)
{
	struct phy_ctx *iphy;
	struct device *dev;
	bool aggr;

	iphy = phy_get_drvdata(phy);
	dev = iphy->dev;

	if (iphy->phy_role == PHY_MASTER) {
		aggr = true;
	} else if (iphy->phy_role == PHY_INDIVIDUAL) {
		aggr = false;
	} else {
		dev_err(dev, "PHY(%u:%u) dl mode error: %d\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy), iphy->phy_role);
		return -EINVAL;
	}

	if (iphy->power_en)
		intel_cbphy_cfg(iphy, aggr, intel_phy_power_off);

	return 0;
}

static int intel_combo_phy_power_on(struct phy *phy)
{
	struct phy_ctx *iphy;
	struct device *dev;
	bool aggr;
	int ret;

	iphy = phy_get_drvdata(phy);
	dev = iphy->dev;

	if (iphy->phy_role == PHY_MASTER) {
		aggr = true;
	} else if (iphy->phy_role == PHY_INDIVIDUAL) {
		aggr = false;
	} else {
		dev_err(dev, "PHY(%u:%u) dl mode error: %d\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy), iphy->phy_role);
		return -EINVAL;
	}

	if (!iphy->power_en) {
		ret = intel_cbphy_cfg(iphy, aggr, intel_phy_power_on);
		if (ret)
			return ret;
	}

	return 0;
}

static int intel_combo_phy_power_off(struct phy *phy)
{
	struct phy_ctx *iphy;
	struct device *dev;
	bool aggr;
	int ret;

	iphy = phy_get_drvdata(phy);
	dev = iphy->dev;

	if (iphy->phy_role == PHY_MASTER) {
		aggr = true;
	} else if (iphy->phy_role == PHY_INDIVIDUAL) {
		aggr = false;
	} else {
		dev_err(dev, "PHY(%u:%u) dl mode error: %d\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy), iphy->phy_role);
		return -EINVAL;
	}

	if (iphy->power_en) {
		ret = intel_cbphy_cfg(iphy, aggr, intel_phy_power_off);
		if (ret)
			return ret;
	}

	return 0;
}

static void __iomem *
intel_combo_phy_ioremap(struct phy_ctx *iphy, const char *res_name)
{
	struct device *dev = iphy->dev;
	struct resource *res;
	void __iomem *base;
	struct platform_device *pdev = iphy->pdev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);
	if (!res) {
		dev_err(dev, "Failed to get %s iomem resource!\n", res_name);
		return ERR_PTR(-EINVAL);
	}
	dev_dbg(dev, "%s iomem: %pr !\n", res->name, res);

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(dev, "Failed to ioremap resource: %pr !\n", res);
		return base;
	}

	return base;
}

static int intel_combo_phy_mem_resource(struct phy_ctx *iphy)
{
	void __iomem *base;

	base = intel_combo_phy_ioremap(iphy, "cr");
	if (IS_ERR(base))
		return PTR_ERR(base);
	iphy->cr_base = base;

	base = intel_combo_phy_ioremap(iphy, "pcie");
	if (IS_ERR(base))
		return PTR_ERR(base);
	iphy->pcie_base = base;

	return 0;
}

static int intel_combo_phy_get_clks(struct phy_ctx *iphy)
{
	int i;
	struct intel_combo_phy *priv;
	struct device *dev = iphy->dev;

	priv = iphy->parent;

	iphy->phy_freq_clk = devm_clk_get(dev, "freq");
	if (IS_ERR(iphy->phy_freq_clk)) {
		dev_err(dev, "Failed to get combo phy %u:%u freq clock!\n",
			priv->id, iphy->id);
		return -EINVAL;
	}

	iphy->phy_gate_clk = devm_clk_get(dev, "phy");

	if (IS_ERR(iphy->phy_gate_clk))
		iphy->phy_gate_clk = NULL;
	else
		dev_dbg(dev, "PHY[%u:%u] Get PHY gate clk!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));

	for (i = PHY_PCIE_MODE; i < PHY_MAX_MODE; i++) {
		if (priv->soc_data->get_clk_rate)
			iphy->phy_clk_rate[i]
				= priv->soc_data->get_clk_rate(i);

		if (iphy->phy_clk_rate[i])
			dev_dbg(dev, "PHY[%u:%u]  %s clk rate %lu !\n",
				COMBO_PHY_ID(iphy), PHY_ID(iphy),
				intel_phy_names[i], iphy->phy_clk_rate[i]);
	}

	return 0;
}

static int intel_combo_phy_get_reset(struct phy_ctx *iphy)
{
	int i;
	struct device *dev = iphy->dev;
	char name[32];

	iphy->phy_rst
		= devm_reset_control_get_optional(dev, "phy");
	if (IS_ERR(iphy->phy_rst)) {
		dev_dbg(dev, "PHY[%u:%u] Get PHY reset ctrl Err!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		iphy->phy_rst = NULL;
	}

	for (i = PHY_PCIE_MODE; i < PHY_MAX_MODE; i++) {
		if (iphy->phy_role != PHY_SLAVE)
			continue;

		snprintf(name, sizeof(name) - 1, "%s_core", intel_phy_names[i]);
		iphy->core_rst[i]
			= devm_reset_control_get_optional(dev, name);
		if (IS_ERR(iphy->core_rst[i])) {
			dev_dbg(dev, "PHY[%u:%u] Get %s reset ctrl Err!\n",
				COMBO_PHY_ID(iphy), PHY_ID(iphy), name);
			iphy->core_rst[i] = NULL;
		} else {
			dev_dbg(dev, "PHY[%u:%u] Get %s reset ctrl!\n",
				COMBO_PHY_ID(iphy), PHY_ID(iphy), name);
		}
	}

	return 0;
}

static int intel_phy_dt_parse(struct intel_combo_phy *priv,
			      struct device_node *np, int idx)
{
	struct phy_ctx *iphy = &priv->phy[idx];
	struct device *dev;
	struct platform_device *pdev;
	u32 prop;
	bool aggregated;

	iphy->parent = priv;
	iphy->id = idx;
	iphy->np = np;
	iphy->enable = false;
	iphy->power_en = false;

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_dbg(priv->dev, "Combo-PHY%u: Failed to find PHY device: %d!\n",
			priv->id, idx);
		return 0;
	}
	dev = &pdev->dev;
	iphy->pdev = pdev;
	iphy->dev = dev;
	platform_set_drvdata(pdev, iphy);

	if (!device_property_read_u32(dev, "mode", &prop)) {
		iphy->phy_mode = prop;
		if (iphy->phy_mode >= PHY_MAX_MODE) {
			dev_err(dev, "PHY mode: %u is invalid\n",
				iphy->phy_mode);
			return -EINVAL;
		}
	}

	if (!(BIT(iphy->phy_mode) & priv->phy_cap)) {
		dev_err(dev,
			"PHY mode %u is not supported by COMBO PHY id %u of %s soc platform!\n",
			iphy->phy_mode, priv->id, priv->soc_data->name);
		return -EINVAL;
	}
	priv->phy_mode[idx] = iphy->phy_mode;

	if (iphy->id == 0) {
		/* Dual lane configuration only required on PHY 0 */
		if (!device_property_read_u32(dev, "aggregation", &prop))
			aggregated = !!prop;
		else
			aggregated = false;

		if (aggregated) {
			priv->aggr_mode = PHY_DL_MODE;
			iphy->phy_role = PHY_MASTER;
		} else {
			priv->aggr_mode = PHY_SL_MODE;
			iphy->phy_role = PHY_INDIVIDUAL;
		}
	} else {
		if (priv->aggr_mode == PHY_DL_MODE)
			iphy->phy_role = PHY_SLAVE;
		else
			iphy->phy_role = PHY_INDIVIDUAL;
	}

	if (intel_combo_phy_mem_resource(iphy)) {
		dev_err(dev, "Failed get phy(%u:%u) memory resource!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return -EINVAL;
	}

	if (intel_combo_phy_get_clks(iphy)) {
		dev_err(dev, "Get phy(%u:%u) clks failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return -EINVAL;
	}

	if (intel_combo_phy_get_reset(iphy)) {
		dev_err(dev, "Get phy(%u:%u) resets failed!\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return -EINVAL;
	}

	iphy->enable = of_device_is_available(np);
	priv->enable[idx] = iphy->enable;

	return 0;
}

static int intel_dt_sanity_check(struct intel_combo_phy *priv)
{
	struct phy_ctx *iphy0, *iphy1;

	iphy0 = &priv->phy[PHY_0];
	iphy1 = &priv->phy[PHY_1];

	if (!iphy0->enable && !iphy1->enable)
		return 0;

	if (priv->aggr_mode == PHY_DL_MODE &&
	    (!iphy0->enable || !iphy1->enable)) {
		dev_err(priv->dev, "CFG check fail: Dual lane mode while line offline, line0: %d, line1: %d\n",
			iphy0->enable, iphy1->enable);
		return -EINVAL;
	}

	return 0;
}

static int intel_combo_phy_dt_parse(struct intel_combo_phy *priv)
{
	struct device *dev = priv->dev;
	struct device_node *np = priv->np;
	struct device_node *child;
	int i, ret;

	/* get global reset */ /* Need update to return error in 4.11 */
	priv->phy_global_rst = devm_reset_control_get_optional(dev, "phy");
	if (IS_ERR(priv->phy_global_rst)) {
		dev_dbg(dev, "COMBO PHY %u get global reset err!\n", priv->id);
		priv->phy_global_rst = NULL;
	}

	/* get chiptop regmap */
	priv->syscfg
		= syscon_regmap_lookup_by_phandle(np, "intel,cbphy-syscon");
	if (IS_ERR(priv->syscfg)) {
		dev_err(dev, "No syscon phandle specified for combo-phy syscon\n");
		ret = -EINVAL;
		goto syscfg_err;
	}

	i = 0;
	for_each_child_of_node(np, child) {
		if (i >= PHY_MAX_NUM) {
			dev_err(dev, "Error: DT child number larger than %d\n",
				PHY_MAX_NUM);
			ret = -EINVAL;
			goto phy_dt_err;
		}
		ret = intel_phy_dt_parse(priv, child, i);
		if (ret) {
			of_node_put(child);
			goto phy_dt_err;
		}
		i++;
	}

	ret = intel_dt_sanity_check(priv);
	if (ret)
		goto phy_dt_err;

	return 0;

phy_dt_err:
	/* Set other phys in disabled status */
	for (i = i - 1; i >= 0; i--)
		priv->enable[i] = false;
syscfg_err:
	return ret;
}

static void intel_combo_phy_reset(struct intel_combo_phy *priv)
{
	if (priv->phy_global_rst) {
		reset_control_assert(priv->phy_global_rst);
		udelay(1);
		reset_control_deassert(priv->phy_global_rst);
	}

	usleep_range(10, 20);
}

static int intel_combo_phy_set_mode(struct intel_combo_phy *priv)
{
	priv->cb_phy_mode = COMBO_PHY_MODE_MAX;
	/* aggregation mode */
	if (priv->aggr_mode == PHY_DL_MODE) {
		if (!priv->enable[PHY_0] || !priv->enable[PHY_1])
			return -EINVAL;
		if (priv->phy_mode[PHY_0] != priv->phy_mode[PHY_1])
			return -EINVAL;
		if (priv->phy_mode[PHY_0] == PHY_PCIE_MODE)
			priv->cb_phy_mode = PCIE_DL_MODE;
		else if (priv->phy_mode[PHY_0] == PHY_XPCS_MODE)
			priv->cb_phy_mode = RXAUI_MODE;
	} else if (priv->aggr_mode == PHY_SL_MODE) {
		if (priv->phy_mode[PHY_0] == PHY_PCIE_MODE &&
		    priv->phy_mode[PHY_1] == PHY_PCIE_MODE)
			priv->cb_phy_mode = PCIE0_PCIE1_MODE;

		else if (priv->phy_mode[PHY_0] == PHY_XPCS_MODE &&
			 priv->phy_mode[PHY_1] == PHY_XPCS_MODE)
			priv->cb_phy_mode = XPCS0_XPCS1_MODE;

		else if (priv->phy_mode[PHY_0] == PHY_XPCS_MODE &&
			 priv->phy_mode[PHY_1] == PHY_PCIE_MODE)
			priv->cb_phy_mode = XPCS0_PCIE1_MODE;

		else if (priv->phy_mode[PHY_0] == PHY_PCIE_MODE &&
			 priv->phy_mode[PHY_1] == PHY_XPCS_MODE)
			priv->cb_phy_mode = PCIE0_XPCS1_MODE;

		else if (priv->phy_mode[PHY_0] == PHY_SATA_MODE &&
			 priv->phy_mode[PHY_1] == PHY_XPCS_MODE)
			priv->cb_phy_mode = SATA0_XPCS1_MODE;

		else if (priv->phy_mode[PHY_0] == PHY_SATA_MODE &&
			 priv->phy_mode[PHY_1] == PHY_PCIE_MODE)
			priv->cb_phy_mode = SATA0_PCIE1_MODE;
	}

	if (priv->cb_phy_mode == COMBO_PHY_MODE_MAX)
		return -EINVAL;

	if (priv->soc_data->combo_phy_mode_set)
		priv->soc_data->combo_phy_mode_set(priv);

	return 0;
}

static int intel_phy_create(struct phy_ctx *iphy)
{
	struct device *dev = iphy->dev;
	struct phy_provider *phy_provider;

	if (!iphy->enable) {
		dev_err(dev, "%s[%u:%u]: Combo PHY not enabled!\n",
			__func__, COMBO_PHY_ID(iphy), PHY_ID(iphy));
		return -ENODEV;
	}

	iphy->phy = devm_phy_create(dev, iphy->np, &intel_cbphy_ops);
	if (IS_ERR(iphy->phy)) {
		dev_err(dev,
			"%s: Failed to create intel combo-phy!\n", __func__);
		return PTR_ERR(iphy->phy);
	}

	phy_set_drvdata(iphy->phy, iphy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev,
			"%s: Failed to register phy provider!\n", __func__);
		return PTR_ERR(phy_provider);
	}

	return 0;
}

static int intel_combo_phy_create(struct intel_combo_phy *priv)
{
	int i;
	struct phy_ctx *iphy;

	if (priv->aggr_mode == PHY_DL_MODE) {/* Only PHY0 is visible */
		iphy = &priv->phy[PHY_0];
		if (intel_phy_create(iphy))
			return -ENODEV;
	}

	for (i = 0; i < PHY_MAX_NUM; i++) {
		if (priv->enable[i]) {
			iphy = &priv->phy[i];
			if (intel_phy_create(iphy))
				return -ENODEV;
			if (intel_phy_clk_freq_set(iphy)) {
				dev_err(iphy->dev, "PHY(%u:%u) clock frequency set to %lu failed!\n",
					COMBO_PHY_ID(iphy), PHY_ID(iphy),
					iphy->phy_clk_rate[iphy->phy_mode]);
			} else {
				dev_err(iphy->dev, "PHY(%u:%u) clock frequency set to %lu Success!\n",
					COMBO_PHY_ID(iphy), PHY_ID(iphy),
					iphy->phy_clk_rate[iphy->phy_mode]);
			}
		}
	}

	return 0;
}

static int intel_combo_phy_sysfs_init(struct intel_combo_phy *priv)
{
	/* return devm_device_add_groups(priv->dev, combo_phy_groups); */
	return sysfs_create_groups(&priv->dev->kobj, combo_phy_groups);
}

static int intel_combo_phy_probe(struct platform_device *pdev)
{
	int id;
	unsigned int cap;
	struct device *dev = &pdev->dev;
	struct intel_combo_phy *priv;
	struct device_node *np = dev->of_node;

	if (!dev->of_node)
		return -ENODEV;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->soc_data = of_device_get_match_data(dev);
	if (WARN_ON(!priv->soc_data))
		return -EINVAL;

	id = of_alias_get_id(np, "combophy");
	if (id < 0) {
		dev_err(dev, "failed to get alias id, errno %d\n", id);
		return -EINVAL;
	}

	cap = priv->soc_data->get_phy_cap(id);
	if (!cap) {
		dev_err(dev, "PHY id: %d is invalid\n", id);
		return -EINVAL;
	}

	priv->id = id;
	priv->phy_cap = cap;
	priv->pdev = pdev;
	priv->dev = dev;
	priv->np = np;
	platform_set_drvdata(pdev, priv);

	if (intel_combo_phy_dt_parse(priv)) {
		dev_err(dev, "dt parse failed!\n");
		return -EINVAL;
	}

	intel_combo_phy_reset(priv);
	if (intel_combo_phy_set_mode(priv)) {
		dev_err(dev, "combo phy mode failed!\n");
		return -EINVAL;
	}

	intel_combo_phy_create(priv);
	intel_combo_phy_start(priv);

	if (intel_combo_phy_sysfs_init(priv))
		return -EINVAL;

	return 0;
}

/* TwinHill platform data */
static unsigned long twh_get_clk_rate(enum intel_phy_mode mode)
{
	if (mode == PHY_PCIE_MODE)
		return CLK_100MHZ;
	else
		return 0;  /* Other mode No support */
}

static u32 twh_get_phy_cap(unsigned int id)
{
	const unsigned int phy_num = 1;

	if (id < phy_num)
		return PHY_PCIE_CAP;
	else
		return 0;
}

static int twh_phy_cr_cfg(struct phy_ctx *iphy, enum intel_phy_mode mode)
{
#define PCIE_PHY_VR_CNT		5000
	int i;
	u32 val;
	void __iomem *base;

	if (mode != PHY_PCIE_MODE)
		return 0;

	base = iphy->pcie_base;
	WARN_ON(!base);

	for (i = 0; i < PCIE_PHY_VR_CNT; i++) {
		if ((combo_phy_r32(base, PCIE_PHY_MPLLA_CTRL) == 0xC0000000) &&
		    (combo_phy_r32(base, PCIE_PHY_MPLLB_CTRL) == 0xC0000000))
			break;
		usleep_range(10, 20);
	}

	if (i >= PCIE_PHY_VR_CNT) {
		dev_err(iphy->dev, "%s PHY%d MPLL locked failed\n",
			__func__, iphy->id);
		return -ETIMEDOUT;
	}

	dev_dbg(iphy->dev, "%s PHY%d MPLL locked succeed\n",
		__func__, iphy->id);

	/* PHY CR setting */
	base = iphy->cr_base;
	WARN_ON(!base);

	for (i = 0; i < PCIE_PHY_VR_CNT; i++) {
		combo_phy_w32_mask(base, BIT(2), BIT(3) | BIT(4), (0x0a << 2));
		val = combo_phy_r32(base, (0x0a << 2));
		if (((val & 0x18) == 0x18) &&
		    !!(combo_phy_r32(base, (0x3059 << 2)) & BIT(0)))
			break;
		usleep_range(10, 20);
	}

	if (i >= PCIE_PHY_VR_CNT) {
		dev_err(iphy->dev, "SUP_DIG_SUP_OVRD_IN 0x%08x\n",
			combo_phy_r32(base, (0x0a << 2)));
		dev_err(iphy->dev,
			"RAWLANEN_DIG_AON_INIT_PWRUP_DOWN PWR_DONE %d\n",
			!!((combo_phy_r32(base, (0x3059 << 2))) & BIT(0)));
	}

	for (i = 0; i < PCIE_PHY_VR_CNT; i++) {
		if (!!(combo_phy_r32(base, (0x3032 << 2)) & BIT(1)))
			break;
		usleep_range(10, 20);
	}

	if (i >= PCIE_PHY_VR_CNT)
		dev_err(iphy->dev, "PCIe PHY[%u:%u] calibration failed\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
	else
		dev_dbg(iphy->dev, "PCIe PHY[%u:%u] calibrtion succeed\n",
			COMBO_PHY_ID(iphy), PHY_ID(iphy));
	return 0;
}

static void twh_combo_phy_mode_set(struct intel_combo_phy *priv)
{
	const int cb_mode_reg_off = 0x480;
	const int cb_mode_bit_off = 0;
	const int cb_mode_bit_mask = 0x7;

	regmap_update_bits(priv->syscfg, cb_mode_reg_off,
			   cb_mode_bit_mask,
			   priv->cb_phy_mode << cb_mode_bit_off);
}

/* Falconmx platform data */
static unsigned long falconmx_get_clk_rate(enum intel_phy_mode mode)
{
	if (mode == PHY_PCIE_MODE) {
		return CLK_100MHZ;
	} else if (mode == PHY_XPCS_MODE) {
		/* 156.25Mhz */
		return CLK_156MHZ;
	}

	return 0;  /* Other mode No support */
}

static u32 falconmx_get_phy_cap(unsigned int id)
{
	const unsigned int phy_num = 1;

	if (id < phy_num)
		return PHY_PCIE_CAP | PHY_XPCS_CAP;
	else
		return 0;
}

static void falconmx_combo_phy_mode_set(struct intel_combo_phy *priv)
{
	const int reg_off = 0x120;
	const int bit_off = 2;
	const int bit_mask = 0x1C;
	int i;
	struct phy_ctx *iphy;

	regmap_update_bits(priv->syscfg, reg_off, bit_mask,
			   priv->cb_phy_mode << bit_off);

	for (i = 0; i < PHY_MAX_NUM; i++) {
		iphy = &priv->phy[i];
		if (iphy->enable && iphy->phy_mode == PHY_XPCS_MODE)
			regmap_update_bits(priv->syscfg, reg_off, 2, 2);
		else if (iphy->enable && iphy->phy_mode == PHY_PCIE_MODE)
			regmap_update_bits(priv->syscfg, reg_off,
					   BIT(PCIE_RCLK(i)), 0);
	}
}

/* Lighting Mountain platform data */
static unsigned long lgm_get_clk_rate(enum intel_phy_mode mode)
{
	switch (mode) {
	case PHY_PCIE_MODE:
	case PHY_SATA_MODE:
		return CLK_100MHZ;
	case PHY_XPCS_MODE:
		return CLK_78MHZ; /* 78.125Mhz */
	case PHY_MAX_MODE:
	default:
		return 0;
	}

	return 0;
}

static u32 lgm_get_phy_cap(unsigned int id)
{
	const unsigned int phy_num = 4;
	const unsigned int mtf_phy_num = 2;

	if (id < mtf_phy_num)
		return PHY_PCIE_CAP | PHY_XPCS_CAP | PHY_SATA_CAP;
	else if (id < phy_num)
		return PHY_PCIE_CAP;
	else
		return 0;
}

static const struct intel_cbphy_soc_data twh_phy_data = {
	.name = "TwinHill",
	.get_clk_rate = twh_get_clk_rate,
	.get_phy_cap = twh_get_phy_cap,
	.phy_cr_cfg = twh_phy_cr_cfg,
	.combo_phy_mode_set = twh_combo_phy_mode_set,
};

static const struct intel_cbphy_soc_data falconmx_phy_data = {
	.name = "Falcon MX",
	.get_clk_rate = falconmx_get_clk_rate,
	.get_phy_cap = falconmx_get_phy_cap,
	.combo_phy_mode_set = falconmx_combo_phy_mode_set,
};

static const struct intel_cbphy_soc_data lgm_phy_data = {
	.name = "Lighting Mountain",
	.get_clk_rate = lgm_get_clk_rate,
	.get_phy_cap = lgm_get_phy_cap,
};

static const struct of_device_id of_intel_combo_phy_match[] = {
	{ .compatible = "intel,combophy-twh", .data = &twh_phy_data },
	{ .compatible = "intel,combophy-falconmx", .data = &falconmx_phy_data },
	{ .compatible = "intel,combophy-lgm", .data = &lgm_phy_data },
	{}
};
MODULE_DEVICE_TABLE(of, of_intel_combo_phy_match);

static struct platform_driver intel_combo_phy_driver = {
	.probe = intel_combo_phy_probe,
	.driver = {
		.name = "intel-combo-phy",
		.of_match_table = of_match_ptr(of_intel_combo_phy_match),
	}
};

builtin_platform_driver(intel_combo_phy_driver);
