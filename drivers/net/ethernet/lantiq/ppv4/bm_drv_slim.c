/*
 *	driver:		buffer_manager platform device driver
 *	@file		bmgr_drv.c
 *	@brief:		Interim ppv4 buffer manager driver
 *	 	based on register configuration given by SOC verification team
 *	@author:	Kavitha
 *	@date:		25/08/2017
 */

#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/device.h>
#include <linux/inetdevice.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/byteorder/generic.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/pid.h>
#include <linux/pid_namespace.h>
#include <linux/module.h>
#include <linux/init.h>
#include <lantiq.h>
#include <lantiq_soc.h>
#include <net/pp_bm_regs.h>
#include <net/bm_drv_slim.h>


#define BM_CHIPTEST_DRIVER
//#undef BM_CHIPTEST_DRIVER

#ifdef BM_CHIPTEST_DRIVER
#define xrx500_bm_r32(m_reg) 0
#define xrx500_bm_w32(m_reg, val)
#define xrx500_bm_w32_mask(m, clear, set, reg)
#define set_val_bm(reg, val, mask, offset)
#else
#define xrx500_bm_r32(m_reg)		__raw_readl(m_reg)
#define xrx500_bm_w32(m_reg, val)	__raw_writel(val, m_reg)
#define xrx500_bm_w32_mask(m, clear, set, reg) \
		ltq_w32((ltq_r32((m) + (reg)) & ~(clear)) | (set), \
		(m) + (reg))

#define set_val_bm(reg, val, mask, offset) \
do {\
	xrx500_bm_w32(reg, xrx500_bm_r32(reg) & ~(mask));\
	xrx500_bm_w32(reg, xrx500_bm_r32(reg)\
	| (((val) << (offset)) & (mask)));\
} while (0)

#endif

void __iomem *bm_config_addr_base;
void __iomem *bm_policy_mngr_addr_base;

#define BM_BASE	(bm_config_addr_base)
#define BM_RAM_BASE	(bm_policy_mngr_addr_base)

void*	pointers_table[PP_BMGR_MAX_POOLS] = {NULL};
void*	pointers_table_1 = NULL;
void*	pointers_table_2 = NULL;
void*	pointers_table_3 = NULL;
u32*	temp_pointers_table_ptr = NULL;
u32	user_array_ptr;
#define num_buf 128
struct device *g_bm_dev;

void* CONFIG_CQEM_BUF_BASE[PP_BMGR_MAX_POOLS];
void* CONFIG_CQEM_BUF_LLK_BASE[PP_BMGR_MAX_POOLS];
#define CQEM_FORCE_COUNT_NUM 0x80

static void buf_addr_adjust(unsigned int buf_base_addr,
		     unsigned int buf_size,
		     unsigned int *adjusted_buf_base,
		     unsigned int *adjusted_buf_size,
		     unsigned int align)
{
	unsigned int base;
	unsigned int size;
	base = ALIGN(buf_base_addr, align);
	size = buf_base_addr + buf_size - base;

	*adjusted_buf_base = base;
	*adjusted_buf_size = size;
}



static void qose_reg_rd_poll (void * reg, uint32_t val, uint32_t offset)
{
#ifndef BM_CHIPTEST_DRIVER
	uint32_t tmpreg;

	while (1) {
		tmpreg = xrx500_bm_r32(reg) & (1 << offset);
		if (tmpreg == (val << offset))
			break;
	}
#endif
}

#ifdef BM_CHIPTEST_DRIVER
static void qose_reg_rd_poll_2 (void * reg, uint32_t val, uint32_t offset)
{
	uint32_t tmpreg;

	while (1) {
		tmpreg = xrx500_bm_r32(reg) & (1 << offset);
		if (tmpreg == (val << offset))
			break;
	}
}
#endif

s32 bmgr_pool_configure(const struct bmgr_pool_params* const pool_params, u8* const pool_id)
{
	
	int index;
	int adjusted_size;
	u32 phy_ll_base;
	u32 reg;
	int l_pool_id = (int)*pool_id;
	void* bm_buf_llk_base;
	void* bm_buf_llk_base_adjust;
	dma_addr_t dma_handle;

	pr_info("Configuring buffer manager pool..%d\n", l_pool_id);

	pointers_table[l_pool_id] = kzalloc(sizeof(u32) * (pool_params->num_buffers + 64), GFP_KERNEL);
	buf_addr_adjust((u32)pointers_table[l_pool_id],
			sizeof(u32) * (pool_params->num_buffers + 64),
			(u32 *)&pointers_table[l_pool_id],
			&adjusted_size,
			64);
	if (pointers_table[l_pool_id] == NULL) {
		pr_info("bmgr_pool_configure: Failed to allocate pointers_table, num_buffers %d", pool_params->num_buffers);
		return -1;
	}
	temp_pointers_table_ptr = (u32 *)pointers_table[l_pool_id];
	user_array_ptr = pool_params->base_addr_low;
	for (index = 0; index < pool_params->num_buffers; index++) {
		*temp_pointers_table_ptr = user_array_ptr >> 6;
		temp_pointers_table_ptr++;
		user_array_ptr += pool_params->size_of_buffer;
	}
	phy_ll_base = dma_map_single(g_bm_dev, (void *)pointers_table[l_pool_id],
				      (pool_params->num_buffers * 4), DMA_TO_DEVICE);

	CONFIG_CQEM_BUF_BASE[l_pool_id] = (void*) pool_params->base_addr_low;
	bm_buf_llk_base = dma_alloc_attrs(g_bm_dev, sizeof(u32) * (pool_params->num_buffers + 64),
					  &dma_handle,
					  GFP_KERNEL,
					  0);

	if (!bm_buf_llk_base) {
		pr_err("Error in pool %d linklist allocation\n", l_pool_id);
		return -1;
	}
	buf_addr_adjust(bm_buf_llk_base,
			sizeof(u32) * (pool_params->num_buffers + 64),
			(u32 *)&bm_buf_llk_base_adjust, &adjusted_size,
			64);

	if (bm_buf_llk_base_adjust > bm_buf_llk_base)
		CONFIG_CQEM_BUF_LLK_BASE[l_pool_id] = dma_handle + (bm_buf_llk_base_adjust - bm_buf_llk_base);
	else
		CONFIG_CQEM_BUF_LLK_BASE[l_pool_id] = dma_handle - (bm_buf_llk_base - bm_buf_llk_base_adjust);

	xrx500_bm_w32(BMGR_POOL_SIZE_REG_ADDR(BM_BASE, l_pool_id), pool_params->num_buffers);
	xrx500_bm_w32(BMGR_POOL_EXT_FIFO_OCC_REG_ADDR(BM_BASE, l_pool_id), pool_params->num_buffers);
	xrx500_bm_w32(BMGR_POOL_EXT_FIFO_BASE_ADDR_LOW_REG_ADDR(BM_BASE, l_pool_id), phy_ll_base);
	xrx500_bm_w32(BMGR_POOL_EXT_FIFO_BASE_ADDR_HIGH_REG_ADDR(BM_BASE, l_pool_id), 0x00000000);
	reg = xrx500_bm_r32(BMGR_GROUP_AVAILABLE_BUFF_REG_ADDR(BM_BASE, pool_params->group_id));
	xrx500_bm_w32(BMGR_GROUP_AVAILABLE_BUFF_REG_ADDR(BM_BASE, pool_params->group_id), reg + pool_params->num_buffers);
	xrx500_bm_w32(BMGR_POOL_PCU_FIFO_BASE_ADDR_REG_ADDR(BM_BASE, l_pool_id),
		BMGR_START_PCU_FIFO_SRAM_ADDR + ((l_pool_id + 1) * BMGR_DEFAULT_PCU_FIFO_SIZE));
	xrx500_bm_w32(BMGR_POOL_PCU_FIFO_SIZE_REG_ADDR(BM_BASE, l_pool_id), 0x40);

#if 0
	status = bmgr_set_pcu_fifo_occupancy(l_pool_id, BMGR_DEFAULT_PCU_FIFO_SIZE);
	if (status != PP_RC_SUCCESS) {
		goto free_memory;
	}
#endif
	xrx500_bm_w32(BMGR_POOL_PCU_FIFO_PROG_EMPTY_REG_ADDR(BM_BASE, l_pool_id), BMGR_DEFAULT_PCU_FIFO_LOW_THRESHOLD);
	xrx500_bm_w32(BMGR_POOL_PCU_FIFO_PROG_FULL_REG_ADDR(BM_BASE, l_pool_id), 0x30);
	xrx500_bm_w32(BMGR_POOL_WATERMARK_LOW_THRESHOLD_REG_ADDR(BM_BASE, l_pool_id), 0x90);
	if (pool_params->flags & POOL_ENABLE_FOR_MIN_GRNT_POLICY_CALC) {
		set_val_bm(BMGR_POOL_MIN_GRNT_MASK_REG_ADDR(BM_BASE), 1, (1 << l_pool_id), l_pool_id);
	}

	if (l_pool_id == 3) {
		xrx500_bm_w32(BMGR_POOL_ENABLE_REG_ADDR(BM_BASE), 0x000f000f);
		xrx500_bm_w32(BMGR_CTRL_REG_ADDR(BM_BASE), 0x00000001);
		xrx500_bm_w32(BMGR_POOL_FIFO_RESET_REG_ADDR(BM_BASE), 0x0000fff0);
		qose_reg_rd_poll(BMGR_STATUS_REG_ADDR(BM_BASE), 0x0, 0x00);
	}
	pr_info("pool %d Done\n", l_pool_id);
	return 0;
}

s32 bmgr_policy_configure(const struct bmgr_policy_params* const policy_params, u8* const policy_id)
{
	int policy = *policy_id;
	pr_info("Configuring buffer manager policy..%d\n", policy);

	xrx500_bm_w32(BMGR_POLICY_GROUP_ASSOCIATED_ADDR(BM_RAM_BASE,policy), policy_params->group_id);

	xrx500_bm_w32(BMGR_POLICY_MAX_ALLOWED_ADDR(BM_RAM_BASE,policy), policy_params->max_allowed);
	xrx500_bm_w32(BMGR_POLICY_MIN_GUARANTEED_ADDR(BM_RAM_BASE,policy), policy_params->min_guaranteed);
	// Set the group's reserved buffers
	xrx500_bm_w32(BMGR_GROUP_RESERVED_BUFF_REG_ADDR(BM_BASE, policy_params->group_id), 0x00000100);

	/*for (index = 0; index < policy_params->num_pools_in_policy; index++) {
		xrx500_bm_w32(
			BMGR_POLICY_MAX_ALLOWED_PER_POOL_ADDR(BM_RAM_BASE, policy, policy_params->pools_in_policy[index].pool_id)
			, policy_params->pools_in_policy[index].max_allowed);
	}*/
	xrx500_bm_w32(BM_RAM_BASE + 0x4000, 0x00000080);//DONE
	xrx500_bm_w32(BM_RAM_BASE + 0x4008, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x4004, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x400c, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x500c, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x5000, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x5008, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x5004, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x600c, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x6000, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x6008, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x6004, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x700c, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x7000, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x7008, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x7004, 0x00000080);
	xrx500_bm_w32(BM_RAM_BASE + 0x700c, 0x00000080);

	
	xrx500_bm_w32(
		BMGR_POLICY_POOLS_MAPPING_ADDR(BM_RAM_BASE, 0)
		, 0x03020100);
	xrx500_bm_w32(
		BMGR_POLICY_POOLS_MAPPING_ADDR(BM_RAM_BASE, 1)
		, 0x00030201);
	xrx500_bm_w32(
		BMGR_POLICY_POOLS_MAPPING_ADDR(BM_RAM_BASE, 2)
		, 0x00000302);

	xrx500_bm_w32(
		BMGR_POLICY_POOLS_MAPPING_ADDR(BM_RAM_BASE, 3)
		, 0x00000003);

	qose_reg_rd_poll(BMGR_STATUS_REG_ADDR(BM_BASE), 0x0, 0x00);


	pr_info("Policy Done\n");
	return 0;


}

s32 bmgr_driver_init(void)
{
	pr_info("Buffer Manager driver is initializing....");
	xrx500_bm_w32(BMGR_CTRL_REG_ADDR(BM_BASE), 0x1); // Buffer manager client enable
	// OCP Master burst size
	xrx500_bm_w32(BMGR_OCPM_BURST_SIZE_REG_ADDR(BM_BASE), 0x48);
	// OCP Master number of bursts
	xrx500_bm_w32(BMGR_OCPM_NUM_OF_BURSTS_REG_ADDR(BM_BASE), 0x33); // 1 burst for all pools
	pr_info("Done\n");
	return 0;
}

#define REG32(addr) (*((volatile uint32_t*)(addr)))
void qose_reg_wr(uint32_t reg, uint32_t val)
{
        REG32(KSEG1ADDR(reg)) = val;
}

/**************************************************************************
 *! \fn	buffer_manager_probe
 **************************************************************************
 *
 *  \brief	probe platform device hook
 * 
 *  \param	pdev:	platform device pointer
 *
 *  \return	0 on success, other error code on failure
 *
 **************************************************************************/
static int buffer_manager_probe(struct platform_device *pdev)
{
	
	struct resource *res[2];
	int i;
	g_bm_dev = &pdev->dev;
#if 1
	/* load the memory ranges */
	for (i = 0; i < 2; i++) {
		res[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res[i]) {
			pr_err("failed to get resources %d\n", i);
			return -ENOENT;
		}
	}
#endif


	bm_config_addr_base = devm_ioremap_resource(&pdev->dev, res[0]);
	bm_policy_mngr_addr_base = devm_ioremap_resource(&pdev->dev, res[1]);

	if (!bm_config_addr_base || !bm_policy_mngr_addr_base) {
		pr_err("failed to request and remap io ranges\n");
		return -ENOMEM;
	}
	return 0;
}

#ifndef BM_CHIPTEST_DRIVER
void new_init_bm()
{
	return;
}
#else
void new_init_bm()
{
	uint32_t * base, *base_org;
	uint32_t count, i, val, val_org;

	qose_reg_wr(0x18b10000, 0x00000280);
	qose_reg_wr(0x18b10008, 0x00000280);
	qose_reg_wr(0x18b10004, 0x00000280);
	qose_reg_wr(0x18b1000c, 0x00000280);
	qose_reg_wr(0x18b11000, 0x00000040);
	qose_reg_wr(0x18b11008, 0x00000040);
	qose_reg_wr(0x18b11004, 0x00000040);
	qose_reg_wr(0x18b1100c, 0x00000040);
	qose_reg_wr(0x18b14000, 0x00000080);
	qose_reg_wr(0x18b14008, 0x00000080);
	qose_reg_wr(0x18b14004, 0x00000080);
	qose_reg_wr(0x18b1400c, 0x00000080);
	qose_reg_wr(0x18b1500c, 0x00000080);
	qose_reg_wr(0x18b15000, 0x00000080);
	qose_reg_wr(0x18b15008, 0x00000080);
	qose_reg_wr(0x18b15004, 0x00000080);
	qose_reg_wr(0x18b1600c, 0x00000080);
	qose_reg_wr(0x18b16000, 0x00000080);
	qose_reg_wr(0x18b16008, 0x00000080);
	qose_reg_wr(0x18b16004, 0x00000080);
	qose_reg_wr(0x18b1700c, 0x00000080);
	qose_reg_wr(0x18b17000, 0x00000080);
	qose_reg_wr(0x18b17008, 0x00000080);
	qose_reg_wr(0x18b17004, 0x00000080);
	qose_reg_wr(0x18b1700c, 0x00000080);
	qose_reg_wr(0x18b12000, 0x00000000);
	qose_reg_wr(0x18b12008, 0x00000000);
	qose_reg_wr(0x18b12004, 0x00000000);
	qose_reg_wr(0x18b1200c, 0x00000000);
	#if 1
	qose_reg_wr(0x18b13000, 0x03020100);
	qose_reg_wr(0x18b13004, 0x00030201);
	qose_reg_wr(0x18b13008, 0x00000302);
	qose_reg_wr(0x18b1300c, 0x00000003);
	#else
	qose_reg_wr(0x18b13000, 0x00000000);
	qose_reg_wr(0x18b13004, 0x00000001);
	qose_reg_wr(0x18b13008, 0x00000002);
	qose_reg_wr(0x18b1300c, 0x00000003);
	#endif
	qose_reg_wr(0x18b00004, 0x0000000e);
	qose_reg_wr(0x18b00020, 0x00000080);
	qose_reg_wr(0x18b005c0, 0x00000080);
	qose_reg_wr(0x18b00024, 0x00000080);
	qose_reg_wr(0x18b005c4, 0x00000080);
	qose_reg_wr(0x18b00028, 0x00000080);
	qose_reg_wr(0x18b005c8, 0x00000080);
	qose_reg_wr(0x18b0002c, 0x00000080);
	qose_reg_wr(0x18b005cc, 0x00000080);
	qose_reg_wr(0x18b00100, 0x00000400);
	qose_reg_wr(0x18b00200, 0x00000100);
	qose_reg_wr(0x18b00440, 0x00000040);
	qose_reg_wr(0x18b004c0, 0x00000001);
	qose_reg_wr(0x18b00500, 0x00000030);
	//qose_reg_wr(0x18b00540, 0x20000000);

	base = (void *)CONFIG_CQEM_BUF_LLK_BASE[0];
	base_org = base;
	val = (uint32_t) CONFIG_CQEM_BUF_BASE[0];

	val = val & 0xFFFFFF80;
	val_org = val;

	count = CQEM_FORCE_COUNT_NUM;

	for (i = 0; i < count; i ++) {
		REG32(KSEG1ADDR(base)) = (val_org + i * 2048) >> 6 ;
		base++;
	}

	pr_info("init bm size 0 llk base 0x%x last llk base 0x%x first val 0x%x last val 0x%x count 0x%x\n",
		 (unsigned int)base_org, (unsigned int)base, val_org, (val_org + i * 2048), count);

	qose_reg_wr(0x18b00540, (uint32_t) base_org);
	qose_reg_wr(0x18b00580, 0x00000000);
	qose_reg_wr(0x18b00400, 0x00000080);
	qose_reg_wr(0x18b00444, 0x00000040);
	qose_reg_wr(0x18b004c4, 0x00000018);
	qose_reg_wr(0x18b00504, 0x0000002c);
	//qose_reg_wr(0x18b00544, 0x20040000);


	base = (void *)CONFIG_CQEM_BUF_LLK_BASE[1];
	base_org = base;
	val = CONFIG_CQEM_BUF_BASE[1];
	val = val & 0xFFFFFF80;
	val_org = val;
	count = CQEM_FORCE_COUNT_NUM;

	for (i = 0; i < count; i ++) {
		REG32(KSEG1ADDR(base)) = (val_org + i * 2048) >> 6 ;
		base++;
	}

	pr_info("init bm size 1 llk base 0x%x last llk base 0x%x first val 0x%x last val 0x%x count 0x%x\n",
		(unsigned int)base_org, (unsigned int)base, val_org, (val_org + i * 2048), count);
	qose_reg_wr(0x18b00544,  (uint32_t) base_org);

	qose_reg_wr(0x18b00584, 0x00000000);
	qose_reg_wr(0x18b00404, 0x00000100);
	qose_reg_wr(0x18b00448, 0x00000040);
	qose_reg_wr(0x18b004c8, 0x00000001);
	qose_reg_wr(0x18b00508, 0x00000020);
	//qose_reg_wr(0x18b00548, 0x20080000);

	base = (void *)CONFIG_CQEM_BUF_LLK_BASE[2];
	base_org = base;
	val = CONFIG_CQEM_BUF_BASE[2];
	val = val & 0xFFFFFF80;
	val_org = val;
	count = CQEM_FORCE_COUNT_NUM;

	for (i = 0; i < count; i ++) {
		REG32(KSEG1ADDR(base)) = (val_org + i * 2048) >> 6 ;
		base++;
	}

	pr_info("init bm size 2 llk base 0x%x last llk base 0x%x first val 0x%x last val 0x%x count 0x%x\n",
		(unsigned int)base_org, (unsigned int)base, val_org, (val_org + i * 2048), count);
	qose_reg_wr(0x18b00548, (uint32_t) base_org);

	qose_reg_wr(0x18b00588, 0x00000000);
	qose_reg_wr(0x18b00408, 0x00000180);
	qose_reg_wr(0x18b0044c, 0x00000040);
	qose_reg_wr(0x18b004cc, 0x0000000f);
	qose_reg_wr(0x18b0050c, 0x00000030);
	//qose_reg_wr(0x18b0054c, 0x200c0000);

        base = (void *)CONFIG_CQEM_BUF_LLK_BASE[3];
        base_org = base;
        val = CONFIG_CQEM_BUF_BASE[3];
        val = val & 0xFFFFFF80;
        val_org = val;
	count = CQEM_FORCE_COUNT_NUM;
	for (i = 0; i < count; i ++) {
		REG32(KSEG1ADDR(base)) = (val_org + i * 2048) >> 6 ;
		base++;
	}

	pr_info("init bm size 3 llk base 0x%x last llk base 0x%x first val 0x%x last val 0x%x count 0x%x\n",
		(unsigned int)base_org, (unsigned int)base, val_org, (val_org + i * 2048) , count);
	qose_reg_wr(0x18b0054c, (uint32_t) base_org);

	qose_reg_wr(0x18b0058c, 0x00000000);
	qose_reg_wr(0x18b0040c, 0x00000200);
	qose_reg_wr(0x18b00014, 0x00000033);
	qose_reg_wr(0x18b00010, 0x00000048);
	qose_reg_wr(0x18b00740, 0x00000090);
	qose_reg_wr(0x18b00744, 0x00000090);
	qose_reg_wr(0x18b00748, 0x00000090);
	qose_reg_wr(0x18b0074c, 0x00000090);
	qose_reg_wr(0x18b00000, 0x00000001);
	qose_reg_wr(0x18b0000c, 0x0000fff0);
	qose_reg_wr(0x18b00008, 0x000f000f);
	qose_reg_rd_poll_2(0x18b00018, 0x0, 0x00);
}
#endif
/**************************************************************************
 *! \fn	buffer_manager_remove
 **************************************************************************
 *
 *  \brief	probe platform device hook
 * 
 *  \param	pdev:	platform device pointer
 *
 *  \return	0 on success, other error code on failure
 *
 **************************************************************************/
static int buffer_manager_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id bm_match[] = {
	{ .compatible = "intel,falconmx-bm" },
	{},
};

static struct platform_driver	g_buffer_manager_platform_driver = {
	.driver = { .name = "buffer_manager", .of_match_table = bm_match,},
	.probe = buffer_manager_probe,
	.remove = buffer_manager_remove,
};

/**************************************************************************
 *! \fn	buffer_manager_driver_init
 **************************************************************************
 *
 *  \brief	Init platform device driver
 * 
 *  \return	0 on success, other error code on failure
 *
 **************************************************************************/
static int __init buffer_manager_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&g_buffer_manager_platform_driver);
	if (ret < 0) {
		pr_err("buffer_manager_driver_init(): Failed to register buffer_manager platform driver: %d\n", ret);
		return ret;
	}
	pr_info("buffer_manager_driver_init(): buffer_manager driver init done\n");

	return 0;
}

/**************************************************************************
 *! \fn	buffer_manager_driver_exit
 **************************************************************************
 *
 *  \brief	Exit platform device driver
 * 
 *  \return	0 on success, other error code on failure
 *
 **************************************************************************/
static void __exit buffer_manager_driver_exit(void)
{
	platform_driver_unregister(&g_buffer_manager_platform_driver);

	pr_info("buffer_manager_driver_exit(): buffer_manager driver exit done\n");
}

/*************************************************/
/**		Module Declarations		**/
/*************************************************/
arch_initcall(buffer_manager_driver_init);
module_exit(buffer_manager_driver_exit);

