/*
 * Realtek Semiconductor Corp.
 *
 * bsp/irq-ictl.c
 *   DesignWare ICTL initialization and handlers
 *
 * This file is to part of BSP irq handlers, and will
 * be included by bsp/irq.c if CONFIG_IRQ_ICTL is set.
 *
 * Copyright (C) 2006-2015 Tony Wu (tonywu@realtek.com)
 */

#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/irqchip/irq-dw-ictl.h>

enum ictl_regs_enum {
	ICTL_REG_INTEN = 0,
	ICTL_REG_MASK,
	ICTL_REG_RAWSTATUS,
	ICTL_REG_STATUS,
	ICTL_REG_MASKSTATUS,
	ICTL_REG_FINALSTATUS,
};

static u32 ictl_regs[] = {
	[ICTL_REG_INTEN] = 0x00,
	[ICTL_REG_MASK] = 0x08,
	[ICTL_REG_RAWSTATUS] = 0x18,
	[ICTL_REG_STATUS] = 0x20,
	[ICTL_REG_MASKSTATUS] = 0x28,
	[ICTL_REG_FINALSTATUS] = 0x30,
};

static void __iomem *ictl_membase;

static inline u32 read_ictl_reg(unsigned reg)
{
	return __raw_readl(ictl_membase + ictl_regs[reg]);
}

static inline void write_ictl_reg(u32 val, unsigned reg)
{
	__raw_writel(val, ictl_membase + ictl_regs[reg]);
}

static inline void set_ictl_reg(u32 val, unsigned reg)
{
	u32 ori = read_ictl_reg(reg);
	u32 new = (ori | val);

	__raw_writel(new, ictl_membase + ictl_regs[reg]);
}

static inline void clear_ictl_reg(u32 val, unsigned reg)
{
	u32 ori = read_ictl_reg(reg);
	u32 new = (ori & ~val);

	__raw_writel(new, ictl_membase + ictl_regs[reg]);
}

static void ictl_irq_mask(struct irq_data *d)
{
	set_ictl_reg(BIT(d->hwirq), ICTL_REG_MASK);
}

static void ictl_irq_unmask(struct irq_data *d)
{
	clear_ictl_reg(BIT(d->hwirq), ICTL_REG_MASK);
}

static struct irq_chip ictl_irq_controller = {
	.name = "Realtek ICTL",
	.irq_mask_ack = ictl_irq_mask,
	.irq_mask = ictl_irq_mask,
	.irq_unmask = ictl_irq_unmask,
};

static void ictl_irq_handler(struct irq_desc *desc)
{
	unsigned int pending = read_ictl_reg(ICTL_REG_FINALSTATUS);

	if (pending) {
		struct irq_domain *domain = irq_desc_get_handler_data(desc);
		generic_handle_irq(irq_find_mapping(domain, __ffs(pending)));
	} else {
		spurious_interrupt();
	}
}

static int ictl_intc_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, &ictl_irq_controller, handle_level_irq);

	return 0;
}

static const struct irq_domain_ops irq_domain_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = ictl_intc_map,
};

static void __init __dw_ictl_init(struct device_node *node,
				  unsigned int cpu_vec)
{
	struct irq_domain *domain;
	int irq;

	irq = cpu_vec;

	/* disable ICTL inten */
	write_ictl_reg(0, ICTL_REG_INTEN);

	domain = irq_domain_add_legacy(node, DW_ICTL_NUM, DW_ICTL_BASE, 0,
				       &irq_domain_ops, NULL);

	if (!domain)
		panic("Failed to add irqdomain");

	/* enable ICTL inten, mask everything initially */
	write_ictl_reg(0xffffffff, ICTL_REG_MASK);
	write_ictl_reg(0xffffffff, ICTL_REG_INTEN);

	irq_set_chained_handler_and_data(irq, ictl_irq_handler, domain);
}

void __init dw_ictl_init(unsigned long ictl_base_addr,
			 unsigned int cpu_vec)
{
	ictl_membase = (void __iomem *)ictl_base_addr;
	__dw_ictl_init(NULL, cpu_vec);
}

static int __init dw_ictl_of_init(struct device_node *node,
				  struct device_node *parent)
{
	struct resource res;
	unsigned int cpu_vec;

	cpu_vec = irq_of_parse_and_map(node, 0);

	if (of_address_to_resource(node, 0, &res))
		panic("Failed to get ictl memory range");

	if (!request_mem_region(res.start, resource_size(&res), res.name))
		pr_err("Failed to request ictl memory");

	ictl_membase = ioremap_nocache(res.start,
				       resource_size(&res));
	if (!ictl_membase)
		panic("Failed to remap ictl memory");

	__dw_ictl_init(node, cpu_vec);
	return 0;
}

IRQCHIP_DECLARE(dw_ictl, "rtk,dw-apb-ictl", dw_ictl_of_init);
