// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Console support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _REALTEK_CONSOLE_CORE_H_
#define _REALTEK_CONSOLE_CORE_H_

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/if_link.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysrq.h>
#include <linux/of.h>
#include <linux/kern_levels.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <asm-generic/io.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

#include <ameba_ipc/ameba_ipc.h>

#define CONSOLE_MAX_CHAR	128

enum {
	IPC_USER_POINT = 0,
	IPC_USER_DATA = 1
};

typedef struct console_ipc_host_req_msg {
	char param_buf[CONSOLE_MAX_CHAR];
} console_ipc_host_req_t;

struct rtk_console {
	struct device			*dev;
	struct aipc_ch          	*pconsole_ipc_ch;
	console_ipc_host_req_t		*preq_msg;              /* host api message to send to device */
	dma_addr_t			req_msg_phy_addr;       /* host api message to send to device */
	struct tasklet_struct		console_tasklet;            /* api task to haddle api msg */
	ipc_msg_struct_t		console_ipc_msg;            /* to store ipc msg for api */
	struct completion		console_complete;           /* only one console process can send ipc instruction */
};

typedef struct console_ipc_rx_res {
	int ret;
	int complete_num;
} console_ipc_rx_res_t;

extern int rtk_console_process(char* data, int len, u8 *result);

#endif
