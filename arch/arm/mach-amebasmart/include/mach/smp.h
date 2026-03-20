// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek SMP support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _SMP_H_
#define _SMP_H_

#include <mach/hardware.h>

#include <asm/hardware/gic.h>

/*
 * set_event() is used to wake up secondary core from wfe using sev. ROM
 * code puts the second core into wfe(standby).
 *
 */
#define set_event()	__asm__ __volatile__ ("sev" : : : "memory")

/*
 * We use Soft IRQ1 as the IPI
 */
static inline void smp_cross_call(const struct cpumask *mask)
{
	gic_raise_softirq(mask, 1);
}

/*
 * Read MPIDR: Multiprocessor affinity register
 */
#define hard_smp_processor_id()				\
	({						\
		unsigned int cpunum;			\
		__asm__("mrc p15, 0, %0, c0, c0, 5"	\
			: "=r" (cpunum));		\
		cpunum &= 0x0F;				\
	})

#endif /* _SMP_H_ */
