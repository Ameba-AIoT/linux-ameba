// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DRM support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/slab.h>
#include <linux/delay.h>
#include <video/mipi_display.h>
#include <drm/drm_print.h>

#include "panel/ameba_panel_base.h"
#include "ameba_drm_comm.h"

//should remove this to mode params
#define MIPI_DSI_RTNI         2//4

#define MIPI_DSI_HSA          4  //hsw mode->hsync_end - mode->hsync_start;
#define MIPI_DSI_HBP          30 //hbp mode->htotal - mode->hsync_end;
#define MIPI_DSI_HFP          30 //hfp hsync_start - mode->hdisplay;

#define MIPI_DSI_VSA          5  //vsw mode->vsync_end - mode->vsync_start;
#define MIPI_DSI_VBP          20 //vbp mode->vtotal - mode->vsync_end;
#define MIPI_DSI_VFP          15 //vfp mode->vsync_start - mode->vdisplay;

#define Mhz                   1000000UL
#define T_LPX                 5
#define T_HS_PREP             6
#define T_HS_TRAIL            8
#define T_HS_EXIT             7
#define T_HS_ZERO             10

#define DUMP_REG(a,b)         b,readl(a + b)

/*
	struct define
*/
typedef struct {
	u8 alpha;
	u8 blue;
	u8 green;
	u8 red;
} rgb888_t;


/*
*	lcdc apis
*/
static void ameba_lcdc_init_config(LCDC_InitTypeDef *plcdc_initstruct, u8 *imgbuffer, u32 bgcolor, u16 widht, u16 height)
{
	u8                    idx ;
	rgb888_t              bg_color;
	if (NULL == plcdc_initstruct) {
		return;
	}
	LCDC_StructInit(plcdc_initstruct);
	plcdc_initstruct->LCDC_ImageWidth = widht;
	plcdc_initstruct->LCDC_ImageHeight = height;

	*(u32 *)&bg_color = bgcolor;
	plcdc_initstruct->LCDC_BgColorRed = bg_color.red;
	plcdc_initstruct->LCDC_BgColorGreen = bg_color.green;
	plcdc_initstruct->LCDC_BgColorBlue = bg_color.blue;

	for (idx = 0; idx < LCDC_LAYER_MAX_NUM; idx++) {
		plcdc_initstruct->layerx[idx].LCDC_LayerEn = 0;
		plcdc_initstruct->layerx[idx].LCDC_LayerImgFormat = LCDC_LAYER_IMG_FORMAT_ARGB8888;//default value
		plcdc_initstruct->layerx[idx].LCDC_LayerImgBaseAddr = (u32)imgbuffer;
		plcdc_initstruct->layerx[idx].LCDC_LayerHorizontalStart = 1;/*1-based*/
		plcdc_initstruct->layerx[idx].LCDC_LayerHorizontalStop = widht;
		plcdc_initstruct->layerx[idx].LCDC_LayerVerticalStart = 1;/*1-based*/
		plcdc_initstruct->layerx[idx].LCDC_LayerVerticalStop = height;
	}
}

static void ameba_lcdc_dsi_mode_set(void __iomem *MIPIx_, u32 video)
{
	u32     MIPIx = (u32) MIPIx_;
	u32     Value32 = readl((void*)(MIPIx + MIPI_MAIN_CTRL_OFFSET)) ;
	if (video) {
		Value32 |= MIPI_BIT_DSI_MODE;
		writel(Value32, (void*)(MIPIx + MIPI_MAIN_CTRL_OFFSET));
	} else {
		Value32 &= ~MIPI_BIT_DSI_MODE;
		writel(Value32, (void*)(MIPIx + MIPI_MAIN_CTRL_OFFSET));
	}
}

/*handle for underflow issue*/
static u32 ameba_lcdc_dsi_intr_acpu_get(void __iomem *MIPIx_)
{
	u32     MIPIx = (u32) MIPIx_;
	return readl((void*)(MIPIx + MIPI_INTS_ACPU_OFFSET));
}

static void ameba_lcdc_dsi_intr_acpu_clr(void __iomem *MIPIx_, u32 DSI_INTS)
{
	u32     MIPIx = (u32) MIPIx_;
	/*clear the specified interrupt*/
	writel(DSI_INTS, (void*)(MIPIx + MIPI_INTS_ACPU_OFFSET));
}

static void ameba_lcdc_dsi_intr_config(void __iomem *MIPIx_, u8 AcpuEn, u8 ScpuEN, u8 IntSelAnS)
{
	u32     Value32;
	u32     MIPIx = (u32) MIPIx_;

	Value32 = readl((void*)(MIPIx + MIPI_INTE_OFFSET));
	Value32 &= ~(MIPI_BIT_INTP_EN_ACPU | MIPI_BIT_INTP_EN | MIPI_BIT_SEL);
	if (AcpuEn) {
		Value32 |= MIPI_BIT_INTP_EN_ACPU;
	}
	if (ScpuEN) {
		Value32 |= MIPI_BIT_INTP_EN;
	}

	if (IntSelAnS) {
		Value32 |= MIPI_BIT_SEL;
	}
	writel(Value32, (void*)(MIPIx + MIPI_INTE_OFFSET));
}

//lcdc global register APIs
void ameba_lcdc_reg_dump(void __iomem *address, const char *filename)
{
	void __iomem              *pLCDCx =  address;
	void __iomem              *LCDC_Layerx;
	u32                       idx;

	/*global register*/
	DRM_INFO("[%s]Dump lcdc register value baseaddr : 0x%08x\n", filename, (u32)pLCDCx);
	
	DRM_INFO("pLCDCx CTRL[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_CTRL_OFFSET));
	DRM_INFO("pLCDCx PLANE_SIZE[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_PLANE_SIZE_OFFSET));
	DRM_INFO("pLCDCx UNDFLW_CFG[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_UNDFLW_CFG_OFFSET));
	DRM_INFO("pLCDCx DMA_MODE_CFG[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_DMA_MODE_CFG_OFFSET));
	DRM_INFO("pLCDCx SHW_RLD_CFG[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_SHW_RLD_CFG_OFFSET));
	DRM_INFO("pLCDCx BKG_COLOR[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_BKG_COLOR_OFFSET));
	DRM_INFO("pLCDCx DEBUG_STATUS[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_DEBUG_STATUS_OFFSET));

	//interrupt related register
	DRM_INFO("pLCDCx IRQ_EN[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_IRQ_EN_OFFSET));
	DRM_INFO("pLCDCx IRQ_STATUS[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_IRQ_STATUS_OFFSET));
	DRM_INFO("pLCDCx IRQ_RAW[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_IRQ_RAW_OFFSET));
	DRM_INFO("pLCDCx LINE_INT_POS[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_LINE_INT_POS_OFFSET));
	DRM_INFO("pLCDCx CUR_POS_STATUS[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_CUR_POS_STATUS_OFFSET));
	DRM_INFO("pLCDCx STATUS[0x%x] = 0x%08x\n", DUMP_REG(pLCDCx , LCDC_STATUS_OFFSET));
	DRM_INFO("\n");

	for (idx = 0; idx < LCDC_LAYER_MAX_NUM; idx++) {
		switch (idx) {
		case LCDC_LAYER_LAYER1:
			LCDC_Layerx = pLCDCx + LCDC_LAYER1_BASEADDRESS_OFFSET ;
			break;
		case LCDC_LAYER_LAYER2:
			LCDC_Layerx = pLCDCx + LCDC_LAYER2_BASEADDRESS_OFFSET ;
			break;
		case LCDC_LAYER_LAYER3:
		default:
			LCDC_Layerx = pLCDCx + LCDC_LAYER3_BASEADDRESS_OFFSET ;
			break;
		}
		/*Layerx related register*/
		DRM_INFO("pLCDCx->LCDC_LAYER%x_CTRL = 0x%08x\n", idx, readl(LCDC_Layerx + LCDC_LAYERx_CTRL_OFFSET));
		DRM_INFO("pLCDCx->LCDC_LAYER%x_BASE_ADDR = 0x%08x\n", idx, readl(LCDC_Layerx + LCDC_LAYERx_BASE_ADDR_OFFSET));
		DRM_INFO("pLCDCx->LCDC_LAYER%x_WIN_XPOS = 0x%08x\n", idx, readl(LCDC_Layerx + LCDC_LAYERx_WIN_XPOS_OFFSET));
		DRM_INFO("pLCDCx->LCDC_LAYER%x_WIN_YPOS = 0x%08x\n", idx, readl(LCDC_Layerx + LCDC_LAYERx_WIN_YPOS_OFFSET));
		DRM_INFO("pLCDCx->LCDC_LAYER%x_COLOR_KEY = 0x%08x\n", idx, readl(LCDC_Layerx+LCDC_LAYERx_COLOR_KEY_OFFSET));
		DRM_INFO("pLCDCx->LCDC_LAYER%x_ALPHA = 0x%08x\n", idx, readl(LCDC_Layerx+LCDC_LAYERx_ALPHA_OFFSET));
		DRM_INFO("\n");
	}
}

void ameba_lcdc_enable(void __iomem *address, u32 NewState)
{
	if (0 == NewState) {
		LCDC_Cmd(address, 0);
	} else {
		/*enable the LCDC*/
		LCDC_Cmd(address, 1);
		while (!LCDC_CheckLCDCReady(address));
	}
}

u32 ameba_lcdc_get_irqstatus(void __iomem *address)
{
	return LCDC_GetINTStatus(address);
}

void ameba_lcdc_clean_irqstatus(void __iomem *address, u32 irq)
{
	LCDC_ClearINT(address, irq);
}

//reset lcdc struct
void ameba_lcdc_reset_config(LCDC_InitTypeDef *LCDC_InitStruct,u16 width, u16 height,u32 bgcolor)
{
	if (NULL == LCDC_InitStruct) {
		return;
	}
	//LCDC_StructInit(LCDC_InitStruct);
	ameba_lcdc_init_config(LCDC_InitStruct, NULL, bgcolor,width, height);
}

void ameba_lcdc_set_planesize(LCDC_InitTypeDef *LCDC_InitStruct, u16 widht, u16 height)
{
	if (NULL == LCDC_InitStruct) {
		return;
	}
	LCDC_InitStruct->LCDC_ImageWidth = widht;
	LCDC_InitStruct->LCDC_ImageHeight = height;
}

void ameba_lcdc_set_background_color(LCDC_InitTypeDef *LCDC_InitStruct, u32 bgcolor)
{
	rgb888_t               bg_color;
	if (NULL == LCDC_InitStruct) {
		return;
	}

	*(u32 *)&bg_color = bgcolor;
	LCDC_InitStruct->LCDC_BgColorRed = bg_color.red;
	LCDC_InitStruct->LCDC_BgColorGreen = bg_color.green;
	LCDC_InitStruct->LCDC_BgColorBlue = bg_color.blue;
}

void ameba_lcdc_config_setvalid(void __iomem *address, LCDC_InitTypeDef *LCDC_InitStruct)
{
	if (NULL == LCDC_InitStruct) {
		return;
	}
	LCDC_Init(address, LCDC_InitStruct);
}

void ameba_lcdc_enable_SHW(void __iomem *address)
{
	LCDC_TrigerSHWReload(address);
}

//dma
void ameba_lcdc_dma_config_bustsize(void __iomem *address, u32 size)
{
	LCDC_DMAModeConfig(address, size);
}

void ameba_lcdc_dma_config_keeplastFrm(void __iomem *address, u32 DmaUnFlwMode, u32 showData)
{
	LCDC_DMAUnderFlowConfig(address, DmaUnFlwMode, showData);
}

void ameba_lcdc_dma_debug_config(void __iomem *address, u32 writeBackFlag, u32 ImgDestAddr)
{
	LCDC_DMADebugConfig(address, writeBackFlag, ImgDestAddr);
}

void ameba_lcdc_dma_get_unint_cnt(void __iomem *address, u32 *DmaUnIntCnt)
{
	LCDC_GetDmaUnINTCnt(address, DmaUnIntCnt);
}

//lcdc irq issue
void ameba_lcdc_irq_enable(void __iomem *address, u32 LCDC_IT, u32 NewState)
{
	LCDC_INTConfig(address, LCDC_IT, NewState);
}

void ameba_lcdc_irq_linepos(void __iomem *address, u32 LineNum)
{
	LCDC_LineINTPosConfig(address, LineNum);
}

void ameba_lcdc_irq_config(void __iomem *address, u32 intType, u32 NewState)
{
	LCDC_INTConfig(address, intType, NewState);
}

///layer
void ameba_lcdc_update_layer_reg(void __iomem *address, u8 layid, LCDC_LayerConfigTypeDef *EachLayer)
{
	LCDC_LayerConfig(address, layid, EachLayer);
}

void ameba_lcdc_layer_enable(LCDC_InitTypeDef *LCDC_InitStruct, u8 layid, u8 able)
{
	if (NULL == LCDC_InitStruct || layid >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[layid].LCDC_LayerEn = able;
}

void ameba_lcdc_layer_imgfmt(LCDC_InitTypeDef *LCDC_InitStruct, u8 layid, u32 fmt)
{
	if (NULL == LCDC_InitStruct || layid >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[layid].LCDC_LayerImgFormat = fmt;
}

void ameba_lcdc_layer_imgaddress(LCDC_InitTypeDef *LCDC_InitStruct, u8 layid, u32 imgaddress)
{
	if (NULL == LCDC_InitStruct || layid >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[layid].LCDC_LayerImgBaseAddr = imgaddress;
}

void ameba_lcdc_layer_pos(LCDC_InitTypeDef *LCDC_InitStruct, u8 idx, u16 start_x, u16 stop_x, u16 start_y, u16 stop_y)
{
	if (NULL == LCDC_InitStruct || idx >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[idx].LCDC_LayerHorizontalStart = start_x;/*1-based*/
	LCDC_InitStruct->layerx[idx].LCDC_LayerHorizontalStop = stop_x;
	LCDC_InitStruct->layerx[idx].LCDC_LayerVerticalStart = start_y;/*1-based*/
	LCDC_InitStruct->layerx[idx].LCDC_LayerVerticalStop = stop_y;
}

void ameba_lcdc_layer_colorkey_enable(LCDC_InitTypeDef *LCDC_InitStruct, u8 idx, u8 able)
{
	if (NULL == LCDC_InitStruct || idx >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[idx].LCDC_LayerColorKeyingEn = able;
}

void ameba_lcdc_layer_colorkey_value(LCDC_InitTypeDef *LCDC_InitStruct, u8 idx, u32 colorkey)
{
	if (NULL == LCDC_InitStruct || idx >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[idx].LCDC_LayerColorKeyingVal = colorkey;
}

void ameba_lcdc_layer_blend_value(LCDC_InitTypeDef *LCDC_InitStruct, u8 idx, u8 blend)
{
	if (NULL == LCDC_InitStruct || idx >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[idx].LCDC_LayerBlendConfig = blend;
}

void ameba_lcdc_layer_alpha_value(LCDC_InitTypeDef *LCDC_InitStruct, u8 idx, u8 alpha)
{
	if (NULL == LCDC_InitStruct || idx >= LCDC_LAYER_MAX_NUM) {
		return;
	}
	LCDC_InitStruct->layerx[idx].LCDC_LayerConstAlpha = alpha;
}

//underflow issue
void ameba_lcdc_dsi_underflow_reset(void __iomem *pmipi_reg)
{
	ameba_lcdc_dsi_intr_acpu_clr(pmipi_reg, ameba_lcdc_dsi_intr_acpu_get(pmipi_reg));
	ameba_lcdc_dsi_intr_config(pmipi_reg, 1, 0, 1);//enable acpu, disable scpu
	ameba_lcdc_dsi_mode_set(pmipi_reg, 0);
}

/*DMA underflow*/
/*
*	1) lcdc get the underflow interrupt
*	2) send msg to dsi : (i)enable acpu (ii)switch mode
*	3) dsi get the interrupt,  (i) send msg to lcdc (ii) switch mode
*	4) lcdc disable/enable lcdc
*/
