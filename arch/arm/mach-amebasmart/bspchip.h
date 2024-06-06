// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Chip support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _BSPCHIP_H_
#define _BSPCHIP_H_

#include <linux/version.h>
#include <linux/irqchip/arm-gic.h>

/*
 * Register access macro
 */
#ifndef REG32
#define REG32(reg)		(*(volatile unsigned int   *)(reg))
#endif
#ifndef REG16
#define REG16(reg)		(*(volatile unsigned short *)(reg))
#endif
#ifndef REG08
#define REG08(reg)		(*(volatile unsigned char  *)(reg))
#endif

/*
 * ARM PERIPHBASE
 */
#define BSP_PERIPHERAL_PADDR	0x02010000
#define BSP_SCU_PADDR		(BSP_PERIPHERAL_PADDR + 0x0000)

#define BSP_PERIPHERAL_VADDR	0xef210000
#define BSP_SCU_VADDR		(BSP_PERIPHERAL_VADDR + 0x0000)

/*
 * IRQ Mapping
 *
 * In ARM_GIC mode, IRQ 0:26 are reserved for IPI, so we
 * add the following mapping for GIC installation:
 *
 * GIC_IRQ_SPI starts at 32.
 * GIC_IRQ_SPI(x) = 32 + x
 */

/*
 * SMPBOOT pen holding address
 */
#define BSP_SMP_PADDR		0x420080E4UL
#define BSP_SMP_VADDR		0xfee080E4UL

/*
 * Earlycon address
 */
#define BSP_EARLYCON_PADDR	0x4200c000UL
#define BSP_EARLYCON_VADDR	0xfee0c000UL

/*
 * LS SMU
 */
#define BSP_SMU_VADDR		0xbb007800UL
#define BSP_SMU_PADDR		0x1b007800UL
#define BSP_SMU_PSIZE		0x00000800UL

/*
 * AON/LSYS registers
 */
#define REG_AON_SYSRST_MSK      0x00D0
#define REG_LSYS_SW_RST_CTRL	0x0238
#define REG_LSYS_SW_RST_TRIG	0x023C
#define REG_LSYS_SYSRST_MSK0    0x02D0
#define REG_LSYS_SYSRST_MSK1    0x02D4
#define REG_LSYS_SYSRST_MSK2    0x02D8

#define LSYS_BIT_LPSYS_RST	((u32)0x00000001 << 29)
#define SYS_RESET_KEY		0x96969696
#define SYS_RESET_TRIG		0x69696969

#define AP_CPU_ID		2

#endif   /* _BSPCHIP_H */
