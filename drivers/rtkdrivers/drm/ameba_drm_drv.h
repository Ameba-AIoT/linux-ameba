// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DRM support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __KIRIN_DRM_DRV_H__
#define __KIRIN_DRM_DRV_H__

struct ameba_drm_reg_t {
	void __iomem        *reg;
};

//drv main struct
struct ameba_drm_struct {
	struct device           *dev;
	struct drm_device       *drm;
	struct device           *dsi_dev;           // dsi dev handle,used for handle underflow

	void                    *ameba_drm_priv;
	struct drm_crtc         *crtc;

	u32                     display_hdisplay; // display_width;
	u32                     display_hsync;
	u32                     display_hfp;
	u32                     display_hbp;
	u32                     display_htotal;
	u32                     display_vdisplay; // display_height;
	u32                     display_vsync;
	u32                     display_vfp;
	u32                     display_vbp;
	u32                     display_vtotal;
	u32                     display_clock;

	u32                     under_flow_count;    // total underflow count
	volatile u32            under_flow_flag;     // one time underflow count

	void                    *lcdc_hw_ctx;        // struct lcdc_hw_ctx_type handle
};


#endif /* __KIRIN_DRM_DRV_H__ */
