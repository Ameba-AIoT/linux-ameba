# SPDX-License-Identifier: GPL-2.0-only


obj-y += panel/

#obj-$(CONFIG_DRM_RTK_AMEBA) += ameba_drm_drv.o ameba_drm_dsi.o ameba_lcdc.o ameba_mipi.o ameba_drm_util.o ameba_dsi_util.o

rtkdrm-y := ameba_drm_drv.o ameba_lcdc.o ameba_drm_util.o
rtkdsi-y := ameba_drm_dsi.o ameba_mipi.o ameba_dsi_util.o

obj-$(CONFIG_DRM_RTK_AMEBA) += rtkdrm.o
obj-$(CONFIG_DRM_DSI_RTK_AMEBA) += rtkdsi.o
