// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek System support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <asm/proc-fns.h>

static inline void arch_idle(void)
{
	cpu_do_idle();
}

extern void (*arch_reset)(char, const char *);

#endif /* _SYSTEM_H_ */
