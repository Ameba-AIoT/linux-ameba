/*
 * Copyright (C) 2021 Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "mrdump.h"

extern struct mrdump_desc *mrdump_desc;

static int file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;

	filp = filp_open(path, flags, rights);

	if (IS_ERR(filp)) {
		return PTR_ERR(filp);
	}

	mrdump_desc->file = filp;
	return 0;
}

static int file_close(void)
{
	struct file *file = mrdump_desc->file;

	return filp_close(file, NULL);
}

static int file_read(loff_t offset, size_t size, size_t *data)
{
	struct file *file = mrdump_desc->file;

	return kernel_read(file, (void *)data, size, &offset);
}

static int file_write(loff_t offset, size_t size, const size_t *data)
{
	struct file *file = mrdump_desc->file;

	return kernel_write(file, (void *)data, size, &offset);
}

static int file_sync(void)
{
	struct file *file = mrdump_desc->file;
	vfs_fsync(file, 0);

	return 0;
}

struct mrdump_data_ops mrdump_file_ops = {
	.open	= file_open,
	.close	= file_close,
	.read	= file_read,
	.write	= file_write,
	.sync	= file_sync,
};
EXPORT_SYMBOL(mrdump_file_ops);
