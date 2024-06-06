/*
 * Copyright (C) 2019  Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>

static struct irq_domain *irq_domain;

static inline void unmask_clic_irq(struct irq_data *d)
{
	csr_set(sie, 1 << d->hwirq);
}

static inline void mask_clic_irq(struct irq_data *d)
{
	csr_clear(sie, 1 << d->hwirq);
}

static struct irq_chip riscv_clic_chip = {
	.name		= "Realtek CLIC",
	.irq_ack	= mask_clic_irq,
	.irq_mask	= mask_clic_irq,
	.irq_mask_ack	= mask_clic_irq,
	.irq_unmask	= unmask_clic_irq,
	.irq_eoi	= unmask_clic_irq,
	.irq_disable	= mask_clic_irq,
	.irq_enable	= unmask_clic_irq,
};

static int riscv_clic_map(struct irq_domain *d, unsigned int irq,
			  irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, &riscv_clic_chip, handle_percpu_irq);
	irq_set_chip_data(irq, NULL);
	irq_set_noprobe(irq);

	return 0;
}

static const struct irq_domain_ops riscv_clic_irq_domain_ops = {
	.map		= riscv_clic_map,
	.xlate		= irq_domain_xlate_onecell,
};

static inline unsigned int riscv_clic_get_invaild_num(void)
{
	unsigned int num;

	csr_clear(sie, SIE_SEIE);

	csr_set(sie, ~0UL << 16);
	num = fls(csr_read(sie));
	csr_clear(sie, ~0UL << 16);

	csr_set(sie, SIE_SEIE);

	return num;
}

void riscv_clic_handle_irq(void)
{
	irq_hw_number_t hwirq = csr_read(scause) & ~(1UL << (__riscv_xlen - 1));
	int irq = irq_find_mapping(irq_domain, hwirq);

	if (unlikely(irq <= 0))
		pr_warn_ratelimited("can't find mapping for hwirq %lu\n",
				    hwirq);
	else
		generic_handle_irq(irq);
}

static int riscv_clic_find_hartid(struct device_node *node)
{
	for (; node; node = node->parent) {
		if (of_device_is_compatible(node, "riscv"))
			return riscv_of_processor_hartid(node);
	}

	return -1;
}

int __init riscv_clic_irq_of_init(struct device_node *of_node,
				  struct device_node *parent)
{
	unsigned int num = riscv_clic_get_invaild_num();
	int hartid = riscv_clic_find_hartid(of_node), cpu;

	if (hartid < 0) {
		pr_warn("unable to fine hart id for %pOF\n", of_node);
		return -EINVAL;
	}

	cpu = riscv_hartid_to_cpuid(hartid);
	if (cpu < 0) {
		pr_warn("Invalid cpuid for hart id %d\n", hartid);
		return -EINVAL;
	}

	if (cpu != smp_processor_id())
		return 0;

	if (num < 16)
		return -ENODEV;

	irq_domain = irq_domain_add_linear(of_node, num + 1,
					   &riscv_clic_irq_domain_ops, NULL);
	if (!irq_domain)
		panic("Failed to add irqdomain for Realtek CLIC\n");

	pr_info("Realtek CLIC: mapped %d interrupts.\n", num);

	return 0;
}

IRQCHIP_DECLARE(cpu_intc, "riscv,cpu-intc", riscv_clic_irq_of_init);
