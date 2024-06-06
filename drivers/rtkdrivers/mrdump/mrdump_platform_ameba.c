/*
 * Copyright (C) 2021 Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define pr_fmt(fmt)	"Ameba Platform: " fmt

#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/slab.h>

#include "mrdump.h"

static void mrdump_hw_enable(bool enabled)
{
	return;
}

static void mrdump_reboot(void)
{
	emergency_restart();
}

static struct mrdump_platform_ops mrdump_platform_ameba = {
	.enable = mrdump_hw_enable,
	.reboot = mrdump_reboot,
};

int mrdump_platform_init(struct mrdump_desc *mrdump_desc)
{
	if (mrdump_desc == NULL) {
		pr_err("mrdump_desc is null");
		return -ENOMEM;
	}

	mrdump_desc->plat_ops = &mrdump_platform_ameba;
	return 0;
}
