// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek VIC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <mach/hardware.h>

	.macro	disable_fiq
	.endm

	.macro	get_irqnr_preamble, base, tmp
	@ enable imprecise aborts
	cpsie	a
	mov	\base, #BSP_VIC_BASE
	.endm

	.macro	arch_ret_to_user, tmp1, tmp2
	.endm

	.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
	@ 0xD0 has irq# or old irq# if the irq has been handled
	@ 0xD4 has irq# or -1 if none pending *but* if you just
	@ read 0xD4 you never get the first irq for some reason
	ldr	\irqnr, [\base, #0xD0]
	ldr	\irqnr, [\base, #0xD4]
	cmp	\irqnr, #0xffffffff
	.endm
