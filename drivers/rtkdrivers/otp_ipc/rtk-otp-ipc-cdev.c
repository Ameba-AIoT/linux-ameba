// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek OTP CTRL support
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
#include <linux/errno.h>

#include "rtk-otp-ipc.h"

struct otp_ipc_tx_msg {
	int otp_id;
	int offset;
	int len;
	u8 tx_value[OPT_REQ_MSG_PARAM_NUM];
	u8 tx_lock;
};

struct user_space_request {
	u8	*result;
	struct otp_ipc_tx_msg otp_order;
};

struct realtek_otp_dev {
	struct cdev cdev;
	int current_affair;
};
struct realtek_otp_dev *realtek_otp_devp;

struct otp_init_table {
	u32 user_msg_type;
	int irq;
	u32 otp_direction;
	u32 otp_channel;
};

static int realtek_otp_major;
static spinlock_t lock;

int realtek_otp_open(struct inode *inode, struct file *filp)
{
	pr_debug("Realtek OTP: open success.");
	return 0;
}

int realtek_otp_release(struct inode *inode, struct file *filp)
{
	pr_debug("\nRealtek OTP: release.");
	return 0;
}

static long realtek_otp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct user_space_request otp_req;
	otp_ipc_host_req_t preq_msg;
	u8 result[OPT_REQ_MSG_PARAM_NUM];
	u8 write = 0;

	spin_lock(&lock);

	if (copy_from_user(&otp_req,
				(struct user_space_request __user *)arg,
				sizeof(struct user_space_request)))
		return -EFAULT;

	preq_msg.otp_id = otp_req.otp_order.otp_id;
	preq_msg.addr = otp_req.otp_order.offset;
	preq_msg.len = otp_req.otp_order.len;
	preq_msg.write_lock = otp_req.otp_order.tx_lock;
	if (preq_msg.otp_id == LINUX_IPC_OTP_PHY_WRITE8 || preq_msg.otp_id == LINUX_IPC_OTP_LOGI_WRITE_MAP) {
		if (preq_msg.len > OPT_REQ_MSG_PARAM_NUM) {
			spin_unlock(&lock);
			return -EFAULT;
		}
		memcpy(preq_msg.param_buf, otp_req.otp_order.tx_value, preq_msg.len);
		write = 1;
	}

	ret = rtk_otp_process(&preq_msg, result);
	if (ret < 0) {
		spin_unlock(&lock);
		return ret;
	}

	/* copy memery from kernel space to user space. */
	if (!write) {
		ret = copy_to_user(otp_req.result, result, otp_req.otp_order.len) ? -EFAULT : ret;
	}

	spin_unlock(&lock);
	return ret;
}

static ssize_t realtek_logic_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	otp_ipc_host_req_t preq_msg;
	u8 result_otp[OPT_REQ_MSG_PARAM_NUM];
	int ret;

	spin_lock(&lock);
	preq_msg.otp_id = 2;
	preq_msg.addr = 0;
	preq_msg.len = 1024;
	preq_msg.write_lock = 0;
	ret = rtk_otp_process(&preq_msg, result_otp);
	ret = copy_to_user(user_buf, result_otp, OPT_REQ_MSG_PARAM_NUM) ? -EFAULT : ret;
	spin_unlock(&lock);

	return 0;
}

static const struct file_operations realtek_otp_fops = {
	.owner = THIS_MODULE,
	.open = realtek_otp_open,
	.read = realtek_logic_read,
	.release = realtek_otp_release,
	.unlocked_ioctl	= realtek_otp_ioctl,
};

static void realtek_otp_setup_cdev(struct realtek_otp_dev *dev, int index)
{
	int err, devno = MKDEV(realtek_otp_major, index);

	cdev_init(&dev->cdev, &realtek_otp_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &realtek_otp_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		pr_err("Error %d add realtek_otp %d", err, index);
	}
}

int realtek_otp_init(void)
{
	int ret;
	dev_t devno;
	struct class *realtek_otp_class;

	ret = alloc_chrdev_region(&devno, 0, 1, "otp-ctrl");
	if (ret < 0) {
		return ret;
	}
	realtek_otp_major = MAJOR(devno);

	realtek_otp_devp = kmalloc(sizeof(struct realtek_otp_dev), GFP_KERNEL);
	if (!realtek_otp_devp) {
		ret = -ENOMEM;
		pr_err("Error add realtek_otp");
		goto fail_malloc;
	}

	memset(realtek_otp_devp, 0, sizeof(struct realtek_otp_dev));
	realtek_otp_setup_cdev(realtek_otp_devp, 0);

	realtek_otp_class = class_create(THIS_MODULE, "otp-ctrl");
	device_create(realtek_otp_class, NULL, MKDEV(realtek_otp_major, 0), NULL, "otp-ctrl");

	spin_lock_init(&lock);

	return 0;
fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

void realtek_otp_exit(void)
{
	pr_debug("End realtek_otp");
	cdev_del(&realtek_otp_devp->cdev);
	kfree(realtek_otp_devp);
	unregister_chrdev_region(MKDEV(realtek_otp_major, 0), 1);
}

fs_initcall(realtek_otp_init);
module_exit(realtek_otp_exit);

MODULE_DESCRIPTION("Realtek Ameba OTP CTRL driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");