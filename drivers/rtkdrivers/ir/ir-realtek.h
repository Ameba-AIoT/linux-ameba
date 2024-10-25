// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek IR support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <media/rc-core.h>
#include "../../media/rc/rc-core-priv.h"

#define RTK_IR_REG_DUMP				0
#define RTK_IR_HW_CONTROL_FOR_FUTURE_USE	0

#define RTK_IR_DEBUG_DETAILS			0

/**************************************************************************//**
 * @defgroup AMEBA_IR
 * @{
 * @brief AMEBA_IR Register Declaration
 *****************************************************************************/
#define IR_CLK_DIV				0x000
#define IR_TX_CONFIG				0x004
#define IR_TX_SR				0x008
#define IR_TX_COMPE_DIV				0x00C
#define IR_TX_INT_CLR				0x010
#define IR_TX_FIFO				0x014
#define IR_RX_CONFIG				0x018
#define IR_RX_SR				0x01C
#define IR_RX_INT_CLR				0x020
#define IR_RX_CNT_INT_SEL			0x024
#define IR_RX_FIFO				0x028
#define IR_VERSION				0x02C

/**************************************************************************//**
 * @defgroup IR_CLK_DIV
 * @brief
 * @{
 *****************************************************************************/
#define IR_MASK_CLK_DIV				((u32)0x00000FFF << 0)		/*!<R/W 0x0  IR_CLK = IO_CLK/(1 + IR_CLK_DIV) *Tx mode: divider number to generate IrDA modulation frequency. For example: sys_clk = 100MHz, modulation_freq = 455kHz, IR_DIV_NUM = (sys_clk/modulation_freq) - 1 *Rx mode: waveform sample clock. IR_DIV_NUM = (sys_clk/sample clock) - 1 For example: sample clock = 100MHz, IR_DIV_NUM = 0; sample clock = 50MHz, IR_DIV_NUM = 1 */
#define IR_CLK_DIV_X(x)				((u32)(((x) & 0x00000FFF) << 0))
#define IR_GET_CLK_DIV(x)			((u32)(((x >> 0) & 0x00000FFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IR_TX_CONFIG
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_MODE_SEL				((u32)0x00000001 << 31)		/*!<R/W 0x0  *0: Tx mode *1: Rx mode */
#define IR_BIT_TX_START				((u32)0x00000001 << 30)		/*!<R/W 0x0  *0: FSM stops at idle state. *1: FSM runs. */
#define IR_MASK_TX_DUTY_NUM			((u32)0x00000FFF << 16)		/*!<R/W 0x0  Duty cycle setting for modulation frequency For example: for 1/3 duty cycle, IR_DUTY_NUM = (IR_DIV_NUM+1)/3 Note: Set this value equals to IR_DIV_NUM to generate 100% duty waveform. */
#define IR_TX_DUTY_NUM(x)			((u32)(((x) & 0x00000FFF) << 16))
#define IR_GET_TX_DUTY_NUM(x)			((u32)(((x >> 16) & 0x00000FFF)))
#define IR_BIT_TX_OUTPUT_INVERSE		((u32)0x00000001 << 14)		/*!<R/W 0x0  *0: Not inverse active output *1: Inverse active output */
#define IR_BIT_TX_DE_INVERSE			((u32)0x00000001 << 13)		/*!<R/W 0x0  *0: Not inverse FIFO define *1: Inverse FIFO define */
#define IR_MASK_TX_FIFO_LEVEL_TH		((u32)0x0000001F << 8)		/*!<R/W 0x0  Tx FIFO interrupt threshold is from 0 to 15. When Tx FIFO depth = < threshold value, interrupt is triggered. */
#define IR_TX_FIFO_LEVEL_TH(x)			((u32)(((x) & 0x0000001F) << 8))
#define IR_GET_TX_FIFO_LEVEL_TH(x)		((u32)(((x >> 8) & 0x0000001F)))
#define IR_BIT_TX_IDLE_STATE			((u32)0x00000001 << 6)		/*!<R/W 0x0  Tx output state in idle *0: Low *1: High */
#define IR_BIT_TX_FIFO_OVER_INT_MASK		((u32)0x00000001 << 5)		/*!<R/W 0x0  Tx FIFO overflow interrupt *0: Unmask *1: Mask */
#define IR_BIT_TX_FIFO_OVER_INT_EN		((u32)0x00000001 << 4)		/*!<R/W 0x0  Tx FIFO overflow interrupt *0: Disable *1: Enable */
#define IR_BIT_TX_FIFO_LEVEL_INT_MASK		((u32)0x00000001 << 3)		/*!<R/W 0x0  Tx FIFO level interrupt *0: Unmask *1: Mask */
#define IR_BIT_TX_FIFO_EMPTY_INT_MASK		((u32)0x00000001 << 2)		/*!<R/W 0x0  Tx FIFO empty interrupt *0: Unmask *1: Mask */
#define IR_BIT_TX_FIFO_LEVEL_INT_EN		((u32)0x00000001 << 1)		/*!<R/W 0x0  Tx FIFO level interrupt When Tx FIFO offset <= threshold value, interrupt is triggered. *0: Disable *1: Enable */
#define IR_BIT_TX_FIFO_EMPTY_INT_EN		((u32)0x00000001 << 0)		/*!<R/W 0x0  Tx FIFO empty interrupt *0: Disable *1: Enable */
/** @} */

/**************************************************************************//**
 * @defgroup IR_TX_SR
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_TX_FIFO_EMPTY			((u32)0x00000001 << 15)		/*!<R 0x0  *0: Not empty *1: Empty */
#define IR_BIT_TX_FIFO_FULL			((u32)0x00000001 << 14)		/*!<R 0x0  *0: Not full *1: Full */
#define IR_MASK_TX_FIFO_OFFSET			((u32)0x0000003F << 8)		/*!<R 0x0  Tx FIFO offset is from 0 to 32. Note: After Tx last packet, hardware can't clear Tx FIFO offset. */
#define IR_TX_FIFO_OFFSET(x)			((u32)(((x) & 0x0000003F) << 8))
#define IR_GET_TX_FIFO_OFFSET(x)		((u32)(((x >> 8) & 0x0000003F)))
#define IR_BIT_TX_STATUS			((u32)0x00000001 << 4)		/*!<R 0x0  *0: Idle *1: Run */
#define IR_BIT_TX_FIFO_OVER_INT_STATUS		((u32)0x00000001 << 2)		/*!<R 0x0  Tx FIFO overflow interrupt *0: Interrupt inactive *1: Interrupt active */
#define IR_BIT_TX_FIFO_LEVEL_INT_STATUS		((u32)0x00000001 << 1)		/*!<R 0x0  When Tx FIFO offset <= threshold value, interrupt is triggered. *0: Interrupt inactive *1: Interrupt active */
#define IR_BIT_TX_FIFO_EMPTY_INT_STATUS		((u32)0x00000001 << 0)		/*!<R 0x0  Tx FIFO empty interrupt *0: Interrupt inactive *1: Interrupt active */
/** @} */

/**************************************************************************//**
 * @defgroup IR_TX_COMPE_DIV
 * @brief
 * @{
 *****************************************************************************/
#define IR_MASK_TX_COMPE_DIV			((u32)0x00000FFF << 0)		/*!<R/W 0x0  IR_TX_CLK_Period = SCLK/(TX_COMPE_DIV + 1) */
#define IR_TX_COMPE_DIV_X(x)			((u32)(((x) & 0x00000FFF) << 0))
#define IR_GET_TX_COMPE_DIV(x)			((u32)(((x >> 0) & 0x00000FFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IR_TX_INT_CLR
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_TX_FIFO_OVER_INT_CLR		((u32)0x00000001 << 3)		/*!<WC   Tx FIFO overflow interrupt Write 1 to clear */
#define IR_BIT_TX_FIFO_LEVEL_INT_CLR		((u32)0x00000001 << 2)		/*!<WC   When Tx FIFO offset <= threshold value, interrupt is triggered. Write 1 to clear */
#define IR_BIT_TX_FIFO_EMPTY_INT_CLR		((u32)0x00000001 << 1)		/*!<WC   Tx FIFO empty interrupt Write 1 to clear */
#define IR_BIT_TX_FIFO_CLR			((u32)0x00000001 << 0)		/*!<WC   Write 1 to clear Tx FIFO */
/** @} */

/**************************************************************************//**
 * @defgroup IR_TX_FIFO
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_TX_DATA_TYPE			((u32)0x00000001 << 31)		/*!<W 0  Data type *0: Inactive carrier (no carrier) *1: Active carrier (carrier) */
#define IR_BIT_TX_DATA_END_FLAG			((u32)0x00000001 << 30)		/*!<W 0  *0: Normal packet *1: Last packet */
#define IR_MASK_TX_COMPENSATION			((u32)0x00000003 << 28)		/*!<W 0  *0x0: IR_TX_CLK_Period = Tsys_clk * IR_CLK_DIV *0x1: IR_TX_CLK_Period = (1 + 1/2) Tsys_clk * IR_CLK_DIV *0x2: IR_TX_CLK_Period = (1 + 1/4) Tsys_clk * IR_CLK_DIV *0x3: IR_TX_CLK_Period = Tsys_clk * (IR_TX_COMPE_DIV + 1) */
#define IR_TX_COMPENSATION(x)			((u32)(((x) & 0x00000003) << 28))
#define IR_GET_TX_COMPENSATION(x)		((u32)(((x >> 28) & 0x00000003)))
#define IR_MASK_TX_DATA_TIME			((u32)0x0FFFFFFF << 0)		/*!<W 0  Real active time = (IR_TX_DATA_TIME + 1) * IR_TX_CLK_Period */
#define IR_TX_DATA_TIME(x)			((u32)(((x) & 0x0FFFFFFF) << 0))
#define IR_GET_TX_DATA_TIME(x)			((u32)(((x >> 0) & 0x0FFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IR_RX_CONFIG
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_RX_START				((u32)0x00000001 << 28)		/*!<R/W 0x0  *0: FSM stops at idle state. *1: FSM runs. */
#define IR_BIT_RX_START_MODE			((u32)0x00000001 << 27)		/*!<R/W 0x0  *0: Manual mode, IR_RX_MAN_START control *1: Auto-mode, Trigger-mode cont */
#define IR_BIT_RX_MAN_START			((u32)0x00000001 << 26)		/*!<R/W 0x0  If IR_RX_TRIGGER_MODE =0, writing 1 means starting to check the waveform. */
#define IR_MASK_RX_TRIGGER_MODE			((u32)0x00000003 << 24)		/*!<R/W 0x0  *00: High --> low trigger *01: Low -> high trigger *10: High -> low or low ->high trigger */
#define IR_RX_TRIGGER_MODE(x)			((u32)(((x) & 0x00000003) << 24))
#define IR_GET_RX_TRIGGER_MODE(x)		((u32)(((x >> 24) & 0x00000003)))
#define IR_MASK_RX_FILTER_STAGETX		((u32)0x00000007 << 21)		/*!<R/W 0x0  *0x0: Filter <= 20ns glitch *0x1: Filter <= 30ns glitch *0x2: Filter <= 40ns glitch *0x3: Filter <= 50ns glitch … */
#define IR_RX_FILTER_STAGETX(x)			((u32)(((x) & 0x00000007) << 21))
#define IR_GET_RX_FILTER_STAGETX(x)		((u32)(((x >> 21) & 0x00000007)))
#define IR_BIT_RX_FIFO_ERROR_INT_MASK		((u32)0x00000001 << 19)		/*!<R/W 0x0  Rx FIFO error read interrupt When Rx FIFO is empty, read Rx FIFO and trigger interrupt. *0: Unmask *1: Mask */
#define IR_BIT_RX_CNT_THR_INT_MASK		((u32)0x00000001 << 18)		/*!<R/W 0x0  Rx count threshold interrupt *0: Unmask *1: Mask */
#define IR_BIT_RX_FIFO_OF_INT_MASK		((u32)0x00000001 << 17)		/*!<R/W 0x0  Rx FIFO overflow *0: Unmask *1: Mask */
#define IR_BIT_RX_CNT_OF_INT_MASK		((u32)0x00000001 << 16)		/*!<R/W 0x0  RX counter overflow *0: Unmask *1: Mask */
#define IR_BIT_RX_FIFO_LEVEL_INT_MASK		((u32)0x00000001 << 15)		/*!<R/W 0x0  Rx FIFO level interrupt *0: Unmask *1: Mask When Rx FIFO offset >= threshold value, this interrupt is triggered. */
#define IR_BIT_RX_FIFO_FULL_INT_MASK		((u32)0x00000001 << 14)		/*!<R/W 0x0  Rx FIFO full interrupt *0: Unmask *1: Mask */
#define IR_BIT_RX_FIFO_DISCARD_SET		((u32)0x00000001 << 13)		/*!<R/W 0x0  When FIFO is full, new data is send to FIFO. *0: Discard oldest data in FIFO. *1: Reject new data sending to FIFO. */
#define IR_MASK_RX_FIFO_LEVEL_TH		((u32)0x0000001F << 8)		/*!<R/W 0x0  Rx FIFO interrupt threshold When Rx FIFO depth > threshold value, this interrupt is triggered. */
#define IR_RX_FIFO_LEVEL_TH(x)			((u32)(((x) & 0x0000001F) << 8))
#define IR_GET_RX_FIFO_LEVEL_TH(x)		((u32)(((x >> 8) & 0x0000001F)))
#define IR_BIT_RX_FIFO_ERROR_INT_EN		((u32)0x00000001 << 5)		/*!<R/W 0x0  Rx FIFO error read interrupt When Rx FIFO is empty, reading the Rx FIFO triggers this interrupt. *0: Disable *1: Enable */
#define IR_BIT_RX_CNT_THR_INT_EN		((u32)0x00000001 << 4)		/*!<R/W 0x0  Rx count threshold interrupt *0: Disable *1: Enable */
#define IR_BIT_RX_FIFO_OF_INT_EN		((u32)0x00000001 << 3)		/*!<R/W 0x0  RX FIFO overflow *0: Disable *1: Enable */
#define IR_BIT_RX_CNT_OF_INT_EN			((u32)0x00000001 << 2)		/*!<R/W 0x0  RX counter overflow *0: Disable *1: Enable */
#define IR_BIT_RX_FIFO_LEVEL_INT_EN		((u32)0x00000001 << 1)		/*!<R/W 0x0  Rx FIFO level interrupt When Rx FIFO offset >= threshold value, this interrupt is triggered. *0: Disable *1: Enable */
#define IR_BIT_RX_FIFO_FULL_INT_EN		((u32)0x00000001 << 0)		/*!<R/W 0x0  Rx FIFO full interrupt *0: Disable *1: Enable */
/** @} */

/**************************************************************************//**
 * @defgroup IR_RX_SR
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_RX_FIFO_EMPTY			((u32)0x00000001 << 17)		/*!<R 0x0  *0: Not empty *1: Empty */
#define IR_BIT_RX_FIFO_FULL			((u32)0x00000001 << 16)		/*!<R 0x0  *0: Not full *1: Full */
#define IR_MASK_RX_FIFO_OFFSET			((u32)0x0000003F << 8)		/*!<R 0x0  Rx FIFO offset */
#define IR_RX_FIFO_OFFSET(x)			((u32)(((x) & 0x0000003F) << 8))
#define IR_GET_RX_FIFO_OFFSET(x)		((u32)(((x >> 8) & 0x0000003F)))
#define IR_BIT_RX_STATE				((u32)0x00000001 << 7)		/*!<R 0x0  *0: Idle *1: Run */
#define IR_BIT_RX_FIFO_ERROR_INT_STATUS		((u32)0x00000001 << 5)		/*!<R 0x0  Rx FIFO error read interrupt status When Rx FIFO is empty, reading the Rx FIFO triggers this interrupt. *0: Interrupt is inactive *1: Interrupt is active */
#define IR_BIT_RX_CNT_THR_INT_STATUS		((u32)0x00000001 << 4)		/*!<R 0x0  Rx count threshold interrupt status *0: Interrupt is inactive *1: Interrupt is active */
#define IR_BIT_RX_FIFO_OF_INT_STATUS		((u32)0x00000001 << 3)		/*!<R 0x0  Rx FIFO overflow interrupt status *0: Interrupt is inactive *1: Interrupt is active */
#define IR_BIT_RX_CNT_OF_INT_STATUS		((u32)0x00000001 << 2)		/*!<R 0x0  Rx counter overflow interrupt status *0: Interrupt is inactive *1: Interrupt is active */
#define IR_BIT_RX_FIFO_LEVEL_INT_STATUS		((u32)0x00000001 << 1)		/*!<R 0x0  Rx FIFO level interrupt status *0: Interrupt is inactive *1: Interrupt is active */
#define IR_BIT_RX_FIFO_FULL_INT_STATUS		((u32)0x00000001 << 0)		/*!<R 0x0  Rx FIFO full interrupt status *0: Interrupt is inactive *1: Interrupt is active */
/** @} */

/**************************************************************************//**
 * @defgroup IR_RX_INT_CLR
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_RX_FIFO_CLR			((u32)0x00000001 << 8)		/*!<WC   Write 1 to clear Rx FIFO */
#define IR_BIT_RX_FIFO_ERROR_INT_CLR		((u32)0x00000001 << 5)		/*!<WC   Rx FIFO error read interrupt Write 1 to clear */
#define IR_BIT_RX_CNT_THR_INT_CLR		((u32)0x00000001 << 4)		/*!<WC   Rx count threshold interrupt Write 1 to clear */
#define IR_BIT_RX_FIFO_OF_INT_CLR		((u32)0x00000001 << 3)		/*!<WC   Rx FIFO overflow interrupt Write 1 to clear */
#define IR_BIT_RX_CNT_OF_INT_CLR		((u32)0x00000001 << 2)		/*!<WC   Rx counter overflow interrupt Write 1 to clear */
#define IR_BIT_RX_FIFO_LEVEL_INT_CLR		((u32)0x00000001 << 1)		/*!<WC   Rx FIFO level interrupt Write 1 to clear */
#define IR_BIT_RX_FIFO_FULL_INT_CLR		((u32)0x00000001 << 0)		/*!<WC   Rx FIFO full interrupt Write 1 to clear */
/** @} */

/**************************************************************************//**
 * @defgroup IR_RX_CNT_INT_SEL
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_RX_CNT_THR_TRIGGER_LV		((u32)0x00000001 << 31)		/*!<R/W 0x0  Trigger level *0: When low level counter >= threshold, trigger interrupt *1: When high level counter >= threshold, trigger interrupt */
#define IR_MASK_RX_CNT_THR			((u32)0x7FFFFFFF << 0)		/*!<R/W 0x0  31-bits threshold */
#define IR_RX_CNT_THR(x)			((u32)(((x) & 0x7FFFFFFF) << 0))
#define IR_GET_RX_CNT_THR(x)			((u32)(((x >> 0) & 0x7FFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IR_RX_FIFO
 * @brief
 * @{
 *****************************************************************************/
#define IR_BIT_RX_LEVEL				((u32)0x00000001 << 31)		/*!<RO 0x0  Rx Level *1: High level *0: Low level */
#define IR_MASK_RX_CNT				((u32)0x7FFFFFFF << 0)		/*!<RO 0x0  31-bits cycle duration */
#define IR_RX_CNT(x)				((u32)(((x) & 0x7FFFFFFF) << 0))
#define IR_GET_RX_CNT(x)			((u32)(((x >> 0) & 0x7FFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IR_VERSION
 * @brief
 * @{
 *****************************************************************************/
#define IR_MASK_VERSION				((u32)0xFFFFFFFF << 0)		/*!<R 0x1410150A  IR IP version */
#define IR_VERSION_X(x)				((u32)(((x) & 0xFFFFFFFF) << 0))
#define IR_GET_VERSION(x)			((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/** @defgroup ir_frequency IR Carrier Frequency
  * @{
  */
#define IR_TX_FREQ_HZ_MIN			(25000)
#define IS_IR_FREQUENCY(F)			((F) >= IR_TX_FREQ_HZ_MIN)
/**
  * @}
  */

/** @defgroup IR_Mode IR Mode
  * @{
  */
#define IR_MODE_TX				(0)
#define IR_MODE_RX				(1)
#define IR_MODE(MODE)				((MODE) & IR_BIT_MODE_SEL)
/**
  * @}
  */

/** @defgroup IR_Tx_Threshold IR TX Threshold
  * @{
  */
#define IR_TX_FIFO_SIZE				(32)
/** End of group IR_Tx_Threshold
  * @}
  */

/** @defgroup IR_TX_Data_LEVEL IR TX Data Level
  * @{
  */
#define IR_TX_DATA_NORMAL_CARRIER_NORMAL	(0)
#define IR_TX_DATA_NORMAL_CARRIER_INVERSE	(1)
#define IR_TX_DATA_INVERSE_CARRIER_NORMAL	(2)
#define IR_TX_DATA_INVERSE_CARRIER_INVERSE	(3)
#define IR_TX_DATA_TYPE_SET(x)			((u32)(((x) << 13) & (IR_BIT_TX_OUTPUT_INVERSE | IR_BIT_TX_DE_INVERSE)))
/** End of group IR_TX_Data_LEVEL
  * @}
  */

/** @defgroup IR_Idle_Status IR Idle Status
  * @{
  */
#define IR_IDLE_OUTPUT_LOW			(0)
#define IR_IDLE_OUTPUT_HIGH			(1)
/** End of group IR_Idle_Status
  * @}
  */

/** @defgroup IR_TX_INT
  * @{
  */
#define IR_TX_INT_ALL_MASK			((u32)0x0000002C)
#define IR_TX_INT_ALL_EN			((u32)0x00000013)
#define IR_TX_INT_ALL_CLR			((u32)0x0000000E)
#define IS_TX_INT_MASK(MASK)			(((MASK) & (~IR_TX_INT_ALL_MASK)) == 0)
#define IS_TX_INT(MASK)				(((MASK) & (~IR_TX_INT_ALL_EN)) == 0)
#define IS_TX_INT_CLR(MASK)			(((MASK) & (~IR_TX_INT_ALL_CLR)) == 0)
/** End of group IR_TX_INT
  * @}
  */


/** @defgroup IR_Rx_Start_Ctrl
  * @{
  */
#define IR_RX_MANUAL_MODE			(0)
#define IR_RX_AUTO_MODE				(1)
/** End of group IR_Rx_Start_Ctrl
  * @}
  */

/** @defgroup IR_RX_Filter_Time
  * @{
  */
#define IR_RX_FILTER_TIME_20NS			(0)
#define IR_RX_FILTER_TIME_30NS			(1)
#define IR_RX_FILTER_TIME_40NS			(2)
#define IR_RX_FILTER_TIME_50NS			(3)
#define IR_RX_FILTER_TIME_60NS			(4)
#define IR_RX_FILTER_TIME_70NS			(5)
#define IR_RX_FILTER_TIME_80NS			(6)
#define IR_RX_FILTER_TIME_90NS			(7)
/** End of group IR_RX_Filter_Time
  * @}
  */

/** @defgroup IR_RX_FIFO_DISCARD_SETTING
  * @{
  */
#define IR_RX_FIFO_FULL_DISCARD_NEWEST		(1)
#define IR_RX_FIFO_FULL_DISCARD_OLDEST		(0)
/** End of group IR_RX_FIFO_DISCARD_SETTING
  * @}
  */

/** @defgroup IR_Rx_Threshold IR RX Threshold
  * @{
  */
#define IR_RX_FIFO_SIZE				(32)
/** End of group IR_Rx_Threshold
  * @}
  */

/** @defgroup IR_RX_INT
  * @{
  */
#define IR_RX_INT_ALL_EN			((u32)(IR_BIT_RX_FIFO_ERROR_INT_EN | \
					IR_BIT_RX_CNT_THR_INT_EN | \
						IR_BIT_RX_FIFO_OF_INT_EN | \
						IR_BIT_RX_FIFO_LEVEL_INT_EN | \
					IR_BIT_RX_FIFO_FULL_INT_EN))
#define IR_RX_INT_ALL_CLR			((u32)0x0000003F)
#define IR_RX_INT_ALL_MASK			((u32)0x000FC000)
#define IS_RX_INT_MASK(MASK)			(((MASK) & (~IR_RX_INT_ALL_MASK)) == 0)
#define IS_RX_INT(MASK)				(((MASK) & (~IR_RX_INT_ALL_EN)) == 0)
#define IS_RX_INT_CLR(MASK)			(((MASK) & (~IR_RX_INT_ALL_CLR)) == 0)
/** End of group IR_RX_INT
  * @}
  */

/** @defgroup IR_RX_COUNTER_THRESHOLD
  * @{
  */
#define IR_RX_COUNT_LOW_LEVEL			(0)
#define IR_RX_COUNT_HIGH_LEVEL			(1)
/** End of group IR_RX_COUNTER_THRESHOLD
  * @}
  */

/** @defgroup IR_RX_TRIGGER_EDGE rx auto start trigger edge
  * @{
  */
#define IR_RX_FALL_EDGE				(0)
#define IR_RX_RISING_EDGE			(1)
#define IR_RX_DOUBLE_EDGE			(2)

#define IS_IR_RX_TRIGGER_EDGE(TRIGGER)		(((TRIGGER) == IR_RX_FALL_EDGE) || \
						((TRIGGER) == IR_RX_RISING_EDGE) || \
						((TRIGGER) == IR_RX_DOUBLE_EDGE))
/** End of group IR_RX_TRIGGER_EDGE
  * @}
  */

#define BIT_SECFG_IR				((u32)0x00000001 << 13)		/*!<R/W 0  1: non-secure attribution 0: secure attribution */


/* the maximum number of carrier,
 * if the counter is larger than MAX_CARRIER, we consider it is a non-carrier
 * this value must be larger enough but less than log0/1 non-carrier time */
#define RTK_IR_MAX_CARRIER			100
#define MAX_BUF_LEN				256
#define BUF_LEN_WARNING				200

#define RTK_IR_TX_RETRY_TIMES			0xffffffff
#define RTK_IR_TX_THRESHOLD			15

#define IR_NEC_WAVE_END_SYMBOL1			562500
#define IR_NEC_WAVE_END_SYMBOL2			5625000

#ifndef ENABLE
#define ENABLE					1
#endif
#ifndef DISABLE
#define DISABLE					0
#endif

enum rtk_ir_status {
	RTK_IR_TX_IDLE,
	RTK_IR_TX_ONGOING,
	RTK_IR_TX_DONE,
};

/**
* @brief IR initialize parameters
*
* IR initialize parameters
*/
struct rtk_ir_hw_params {
	u32				ir_clock;
	u32				ir_freq;
	u32				ir_duty_cycle;
	u32				ir_mode;
	u32				ir_tx_idle_level;
	u32				ir_tx_inverse;
	u32				ir_tx_fifo_threshold;
	u32				ir_tx_comp_clk;
	u32				ir_rx_enable_mode;
	u32				ir_rx_auto;
	u32				ir_tx_encode_enable;
	u32				ir_rx_fifo_threshold;
	u32				ir_rx_fifo_full_ctrl;
	u32				ir_rx_trigger_mode;
	u32				ir_rx_filter_time;
	u32				ir_rx_cnt_thr_type;
	u32				ir_rx_cnt_thr;
	u32				ir_rx_inverse;
};

struct ir_management_adapter {
	unsigned int			freq;		/* carrier frequency */
	unsigned int			duty_cycle;	/* carrier duty cycle */

	int				wbuf[MAX_BUF_LEN];
	int				wbuf_len;
	int				wbuf_index;

	int				pulse_duration;
	int				ir_status;
	unsigned long			device_is_open;
};

struct rtk_ir_dev {
	struct rc_dev			*rcdev;
	struct device			*dev;
	void __iomem			*base;
	struct clk			*clk;
	int				irq;

	struct rtk_ir_hw_params		ir_param;
	struct ir_management_adapter	ir_manage;
};
