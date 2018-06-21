#ifndef _ASM_IRQ_WORK_H
#define _ASM_IRQ_WORK_H

static inline bool arch_irq_work_has_interrupt(void)
{
#ifndef CONFIG_SMP
	return false;
#else
	return true;
#endif
}

#endif /* _ASM_IRQ_WORK_H */
