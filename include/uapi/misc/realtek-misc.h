// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
* Realtek Misc support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _UAPI_REALTEK_MISC_H_
#define _UAPI_REALTEK_MISC_H_

#include <linux/types.h>
#include <linux/ioctl.h>

/* ATTENTION */
/* Please keep the realtek-misc header for kernel space and user space in sync manually. */
/* Misc header for kernel space: <sdk>/kernel/linux-5.4/include/uapi/misc/realtek-misc.h */

/* IOCTL commands */
#define MISC_IOC_MAGIC              'm'

#define RTK_MISC_IOC_RLV            _IOR(MISC_IOC_MAGIC, 1, __u32)
#define RTK_MISC_IOC_UUID           _IOR(MISC_IOC_MAGIC, 2, __u32)
#define RTK_MISC_IOC_DDRC_AUTO_GATE _IOR(MISC_IOC_MAGIC, 3, __u32)
#define RTK_MISC_IOC_DDRC_DISGATE   _IOR(MISC_IOC_MAGIC, 4, __u32)


#endif
