// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek LEDC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/of_device.h>

#include <dt-bindings/realtek/dmac/realtek-ameba-dmac.h>

#define RTK_LEDC_TEST_FOR_DMA_MODE		0

/***********************************************************************/
/************************** Hardware Layer *****************************/
/***********************************************************************/

/* ID:HSLV9-8, Inter. Type:APB4, Top Address:0x41008FFF, Size(KB):4K, Clk Domain:HS_APB_CLK */
/* Read only here, refer to dts if needed. */
#define LEDC_REG_BASE				0x41008000


#define LEDC_CTRL_REG				0x000
#define LEDC_LED_T01_TIMING_CTRL_REG		0x004
#define LEDC_DATA_FINISH_CNT_REG		0x008
#define LEDC_LED_RESET_TIMING_CTRL_REG		0x00C
#define LEDC_WAIT_TIME0_CTRL_REG		0x010
#define LEDC_WAIT_TIME1_CTRL_REG		0x014
#define LEDC_DATA_REG				0x018
#define LEDC_DMA_CTRL_REG			0x01C
#define LEDC_LED_INTERRUPT_CTRL_REG		0x020
#define LEDC_LED_INT_STS_REG			0x024
#define RSVD0					0x028
#define RSVD1					0x02C
#define LEDC_DEBUG_SEL_REG			0x030

/*
 * @defgroup LEDC_CTRL_REG
 * @brief
 * @{
 */
#define LEDC_MASK_TOTAL_DATA_LENGTH	((u32)0x00001FFF << 16)          /*!<R/W 0x0  Total length of transfer data, range from 1 to 8K (unit:32 bits, only low 24-bit is valid). Actual transfer packages is TOTAL_DATA_LENGTH+1. The field is recommended to an integer multiple of (LED_NUM+1). Note: If TOTAL_DATA_LENGTH is greater than (LED_NUM+1) but not integer multiple, the last frame of data will transfer data less than (LED_NUM+1). */
#define LEDC_TOTAL_DATA_LENGTH(x)	((u32)(((x) & 0x00001FFF) << 16))
#define LEDC_GET_TOTAL_DATA_LENGTH(x)	((u32)(((x >> 16) & 0x00001FFF)))
#define LEDC_BIT_RESET_LED_EN		((u32)0x00000001 << 10)          /*!<R/W/ES 0x0  Write operation: When software writes 1 to this bit, LEDC FSM turns to CPU_RESET_LED state, and CPU triggers LEDC to transfer a reset to LED. Note: Only when LEDC is in the IDLE state, can FSM turn to CPU_RESET_LED state. * When LEDC finishs to send RESET to LED, LEDC FSM returns to the IDLE state, and hardware clears RESET_LED_EN. * Or when software sets SOFT_RESET, LEDC FSM returns to IDLE state, and hardware clears RESET_LED_EN. Read operation: 0: LEDC completes the transmission of LED reset operation. 1: LEDC does not complete the transmission of LED reset operation. */
#define LEDC_BIT_LED_POLARITY		((u32)0x00000001 << 9)          /*!<R/W 0x0  LED DI level when free. 0: Low level 1: High level */
#define LEDC_MASK_LED_RGB_MODE		((u32)0x00000007 << 6)          /*!<R/W 0x0  LEDC inputs 24 bits data pakage in order of {byte2, byte1, byte0}. 000: Output 24 bits data pakage in order of {byte2, byte1, byte0} 001: Output 24 bits data pakage in order of {byte2, byte0, byte1} 010: Output 24 bits data pakage in order of {byte1, byte2, byte0} 011: Output 24 bits data pakage in order of {byte0, byte2, byte1} 100: Output 24 bits data pakage in order of {byte1, byte0, byte2} 101: Output 24 bits data pakage in order of {byte0, byte1, byte2} */
#define LEDC_LED_RGB_MODE(x)		((u32)(((x) & 0x00000007) << 6))
#define LEDC_GET_LED_RGB_MODE(x)	((u32)(((x >> 6) & 0x00000007)))
#define LEDC_BIT_LED_MSB_TOP		((u32)0x00000001 << 5)          /*!<R/W 0x1  Source RGB data format. 0: LSB 1: MSB */
#define LEDC_BIT_LED_MSB_BYTE2		((u32)0x00000001 << 4)          /*!<R/W 0x1  LEDC outputs 24 bits data pakage in order of {byte2, byte1, byte0}; 1: Output data byte2 MSB first 0: Output data byte2 LSB first */
#define LEDC_BIT_LED_MSB_BYTE1		((u32)0x00000001 << 3)          /*!<R/W 0x1  LEDC outputs 24 bits data pakage in order of {byte2, byte1, byte0}; 1: Output data byte1 MSB first 0: Output data byte1 LSB first */
#define LEDC_BIT_LED_MSB_BYTE0		((u32)0x00000001 << 2)          /*!<R/W 0x1  LEDC outputs 24 bits data pakage in order of {byte2, byte1, byte0}; 1: Output data byte0 MSB first 0: Output data byte0 LSB first */
#define LEDC_BIT_SOFT_RESET		((u32)0x00000001 << 1)          /*!<R/WA0 0x0  LEDC software reset. If software writes 1 to LEDC_SOFT_RESET, the next cycle hardware will clear LEDC_SOFT_RESET to 0, which will generate a pulse. LEDC soft reset includes all interrupt status register, the control state machine returns to the IDLE state, LEDC FIFO read and write point is cleared to 0, LEDC interrupt is cleared. The related registers are as below: * LEDC_CTRL_REG (LEDC_EN is cleared to 0) * LEDC_DATA_FINISH_CNT_REG (LEDC_DATA_FINISH_CNT is cleared to 0) * LEDC_INT_STS_REG (all interrupt are cleared) Other registers remain unchanged. */
#define LEDC_BIT_EN			((u32)0x00000001 << 0)          /*!<R/W/ES 0x0  LEDC enable bit. 0: Disable 1: Enable This bit enable indicates that LEDC can be started when LEDC data finished transmission, or this bit is cleared to 0 by hardware when software sets LEDC_SOFT_RESET. Software clears LEDC_EN when LEDC FSM is not IDLE don't affect data transfer. */
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_LED_T01_Timing_CTRL_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_MASK_T1H_CNT		((u32)0x000000FF << 24)          /*!<R/W 0x20  LED modulate logical "1" high level time (T1H_TIME). Unit: cycle (40MHz), T1H_TIME = 25ns * T1H_CNT , where T1H_CNT>0 T1H_TIME default value is 800ns. */
#define LEDC_T1H_CNT(x)			((u32)(((x) & 0x000000FF) << 24))
#define LEDC_GET_T1H_CNT(x)		((u32)(((x >> 24) & 0x000000FF)))
#define LEDC_MASK_T1L_CNT		((u32)0x000000FF << 16)          /*!<R/W 0xc  LED modulate logical "1" low level time. Unit: cycle (40MHz), T1L_TIME = 25ns *T1L_CNT, where T1L_CNT>0 T1L_TIME default value is 300ns. */
#define LEDC_T1L_CNT(x)			((u32)(((x) & 0x000000FF) << 16))
#define LEDC_GET_T1L_CNT(x)		((u32)(((x >> 16) & 0x000000FF)))
#define LEDC_MASK_T0H_CNT		((u32)0x000000FF << 8)          /*!<R/W 0xc  LED modulate logical "0" hgh level time. Unit: cycle (40MHz), T0H_TIME = 25ns * T0H_CNT , where T0H_CNT>0 T0H_TIME default value is 300ns. */
#define LEDC_T0H_CNT(x)			((u32)(((x) & 0x000000FF) << 8))
#define LEDC_GET_T0H_CNT(x)		((u32)(((x >> 8) & 0x000000FF)))
#define LEDC_MASK_T0L_CNT		((u32)0x000000FF << 0)          /*!<R/W 0x20  LED modulate logical "0" low level time. Unit: cycle (40MHz), T0L_TIME = 25ns *T0L_CNT, where T0L_CNT>0 T0L_TIME default value is 800ns. */
#define LEDC_T0L_CNT(x)			((u32)(((x) & 0x000000FF) << 0))
#define LEDC_GET_T0L_CNT(x)		((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_DATA_FINISH_CNT_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_MASK_LED_WAIT_DATA_TIME	((u32)0x00007FFF << 16)          /*!<R/W 0x2edf  The time that internal FIFO of LEDC is waiting for data. The default value is 300us by default. To avoid insert LED RESET between two pixel data, hardware will send WAITDATA_TIMEOUT_INT when wait_time>= min (RESET_TIME, LED_WAIT_DATA_TIME). The adjust range is from 25ns ~ 819us, LED_WAIT_DATA_TIME = 25ns *(N+1) where N is from 0 to 0x7FFF. When the setting time is exceeded, LEDC will send WAITDATA_TIMEOUT_INT interrupt. Under this abnormal situation, software should reset LEDC. */
#define LEDC_LED_WAIT_DATA_TIME(x)	((u32)(((x) & 0x00007FFF) << 16))
#define LEDC_GET_LED_WAIT_DATA_TIME(x)	((u32)(((x >> 16) & 0x00007FFF)))
#define LEDC_MASK_LED_DATA_FINISH_CNT	((u32)0x00001FFF << 0)          /*!<R 0x0  The total LED data have been sent. (range: 0 ~ 8K). When LEDC transfer finishs normally, this register is cleared by hardware when generate trans_finish_int. Software knows LED_DATA_FINISH_CNTequals to TOTAL_DATA_LENGTH. When LEDC transfer interrupt abnormally,this register will clear to 0 by software by setting LEDC_SOFT_RESET. */
#define LEDC_LED_DATA_FINISH_CNT(x)	((u32)(((x) & 0x00001FFF) << 0))
#define LEDC_GET_LED_DATA_FINISH_CNT(x)	((u32)(((x >> 0) & 0x00001FFF)))
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_LED_RESET_TIMING_CTRL_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_MASK_RESET_TIME		((u32)0x00003FFF << 16)          /*!<R/W 0x2edf  Reset time control of LED lamp. The default reset time is 300us. The adjust range is from 25ns to 409.6us. RESET_TIME = 25ns *(N+1) where N is from 0 to 0x3FFF. */
#define LEDC_RESET_TIME(x)		((u32)(((x) & 0x00003FFF) << 16))
#define LEDC_GET_RESET_TIME(x)		((u32)(((x >> 16) & 0x00003FFF)))
#define LEDC_MASK_LED_NUM		((u32)0x000003FF << 0)          /*!<R/W 0x0  The number of external LED lamp. Maximum up to 1024. The default value is 0, which indicate that 1 LED lamp is connected. The range is from 0 to 1023. */
#define LEDC_LED_NUM(x)			((u32)(((x) & 0x000003FF) << 0))
#define LEDC_GET_LED_NUM(x)		((u32)(((x >> 0) & 0x000003FF)))
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_WAIT_TIME0_CTRL_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_BIT_WAIT_TIME0_EN		((u32)0x00000001 << 9)          /*!<R/W 0  Wait time0 enable. When set this bit, the controller automatically insert waiting time before next LED package data. 0: Disable 1: Enable */
#define LEDC_MASK_TOTAL_WAIT_TIME0	((u32)0x000001FF << 0)          /*!<R/W 0x1FF  Waiting time between 2 LED data, and LEDC output Low level during waiting time0. The adjust range is from 25ns to 12.5us, WAIT_TIME0 = 25ns *(N+1) where N is from 0 to 0x1FF. */
#define LEDC_TOTAL_WAIT_TIME0(x)	((u32)(((x) & 0x000001FF) << 0))
#define LEDC_GET_TOTAL_WAIT_TIME0(x)	((u32)(((x >> 0) & 0x000001FF)))
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_WAIT_TIME1_CTRL_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_BIT_WAIT_TIME1_EN		((u32)0x00000001 << 31)          /*!<R/W 0  Wait time1 enable. When set this bit, the controller automatically insert waiting time before next LED frame data. 0: Disable 1: Enable */
#define LEDC_MASK_TOTAL_WAIT_TIME1	((u32)0x7FFFFFFF << 0)          /*!<R/W 0x1FFFFFF  Waiting time between 2 frame data, and LEDC output Low level during waiting time1. The adjust range is from 25ns to 53s, WAIT_TIME0 = 25ns *(N+1) where N is from 0 to 0x7FFFFFFF. */
#define LEDC_TOTAL_WAIT_TIME1(x)	((u32)(((x) & 0x7FFFFFFF) << 0))
#define LEDC_GET_TOTAL_WAIT_TIME1(x)	((u32)(((x >> 0) & 0x7FFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_DATA_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_MASK_DATA			((u32)0x00FFFFFF << 0)          /*!<WPD 0  LEDC display data, the lower 24-bit is valid. Write this register means push data to LEDC FIFO. Register LEDC_EN toggle from 0->1 will clear LEDC FIFO. Thus, only data pushed when LEDC_EN=1 will be send to LED. */
#define LEDC_DATA(x)			((u32)(((x) & 0x00FFFFFF) << 0))
#define LEDC_GET_DATA(x)		((u32)(((x >> 0) & 0x00FFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_DMA_CTRL_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_BIT_DMA_EN			((u32)0x00000001 << 5)          /*!<R/W 0x1  LEDC DMA request enable. 0: Disable request of DMA transfer data 1: Enable request of DMA transfer data */
#define LEDC_MASK_FIFO_TRIG_LEVEL	((u32)0x0000001F << 0)          /*!<R/W 0xF  The remaining space of internal FIFO in LEDC When the remain space of internal LEDC FIFO is larger than LEDC_FIFO_TRIG_LEVEL,will generate dma request or cpu_req_int. Note : The default value is 15. The adjust range is from 0 to 31, and the recommended cofiguration is 7 or 15. */
#define LEDC_FIFO_TRIG_LEVEL(x)		((u32)(((x) & 0x0000001F) << 0))
#define LEDC_GET_FIFO_TRIG_LEVEL(x)	((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_LED_INTERRUPT_CTRL_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_BIT_GLOBAL_INT_EN			((u32)0x00000001 << 4)          /*!<R/W 0  Global interrupt enable. 0: Disable 1: Enable */
#define LEDC_BIT_FIFO_OVERFLOW_INT_EN		((u32)0x00000001 << 3)          /*!<R/W 0  FIFO overflow interrupt enable. When the data wriiten by software is more than internal FIFO level of LEDC, LEDC is in data loss state. 0: Disable 1: Enable */
#define LEDC_BIT_WAITDATA_TIMEOUT_INT_EN	((u32)0x00000001 << 2)          /*!<R/W 0  The internal FIFO in LEDC can`t get data after the LED_WAIT_DATA_TIME, the interrupt will be enable. 0: Disable 1: Enable */
#define LEDC_BIT_FIFO_CPUREQ_INT_EN		((u32)0x00000001 << 1)          /*!<R/W 0  FIFO request CPU data interrupt enable. 0: Disable 1: Enable */
#define LEDC_BIT_LED_TRANS_FINISH_INT_EN	((u32)0x00000001 << 0)          /*!<R/W 0  Data transmission complete interrupt enable. 0: Disable 1: Enable */
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_LED_INT_STS_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_BIT_FIFO_EMPTY		((u32)0x00000001 << 17)          /*!<R 0x1  FIFO empty status flag. When LEDC_EN is 0, FIFO_EMPTY equal 1. This flush residual data when LED_NUM is not integer multiples of burst length. */
#define LEDC_BIT_FIFO_FULL		((u32)0x00000001 << 16)          /*!<R 0  FIFO full status flag. */
#define LEDC_BIT_FIFO_OVERFLOW_INT	((u32)0x00000001 << 3)          /*!<RW1C 0  FIFO overflow interrupt Clear 0 when software set LEDC_SOFT_RESET. The data written by external is more than the maximum storage space of LEDC FIFO. At this time, software needs to deal with the abnormal situation.LEDC performs soft_reset operation to refresh all data. 0: FIFO not overflow 1: FIFO overflow */
#define LEDC_BIT_WAITDATA_TIMEOUT_INT	((u32)0x00000001 << 2)          /*!<RW1C 0  Before transfer a frame: When the internal FIFO in LEDC can`t get data after the LED_WAIT_DATA_TIME,this bit is set. During transfer of a frame (between two pixel of data): To avoid insert LED RESET between two pixel data, hardware will send WAITDATA_TIMEOUT_INT when wait_time>= min(RESET_TIME,LED_WAIT_DATA_TIME). Clear 0 when software set LEDC_SOFT_RESET. When new data arrives at WAIT_DATA state, LEDC will continue sending data. Note : software should notice if the waiting time exceeds the operation of reset time, LEDC may enter refresh state, new data has not been sent. 0: LEDC not timeout 1: LEDC timeout */
#define LEDC_BIT_FIFO_CPUREQ_INT	((u32)0x00000001 << 1)          /*!<RW1C 0  When FIFO data available is less than the threshold left, the interrupt is reported to CPU. Clear 0 when software set LEDC_SOFT_RESET. 0: FIFO doesn`t request CPU for data 1: FIFO request CPU to transfer data Note: when get FIFO_CPUREQ_INT, CPU write 1 data to LEDC Buffer. It is inefficiently compares to DMA mode. */
#define LEDC_BIT_LED_TRANS_FINISH_INT	((u32)0x00000001 << 0)          /*!<RW1C 0  When the data configured as TOTAL_DATA_LENGTH has been transferred complete, this bit is set. Clear 0 when software set LEDC_SOFT_RESET. 0: Data transfer has not complete 1: Data transfer complete */
/** @} */

/**************************************************************************//**
 * @defgroup LEDC_DEBUG_SEL_REG
 * @brief
 * @{
 *****************************************************************************/
#define LEDC_MASK_DEBUG_SEL_DRIVE	((u32)0x00000003 << 7)          /*!<R/W 0  drive module debug out select. */
#define LEDC_DEBUG_SEL_DRIVE(x)		((u32)(((x) & 0x00000003) << 7))
#define LEDC_GET_DEBUG_SEL_DRIVE(x)	((u32)(((x >> 7) & 0x00000003)))
#define LEDC_MASK_DEBUG_SEL_FIFO	((u32)0x0000000F << 3)          /*!<R/W 0  fifo module debug out select. [6:5] select debug_data of ledc_txfifo, valid range 0,1 [4:3] select output data byte when [6:5] is 0. 2'b00: byte0 2'b01: byte1 2'b1x: byte2 */
#define LEDC_DEBUG_SEL_FIFO(x)		((u32)(((x) & 0x0000000F) << 3))
#define LEDC_GET_DEBUG_SEL_FIFO(x)	((u32)(((x >> 3) & 0x0000000F)))
#define LEDC_MASK_DEBUG_SEL_TOP		((u32)0x00000007 << 0)          /*!<R/W 0  ledc top module debug out select. */
#define LEDC_DEBUG_SEL_TOP(x)		((u32)(((x) & 0x00000007) << 0))
#define LEDC_GET_DEBUG_SEL_TOP(x)	((u32)(((x >> 0) & 0x00000007)))
/** @} */


/** @defgroup FLASH_CLK_Div_definitions
  * @{
  */
#define FLASH_CLK_DIV2P0		0
#define FLASH_CLK_DIV2P5		1
#define FLASH_CLK_DIV3P0		2
#define FLASH_CLK_DIV3P5		3
#define FLASH_CLK_DIV4P0		4
/**
  * @}
  */

/** @defgroup FLASH_BIT_Mode_definitions
  * @{
  */
#define SpicOneBitMode			0
#define SpicDualBitMode			1
#define SpicQuadBitMode			2
/**
  * @}
  */

/** @defgroup FLASH_ERASE_Type_definitions
  * @{
  */
#define EraseChip			0
#define EraseBlock			1
#define EraseSector			2
/**
  * @}
  */

/** @defgroup FLASH_WAIT_Type_definitions
  * @{
  */
#define WAIT_SPIC_BUSY			0
#define WAIT_FLASH_BUSY			1
#define WAIT_WRITE_DONE			2
#define WAIT_WRITE_EN			3
/**
  * @}
  */

/** @defgroup SPIC_ADDR_PHASE_LEN_definitions
  * @{
  */
#define ADDR_3_BYTE			0x3
#define ADDR_4_BYTE			0x0
#define ADDR_3_BYTE_USER_PRM		0x0
#define ADDR_4_BYTE_USER_PRM		0x4
/**
  * @}
  */

/** @defgroup WINBOND_W25Q16DV_Spec
  * @{
  */
#define FLASH_CMD_WREN			0x06            //write enable
#define FLASH_CMD_WRDI			0x04            //write disable
#define FLASH_CMD_WRSR			0x01            //write status register
#define FLASH_CMD_RDID			0x9F            //read idenfication
#define FLASH_CMD_RDSR			0x05            //read status register
#define FLASH_CMD_RDSR2			0x35            //read status register-2
#define FLASH_CMD_READ			0x03            //read data
#define FLASH_CMD_FREAD			0x0B            //fast read data
#define FLASH_CMD_RDSFDP		0x5A            //Read SFDP
#define FLASH_CMD_RES			0xAB            //Read Electronic ID
#define FLASH_CMD_REMS			0x90            //Read Electronic Manufacturer & Device ID
#define FLASH_CMD_DREAD			0x3B            //Double Output Mode command
#define FLASH_CMD_SE			0x20            //Sector Erase
#define FLASH_CMD_BE			0xD8            //0x52 //64K Block Erase
#define FLASH_CMD_CE			0x60            //Chip Erase(or 0xC7)
#define FLASH_CMD_PP			0x02            //Page Program
#define FLASH_CMD_DP			0xB9            //Deep Power Down
#define FLASH_CMD_RDP			0xAB            //Release from Deep Power-Down
#define FLASH_CMD_2READ			0xBB            // 2 x I/O read  command
#define FLASH_CMD_4READ			0xEB            // 4 x I/O read  command
#define FLASH_CMD_QREAD			0x6B            // 1I / 4O read command
#define FLASH_CMD_4PP			0x32            //quad page program //this is diff with MXIC
#define FLASH_CMD_FF			0xFF            //Release Read Enhanced
#define FLASH_CMD_REMS2			0x92            // read ID for 2x I/O mode //this is diff with MXIC
#define FLASH_CMD_REMS4			0x94            // read ID for 4x I/O mode //this is diff with MXIC
#define FLASH_CMD_RDSCUR		0x48            // read security register //this is diff with MXIC
#define FLASH_CMD_WRSCUR		0x42            // write security register //this is diff with MXIC

#define FLASH_DM_CYCLE_2O		0x08
#define FLASH_DM_CYCLE_2IO		0x04
#define FLASH_DM_CYCLE_4O		0x08
#define FLASH_DM_CYCLE_4IO		0x06

#define FLASH_STATUS_BUSY		((u32)0x00000001)
#define FLASH_STATUS_WLE		((u32)0x00000002)
/**
  * @}
  */

/** @defgroup WINBOND_W25Q256FV_Spec
  * @{
  */

#define FLASH_CMD_ENT_ADDR4B 		0xB7
#define FLASH_CMD_EXT_ADDR4B		0xE9

/**
  * @}
  */

/** @defgroup FLASH_VENDOR_ID_definitions
  * @{
  */
#define FLASH_ID_OTHERS			0
#define FLASH_ID_MXIC			1
#define FLASH_ID_WINBOND		2
#define FLASH_ID_MICRON			3
#define FLASH_ID_EON			4
#define FLASH_ID_GD			5
#define FLASH_ID_BOHONG			6

/**
  * @}
  */

/** @defgroup FLASH_MANUFACTURER_ID_definitions
  * @{
  */
#define MANUFACTURER_ID_MXIC		0xC2
#define MANUFACTURER_ID_WINBOND		0xEF
#define MANUFACTURER_ID_MICRON		0x20
#define MANUFACTURER_ID_BOHONG		0x68
#define MANUFACTURER_ID_GD		0xC8
#define MANUFACTURER_ID_EON		0x1C
#define MANUFACTURER_ID_FM		0xA1

/* Registers Definitions --------------------------------------------------------*/
/**************************************************************************//**
 * @defgroup FLASH_Register_Definitions FLASH Register Definitions
 * @{
 *****************************************************************************/

/**************************************************************************//**
 * @defgroup SPIC_CTRLR0
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_CTRLR0 register  *******************/
#define BIT_CMD_CH(x)			(((x) & 0x00000003) << 20)
#define BIT_DATA_CH(x)			(((x) & 0x00000003) << 18)
#define BIT_ADDR_CH(x)			(((x) & 0x00000003) << 16)
#define BIT_TMOD(x)			(((x) & 0x00000003) << 8)
#define BIT_SCPOL			(0x00000001 << 7)
#define BIT_SCPH			(0x00000001 << 6)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_CTRLR1
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_CTRLR1 register  *******************/
#define BIT_NDF(x)			((x) & 0xffff)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_SSIENR
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_SSIENR register  *******************/
#define BIT_ATCK_CMD			(0x00000001 << 1)
#define BIT_SPIC_EN			(0x00000001)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_SER
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_SER register  *******************/
#define BIT_SER				(0x00000001)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_BAUDR
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_BAUDR register  *******************/
#define BIT_SCKDV(x)			((x) & 0x0fff)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_SR
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_SR register  *******************/
#define BIT_TXE				(0x00000001 << 5)
#define BIT_RFF				(0x00000001 << 4)
#define BIT_RFNE			(0x00000001 << 3)
#define BIT_TFE				(0x00000001 << 2)
#define BIT_TFNF			(0x00000001 << 1)
#define BIT_BUSY			(0x00000001)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_IMR
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_IMR register  *******************/
#define BIT_TXSIM			(0x00000001 << 9)
#define BIT_ACEIM			(0x00000001 << 8)
#define BIT_BYEIM			(0x00000001 << 7)
#define BIT_WBEIM			(0x00000001 << 6)
#define BIT_FSEIM			(0x00000001 << 5)
#define BIT_RXFIM			(0x00000001 << 4)
#define BIT_RXOIM			(0x00000001 << 3)
#define BIT_RXUIM			(0x00000001 << 2)
#define BIT_TXOIM			(0x00000001 << 1)
#define BIT_TXEIM			(0x00000001)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_ISR
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_ISR register  *******************/
#define BIT_TXSIS			(0x00000001 << 9)
#define BIT_ACEIS			(0x00000001 << 8)
#define BIT_BYEIS			(0x00000001 << 7)
#define BIT_WBEIS			(0x00000001 << 6)
#define BIT_FSEIS			(0x00000001 << 5)
#define BIT_RXFIS			(0x00000001 << 4)
#define BIT_RXOIS			(0x00000001 << 3)
#define BIT_RXUIS			(0x00000001 << 2)
#define BIT_TXOIS			(0x00000001 << 1)
#define BIT_TXEIS			(0x00000001)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_RISR
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_RISR register  *******************/
#define BIT_ACEIR			(0x00000001 << 8)
#define BIT_BYEIR			(0x00000001 << 7)
#define BIT_WBEIR			(0x00000001 << 6)
#define BIT_FSEIR			(0x00000001 << 5)
#define BIT_RXFIR			(0x00000001 << 4)
#define BIT_RXOIR			(0x00000001 << 3)
#define BIT_RXUIR			(0x00000001 << 2)
#define BIT_TXOIR			(0x00000001 << 1)
#define BIT_TXEIR			(0x00000001)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_CTRLR2
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_CTRLR2 register  *******************/
#define BIT_FIFO_ENTRY(x)		(((x) & 0x0000000f) << 4)
#define BIT_WR_SEQ			(0x00000001 << 3)
#define BIT_WPN_DNUM			(0x00000001 << 2) /* Indicate the WPn input pin of SPI Flash is connected to, 0(default): WP=spi_sout[2], 1:WP=spi_sout[3]. */
#define BIT_WPN_SET			(0x00000001 << 1) /* To implement write protect function. spi_wen_out and the bit of spi_sout which connects to WPN would be initial to 0. */
#define BIT_SO_DUM			(0x00000001) /* SO input pin of SPI Flash, 0: SO connects to spi_sout[0]. 1(default): SO connects to spi_sout[1].*/
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_ADDR_LENGTH
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_ADDR_LENGTH register  *******************/
#define BIT_ADDR_PHASE_LENGTH(x)	((x) & 0x00000007)
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_AUTO_LENGTH
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_AUTO_LENGTH register  *******************/
#define BIT_CS_H_WR_DUM_LEN(x)		(((x) & 0x0000000f) << 28)
#define BIT_CS_H_RD_DUM_LEN(x)		(((x) & 0x00000003) << 26)
#define BIT_AUTO_DUM_LEN(x)		(((x) & 0x000000ff) << 18)
#define BIT_AUTO_ADDR_LENGTH(x)		(((x) & 0x00000003) << 16)
#define BIT_RD_DUMMY_LENGTH(x)		(((x) & 0x00000fff))
/** @} */

/**************************************************************************//**
 * @defgroup SPIC_VALID_CMD
 * @{
 *****************************************************************************/
/********************  Bits definition for SPIC_VALID_CMD register  *******************/
#define BIT_CTRLR0_CH			(0x00000001 << 12)
#define BIT_PRM_EN			(0x00000001 << 11)
#define BIT_WR_BLOCKING			(0x00000001 << 9)
#define BIT_WR_QUAD_II			(0x00000001 << 8)
#define BIT_WR_QUAD_I			(0x00000001 << 7)
#define BIT_WR_DUAL_II			(0x00000001 << 6)
#define BIT_WR_DUAL_I			(0x00000001 << 5)
#define BIT_RD_QUAD_IO			(0x00000001 << 4)
#define BIT_RD_QUAD_O			(0x00000001 << 3)
#define BIT_RD_DUAL_IO			(0x00000001 << 2)
#define BIT_RD_DUAL_I			(0x00000001 << 1)
#define BIT_FRD_SINGEL			(0x00000001)
#define SPIC_VALID_CMD_MASK		(0x7fff)

#define DUAL_PRM_CYCLE_NUM		4
#define QUAD_PRM_CYCLE_NUM		2
/** @} */

/** @defgroup LEDC_Transfer_Mode_definitions
  * @{
  */
#define LEDC_CPU_MODE			(0)
#define LEDC_DMA_MODE			(1)
#define IS_LEDC_TRANS_MODE(MODE)	(((MODE) == LEDC_CPU_MODE ||\
					(MODE) == LEDC_DMA_MODE))
/**
  * @}
  */

/** @defgroup LEDC_FIFO_Level_definitions
  * @{
  */
#define LEDC_FIFO_DEPTH			(32)
#define LEDC_DEFAULT_FIFO_TRIG_LEVEL	(15)
/**
  * @}
  */

/** @defgroup LEDC_Idle_Level_definitions
  * @{
  */
#define LEDC_IDLE_POLARITY_LOW		(0)
#define LEDC_IDLE_POLARITY_HIGH		(1)
/**
  * @}
  */

/** @defgroup LEDC_RGB_Mode_definitions default input GRB mode
  * @{
  */
#define LEDC_OUTPUT_GRB			(0)
#define LEDC_OUTPUT_GBR			(1)
#define LEDC_OUTPUT_RGB			(2)
#define LEDC_OUTPUT_BGR			(3)
#define LEDC_OUTPUT_RBG			(4)
#define LEDC_OUTPUT_BRG			(5)

#define IS_LEDC_OUTPUT_MODE(MODE)	((MODE) == LEDC_OUTPUT_GRB || \
					(MODE) == LEDC_OUTPUT_GBR || \
					(MODE) == LEDC_OUTPUT_RGB || \
					(MODE) == LEDC_OUTPUT_BGR || \
					(MODE) == LEDC_OUTPUT_RBG || \
					(MODE) == LEDC_OUTPUT_BRG)
/**
  * @}
  */

/** @defgroup LEDC_Input_Order_definitions
  * @{
  */
#define LEDC_INPUT_LSB			(0)
#define LEDC_INPUT_MSB			(1)
#define IS_LEDC_INPUT_ORDER(ORDER)	((ORDER) == LEDC_INPUT_LSB || \
					(ORDER) == LEDC_INPUT_MSB)
/**
  * @}
  */

/** @defgroup LEDC_Output_Order_definitions
  * @{
  */
#define LEDC_OUTPUT_ORDER_MASK		((u32)0x0000001C)
#define LEDC_OUTPUT_ORDER(x)		((u32)((x) & 0x00000007) << 2)
/**
  * @}
  */

/** @defgroup LEDC_Interrupt_definitions
  * @{
  */
#define LEDC_INT_ALL			((u32)0x0000001F)
#define LEDC_INT_EXT_EN			((u32)0x0000001D)

#define IS_LEDC_INTERRUPT(INT)		((INT) <= LEDC_INT_ALL)
/**
  * @}
  */

/** @defgroup LEDC_Data_Length_definitions
  * @{
  */
#define LEDC_MAX_DATA_LENGTH		(0x2000)

#define IS_LEDC_DATA_LENGTH(LENGTH)	((LENGTH > 0) && ((LENGTH) <= LEDC_MAX_DATA_LENGTH))
/**
  * @}
  */
#define LEDC_TRANS_FAIL_ATTEMPS		300

/** @defgroup LEDC_LED_Num_definitions
  * @{
  */
#define LEDC_DEFAULT_LED_NUM		(32)
#define LEDC_MAX_LED_NUM		(1024)
#define LEDC_TO_LINUX_LED_NUM		(1)

#define IS_LEDC_LED_NUM(NUM)		((NUM > 0) && (NUM <= LEDC_MAX_LED_NUM))
/**
  * @}
  */

/***********************************************************************/
/************************** Software Layer *****************************/
/***********************************************************************/

struct rtk_ws28xxx_ledc_init {
	u32				led_count;
	u32				data_length;
	u32				t1h_ns;
	u32				t1l_ns;
	u32				t0h_ns;
	u32				t0l_ns;
	u32				reset_ns;
	u32				wait_data_time_ns;
	u32				wait_time0_en;
	u32				wait_time1_en;
	u32				wait_time0_ns;
	u32				wait_time1_ns;
	u32				output_RGB_mode;
	u32				ledc_polarity;
	u32				ledc_trans_mode;
	u32				ledc_fifo_level;
	u32				auto_dma_thred;
};

struct rtk_ws28xxx_led {
	struct fwnode_handle		*fwnode;
	struct led_classdev		ldev;
	struct rtk_ws28xxx_led_priv	*priv;
	u8				line;
	bool				active;
};

struct rtk_ledc_cpu_parameters {
	u32				*pattern_data_buff;
	u32				*data_start_p;
	u32				*data_current_p;
	int				left_u32;
	int				u32_in_total;
	struct device			*cpu_dev;
	bool				cpu_trans_done;
};

struct rtk_ledc_dmac_parameters {
	struct dma_slave_config		*config;
	struct dma_chan			*chan;
	struct dma_async_tx_descriptor	*txdesc;
	dma_addr_t			dma_buf_phy_addr;
	u8				*dma_buf_addr;
	u32				dma_length;
	bool				gdma_done;
	int				gdma_loop;
	int				gdma_count;
	u32				sent_len;
	struct device			*dma_dev;
};

struct rtk_ws28xxx_led_priv {
	struct rtk_ws28xxx_led		leds[LEDC_TO_LINUX_LED_NUM];
	struct regmap			*regmap;
	struct mutex			lock;
	void __iomem			*base;
	u32				ledc_phy_addr;
	struct clk			*clock;
	int				irq;
	u32				irq_result;
	struct rtk_ws28xxx_ledc_init	ledc_params;
	struct rtk_ledc_dmac_parameters	dma_params;
	struct rtk_ledc_cpu_parameters	cpu_params;
	struct device *dev;
};

#ifndef ENABLE
#define ENABLE				1
#endif
#ifndef DISABLE
#define DISABLE				0
#endif

#define RESULT_COMPLETE			1
#define RESULT_ERR			2
#define RESULT_RUNNING			3

void rtk_ledc_dma_done_callback(void *data);
void rtk_ledc_gdma_tx_periodic(struct rtk_ledc_dmac_parameters *dma_params);


