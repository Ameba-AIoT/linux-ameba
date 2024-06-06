// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __AMEBA_IPC_RESOURCE_H__
#define __AMEBA_IPC_RESOURCE_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */

/* -------------------------------- Defines --------------------------------- */
/* address of IPC LP shared memory */
#define AIPC_SHARED_MEM_ADDR (0x2301FD00)
/* size of IPC LP shared memory */
#define AIPC_SHARED_MEM_SIZE (0x300)

/* start for AP armV7 IPC registers */
#define AIPC_REG_BASE (0x41000580) /* the base address of ameba IPC register */
/* length of ameba IPC registers */
#define AIPC_REG_SIZE (4) /* 32 bits for one registers */

/* TX REGISTER,  Address offset: 0x0000 */
#define AIPC_REG_TX_DATA (AIPC_REG_BASE + 0x0000)
/* RX REGISTER,  Address offset: 0x0004 */
#define AIPC_REG_RX_DATA (AIPC_REG_BASE + 0x0004)
/* INTERRUPT STATUS REGISTER,  Address offset: 0x0008 */
#define AIPC_REG_ISR (AIPC_REG_BASE + 0x0008)
/* INTERRUPT MASK REGISTER,  Address offset: 0x000C */
#define AIPC_REG_IMR (AIPC_REG_BASE + 0x000C)
/* CLEAR TX REGISTER,  Address offset: 0x0010 */
#define AIPC_REG_ICR (AIPC_REG_BASE + 0x0010)
/* end for AP armV7 IPC registers */

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */

#endif /* __AMEBA_IPC_RESOURCE_H__ */
