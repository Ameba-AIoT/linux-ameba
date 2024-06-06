// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Km4-console support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/errno.h>

#include <misc/realtek-console-core.h>

static int realtek_console_major;
static spinlock_t lock;

struct realtek_console_dev {
	struct cdev cdev;
};
struct realtek_console_dev *realtek_console_devp;

int realtek_console_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int realtek_console_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long realtek_console_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	char preq_msg[CONSOLE_MAX_CHAR];
	u8 result[CONSOLE_MAX_CHAR];

	spin_lock(&lock);

	memset(preq_msg, 0, CONSOLE_MAX_CHAR);
	if (copy_from_user(&preq_msg, (char __user *)arg, CONSOLE_MAX_CHAR - 1)) {
		return -EFAULT;
	}

	ret = rtk_console_process(preq_msg, strlen(preq_msg), result);
	if (ret < 0) {
		spin_unlock(&lock);
		return ret;
	}

	spin_unlock(&lock);
	return ret;
}

static const struct file_operations realtek_console_fops = {
	.owner = THIS_MODULE,
	.open = realtek_console_open,
	.release = realtek_console_release,
	.unlocked_ioctl	= realtek_console_ioctl,
};

static void realtek_console_setup_cdev(struct realtek_console_dev *dev, int index)
{
	int err, devno = MKDEV(realtek_console_major, index);

	cdev_init(&dev->cdev, &realtek_console_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &realtek_console_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		pr_err("Error: Failed to add realtek console: %d\n", err);
	}
}

int realtek_console_init(void)
{
	int ret;
	dev_t devno;
	struct class *realtek_console_class;

	ret = alloc_chrdev_region(&devno, 0, 1, "console-ctrl");
	if (ret < 0) {
		return ret;
	}
	realtek_console_major = MAJOR(devno);

	realtek_console_devp = kmalloc(sizeof(struct realtek_console_dev), GFP_KERNEL);
	if (!realtek_console_devp) {
		ret = -ENOMEM;
		pr_err("Error: Failed to alloc realtek console\n");
		goto fail_malloc;
	}

	memset(realtek_console_devp, 0, sizeof(struct realtek_console_dev));
	realtek_console_setup_cdev(realtek_console_devp, 0);

	realtek_console_class = class_create(THIS_MODULE, "console-ctrl");
	device_create(realtek_console_class, NULL, MKDEV(realtek_console_major, 0), NULL, "console-ctrl");

	spin_lock_init(&lock);

	return 0;
fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

void realtek_console_exit(void)
{
	pr_info("Realtek console exit\n");
	cdev_del(&realtek_console_devp->cdev);
	kfree(realtek_console_devp);
	unregister_chrdev_region(MKDEV(realtek_console_major, 0), 1);
}

fs_initcall(realtek_console_init);
module_exit(realtek_console_exit);

MODULE_DESCRIPTION("Realtek Ameba Console driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
