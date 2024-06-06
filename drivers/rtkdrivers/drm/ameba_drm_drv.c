// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DRM support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/of_platform.h>
#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "ameba_drm_base.h"
#include "ameba_lcdc.h"
#include "ameba_drm_drv.h"

#define to_ameba_crtc(crtc)                 container_of(crtc, struct ameba_crtc, base)
#define to_ameba_plane(plane)               container_of(plane, struct ameba_plane, base)

#define LCDC_LAYER_IMG_FORMAT_NOT_SUPPORT   (LCDC_LAYER_IMG_FORMAT_ARGB8666+1)


// ameba-format translate table
struct ameba_format {
	u32 pixel_bpp;
	u32 pixel_format;
	u32 hw_format;
};

struct ameba_crtc {
	struct drm_crtc     base;

	void                *lcdc_hw_ctx;        // struct lcdc_hw_ctx_type handle
	bool                enable;
};

struct ameba_plane {
	struct drm_plane    base;

	void                *lcdc_hw_ctx;        // struct lcdc_hw_ctx_type handle
	u32                 ch;                  // layer index
};

struct ameba_drm_private {
	struct ameba_crtc   crtc;
	struct ameba_plane  planes[LCDC_LAYER_MAX_NUM];

	void                *lcdc_hw_ctx;        // struct lcdc_hw_ctx_type handle
};

// display controller init/cleanup ops
struct ameba_drm_driver_data {
	const u32        *channel_formats;
	u32              channel_formats_cnt;
	u32              num_planes;
	u32              prim_plane;
	int              config_max_width;
	int              config_max_height;

	struct drm_driver                         *driver;
	const struct drm_crtc_helper_funcs        *crtc_helper_funcs;
	const struct drm_crtc_funcs               *crtc_funcs;
	const struct drm_plane_helper_funcs       *plane_helper_funcs;
	const struct drm_plane_funcs              *plane_funcs;
	const struct drm_mode_config_funcs        *mode_config_funcs;

	void *(*alloc_hw_ctx)(struct platform_device *pdev,struct drm_crtc *crtc);
	void (*cleanup_hw_ctx)(struct platform_device *pdev, void *hw_ctx);
};

#include "ameba_drm_obj.c"

static int ameba_drm_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
                               struct drm_plane *plane,
                               const struct ameba_drm_driver_data *driver_data)
{
	struct device_node      *port;
	int                     ret;

	/* set crtc port so that
	 * drm_of_find_possible_crtcs call works
	 */
	port = of_get_child_by_name(dev->dev->of_node, "port");
	if (!port) {
		DRM_ERROR("No port node found in %pOF\n", dev->dev->of_node);
		return -EINVAL;
	}
	of_node_put(port);
	crtc->port = port;

	ret = drm_crtc_init_with_planes(dev, crtc, plane, NULL,driver_data->crtc_funcs, NULL);
	if (ret) {
		DRM_ERROR("Fail to init crtc.\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, driver_data->crtc_helper_funcs);

	return 0;
}

static int ameba_drm_plane_init(struct drm_device *dev, struct drm_plane *plane,
                                enum drm_plane_type type,
                                const struct ameba_drm_driver_data *data)
{
	int        ret;

	ret = drm_universal_plane_init(dev, plane, 0xff, data->plane_funcs,
								data->channel_formats,
								data->channel_formats_cnt,
								NULL, type, NULL);
	if (ret) {
		DRM_ERROR("Fail to init plane\n");
		return ret;
	}

	drm_plane_helper_add(plane, data->plane_helper_funcs);

	return 0;
}

static void ameba_drm_private_cleanup(struct drm_device *drm)
{
	struct ameba_drm_driver_data        *data;
	struct ameba_drm_struct             *ameba_struct = drm->dev_private;
	struct ameba_drm_private            *ameba_priv = ameba_struct->ameba_drm_priv;
	struct platform_device              *pdev = to_platform_device(drm->dev);

	data = (struct ameba_drm_driver_data *)of_device_get_match_data(drm->dev);
	if (data->cleanup_hw_ctx) {
		data->cleanup_hw_ctx(pdev, ameba_priv->lcdc_hw_ctx);
	}

	devm_kfree(drm->dev, ameba_priv);

	ameba_struct->ameba_drm_priv = NULL;
}

static int ameba_drm_private_init(struct drm_device *drm,
                                  const struct ameba_drm_driver_data *driver_data)
{
	u32                         ch;
	int                         ret;
	void                        *ctx;
	enum drm_plane_type         type;
	struct drm_plane            *prim_plane;
	struct ameba_drm_private    *ameba_priv;
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct platform_device      *pdev = to_platform_device(drm->dev);

	ameba_priv = devm_kzalloc(drm->dev, sizeof(*ameba_priv), GFP_KERNEL);
	if (!ameba_priv) {
		DRM_ERROR("Fail to alloc ameba_drm_private\n");
		return -ENOMEM;
	}

	ctx = driver_data->alloc_hw_ctx(pdev, &ameba_priv->crtc.base);
	if (IS_ERR(ctx)) {
		DRM_ERROR("Fail to initialize ameba_priv hw ctx\n");
		return -EINVAL;
	}
	ameba_priv->lcdc_hw_ctx = ctx;
	ameba_struct->lcdc_hw_ctx = ctx;

	/*
	 * plane init, support all plane info
	 */
	for (ch = 0; ch < driver_data->num_planes; ch++) {
		if (ch == driver_data->prim_plane) {
			type = DRM_PLANE_TYPE_PRIMARY;
		} else {
			type = DRM_PLANE_TYPE_OVERLAY;
		}
		ret = ameba_drm_plane_init(drm, &ameba_priv->planes[ch].base, type, driver_data);
		if (ret) {
			return ret;
		}
		ameba_priv->planes[ch].ch = ch;
		ameba_priv->planes[ch].lcdc_hw_ctx = ctx;
	}

	/* crtc init */
	prim_plane = &ameba_priv->planes[driver_data->prim_plane].base;
	ret = ameba_drm_crtc_init(drm, &ameba_priv->crtc.base, prim_plane, driver_data);
	if (ret) {
		return ret;
	}
	ameba_priv->crtc.lcdc_hw_ctx = ctx;
	ameba_priv->crtc.enable = false ;

	ameba_struct->ameba_drm_priv = ameba_priv;

	return 0;
}

static int ameba_drm_kms_init(struct drm_device *dev,
                              const struct ameba_drm_driver_data *driver_data)
{
	int        ret;

	/* dev->mode_config initialization */
	drm_mode_config_init(dev);
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.funcs = driver_data->mode_config_funcs;

	/* display controller init */
	ret = ameba_drm_private_init(dev, driver_data);
	if (ret) {
		goto err_mode_config_cleanup;
	}

	dev->mode_config.max_width = driver_data->config_max_width;
	dev->mode_config.max_height = driver_data->config_max_width;

	/* bind and init sub drivers */
	ret = component_bind_all(dev->dev, dev);
	if (ret) {
		DRM_ERROR("Fail to bind all component.\n");
		goto err_private_cleanup;
	}

	/* vblank init */
	ret = drm_vblank_init(dev, dev->mode_config.num_crtc);
	if (ret) {
		DRM_ERROR("Fail to initialize vblank.\n");
		goto err_unbind_all;
	}
	/* with irq_enabled = true, we can use the vblank feature. */
	dev->irq_enabled = true;

	/* reset all the states of crtc/plane/encoder/connector */
	drm_mode_config_reset(dev);

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(dev);

	return 0;

err_unbind_all:
	component_unbind_all(dev->dev, dev);
err_private_cleanup:
	ameba_drm_private_cleanup(dev);
err_mode_config_cleanup:
	drm_mode_config_cleanup(dev);

	return ret;
}

static int ameba_drm_compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int ameba_drm_bind(struct device *dev)
{
	struct ameba_drm_struct         *ameba_struct;
	struct ameba_drm_driver_data    *driver_data;
	struct drm_device               *drm;
	int                             ret;

	AMEBA_DRM_DEBUG();

	driver_data = (struct ameba_drm_driver_data *)of_device_get_match_data(dev);
	if (!driver_data) {
		return -EINVAL;
	}

	//will call drm_dev_init
	drm = drm_dev_alloc(driver_data->driver, dev);
	if (IS_ERR(drm)) {
		return PTR_ERR(drm);
	}

	ameba_struct = devm_kzalloc(dev, sizeof(*ameba_struct), GFP_KERNEL);
	if (!ameba_struct)
		return -ENOMEM;

	ameba_struct->dev = dev;
	ameba_struct->drm = drm;

	drm->dev_private = ameba_struct;
	dev_set_drvdata(dev, drm);

	/* display controller init */
	ret = ameba_drm_kms_init(drm, driver_data);
	if (ret) {
		goto err_drm_dev_put;
	}

	//register the device
	ret = drm_dev_register(drm, 0);
	if (ret) {
		goto err_kms_cleanup;
	}

	//drm_fbdev_generic_setup(drm, 24);

	DRM_INFO("DRM Bind Success!\n");

	return 0;

err_kms_cleanup:
	component_unbind_all(drm->dev, drm);
	ameba_drm_private_cleanup(drm);
	drm_mode_config_cleanup(drm);
err_drm_dev_put:
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm);

	return ret;
}

static void ameba_drm_destroy_crtc(struct drm_device *drm_dev)
{
	struct ameba_drm_struct             *ameba_struct = drm_dev->dev_private;
	struct ameba_drm_private            *ameba_priv = ameba_struct->ameba_drm_priv;

	struct drm_crtc                     *crtc = &(ameba_priv->crtc.base);
	struct drm_plane                    *plane, *tmp;

	drm_crtc_vblank_off(crtc);

	//drm_self_refresh_helper_cleanup(crtc);
	of_node_put(crtc->port);

	/*
	 * We need to cleanup the planes now.  Why?
	 *
	 * The planes are "&vop->win[i].base".  That means the memory is
	 * all part of the big "struct vop" chunk of memory.  That memory
	 * was devm allocated and associated with this component.  We need to
	 * free it ourselves before vop_unbind() finishes.
	 */
	list_for_each_entry_safe(plane, tmp, &drm_dev->mode_config.plane_list, head) 
	{
		drm_plane_cleanup(plane);
	}

	/*
	 * Destroy CRTC after vop_plane_destroy() since vop_disable_plane()
	 * references the CRTC.
	 */
	drm_crtc_cleanup(crtc);
}

static void ameba_drm_unbind(struct device *dev)
{
	struct drm_device           *drm = dev_get_drvdata(dev);

	DRM_INFO("Run DRM Unbind\n");

	drm_dev_unregister(drm);
	ameba_drm_destroy_crtc(drm);

	//if(ameba_struct->state) {
		//drm_atomic_helper_commit_modeset_disables(drm,ameba_struct->state);
	//	ameba_struct->state = NULL ;
	//}

	drm_kms_helper_poll_fini(drm);
	drm_atomic_helper_shutdown(drm);
	ameba_drm_private_cleanup(drm);
	drm_mode_config_cleanup(drm);

	component_unbind_all(drm->dev, drm);

	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm);
}

static const struct component_master_ops ameba_drm_ops = {
	.bind = ameba_drm_bind,
	.unbind = ameba_drm_unbind,
};

static int ameba_drm_probe(struct platform_device *pdev)
{
	struct device_node      *remote;
	struct component_match  *match = NULL;
	struct device           *dev = &pdev->dev;
	struct device_node      *np = dev->of_node;
	int i;

	drm_debug = DRM_UT_DRIVER;
	DRM_DEBUG_DRIVER("Run Drm Probe!\n");

	for (i = 0; i<LCDC_MAX_REMOTE_DEV; i++) {
		remote = of_graph_get_remote_node(np,0,i);

		if (!remote){
			break;
		}

		if (!of_device_is_available(remote)) {
			continue;
		}

		component_match_add(dev, &match, ameba_drm_compare_of, remote);
		of_node_put(remote);
	}

	if (i == 0) {
		DRM_ERROR("Missing 'ports' property\n");
		return -ENODEV;
	}

	if (!match) {
		DRM_ERROR("No available vop found for component-subsystem.\n");
		return -ENODEV;
	}

	return component_master_add_with_match(dev, &ameba_drm_ops, match);
}

static int ameba_drm_remove(struct platform_device *pdev)
{
	DRM_DEBUG_DRIVER("Run Drm Remove!\n");

	component_master_del(&pdev->dev, &ameba_drm_ops);

	drm_debug = 0;

	return 0;
}

#ifdef CONFIG_PM
/*
	MIPI CG/PG Issue
	CG: switch mipi mode to support CG suspend/resume
*/
static int rtk_drm_pm_suspend(struct device *dev)
{
	struct drm_device           *drm = dev_get_drvdata(dev);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct ameba_drm_private    *ameba_priv = (struct ameba_drm_private*)(ameba_struct->ameba_drm_priv);
	struct lcdc_hw_ctx_type     *ctx = (struct lcdc_hw_ctx_type*)(ameba_priv->lcdc_hw_ctx);

	AMEBA_DRM_DEBUG();

	ctx->pm_status = 1;
	return drm_mode_config_helper_suspend(drm);
}

static int rtk_drm_pm_resume(struct device *dev)
{
	struct drm_device           *drm = dev_get_drvdata(dev);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct ameba_drm_private    *ameba_priv = (struct ameba_drm_private*)(ameba_struct->ameba_drm_priv);
	struct lcdc_hw_ctx_type     *ctx = (struct lcdc_hw_ctx_type*)(ameba_priv->lcdc_hw_ctx);

	AMEBA_DRM_DEBUG();

	return drm_mode_config_helper_resume(drm);
}

static void rtk_drm_pm_resume_complete(struct device *dev)
{
	struct drm_device           *drm = dev_get_drvdata(dev);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct ameba_drm_private    *ameba_priv = (struct ameba_drm_private*)(ameba_struct->ameba_drm_priv);
	struct lcdc_hw_ctx_type     *lcdc_ctx = (struct lcdc_hw_ctx_type*)(ameba_priv->lcdc_hw_ctx);
	struct ameba_drm_reg_t      *dsi;

	AMEBA_DRM_DEBUG();

	/* For sleep type CG */
	if (pm_suspend_target_state == PM_SUSPEND_CG) {
		lcdc_ctx->pm_status = 0;

		dsi = (struct ameba_drm_reg_t *)dev_get_drvdata(ameba_struct->dsi_dev);
		ameba_lcdc_dsi_underflow_reset(dsi->reg);
	}
}


static const struct dev_pm_ops realtek_ameba_drm_pm_ops = {
	.suspend  = rtk_drm_pm_suspend,          // suspend
	.resume   = rtk_drm_pm_resume,           // resume
	.complete = rtk_drm_pm_resume_complete,  // resume last call
};
#endif

static const struct of_device_id ameba_drm_dt_ids[] = {
	{
		.compatible = "realtek,ameba-drm",
		.data = (void *) &ameba_lcdc_driver_data,
	},
	{ /* end node */ },
};
MODULE_DEVICE_TABLE(of, ameba_drm_dt_ids);

static struct platform_driver ameba_drm_platform_driver = {
	.probe  = ameba_drm_probe,
	.remove = ameba_drm_remove,
	.driver = {
		.name = "realtek-ameba-drm",
		.of_match_table = ameba_drm_dt_ids,
#ifdef CONFIG_PM
		.pm = &realtek_ameba_drm_pm_ops,
#endif
	},
};

module_platform_driver(ameba_drm_platform_driver);

MODULE_DESCRIPTION("Realtek Ameba DRM driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
