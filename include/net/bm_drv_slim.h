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
 *	driver:		buffer_manager platform device driver
 *	@file		bm_drv.h
 *	@brief:		ppv4 buffer manager implementation 
 *	@author:	obakshe
 *	@date:		5/3/2017
 */

#ifndef _BMGR_H_
#define _BMGR_H_

#include <net/pp_bm_regs.h>


#define FALCON_SOC

#ifdef FALCON_SOC
	/*! \def PP_BMGR_MAX_POOLS
	 *       Max supported pools
	 */
	#define PP_BMGR_MAX_POOLS		(4)

	/*! \def PP_BMGR_MAX_POOLS_IN_GROUP
	 *       Max pools in group
	 */
	#define PP_BMGR_MAX_POOLS_IN_GROUP	(4)

	/*! \def PP_BMGR_MAX_GROUPS
	 *       Max supported groups
	 */
	#define PP_BMGR_MAX_GROUPS		(2)

	/*! \def PP_BMGR_MAX_POLICIES
	 *       Max supoorted policies
	 */
	#define PP_BMGR_MAX_POLICIES		(32)
#else /* PPv4 */
	/*! \def PP_BMGR_MAX_POOLS
	 *       Max supported pools
	 */
	#define PP_BMGR_MAX_POOLS		(16)

	/*! \def PP_BMGR_MAX_POOLS_IN_GROUP
	 *       Max pools in group
	 */
	#define PP_BMGR_MAX_POOLS_IN_GROUP	(4)

	/*! \def PP_BMGR_MAX_GROUPS
	 *       Max supported groups
	 */
	#define PP_BMGR_MAX_GROUPS		(16)

	/*! \def PP_BMGR_MAX_POLICIES
	 *       Max supoorted policies
	 */
	#define PP_BMGR_MAX_POLICIES		(256)
#endif

//-----------------------------------------------------------------------------------------------------------------------------------------------
//
// ########  ######## ######## #### ##    ## ########  ######
// ##     ## ##       ##        ##  ###   ## ##       ##    ##
// ##     ## ##       ##        ##  ####  ## ##       ##
// ##     ## ######   ######    ##  ## ## ## ######    ######
// ##     ## ##       ##        ##  ##  #### ##             ##
// ##     ## ##       ##        ##  ##   ### ##       ##    ##
// ########  ######## ##       #### ##    ## ########  ######
//
//-----------------------------------------------------------------------------------------------------------------------------------------------

/*! \def BMGR_DRIVER_VERSION */
#define BMGR_DRIVER_VERSION			"1.0.0"

/*! \def BMGR_MIN_POOL_BUFFER_SIZE
 *       Minimum buffer size in configured pool
 */
#define BMGR_MIN_POOL_BUFFER_SIZE		(64)

/*! \def BMGR_START_PCU_SRAM_ADDR 
 *       Start PCU address in SRAM. Used in confiure pool
 */
#define BMGR_START_PCU_FIFO_SRAM_ADDR		(0)

/*! \def BMGR_DEFAULT_PCU_FIFO_SIZE 
 *       PCU fifo size
 */
#define BMGR_DEFAULT_PCU_FIFO_SIZE		(0x80)

/*! \def BMGR_DEFAULT_PCU_FIFO_LOW_THRESHOLD 
 *       PCU fifo low threshold
 */
#define BMGR_DEFAULT_PCU_FIFO_LOW_THRESHOLD	(1)

/*! \def BMGR_DEFAULT_PCU_FIFO_HIGH_THRESHOLD 
 *       PCU fifo high threshold
 */
#define BMGR_DEFAULT_PCU_FIFO_HIGH_THRESHOLD	(0x70)

/*! \def BMGR_DEFAULT_WATERMARK_LOW_THRESHOLD 
 *       Watermark low threshold
 */
#define BMGR_DEFAULT_WATERMARK_LOW_THRESHOLD	(0x200)

//-----------------------------------------------------------------------------------------------------------------------------------------------
//
//  ######  ######## ########  ##     ##  ######  ######## ##     ## ########  ########  ######
// ##    ##    ##    ##     ## ##     ## ##    ##    ##    ##     ## ##     ## ##       ##    ##
// ##          ##    ##     ## ##     ## ##          ##    ##     ## ##     ## ##       ##
//  ######     ##    ########  ##     ## ##          ##    ##     ## ########  ######    ######
//       ##    ##    ##   ##   ##     ## ##          ##    ##     ## ##   ##   ##             ##
// ##    ##    ##    ##    ##  ##     ## ##    ##    ##    ##     ## ##    ##  ##       ##    ##
//  ######     ##    ##     ##  #######   ######     ##     #######  ##     ## ########  ######
//
//-----------------------------------------------------------------------------------------------------------------------------------------------

/**************************************************************************
 *! \enum	bmgr_policy_pools_priority_e
 **************************************************************************
 *
 * \brief enum to describe the pool's priority
 *
 **************************************************************************/
enum bmgr_policy_pools_priority_e {
	bmgr_policy_pool_priority_high,		//!< High priority
	bmgr_policy_pool_priority_mid_high,	//!< Mid-High priority
	bmgr_policy_pool_priority_mid_low,	//!< Mid-Low priority
	bmgr_policy_pool_priority_low,		//!< Low priority
	bmgr_policy_pool_priority_max		//!< Last priority
};

/*! \def POOL_ENABLE_FOR_MIN_GRNT_POLICY_CALC 
 *       bmgr pools flags (Used in bmgr_pool_params.pool_flags)
 *       When set pool will be take part in policy minimum guaranteed calculation
 */
#define POOL_ENABLE_FOR_MIN_GRNT_POLICY_CALC	BIT(0)

/**************************************************************************
 *! \struct	bmgr_pool_params
 **************************************************************************
 *
 * \brief This structure is used in bmgr_pool_configure API in parameter
 *
 **************************************************************************/
struct bmgr_pool_params {
	u16	flags;			//!< Pool flags
	u8	group_id;		//!< Group index which the pool belong to
	u32	num_buffers;		//!< Amount of buffers in pool
	u32	size_of_buffer;		//!< Buffer size for this pool (in bytes). Minimum is 64 bytes
	u32	base_addr_low;		//!< Base address of the pool (low)
	u32	base_addr_high;		//!< Base address of the pool (high)
};

/**************************************************************************
 *! \struct	bmgr_group_params
 **************************************************************************
 *
 * \brief This structure is used for buffer manager database
 *
 **************************************************************************/
struct bmgr_group_params {
	u32	available_buffers;		//!< available buffers in group
	u32	reserved_buffers;		//!< reserved buffers in this group
	u8	pools[PP_BMGR_MAX_POOLS];	//!< Pools in policy
};

/**************************************************************************
 *! \struct	bmgr_pool_in_policy_info
 **************************************************************************
 *
 * \brief	This structure is used in policy_params struct and holds
 * 		the information about the pools in policy 
 *
 **************************************************************************/
struct bmgr_pool_in_policy_info {
	u8	pool_id;	//!< Pool id
	u8	max_allowed;	//!< Max allowed per pool per policy
};

/*! \def POLICY_FLAG_RESERVED1 
 *       bmgr policy flags (Used in bmgr_policy_param.policy_flags)
 */
#define POLICY_FLAG_RESERVED1	BIT(0)

/**************************************************************************
 *! \struct	bmgr_policy_params
 **************************************************************************
 *
 * \brief This structure is used in bmgr_policy_configure API in parameter
 *
 **************************************************************************/
struct bmgr_policy_params {
	u16				flags;						//!< Policy flags
	u8				group_id;					//!< Group index
	u32				max_allowed;					//!< Policy maximum allowed
	u32				min_guaranteed;					//!< Policy minimum guaranteed
	struct bmgr_pool_in_policy_info	pools_in_policy[PP_BMGR_MAX_POOLS_IN_POLICY];	//!< Pools information. Sorted according to priority - highest in index 0
	u8				num_pools_in_policy;				//!< Number of pools in pools_in_policy
};

/**************************************************************************
 *! \struct	bmgr_buff_info
 **************************************************************************
 *
 * \brief This structure is used for allocate and deallocate buffer
 *
 **************************************************************************/
struct bmgr_buff_info {
	u32	addr_low;	//!< [Out] buffer pointer low
	u32	addr_high;	//!< [Out] buffer pointer high
	u8	policy_id;	//!< policy to allocate from
	u8	pool_id;	//!< pool for deallocate buffer back
	u8	is_burst;	//!< Single/Burst allocation/s
//	u8	num_allocs;	//!< number of pointers to allocate (up to 32 pointers)
};

/**************************************************************************
 *! \struct	bmgr_pool_db_entry
 **************************************************************************
 *
 * \brief This structure holds the pool database
 *
 **************************************************************************/
struct bmgr_pool_db_entry {
	struct bmgr_pool_params	pool_params;			//!< Pool params
	u32			num_allocated_buffers;		//!< Number of allocated buffers
	u32			num_deallocated_buffers;	//!< Number of deallocated buffers
	u8			is_busy;			//!< Is entry in used
	void*			internal_pointers_tables;	//!< Pointers table to be used in HW
};

/**************************************************************************
 *! \struct	bmgr_group_db_entry
 **************************************************************************
 *
 * \brief This structure holds the group database
 *
 **************************************************************************/
struct bmgr_group_db_entry {
	u32	available_buffers;			//!< available buffers in group
	u32	reserved_buffers;			//!< reserved buffers in this group
	u8	pools[PP_BMGR_MAX_POOLS_IN_GROUP];	//!< Pools in policy (if set, pool is part of this group)
	u8	num_pools_in_group;			//!< Number of pools in group
};

/**************************************************************************
 *! \struct	bmgr_policy_db_entry
 **************************************************************************
 *
 * \brief This structure holds the policy database
 *
 **************************************************************************/
struct bmgr_policy_db_entry {
	struct bmgr_policy_params	policy_params;			//!< Policy params
	u32				num_allocated_buffers;		//!< Number of allocated buffers
	u32				num_deallocated_buffers;	//!< Number of deallocated buffers
	u8				is_busy;			//!< Is entry in used
};

/**************************************************************************
 *! \struct	bmgr_driver_db
 **************************************************************************
 *
 * \brief This structure holds the buffer manager database
 *
 **************************************************************************/
struct bmgr_driver_db {
	struct bmgr_pool_db_entry	bmgr_db_pools[PP_BMGR_MAX_POOLS];	//!< Pools information
	struct bmgr_group_db_entry	bmgr_db_groups[PP_BMGR_MAX_GROUPS];	//!< Groups information
	struct bmgr_policy_db_entry	bmgr_db_policies[PP_BMGR_MAX_POLICIES];	//!< Policies information

	// general counters
	u32				num_active_pools;			//!< Number of active pools
	u32				num_active_groups;			//!< Number of active groups
	u32				num_active_policies;			//!< Number of active policies

	// spinlock
	spinlock_t			db_lock;				//!< spinlock
};

/**************************************************************************
 *! \struct	bmgr_driver_private
 **************************************************************************
 *
 * \brief This struct defines the driver's private data
 *
 **************************************************************************/
struct bmgr_driver_private {
	struct platform_device*		pdev;		//!< Platform device pointer
	struct kobject*			kobj;		//!< Sysfs kobject
	int				enabled;	//!< Is driver enabled
	struct bmgr_driver_db		driver_db;	//!< Platform device DB
};

//-----------------------------------------------------------------------------------------------------------------------------------------------
//
// ########  ########   #######  ########  #######  ######## ##    ## ########  ########  ######
// ##     ## ##     ## ##     ##    ##    ##     ##    ##     ##  ##  ##     ## ##       ##    ##
// ##     ## ##     ## ##     ##    ##    ##     ##    ##      ####   ##     ## ##       ##
// ########  ########  ##     ##    ##    ##     ##    ##       ##    ########  ######    ######
// ##        ##   ##   ##     ##    ##    ##     ##    ##       ##    ##        ##             ##
// ##        ##    ##  ##     ##    ##    ##     ##    ##       ##    ##        ##       ##    ##
// ##        ##     ##  #######     ##     #######     ##       ##    ##        ########  ######
//
//-----------------------------------------------------------------------------------------------------------------------------------------------

/**************************************************************************
 *! \fn	bmgr_driver_init
 **************************************************************************
 *
 *  \brief	Initializes the buffer manager driver.
 *  		Must be the first driver's function to be called
 *
 *  \return	PP_RC_SUCCESS on success, other error code on failure
 *
 **************************************************************************/
s32 bmgr_driver_init(void);

/**************************************************************************
 *! \fn	bmgr_pool_configure
 **************************************************************************
 *
 *  \brief	Configure a Buffer Manager pool
 *
 *  \param	pool_params:	Pool param from user
 *  \param	pool_id[OUT]:	Pool ID
 *
 *  \return	PP_RC_SUCCESS on success, other error code on failure
 *
 **************************************************************************/
s32 bmgr_pool_configure(const struct bmgr_pool_params* const pool_params, u8* const pool_id);

/**************************************************************************
 *! \fn	bmgr_policy_configure
 **************************************************************************
 *
 *  \brief	Configure a Buffer Manager policy
 *
 *  \param	policy_params:	Policy param from user
 *  \param	policy_id[OUT]:	Policy ID
 *
 *  \return	PP_RC_SUCCESS on success, other error code on failure
 *
 **************************************************************************/
s32 bmgr_policy_configure(const struct bmgr_policy_params* const policy_params, u8* const policy_id);

/**************************************************************************
 *! \fn	bmgr_pop_buffer
 **************************************************************************
 *
 *  \brief	Pops a buffer from the buffer manager
 *
 *  \param	buff_info:	Buffer information
 *
 *  \return	PP_RC_SUCCESS on success, other error code on failure
 *
 **************************************************************************/
s32 bmgr_pop_buffer(struct bmgr_buff_info* const buff_info);

/**************************************************************************
 *! \fn	bmgr_push_buffer
 **************************************************************************
 *
 *  \brief	Pushes a buffer back to the buffer manager
 *
 *  \param	buff_info:	Buffer information
 *
 *  \return	PP_RC_SUCCESS on success, other error code on failure
 *
 **************************************************************************/
s32 bmgr_push_buffer(struct bmgr_buff_info* const buff_info);
s32 qos_config(void *qmgr_buf);

void new_init_bm(void);
#endif /* _BMGR_H_ */
