// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DSI support
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
#define MIPI_DSI_RTNI       2//4
#define MIPI_DSI_HSA        4
#define MIPI_DSI_HBP        30
#define MIPI_DSI_HFP        30

#define MIPI_DSI_VSA        5
#define MIPI_DSI_VBP        20
#define MIPI_DSI_VFP        15

#define Mhz                 1000000UL
#define T_LPX               5
#define T_HS_PREP           6
#define T_HS_TRAIL          8
#define T_HS_EXIT           7
#define T_HS_ZERO           10

/*
*	mipi dsi apis
*/

/*handle for underflow issue*/
static u8 ameba_dsi_check_lcdc_ready(void __iomem *LCDCx)
{
	/*get the LCDC Ready state of LCDC_STATUS*/
	if (readl(LCDCx+LCDC_STATUS_OFFSET) & LCDC_BIT_LCDCREADY) {
		return true;
	} else {
		return false;
	}
}

static void ameba_dsi_set_lcdc_status(void __iomem *address, u32 NewState)
{
	if (0 == NewState) {
		ameba_dsi_set_lcdc_state(address, 0);
	} else {
		/*enable the LCDC*/
		ameba_dsi_set_lcdc_state(address, 1);
		while (!ameba_dsi_check_lcdc_ready(address));
	}
}

static void ameba_dsi_send_dcs_cmd(void __iomem *MIPIx, u8 cmd, u8 payload_len, u8 *para_list)
{
	u32 word0, word1, addr, idx;

	if (payload_len == 1) {
		MIPI_DSI_CMD_Send(MIPIx, cmd, para_list[0], 0);
		return;
	} else if (payload_len == 2) {
		MIPI_DSI_CMD_Send(MIPIx, cmd, para_list[0], para_list[1]);
		return;
	}

	/* the addr payload_len 1 ~ 8 is 0 */
	for (addr = 0; addr < (u32)(payload_len + 7) / 8; addr++) {
		idx = addr * 8;
		word0 = (para_list[idx + 3] << 24) + (para_list[idx + 2] << 16) + (para_list[idx + 1] << 8) + para_list[idx + 0];
		word1 = (para_list[idx + 7] << 24) + (para_list[idx + 6] << 16) + (para_list[idx + 5] << 8) + para_list[idx + 4];

		MIPI_DSI_CMD_LongPkt_MemQWordRW(MIPIx, addr, &word0, &word1, false);
	}
	MIPI_DSI_CMD_Send(MIPIx, cmd, payload_len, 0);
}

static void ameba_dsi_send_cmd(void __iomem *MIPIx, LCM_setting_table_t *table, u32 *initdone,u32 *rxcmd)
{
	static u8    send_cmd_idx_s = 0;
	u32          payload_len;
	u8           cmd, send_flag = false;
	u8           *para_list;

	while (1) {
		cmd = table[send_cmd_idx_s].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
			mdelay(table[send_cmd_idx_s].count);
			break;
		case REGFLAG_END_OF_TABLE:
			*initdone = 1;
			return;
		default:
			if (send_flag) {
				return;
			}
			para_list = table[send_cmd_idx_s].para_list;
			payload_len = table[send_cmd_idx_s].count;

			if(rxcmd) {
				if(MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM == cmd) {
					*rxcmd = 1;
				} else {
					*rxcmd = 0;
				}
			}
			ameba_dsi_send_dcs_cmd(MIPIx, cmd, payload_len, para_list);

			send_flag = true;
		}
		send_cmd_idx_s++;
	}
}

u32 ameba_dsi_reg_read(void __iomem *address)
{
	return readl(address);
}

void ameba_dsi_reg_write(void __iomem *address,u32 Value32)
{
	writel(Value32,address);
}

void ameba_dsi_set_lcdc_state(void __iomem *LCDCx, u32 NewState)
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

void ameba_dsi_lcdc_reenable(void __iomem* address)
{
	ameba_dsi_set_lcdc_status(address, 0);
	ameba_dsi_set_lcdc_status(address, 1);
}

void ameba_dsi_init_config(MIPI_InitTypeDef *MIPI_InitStruct,u32 width,u32 height,u32 framerate, u32 *mipi_ckd)
{
	u32 vtotal, htotal_bits, bit_per_pixel, overhead_cycles, overhead_bits, total_bits;
	u32 MIPI_HACT_g = width;
	u32 MIPI_VACT_g = height;
	u32 vo_freq, vo_totalx, vo_totaly, mipi_div;

	switch (MIPI_InitStruct->MIPI_VideoDataFormat) {
	case MIPI_VIDEO_DATA_FORMAT_RGB565:
		bit_per_pixel = 16;
		break;
	case MIPI_VIDEO_DATA_FORMAT_RGB666_PACKED:
		bit_per_pixel = 18;
		break;
	case MIPI_VIDEO_DATA_FORMAT_RGB666_LOOSELY:
	case MIPI_VIDEO_DATA_FORMAT_RGB888:
	default:
		bit_per_pixel = 24;
		break;
	}

	MIPI_InitStruct->MIPI_LaneNum = 2;
	MIPI_InitStruct->MIPI_FrameRate = framerate;

	MIPI_InitStruct->MIPI_HSA = MIPI_DSI_HSA * bit_per_pixel / 8 ;//- 10; /* here the unit is pixel but not us */
	if (MIPI_InitStruct->MIPI_VideoModeInterface == MIPI_VIDEO_NON_BURST_MODE_WITH_SYNC_PULSES) {
		MIPI_InitStruct->MIPI_HBP = MIPI_DSI_HBP * bit_per_pixel / 8 ;//- 10;
	} else {
		MIPI_InitStruct->MIPI_HBP = (MIPI_DSI_HSA + MIPI_DSI_HBP) * bit_per_pixel / 8 ;//-10 ;
	}

	MIPI_InitStruct->MIPI_HACT = MIPI_HACT_g;
	MIPI_InitStruct->MIPI_HFP = MIPI_DSI_HFP * bit_per_pixel / 8 ;//-12;

	MIPI_InitStruct->MIPI_VSA = MIPI_DSI_VSA;
	MIPI_InitStruct->MIPI_VBP = MIPI_DSI_VBP;
	MIPI_InitStruct->MIPI_VACT = MIPI_VACT_g;
	MIPI_InitStruct->MIPI_VFP = MIPI_DSI_VFP;

	/*DataLaneFreq * LaneNum = FrameRate * (VSA+VBP+VACT+VFP) * (HSA+HBP+HACT+HFP) * PixelFromat*/
	vtotal = MIPI_InitStruct->MIPI_VSA + MIPI_InitStruct->MIPI_VBP + MIPI_InitStruct->MIPI_VACT + MIPI_InitStruct->MIPI_VFP;
	htotal_bits = (MIPI_DSI_HSA + MIPI_DSI_HBP + MIPI_InitStruct->MIPI_HACT + MIPI_DSI_HFP) * bit_per_pixel;
	overhead_cycles = T_LPX + T_HS_PREP + T_HS_ZERO + T_HS_TRAIL + T_HS_EXIT;
	overhead_bits = overhead_cycles * MIPI_InitStruct->MIPI_LaneNum * 8;
	total_bits = htotal_bits + overhead_bits;

	MIPI_InitStruct->MIPI_VideDataLaneFreq = MIPI_InitStruct->MIPI_FrameRate * total_bits * vtotal / MIPI_InitStruct->MIPI_LaneNum / Mhz + 20;

	MIPI_InitStruct->MIPI_LineTime = (MIPI_InitStruct->MIPI_VideDataLaneFreq * Mhz) / 8 / MIPI_InitStruct->MIPI_FrameRate / vtotal;
	MIPI_InitStruct->MIPI_BllpLen = MIPI_InitStruct->MIPI_LineTime / MIPI_InitStruct->MIPI_LaneNum ;

	if (MIPI_DSI_HSA + MIPI_DSI_HBP + MIPI_HACT_g + MIPI_DSI_HFP < (512 + MIPI_DSI_RTNI * 16)) {
		DRM_ERROR("!!ERROR!!, LCM NOT SUPPORT\n");
	}

	if (MIPI_InitStruct->MIPI_LineTime * MIPI_InitStruct->MIPI_LaneNum < total_bits / 8) {
		DRM_ERROR("!!ERROR!!, LINE TIME TOO SHORT!\n");
	}

	//vo frequency , //output format is RGB888
	vo_totalx = MIPI_DSI_HSA + MIPI_DSI_HBP + MIPI_DSI_HFP + width;
	vo_totaly = MIPI_DSI_VSA + MIPI_DSI_VBP + MIPI_DSI_VFP + height;
	vo_freq = vo_totalx * vo_totaly * MIPI_InitStruct->MIPI_FrameRate * 24 / 24 / Mhz + 4;
	//assert_param(vo_freq < 67);

	//vo_freq = 80;
	mipi_div = 1000 / vo_freq - 1;//shall get NPPLL frequency 
	if(mipi_ckd)
	{
		*mipi_ckd = mipi_div;
	}

	DRM_DEBUG_DRIVER("DataLaneFreq(%d)LineTime(%d)vo_freq(%d[Mhz])mipi_ckd(%d)\n", 
		MIPI_InitStruct->MIPI_VideDataLaneFreq, MIPI_InitStruct->MIPI_LineTime, vo_freq, mipi_div);
}

void ameba_dsi_do_init(void __iomem *MIPIx, MIPI_InitTypeDef *MIPI_InitStruct, u32 *tx_done,u32 *rxcmd,void *init_table)
{
	u32 initdone = 0;
	*tx_done = true;

	if(NULL == init_table)
		return;

	MIPI_DSI_TO1_Set(MIPIx, 0, 0);
	MIPI_DSI_TO2_Set(MIPIx, 1, 0x7FFFFFFF);
	MIPI_DSI_TO3_Set(MIPIx, 0, 0);

	//enable ir ,get the cmd send finish issue
	MIPI_DSI_INT_Config(MIPIx, 0, 1, 0);
	MIPI_DSI_init(MIPIx, MIPI_InitStruct);

	while (1) {
		if (*tx_done) {
			*tx_done = 0;

			if (!(initdone)) {
				ameba_dsi_send_cmd(MIPIx, init_table, &initdone, rxcmd);
				mdelay(1);
			} else {
				break;
			}
		} else {
			mdelay(1);
		}
	}

	MIPI_DSI_INT_Config(MIPIx, 0, 0, 0);
}

void ameba_dsi_reg_dump(void __iomem *address)
{
	void __iomem *MIPIx = address;

	/*global register*/
	DRM_INFO( "Dump mipi register value baseaddr : 0x%08x\n", (u32)MIPIx);	
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_MAIN_CTRL_OFFSET,readl(MIPIx + MIPI_MAIN_CTRL_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_INTE_OFFSET,readl(MIPIx + MIPI_INTE_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_INTS_ACPU_OFFSET,readl(MIPIx + MIPI_INTS_ACPU_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_PAT_GEN_OFFSET,readl(MIPIx + MIPI_PAT_GEN_OFFSET));
	DRM_INFO( "\n");

	/*Dphy register*/
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_CLOCK_GEN_OFFSET,readl(MIPIx + MIPI_CLOCK_GEN_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_WATCHDOG_OFFSET,readl(MIPIx + MIPI_WATCHDOG_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_DF_OFFSET,readl(MIPIx + MIPI_DF_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_SSC2_OFFSET,readl(MIPIx + MIPI_SSC2_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_SSC3_OFFSET,readl(MIPIx + MIPI_SSC3_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_MPLL_OFFSET,readl(MIPIx + MIPI_MPLL_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_ESCAPE_TX_DATA_1_OFFSET,readl(MIPIx + MIPI_ESCAPE_TX_DATA_1_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_ESCAPE_TX_DATA_2_OFFSET,readl(MIPIx + MIPI_ESCAPE_TX_DATA_2_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_ESCAPE_TX_DATA_3_OFFSET,readl(MIPIx + MIPI_ESCAPE_TX_DATA_3_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_ESCAPE_TX_CLK_0_OFFSET,readl(MIPIx + MIPI_ESCAPE_TX_CLK_0_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_ESCAPE_TX_DATA_6_OFFSET,readl(MIPIx + MIPI_ESCAPE_TX_DATA_6_OFFSET));
	DRM_INFO( "\n");

	/*DSI register*/
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_TC0_OFFSET,readl(MIPIx + MIPI_TC0_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_TC1_OFFSET,readl(MIPIx + MIPI_TC1_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_TC2_OFFSET,readl(MIPIx + MIPI_TC2_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_TC3_OFFSET,readl(MIPIx + MIPI_TC3_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_TC4_OFFSET,readl(MIPIx + MIPI_TC4_OFFSET));
	DRM_INFO( "MIPIx[0x%x] = 0x%08x\n", MIPI_TC5_OFFSET,readl(MIPIx + MIPI_TC5_OFFSET));
	DRM_INFO( "\n");
}
