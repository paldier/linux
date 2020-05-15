// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Intel Corporation.
 * Lei Chuanhua <Chuanhua.lei@intel.com>
 */

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

/* Reset request register */
#define RCU_RST_REQ		0x0010
/* Reset status register */
#define RCU_RST_STAT		0x0014

#define RCU_RST_STAT2		0x0024
#define RCU_RST_REQ2		0x0048

/* Global software reboot */
#define RCU_RD_SRST		30

/* Status */
static const u32 rcu_stat[] = {
	RCU_RST_STAT,
	RCU_RST_STAT2,
};

/* Request */
static const u32 rcu_req[] = {
	RCU_RST_REQ,
	RCU_RST_REQ2,
};

#define RCU_STAT_REG(x)		(rcu_stat[(x)])
#define RCU_REQ_REG(x)		(rcu_req[(x)])

#define to_reset_data(x) container_of(x, struct intel_reset_data, rcdev)

struct intel_reset_data {
	struct reset_controller_dev rcdev;
	struct notifier_block restart_nb;
	struct regmap *regmap;
};

static int intel_assert_device(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct intel_reset_data *data = to_reset_data(rcdev);
	u32 regidx = id >> 5;
	u32 regbit = id & 0x1f;

	return regmap_update_bits(data->regmap, RCU_REQ_REG(regidx),
				  BIT(regbit), BIT(regbit));
}

static int intel_deassert_device(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	struct intel_reset_data *data = to_reset_data(rcdev);
	u32 regidx = id >> 5;
	u32 regbit = id & 0x1f;

	return regmap_update_bits(data->regmap, RCU_REQ_REG(regidx),
				  BIT(regbit), 0 << regbit);
}

static int intel_reset_device(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct intel_reset_data *data = to_reset_data(rcdev);
	u32 regidx = id >> 5;
	u32 regbit = id & 0x1F;
	unsigned int val = 0;
	int ret;

	ret = regmap_write(data->regmap, RCU_REQ_REG(regidx), BIT(regbit));
	if (ret)
		return ret;

	return regmap_read_poll_timeout(data->regmap, RCU_STAT_REG(regidx), val,
					val & BIT(regbit), 20, 20000);
}

static int intel_reset_status(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct intel_reset_data *data = to_reset_data(rcdev);
	u32 regidx = id >> 5;
	u32 regbit = id & 0x1F;
	unsigned int val;
	int ret;

	ret = regmap_read(data->regmap, RCU_STAT_REG(regidx), &val);
	if (ret)
		return ret;

	return !!(val & BIT(regbit));
}

static const struct reset_control_ops intel_reset_ops = {
	.reset = intel_reset_device,
	.assert = intel_assert_device,
	.deassert = intel_deassert_device,
	.status = intel_reset_status,
};

static int intel_reset_restart_handler(struct notifier_block *nb,
				       unsigned long action, void *data)
{
	struct intel_reset_data *reset_data =
		container_of(nb, struct intel_reset_data, restart_nb);

	intel_assert_device(&reset_data->rcdev, RCU_RD_SRST);

	return NOTIFY_DONE;
}

static int intel_reset_probe(struct platform_device *pdev)
{
	int err;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct intel_reset_data *data;
	struct regmap *regmap;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	regmap = syscon_node_to_regmap(np);
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to get reset controller regmap\n");
		return PTR_ERR(regmap);
	}

	data->regmap = regmap;
	data->rcdev.of_node = np;
	data->rcdev.owner = dev->driver->owner;
	data->rcdev.ops = &intel_reset_ops;
	data->rcdev.nr_resets = 64;

	err = devm_reset_controller_register(&pdev->dev, &data->rcdev);
	if (err)
		return err;

	data->restart_nb.notifier_call = intel_reset_restart_handler;
	data->restart_nb.priority = 128;

	err = register_restart_handler(&data->restart_nb);
	if (err)
		dev_warn(&pdev->dev, "Failed to register restart handler\n");

	return 0;
}

/* If some SoCs have different property, customized in data field */
static const struct of_device_id intel_reset_match[] = {
	{
		.compatible = "lantiq,rcu-xway",
	},
	{
		.compatible = "lantiq,rcu-grx500",
	},
	{
		.compatible = "intel,rcu-lgh",
	},
	{
		.compatible = "intel,rcu-lgm",
	},
	{ /* sentinel */ },
};

static struct platform_driver intel_reset_driver = {
	.probe = intel_reset_probe,
	.driver = {
		.name = "intel-reset-syscon",
		.of_match_table = of_match_ptr(intel_reset_match),
	},
};

static int __init intel_reset_init(void)
{
	return platform_driver_register(&intel_reset_driver);
}
postcore_initcall(intel_reset_init);
