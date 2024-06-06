#ifndef __INCLUDE_LINUX_IRQCHIP_IRQ_DW_ICTL_H
#define __INCLUDE_LINUX_IRQCHIP_IRQ_DW_ICTL_H

#define DW_ICTL_BASE	16
#define DW_ICTL_NUM	32

extern void dw_ictl_init(unsigned long ictl_base_addr,
			 unsigned int cpu_vec);

#endif /* __INCLUDE_LINUX_IRQCHIP_IRQ_DW_ICTL_H */
