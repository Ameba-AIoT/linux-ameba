// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Panel support
*
* MIPI-DSI jd9165ba panel driver. This is a 1024 * 600
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

#include "ameba_panel_base.h"
#include "ameba_panel_priv.h"

/*
 * Driver IC: jd9165ba
 * Screen: hj7001-01

 RGB Output Timing Diagram
 Horizontal Timing:
  |<-----Hsync----->|<------HBP------>|<-------------HAdr-------------->|<------HFP------>|
  |        10       |       60        |              1024               |        60       |

 Vertical Timing:
  +------Vsync------+
  |        2        |
  +------VBP--------+
  |       21        |
  +------VAdr-------+
  |      600        |
  +------VFP--------+
  |       12        |

 Example:
	&rtkpanel {
		compatible = "realtek,jd9165ba";
		pinctrl-names="default";
		pinctrl-0 = <&drm_disable_swd_pins>;
		mipi-gpios = <&gpioa 14 0>;
		status = "okay";

		display-timings {
			native-mode = <&timing0>;
			timing0: timing0 {
				// 1024x600 @ 60Hz 2-lanes RGB888-24bits (typical)
				clock-frequency = <263804400>; // (frame-rate * htotal * vtotal * rgb-bpp) / (2 * lane-num)
				hactive = <1024>;
				hfront-porch = <60>;
				hback-porch = <60>;
				hsync-len = <10>;
				vactive = <600>;
				vfront-porch = <12>;
				vback-porch = <21>;
				vsync-len = <2>;
			};
		};
	};
*/

struct jd9165ba {
	int gpio;
};

static LCM_setting_table_t jd9165ba_initialization[] = {/* DCS Write Long */
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x00}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 5, {0xF7, 0x49, 0x61, 0x02, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x04, 0x0C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x05, 0x00}}, //05=06(xhs)
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x06, 0x00}}, //06=80(xhs)
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0B, 0x11}}, //0x13=4lanes，0x12=3lanes，0x11=2lanes，0x10=1 lanes
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x17, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x1F, 0x05}}, //add hs_settle time
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x23, 0x00}}, //add //close gas
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x25, 0x19}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x28, 0x18}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x29, 0xFC}}, //revcom
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2A, 0x00}}, //revcom
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2B, 0xFC}}, //vcom
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2C, 0x00}}, //vcom
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x02}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x01, 0x22}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x03, 0x12}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x04, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x05, 0x64}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0A, 0x08}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x0B, 0x0A, 0x1A, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x06, 0x08, 0x1F, 0x1D}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x0D, 0x16, 0x1B, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x07, 0x09, 0x1E, 0x1C}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x0E, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x0F, 0x16, 0x1B, 0x0D, 0x0B, 0x0D, 0x11, 0x10, 0x1C, 0x1E, 0x09, 0x07}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x10, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x11, 0x0A, 0x1A, 0x0D, 0x0B, 0x0D, 0x11, 0x10, 0x1D, 0x1F, 0x08, 0x06}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 12, {0x12, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 5, {0x14, 0x00, 0x00, 0x00, 0x00}},  //CKV_OFF
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x18, 0x99}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x06}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 15, {0x12, 0x36, 0x2B, 0x2C, 0x3A, 0x33, 0x30, 0x30, 0x2E, 0x2B, 0x1B, 0x2A, 0x20, 0x16, 0x29}},
	{MIPI_DSI_GENERIC_LONG_WRITE, 15, {0x13, 0x36, 0x2B, 0x2C, 0x3A, 0x33, 0x30, 0x30, 0x2E, 0x2B, 0x1B, 0x2A, 0x20, 0x16, 0x29}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x0A}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x02, 0x4F}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0B, 0x40}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x12, 0x3E}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x13, 0x78}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x0D}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0D, 0x04}}, //0x0C, 0x04
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x10, 0x0C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x11, 0x0C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x12, 0x0C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x13, 0x0C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, {0x11}},
	{REGFLAG_DELAY, 120, {}},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, {0x29}},
	{REGFLAG_DELAY, 20, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static int dsi_gpio_reset(int iod)
{
	int req_status;
	int set_direct_status;
	int gpio_index = iod ;

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
	mdelay(15);
	gpio_set_value(iod,1);
	gpio_free(gpio_index);
	mdelay(120);

	return 0;
}

static int jd9165ba_enable(struct drm_panel *panel)
{
	struct ameba_panel_desc  *desc = panel_to_desc(panel);
	struct jd9165ba          *handle = desc->priv;
	int                      ret;

	ret = dsi_gpio_reset(handle->gpio);
	if (ret) {
		DRM_ERROR("Fail to set dis spio\n");
		return ret;
	}
	return 0;
}

static int jd9165ba_disable(struct drm_panel *panel)
{
	(void)panel;
	return 0;
}

static int jd9165ba_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode	*mode = drm_mode_create(connector->dev);
	struct device_node		*np = panel->dev->of_node;
	int						ret;

	if (!mode)
		return 0;

	ret = of_get_drm_display_mode(np, mode, 0, 0); // no bus_flags, pannel timing index = 0.
	if (ret) {
		drm_mode_destroy(connector->dev, mode);
		return 0;
	}

	mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1; /* Number of modes */
}

static int jd9165ba_probe(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct device_node              *np = dev->of_node;
	struct jd9165ba                   *jd9165ba_data;

	jd9165ba_data = devm_kzalloc(dev, sizeof(struct jd9165ba), GFP_KERNEL);
	if (!jd9165ba_data)
		return -ENOMEM;

	priv_data->priv = jd9165ba_data ;

	jd9165ba_data->gpio = of_get_named_gpio(np, "mipi-gpios", 0);
	if (!gpio_is_valid(jd9165ba_data->gpio)) {
		DRM_ERROR("Panel fail to get mipi-gpios\n");
		return -ENODEV;
	}

	return 0;
}

static int jd9165ba_remove(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct jd9165ba      *handle = priv_data->priv;
	AMEBA_DRM_DEBUG();

	gpio_free(handle->gpio);
	return 0;
}

static struct drm_panel_funcs jd9165ba_panel_funcs = {
	.disable   = jd9165ba_disable,
	.enable    = jd9165ba_enable,
	.get_modes = jd9165ba_get_modes,
};

struct ameba_panel_desc panel_jd9165ba_desc = {
	.dev             = NULL,
	.priv            = NULL,
	.init_table      = jd9165ba_initialization,
	.rtk_panel_funcs = &jd9165ba_panel_funcs,

	.init   = jd9165ba_probe,
	.deinit = jd9165ba_remove,
};
EXPORT_SYMBOL(panel_jd9165ba_desc);
