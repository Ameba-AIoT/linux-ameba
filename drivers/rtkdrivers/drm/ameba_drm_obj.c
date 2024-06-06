// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DRM support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <video/display_timing.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include "ameba_lcdc.h"
#include "ameba_drm_drv.h"
#include "ameba_drm_comm.h"


#define CHECK_IS_NULL(a)        ((NULL==a)?(1):(0))

// just update some area in the plane
struct ameba_buf_struct {
	u32        layer_address;    // saving the buffer handle, used to free the layer buffer

	// used to copy window from fb to show
	bool       use_sec_buffer ;
	size_t     size;
	dma_addr_t sec_paddr;        // phy address
	void       *sec_vaddr;       // virtual address
};
static struct ameba_buf_struct sec_layer_info[LCDC_LAYER_MAX_NUM]={0,};

/**
  * @brief  lcdc hw control struction
  */
struct lcdc_hw_ctx_type {
	void __iomem         *lcdc_reg;                 // lcdc register
	LCDC_InitTypeDef     lcdc_initdef;              // lcdc init struct define
	int                  irq;                       // lcdc irq
	struct clk           *lcdc_clk;                 // lcdc clock 

	u32                  ldcd_writeback_buffer_length;
	u32                  ldcd_writeback_buffer;
	dma_addr_t           dump_paddr;

	struct device        *dev;                      // lcdc dev
	struct drm_crtc      *crtc;

	//display config params
	u32 lcdc_bkg_color;
	u32 lcdc_undflw_mode;
	u32 lcdc_undflw_color;
	u32 lcdc_burstsize;
	u32 lcdc_laycolor_key_en;
	u32 lcdc_laycolor_key;
	u32 lcdc_layblend_factor;
	u32 lcdc_layblend_alpha;

	u8  pm_status;       // pm status
	u8  lcdc_config;
	u32 lcdc_debug;
};

static void ameba_drm_reconfig_hw(struct lcdc_hw_ctx_type *lcdc_ctx,struct ameba_drm_struct *ameba_struct);

// sys debug
// path:sys/devices/platform/ocp/400d8000.drm
static ssize_t lcdc_debug_show(struct device *dev, struct device_attribute*attr, char *buf)
{
	struct drm_device           *drm = dev_get_drvdata(dev);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct ameba_drm_private    *ameba_priv = (struct ameba_drm_private*)(ameba_struct->ameba_drm_priv);
	struct lcdc_hw_ctx_type     *lcdc_ctx = (struct lcdc_hw_ctx_type*)(ameba_priv->lcdc_hw_ctx);

	return sprintf(buf, "lcdc_debug=%d\n", lcdc_ctx->lcdc_debug);
}

static ssize_t lcdc_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct drm_device           *drm = dev_get_drvdata(dev);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct ameba_drm_private    *ameba_priv = (struct ameba_drm_private*)(ameba_struct->ameba_drm_priv);
	struct lcdc_hw_ctx_type     *lcdc_ctx = (struct lcdc_hw_ctx_type*)(ameba_priv->lcdc_hw_ctx);
	u32 tmp = 0 ;
	char c;
	size_t tmp_count = count;
	char* pbuf = (char*)buf;

	while(*pbuf != 0 && tmp_count > 1)
	{
		c = *pbuf;
		if((c<'0')||(c>'9'))
			break;
		tmp = tmp * 10 + (c - '0');
		pbuf++;
		tmp_count--;
	}

	if(TRIGGER_DUMP_LDCD_REG == tmp) {
		ameba_lcdc_reg_dump(lcdc_ctx->lcdc_reg, __func__);
	} else if(UPDATE_DRM_DEBUG_MASK == tmp){
		drm_debug = 0xFFFFFFFF&(~DRM_UT_VBL);
	} else if(UPDATE_AMEBA_DRM_DEBUG == tmp){
		drm_debug = DRM_DEBUG_VALUE;
	} else {
		lcdc_ctx->lcdc_debug = tmp;
	}

	return count;
}

static DEVICE_ATTR(lcdc_debug, S_IWUSR |S_IRUGO, lcdc_debug_show, lcdc_debug_store);

static struct attribute *ameba_lcdc_debug_attrs[] = {
        &dev_attr_lcdc_debug.attr,
        NULL
};

static const struct attribute_group ameba_lcdc_debug_attr_grp = {
        .attrs =ameba_lcdc_debug_attrs,
};


///defined in drm_fourcc.c :: drm_mode_legacy_fb_format
static const struct ameba_format ameba_formats[] = {
	/* 16bpp RGB: */
	{2, DRM_FORMAT_RGB565, LCDC_LAYER_IMG_FORMAT_RGB565 },
	/* 32bpp [A]RGB: */
	{4, DRM_FORMAT_ARGB8888, LCDC_LAYER_IMG_FORMAT_ARGB8888 },
	/* 16bpp ARGB: */
	{2, DRM_FORMAT_XRGB1555, LCDC_LAYER_IMG_FORMAT_ARGB1555 },
	/* 24bpp RGB: */
	{3, DRM_FORMAT_RGB888, LCDC_LAYER_IMG_FORMAT_RGB888 },
};

static const u32 ameba_channel_formats[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_RGB888
};

static u32 ameba_get_bpp(u32 pixel_format)
{
	int        i;
	for (i = 0; i < ARRAY_SIZE(ameba_formats); i++)
	{
		if (ameba_formats[i].pixel_format == pixel_format) {
			return ameba_formats[i].pixel_bpp;
		}
	}
	return 4;
}

/* convert from fourcc format to lcdc format */
static u32 ameba_get_format(u32 pixel_format)
{
	int        i;
	for (i = 0; i < ARRAY_SIZE(ameba_formats); i++)
	{
		if (ameba_formats[i].pixel_format == pixel_format) {
			return ameba_formats[i].hw_format;
		}
	}

	/* not found */
	DRM_ERROR("Not found pixel format!!fourcc_format= 0x%x\n", pixel_format);
	return LCDC_LAYER_IMG_FORMAT_NOT_SUPPORT;
}

static bool ameba_crtc_mode_fixup(struct drm_crtc *crtc,
                                  const struct drm_display_mode *mode,
                                  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static int ameba_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct lcdc_hw_ctx_type     *lcdc_ctx;
	struct ameba_crtc           *kcrtc = to_ameba_crtc(crtc);
	AMEBA_DRM_DEBUG();

	if( CHECK_IS_NULL(kcrtc) || CHECK_IS_NULL(kcrtc->lcdc_hw_ctx))
		return 0;

	lcdc_ctx = kcrtc->lcdc_hw_ctx;

	ameba_lcdc_irq_enable(lcdc_ctx->lcdc_reg, LCDC_BIT_LCD_LIN_INTEN, 1);
	ameba_lcdc_enable_SHW(lcdc_ctx->lcdc_reg);

	return 0 ;
}

static void ameba_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct lcdc_hw_ctx_type     *lcdc_ctx;
	struct ameba_crtc           *kcrtc = to_ameba_crtc(crtc);
	AMEBA_DRM_DEBUG();

	if( CHECK_IS_NULL(kcrtc) || CHECK_IS_NULL(kcrtc->lcdc_hw_ctx))
		return;

	lcdc_ctx = (struct lcdc_hw_ctx_type *)(kcrtc->lcdc_hw_ctx);

	ameba_lcdc_irq_enable(lcdc_ctx->lcdc_reg, LCDC_BIT_LCD_LIN_INTEN, 0);

	ameba_lcdc_enable_SHW(lcdc_ctx->lcdc_reg);
}

static irqreturn_t ameba_irq_handler(int irq, void *data)
{
	/* vblank irq */
	u32 irq_status;
	struct ameba_drm_struct     *ameba_struct = (struct ameba_drm_struct *)data;
	struct drm_crtc             *crtc = ameba_struct->crtc;
	struct ameba_crtc           *kcrtc = to_ameba_crtc(crtc);
	struct lcdc_hw_ctx_type     *lcdc_ctx;
	struct ameba_drm_reg_t      *dsi;

	if( CHECK_IS_NULL(kcrtc) || CHECK_IS_NULL(kcrtc->lcdc_hw_ctx))
		return IRQ_HANDLED;

	lcdc_ctx = kcrtc->lcdc_hw_ctx;

	irq_status = ameba_lcdc_get_irqstatus(lcdc_ctx->lcdc_reg) ;
	LCDC_ClearINT(lcdc_ctx->lcdc_reg, irq_status);

	if (irq_status & LCDC_BIT_LCD_LIN_INTS) {
		///send out vblank msg
		drm_crtc_handle_vblank(crtc);
	}

	if ((lcdc_ctx->pm_status == 0) && (irq_status & LCDC_BIT_DMA_UN_INTS)) {
		AMEBA_DRM_DEBUG();
		ameba_struct->under_flow_count ++;
		ameba_struct->under_flow_flag ++;
		if (1 == ameba_struct->under_flow_flag) {
			DRM_DEBUG_DRIVER("Underflow happen [%d-%d]\n",ameba_struct->under_flow_count,ameba_struct->under_flow_flag);
			dsi = (struct ameba_drm_reg_t *)dev_get_drvdata(ameba_struct->dsi_dev);
			ameba_lcdc_dsi_underflow_reset(dsi->reg);
		}
	}

	return IRQ_HANDLED;
}

static inline void ameba_dump_regs(struct lcdc_hw_ctx_type *ctx, const char *filename)
{
	if(ctx->lcdc_debug)
	{
		ameba_lcdc_reg_dump(ctx->lcdc_reg, filename);
	}
}

static void ameba_crtc_atomic_enable(struct drm_crtc *crtc,
                                     struct drm_crtc_state *old_state)
{
	int                         ret;
	void __iomem                *base;
	struct lcdc_hw_ctx_type     *ctx;
	struct device               *dev;
	struct drm_device           *drm;
	struct ameba_drm_struct     *ameba_struct;
	struct ameba_crtc           *kcrtc = to_ameba_crtc(crtc);

	AMEBA_DRM_DEBUG();

	if( CHECK_IS_NULL(kcrtc) 
		|| kcrtc->enable )
		return ;

	ctx = kcrtc->lcdc_hw_ctx;
	dev = ctx->dev;
	drm = dev_get_drvdata(dev);
	ameba_struct = drm->dev_private;

	base = ctx->lcdc_reg;

	ret = clk_prepare_enable(ctx->lcdc_clk);
	if (ret < 0) {
		DRM_ERROR("Fail to enable lcdc clock(%d)\n", ret);
	} else {
		DRM_DEBUG("Enable lcdc clk success\n");
	}

	if(ctx->lcdc_config == 0){
		//init the lcdc reg
		ameba_drm_reconfig_hw(ctx,ameba_struct);
		ctx->lcdc_config = 1;
	}

	// for PM status, enable lcdc before set dsi to video mode
	if(ctx->pm_status == 0) {
		ameba_lcdc_enable(base, 0);
		ameba_lcdc_enable(base, 1);
	} 

	ameba_lcdc_enable_SHW(base);

	ameba_dump_regs(ctx, __func__);

	drm_crtc_vblank_on(crtc);

	kcrtc->enable = true;
}

static void ameba_crtc_atomic_disable(struct drm_crtc *crtc,
                                      struct drm_crtc_state *old_state)
{
	struct lcdc_hw_ctx_type     *ctx;
	struct ameba_crtc           *kcrtc = to_ameba_crtc(crtc);
	AMEBA_DRM_DEBUG();

	if( CHECK_IS_NULL(kcrtc) 
		|| !kcrtc->enable )
		return ;

	ctx = kcrtc->lcdc_hw_ctx;
	drm_crtc_vblank_off(crtc);

	if(ctx->pm_status)
	{
		//disable clk
		DRM_DEBUG("Close lcdc clk\n");
		clk_disable_unprepare(ctx->lcdc_clk);

		ctx->lcdc_config = 0;

		ameba_lcdc_enable(ctx->lcdc_reg, 0);
		ameba_lcdc_enable_SHW(ctx->lcdc_reg);
	}

	kcrtc->enable = false;
}
/*
	This callback is used to update the display mode of a CRTC without
	* changing anything of the primary plane configuration
*/
static void ameba_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
}

static void ameba_crtc_atomic_flush(struct drm_crtc *crtc,
                                    struct drm_crtc_state *old_state)
{
	struct ameba_crtc                 *kcrtc = to_ameba_crtc(crtc);
	struct drm_pending_vblank_event   *event;
	struct lcdc_hw_ctx_type           *lcdc_ctx ;
	
	if( CHECK_IS_NULL(kcrtc) || CHECK_IS_NULL(kcrtc->lcdc_hw_ctx) 
		|| CHECK_IS_NULL(crtc->state) )
		return;

	lcdc_ctx = kcrtc->lcdc_hw_ctx;
	event = crtc->state->event;

	/* only crtc is enabled regs take effect */
	if (kcrtc->enable) {
		/* flush lcdc registers */
		ameba_lcdc_enable(lcdc_ctx->lcdc_reg, 1);
		ameba_dump_regs(lcdc_ctx, __func__);
	}

	if (event) {
		crtc->state->event = NULL;

		spin_lock_irq(&crtc->dev->event_lock);
		if (drm_crtc_vblank_get(crtc) == 0) {
			drm_crtc_arm_vblank_event(crtc, event);
		} else {
			drm_crtc_send_vblank_event(crtc, event);
		}
		spin_unlock_irq(&crtc->dev->event_lock);
	}
}

static const struct drm_crtc_helper_funcs ameba_crtc_helper_funcs = {
	.mode_fixup          = ameba_crtc_mode_fixup,
	.mode_set_nofb       = ameba_crtc_mode_set_nofb,
	.atomic_flush        = ameba_crtc_atomic_flush,
	.atomic_enable       = ameba_crtc_atomic_enable,
	.atomic_disable      = ameba_crtc_atomic_disable,
};

static const struct drm_crtc_funcs ameba_crtc_funcs = {
	.destroy                = drm_crtc_cleanup,
	.set_config             = drm_atomic_helper_set_config,
	.page_flip              = drm_atomic_helper_page_flip,
	.reset                  = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank          = ameba_crtc_enable_vblank,
	.disable_vblank         = ameba_crtc_disable_vblank,
};

static int ameba_plane_atomic_check(struct drm_plane *plane,
                                    struct drm_plane_state *state)
{
	struct drm_framebuffer  *fb;
	struct drm_crtc         *crtc;
	struct drm_crtc_state   *crtc_state;
	u32                     src_x, src_y, src_w, src_h, crtc_w, crtc_h, fmt;
	int                     crtc_x, crtc_y ;

	if( CHECK_IS_NULL(state) )
		return -EINVAL;

	AMEBA_DRM_DEBUG();

	fb = state->fb;
	crtc = state->crtc;

	src_x = state->src_x >> 16;
	src_y = state->src_y >> 16;
	src_w = state->src_w >> 16;
	src_h = state->src_h >> 16;
	crtc_x = state->crtc_x;
	crtc_y = state->crtc_y;
	crtc_w = state->crtc_w;
	crtc_h = state->crtc_h;

	if (!crtc || !fb) {
		return 0;
	}

	fmt = ameba_get_format(fb->format->format);
	if (fmt >= LCDC_LAYER_IMG_FORMAT_NOT_SUPPORT) {
		return -EINVAL;
	}

	crtc_state = drm_atomic_get_crtc_state(state->state, crtc);
	if (IS_ERR(crtc_state)) {
		return PTR_ERR(crtc_state);
	}

	if (src_w != crtc_w || src_h != crtc_h) {
		return -EINVAL;
	}

	if (src_x + src_w > fb->width ||
		src_y + src_h > fb->height) {
		return -EINVAL;
	}

	if (crtc_x < 0 || crtc_y < 0) {
		return -EINVAL;
	}

	if (crtc_x + crtc_w > crtc_state->adjusted_mode.hdisplay ||
		crtc_y + crtc_h > crtc_state->adjusted_mode.vdisplay) {
		return -EINVAL;
	}

	return 0;
}

static void ameba_plane_atomic_update(struct drm_plane *plane,
                                      struct drm_plane_state *old_state)
{
	struct lcdc_hw_ctx_type     *lcdc_local;
	LCDC_InitTypeDef            *lcdc_tmp;
	struct drm_framebuffer      *fb;
	struct drm_gem_cma_object   *gem_cma_obj;
	struct drm_plane_state      *state;
	struct ameba_plane          *kplane = to_ameba_plane(plane);
	int                         dest_x, dest_y, dest_w, dest_h;
	int                         display_width,display_height;
	u8                          idx;
	u32                         fmt;
	u32                         src_x, src_y, src_w, src_h;
	struct drm_device           *drm;
	struct ameba_drm_struct     *ameba_struct;

	AMEBA_DRM_DEBUG();

	if( CHECK_IS_NULL(plane)
		|| CHECK_IS_NULL(plane->state)
		|| CHECK_IS_NULL(plane->state->fb))
		return ;

	state = plane->state;
	lcdc_local = (struct lcdc_hw_ctx_type *)(kplane->lcdc_hw_ctx);
	lcdc_tmp = &(lcdc_local->lcdc_initdef);
	idx = kplane->ch;
	fb = state->fb;
	src_x = state->src_x >> 16;
	src_y = state->src_y >> 16;
	src_w = state->src_w >> 16;
	src_h = state->src_h >> 16;

	gem_cma_obj = drm_fb_cma_get_gem_obj(fb, 0);
	if( CHECK_IS_NULL(gem_cma_obj))
		return;

	drm = fb->dev;
	fmt = ameba_get_format(fb->format->format);
	if (fmt >= LCDC_LAYER_IMG_FORMAT_NOT_SUPPORT) {
		DRM_ERROR("Can not found pixel format,format(0x%x)\n", fb->format->format);
		return;
	}

	ameba_struct = drm->dev_private;
	display_width = ameba_struct->display_width;
	display_height = ameba_struct->display_height;

	//crtc_* is the display position
	//src_*  is the source display position
	//DRM_DEBUG_DRIVER("Debug %s-%d[id=%d,fmt=%d][%d*%d][%d-%d-%d-%d][%d-%d-%d-%d] \n", __func__, __LINE__,
	//				idx, fmt, fb->width, fb->height,
	//				state->crtc_x, state->crtc_y, state->crtc_w, state->crtc_h,
	//				src_x, src_y, src_w, src_h);

	///check the params , x <  display_width ; y < display_height
	if (state->crtc_x >= display_width || state->crtc_y >= display_height) {
		DRM_WARN("[error happen] %s-%d [start[%d-%d][%d-%d]\n", __func__, __LINE__,
				 state->crtc_x, state->crtc_y, display_width, display_height);
		return;
	}

	//display from 1~MIPI_DISPLAY_WIDTH
	dest_x = state->crtc_x ; //((state->crtc_x==0)?(1):(state->crtc_x));
	dest_y = state->crtc_y ; //((state->crtc_y==0)?(1):(state->crtc_y));

	{
		//check the data valid
		//crtc[] is the plane/layer position,
		//src[] is the source info , surface[0,0,w,h], copy the data from the fb[src_x,src_y,w,h] to [0,0,w,h]
		//src[src.x,src.y,dest.w,dest.h] , dest[crtc.x,crtc.y,dest.w,dest.h]

		//How to get the dest.w/dest.h
		//1.1 src.w > crtc.w , surface.w = crtc.w
		//1.2 src.w < crtc.w , surface.w = src.w
		//2.1 src.h > crtc.h , surface.h = crtc.h
		//2.2 src.h < crtc.h , surface.h = src.h

		//the desc w&h is the common place
		dest_w = ((state->crtc_w > src_w) ? (src_w) : (state->crtc_w));
		if (dest_x + dest_w > display_width) {
			dest_w = display_width - dest_x;
		}
		dest_h = ((state->crtc_h > src_h) ? (src_h) : (state->crtc_h));
		if (dest_y + dest_h > display_height) {
			dest_h = display_height - dest_y;
		}

		//DRM_DEBUG_DRIVER("Debug %s-%d[%d-%d-%d-%d]fb->width=%d \n", __func__, __LINE__, dest_x, dest_y, dest_w, dest_h, fb->width);

		//from src_x&src_y  to  crtc_x & crtc_y
		//w not match , x not 0 ,
		//if y !=0, maybe just set a offset from the pixmap
		//dest_w != fb->width it is just a window
		//state->src_x!=0 or state->src_y !=0 , it is not from the first pixel
		if (dest_w != fb->width || src_x || src_y) {
			int y ;
			u32 bpp = ameba_get_bpp(fb->format->format);
			//2.do copy
			//DRM_TEST_PRINTK("here %s-%d[id=%d][%d-%d-%d-%d]width=%d/bpp=%d \n", __func__, __LINE__,idx, dest_x, dest_y, dest_w, dest_h, fb->width, bpp);
			for (y = 0; y < dest_h ; ++y) {
				memcpy((uint8_t *)sec_layer_info[idx].sec_vaddr + (y * dest_w * bpp),
					   (uint8_t *)gem_cma_obj->vaddr + ((y + src_y) * (fb->width) * bpp) + (bpp * src_x),
					   dest_w * bpp);
			}
			sec_layer_info[idx].use_sec_buffer = 1;
		} else {
			sec_layer_info[idx].use_sec_buffer = 0 ;
		}
	}

	ameba_lcdc_layer_enable(lcdc_tmp, idx, 1);
	ameba_lcdc_layer_imgfmt(lcdc_tmp, idx, fmt);

	if (sec_layer_info[idx].use_sec_buffer) {
		ameba_lcdc_layer_imgaddress(lcdc_tmp, idx, (u32)(sec_layer_info[idx].sec_paddr));
	} else {
		ameba_lcdc_layer_imgaddress(lcdc_tmp, idx, (u32)(gem_cma_obj->paddr));
	}
	sec_layer_info[idx].layer_address = (u32)gem_cma_obj->vaddr;
	//DRM_DEBUG_DRIVER("Debug %s-%d[%d-%d-%d-%d] \n", __func__, __LINE__, dest_x, dest_y, dest_w, dest_h);

	if (state->crtc_x <= 0) {
		dest_x = 1;
	}
	if (state->crtc_y <= 0) {
		dest_y = 1;
	}
	dest_w -= 1;
	dest_h -= 1;

	if (dest_x + dest_w > display_width) {
		dest_w = display_width - dest_x;
	}
	if (dest_y + dest_h > display_height) {
		dest_h = display_height - dest_y;
	}

	ameba_lcdc_layer_pos(lcdc_tmp, idx, dest_x, dest_x + dest_w, dest_y, dest_y + dest_h);

	ameba_lcdc_update_layer_reg(lcdc_local->lcdc_reg, idx, &(lcdc_tmp->layerx[idx]));
	//enable DSI issue
	ameba_lcdc_enable_SHW(lcdc_local->lcdc_reg);
}

static const struct vm_operations_struct ameba_drm_gem_vm_ops = {
	.open  = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};
static const struct file_operations ameba_drm_file_fops = {
	.owner          = THIS_MODULE,
	.open           = drm_open,
	.mmap           = drm_gem_cma_mmap,
	.unlocked_ioctl = drm_ioctl,
	.compat_ioctl   = drm_compat_ioctl,
	.poll           = drm_poll,
	.read           = drm_read,
	.llseek         = no_llseek,
	.release        = drm_release,
};

void ameba_drm_gem_free_object(struct drm_gem_object *gem_obj)
{
	u8                          i = 0;
	struct drm_device           *drm = gem_obj->dev;
	struct drm_gem_cma_object   *cma_obj = to_drm_gem_cma_obj(gem_obj);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct lcdc_hw_ctx_type     *lcdc_local = (struct lcdc_hw_ctx_type *)ameba_struct->lcdc_hw_ctx;

	AMEBA_DRM_DEBUG();

	for (i = 0 ; i < LCDC_LAYER_MAX_NUM ; i ++) {
		LCDC_InitTypeDef *lcdc_tmp = &(lcdc_local->lcdc_initdef) ;
		if (cma_obj->vaddr && ((u32)cma_obj->vaddr) == sec_layer_info[i].layer_address) {
			ameba_lcdc_layer_enable(lcdc_tmp, i, 0);

			ameba_lcdc_update_layer_reg(lcdc_local->lcdc_reg, i, &(lcdc_tmp->layerx[i]));
			ameba_lcdc_enable_SHW(lcdc_local->lcdc_reg);

			sec_layer_info[i].layer_address = 0 ;
			sec_layer_info[i].use_sec_buffer = 0 ;
			/*
				fps=60 , shadow will be enable in 16.7ms,
				sleep 50ms ,lcdc will not use the buffer anymore
				it is safe to release the mem
			*/
			msleep(50);
			ameba_dump_regs(lcdc_local, __func__);
		}
	}

	drm_gem_cma_free_object(gem_obj);
}
static bool  ameba_drm_get_vblank_timestamp(struct drm_device *dev, unsigned int pipe,
                                            int *max_error, ktime_t *vblank_time,
                                            bool in_vblank_irq)
{
	return false;
}

/*
* @plane: plane object to update
* @crtc: owning CRTC of owning plane
* @fb: framebuffer to flip onto plane
* @crtc_x: x offset of primary plane on crtc
* @crtc_y: y offset of primary plane on crtc
* @crtc_w: width of primary plane rectangle on crtc
* @crtc_h: height of primary plane rectangle on crtc
* @src_x: x offset of @fb for panning
* @src_y: y offset of @fb for panning
* @src_w: width of source rectangle in @fb
* @src_h: height of source rectangle in @fb
* @ctx: lock acquire context
*
*/
static int ameba_drm_update_plane(struct drm_plane *plane,
                                   struct drm_crtc *crtc,
                                   struct drm_framebuffer *fb,
                                   int crtc_x, int crtc_y,
                                   unsigned int crtc_w, unsigned int crtc_h,
                                   uint32_t src_x, uint32_t src_y,
                                   uint32_t src_w, uint32_t src_h,
                                   struct drm_modeset_acquire_ctx *ctx)
{
	return drm_atomic_helper_update_plane(plane, crtc, fb, crtc_x, crtc_y, crtc_w, crtc_h, src_x, src_y, src_w, src_h, ctx);
}

//plane
static const struct drm_plane_helper_funcs ameba_plane_helper_funcs = {
	.atomic_check   = ameba_plane_atomic_check,
	.atomic_update  = ameba_plane_atomic_update,
};

static struct drm_plane_funcs ameba_plane_funcs = {
	.update_plane           = ameba_drm_update_plane,
	.disable_plane          = drm_atomic_helper_disable_plane,
	.destroy                = drm_plane_cleanup,
	.reset                  = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_plane_destroy_state,
};

static void *ameba_hw_ctx_alloc(struct platform_device *pdev,
                                struct drm_crtc *crtc)
{
	int                         ret;
	struct resource             *res;
	struct device               *dev = &pdev->dev;
	struct drm_device           *drm = dev_get_drvdata(dev);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;
	struct lcdc_hw_ctx_type     *lcdc_ctx = NULL;

	AMEBA_DRM_DEBUG();

	for (ret = 0 ; ret < LCDC_LAYER_MAX_NUM; ret ++) {
		memset(&(sec_layer_info[ret]),0x00,sizeof(struct ameba_buf_struct));
	}

	lcdc_ctx = devm_kzalloc(dev, sizeof(*lcdc_ctx), GFP_KERNEL);
	if (!lcdc_ctx) {
		DRM_ERROR("Fail to alloc lcdc_hw_ctx\n");
		return ERR_PTR(-ENOMEM);
	}
	lcdc_ctx->crtc = crtc;
	lcdc_ctx->dev = dev;
	ameba_struct->lcdc_hw_ctx = lcdc_ctx;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	lcdc_ctx->lcdc_reg = devm_ioremap_resource(dev, res);
	if (0 == (lcdc_ctx->lcdc_reg)) {
		DRM_ERROR("Fail to remap lcdc io base\n");
		return ERR_PTR(-ENODEV);
	}

	// get lcdc clk
	lcdc_ctx->lcdc_clk = devm_clk_get(dev, NULL);
	if (IS_ERR(lcdc_ctx->lcdc_clk)) {
		DRM_ERROR("Fail to get lcdc clock(%d)\n", __LINE__);
		return ERR_PTR(-ENODEV);
	}

	// enable lcdc clock
	ret = clk_prepare_enable(lcdc_ctx->lcdc_clk);
	if (ret < 0) {
		DRM_ERROR("Fail to enable lcdc clock(%d)\n", ret);
		return ERR_PTR(-ENODEV);
	}

	lcdc_ctx->irq = platform_get_irq(pdev, 0);
	if (lcdc_ctx->irq < 0) {
		DRM_ERROR("Fail to get irq\n");
		return ERR_PTR(-ENODEV);
	}

	// irq init
	ameba_struct->crtc = crtc;
	ret = devm_request_irq(dev, lcdc_ctx->irq, ameba_irq_handler, 0, dev_name(dev), ameba_struct);
	if (ret) {
		return ERR_PTR(-EIO);
	}

	// Initialize reserved memory resources
	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		DRM_ERROR("Could not get reserved memory\n");
		return ERR_PTR(-ENODEV);
	}

	// debug attribute
	lcdc_ctx->lcdc_debug = 0;
	if ( sysfs_create_group(&pdev->dev.kobj,&ameba_lcdc_debug_attr_grp) )
		DRM_ERROR("Error creating lcdc sysfs entry\n");

	lcdc_ctx->lcdc_burstsize = LCDC_LAYER_BURSTSIZE_4X64BYTES;
	lcdc_ctx->lcdc_bkg_color = LCDC_KG_COLOR;
	lcdc_ctx->lcdc_undflw_mode = 1;
	lcdc_ctx->lcdc_undflw_color = LCDC_UNDFLOW_COLOR;
	lcdc_ctx->lcdc_laycolor_key_en = 0;
	lcdc_ctx->lcdc_laycolor_key = LCDC_LAY_COLOR_KEY;
	lcdc_ctx->lcdc_layblend_factor = 1;
	lcdc_ctx->lcdc_layblend_alpha = LCDC_LAY_BLEND_ALPHA;

/*
	lcdc_ctx->ldcd_writeback_buffer_length = (MIPI_DISPLAY_X * MIPI_DISPLAY_Y * LDCD_PIXFORMAT_MAX_LEN) ;
	//lcdc_ctx->ldcd_writeback_buffer = dma_alloc_coherent(&pdev->dev, lcdc_ctx->ldcd_writeback_buffer_length, &(lcdc_ctx->dump_paddr), GFP_KERNEL);
	lcdc_ctx->ldcd_writeback_buffer = 0 ;
	if (lcdc_ctx->ldcd_writeback_buffer) {
		DRM_DEBUG_DRIVER("Allocated coherent memory, vaddr: 0x%0llX, paddr: 0x%0llX[0x%x]\n",
				 (u64)lcdc_ctx->ldcd_writeback_buffer, (u64)lcdc_ctx->dump_paddr, (u32)lcdc_ctx->ldcd_writeback_buffer_length);
		memset((void *)lcdc_ctx->ldcd_writeback_buffer, 0x00, lcdc_ctx->ldcd_writeback_buffer_length);
	} else {
		lcdc_ctx->ldcd_writeback_buffer_length = 0 ;
	}
*/

	lcdc_ctx->ldcd_writeback_buffer = 0 ;
	lcdc_ctx->ldcd_writeback_buffer_length = 0 ;

	return lcdc_ctx;
}

static void ameba_drm_reconfig_hw(struct lcdc_hw_ctx_type *lcdc_ctx,struct ameba_drm_struct *ameba_struct)
{
	u8                          idx;
	size_t                      new_size;
	LCDC_InitTypeDef            *lcdc_tmp ;

	AMEBA_DRM_DEBUG();

	lcdc_tmp = &(lcdc_ctx->lcdc_initdef) ;

	//init the lcdc info
	ameba_lcdc_reset_config(lcdc_tmp, ameba_struct->display_width, ameba_struct->display_height, lcdc_ctx->lcdc_bkg_color);
	ameba_lcdc_set_planesize(lcdc_tmp, ameba_struct->display_width, ameba_struct->display_height);
	ameba_lcdc_set_background_color(lcdc_tmp, lcdc_ctx->lcdc_bkg_color);

	//dma issue
	ameba_lcdc_dma_config_bustsize(lcdc_ctx->lcdc_reg, lcdc_ctx->lcdc_burstsize);
	ameba_lcdc_dma_config_keeplastFrm(lcdc_ctx->lcdc_reg, lcdc_ctx->lcdc_undflw_mode, lcdc_ctx->lcdc_undflw_color);

	// line number interrupt
	ameba_lcdc_irq_linepos(lcdc_ctx->lcdc_reg, ameba_struct->display_height * 4 / 5);
	//enable dma underflow interrupt
	ameba_lcdc_irq_config(lcdc_ctx->lcdc_reg, LCDC_BIT_DMA_UN_INTEN, 1);

	if (lcdc_ctx->ldcd_writeback_buffer) {
		ameba_lcdc_dma_debug_config(lcdc_ctx->lcdc_reg, LCDC_DMA_OUT_ENABLE, (u32)lcdc_ctx->ldcd_writeback_buffer);
	} else {
		ameba_lcdc_dma_debug_config(lcdc_ctx->lcdc_reg, LCDC_DMA_OUT_DISABLE, 0);
	}

	//set valid , disable all display , it should not display anything right now
	ameba_lcdc_config_setvalid(lcdc_ctx->lcdc_reg, lcdc_tmp);

	//layer info
	for (idx = 0; idx < LCDC_LAYER_MAX_NUM; idx++) {
		ameba_lcdc_layer_enable(lcdc_tmp, idx, 0);
		ameba_lcdc_layer_colorkey_enable(lcdc_tmp, idx, lcdc_ctx->lcdc_laycolor_key_en);
		ameba_lcdc_layer_colorkey_value(lcdc_tmp, idx, lcdc_ctx->lcdc_laycolor_key);
		ameba_lcdc_layer_blend_value(lcdc_tmp, idx, lcdc_ctx->lcdc_layblend_factor);
		ameba_lcdc_layer_alpha_value(lcdc_tmp, idx, lcdc_ctx->lcdc_layblend_alpha);
	}

	//second surface init
	for (idx = 0 ; idx < LCDC_LAYER_MAX_NUM; idx ++) {
		struct ameba_buf_struct * layinfo = &(sec_layer_info[idx]);
		new_size = round_up(ameba_struct->display_width * ameba_struct->display_height * 4, PAGE_SIZE);
		if(layinfo->sec_vaddr && new_size != layinfo->size){
			layinfo->layer_address = 0;
			layinfo->use_sec_buffer = 0;
			dma_free_coherent(lcdc_ctx->dev, layinfo->size, layinfo->sec_vaddr, layinfo->sec_paddr);
			layinfo->sec_vaddr = NULL;
		}
		layinfo->size = new_size;
		if(NULL == layinfo->sec_vaddr){
			layinfo->sec_vaddr = dma_alloc_coherent(lcdc_ctx->dev, layinfo->size, &(layinfo->sec_paddr), GFP_KERNEL);
		}
	}

	ameba_lcdc_enable_SHW(lcdc_ctx->lcdc_reg);
}

static void ameba_hw_ctx_cleanup(struct platform_device *pdev, void *hw_ctx)
{
	int                         ret; 
	struct device               *dev = &pdev->dev;
	struct lcdc_hw_ctx_type     *lcdc_ctx = (struct lcdc_hw_ctx_type *)hw_ctx ;
	struct drm_device           *drm = dev_get_drvdata(dev);
	struct ameba_drm_struct     *ameba_struct = drm->dev_private;

	//release all resource
	devm_free_irq(dev, lcdc_ctx->irq, ameba_struct);

	clk_disable_unprepare(lcdc_ctx->lcdc_clk);

	for (ret = 0 ; ret < LCDC_LAYER_MAX_NUM; ret ++) {
		struct ameba_buf_struct * layinfo = &(sec_layer_info[ret]);
		dma_free_coherent(dev, layinfo->size, layinfo->sec_vaddr, layinfo->sec_paddr);
	}
	of_reserved_mem_device_release(dev);

	sysfs_remove_group(&dev->kobj,&ameba_lcdc_debug_attr_grp);
}

static struct drm_framebuffer * ameba_user_fb_create(struct drm_device *dev, 
                                                     struct drm_file *file_priv,
                                                     const struct drm_mode_fb_cmd2 *mode_cmd)
{
	if( CHECK_IS_NULL(mode_cmd))
			return ERR_PTR(-EINVAL);

	if (ameba_get_format(mode_cmd->pixel_format) >= LCDC_LAYER_IMG_FORMAT_NOT_SUPPORT) {
		DRM_ERROR("Pixel format error, not support(%x)\n", mode_cmd->pixel_format);
		return ERR_PTR(-EINVAL) ;
	}
	return drm_gem_fb_create(dev, file_priv, mode_cmd);
}

static int ameba_drm_atomic_helper_check(struct drm_device *dev,
                                         struct drm_atomic_state *state)
{
	return drm_atomic_helper_check(dev, state);
}
static int ameba_drm_atomic_helper_commit(struct drm_device *dev,
                                          struct drm_atomic_state *state,
                                          bool nonblock)
{
	AMEBA_DRM_DEBUG();
	return drm_atomic_helper_commit(dev, state, nonblock);
}

static const struct drm_mode_config_funcs ameba_mode_config_funcs = {
	.fb_create      = ameba_user_fb_create,
	.atomic_check   = ameba_drm_atomic_helper_check,
	.atomic_commit  = ameba_drm_atomic_helper_commit,
};

static struct drm_driver ameba_lcdc_driver = {
	.driver_features            = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops                       = &ameba_drm_file_fops,
	.dumb_create                = drm_gem_cma_dumb_create_internal,
	.gem_vm_ops                 = &ameba_drm_gem_vm_ops,
	.gem_free_object_unlocked   = ameba_drm_gem_free_object,
	.get_vblank_timestamp       = ameba_drm_get_vblank_timestamp,

//	.prime_handle_to_fd	    = drm_gem_prime_handle_to_fd,
//	.prime_fd_to_handle	    = drm_gem_prime_fd_to_handle,
//	.gem_prime_get_sg_table	    = drm_gem_cma_prime_get_sg_table,
//	.gem_prime_import_sg_table  = drm_gem_cma_prime_import_sg_table,
//	.gem_prime_vmap		    = drm_gem_cma_prime_vmap,
//	.gem_prime_vunmap	    = drm_gem_cma_prime_vunmap,
//	.gem_prime_mmap		    = drm_gem_cma_prime_mmap,

	.name   = "ameba",
	.desc   = "Realtek Ameba SoC DRM Driver",
	.date   = "20210916",
	.major  = 1,
	.minor  = 0,
};

static struct ameba_drm_driver_data ameba_lcdc_driver_data = {
	.num_planes           = LCDC_LAYER_MAX_NUM,
	.prim_plane           = LCDC_LAYER_LAYER1,
	.channel_formats      = ameba_channel_formats,
	.channel_formats_cnt  = ARRAY_SIZE(ameba_channel_formats),
	.config_max_width     = 2048,
	.config_max_height    = 2048,
	.driver               = &ameba_lcdc_driver,
	.crtc_helper_funcs    = &ameba_crtc_helper_funcs,
	.crtc_funcs           = &ameba_crtc_funcs,
	.plane_helper_funcs   = &ameba_plane_helper_funcs,
	.plane_funcs          = &ameba_plane_funcs,
	.mode_config_funcs    = &ameba_mode_config_funcs,
	.alloc_hw_ctx         = ameba_hw_ctx_alloc,
	.cleanup_hw_ctx       = ameba_hw_ctx_cleanup,
};
