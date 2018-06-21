/*
 * Copyright (c) 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <lantiq_soc.h>

#define VERSIONS_COUNT 1

/* Supported XRX500 family SoC HW versions */
#define SOC_HW_ID_GRX5838	0x1
#define SOC_HW_ID_GRX5828	0x2
#define SOC_HW_ID_GRX5628	0x4
#define SOC_HW_ID_GRX5821	0x8
#define SOC_HW_ID_GRX5831	0x10
#define SOC_HW_ID_GRX58312	0x20
#define SOC_HW_ID_GRX3506	0x40
#define SOC_HW_ID_GRX3508	0x80
#define SOC_HW_ID_IRX200	0x100

static unsigned int xrx500_pnum2version(struct device *dev, unsigned int id)
{
	switch (id) {
	case SOC_ID_GRX5838:
		return SOC_HW_ID_GRX5838;
	case SOC_ID_GRX5828:
		return SOC_HW_ID_GRX5828;
	case SOC_ID_GRX5628:
		return SOC_HW_ID_GRX5628;
	case SOC_ID_GRX5821:
		return SOC_HW_ID_GRX5821;
	case SOC_ID_GRX5831:
		return SOC_HW_ID_GRX5831;
	case SOC_ID_GRX58312:
		return SOC_HW_ID_GRX58312;
	case SOC_ID_GRX3506:
		return SOC_HW_ID_GRX3506;
	case SOC_ID_GRX3508:
		return SOC_HW_ID_GRX3508;
	case SOC_ID_IRX200:
		return SOC_HW_ID_IRX200;
	default:
		return -EINVAL;
	}
}


int xrx500_opp_set_supported_hw(struct device *cpu_dev)
{
	unsigned int version, id;
	int ret;

	id = ltq_get_cpu_id();

	version = xrx500_pnum2version(cpu_dev, id);
	if (version < 0) {
		dev_err(cpu_dev, "unknown xrx500 chip id (0x%x)\n", id);
		return ret;
	}

	ret = dev_pm_opp_set_supported_hw(cpu_dev, &version, VERSIONS_COUNT);
	if (ret) {
		dev_err(cpu_dev, "Failed to set supported hardware\n");
		return ret;
	}

	return ret;
}

static int xrx500_cpufreq_driver_init(void)
{
	struct platform_device_info devinfo = { .name = "cpufreq-dt", };
	struct device *cpu_dev;
	int ret;

	if ((!of_machine_is_compatible("lantiq,xrx500")))
		return -ENODEV;

	cpu_dev = get_cpu_device(0);
	if (!cpu_dev) {
		pr_err("failed to get cpu device\n");
		return -ENODEV;
	}

	ret = xrx500_opp_set_supported_hw(cpu_dev);
	if (ret)
		return ret;

	/* Instantiate cpufreq-dt */
	platform_device_register_full(&devinfo);

	return ret;

}
module_init(xrx500_cpufreq_driver_init);

MODULE_DESCRIPTION("XRX500 Family SoC CPUFreq driver");
MODULE_AUTHOR("Waldemar Rymarkiewicz <waldemarx.rymarkiewicz@intel.com>");
MODULE_LICENSE("GPL v2");
