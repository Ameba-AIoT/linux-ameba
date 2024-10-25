// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Panel support
*
* MIPI-DSI st7701s panel driver. This is a 480*800
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <drm/drm_modes.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_drv.h>
#include <video/mipi_display.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/of_device.h>
//#include <linux/module.h>

#include "ameba_panel_base.h"
#include "ameba_panel_priv.h"

struct st7701s {
	int                     gpio;
};

/*
 * The timings are not very helpful as the display is used in
 * command mode.
 */
static struct drm_display_mode st7701s_mode = {
	/* HS clock, (htotal*vtotal*vrefresh)/1000 */
	.clock = 30720,
	.hdisplay = 480,
	.hsync_start = 481,
	.hsync_end = 484,
	.htotal = 500,
	.vdisplay = 800,
	.vsync_start = 824,
	.vsync_end = 896,
	.vtotal = 1024,

	.vrefresh = 60,
};

static LCM_setting_table_t st7701s_initialization[] = {/* DCS Write Long */
	/* ST7701S Reset Sequence */
	/* LCD_Nreset (1); Delayms (1); */
	/* LCD_Nreset (0); Delayms (1); */
	/* LCD_Nreset (1); Delayms (120); */
	{MIPI_DSI_DCS_SHORT_WRITE, 1, {0x11, 0x00}},
	{REGFLAG_DELAY, 120, {}},/* Delayms (120) */

	/* Bank0 Setting */
	/* Display Control setting */
	{MIPI_DSI_DCS_LONG_WRITE, 6, {0xFF, 0x77, 0x01, 0x00, 0x00, 0x10}},
	{MIPI_DSI_DCS_LONG_WRITE, 3, {0xC0, 0x63, 0x00}},
	{MIPI_DSI_DCS_LONG_WRITE, 3, {0xC1, 0x0C, 0x02}},
	{MIPI_DSI_DCS_LONG_WRITE, 3, {0xC2, 0x31, 0x08}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xCC, 0x10}},

	/* Gamma Cluster Setting */
	{MIPI_DSI_DCS_LONG_WRITE, 17, {0xB0, 0x40, 0x02, 0x87, 0x0E, 0x15, 0x0A, 0x03, 0x0A, 0x0A, 0x18, 0x08, 0x16, 0x13, 0x07, 0x09, 0x19}},
	{MIPI_DSI_DCS_LONG_WRITE, 17, {0xB1, 0x40, 0x01, 0x86, 0x0D, 0x13, 0x09, 0x03, 0x0A, 0x09, 0x1C, 0x09, 0x15, 0x13, 0x91, 0x16, 0x19}},
	/* End Gamma Setting */
	/* End Display Control setting */
	/* End Bank0 Setting */

	/* Bank1 Setting */
	/* Power Control Registers Initial */
	{MIPI_DSI_DCS_LONG_WRITE, 6, {0xFF, 0x77, 0x01, 0x00, 0x00, 0x11}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB0, 0x4D}},

	/* Vcom Setting */
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB1, 0x64}},
	/* End End Vcom Setting */

	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB2, 0x07}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB3, 0x80}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB5, 0x47}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB7, 0x85}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB8, 0x21}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xB9, 0x10}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xC1, 0x78}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xC2, 0x78}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xD0, 0x88}},
	/* End Power Control Registers Initial */
	{REGFLAG_DELAY, 100, {}},/* Delayms (100) */
	/* GIP Setting */
	{MIPI_DSI_DCS_LONG_WRITE, 4, {0xE0, 0x00, 0x84, 0x02}},
	{MIPI_DSI_DCS_LONG_WRITE, 12, {0xE1, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20}},
	{MIPI_DSI_DCS_LONG_WRITE, 14, {0xE2, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{MIPI_DSI_DCS_LONG_WRITE, 5, {0xE3, 0x00, 0x00, 0x33, 0x33}},
	{MIPI_DSI_DCS_LONG_WRITE, 3, {0xE4, 0x44, 0x44}},
	{MIPI_DSI_DCS_LONG_WRITE, 17, {0xE5, 0x09, 0x31, 0xBE, 0xA0, 0x0B, 0x31, 0xBE, 0xA0, 0x05, 0x31, 0xBE, 0xA0, 0x07, 0x31, 0xBE, 0xA0}},
	{MIPI_DSI_DCS_LONG_WRITE, 5, {0xE6, 0x00, 0x00, 0x33, 0x33}},
	{MIPI_DSI_DCS_LONG_WRITE, 3, {0xE7, 0x44, 0x44}},
	{MIPI_DSI_DCS_LONG_WRITE, 17, {0xE8, 0x08, 0x31, 0xBE, 0xA0, 0x0A, 0x31, 0xBE, 0xA0, 0x04, 0x31, 0xBE, 0xA0, 0x06, 0x31, 0xBE, 0xA0}},
	{MIPI_DSI_DCS_LONG_WRITE, 17, {0xEA, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00}},
	{MIPI_DSI_DCS_LONG_WRITE, 8, {0xEB, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{MIPI_DSI_DCS_LONG_WRITE, 3, {0xEC, 0x02, 0x00}},
	{MIPI_DSI_DCS_LONG_WRITE, 17, {0xED, 0xF5, 0x47, 0x6F, 0x0B, 0x8F, 0x9F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9, 0xF8, 0xB0, 0xF6, 0x74, 0x5F}},
	{MIPI_DSI_DCS_LONG_WRITE, 13, {0xEF, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
	/* End GIP Setting */

	{MIPI_DSI_DCS_LONG_WRITE, 6, {0xFF, 0x77, 0x01, 0x00, 0x00, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 1, {0x29, 0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}},
} ;

static int dsi_gpio_reset(int iod)
{
	int                 req_status;
	int                 set_direct_status;
	int                 gpio_index = iod ;

	req_status = gpio_request(gpio_index, NULL);
	if (req_status != 0) {
		DRM_ERROR("Gpio request fail!\n");
		return -EINVAL;
	}

	set_direct_status = gpio_direction_output(gpio_index,0);
	if (IS_ERR_VALUE(set_direct_status)) {
		DRM_ERROR("Set gpio direction output fail!\n");
		return -EINVAL;
	}

	/* to prevent electric leakage */
	gpio_set_value(iod,1);
	mdelay(10);
	gpio_set_value(iod,0);
	mdelay(10);
	gpio_set_value(iod,1);
	gpio_free(gpio_index);

	mdelay(120);

	return  0 ;
}

static int st7701s_enable(struct drm_panel *panel)
{
	struct ameba_panel_desc  *desc = panel_to_desc(panel);
	struct st7701s           *handle = desc->priv;
	struct device            *dev = desc->dev;
	int                      ret;

	ret = dsi_gpio_reset(handle->gpio);
	if (ret) {
		DRM_ERROR("Fail to set dis spio\n");
		return ret ;
	}

	return 0;
}

static int st7701s_disable(struct drm_panel *panel)
{
	(void)panel;
	return 0;
}

static int st7701s_get_modes(struct drm_panel *panel)
{
	struct drm_connector        *connector = panel->connector;
	struct drm_display_mode     *mode;

	mode = drm_mode_duplicate(panel->drm, &st7701s_mode);
	if (!mode) {
		DRM_ERROR("Bad mode or fail to add mode\n");
		return -EINVAL;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	drm_mode_probed_add(connector, mode);

	return 1; /* Number of modes */
}

static int st7701s_probe(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct device_node              *np = dev->of_node;
	struct st7701s                  *st7701s_data;
	enum of_gpio_flags              flags;

	st7701s_data = devm_kzalloc(dev, sizeof(struct st7701s), GFP_KERNEL);
	if (!st7701s_data)
		return -ENOMEM;

	priv_data->priv = st7701s_data ;

	//gpio
	st7701s_data->gpio = of_get_named_gpio_flags(np, "mipi-gpios", 0, &flags);
	if (!gpio_is_valid(st7701s_data->gpio)) {
		DRM_ERROR("Panel fail to get mipi-gpios\n");
		return -ENODEV;
	}

	return 0;
}

static int st7701s_remove(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct st7701s      *handle = priv_data->priv;
	AMEBA_DRM_DEBUG();

	//disable gpio 
	gpio_free(handle->gpio);
	//devm_kfree
	return 0;
}

static struct drm_panel_funcs st7701s_panel_funcs = {
	.disable   = st7701s_disable,
	.enable    = st7701s_enable,
	.get_modes = st7701s_get_modes,
};

struct ameba_panel_desc panel_st7701s_desc = {
	.dev          = NULL,
	.priv         = NULL,
	.init_table   = st7701s_initialization,
	.panel_module = &st7701s_mode,
	.rtk_panel_funcs  = &st7701s_panel_funcs,

	.init   = st7701s_probe,
	.deinit = st7701s_remove,
};
EXPORT_SYMBOL(panel_st7701s_desc);


