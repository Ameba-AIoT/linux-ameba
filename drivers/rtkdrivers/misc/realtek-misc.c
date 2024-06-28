// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Misc support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <misc/realtek-misc.h>

#define MISC_CTRL	"misc-ioctl"

#define DDRC_BIT_DYN_SRE ((u32)0x00000001 << 7)

struct rtk_misc_dev {
	struct device device;
	struct cdev cdev;
	struct mutex misc_mutex;
	u32 uuid;
	int rlv;
	u32 ddr_auto_gating_ctrl;
	void __iomem *ddrc_iocr;
};

struct rtk_misc_dev *mdev;
static struct class *misc_class;

/* proc fs dir entry */
static struct proc_dir_entry *misc_proc_dir;
static struct proc_dir_entry *uuid_proc_ent;

extern int rtk_misc_get_rlv(void);

int rtk_misc_hw_ddrc_autogating(u8 enable)
{
	u32 val = 0;

	if (!mdev->ddr_auto_gating_ctrl) {
		return -ENODEV;
	}

	if (enable) {
		val = readl(mdev->ddrc_iocr);
		val |= DDRC_BIT_DYN_SRE;
		writel(val, mdev->ddrc_iocr);
	} else {
		val = readl(mdev->ddrc_iocr);
		val &= (~DDRC_BIT_DYN_SRE);
		writel(val, mdev->ddrc_iocr);
	}
	return 0;
}

/*
* uuid proc ops
*/
static int rtk_uuid_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", mdev->uuid);
	return 0;
}

static int rtk_uuid_proc_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, rtk_uuid_proc_show, NULL);
}

static const struct file_operations rtk_uuid_proc_fops = {
	.open = rtk_uuid_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static long rtk_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 val = 0;

	if (!mutex_trylock(&mdev->misc_mutex)) {
		return -EBUSY;
	}

	switch (cmd) {
	case RTK_MISC_IOC_RLV:
		val = (u32)mdev->rlv;
		ret = put_user(val, (__u32 __user *)arg);
		break;
	case RTK_MISC_IOC_UUID:
		val = mdev->uuid;
		ret = put_user(val, (__u32 __user *)arg);
		break;
	case RTK_MISC_IOC_DDRC_AUTO_GATE:
		ret = rtk_misc_hw_ddrc_autogating(1);
		break;
	case RTK_MISC_IOC_DDRC_DISGATE:
		ret = rtk_misc_hw_ddrc_autogating(0);
		break;
	default:
		dev_warn(&mdev->device, "Unsupported misc ioctl cmd: 0x%08X\n", cmd);
		break;
	}

	mutex_unlock(&mdev->misc_mutex);
	return ret;
}

static void rtk_misc_create_proc(struct device *dev)
{
	misc_proc_dir = proc_mkdir("realtek", NULL);
	if (!misc_proc_dir) {
		dev_err(dev, "Failed to mkdir for procsys\n");
		return;
	}

	uuid_proc_ent = proc_create("uuid", 0644, misc_proc_dir, &rtk_uuid_proc_fops);
}

static void rtk_misc_remove_proc(void)
{
	proc_remove(uuid_proc_ent);
	proc_remove(misc_proc_dir);
}

static int rtk_misc_get_uuid(struct device *dev, u32 *uuid)
{
	struct nvmem_cell *cell;
	unsigned char *efuse_buf;
	size_t len;

	cell = nvmem_cell_get(dev, "uuid");
	if (IS_ERR(cell)) {
		return PTR_ERR(cell);
	}

	efuse_buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(efuse_buf)) {
		return PTR_ERR(efuse_buf);
	}

	*uuid = (efuse_buf[3] << 24) | (efuse_buf[2] << 16)
			| (efuse_buf[1] << 8) | efuse_buf[0];

	return 0;
}

/*
* misc device file ops
*/
static const struct file_operations rtk_misc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = rtk_misc_ioctl,
};

int rtk_misc_init(void)
{
	int ret = 0;
	struct device_node *np;
	struct device *dev;
	dev_t devno;

	mdev = kmalloc(sizeof(struct rtk_misc_dev), GFP_KERNEL);
	if (!mdev) {
		pr_err("MISC: Failed to malloc mdev\n");
		ret = -ENOMEM;
		return ret;
	}

	memset(mdev, 0, sizeof(struct rtk_misc_dev));

	np = of_find_compatible_node(NULL, NULL, "realtek,ameba-misc");
	if (!np) {
		pr_err("MISC: Failed to find node\n");
		goto of_error;
	}

	mdev->ddr_auto_gating_ctrl = 0;
	mdev->ddrc_iocr = of_iomap(np, 0);
	if (!mdev->ddrc_iocr) {
		pr_err("MISC: Failed to iomap the reg.\n");
		goto of_iomap_error;
	}
	of_property_read_u32(np, "rtk,ddr-auto-gating-ctrl", &mdev->ddr_auto_gating_ctrl);

	misc_class = class_create(THIS_MODULE, MISC_CTRL);

	ret = alloc_chrdev_region(&devno, 0, 1, MISC_CTRL);
	if (ret < 0) {
		pr_err("MISC: Failed to alloc chrdev region\n");
		goto chrdev_error;
	}

	cdev_init(&mdev->cdev, &rtk_misc_fops);
	mdev->cdev.owner = THIS_MODULE;
	mdev->cdev.ops = &rtk_misc_fops;
	mdev->cdev.dev = devno;
	ret = cdev_add(&mdev->cdev, mdev->cdev.dev, 1);
	if (ret < 0) {
		pr_err("MISC: Failed to add cdev\n");
		goto cdev_error;
	}

	dev = device_create(misc_class, NULL, devno, NULL, MISC_CTRL);
	if (!dev) {
		pr_err("MISC: Failed to create device\n");
		goto device_error;
	}
	dev->of_node = np;
	mdev->device = *dev;

	mutex_init(&mdev->misc_mutex);

	rtk_misc_create_proc(dev);

	rtk_misc_get_uuid(dev, &mdev->uuid);
	mdev->rlv = rtk_misc_get_rlv();

	return 0;

device_error:
	cdev_del(&mdev->cdev);
cdev_error:
	unregister_chrdev_region(devno, 1);
chrdev_error:
	class_destroy(misc_class);
	iounmap(mdev->ddrc_iocr);
of_iomap_error:
	of_node_put(np);
of_error:
	kfree(mdev);
	return ret;
}

void rtk_misc_exit(void)
{
	rtk_misc_remove_proc();

	if (mdev != NULL) {
		device_destroy(misc_class, mdev->cdev.dev);
		class_destroy(misc_class);
		cdev_del(&mdev->cdev);
		unregister_chrdev_region(mdev->cdev.dev, 1);
		iounmap(mdev->ddrc_iocr);
		of_node_put(mdev->device.of_node);
		kfree(mdev);
		mdev = NULL;
	}
}

module_init(rtk_misc_init);
module_exit(rtk_misc_exit);

MODULE_DESCRIPTION("Realtek Ameba Misc driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");

