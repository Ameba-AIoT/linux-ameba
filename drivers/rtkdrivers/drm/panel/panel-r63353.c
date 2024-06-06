// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Panel support
*
* MIPI-DSI Synaptics R63353 panel driver. This is a 1280*720
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

struct r63353 {
	int                     gpio_reset; //pa 14
	int                     gpio_v01;   //v01 pb 25
	int                     gpio_v02;   //v02 pb 26
	int                     gpio_pwm;   //v03 pa 13
};

/*
 * The timings are not very helpful as the display is used in
 * command mode.
 */
static struct drm_display_mode r63353_mode = {
	/* HS clock, (htotal*vtotal*vrefresh)/1000 */
	.clock = 70000,
	.hdisplay = 720,
	.hsync_start = 720 + 35,
	.hsync_end = 720 + 35 + 2,
	.htotal = 720 + 35 + 2 + 150,
	.vdisplay = 1280,
	.vsync_start = 1280 + 2,
	.vsync_end = 1280 + 2 + 4,
	.vtotal = 1280 + 2 + 4 + 0,
	.vrefresh = 30,
};

static LCM_setting_table_t r63353_initialization[] = {/* DCS Write Long */

	//# Manufacture Command Access Protect write 0xb0 0x00
	{ MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 2, {0xb0, 0x00}},
	//# Interface ID Setting write 0xb4 0x04 0x00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xb4, 0x04, 0x00}},
	//{MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}},					//03 03 00
	//{MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM, 2, {0xb4, 0x02}},								//04 04 bf 04

	//# Dummy 7-times write 0x00
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}}, 					//03 03 00
	{ MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM, 2, {0xbf, 0x04}}, 								//04 04 bf 04
//	{ 0x05, 4, {0x02, 0x3c, 0x33, 0x53}}, 					//06 05 02 3c 33 53	/* compare 0xbf vs data from datasheet */
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}}, 					//03 03 00
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}}, 					//03 03 00
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}}, 					//03 03 00
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}}, 					//03 03 00
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}}, 					//03 03 00
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x00}}, 					//03 03 00

	//# Interface Setting
	{ MIPI_DSI_GENERIC_LONG_WRITE, 6, {0xb3, 0x14, 0x00, 0x00, 0x00, 0x00}},	//08 03 b3 14 00 00 00 00
	//# Interface ID Setting
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xb4, 0x04, 0x00}}, 			//05 03 b4 04 00
	//# DSI Control
	{ MIPI_DSI_GENERIC_LONG_WRITE, 4, {0xb6, 0x4a, 0xcb, 0x16}}, 	//06 03 b6 4a cb 16
	//# Back Light Control 1,2,3
	{ MIPI_DSI_GENERIC_LONG_WRITE, 8, {0xb8, 0x57, 0x3d, 0x19, 0x1e, 0x0a, 0x50, 0x50}}, 	//0a 03 b8 57 3d 19 1e 0a 50 50
	{ MIPI_DSI_GENERIC_LONG_WRITE, 8, {0xb9, 0x6f, 0x3d, 0x28, 0x3c, 0x14, 0xc8, 0xc8}}, 	//0a 03 b9 6f 3d 28 3c 14 c8 c8
	{ MIPI_DSI_GENERIC_LONG_WRITE, 8, {0xba, 0xb5, 0x33, 0x41, 0x64, 0x23, 0xa0, 0xa0}}, 	//0a 03 ba b5 33 41 64 23 a0 a0

	//# SRE Control 1,2,3
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xb4, 0x14, 0x14}},			//05 03 bb 14 14
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xbc, 0x37, 0x32}}, 			//05 03 bc 37 32
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xbd, 0x64, 0x32}}, 			//05 03 bd 64 32
	//# Test Mode
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xbe, 0x00, 0x04}}, 			//05 03 be 00 04
	//# Skew Rate Adjustment
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xc0, 0x00, 0x00}}, 			//05 03 c0 00 00

	//# Display Setting 1,2
	//2a 03 c1 0c a2 00 40 0c 52 1c 8c 2c 4e 5a ed c5 93 56 7b 31 be b6 d2 89 af ad 74 46 16 a1 2b 64 88 00 00 00 00 40 80 02 80 11
	{ MIPI_DSI_GENERIC_LONG_WRITE, 40,{0xc1, 0x0c, 0xa2, 0x00, 0x40, 0x0c, 0x52, 0x1c, 0x8c, 0x2c, 0x4e, 0x5a, 0xed, 0xc5, 0x93, 0x56, 0x7b, 0x31, 0xbe, 0xb6, 0xd2, 0x89, 0xaf, 0xad, 0x74, 0x46, 0x16, 0xa1, 0x2b, 0x64, 0x88, 0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x02, 0x80, 0x11}}, 			
	//2f 03 C2 f0 f5 00 04 02 00 00 08 e5 14 94 53 50 4e 41 39 05 e5 14 94 53 50 4e 41 39 05 e5 14 94 53 50 4e 41 39 05 e5 14 94 53 50 4e 41 39 05
	{ MIPI_DSI_GENERIC_LONG_WRITE, 45, {0xC2, 0xf0, 0xf5, 0x00, 0x04, 0x02, 0x00, 0x00, 0x08, 0xe5, 0x14, 0x94, 0x53, 0x50, 0x4e, 0x41, 0x39, 0x05, 0xe5, 0x14, 0x94, 0x53, 0x50, 0x4e, 0x41, 0x39, 0x05, 0xe5, 0x14, 0x94, 0x53, 0x50, 0x4e, 0x41, 0x39, 0x05, 0xe5, 0x14, 0x94, 0x53, 0x50, 0x4e, 0x41, 0x39, 0x05}}, 			

	//#  TPC Sync Control 06 03 c3 00 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 4, {0xc3, 0x00, 0x00, 0x00}}, 
	//#  Source Timing Setting 12 03 C4 30 00 00 00 00 00 00 00 00 00 00 00 00 01 02
	{ MIPI_DSI_GENERIC_LONG_WRITE, 16, {0xC4, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02}},	
	//# Realtime Scaling & Cross Hair 0b 03 c5 00 08 00 00 00 00 70 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 9, {0xc5, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00}},
	//# LTPS Timing Setting 1b 03 C6 01 90 0c 9b 0c 9b 00 00 00 00 00 00 00 00 00 00 00 00 0d 12 04 01 90 05
	{ MIPI_DSI_GENERIC_LONG_WRITE, 25, {0xC6, 0x01, 0x90, 0x0c, 0x9b, 0x0c, 0x9b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x12, 0x04, 0x01, 0x90, 0x05}},
	//# Gamma Setting Common Set 29 03 c7 00 14 1f 2c 39 43 5b 6d 7c 89 3d 4a 5b 73 7e 8d 9e a8 b2 00 14 1f 2c 39 43 5b 6d 7c 89 3d 4a 5b 73 7e 8d 9e a8 b2
	{ MIPI_DSI_GENERIC_LONG_WRITE, 39, {0xc7, 0x00, 0x14, 0x1f, 0x2c, 0x39, 0x43, 0x5b, 0x6d, 0x7c, 0x89, 0x3d, 0x4a, 0x5b, 0x73, 0x7e, 0x8d, 0x9e, 0xa8, 0xb2, 0x00, 0x14, 0x1f, 0x2c, 0x39, 0x43, 0x5b, 0x6d, 0x7c, 0x89, 0x3d, 0x4a, 0x5b, 0x73, 0x7e, 0x8d, 0x9e, 0xa8, 0xb2}},

	//# Digital Gamma Setting 1,2
	//3a 03 c8 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 56, {0xc8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00}},
	//16 03 c9 00 00 00 00 00 fc 00 00 00 00 00 fc 00 00 00 00 00 fc 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 20, {0xc9, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00}},

	//# Color Enhancementt 2e 03 ca 1c fc fc fc 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 44, {0xca, 0x1c, 0xfc, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	//# Panel PIN Control 18 03 CB 7f fd ff ff eb 0f 00 fc 00 f0 03 00 40 00 00 00 20 00 e8 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 22, {0xCB, 0x7f, 0xfd, 0xff, 0xff, 0xeb, 0x0f, 0x00, 0xfc, 0x00, 0xf0, 0x03, 0x00, 0x40, 0x00, 0x00, 0x00, 0x20, 0x00, 0xe8, 0x00, 0x00}},

	//# Panel Interface Control 04 03 cc 05
	{ MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 2, {0xcc, 0x05}}, 	
	//# Backlight Control 4 1c 03 ce 55 40 49 53 59 5e 63 68 6e 74 7e 8a 98 a8 bb d0 ff 04 00 04 04 42 00 69 5a
	{ MIPI_DSI_GENERIC_LONG_WRITE, 26, {0xce, 0x55, 0x40, 0x49, 0x53, 0x59, 0x5e, 0x63, 0x68, 0x6e, 0x74, 0x7e, 0x8a, 0x98, 0xa8, 0xbb, 0xd0, 0xff, 0x04, 0x00, 0x04, 0x04, 0x42, 0x00, 0x69, 0x5a}},

	//# Power Setting for Charge Pump 0d 03 d0 19 00 00 26 9c 40 19 19 09 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 11, {0xd0, 0x19, 0x00, 0x00, 0x26, 0x9c, 0x40, 0x19, 0x19, 0x09, 0x00}},
	//# External Charge Pump Setting 07 03 d1 04 40 06 0f
	{ MIPI_DSI_GENERIC_LONG_WRITE, 5, {0xd1, 0x04, 0x40, 0x06, 0x0f}},
	//# Test Register 07 03 d2 00 00 ff ff
	{ MIPI_DSI_GENERIC_LONG_WRITE, 5, {0xd2, 0x00, 0x00, 0xff, 0xff}},
	//# Power Setting for Internal Power 1d 03 d3 1b 33 bb 77 77 77 33 33 33 00 01 00 00 7f a0 0f 63 63 33 33 72 12 8a 07 3d bc
	{ MIPI_DSI_GENERIC_LONG_WRITE, 27, {0xd3, 0x1b, 0x33, 0xbb, 0x77, 0x77, 0x77, 0x33, 0x33, 0x33, 0x00, 0x01, 0x00, 0x00, 0x7f, 0xa0, 0x0f, 0x63, 0x63, 0x33, 0x33, 0x72, 0x12, 0x8a, 0x07, 0x3d, 0xbc}},
	//# Glass Break Detection Control 06 03 d4 41 04 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 4, {0xd4, 0x41, 0x04, 0x00}},
	//# VCOM Setting 0a 03 d5 06 00 00 01 08 01 08
	{ MIPI_DSI_GENERIC_LONG_WRITE, 8, {0xd5, 0x06, 0x00, 0x00, 0x01, 0x08, 0x01, 0x08}},
	//# Sequencer Test Control 04 03 d6 81
	{ MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 2, {0xd6, 0x81}},
	//# Sequencer Timing Control 1c 03 d7 bf f8 7f a8 ce 3e fc c1 f1 ef 83 07 3f 10 7f c0 01 e7 40 1c 00 00 00 01 0f
	{ MIPI_DSI_GENERIC_LONG_WRITE, 26, {0xd7, 0xbf, 0xf8, 0x7f, 0xa8, 0xce, 0x3e, 0xfc, 0xc1, 0xf1, 0xef, 0x83, 0x07, 0x3f, 0x10, 0x7f, 0xc0, 0x01, 0xe7, 0x40, 0x1c, 0x00, 0x00, 0x00, 0x01, 0x0f}},
	//# Test Register 06 03 d8 00 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 4, {0xd8, 0x00, 0x00, 0x00}},
	//# Sequencer Control 06 03 d9 20 08 55
	{ MIPI_DSI_GENERIC_LONG_WRITE, 4, {0xd9, 0x20, 0x08, 0x55}},
	//# Outline Sharpening Control 07 03 dd 30 06 23 65
	{ MIPI_DSI_GENERIC_LONG_WRITE, 5, {0xdd, 0x30, 0x06, 0x23, 0x65}},
	//# Test Image Generator 07 03 de 00 3f ff 10
	{ MIPI_DSI_GENERIC_LONG_WRITE, 5, {0xde, 0x00, 0x3f, 0xff, 0x10}},
	//# Set_DDB Write Control 0d 03 e1 00 00 00 00 00 00 00 00 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 11, {0xe1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	//# Read ID Code 09 03 e2 00 00 00 00 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 7, {0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	//# NVM Load Control 05 03 e3 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xe3, 0x00, 0x00}},
	//# Test Register 05 03 e7 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xe7, 0x00, 0x00}},
	//# Read Chip Information Enable 04 03 e9 00
	{ MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 2, {0xe9, 0x00}},
	//# ESD Detect Mode 04 03 ea 00
	{ MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 2, {0xea, 0x00}},
	//# Panel Information 09 03 eb 00 00 00 00 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 7, {0xeb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},

	//# Panel Synchronous Output
	// 05 03 ec 40 10
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xec, 0x40, 0x10}},
	//06 03 ed 00 00 00
	{ MIPI_DSI_GENERIC_LONG_WRITE, 4, {0xed, 0x00, 0x00, 0x00}},
	//05 03 ee 00 32
	{ MIPI_DSI_GENERIC_LONG_WRITE, 3, {0xee, 0x00, 0x32}},

	//# NVM Load Setting 04 03 d6 01
	{ MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 2, {0xd6, 0x01}},
	//# Manufacture Command Access Protect 04 03 b0 03
	{ MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 2, {0xb0, 0x03}},

	//# Write_Display_Brightness
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x51, 0xff}}, //04 03 51 ff		/* Only the first 10 displays ever shipped to BSH were not pre-programmed */
	//# Write_CTRL_Display
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x53, 0x0c}}, //04 03 53 0c		/* certain settings are optimized / calibrated */
	//# Write_CABC
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x55, 0x00}}, //04 03 55 00
	//# Set_Color_Temperature
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {0x84, 0x00}}, //04 03 84 00

	//# Set_Display_On
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x29}}, //03 03 29
	//# Exit_Sleep_Mode
	{ MIPI_DSI_DCS_SHORT_WRITE, 1, {0x11}}, //03 03 11
	{ REGFLAG_DELAY, 120, {}},/* Delayms (120) */

	{ REGFLAG_END_OF_TABLE, 0x00, {}},
};

static int dsi_gpio_reset(int iod,int value,int flag)
{	
	int req_status;
	int set_direct_status;
	int gpio_index = iod ;	

	if(flag == 0) {
		req_status = gpio_request(gpio_index, NULL);
		if (req_status != 0) {
			DRM_ERROR("Gpio request failed!\n");
			return -EINVAL;
		}
		gpio_set_value(gpio_index,value);
	}

	set_direct_status = gpio_direction_output(gpio_index,value);
	if (IS_ERR_VALUE(set_direct_status)) {
		DRM_ERROR("Set gpio direction output failed\n");
		return -EINVAL;
	}
	if(flag){
		gpio_free(gpio_index);
	}

	return  0 ;
}

static int r63353_enable(struct drm_panel *panel)
{
	struct ameba_panel_desc *desc = panel_to_desc(panel);
	struct r63353           *handle = desc->priv;
	struct device           *dev = desc->dev;
	int                     ret;

	//dsi_gpio_reset(dsi->gpio_reset,1,0));
	dsi_gpio_reset(handle->gpio_v01,1,0);
	dsi_gpio_reset(handle->gpio_v02,1,0);
	dsi_gpio_reset(handle->gpio_pwm,0,0);
	mdelay(10);

	ret = dsi_gpio_reset(handle->gpio_reset,0,0);
	//gpio_set_value(dsi->gpio_v02,0);
	ret |= dsi_gpio_reset(handle->gpio_v02,0,1);
	mdelay(12);
	ret |= dsi_gpio_reset(handle->gpio_v01,0,1);
	mdelay(310);
	ret |= dsi_gpio_reset(handle->gpio_reset,1,1);
	mdelay(10);
	ret |= dsi_gpio_reset(handle->gpio_pwm,1,1);
	//mdelay(1000);
	if (ret) {
		DRM_ERROR("Failed to set dsi gpio\n");
		return ret ;
	}

	return 0;
}

static int r63353_disable(struct drm_panel *panel)
{
	(void)panel;
	return 0;
}

static int r63353_get_modes(struct drm_panel *panel)
{
	struct drm_connector        *connector = panel->connector;
	struct drm_display_mode     *mode;

	mode = drm_mode_duplicate(panel->drm, &r63353_mode);
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

static int r63353_probe(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct device_node              *np = dev->of_node;
	struct r63353                   *r63353_data;
	enum of_gpio_flags              flags;

	r63353_data = devm_kzalloc(dev, sizeof(struct r63353), GFP_KERNEL);
	if (!r63353_data)
		return -ENOMEM;

	priv_data->priv = r63353_data ;

	//gpio
	r63353_data->gpio_reset = of_get_named_gpio_flags(np, "mipi-reset", 0, &flags);
	if (!gpio_is_valid(r63353_data->gpio_reset)) {
		DRM_ERROR("Drm mipi dsi node fail to get mipi-reset\n");
		return -ENODEV;
	}

	r63353_data->gpio_v01 = of_get_named_gpio_flags(np, "mipi-v01", 0, &flags);
	if (!gpio_is_valid(r63353_data->gpio_v01)) {
		DRM_ERROR("Drm mipi dsi node fail to get mipi-v01\n");
		return -ENODEV;
	}
	r63353_data->gpio_v02 = of_get_named_gpio_flags(np, "mipi-v02", 0, &flags);
	if (!gpio_is_valid(r63353_data->gpio_v02)) {
		DRM_ERROR("Drm mipi dsi node fail to get mipi-v02\n");
		return -ENODEV;
	}

	r63353_data->gpio_pwm = of_get_named_gpio_flags(np, "mipi-pwm", 0, &flags);
	if (!gpio_is_valid(r63353_data->gpio_pwm)) {
		DRM_ERROR("Drm mipi dsi node fail to get mipi-pwm\n");
		return -ENODEV;
	}

	return 0;
}

static int r63353_remove(struct device *dev,struct ameba_panel_desc *priv_data)
{
	struct r63353      *handle = priv_data->priv;
	AMEBA_DRM_DEBUG();

	//disable gpio 
	gpio_free(handle->gpio_reset);
	gpio_free(handle->gpio_v01);
	gpio_free(handle->gpio_v02);
	gpio_free(handle->gpio_pwm);

	//devm_kfree
	return 0;
}

static struct drm_panel_funcs r63353_panel_funcs = {
	.disable   = r63353_disable,
	.enable    = r63353_enable,
	.get_modes = r63353_get_modes,
};

struct ameba_panel_desc panel_r63353_desc = {
	.dev          = NULL,
	.priv         = NULL,
	.init_table   = r63353_initialization,
	.panel_module = &r63353_mode,
	.rtk_panel_funcs  = &r63353_panel_funcs,

	.init   = r63353_probe,
	.deinit = r63353_remove,
};
EXPORT_SYMBOL(panel_r63353_desc);

