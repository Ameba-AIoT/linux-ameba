// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __AMEBA_IPC_REG_H__
#define __AMEBA_IPC_REG_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */

/* -------------------------------- Defines --------------------------------- */

/**
 * structure of ISP (unit bit):
 * |<------8------>|<------8------>|<------8------>|<------8------>|
 * |----LP_FULL----|----NP_FULL----|----LP_TMPT----|----NP_EMPT----|
 * |7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|
 */
#define ISR_FULL_MASK (u32)(0xffff0000)
#define ISR_EMPT_MASK (u32)(0x0000ffff)

#define ISR_FROM_NP_MASK (u32)(0x00ff00ff)
#define ISR_FROM_LP_MASK (u32)(0xff00ff00)

/* the structure of register is NP|LP */
#define NP_OFFSET (0)
#define LP_OFFSET (8)
#define FULL_OFFSET (16)
#define EMPT_OFFSET (0)

/* -------------------------------- Macros ---------------------------------- */
/**
 * the port and channel memory mapping is below:
 * |<--8-->|<--8-->|<--8-->|<--8-->|<--8-->|<--8-->|
 * | AP2NP | AP2LP | NP2AP | NP2LP | LP2AP | LP2NP |
 * The unit is not byte, and is the size of ipc_msg_struct_t.
 */
#define BUF_AP2NP_IDX(ch) (16 * 2 + 8 + ch)
#define BUF_AP2LP_IDX(ch) (16 * 2 + 0 + ch)
#define BUF_NP2AP_IDX(ch) (16 * 1 + 8 + ch)
#define BUF_LP2AP_IDX(ch) (8 + ch)

/* to operate normal registers, include AIPC_REG_TX_DATA,
 * AIPC_REG_RX_DATA and AIPC_REG_ICR.
 */

/* Do not write 1 to other channel. Do not use |= ()!!! */
#define AIPC_SET_NP_CH_NR(ch) (u32)((0x00000001 << NP_OFFSET) << ch)
#define AIPC_SET_LP_CH_NR(ch) (u32)((0x00000001 << LP_OFFSET) << ch)

#define AIPC_GET_NP_CH_NR(ch, reg) (u32)(reg & ((0x00000001 << NP_OFFSET) << ch))
#define AIPC_GET_LP_CH_NR(ch, reg) (u32)(reg & ((0x00000001 << LP_OFFSET) << ch))
#define AIPC_CLR_NP_CH_NR(ch, reg) (u32)(reg & ~((0x00000001 << NP_OFFSET) << ch))
#define AIPC_CLR_LP_CH_NR(ch, reg) (u32)(reg & ~((0x00000001 << LP_OFFSET) << ch))

/* to operate interrupt registers, include AIPC_REG_IMR and AIPC_REG_ISR.*/
#define AIPC_SET_NP_CH_IR_EMPT(ch, reg) (u32)(reg | (0x00000001 << NP_OFFSET) << ch)
#define AIPC_SET_LP_CH_IR_EMPT(ch, reg) (u32)(reg | (0x00000001 << LP_OFFSET) << ch)
#define AIPC_GET_NP_CH_IR_EMPT(ch, reg) (u32)(reg & ((0x00000001 << NP_OFFSET) << ch))
#define AIPC_GET_LP_CH_IR_EMPT(ch, reg) (u32)(reg & ((0x00000001 << LP_OFFSET) << ch))
#define AIPC_SET_NP_CH_IR_FULL(ch, reg) (u32)(reg | ((0x00000001 << NP_OFFSET) << FULL_OFFSET) << ch)
#define AIPC_SET_LP_CH_IR_FULL(ch, reg) (u32)(reg | ((0x00000001 << LP_OFFSET) << FULL_OFFSET) << ch)
#define AIPC_GET_NP_CH_IR_FULL(ch, reg) (u32)(reg & (((0x00000001 << NP_OFFSET) << FULL_OFFSET) << ch))
#define AIPC_GET_LP_CH_IR_FULL(ch, reg) (u32)(reg & (((0x00000001 << LP_OFFSET) << FULL_OFFSET) << ch))
#define AIPC_CLR_NP_CH_IR_EMPT(ch, reg) (u32)(reg & ~((0x00000001 << NP_OFFSET) << ch))
#define AIPC_CLR_LP_CH_IR_EMPT(ch, reg) (u32)(reg & ~((0x00000001 << LP_OFFSET) << ch))
#define AIPC_CLR_NP_CH_IR_FULL(ch, reg) (u32)(reg & ~(((0x00000001 << NP_OFFSET) << FULL_OFFSET) << ch))
#define AIPC_CLR_LP_CH_IR_FULL(ch, reg) (u32)(reg & ~(((0x00000001 << LP_OFFSET) << FULL_OFFSET) << ch))

/* ------------------------------- Data Types ------------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */

#endif /* __AMEBA_IPC_REG_H__ */
