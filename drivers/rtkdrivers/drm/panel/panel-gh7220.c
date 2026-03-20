// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Panel support
*
* MIPI-DSI gh7220 panel driver. This is a 1024 * 600
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
 * Driver IC: gh7220
 * Screen: hj7001-02

 RGB Output Timing Diagram
 Horizontal Timing:
  |<-----Hsync----->|<------HBP------>|<-------------HAdr-------------->|<------HFP------>|
  |        10       |       60        |              1024               |       120       |

 Vertical Timing:
  +------Vsync------+
  |       10        |
  +------VBP--------+
  |       10        |
  +------VAdr-------+
  |      600        |
  +------VFP--------+
  |       12        |

 Example:
	&rtkpanel {
		compatible = "realtek,gh7220";
		pinctrl-names="default";
		pinctrl-0 = <&drm_disable_swd_pins>;
		mipi-gpios = <&gpioa 14 0>;
		status = "okay";

		display-timings {
			native-mode = <&timing0>;
			timing0: timing0 {
				// 1024x600 @ 60Hz 2-lanes RGB888-24bits (typical)
				clock-frequency = <276209280>; // (frame-rate * htotal * vtotal * rgb-bpp) / (2 * lane-num)
				hactive = <1024>;
				hfront-porch = <120>;
				hback-porch = <60>;
				hsync-len = <10>;
				vactive = <600>;
				vfront-porch = <12>;
				vback-porch = <10>;
				vsync-len = <10>;
			};
		};
	};
*/

struct gh7220 {
	int gpio;
};

static LCM_setting_table_t gh7220_initialization[] = {/* DCS Write Long */
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x01}}, // 无
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xea, 0x07}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xeb, 0x12}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0a, 0x55}},
	// {MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x05, 0x10}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0c, 0x70}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x13, 0x14}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x15, 0x58}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x17, 0x32}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x1d, 0x33}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x21, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x28, 0x23}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x29, 0x23}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2a, 0x03}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2f, 0xf3}},
	//
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x02}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x39, 0xb0}},
	//
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x00, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x01, 0x11}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x02, 0x18}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x03, 0x0D}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x04, 0x15}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x05, 0x35}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x06, 0x0e}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x07, 0x10}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x08, 0x11}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x09, 0x0e}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0a, 0x12}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0b, 0x55}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0c, 0x12}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0d, 0x15}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0e, 0x3a}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0f, 0x3d}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x10, 0x3f}},
	//
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x20, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x21, 0x11}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x22, 0x18}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x23, 0x0d}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x24, 0x15}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x25, 0x35}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x26, 0x0e}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x27, 0x10}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x28, 0x11}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x29, 0x0e}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2a, 0x12}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2b, 0x55}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2c, 0x12}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2d, 0x15}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2e, 0x3a}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2f, 0x3d}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x3f}},
	//
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x03}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0f, 0xb9}},
	// {MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x10, 0x00}},
	// PAGE4
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x04}},  // ENTER PAGE4
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x00, 0x05}},  // 05=512 SOURCE chane
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x01, 0x01}},  // GAT
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x02, 0x2C}},  // GAT
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x03, 0x04}},  // SOURCE H
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x04, 0x00}},  // SOURCE H
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x06, 0x06}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x07, 0x05}},  // sstp
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x08, 0x15}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x09, 0x20}},  // pol
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0a, 0x0a}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0b, 0x07}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0f, 0x0a}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x19, 0xcc}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x1a, 0xcc}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x20, 0x40}},
	// {MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x23, 0x38}},  // 写R23H=38，MIPI接口下代码不用下11/29可显示
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x24, 0x08}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x25, 0x02}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x29, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x1d}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x31, 0x1d}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x37, 0x22}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x40, 0x80}},  // 80 bist=00
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x41, 0x55}},
	// PAGE5
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x05}},  // ENTER PAGE5
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x00, 0x01}},  // Stva
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x01, 0x05}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x02, 0x45}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x03, 0x05}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x07, 0xBD}},  // Stvb
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x08, 0xC1}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x09, 0x44}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x10, 0x03}},  // CLKA
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x11, 0x07}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x12, 0x45}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x13, 0x05}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x19, 0xBB}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x1a, 0x74}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x31, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x32, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x33, 0x14}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x34, 0x14}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x35, 0x78}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x36, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x37, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x38, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x39, 0x14}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x3A, 0x14}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x40, 0xEE}},  // 66
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x41, 0x44}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x43, 0x13}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x44, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x45, 0x81}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x46, 0x06}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x47, 0x00}},
	// PAGE6
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x06}},  // PAGE6 GIP back
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x00, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x02, 0x45}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x06, 0xcd}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x08, 0x67}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x09, 0x45}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0a, 0x23}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0b, 0x01}},
	// PAGE7
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x07}},  // PAGE7 GIP LEFT 1-24
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x00, 0x01}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x01, 0x05}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x02, 0x0C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x03, 0x0D}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x04, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x05, 0x21}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x06, 0x20}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x07, 0x12}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x08, 0x10}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x09, 0x16}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0A, 0x14}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0b, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0c, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0d, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0e, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x0f, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x10, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x11, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x12, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x13, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x14, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x15, 0x3c}},
	// GIP RIGHT 1-24
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x20, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x21, 0x04}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x22, 0x0C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x23, 0x0D}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x24, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x25, 0x21}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x26, 0x20}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x27, 0x13}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x28, 0x11}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x29, 0x17}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2A, 0x15}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2b, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2c, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2d, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2e, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x2f, 0x3C}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x30, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x31, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x32, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x33, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x34, 0x3c}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x35, 0x3c}},
	// PAGE8
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x08}},  // PAGE8
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x10, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x12, 0xda}},  // VDDH
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x13, 0x1c}},
	// {MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x14, 0x10}},
	// below setting changes mipi's electric current, 0x20 and 0x10 are both ok.
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x18, 0x10}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x20, 0x80}},
	// PAGEf
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x0f}},  // PAGEf
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x00, 0x01}},  // dualgate en
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x03, 0x95}},
	// PAGE0
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xee, 0x00}},  // ENTER PAGE0
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xea, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0xeb, 0x00}},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x36, 0x00}},
	//
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

static int gh7220_enable(struct drm_panel *panel)
{
	struct ameba_panel_desc  *desc = panel_to_desc(panel);
	struct gh7220            *handle = desc->priv;
	int                      ret;

	ret = dsi_gpio_reset(handle->gpio);
	if (ret) {
		DRM_ERROR("Fail to set dis spio\n");
		return ret;
	}
	return 0;
}

static int gh7220_disable(struct drm_panel *panel)
{
	(void)panel;
	return 0;
}

static int gh7220_get_modes(struct drm_panel *panel, struct drm_connector *connector)
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

static int gh7220_probe(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct device_node              *np = dev->of_node;
	struct gh7220                   *gh7220_data;

	gh7220_data = devm_kzalloc(dev, sizeof(struct gh7220), GFP_KERNEL);
	if (!gh7220_data)
		return -ENOMEM;

	priv_data->priv = gh7220_data ;

	gh7220_data->gpio = of_get_named_gpio(np, "mipi-gpios", 0);
	if (!gpio_is_valid(gh7220_data->gpio)) {
		DRM_ERROR("Panel fail to get mipi-gpios\n");
		return -ENODEV;
	}

	return 0;
}

static int gh7220_remove(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct gh7220      *handle = priv_data->priv;
	AMEBA_DRM_DEBUG();

	gpio_free(handle->gpio);
	return 0;
}

static struct drm_panel_funcs gh7220_panel_funcs = {
	.disable   = gh7220_disable,
	.enable    = gh7220_enable,
	.get_modes = gh7220_get_modes,
};

struct ameba_panel_desc panel_gh7220_desc = {
	.dev             = NULL,
	.priv            = NULL,
	.init_table      = gh7220_initialization,
	.rtk_panel_funcs = &gh7220_panel_funcs,

	.init   = gh7220_probe,
	.deinit = gh7220_remove,
};
EXPORT_SYMBOL(panel_gh7220_desc);
