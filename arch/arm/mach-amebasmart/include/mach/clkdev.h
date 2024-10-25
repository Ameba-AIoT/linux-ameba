// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Clock support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _CLKDEV_H_
#define _CLKDEV_H_

#include <mach/hardware.h>

static inline int __clk_get(struct clk *clk)
{
	return 1;
}

static inline void __clk_put(struct clk *clk)
{
}

#endif /* _CLKDEV_H_ */
