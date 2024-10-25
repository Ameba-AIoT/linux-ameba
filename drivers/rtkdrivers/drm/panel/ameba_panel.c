// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Panel support
*
* MIPI-DSI ameba_panel_desc panel driver. This is a 480*800
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <drm/drm_modes.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_drv.h>
// #include <video/mipi_display.h>
#include <linux/component.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/module.h>

#include "ameba_panel_base.h"
#include "ameba_panel_priv.h"

//panel description
extern struct ameba_panel_desc panel_st7701s_desc;
extern struct ameba_panel_desc panel_r63353_desc;
static const struct of_device_id ameba_panel_match[] = {
	{ 
		.compatible = "realtek,st7701s",
		.data = &panel_st7701s_desc,
	}, {
		.compatible = "realtek,r63353",
		.data = &panel_r63353_desc,
	}, {	
		/* NULL */
	}
};
MODULE_DEVICE_TABLE(of, ameba_panel_match);

//components
static int ameba_panel_bind(struct device *dev, struct device *master, void *data)
{
	DRM_INFO("Panel Bind Success!\n");
	return 0;
}

static void ameba_panel_unbind(struct device *dev, struct device *master, void *data)
{
	DRM_INFO("Run Panel Unbind\n");
}

static const struct component_ops ameba_panel_ops = {
	.bind	= ameba_panel_bind,
	.unbind	= ameba_panel_unbind,
};

static int ameba_panel_probe(struct platform_device *pdev)
{
	struct device              *dev = &pdev->dev;
	const struct of_device_id  *id;
	struct ameba_panel_desc    *priv_data;
	struct ameba_drm_panel_struct   *ameba_panel;
	int                             ret = 0;

	DRM_DEBUG_DRIVER("Run panel probe!\n");

	id = of_match_node(ameba_panel_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	priv_data = (struct ameba_panel_desc *)id->data;
	if (!priv_data)
		return -ENODEV;

	ameba_panel = devm_kzalloc(dev, sizeof(struct ameba_drm_panel_struct), GFP_KERNEL);
	if (!ameba_panel)
		return -ENOMEM;

	ameba_panel->panel_priv = priv_data;
	ameba_panel->init_table = priv_data->init_table;

	dev_set_drvdata(dev, ameba_panel);
	priv_data->dev = dev;

	drm_panel_init(&priv_data->panel);

	if (priv_data->init)
		priv_data->init(dev,priv_data);

	priv_data->panel.dev = dev;
	priv_data->panel.funcs = priv_data->rtk_panel_funcs;

	ret = drm_panel_add(&priv_data->panel);

	return component_add(dev, &ameba_panel_ops);
}

static int ameba_panel_remove(struct platform_device *pdev)
{
	struct device           *dev = &pdev->dev;
	struct ameba_panel_desc *priv_data;
	const struct of_device_id *id;

	DRM_DEBUG_DRIVER("Run panel remove!\n");

	id = of_match_node(ameba_panel_match, pdev->dev.of_node);
	if (!id)
		return -ENOMEM;

	priv_data = (struct ameba_panel_desc *)id->data;

	if (priv_data && priv_data->deinit)
		priv_data->deinit(dev,priv_data);

	drm_panel_remove(&priv_data->panel);

	component_del(dev, &ameba_panel_ops);

	return 0;
}

static struct platform_driver ameba_panel_driver = {
	.probe = ameba_panel_probe,
	.remove = ameba_panel_remove,
	.driver = {
		.name = "realtek-ameba-drm-panel",
		.of_match_table = ameba_panel_match,
	},
};

//module_platform_driver(ameba_drm_panel_platform_driver);
static int __init rtk_panel_init(void)
{
	int err;
	AMEBA_DRM_DEBUG();
	err = platform_driver_register(&ameba_panel_driver);
	if (err < 0)
		return err;

	return 0;
}
postcore_initcall(rtk_panel_init);//init 2

static void __exit rtk_panel_exit(void)
{
	AMEBA_DRM_DEBUG();
	platform_driver_unregister(&ameba_panel_driver);
}
module_exit(rtk_panel_exit);

MODULE_DESCRIPTION("Realtek Ameba Panel driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
