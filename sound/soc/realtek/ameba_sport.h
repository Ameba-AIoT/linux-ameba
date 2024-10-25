// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#ifndef _AMEBA_SPORT_H_
#define _AMEBA_SPORT_H_

#include <linux/types.h>
#include <linux/io.h>

#include "ameba_gdma.h"

/* AUTO_GEN_START */

/**************************************************************************//**
 * @defgroup SP_REG_MUX
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_REG_MUX                ((u32)0xFFFFFFFF << 0)          /*!<R/W 32'hFFFF_FFFF  Mux of register write with different base address of the same SPORT. "sp_reg_mux" can be set as different value with four different base address in one SPORT, but other registers share the same value with four different base address in one SPORT. */
#define SP_REG_MUX(x)                  ((u32)(((x) & 0xFFFFFFFF) << 0))
#define SP_GET_REG_MUX(x)              ((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_CTRL0
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_MCLK_SEL               ((u32)0x00000003 << 30)          /*!<R/W 2’b00  2’b00: MCLK output=dsp_src_clk/4 2’b01: MCLK output=dsp_src_clk/2 2’b10/2’b11: MCLK output=dsp_src_clk */
#define SP_MCLK_SEL(x)                 ((u32)(((x) & 0x00000003) << 30))
#define SP_GET_MCLK_SEL(x)             ((u32)(((x >> 30) & 0x00000003)))
#define SP_MASK_SEL_I2S_RX_CH          ((u32)0x00000003 << 28)          /*!<R/W 2’b00  2’b00: L/R 2’b01: R/L 2’b10: L/L 2’b11: R/R x ADC path */
#define SP_SEL_I2S_RX_CH(x)            ((u32)(((x) & 0x00000003) << 28))
#define SP_GET_SEL_I2S_RX_CH(x)        ((u32)(((x >> 28) & 0x00000003)))
#define SP_MASK_SEL_I2S_TX_CH          ((u32)0x00000003 << 26)          /*!<R/W 2’b00  2’b00: L/R 2’b01: R/L 2’b10: L/L 2’b11: R/R x DAC path */
#define SP_SEL_I2S_TX_CH(x)            ((u32)(((x) & 0x00000003) << 26))
#define SP_GET_SEL_I2S_TX_CH(x)        ((u32)(((x >> 26) & 0x00000003)))
#define SP_BIT_START_RX                ((u32)0x00000001 << 25)          /*!<R/W 1’b0  1’b0: RX is disabled 1’b1: RX is started */
#define SP_BIT_RX_DISABLE              ((u32)0x00000001 << 24)          /*!<R/W 1’b1  1’b1: SPORT RX is disabled. 1’b0: SPORT RX is enabled. */
#define SP_BIT_RX_LSB_FIRST_0          ((u32)0x00000001 << 23)          /*!<R/W 1’b0  1’b0: MSB first when RX 1’b1: LSB first */
#define SP_BIT_TX_LSB_FIRST_0          ((u32)0x00000001 << 22)          /*!<R/W 1’b0  1’b0: MSB first when TX 1’b1: LSB first */
#define SP_MASK_TDM_MODE_SEL_RX        ((u32)0x00000003 << 20)          /*!<R/W 2’b00  2’b00: Without TDM 2’b01: TDM4 2’b10: TDM6 2’b11: TDM8 */
#define SP_TDM_MODE_SEL_RX(x)          ((u32)(((x) & 0x00000003) << 20))
#define SP_GET_TDM_MODE_SEL_RX(x)      ((u32)(((x >> 20) & 0x00000003)))
#define SP_MASK_TDM_MODE_SEL_TX        ((u32)0x00000003 << 18)          /*!<R/W 2’b00  2’b00: Without TDM 2’b01: TDM4 2’b10: TDM6 2’b11: TDM8 */
#define SP_TDM_MODE_SEL_TX(x)          ((u32)(((x) & 0x00000003) << 18))
#define SP_GET_TDM_MODE_SEL_TX(x)      ((u32)(((x >> 18) & 0x00000003)))
#define SP_BIT_START_TX                ((u32)0x00000001 << 17)          /*!<R/W 1’b0  1’b0: TX is disabled. 1’b1: TX is started. */
#define SP_BIT_TX_DISABLE              ((u32)0x00000001 << 16)          /*!<R/W 1’b1  1’b1: SPORT TX is disabled. 1’b0: SPORT TX is enabled. */
#define SP_BIT_I2S_SELF_LPBK_EN        ((u32)0x00000001 << 15)          /*!<R/W 1’b0  1’b1: internal loopback mode is enabled */
#define SP_MASK_DATA_LEN_SEL_TX_0      ((u32)0x00000007 << 12)          /*!<R/W 2’b00  3’b000: 16 bits 3’b001: 20 bits 3’b010: 24 bits 3’b100: 32 bits */
#define SP_DATA_LEN_SEL_TX_0(x)        ((u32)(((x) & 0x00000007) << 12))
#define SP_GET_DATA_LEN_SEL_TX_0(x)    ((u32)(((x >> 12) & 0x00000007)))
#define SP_BIT_EN_I2S_MONO_TX_0        ((u32)0x00000001 << 11)          /*!<R/W 1’b0  1’b1: mono 1’b0: stereo */
#define SP_BIT_INV_I2S_SCLK            ((u32)0x00000001 << 10)          /*!<R/W 1’b0  1’b1: I2S/PCM bit clock is inverted */
#define SP_MASK_DATA_FORMAT_SEL_TX     ((u32)0x00000003 << 8)          /*!<R/W 2’b00  2’b00: I2S 2’b01: Left Justified 2’b10: PCM mode A 2’b11: PCM mode B */
#define SP_DATA_FORMAT_SEL_TX(x)       ((u32)(((x) & 0x00000003) << 8))
#define SP_GET_DATA_FORMAT_SEL_TX(x)   ((u32)(((x >> 8) & 0x00000003)))
#define SP_BIT_DSP_CTL_MODE            ((u32)0x00000001 << 7)          /*!<R/W 1’b0  1’b1: DSP and SPORT1 handshaking is enabled. 1’b0: GDMA and SPORT1 handshaking is enabled. */
#define SP_BIT_LOOPBACK                ((u32)0x00000001 << 6)          /*!<R/W 1’b0  1’b1: self loopback mode */
#define SP_BIT_WCLK_TX_INVERSE         ((u32)0x00000001 << 5)          /*!<R/W 1’h0  1’b1: I2S/PCM word clock is inverted for TX (SPK path) */
#define SP_BIT_SLAVE_DATA_SEL          ((u32)0x00000001 << 4)          /*!<R/W 1’b0  1’b1: To be an I2S or PCM slave (data path) */
#define SP_BIT_SLAVE_CLK_SEL           ((u32)0x00000001 << 3)          /*!<R/W 1’b0  1’b1: To be an I2S or PCM slave (CLK path) */
#define SP_BIT_RX_INV_I2S_SCLK         ((u32)0x00000001 << 2)          /*!<R/W 1’b0  1'b1: sclk to RX path (ADC path) is inverted */
#define SP_BIT_TX_INV_I2S_SCLK         ((u32)0x00000001 << 1)          /*!<R/W 1’b0  1'b1: sclk to TX path (DAC path) is inverted */
#define SP_BIT_RESET                   ((u32)0x00000001 << 0)          /*!<R/W 1’b0  1’b1: reset SPORT1 module, and remember to write “1” to reset and then write “0” to release from reset. */
/** @} */

/**************************************************************************//**
 * @defgroup SP_CTRL1
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_RX_FIFO_1_REG_1_EN      ((u32)0x00000001 << 31)          /*!<R/W 1’b0  1’b1: Enable last two channel of RX_FIFO_1. Only enable when "rx_fifo_1_reg_0_en" = 1. */
#define SP_BIT_RX_FIFO_1_REG_0_EN      ((u32)0x00000001 << 30)          /*!<R/W 1’b0  1’b1: Enable first two channel of RX_FIFO_1. Only enable when "rx_fifo_0_reg_0_en" = 1. */
#define SP_BIT_RX_FIFO_0_REG_1_EN      ((u32)0x00000001 << 29)          /*!<R/W 1’b0  1’b1: Enable last two channel of RX_FIFO_0. Only enable when "rx_fifo_0_reg_0_en" = 1. */
#define SP_BIT_RX_FIFO_0_REG_0_EN      ((u32)0x00000001 << 28)          /*!<R/W 1’b1  1’b1: Enable first two channel of RX_FIFO_0. Disable 0x0008[28] ~ Disable 0x0008[31] at the same time to reset RX FIFO. */
#define SP_BIT_TX_FIFO_1_REG_1_EN      ((u32)0x00000001 << 27)          /*!<R/W 1’b0  1’b1: Enable last two channel of TX_FIFO_1. Only enable when "tx_fifo_1_reg_0_en" = 1. */
#define SP_BIT_TX_FIFO_1_REG_0_EN      ((u32)0x00000001 << 26)          /*!<R/W 1’b0  1’b1: Enable first two channel of TX_FIFO_1. Only enable when "tx_fifo_0_reg_0_en" = 1. */
#define SP_BIT_TX_FIFO_0_REG_1_EN      ((u32)0x00000001 << 25)          /*!<R/W 1’b0  1’b1: Enable last two channel of TX_FIFO_0. Only enable when "tx_fifo_0_reg_0_en" = 1. */
#define SP_BIT_TX_FIFO_0_REG_0_EN      ((u32)0x00000001 << 24)          /*!<R/W 1’b1  1’b1: Enable first two channel of TX_FIFO_0. Disable 0x0008[24] ~ Disable 0x0008[27] at the same time to reset TX FIFO. */
#define SP_BIT_RX_SNK_LR_SWAP_0        ((u32)0x00000001 << 23)          /*!<R/W 1’b0  1’b1: swap L/R audio samples written to the sink memory of RX_FIFO_0; */
#define SP_BIT_RX_SNK_BYTE_SWAP_0      ((u32)0x00000001 << 22)          /*!<R/W 1’b0  1’b1: swap H/L bytes written to the sink memory of RX_FIFO_0; */
#define SP_BIT_TX_SRC_LR_SWAP_0        ((u32)0x00000001 << 21)          /*!<R/W 1’b0  1’b1: swap L/R audio samples read from the source memory of TX_FIFO_0; */
#define SP_BIT_TX_SRC_BYTE_SWAP_0      ((u32)0x00000001 << 20)          /*!<R/W 1’b0  1’b1: swap H/L bytes read from the source memory of TX_FIFO_0; */
#define SP_BIT_DIRECT_MODE_EN          ((u32)0x00000001 << 19)          /*!<R/W 1’b0  1’b1: WS(LRCK) and SCK(BCLK) are from other sport */
#define SP_MASK_DIRECT_SRC_SEL         ((u32)0x00000003 << 17)          /*!<R/W 2’h0  2’b00: ws and sck are from sport0. 2’b01: ws and sck are from sport1. 2’b10: ws and sck are from sport2. 2’b11: ws and sck are from sport3. */
#define SP_DIRECT_SRC_SEL(x)           ((u32)(((x) & 0x00000003) << 17))
#define SP_GET_DIRECT_SRC_SEL(x)       ((u32)(((x >> 17) & 0x00000003)))
#define SP_BIT_ERR_CNT_SAT_SET         ((u32)0x00000001 << 16)          /*!<R/W 1’b0  1’b1: saturation count (65534 --> 65535 --> 65535 ...) 1’b0: wrap count (65534 --> 65535 --> 0 --> 1 --> 2 ...) */
#define SP_MASK_SPORT_CLK_SEL          ((u32)0x00000003 << 14)          /*!<R/W 2’h0  2’b0x00: dsp_src_clk(BCLK*2) 2’b10: dsp_src_clk(BCLK*4)/2 2’b11: dsp_src_clk (BCLK*8)/4 */
#define SP_SPORT_CLK_SEL(x)            ((u32)(((x) & 0x00000003) << 14))
#define SP_GET_SPORT_CLK_SEL(x)        ((u32)(((x >> 14) & 0x00000003)))
#define SP_BIT_CLEAR_RX_ERR_CNT        ((u32)0x00000001 << 13)          /*!<R/W 1’b0  Write 1'b1 and then write 1'b0 to clear RX error counter */
#define SP_BIT_CLEAR_TX_ERR_CNT        ((u32)0x00000001 << 12)          /*!<R/W 1’b0  Write 1'b1 and then write 1'b0 to clear TX error counter */
#define SP_BIT_ENABLE_MCLK             ((u32)0x00000001 << 11)          /*!<R/W 1’b0  Enable mclk. */
#define SP_MASK_DEBUG_BUS_SEL          ((u32)0x00000007 << 8)          /*!<R/W 3’h0  3’b000: debug_bus_a 3’b001: debug_bus_b … 3’b111: debug_bus_h */
#define SP_DEBUG_BUS_SEL(x)            ((u32)(((x) & 0x00000007) << 8))
#define SP_GET_DEBUG_BUS_SEL(x)        ((u32)(((x >> 8) & 0x00000007)))
#define SP_BIT_WS_FORCE_VAL            ((u32)0x00000001 << 7)          /*!<R/W 1'h1  When "ws_force" = 1, ws_out_tx and ws_out_rx = "ws_force_val." */
#define SP_BIT_WS_FORCE                ((u32)0x00000001 << 6)          /*!<R/W 1'h0  1'b1: Make ws_out_tx and ws_out_rx = "ws_force_val." */
#define SP_BIT_BCLK_RESET              ((u32)0x00000001 << 5)          /*!<R/W 1’b0  1’b0: Enable bclk 1’b1: Disable and reset bclk */
#define SP_BIT_BCLK_PULL_ZERO          ((u32)0x00000001 << 4)          /*!<R/W 1’b0  Write 1’b1 to pull bclk to 0 smoothly. Write 1’b0 to reopen bclk. */
#define SP_BIT_MULTI_IO_EN_RX          ((u32)0x00000001 << 3)          /*!<R/W 1’b1  1’b1: Enable multi-IO of rx 1’b0: Disable multi-IO of rx; SPORT2 & SPORT3 only. */
#define SP_BIT_MULTI_IO_EN_TX          ((u32)0x00000001 << 2)          /*!<R/W 1’b1  1’b1: Enable multi-IO of tx 1’b0: Disable multi-IO of tx; SPORT2 & SPORT3 only. */
#define SP_BIT_TX_FIFO_FILL_ZERO       ((u32)0x00000001 << 1)          /*!<R/W 1’b0  X is the burst size of TX_FIFO_0. Y is the burst size of TX_FIFO_1. Fill TX_FIFO_0 with X zero data and fill TX_FIFO_1 with Y zero data. This control bit is "write 1 clear" type */
#define SP_BIT_RESET_SMOOTH            ((u32)0x00000001 << 0)          /*!<R/W 1'h0  1’b1: reset SPORT1 module with complete LRCK cycle. */
/** @} */

/**************************************************************************//**
 * @defgroup SP_INT_CTRL
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_INT_ENABLE_DSP_1       ((u32)0x000000FF << 24)          /*!<R/W 8’h0  int_enable_1[0]: for the interrupt of “sp_ready_to_tx_1” int_enable_1[1]: for the interrupt of “sp_ready_to_rx_1” int_enable_1[2]: for the interrupt of “tx_fifo_full_intr_1” int_enable_1[3]: for the interrupt of “rx_fifo_full_intr_1” int_enable_1[4]: for the interrupt of “tx_fifo_empty_intr_1” int_enable_1[5]: for the interrupt of “rx_fifo_empty_intr_1” int_enable_1[6]: for the interrupt of “tx_i2s_idle_1” */
#define SP_INT_ENABLE_DSP_1(x)         ((u32)(((x) & 0x000000FF) << 24))
#define SP_GET_INT_ENABLE_DSP_1(x)     ((u32)(((x >> 24) & 0x000000FF)))
#define SP_MASK_INT_ENABLE_DSP_0       ((u32)0x000000FF << 16)          /*!<R/W 8’h0  int_enable[0]: for the interrupt of “sp_ready_to_tx” int_enable[1]: for the interrupt of “sp_ready_to_rx” int_enable[2]: for the interrupt of “tx_fifo_full_intr” int_enable[3]: for the interrupt of “rx_fifo_full_intr” int_enable[4]: for the interrupt of “tx_fifo_empty_intr” int_enable[5]: for the interrupt of “rx_fifo_empty_intr” int_enable[6]: for the interrupt of “tx_i2s_idle” */
#define SP_INT_ENABLE_DSP_0(x)         ((u32)(((x) & 0x000000FF) << 16))
#define SP_GET_INT_ENABLE_DSP_0(x)     ((u32)(((x >> 16) & 0x000000FF)))
#define SP_MASK_INTR_CLR_1             ((u32)0x0000001F << 9)          /*!<R/W 5’h0  intr_clr_1[0]: for the interrupt of “tx_fifo_full_intr_1” intr_clr_1[1]: for the interrupt of “rx_fifo_full_intr_1” intr_clr_1[2]: for the interrupt of “tx_fifo_empty_intr_1” intr_clr_1[3]: for the interrupt of “rx_fifo_empty_intr_1” */
#define SP_INTR_CLR_1(x)               ((u32)(((x) & 0x0000001F) << 9))
#define SP_GET_INTR_CLR_1(x)           ((u32)(((x >> 9) & 0x0000001F)))
#define SP_BIT_RX_DSP_CLEAR_INT_1      ((u32)0x00000001 << 8)          /*!<R/W 1’b0  For DSP mode (bypass GDMA), F/W writes 1’b1 and then 1’b0 to clear RX interrupt. Note: RX interrupt is to indicate that DSP can get audio data from RX FIFO_1 */
#define SP_BIT_TX_DSP_CLEAR_INT_1      ((u32)0x00000001 << 7)          /*!<R/W 1’b0  For DSP mode (bypass GDMA), F/W writes 1’b1 and then 1’b0 to clear TX interrupt. Note: TX interrupt is to indicate that DSP can write audio data to TX FIFO_1 */
#define SP_MASK_INTR_CLR_0             ((u32)0x0000001F << 2)          /*!<R/W 5’h0  intr_clr[0]: for the interrupt of “tx_fifo_full_intr” intr_clr[1]: for the interrupt of “rx_fifo_full_intr” intr_clr[2]: for the interrupt of “tx_fifo_empty_intr” intr_clr[3]: for the interrupt of “rx_fifo_empty_intr” */
#define SP_INTR_CLR_0(x)               ((u32)(((x) & 0x0000001F) << 2))
#define SP_GET_INTR_CLR_0(x)           ((u32)(((x >> 2) & 0x0000001F)))
#define SP_BIT_RX_DSP_CLEAR_INT_0      ((u32)0x00000001 << 1)          /*!<R/W 1’b0  For DSP mode (bypass GDMA), F/W writes 1’b1 and then 1’b0 to clear RX interrupt. Note: RX interrupt is to indicate that DSP can get audio data from RX FIFO_0 */
#define SP_BIT_TX_DSP_CLEAR_INT_0      ((u32)0x00000001 << 0)          /*!<R/W 1’b0  For DSP mode (bypass GDMA), F/W writes 1’b1 and then 1’b0 to clear TX interrupt. Note: TX interrupt is to indicate that DSP can write audio data to TX FIFO_0 */
/** @} */

/**************************************************************************//**
 * @defgroup SP_TRX_COUNTER_STATUS
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_RESET_STATE             ((u32)0x00000001 << 31)          /*!<R 1’b0  1’b1: sp_reset is enable 1’b0: sp_reset is disable */
#define SP_MASK_RX_DEPTH_CNT_1         ((u32)0x0000003F << 24)          /*!<R 5’h0  RX FIFO_1 depth counter status (MIC path) */
#define SP_RX_DEPTH_CNT_1(x)           ((u32)(((x) & 0x0000003F) << 24))
#define SP_GET_RX_DEPTH_CNT_1(x)       ((u32)(((x >> 24) & 0x0000003F)))
#define SP_MASK_TX_DEPTH_CNT_1         ((u32)0x0000003F << 16)          /*!<R 5’h0  TX FIFO_1 depth counter status (SPK path) */
#define SP_TX_DEPTH_CNT_1(x)           ((u32)(((x) & 0x0000003F) << 16))
#define SP_GET_TX_DEPTH_CNT_1(x)       ((u32)(((x >> 16) & 0x0000003F)))
#define SP_MASK_RX_DEPTH_CNT_0         ((u32)0x0000003F << 8)          /*!<R 5’h0  RX FIFO_0 depth counter status (MIC path) */
#define SP_RX_DEPTH_CNT_0(x)           ((u32)(((x) & 0x0000003F) << 8))
#define SP_GET_RX_DEPTH_CNT_0(x)       ((u32)(((x >> 8) & 0x0000003F)))
#define SP_MASK_TX_DEPTH_CNT_0         ((u32)0x0000003F << 0)          /*!<R 5’h0  TX FIFO_0 depth counter status (SPK path) */
#define SP_TX_DEPTH_CNT_0(x)           ((u32)(((x) & 0x0000003F) << 0))
#define SP_GET_TX_DEPTH_CNT_0(x)       ((u32)(((x >> 0) & 0x0000003F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_ERR
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_RX_ERR_CNT             ((u32)0x0000FFFF << 16)          /*!<R 16’h0  RX error counter (MIC path) Note: This counter should always be zero if everything works well. */
#define SP_RX_ERR_CNT(x)               ((u32)(((x) & 0x0000FFFF) << 16))
#define SP_GET_RX_ERR_CNT(x)           ((u32)(((x >> 16) & 0x0000FFFF)))
#define SP_MASK_TX_ERR_CNT             ((u32)0x0000FFFF << 0)          /*!<R 16’h0  TX error counter (SPK path) Note: This counter should always be zero if everything works well. */
#define SP_TX_ERR_CNT(x)               ((u32)(((x) & 0x0000FFFF) << 0))
#define SP_GET_TX_ERR_CNT(x)           ((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_SR_TX_BCLK
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_TX_MI_NI_UPDATE         ((u32)0x00000001 << 31)          /*!<R/W 1’b0  1’b1: to update “mi” and “ni” to get the new clock rate. This bit will be reset automatically when the update is done */
#define SP_MASK_TX_NI                  ((u32)0x00007FFF << 16)          /*!<R/W 15’d48  BCLK = 40MHz*(ni/mi) For example: BCLK=3.072MHz=40MHz*(48/625) */
#define SP_TX_NI(x)                    ((u32)(((x) & 0x00007FFF) << 16))
#define SP_GET_TX_NI(x)                ((u32)(((x >> 16) & 0x00007FFF)))
#define SP_MASK_TX_MI                  ((u32)0x0000FFFF << 0)          /*!<R/W 16’d625   */
#define SP_TX_MI(x)                    ((u32)(((x) & 0x0000FFFF) << 0))
#define SP_GET_TX_MI(x)                ((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_TX_LRCLK
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_RX_BCLK_DIV_RATIO      ((u32)0x000000FF << 24)          /*!<R/W 8’h3F  RX bclk even-bit integer divider. Used in “mode_40mhz” set as 1’b1. (rx_bclk_div_ratio + 1) is the number of “sck_out” cycles within a “ws_out_rx” cycle (1/fs). Default of (rx_bclk_div_ratio + 1) is 64. Set as 64 – 1 = 63. Only odd number supported. Maximum is 255. */
#define SP_RX_BCLK_DIV_RATIO(x)        ((u32)(((x) & 0x000000FF) << 24))
#define SP_GET_RX_BCLK_DIV_RATIO(x)    ((u32)(((x >> 24) & 0x000000FF)))
#define SP_MASK_TX_BCLK_DIV_RATIO      ((u32)0x000000FF << 16)          /*!<R/W 8’h3F  TX bclk even-bit integer divider. Used in “mode_40mhz” set as 1’b1. (tx_bclk_div_ratio + 1) is the number of “sck_out” cycles within a “ws_out_tx” cycle (1/fs). Default of (tx_bclk_div_ratio + 1) is 64. Set as 64 – 1 = 63. Only odd number supported. Maximum is 255. */
#define SP_TX_BCLK_DIV_RATIO(x)        ((u32)(((x) & 0x000000FF) << 16))
#define SP_GET_TX_BCLK_DIV_RATIO(x)    ((u32)(((x >> 16) & 0x000000FF)))
#define SP_MASK_RXDMA_BUSRTSIZE        ((u32)0x0000003F << 8)          /*!<R/W 6’h10  RX DMA burst size */
#define SP_RXDMA_BUSRTSIZE(x)          ((u32)(((x) & 0x0000003F) << 8))
#define SP_GET_RXDMA_BUSRTSIZE(x)      ((u32)(((x >> 8) & 0x0000003F)))
#define SP_MASK_TXDMA_BURSTSIZE        ((u32)0x0000003F << 0)          /*!<R/W 6’h10  TX DMA burst size */
#define SP_TXDMA_BURSTSIZE(x)          ((u32)(((x) & 0x0000003F) << 0))
#define SP_GET_TXDMA_BURSTSIZE(x)      ((u32)(((x >> 0) & 0x0000003F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_FIFO_CTRL
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_RX_FIFO_EMPTY_0         ((u32)0x00000001 << 31)          /*!<R 1’b1  1: RX FIFO_0 is empty */
#define SP_BIT_TX_FIFO_EMPTY_0         ((u32)0x00000001 << 30)          /*!<R 1’b1  1: TX FIFO_0 is empty */
#define SP_BIT_RX_FIFO_FULL_0          ((u32)0x00000001 << 29)          /*!<R 1’b0  1: RX FIFO_0 is full */
#define SP_BIT_TX_FIFO_FULL_0          ((u32)0x00000001 << 28)          /*!<R 1’b0  1: TX FIFO_0 is full */
#define SP_BIT_RX_FIFO_EMPTY_1         ((u32)0x00000001 << 27)          /*!<R 1’b1  1: RX FIFO_1 is empty */
#define SP_BIT_TX_FIFO_EMPTY_1         ((u32)0x00000001 << 26)          /*!<R 1’b1  1: TX FIFO_1 is empty */
#define SP_BIT_RX_FIFO_FULL_1          ((u32)0x00000001 << 25)          /*!<R 1’b0  1: RX FIFO_1 is full */
#define SP_BIT_TX_FIFO_FULL_1          ((u32)0x00000001 << 24)          /*!<R 1’b0  1: TX FIFO_1 is full */
#define SP_BIT_TX_I2S_IDLE_1           ((u32)0x00000001 << 13)          /*!<R 1’b0  1: TX is working but FIFO_1 is empty. */
#define SP_BIT_RX_FIFO_EMPTY_INTR_1    ((u32)0x00000001 << 12)          /*!<R 1’b0  1: RX FIFO_1 is empty (MIC path) */
#define SP_BIT_TX_FIFO_EMPTY_INTR_1    ((u32)0x00000001 << 11)          /*!<R 1’b0  1: TX FIFO_1 is empty (SPK path) */
#define SP_BIT_RX_FIFO_FULL_INTR_1     ((u32)0x00000001 << 10)          /*!<R 1’b0  1: RX FIFO_1 is full (MIC path) */
#define SP_BIT_TX_FIFO_FULL_INTR_1     ((u32)0x00000001 << 9)          /*!<R 1’b0  1: TX FIFO_1 is full (SPK path) */
#define SP_BIT_READY_TO_RX_1           ((u32)0x00000001 << 8)          /*!<R 1’b0  1: It is ready to receive data (MIC path) */
#define SP_BIT_READY_TO_TX_1           ((u32)0x00000001 << 7)          /*!<R 1’b0  1: It is ready to send data out (SPK path) */
#define SP_BIT_TX_I2S_IDLE_0           ((u32)0x00000001 << 6)          /*!<R 1’b0  1: TX is working but FIFO_0 is empty. */
#define SP_BIT_RX_FIFO_EMPTY_INTR_0    ((u32)0x00000001 << 5)          /*!<R 1’b0  1: RX FIFO_0 is empty (MIC path) */
#define SP_BIT_TX_FIFO_EMPTY_INTR_0    ((u32)0x00000001 << 4)          /*!<R 1’b0  1: TX FIFO_0 is empty (SPK path) */
#define SP_BIT_RX_FIFO_FULL_INTR_0     ((u32)0x00000001 << 3)          /*!<R 1’b0  1: RX FIFO_0 is full (MIC path) */
#define SP_BIT_TX_FIFO_FULL_INTR_0     ((u32)0x00000001 << 2)          /*!<R 1’b0  1: TX FIFO_0 is full (SPK path) */
#define SP_BIT_READY_TO_RX_0           ((u32)0x00000001 << 1)          /*!<R 1’b0  1: It is ready to receive data (MIC path) */
#define SP_BIT_READY_TO_TX_0           ((u32)0x00000001 << 0)          /*!<R 1’b0  1: It is ready to send data out (SPK path) */
/** @} */

/**************************************************************************//**
 * @defgroup SP_FORMAT
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_TRX_SAME_CH_LEN         ((u32)0x00000001 << 31)          /*!<R/W 1’h0  1: TX (SPK path) and RX (MIC path) have the same channel length. */
#define SP_MASK_CH_LEN_SEL_RX          ((u32)0x00000007 << 28)          /*!<R/W 3’b100  3’b000: 16 bits 3’b001: 20 bits 3’b010: 24 bits 3’b011: 8 bits 3’b100: 32 bits */
#define SP_CH_LEN_SEL_RX(x)            ((u32)(((x) & 0x00000007) << 28))
#define SP_GET_CH_LEN_SEL_RX(x)        ((u32)(((x >> 28) & 0x00000007)))
#define SP_MASK_CH_LEN_SEL_TX          ((u32)0x00000007 << 24)          /*!<R/W 3’b100  3’b000: 16 bits 3’b001: 20 bits 3’b010: 24 bits 3’b011: 8 bits 3’b100: 32 bits */
#define SP_CH_LEN_SEL_TX(x)            ((u32)(((x) & 0x00000007) << 24))
#define SP_GET_CH_LEN_SEL_TX(x)        ((u32)(((x >> 24) & 0x00000007)))
#define SP_BIT_RX_IDEAL_LEN_EN         ((u32)0x00000001 << 23)          /*!<R/W 1’h0  Function enable of rx_ideal_len. */
#define SP_MASK_RX_IDEAL_LEN           ((u32)0x00000007 << 20)          /*!<R/W 3’b000  sd_in can be received 1 ~ 8 ( = rx_ideal_len + 1 ) BCLK cycle latter. */
#define SP_RX_IDEAL_LEN(x)             ((u32)(((x) & 0x00000007) << 20))
#define SP_GET_RX_IDEAL_LEN(x)         ((u32)(((x >> 20) & 0x00000007)))
#define SP_BIT_TX_IDEAL_LEN_EN         ((u32)0x00000001 << 19)          /*!<R/W 1’h0  Function enable of tx_ideal_len. PCMA SDO will be delayed 1 LRCK. */
#define SP_MASK_TX_IDEAL_LEN           ((u32)0x00000007 << 16)          /*!<R/W 3’b000  sd_out can be sent 1 ~ 8 ( = tx_ideal_len + 1 ) BCLK cycle earlier. */
#define SP_TX_IDEAL_LEN(x)             ((u32)(((x) & 0x00000007) << 16))
#define SP_GET_TX_IDEAL_LEN(x)         ((u32)(((x >> 16) & 0x00000007)))
#define SP_MASK_DATA_LEN_SEL_RX_0      ((u32)0x00000007 << 12)          /*!<R/W 3’h0  Data length of MIC path and it is valid if “trx_same_length” == 1’b0. 3’b000: 16 bits 3’b001: 20 bits 3’b010: 24 bits 3’b011: 8 bits 3’b100: 32 bits */
#define SP_DATA_LEN_SEL_RX_0(x)        ((u32)(((x) & 0x00000007) << 12))
#define SP_GET_DATA_LEN_SEL_RX_0(x)    ((u32)(((x >> 12) & 0x00000007)))
#define SP_BIT_EN_I2S_MONO_RX_0        ((u32)0x00000001 << 11)          /*!<R/W 1’h0  Channel format of MIC path and it is valid if “trx_same_ch” == 1’b0. 1: mono 0: stereo */
#define SP_BIT_TRX_SAME_LRC            ((u32)0x00000001 << 10)          /*!<R/W 1’h0  1: “ws_out_rx” is as same as “ws_out_tx” */
#define SP_MASK_DATA_FORMAT_SEL_RX     ((u32)0x00000003 << 8)          /*!<R/W 2’h0  Data format of MIC path and it is valid if “trx_same_fs” == 1’b0. 2’b00: I2S 2’b01: Left Justified 2’b10: PCM mode A 2’b11: PCM mode B */
#define SP_DATA_FORMAT_SEL_RX(x)       ((u32)(((x) & 0x00000003) << 8))
#define SP_GET_DATA_FORMAT_SEL_RX(x)   ((u32)(((x >> 8) & 0x00000003)))
#define SP_BIT_FIXED_BCLK              ((u32)0x00000001 << 7)          /*!<R/W 1’h0  1: Refer to the description of “fixed_bclk_sel” 0: BCLK = dsp_clk/2 when “mode_40mhz = 0” */
#define SP_BIT_FIXED_BCLK_SEL          ((u32)0x00000001 << 6)          /*!<R/W 1’h0  0: BCLK is fixed at dsp_src_clk/4 1: BCLK is fixed at dsp_src_clk/2 */
#define SP_BIT_WCLK_RX_INVERSE         ((u32)0x00000001 << 5)          /*!<R/W 1’h0  1: invert the phase of “ws_out_rx” which is also called as “ADCLRC” */
#define SP_BIT_SCK_OUT_INVERSE         ((u32)0x00000001 << 3)          /*!<R/W 1’h0  1: invert the phase of “sck_out” */
#define SP_BIT_TRX_SAME_LENGTH         ((u32)0x00000001 << 2)          /*!<R/W 1’h1  1: TX (SPK path) and RX (MIC path) have the same data length. Both are either 16 or 24 bits */
#define SP_BIT_TRX_SAME_CH             ((u32)0x00000001 << 1)          /*!<R/W 1’h1  1: TX (SPK path) and RX (MIC path) have the same channel setting. Both are either stereo or mono */
#define SP_BIT_TRX_SAME_FS             ((u32)0x00000001 << 0)          /*!<R/W 1’h1  1: TX (SPK path) and RX (MIC path) have the same sampling rate */
/** @} */

/**************************************************************************//**
 * @defgroup SP_RX_BCLK
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_RX_MI_NI_UPDATE         ((u32)0x00000001 << 31)          /*!<R/W 1’b0  1’b1: to update “mi” and “ni” to get the new clock rate. This bit will be reset automatically when the update is done */
#define SP_MASK_RX_NI                  ((u32)0x00007FFF << 16)          /*!<R/W 15’d48  BCLK = 40MHz*(ni/mi) For example: BCLK=3.072MHz=40MHz*(48/625) */
#define SP_RX_NI(x)                    ((u32)(((x) & 0x00007FFF) << 16))
#define SP_GET_RX_NI(x)                ((u32)(((x >> 16) & 0x00007FFF)))
#define SP_MASK_RX_MI                  ((u32)0x0000FFFF << 0)          /*!<R/W 16’d625   */
#define SP_RX_MI(x)                    ((u32)(((x) & 0x0000FFFF) << 0))
#define SP_GET_RX_MI(x)                ((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_RX_LRCLK
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_CLR_TX_SPORT_RDY        ((u32)0x00000001 << 31)          /*!<W1C 1’h0  0x001: clear tx_sport_interrupt signal This control bit is "write 1 clear" type For read, the read data is from clr_tx_sport_rdy */
#define SP_BIT_EN_TX_SPORT_INTERRUPT   ((u32)0x00000001 << 30)          /*!<R/W 1’h0  Enable tx_sport_interrupt */
#define SP_BIT_EN_FS_PHASE_LATCH       ((u32)0x00000001 << 29)          /*!<R/W 1’h0  0x001: Latch the value of tx_fs_phase_rpt, rx_fs_phase_rpt, tx_sport_sounter, rx_sport_sounter at the same time. This control bit is "write 1 clear" type */
#define SP_MASK_TX_SPORT_COMPARE_VAL   ((u32)0x07FFFFFF << 0)          /*!<R/W 14’d64  X = (tx_sport_compare_val). When counter equal X. Sport will send tx_sport_interrupt to DSP. FW should take care X={32~8191} */
#define SP_TX_SPORT_COMPARE_VAL(x)     ((u32)(((x) & 0x07FFFFFF) << 0))
#define SP_GET_TX_SPORT_COMPARE_VAL(x) ((u32)(((x >> 0) & 0x07FFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_DSP_COUNTER
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_TX_SPORT_COUNTER       ((u32)0x07FFFFFF << 5)          /*!<R 27’h0  For DSP read instant tx sport counter value, counter down */
#define SP_TX_SPORT_COUNTER(x)         ((u32)(((x) & 0x07FFFFFF) << 5))
#define SP_GET_TX_SPORT_COUNTER(x)     ((u32)(((x >> 5) & 0x07FFFFFF)))
#define SP_MASK_TX_FS_PHASE_RPT        ((u32)0x0000001F << 0)          /*!<R 5’h0  Report tx phase */
#define SP_TX_FS_PHASE_RPT(x)          ((u32)(((x) & 0x0000001F) << 0))
#define SP_GET_TX_FS_PHASE_RPT(x)      ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_DIRECT_CTRL0
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_TX_CH7_DATA_SEL        ((u32)0x0000000F << 28)          /*!<R/W 4’h7  4’h0: tx_fifo_0_reg_0_l 4’h1: tx_fifo_0_reg_0_r 4’h2: tx_fifo_0_reg_1_l 4’h3: tx_fifo_0_reg_1_r 4’h4: tx_fifo_1_reg_0_l 4’h5: tx_fifo_1_reg_0_r 4’h6: tx_fifo_1_reg_1_l 4’h7: tx_fifo_1_reg_1_r 4’h8: direct_reg_7 */
#define SP_TX_CH7_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 28))
#define SP_GET_TX_CH7_DATA_SEL(x)      ((u32)(((x >> 28) & 0x0000000F)))
#define SP_MASK_TX_CH6_DATA_SEL        ((u32)0x0000000F << 24)          /*!<R/W 4’h6  (Ibid.) 4’h8: direct_reg_6 */
#define SP_TX_CH6_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 24))
#define SP_GET_TX_CH6_DATA_SEL(x)      ((u32)(((x >> 24) & 0x0000000F)))
#define SP_MASK_TX_CH5_DATA_SEL        ((u32)0x0000000F << 20)          /*!<R/W 4’h5  (Ibid.) 4’h8: direct_reg_5 */
#define SP_TX_CH5_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 20))
#define SP_GET_TX_CH5_DATA_SEL(x)      ((u32)(((x >> 20) & 0x0000000F)))
#define SP_MASK_TX_CH4_DATA_SEL        ((u32)0x0000000F << 16)          /*!<R/W 4’h4  (Ibid.) 4’h8: direct_reg_4 */
#define SP_TX_CH4_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 16))
#define SP_GET_TX_CH4_DATA_SEL(x)      ((u32)(((x >> 16) & 0x0000000F)))
#define SP_MASK_TX_CH3_DATA_SEL        ((u32)0x0000000F << 12)          /*!<R/W 4’h3  (Ibid.) 4’h8: direct_reg_3 */
#define SP_TX_CH3_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 12))
#define SP_GET_TX_CH3_DATA_SEL(x)      ((u32)(((x >> 12) & 0x0000000F)))
#define SP_MASK_TX_CH2_DATA_SEL        ((u32)0x0000000F << 8)          /*!<R/W 4’h2  (Ibid.) 4’h8: direct_reg_2 */
#define SP_TX_CH2_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 8))
#define SP_GET_TX_CH2_DATA_SEL(x)      ((u32)(((x >> 8) & 0x0000000F)))
#define SP_MASK_TX_CH1_DATA_SEL        ((u32)0x0000000F << 4)          /*!<R/W 4’h1  (Ibid.) 4’h8: direct_reg_1 */
#define SP_TX_CH1_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 4))
#define SP_GET_TX_CH1_DATA_SEL(x)      ((u32)(((x >> 4) & 0x0000000F)))
#define SP_MASK_TX_CH0_DATA_SEL        ((u32)0x0000000F << 0)          /*!<R/W 4’h0  (Ibid.) 4’h8: direct_reg_0 */
#define SP_TX_CH0_DATA_SEL(x)          ((u32)(((x) & 0x0000000F) << 0))
#define SP_GET_TX_CH0_DATA_SEL(x)      ((u32)(((x >> 0) & 0x0000000F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_FIFO_IRQ
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_RX_LSB_FIRST_1          ((u32)0x00000001 << 31)          /*!<R/W 1’b0  1’b0: MSB first when TX 1’b1: LSB first */
#define SP_BIT_TX_LSB_FIRST_1          ((u32)0x00000001 << 30)          /*!<R/W 1’b0  1’b0: MSB first when TX 1’b1: LSB first */
#define SP_BIT_RX_SNK_LR_SWAP_1        ((u32)0x00000001 << 29)          /*!<R/W 1’b0  1’b1: swap L/R audio samples written to the sink memory of RX_FIFO_1 */
#define SP_BIT_RX_SNK_BYTE_SWAP_1      ((u32)0x00000001 << 28)          /*!<R/W 1’b0  1’b1: swap H/L bytes written to the sink memory of RX_FIFO_1 */
#define SP_BIT_TX_SRC_LR_SWAP_1        ((u32)0x00000001 << 27)          /*!<R/W 1’b0  1’b1: swap L/R audio samples read from the source memory of TX_FIFO_1 */
#define SP_BIT_TX_SRC_BYTE_SWAP_1      ((u32)0x00000001 << 26)          /*!<R/W 1’b0  1’b1: swap H/L bytes read from the source memory of TX_FIFO_1 */
#define SP_MASK_INT_ENABLE_MCU_1       ((u32)0x000000FF << 8)          /*!<R/W 8’h0  int_enable[0]: for the interrupt of “sp_ready_to_tx” int_enable[1]: for the interrupt of “sp_ready_to_rx” int_enable[2]: for the interrupt of “tx_fifo_full_intr” int_enable[3]: for the interrupt of “rx_fifo_full_intr” int_enable[4]: for the interrupt of “tx_fifo_empty_intr” int_enable[5]: for the interrupt of “rx_fifo_empty_intr” int_enable[6]: for the interrupt of “tx_i2s_idle” */
#define SP_INT_ENABLE_MCU_1(x)         ((u32)(((x) & 0x000000FF) << 8))
#define SP_GET_INT_ENABLE_MCU_1(x)     ((u32)(((x >> 8) & 0x000000FF)))
#define SP_MASK_INT_ENABLE_MCU_0       ((u32)0x000000FF << 0)          /*!<R/W 8’h0  int_enable[0]: for the interrupt of “sp_ready_to_tx” int_enable[1]: for the interrupt of “sp_ready_to_rx” int_enable[2]: for the interrupt of “tx_fifo_full_intr” int_enable[3]: for the interrupt of “rx_fifo_full_intr” int_enable[4]: for the interrupt of “tx_fifo_empty_intr” int_enable[5]: for the interrupt of “rx_fifo_empty_intr” int_enable[6]: for the interrupt of “tx_i2s_idle” */
#define SP_INT_ENABLE_MCU_0(x)         ((u32)(((x) & 0x000000FF) << 0))
#define SP_GET_INT_ENABLE_MCU_0(x)     ((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_DIRECT_CTRL1
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_EN_I2S_MONO_RX_1        ((u32)0x00000001 << 31)          /*!<R/W 1’h0  Channel format of MIC path and it is valid if “trx_same_ch” == 1’b0. 1’b1: mono 1’b0: stereo */
#define SP_MASK_DATA_LEN_SEL_RX_1      ((u32)0x00000007 << 28)          /*!<R/W 3’h0  Data length of MIC path and it is valid if “trx_same_length” == 1’b0. 3’b000: 16 bits 3’b001: 20 bits 3’b010: 24 bits 3’b011: 8 bits 3’b100: 32 bits */
#define SP_DATA_LEN_SEL_RX_1(x)        ((u32)(((x) & 0x00000007) << 28))
#define SP_GET_DATA_LEN_SEL_RX_1(x)    ((u32)(((x >> 28) & 0x00000007)))
#define SP_BIT_EN_I2S_MONO_TX_1        ((u32)0x00000001 << 27)          /*!<R/W 1’h0  1’b1: mono 1’b0: stereo */
#define SP_MASK_DATA_LEN_SEL_TX_1      ((u32)0x00000007 << 24)          /*!<R/W 3’h0  3’b000: 16 bits 3’b001: 20 bits 3’b010: 24 bits 3’b011: 8 bits 3’b100: 32 bits */
#define SP_DATA_LEN_SEL_TX_1(x)        ((u32)(((x) & 0x00000007) << 24))
#define SP_GET_DATA_LEN_SEL_TX_1(x)    ((u32)(((x >> 24) & 0x00000007)))
#define SP_BIT_DIRECT_REG_3_EN         ((u32)0x00000001 << 23)          /*!<R/W 1’h0  1’b1: Enable direct_reg_3. */
#define SP_MASK_DIRECT_REG_3_SEL       ((u32)0x0000001F << 18)          /*!<R/W 5’h0  5’h0: spa_direct_in_0 5’h1: spa_direct_in_1 5’h2: spa_direct_in_2 5’h3: spa_direct_in_3 5’h4: spa_direct_in_4 5’h5: spa_direct_in_5 5’h6: spa_direct_in_6 5’h7: spa_direct_in_7 5’h8: spb_direct_in_0 5’h9: spb_direct_in_1 5’ha: spb_direct_in_2 5’hb: spb_direct_in_3 5’hc: spb_direct_in_4 5’hd: spb_direct_in_5 5’he: spb_direct_in_6 5’hf: spb_direct_in_7 5’h10: spc_direct_in_0 5’h11: spc_direct_in_1 5’h12: spc_direct_in_2 5’h13: spc_direct_in_3 5’h14: spc_direct_in_4 5’h15: spc_direct_in_5 5’h16: spc_direct_in_6 5’h17: spc_direct_in_7 5’h18: sp0_direct_in_tx_fifo_0_reg_0_l 5’h19: sp0_direct_in_tx_fifo_0_reg_0_r 5’h1a: sp0_direct_in_tx_fifo_0_reg_1_l 5’h1b: sp0_direct_in_tx_fifo_0_reg_1_r 5’h1c: TDM_RX_CH3 SPORT0: a = 1, b = 2, c = 3 SPORT1: a = 0, b = 2, c = 3 SPORT2: a = 0, b = 1, c = 3 SPORT3: a = 0, b = 1, c = 2 */
#define SP_DIRECT_REG_3_SEL(x)         ((u32)(((x) & 0x0000001F) << 18))
#define SP_GET_DIRECT_REG_3_SEL(x)     ((u32)(((x >> 18) & 0x0000001F)))
#define SP_BIT_DIRECT_REG_2_EN         ((u32)0x00000001 << 17)          /*!<R/W 1’h0  1’b1: Enable direct_reg_2. */
#define SP_MASK_DIRECT_REG_2_SEL       ((u32)0x0000001F << 12)          /*!<R/W 5’h0  (Ibid.) 5’h1c: TDM_RX_CH2 */
#define SP_DIRECT_REG_2_SEL(x)         ((u32)(((x) & 0x0000001F) << 12))
#define SP_GET_DIRECT_REG_2_SEL(x)     ((u32)(((x >> 12) & 0x0000001F)))
#define SP_BIT_DIRECT_REG_1_EN         ((u32)0x00000001 << 11)          /*!<R/W 1’h0  1’b1: Enable direct_reg_1. */
#define SP_MASK_DIRECT_REG_1_SEL       ((u32)0x0000001F << 6)          /*!<R/W 5’h0  (Ibid.) 5’h1c: TDM_RX_CH1 */
#define SP_DIRECT_REG_1_SEL(x)         ((u32)(((x) & 0x0000001F) << 6))
#define SP_GET_DIRECT_REG_1_SEL(x)     ((u32)(((x >> 6) & 0x0000001F)))
#define SP_BIT_DIRECT_REG_0_EN         ((u32)0x00000001 << 5)          /*!<R/W 1’h0  1’b1: Enable direct_reg_0. */
#define SP_MASK_DIRECT_REG_0_SEL       ((u32)0x0000001F << 0)          /*!<R/W 5’h0  (Ibid.) 5’h1c: TDM_RX_CH0 */
#define SP_DIRECT_REG_0_SEL(x)         ((u32)(((x) & 0x0000001F) << 0))
#define SP_GET_DIRECT_REG_0_SEL(x)     ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_DIRECT_CTRL2
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_DIRECT_OUT_7_EN         ((u32)0x00000001 << 31)          /*!<R/W 1’h0  Enable sp_direct_out_7. */
#define SP_BIT_DIRECT_OUT_6_EN         ((u32)0x00000001 << 30)          /*!<R/W 1’h0  Enable sp_direct_out_6. */
#define SP_BIT_DIRECT_OUT_5_EN         ((u32)0x00000001 << 29)          /*!<R/W 1’h0  Enable sp_direct_out_5. */
#define SP_BIT_DIRECT_OUT_4_EN         ((u32)0x00000001 << 28)          /*!<R/W 1’h0  Enable sp_direct_out_4. */
#define SP_BIT_DIRECT_OUT_3_EN         ((u32)0x00000001 << 27)          /*!<R/W 1’h0  Enable sp_direct_out_3. */
#define SP_BIT_DIRECT_OUT_2_EN         ((u32)0x00000001 << 26)          /*!<R/W 1’h0  Enable sp_direct_out_2. */
#define SP_BIT_DIRECT_OUT_1_EN         ((u32)0x00000001 << 25)          /*!<R/W 1’h0  Enable sp_direct_out_1. */
#define SP_BIT_DIRECT_OUT_0_EN         ((u32)0x00000001 << 24)          /*!<R/W 1’h0  Enable sp_direct_out_0. */
#define SP_BIT_DIRECT_REG_7_EN         ((u32)0x00000001 << 23)          /*!<R/W 1’h0  1’b1: Enable direct_reg_7. */
#define SP_MASK_DIRECT_REG_7_SEL       ((u32)0x0000001F << 18)          /*!<R/W 5’h0  5’h0: spa_direct_in_0 5’h1: spa_direct_in_1 5’h2: spa_direct_in_2 5’h3: spa_direct_in_3 5’h4: spa_direct_in_4 5’h5: spa_direct_in_5 5’h6: spa_direct_in_6 5’h7: spa_direct_in_7 5’h8: spb_direct_in_0 5’h9: spb_direct_in_1 5’ha: spb_direct_in_2 5’hb: spb_direct_in_3 5’hc: spb_direct_in_4 5’hd: spb_direct_in_5 5’he: spb_direct_in_6 5’hf: spb_direct_in_7 5’h10: spc_direct_in_0 5’h11: spc_direct_in_1 5’h12: spc_direct_in_2 5’h13: spc_direct_in_3 5’h14: spc_direct_in_4 5’h15: spc_direct_in_5 5’h16: spc_direct_in_6 5’h17: spc_direct_in_7 5’h18: sp0_direct_in_tx_fifo_0_reg_0_l 5’h19: sp0_direct_in_tx_fifo_0_reg_0_r 5’h1a: sp0_direct_in_tx_fifo_0_reg_1_l 5’h1b: sp0_direct_in_tx_fifo_0_reg_1_r 5’h1c: TDM_RX_CH7 SPORT0: a = 1, b = 2, c = 3 SPORT1: a = 0, b = 2, c = 3 SPORT2: a = 0, b = 1, c = 3 SPORT3: a = 0, b = 1, c = 2 */
#define SP_DIRECT_REG_7_SEL(x)         ((u32)(((x) & 0x0000001F) << 18))
#define SP_GET_DIRECT_REG_7_SEL(x)     ((u32)(((x >> 18) & 0x0000001F)))
#define SP_BIT_DIRECT_REG_6_EN         ((u32)0x00000001 << 17)          /*!<R/W 1’h0  1’b1: Enable direct_reg_6. */
#define SP_MASK_DIRECT_REG_6_SEL       ((u32)0x0000001F << 12)          /*!<R/W 5’h0  (Ibid.) 5’h1c: TDM_RX_CH6 */
#define SP_DIRECT_REG_6_SEL(x)         ((u32)(((x) & 0x0000001F) << 12))
#define SP_GET_DIRECT_REG_6_SEL(x)     ((u32)(((x >> 12) & 0x0000001F)))
#define SP_BIT_DIRECT_REG_5_EN         ((u32)0x00000001 << 11)          /*!<R/W 1’h0  1’b1: Enable direct_reg_5. */
#define SP_MASK_DIRECT_REG_5_SEL       ((u32)0x0000001F << 6)          /*!<R/W 5’h0  (Ibid.) 5’h1c: TDM_RX_CH5 */
#define SP_DIRECT_REG_5_SEL(x)         ((u32)(((x) & 0x0000001F) << 6))
#define SP_GET_DIRECT_REG_5_SEL(x)     ((u32)(((x >> 6) & 0x0000001F)))
#define SP_BIT_DIRECT_REG_4_EN         ((u32)0x00000001 << 5)          /*!<R/W 1’h0  1’b1: Enable direct_reg_4. */
#define SP_MASK_DIRECT_REG_4_SEL       ((u32)0x0000001F << 0)          /*!<R/W 5’h0  (Ibid.) 5’h1c: TDM_RX_CH4 */
#define SP_DIRECT_REG_4_SEL(x)         ((u32)(((x) & 0x0000001F) << 0))
#define SP_GET_DIRECT_REG_4_SEL(x)     ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_DIRECT_CTRL3
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_RX_FIFO_0_REG_1_R_SEL  ((u32)0x0000001F << 24)          /*!<R/W 5’h3  5’d0: RX_CH0_data_out (MIC path) 5’d1: RX_CH1_data_out (MIC path) 5’d2: RX_CH2_data_out (MIC path) 5’d3: RX_CH3_data_out (MIC path) 5’d4: RX_CH4_data_out (MIC path) 5’d5: RX_CH5_data_out (MIC path) 5’d6: RX_CH6_data_out (MIC path) 5’d7: RX_CH7_data_out (MIC path) 5’d8: spa_direct_in_0 5’d9: spa_direct_in_1 5’d10: spa_direct_in_2 5’d11: spa_direct_in_3 5’d12: spa_direct_in_4 5’d13: spa_direct_in_5 5’d14: spa_direct_in_6 5’d15: spa_direct_in_7 5’d16: spb_direct_in_0 5’d17: spb_direct_in_1 5’d18: spb_direct_in_2 5’d19: spb_direct_in_3 5’d20: spb_direct_in_4 5’d21: spb_direct_in_5 5’d22: spb_direct_in_6 5’d23: spb_direct_in_7 5’d24: spc_direct_in_0 5’d25: spc_direct_in_1 5’d26: spc_direct_in_2 5’d27: spc_direct_in_3 5’d28: spc_direct_in_4 5’d29: spc_direct_in_5 5’d30: spc_direct_in_6 5’d31: spc_direct_in_7 SPORT0: a = 1, b = 2, c = 3 SPORT1: a = 0, b = 2, c = 3 SPORT2: a = 0, b = 1, c = 3 SPORT3: a = 0, b = 1, c = 2 */
#define SP_RX_FIFO_0_REG_1_R_SEL(x)    ((u32)(((x) & 0x0000001F) << 24))
#define SP_GET_RX_FIFO_0_REG_1_R_SEL(x) ((u32)(((x >> 24) & 0x0000001F)))
#define SP_MASK_RX_FIFO_0_REG_1_L_SEL  ((u32)0x0000001F << 16)          /*!<R/W 5’h2  (Ibid.) */
#define SP_RX_FIFO_0_REG_1_L_SEL(x)    ((u32)(((x) & 0x0000001F) << 16))
#define SP_GET_RX_FIFO_0_REG_1_L_SEL(x) ((u32)(((x >> 16) & 0x0000001F)))
#define SP_MASK_RX_FIFO_0_REG_0_R_SEL  ((u32)0x0000001F << 8)          /*!<R/W 5’h1  (Ibid.) */
#define SP_RX_FIFO_0_REG_0_R_SEL(x)    ((u32)(((x) & 0x0000001F) << 8))
#define SP_GET_RX_FIFO_0_REG_0_R_SEL(x) ((u32)(((x >> 8) & 0x0000001F)))
#define SP_MASK_RX_FIFO_0_REG_0_L_SEL  ((u32)0x0000001F << 0)          /*!<R/W 5’h0  (Ibid.) */
#define SP_RX_FIFO_0_REG_0_L_SEL(x)    ((u32)(((x) & 0x0000001F) << 0))
#define SP_GET_RX_FIFO_0_REG_0_L_SEL(x) ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_DIRECT_CTRL4
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_RX_FIFO_1_REG_1_R_SEL  ((u32)0x0000001F << 24)          /*!<R/W 5’h7  5’d0: RX_CH0_data_out (MIC path) 5’d1: RX_CH1_data_out (MIC path) 5’d2: RX_CH2_data_out (MIC path) 5’d3: RX_CH3_data_out (MIC path) 5’d4: RX_CH4_data_out (MIC path) 5’d5: RX_CH5_data_out (MIC path) 5’d6: RX_CH6_data_out (MIC path) 5’d7: RX_CH7_data_out (MIC path) 5’d8: spa_direct_in_0 5’d9: spa_direct_in_1 5’d10: spa_direct_in_2 5’d11: spa_direct_in_3 5’d12: spa_direct_in_4 5’d13: spa_direct_in_5 5’d14: spa_direct_in_6 5’d15: spa_direct_in_7 5’d16: spb_direct_in_0 5’d17: spb_direct_in_1 5’d18: spb_direct_in_2 5’d19: spb_direct_in_3 5’d20: spb_direct_in_4 5’d21: spb_direct_in_5 5’d22: spb_direct_in_6 5’d23: spb_direct_in_7 5’d24: spc_direct_in_0 5’d25: spc_direct_in_1 5’d26: spc_direct_in_2 5’d27: spc_direct_in_3 5’d28: spc_direct_in_4 5’d29: spc_direct_in_5 5’d30: spc_direct_in_6 5’d31: spc_direct_in_7 SPORT0: a = 1, b = 2, c = 3 SPORT1: a = 0, b = 2, c = 3 SPORT2: a = 0, b = 1, c = 3 SPORT3: a = 0, b = 1, c = 2 */
#define SP_RX_FIFO_1_REG_1_R_SEL(x)    ((u32)(((x) & 0x0000001F) << 24))
#define SP_GET_RX_FIFO_1_REG_1_R_SEL(x) ((u32)(((x >> 24) & 0x0000001F)))
#define SP_MASK_RX_FIFO_1_REG_1_L_SEL  ((u32)0x0000001F << 16)          /*!<R/W 5’h6  (Ibid.) */
#define SP_RX_FIFO_1_REG_1_L_SEL(x)    ((u32)(((x) & 0x0000001F) << 16))
#define SP_GET_RX_FIFO_1_REG_1_L_SEL(x) ((u32)(((x >> 16) & 0x0000001F)))
#define SP_MASK_RX_FIFO_1_REG_0_R_SEL  ((u32)0x0000001F << 8)          /*!<R/W 5’h5  (Ibid.) */
#define SP_RX_FIFO_1_REG_0_R_SEL(x)    ((u32)(((x) & 0x0000001F) << 8))
#define SP_GET_RX_FIFO_1_REG_0_R_SEL(x) ((u32)(((x >> 8) & 0x0000001F)))
#define SP_MASK_RX_FIFO_1_REG_0_L_SEL  ((u32)0x0000001F << 0)          /*!<R/W 5’h4  (Ibid.) */
#define SP_RX_FIFO_1_REG_0_L_SEL(x)    ((u32)(((x) & 0x0000001F) << 0))
#define SP_GET_RX_FIFO_1_REG_0_L_SEL(x) ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_RX_COUNTER1
 * @brief
 * @{
 *****************************************************************************/
#define SP_BIT_CLR_RX_SPORT_RDY        ((u32)0x00000001 << 31)          /*!<W1C 1’h0  X = (tx_sport_compare_val). When counter equal X. Sport will send tx_sport_interrupt to DSP. FW should take care X={32~8191} */
#define SP_BIT_EN_RX_SPORT_INTERRUPT   ((u32)0x00000001 << 30)          /*!<R/W 1’h0  Enable rx sport interrupt. */
#define SP_MASK_RX_SPORT_COMPARE_VAL   ((u32)0x07FFFFFF << 0)          /*!<R/W 14’d64  X = (rx_sport_compare_val). When counter equal X. Sport will send rx_sport_interrupt to DSP. FW should take care X={32~8191} */
#define SP_RX_SPORT_COMPARE_VAL(x)     ((u32)(((x) & 0x07FFFFFF) << 0))
#define SP_GET_RX_SPORT_COMPARE_VAL(x) ((u32)(((x >> 0) & 0x07FFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_RX_COUNTER2
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_RX_SPORT_COUNTER       ((u32)0x07FFFFFF << 5)          /*!<R 27’h0  For DSP read instant rx sport counter value, counter down */
#define SP_RX_SPORT_COUNTER(x)         ((u32)(((x) & 0x07FFFFFF) << 5))
#define SP_GET_RX_SPORT_COUNTER(x)     ((u32)(((x >> 5) & 0x07FFFFFF)))
#define SP_MASK_RX_FS_PHASE_RPT        ((u32)0x0000001F << 0)          /*!<R 5’h0  Report RX phase */
#define SP_RX_FS_PHASE_RPT(x)          ((u32)(((x) & 0x0000001F) << 0))
#define SP_GET_RX_FS_PHASE_RPT(x)      ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_TX_FIFO_0_WR_ADDR
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_TX_FIFO_0_WR_ADDR      ((u32)0xFFFFFFFF << 0)          /*!<W 32'b0  TX_FIFO_0_WR_ADDR */
#define SP_TX_FIFO_0_WR_ADDR(x)        ((u32)(((x) & 0xFFFFFFFF) << 0))
#define SP_GET_TX_FIFO_0_WR_ADDR(x)    ((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_RX_FIFO_0_RD_ADDR
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_RX_FIFO_0_RD_ADDR      ((u32)0xFFFFFFFF << 0)          /*!<R 32'b0  RX_FIFO_0_RD_ADDR */
#define SP_RX_FIFO_0_RD_ADDR(x)        ((u32)(((x) & 0xFFFFFFFF) << 0))
#define SP_GET_RX_FIFO_0_RD_ADDR(x)    ((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_TX_FIFO_1_WR_ADDR
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_TX_FIFO_1_WR_ADDR      ((u32)0xFFFFFFFF << 0)          /*!<W 32'b0  TX_FIFO_1_WR_ADDR */
#define SP_TX_FIFO_1_WR_ADDR(x)        ((u32)(((x) & 0xFFFFFFFF) << 0))
#define SP_GET_TX_FIFO_1_WR_ADDR(x)    ((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SP_RX_FIFO_1_RD_ADDR
 * @brief
 * @{
 *****************************************************************************/
#define SP_MASK_RX_FIFO_1_RD_ADDR      ((u32)0xFFFFFFFF << 0)          /*!<R 32'b0  RX_FIFO_1_RD_ADDR */
#define SP_RX_FIFO_1_RD_ADDR(x)        ((u32)(((x) & 0xFFFFFFFF) << 0))
#define SP_GET_RX_FIFO_1_RD_ADDR(x)    ((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

#define REG_SP_REG_MUX                 0x0000
#define REG_SP_CTRL0                   0x0004
#define REG_SP_CTRL1                   0x0008
#define REG_SP_INT_CTRL                0x000C
#define REG_RSVD0                      0x0010
#define REG_SP_TRX_COUNTER_STATUS      0x0014
#define REG_SP_ERR                     0x0018
#define REG_SP_SR_TX_BCLK              0x001C
#define REG_SP_TX_LRCLK                0X0020
#define REG_SP_FIFO_CTRL               0X0024
#define REG_SP_FORMAT                  0X0028
#define REG_SP_RX_BCLK                 0X002C
#define REG_SP_RX_LRCLK                0X0030
#define REG_SP_DSP_COUNTER             0X0034
#define REG_RSVD1                      0x0038
#define REG_SP_DIRECT_CTRL0            0X003C
#define REG_RSVD2                      0x0040
#define REG_SP_FIFO_IRQ                0X0044
#define REG_SP_DIRECT_CTRL1            0X0048
#define REG_SP_DIRECT_CTRL2            0X004C
#define REG_RSVD3                      0x0050
#define REG_SP_DIRECT_CTRL3            0X0054
#define REG_SP_DIRECT_CTRL4            0X0058
#define REG_SP_RX_COUNTER1             0X005C
#define REG_SP_RX_COUNTER2             0X0060
#define REG_SP_TX_FIFO_0_WR_ADDR       0X0800
#define REG_SP_RX_FIFO_0_RD_ADDR       0X0880
#define REG_SP_TX_FIFO_1_WR_ADDR       0X0900
#define REG_SP_RX_FIFO_1_RD_ADDR       0X0980



/**************************************************************************//**
 * @defgroup AMEBA_SPORT
 * @{
 * @brief AMEBA_SPORT Register Declaration
 *****************************************************************************/
typedef struct {
	volatile uint32_t SP_REG_MUX;                             /*!<  Register,  Address offset: 0x0000 */
	volatile uint32_t SP_CTRL0;                               /*!<  Register,  Address offset: 0x0004 */
	volatile uint32_t SP_CTRL1;                               /*!<  Register,  Address offset: 0x0008 */
	volatile uint32_t SP_INT_CTRL;                            /*!<  Register,  Address offset: 0x000C */
	volatile uint32_t RSVD0;                                  /*!<  Reserved,  Address offset:0x10 */
	volatile const  uint32_t SP_TRX_COUNTER_STATUS;                  /*!<  Register,  Address offset: 0x0014 */
	volatile const  uint32_t SP_ERR;                                 /*!<  Register,  Address offset: 0x0018 */
	volatile uint32_t SP_SR_TX_BCLK;                          /*!<  Register,  Address offset: 0x001C */
	volatile uint32_t SP_TX_LRCLK;                            /*!<  Register,  Address offset: 0X0020 */
	volatile const  uint32_t SP_FIFO_CTRL;                           /*!<  Register,  Address offset: 0X0024 */
	volatile uint32_t SP_FORMAT;                              /*!<  Register,  Address offset: 0X0028 */
	volatile uint32_t SP_RX_BCLK;                             /*!<  Register,  Address offset: 0X002C */
	volatile uint32_t SP_RX_LRCLK;                            /*!<  Register,  Address offset: 0X0030 */
	volatile const  uint32_t SP_DSP_COUNTER;                         /*!<  Register,  Address offset: 0X0034 */
	volatile uint32_t RSVD1;                                  /*!<  Reserved,  Address offset:0x38 */
	volatile uint32_t SP_DIRECT_CTRL0;                        /*!<  Register,  Address offset: 0X003C */
	volatile uint32_t RSVD2;                                  /*!<  Reserved,  Address offset:0x40 */
	volatile uint32_t SP_FIFO_IRQ;                            /*!<  Register,  Address offset: 0X0044 */
	volatile uint32_t SP_DIRECT_CTRL1;                        /*!<  Register,  Address offset: 0X0048 */
	volatile uint32_t SP_DIRECT_CTRL2;                        /*!<  Register,  Address offset: 0X004C */
	volatile uint32_t RSVD3;                                  /*!<  Reserved,  Address offset:0x50 */
	volatile uint32_t SP_DIRECT_CTRL3;                        /*!<  Register,  Address offset: 0X0054 */
	volatile uint32_t SP_DIRECT_CTRL4;                        /*!<  Register,  Address offset: 0X0058 */
	volatile uint32_t SP_RX_COUNTER1;                         /*!<  Register,  Address offset: 0X005C */
	volatile const  uint32_t SP_RX_COUNTER2;                         /*!<  Register,  Address offset: 0X0060 */
	volatile uint32_t RSVD4[487];                              /*!<  Reserved,  Address offset:0x64-0x7FC */
	volatile  uint32_t SP_TX_FIFO_0_WR_ADDR;                   /*!<  Register,  Address offset: 0X0800 */
	volatile uint32_t RSVD5[31];                              /*!<  Reserved,  Address offset:0x804-0x87C */
	volatile const  uint32_t SP_RX_FIFO_0_RD_ADDR;                   /*!<  Register,  Address offset: 0X0880 */
	volatile uint32_t RSVD6[31];                              /*!<  Reserved,  Address offset:0x884-0x8FC */
	volatile  uint32_t SP_TX_FIFO_1_WR_ADDR;                   /*!<  Register,  Address offset: 0X0900 */
	volatile uint32_t RSVD7[31];                              /*!<  Reserved,  Address offset:0x904-0x97C */
	volatile const  uint32_t SP_RX_FIFO_1_RD_ADDR;                   /*!<  Register,  Address offset: 0X0980 */
} AUDIO_SPORT_TypeDef;
/** @} */
/* AUTO_GEN_END */

/**
  * @brief	AUDIO SPORT Init structure definition
  */
typedef struct {
	u32 sp_sel_data_format;
	u32 sp_sel_word_len;
	u32 sp_sel_ch_len;
	u32 sp_sel_ch;
	u32 sp_sr;
	u32 sp_sel_tdm;
	u32 sp_sel_fifo;
	u32 sp_sel_i2s0_mono_stereo;
	u32 sp_sel_i2s1_mono_stereo;
	u32 sp_set_multi_io;
	u32 sp_in_clock;
} sport_init_params;


/** @defgroup SP_word_length AUDIO SPORT Word Length
  * @{
  */
#define SP_TXWL_16							((u32)0x00000000)
#define SP_TXWL_20							((u32)0x00000001)
#define SP_TXWL_24							((u32)0x00000002)
#define SP_TXWL_32							((u32)0x00000004)

#define IS_SP_TX_WL(LEN) (((LEN) == SP_TXWL_16) || \
										((LEN) == SP_TXWL_20) || \
										((LEN) == SP_TXWL_24) || \
										((LEN) == SP_TXWL_32))

/** @defgroup SP_word_length AUDIO SPORT Word Length
  * @{
  */
#define SP_RXWL_16							((u32)0x00000000)
#define SP_RXWL_20							((u32)0x00000001)
#define SP_RXWL_24							((u32)0x00000002)
#define SP_RXWL_32							((u32)0x00000004)

#define IS_SP_RX_WL(LEN) (((LEN) == SP_RXWL_16) || \
										((LEN) == SP_RXWL_20) || \
										((LEN) == SP_RXWL_24) || \
										((LEN) == SP_RXWL_32))


/** @defgroup SP_data_format AUDIO SPORT Interface Format
  * @{
  */
#define SP_DF_I2S						((u32)0x00000000)
#define SP_DF_LEFT						((u32)0x00000001)
#define SP_DF_PCM_A						((u32)0x00000002)
#define SP_DF_PCM_B						((u32)0x00000003)

#define IS_SP_DATA_FMT(FORMAT) (((FORMAT) == SP_DF_I2S) || \
											((FORMAT) == SP_DF_LEFT) || \
											((FORMAT) == SP_DF_PCM_A) || \
											((FORMAT) == SP_DF_PCM_B))

/** @defgroup SP_channel_number AUDIO SPORT Channel Number
  * @{
  */
#define SP_CH_STEREO					((u32)0x00000000)
#define SP_CH_MONO						((u32)0x00000001)

#define IS_SP_CHN_NUM(NUM) (((NUM) == SP_CH_STEREO) || \
								((NUM) == SP_CH_MONO))


/** @defgroup SP_SEL_RX_channel AUDIO SPORT Selection of RX Channel
  * @{
  */
#define SP_RX_CH_LR						((u32)0x00000000)
#define SP_RX_CH_RL						((u32)0x00000001)
#define SP_RX_CH_LL						((u32)0x00000002)
#define SP_RX_CH_RR						((u32)0x00000003)

#define IS_SP_SEL_RX_CH(CH) (((CH) == SP_RX_CH_LR) || \
							((CH) == SP_RX_CH_RL) || \
							((CH) == SP_RX_CH_LL) || \
							((CH) == SP_RX_CH_RR))

/** @defgroup SP_SEL_TX_channel AUDIO SPORT Selection of TX Channel
  * @{
  */
#define SP_TX_CH_LR						((u32)0x00000000)
#define SP_TX_CH_RL						((u32)0x00000001)
#define SP_TX_CH_LL						((u32)0x00000002)
#define SP_TX_CH_RR						((u32)0x00000003)

#define IS_SP_SEL_TX_CH(CH) (((CH) == SP_TX_CH_LR) || \
							((CH) == SP_TX_CH_RL) || \
							((CH) == SP_TX_CH_LL) || \
							((CH) == SP_TX_CH_RR))

/** @defgroup SP_SEL_Tx_TDMMODE AUDIO SPORT Selection of Tx TDM Mode
  * @{
  */
#define SP_TX_NOTDM						((u32)0x00000000)
#define SP_TX_TDM4							((u32)0x00000001)
#define SP_TX_TDM6							((u32)0x00000002)
#define SP_TX_TDM8							((u32)0x00000003)

#define IS_SP_SEL_TX_TDM(SEL) (((SEL) == SP_TX_NOTDM) || \
							((SEL) == SP_TX_TDM4) || \
							((SEL) == SP_TX_TDM6) || \
							((SEL) == SP_TX_TDM8))

/** @defgroup SP_SEL_RX_TDMMODE AUDIO SPORT Selection of RX TDM Mode
  * @{
  */
#define SP_RX_NOTDM						((u32)0x00000000)
#define SP_RX_TDM4							((u32)0x00000001)
#define SP_RX_TDM6							((u32)0x00000002)
#define SP_RX_TDM8							((u32)0x00000003)

#define IS_SP_SEL_RX_TDM(SEL) (((SEL) == SP_RX_NOTDM) || \
							((SEL) == SP_RX_TDM4) || \
							((SEL) == SP_RX_TDM6) || \
							((SEL) == SP_RX_TDM8))

/** @defgroup SP_SEL_TX_FIFO AUDIO SPORT Selection of TX FIFO
  * @{
  */
#define SP_TX_FIFO2						((u32)0x00000000)
#define SP_TX_FIFO4						((u32)0x00000001)
#define SP_TX_FIFO6						((u32)0x00000002)
#define SP_TX_FIFO8						((u32)0x00000003)

#define IS_SP_SEL_TX_FIFO(SEL) (((SEL) == SP_TX_FIFO2) || \
							((SEL) == SP_TX_FIFO4) || \
							((SEL) == SP_TX_FIFO6) || \
							((SEL) == SP_TX_FIFO8))

#define SP_TX_FIFO_DEPTH                 16

/** @defgroup SP_SEL_RX_FIFO AUDIO SPORT Selection of RX FIFO
  * @{
  */
#define SP_RX_FIFO2							((u32)0x00000000)
#define SP_RX_FIFO4								((u32)0x00000001)
#define SP_RX_FIFO6							((u32)0x00000002)
#define SP_RX_FIFO8								((u32)0x00000003)

#define IS_SP_SEL_RX_FIFO(SEL) (((SEL) == SP_RX_FIFO2) || \
							((SEL) == SP_RX_FIFO4) || \
							((SEL) == SP_RX_FIFO6) || \
							((SEL) == SP_RX_FIFO8))
#define SP_RX_FIFO_DEPTH                 16

/** @defgroup SP_TX_channel_length AUDIO SPORT Channel Length
  * @{
  */
#define SP_TXCL_16							((u32)0x00000000)
#define SP_TXCL_20							((u32)0x00000001)
#define SP_TXCL_24							((u32)0x00000002)
#define SP_TXCL_8							((u32)0x00000003)
#define SP_TXCL_32							((u32)0x00000004)

#define IS_SP_TXCH_LEN(LEN) (((LEN) == SP_TXCL_16) || \
										((LEN) == SP_TXCL_20) || \
										((LEN) == SP_TXCL_24) || \
										((LEN) == SP_TXCL_8) || \
										((LEN) == SP_TXCL_32))

/** @defgroup SP_TX_channel_length AUDIO SPORT Channel Length
  * @{
  */
#define SP_RXCL_16							((u32)0x00000000)
#define SP_RXCL_20							((u32)0x00000001)
#define SP_RXCL_24							((u32)0x00000002)
#define SP_RXCL_8							((u32)0x00000003)
#define SP_RXCL_32							((u32)0x00000004)

#define IS_SP_RXCH_LEN(LEN) (((LEN) == SP_RXCL_16) || \
										((LEN) == SP_RXCL_20) || \
										((LEN) == SP_RXCL_24) || \
										((LEN) == SP_RXCL_8) || \
										((LEN) == SP_RXCL_32))

/** @defgroup IS_SP_SEL_GDMA AUDIO SPORT Selection of GMDA
  * @{
  */
#define GDMA_INT						((u32)0x00000000)
#define GDMA_EXT							((u32)0x00000001)

#define IS_SP_SEL_GDMA(SEL) (((SEL) == GDMA_INT) || \
									((SEL) == GDMA_EXT))


/** @defgroup IS_SP_SetTXMULTIIO AUDIO SPORT Selection of MultiIO Mode
  * @{
  */
#define SP_TX_MULTIIO_DIS 					((u32)0x00000000)
#define SP_TX_MULTIIO_EN 					((u32)0x00000001)

#define IS_SP_SET_TX_MULTIIO(SEL) (((SEL) == SP_TX_MULTIIO_DIS) || \
										((SEL) == SP_TX_MULTIIO_EN))

/** @defgroup IS_SP_SetRXMULTIIO AUDIO SPORT Selection of MultiIO Mode
  * @{
  */
#define SP_RX_MULTIIO_DIS						((u32)0x00000000)
#define SP_RX_MULTIIO_EN 						((u32)0x00000001)

#define IS_SP_SET_RX_MULTIIO(SEL) (((SEL) == SP_RX_MULTIIO_DIS) || \
											((SEL) == SP_RX_MULTIIO_EN))

/*TX/RX TDM Mode define*/
#define NOTDM			(0)
#define TDM4			(1)
#define TDM6			(2)
#define TDM8			(3)

#define SP_CL_16		(16)
#define SP_CL_20		(20)
#define SP_CL_24		(24)
#define SP_CL_32		(32)

#define SP_8K			(8)
#define SP_12K			(12)
#define SP_24K			(24)
#define SP_16K			(16)
#define SP_32K			(32)
#define SP_48K			(48)
#define SP_96K			(96)
#define SP_192K			(192)
#define SP_384K			(384)

#define SP_11P025K		(11025)
#define SP_44P1K		(44100)
#define SP_88P2K		(88200)
#define SP_22P05K		(22050)
#define SP_176P4K		(176400)

#define IS_RATE_SUPPORTED(NUM) (((NUM) == 8000) || \
								((NUM) == 11025)  || \
								((NUM) == 12000)  || \
								((NUM) == 16000)  || \
								((NUM) == 22050)  || \
								((NUM) == 24000)  || \
								((NUM) == 32000)  || \
								((NUM) == 44100)  || \
								((NUM) == 48000)  || \
								((NUM) == 88200)  || \
								((NUM) == 96000)  || \
								((NUM) == 176400)  || \
								((NUM) == 192000))

#define MCLK_DIV_4      0
#define MCLK_DIV_2      1
#define MCLK_DIV_1      2

#define I2S_MODE_MASTER      0
#define I2S_MODE_SLAVE       1

void audio_sp_enable(void __iomem * sportx);
void audio_sp_disable(void __iomem * sportx);
void audio_sp_tx_init(void __iomem * sportx,sport_init_params *SP_TXInitStruct);
void audio_sp_rx_init(void __iomem * sportx,sport_init_params *SP_RXInitStruct);
void audio_sp_tx_start(void __iomem * sportx, bool NewState);
void audio_sp_rx_start(void __iomem * sportx, bool NewState);
void audio_sp_dma_cmd(void __iomem * sportx, bool NewState);
void audio_sp_set_tx_word_len(void __iomem * sportx, u32 SP_TX_WordLen, u32 SP_RX_WordLen);
void audio_sp_set_rx_word_len(void __iomem * sportx, u32 SP_TX_WordLen, u32 SP_RX_WordLen);
u32 audio_sp_get_tx_word_len(void __iomem * sportx);
u32 audio_sp_get_rx_word_len(void __iomem * sportx);
void audio_sp_set_mono_stereo(void __iomem * sportx, u32 SP_MonoStereo);
void audio_sp_set_tx_clk_div(void __iomem * sportx, unsigned long clock, u32 sr, u32 tdm, u32 chn_len, u32 multi_io);
void audio_sp_set_rx_clk_div(void __iomem * sportx, u32 clock, u32 sr, u32 tdm, u32 chn_len, u32 multi_io);
void audio_sp_set_mclk_clk_div(void __iomem * sportx, u32 divider);
void audio_sp_set_tx_count(void __iomem * sportx, u32 comp_val);
void audio_sp_disable_tx_count(void __iomem * sportx);
u32 audio_sp_get_tx_count(void __iomem * sportx);
bool audio_sp_is_tx_sport_irq(void __iomem * sportx);
void audio_sp_clear_tx_sport_irq(void __iomem * sportx);
void audio_sp_set_rx_count(void __iomem * sportx, u32 comp_val);
void audio_sp_disable_rx_count(void __iomem * sportx);
u32 audio_sp_get_rx_count(void __iomem * sportx);
bool audio_sp_is_rx_sport_irq(void __iomem * sportx);
void audio_sp_clear_rx_sport_irq(void __iomem * sportx);
void audio_sp_disable_rx_tx_sport_irq(void __iomem * sportx);
void audio_sp_set_i2s_mode(void __iomem * sportx, u32 mode);
void audio_sp_set_phase_latch(void __iomem * sportx);
u32 audio_sp_get_tx_phase_val(void __iomem * sportx);
u32 audio_sp_get_rx_phase_val(void __iomem * sportx);
void audio_sp_tx_set_fifo(void __iomem * sportx, u32 fifo_num, bool new_state);
void audio_sp_rx_set_fifo(void __iomem * sportx, u32 fifo_num, bool new_state);

#if 0
void audio_sp_set_sport_addr(u32 sport_index, void __iomem * sportx);
__iomem * audio_sp_get_sport_addr(u32 sport_index);
void audio_sp_set_sport_irq_total_tx_counter(u32 sport_index, u32 irq_total_tx_counter);
u32 audio_sp_get_sport_irq_total_tx_counter(u32 sport_index);
void audio_sp_set_sport_irq_total_rx_counter(u32 sport_index, u32 irq_total_rx_counter);
u32 audio_sp_get_sport_irq_total_rx_counter(u32 sport_index);
#endif

#define AUDIO_BLOCK_SIZE 2048

#endif



