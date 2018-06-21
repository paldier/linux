/******************************************************************************
                Copyright (c) 2016, 2017 Intel Corporation

For licensing information, see the file 'LICENSE' in the root folder of
this software module.
******************************************************************************/

#include <gsw_init.h>

typedef void (*gsw_call_back)(void *param);

extern ethsw_api_dev_t *ecoredev[LTQ_FLOW_DEV_MAX];

GSW_return_t  GSW_Debug_PrintPceIrqList(void *cdev)
{
	gsw_pce_irq *irq;
	unsigned int i = 0;
	unsigned int port = 0;
	ethsw_api_dev_t *gswdev = GSW_PDATA_GET(cdev);

	if (gswdev == NULL) {
		pr_err("%s:%s:%d", __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	irq = gswdev->PceIrqList->first_ptr;
	printk("\n");

	while (irq != NULL) {
		printk("PCE Node %d:\n", i);
		printk("Irq address          = 0x%x\n", (u32)irq);
		printk("Next Irq Address     = 0x%x\n\n", (u32)irq->pNext);

		if (irq->Port_ier_enabled)
			printk("\tPort IER			= ENABLED\n");
		else
			printk("\tPort IER			= DISABLED\n");

		for (i = 0; i < 12; i++) {
			if (irq->P_IER_MASK & (1 << i)) {
				port = i;
				break;
			}
		}

		printk("\tP_IER_MASK			= %u (port no)\n", port);

		if (irq->Event_ier_enable)
			printk("\tEvent IER			= ENABLED\n");
		else
			printk("\tEvent IER			= DISABLED\n");

		switch (irq->E_IER_MASK) {
		case PCE_MAC_TABLE_CHANGE:
			printk("\tE_IER_MASK			= PCE_MAC_TABLE_CHANGE\n");
			break;

		case PCE_FLOW_TABLE_RULE_MATCHED:
			printk("\tE_IER_MASK			= PCE_FLOW_TABLE_RULE_MATCHED\n");
			break;

		case PCE_CLASSIFICATION_PHASE_2:
			printk("\tE_IER_MASK			= PCE_CLASSIFICATION_PHASE_2\n");
			break;

		case PCE_CLASSIFICATION_PHASE_1:
			printk("\tE_IER_MASK			= PCE_CLASSIFICATION_PHASE_1\n");
			break;

		case PCE_CLASSIFICATION_PHASE_0:
			printk("\tE_IER_MASK			= PCE_CLASSIFICATION_PHASE_0\n");
			break;

		case PCE_PARSER_READY:
			printk("\tE_IER_MASK			= PCE_PARSER_READY\n");
			break;

		case PCE_IGMP_TABLE_FULL:
			printk("\tE_IER_MASK			= PCE_IGMP_TABLE_FULL\n");
			break;

		case PCE_MAC_TABLE_FULL:
			printk("\tE_IER_MASK			= PCE_MAC_TABLE_FULL\n");
			break;
		}

		printk("\tP_ISR_MASK			= %u\n", irq->P_ISR_MASK);
		printk("\tE_ISR_MASK			= %u\n", irq->E_ISR_MASK);
		printk("\tcall_back				= 0x%x\n", (u32)irq->call_back);
		printk("\n");
		irq = irq->pNext;
		i++;
	}

	return GSW_statusOk;
}

static unsigned int PortIrq_Match(gsw_pce_irq *p1,
				  gsw_pce_irq *p2)
{
	if (p1->P_IER_MASK == p2->P_IER_MASK &&
	    p1->P_ISR_MASK == p2->P_ISR_MASK)
		return 1;
	else
		return 0;
}

static unsigned int EventIrq_Match(gsw_pce_irq *p1,
				   gsw_pce_irq *p2)
{
	if (p1->E_IER_MASK == p2->E_IER_MASK &&
	    p1->E_ISR_MASK == p2->E_ISR_MASK)
		return 1;
	else
		return 0;
}

static GSW_return_t pce_irq_add(void *cdev, gsw_pce_irq pce_irg,
				struct pce_irq_linklist *node)
{
	gsw_pce_irq	*register_irq = NULL;
	gsw_pce_irq *irq = NULL;
	irq = node->first_ptr;

	while (irq != NULL) {
		if (PortIrq_Match(&pce_irg, irq) &&
		    EventIrq_Match(&pce_irg, irq)) {
			pr_err("ERROR : Invalid operation , IRQ already registered %s:%s:%d\n",
			       __FILE__, __func__, __LINE__);
			return GSW_statusErr;
		}

		irq = irq->pNext;
	}

#ifdef __KERNEL__
	register_irq = (gsw_pce_irq *)kmalloc(sizeof(gsw_pce_irq), GFP_KERNEL);
	memset(register_irq, 0, sizeof(gsw_pce_irq));
#else
	register_irq = (gsw_pce_irq *)malloc(sizeof(gsw_pce_irq));
	memset(register_irq, 0, sizeof(gsw_pce_irq));
#endif

	register_irq->pNext			=	NULL;
	register_irq->Port_ier_enabled	=	0;
	register_irq->P_IER_MASK	=	pce_irg.P_IER_MASK;
	register_irq->Event_ier_enable	=	0;
	register_irq->E_IER_MASK	=	pce_irg.E_IER_MASK;
	register_irq->P_ISR_MASK	=	pce_irg.P_ISR_MASK;
	register_irq->E_ISR_MASK	=	pce_irg.E_ISR_MASK;
	register_irq->call_back		=	pce_irg.call_back;
	register_irq->param			=	pce_irg.param;

	if (node->first_ptr != NULL) {
		node->last_ptr->pNext = register_irq;
		node->last_ptr = register_irq;
	} else {
		node->first_ptr = node->last_ptr = register_irq;
	}

	return GSW_statusOk;
}

static GSW_return_t pce_irq_del(void *cdev, gsw_pce_irq pce_irg,
				struct pce_irq_linklist *node)
{
	gsw_pce_irq *prv_irq = NULL, *delete_irq = NULL;
	u32 p_ierq = 0;
	delete_irq = node->first_ptr;

	while (delete_irq != NULL) {
		if (PortIrq_Match(&pce_irg, delete_irq) &&
		    EventIrq_Match(&pce_irg, delete_irq)) {

			gsw_r32(cdev, PCE_IER_0_OFFSET,
				delete_irq->P_IER_MASK,
				1, &p_ierq);

			/*Check only Port IER*/
			if (p_ierq) {
				pr_err("ERROR : Can not Un-Register IRQ, disable IRQ first %s:%s:%d\n",
				       __FILE__, __func__, __LINE__);
				return -1;
			}

			if (node->first_ptr == delete_irq &&
			    node->last_ptr == delete_irq) {
				node->first_ptr = delete_irq->pNext;
				node->last_ptr = delete_irq->pNext;
			} else if (node->first_ptr == delete_irq) {
				node->first_ptr = delete_irq->pNext;
			} else if (node->last_ptr == delete_irq) {
				node->last_ptr = prv_irq;
				prv_irq->pNext = delete_irq->pNext;
			} else {
				prv_irq->pNext = delete_irq->pNext;
			}

#ifdef __KERNEL__
			kfree(delete_irq);
#else
			free(delete_irq);
#endif
			return GSW_statusOk;
		}

		prv_irq = delete_irq;
		delete_irq = delete_irq->pNext;
	}

	pr_err("ERROR : Invalid operation , IRQ not registered %s:%s:%d\n",
	       __FILE__, __func__, __LINE__);
	return GSW_statusErr;
}

static GSW_return_t pce_irq_enable(void *cdev, gsw_pce_irq pce_irg,
				   struct pce_irq_linklist *node)
{
	u32 p_ierq = 0, e_ierq = 0, irq_registered = 0;
	gsw_pce_irq *irq = NULL;

	irq = node->first_ptr;

	while (irq != NULL) {
		p_ierq = 0;
		e_ierq = 0;

		if (PortIrq_Match(&pce_irg, irq))
			p_ierq = 1;

		if (EventIrq_Match(&pce_irg, irq))
			e_ierq = 1;

		if (p_ierq && e_ierq &&
		    pce_irg.P_IER_MASK != PCE_INVALID_PORT_IERQ &&
		    pce_irg.E_IER_MASK != PCE_INVALID_EVENT_IERQ) {
			/*If both Port IER and Event IER found
				in the IRQ register list*/
			irq->Port_ier_enabled = 1;
			gsw_w32(cdev, PCE_IER_0_OFFSET,
				irq->P_IER_MASK, 1, 1);

			irq->Event_ier_enable = 1;
			gsw_w32(cdev, PCE_IER_1_OFFSET,
				irq->E_IER_MASK, 1, 1);

			return 1;
		} else if (pce_irg.P_IER_MASK == PCE_INVALID_PORT_IERQ
			   && e_ierq) {
			/*if only Event IER in
			  the IRQ register list*/
			irq->Event_ier_enable = 1;
			gsw_w32(cdev, PCE_IER_1_OFFSET,
				irq->E_IER_MASK, 1, 1);
			irq_registered = 1;
		} else if (pce_irg.E_IER_MASK == PCE_INVALID_EVENT_IERQ
			   && p_ierq) {
			/*if only Port IER in
			  the IRQ register list*/
			irq->Port_ier_enabled = 1;
			gsw_w32(cdev, PCE_IER_0_OFFSET,
				irq->P_IER_MASK, 1, 1);
			irq_registered = 1;
		}

		irq = irq->pNext;
	}

	if (!irq_registered) {
		pr_err("ERROR : Invalid operation , IRQ not registered %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	return GSW_statusOk;
}

static GSW_return_t pce_irq_disable(void *cdev, gsw_pce_irq pce_irg,
				    struct pce_irq_linklist *node)
{
	u32 p_ierq = 0, e_ierq = 0;
	gsw_pce_irq *irq = NULL;
	irq = node->first_ptr;

	if (pce_irg.P_IER_MASK == PCE_INVALID_PORT_IERQ) {
		p_ierq = 1;
		irq = NULL;
	} else {
		irq = node->first_ptr;
	}

	while (irq != NULL) {
		if (PortIrq_Match(&pce_irg, irq)) {
			irq->Port_ier_enabled = 0;
			gsw_w32(cdev, PCE_IER_0_OFFSET,
				irq->P_IER_MASK, 1, 0);
			p_ierq = 1;
		}

		irq = irq->pNext;
	}

	if (!p_ierq) {
		pr_err("ERROR : Invalid operation , PORT not registered %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	if (pce_irg.E_IER_MASK == PCE_INVALID_EVENT_IERQ) {
		e_ierq = 1;
		irq = NULL;
	} else {
		irq = node->first_ptr;
	}

	while (irq != NULL) {
		if (EventIrq_Match(&pce_irg, irq)) {
			irq->Event_ier_enable = 0;
			gsw_w32(cdev, PCE_IER_1_OFFSET,
				irq->E_IER_MASK, 1, 0);
			e_ierq = 1;
		}

		irq = irq->pNext;
	}

	if (!e_ierq) {
		pr_err("ERROR : Invalid operation , EVENT not registered %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	return GSW_statusOk;
}

static GSW_return_t pce_irq_config(void *cdev, GSW_Irq_Op_t *irq, IRQ_TYPE IrqType)
{
	gsw_pce_irq pce_irg;
	ethsw_api_dev_t *gswdev = GSW_PDATA_GET(cdev);

	if (gswdev == NULL) {
		pr_err("%s:%s:%d", __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	if (irq->portid > gswdev->tpnum &&
	    irq->portid != PCE_INVALID_PORT_IERQ) {
		pr_err("ERROR : PortId %d is not with in GSWIP capabilty %s:%s:%d\n",
		       irq->portid, __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	} else if (irq->portid == PCE_INVALID_PORT_IERQ) {
		pce_irg.P_IER_MASK 	= PCE_INVALID_PORT_IERQ;
	} else {
		pce_irg.P_IER_MASK 	= PCE_IER_0_PORT_MASK_GET(irq->portid);
		pce_irg.P_ISR_MASK 	= PCE_ISR_0_PORT_MASK_GET(irq->portid);
	}

	switch (irq->event) {
	case PCE_MAC_TABLE_CHANGE:
		pce_irg.E_IER_MASK = PCE_IER_1_CHG_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_CHG_SHIFT;
		break;

	case PCE_FLOW_TABLE_RULE_MATCHED:
		pce_irg.E_IER_MASK = PCE_IER_1_FLOWINT_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_FLOWINT_SHIFT;
		break;

	case PCE_CLASSIFICATION_PHASE_2:
		pce_irg.E_IER_MASK = PCE_IER_1_CPH2_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_CPH2_SHIFT;
		break;

	case PCE_CLASSIFICATION_PHASE_1:
		pce_irg.E_IER_MASK = PCE_IER_1_CPH1_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_CPH1_SHIFT;
		break;

	case PCE_CLASSIFICATION_PHASE_0:
		pce_irg.E_IER_MASK = PCE_IER_1_CPH0_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_CPH0_SHIFT;
		break;

	case PCE_PARSER_READY:
		pce_irg.E_IER_MASK = PCE_IER_1_PRDY_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_PRDY_SHIFT;
		break;

	case PCE_IGMP_TABLE_FULL:
		pce_irg.E_IER_MASK = PCE_IER_1_IGTF_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_IGTF_SHIFT;
		break;

	case PCE_MAC_TABLE_FULL:
		pce_irg.E_IER_MASK = PCE_IER_1_MTF_SHIFT;
		pce_irg.E_ISR_MASK = PCE_ISR_1_MTF_SHIFT;
		break;

	case PCE_INVALID_EVENT_IERQ:
		pce_irg.E_IER_MASK = PCE_INVALID_EVENT_IERQ;
		break;

	default:
		pr_err("ERROR : Invalid pce blk event ENUM = %d %s:%s:%d\n",
		       irq->event, __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	switch (IrqType)	{
	case IRQ_REGISTER:
		if (irq->event == PCE_INVALID_EVENT_IERQ) {
			pr_err("ERROR : PCE_INVALID_EVENT_IERQ %s:%s:%d\n",
			       __FILE__, __func__, __LINE__);
			return GSW_statusErr;
		}

		if (irq->portid == PCE_INVALID_PORT_IERQ) {
			pr_err("ERROR : PCE PCE_INVALID_PORT_IERQ %s:%s:%d\n",
			       __FILE__, __func__, __LINE__);
			return GSW_statusErr;
		}

		if (irq->call_back != NULL) {
			pce_irg.call_back	= irq->call_back;
			pce_irg.param		= irq->param;
		} else {
			pr_err("ERROR : callback handle is NULL %s:%s:%d\n",
			       __FILE__, __func__, __LINE__);
			return GSW_statusErr;
		}

		pce_irq_add(cdev, pce_irg, gswdev->PceIrqList);
		break;

	case IRQ_UNREGISTER:
		if (irq->event == PCE_INVALID_EVENT_IERQ) {
			pr_err("ERROR : PCE_INVALID_EVENT_IERQ %s:%s:%d\n",
			       __FILE__, __func__, __LINE__);
			return GSW_statusErr;
		}

		if (irq->portid == PCE_INVALID_PORT_IERQ) {
			pr_err("ERROR : PCE PCE_INVALID_PORT_IERQ %s:%s:%d\n",
			       __FILE__, __func__, __LINE__);
			return GSW_statusErr;
		}

		pce_irq_del(cdev, pce_irg, gswdev->PceIrqList);
		break;

	case IRQ_ENABLE:
		pce_irq_enable(cdev, pce_irg, gswdev->PceIrqList);
		break;

	case IRQ_DISABLE:
		pce_irq_disable(cdev, pce_irg, gswdev->PceIrqList);
		break;

	default:
		pr_err("invalid irq type %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	return GSW_statusOk;
}

static GSW_return_t swcore_irq_config(void *cdev, GSW_Irq_Op_t *irq, IRQ_TYPE IrqType)
{
	ethsw_api_dev_t *gswdev = GSW_PDATA_GET(cdev);
	u32 ret;

	if (gswdev == NULL) {
		pr_err("%s:%s:%d", __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

#ifdef __KERNEL__
	spin_lock_bh(&gswdev->lock_irq);
#endif

	switch (irq->blk) {
	case BM:
		pr_err("BM BLK IRQ not supported %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		break;

	case SDMA:
		pr_err("SDMA IRQ not supported %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		break;

	case FDMA:
		pr_err("FDMA IRQ not supported %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		break;

	case PMAC:
		pr_err("PMAC IRQ not supported %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		break;

	case PCE:
		printk("Switch Core PCE BLK %s:%s:%d\n"
		       , __FILE__, __func__, __LINE__);
		pce_irq_config(cdev, irq, IrqType);
		break;

	default:
		pr_err("invalid Switch IRQ Blk %s:%s:%d\n",
		       __FILE__, __func__, __LINE__);
		ret = GSW_statusErr;
		goto UNLOCK_AND_RETURN;
	}

	ret = GSW_statusOk;

UNLOCK_AND_RETURN:

#ifdef __KERNEL__
	spin_unlock_bh(&gswdev->lock_irq);
#endif
	return ret;
}

void GSW_Irq_tasklet(unsigned long prvdata)
{
	gsw_pce_irq *pceirq = NULL;
	gsw_call_back callback;
	u32 p_isr = 0, e_isr = 0;
	ethsw_api_dev_t *cdev = (ethsw_api_dev_t *)prvdata;
	u32 pce_event = 0;

	if (cdev) {

		/*PCE IRQ*/
		/* Read PCE ETHSW_ISR if anything is set ,
			disable the global PCEINT IER
			and Service PCE register IRQ list and Enable back
			global PCEINT IER */
		/*Global PCE ISR is read only,Hardware Status*/
		gsw_r32(cdev, ETHSW_ISR_PCEINT_OFFSET,
			ETHSW_ISR_PCEINT_SHIFT, ETHSW_ISR_PCEINT_SIZE,
			&pce_event);

		if (pce_event) {
			/*Disable PCE Global IER*/
			gsw_w32(cdev, ETHSW_IER_PCEIE_OFFSET,
				ETHSW_IER_PCEIE_SHIFT, ETHSW_IER_PCEIE_SIZE, 0);

			/*PCE : Service registered IRQ list*/
			if (cdev->PceIrqList)
				pceirq = cdev->PceIrqList->first_ptr;

			while (pceirq != NULL) {
				callback = NULL;

				if (pceirq->Port_ier_enabled &&
				    pceirq->Event_ier_enable) {
					/*Check for Pending Interrupt*/
					gsw_r32(cdev, PCE_ISR_0_OFFSET,
						pceirq->P_ISR_MASK,
						1, &p_isr);
					gsw_r32(cdev, PCE_ISR_1_OFFSET,
						pceirq->E_ISR_MASK,
						1, &e_isr);

					if (p_isr && e_isr) {
						/*Clear only the Event
						ISR (lhsc)-> cleared by writting 1.
						PORT ISR cleared by Hardware (Read Only)*/
						gsw_w32(cdev, PCE_ISR_1_OFFSET,
							pceirq->P_ISR_MASK, 1, 1);

						callback = pceirq->call_back;

						/*Call back Service*/
						if (callback)
							callback(pceirq->param);
					}
				}

				pceirq = pceirq->pNext;
			}

			/*Enable PCE Global IER*/
			gsw_w32(cdev, ETHSW_IER_PCEIE_OFFSET,
				ETHSW_IER_PCEIE_SHIFT, ETHSW_IER_PCEIE_SIZE, 1);
		}


		/*BM : Service registered IRQ list*/
		/*yet to be done*/

		/*SDMA : Service registered IRQ list*/
		/*yet to be done*/

		/*FDMA : Service registered IRQ list*/
		/*yet to be done*/

		/*PMAC : Service registered IRQ list*/
		/*yet to be done*/

	}
}


#ifdef __KERNEL__
static irqreturn_t GSW_ISR(int irq, void *dev_id)
{
	struct core_ops *sw_ops;
	ethsw_api_dev_t *gswdev;
	u32 dev;

	for (dev = 0; dev < LTQ_FLOW_DEV_MAX; dev++) {
		sw_ops = NULL;
		gswdev = NULL;
		sw_ops = gsw_get_swcore_ops(dev);

		if (sw_ops) {
			gswdev = GSW_PDATA_GET(sw_ops);

			if (gswdev) {
				printk("\tSwitch IRQ tasklet for device %d\n", dev);
				tasklet_schedule(&gswdev->gswip_tasklet);
			}
		}
	}

	return IRQ_HANDLED;
}
#endif


GSW_return_t GSW_Irq_init(void *cdev)
{

	ethsw_api_dev_t *gswdev = GSW_PDATA_GET(cdev);
	u32 ret;

	if (gswdev == NULL) {
		pr_err("%s:%s:%d", __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	/*GSWIP PCE BLK IRQ Pointer*/
#ifdef __KERNEL__
	gswdev->PceIrqList =
		(struct pce_irq_linklist *)kmalloc(sizeof(struct pce_irq_linklist), GFP_KERNEL);
#else
	gswdev->PceIrqList =
		(struct pce_irq_linklist *)malloc(sizeof(struct pce_irq_linklist));
#endif

	gswdev->PceIrqList->first_ptr = NULL;
	gswdev->PceIrqList->last_ptr = NULL;

	/*Enable PCE Global IER*/
	gsw_w32(cdev, ETHSW_IER_PCEIE_OFFSET,
		ETHSW_IER_PCEIE_SHIFT, ETHSW_IER_PCEIE_SIZE, 1);
	/*Enable BM Global IER*/
	/*yet to be done*/
	/*Enable SDMA Global IER*/
	/*yet to be done*/
	/*Enable FDMA Global IER*/
	/*yet to be done*/
	/*Enable PMAC Global IER*/
	/*yet to be done*/

#ifdef __KERNEL__

	ret = request_irq(gswdev->irq_num, GSW_ISR, 0, "gswip", NULL);

	if (ret) {
		pr_err("Switch irq request error %s:%s:%d", __FILE__, __func__, __LINE__);
		return ret;
	}

	tasklet_init(&gswdev->gswip_tasklet,
		     GSW_Irq_tasklet,
		     (unsigned long)gswdev);
#endif

	return GSW_statusOk;
}

GSW_return_t GSW_Irq_deinit(void *cdev)
{

	gsw_pce_irq *irq = NULL;
	gsw_pce_irq *free_irq = NULL;
	ethsw_api_dev_t *gswdev = (ethsw_api_dev_t *)cdev;

	if (gswdev == NULL) {
		pr_err("%s:%s:%d", __FILE__, __func__, __LINE__);
		return GSW_statusErr;
	}

	/*Free PCE Irq list*/
	if (gswdev->PceIrqList) {
		irq = gswdev->PceIrqList->first_ptr;

		while (irq != NULL) {
			free_irq = irq;
			irq = irq->pNext;
#ifdef __KERNEL__
			kfree(free_irq);
#else
			free(free_irq);
#endif
		}

		/*Free PCE Irq blk*/
#ifdef __KERNEL__
		kfree(gswdev->PceIrqList);
#else
		free(gswdev->PceIrqList);
#endif
	}

	return GSW_statusOk;
}



GSW_return_t GSW_Irq_register(void *cdev, GSW_Irq_Op_t *irq)
{
	return swcore_irq_config(cdev, irq, IRQ_REGISTER);
}

GSW_return_t GSW_Irq_unregister(void *cdev, GSW_Irq_Op_t *irq)
{
	return swcore_irq_config(cdev, irq, IRQ_UNREGISTER);
}

GSW_return_t GSW_Irq_enable(void *cdev, GSW_Irq_Op_t *irq)
{
	return swcore_irq_config(cdev, irq, IRQ_ENABLE);
}

GSW_return_t GSW_Irq_disable(void *cdev, GSW_Irq_Op_t *irq)
{
	return swcore_irq_config(cdev, irq, IRQ_DISABLE);
}


