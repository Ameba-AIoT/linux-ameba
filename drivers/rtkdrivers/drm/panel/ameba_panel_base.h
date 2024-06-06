// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Panel support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _AMEBA_DRM_PANEL_BASE_H_
#define _AMEBA_DRM_PANEL_BASE_H_

#include "../ameba_drm_base.h"

typedef struct LCM_setting_table {
	u8                      cmd;
	u8                      count;
	u8                      para_list[128];
} LCM_setting_table_t;

//make sure below cmd is not conflict with the cmd that send to DSI
#define REGFLAG_DELAY                           0xFC        /* RUN DELAY */
#define REGFLAG_END_OF_TABLE                    0xFD        /* END OF REGISTERS MARKER */

struct ameba_drm_panel_struct {
	void                    *panel_priv;
	LCM_setting_table_t     *init_table;
};

#endif  /*_AMEBA_DRM_PANEL_BASE_H_*/
