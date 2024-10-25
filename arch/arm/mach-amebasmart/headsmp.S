// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek SMP support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/version.h>
#include <asm/assembler.h>
/*
 * Entry point for secondary CPUs. This provides a
 * "holding pen" into which all secondary cores are
 * held until we're ready for them to initialise.
 */
ENTRY(plat_secondary_startup)
 ARM_BE8(setend be)
	mrc	p15, 0, r0, c0, c0, 5
	mvn	r1, #0xff000000
	and	r0, r0, r1
	adr	r4, 1f
	ldmia	r4, {r5, r6}
	sub	r4, r4, r5
	add	r6, r6, r4
pen:	ldr	r7, [r6]
	cmp	r7, r0
	bne	pen

#if defined(CONFIG_CPU_V8)
#if defined(CONFIG_CPU_ARMA53)
	/*
	 * turn on SMP bit for each secondary core
	 */
	mrrc	p15, 1, r0, r1, c15
	tst	r0, #(1 << 6)
	orreq	r0, #(1 << 6)
	mcrreq	p15, 1, r0, r1, c15
#endif
#endif
	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
	b	secondary_startup

	.align
1:	.long	.
	.long	pen_release
ENDPROC(plat_secondary_startup)
