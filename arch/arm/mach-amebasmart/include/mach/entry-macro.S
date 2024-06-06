// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek IRQ support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#if defined(CONFIG_ARM_GIC)
#include <mach/entry-macro-gic.S>
#else
#include <mach/entry-macro-vic.S>
#endif
