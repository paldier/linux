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
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if_ether.h>
#include <linux/ethtool.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/if_ether.h>
#include <linux/clk.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <lantiq_soc.h>
#include <net/lantiq_cbm_api.h>
#include <net/datapath_api.h>
#include <net/datapath_api_skb.h>
#include "datapath.h"
#include "datapath_instance.h"
#include "datapath_swdev_api.h"

#if IS_ENABLED(CONFIG_PPA_API_SW_FASTPATH)
#include <net/ppa/ppa_api.h>
#endif

#if defined(CONFIG_LTQ_DATAPATH_DBG) && CONFIG_LTQ_DATAPATH_DBG
unsigned int dp_max_print_num = -1, dp_print_num_en = 0;
#endif

GSW_API_HANDLE gswr_r;
u32    dp_rx_test_mode = DP_RX_MODE_NORMAL;
struct dma_rx_desc_1 dma_rx_desc_mask1;
struct dma_rx_desc_3 dma_rx_desc_mask3;
struct dma_rx_desc_0 dma_tx_desc_mask0;
struct dma_rx_desc_1 dma_tx_desc_mask1;
u32 dp_drop_all_tcp_err;
u32 dp_pkt_size_check;

u32 dp_dbg_flag;
EXPORT_SYMBOL(dp_dbg_flag);

#ifdef CONFIG_LTQ_DATAPATH_MPE_FASTHOOK_TEST
u32 ltq_mpe_eanble;
EXPORT_SYMBOL(ltq_mpe_eanble);

int (*ltq_mpe_fasthook_free_fn)(struct sk_buff *) = NULL;
EXPORT_SYMBOL(ltq_mpe_fasthook_free_fn);

int (*ltq_mpe_fasthook_tx_fn)(struct sk_buff *, u32, void *) = NULL;
EXPORT_SYMBOL(ltq_mpe_fasthook_tx_fn);

int (*ltq_mpe_fasthook_rx_fn)(struct sk_buff *, u32, void *) = NULL;
EXPORT_SYMBOL(ltq_mpe_fasthook_rx_fn);
#endif	/*CONFIG_LTQ_DATAPATH_MPE_FASTHOOK_TEST */

#undef DP_DBG_ENUM_OR_STRING
#define DP_DBG_ENUM_OR_STRING(name, short_name) short_name
char *dp_dbg_flag_str[] = DP_DBG_FLAG_LIST;

#undef DP_DBG_ENUM_OR_STRING
#define DP_DBG_ENUM_OR_STRING(name, short_name) name
u32 dp_dbg_flag_list[] = DP_DBG_FLAG_LIST;

#undef DP_F_ENUM_OR_STRING
#define DP_F_ENUM_OR_STRING(name, short_name) short_name
char *dp_port_type_str[] = DP_F_FLAG_LIST;

#undef DP_F_ENUM_OR_STRING
#define DP_F_ENUM_OR_STRING(name, short_name) name
u32 dp_port_flag[] = DP_F_FLAG_LIST;

char *dp_port_status_str[] = {
	"PORT_FREE",
	"PORT_ALLOCATED",
	"PORT_DEV_REGISTERED",
	"PORT_SUBIF_REGISTERED",
	"Invalid"
};

static int try_walkaround;
static int dp_init_ok;
DP_DEFINE_LOCK(dp_lock);
unsigned int dp_dbg_err = 1; /*print error */
static int32_t dp_rx_one_skb(struct sk_buff *skb, uint32_t flags);
/*port 0 is reserved and never assigned to any one */
int dp_inst_num;
/* Keep per DP instance information here */
struct inst_property dp_port_prop[DP_MAX_INST];
/* Keep all subif information per instance/LPID/subif */
struct pmac_port_info dp_port_info[DP_MAX_INST][MAX_DP_PORTS];
/* Keep all default DMA descriptor mask/bit per instance/LPID */
struct pmac_port_info2 dp_port_info2[DP_MAX_INST][MAX_DP_PORTS];

/* bp_mapper_dev[] is mainly for PON case
 * Only if multiple gem port are attached to same bridge port,
 * This bridge port device will be recorded into this bp_mapper_dev[].
 * later other information, like pmapper ID/mapping table will be put here also
 */
struct bp_pmapper_dev dp_bp_dev_tbl[DP_MAX_INST][DP_MAX_BP_NUM];
/* q_tbl[] is mainly for the queue created/used during dp_register_subif_ext
 */
struct q_info dp_q_tbl[DP_MAX_INST][DP_MAX_QUEUE_NUM];
/* sched_tbl[] is mainly for the sched created/used during dp_register_subif_ext
 */
struct sched_info dp_sched_tbl[DP_MAX_INST][DP_MAX_SCHED_NUM];
/* dp_deq_port_tbl[] is to record cqm dequeue port info
 */
struct cqm_port_info dp_deq_port_tbl[DP_MAX_INST][DP_MAX_CQM_DEQ];

struct parser_info pinfo[4];
static int print_len;
#ifdef CONFIG_LTQ_DATAPATH_ACA_CSUM_WORKAROUND
static struct module aca_owner;
static struct net_device aca_dev;
static int aca_portid = -1;
#endif

char *get_dp_port_type_str(int k)
{
	return dp_port_type_str[k];
}

u32 get_dp_port_flag(int k)
{
	return dp_port_flag[k];
}

int get_dp_port_type_str_size(void)
{
	return ARRAY_SIZE(dp_port_type_str);
}

int get_dp_dbg_flag_str_size(void)
{
	return ARRAY_SIZE(dp_dbg_flag_str);
}

int get_dp_port_status_str_size(void)
{
	return ARRAY_SIZE(dp_port_status_str);
}

int parser_size_via_index(u8 index)
{
	if (index >= ARRAY_SIZE(pinfo)) {
		PR_ERR("Wrong index=%d, it should less than %d\n", index,
		       ARRAY_SIZE(pinfo));
		return 0;
	}

	return pinfo[index].size;
}

static inline int parser_enabled(int ep, struct dma_rx_desc_1 *desc_1)
{
#ifdef CONFIG_LTQ_DATAPATH_EXTRA_DEBUG
	if (!desc_1) {
		PR_ERR("NULL desc_1 is not allowed\n");
		return 0;
	}
#endif
	if (!ep)
		return pinfo[(desc_1->field.mpe2 << 1) +
			desc_1->field.mpe1].size;
	return 0;
}

struct inst_property *get_dp_port_prop(int inst)
{
	if (!((inst < 0) || (inst >= DP_MAX_INST)))
		return &dp_port_prop[inst];
	return NULL;
}
EXPORT_SYMBOL(get_dp_port_prop);

struct pmac_port_info *get_dp_port_info(int inst, int index)
{
	if (!((inst < 0) || (inst >= DP_MAX_INST)) &&
	    (index < dp_port_prop[inst].info.cap.max_num_dp_ports))
		return &dp_port_info[inst][index];
	return NULL;
}
EXPORT_SYMBOL(get_dp_port_info);

struct pmac_port_info *get_port_info(int inst, int index)
{
	if (index < dp_port_prop[inst].info.cap.max_num_dp_ports)
		return &dp_port_info[inst][index];

	return NULL;
}

u32 *get_port_flag(int inst, int index)
{
	if (index < dp_port_prop[inst].info.cap.max_num_dp_ports)
		return &dp_port_info[inst][index].alloc_flags;

	return NULL;
}

struct pmac_port_info *get_port_info_via_dp_port(int inst, int dp_port)
{
	int i;

	for (i = 0; i < dp_port_prop[inst].info.cap.max_num_dp_ports; i++) {
		if ((dp_port_info[inst][i].status & PORT_DEV_REGISTERED) &&
		    (dp_port_info[inst][i].port_id == dp_port))
			return &dp_port_info[inst][i];
	}

	return NULL;
}

struct pmac_port_info *get_port_info_via_dev(struct net_device *dev)
{
	int i;
	int inst = dp_get_inst_via_dev(dev, NULL, 0);

	for (i = 0; i < dp_port_prop[inst].info.cap.max_num_dp_ports; i++) {
		if ((dp_port_info[inst][i].status & PORT_DEV_REGISTERED) &&
		    (dp_port_info[inst][i].dev == dev))
			return &dp_port_info[inst][i];
	}
	return NULL;
}

int8_t parser_size(int8_t v)
{
	if (v == DP_PARSER_F_DISABLE)
		return 0;

	if (v == DP_PARSER_F_HDR_ENABLE)
		return PASAR_OFFSETS_NUM;

	if (v == DP_PARSER_F_HDR_OFFSETS_ENABLE)
		return PASAR_OFFSETS_NUM + PASAR_FLAGS_NUM;

	PR_ERR("Wrong parser setting: %d\n", v);
	/*error */
	return -1;
}

/*Only for SOC side, not for peripheral device side */
int dp_set_gsw_parser(u8 flag, u8 cpu, u8 mpe1, u8 mpe2, u8 mpe3)
{
	int inst = 0;

	if (!dp_port_prop[inst].info.dp_set_gsw_parser)
		return -1;

	return dp_port_prop[inst].info.dp_set_gsw_parser(flag, cpu, mpe1,
							 mpe2, mpe3);
}
EXPORT_SYMBOL(dp_set_gsw_parser);

int dp_get_gsw_parser(u8 *cpu, u8 *mpe1, u8 *mpe2, u8 *mpe3)
{
	int inst = 0;

	if (!dp_port_prop[inst].info.dp_get_gsw_parser)
		return -1;

	return dp_port_prop[inst].info.dp_get_gsw_parser(cpu, mpe1,
							 mpe2, mpe3);
}
EXPORT_SYMBOL(dp_get_gsw_parser);

char *parser_str(int index)
{
	if (index == 0)
		return "cpu";

	if (index == 1)
		return "mpe1";

	if (index == 2)
		return "mpe2";

	if (index == 3)
		return "mpe3";

	PR_ERR("Wrong index:%d\n", index);
	return "Wrong index";
}

/* some module may have reconfigure parser configuration in FMDA_PASER.
 * It is necessary to refresh the pinfo
 */
void dp_parser_info_refresh(u32 cpu, u32 mpe1, u32 mpe2,
			    u32 mpe3, u32 verify)
{
	int i;

	pinfo[0].v = cpu;
	pinfo[1].v = mpe1;
	pinfo[2].v = mpe2;
	pinfo[3].v = mpe3;

	for (i = 0; i < ARRAY_SIZE(pinfo); i++) {
		if (verify &&
		    (pinfo[i].size != parser_size(pinfo[i].v)))
			PR_ERR
			 ("Lcal parser pinfo[%d](%d) != register cfg(%d)??\n",
			 i, pinfo[i].size,
			 parser_size(pinfo[i].v));

		/*force to update */
		pinfo[i].size = parser_size(pinfo[i].v);

		if ((pinfo[i].size < 0) || (pinfo[i].size > PKT_PMAC_OFFSET)) {
			PR_ERR("Wrong parser setting for %s: %d\n",
			       parser_str(i), pinfo[i].v);
		}
	}
}
EXPORT_SYMBOL(dp_parser_info_refresh);

void print_parser_status(struct seq_file *s)
{
	if (!s)
		return;

	seq_printf(s, "REG.cpu  value=%u size=%u\n", pinfo[0].v,
		   pinfo[0].size);
	seq_printf(s, "REG.MPE1 value=%u size=%u\n", pinfo[1].v,
		   pinfo[1].size);
	seq_printf(s, "REG.MPE2 value=%u size=%u\n", pinfo[2].v,
		   pinfo[2].size);
	seq_printf(s, "REG.MPE3 value=%u size=%u\n", pinfo[3].v,
		   pinfo[3].size);
}

/*note: dev can be NULL */
static int32_t dp_alloc_port_private(int inst,
				     struct module *owner,
				     struct net_device *dev,
				     u32 dev_port, s32 port_id,
				     dp_pmac_cfg_t *pmac_cfg,
				     struct dp_port_data *data,
				     u32 flags)
{
	int i;
	struct cbm_dp_alloc_data cbm_data = {0};

	if (!owner) {
		PR_ERR("Allocate port failed for owner NULL\n");
		return DP_FAILURE;
	}

	if (port_id >= MAX_DP_PORTS || port_id < 0) {
		DP_DEBUG_ASSERT((port_id >= MAX_DP_PORTS),
				"port_id(%d) >= MAX_DP_PORTS(%d)", port_id,
				MAX_DP_PORTS);
		DP_DEBUG_ASSERT((port_id < 0), "port_id(%d) < 0", port_id);
		return DP_FAILURE;
	}

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DBG)
	if (unlikely(dp_dbg_flag & DP_DBG_FLAG_REG)) {
		DP_DEBUG(DP_DBG_FLAG_REG, "Flags=");
		for (i = 0; i < ARRAY_SIZE(dp_port_type_str); i++)
			if (flags & dp_port_flag[i])
				DP_DEBUG(DP_DBG_FLAG_REG, "%s ",
					 dp_port_type_str[i]);
		DP_DEBUG(DP_DBG_FLAG_REG, "\n");
	}
#endif
	cbm_data.dp_inst = inst;
	cbm_data.cbm_inst = dp_port_prop[inst].cbm_inst;

	if (flags & DP_F_DEREGISTER) {	/*De-register */
		if (dp_port_info[inst][port_id].status != PORT_ALLOCATED) {
			PR_ERR
			    ("No Deallocate for module %s w/o deregistered\n",
			     owner->name);
			return DP_FAILURE;
		}
		cbm_data.deq_port = dp_port_info[inst][port_id].deq_port_base;
		cbm_data.dma_chan = dp_port_info[inst][port_id].dma_chan;
		cbm_dp_port_dealloc(owner, dev_port, port_id, &cbm_data, flags);
		dp_inst_insert_mod(owner, port_id, inst, 0);
		DP_DEBUG(DP_DBG_FLAG_REG, "de-alloc port %d\n", port_id);
		DP_CB(inst, port_platform_set)(inst, port_id, data, flags);
		memset(&dp_port_info[inst][port_id], 0,
		       sizeof(dp_port_info[inst][port_id]));
		return DP_SUCCESS;
	}
	if (port_id) { /*with specified port_id */
		if (dp_port_info[inst][port_id].status != PORT_FREE) {
			PR_ERR("%s %s(%s %d) fail: port %d used by %s %d\n",
			       "module", owner->name,
			       "dev_port", dev_port, port_id,
			       dp_port_info[inst][port_id].owner->name,
			       dp_port_info[inst][port_id].dev_port);
			return DP_FAILURE;
		}
	}
	if (cbm_dp_port_alloc(owner, dev, dev_port, port_id,
			      &cbm_data, flags)) {
		PR_ERR
		    ("cbm_dp_port_alloc fail for %s/dev_port %d: %d\n",
		     owner->name, dev_port, port_id);
		return DP_FAILURE;
	} else if (!(cbm_data.flags & CBM_PORT_DP_SET) &&
		   !(cbm_data.flags & CBM_PORT_DQ_SET)) {
		PR_ERR("%s NO DP_SET/DQ_SET(%x):%s/dev_port %d\n",
		       "cbm_dp_port_alloc",
		       cbm_data.flags,
		       owner->name, dev_port);
		return DP_FAILURE;
	}
	port_id = cbm_data.dp_port;
	memset(&dp_port_info[inst][port_id], 0,
	       sizeof(dp_port_info[inst][port_id]));
	/*save info from caller */
	dp_port_info[inst][port_id].owner = owner;
	dp_port_info[inst][port_id].dev = dev;
	dp_port_info[inst][port_id].dev_port = dev_port;
	dp_port_info[inst][port_id].alloc_flags = flags;
	dp_port_info[inst][port_id].status = PORT_ALLOCATED;
	/*save info from cbm_dp_port_alloc*/
	dp_port_info[inst][port_id].flag_other = cbm_data.flags;
	dp_port_info[inst][port_id].port_id = cbm_data.dp_port;
	dp_port_info[inst][port_id].deq_port_base = cbm_data.deq_port;
	dp_port_info[inst][port_id].deq_port_num = cbm_data.deq_port_num;
	DP_DEBUG(DP_DBG_FLAG_REG,
		 "cbm alloc dp_port:%d deq:%d deq_num:%d\n",
		 cbm_data.dp_port, cbm_data.deq_port, cbm_data.deq_port_num);
	if (cbm_data.flags & CBM_PORT_DMA_CHAN_SET)
		dp_port_info[inst][port_id].dma_chan = cbm_data.dma_chan;
	if (cbm_data.flags & CBM_PORT_PKT_CRDT_SET)
		dp_port_info[inst][port_id].tx_pkt_credit =
				cbm_data.tx_pkt_credit;
	if (cbm_data.flags & CBM_PORT_BYTE_CRDT_SET)
	dp_port_info[inst][port_id].tx_b_credit = cbm_data.tx_b_credit;
	if (cbm_data.flags & CBM_PORT_RING_ADDR_SET)
	dp_port_info[inst][port_id].tx_ring_addr = cbm_data.tx_ring_addr;
	if (cbm_data.flags & CBM_PORT_RING_SIZE_SET)
	dp_port_info[inst][port_id].tx_ring_size = cbm_data.tx_ring_size;
	if (cbm_data.flags & CBM_PORT_RING_OFFSET_SET)
		dp_port_info[inst][port_id].tx_ring_offset =
				cbm_data.tx_ring_offset;
	if (dp_port_prop[inst].info.port_platform_set(inst, port_id,
						      data, flags)) {
		PR_ERR("Failed port_platform_set for port_id=%d(%s)\n",
		       port_id, owner ? owner->name : "");
		cbm_dp_port_dealloc(owner, dev_port, port_id, &cbm_data,
				    flags | DP_F_DEREGISTER);
		memset(&dp_port_info[inst][port_id], 0,
		       sizeof(dp_port_info[inst][port_id]));
		return DP_FAILURE;
	}
	if (pmac_cfg)
		dp_pmac_set(inst, port_id, pmac_cfg);
	/*only 1st dp instance support real CPU path traffic */
	if (!inst && dp_port_prop[inst].info.init_dma_pmac_template)
		dp_port_prop[inst].info.init_dma_pmac_template(port_id, flags);
	for (i = 0; i < MAX_SUBIFS; i++)
		INIT_LIST_HEAD(&dp_port_info[inst][port_id].
			subif_info[i].logic_dev);
	dp_inst_insert_mod(owner, port_id, inst, 0);

	DP_DEBUG(DP_DBG_FLAG_REG,
		 "Port %d allocation succeed for module %s with dev_port %d\n",
		 port_id, owner->name, dev_port);
	return port_id;
}

int32_t dp_register_subif_private(int inst, struct module *owner,
				  struct net_device *dev,
				  char *subif_name, dp_subif_t *subif_id,
				  /*device related info*/
				  struct dp_subif_data *data, u32 flags)
{
	int res = DP_FAILURE;

	int i, port_id, start, end;
	struct pmac_port_info *port_info;
	struct cbm_dp_en_data cbm_data = {0};
	struct subif_platform_data platfrm_data = {0};

	port_id = subif_id->port_id;
	port_info = &dp_port_info[inst][port_id];
	subif_id->inst = inst;
	subif_id->subif_num = 1;
	platfrm_data.subif_data = data;
	platfrm_data.dev = dev;
	/*Sanity Check*/
	if (port_info->status < PORT_DEV_REGISTERED) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "register subif failed:%s is not a registered dev!\n",
			 subif_name);
		return res;
	}

	if (subif_id->subif < 0) {/*dynamic mode */
		if (flags & DP_F_SUBIF_LOGICAL) {
			if (!(DP_CB(inst, supported_logic_dev)(inst,
							       dev,
							       subif_name))) {
				DP_DEBUG(DP_DBG_FLAG_REG,
					 "reg subif fail:%s not support dev\n",
				 subif_name);
				return res;
			}
			if (!(flags & DP_F_ALLOC_EXPLICIT_SUBIFID)) {
				/*Share same subif with its base device
				 *For GRX350: nothing need except save it
				 *For PRX300: it need to allocate BP for it
				 */
				res = add_logic_dev(inst, port_id, dev,
						    subif_id, flags);
				return res;
			}
		}
		start = 0;
		end = port_info->ctp_max;
	} else {
		/*caller provided subif. Try to get its vap value as start */
		start = GET_VAP(subif_id->subif, port_info->vap_offset,
				port_info->vap_mask);
		end = start + 1;
	}

	/*PR_INFO("search range: start=%d end=%d\n",start, end);*/
    /*allocate a free subif */
	for (i = start; i < end; i++) {
		if (port_info->subif_info[i].flags) /*used already & not free*/
			continue;

		/*now find a free subif or valid subif
		 *need to do configuration HW
		 */
		if (port_info->status) {
			if (dp_port_prop[inst].info.
					subif_platform_set(inst, port_id, i,
							   &platfrm_data,
							   flags)) {
				PR_ERR("subif_platform_set fail\n");
				goto EXIT;
			} else {
				DP_DEBUG(DP_DBG_FLAG_REG,
					 "subif_platform_set succeed\n");
			}
		} else {
			PR_ERR("port info status fail for 0\n");
			return res;
		}
		port_info->subif_info[i].flags = 1;
		port_info->subif_info[i].netif = dev;
		port_info->port_id = port_id;

		if (subif_id->subif < 0) /*dynamic:shift bits as HW defined*/
			port_info->subif_info[i].subif =
				SET_VAP(i, port_info->vap_offset,
					port_info->vap_mask);
		else /*provided by caller since it is alerady shifted properly*/
			port_info->subif_info[i].subif =
			    subif_id->subif;
		strncpy(port_info->subif_info[i].device_name,
			subif_name,
		       sizeof(port_info->subif_info[i].device_name) - 1);
		port_info->subif_info[i].flags = PORT_SUBIF_REGISTERED;
		port_info->subif_info[i].subif_flag = flags;
		port_info->status = PORT_SUBIF_REGISTERED;
		subif_id->port_id = port_id;
		subif_id->subif = port_info->subif_info[i].subif;
		/* set port as LCT port */
		if (data->flag_ops & DP_F_DATA_LCT_SUBIF)
			port_info->lct_idx = i;
		port_info->num_subif++;
		if ((port_info->num_subif == 1) ||
		    (platfrm_data.act & TRIGGER_CQE_DP_ENABLE)) {
			cbm_data.dp_inst = inst;
			cbm_data.cbm_inst = dp_port_prop[inst].cbm_inst;
			cbm_data.deq_port = port_info->deq_port_base +
				(data ? data->deq_port_idx : 0);
			if ((cbm_data.deq_port == 0) ||
			    (cbm_data.deq_port >= DP_MAX_CQM_DEQ)) {
				PR_ERR("Wrong deq_port: %d\n",
				       cbm_data.deq_port);
				return res;
			}
			if (port_info->num_subif == 1)
				cbm_data.dma_chnl_init = 1; /*to enable DMA*/
			DP_DEBUG(DP_DBG_FLAG_REG, "%s:%s%d %s%d %s%d\n",
				 "cbm_dp_enable",
				 "dp_port=", port_id,
				 "deq_port=", cbm_data.deq_port,
				 "dma_chnl_init=", cbm_data.dma_chnl_init);
			if (cbm_dp_enable(owner, port_id, &cbm_data, 0,
					  port_info->alloc_flags)) {
				DP_DEBUG(DP_DBG_FLAG_REG,
					 "cbm_dp_enable fail\n");
				return res;
			}
			DP_DEBUG(DP_DBG_FLAG_REG, "cbm_dp_enable ok\n");
		} else {
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "No need cbm_dp_enable:dp_port=%d subix=%d\n",
				 port_id, i);
		}
		break;
	}

	if (i < end) {
		res = DP_SUCCESS;
		if (dp_bp_dev_tbl[inst][port_info->subif_info[i].bp].
		    ref_cnt > 1)
			return res;
		dp_inst_add_dev(dev, subif_name,
				subif_id->inst, subif_id->port_id,
				port_info->subif_info[i].bp,
				subif_id->subif, flags);
	} else {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "register subif failed for no matched vap\n");
	}
EXIT:
	return res;
}

int32_t dp_deregister_subif_private(int inst, struct module *owner,
				    struct net_device *dev,
				    char *subif_name, dp_subif_t *subif_id,
				    struct dp_subif_data *data,
				    uint32_t flags)
{
	int res = DP_FAILURE;
	int i, port_id, cqm_port, bp;
	u8 find = 0;
	struct pmac_port_info *port_info;
	struct cbm_dp_en_data cbm_data = {0};
	struct subif_platform_data platfrm_data = {0};

	port_id = subif_id->port_id;
	port_info = &dp_port_info[inst][port_id];
	platfrm_data.subif_data = data;
	platfrm_data.dev = dev;

	DP_DEBUG(DP_DBG_FLAG_REG,
		 "Try to unregister subif=%s with dp_port=%d subif=%d\n",
		 subif_name, subif_id->port_id, subif_id->subif);

	if (port_info->status != PORT_SUBIF_REGISTERED) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "Unregister failed:%s not registered subif!\n",
			 subif_name);
		return res;
	}

	for (i = 0; i < port_info->ctp_max; i++) {
		if (port_info->subif_info[i].subif ==
		    subif_id->subif) {
			find = 1;
			break;
		}
	}
	if (!find)
		return res;

	DP_DEBUG(DP_DBG_FLAG_REG,
		 "Found matched subif: port_id=%d subif=%x vap=%d\n",
		 subif_id->port_id, subif_id->subif, i);
	if (port_info->subif_info[i].netif != dev) {
		/* device not match. Maybe it is unexplicit logical dev */
		res = del_logic_dev(inst, &port_info->subif_info[i].logic_dev,
				    dev, flags);
		return res;
	}
	/* reset LCT port */
	if (data->flag_ops & DP_F_DATA_LCT_SUBIF)
		port_info->lct_idx = 0;
	if (!list_empty(&port_info->subif_info[i].logic_dev)) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "Unregister fail: logic_dev of %s not empty yet!\n",
			 subif_name);
		return res;
	}
	cqm_port = port_info->subif_info[i].cqm_deq_port;
	bp = port_info->subif_info[i].bp;
	/* reset mib, flag, and others */
	memset(&port_info->subif_info[i].mib, 0,
	       sizeof(port_info->subif_info[i].mib));
	port_info->subif_info[i].flags = 0;
	port_info->num_subif--;
	if (dp_port_prop[inst].info.subif_platform_set(inst,
						       port_id, i,
						       &platfrm_data, flags)) {
		PR_ERR("subif_platform_set fail\n");
		/*return res;*/
	}

	if (!dp_deq_port_tbl[inst][cqm_port].ref_cnt) {
		/*delete all queues which may created by PPA or other apps*/
		struct dp_node_alloc port_node;

		port_node.inst = inst;
		port_node.dp_port = port_id;
		port_node.type = DP_NODE_PORT;
		port_node.id.cqm_deq_port = cqm_port;
		dp_node_children_free(&port_node, 0);
		/*disable cqm port */
		cbm_data.dp_inst = inst;
		cbm_data.cbm_inst = dp_port_prop[inst].cbm_inst;
		cbm_data.deq_port = cqm_port;
		if (!port_info->num_subif) {
			port_info->status = PORT_DEV_REGISTERED;
			cbm_data.dma_chnl_init = 1; /*to disable DMA */
		}
		if (cbm_dp_enable(owner, port_id, &cbm_data,
				  CBM_PORT_F_DISABLE, port_info->alloc_flags)) {
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "cbm_dp_disable fail:port=%d subix=%d %s=%d\n",
				 port_id, i,
				 "dma_chnl_init", cbm_data.dma_chnl_init);

			return res;
		}
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "cbm_dp_disable ok:port=%d subix=%d cqm_port=%d\n",
			 port_id, i, cqm_port);
	}
	/* for pmapper and non-pmapper both
	 *  1)for PRX300, dev is managed at its HAL level
	 *  2)for GRX350, bp/dev should be always zero/NULL at present
	 *        before adapting to new datapath framework
	 */
	if (!dp_bp_dev_tbl[inst][bp].dev) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "dp_inst_del_dev for %s inst=%d bp=%d\n",
			 dev->name, inst, bp);
		dp_inst_del_dev(dev, subif_name, inst, port_id,
				subif_id->subif, 0);
	}

	DP_DEBUG(DP_DBG_FLAG_REG, "  dp_port=%d subif=%d cqm_port=%d\n",
		 subif_id->port_id, subif_id->subif, cqm_port);
	res = DP_SUCCESS;

	return res;
}

/*Note: For same owner, it should be in the same HW instance
 *          since dp_register_dev/subif no dev_port information at all,
 *          at the same time, dev is optional and can be NULL
 */

int32_t dp_alloc_port(struct module *owner, struct net_device *dev,
		      u32 dev_port, int32_t port_id,
		      dp_pmac_cfg_t *pmac_cfg, uint32_t flags)
{
	struct dp_port_data data = {0};

	return dp_alloc_port_ext(0, owner, dev, dev_port, port_id, pmac_cfg,
				 &data, flags);
}
EXPORT_SYMBOL(dp_alloc_port);

int32_t dp_alloc_port_ext(int inst, struct module *owner,
			  struct net_device *dev,
			  u32 dev_port, int32_t port_id,
			  dp_pmac_cfg_t *pmac_cfg,
			  struct dp_port_data *data, uint32_t flags)
{
	int res;
	struct dp_port_data tmp_data = {0};

	if (unlikely(!dp_init_ok)) {
		DP_LIB_LOCK(&dp_lock);
		if (!try_walkaround) {
			try_walkaround = 1;
			dp_probe(NULL); /*workaround to re-init */
		}
		DP_LIB_UNLOCK(&dp_lock);
		if (!dp_init_ok) {
			PR_ERR("dp_alloc_port fail: datapath can't init\n");
			return DP_FAILURE;
		}
	}
	if (!dp_port_prop[0].valid) {
		PR_ERR("No Valid datapath instance yet?\n");
		return DP_FAILURE;
	}
	if (!data)
		data = &tmp_data;
	DP_LIB_LOCK(&dp_lock);
	res = dp_alloc_port_private(inst, owner, dev, dev_port,
				    port_id, pmac_cfg, data, flags);
	DP_LIB_UNLOCK(&dp_lock);
	if (inst) /* only inst zero need ACA workaround */
		return res;

#ifdef CONFIG_LTQ_DATAPATH_ACA_CSUM_WORKAROUND
	/*For VRX518, it will always carry DP_F_FAST_WLAN flag for
	 * ACA HW resource purpose in CBM
	 */
	if ((res > 0) &&
	    (flags & DP_F_FAST_WLAN) &&
	    (aca_portid < 0)) {
		dp_subif_t subif_id;
		#define ACA_CSUM_NAME "aca_csum"
		strcpy(aca_owner.name, ACA_CSUM_NAME);
		strcpy(aca_dev.name, ACA_CSUM_NAME);
		aca_portid = dp_alloc_port(&aca_owner, &aca_dev,
					   0, 0, NULL, DP_F_CHECKSUM);
		if (aca_portid <= 0) {
			PR_ERR("dp_alloc_port failed for %s\n", ACA_CSUM_NAME);
			return res;
		}
		if (dp_register_dev(&aca_owner, aca_portid,
				    NULL, DP_F_CHECKSUM)) {
			PR_ERR("dp_register_dev fail for %s\n", ACA_CSUM_NAME);
			return res;
		}
		subif_id.port_id = aca_portid;
		subif_id.subif = -1;
		if (dp_register_subif(&aca_owner, &aca_dev,
				      ACA_CSUM_NAME, &subif_id,
				      DP_F_CHECKSUM)) {
			PR_ERR("dp_register_subif fail for %s\n",
			       ACA_CSUM_NAME);
			return res;
		}
	}
#endif
	return res;
}
EXPORT_SYMBOL(dp_alloc_port_ext);

int32_t dp_register_dev(struct module *owner, uint32_t port_id,
			dp_cb_t *dp_cb, uint32_t flags)
{
	int inst = dp_get_inst_via_module(owner, port_id, 0);
	struct dp_dev_data data = {0};

	if (inst < 0) {
		PR_ERR("dp_register_dev not valid module %s\n", owner->name);
		return -1;
	}

	return dp_register_dev_ext(inst, owner, port_id, dp_cb, &data, flags);
}
EXPORT_SYMBOL(dp_register_dev);

int32_t dp_register_dev_ext(int inst, struct module *owner, uint32_t port_id,
			    dp_cb_t *dp_cb, struct dp_dev_data *data,
			    uint32_t flags)
{
	int res = DP_FAILURE;
	struct pmac_port_info *port_info;

	if (unlikely(!dp_init_ok)) {
		PR_ERR("dp_register_dev failed for datapath not init yet\n");
		return DP_FAILURE;
	}

	if (!port_id || !owner || (port_id >= MAX_DP_PORTS)) {
		if ((inst < 0) || (inst >= DP_MAX_INST))
			DP_DEBUG(DP_DBG_FLAG_REG, "wrong inst=%d\n", inst);
		else if (!owner)
			DP_DEBUG(DP_DBG_FLAG_REG, "owner NULL\n");
		else
			DP_DEBUG(DP_DBG_FLAG_REG, "Wrong port_id:%d\n",
				 port_id);

		return DP_FAILURE;
	}
	port_info = &dp_port_info[inst][port_id];

	DP_LIB_LOCK(&dp_lock);
	if (flags & DP_F_DEREGISTER) {	/*de-register */
		if (port_info->status != PORT_DEV_REGISTERED) {
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "No or %s to de-register for num_subif=%d\n",
				 owner->name,
				 port_info->num_subif);
		} else if (port_info->status ==
			   PORT_DEV_REGISTERED) {
			port_info->status = PORT_ALLOCATED;
			res = DP_SUCCESS;
		} else {
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "No for %s to de-register for unknown status:%d\n",
				 owner->name, port_info->status);
		}

		DP_LIB_UNLOCK(&dp_lock);
		return res;
	}

	/*register a device */
	if (port_info->status != PORT_ALLOCATED) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "No de-register for %s for unknown status:%d\n",
			 owner->name, port_info->status);
		return DP_FAILURE;
	}

	if (port_info->owner != owner) {
		DP_DEBUG(DP_DBG_FLAG_REG, "No matched owner(%s):%p->%p\n",
			 owner->name, owner, port_info->owner);
		DP_LIB_UNLOCK(&dp_lock);
		return res;
	}
	port_info->status = PORT_DEV_REGISTERED;
	if (dp_cb)
		port_info->cb = *dp_cb;

	DP_LIB_UNLOCK(&dp_lock);
	return DP_SUCCESS;
}
EXPORT_SYMBOL(dp_register_dev_ext);

/* if subif_id->subif < 0: Dynamic mode
 * else subif is provided by caller itself
 * Note: 1) for register logical device, if DP_F_ALLOC_EXPLICIT_SUBIFID is not
 *       specified, subif will take its base dev's subif.
 *       2) for IPOA/PPPOA, dev is NULL and subif_name is dummy string.
 *          in this case, dev->name may not be subif_name
 */
int32_t dp_register_subif_ext(int inst, struct module *owner,
			      struct net_device *dev,
			      char *subif_name, dp_subif_t *subif_id,
			      /*device related info*/
			      struct dp_subif_data *data, uint32_t flags)
{
	int res = DP_FAILURE;
	int port_id, f_subif = -1;
	struct pmac_port_info *port_info;
	struct dp_subif_data tmp_data = {0};
	dp_subif_t *subif_id_sync;
	dp_get_netif_subifid_fn_t subifid_fn_t = NULL;

	if (unlikely(!dp_init_ok)) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "dp_register_subif fail for datapath not init yet\n");
		return DP_FAILURE;
	}
	DP_DEBUG(DP_DBG_FLAG_REG,
		 "%s:owner=%s dev=%s %s=%s port_id=%d subif=%d(%s) %s%s\n",
		 (flags & DP_F_DEREGISTER) ?
			"unregister subif:" : "register subif",
		 owner ? owner->name : "NULL",
		 dev ? dev->name : "NULL",
		 "subif_name",
		 subif_name,
		 subif_id->port_id,
		 subif_id->subif,
		 (subif_id->subif < 0) ? "dynamic" : "fixed",
		 (flags & DP_F_SUBIF_LOGICAL) ? "Logical" : "",
		 (flags & DP_F_ALLOC_EXPLICIT_SUBIFID) ?
			"Explicit" : "Non-Explicit");

	if ((!subif_id) || (!subif_id->port_id) || (!owner) ||
	    (subif_id->port_id >= MAX_DP_PORTS) ||
	    (subif_id->port_id <= 0) ||
	    ((inst < 0) || (inst >= DP_MAX_INST))) {
		if (!owner)
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "register subif failed for owner NULL\n");
		else if (!subif_id)
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "register subif failed for NULL subif_id\n");
		else if ((inst < 0) || (inst >= DP_MAX_INST))
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "register subif failed for wrong inst=%d\n",
				 inst);
		else
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "register subif failed port_id=%d or others\n",
				 subif_id->port_id);

		return DP_FAILURE;
	}
	port_id = subif_id->port_id;
	port_info = &dp_port_info[inst][port_id];

	if (((!dev) && !(port_info->alloc_flags & DP_F_FAST_DSL)) ||
	    !subif_name) {
		DP_DEBUG(DP_DBG_FLAG_REG, "Wrong dev=%p, subif_name=%p\n",
			 dev, subif_name);
		return DP_FAILURE;
	}
	if (!data)
		data = &tmp_data;
	DP_LIB_LOCK(&dp_lock);
	if (port_info->owner != owner) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "Unregister subif fail:Not matching:%p(%s)->%p(%s)\n",
			 owner, owner->name, port_info->owner,
			 port_info->owner->name);
		DP_LIB_UNLOCK(&dp_lock);
		return res;
	}

	if (flags & DP_F_DEREGISTER) /*de-register */
		res =
		dp_deregister_subif_private(inst, owner, dev,
					    subif_name,
					    subif_id, data, flags);
	else /*register */
		res =
		dp_register_subif_private(inst, owner, dev,
					  subif_name,
					  subif_id, data, flags);
	if (!(flags & DP_F_SUBIF_LOGICAL))
		subifid_fn_t = port_info->cb.get_subifid_fn;
	
	subif_id_sync = kmalloc(sizeof(*subif_id_sync) * 2, GFP_KERNEL);
	if (!subif_id_sync) {
		PR_ERR("Failed to alloc %d bytes\n",
		       sizeof(*subif_id_sync) * 2);
		return DP_FAILURE;
	}
	memcpy(&subif_id_sync[0], subif_id, sizeof(dp_subif_t));
	memcpy(&subif_id_sync[1], subif_id, sizeof(dp_subif_t));
	dp_sync_subifid(dev, subif_name, subif_id_sync, data, flags, &f_subif);
	DP_LIB_UNLOCK(&dp_lock);
	if (!res)
		dp_sync_subifid_priv(dev, subif_name, subif_id_sync, data,
				     flags, subifid_fn_t, &f_subif);
	kfree(subif_id_sync);
	return res;
}
EXPORT_SYMBOL(dp_register_subif_ext);

int32_t dp_register_subif(struct module *owner, struct net_device *dev,
			  char *subif_name, dp_subif_t *subif_id,
			  uint32_t flags)
{
	int inst;
	struct dp_subif_data data = {0};

	if ((!subif_id) || (!subif_id->port_id) || (!owner) ||
	    (subif_id->port_id >= MAX_DP_PORTS) ||
	    (subif_id->port_id <= 0)) {
		if (!owner)
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "register subif fail for owner NULL\n");
		else if (!subif_id)
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "register subif fail for NULL subif_id\n");
		else
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "register subif fail port_id=%d or others\n",
				 subif_id->port_id);

		return DP_FAILURE;
	}
	inst = dp_get_inst_via_module(owner, subif_id->port_id, 0);
	if (inst < 0) {
		PR_ERR("wrong inst for owner=%s with ep=%d\n", owner->name,
		       subif_id->port_id);
		return DP_FAILURE;
	}
	return dp_register_subif_ext(inst, owner, dev, subif_name,
				     subif_id, &data, flags);
}
EXPORT_SYMBOL(dp_register_subif);

int32_t dp_get_netif_subifid(struct net_device *netif, struct sk_buff *skb,
			     void *subif_data, u8 dst_mac[DP_MAX_ETH_ALEN],
			     dp_subif_t *subif, uint32_t flags)
{
	struct dp_subif_cache *dp_subif;
	u32 idx;
	dp_get_netif_subifid_fn_t subifid_fn_t;
	int res = DP_FAILURE;

	idx = dp_subif_hash(netif);
	//TODO handle DSL case in future
	rcu_read_lock_bh();
	dp_subif = dp_subif_lookup_safe(&dp_subif_list[idx], netif, subif_data);
	if (!dp_subif) {
		PR_ERR("Failed dp_subif_lookup: %s\n",
		       netif ? netif->name : "NULL");
		rcu_read_unlock_bh();
		return res;
	}
	memcpy(subif, &dp_subif->subif, sizeof(*subif));
	subifid_fn_t = dp_subif->subif_fn;
	rcu_read_unlock_bh();
	if (subifid_fn_t) {
		/*subif->subif will be set by callback api itself */
		res =
		    subifid_fn_t(netif, skb, subif_data, dst_mac, subif,
				 flags);
		if (res != 0)
			PR_ERR("get_netif_subifid callback function failed\n");
	} else {
		res = DP_SUCCESS;
	}
	return res;
}
EXPORT_SYMBOL(dp_get_netif_subifid);

/*Note:
 * try to get subif according to netif, skb,vcc,dst_mac.
 * For DLS nas interface, must provide valid subif_data, otherwise set to NULL.
 * Note: subif_data is mainly used for DSL WAN mode, esp ATM.
 * If subif->port_id valid, take it, otherwise search all to get the port_id
 */
int32_t dp_get_netif_subifid_priv(struct net_device *netif, struct sk_buff *skb,
				  void *subif_data,
				  u8 dst_mac[DP_MAX_ETH_ALEN],
				  dp_subif_t *subif, uint32_t flags)
{
	int res = -1;
	int i, k;
	int port_id = -1;
	u16 bport = 0;
	int inst, start, end;
	u8 match = 0;
	u8 num = 0;
	u16 *subifs = NULL;
	u32 *subif_flag = NULL;
	struct logic_dev *tmp = NULL;

	subifs = kmalloc(sizeof(*subifs) * DP_MAX_CTP_PER_DEV,
			 GFP_ATOMIC);
	if (!subifs) {
		PR_ERR("Failed to alloc %d bytes\n",
		       sizeof(*subifs) * DP_MAX_CTP_PER_DEV);
		return res;
	}
	subif_flag = kmalloc(sizeof(*subif_flag) * DP_MAX_CTP_PER_DEV,
			     GFP_ATOMIC);
	if (!subif_flag) {
		PR_ERR("Failed to alloc %d bytes\n",
		       sizeof(*subif_flag) * DP_MAX_CTP_PER_DEV);
		kfree(subifs);
		return res;
	}
	if (!netif && !subif_data) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "dp_get_netif_subifid failed: netif=%p subif_data=%p\n",
			 netif, subif_data);
		goto EXIT;
	}
	if (!subif) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "dp_get_netif_subifid failed:subif=%p\n", subif);
		goto EXIT;
	}
	if (!netif && subif_data)
		inst = 0;
	else
		inst = dp_get_inst_via_dev(netif, NULL, 0);
	start = 0;
	end = dp_port_prop[inst].info.cap.max_num_dp_ports;
#ifdef DP_FAST_SEARCH /* Don't enable since need to be back-compatible */
	if ((subif->port_id < MAX_DP_PORTS) && (subif->port_id > 0)) {
		start = subif->port_id;
		end = start + 1;
	}
#endif
	subif->flag_pmapper = 0;
	for (k = start; k < end; k++) {
		if (dp_port_info[inst][k].status != PORT_SUBIF_REGISTERED)
			continue;

		/*Workaround for VRX318 */
		if (subif_data &&
		    (dp_port_info[inst][k].alloc_flags & DP_F_FAST_DSL)) {
			/*VRX318 should overwritten them later if necessary */
			port_id = k;
			break;
		}

		/*search sub-interfaces/VAP */
		for (i = 0; i < dp_port_info[inst][k].ctp_max; i++) {
			if (!dp_port_info[inst][k].subif_info[i].flags)
				continue;
			if (dp_port_info[inst][k].subif_info[i].ctp_dev ==
				netif) { /*for PON pmapper case*/
				match = 1;
				port_id = k;
				if (num > 0) {
					PR_ERR("Multiple same ctp_dev exist\n");
					goto EXIT;
				}
				subifs[num] = PORT_SUBIF(inst, k, i, subif);
				subif_flag[num] = PORT_SUBIF(inst, k, i,
							subif_flag);
				bport = PORT_SUBIF(inst, k, i, bp);
				subif->flag_bp = 0;
				num++;
				break;
			}

			if (dp_port_info[inst][k].subif_info[i].netif ==
			    netif) {
				if ((subif->port_id > 0) &&
				    (subif->port_id != k)) {
					DP_DEBUG(DP_DBG_FLAG_REG,
						 "dp_get_netif_subifid portid not match:%d expect %d\n",
						 subif->port_id, k);
				} else {
					match = 1;
					subif->flag_bp = 1;
					port_id = k;
					if (num >= DP_MAX_CTP_PER_DEV) {
						PR_ERR("%s: Why CTP over %d\n",
						       netif ? netif->name : "",
						       DP_MAX_CTP_PER_DEV);
						goto EXIT;
					}
					/* some dev may have multiple
					 * subif,like pon
					 */
					subifs[num] = PORT_SUBIF(inst, k, i,
								 subif);
					subif_flag[num] = PORT_SUBIF(inst, k, i,
								subif_flag);
					if (dp_port_info[inst][k].subif_info[i].
						ctp_dev) {
						subif->flag_pmapper = 1;
					}
					bport = PORT_SUBIF(inst, k, i, bp);
					if (num &&
					    (bport != dp_port_info[inst][k].
					     subif_info[i].bp)) {
						PR_ERR("%s:Why many bp:%d %d\n",
						       netif ? netif->name : "",
						       dp_port_info[inst][k].
							  subif_info[i].bp,
						       bport);
						goto EXIT;
					}
					num++;
				}
			}
			/*continue search non-explicate logical device */
			list_for_each_entry(tmp,
					    &dp_port_info[inst][k].
					    subif_info[i].logic_dev,
					    list) {
				if (tmp->dev == netif) {
					subif->subif_num = 1;
					subif->subif_list[0] = tmp->ctp;
					subif->inst = inst;
					subif->port_id = k;
					subif->bport = tmp->bp;
					res = 0;
					/*note: logical device no callback */
					goto EXIT;
				}
			}
		}
		if (match)
			break;
	}

	if (port_id < 0) {
		if (subif_data)
			DP_DEBUG(DP_DBG_FLAG_DBG,
				 "dp_get_netif_subifid failed with subif_data %p\n",
				 subif_data);
		else /*netif must should be valid */
			DP_DEBUG(DP_DBG_FLAG_DBG,
				 "dp_get_netif_subifid failed: %s\n",
				 netif->name);

		goto EXIT;
	}
	subif->inst = inst;
	subif->port_id = port_id;
	subif->bport = bport;
	subif->alloc_flag = dp_port_info[inst][port_id].alloc_flags;
	subif->subif_num = num;
	for (i = 0; i < num; i++) {
		subif->subif_list[i] = subifs[i];
		subif->subif_flag[i] = subif_flag[i];
	}
	res = 0;
EXIT:
	kfree(subifs);
	kfree(subif_flag);
	return res;
}

#ifdef CONFIG_LTQ_DATAPATH_CPUFREQ
int update_coc_up_sub_module(int new_state,
			     int old_state, uint32_t flag)
{
	int i;
	dp_coc_confirm_stat fn;
	int inst = 0;

	for (i = 0; i < dp_port_prop[inst].info.cap.max_num_dp_ports; i++) {
		fn = dp_port_info[inst][i].cb.dp_coc_confirm_stat_fn;

		if (fn)
			fn(new_state, old_state, flag);
	}

	return 0;
}
#endif

/* return DP_SUCCESS -- found
 * return DP_FAILURE -- not found
 */
int dp_get_port_subitf_via_dev_private(struct net_device *dev,
				       dp_subif_t *subif)
{
	int i, j;
	int inst;

	inst = dp_get_inst_via_dev(dev, NULL, 0);
	for (i = 0; i < dp_port_prop[inst].info.cap.max_num_dp_ports; i++)
		for (j = 0; j < dp_port_info[inst][i].ctp_max; j++) {
			if (!dp_port_info[inst][i].subif_info[j].flags)
				continue;
			if (dp_port_info[inst][i].subif_info[j].netif != dev)
				continue;
			subif->port_id = i;
			subif->subif =
				SET_VAP(j,
					PORT_INFO(inst, i, vap_offset),
					PORT_INFO(inst, i, vap_mask));
			subif->inst = inst;
			subif->bport = dp_port_info[inst][i].
				subif_info[j].bp;
			return DP_SUCCESS;
		}
	return DP_FAILURE;
}

int dp_get_port_subitf_via_dev(struct net_device *dev, dp_subif_t *subif)
{
	int res;

	DP_LIB_LOCK(&dp_lock);
	res = dp_get_port_subitf_via_dev_private(dev, subif);
	DP_LIB_UNLOCK(&dp_lock);
	return res;
}
EXPORT_SYMBOL(dp_get_port_subitf_via_dev);

int dp_get_port_subitf_via_ifname_private(char *ifname, dp_subif_t *subif)
{
	int i, j;
	int inst;

	inst = dp_get_inst_via_dev(NULL, ifname, 0);

	for (i = 0; i < dp_port_prop[inst].info.cap.max_num_dp_ports; i++) {
		for (j = 0; j < dp_port_info[inst][i].ctp_max; j++) {
			if (strcmp
			    (dp_port_info[inst][i].subif_info[j].device_name,
			     ifname) == 0) {
				subif->port_id = i;
				subif->subif =
					SET_VAP(j,
						PORT_INFO(inst, i, vap_offset),
						PORT_INFO(inst, i, vap_mask));
				subif->inst = inst;
				subif->bport = dp_port_info[inst][i].
					subif_info[j].bp;
				return DP_SUCCESS;
			}
		}
	}

	return DP_FAILURE;
}

int dp_get_port_subitf_via_ifname(char *ifname, dp_subif_t *subif)
{
	int res;
	struct net_device *dev;

	if (!ifname)
		return -1;
	dev = dev_get_by_name(&init_net, ifname);
	if (!dev)
		return -1;
	res = dp_get_port_subitf_via_dev(dev, subif);
	dev_put(dev);
	return res;
}
EXPORT_SYMBOL(dp_get_port_subitf_via_ifname);

int32_t dp_check_if_netif_fastpath_fn(struct net_device *netif,
				      dp_subif_t *subif, char *ifname,
				      uint32_t flags)
{
	int res = 1;
	dp_subif_t tmp_subif = { 0 };

	DP_LIB_LOCK(&dp_lock);
	if (unlikely(!dp_init_ok)) {
		PR_ERR("dp_check_if_netif_fastpath_fn fail: dp not ready\n");
		return DP_FAILURE;
	}
	if (subif) {
		tmp_subif = *subif;
		tmp_subif.inst = 0;
	} else if (netif) {
		dp_get_port_subitf_via_dev_private(netif, &tmp_subif);
	} else if (ifname) {
		dp_get_port_subitf_via_ifname_private(ifname, &tmp_subif);
	}

	if (tmp_subif.port_id <= 0 && tmp_subif.port_id >=
	    dp_port_prop[tmp_subif.inst].info.cap.max_num_dp_ports)
		res = 0;
	else if (!(dp_port_info[tmp_subif.inst][tmp_subif.port_id].alloc_flags &
		 (DP_F_FAST_DSL || DP_F_FAST_ETH_LAN ||
		 DP_F_FAST_ETH_WAN || DP_F_FAST_WLAN)))
		res = 0;

	DP_LIB_UNLOCK(&dp_lock);
	return res;
}
EXPORT_SYMBOL(dp_check_if_netif_fastpath_fn);

struct module *dp_get_module_owner(int ep)
{
	int inst = 0; /*here hardcode for PPA only */

	if (unlikely(!dp_init_ok)) {
		PR_ERR
		    ("dp_get_module_owner failed for datapath not init yet\n");
		return NULL;
	}

	if ((ep >= 0) && (ep < dp_port_prop[inst].info.cap.max_num_dp_ports))
		return dp_port_info[inst][ep].owner;

	return NULL;
}
EXPORT_SYMBOL(dp_get_module_owner);

/*if subif->vap == -1, it means all vap */
void dp_clear_mib(dp_subif_t *subif, uint32_t flag)
{
	int i, j, start_vap, end_vap;
	dp_reset_mib_fn_t reset_mib_fn;
	struct pmac_port_info *port_info;

	if (!subif || (subif->port_id >= MAX_DP_PORTS) ||
	    (subif->port_id < 0)) {
		DP_DEBUG(DP_DBG_FLAG_DBG, "dp_clear_mib Wrong subif\n");
		return;
	}

	i = subif->port_id;
	port_info = &dp_port_info[subif->inst][i];

	if (subif->subif == -1) {
		start_vap = 0;
		end_vap = port_info->ctp_max;
	} else {
		start_vap = GET_VAP(subif->subif, port_info->vap_offset,
				    port_info->vap_mask);
		end_vap = start_vap + 1;
	}

	for (j = start_vap; j < end_vap; j++) {
		STATS_SET(port_info->tx_err_drop, 0);
		STATS_SET(port_info->rx_err_drop, 0);
		memset(&port_info->subif_info[j].mib, 0,
		       sizeof(port_info->subif_info[j].mib));
		reset_mib_fn = port_info->cb.reset_mib_fn;

		if (reset_mib_fn)
			reset_mib_fn(subif, 0);
	}
}

void dp_clear_all_mib_inside(uint32_t flag)
{
	dp_subif_t subif;
	int i;

	memset(&subif, 0, sizeof(subif));
	for (i = 0; i < MAX_DP_PORTS; i++) {
		subif.port_id = i;
		subif.subif = -1;
		dp_clear_mib(&subif, flag);
	}
}

int dp_get_drv_mib(dp_subif_t *subif, dp_drv_mib_t *mib, uint32_t flag)
{
	dp_get_mib_fn_t get_mib_fn;
	dp_drv_mib_t tmp;
	int i, vap;
	struct pmac_port_info *port_info;

	if (unlikely(!dp_init_ok)) {
		DP_DEBUG(DP_DBG_FLAG_DBG,
			 "dp_get_drv_mib failed for datapath not init yet\n");
		return DP_FAILURE;
	}

	if (!subif || !mib)
		return -1;
	memset(mib, 0, sizeof(*mib));
	port_info = &dp_port_info[subif->inst][subif->port_id];
	vap = GET_VAP(subif->subif, port_info->vap_offset,
		      port_info->vap_mask);
	get_mib_fn = port_info->cb.get_mib_fn;

	if (!get_mib_fn)
		return -1;

	if (!(flag & DP_F_STATS_SUBIF)) {
		/*get all VAP's  mib counters if it is -1 */
		for (i = 0; i < port_info->ctp_max; i++) {
			if (!port_info->subif_info[i].flags)
				continue;

			subif->subif =
			    port_info->subif_info[i].subif;
			memset(&tmp, 0, sizeof(tmp));
			get_mib_fn(subif, &tmp, flag);
			mib->rx_drop_pkts += tmp.rx_drop_pkts;
			mib->rx_error_pkts += tmp.rx_error_pkts;
			mib->tx_drop_pkts += tmp.tx_drop_pkts;
			mib->tx_error_pkts += tmp.tx_error_pkts;
		}
	} else {
		if (port_info->subif_info[vap].flags)
			get_mib_fn(subif, mib, flag);
	}

	return 0;
}

int dp_get_netif_stats(struct net_device *dev, dp_subif_t *subif_id,
		       struct rtnl_link_stats64 *stats, uint32_t flags)
{
	dp_subif_t subif;
	int res;
	int (*get_mib)(dp_subif_t *subif_id, void *priv,
		       struct rtnl_link_stats64 *stats,
		       uint32_t flags);

	if (subif_id) {
		subif = *subif_id;
	} else if (dev) {
		res = dp_get_port_subitf_via_dev(dev, &subif);
		if (res) {
			DP_DEBUG(DP_DBG_FLAG_MIB,
				 "dp_get_netif_stats fail:%s not registered yet to datapath\n",
				 dev->name);
			return DP_FAILURE;
		}
	} else {
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "dp_get_netif_stats: dev/subif_id both NULL\n");
		return DP_FAILURE;
	}
	get_mib = dp_port_prop[subif.inst].info.dp_get_port_vap_mib;
	if (!get_mib)
		return DP_FAILURE;

	return get_mib(&subif, NULL, stats, flags);
}
EXPORT_SYMBOL(dp_get_netif_stats);

int dp_clear_netif_stats(struct net_device *dev, dp_subif_t *subif_id,
			 uint32_t flag)
{
	dp_subif_t subif;
	int (*clear_netif_mib_fn)(dp_subif_t *subif, void *priv, u32 flag);
	int i;

	if (subif_id) {
		clear_netif_mib_fn =
			dp_port_prop[subif_id->inst].info.dp_clear_netif_mib;
		if (!clear_netif_mib_fn)
			return -1;
		return clear_netif_mib_fn(subif_id, NULL, flag);
	}
	if (dev) {
		if (dp_get_port_subitf_via_dev(dev, &subif)) {
			DP_DEBUG(DP_DBG_FLAG_MIB,
				 "%s not register to dp_clear_netif_stats\n",
				 dev->name);
			return -1;
		}
		clear_netif_mib_fn =
			dp_port_prop[subif.inst].info.dp_clear_netif_mib;
		if (!clear_netif_mib_fn)
			return -1;
		return clear_netif_mib_fn(&subif, NULL, flag);
	}
	/*clear all */
	for (i = 0; i < DP_MAX_INST; i++) {
		clear_netif_mib_fn =
			dp_port_prop[i].info.dp_clear_netif_mib;
		if (!clear_netif_mib_fn)
			continue;
		clear_netif_mib_fn(NULL, NULL, flag);
	}
	return 0;
}
EXPORT_SYMBOL(dp_clear_netif_stats);

int dp_pmac_set(int inst, u32 port, dp_pmac_cfg_t *pmac_cfg)
{
	int (*dp_pmac_set_fn)(int inst, u32 port, dp_pmac_cfg_t *pmac_cfg);

	if (inst >= DP_MAX_INST) {
		PR_ERR("Wrong inst(%d) id: should less than %d\n",
		       inst, DP_MAX_INST);
		return DP_FAILURE;
	}
	dp_pmac_set_fn = dp_port_prop[inst].info.dp_pmac_set;
	if (!dp_pmac_set_fn)
		return DP_FAILURE;
	return dp_pmac_set_fn(inst, port, pmac_cfg);
}
EXPORT_SYMBOL(dp_pmac_set);

/*\brief Datapath Manager Pmapper Configuration Set
 *\param[in] dev: network device point to set pmapper
 *\param[in] mapper: buffer to get pmapper configuration
 *\param[in] flag: reserve for future
 *\return Returns 0 on succeed and -1 on failure
 *\note  for pcp mapper case, all 8 mapping must be configured properly
 *       for dscp mapper case, all 64 mapping must be configured properly
 *       def ctp will match non-vlan and non-ip case
 *	For drop case, assign CTP value == DP_PMAPPER_DISCARD_CTP
 */
int dp_set_pmapper(struct net_device *dev, struct dp_pmapper *mapper, u32 flag)
{
	int inst, ret, bport, i;
	dp_subif_t subif = {0};
	struct dp_pmapper *map = NULL;
	int res = DP_FAILURE;

	if (!dev || !mapper) {
		PR_ERR("dev or mapper is NULL\n");
		return DP_FAILURE;
	}

	if (unlikely(!dp_init_ok)) {
		PR_ERR("Failed for datapath not init yet\n");
		return DP_FAILURE;
	}
	if (mapper->mode >= DP_PMAP_MAX) {
		PR_ERR("mapper->mode(%d) out of range %d\n",
		       mapper->mode, DP_PMAP_MAX);
		return DP_FAILURE;
	}
	/* get the subif from the dev */
	ret = dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0);
	if ((ret == DP_FAILURE) || (subif.flag_pmapper == 0)) {
		PR_ERR("Fail to get subif:dev=%s ret=%d flag_pmap=%d bp=%d\n",
		       dev->name, ret, subif.flag_pmapper, subif.bport);
		return DP_FAILURE;
	}
	inst = subif.inst;
	if (!dp_port_prop[inst].info.dp_set_gsw_pmapper) {
		PR_ERR("Set pmapper is not supported\n");
		return DP_FAILURE;
	}

	bport = subif.bport;
	if (bport >= DP_MAX_BP_NUM) {
		PR_ERR("BP port(%d) out of range %d\n", bport, DP_MAX_BP_NUM);
		return DP_FAILURE;
	}
	map = kmalloc(sizeof(*map), GFP_ATOMIC);
	if (!map) {
		PR_ERR("Failed for kmalloc: %d bytes\n", sizeof(*map));
		return DP_FAILURE;
	}
	memcpy(map, mapper, sizeof(*map));
	if ((mapper->mode == DP_PMAP_PCP) ||
	    (mapper->mode == DP_PMAP_DSCP)) {
		map->mode = GSW_PMAPPER_MAPPING_PCP;
	} else if (mapper->mode == DP_PMAP_DSCP_ONLY) {
		map->mode = GSW_PMAPPER_MAPPING_DSCP;
	} else {
		PR_ERR("Unknown mapper mode: %d\n", map->mode);
		goto EXIT;
	}

	/* workaround in case caller forget to set to default ctp */
	if (mapper->mode == DP_PMAP_PCP)
		for (i = 0; i < DP_PMAP_DSCP_NUM; i++)
			map->dscp_map[i] = mapper->def_ctp;

	ret = dp_port_prop[inst].info.dp_set_gsw_pmapper(inst, bport,
							 subif.port_id, map,
							 flag);
	if (ret == DP_FAILURE) {
		PR_ERR("Failed to set mapper\n");
		goto EXIT;
	}

	/* update local table for pmapper */
	dp_bp_dev_tbl[inst][bport].def_ctp = map->def_ctp;
	dp_bp_dev_tbl[inst][bport].mode = mapper->mode; /* original mode */
	for (i = 0; i < DP_PMAP_PCP_NUM; i++)
		dp_bp_dev_tbl[inst][bport].pcp[i] = map->pcp_map[i];
	for (i = 0; i < DP_PMAP_DSCP_NUM; i++)
		dp_bp_dev_tbl[inst][bport].dscp[i] = map->dscp_map[i];
	res = DP_SUCCESS;
EXIT:
	kfree(map);
	return res;
}
EXPORT_SYMBOL(dp_set_pmapper);

/*\brief Datapath Manager Pmapper Configuration Get
 *\param[in] dev: network device point to set pmapper
 *\param[out] mapper: buffer to get pmapper configuration
 *\param[in] flag: reserve for future
 *\return Returns 0 on succeed and -1 on failure
 *\note  for pcp mapper case, all 8 mapping must be configured properly
 *       for dscp mapper case, all 64 mapping must be configured properly
 *       def ctp will match non-vlan and non-ip case
 *	 For drop case, assign CTP value == DP_PMAPPER_DISCARD_CTP
 */
int dp_get_pmapper(struct net_device *dev, struct dp_pmapper *mapper, u32 flag)
{
	int inst, ret, bport;
	dp_subif_t subif = {0};

	if (!dev || !mapper) {
		PR_ERR("The parameter dev or mapper can not be NULL\n");
		return DP_FAILURE;
	}

	if (unlikely(!dp_init_ok)) {
		PR_ERR("Failed for datapath not init yet\n");
		return DP_FAILURE;
	}

	/*get the subif from the dev*/
	ret = dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0);
	if (ret == DP_FAILURE || subif.flag_pmapper == 0) {
		PR_ERR("Can not get the subif from the dev\n");
		return DP_FAILURE;
	}
	inst = subif.inst;
	if (!dp_port_prop[inst].info.dp_get_gsw_pmapper) {
		PR_ERR("Get pmapper is not supported\n");
		return DP_FAILURE;
	}

	bport = subif.bport;
	if (bport > DP_MAX_BP_NUM) {
		PR_ERR("BP port(%d) out of range %d\n", bport, DP_MAX_BP_NUM);
		return DP_FAILURE;
	}
	/* init the subif into the dp_port_info*/
	/* call the switch api to get the HW*/
	ret = dp_port_prop[inst].info.dp_get_gsw_pmapper(inst, bport,
							 subif.port_id, mapper,
							 flag);
	if (ret == DP_FAILURE) {
		PR_ERR("Failed to get mapper\n");
		return DP_FAILURE;
	}
	return ret;
}
EXPORT_SYMBOL(dp_get_pmapper);

int32_t dp_rx(struct sk_buff *skb, uint32_t flags)
{
	struct sk_buff *next;
	int res = -1;

	if (unlikely(!dp_init_ok)) {
		while (skb) {
			next = skb->next;
			skb->next = 0;
			dev_kfree_skb_any(skb);
			skb = next;
		}
	}

	while (skb) {
		next = skb->next;
		skb->next = 0;
		res = dp_rx_one_skb(skb, flags);
		skb = next;
	}

	return res;
}
EXPORT_SYMBOL(dp_rx);

int dp_lan_wan_bridging(int port_id, struct sk_buff *skb)
{
	dp_subif_t subif;
	struct net_device *dev;
	static int lan_port = 4;
	int inst = 0;

	if (!skb)
		return DP_FAILURE;

	skb_pull(skb, 8);	/*remove pmac */

	memset(&subif, 0, sizeof(subif));
	if (port_id == 15) {
		/*recv from WAN and forward to LAN via lan_port */
		subif.port_id = lan_port;	/*send to last lan port */
		subif.subif = 0;
	} else if (port_id <= 6) { /*recv from LAN and forward to WAN */
		subif.port_id = 15;
		subif.subif = 0;
		lan_port = port_id;	/*save lan port id */
	} else {
		dev_kfree_skb_any(skb);
		return DP_FAILURE;
	}

	dev = dp_port_info[inst][subif.port_id].subif_info[0].netif;

	if (!dp_port_info[inst][subif.port_id].subif_info[0].flags || !dev) {
		dev_kfree_skb_any(skb);
		return DP_FAILURE;
	}

	((struct dma_tx_desc_1 *)&skb->DW1)->field.ep = subif.port_id;
	((struct dma_tx_desc_0 *)&skb->DW0)->field.dest_sub_if_id =
	    subif.subif;

	dp_xmit(dev, &subif, skb, skb->len, 0);
	return DP_SUCCESS;
}

static void rx_dbg(u32 f, struct sk_buff *skb, struct dma_rx_desc_0 *desc0,
		   struct dma_rx_desc_1 *desc1, struct dma_rx_desc_2 *desc2,
		   struct dma_rx_desc_3 *desc3, unsigned char *parser,
		   struct pmac_rx_hdr *pmac, int paser_exist)
{
	int inst = 0;

	DP_DEBUG(DP_DBG_FLAG_DUMP_RX,
		 "\ndp_rx:skb->data=%p Loc=%x offset=%d skb->len=%d\n",
		 skb->data, desc2->field.data_ptr,
		 desc3->field.byte_offset, skb->len);
	if ((f) & DP_DBG_FLAG_DUMP_RX_DATA)
		dp_dump_raw_data(skb->data,
				 (skb->len >
				  (print_len)) ? skb->len : (print_len),
				 "Original Data");
	DP_DEBUG(DP_DBG_FLAG_DUMP_RX, "parse hdr size = %d\n",
		 paser_exist);
	if ((f) & DP_DBG_FLAG_DUMP_RX_DESCRIPTOR)
		dp_port_prop[inst].info.dump_rx_dma_desc(desc0, (desc1),
			desc2, desc3);
	if (paser_exist && (dp_dbg_flag & DP_DBG_FLAG_DUMP_RX_PASER))
		dump_parser_flag(parser);
	if ((f) & DP_DBG_FLAG_DUMP_RX_PMAC)
		dp_port_prop[inst].info.dump_rx_pmac(pmac);
}

#define PRINT_INTERVAL  (5 * HZ) /* 5 seconds */
unsigned long dp_err_interval = PRINT_INTERVAL;
static void rx_dbg_zero_port(struct sk_buff *skb, struct dma_rx_desc_0 *desc0,
			     struct dma_rx_desc_1 *desc1,
			     struct dma_rx_desc_2 *desc2,
			     struct dma_rx_desc_3 *desc3,
			     unsigned char *parser,
			     struct pmac_rx_hdr *pmac, int paser_exist,
			     u32 ep, u32 port_id, int vap)
{
	int inst = 0;
	static unsigned long last;

	if (!dp_dbg_err) /*bypass dump */
		return;
	if (time_before(jiffies, last + dp_err_interval))
		/* not print in order to keep console not busy */
		return;
	last = jiffies;
	DP_DEBUG(-1, "%s=%d vap=%d\n",
		 (ep) ? "ep" : "port_id", port_id, vap);
	PR_ERR("\nDrop for ep and source port id both zero ??\n");
	dp_port_prop[inst].info.dump_rx_dma_desc(desc0, desc1, desc2, desc3);

	if (paser_exist)
		dump_parser_flag(parser);
	if (pmac)
		dp_port_prop[inst].info.dump_rx_pmac(pmac);
	dp_dump_raw_data((char *)(skb->data),
			 (skb->len >
			  print_len) ? skb->len : print_len,
			 "Recv Data");
}

/* clone skb to send one copy to lct dev for multicast/broadcast
 * otherwise for unicast send only to lct device
 * return 0 - Caller will not proceed handling i.e. for unicast do rx only for
 *	      LCT port
 *	  1 - Caller continue to handle rx for other device
 */
static int dp_handle_lct(struct pmac_port_info *dp_port,
			 struct sk_buff *skb, dp_rx_fn_t rx_fn)
{
	struct sk_buff *lct_skb;
	int vap, ret;

	vap = dp_port->lct_idx;
	skb->dev = dp_port->subif_info[vap].netif;
	if (skb->data[PMAC_SIZE] & 0x1) {
		/* multicast/broadcast */
		DP_DEBUG(DP_DBG_FLAG_PAE, "LCT mcast or broadcast\n");
		lct_skb = skb_clone(skb, GFP_ATOMIC);
		if (!lct_skb) {
			PR_ERR("LCT mcast/bcast skb clone fail\n");
			return -1;
		}
		lct_skb->dev = dp_port->subif_info[vap].netif;
		UP_STATS(dp_port->subif_info[vap].mib.rx_fn_rxif_pkt);
		DP_DEBUG(DP_DBG_FLAG_PAE, "pkt sent lct(%s) ret(%d)\n",
			 lct_skb->dev->name ? lct_skb->dev->name : "NULL",
			 ret);
		rx_fn(lct_skb->dev, NULL, lct_skb, lct_skb->len);
		return 1;
	} else if (memcmp(skb->data + PMAC_SIZE, skb->dev->dev_addr, 6) == 0) {
		/* unicast */
		DP_DEBUG(DP_DBG_FLAG_PAE, "LCT unicast\n");
		DP_DEBUG(DP_DBG_FLAG_PAE, "unicast pkt sent lct(%s) ret(%d)\n",
				 skb->dev->name ? skb->dev->name : "NULL", ret);
		rx_fn(skb->dev, NULL, skb, skb->len);
		UP_STATS(dp_port->subif_info[vap].mib.rx_fn_rxif_pkt);
		return 0;
	}
	return 1;
}

#define DP_TS_HDRLEN	10

static inline int32_t dp_rx_one_skb(struct sk_buff *skb, uint32_t flags)
{
	int res = DP_SUCCESS;
	struct dma_rx_desc_0 *desc_0 = (struct dma_rx_desc_0 *)&skb->DW0;
	struct dma_rx_desc_1 *desc_1 = (struct dma_rx_desc_1 *)&skb->DW1;
	struct dma_rx_desc_2 *desc_2 = (struct dma_rx_desc_2 *)&skb->DW2;
	struct dma_rx_desc_3 *desc_3 = (struct dma_rx_desc_3 *)&skb->DW3;
	struct pmac_rx_hdr *pmac;
	unsigned char *parser = NULL;
	int rx_tx_flag = 0;	/*0-rx, 1-tx */
	u32 ep = desc_1->field.ep;	/* ep: 0 -15 */
	int vap; /*vap: 0-15 */
	int paser_exist;
	u32 port_id = ep; /*same with ep now, later set to sspid if ep is 0 */
	struct net_device *dev;
	dp_rx_fn_t rx_fn;
	char decryp = 0;
	u8 inst = 0;
	struct pmac_port_info *dp_port;
	struct mac_ops *ops;
	int ret_lct = 1;

	dp_port = &dp_port_info[inst][0];
	if (!skb) {
		PR_ERR("skb NULL\n");
		return DP_FAILURE;
	}
	if (!skb->data) {
		PR_ERR("skb->data NULL\n");
		return DP_FAILURE;
	}

	paser_exist = parser_enabled(port_id, desc_1);
	if (paser_exist)
		parser = skb->data;
	pmac = (struct pmac_rx_hdr *)(skb->data + paser_exist);

	if (unlikely(dp_dbg_flag))
		rx_dbg(dp_dbg_flag, skb, desc_0, desc_1, desc_2,
		       desc_3, parser, pmac, paser_exist);
	if (paser_exist) {
		skb_pull(skb, paser_exist);	/*remove parser */
#if IS_ENABLED(CONFIG_PPA_API_SW_FASTPATH)
		skb->mark |= FLG_PPA_PROCESSED;
#endif
	}
#ifdef CONFIG_LTQ_DATAPATH_EXTRA_DEBUG
	/*Sanity check */
	if (unlikely(dp_port_prop[inst].info.not_valid_rx_ep(ep))) {
		DP_DEBUG(DP_DBG_FLAG_DUMP_RX, "Wrong: why ep=%d??\n", ep);
		rx_dbg(-1, skb, desc_0, desc_1, desc_2, desc_3,
		       parser, pmac, paser_exist);
		goto RX_DROP;
	}
	if (unlikely(dp_drop_all_tcp_err && desc_1->field.tcp_err)) {
		DP_DEBUG(DP_DBG_FLAG_DUMP_RX, "\n----dp_rx why tcp_err ???\n");
		rx_dbg(-1, skb, desc_0, desc_1, desc_2, desc_3, parser,
		       pmac, paser_exist);
		goto RX_DROP;
	}
#endif

	if (port_id == PMAC_CPU_ID) { /*To CPU and need check src pmac port */
		dp_port_prop[inst].info.update_port_vap(inst, &port_id, &vap,
			skb,
			pmac, &decryp);
	} else {		/*GSWIP-R already know the destination */
		rx_tx_flag = 1;
		vap = GET_VAP(desc_0->field.dest_sub_if_id,
			      dp_port_info[inst][port_id].vap_offset,
			      dp_port_info[inst][port_id].vap_mask);
	}
	if (unlikely(!port_id)) { /*Normally shouldnot go to here */
		rx_dbg_zero_port(skb, desc_0, desc_1, desc_2, desc_3, parser,
				 pmac, paser_exist, ep, port_id, vap);
		goto RX_DROP;
	}
	dp_port = &dp_port_info[inst][port_id];
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
	if (dp_port->f_ptp) {
		ops = dp_port_prop[inst].mac_ops[port_id];
		if (ops)
			ops->do_rx_hwts(ops, skb);
	}
#endif
	/*PON traffic always have timestamp attached,removing Timestamp */
	if (dp_port->alloc_flags & (DP_F_GPON | DP_F_EPON)) {
		/* Stripping of last 10 bytes timestamp */
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
		if (!dp_port->f_ptp)
			__pskb_trim(skb, skb->len - DP_TS_HDRLEN);
#else
		__pskb_trim(skb, skb->len - DP_TS_HDRLEN);
#endif
	}

	rx_fn = dp_port->cb.rx_fn;
	if (likely(rx_fn && dp_port->status)) {
		/*Clear some fields as SWAS V3.7 required */
		//desc_1->all &= dma_rx_desc_mask1.all;
		desc_3->all &= dma_rx_desc_mask3.all;
		skb->priority = desc_1->field.classid;
		skb->dev = dp_port->subif_info[vap].netif;
		dev = dp_port->subif_info[vap].netif;
		if (decryp) { /*workaround mark for bypass xfrm policy*/
			desc_1->field.dec = 1;
			desc_1->field.enc = 1;
		}
		if (!dev &&
		    ((dp_port->alloc_flags & DP_F_FAST_DSL) == 0)) {
			UP_STATS(dp_port->subif_info[vap].mib.rx_fn_dropped);
			goto RX_DROP;
		}

		if (unlikely(dp_dbg_flag)) {
			DP_DEBUG(DP_DBG_FLAG_DUMP_RX, "%s=%d vap=%d\n",
				 (ep) ? "ep" : "port_id", port_id, vap);

			if (dp_dbg_flag & DP_DBG_FLAG_DUMP_RX_DATA) {
				dp_dump_raw_data(skb->data, PMAC_SIZE,
						 "pmac to top drv");
				dp_dump_raw_data(skb->data + PMAC_SIZE,
						 ((skb->len - PMAC_SIZE) >
							print_len) ?
							skb->len - PMAC_SIZE :
							print_len,
						 "Data to top drv");
			}
			if (dp_dbg_flag & DP_DBG_FLAG_DUMP_RX_DESCRIPTOR)
				dp_port_prop[inst].info.dump_rx_dma_desc(
					desc_0, desc_1,
					desc_2, desc_3);
		}
#ifdef CONFIG_LTQ_DATAPATH_MPE_FASTHOOK_TEST
		if (unlikely(ltq_mpe_fasthook_rx_fn))
			ltq_mpe_fasthook_rx_fn(skb, 1, NULL);	/*with pmac */
#endif
		if (unlikely((enum TEST_MODE)dp_rx_test_mode ==
			DP_RX_MODE_LAN_WAN_BRIDGE)) {
			/*for datapath performance test only */
			dp_lan_wan_bridging(port_id, skb);
			/*return DP_SUCCESS;*/
		}
		/*If switch h/w acceleration is enabled,setting of this bit
		 *avoid forwarding duplicate packets from linux
		 */
		#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
			if (dp_port->subif_info[vap].fid > 0)
				skb->offload_fwd_mark = 1;
		#endif
		if (rx_tx_flag == 0) {
			if (dp_port->lct_idx > 0)
				ret_lct = dp_handle_lct(dp_port, skb, rx_fn);
			if (ret_lct) {
				rx_fn(dev, NULL, skb, skb->len);
				UP_STATS(dp_port->subif_info[vap].mib.
								rx_fn_rxif_pkt);
			}
		} else {
			rx_fn(NULL, dev, skb, skb->len);
			UP_STATS(dp_port->subif_info[vap].mib.rx_fn_txif_pkt);
		}

		return DP_SUCCESS;
	}

	if (unlikely(port_id >=
	    dp_port_prop[inst].info.cap.max_num_dp_ports - 1)) {
		PR_ERR("Drop for wrong ep or src port id=%u ??\n",
		       port_id);
		goto RX_DROP;
	} else if (unlikely(dp_port->status == PORT_FREE)) {
		DP_DEBUG(DP_DBG_FLAG_DUMP_RX, "Drop for port %u free\n",
			 port_id);
		goto RX_DROP;
	} else if (unlikely(!rx_fn)) {
		DP_DEBUG(DP_DBG_FLAG_DUMP_RX,
			 "Drop for subif of port %u not registered yet\n",
			 port_id);
		UP_STATS(dp_port->subif_info[vap].mib.rx_fn_dropped);
		goto RX_DROP2;
	} else {
		pr_info("Unknown issue\n");
	}
RX_DROP:
	UP_STATS(dp_port->rx_err_drop);
RX_DROP2:
	if (skb)
		dev_kfree_skb_any(skb);
	return res;
}

void dp_xmit_dbg(
	char *title,
	struct sk_buff *skb,
	s32 ep,
	s32 len,
	u32 flags,
	struct pmac_tx_hdr *pmac,
	dp_subif_t *rx_subif,
	int need_pmac,
	int gso,
	int checksum)
{
	DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
		 "%s: dp_xmit:skb->data/len=0x%p/%d data_ptr=%x from port=%d and subitf=%d\n",
		 title,
		 skb->data, len,
		 ((struct dma_tx_desc_2 *)&skb->DW2)->field.data_ptr,
		 ep, rx_subif->subif);
	if (dp_dbg_flag & DP_DBG_FLAG_DUMP_TX_DATA) {
		if (pmac) {
			dp_dump_raw_data((char *)pmac, PMAC_SIZE, "Tx Data");
			dp_dump_raw_data(skb->data,
					 (skb->len > print_len) ?
						skb->len :
						print_len,
					 "Tx Data");
		} else
			dp_dump_raw_data(skb->data,
					 (skb->len > print_len) ?
						skb->len : print_len,
					 "Tx Data");
	}
	DP_DEBUG(DP_DBG_FLAG_DUMP_TX_SUM,
		 "ip_summed=%s(%d) encapsulation=%s\n",
		 dp_skb_csum_str(skb), skb->ip_summed,
		 skb->encapsulation ? "Yes" : "No");
	if (skb->encapsulation)
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX_SUM,
			 "inner ip start=0x%x(%d), transport=0x%x(%d)\n",
			 (unsigned int)skb_inner_network_header(skb),
			 (int)(skb_inner_network_header(skb) -
			       skb->data),
			 (unsigned int)
			 skb_inner_transport_header(skb),
			 (int)(skb_inner_transport_header(skb) -
			       skb_inner_network_header(skb)));
	else
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX_SUM,
			 "ip start=0x%x(%d), transport=0x%x(%d)\n",
			 (unsigned int)(unsigned int)
			 skb_network_header(skb),
			 (int)(skb_network_header(skb) - skb->data),
			 (unsigned int)skb_transport_header(skb),
			 (int)(skb_transport_header(skb) -
			       skb_network_header(skb)));

	if (dp_dbg_flag & DP_DBG_FLAG_DUMP_TX_DESCRIPTOR)
		dp_port_prop[0].info.dump_tx_dma_desc(
				 (struct dma_tx_desc_0 *)&skb->DW0,
				 (struct dma_tx_desc_1 *)&skb->DW1,
				 (struct dma_tx_desc_2 *)&skb->DW2,
				 (struct dma_tx_desc_3 *)&skb->DW3);

	DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "flags=0x%x skb->len=%d\n",
		 flags, skb->len);
	DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
		 "skb->data=0x%p with pmac hdr size=%u\n", skb->data,
		 sizeof(struct pmac_tx_hdr));
	if (need_pmac) { /*insert one pmac header */
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
			 "need pmac\n");
		if (pmac && (dp_dbg_flag & DP_DBG_FLAG_DUMP_TX_DESCRIPTOR))
			dp_port_prop[0].info.dump_tx_pmac(pmac);
	} else {
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "no pmac\n");
	}
	if (gso)
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "GSO pkt\n");
	else
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "Non-GSO pkt\n");
	if (checksum)
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "Need checksum offload\n");
	else
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "No need checksum offload pkt\n");

	DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "\n\n");
}

#define NO_NEED_PMAC(flags)  ((dp_info->alloc_flags & \
		(DP_F_FAST_WLAN | DP_F_FAST_DSL)) && \
		!((flags) & (DP_TX_CAL_CHKSUM | DP_TX_DSL_FCS)))

static void set_chksum(struct pmac_tx_hdr *pmac, u32 tcp_type,
		       u32 ip_offset, int ip_off_hw_adjust,
		       u32 tcp_h_offset)
{
	pmac->tcp_type = tcp_type;
	pmac->ip_offset = ip_offset + ip_off_hw_adjust;
	pmac->tcp_h_offset = tcp_h_offset >> 2;
}

int32_t dp_xmit(struct net_device *rx_if, dp_subif_t *rx_subif,
		struct sk_buff *skb, int32_t len, uint32_t flags)
{
	struct dma_tx_desc_0 *desc_0;
	struct dma_tx_desc_1 *desc_1;
	struct dma_tx_desc_2 *desc_2;
	struct dma_tx_desc_3 *desc_3;
	struct pmac_port_info *dp_info = NULL;
	struct pmac_port_info2 *dp_info2 = NULL;
	struct pmac_tx_hdr pmac = {0};
	u32 ip_offset, tcp_h_offset, tcp_type;
	char tx_chksum_flag = 0; /*check csum cal can be supported or not */
	char insert_pmac_f = 1;	/*flag to insert one pmac */
	int res = DP_SUCCESS;
	int ep, vap;
	enum dp_xmit_errors err_ret = 0;
	int inst = 0;
	struct cbm_tx_data data;
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
	struct mac_ops *ops;
	int rec_id = 0;
#endif

#ifdef CONFIG_LTQ_DATAPATH_EXTRA_DEBUG
	if (unlikely(!dp_init_ok)) {
		err_ret = DP_XMIT_ERR_NOT_INIT;
		goto lbl_err_ret;
	}
	if (unlikely(!rx_subif)) {
		err_ret = DP_XMIT_ERR_NULL_SUBIF;
		goto lbl_err_ret;
	}
	if (unlikely(!skb)) {
		err_ret = DP_XMIT_ERR_NULL_SKB;
		goto lbl_err_ret;
	}
#endif
	ep = rx_subif->port_id;
	if (unlikely(ep >= dp_port_prop[inst].info.cap.max_num_dp_ports)) {
		err_ret = DP_XMIT_ERR_PORT_TOO_BIG;
		goto lbl_err_ret;
	}
#ifdef CONFIG_LTQ_DATAPATH_EXTRA_DEBUG
	if (unlikely(in_irq())) {
		err_ret = DP_XMIT_ERR_IN_IRQ;
		goto lbl_err_ret;
	}
#endif
	dp_info = &dp_port_info[inst][ep];
	dp_info2 = &dp_port_info2[inst][ep];
	vap = GET_VAP(rx_subif->subif, dp_info->vap_offset, dp_info->vap_mask);
	if (unlikely(!rx_if && /*For atm pppoa case, rx_if is NULL now */
		     !(dp_info->alloc_flags & DP_F_FAST_DSL))) {
		err_ret = DP_XMIT_ERR_NULL_IF;
		goto lbl_err_ret;
	}
#ifdef CONFIG_LTQ_DATAPATH_MPE_FASTHOOK_TEST
	if (unlikely(ltq_mpe_fasthook_tx_fn))
		ltq_mpe_fasthook_tx_fn(skb, 0, NULL);
#endif
	if (unlikely(dp_dbg_flag))
		dp_xmit_dbg("\nOrig", skb, ep, len, flags,
			    NULL, rx_subif, 0, 0, flags & DP_TX_CAL_CHKSUM);

	/*No PMAC for WAVE500 and DSL by default except bonding case */
	if (unlikely(NO_NEED_PMAC(dp_info->alloc_flags)))
		insert_pmac_f = 0;

	/**********************************************
	 *Must put these 4 lines after INSERT_PMAC
	 *since INSERT_PMAC will change skb if needed
	 *********************************************/
	desc_0 = (struct dma_tx_desc_0 *)&skb->DW0;
	desc_1 = (struct dma_tx_desc_1 *)&skb->DW1;
	desc_2 = (struct dma_tx_desc_2 *)&skb->DW2;
	desc_3 = (struct dma_tx_desc_3 *)&skb->DW3;

	if (flags & DP_TX_CAL_CHKSUM) {
		int ret_flg;

		if (!dp_port_prop[inst].info.check_csum_cap()) {
			err_ret = DP_XMIT_ERR_CSM_NO_SUPPORT;
			goto lbl_err_ret;
		}
		ret_flg = get_offset_clear_chksum(skb, &ip_offset,
						  &tcp_h_offset, &tcp_type);
		if (likely(ret_flg == 0))
			/*HW can support checksum offload*/
			tx_chksum_flag = 1;
#ifdef CONFIG_LTQ_DATAPATH_EXTRA_DEBUG
		else if (ret_flg == -1)
			pr_info_once("packet can't do hw checksum\n");
#endif
	}

	/*reset all descriptors as SWAS required since SWAS 3.7 */
	/*As new SWAS 3.7 required, MPE1/Color/FlowID is set by applications */
	desc_0->all &= dma_tx_desc_mask0.all;
	desc_1->all &= dma_tx_desc_mask1.all;
	/*desc_2->all = 0;*/ /*remove since later it will be set properly */
	if (desc_3->field.dic) {
		desc_3->all = 0; /*keep DIC bit to support test tool*/
		desc_3->field.dic = 1;
	} else {
		desc_3->all = 0;
	}

	if (flags & DP_TX_OAM) /* OAM */
		desc_3->field.pdu_type = 1;
	desc_1->field.classid = (skb->priority >= 15) ? 15 : skb->priority;
	desc_2->field.data_ptr = (uint32_t)skb->data;

	/*for ETH LAN/WAN */
	if (dp_info->alloc_flags & (DP_F_FAST_ETH_LAN | DP_F_FAST_ETH_WAN |
	    DP_F_GPON | DP_F_EPON)) {
		/*always with pmac*/
		if (likely(tx_chksum_flag)) {
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_CHECKSUM, &pmac,
							desc_0, desc_1,
							dp_info2);
			set_chksum(&pmac, tcp_type, ip_offset,
				   ip_offset_hw_adjust, tcp_h_offset);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
		} else {
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_NORMAL, &pmac,
							desc_0, desc_1,
							dp_info2);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
		}
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588_SW_WORKAROUND)
		if (dp_info->f_ptp)
#else
		if (dp_info->f_ptp &&
		    (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP))
#endif
		{	ops = dp_port_prop[inst].mac_ops[dp_info->port_id];
			if (!ops) {
				err_ret = DP_XMIT_PTP_ERR;
				goto lbl_err_ret;
			}
			rec_id = ops->do_tx_hwts(ops, skb);
			if (rec_id < 0) {
				err_ret = DP_XMIT_PTP_ERR;
				goto lbl_err_ret;
			}
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_PTP, &pmac,
							desc_0, desc_1,
							dp_info2);
			pmac.record_id_msb = rec_id;
		}
#endif
	} else if (dp_info->alloc_flags & DP_F_FAST_DSL) { /*some with pmac*/
		if (unlikely(flags & DP_TX_CAL_CHKSUM)) { /* w/ pmac*/
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_CHECKSUM, &pmac,
							desc_0, desc_1,
							dp_info2);
			set_chksum(&pmac, tcp_type, ip_offset,
				   ip_offset_hw_adjust, tcp_h_offset);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
#ifdef CONFIG_LTQ_DATAPATH_ACA_CSUM_WORKAROUND
			if (aca_portid > 0)
				desc_1->field.ep = aca_portid;
#endif
		} else if (flags & DP_TX_DSL_FCS) {/* after checksum check */
			/* w/ pmac for FCS purpose*/
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_OTHERS, &pmac,
							desc_0, desc_1,
							dp_info2);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
			insert_pmac_f = 1;
#ifdef CONFIG_LTQ_DATAPATH_ACA_CSUM_WORKAROUND
			if (aca_portid > 0)
				desc_1->field.ep = aca_portid;
#endif
		} else { /*no pmac */
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_NORMAL, NULL,
							desc_0, desc_1,
							dp_info2);
		}
	} else if (dp_info->alloc_flags & DP_F_FAST_WLAN) {/*some with pmac*/
		/*normally no pmac. But if need checksum, need pmac*/
		if (unlikely(tx_chksum_flag)) { /*with pmac*/
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_CHECKSUM, &pmac,
							desc_0, desc_1,
							dp_info2);
			set_chksum(&pmac, tcp_type, ip_offset,
				   ip_offset_hw_adjust, tcp_h_offset);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
#ifdef CONFIG_LTQ_DATAPATH_ACA_CSUM_WORKAROUND
			if (aca_portid > 0)
				desc_1->field.ep = aca_portid;
#endif
		} else { /*no pmac*/
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_NORMAL, NULL,
							desc_0, desc_1,
							dp_info2);
		}
	} else if (dp_info->alloc_flags & DP_F_DIRECTLINK) { /*always w/ pmac*/
		if (unlikely(flags & DP_TX_CAL_CHKSUM)) { /* w/ pmac*/
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_CHECKSUM, &pmac,
							desc_0, desc_1,
							dp_info2);
			set_chksum(&pmac, tcp_type, ip_offset,
				   ip_offset_hw_adjust, tcp_h_offset);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
		} else if (flags & DP_TX_TO_DL_MPEFW) { /*w/ pmac*/
			/*copy from checksum's pmac template setting,
			 *but need to reset tcp_chksum in TCP header
			 */
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_OTHERS, &pmac,
							desc_0, desc_1,
							dp_info2);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
		} else { /*do like normal directpath with pmac */
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_NORMAL, &pmac,
							desc_0, desc_1,
							dp_info2);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
		}
	} else { /*normal directpath: always w/ pmac */
		if (unlikely(tx_chksum_flag)) {
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_CHECKSUM,
							&pmac,
							desc_0,
							desc_1,
							dp_info2);
			set_chksum(&pmac, tcp_type, ip_offset,
				   ip_offset_hw_adjust, tcp_h_offset);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
		} else { /*w/ pmac */
			DP_CB(inst, get_dma_pmac_templ)(TEMPL_NORMAL, &pmac,
							desc_0, desc_1,
							dp_info2);
			DP_CB(inst, set_pmac_subif)(&pmac, rx_subif->subif);
		}
	}
	desc_3->field.data_len = skb->len;

	if (unlikely(dp_dbg_flag)) {
		if (insert_pmac_f)
			dp_xmit_dbg("After", skb, ep, len, flags, &pmac,
				    rx_subif, insert_pmac_f, skb_is_gso(skb),
				    tx_chksum_flag);
		else
			dp_xmit_dbg("After", skb, ep, len, flags, NULL,
				    rx_subif, insert_pmac_f, skb_is_gso(skb),
				    tx_chksum_flag);
	}

#if IS_ENABLED(CONFIG_LTQ_TOE_DRIVER)
	if (skb_is_gso(skb)) {
		res = ltq_tso_xmit(skb, &pmac, sizeof(pmac), 0);
		UP_STATS(dp_info->subif_info[vap].mib.tx_tso_pkt);
		return res;
	}
#endif /* CONFIG_LTQ_TOE_DRIVER */

#ifdef CONFIG_LTQ_DATAPATH_EXTRA_DEBUG
	if (unlikely(!desc_1->field.ep)) {
		err_ret = DP_XMIT_ERR_EP_ZERO;
		goto lbl_err_ret;
	}
#endif
	if (insert_pmac_f) {
		data.pmac = (u8 *)&pmac;
		data.pmac_len = sizeof(pmac);
		data.dp_inst = inst;
		data.dp_inst = 0;
	} else {
		data.pmac = NULL;
		data.pmac_len = 0;
		data.dp_inst = inst;
		data.dp_inst = 0;
	}
	res = cbm_cpu_pkt_tx(skb, &data, 0);
	UP_STATS(dp_info->subif_info[vap].mib.tx_cbm_pkt);
	return res;

lbl_err_ret:
	switch (err_ret) {
	case DP_XMIT_ERR_NOT_INIT:
		PR_RATELIMITED("dp_xmit failed for dp no init yet\n");
		break;
	case DP_XMIT_ERR_IN_IRQ:
		PR_RATELIMITED("dp_xmit not allowed in interrupt context\n");
		break;
	case DP_XMIT_ERR_NULL_SUBIF:
		PR_RATELIMITED("dp_xmit failed for rx_subif null\n");
		UP_STATS(PORT_INFO(inst, 0, tx_err_drop));
		break;
	case DP_XMIT_ERR_PORT_TOO_BIG:
		UP_STATS(PORT_INFO(inst, 0, tx_err_drop));
		PR_RATELIMITED("rx_subif->port_id >= max_ports");
		break;
	case DP_XMIT_ERR_NULL_SKB:
		PR_RATELIMITED("skb NULL");
		UP_STATS(PORT_INFO(inst, rx_subif->port_id, tx_err_drop));
		break;
	case DP_XMIT_ERR_NULL_IF:
		UP_STATS(PORT_VAP_MIB(inst, ep, vap, tx_pkt_dropped));
		PR_RATELIMITED("rx_if NULL");
		break;
	case DP_XMIT_ERR_REALLOC_SKB:
		PR_INFO_ONCE("dp_create_new_skb failed\n");
		break;
	case DP_XMIT_ERR_EP_ZERO:
		PR_ERR("Why ep zero in dp_xmit for %s\n",
		       skb->dev ? skb->dev->name : "NULL");
		break;
	case DP_XMIT_ERR_GSO_NOHEADROOM:
		PR_ERR("No enough skb headerroom(GSO). Need tune SKB buffer\n");
		break;
	case DP_XMIT_ERR_CSM_NO_SUPPORT:
		PR_RATELIMITED("dp_xmit not support checksum\n");
		break;
	case DP_XMIT_PTP_ERR:
		break;
	default:
		UP_STATS(dp_info->subif_info[vap].mib.tx_pkt_dropped);
		PR_INFO_ONCE("Why come to here:%x\n",
			     dp_port_info[inst][ep].status);
	}
	if (skb)
		dev_kfree_skb_any(skb);
	return DP_FAILURE;
}
EXPORT_SYMBOL(dp_xmit);

void set_dp_dbg_flag(uint32_t flags)
{
	dp_dbg_flag = flags;
}

uint32_t get_dp_dbg_flag(void)
{
	return dp_dbg_flag;
}

/*!
 *@brief  The API is for dp_get_cap
 *@param[in,out] cap dp_cap pointer, caller must provide the buffer
 *@param[in] flag for future
 *@return 0 if OK / -1 if error
 */
int dp_get_cap(struct dp_cap *cap, int flag)
{
	if (!cap)
		return DP_FAILURE;
	if ((cap->inst < 0) || (cap->inst >= DP_MAX_INST))
		return DP_FAILURE;
	if (!hw_cap_list[cap->inst].valid)
		return DP_FAILURE;
	*cap = hw_cap_list[cap->inst].info.cap;

	return DP_SUCCESS;
}
EXPORT_SYMBOL(dp_get_cap);

int dp_set_min_frame_len(s32 dp_port,
			 s32 min_frame_len,
			 uint32_t flags)
{
	return DP_SUCCESS;
}
EXPORT_SYMBOL(dp_set_min_frame_len);

int dp_rx_enable(struct net_device *netif, char *ifname, uint32_t flags)
{
	return DP_SUCCESS;
}
EXPORT_SYMBOL(dp_rx_enable);

int dp_vlan_set(struct dp_tc_vlan *vlan, int flags)
{
	dp_subif_t subif = {0};
	struct dp_tc_vlan_info info = {0};
	struct pmac_port_info *port_info;

	if (dp_get_netif_subifid(vlan->dev, NULL, NULL, NULL, &subif, 0))
		return DP_FAILURE;
	port_info = PORT(subif.inst, subif.port_id);
	info.subix = GET_VAP(subif.subif, port_info->vap_offset,
			     port_info->vap_mask);
	info.bp = subif.bport;
	info.dp_port = subif.port_id;
	info.inst = subif.inst;
	
	if ((vlan->def_apply == DP_VLAN_APPLY_CTP) && 
				(subif.flag_pmapper == 1)) {
		PR_ERR("cannot apply VLAN rule for pmapper device\n");
		return DP_FAILURE;
	} else if (vlan->def_apply == DP_VLAN_APPLY_CTP) {
		info.dev_type = 0;
	} else {
		info.dev_type |= subif.flag_bp;
	}
	if (vlan->mcast_flag == DP_MULTICAST_SESSION) 
		info.dev_type |= 0x02;
	DP_DEBUG(DP_DBG_FLAG_PAE, "dev_type:0x%x\n", info.dev_type);
	if (DP_CB(subif.inst, dp_tc_vlan_set))
		return DP_CB(subif.inst, dp_tc_vlan_set)
			    (dp_port_prop[subif.inst].ops[0],
			     vlan, &info, flags);
	return DP_FAILURE;
}
EXPORT_SYMBOL(dp_vlan_set);

/*Return the table entry index based on dev:
 *success: >=0
 *fail: DP_FAILURE
 */
int bp_pmapper_dev_get(int inst, struct net_device *dev)
{
	int i;

	if (!dev)
		return -1;
	for (i = 0; i < ARRAY_SIZE(dp_bp_dev_tbl[inst]); i++) {
		if (!dp_bp_dev_tbl[inst][i].flag)
			continue;
		if (dp_bp_dev_tbl[inst][i].dev == dev) {
			DP_DEBUG(DP_DBG_FLAG_PAE, "matched %s\n", dev->name);
			return i;
		}
	}
	return -1;
}

#ifdef DP_TEST_EXAMPLE
void test(void)
{
	/* Base on below example data, it should print like below log
	 *DMA Descripotr:D0=0x00004000 D1=0x00001000 D2=0xa0c02080 D3=0xb0000074
	 *DW0:resv0=0 tunnel_id=00 flow_id=0 eth_type=0 dest_sub_if_id=0x4000
	 *DW1:session_id=0x000 tcp_err=0 nat=0 dec=0 enc=0 mpe2=0 mpe1=0
	 *color=01 ep=00 resv1=0 classid=00
	 *DW2:data_ptr=0xa0c02080
	 *DW3:own=1 c=0 sop=1 eop=1 dic=0 pdu_type=0
	 *byte_offset=0 atm_qid=0 mpoa_pt=0 mpoa_mode=0 data_len= 116
	 *paser flags: 00 00 00 00 80 18 80 00
	 *paser flags: 00 80 18 80 00 00 00 00 (reverse)
	 *flags 15 offset=14: PASER_FLAGS_1IPV4
	 *flags 19 offset=22: PASER_FLAGS_ROUTEXP
	 *flags 20 offset=34: PASER_FLAGS_TCP
	 *flags 31 offset=46: PASER_FLAGS_LROEXP
	 *pmac:0x4e 0x28 0xf0 0x00 0x00 0x00 0x00 0x01
	 *byte 0:res=0 ver_done =1 ip_offset=14
	 *byte 1:tcp_h_offset=5 tcp_type=0
	 *byte 2:ppid=15 class=0
	 *byte 3:res=0 pkt_type=0
	 *byte 4:res=0 redirect=0 res2=0 src_sub_inf_id=0
	 *byte 5:src_sub_inf_id2=0
	 *byte 6:port_map=0
	 *byte 7:port_map2=1
	 */
#ifdef CONFIG_LITTLE_ENDIAN
	char example_data[] = {
		0x00, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e,
		0x00, 0x00, 0x00, 0x16, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x2e,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x18, 0x80, 0x00,
		0x00, 0xf0, 0x28, 0x4e, 0x01, 0x00, 0x00, 0x00, 0xaa, 0x00,
		0x00, 0x00, 0x04, 0x03, 0xbb, 0x00,
		0x00, 0x00, 0x04, 0x02, 0x08, 0x00, 0x45, 0x00, 0x00, 0x2e,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x06,
		0xb9, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x04, 0x00, 0xb2, 0x9a, 0x03, 0xde
	};
#else
	char example_data[] = {
		0x00, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e,
		0x00, 0x00, 0x00, 0x16, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x2e,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x18, 0x80, 0x00,
		0x4e, 0x28, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xaa, 0x00,
		0x00, 0x00, 0x04, 0x03, 0xbb, 0x00,
		0x00, 0x00, 0x04, 0x02, 0x08, 0x00, 0x45, 0x00, 0x00, 0x2e,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x06,
		0xb9, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x04, 0x00, 0xb2, 0x9a, 0x03, 0xde
	};
#endif
	struct sk_buff skb;

	skb.DW0 = 0x4000;
	skb.DW1 = 0x1000;
	skb.DW2 = 0xa0c02080;
	skb.DW3 = 0xb0000074;
	skb.data = example_data;
	skb.len = sizeof(example_data);
	dp_rx(&skb, 0);
}
#endif				/* DP_TEST_EXAMPLE */

int dp_basic_proc(void)
{
#ifdef CONFIG_LTQ_DATAPATH_LOOPETH
	struct dentry *p_node;
#endif

	/*mask to reset some field as SWAS required  all others try to keep */
	memset(dp_port_prop, 0, sizeof(dp_port_prop));
	memset(dp_port_info, 0, sizeof(dp_port_info));
#ifdef CONFIG_LTQ_DATAPATH_LOOPETH
	p_node = dp_proc_install();
	dp_loop_eth_dev_init(p_node);
#else
	dp_proc_install();
#endif
	dp_inst_init(0);
	dp_subif_list_init();
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
	dp_switchdev_init();
#endif
	return 0;
}

/*static __init */ int dp_init_module(void)
{
	int res = 0;

	if (dp_init_ok) /*alredy init */
		return 0;
	register_notifier(0);
#ifdef CONFIG_LTQ_DATAPATH_DUMMY_QOS_VIA_PRX300_TEST
	PR_INFO("\n\n--prx300_test to simulate SLIM QOS drv---\n\n\n");
#endif /*CONFIG_LTQ_DATAPATH_DUMMY_QOS_VIA_PRX300_TEST*/
	register_dp_cap(0);
	if (request_dp(0)) /*register 1st dp instance */ {
		PR_ERR("register_dp instance fail\n");
		return -1;
	}
#ifdef CONFIG_LTQ_DATAPATH_EXTRA_DEBUG
	PR_INFO("preempt_count=%x\n", preempt_count());
	if (preempt_count() & HARDIRQ_MASK)
		PR_INFO("HARDIRQ_MASK\n");
	if (preempt_count() & SOFTIRQ_MASK)
		PR_INFO("SOFTIRQ_MASK\n");
	if (preempt_count() & NMI_MASK)
		PR_INFO("NMI_MASK\n");
#endif
	dp_init_ok = 1;
	PR_INFO("datapath init done\n");
	return res;
}

/*static __exit*/ void dp_cleanup_module(void)
{
	int i;

	if (dp_init_ok) {
		DP_LIB_LOCK(&dp_lock);
		memset(dp_port_info, 0, sizeof(dp_port_info));
#ifdef CONFIG_LTQ_DATAPATH_MIB
		dp_mib_exit();
#endif
		for (i = 0; i < dp_inst_num; i++)
			DP_CB(i, dp_platform_set)(i, DP_PLATFORM_DE_INIT);
		dp_init_ok = 0;
#ifdef CONFIG_LTQ_DATAPATH_LOOPETH
		dp_loop_eth_dev_exit();
#endif
#ifdef CONFIG_LTQ_DATAPATH_CPUFREQ
		dp_coc_cpufreq_exit();
#endif
		unregister_notifier(0);
	}
}

MODULE_LICENSE("GPL");

static int __init dp_dbg_lvl_set(char *str)
{
	PR_INFO("\n\ndp_dbg=%s\n\n", str);
	dp_dbg_flag = dp_atoi(str);

	return 0;
}

early_param("dp_dbg", dp_dbg_lvl_set);

