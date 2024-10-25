// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek SMP support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/of_fdt.h>

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>

/* Detect number of CPU via DT */
static int __init plat_dt_cpus_num(unsigned long node, const char *uname,
				  int depth, void *data)
{
	static int prev_depth = -1;
	static int nr_cpus = -1;

	if (prev_depth > depth && nr_cpus > 0)
		return nr_cpus;

	if (nr_cpus < 0 && strcmp(uname, "cpus") == 0)
		nr_cpus = 0;

	if (nr_cpus >= 0) {
		const char *dev_type;

		dev_type = of_get_flat_dt_prop(node, "device_type", NULL);
		if (dev_type && strcmp(dev_type, "cpu") == 0)
			nr_cpus++;
	}

	prev_depth = depth;
	return 0;
}

static DEFINE_SPINLOCK(boot_lock);
void plat_smp_map_io(void);

void __init plat_smp_map_io(void)
{
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
volatile int pen_release = -1;
#endif

static void write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	sync_cache_w(&pen_release);
}

/*
 * This is executed on the primary core to boot secondary ones.
 */
static int plat_smp_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}

/*
 * This is executed on the secondary core.
 */
static void plat_smp_secondary_init(unsigned int cpu)
{
	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

extern void plat_secondary_startup(void);
void plat_smp_prepare_cpus(unsigned int max_cpus);
void plat_smp_init_cpus(void);

void __init plat_smp_prepare_cpus(unsigned int max_cpus)
{
	void __iomem *boot_reg = (void *) BSP_SMP_VADDR;
	unsigned long boot_addr;
	int i;

	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	/*
	 * Write the address of secondary startup into the
	 * system-wide register and then the secondary CPU
	 * branches to this address.
	 */
	boot_addr = virt_to_phys(plat_secondary_startup);
	__raw_writel(boot_addr, boot_reg);
}

void __init plat_smp_init_cpus(void)
{
	int ncores = 0, i;

	ncores = of_scan_flat_dt(plat_dt_cpus_num, NULL);

	if (ncores < 2)
		return;

	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; ++i)
		set_cpu_possible(i, true);
}

extern void plat_cpu_die(unsigned int cpu);

struct smp_operations plat_smp_ops __initdata = {
	.smp_init_cpus		= plat_smp_init_cpus,
	.smp_prepare_cpus       = plat_smp_prepare_cpus,
	.smp_secondary_init     = plat_smp_secondary_init,
	.smp_boot_secondary     = plat_smp_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die                = plat_cpu_die,
#endif
};
