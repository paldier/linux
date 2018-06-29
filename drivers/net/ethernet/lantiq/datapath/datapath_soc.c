/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
#include<linux/init.h>
#include<linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/if_ether.h>
#include <linux/ethtool.h>
#include <lantiq_soc.h>
#include <net/datapath_api.h>
#include "datapath.h"
#include "datapath_instance.h"
#include "datapath_swdev_api.h"

#ifdef CONFIG_FALCONMX_CQM
#define LTQ_DATAPATH_SOC_FALCON_MX
#endif

int request_dp(u32 flag)
{
	struct dp_inst_info info;

#if IS_ENABLED(CONFIG_FALCONMX_CQM) || \
	IS_ENABLED(CONFIG_LTQ_DATAPATH_DDR_SIMULATE_GSWIP31) /*testing only */
	info.type = GSWIP31_TYPE;
	info.ver = GSWIP31_VER;
	info.ops[0] = gsw_get_swcore_ops(0);
	info.ops[1] = gsw_get_swcore_ops(0);
	info.mac_ops[0] = NULL;
	info.mac_ops[1] = NULL;
	info.mac_ops[2] = gsw_get_mac_ops(0, 0);
	info.mac_ops[3] = gsw_get_mac_ops(0, 1);
	info.mac_ops[4] = gsw_get_mac_ops(0, 2);
#else
	info.type = GSWIP30_TYPE;
	info.ver = GSWIP30_VER;
	info.ops[0] = gsw_get_swcore_ops(0);
	info.ops[1] = gsw_get_swcore_ops(1);
#endif
	info.cbm_inst = 0;
	info.qos_inst = 0;
	if (dp_request_inst(&info, flag)) {
		PR_ERR("dp_request_inst failed\n");
		return -1;
	}
	return 0;
}

int register_dp_cap(u32 flag)
{
#ifdef CONFIG_LTQ_DATAPATH_HAL_GSWIP31
	register_dp_cap_gswip31(0);
#endif

#ifdef CONFIG_LTQ_DATAPATH_HAL_GSWIP30
	register_dp_cap_gswip30(0);
#endif
	return 0;
}
