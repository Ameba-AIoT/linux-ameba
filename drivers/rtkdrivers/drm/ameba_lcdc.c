// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek LCDC support
*
*           - LCDC Initialization For DSI Vedio Mode
*           - write/read data through R-BUS to DSI Command Mode
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

//#include "ameba_soc.h"
#include "ameba_drm_base.h"
#include "ameba_lcdc.h"

/*-----------------------------------------------DMA functions------------------------------------------------*/

/**
* @brief  Configure LCDC DMA burst size .
* @param  LCDCx: where LCDCx can be LCDC.
* @param  BurstSize: DMA burst size; Unit 64 Bytes.
*                @arg LCDC_LAYER_BURSTSIZE_1X64BYTES:  Burst Trasnstions = 1;
*                @arg LCDC_LAYER_BURSTSIZE_2X64BYTES:  Burst Trasnstions = 2;
*                @arg LCDC_LAYER_BURSTSIZE_3X64BYTES:  Burst Trasnstions = 3;
*                @arg LCDC_LAYER_BURSTSIZE_4X64BYTES:  Burst Trasnstions = 4;
* @note     If BurstSize=1, the actual burstsize = 1x64 Bytes; if the BurstSize=2, the actual burstsize = 2x64 Bytes....
*                  The parameter "BurstSize" is not more than 4.
* @retval   None
*/
void LCDC_DMAModeConfig(void __iomem *LCDCx, u32 BurstSize)
{
	u32 Value32 ;
	/*check the parameters*/
	assert_param(IS_LCDC_LAYER_BURSTSIZE(BurstSize));

	/*fill the RDOTST field in register LCDC_DMA_MODE_CFG*/
	Value32 = readl(LCDCx+LCDC_DMA_MODE_CFG_OFFSET);
	Value32 &= ~LCDC_MASK_RD_OTSD;
	Value32 |= LCDC_RD_OTSD(BurstSize - 1);
	writel(Value32,LCDCx+LCDC_DMA_MODE_CFG_OFFSET);
}

/**
* @brief  Configure LCDC DMA under flow mode and under flow error data .
* @param  LCDCx: where LCDCx can be LCDC.
* @param  DmaUnFlwMode: DMA under flow mode, this parameter can be one of the following values:
*                @arg LCDC_DMAUNFW_OUTPUT_LASTDATA:  output last data
*                @arg LCDC_DMAUNFW_OUTPUT_ERRORDATA:  output error data
* @param  ErrorData: the output data when  DMA FIFO underflow occurred. When under flow mode is configured as
*                LCDC_DMAUNFW_OUTPUT_ERRORDATA, this parameter is needed, and otherwise it can be ignored.
* @retval   None
*/
void LCDC_DMAUnderFlowConfig(void __iomem *LCDCx, u32 DmaUnFlwMode, u32 ErrorData)
{
	/*read registers for configurartion*/
	u32 Value32 = readl(LCDCx+LCDC_UNDFLW_CFG_OFFSET);

	Value32 &= ~LCDC_BIT_DMA_UN_MODE;
	if (DmaUnFlwMode) {
		/*fill the DMAUNMODE field in register LCDC_UNDFLW_CFG*/
		Value32 |= LCDC_BIT_DMA_UN_MODE;

		/*fill the ERROUTDATA field in register LCDC_UNDFLW_CFG*/
		Value32 &= ~LCDC_MASK_ERROUT_DATA;
		Value32 |= LCDC_ERROUT_DATA(ErrorData);
	}

	/*write the value configured back to registers*/
	writel(Value32,LCDCx+LCDC_UNDFLW_CFG_OFFSET);
}

/**
* @brief  Configure LCDC DMA under flow mode and under flow error data .
* @param  LCDCx: where LCDCx can be LCDC.
* @param  DmaWriteBack: Secondary RGB output port enabled, the blended data of entire image will be written back to memory.
*         this parameter can be one of the following values:
*                @arg LCDC_DMA_OUT_ENABLE:  enable the debug function
*                @arg LCDC_DMA_OUT_DISABLE:  disable the debug function
* @param  ImgDestAddr: Image DMA destination address. (for debug)
* @retval   None
*/
void LCDC_DMADebugConfig(void __iomem *LCDCx, u32 DmaWriteBack, u32 ImgDestAddr)
{
	u32 Value32 = readl(LCDCx+LCDC_DMA_MODE_CFG_OFFSET);
	Value32 &= ~LCDC_BIT_LCD_DMA_OUT;
	writel(Value32,LCDCx+LCDC_DMA_MODE_CFG_OFFSET);
	
	if (DmaWriteBack) {
		/*fill the LCD_DMA_OUT field in register LCDC_DMA_MODE_CFG*/
		Value32 = readl(LCDCx+LCDC_DMA_MODE_CFG_OFFSET);
		Value32 |= LCDC_BIT_LCD_DMA_OUT  ;
		writel(Value32,LCDCx+LCDC_DMA_MODE_CFG_OFFSET);

		/*fill the IMG_DEST_ADDR field in register LCDC_SEC_DEST_ADDR*/
		writel(LCDC_IMG_DEST_ADDR(ImgDestAddr),LCDCx+LCDC_SEC_DEST_ADDR_OFFSET);
	}
}

/*---------------------------------------Interrupt functions--------------------------------------------*/

/**
  * @brief  Enables or disables the specified LCDC interrupts.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  LCDC_IT: specifies the LCDC interrupts sources to be enabled or disabled.
  *   This parameter can be any combination of the following values:
  *     @arg LCDC_BIT_DMA_UN_INTEN: DMA FIFO underflow interrupt
  *     @arg LCDC_BIT_LCD_FRD_INTEN: LCD refresh done interrupt
  *     @arg LCDC_BIT_LCD_LIN_INTEN: line interrupt
  *     @arg LCDC_BIT_FRM_START_INTEN: Frame Start interrupt
  * @param  NewState: new state of the specified LCDC interrupts.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void LCDC_INTConfig(void __iomem *LCDCx, u32 LCDC_IT, u32 NewState)
{
	u32 Value32 = readl(LCDCx+LCDC_IRQ_EN_OFFSET);
	if (NewState != 0) {
		/* Enable the selected LCDC interrupts */
		Value32 |= LCDC_IT;
	} else {
		/* Disable the selected LCDC interrupts */
		Value32 &= ~LCDC_IT;
	}
	writel(Value32,LCDCx+LCDC_IRQ_EN_OFFSET);
}

/**
  * @brief  Configure line interrupt position.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  LineNum: the line number.
  * @retval None
  */
void LCDC_LineINTPosConfig(void __iomem *LCDCx, u32 LineNum)
{
	/*fill the LINE_INT_POS field in register LCDC_LINE_INT_POS*/
	u32 Value32 = readl(LCDCx+LCDC_LINE_INT_POS_OFFSET);
	Value32 &= ~LCDC_MASK_LINE_INT_POS;
	Value32 |= LCDC_LINE_INT_POS(LineNum);
	writel(Value32,LCDCx+LCDC_LINE_INT_POS_OFFSET);
}

/**
  * @brief  Get lcdc interrupt status.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @retval interrupt status
  */
u32 LCDC_GetINTStatus(void __iomem *LCDCx)
{
	return readl(LCDCx+LCDC_IRQ_STATUS_OFFSET);
}

/**
  * @brief  Get LCDC Raw Interrupt Status.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @retval raw interrupt status
  */
u32 LCDC_GetRawINTStatus(void __iomem *LCDCx)
{
	return readl(LCDCx+LCDC_IRQ_RAW_OFFSET);
}

/**
  * @brief  Clears all of the LCDC interrupt pending bit.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @retval None
  */
void LCDC_ClearAllINT(void __iomem *LCDCx)
{
	/*write 1 to clear all interrupts*/
	writel(0xFFFFFFFF,LCDCx+LCDC_IRQ_STATUS_OFFSET);
}

/**
  * @brief  Clears the LCDC's interrupt pending bits.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  LCDC_IT: specifies the interrupt to be cleared.
  *   This parameter can be any combination of the following values:
  *     @arg LCDC_BIT_DMA_UN_INTS: DMA FIFO under flow interrupt
  *     @arg LCDC_BIT_LCD_FRD_INTS: refresh frame done interrupt
  *     @arg LCDC_BIT_LCD_LIN_INTS:line interrupt
  *     @arg LCDC_BIT_FRM_START_INTS: Frame Start interrupt
  * @retval None
  */
void LCDC_ClearINT(void __iomem *LCDCx, u32 LCDC_IT)
{
	/*check the parameters*/
	assert_param(IS_LCDC_CLEAR_IT(LCDC_IT));

	/*clear the specified interrupt*/
	writel(LCDC_IT,LCDCx+LCDC_IRQ_STATUS_OFFSET);
}

/**
  * @brief  Get the current position.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  pCurPosX: the current X position pointer.
  * @param  pCurPosY: the current Y position pointer.
  * @retval None
  */
void LCDC_GetCurPosStatus(void __iomem *LCDCx, u32 *pCurPosX, u32 *pCurPosY)
{
	/*Get the X position*/
	*pCurPosX = LCDC_GET_CUR_POS_X(readl(LCDCx+LCDC_CUR_POS_STATUS_OFFSET));

	/*Get the Y position*/
	*pCurPosY = LCDC_GET_CUR_POS_Y(readl(LCDCx+LCDC_CUR_POS_STATUS_OFFSET));
}

/**
  * @brief  Get the DMA FIFO under flow interrupt count.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  DmaUnIntCnt: the DMA under flow interrupt count pointer.
  * @retval None
  */
void LCDC_GetDmaUnINTCnt(void __iomem *LCDCx, u32 *DmaUnIntCnt)
{
	/*get the DMA under flow interrupt count*/
	*DmaUnIntCnt = (readl(LCDCx+LCDC_STATUS_OFFSET) & LCDC_MASK_DMA_UNINT_CNT);
}

/*------------------------------------------------Global  APIs------------------------------------------------*/

/**
  * @brief  Enables or disables the LCDC.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  NewState: new state of the LCDC. This parameter can be: ENABLE or DISABLE.
  * @note   Disable LCDC instantly. HW will reset LCDC internal states and disable LCDC
  *              then clear both this bit and LCDCEN.
  * @retval None
  */
void LCDC_Cmd(void __iomem *LCDCx, u32 NewState)
{
	u32 TempCtrl = readl(LCDCx+LCDC_CTRL_OFFSET);

	if (NewState != 0) {
		/* clear DISABLE bits, or it leads to enable LCDC unsuccessfully*/
		TempCtrl &= ~LCDC_BIT_DIS;

		/*set ENBALE bit*/
		TempCtrl |= LCDC_BIT_EN;
	} else {
		/*set DISABLE bit*/
		/*Note: LCDC just clear LCDC_BIT_EN, LCDC_BIT_DIS, and stop DMA Module*/
		TempCtrl |= LCDC_BIT_DIS;
	}

	writel(TempCtrl,LCDCx+LCDC_CTRL_OFFSET);
}

/**
  * @brief  Deinitializes the LCDC.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @note   Disable LCDC instantly, clear and disable all interrupts. which means LCDC_Cmd() does not clear interrupts
  * @retval None
  */
void LCDC_DeInit(void __iomem *LCDCx)
{
	/*disable LCDC instantly*/
	u32 Value32 = readl(LCDCx+LCDC_CTRL_OFFSET);
	Value32 |= LCDC_BIT_DIS;
	writel(Value32,LCDCx+LCDC_CTRL_OFFSET);

	/*clear all interrupts*/
	writel(0xFFFFFFFF,LCDCx+LCDC_IRQ_STATUS_OFFSET);

	/*disable all interrupts*/
	writel(0,LCDCx+LCDC_IRQ_EN_OFFSET);
}

/**
  * @brief  Check LCDC is ready to work with VO interface after LCDC is enable.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @retval status: Ready:1 or Not Ready: Others.
  */
u8 LCDC_CheckLCDCReady(void __iomem *LCDCx)
{
	/*get the LCDC Ready state of LCDC_STATUS*/
	if (readl(LCDCx+LCDC_STATUS_OFFSET) & LCDC_BIT_LCDCREADY) {
		return true;
	} else {
		return false;
	}
}

/**
  * @brief  Get the LCDC Plane Size
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  ImageWidth: The width of image (X-channel based), which means pixel number per line.
  * @param  ImageHeight: The height of image (Y-channel based)
  * @retval None
  */
void LCDC_SetPlaneSize(void __iomem *LCDCx, u32 ImageWidth, u32 ImageHeight)
{
	/*read registers for configurartion*/
	u32 Value32 = readl(LCDCx+LCDC_PLANE_SIZE_OFFSET);

	/*configure LCDC Plane Size*/
	Value32 &= ~(LCDC_MASK_IMAGEWIDTH | LCDC_MASK_IMAGEHEIGHT);
	Value32 |= LCDC_IMAGEWIDTH(ImageWidth);
	Value32 |= LCDC_IMAGEHEIGHT(ImageHeight);

	/*write the value configured back to registers*/
	writel(Value32,LCDCx+LCDC_PLANE_SIZE_OFFSET);
}


/**
  * @brief  Set the LCDC background color
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  RGBData: in the format of RGB888
  * @retval None
  */
void LCDC_SetBkgColor(void __iomem *LCDCx, u8 red_color, u8 green_color, u8 blue_color)
{
	/*read registers for configurartion*/
	u32 Value32 = readl(LCDCx+LCDC_BKG_COLOR_OFFSET);

	/*configure LCDC BackGround Color*/
	Value32 &= ~(LCDC_MASK_BKG_RED | LCDC_MASK_BKG_GREEN | LCDC_MASK_BKG_BLUE);
	Value32 |= LCDC_BKG_RED(red_color);
	Value32 |= LCDC_BKG_GREEN(green_color);
	Value32 |= LCDC_BKG_BLUE(blue_color);

	/*write the value configured back to registers*/
	writel(Value32,LCDCx+LCDC_BKG_COLOR_OFFSET);
}

/**
  * @brief  Triger Layer's shadow register reload to apply new configuration
  * @param  LCDCx: where LCDCx can be LCDC.
  * @note   The shadow registers read back the active values. Until the reload has been done, the 'old' value is read.
  * @retval None
  */
void LCDC_TrigerSHWReload(void __iomem *LCDCx)
{
	u32 Value32 = readl(LCDCx+LCDC_SHW_RLD_CFG_OFFSET);
	Value32 |= LCDC_BIT_VBR;
	writel(Value32,LCDCx+LCDC_SHW_RLD_CFG_OFFSET);
}

/**
  * @brief    Initializes the LCDC Layer according to the specified parameters in the EachLayer.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  EachLayer: pointer to a LCDC_EachLayerInitTypeDef structure that contains
  *              the configuration information for the specified LCDC layer.
  * @retval None
  */
void LCDC_LayerConfig(void __iomem *LCDCx, u8 LayerId, LCDC_LayerConfigTypeDef *EachLayer)
{
	/*variables for reading register*/
	u32 Ctrl;
	u32 BaseAddr;
	u32 WinXpos;
	u32 WinYpos;
	u32 ColorKey;
	u32 Alpha;
	void __iomem *LCDC_Layerx ;
		
	/*check the parameters*/
	assert_param(IS_LCDC_A_VALID_LAYER(LayerId));

	switch(LayerId)
	{
		case LCDC_LAYER_LAYER1:
			LCDC_Layerx = LCDCx + LCDC_LAYER1_BASEADDRESS_OFFSET ;
			break;
		case LCDC_LAYER_LAYER2:
			LCDC_Layerx = LCDCx + LCDC_LAYER2_BASEADDRESS_OFFSET ;
			break;
		case LCDC_LAYER_LAYER3:
		default:
			LCDC_Layerx = LCDCx + LCDC_LAYER3_BASEADDRESS_OFFSET ;
			break;
	}
	
	/*Note: the setting of this layer is not clear by LCDC_BIT_LAYERx_IMG_LAYER_EN bit*/
	if (0 == EachLayer->LCDC_LayerEn) {
		writel(0,LCDC_Layerx+LCDC_LAYERx_CTRL_OFFSET);
		return;
	}

	/*read registers for configurartion*/
	Ctrl = readl(LCDC_Layerx+LCDC_LAYERx_CTRL_OFFSET);
	BaseAddr = readl(LCDC_Layerx+LCDC_LAYERx_BASE_ADDR_OFFSET);
	WinXpos = readl(LCDC_Layerx+LCDC_LAYERx_WIN_XPOS_OFFSET);
	WinYpos = readl(LCDC_Layerx+LCDC_LAYERx_WIN_YPOS_OFFSET);
	ColorKey = readl(LCDC_Layerx+LCDC_LAYERx_COLOR_KEY_OFFSET);
	Alpha = readl(LCDC_Layerx+LCDC_LAYERx_ALPHA_OFFSET);

	/*configure layer ctrl register*/
	Ctrl &= ~(LCDC_BIT_LAYERx_IMG_LAYER_EN | LCDC_BIT_LAYERx_COLOR_KEYING_EN | LCDC_MASK_LAYERx_IMG_FORMAT);
	Ctrl |= LCDC_BIT_LAYERx_IMG_LAYER_EN;
	if (EachLayer->LCDC_LayerColorKeyingEn) {
		Ctrl |= LCDC_BIT_LAYERx_COLOR_KEYING_EN;
	}
	Ctrl |= LCDC_LAYERx_IMG_FORMAT(EachLayer->LCDC_LayerImgFormat);

	/*configure layer base address*/
	BaseAddr = LCDC_LAYERx_IMG_BASE_ADDR(EachLayer->LCDC_LayerImgBaseAddr);

	/*configure layer windows x position ctrl*/
	WinXpos &= ~(LCDC_MASK_LAYERx_WIN_X_START | LCDC_MASK_LAYERx_WIN_X_STOP);
	WinXpos |= LCDC_LAYERx_WIN_X_START(EachLayer->LCDC_LayerHorizontalStart);/*1-based*/
	WinXpos |= LCDC_LAYERx_WIN_X_STOP(EachLayer->LCDC_LayerHorizontalStop);

	/*configure layer windows x position ctrl*/
	WinYpos &= ~(LCDC_MASK_LAYERx_WIN_Y_START | LCDC_MASK_LAYERx_WIN_Y_STOP);
	WinYpos |= LCDC_LAYERx_WIN_Y_START(EachLayer->LCDC_LayerVerticalStart);/*1-based*/
	WinYpos |= LCDC_LAYERx_WIN_Y_STOP(EachLayer->LCDC_LayerVerticalStop);

	/*configure layer color keying data*/
	ColorKey &= ~LCDC_MASK_LAYERx_COLOR_KEY_VALUE;
	ColorKey |= LCDC_LAYERx_COLOR_KEY_VALUE(EachLayer->LCDC_LayerColorKeyingVal);

	/*configure layer alpha register*/
	Alpha &= ~(LCDC_MASK_LAYERx_BF1 | LCDC_MASK_LAYERx_CONSTA);
	Alpha |= LCDC_LAYERx_BF1(EachLayer->LCDC_LayerBlendConfig);
	Alpha |= LCDC_LAYERx_CONSTA(EachLayer->LCDC_LayerConstAlpha);

	/*write the value configured back to registers*/
	writel(Ctrl,LCDC_Layerx+LCDC_LAYERx_CTRL_OFFSET);
	writel(BaseAddr,LCDC_Layerx+LCDC_LAYERx_BASE_ADDR_OFFSET);
	writel(WinXpos,LCDC_Layerx+LCDC_LAYERx_WIN_XPOS_OFFSET);
	writel(WinYpos,LCDC_Layerx+LCDC_LAYERx_WIN_YPOS_OFFSET);
	writel(ColorKey,LCDC_Layerx+LCDC_LAYERx_COLOR_KEY_OFFSET);
	writel(Alpha,LCDC_Layerx+LCDC_LAYERx_ALPHA_OFFSET);
}

/**
  * @brief  Fills each LCDC_LayerConfigTypeDef member with its default value.
  * @param  LayerInit: pointer to an LCDC_LayerConfigTypeDef structure which will be initialized.
  * @retval   None
  */
void LCDC_StructInit(LCDC_InitTypeDef *LCDC_InitStruct)
{
	u8 idx;
	LCDC_InitStruct->LCDC_ImageWidth = 0;
	LCDC_InitStruct->LCDC_ImageHeight = 0;
	LCDC_InitStruct->LCDC_BgColorRed = 0;
	LCDC_InitStruct->LCDC_BgColorGreen = 0;
	LCDC_InitStruct->LCDC_BgColorBlue = 0;

	for (idx = 0; idx < LCDC_LAYER_MAX_NUM; idx++) {
		LCDC_InitStruct->layerx[idx].LCDC_LayerEn = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerColorKeyingEn = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerImgFormat = LCDC_LAYER_IMG_FORMAT_ARGB8888;
		LCDC_InitStruct->layerx[idx].LCDC_LayerImgBaseAddr = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerHorizontalStart = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerHorizontalStop = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerVerticalStart = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerVerticalStop = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerColorKeyingVal = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerBlendConfig = 0;
		LCDC_InitStruct->layerx[idx].LCDC_LayerConstAlpha = 0xFF;
	}
}

/**
  * @brief    Initializes the LCDC Layer according to the specified parameters in the LayerInit.
  * @param  LCDCx: where LCDCx can be LCDC.
  * @param  LayerInit: pointer to a LCDC_LayerConfigTypeDef structure that contains
  *              the configuration information for all LCDC layer.
  * @retval None
  */
void LCDC_Init(void __iomem *LCDCx, LCDC_InitTypeDef *LCDC_InitStruct)
{
	u8 idx;

	LCDC_SetPlaneSize(LCDCx, LCDC_InitStruct->LCDC_ImageWidth, LCDC_InitStruct->LCDC_ImageHeight);
	LCDC_SetBkgColor(LCDCx, LCDC_InitStruct->LCDC_BgColorRed, LCDC_InitStruct->LCDC_BgColorGreen, LCDC_InitStruct->LCDC_BgColorBlue);

	for (idx = 0; idx < LCDC_LAYER_MAX_NUM; idx++) {
		LCDC_LayerConfig(LCDCx, idx, &LCDC_InitStruct->layerx[idx]);
	}

	LCDC_TrigerSHWReload(LCDCx);
}

