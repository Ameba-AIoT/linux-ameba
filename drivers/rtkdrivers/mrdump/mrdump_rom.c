/*
 * Copyright (C) 2021 Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/errno.h>

#include "mrdump.h"

static unsigned int mrdump_iv = 0xaabbccdd;
struct mrdump_data_ops *data_ops;

static void mrdump_block_scramble(void *buf, size_t len)
{
	unsigned int i;
	unsigned int *p = (unsigned int *)buf;

	for (i = 0; i < len; i += 4, p++) {
		*p = *p ^ mrdump_iv;
	}
}

int mrdump_data_read(loff_t off, size_t len, void *buf, int encrypt)
{
	int ret;

	if (!data_ops || !data_ops->read) {
		return -EINVAL;
	}

	ret = data_ops->read(off, len, buf);
	if (ret < 0) {
		goto out;
	}

	if (encrypt) {
		mrdump_block_scramble(buf, len);
	}
out:
	return ret;
}

int mrdump_data_write(loff_t off, size_t len, void *buf, int encrypt)
{
	if (!data_ops || !data_ops->write) {
		return -EINVAL;
	}

	if (encrypt) {
		mrdump_block_scramble(buf, len);
	}

	return data_ops->write(off, len, buf);
}

int mrdump_data_erase(void)
{
	if (!data_ops || !data_ops->erase) {
		return -EINVAL;
	}

	return data_ops->erase();
}

int mrdump_data_sync(void)
{
	if (!data_ops || !data_ops->sync) {
		return -EINVAL;
	}

	return data_ops->sync();
}

int mrdump_data_open(const char *path, int flags, int rights)
{
	if (!data_ops || !data_ops->open) {
		return -EINVAL;
	}

	return data_ops->open(path, flags, rights);
}

int mrdump_data_close(void)
{
	if (!data_ops || !data_ops->close) {
		return -EINVAL;
	}

	return data_ops->close();
}

void register_mrdump_data_ops(struct mrdump_data_ops *ops)
{
	data_ops = ops;
}

void unregister_mrdump_data_ops(void)
{
	data_ops = NULL;
}