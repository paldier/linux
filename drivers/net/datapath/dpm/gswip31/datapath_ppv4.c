/*
 * Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <net/datapath_api.h>
#include "../datapath.h"
#include "datapath_misc.h"

int (*qos_queue_remove)(struct pp_qos_dev *qos_dev, unsigned int id);
int (*qos_queue_allocate)(struct pp_qos_dev *qos_dev, unsigned int *id);
int (*qos_queue_info_get)(struct pp_qos_dev *qos_dev, unsigned int id,
			  struct pp_qos_queue_info *info);
int (*qos_port_remove)(struct pp_qos_dev *qos_dev, unsigned int id);
int (*qos_sched_allocate)(struct pp_qos_dev *qos_dev, unsigned int *id);
int (*qos_sched_remove)(struct pp_qos_dev *qos_dev, unsigned int id);
int (*qos_port_allocate)(struct pp_qos_dev *qos_dev,
			 unsigned int physical_id,
			 unsigned int *id);
int (*qos_port_set)(struct pp_qos_dev *qos_dev, unsigned int id,
		    const struct pp_qos_port_conf *conf);
void (*qos_port_conf_set_default)(struct pp_qos_port_conf *conf);
void (*qos_queue_conf_set_default)(struct pp_qos_queue_conf *conf);
int (*qos_queue_set)(struct pp_qos_dev *qos_dev, unsigned int id,
		     const struct pp_qos_queue_conf *conf);
void (*qos_sched_conf_set_default)(struct pp_qos_sched_conf *conf);
int (*qos_sched_set)(struct pp_qos_dev *qos_dev, unsigned int id,
		     const struct pp_qos_sched_conf *conf);
int (*qos_queue_conf_get)(struct pp_qos_dev *qos_dev, unsigned int id,
			  struct pp_qos_queue_conf *conf);
int (*qos_queue_flush)(struct pp_qos_dev *qos_dev, unsigned int id);
int (*qos_sched_conf_get)(struct pp_qos_dev *qos_dev, unsigned int id,
			  struct pp_qos_sched_conf *conf);
int (*qos_sched_get_queues)(struct pp_qos_dev *qos_dev, unsigned int id,
			    u16 *queue_ids, unsigned int size,
			    unsigned int *queues_num);
int (*qos_port_get_queues)(struct pp_qos_dev *qos_dev, unsigned int id,
			   u16 *queue_ids, unsigned int size,
				  unsigned int *queues_num);
int (*qos_port_conf_get)(struct pp_qos_dev *qdev, unsigned int id,
			 struct pp_qos_port_conf *conf);
int (*qos_port_info_get)(struct pp_qos_dev *qdev, unsigned int id,
			 struct pp_qos_port_info *info);
struct pp_qos_dev *(*qos_dev_open)(unsigned int id);
int (*qos_dev_init)(struct pp_qos_dev *qos_dev,
		    struct pp_qos_init_param *conf);

void init_qos_fn(void)
{
#if IS_ENABLED(CONFIG_LTQ_PPV4_QOS)
	qos_queue_remove = pp_qos_queue_remove;
	qos_queue_allocate = pp_qos_queue_allocate;
	qos_queue_info_get = pp_qos_queue_info_get;
	qos_port_remove = pp_qos_port_remove;
	qos_sched_allocate = pp_qos_sched_allocate;
	qos_sched_remove = pp_qos_sched_remove;
	qos_port_allocate = pp_qos_port_allocate;
	qos_port_set = pp_qos_port_set;
	qos_port_conf_set_default = pp_qos_port_conf_set_default;
	qos_queue_conf_set_default = pp_qos_queue_conf_set_default;
	qos_queue_set = pp_qos_queue_set;
	qos_sched_conf_set_default = pp_qos_sched_conf_set_default;
	qos_sched_set = pp_qos_sched_set;
	qos_queue_conf_get = pp_qos_queue_conf_get;
	qos_queue_flush = pp_qos_queue_flush;
	qos_sched_conf_get = pp_qos_sched_conf_get;
	qos_sched_get_queues = pp_qos_sched_get_queues;
	qos_port_get_queues = pp_qos_port_get_queues;
	qos_port_conf_get = pp_qos_port_conf_get;
	qos_dev_open = pp_qos_dev_open;
	qos_dev_init = pp_qos_dev_init;
#else
	/*all NULL function pointer */
	DP_DEBUG(DP_DBG_FLAG_QOS, "call QOS function pointer set to NULL\n");
#endif /*CONFIG_LTQ_PPV4_QOS*/
}

int dp_pp_alloc_port(struct ppv4_port *info)
{
	int qos_p_id = 0;
	struct pp_qos_port_conf conf;
	struct hal_priv *priv = HAL(info->inst);
	struct pp_qos_dev *qos_dev = priv->qdev;

	if (qos_port_allocate(qos_dev,
			      info->cqm_deq_port,
			      &qos_p_id)) {
		PR_ERR("Failed to alloc QoS for deq_port=%d\n",
		       info->cqm_deq_port);
		return -1;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "qos_port_allocate ok with port(cqm/qos)=%d/%d\n",
		 info->cqm_deq_port, qos_p_id);

	qos_port_conf_set_default(&conf);
	conf.port_parent_prop.arbitration = PP_QOS_ARBITRATION_WSP;
	conf.ring_address = (void *)info->tx_ring_addr;
	conf.ring_size = info->tx_ring_size;
	conf.packet_credit_enable = 1;
	conf.credit = info->tx_pkt_credit;
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		DP_DEBUG(DP_DBG_FLAG_QOS,
			 "qos_port_set info for p[%d/%d] dp_port=%d:\n",
			 info->cqm_deq_port, qos_p_id,
			 info->dp_port);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  arbitration=%d\n",
			 conf.port_parent_prop.arbitration);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  ring_address=0x%x\n",
			 (unsigned int)conf.ring_address);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  ring_size=%d\n",
			 conf.ring_size);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  packet_credit_enable=%d\n",
			 conf.packet_credit_enable);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  credit=%d\n",
			 conf.credit);
	}
#endif
	if (qos_port_set(qos_dev, qos_p_id, &conf)) {
		PR_ERR("qos_port_set fail for port(cqm/qos) %d/%d\n",
		       info->cqm_deq_port, qos_p_id);
		qos_port_remove(qos_dev, qos_p_id);
		return -1;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "qos_port_set ok for port(cqm/qos) %d/%d\n",
		       info->cqm_deq_port, qos_p_id);
	info->node_id = qos_p_id;
	priv->deq_port_stat[info->cqm_deq_port].flag = PP_NODE_ALLOC;
	priv->deq_port_stat[info->cqm_deq_port].node_id = qos_p_id;
	info->node_id = qos_p_id;
	return 0;
}

int dp_pp_alloc_queue(struct ppv4_queue *info)
{
	struct pp_qos_queue_conf conf;
	int q_node_id;
	struct pp_qos_queue_info q_info;
	struct hal_priv *priv = HAL(info->inst);
	struct pp_qos_dev *qos_dev = priv->qdev;

	if (qos_queue_allocate(qos_dev, &q_node_id)) {
		PR_ERR("qos_queue_allocate fail\n");
		return -1;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS, "qos_queue_allocate ok q_node=%d\n",
		 q_node_id);

	qos_queue_conf_set_default(&conf);
	conf.wred_enable = 0;
	conf.queue_wred_max_allowed = 0x400; /*max qocc in pkt */
	conf.queue_child_prop.parent = info->parent;
	if (qos_queue_set(qos_dev, q_node_id, &conf)) {
		PR_ERR("qos_queue_set fail for node_id=%d to parent=%d\n",
		       q_node_id, info->parent);
		return -1;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS, "To attach q_node=%d to parent_node=%d\n",
		 q_node_id, conf.queue_child_prop.parent);
	if (qos_queue_info_get(qos_dev, q_node_id, &q_info)) {
		PR_ERR("qos_queue_info_get fail for queue node_id=%d\n",
		       q_node_id);
		return -1;
	}
	info->qid = q_info.physical_id;
	info->node_id = q_node_id;
	DP_DEBUG(DP_DBG_FLAG_QOS, "Attached q[%d/%d] to parent_node=%d\n",
		 q_info.physical_id, q_node_id,
		 info->parent);
	return 0;
}

int init_ppv4_qos(int inst, int flag)
{
	union qos_init {
		struct pp_qos_port_conf p_conf;
		struct pp_qos_queue_conf q_conf;
		struct pp_qos_queue_info q_info;
		struct pp_qos_init_param param;
	};
	union qos_init *t = NULL;
	int res = DP_FAILURE, i;
	struct hal_priv *priv = HAL(inst);
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_QOS_HAL)
	unsigned int q, idx;
	struct cbm_tx_push *flush_port;
	struct cbm_cpu_port_data cpu_data = {0};
#endif

	if (!priv) {
		PR_ERR("priv is NULL\n");
		return DP_FAILURE;
	}
	if (!(flag & DP_PLATFORM_INIT)) {
		/*need to implement de-initialization for qos later*/
		priv->qdev = NULL;
		return DP_SUCCESS;
	}
	priv->qdev = qos_dev_open(dp_port_prop[inst].qos_inst);
	if (!priv->qdev) {
		PR_ERR("Could not open qos instance %d\n",
		       dp_port_prop[inst].qos_inst);
		return DP_FAILURE;
	}
	t = kzalloc(sizeof(*t), GFP_ATOMIC);
	if (!t) {
		PR_ERR("kzalloc fail: %d bytes\n", sizeof(*t));
		return DP_FAILURE;
	}
	t->param.wred_total_avail_resources = 0x38000;
	t->param.wred_p_const = 512;
	t->param.wred_max_q_size = 0x3fffff;
	/*reserve all ppv4 port to 1:1 sequnce link cqm port */
	for (i = 0; i < ARRAY_SIZE(t->param.reserved_ports); i++)
		t->param.reserved_ports[i] = 1;
	res = qos_dev_init(priv->qdev, &t->param);
	if (res) {
		PR_ERR("qos_dev_init fail for qos inst %d\n",
		       dp_port_prop[inst].qos_inst);
		goto EXIT;
	}
	if (cbm_cpu_port_get(&cpu_data, 0)) {
		PR_ERR("cbm_cpu_port_get for CPU port?\n");
		goto EXIT;
	}
	/* Sotre drop/flush port's info */
	flush_port = &cpu_data.dq_tx_flush_info;
	idx = flush_port->deq_port;
	if ((idx == 0) || (idx >= ARRAY_SIZE(dp_deq_port_tbl[inst]))) {
		PR_ERR("Wrog DP Flush port[%d]\n", idx);
		goto EXIT;
	}
	priv->cqm_drop_p = idx;
	dp_deq_port_tbl[inst][idx].tx_pkt_credit = flush_port->tx_pkt_credit;
	dp_deq_port_tbl[inst][idx].txpush_addr =
				(void *)flush_port->txpush_addr;
	dp_deq_port_tbl[inst][idx].txpush_addr_qos =
				(void *)flush_port->txpush_addr_qos;
	dp_deq_port_tbl[inst][idx].tx_ring_size = flush_port->tx_ring_size;
	dp_deq_port_tbl[inst][idx].dp_port = 0;/* dummy one */
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "DP Flush port[%d]: ring addr/push=0x%px/0x%px size=%d pkt_credit=%d\n",
		 priv->cqm_drop_p, dp_deq_port_tbl[inst][idx].txpush_addr_qos,
		 dp_deq_port_tbl[inst][idx].txpush_addr,
		 dp_deq_port_tbl[inst][idx].tx_ring_size,
		 dp_deq_port_tbl[inst][idx].tx_pkt_credit);
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_QOS_HAL)
	DP_DEBUG(DP_DBG_FLAG_DBG, "priv=%p deq_port_stat=%p q_dev=%p\n",
		 priv, priv ? priv->deq_port_stat : NULL,
		 priv ? priv->qdev : NULL);
	if (qos_port_allocate(priv->qdev,
			      priv->cqm_drop_p,
			      &priv->ppv4_drop_p)) {
		PR_ERR("Failed to alloc  qos drop port=%d\n",
		       priv->cqm_drop_p);
		goto EXIT;
	}
	qos_port_conf_set_default(&t->p_conf);
	t->p_conf.port_parent_prop.arbitration = PP_QOS_ARBITRATION_WSP;
	t->p_conf.ring_address =
	(void *)dp_deq_port_tbl[inst][idx].txpush_addr_qos;
	t->p_conf.ring_size = dp_deq_port_tbl[inst][idx].tx_ring_size;
	t->p_conf.packet_credit_enable = 1;
	t->p_conf.credit = dp_deq_port_tbl[inst][idx].tx_pkt_credit;
	t->p_conf.disable = 1; /*not allowed for dequeue*/

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		DP_DEBUG(DP_DBG_FLAG_QOS,
			 "qos_port_set param: %d/%d for drop pot:\n",
			 priv->cqm_drop_p, priv->ppv4_drop_p);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  arbitration=%d\n",
			 t->p_conf.port_parent_prop.arbitration);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  ring_address=0x%x\n",
			 (unsigned int)t->p_conf.ring_address);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  ring_size=%d\n",
			 t->p_conf.ring_size);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  packet_credit_enable=%d\n",
			 t->p_conf.packet_credit_enable);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  credit=%d\n",
			 t->p_conf.credit);
		DP_DEBUG(DP_DBG_FLAG_QOS, "  disabled=%d\n",
			 t->p_conf.disable);
	}
#endif
	if (qos_port_set(priv->qdev, priv->ppv4_drop_p, &t->p_conf)) {
		PR_ERR("qos_port_set fail for port(cqm/qos) %d/%d\n",
		       priv->cqm_drop_p, priv->ppv4_drop_p);
		qos_port_remove(priv->qdev, priv->ppv4_drop_p);
		goto EXIT;
	}

	if (qos_queue_allocate(priv->qdev, &q)) {
		PR_ERR("qos_queue_allocate fail\n");
		qos_port_remove(priv->qdev, q);
		goto EXIT;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS, "ppv4_drop_q alloc ok q_node=%d\n", q);

	qos_queue_conf_set_default(&t->q_conf);
	t->q_conf.blocked = 1; /*drop mode */
	t->q_conf.wred_enable = 0;
	t->q_conf.queue_wred_max_allowed = 0; /*max qocc in pkt */
	t->q_conf.queue_child_prop.parent = priv->ppv4_drop_p;
	if (qos_queue_set(priv->qdev, q, &t->q_conf)) {
		PR_ERR("qos_queue_set fail for node_id=%d to parent=%d\n",
		       q, t->q_conf.queue_child_prop.parent);
		goto EXIT;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS, "To attach q_node=%d to parent_node=%d\n",
		 q, priv->ppv4_drop_p);
	if (qos_queue_info_get(priv->qdev, q, &t->q_info)) {
		PR_ERR("qos_queue_info_get fail for queue node_id=%d\n",
		       q);
		goto EXIT;
	}
	priv->ppv4_drop_q = t->q_info.physical_id;
	DP_DEBUG(DP_DBG_FLAG_QOS, "Drop queue q[%d/%d] to parent=%d/%d\n",
		 priv->ppv4_drop_q, q,
		 priv->cqm_drop_p,
		 priv->ppv4_drop_p);
#endif /* end of CONFIG_INTEL_DATAPATH_QOS_HAL */
	DP_DEBUG(DP_DBG_FLAG_DBG, "init_ppv4_qos done\n");
	res = DP_SUCCESS;

EXIT:
	kfree(t);
	t = NULL;
	return res;
}

/**
 * __mark_ppv4_port - Mark the allocated ppv4 ports.
 * @inst:	DP instance ID.
 * @*priv:	hal private structure info.
 * @base:	base of the continuous allocated ports.
 * @mark:	no of ports allocated to be marked as 1.
 **/
static void __mark_ppv4_port(int inst, int base, int mark)
{
	int tmp;
	struct hal_priv *priv = HAL(inst);

	for (tmp = base; (tmp < mark) && (tmp < MAX_PPV4_PORTS); tmp++)
		priv->ppv4_port_map.flags[tmp] = 1;
}

/**
 * ppv4_alloc_port_31 -	Allocate continuous requested deq_ports.
 * @inst:			DP instance ID
 * @deq_port_num:	no'of continuous PPV4 ports to be allocated.
 *
 * Returns the base of the continuous allocated ports.
 * else -ERROR.
 **/
int ppv4_alloc_port_31(int inst, int deq_port_num)
{
	u32 base, match;
	struct hal_priv *priv = HAL(inst);

	for (base = 0; base < MAX_PPV4_PORTS; base++) {
		for (match = 0; (match < deq_port_num) && ((base + match)
		     < MAX_PPV4_PORTS); match++) {
			if (priv->ppv4_port_map.flags[base + match])
				break;
		}
		if (match == deq_port_num) {
			__mark_ppv4_port(inst, base, (base + match));
			return (base + PPV4_PORT_BASE);
		}
	}
	return DP_FAILURE; /* port not found */
}

/**
 * ppv4_port_free_31 - Unmark the ppv4 ports for inst.
 * @inst:			DP instance ID.
 * @base:			base of continuous allocated PPV4 ports.
 * @deq_port_num:	no'of continuous PPV4 ports allocated.
 *
 * Free the ports allocated by ppv4_alloc_port_31() by marking zero.
 **/
int ppv4_port_free_31(int inst, int base, int deq_port_num)
{
	u32 tmp;
	struct hal_priv *priv = HAL(inst);

	/* Expecting base always greater than PPV4_PORT_BASE */
	if (base < PPV4_PORT_BASE)
		return DP_FAILURE;

	base = base - PPV4_PORT_BASE;
	for (tmp = base; (tmp < MAX_PPV4_PORTS) && deq_port_num; tmp++) {
		priv->ppv4_port_map.flags[tmp] = 0;
		deq_port_num--;
	}

	return DP_SUCCESS;
}
