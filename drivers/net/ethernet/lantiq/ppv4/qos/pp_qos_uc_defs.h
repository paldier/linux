/*
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
/*
 * uc_host_defs.h
 *
 *  Created on: Dec 17, 2017
 *      Author: obakshe
 */

#ifndef SRC_UC_HOST_DEFS_H_
#define SRC_UC_HOST_DEFS_H_

/* UC version */
#define UC_VERSION_MAJOR	(1)
#define UC_VERSION_MINOR	(0)
#define UC_VERSION_BUILD	(9)

/**************************************************************************
 *! @enum	UC_STATUS
 **************************************************************************
 *
 * @brief UC general status enum
 *
 **************************************************************************/
enum uc_status {
	/*!< Status OK */
	UC_STATUS_OK,

	/*!< General failure */
	UC_STATUS_GENERAL_FAILURE,

	/*!< Invalid user input */
	UC_STATUS_INVALID_INPUT,
};

/**************************************************************************
 *! @enum	UC_LOGGER_LEVEL
 **************************************************************************
 *
 * @brief UC Logger level enum. It is recommended to use the defines below
 * for presets
 *
 **************************************************************************/
enum uc_logger_level {
	//!< FATAL error occurred. SW will probably fail to proceed
	UC_LOGGER_LEVEL_FATAL_ONLY		=	0x01,

	//!< General ERROR occurred.
	UC_LOGGER_LEVEL_ERROR_ONLY		=	0x02,

	//!< WARNING
	UC_LOGGER_LEVEL_WARNING_ONLY		=	0x04,

	//!< Information print to the user
	UC_LOGGER_LEVEL_INFO_ONLY		=	0x08,

	//!< Debug purposes level
	UC_LOGGER_LEVEL_DEBUG_ONLY		=	0x10,

	//!< Dump all writings to registers
	UC_LOGGER_LEVEL_DUMP_REG_ONLY		=	0x20,

	//!< Dump all commands
	UC_LOGGER_LEVEL_COMMANDS_ONLY		=	0x40,
};

/* Below levels will be normally used from host. */
/* Each level includes all higher priorities levels messages */

//!< FATAL level messages
#define UC_LOGGER_LEVEL_FATAL	(UC_LOGGER_LEVEL_FATAL_ONLY	|	\
				UC_LOGGER_LEVEL_COMMANDS_ONLY)

//!< ERRORS level messages
#define UC_LOGGER_LEVEL_ERROR	(UC_LOGGER_LEVEL_FATAL_ONLY	|	\
				UC_LOGGER_LEVEL_ERROR_ONLY	|	\
				UC_LOGGER_LEVEL_COMMANDS_ONLY)

//!< WARNING level messages
#define UC_LOGGER_LEVEL_WARNING	(UC_LOGGER_LEVEL_FATAL_ONLY	|	\
				UC_LOGGER_LEVEL_ERROR_ONLY	|	\
				UC_LOGGER_LEVEL_WARNING_ONLY	|	\
				UC_LOGGER_LEVEL_COMMANDS_ONLY)

//!< INFO level messages
#define UC_LOGGER_LEVEL_INFO	(UC_LOGGER_LEVEL_FATAL_ONLY	|	\
				UC_LOGGER_LEVEL_ERROR_ONLY	|	\
				UC_LOGGER_LEVEL_WARNING_ONLY	|	\
				UC_LOGGER_LEVEL_INFO_ONLY	|	\
				UC_LOGGER_LEVEL_COMMANDS_ONLY)

//!< DEBUG level messages
#define UC_LOGGER_LEVEL_DEBUG	(UC_LOGGER_LEVEL_FATAL_ONLY	|	\
				UC_LOGGER_LEVEL_ERROR_ONLY	|	\
				UC_LOGGER_LEVEL_WARNING_ONLY	|	\
				UC_LOGGER_LEVEL_INFO_ONLY	|	\
				UC_LOGGER_LEVEL_DEBUG_ONLY	|	\
				UC_LOGGER_LEVEL_COMMANDS_ONLY)

//!< DUMP to registers level messages
#define UC_LOGGER_LEVEL_DUMP_REGS	(UC_LOGGER_LEVEL_FATAL_ONLY	| \
					UC_LOGGER_LEVEL_ERROR_ONLY	| \
					UC_LOGGER_LEVEL_WARNING_ONLY	| \
					UC_LOGGER_LEVEL_DUMP_REG_ONLY	| \
					UC_LOGGER_LEVEL_COMMANDS_ONLY)

/**************************************************************************
 *! @enum	UC_LOGGER_MODE
 **************************************************************************
 *
 * @brief UC Logger operation mode
 *
 **************************************************************************/
enum uc_logger_mode {
	/*!< Logger is disabled */
	UC_LOGGER_MODE_NONE,

	/*!< Messages are written to the standard output */
	UC_LOGGER_MODE_STDOUT,

	/*!< Local file. N/A */
//	UC_LOGGER_MODE_LOCAL_FILE,

	/*!< Messages are written to the host allocated memory */
	UC_LOGGER_MODE_WRITE_HOST_MEM,
};

/**************************************************************************
 *! \enum	TSCD_NODE_CONF
 **************************************************************************
 *
 * \brief TSCD node configuration valid bits. Used in modify existing node
 *
 **************************************************************************/
enum tscd_node_conf {
	/*!< None */
	TSCD_NODE_CONF_NONE					=	0x0000,

	/*!< Suspend/Resume node */
	TSCD_NODE_CONF_SUSPEND_RESUME		=	0x0001,

	/*!< first child (Not relevant for queue) */
	TSCD_NODE_CONF_FIRST_CHILD			=	0x0002,

	/*!< last child (Not relevant for queue) */
	TSCD_NODE_CONF_LAST_CHILD			=	0x0004,

	/*!< 0 - BW Limit disabled >0 - define BW */
	TSCD_NODE_CONF_BW_LIMIT				=	0x0008,

	/*!< Best Effort enable */
	TSCD_NODE_CONF_BEST_EFFORT_ENABLE	=	0x0010,

	/*!< First Weighted-Round-Robin node (Not relevant for queue) */
	TSCD_NODE_CONF_FIRST_WRR_NODE		=	0x0020,

	/*!< Node Weight (Not relevant for ports) */
	TSCD_NODE_CONF_NODE_WEIGHT			=	0x0040,

	/*!< Update predecessor 0 (Not relevant for port) */
	TSCD_NODE_CONF_PREDECESSOR_0		=	0x0080,

	/*!< Update predecessor 1 (Not relevant for port) */
	TSCD_NODE_CONF_PREDECESSOR_1		=	0x0100,

	/*!< Update predecessor 2 (Not relevant for port) */
	TSCD_NODE_CONF_PREDECESSOR_2		=	0x0200,

	/*!< Update predecessor 3 (Not relevant for port) */
	TSCD_NODE_CONF_PREDECESSOR_3		=	0x0400,

	/*!< Update predecessor 4 (Not relevant for port) */
	TSCD_NODE_CONF_PREDECESSOR_4		=	0x0800,

	/*!< Update predecessor 5 (Not relevant for port) */
	TSCD_NODE_CONF_PREDECESSOR_5		=	0x1000,

	/*!< Shared BW limit group (0: no shared BW limit, 1-511: group ID) */
	TSCD_NODE_CONF_SHARED_BWL_GROUP		=	0x2000,

	//!< Set if Queue's port was changed (Relevant only for queue)
	TSCD_NODE_CONF_SET_PORT_TO_QUEUE	=	0x4000,

	/*!< All flags are set */
	TSCD_NODE_CONF_ALL					=	0xFFFF
};

/**************************************************************************
 *! \enum	WRED_QUEUE_CONF
 **************************************************************************
 *
 * \brief WRED queue configuration valid bits. Used in modify existing queue
 *
 **************************************************************************/
enum wred_queue_conf {
	/*!< None */
	WRED_QUEUE_CONF_NONE				=	0x0000,

	/*!< Q is active */
	WRED_QUEUE_CONF_ACTIVE_Q			=	0x0001,

	/*!< Disable flags valid */
	WRED_QUEUE_CONF_DISABLE				=	0x0002,

	/*!< Use fixed green drop probability */
	WRED_QUEUE_CONF_FIXED_GREEN_DROP_P	=	0x0004,

	/*!< Use fixed yellow drop probability */
	WRED_QUEUE_CONF_FIXED_YELLOW_DROP_P	=	0x0008,

	/*!< Min average yellow */
	WRED_QUEUE_CONF_MIN_AVG_YELLOW		=	0x0010,

	/*!< Max average yellow */
	WRED_QUEUE_CONF_MAX_AVG_YELLOW		=	0x0020,

	/*!< Slope yellow */
	WRED_QUEUE_CONF_SLOPE_YELLOW		=	0x0040,

	/*!< INTERNAL CONFIGURATION. SHOULD NOT BE SET BY HOST */
	WRED_QUEUE_CONF_SHIFT_AVG_YELLOW	=	0x0080,

	/*!< Min average green */
	WRED_QUEUE_CONF_MIN_AVG_GREEN		=	0x0100,

	/*!< Max average green */
	WRED_QUEUE_CONF_MAX_AVG_GREEN		=	0x0200,

	/*!< Slope green */
	WRED_QUEUE_CONF_SLOPE_GREEN			=	0x0400,

	/*!< INTERNAL CONFIGURATION. SHOULD NOT BE SET BY HOST */
	WRED_QUEUE_CONF_SHIFT_AVG_GREEN		=	0x0800,

	/*!< Min guaranteed */
	WRED_QUEUE_CONF_MIN_GUARANTEED		=	0x1000,

	/*!< max allowed */
	WRED_QUEUE_CONF_MAX_ALLOWED			=	0x2000,

	/*!< All flags are set */
	WRED_QUEUE_CONF_ALL					=	0xFFFF
};

/**************************************************************************
 *! \enum	PORT_CONF
 **************************************************************************
 *
 * \brief Port configuration valid bits. Used in modify existing port
 *
 **************************************************************************/
enum port_conf {
	/*!< None */
	PORT_CONF_NONE					=	0x0000,

	/*!< Ring Size */
	PORT_CONF_RING_SIZE				=	0x0001,

	/*!< Ring address high */
	PORT_CONF_RING_ADDRESS_HIGH		=	0x0002,

	/*!< Ring address low */
	PORT_CONF_RING_ADDRESS_LOW		=	0x0004,

	/*!< Enable port */
	PORT_CONF_ACTIVE				=	0x0008,

	/*!< All flags are set */
	PORT_CONF_ALL					=	0xFFFF
};

/**************************************************************************
 *! \struct	port_stats_s
 **************************************************************************
 *
 * \brief Port stats
 *
 **************************************************************************/
struct port_stats_s {
	u32	total_green_bytes;
	u32	total_yellow_bytes;

	/* Following stats can not be reset */
	u32	debug_back_pressure_status;
};

/**************************************************************************
 *! \struct	hw_node_info_s
 **************************************************************************
 *
 * \brief HW node info
 *
 **************************************************************************/
struct hw_node_info_s {
	u32	first_child;
	u32	last_child;
	u32	is_suspended;
	u32	bw_limit;
	u32	predecessor0;
	u32	predecessor1;
	u32	predecessor2;
	u32	predecessor3;
	u32	predecessor4;
	u32	predecessor5;
	u32	queue_physical_id;
	u32	queue_port;
};

/**************************************************************************
 *! \enum	PORT_STATS_CLEAR_FLAGS
 **************************************************************************
 *
 * \brief	port stats clear flags.
 *			Used in get port stats command to set which stats
 *			will be reset after read
 *
 **************************************************************************/
enum port_stats_clear_flags {
	/*!< None */
	PORT_STATS_CLEAR_NONE					=	0x0000,

	/*!< Clear port total green bytes stats */
	PORT_STATS_CLEAR_TOTAL_GREEN_BYTES		=	0x0001,

	/*!< Clear port total yellow bytes stats */
	PORT_STATS_CLEAR_TOTAL_YELLOW_BYTES		=	0x0002,

	/*!< All above stats will be cleared */
	PORT_STATS_CLEAR_ALL					=	0xFFFF,
};

/**************************************************************************
 *! \struct	queue_stats_s
 **************************************************************************
 *
 * \brief Queue stats
 *
 **************************************************************************/
struct queue_stats_s {
	u32	queue_size_bytes;
	u32	queue_average_size_bytes;
	u32	queue_size_entries;
	u32	drop_p_yellow;
	u32	drop_p_green;
	u32	total_bytes_added_low;
	u32	total_bytes_added_high;
	u32	total_accepts;
	u32	total_drops;
	u32	total_dropped_bytes_low;
	u32	total_dropped_bytes_high;
	u32	total_red_dropped;

	/* Following stats can not be reset */
	u32	qmgr_num_queue_entries;
	u32	qmgr_null_pop_queue_counter;
	u32	qmgr_empty_pop_queue_counter;
	u32	qmgr_null_push_queue_counter;
};

/**************************************************************************
 *! \enum	QUEUE_STATS_CLEAR_FLAGS
 **************************************************************************
 *
 * \brief	queue stats clear flags.
 *			Used in get queue stats command to set which stats
 *			will be reset after read
 *
 **************************************************************************/
enum queue_stats_clear_flags {
	/*!< None */
	QUEUE_STATS_CLEAR_NONE					=	0x0000,

	/*!< Clear queue size bytes stats */
	QUEUE_STATS_CLEAR_Q_SIZE_BYTES			=	0x0001,

	/*!< Clear queue average size bytes stats */
	QUEUE_STATS_CLEAR_Q_AVG_SIZE_BYTES		=	0x0002,

	/*!< Clear queue size entries stats */
	QUEUE_STATS_CLEAR_Q_SIZE_ENTRIES		=	0x0004,

	/*!< Clear drop probability yellow stats */
	QUEUE_STATS_CLEAR_DROP_P_YELLOW			=	0x0008,

	/*!< Clear drop probability green stats */
	QUEUE_STATS_CLEAR_DROP_P_GREEN			=	0x0010,

	/*!< Clear total bytes added stats */
	QUEUE_STATS_CLEAR_TOTAL_BYTES_ADDED		=	0x0020,

	/*!< Clear total accepts stats */
	QUEUE_STATS_CLEAR_TOTAL_ACCEPTS			=	0x0040,

	/*!< Clear total drops stats */
	QUEUE_STATS_CLEAR_TOTAL_DROPS			=	0x0080,

	/*!< Clear total dropped bytes stats */
	QUEUE_STATS_CLEAR_TOTAL_DROPPED_BYTES	=	0x0100,

	/*!< Clear total RED drops stats */
	QUEUE_STATS_CLEAR_TOTAL_RED_DROPS		=	0x0200,

	/*!< All above stats will be cleared */
	QUEUE_STATS_CLEAR_ALL					=	0xFFFF,
};

/**************************************************************************
 *! \struct	system_stats_s
 **************************************************************************
 *
 * \brief system stats
 *
 **************************************************************************/
struct system_stats_s {
	u32	qmgr_cache_free_pages_counter;
	u32	qmgr_sm_current_state;
	u32	qmgr_cmd_machine_busy;
	u32	qmgr_cmd_machine_pop_busy;
	u32	qmgr_null_pop_counter;
	u32	qmgr_empty_pop_counter;
	u32	qmgr_null_push_counter;
	u32	qmgr_ddr_stop_push_low_threshold;
	u32	qmgr_fifo_error_register;
	u32	qmgr_ocp_error_register;
	u32	qmgr_cmd_machine_sm_current_state_0;
	u32	qmgr_cmd_machine_sm_current_state_1;
	u32	qmgr_cmd_machine_sm_current_state_2;
	u32	qmgr_cmd_machine_sm_current_state_3;
	u32	qmgr_cmd_machine_sm_current_state_4;
	u32	qmgr_cmd_machine_sm_current_state_5;
	u32	qmgr_cmd_machine_sm_current_state_6;
	u32	qmgr_cmd_machine_sm_current_state_7;
	u32	qmgr_cmd_machine_sm_current_state_8;
	u32	qmgr_cmd_machine_sm_current_state_9;
	u32	qmgr_cmd_machine_sm_current_state_10;
	u32	qmgr_cmd_machine_sm_current_state_11;
	u32	qmgr_cmd_machine_sm_current_state_12;
	u32	qmgr_cmd_machine_sm_current_state_13;
	u32	qmgr_cmd_machine_sm_current_state_14;
	u32	qmgr_cmd_machine_sm_current_state_15;
	u32	qmgr_cmd_machine_queue_0;
	u32	qmgr_cmd_machine_queue_1;
	u32	qmgr_cmd_machine_queue_2;
	u32	qmgr_cmd_machine_queue_3;
	u32	qmgr_cmd_machine_queue_4;
	u32	qmgr_cmd_machine_queue_5;
	u32	qmgr_cmd_machine_queue_6;
	u32	qmgr_cmd_machine_queue_7;
	u32	qmgr_cmd_machine_queue_8;
	u32	qmgr_cmd_machine_queue_9;
	u32	qmgr_cmd_machine_queue_10;
	u32	qmgr_cmd_machine_queue_11;
	u32	qmgr_cmd_machine_queue_12;
	u32	qmgr_cmd_machine_queue_13;
	u32	qmgr_cmd_machine_queue_14;
	u32	qmgr_cmd_machine_queue_15;

	u32	tscd_num_of_used_nodes;

	/* Error in Scheduler tree configuration */
	u32	tscd_infinite_loop_error_occurred;

	/* HW failed to complete the bwl credits updates */
	u32	tscd_bwl_update_error_occurred;

	/* Quanta size in KB */
	u32	tscd_quanta;
};

/**************************************************************************
 *! @enum	UC_QOS_COMMAND
 **************************************************************************
 *
 * @brief UC QOS command enum. Must be synced with the Host definition
 *
 **************************************************************************/
enum uc_qos_command {
	UC_QOS_COMMAND_GET_FW_VERSION,
	UC_QOS_COMMAND_MULTIPLE_COMMANDS,
	UC_QOS_COMMAND_INIT_UC_LOGGER,
	UC_QOS_COMMAND_SET_UC_LOGGER_LEVEL,
	UC_QOS_COMMAND_INIT_QOS,
	UC_QOS_COMMAND_ADD_PORT,
	UC_QOS_COMMAND_REMOVE_PORT,
	UC_QOS_COMMAND_ADD_SCHEDULER,
	UC_QOS_COMMAND_REMOVE_SCHEDULER,
	UC_QOS_COMMAND_ADD_QUEUE,
	UC_QOS_COMMAND_REMOVE_QUEUE,
	UC_QOS_COMMAND_FLUSH_QUEUE,
	UC_QOS_COMMAND_SET_PORT,
	UC_QOS_COMMAND_SET_SCHEDULER,
	UC_QOS_COMMAND_SET_QUEUE,
	UC_QOS_COMMAND_MOVE_SCHEDULER,
	UC_QOS_COMMAND_MOVE_QUEUE,
	UC_QOS_COMMAND_GET_PORT_STATS,
	UC_QOS_COMMAND_GET_QUEUE_STATS,
	UC_QOS_COMMAND_GET_SYSTEM_STATS,
	UC_QOS_COMMAND_ADD_SHARED_BW_LIMIT_GROUP,
	UC_QOS_COMMAND_REMOVE_SHARED_BW_LIMIT_GROUP,
	UC_QOS_COMMAND_SET_SHARED_BW_LIMIT_GROUP,
	UC_QOS_COMMAND_GET_NODE_INFO,
	UC_QOS_COMMAND_DEBUG_READ_NODE,
	UC_QOS_COMMAND_DEBUG_PUSH_DESC,
	UC_QOS_COMMAND_DEBUG_ADD_CREDIT_TO_PORT,
	UC_QOS_COMMAND_GET_ACTIVE_QUEUES_STATS,
};

/**************************************************************************
 *! @struct	uc_qos_cmd_s
 **************************************************************************
 *
 * @brief UC commands.
 * This structure defines the Host <-->UC interface
 *
 **************************************************************************/
struct uc_qos_cmd_s {
	/*!< Type of command (UC_QOS_COMMAND) */
	u32			type;

	/*!< Commands flags */
	u32			flags;
#define	UC_CMD_FLAG_IMMEDIATE				BIT(0)
#define	UC_CMD_FLAG_BATCH_FIRST				BIT(1)
#define	UC_CMD_FLAG_BATCH_LAST				BIT(2)
#define	UC_CMD_FLAG_MULTIPLE_COMMAND_LAST	BIT(3)
#define	UC_CMD_FLAG_UC_DONE					BIT(4)
#define	UC_CMD_FLAG_UC_ERROR				BIT(5)

	/*!< Number of 32bit parameters available for this command. */
	/* must be synced between the host and uc! */
	u32			num_params;

	u32			param0;
	u32			param1;
	u32			param2;
	u32			param3;
	u32			param4;
	u32			param5;
	u32			param6;
	u32			param7;
	u32			param8;
	u32			param9;
	u32			param10;
	u32			param11;
	u32			param12;
	u32			param13;
	u32			param14;
	u32			param15;
	u32			param16;
	u32			param17;
	u32			param18;
	u32			param19;
	u32			param20;
	u32			param21;
	u32			param22;
	u32			param23;
	u32			param24;
	u32			param25;
};

#endif /* SRC_UC_HOST_DEFS_H_ */
