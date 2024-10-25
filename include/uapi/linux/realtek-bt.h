// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
* Realtek Bluteooth support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _UAPI_REALTEK_BT_H_
#define _UAPI_REALTEK_BT_H_

#include <linux/ioctl.h>

/* IOCTL commands */
#define RTK_BT_IOC_MAGIC            'b'

#define RTK_BT_IOC_SET_BT_POWER     _IOW(RTK_BT_IOC_MAGIC, 1, struct rtk_bt_power_info)

struct rtk_bt_power_info {
	/*set bluetooth power on or off*/
	unsigned int power_on;
	/*ubt_ant_switch*/
	unsigned int bt_ant_switch;
};

#endif
