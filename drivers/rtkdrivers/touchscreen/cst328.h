// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Touchscreen support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef REALTEK_TS_H__
#define REALTEK_TS_H__

#include <linux/i2c.h>
#include <linux/input.h>

struct rtk_ts_platform_data {
	int irq_gpio;
	u32 irq_gpio_flags;
	int reset_gpio;
	u32 reset_gpio_flags;
	u32 x_resolution;
	u32 y_resolution;
	u32 max_touch_num;
};

struct rtk_ts_data {
	struct rtk_ts_platform_data *pdata;
	struct i2c_client *client;
	struct device *dev;
	struct input_dev  *input_dev;
	u8  device_id;
	u8  int_trigger_type;
	s32 use_irq;
	u8  irq_is_disable;
	u8  report_flag;
};
extern struct rtk_ts_data *rtk_ts_data;


#endif
