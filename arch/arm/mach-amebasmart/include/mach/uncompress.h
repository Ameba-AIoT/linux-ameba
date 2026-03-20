// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Uncompress support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _UNCOMPRESS_H_
#define _UNCOMPRESS_H_
//#ifndef CONFIG_AMEBASMART
#if 0
#include <linux/serial_reg.h>

#define BSP_EARLYCON_PADDR	0x1fb03000UL

volatile unsigned long *uart;

static void putc(int c)
{
	while (!(uart[UART_LSR] & UART_LSR_THRE))
		barrier();

	uart[UART_TX] = c;
}

static inline void flush(void)
{
	while (!(uart[UART_LSR] & UART_LSR_THRE))
		barrier();
}

static inline void early_uart_init(void)
{
	uart[UART_LCR] = 0x80;
	uart[UART_IER] = 0x0;
	/*
	 *  baud rate = (serial clock freq) / (16 * divisor)
	 *  baud rate = 57600
	 *  serial clock freq = 25MHz
	 */
	uart[UART_TX] = 0x1b;
	uart[UART_LCR] = 0x3;
}

static inline void __arch_decomp_setup(unsigned long arch_id)
{
	uart = (unsigned long *)BSP_EARLYCON_PADDR;
	early_uart_init();
}
#else
#define BSP_EARLYCON_PADDR      0x4200C000UL
#define LSR	0x14
#define THR4	0x68	
#define LOGUART_LINE_STATUS_REG_P4_FIFONF		((u32)0x00000001<<23)
#define LOGUART_LINE_STATUS_REG_P4_THRE			((u32)0x00000001<<19)	     /*BIT[19], Path4 FIFO empty indicator*/
volatile unsigned long *uart;
static void putc(int c)
{
#ifdef WJJ_TEST
//should open later and verify, as paladin is too slow, close now
//        while (!(uart[LSR] & LOGUART_LINE_STATUS_REG_P4_FIFONF))
//		barrier();
#endif
	uart[THR4>>2] = c;
}

static inline void flush(void)
{
	while (!(uart[LSR>>2] & LOGUART_LINE_STATUS_REG_P4_THRE))
		barrier();
}

static inline void early_uart_init(void)
{
	uart[THR4>>2] = 0x1b;
}

static inline void __arch_decomp_setup(unsigned long arch_id)
{
	uart = (unsigned long *)BSP_EARLYCON_PADDR;
        early_uart_init();
}

#endif

#define arch_decomp_setup()	__arch_decomp_setup(arch_id)

/*
 * nothing to do
 */
#define arch_decomp_wdog()

#endif /* mach/uncompress.h */
