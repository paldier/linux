/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef DATAPATH_GSWIP_SIMULATE_H_
#define DATAPATH_GSWIP_SIMULATE_H_

u32  Pmapper_Alloc(int size);
int Pmapper_Free(u32 offset, int size);
int CTP_PortAssignmentAlloc(GSW_CTP_portAssignment_t *param);
int CTP_PortAssignmentFree(GSW_CTP_portAssignment_t *param);
int CTP_PortAssignmentSet(GSW_CTP_portAssignment_t *param);
int CTP_PortAssignmentGet(GSW_CTP_portAssignment_t *param);
int CtpPortConfigSet(GSW_CTP_portConfig_t *param);
int CtpPortConfigGet(GSW_CTP_portConfig_t *param);
int BridgePortAlloc(GSW_BRIDGE_portConfig_t *param);
int BridgePortFree(GSW_BRIDGE_portConfig_t *param);
int BridgeAlloc(GSW_BRIDGE_alloc_t *param);
int BridgeFree(GSW_BRIDGE_alloc_t *param);
int BridgePortConfigSet(GSW_BRIDGE_portConfig_t *param);
int BridgePortConfigGet(GSW_BRIDGE_portConfig_t *param);
int BridgeConfigSet(GSW_BRIDGE_config_t *param);
int BridgeConfigGet(GSW_BRIDGE_config_t *param);
int MacTableAdd(GSW_MAC_tableAdd_t *param);
int MacTableRemove(GSW_MAC_tableRemove_t *param);
int MacTableQuery(GSW_MAC_tableQuery_t *param);
int MacTableRead(GSW_MAC_tableAdd_t *param);

GSW_return_t ExtendedVlanAlloc(GSW_EXTENDEDVLAN_alloc_t *param);
int ExtendedVlanFree(GSW_EXTENDEDVLAN_alloc_t *param);
GSW_return_t ExtendedVlanSet(GSW_EXTENDEDVLAN_config_t *param);
GSW_return_t ExtendedVlanGet(GSW_EXTENDEDVLAN_config_t *param);
#endif

