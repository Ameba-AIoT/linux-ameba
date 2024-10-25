// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DRM support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _AMEBA_DRM_BASE_H_
#define _AMEBA_DRM_BASE_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/io.h>
#include <drm/drm_print.h>

#define LCDC_MAX_REMOTE_DEV                 (2)

//dsi default value
#define LCDC_KG_COLOR                       0xFFFFFFFF
#define LCDC_UNDFLOW_COLOR                  0xFFFFFFFF

#define DRM_DEBUG_VALUE                     0x80000000  // enable ameba_drm_debug mask

#define LCDC_LAY_COLOR_KEY                  0x00000000
#define LCDC_LAY_BLEND_ALPHA                0xff

#define TRIGGER_DUMP_LDCD_REG               (10)  // lcdc reg dump
#define UPDATE_DRM_DEBUG_MASK               (20)  // drm debug mask
#define UPDATE_AMEBA_DRM_DEBUG              (30)  // enable AMEBA_DRM_DEBUG
#define TRIGGER_DUMP_DSI_REG                (10)  // dsi reg dump

#define assert_param(expr)                  ((expr) ? (void)0 : printk("[DRM]assert issue:%s,%d",__func__,__LINE__))
#define AMEBA_DRM_DEBUG()                   if(DRM_DEBUG_VALUE == drm_debug)DRM_INFO("%s Enter %d\n", __func__, __LINE__)

#endif  /*_AMEBA_DRM_BASE_H_*/
