#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <asm/div64.h>

#include "mrdump.h"

struct mrdump_mtd_device {
	struct mtd_info *mtd;
	u32 writesize_shift;
	u32 blk_nr;
	u32 blk_offset[1024];
};

static struct mrdump_mtd_device *mrdump_mtd_dev;

static int mrdump_mtd_erase(void)
{
	struct mrdump_mtd_device *dev = mrdump_mtd_dev;
	struct erase_info ei;
	int err, i;

	ei.len = dev->mtd->erasesize;

	for (i = 0; i < dev->mtd->size; i += dev->mtd->erasesize) {
		ei.addr = i;

		err = mtd_block_isbad(dev->mtd, i);
		if (err < 0) {
			pr_err("%s: error(%d) check bad block failed\n",
				   __func__, err);
			return err;
		}

		if (err) {
			pr_info("%s: skipping erase of bad block @%llx\n",
					__func__, ei.addr);
			continue;
		}

		err = mtd_erase(dev->mtd, &ei);
		if (err) {
			pr_err("%s: erase 0x%llx, +0x%llx failed\n", __func__,
				   (unsigned long long)ei.addr,
				   (unsigned long long)ei.len);

			if (err == -EIO) {
				if (mtd_block_markbad(dev->mtd, ei.addr)) {
					pr_err("%s: error(%d) mark bad block failed\n",
						   __func__, err);
					return err;
				}

				pr_info("%s: err marking bad block %llx\n",
						__func__, ei.addr);
				continue;
			}
			return err;
		}
		cond_resched();
	}

	pr_debug("%s partition erased\n", dev->mtd->name);
	return 0;
}

static int mrdump_mtd_block_read(loff_t from, size_t rlen, size_t *retlen,
								 void *buf)
{
	struct mrdump_mtd_device *dev = mrdump_mtd_dev;
	int index, err;
	loff_t offset;
	u64 start = (u64)from;

	index = from >> dev->writesize_shift;
	if (index < 0 || index >= dev->blk_nr) {
		return -EINVAL;
	}

	offset = dev->blk_offset[index] + do_div(start, dev->mtd->writebufsize);

	err = mtd_read(dev->mtd, offset, rlen, retlen, buf);
	/* Ignore corrected ECC errors */
	if (mtd_is_bitflip(err)) {
		err = 0;
	}
	if (!err && *retlen != rlen) {
		err = -EIO;
	}

	return err;
}

static int mrdump_mtd_read(loff_t off, size_t len, size_t *buf)
{
	struct mrdump_mtd_device *dev = mrdump_mtd_dev;
	size_t rlen, retlen = 0;
	loff_t start = off;
	int err;

	if (mrdump_mtd_dev == NULL || mrdump_mtd_dev->mtd == NULL) {
		pr_err("%s: mtd is not ready\n", __func__);
		return -ENODEV;
	}

	if (len == 0) {
		return 0;
	}

	if (off + len > dev->mtd->size) {
		len = dev->mtd->size - off;
	}

	/* read first page */
	if (off & (mrdump_mtd_dev->mtd->writebufsize - 1)) {
		u64 temp = (u64)off;
		rlen = dev->mtd->writebufsize -
			   do_div(temp, dev->mtd->writebufsize);
		err = mrdump_mtd_block_read(off, rlen, &retlen, buf);
		if (err < 0) {
			goto err_out;
		}

		off += retlen;
		buf += retlen;
		len -= retlen;
	}

	/* read left page aligned data */
	while (len > 0) {
		rlen = len > dev->mtd->writebufsize ? dev->mtd->writebufsize :
			   len;
		err = mrdump_mtd_block_read(off, rlen, &retlen, buf);
		if (err < 0) {
			goto err_out;
		}

		off += retlen;
		buf += retlen;
		len -= retlen;
	}

	return (off - start);

err_out:
	pr_err("%s: error(%d): write failed at %#llx\n", __func__, err, off);
	return err;
}

static int mrdump_mtd_block_write(loff_t to, size_t wlen, size_t *retlen,
								  const void *buf)
{
	struct mrdump_mtd_device *dev = mrdump_mtd_dev;
	int index, err;
	loff_t offset;
	u64 start = (u64)to;

	index = to >> dev->writesize_shift;
	if (index < 0 || index >= dev->blk_nr) {
		return -EINVAL;
	}

	offset = dev->blk_offset[index] + do_div(start, dev->mtd->writebufsize);

	if (in_interrupt()) {
		err = mtd_panic_write(dev->mtd, offset, wlen, retlen, buf);
	} else {
		err = mtd_write(dev->mtd, offset, wlen, retlen, buf);
	}

	if (err) {
		pr_err("%s: error(%d) writing data to flash\n", __func__, err);
	}

	return err;
}

static int mrdump_mtd_write(loff_t off, size_t len, const size_t *buf)
{
	struct mrdump_mtd_device *dev = mrdump_mtd_dev;
	size_t wlen, retlen = 0;
	loff_t start = off;
	int err;

	if (dev == NULL || dev->mtd == NULL) {
		pr_err("%s: mtd is not ready\n", __func__);
		return -ENODEV;
	}

	if (len == 0) {
		return -EINVAL;
	}

	if (off + len > dev->mtd->size) {
		len = dev->mtd->size - off;
	}

	/* write first page */
	if (off & (dev->mtd->writebufsize - 1)) {
		u64 temp = (u64)off;
		wlen = dev->mtd->writebufsize -
			   do_div(temp, dev->mtd->writebufsize);
		err = mrdump_mtd_block_write(off, wlen, &retlen, buf);
		if (err < 0) {
			goto err_out;
		}

		off += retlen;
		buf += retlen;
		len -= retlen;
	}

	/* write left page aligned data */
	while (len > 0) {
		wlen = len > dev->mtd->writebufsize ? dev->mtd->writebufsize :
			   len;
		err = mrdump_mtd_block_write(off, wlen, &retlen, buf);
		if (err < 0) {
			goto err_out;
		}

		off += retlen;
		buf += retlen;
		len -= retlen;
	}

	return (off - start);

err_out:
	pr_err("%s: error(%d): write failed at %#llx\n", __func__, err, off);
	return err;
}

struct mrdump_data_ops mrdump_mtd_ops = {
	.read = mrdump_mtd_read,
	.write = mrdump_mtd_write,
	.erase = mrdump_mtd_erase,
};
EXPORT_SYMBOL(mrdump_mtd_ops);

static int mrdump_mtd_block_scan(struct mtd_info *mtd,
								 struct mrdump_mtd_device *dev)
{
	int index = 0, offset;

	/*
	 * Calcuate total number of non-bad blocks on NAND device,
	 * and record it's offset.
	 */
	for (offset = 0; offset < mtd->size; offset += mtd->writebufsize) {
		if (!mtd_block_isbad(mtd, offset)) {
			/* index can't surpass array size */
			if (index >= (sizeof(dev->blk_offset) / sizeof(u32))) {
				break;
			}

			dev->blk_offset[index++] = offset;
		}
	}

	dev->blk_nr = index;

	return 0;
}

static void mrdump_mtd_notify_add(struct mtd_info *mtd)
{
	static struct mrdump_mtd_device mrdump_device;
	struct mrdump_mtd_device *dev = &mrdump_device;

	if (strcmp(mtd->name, "expdb")) {
		return;
	}

	if (mrdump_mtd_block_scan(mtd, dev)) {
		return;
	}

	pr_info("Bound to '%s', write size(%d), write size shift(%d)\n",
			mtd->name, mtd->writebufsize, mtd->writesize_shift);

	dev->mtd = mtd;
	dev->writesize_shift = ffs(mtd->writebufsize) - 1;
	mrdump_mtd_dev = dev;
}

static void mrdump_mtd_notify_remove(struct mtd_info *mtd)
{
	struct mrdump_mtd_device *dev = mrdump_mtd_dev;

	if (dev->mtd == mtd) {
		dev->mtd = NULL;
		pr_info("mrdump: Ubound from %s\n", dev->mtd->name);
	}
}

static struct mtd_notifier mrdump_mtd_notifier = {
	.add = mrdump_mtd_notify_add,
	.remove = mrdump_mtd_notify_remove,
};

void mrdump_mtd_init(void)
{
	register_mtd_user(&mrdump_mtd_notifier);
}