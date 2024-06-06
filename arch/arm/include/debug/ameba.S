/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * arch/arm/include/debug/ameba.S
 *
 *  Copyright (C) 2020-2023 Realtek
 */
#include <linux/serial_ameba.h>

		.macro	addruart, rp, rv, tmp
		ldr	\rp, =CONFIG_DEBUG_UART_PHYS
		ldr	\rv, =CONFIG_DEBUG_UART_VIRT
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx, #AMEBA_LOGUART_THR4]
		.endm

		.macro	busyuart,rd,rx
#1002:		ldrb	\rd, [\rx, #AMEBA_LOGUART_LSR]
#		and	\rd, \rd, #LOGUART_LINE_STATUS_REG_P4_THRE
#		teq	\rd, #1
#		bne	1002b
		.endm

		.macro	waituart,rd,rx
		.endm
