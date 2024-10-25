// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek OTP IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _OTP_IPC_AMEBA_H
#define _OTP_IPC_AMEBA_H

#include <linux/types.h>

#define LINUX_IPC_OTP_PHY_READ8			0
#define LINUX_IPC_OTP_PHY_WRITE8		1
#define LINUX_IPC_OTP_LOGI_READ_MAP		2
#define LINUX_IPC_OTP_LOGI_WRITE_MAP	3
#define LINUX_IPC_EFUSE_REMAIN_LEN		4

#define OTP_LMAP_LEN				0x400
#define OPT_REQ_MSG_PARAM_NUM		OTP_LMAP_LEN

typedef struct otp_ipc_host_req_msg {
	u32 otp_id;
	u32 addr;
	u32 len;
	u32 write_lock;
	u8 param_buf[OPT_REQ_MSG_PARAM_NUM];
} otp_ipc_host_req_t;

int rtk_otp_process(void* data, u8 *result);

#endif
