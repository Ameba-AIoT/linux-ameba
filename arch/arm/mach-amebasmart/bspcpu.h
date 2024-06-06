// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek CPU support
*
* bsp/bspcpu.h
*     bsp cpu and memory configuration file
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _BSPCPU_H_
#define _BSPCPU_H_

/*
 * scache => L2 cache
 * dcache => D cache
 * icache => I cache
 */
#define cpu_mem_size        (256 << 20)

#define cpu_imem_size       0
#define cpu_dmem_size       0
#define cpu_smem_size       0

#endif /* _BSPCPU_H_ */
