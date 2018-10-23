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

#ifdef CONFIG_LTQ_DATAPATH_DUMMY_QOS
struct fixed_q_port {
	int deq_port; /*cqm dequeue port */
	int queue_id; /*queue physical id*/
	int port_node; /*qos dequeue port node id */
	int queue_node; /*queue node id */
	int q_used;    /*flag to indicate used or free*/
};

struct fixed_q_port q_port[] = {
	{0, 14, 0, 2, 0},
	{0, 74, 0, 4, 0},
	{0, 30, 0, 6, 0},
	{0, 87, 0, 8, 0},
	{7, 235, 7, 10, 0},
	{7, 42, 7, 12, 0},
	{7, 242, 7, 14, 0},
	{26, 190, 26, 16, 0},
	{26, 119, 26, 18, 0},
	{26, 103, 26, 20, 0}
};

static struct pp_qos_dev qdev;

int test_qos_queue_remove(struct pp_qos_dev *qos_dev, unsigned int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(q_port); i++) {
		if (q_port[i].queue_id == id) {
			q_port[i].q_used = PP_NODE_FREE;
			DP_DEBUG(DP_DBG_FLAG_DBG, "to free qid=%d\n", id);
			return 0;
		}
	}
	return -1;
}

int test_qos_queue_allocate(struct pp_qos_dev *qos_dev, unsigned int *id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(q_port); i++) {
		if ((q_port[i].deq_port == qos_dev->dq_port) &&
		    (q_port[i].q_used == PP_NODE_FREE)) {
			q_port[i].q_used = PP_NODE_ALLOC;
			DP_DEBUG(DP_DBG_FLAG_DBG, "allocate qnode_id=%d\n",
				 q_port[i].queue_node);
			*id = q_port[i].queue_node;
			return 0;
		}
	}
	return -1;
}

int test_qos_queue_info_get(struct pp_qos_dev *qos_dev, unsigned int id,
			    struct pp_qos_queue_info *info)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(q_port); i++) {
		if (q_port[i].queue_node == id) {
			DP_DEBUG(DP_DBG_FLAG_DBG, "q[%d]'s qid=%d\n",
				 id, q_port[i].queue_id);
			info->physical_id = q_port[i].queue_id;
			return 0;
		}
	}
	return -1;
}

int test_qos_port_remove(struct pp_qos_dev *qos_dev, unsigned int id)
{
	return 0;
}

int test_qos_sched_allocate(struct pp_qos_dev *qos_dev, unsigned int *id)
{
	return 0;
}

int test_qos_sched_remove(struct pp_qos_dev *qos_dev, unsigned int id)
{
	return 0;
}

int test_qos_port_allocate(struct pp_qos_dev *qos_dev, unsigned int physical_id,
			   unsigned int *id)
{
	int i;

	if (!id)
		return -1;

	for (i = 0; i < ARRAY_SIZE(q_port); i++) {
		if (physical_id == q_port[i].deq_port) {
			*id = q_port[i].port_node;
			DP_DEBUG(DP_DBG_FLAG_DBG,
				 "Ok: Deq_port/Node_id=%d/%d\n",
				 physical_id, *id);
			return 0;
		}
	}
	return -1;
}

int test_qos_port_set(struct pp_qos_dev *qos_dev, unsigned int id,
		      const struct pp_qos_port_conf *conf)
{
	return 0;
}

void test_qos_port_conf_set_default(struct pp_qos_port_conf *conf)
{
}

void test_qos_queue_conf_set_default(struct pp_qos_queue_conf *conf)
{
}

int test_qos_queue_set(struct pp_qos_dev *qos_dev, unsigned int id,
		       const struct pp_qos_queue_conf *conf)
{
	return 0;
}

void test_qos_sched_conf_set_default(struct pp_qos_sched_conf *conf)
{
}

int test_qos_sched_set(struct pp_qos_dev *qos_dev, unsigned int id,
		       const struct pp_qos_sched_conf *conf)
{
	return 0;
}

int test_qos_queue_conf_get(struct pp_qos_dev *qos_dev, unsigned int id,
			    struct pp_qos_queue_conf *conf)
{
	int i;

	if (!conf)
		return -1;
	memset(conf, 0, sizeof(*conf));
	for (i = 0; i < ARRAY_SIZE(q_port); i++) {
		if (id == q_port[i].queue_node) {
			conf->queue_child_prop.parent = q_port[i].port_node;
			conf->common_prop.bandwidth_limit = 0;
			conf->blocked = 0;
			return 0;
		}
	}
	return -1;
}

int test_qos_queue_flush(struct pp_qos_dev *qos_dev, unsigned int id)
{
	return 0;
}

int test_qos_sched_conf_get(struct pp_qos_dev *qos_dev, unsigned int id,
			    struct pp_qos_sched_conf *conf)
{
	return -1;
}

int test_qos_sched_get_queues(struct pp_qos_dev *qos_dev, unsigned int id,
			      u16 *queue_ids, unsigned int size,
			    unsigned int *queues_num)
{
	return 0;
}

int test_qos_port_get_queues(struct pp_qos_dev *qos_dev, unsigned int id,
			     u16 *queue_ids, unsigned int size,
			   unsigned int *queues_num)
{
	int i, num = 0;

	for (i = 0; i < ARRAY_SIZE(q_port); i++) {
		if (q_port[i].port_node != id)
			continue;
		if (queue_ids && (num < size)) {
			queue_ids[num] = q_port[i].queue_node;
			DP_DEBUG(DP_DBG_FLAG_QOS,
				 "saved[%d] qid[%d/%d] for cqm[%d/%d]\n",
				 num,
				 q_port[i].queue_id,
				 q_port[i].queue_node,
				 q_port[i].deq_port,
				 q_port[i].port_node);
		} else {
			DP_DEBUG(DP_DBG_FLAG_QOS,
				 "unsaved[%d]: qid[%d/%d] for cqm[%d/%d]\n",
				 num,
				 q_port[i].queue_id,
				 q_port[i].queue_node,
				 q_port[i].deq_port,
				 q_port[i].port_node);
		}
		num++;
	}
	if (queues_num)
		*queues_num = num;
	return 0;
}

int test_qos_port_conf_get(struct pp_qos_dev *qdev, unsigned int id,
			   struct pp_qos_port_conf *conf)
{
	return 0;
}

int test_qos_port_info_get(struct pp_qos_dev *qdev, unsigned int id,
			   struct pp_qos_port_info *info)
{
	return 0;
}

/*this test code is only support one instance */
struct pp_qos_dev *test_qos_dev_open(unsigned int id)
{
	return &qdev;
}

int test_qos_dev_init(struct pp_qos_dev *qos_dev,
		      struct pp_qos_init_param *conf)
{
	return 0;
}

#ifdef DUMMY_PPV4_QOS_API_OLD
s32 qos_node_config(struct qos_node_api_param *param)
{
	int i;

	if (param->op_type != QOS_OP_ADD)  {/*only support ADD op */
		PR_ERR("Wrong op_type=%d\n", param->op_type);
		return -1;
	}
	if (param->node_conf.node_type == (enum qos_node_type_e)DP_NODE_PORT) {
		for (i = 0; i < ARRAY_SIZE(q_port); i++) {
			if (param->deq_port == q_port[i].deq_port) {
				param->out_param.node_id = q_port[i].port_node;
				DP_DEBUG(DP_DBG_FLAG_DBG,
					 "Ok: Deq_port/Node_id=%d/%d\n",
					 param->deq_port,
					 param->out_param.node_id);
				return 0;
			}
		}
		PR_ERR("QoS Port Add failed for deq_port=%d\n",
		       param->deq_port);
		return -1;
	}

	if (param->node_conf.node_type == (enum qos_node_type_e)DP_NODE_QUEUE) {
		for (i = 0; i < ARRAY_SIZE(q_port); i++) {
			if (q_port[i].q_used)
				continue;
			if (param->node_conf.parent_node_id ==
						q_port[i].port_node) {
				param->out_param.node_id = q_port[i].queue_node;
				param->out_param.queue_id = q_port[i].queue_id;
				q_port[i].q_used = 1;
				DP_DEBUG(DP_DBG_FLAG_DBG,
					 "Ok:Deq_port/Node_id=%d/%d %s=%d/%d\n",
					 q_port[i].deq_port,
					 param->node_conf.parent_node_id,
					 "Queue_id/Node",
					 param->out_param.queue_id,
					 param->out_param.node_id);
				return 0;
			}
		}
	}
	PR_ERR("QoS Queue Add failed for parent node=%d\n",
	       param->node_conf.parent_node_id);
	return -1;
}
#endif /*DUMMY_PPV4_QOS_API_OLD*/

#endif /*CONFIG_LTQ_DATAPATH_DUMMY_QOS*/

void init_qos_fn(void)
{
#ifdef CONFIG_LTQ_DATAPATH_DUMMY_QOS
	qos_queue_remove = test_qos_queue_remove;
	qos_queue_allocate = test_qos_queue_allocate;
	qos_queue_info_get = test_qos_queue_info_get;
	qos_port_remove = test_qos_port_remove;
	qos_sched_allocate = test_qos_sched_allocate;
	qos_sched_remove = test_qos_sched_remove;
	qos_port_allocate = test_qos_port_allocate;
	qos_port_set = test_qos_port_set;
	qos_port_conf_set_default = test_qos_port_conf_set_default;
	qos_queue_conf_set_default = test_qos_queue_conf_set_default;
	qos_queue_set = test_qos_queue_set;
	qos_sched_conf_set_default = test_qos_sched_conf_set_default;
	qos_sched_set = test_qos_sched_set;
	qos_queue_conf_get = test_qos_queue_conf_get;
	qos_queue_flush = test_qos_queue_flush;
	qos_sched_conf_get = test_qos_sched_conf_get;
	qos_sched_get_queues = test_qos_sched_get_queues;
	qos_port_get_queues = test_qos_port_get_queues;
	qos_port_conf_get = test_qos_port_conf_get;
	qos_dev_open = test_qos_dev_open;
	qos_dev_init = test_qos_dev_init;
#elif CONFIG_LTQ_PPV4_QOS
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
#endif /*CONFIG_LTQ_DATAPATH_DUMMY_QOS*/
}

#ifdef DUMMY_PPV4_QOS_API_OLD
/*Port Add
 * Input
 *    Operation Type: Add
 *    Operation Flags
 *       BW Limit param set
 *       Extra Port Configuration param set
 *    Node information
 *       Node Type: Port
 *       Scheduling Type:WSP(Weighted Strict Priority)+WRR(W Round Robin),WFQ
 *       BW Limit in Mbps (0 = No BW Limit applied)
 *    Extra Port Configuration:
 *       TX manager configuration:
 *          TX manager configuration flags
 *            Packet credit set
 *            Byte credit attributes set
 *            Ring address set
 *            Ring size set
 *          TX Port Packet credit
 *          Port Byte Credit EN/DIS
 *          If Port Byte Credit EN ' TX Port Byte credit
 *          TX Port Ring address
 *          TX Port Ring size
 * Output
 *    Logical Node ID
 */
int dp_pp_alloc_port(struct ppv4_port *info)
{
	struct qos_node_api_param param = {0};
	struct hal_priv *priv = HAL(info->inst);

	param.op_type = QOS_OP_ADD;
	param.node_conf_flags = NODE_PARAM_FLAG_BW_LIMIT_SET |
		NODE_PARAM_FLAG_EXTRA_PORT_CONF_SET;

	param.node_conf.node_type = DP_NODE_PORT;
	param.node_conf.sch_type = QOS_SCH_WSP_WRR;
	/*NODE_PARAM_FLAG_BW_LIMIT_SET */
	param.node_conf.bw_limit_Mbps = 0; /*No limit*/
	//param.node_conf.bw_allocation_weight = 0; /* ??? not valid for port */
	//param.node_conf.priority = 0; /* ??? not valid for port */

	/*NODE_PARAM_FLAG_EXTRA_PORT_CONF_SET*/
	param.port_conf.port_conf_flags =
			PORT_PARAM_FLAG_DISABLE_BYTES_CREDIT; /* ??*/
	param.port_conf.port_tx_packets_credit = info->tx_pkt_credit;
	param.port_conf.port_tx_ring_address = info->tx_ring_addr;
	param.port_conf.port_tx_ring_size = info->tx_ring_size;
	param.deq_port = info->cqm_port;

	if (qos_node_config(&param)) {
		PR_ERR("Failed to alloc QoS for deq_port=%d\n", param.deq_port);
		return -1;
	}
	priv->deq_port_stat[info->cqm_port].flag = PP_NODE_ALLOC;
	priv->deq_port_stat[info->cqm_port].node_id = param.out_param.node_id;
	info->node_id = param.out_param.node_id;
	return 0;
}

/*Scheduler Add
 * Input
 *    Operation Type: Add
 *    Operation Flags
 *      BW Allocation param set
 *      Priority param set
 *      Parent Node ID param set
 *    Node information
 *      Node Type: Scheduler
 *      Scheduling Type: WSP+WRR, WFQ
 *      BW Allocation weight (0 = Best Effort [1 in credit])
 *      Priority (0-15, 0xFF = WRR, not relevant for WFQ)
 *      Parent Node ID
 *    Output
 *      Logical Node ID
 */
int dp_pp_alloc_sched(struct ppv4_scheduler *info)
{
	struct qos_node_api_param param = {0};

	param.op_type = QOS_OP_ADD;
	param.node_conf_flags = NODE_PARAM_FLAG_BW_ALLOCATION_SET |
		NODE_PARAM_FLAG_PARENT_NODE_SET |
		NODE_PARAM_FLAG_PRIORITY_SET;

	param.node_conf.node_type = QOS_NODE_SCH;
	param.node_conf.sch_type = QOS_SCH_WSP_WRR;
	/*NODE_PARAM_FLAG_BW_ALLOCATION_SET */
	param.node_conf.bw_allocation_weight = 0; /*best effort*/

	/* NODE_PARAM_FLAG_PRIORITY_SET */
	param.node_conf.priority = 0xFF; /* ??? */

	/*NODE_PARAM_FLAG_PARENT_NODE_SET */
	param.node_conf.parent_node_id = info->parent;

	if (qos_node_config(&param)) {
		PR_ERR("Failed to alloc ppv4 scheduler\n");
		return -1;
	}
	info->node_id = param.out_param.node_id;
	return 0;
}

/* Queue Add
 * Input
 *   Operation Type: Add
 *     Operation Flags
 *       BW Allocation param set/Priority param set/Parent Node ID param set
 *       Extra Queue Configuration param set
 *   Node information
 *      Node Type: Queue
 *      BW Allocation weight (0 = Best Effort [1 in credit])
 *      Priority (0-15, 0xFF = WRR, not relevant for WFQ)
 *      Parent Node ID
 *  Extra Queue Configuration:
 *    WRED(Weighted Random Early Drop) configuration
 *     WRED configuration flags
 *      WRED DIS
 *      Following flags are relevant only if this flag is not set
 *        Min average green set
 *        Max average green set
 *        Slope green set
 *        Min average yellow set
 *        Max average yellow set
 *        Slope yellow set
 *      Min guaranteed DIS
 *      Min guaranteed set (Relevant only if Min guaranteed DIS is clear)
 *      Max allowed DIS
 *      Max allowed set (Relevant only if Max allowed DIS is clear)
 *     Min average green
 *     Max average green
 *     Slope green
 *     Min average yellow
 *     Max average yellow
 *     Slope yellow
 *     Min guaranteed
 *     Max allowed
 */
int dp_pp_alloc_queue(struct ppv4_queue *info)
{
	struct qos_node_api_param param = {0};

	param.op_type = QOS_OP_ADD;
	param.node_conf_flags = NODE_PARAM_FLAG_BW_ALLOCATION_SET |
		NODE_PARAM_FLAG_PRIORITY_SET |
		NODE_PARAM_FLAG_PARENT_NODE_SET |
		NODE_PARAM_FLAG_EXTRA_QUEUE_CONF_SET;

	param.node_conf.node_type = DP_NODE_QUEUE;

	/*NODE_PARAM_FLAG_BW_ALLOCATION_SET */
	param.node_conf.bw_allocation_weight = 0; /*best effort*/

	/* NODE_PARAM_FLAG_PRIORITY_SET */
	param.node_conf.priority = 0; /* ??? */

	/*NODE_PARAM_FLAG_PARENT_NODE_SET */
	param.node_conf.parent_node_id = info->parent;

	/* NODE_PARAM_FLAG_EXTRA_QUEUE_CONF_SET */
	param.queue_conf.queue_conf_flags = QUEUE_PARAM_FLAG_DISABLE_WRED |
		QUEUE_PARAM_FLAG_DISABLE_MIN_GUARANTEED |
		QUEUE_PARAM_FLAG_DISABLE_MAX_ALLOWED;

	if (qos_node_config(&param)) {
		PR_ERR("Failed to alloc ppv4 queue\n");
		return -1;
	}
	info->qid = param.out_param.queue_id;
	info->node_id = param.out_param.node_id;
	return 0;
}

#else /*DUMMY_PPV4_QOS_API_OLD*/

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
	conf.port_parent_prop.arbitration = PP_QOS_ARBITRATION_WRR;
	conf.ring_address = (void *)info->tx_ring_addr;
	conf.ring_size = info->tx_ring_size;
	conf.packet_credit_enable = 1;
	conf.credit = info->tx_pkt_credit;
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DBG)
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

#ifdef CONFIG_LTQ_DATAPATH_DUMMY_QOS
	qos_dev->dq_port = info->dq_port;
#endif
	if (qos_queue_allocate(qos_dev, &q_node_id)) {
#ifdef CONFIG_LTQ_DATAPATH_DUMMY_QOS
		PR_ERR("qos_queue_allocate fail for dq_port %d\n",
		       info->dq_port);
#else
		PR_ERR("qos_queue_allocate fail\n");
#endif
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
#endif /*DUMMY_PPV4_QOS_API_OLD*/

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
#ifdef CONFIG_LTQ_DATAPATH_QOS_HAL
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
	t->param.wred_total_avail_resources = 0x10000;
	t->param.wred_p_const = 512;
	t->param.wred_max_q_size = 10000;
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
	dp_deq_port_tbl[inst][idx].tx_ring_addr = flush_port->tx_ring_addr;
	dp_deq_port_tbl[inst][idx].tx_ring_size = flush_port->tx_ring_size;
	dp_deq_port_tbl[inst][idx].dp_port = 0;/* dummy one */
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "DP Flush port[%d]: ring addr=0x%x size=%d pkt_credit=%d\n",
		 priv->cqm_drop_p, dp_deq_port_tbl[inst][idx].tx_ring_addr,
		 dp_deq_port_tbl[inst][idx].tx_ring_size,
		 dp_deq_port_tbl[inst][idx].tx_pkt_credit);
#ifdef CONFIG_LTQ_DATAPATH_QOS_HAL
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
	t->p_conf.port_parent_prop.arbitration = PP_QOS_ARBITRATION_WRR;
	t->p_conf.ring_address =
	(void *)dp_deq_port_tbl[inst][idx].tx_ring_addr;
	t->p_conf.ring_size = dp_deq_port_tbl[inst][idx].tx_ring_size;
	t->p_conf.packet_credit_enable = 1;
	t->p_conf.credit = dp_deq_port_tbl[inst][idx].tx_pkt_credit;
	t->p_conf.disable = 1; /*not allowed for dequeue*/

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_DBG)
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
#endif /* end of CONFIG_LTQ_DATAPATH_QOS_HAL */
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

/**
 * ppv4_alloc_ring_31 - Allocate ring buffer for port
 * @size:	size of the descriptor.
 * @phy:	Phy addr of ring.
 * @virt:	Virt addr of the ring.
 *
 * Allocate the ring buffer of @size(*DP_TXIN_RING_SIZE_DEF) requested by
 * caller.
 **/
int ppv4_alloc_ring_31(int size, void **phy, void **virt)
{
	*virt = kmalloc(DP_TXIN_RING_SIZE_DEF * size, GFP_KERNEL);
	if (!*virt)
		return DP_FAILURE;

	*phy = virt_to_phys(*virt);

	return DP_SUCCESS;
}

/**
 * ppv4_ring_free_31 - Free ring buffer.
 * @ptr:	virt addr of the ring.
 *
 * Free the ring buffer allocated by ppv4_alloc_ring_31().
 **/
void ppv4_ring_free_31(void *ptr)
{
	kfree(ptr);
}
