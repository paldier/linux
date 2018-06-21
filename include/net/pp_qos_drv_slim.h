/*
 * pp_qos_drv.h
 * Description:
 * PP QoS Manager driver definitions
 *
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2017 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */
#ifndef PPQOSDRV_H_
#define PPQOSDRV_H_

#include <linux/types.h>

#define PP_QOS_MAX_NODES			(2048)
#ifdef FALCON_SOC
	#define PP_QOS_MAX_PORT_NODES	(128)
	#define PP_QOS_MAX_QUEUE_NODES	(256)
#else /* PPv4 */
	#define PP_QOS_MAX_PORT_NODES	(256)
	#define PP_QOS_MAX_QUEUE_NODES	(512)
#endif
//#define BIT(x)						(1<<(x))

/* enum qos_node_type_e:
 * Describing the QoS node type
 */
enum qos_node_type_e {
	QOS_NODE_PORT,
	QOS_NODE_SCH,
	QOS_NODE_QUEUE,
	QOS_NODE_TYPE_MAX
};

/* enum qos_op_type_e:
 * Describing the qos_node_config API
 * Operation type
 */
enum qos_op_type_e {
	QOS_OP_ADD,
	QOS_OP_REMOVE,
	QOS_OP_MODIFY,
	QOS_OP_SUSPEND,
	QOS_OP_RESUME,
	QOS_OP_MOVE,
	QOS_OP_QUERY
};

/* enum qos_sch_type_e:
 * Describing the node
 * Scheduling type
 */
enum qos_sch_type_e {
	QOS_SCH_WSP_WRR,
	QOS_SCH_WFQ
};

/* struct qos_init_param:
 * This structure is using
 * For Initialized the QoS subsystem
 */
struct qos_init_param {
	u32	qm_base_addr;
	u32	qm_num_of_pages;
	u32	wred_prioritized_pop;
	u32	wred_p_const;
};

/* struct qos_node_conf:
 * General information for node
 * Configuration
 */
struct qos_node_conf {
	enum qos_node_type_e	node_type;
	enum qos_sch_type_e	sch_type;
	u16			node_id;
	u16			bw_limit_Mbps;
	u16			shared_bw_limit_group;
	u8			bw_allocation_weight;
	u8			priority;
	u16			parent_node_id;
	u16			child_node_id;
};

/* struct qos_port_conf:
 * Information for port
 * Configuration
 */
struct qos_port_conf {
	u32		port_conf_flags;
#define PORT_PARAM_FLAG_DISABLE_BYTES_CREDIT	BIT(0)
#define PORT_PARAM_FLAG_BYTES_CREDIT_SET		BIT(1)
#define PORT_PARAM_FLAG_PACKETS_CREDIT_SET		BIT(2)
#define PORT_PARAM_FLAG_RING_ADDR_SET			BIT(3)
#define PORT_PARAM_FLAG_RING_SIZE_SET			BIT(4)
	u32		port_tx_packets_credit;
	u32		port_tx_bytes_credit;
	u32		port_tx_ring_address;
	u32		port_tx_ring_size;
};

/* struct qos_queue_conf:
 * Information for queue
 * Configuration
 */
struct qos_queue_conf {
	u32		queue_conf_flags;
#define QUEUE_PARAM_FLAG_DISABLE_WRED				BIT(0)
#define QUEUE_PARAM_FLAG_WRED_MIN_AVG_GREEN_SET		BIT(1)
#define QUEUE_PARAM_FLAG_WRED_MAX_AVG_GREEN_SET		BIT(2)
#define QUEUE_PARAM_FLAG_WRED_SLOPE_GREEN_SET		BIT(3)
#define QUEUE_PARAM_FLAG_WRED_MIN_AVG_YELLOW_SET	BIT(4)
#define QUEUE_PARAM_FLAG_WRED_MAX_AVG_YELLOW_SET	BIT(5)
#define QUEUE_PARAM_FLAG_WRED_SLOPE_YELLOW_SET		BIT(6)
#define QUEUE_PARAM_FLAG_DISABLE_MIN_GUARANTEED		BIT(7)
#define QUEUE_PARAM_FLAG_MIN_GUARANTEED_SET			BIT(8)
#define QUEUE_PARAM_FLAG_DISABLE_MAX_ALLOWED		BIT(9)
#define QUEUE_PARAM_FLAG_MAX_ALLOWED_SET			BIT(10)
	u16		queue_wred_min_avg_green;
	u16		queue_wred_max_avg_green;
	u16		queue_wred_slope_green;
	u16		queue_wred_min_avg_yellow;
	u16		queue_wred_max_avg_yellow;
	u16		queue_wred_slope_yellow;
	u16		queue_wred_min_guaranteed;
	u16		queue_wred_max_allowed;
};

/* struct output_param:
 * Information for output
 * Parameters
 */
struct output_param {
	u32		output_flags;
#define OUTPUT_PARAM_FLAG_NODE_ID_SET		BIT(0)
#define OUTPUT_PARAM_FLAG_PHY_QUEUE_ID_SET	BIT(1)
	u16		node_id;
	u16		queue_id;
};

/* struct qos_node_api_param:
 * This structure is using as
 * qos_node_config API parameter
 */
struct qos_node_api_param {
	enum qos_op_type_e		op_type;
	u32				node_conf_flags;
	int 				deq_port;
#define NODE_PARAM_FLAG_NODE_ID_SET		BIT(0)
#define NODE_PARAM_FLAG_BW_LIMIT_SET		BIT(1)
#define NODE_PARAM_FLAG_SBW_LIMIT_SET		BIT(2)
#define NODE_PARAM_FLAG_BW_ALLOCATION_SET	BIT(3)
#define NODE_PARAM_FLAG_PRIORITY_SET		BIT(4)
#define NODE_PARAM_FLAG_PARENT_NODE_SET		BIT(5)
#define NODE_PARAM_FLAG_CHILD_NODE_SET		BIT(6)
#define NODE_PARAM_FLAG_EXTRA_PORT_CONF_SET	BIT(7)
#define NODE_PARAM_FLAG_EXTRA_QUEUE_CONF_SET	BIT(8)
	struct qos_node_conf	node_conf;
	struct qos_port_conf	port_conf;
	struct qos_queue_conf	queue_conf;
	struct output_param	out_param;
};

/* struct qos_node_stats:
 * Information for node statistics
 */
struct qos_node_stats {
	u16	object_id;
	u16	stats_req_flags;
#define STATS_FLAG_GET_BY_NODE_ID			BIT(0)
#define STATS_FLAG_GET_BY_QUEUE_ID			BIT(1)
#define STATS_FLAG_GET_BY_PORT_ID			BIT(2)
#define STATS_FLAG_GET_TOTAL_FWD_PACKETS	BIT(3)
#define STATS_FLAG_GET_TOTAL_FWD_BYTES		BIT(4)
#define STATS_FLAG_GET_TOTAL_DRP_PACKETS	BIT(5)
#define STATS_FLAG_GET_TOTAL_DRP_BYTES		BIT(6)
#define STATS_FLAG_GET_CURR_Q_PACKETS_SIZE	BIT(7)
#define STATS_FLAG_GET_CURR_Q_BYTES_SIZE	BIT(8)
#define STATS_FLAG_GET_CURR_WRED_AVG_SIZE	BIT(9)
#define STATS_FLAG_GET_CURR_WRED_DRP_PROB_G	BIT(10)
#define STATS_FLAG_GET_CURR_WRED_DRP_PROB_Y	BIT(11)
#define STATS_FLAG_RESET_COUNTERS			BIT(12)
#define STATS_FLAG_GET_ALL_COUNTERS			BIT(15)
	u32	total_fwd_pkt;
	u64	total_fwd_byte;
	u32	total_drp_pkt;
	u64	total_drp_byte;
	u32	curr_q_pkt_size; /* Available only for node type queue */
	u32	curr_q_byte_size; /* Available only for node type queue */
	u32	curr_wred_avg_size; /* Available only for node type queue */
	u32	curr_wred_drp_prob_green; /* Available only for node type queue */
	u32	curr_wred_drp_prob_yellow; /* Available only for node type queue */
};

/* struct qos_node_api_param:
 * enum qos_op_type_e		op_type;				Add/Remove/Modify/Suspend/Resume/Move/Query
 * u32						node_conf_flags;
 * NODE_PARAM_FLAG_NODE_ID_SET						Node ID set
 * NODE_PARAM_FLAG_BW_LIMIT_SET						BW Limit set
 * NODE_PARAM_FLAG_SBW_LIMIT_SET					Shared BW Limit set
 * NODE_PARAM_FLAG_BW_ALLOCATION_SET				BW Allocation set
 * NODE_PARAM_FLAG_PRIORITY_SET						Priority set
 * NODE_PARAM_FLAG_PARENT_NODE_SET					Parent Node ID set
 * NODE_PARAM_FLAG_CHILD_NODE_SET					Child Node ID set
 * NODE_PARAM_FLAG_EXTRA_PORT_CONF_SET				Extra Port Configuration set
 * NODE_PARAM_FLAG_EXTRA_QUEUE_CONF_SET				Extra Queue Configuration set
 *
 * struct qos_node_conf:
 * enum qos_node_type_e	node_type;					Port/Scheduler/Queue Node
 * enum qos_sch_type_e		sch_type;				Not relevant for queue: WSP+WRR, WFQ [Cannot be modified]
 * u16						node_id;				Logical Node ID For Input [0:2047]
 * u16						bw_limit_Mbps;			0 = No BW Limit applied
 * u16						shared_bw_limit_group;	[0-511], Shared BW limit group ID
 * u8						bw_allocation_weight;	0 = Best Effort [1 in credit]
 * u8						priority;				[0..7], 0xFF = WRR, not relevant for WFQ
 * u16						parent_node_id;			Relevant only for Add operation for Scheduler/Queue
 * u16						child_node_id;			Optional.
 *													Relevant only for Add operation in case Node need to be set up between parent and specific child.
 *													In this case, the added Node inherits all its non-configured attributes from the child node
 *
 * struct qos_port_conf:
 * u32	port_conf_flags;
 * PORT_PARAM_FLAG_DISABLE_BYTES_CREDIT				Disable bytes credit option
 * PORT_PARAM_FLAG_BYTES_CREDIT_SET					Bytes credit set
 * PORT_PARAM_FLAG_PACKETS_CREDIT_SET				Packet credit set
 * PORT_PARAM_FLAG_RING_ADDR_SET					Ring address set
 * PORT_PARAM_FLAG_RING_SIZE_SET					Ring size set
 * u32	port_tx_packets_credit;						TX Port Packet credit
 * u32	port_tx_bytes_credit;						TX Port Byte Credit
 * u32	port_tx_ring_address;						TX Port Ring address
 * u32	port_tx_ring_size;							TX Port Ring size
 *
 * struct qos_queue_conf:
 * u32	queue_conf_flags;
 * QUEUE_PARAM_FLAG_DISABLE_WRED					Disable WRED feature
 * QUEUE_PARAM_FLAG_WRED_MIN_AVG_GREEN_SET			WRED Min average green set
 * QUEUE_PARAM_FLAG_WRED_MAX_AVG_GREEN_SET			WRED Max average green set
 * QUEUE_PARAM_FLAG_WRED_SLOPE_GREEN_SET			WRED Slope green set
 * QUEUE_PARAM_FLAG_WRED_MIN_AVG_YELLOW_SET			WRED Min average yellow set
 * QUEUE_PARAM_FLAG_WRED_MAX_AVG_YELLOW_SET			WRED Max average yellow set
 * QUEUE_PARAM_FLAG_WRED_SLOPE_YELLOW_SET			WRED Slope yellow set
 * QUEUE_PARAM_FLAG_DISABLE_MIN_GUARANTEED			Disable Min guaranteed feature
 * QUEUE_PARAM_FLAG_MIN_GUARANTEED_SET				Min guaranteed set
 * QUEUE_PARAM_FLAG_DISABLE_MAX_ALLOWED				Disable Max allowed feature
 * QUEUE_PARAM_FLAG_MAX_ALLOWED_SET					Max allowed set
 * u16	queue_wred_min_avg_green;					WRED Min average green
 * u16	queue_wred_max_avg_green;					WRED Max average green
 * u16	queue_wred_slope_green;						WRED Slope green
 * u16	queue_wred_min_avg_yellow;					WRED Min average yellow
 * u16	queue_wred_max_avg_yellow;					WRED Max average yellow
 * u16	queue_wred_slope_yellow;					WRED Slope yellow
 * u16	queue_wred_min_guaranteed;					Min guaranteed
 * u16	queue_wred_max_allowed;						Max allowed
 *
 * struct output_param:
 * u32	output_flags;
 * OUTPUT_PARAM_FLAG_NODE_ID_SET					Logical Node ID set (by the QoS driver)
 * OUTPUT_PARAM_FLAG_PHY_QUEUE_ID_SET				Phisical Queue ID set (by the QoS driver)
 * u16	node_id;									Logical Node ID
 * u16	queue_id;									Phisical Queue ID
 */

/**
 * qos_node_config - Configure a QoS Node
 * This API is the main QoS driver API
 * Supported operation types are
 * Defined in enum qos_op_type_e
 * @param: API param from user
 **/
s32 qos_node_config(struct qos_node_api_param *param);

/**
 * qos_node_get_stats - Get Node stats structure
 * @param: Stats structure from user
 **/
s32 qos_node_get_stats(struct qos_node_stats *param);

/**
 * qos_init_subsystem - Initialized QoS subsystem
 * @param: Init structure from user
 **/
s32 qos_init_subsystem(struct qos_init_param *param);

#endif /* PPQOSDRV_H_ */
