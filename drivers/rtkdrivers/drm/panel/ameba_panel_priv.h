// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Panel support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _AMEBA_DRM_PANEL_PRIV_H_
#define _AMEBA_DRM_PANEL_PRIV_H_

struct ameba_panel_desc {
	struct device                   *dev;
	LCM_setting_table_t             *init_table;
	struct drm_display_mode         *panel_module;
	void                            *priv;
	struct drm_panel_funcs          *rtk_panel_funcs;
	struct drm_panel                panel;

	int(*init)(struct device *dev,struct ameba_panel_desc *priv_data);
	int(*deinit)(struct device *dev,struct ameba_panel_desc *priv_data);
};

static inline struct ameba_panel_desc *panel_to_desc(struct drm_panel *panel)
{
	return container_of(panel, struct ameba_panel_desc, panel);
}

#endif  /*_AMEBA_DRM_PANEL_PRIV_H_*/
