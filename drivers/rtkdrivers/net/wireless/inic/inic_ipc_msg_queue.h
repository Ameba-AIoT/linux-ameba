/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
#ifndef __INIC_IPC_MSG_QUEUE_H__
#define __INIC_IPC_MSG_QUEUE_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

/* internal head files */
#include "inic_ipc.h"
#include "inic_linux_base_type.h"

/* -------------------------------- Defines --------------------------------- */
#define CONFIG_INIC_IPC_MSG_Q_PRI	(3)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */
void inic_ipc_msg_q_init(struct device *pdev, void (*task_hdl)(inic_ipc_ex_msg_t *));
int inic_ipc_msg_enqueue(inic_ipc_ex_msg_t *p_ipc_msg);
void inic_ipc_msg_q_deinit(void);
unsigned char inic_ipc_msg_get_queue_status(void);
void inic_ipc_ipc_send_msg(inic_ipc_ex_msg_t *p_ipc_msg);

#endif /* __INIC_IPC_MSG_QUEUE_H__ */
