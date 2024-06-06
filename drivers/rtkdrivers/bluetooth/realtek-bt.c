// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Bluetooth control support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/realtek-bt.h>

#define BT_CDEV_CTRL			"bt-cdev"

#define RTK_BT_CONTROLLER_POWER	0x200

struct rtk_bt_cdev {
	struct cdev cdev;
	struct device *device;
	struct class *bt_cdev_class;
	struct mutex bt_cdev_mutex;
	void __iomem *base;
	u8 bt_ant_switch;
};

struct rtk_bt_cdev *btdev;

static u32 rtk_bt_cal_bit_shift(u32 mask)
{
	u32 i;
	for (i = 0; i < 31; i++) {
		if (((mask >> i) & 0x1) == 1) {
			break;
		}
	}
	return i;
}

static void rtk_bt_set_reg_value(void __iomem *reg_address, u32 mask, u32 val)
{
	u32 shift = 0;
	u32 data = 0;

	data = readl(reg_address);
	shift = rtk_bt_cal_bit_shift(mask);
	data = ((data & (~mask)) | (val << shift));
	writel(data, reg_address);
	data = readl(reg_address);
}

static void rtk_bt_power_off(void __iomem *reg_base)
{
	rtk_bt_set_reg_value(reg_base + 0x8, BIT(13), 0);
	mdelay(5);
	rtk_bt_set_reg_value(reg_base, BIT(25), 0);
	mdelay(5);

	if (btdev->bt_ant_switch == 0) {
		rtk_bt_set_reg_value(reg_base + 0x50, BIT(1) | BIT(2), 0);
		mdelay(5);
	} else {
		rtk_bt_set_reg_value(reg_base + 0x50, BIT(1) | BIT(0), 0);
		mdelay(5);
	}

	rtk_bt_set_reg_value(reg_base + 0x4, BIT(9), 1);
	mdelay(5);
	rtk_bt_set_reg_value(reg_base, BIT(9), 0);
	mdelay(5);
}

static void rtk_bt_power_on(void __iomem *reg_base)
{
	rtk_bt_set_reg_value(reg_base, BIT(9), 1);
	mdelay(5);
	rtk_bt_set_reg_value(reg_base + 0x4, BIT(9), 0);
	mdelay(5);
	rtk_bt_set_reg_value(reg_base + 0x50, BIT(21) | BIT(22), 3);
	mdelay(5);
	rtk_bt_set_reg_value(reg_base + 0x50, BIT(1) | BIT(2), 3);
	mdelay(5);

	if (btdev->bt_ant_switch == 0) {
		rtk_bt_set_reg_value(reg_base + 0x50, BIT(0), 0);
		mdelay(5);
	} else if (btdev->bt_ant_switch == 1) {
		rtk_bt_set_reg_value(reg_base + 0x50, BIT(0), 1);
		mdelay(5);
		rtk_bt_set_reg_value(reg_base + 0x8, BIT(24), 1);
		mdelay(5);
		rtk_bt_set_reg_value(reg_base + 0x740, BIT(5) | BIT(6), 3);
		mdelay(5);
	}

	rtk_bt_set_reg_value(reg_base, BIT(25), 1);
	mdelay(5);
	rtk_bt_set_reg_value(reg_base + 0x8, BIT(13), 1);
}

static int rtk_bt_cdev_set_power(struct rtk_bt_power_info *info)
{
	u8 power_on;
	if (get_user(power_on, (u8 __user *)(&info->power_on)))
		return -EFAULT;

	if (get_user(btdev->bt_ant_switch, (u8 __user *)(&info->bt_ant_switch)))
		return -EFAULT;

	pr_debug("Bt-cdev: %s: power_on: %u, bt_ant_switch: %u\n", __FUNCTION__, power_on, btdev->bt_ant_switch);

	if (power_on) {
		rtk_bt_power_on(btdev->base + RTK_BT_CONTROLLER_POWER);
		mdelay(55);
	} else {
		rtk_bt_power_off(btdev->base + RTK_BT_CONTROLLER_POWER);
	}

	return 0;
}

static long rtk_bt_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 val = 0;

	if (!mutex_trylock(&btdev->bt_cdev_mutex)) {
		return -EBUSY;
	}

	switch (cmd) {
	case RTK_BT_IOC_SET_BT_POWER:
		ret = rtk_bt_cdev_set_power((struct rtk_bt_power_info *)arg);
		break;
	default:
		pr_warn("Bt-cdev: Unsupported rtk bluetooth ioctl cmd: %d\n", cmd);
		break;
	}

	mutex_unlock(&btdev->bt_cdev_mutex);
	return ret;
}

/*
* bt cdev file ops
*/
static const struct file_operations rtk_bt_cdev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = rtk_bt_cdev_ioctl,
};

int rtk_bt_cdev_init(void)
{
	int ret = 0;
	struct device_node *np;
	struct device *dev;
	dev_t devno;

	btdev = kmalloc(sizeof(struct rtk_bt_cdev), GFP_KERNEL);
	if (!btdev) {
		pr_err("Bt-cdev: %s malloc failed\n", __FUNCTION__);
		ret = -ENOMEM;
		return ret;
	}

	memset(btdev, 0, sizeof(struct rtk_bt_cdev));

	np = of_find_compatible_node(NULL, NULL, "realtek,ameba-bt-ctrl");
	if (!np) {
		pr_err("Bt-cdev: %s: of find node error\n", __FUNCTION__);
		goto of_error;
	}

	btdev->base = of_iomap(np, 0);
	if (!btdev->base) {
		pr_err("Bt-cdev: %s: iomap failed\n", __FUNCTION__);
		goto of_iomap_error;
	}

	btdev->bt_cdev_class = class_create(THIS_MODULE, BT_CDEV_CTRL);

	ret = alloc_chrdev_region(&devno, 0, 1, BT_CDEV_CTRL);
	if (ret < 0) {
		pr_err("Bt-cdev: %s: alloc_chrdev_region failed\n", __FUNCTION__);
		goto chrdev_error;
	}

	cdev_init(&btdev->cdev, &rtk_bt_cdev_fops);
	btdev->cdev.owner = THIS_MODULE;
	btdev->cdev.ops = &rtk_bt_cdev_fops;
	btdev->cdev.dev = devno;
	ret = cdev_add(&btdev->cdev, btdev->cdev.dev, 1);
	if (ret < 0) {
		pr_err("Bt-cdev: %s: add cdev failed\n", __FUNCTION__);
		goto cdev_error;
	}

	dev = device_create(btdev->bt_cdev_class, NULL, devno, NULL, BT_CDEV_CTRL);
	if (!dev) {
		pr_err("Bt-cdev: %s: create device failed\n", __FUNCTION__);
		goto device_error;
	}
	dev->of_node = np;
	btdev->device = dev;

	mutex_init(&btdev->bt_cdev_mutex);

	pr_info("Bt-cdev: bluetooth ioctl driver probe success!\n");

	return 0;

device_error:
	cdev_del(&btdev->cdev);
cdev_error:
	unregister_chrdev_region(devno, 1);
chrdev_error:
	class_destroy(btdev->bt_cdev_class);
	iounmap(btdev->base);
of_iomap_error:
	of_node_put(np);
of_error:
	kfree(btdev);
	return ret;
}

void rtk_bt_cdev_exit(void)
{
	if (btdev != NULL) {
		device_destroy(btdev->bt_cdev_class, btdev->cdev.dev);
		class_destroy(btdev->bt_cdev_class);
		cdev_del(&btdev->cdev);
		unregister_chrdev_region(btdev->cdev.dev, 1);
		iounmap(btdev->base);
		of_node_put(btdev->device->of_node);
		kfree(btdev);
		btdev = NULL;
	}
}

module_init(rtk_bt_cdev_init);
module_exit(rtk_bt_cdev_exit);

MODULE_DESCRIPTION("Realtek Ameba Bluetooth ioctl driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");

