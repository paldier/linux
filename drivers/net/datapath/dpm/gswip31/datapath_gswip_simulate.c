/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/etherdevice.h>
#include <net/datapath_api.h>
#include "../datapath.h"
#include "datapath_misc.h"

#define MAX_CPT_PORT 288
u8 ctp_assign_f[MAX_CPT_PORT] = {0};
GSW_CTP_portAssignment_t ctp_assign[MAX_CPT_PORT] = {0};

#define MAX_PMAPPER 2336
u8 pmapper_f[MAX_PMAPPER] = {0};

u32  Pmapper_Alloc(int size)
{
	u32 i, j;

	for (i = 1; i < MAX_PMAPPER; i++) {
		if (pmapper_f[i])
			continue;
		for (j = 0; j < size; j++)
			if (pmapper_f[i + j])
				continue;

		for (j = 0; j < size; j++)
			pmapper_f[i] = 1;
		return i;
	}
	return -1;
}

int Pmapper_Free(u32 offset, int size)
{
	int i;

	for (i = 0; i < size; i++)
		pmapper_f[i + offset] = 0;
	return -1;
}

int CTP_PortAssignmentAlloc(GSW_CTP_portAssignment_t *param)
{
	int i, j;

	for (i = 1; i < MAX_CPT_PORT; i++) {
		if (ctp_assign_f[i])
			continue;
		for (j = 0; j < param->nNumberOfCtpPort; j++)
			if (ctp_assign_f[i + j])
				continue;
		param->nFirstCtpPortId = i;
		for (j = 0; j < param->nNumberOfCtpPort; j++) {
			ctp_assign_f[i + j] = 1;
			ctp_assign[i + j].eMode = param->eMode;
			ctp_assign[i + j].nBridgePortId =
				param->nBridgePortId;
			ctp_assign[i + j].nFirstCtpPortId =
				param->nFirstCtpPortId;
			ctp_assign[i + j].nLogicalPortId =
				param->nLogicalPortId;
			ctp_assign[i + j].nNumberOfCtpPort =
				param->nNumberOfCtpPort;
		}
		return 0;
	}
	return -1;
}

int CTP_PortAssignmentFree(GSW_CTP_portAssignment_t *param)
{
	int i;

	for (i = 0; i < param->nNumberOfCtpPort; i++)
		ctp_assign_f[i + param->nNumberOfCtpPort] = 0;
	return 0;
}

int CTP_PortAssignmentSet(GSW_CTP_portAssignment_t *param)
{
	u32 idx = param->nFirstCtpPortId;
	int i;

	for (i = 0; i < param->nNumberOfCtpPort; i++) {
		ctp_assign[idx + i].eMode = param->eMode;
		ctp_assign[idx + i].nBridgePortId = param->nBridgePortId;
		ctp_assign[idx + i].nFirstCtpPortId = param->nFirstCtpPortId;
		ctp_assign[idx + i].nLogicalPortId = param->nLogicalPortId;
		ctp_assign[idx + i].nNumberOfCtpPort = param->nNumberOfCtpPort;
	}
	return 0;
}

int CTP_PortAssignmentGet(GSW_CTP_portAssignment_t *param)
{
	int i;

	for (i = 0; i < MAX_CPT_PORT; i++) {
		if (!ctp_assign_f[i])
			continue;
		if (ctp_assign[i].nLogicalPortId != param->nLogicalPortId)
			continue;

		param->eMode = ctp_assign[i].eMode;
		param->nBridgePortId = ctp_assign[i].nBridgePortId;
		param->nFirstCtpPortId = ctp_assign[i].nFirstCtpPortId;
		param->nNumberOfCtpPort = ctp_assign[i].nNumberOfCtpPort;
		return 0;
	}
	return -1;
}

GSW_CTP_portConfig_t ctp_port_cfg[MAX_CPT_PORT] = {0};
int CtpPortConfigSet(GSW_CTP_portConfig_t *param)
{
	GSW_CTP_portAssignment_t ctp_get;
	int ret;
	u32 ctp_port;

	ctp_get.nLogicalPortId = param->nLogicalPortId;
	ret = CTP_PortAssignmentGet(&ctp_get);
	if (ret) {
		PR_ERR("CTP_PortAssignmentGet fail for bp=%d\n",
		       ctp_get.nLogicalPortId);
		return GSW_statusErr;
	}
	ctp_port = ctp_get.nFirstCtpPortId  + param->nSubIfIdGroup;
	if ((ctp_port >= MAX_CPT_PORT) ||
	    (ctp_port < ctp_get.nFirstCtpPortId)) {
		PR_ERR("CtpPortConfigSet wrong ctp_port %d (%d ~ %d)\n",
		       ctp_port, ctp_get.nFirstCtpPortId, MAX_CPT_PORT);
		return GSW_statusErr;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_BRIDGE_PORT_ID)
		ctp_port_cfg[ctp_port].nBridgePortId = param->nBridgePortId;

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_FORCE_TRAFFIC_CLASS) {
		ctp_port_cfg[ctp_port].nDefaultTrafficClass =
			param->nDefaultTrafficClass;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_INGRESS_VLAN) {
		ctp_port_cfg[ctp_port].bIngressExtendedVlanEnable =
			param->bIngressExtendedVlanEnable;
		ctp_port_cfg[ctp_port].nIngressExtendedVlanBlockId =
			param->nIngressExtendedVlanBlockId;
	}
	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_INGRESS_VLAN_IGMP) {
		ctp_port_cfg[ctp_port].bIngressExtendedVlanIgmpEnable = param->
			bIngressExtendedVlanIgmpEnable;
		ctp_port_cfg[ctp_port].nIngressExtendedVlanBlockIdIgmp = param->
			nIngressExtendedVlanBlockIdIgmp;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_EGRESS_VLAN) {
		ctp_port_cfg[ctp_port].bEgressExtendedVlanEnable = param->
			bEgressExtendedVlanEnable;
		ctp_port_cfg[ctp_port].nEgressExtendedVlanBlockId = param->
			nEgressExtendedVlanBlockId;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_EGRESS_VLAN_IGMP) {
		ctp_port_cfg[ctp_port].bEgressExtendedVlanIgmpEnable = param->
			bEgressExtendedVlanIgmpEnable;
		ctp_port_cfg[ctp_port].nEgressExtendedVlanBlockIdIgmp = param->
			nEgressExtendedVlanBlockIdIgmp;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_INRESS_NTO1_VLAN)
		ctp_port_cfg[ctp_port].bIngressNto1VlanEnable = param->
			bIngressNto1VlanEnable;

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_EGRESS_NTO1_VLAN)
		ctp_port_cfg[ctp_port].eMask = param->eMask;

	if (param->eMask & GSW_CTP_PORT_CONFIG_INGRESS_METER) {
		ctp_port_cfg[ctp_port].bIngressMeteringEnable = param->
			bIngressMeteringEnable;
		ctp_port_cfg[ctp_port].nIngressTrafficMeterId = param->
			nIngressTrafficMeterId;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_METER) {
		ctp_port_cfg[ctp_port].bEgressMeteringEnable =
			param->bEgressMeteringEnable;
		ctp_port_cfg[ctp_port].nEgressTrafficMeterId =
			param->nEgressTrafficMeterId;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_BRIDGING_BYPASS) {
		ctp_port_cfg[ctp_port].bBridgingBypass =
			param->bBridgingBypass;
		ctp_port_cfg[ctp_port].nDestLogicalPortId =
			param->nDestLogicalPortId;
		ctp_port_cfg[ctp_port].ePmapperMappingMode =
			param->ePmapperMappingMode;
		ctp_port_cfg[ctp_port].nDestSubIfIdGroup =
			param->nDestSubIfIdGroup;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_INGRESS_MARKING) {
		ctp_port_cfg[ctp_port].eIngressMarkingMode =
			param->eIngressMarkingMode;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_MARKING) {
		ctp_port_cfg[ctp_port].eEgressMarkingMode =
			param->eEgressMarkingMode;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_REMARKING) {
		ctp_port_cfg[ctp_port].eEgressRemarkingMode =
			param->eEgressRemarkingMode;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_MARKING_OVERRIDE) {
		ctp_port_cfg[ctp_port].bEgressMarkingOverrideEnable = param->
			bEgressMarkingOverrideEnable;
		ctp_port_cfg[ctp_port].eEgressMarkingModeOverride = param->
			eEgressMarkingModeOverride;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_FLOW_ENTRY) {
		ctp_port_cfg[ctp_port].nFirstFlowEntryIndex =
			param->nFirstFlowEntryIndex;
		ctp_port_cfg[ctp_port].nNumberOfFlowEntries =
			param->nNumberOfFlowEntries;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_LOOPBACK_AND_MIRROR) {
		ctp_port_cfg[ctp_port].bIngressLoopbackEnable = param->
			bIngressLoopbackEnable;
		ctp_port_cfg[ctp_port].bEgressLoopbackEnable =
			param->bEgressLoopbackEnable;
		ctp_port_cfg[ctp_port].bIngressMirrorEnable =
			param->bIngressMirrorEnable;
		ctp_port_cfg[ctp_port].bEgressMirrorEnable =
			param->bEgressMirrorEnable;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_LOOPBACK_AND_MIRROR) {
		ctp_port_cfg[ctp_port].bIngressDaSaSwapEnable = param->
			bIngressDaSaSwapEnable;
		ctp_port_cfg[ctp_port].bEgressDaSaSwapEnable =
			param->bEgressDaSaSwapEnable;
	}

	return 0;
}

int CtpPortConfigGet(GSW_CTP_portConfig_t *param)
{
	GSW_CTP_portAssignment_t ctp_get;
	int ret;
	u32 ctp_port;

	ctp_get.nLogicalPortId = param->nLogicalPortId;
	ret = CTP_PortAssignmentGet(&ctp_get);
	if (ret) {
		PR_ERR("CtpPortConfigGet returns ERROR\n");
		return GSW_statusErr;
	}
	ctp_port = ctp_get.nFirstCtpPortId  + param->nSubIfIdGroup;
	if ((ctp_port >= MAX_CPT_PORT) ||
	    (ctp_port < ctp_get.nFirstCtpPortId)) {
		PR_ERR("CtpPortConfigGet wrong ctp_port %d (%d ~ %d)\n",
		       ctp_port, ctp_get.nFirstCtpPortId, MAX_CPT_PORT);
		return GSW_statusErr;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_BRIDGE_PORT_ID)
		param->nBridgePortId = ctp_port_cfg[ctp_port].nBridgePortId;

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_FORCE_TRAFFIC_CLASS) {
		param->nDefaultTrafficClass =
			ctp_port_cfg[ctp_port].nDefaultTrafficClass;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_INGRESS_VLAN) {
		param->bIngressExtendedVlanEnable = ctp_port_cfg[ctp_port].
			bIngressExtendedVlanEnable;
		param->nIngressExtendedVlanBlockId = ctp_port_cfg[ctp_port].
			nIngressExtendedVlanBlockId;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_INGRESS_VLAN_IGMP) {
		param->bIngressExtendedVlanIgmpEnable = ctp_port_cfg[ctp_port].
			bIngressExtendedVlanIgmpEnable;
		param->nIngressExtendedVlanBlockIdIgmp = ctp_port_cfg[ctp_port].
			nIngressExtendedVlanBlockIdIgmp;
		/*param->nIngressExtendedVlanBlockSizeIgmp =
		 *ctp_port_cfg[ctp_port].nIngressExtendedVlanBlockSizeIgmp;
		 */
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_EGRESS_VLAN) {
		param->bEgressExtendedVlanEnable = ctp_port_cfg[ctp_port].
			bEgressExtendedVlanEnable;
		param->nEgressExtendedVlanBlockId = ctp_port_cfg[ctp_port].
			nEgressExtendedVlanBlockId;
		/*param->nEgressExtendedVlanBlockSize = ctp_port_cfg[ctp_port].
		 *nEgressExtendedVlanBlockSize;
		 */
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_EGRESS_VLAN_IGMP) {
		param->bEgressExtendedVlanIgmpEnable = ctp_port_cfg[ctp_port].
			bEgressExtendedVlanIgmpEnable;
		param->nEgressExtendedVlanBlockIdIgmp = ctp_port_cfg[ctp_port].
			nEgressExtendedVlanBlockIdIgmp;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_INRESS_NTO1_VLAN)
		param->bIngressNto1VlanEnable = ctp_port_cfg[ctp_port].
			bIngressNto1VlanEnable;

	if (param->eMask & GSW_CTP_PORT_CONFIG_MASK_EGRESS_NTO1_VLAN)
		param->eMask = ctp_port_cfg[ctp_port].eMask;

	if (param->eMask & GSW_CTP_PORT_CONFIG_INGRESS_METER) {
		param->bIngressMeteringEnable = ctp_port_cfg[ctp_port].
			bIngressMeteringEnable;
		param->nIngressTrafficMeterId = ctp_port_cfg[ctp_port].
			nIngressTrafficMeterId;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_METER) {
		param->bEgressMeteringEnable =
			ctp_port_cfg[ctp_port].bEgressMeteringEnable;
		param->nEgressTrafficMeterId =
			ctp_port_cfg[ctp_port].nEgressTrafficMeterId;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_BRIDGING_BYPASS) {
		param->bBridgingBypass =
			ctp_port_cfg[ctp_port].bBridgingBypass;
		param->nDestLogicalPortId =
			ctp_port_cfg[ctp_port].nDestLogicalPortId;
		param->ePmapperMappingMode =
			ctp_port_cfg[ctp_port].ePmapperMappingMode;
		param->nDestSubIfIdGroup =
			ctp_port_cfg[ctp_port].nDestSubIfIdGroup;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_INGRESS_MARKING) {
		param->eIngressMarkingMode =
			ctp_port_cfg[ctp_port].eIngressMarkingMode;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_MARKING) {
		param->eEgressMarkingMode =
			ctp_port_cfg[ctp_port].eEgressMarkingMode;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_REMARKING) {
		param->eEgressRemarkingMode =
			ctp_port_cfg[ctp_port].eEgressRemarkingMode;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_EGRESS_MARKING_OVERRIDE) {
		param->bEgressMarkingOverrideEnable = ctp_port_cfg[ctp_port].
			bEgressMarkingOverrideEnable;
		param->eEgressMarkingModeOverride = ctp_port_cfg[ctp_port].
			eEgressMarkingModeOverride;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_FLOW_ENTRY) {
		param->nFirstFlowEntryIndex =
			ctp_port_cfg[ctp_port].nFirstFlowEntryIndex;
		param->nNumberOfFlowEntries =
			ctp_port_cfg[ctp_port].nNumberOfFlowEntries;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_LOOPBACK_AND_MIRROR) {
		param->bIngressLoopbackEnable = ctp_port_cfg[ctp_port].
			bIngressLoopbackEnable;
		param->bEgressLoopbackEnable = ctp_port_cfg[ctp_port].
			bEgressLoopbackEnable;
		param->bIngressMirrorEnable = ctp_port_cfg[ctp_port].
			bIngressMirrorEnable;
		param->bEgressMirrorEnable = ctp_port_cfg[ctp_port].
			bEgressMirrorEnable;
	}

	if (param->eMask & GSW_CTP_PORT_CONFIG_LOOPBACK_AND_MIRROR) {
		param->bIngressDaSaSwapEnable = ctp_port_cfg[ctp_port].
			bIngressDaSaSwapEnable;
		param->bEgressDaSaSwapEnable = ctp_port_cfg[ctp_port].
			bEgressDaSaSwapEnable;
	}
	return 0;
}

#define MAX_BRIDGE_PORT 120
u8 bridge_port_f[MAX_BRIDGE_PORT];
GSW_BRIDGE_portConfig_t bridge_port[MAX_BRIDGE_PORT];

#define MAX_BRIDGE	64
u8 bridge_f[MAX_BRIDGE_PORT];
GSW_BRIDGE_config_t bridge[MAX_BRIDGE];

int BridgePortAlloc(GSW_BRIDGE_portConfig_t *param)
{
	int i;

	for (i = 1; i < MAX_BRIDGE_PORT; i++) {
		if (bridge_port_f[i])
			continue;
		bridge_port_f[i] = 1;
		param->nBridgePortId = i;
		return 0;
	}

	return -1;
}

int BridgePortFree(GSW_BRIDGE_portConfig_t *param)
{
	bridge_port_f[param->nBridgePortId] = 0;
	return 0;
}

int BridgeAlloc(GSW_BRIDGE_alloc_t *param)
{
	int i;

	for (i = 1; i < MAX_BRIDGE_PORT; i++) {
		if (bridge_f[i])
			continue;
		bridge_f[i] = 1;
		param->nBridgeId = i;
		return 0;
	}

	return -1;
}

int BridgeFree(GSW_BRIDGE_alloc_t *param)
{
	bridge_f[param->nBridgeId] = 0;
	return 0;
}

int BridgePortConfigSet(GSW_BRIDGE_portConfig_t *param)
{
	int i = param->nBridgePortId;
	//int k;
	bridge_port[i].nBridgePortId = param->nBridgePortId;

	/*If Bridge Port ID is invalid ,find a free Bridge
	 *port configuration table
	 *index and allocate
	 *New Bridge Port configuration table index
	 */
	if (param->nBridgePortId >= MAX_BRIDGE_PORT) {
		PR_ERR("wrong bridge port %d\n", param->nBridgePortId);
		return GSW_statusErr;
	}
	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID)
		bridge_port[i].nBridgeId = param->nBridgeId;

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN) {
		bridge_port[i].bIngressExtendedVlanEnable = param->
			bIngressExtendedVlanEnable;
		if (param->bIngressExtendedVlanEnable)
			bridge_port[i].nIngressExtendedVlanBlockId = param->
				nIngressExtendedVlanBlockId;
		/*bridge_port[i].nIngressExtendedVlanBlockSize = param->
		 *nIngressExtendedVlanBlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN) {
		bridge_port[i].bEgressExtendedVlanEnable = param->
			bEgressExtendedVlanEnable;
		bridge_port[i].nEgressExtendedVlanBlockId = param->
			nEgressExtendedVlanBlockId;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_MARKING)
		bridge_port[i].eIngressMarkingMode = param->eIngressMarkingMode;

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_REMARKING) {
		bridge_port[i].eEgressRemarkingMode = param->
			eEgressRemarkingMode;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_METER) {
		bridge_port[i].bIngressMeteringEnable = param->
			bIngressMeteringEnable;
		bridge_port[i].nIngressTrafficMeterId = param->
			nIngressTrafficMeterId;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_SUB_METER) {
		memcpy(bridge_port[i].bEgressSubMeteringEnable, param->
		       bEgressSubMeteringEnable,
		       sizeof(bridge_port[i].bEgressSubMeteringEnable));
		memcpy(bridge_port[i].nEgressTrafficSubMeterId, param->
		       nEgressTrafficSubMeterId,
		       sizeof(bridge_port[i].nEgressTrafficSubMeterId));
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_CTP_MAPPING) {
#define PMAPPER_BLOCK_SIZE 36
		bridge_port[i].bPmapperEnable = param->bPmapperEnable;
		bridge_port[i].nDestLogicalPortId = param->nDestLogicalPortId;
		bridge_port[i].ePmapperMappingMode = param->ePmapperMappingMode;
		if (bridge_port[i].bPmapperEnable) {
			if (!bridge_port[i].sPmapper.nPmapperId) {
				bridge_port[i].sPmapper.nPmapperId =
					Pmapper_Alloc(PMAPPER_BLOCK_SIZE);
				param->sPmapper.nPmapperId =
					bridge_port[i].sPmapper.nPmapperId;
			}
		} else if (bridge_port[i].sPmapper.nPmapperId)
			Pmapper_Free(bridge_port[i].sPmapper.nPmapperId,
				     PMAPPER_BLOCK_SIZE);
	}
	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP)
		memcpy(bridge_port[i].nBridgePortMap, param->nBridgePortMap,
		       sizeof(bridge_port[i].nBridgePortMap));

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_DEST_IP_LOOKUP) {
		bridge_port[i].bMcDestIpLookupDisable = param->
			bMcDestIpLookupDisable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_SRC_IP_LOOKUP) {
		bridge_port[i].bMcSrcIpLookupEnable = param->
			bMcSrcIpLookupEnable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_DEST_MAC_LOOKUP) {
		bridge_port[i].bDestMacLookupDisable = param->
			bDestMacLookupDisable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING) {
		bridge_port[i].bSrcMacLearningDisable = param->
			bSrcMacLearningDisable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MAC_SPOOFING) {
		bridge_port[i].bMacSpoofingDetectEnable = param->
			bMacSpoofingDetectEnable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_PORT_LOCK)
		bridge_port[i].bPortLockEnable = param->bPortLockEnable;

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN_FILTER) {
		bridge_port[i].bBypassEgressVlanFilter1 = param->
			bBypassEgressVlanFilter1;
		bridge_port[i].bIngressVlanFilterEnable = param->
			bIngressVlanFilterEnable;
		bridge_port[i].nIngressVlanFilterBlockId = param->
			nIngressVlanFilterBlockId;
		/*bridge_port[i].nIngressVlanFilterBlockSize = param->
		 *nIngressVlanFilterBlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN_FILTER1) {
		bridge_port[i].bEgressVlanFilter1Enable = param->
			bEgressVlanFilter1Enable;
		bridge_port[i].nEgressVlanFilter1BlockId = param->
			nEgressVlanFilter1BlockId;
		/*bridge_port[i].nEgressVlanFilter1BlockSize = param->
		 *nEgressVlanFilter1BlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN_FILTER2) {
		bridge_port[i].bEgressVlanFilter2Enable = param->
			bEgressVlanFilter2Enable;
		bridge_port[i].nEgressVlanFilter2BlockId = param->
			nEgressVlanFilter2BlockId;
		/*bridge_port[i].nEgressVlanFilter2BlockSize = param->
		 *nEgressVlanFilter2BlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MAC_LEARNING_LIMIT) {
		bridge_port[i].bMacLearningLimitEnable = param->
			bMacLearningLimitEnable;
		bridge_port[i].nMacLearningLimit = param->nMacLearningLimit;
	}
	return 0;
}

int BridgePortConfigGet(GSW_BRIDGE_portConfig_t *param)
{
	int i = param->nBridgePortId;
	//int k;

	if (param->nBridgePortId >= MAX_BRIDGE_PORT) {
		PR_ERR("wrong bridge port %d\n", param->nBridgePortId);
		return GSW_statusErr;
	}
	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID)
		param->nBridgeId = bridge_port[i].nBridgeId;

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN) {
		param->bIngressExtendedVlanEnable = bridge_port[i].
			bIngressExtendedVlanEnable;

		param->nIngressExtendedVlanBlockId = bridge_port[i].
			nIngressExtendedVlanBlockId;
		/*param->nIngressExtendedVlanBlockSize = bridge_port[i].
		 *nIngressExtendedVlanBlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN) {
		param->bEgressExtendedVlanEnable = bridge_port[i].
			bEgressExtendedVlanEnable;
		param->nEgressExtendedVlanBlockId = bridge_port[i].
			nEgressExtendedVlanBlockId;
		/*param->nEgressExtendedVlanBlockSize = bridge_port[i].
		 *nEgressExtendedVlanBlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_MARKING)
		param->eIngressMarkingMode = bridge_port[i].eIngressMarkingMode;

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_REMARKING) {
		param->eEgressRemarkingMode = bridge_port[i].
			eEgressRemarkingMode;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_METER) {
		param->bIngressMeteringEnable = bridge_port[i].
			bIngressMeteringEnable;
		param->nIngressTrafficMeterId = bridge_port[i].
			nIngressTrafficMeterId;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_SUB_METER) {
		memcpy(param->bEgressSubMeteringEnable, bridge_port[i].
		       bEgressSubMeteringEnable,
		       sizeof(bridge_port[i].bEgressSubMeteringEnable));
		memcpy(param->nEgressTrafficSubMeterId, bridge_port[i].
		       nEgressTrafficSubMeterId,
		       sizeof(bridge_port[i].nEgressTrafficSubMeterId));
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_CTP_MAPPING) {
#define PMAPPER_BLOCK_SIZE 36
		param->bPmapperEnable = bridge_port[i].bPmapperEnable;
		param->nDestLogicalPortId = bridge_port[i].nDestLogicalPortId;
		param->ePmapperMappingMode = bridge_port[i].ePmapperMappingMode;
		memcpy(&param->sPmapper, &bridge_port[i].sPmapper,
		       sizeof(param->sPmapper));
	}
	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP) {
		memcpy(param->nBridgePortMap, bridge_port[i].nBridgePortMap,
		       sizeof(bridge_port[i].nBridgePortMap));
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_DEST_IP_LOOKUP) {
		param->bMcDestIpLookupDisable = bridge_port[i].
			bMcDestIpLookupDisable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_SRC_IP_LOOKUP) {
		param->bMcSrcIpLookupEnable = bridge_port[i].
			bMcSrcIpLookupEnable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_DEST_MAC_LOOKUP) {
		param->bDestMacLookupDisable = bridge_port[i].
			bDestMacLookupDisable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING) {
		param->bSrcMacLearningDisable = bridge_port[i].
			bSrcMacLearningDisable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MAC_SPOOFING) {
		param->bMacSpoofingDetectEnable = bridge_port[i].
			bMacSpoofingDetectEnable;
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_PORT_LOCK)
		param->bPortLockEnable = bridge_port[i].bPortLockEnable;

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN_FILTER) {
		param->bBypassEgressVlanFilter1 = bridge_port[i].
			bBypassEgressVlanFilter1;
		param->bIngressVlanFilterEnable = bridge_port[i].
			bIngressVlanFilterEnable;
		param->nIngressVlanFilterBlockId = bridge_port[i].
			nIngressVlanFilterBlockId;
		/*param->nIngressVlanFilterBlockSize = bridge_port[i].
		 *nIngressVlanFilterBlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN_FILTER1) {
		param->bEgressVlanFilter1Enable = bridge_port[i].
			bEgressVlanFilter1Enable;
		param->nEgressVlanFilter1BlockId = bridge_port[i].
			nEgressVlanFilter1BlockId;
		/*param->nEgressVlanFilter1BlockSize = bridge_port[i].
		 *nEgressVlanFilter1BlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN_FILTER2) {
		param->bEgressVlanFilter2Enable = bridge_port[i].
			bEgressVlanFilter2Enable;
		param->nEgressVlanFilter2BlockId = bridge_port[i].
			nEgressVlanFilter2BlockId;
		/*param->nEgressVlanFilter2BlockSize = bridge_port[i].
		 *nEgressVlanFilter2BlockSize;
		 */
	}

	if (param->eMask & GSW_BRIDGE_PORT_CONFIG_MASK_MAC_LEARNING_LIMIT) {
		param->bMacLearningLimitEnable = bridge_port[i].
			bMacLearningLimitEnable;
		param->nMacLearningLimit = bridge_port[i].nMacLearningLimit;
	}
	return 0;
}

int BridgeConfigSet(GSW_BRIDGE_config_t *param)
{
	int i = param->nBridgeId;

	if (param->eMask & GSW_BRIDGE_CONFIG_MASK_MAC_LEARNING_LIMIT) {
		bridge[i].bMacLearningLimitEnable = param->
			bMacLearningLimitEnable;
	}

	if (param->eMask & GSW_BRIDGE_CONFIG_MASK_SUB_METER) {
		memcpy(bridge[i].bSubMeteringEnable, param->bSubMeteringEnable,
		       sizeof(bridge[i].bSubMeteringEnable));
		memcpy(bridge[i].nTrafficSubMeterId, param->nTrafficSubMeterId,
		       sizeof(bridge[i].nTrafficSubMeterId));
	}

	if (param->eMask & GSW_BRIDGE_CONFIG_MASK_FORWARDING_MODE) {
		bridge[i].eForwardBroadcast = param->eForwardBroadcast;
		bridge[i].eForwardUnknownUnicast = param->
			eForwardUnknownUnicast;
		bridge[i].eForwardUnknownMulticastNonIp = param->
			eForwardUnknownMulticastNonIp;
	}
	return 0;
}

int BridgeConfigGet(GSW_BRIDGE_config_t *param)
{
	int i = param->nBridgeId;

	if (param->eMask & GSW_BRIDGE_CONFIG_MASK_MAC_LEARNING_LIMIT) {
		param->bMacLearningLimitEnable = bridge[i].
			bMacLearningLimitEnable;
	}

	if (param->eMask & GSW_BRIDGE_CONFIG_MASK_SUB_METER) {
		memcpy(param->bSubMeteringEnable, bridge[i].bSubMeteringEnable,
		       sizeof(bridge[i].bSubMeteringEnable));
		memcpy(param->nTrafficSubMeterId, bridge[i].nTrafficSubMeterId,
		       sizeof(bridge[i].nTrafficSubMeterId));
	}

	if (param->eMask & GSW_BRIDGE_CONFIG_MASK_FORWARDING_MODE) {
		param->eForwardBroadcast = bridge[i].eForwardBroadcast;
		param->eForwardUnknownUnicast = bridge[i].
			eForwardUnknownUnicast;
		param->eForwardUnknownMulticastNonIp = bridge[i].
			eForwardUnknownMulticastNonIp;
	}
	return 0;
}

#define MAX_EXTENDVLAN  512
u8 extvlan_f[MAX_EXTENDVLAN] = {0};
GSW_EXTENDEDVLAN_alloc_t extvlan_alloc[MAX_EXTENDVLAN] = {0};
GSW_EXTENDEDVLAN_config_t extvlan[MAX_EXTENDVLAN] = {0};
GSW_return_t ExtendedVlanAlloc(GSW_EXTENDEDVLAN_alloc_t *param)
{
	int i, j;

	for (i = 1; i < MAX_EXTENDVLAN; i++) {
		if (extvlan_f[i])
			continue;
		for (j = 0; j < param->nNumberOfEntries; j++)
			if (extvlan_f[i + j])
				continue;
		param->nExtendedVlanBlockId = i;
		for (j = 0; j < param->nNumberOfEntries; j++) {
			extvlan_f[i + j] = 1;
			extvlan_alloc[i + j].nExtendedVlanBlockId = i;
			extvlan_alloc[i + j].nNumberOfEntries =  param->
				nNumberOfEntries;
		}
		return 0;
	}
	return -1;
}

int ExtendedVlanFree(GSW_EXTENDEDVLAN_alloc_t *param)
{
	int i;

	for (i = 0; i < extvlan_alloc[param->nExtendedVlanBlockId].
	     nNumberOfEntries ; i++)
		extvlan_f[i + param->nExtendedVlanBlockId] = 0;
	return 0;
}

GSW_return_t ExtendedVlanSet(GSW_EXTENDEDVLAN_config_t *param)
{ /*Note: not support pmapper here */
	u32 idx = param->nExtendedVlanBlockId + param->nEntryIndex;

	if (idx >= MAX_EXTENDVLAN) {
		PR_ERR("ERROR : idx %d >= %d\n", idx, MAX_EXTENDVLAN);
		return -1;
	}

	memcpy(&extvlan[idx].sFilter, &param->sFilter,
	       sizeof(extvlan[idx].sFilter));
	memcpy(&extvlan[idx].sTreatment, &param->sTreatment,
	       sizeof(extvlan[idx].sTreatment));

	return 0;
}

GSW_return_t ExtendedVlanGet(GSW_EXTENDEDVLAN_config_t *param)
{ /*Note: not support pmapper here */
	u32 idx = param->nExtendedVlanBlockId + param->nEntryIndex;

	if (idx >= MAX_EXTENDVLAN) {
		PR_ERR("ERROR: idx %d >= %d\n", idx, MAX_EXTENDVLAN);
		return -1;
	}
	memcpy(&param->sFilter, &extvlan[idx].sFilter,
	       sizeof(extvlan[idx].sFilter));
	memcpy(&param->sTreatment, &extvlan[idx].sTreatment,
	       sizeof(extvlan[idx].sTreatment));

	return 0;
}

//TODO Yet to test below simulation code completely
//#define MAC_MAX_ENTRY 4096
#define MAC_MAX_ENTRY 10
u8 mac_f[MAC_MAX_ENTRY];
GSW_MAC_tableAdd_t MacAdd[MAC_MAX_ENTRY];

int MacTableAdd(GSW_MAC_tableAdd_t *param)
{
	int i;

	for (i = 0; i < MAC_MAX_ENTRY; i++) {
		if (mac_f[i]) {
			continue;
		} else {
			PR_ERR("MAC add i(%d) value\n", i);
			mac_f[i] = 1;
			break;
		}
	}

	if (param->nPortId >= MAX_BRIDGE_PORT) {
		PR_ERR("wrong bridge port for MAC add%d\n", param->nPortId);
		return GSW_statusErr;
	}

	if (param->nPortMap) {
		memcpy(MacAdd[i].nPortMap, param->nPortMap,
		       sizeof(param->nPortMap));
	}
	MacAdd[i].nFId = param->nFId;
	MacAdd[i].bStaticEntry = param->bStaticEntry;
	MacAdd[i].nPortId = param->nPortId;
	MacAdd[i].nSubIfId = param->nSubIfId;
	memcpy(MacAdd[i].nMAC, param->nMAC, sizeof(param->nMAC));
	PR_ERR("MAC add for entry:%d %02x:%02x:%02x:%02x:%02x:%02x\n", i,
	       MacAdd[i].nMAC[0], MacAdd[i].nMAC[1], MacAdd[i].nMAC[2],
	       MacAdd[i].nMAC[3], MacAdd[i].nMAC[4], MacAdd[i].nMAC[5]);
	return 0;
}

int MacTableRemove(GSW_MAC_tableRemove_t *param)
{
	int i;

	for (i = 0; i < MAC_MAX_ENTRY; i++) {
		if (!mac_f[i])
			continue;
		if (MacAdd[i].nFId != param->nFId)
			continue;
		if (memcmp(MacAdd[i].nMAC, param->nMAC, sizeof(param->nMAC))) {
			memset(&MacAdd[i], 0, sizeof(MacAdd[i]));
			PR_ERR("MAC remove for entry:%d\n", i);
		}
		mac_f[i] = 0;
		return 0;
	}
	return -1;
}

int MacTableQuery(GSW_MAC_tableQuery_t *param)
{
	int i;

	for (i = 0; i < MAC_MAX_ENTRY; i++) {
		if (!mac_f[i])
			continue;
		if (MacAdd[i].nFId != param->nFId)
			continue;
		if (memcmp(MacAdd[i].nMAC, param->nMAC, sizeof(param->nMAC))) {
			param->nFId = MacAdd[i].nFId;
			param->bFound = 1;
			param->nPortId = MacAdd[i].nPortId;
			param->nSubIfId = MacAdd[i].nSubIfId;
			param->bStaticEntry = MacAdd[i].bStaticEntry;
			memcpy(param->nPortMap,
			       MacAdd[i].nPortMap,
			       sizeof(MacAdd[i].nPortMap));
			memcpy(param->nMAC, MacAdd[i].nMAC,
			       sizeof(MacAdd[i].nMAC));

		PR_ERR("%d    %d    %d	%02x:%02x:%02x:%02x:%02x:%02x\n",
		       param->nFId, param->nPortId,
		       param->bStaticEntry,
		       param->nMAC[0], param->nMAC[1], param->nMAC[2],
		       param->nMAC[3], param->nMAC[4], param->nMAC[5]);
			return 0;
		} else if (MacAdd[i].nPortId == param->nPortId) {
			param->nFId = MacAdd[i].nFId;
			param->bFound = 1;
			param->nPortId = MacAdd[i].nPortId;
			param->nSubIfId = MacAdd[i].nSubIfId;
			memcpy(param->nPortMap,
			       MacAdd[i].nPortMap,
			       sizeof(MacAdd[i].nPortMap));
			memcpy(param->nMAC, MacAdd[i].nMAC,
			       sizeof(MacAdd[i].nMAC));
			return 0;
		}
	}
	PR_ERR("MAC Tbl query not match\n");
	return -1;
}

int MacTableRead(GSW_MAC_tableRead_t *param)
{
	int i;

	PR_ERR("MAC Tbl read\n");
	for (i = 0; i < MAC_MAX_ENTRY; i++) {
		if (!mac_f[i])
			return -1;
		param->nFId = MacAdd[i].nFId;
		param->nPortId = MacAdd[i].nPortId;
		param->nSubIfId = MacAdd[i].nSubIfId;
		param->bStaticEntry = MacAdd[i].bStaticEntry;
		memcpy(param->nPortMap,
		       MacAdd[i].nPortMap,
		       sizeof(MacAdd[i].nPortMap));
		memcpy(param->nMAC, MacAdd[i].nMAC,
		       sizeof(MacAdd[i].nMAC));
	}
	return 0;
}
