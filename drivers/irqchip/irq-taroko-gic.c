/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2012 MIPS Technologies, Inc.  All rights reserved.
 * Copyright 2012  Tony Wu (tonywu@realtek.com)
 */
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/irqchip/taroko-gic.h>
#include <linux/sched.h>

#include <asm/time.h>
#include <asm/taroko-mmcr.h>

#include <dt-bindings/interrupt-controller/mips-gic.h>

struct gic_pcpu_mask {
	DECLARE_BITMAP(pcpu_mask, GIC_MAX_INTRS);
};

struct gic_irq_spec {
	enum {
		GIC_DEVICE,
		GIC_IPI
	} type;

	union {
		struct cpumask *ipimask;
		unsigned int hwirq;
	};
};

static struct gic_pcpu_mask pcpu_masks[NR_CPUS];
static DEFINE_SPINLOCK(gic_lock);
static struct irq_domain *gic_irq_domain;
static struct irq_domain *gic_dev_domain;
static struct irq_domain *gic_ipi_domain;
unsigned int gic_cpu_pin;
static struct irq_chip gic_level_irq_controller, gic_edge_irq_controller;
DECLARE_BITMAP(ipi_resrv, GIC_MAX_INTRS);

static void gic_send_ipi(struct irq_data *d, unsigned int cpu)
{
	irq_hw_number_t hwirq = GIC_HWIRQ_TO_SHARED(irqd_to_hwirq(d));

	GIC_SET_IPI_MASK(hwirq);
}

static void gic_handle_shared_int(bool chained)
{
	unsigned int i, intr, virq;
	unsigned long *pcpu_mask;
	DECLARE_BITMAP(pending, GIC_MAX_INTRS);
	DECLARE_BITMAP(intrmask, GIC_MAX_INTRS);

	/* Get per-cpu bitmaps */
	pcpu_mask = pcpu_masks[smp_processor_id()].pcpu_mask;

	for (i = 0; i < BITS_TO_LONGS(GIC_NUM_INTRS); i++) {
		pending[i] = GIC_REG32p(PEND, i*4);
		intrmask[i] = GIC_REG32p(MASK, i*4);
	}

	bitmap_and(pending, pending, intrmask, GIC_NUM_INTRS);
	bitmap_and(pending, pending, pcpu_mask, GIC_NUM_INTRS);

	intr = find_first_bit(pending, GIC_NUM_INTRS);
	while (intr != GIC_NUM_INTRS) {
		virq = irq_linear_revmap(gic_irq_domain,
					 GIC_SHARED_TO_HWIRQ(intr));
		if (chained)
			generic_handle_irq(virq);
		else
			do_IRQ(virq);

		/* go to next pending bit */
		bitmap_clear(pending, intr, 1);
		intr = find_first_bit(pending, GIC_NUM_INTRS);
	}
}

static void gic_mask_irq(struct irq_data *d)
{
	GIC_CLR_INTR_MASK(GIC_HWIRQ_TO_SHARED(d->hwirq));
}

static void gic_unmask_irq(struct irq_data *d)
{
	GIC_SET_INTR_MASK(GIC_HWIRQ_TO_SHARED(d->hwirq));
}

static void gic_ack_irq(struct irq_data *d)
{
	unsigned int intr = GIC_HWIRQ_TO_SHARED(d->hwirq);

	GIC_CLR_IPI_MASK(intr);
}

static int gic_set_type(struct irq_data *d, unsigned int type)
{
	unsigned long flags;
	bool is_edge;

	spin_lock_irqsave(&gic_lock, flags);
	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_FALLING:
		is_edge = true;
		break;
	case IRQ_TYPE_EDGE_RISING:
		is_edge = true;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		is_edge = true;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		is_edge = false;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
	default:
		is_edge = false;
		break;
	}

	if (is_edge)
		irq_set_chip_handler_name_locked(d, &gic_edge_irq_controller,
						 handle_edge_irq, NULL);
	else
		irq_set_chip_handler_name_locked(d, &gic_level_irq_controller,
						 handle_level_irq, NULL);
	spin_unlock_irqrestore(&gic_lock, flags);

	return 0;
}

#ifdef CONFIG_SMP
static int gic_set_affinity(struct irq_data *d, const struct cpumask *cpumask,
			    bool force)
{
	unsigned int irq = GIC_HWIRQ_TO_SHARED(d->hwirq);
	cpumask_t	tmp = CPU_MASK_NONE;
	unsigned long	flags;
	int		i;

	cpumask_and(&tmp, cpumask, cpu_online_mask);
	if (cpumask_empty(&tmp))
		return -EINVAL;

	/* Assumption : cpumask refers to a single CPU */
	spin_lock_irqsave(&gic_lock, flags);

	/* Re-route this IRQ */
	GIC_ROUTE_TO_CORE(irq, cpumask_first(&tmp));

	/* Update the pcpu_masks */
	for (i = 0; i < NR_CPUS; i++)
		clear_bit(irq, pcpu_masks[i].pcpu_mask);
	set_bit(irq, pcpu_masks[cpumask_first(&tmp)].pcpu_mask);

	cpumask_copy(irq_data_get_affinity_mask(d), cpumask);
	spin_unlock_irqrestore(&gic_lock, flags);

	return IRQ_SET_MASK_OK_NOCOPY;
}
#endif

static struct irq_chip gic_level_irq_controller = {
	.name			=	"TAROKO GIC",
	.irq_mask		=	gic_mask_irq,
	.irq_unmask		=	gic_unmask_irq,
	.irq_set_type		=	gic_set_type,
#ifdef CONFIG_SMP
	.irq_set_affinity	=	gic_set_affinity,
#endif
};

static struct irq_chip gic_edge_irq_controller = {
	.name			=	"TAROKO GIC",
	.irq_ack		=	gic_ack_irq,
	.irq_mask		=	gic_mask_irq,
	.irq_unmask		=	gic_unmask_irq,
	.irq_set_type		=	gic_set_type,
#ifdef CONFIG_SMP
	.irq_set_affinity	=	gic_set_affinity,
#endif
	.ipi_send_single	=	gic_send_ipi,
};

static void gic_irq_dispatch(struct irq_desc *desc)
{
	gic_handle_shared_int(true);
}

static void __init gic_basic_init(void)
{
	unsigned int i;

	/* Setup defaults */
	for (i = 0; i < GIC_NUM_INTRS; i++)
		GIC_CLR_INTR_MASK(i);

	for (i = 0; i < GIC_NUM_INTRS; i++) {
		GIC_REG32p(SITIMER_R2P, i*4) = get_c0_compare_int() -
					       GIC_CPU_PIN_OFFSET;
	}
}

static int gic_shared_irq_domain_map(struct irq_domain *d, unsigned int virq,
				     irq_hw_number_t hw, unsigned int core)
{
	int intr = GIC_HWIRQ_TO_SHARED(hw);
	unsigned long flags;
	int i;

	spin_lock_irqsave(&gic_lock, flags);

	/* Setup Intr to PIN mapping */
	GIC_ROUTE_TO_PIN(intr, gic_cpu_pin);

	/* Setup Intr to CPU mapping */
	GIC_ROUTE_TO_CORE(intr, core);

	for (i = 0; i < NR_CPUS; i++)
		clear_bit(intr, pcpu_masks[i].pcpu_mask);
	set_bit(intr, pcpu_masks[core].pcpu_mask);
	spin_unlock_irqrestore(&gic_lock, flags);

	return 0;
}

static int gic_setup_dev_chip(struct irq_domain *d, unsigned int virq,
			      unsigned int hwirq)
{
	int err;

	err = irq_domain_set_hwirq_and_chip(d, virq, hwirq,
					    &gic_level_irq_controller, NULL);

	return err;
}

#ifndef CONFIG_OF
static int gic_irq_domain_map(struct irq_domain *d, unsigned int virq,
			      irq_hw_number_t hw)
{
	irq_set_chip_and_handler(virq, &gic_level_irq_controller,
				 handle_level_irq);

	return gic_shared_irq_domain_map(d, virq, hw, 0);
}
#endif

static int gic_irq_domain_alloc(struct irq_domain *d, unsigned int virq,
				unsigned int nr_irqs, void *arg)
{
	struct gic_irq_spec *spec = arg;
	irq_hw_number_t hwirq, base_hwirq;
	int cpu, ret, i;

	if (spec->type == GIC_DEVICE) {
		/* verify that shared irqs don't conflict with an IPI irq */
		if (test_bit(GIC_HWIRQ_TO_SHARED(spec->hwirq), ipi_resrv))
			return -EBUSY;

		return gic_setup_dev_chip(d, virq, spec->hwirq);
	} else {
		base_hwirq = find_first_bit(ipi_resrv, GIC_NUM_INTRS);
		if (base_hwirq == GIC_NUM_INTRS) {
			return -ENOMEM;
		}

		/* check that we have enough space */
		for (i = base_hwirq; i < nr_irqs; i++) {
			if (!test_bit(i, ipi_resrv))
				return -EBUSY;
		}
		bitmap_clear(ipi_resrv, base_hwirq, nr_irqs);

		/* map the hwirq for each cpu consecutively */
		i = 0;
		for_each_cpu(cpu, spec->ipimask) {
			hwirq = GIC_SHARED_TO_HWIRQ(base_hwirq + i);

			ret = irq_domain_set_hwirq_and_chip(d, virq + i, hwirq,
							    &gic_level_irq_controller,
							    NULL);
			if (ret)
				goto error;

			irq_set_handler(virq + i, handle_level_irq);

			ret = gic_shared_irq_domain_map(d, virq + i, hwirq, cpu);
			if (ret)
				goto error;

			i++;
		}

		/*
		 * tell the parent about the base hwirq we allocated so it can
		 * set its own domain data
		 */
		spec->hwirq = base_hwirq;
	}

	return 0;
error:
	bitmap_set(ipi_resrv, base_hwirq, nr_irqs);
	return ret;
}

static void gic_irq_domain_free(struct irq_domain *d, unsigned int virq,
			 unsigned int nr_irqs)
{
	irq_hw_number_t base_hwirq;
	struct irq_data *data;

	data = irq_get_irq_data(virq);
	if (!data)
		return;

	base_hwirq = GIC_HWIRQ_TO_SHARED(irqd_to_hwirq(data));
	bitmap_set(ipi_resrv, base_hwirq, nr_irqs);
}

static int gic_irq_domain_match(struct irq_domain *d, struct device_node *node,
			 enum irq_domain_bus_token bus_token)
{
	/* this domain should'nt be accessed directly */
	return 0;
}

static struct irq_domain_ops gic_irq_domain_ops = {
#ifndef CONFIG_OF
	.map = gic_irq_domain_map,
#endif
	.alloc = gic_irq_domain_alloc,
	.free = gic_irq_domain_free,
	.match = gic_irq_domain_match,
};

static int gic_dev_domain_xlate(struct irq_domain *d, struct device_node *ctrlr,
				const u32 *intspec, unsigned int intsize,
				irq_hw_number_t *out_hwirq,
				unsigned int *out_type)
{
	if (intsize != 3)
		return -EINVAL;

	if (intspec[0] == GIC_SHARED)
		*out_hwirq = GIC_SHARED_TO_HWIRQ(intspec[1]);
	else if (intspec[0] == GIC_LOCAL)
		*out_hwirq = GIC_LOCAL_TO_HWIRQ(intspec[1]);
	else
		return -EINVAL;
	*out_type = intspec[2] & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static int gic_dev_domain_alloc(struct irq_domain *d, unsigned int virq,
				unsigned int nr_irqs, void *arg)
{
	struct irq_fwspec *fwspec = arg;
	struct gic_irq_spec spec = {
		.type = GIC_DEVICE,
	};
	int i, ret;

	if (fwspec->param[0] == GIC_SHARED)
		spec.hwirq = GIC_SHARED_TO_HWIRQ(fwspec->param[1]);
	else
		spec.hwirq = GIC_LOCAL_TO_HWIRQ(fwspec->param[1]);

	ret = irq_domain_alloc_irqs_parent(d, virq, nr_irqs, &spec);
	if (ret)
		return ret;

	for (i = 0; i < nr_irqs; i++) {
		ret = gic_setup_dev_chip(d, virq + i, spec.hwirq + i);
		if (ret)
			goto error;
	}

	return 0;

error:
	irq_domain_free_irqs_parent(d, virq, nr_irqs);
	return ret;
}

static void gic_dev_domain_free(struct irq_domain *d, unsigned int virq,
			 unsigned int nr_irqs)
{
	/* no real allocation is done for dev irqs, so no need to free anything */
	return;
}

static int gic_dev_domain_activate(struct irq_domain *domain,
				    struct irq_data *d, bool early)
{
	return gic_shared_irq_domain_map(domain, d->irq, d->hwirq, 0);
}

static struct irq_domain_ops gic_dev_domain_ops = {
	.xlate = gic_dev_domain_xlate,
	.alloc = gic_dev_domain_alloc,
	.free = gic_dev_domain_free,
	.activate = gic_dev_domain_activate,
};

static int gic_ipi_domain_xlate(struct irq_domain *d, struct device_node *ctrlr,
				const u32 *intspec, unsigned int intsize,
				irq_hw_number_t *out_hwirq,
				unsigned int *out_type)
{
	/*
	 * There's nothing to translate here. hwirq is dynamically allocated and
	 * the irq type is always edge triggered.
	 * */
	*out_hwirq = 0;
	*out_type = IRQ_TYPE_EDGE_RISING;

	return 0;
}

static int gic_ipi_domain_alloc(struct irq_domain *d, unsigned int virq,
				unsigned int nr_irqs, void *arg)
{
	struct cpumask *ipimask = arg;
	struct gic_irq_spec spec = {
		.type = GIC_IPI,
		.ipimask = ipimask
	};
	int ret, i;

	ret = irq_domain_alloc_irqs_parent(d, virq, nr_irqs, &spec);
	if (ret)
		return ret;

	/* the parent should have set spec.hwirq to the base_hwirq it allocated */
	for (i = 0; i < nr_irqs; i++) {
		ret = irq_domain_set_hwirq_and_chip(d, virq + i,
						    GIC_SHARED_TO_HWIRQ(spec.hwirq + i),
						    &gic_edge_irq_controller,
						    NULL);
		if (ret)
			goto error;

		ret = irq_set_irq_type(virq + i, IRQ_TYPE_EDGE_RISING);
		if (ret)
			goto error;
	}

	return 0;
error:
	irq_domain_free_irqs_parent(d, virq, nr_irqs);
	return ret;
}

static void gic_ipi_domain_free(struct irq_domain *d, unsigned int virq,
			 unsigned int nr_irqs)
{
	irq_domain_free_irqs_parent(d, virq, nr_irqs);
}

static int gic_ipi_domain_match(struct irq_domain *d, struct device_node *node,
			 enum irq_domain_bus_token bus_token)
{
	bool is_ipi;

	switch (bus_token) {
	case DOMAIN_BUS_IPI:
		is_ipi = d->bus_token == bus_token;
		return (!node || to_of_node(d->fwnode) == node) && is_ipi;
		break;
	default:
		return 0;
	}
}

static struct irq_domain_ops gic_ipi_domain_ops = {
	.xlate = gic_ipi_domain_xlate,
	.alloc = gic_ipi_domain_alloc,
	.free = gic_ipi_domain_free,
	.match = gic_ipi_domain_match,
};

static void __init __gic_init(unsigned int cpu_vec, unsigned int irqbase,
			      struct device_node *node)
{
	unsigned int v[2];

	gic_cpu_pin = cpu_vec - GIC_CPU_PIN_OFFSET;
	irq_set_chained_handler(MIPS_CPU_IRQ_BASE + cpu_vec,
				gic_irq_dispatch);

	gic_irq_domain = irq_domain_add_simple(node, GIC_NUM_INTRS, irqbase,
					       &gic_irq_domain_ops, NULL);
	if (!gic_irq_domain)
		panic("Failed to add GIC IRQ domain");
	gic_irq_domain->name = "mips-gic-irq";


	gic_dev_domain = irq_domain_add_hierarchy(gic_irq_domain, 0,
						  GIC_NUM_INTRS,
						  node, &gic_dev_domain_ops, NULL);
	if (!gic_dev_domain)
		panic("Failed to add GIC DEV domain");
	gic_dev_domain->name = "mips-gic-dev";

	gic_ipi_domain = irq_domain_add_hierarchy(gic_irq_domain,
						  IRQ_DOMAIN_FLAG_IPI_PER_CPU,
						  GIC_NUM_INTRS,
						  node, &gic_ipi_domain_ops, NULL);
	if (!gic_ipi_domain)
		panic("Failed to add GIC IPI domain");

	gic_ipi_domain->name = "mips-gic-ipi";
	gic_ipi_domain->bus_token = DOMAIN_BUS_IPI;

	if (node &&
	    !of_property_read_u32_array(node, "rtk,reserved-ipi-vectors", v, 2)) {
		bitmap_set(ipi_resrv, v[0], v[1]);
	} else {
		/* Make the last 2 * gic_vpes available for IPIs */
		bitmap_set(ipi_resrv,
			   GIC_NUM_INTRS - 2 * NR_CPUS,
			   2 * NR_CPUS);
	}

	gic_basic_init();
}

void __init gic_init(unsigned int cpu_vec, unsigned int irqbase)
{
	__gic_init(cpu_vec, irqbase, NULL);
}

static int __init gic_of_init(struct device_node *node,
			      struct device_node *parent)
{
	unsigned int cpu_vec, i = 0, reserved = 0;

	/* Find the first available CPU vector. */
	while (!of_property_read_u32_index(node, "rtk,reserved-cpu-vectors",
					   i++, &cpu_vec))
		reserved |= BIT(cpu_vec);
	for (cpu_vec = 2; cpu_vec < 8; cpu_vec++) {
		if (!(reserved & BIT(cpu_vec)))
			break;
	}
	if (cpu_vec == 8) {
		pr_err("No CPU vectors available for GIC\n");
		return -ENODEV;
	}

	__gic_init(cpu_vec, 0, node);

	return 0;
}
IRQCHIP_DECLARE(taroko_gic, "rtk,gic", gic_of_init);
