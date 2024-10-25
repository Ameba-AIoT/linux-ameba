// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Touchscreen support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/input/mt.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <uapi/linux/sched/types.h>

#include "cst328.h"

#define RTK_DRIVER_NAME                     "rtk_ts"

#define RTK_IRQ_TRIGGER_RISING_CONFIG               0x01
#define RTK_MAIN_IIC_ADDR_CONFIG                    0x1A
#define RTK_COORDS_ARR_SIZE                     2
#define RTK_MAX_POINTS                              5
#define RTK_X_DISPLAY_DEFAULT                       320
#define RTK_Y_DISPLAY_DEFAULT                       800

#define CST_TP1_REG         0XD000
#define CST_DEBUG_INFO_MODE     0xD101
#define CST_FW_INFO     0xD208
#define CST_DEVIDE_MODE     0xD109

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct task_struct *thread = NULL;
static int thread_ts_flag = 0;
struct rtk_ts_data *rtk_ts_data = NULL;

static const struct i2c_device_id rtk_ts_id[] = {
	{RTK_DRIVER_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rtk_ts_id);

static const struct of_device_id rtk_dt_match[] = {
	{.compatible = "realtek,ameba-ts", },
	{}
};
MODULE_DEVICE_TABLE(of, rtk_dt_match);


static void rtk_ts_irq_disable(void)
{
	if (!rtk_ts_data->irq_is_disable) {
		disable_irq(rtk_ts_data->use_irq);
		rtk_ts_data->irq_is_disable = 1;
	}
}


static void rtk_ts_irq_enable(void)
{
	if (rtk_ts_data->irq_is_disable) {
		enable_irq(rtk_ts_data->use_irq);
		rtk_ts_data->irq_is_disable = 0;
	}
}

/*******************************************************
Function:
    read data from register.
Input:
    buf: first two byte is register addr, then read data store into buf
    len: length of data that to read
Output:
    success: number of messages
    fail:   negative errno
*******************************************************/
static int rtk_ts_i2c_read_register(struct i2c_client *client, unsigned char *buf, int len)
{
	struct i2c_msg msgs[2] = {0};
	int ret = -1;
	int retries = 0;

	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].addr  = client->addr;
	msgs[0].len   = 2;
	msgs[0].buf   = buf;

	msgs[1].flags |= I2C_M_RD;
	msgs[1].addr   = client->addr;
	msgs[1].len    = len;
	msgs[1].buf    = buf;

	while (retries < 2) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2) {
			break;
		}
		retries++;
	}

	return ret;
}

/**
 * rtk_ts_i2c_write - write data to a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to write to.
 * @buf: raw data buffer to write.
 * @len: length of the buffer to write
 */
static int rtk_ts_i2c_write(struct i2c_client *client, u8 *buf, int len)
{
	struct i2c_msg msg;
	int ret = -1;
	int retries = 0;

	msg.flags = client->flags & I2C_M_TEN;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;

	while (retries < 2) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1) {
			break;
		}
		retries++;
	}

	return ret;
}

static irqreturn_t rtk_eint_interrupt_handler(int irq, void *data)
{
	thread_ts_flag = 1;
	wake_up(&waiter);

	return IRQ_HANDLED;
}

static void rtk_ts_touch_down(struct input_dev *input_dev, s32 id, s32 x, s32 y, s32 w)
{
	s32 temp_w = (w >> 3);

	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
	input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, temp_w);
	input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, temp_w);
}

static void rtk_ts_touch_up(struct input_dev *input_dev, int id)
{
	input_mt_slot(input_dev, id);
	input_report_abs(input_dev, ABS_MT_TRACKING_ID, -1);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
}

static void rtk_ts_touch_report(void)
{
	u8 buf[70];
	u8 i2c_buf[8];
	u8 finger_id, sw;
	unsigned int  input_x = 0;
	unsigned int  input_y = 0;
	unsigned int  input_w = 0;
	unsigned char cnt_up = 0, cnt_down = 0;
	int i, ret, idx;
	int cnt, i2c_len;
	int  len_1, len_2;

	buf[0] = 0xD0;
	buf[1] = 0x00;
	ret = rtk_ts_i2c_read_register(rtk_ts_data->client, buf, 7);
	if (ret < 0) {
		dev_err(rtk_ts_data->dev, "Failed to read touch point data\n");
		goto OUT_PROCESS;
	}

	if (buf[6] != 0xAB) {
		dev_err(rtk_ts_data->dev, "Invalid data: buf[6] != 0xAB\n");
		goto OUT_PROCESS;
	}

	if (buf[0] == 0xAB) {
		dev_err(rtk_ts_data->dev, "Invalid data: buf[0] == 0xAB\n");
		goto OUT_PROCESS;
	}

	cnt = buf[5] & 0x7F;
	if (cnt > RTK_MAX_POINTS) {
		goto OUT_PROCESS;
	} else if (cnt == 0) {
		goto CLR_POINT;
	}

	if (cnt == 0x01) {
		goto FINGER_PROCESS;
	} else {
		dev_err(rtk_ts_data->dev, "Report touch cnt = %d\n", cnt);
		i2c_len = (cnt - 1) * 5 + 1;
		len_1 = i2c_len;

		for (idx = 0; idx < i2c_len; idx += 6) {
			i2c_buf[0] = 0xD0;
			i2c_buf[1] = 0x07 + idx;

			if (len_1 >= 6) {
				len_2  = 6;
				len_1 -= 6;
			} else {
				len_2 = len_1;
				len_1 = 0;
			}

			ret = rtk_ts_i2c_read_register(rtk_ts_data->client, i2c_buf, len_2);
			if (ret < 0) {
				goto OUT_PROCESS;
			}

			for (i = 0; i < len_2; i++) {
				buf[5 + idx + i] = i2c_buf[i];
			}
		}
		i2c_len += 5;

		if (buf[i2c_len - 1] != 0xAB) {
			goto OUT_PROCESS;
		}
	}


FINGER_PROCESS:
	i2c_buf[0] = 0xD0;
	i2c_buf[1] = 0x00;
	i2c_buf[2] = 0xAB;
	ret = rtk_ts_i2c_write(rtk_ts_data->client, i2c_buf, 3);
	if (ret < 0) {
		dev_err(rtk_ts_data->dev, "Failed to send read touch info ending\n");
		goto OUT_PROCESS;
	}

	idx = 0;
	cnt_up = 0;
	cnt_down = 0;
	for (i = 0; i < cnt; i++) {
		input_x = (unsigned int)((buf[idx + 1] << 4) | ((buf[idx + 3] >> 4) & 0x0F));
		input_y = (unsigned int)((buf[idx + 2] << 4) | (buf[idx + 3] & 0x0F));

		/* Adapter for new touch firmware */
		if (!rtk_ts_data || !rtk_ts_data->pdata) {
			input_x = RTK_X_DISPLAY_DEFAULT - input_x;
			input_y = RTK_Y_DISPLAY_DEFAULT - input_y;
		} else {
			input_x = rtk_ts_data->pdata->x_resolution - input_x;
			input_y = rtk_ts_data->pdata->y_resolution - input_y;
		}

		input_w = (unsigned int)(buf[idx + 4]);
		sw = (buf[idx] & 0x0F) >> 1;
		finger_id = (buf[idx] >> 4) & 0x0F;

		dev_dbg(rtk_ts_data->dev, "Point x:%d, y:%d, id:%d, sw:%d\n", input_x, input_y, finger_id, sw);

		if (sw == 0x03) {
			rtk_ts_touch_down(rtk_ts_data->input_dev, finger_id, input_x, input_y, input_w);
			cnt_down++;
		} else {
			cnt_up++;
			rtk_ts_touch_up(rtk_ts_data->input_dev, finger_id);
		}
		idx += 5;

	}

	if ((cnt_up > 0) && (cnt_down == 0)) {
		input_report_key(rtk_ts_data->input_dev, BTN_TOUCH, 0);
	} else {
		input_report_key(rtk_ts_data->input_dev, BTN_TOUCH, 1);
	}

	if (cnt_down == 0) {
		rtk_ts_data->report_flag = 0;
	} else {
		rtk_ts_data->report_flag = 0xCA;
	}

	input_sync(rtk_ts_data->input_dev);
	goto END;

CLR_POINT:
	for (i = 0; i <= 10; i++) {
		input_mt_slot(rtk_ts_data->input_dev, i);
		input_report_abs(rtk_ts_data->input_dev, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(rtk_ts_data->input_dev, MT_TOOL_FINGER, false);
	}
	input_report_key(rtk_ts_data->input_dev, BTN_TOUCH, 0);
	input_sync(rtk_ts_data->input_dev);

OUT_PROCESS:
	buf[0] = 0xD0;
	buf[1] = 0x00;
	buf[2] = 0xAB;
	ret = rtk_ts_i2c_write(rtk_ts_data->client, buf, 3);
	if (ret < 0) {
		dev_err(rtk_ts_data->dev, " Failed to send read touch info ending\n");
	}

END:
	cnt_up = 0;
	cnt_down = 0;
	return;
}

static int rtk_touch_handler(void *unused)
{
	struct sched_param param = { .sched_priority = 4 };

	sched_setscheduler(current, SCHED_RR, &param);
	do {

		set_current_state(TASK_INTERRUPTIBLE);
		wait_event(waiter, thread_ts_flag != 0);

		thread_ts_flag = 0;

		set_current_state(TASK_RUNNING);

		rtk_ts_touch_report();

	} while (!kthread_should_stop());

	return 0;
}

static int rtk_input_dev_init(struct rtk_ts_data *ts_data)
{
	int ret = 0;
	struct input_dev *input_dev;

	ts_data->input_dev = input_allocate_device();
	if (!ts_data->input_dev) {
		dev_err(ts_data->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}
	input_dev = ts_data->input_dev;

	ts_data->input_dev->name = RTK_DRIVER_NAME;
	ts_data->input_dev->id.bustype = BUS_I2C;
	ts_data->input_dev->id.vendor = 0x00;

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	input_mt_init_slots(input_dev, ts_data->pdata->max_touch_num, INPUT_MT_DIRECT);

	input_set_capability(input_dev, EV_ABS, ABS_MT_POSITION_X);
	input_set_capability(input_dev, EV_ABS, ABS_MT_POSITION_Y);

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,  ts_data->pdata->max_touch_num, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ts_data->pdata->x_resolution, 0, 0); // fuzz --flat
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ts_data->pdata->y_resolution, 0, 0);

	ret = input_register_device(ts_data->input_dev);
	if (ret) {
		dev_err(ts_data->dev, "Failed to register input device: %d\n", ret);
		return ret;
	}

	thread = kthread_run(rtk_touch_handler, 0, RTK_DRIVER_NAME);
	if (IS_ERR(thread)) {
		ret = PTR_ERR(thread);
		dev_err(ts_data->dev, "Failed to create touch event handler thread: %d\n", ret);
	}

	return ret;
}

static void rtk_ts_data_init(struct i2c_client *client)
{
	rtk_ts_data->client = client;
	rtk_ts_data->pdata->max_touch_num  = RTK_MAX_POINTS;
}

static int rtk_gpio_configure(struct rtk_ts_data *data)
{
	int ret = -1;

	/* Request IRQ GPIO */
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		ret = gpio_request(data->pdata->irq_gpio, NULL);
		if (ret < 0) {
			dev_err(data->dev, "Failed to request irq_gpio\n");
			goto err_irq_gpio_req;
		}

		ret = gpio_direction_input(data->pdata->irq_gpio);
		if (ret < 0) {
			dev_err(data->dev, "Failed to set irq_gpio direction\n");
			goto err_irq_gpio_dir;
		}
	}

	/* Request reset GPIO */
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		ret = gpio_request(data->pdata->reset_gpio, NULL);
		if (ret < 0) {
			dev_err(data->dev, "Failed to request reset_gpio\n");
			goto err_reset_gpio_req;
		}

		ret = gpio_direction_output(data->pdata->reset_gpio, 1);
		if (ret < 0) {
			dev_err(data->dev, "Failed to set reset_gpio direction\n");
			goto err_reset_gpio_dir;
		}
	}
	gpio_direction_output(rtk_ts_data->pdata->reset_gpio, 0);
	mdelay(20);
	gpio_direction_output(rtk_ts_data->pdata->reset_gpio, 1);

	return 0;

err_reset_gpio_req:
err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_free(data->pdata->reset_gpio);
	}
err_irq_gpio_req:
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		gpio_free(data->pdata->irq_gpio);
	}
	return ret;
}

static int rtk_get_dt_coords(struct device *dev, char *name, struct rtk_ts_platform_data *pdata)
{
	int ret = 0;
	u32 coords[2] = { 0 };
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;

	prop = of_find_property(np, name, NULL);
	if (!prop) {
		return -EINVAL;
	}
	if (!prop->value) {
		return -ENODATA;
	}

	coords_size = prop->length / sizeof(u32);
	if (coords_size != RTK_COORDS_ARR_SIZE) {
		dev_err(dev, "Invalid %s with size %d\n", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		dev_err(dev, "Invalid %s property in DTS\n", name);
		pdata->x_resolution = RTK_X_DISPLAY_DEFAULT;
		pdata->y_resolution = RTK_Y_DISPLAY_DEFAULT;
		return -ENODATA;
	}
	pdata->x_resolution = coords[0];
	pdata->y_resolution = coords[1];

	return 0;
}

static int rtk_parse_dt(struct device *dev, struct rtk_ts_platform_data *pdata)
{
	int ret = -1;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;
	const  struct of_device_id *match;

	match = of_match_device(of_match_ptr(rtk_dt_match), dev);
	if (!match) {
		dev_err(dev, "Unable to find matchv device in DTS\n");
		return ENODEV;
	}
	ret = rtk_get_dt_coords(dev, "ts-display-coords", pdata);
	if (ret < 0) {
		dev_err(dev, "Invalid display-coords property in DTS\n");
		return -1;
	}

	//printk("rtk_parse_dt np: %s, name: %s", np->full_name,  np->name);
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "ts-reset-gpios", 0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "Invalid reset_gpio property in DTS\n");
		return -1;
	}

	pdata->irq_gpio = of_get_named_gpio_flags(np, "ts-irq-gpios", 0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0) {
		dev_err(dev, "Invalid irq_gpio property in DTS\n");
		return -1;
	}

	ret = of_property_read_u32(np, "max-touch-number", &temp_val);
	if (ret < 0) {
		dev_err(dev, "Invalid max-touch-number property in DTS\n");
		pdata->max_touch_num = RTK_MAX_POINTS;
		return -1;
	} else {
		if (temp_val < 2) {
			pdata->max_touch_num = 2;    /* max_touch_number must >= 2 */
		} else if (temp_val > RTK_MAX_POINTS) {
			pdata->max_touch_num = RTK_MAX_POINTS;
		} else {
			pdata->max_touch_num = temp_val;
		}
	}

	dev_dbg(dev, "Parse DTS: max touch number = %d, irq_gpio = %d, reset_gpio = %d\n", pdata->max_touch_num, pdata->irq_gpio, pdata->reset_gpio);

	return ret;
}

static int rtk_platform_data_init(struct rtk_ts_data *ts_data)
{
	int ret = -1;
	int pdata_size = sizeof(struct rtk_ts_platform_data);

	ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
	if (!ts_data->pdata) {
		dev_err(ts_data->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	if (ts_data->dev->of_node) {
		ret = rtk_parse_dt(ts_data->dev, ts_data->pdata);
		if (ret < 0) {
			dev_err(ts_data->dev, "Failed to parse DTS\n");
			return -ENODEV;
		}
	}

	return ret;

}

static int rtk_ts_i2c_test(struct i2c_client *client)
{
	int retry = 0;
	int ret;
	u8 buf[4];

	buf[0] = 0xD1;
	buf[1] = 0x06;
	while (retry++ < 1) {
		ret = rtk_ts_i2c_write(client, buf, 2);
		if (ret > 0) {
			return ret;
		}

		mdelay(2);
	}

	if (retry == 6) {
		dev_err(&client->dev, "Test error: %d\n", ret);
	}

	return ret;
}

static int rtk_ts_firmware_info(struct i2c_client *client)
{
	int ret;
	u8 buf[28];
	u32 rtk_ts_ic_version  = 0;
	u16 rtk_ts_ic_checksum = 0;

	buf[0] = 0xD1;
	buf[1] = 0x01;
	ret = rtk_ts_i2c_write(client, buf, 2);
	if (ret < 0) {
		return -1;
	}

	mdelay(10);

	buf[0] = 0xD2;
	buf[1] = 0x08;
	ret = rtk_ts_i2c_read_register(client, buf, 8);
	if (ret < 0) {
		return -1;
	}

	rtk_ts_ic_version = buf[3];
	rtk_ts_ic_version <<= 8;
	rtk_ts_ic_version |= buf[2];
	rtk_ts_ic_version <<= 8;
	rtk_ts_ic_version |= buf[1];
	rtk_ts_ic_version <<= 8;
	rtk_ts_ic_version |= buf[0];

	rtk_ts_ic_checksum = buf[7];
	rtk_ts_ic_checksum <<= 8;
	rtk_ts_ic_checksum |= buf[6];
	rtk_ts_ic_checksum <<= 8;
	rtk_ts_ic_checksum |= buf[5];
	rtk_ts_ic_checksum <<= 8;
	rtk_ts_ic_checksum |= buf[4];

	dev_dbg(&client->dev, "IC version = 0x%08X, checksum = 0x%04X\n", rtk_ts_ic_version, rtk_ts_ic_checksum);

	if (rtk_ts_ic_version == 0xA5A5A5A5) {
		dev_err(&client->dev, "No firmware\n");
		return -1;
	}

	buf[0] = 0xD1;
	buf[1] = 0x09;
	ret = rtk_ts_i2c_write(client, buf, 2);
	if (ret < 0) {
		return -1;
	}
	mdelay(5);

	return 0;
}

static int rtk_irq_init(struct i2c_client *client)
{
	int ret = -1;

	rtk_ts_data->int_trigger_type = RTK_IRQ_TRIGGER_RISING_CONFIG;
	rtk_ts_data->pdata->irq_gpio_flags = ((RTK_IRQ_TRIGGER_RISING_CONFIG) ? IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING);
	rtk_ts_data->pdata->irq_gpio_flags |= IRQF_ONESHOT;

	rtk_ts_data->use_irq = gpio_to_irq(rtk_ts_data->pdata->irq_gpio);

	/* Configure GPIO to IRQ and request IRQ */
	ret = request_irq(rtk_ts_data->use_irq, (irq_handler_t)rtk_eint_interrupt_handler, rtk_ts_data->pdata->irq_gpio_flags, "Realtek Touch Int", NULL);
	if (ret == 0) {
		client->irq = rtk_ts_data->use_irq;
	}

	return ret;
}

static int rtk_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = -1;
	struct rtk_ts_data *ts_data = NULL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		return -ENODEV;
	}

	ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		dev_err(&client->dev, "Failed to allocate memory for ts_data\n");
		return -ENOMEM;
	}

	rtk_ts_data = ts_data;
	ts_data->client = client;
	ts_data->dev = &client->dev;

	i2c_set_clientdata(client, ts_data);

	ret = rtk_platform_data_init(ts_data);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to init rtk_platform_data, please check DTS\n");
		goto err_end;
		//kfree(ts_data);
		//return ret;
	}

	ret = rtk_gpio_configure(ts_data);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to configure GPIO\n");
		//return -1;
		goto err_end;
	}

	rtk_ts_data_init(client);

	mdelay(60);

	ret = rtk_ts_i2c_test(client);
	if (ret >= 0) {
		mdelay(20);

		ret = rtk_ts_firmware_info(client);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to get firmware info\n");
			//return -1;
			goto err_end;
		}
	}

	ret = rtk_input_dev_init(ts_data);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to init input_dev\n");
		goto err_end;
	}

	ret = rtk_irq_init(client);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to init IRQ\n");
		goto err_end;
	}

	return 0;

err_end:
	if (ts_data->pdata && gpio_is_valid(ts_data->pdata->reset_gpio)) {
		gpio_free(ts_data->pdata->reset_gpio);
	}
	if (ts_data->pdata && gpio_is_valid(ts_data->pdata->irq_gpio)) {
		gpio_free(ts_data->pdata->irq_gpio);
	}

	if (rtk_ts_data->use_irq != 0) {
		free_irq(rtk_ts_data->use_irq, NULL);
	}

	if (ts_data->input_dev != NULL) {
		input_free_device(ts_data->input_dev);
	}

	if (ts_data->pdata != NULL) {
		kfree(ts_data->pdata);
	}

	if (ts_data != NULL) {
		kfree(ts_data);
	}

	return ret;
}

static void rtk_ts_enter_sleep(struct i2c_client *client)
{
	int ret;
	int retry = 0;
	unsigned char buf[2];

	buf[0] = 0xD1;
	buf[1] = 0x05;
	while (retry++ < 5) {
		ret = rtk_ts_i2c_write(client, buf, 2);
		if (ret > 0) {
			return;
		}
		mdelay(2);
	}

	return;
}

static int rtk_ts_remove(struct i2c_client *client)
{
	struct rtk_ts_data *ts = i2c_get_clientdata(client);

	if (!ts) {
		return -EINVAL;
	}

	if (ts->pdata && gpio_is_valid(ts->pdata->reset_gpio)) {
		gpio_free(ts->pdata->reset_gpio);
	}
	if (ts->pdata && gpio_is_valid(ts->pdata->irq_gpio)) {
		gpio_free(ts->pdata->irq_gpio);
	}

	if (ts->use_irq != 0) {
		free_irq(ts->use_irq, NULL);
	}

	input_unregister_device(ts->input_dev);

	if (ts->pdata != NULL) {
		kfree(ts->pdata);
	}

	if (ts != NULL) {
		kfree(ts);
	}

	return 0;
}

static int __maybe_unused rtk_ts_suspend(struct device *dev)
{
	int idx;
	struct i2c_client *client = to_i2c_client(dev);
	struct rtk_ts_data *ts = i2c_get_clientdata(client);

	rtk_ts_irq_disable();

	for (idx = 0; idx <= 10; idx++) {
		input_mt_slot(ts->input_dev, idx);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
	}
	input_sync(ts->input_dev);

	rtk_ts_enter_sleep(client);

	return 0;
}

static int __maybe_unused rtk_ts_resume(struct device *dev)
{
	int idx;

	struct i2c_client *client = to_i2c_client(dev);
	struct rtk_ts_data *ts = i2c_get_clientdata(client);

	for (idx = 0; idx <= 10; idx++) {
		input_mt_slot(ts->input_dev, idx);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
	}
	input_sync(ts->input_dev);

	rtk_ts_irq_enable();

	return 0;
}

static SIMPLE_DEV_PM_OPS(rtk_pm_ops, rtk_ts_suspend, rtk_ts_resume);

static struct i2c_driver rtk_ts_driver = {
	.driver = {
		.name = RTK_DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(rtk_dt_match),
		.pm = &rtk_pm_ops,
	},
	.probe = rtk_ts_probe,
	.remove = rtk_ts_remove,
	.id_table = rtk_ts_id,

};

module_i2c_driver(rtk_ts_driver);

MODULE_DESCRIPTION("Realtek Ameba Touchscreen driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
