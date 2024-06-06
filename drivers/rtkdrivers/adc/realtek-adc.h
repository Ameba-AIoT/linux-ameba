// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ADC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __REALTEK_ADC_H
#define __REALTEK_ADC_H

/**************************************************************************//**
 * @defgroup RTK_ADC_CONF
 * @brief ADC Configuration Register
 * @{
 *****************************************************************************/
#define ADC_BIT_ENABLE                      ((u32)0x00000001 << 9)          /*!<R/W 0h  This bit is for ADC enable control */
#define ADC_MASK_CVLIST_LEN                 ((u32)0x0000000F << 4)          /*!<R/W 0h  This field defines the number of items in the ADC conversion channel list. */
#define ADC_CVLIST_LEN(x)                   ((u32)(((x) & 0x0000000F) << 4))
#define ADC_GET_CVLIST_LEN(x)               ((u32)(((x >> 4) & 0x0000000F)))
#define ADC_MASK_OP_MOD                     ((u32)0x00000007 << 1)          /*!<R/W 0h  These bits selects ADC operation mode. 0: software trigger mode. 1:automatic mode. 2:timer-trigger mode. 3:comparator-assist mode */
#define ADC_OP_MOD(x)                       ((u32)(((x) & 0x00000007) << 1))
#define ADC_GET_OP_MOD(x)                   ((u32)(((x >> 1) & 0x00000007)))
#define ADC_BIT_REF_IN_SEL                  ((u32)0x00000001 << 0)          /*!<R/W 0h  This bit selects ADC reference voltage input. 0: ADC's reference voltage is from internal source. 1:ADC's reference voltage is from external pin. */
/** @} */


/**************************************************************************//**
 * @defgroup ADC_AUTO_CSW_CTRL
 * @brief ADC Automatic Channel Switch Control Register
 * @{
 *****************************************************************************/
#define ADC_BIT_AUTO_CSW_EN                 ((u32)0x00000001 << 0)          /*!<R/W 0h  This bit controls the automatic channel switch enabled or disabled. 0: Disable the automatic channel switch.If an automatic channel switch is in process, writing 0 will terminate the automatic channel switch. 1: Enable the automatic channel switch. When setting this bit, an automatic channel switch starts from the first channel in the channel switch list. */
/** @} */

/**************************************************************************//**
 * @defgroup ADC_SW_TRIG
 * @brief ADC Software Trigger Register
 * @{
 *****************************************************************************/
#define ADC_BIT_SW_TRIG                     ((u32)0x00000001 << 0)          /*!<R/W 0h  This bit controls the ADC module to do a conversion. Every time this bit is set to 1, ADC module would switch to a new channel and do one conversion. Therefore, this bit could be used as a start-convert event which is controlled by software. Every time a conversion is done, software MUST clear this bit manually. The interval between clearing and restart this bit must exceed one sample clock period. 0: disable the analog module and analog mux. 1: enable the analog module and analog mux. And then start a new channel conversion. */
/** @} */

/**************************************************************************//**
 * @defgroup ADC_BUSY_STS
 * @brief ADC Busy Status Register
 * @{
 *****************************************************************************/
#define ADC_BIT_FIFO_EMPTY                  ((u32)0x00000001 << 2)          /*!<R 1  0: FIFO in ADC not empty; 1: FIFO in ADC empty */
#define ADC_BIT_FIFO_FULL_REAL              ((u32)0x00000001 << 1)          /*!<R 0  0: FIFO in ADC not full real; 1: FIFO in ADC full real */
#define ADC_BIT_BUSY_STS                    ((u32)0x00000001 << 0)          /*!<R 0  This bit reflects the ADC is busy or not. If the ADC is processing a conversion of a channel, this bit remains 1 which indicates it's busy. Once a conversion is done, this bit becomes 0 which indicates it's ready to do another conversion. 0: The ADC is ready. 1: The ADC is busy. */
/** @} */

/**************************************************************************//**
 * @defgroup ADC_INTR_CTRL
 * @brief ADC Interrupt Control Register
 * @{
 *****************************************************************************/
#define ADC_BIT_IT_COMP_CH9_EN              ((u32)0x00000001 << 17)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 9 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH8_EN              ((u32)0x00000001 << 16)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 8 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH7_EN              ((u32)0x00000001 << 15)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 7 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH6_EN              ((u32)0x00000001 << 14)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 6 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH5_EN              ((u32)0x00000001 << 13)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 5 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH4_EN              ((u32)0x00000001 << 12)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 4 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH3_EN              ((u32)0x00000001 << 11)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 3 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH2_EN              ((u32)0x00000001 << 10)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 2 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH1_EN              ((u32)0x00000001 << 9)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 1 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_COMP_CH0_EN              ((u32)0x00000001 << 8)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when channel 0 comparison criterion matches. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_ERR_EN                   ((u32)0x00000001 << 7)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when an error state takes place. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_DAT_OVW_EN               ((u32)0x00000001 << 6)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when a data overwritten situation takes place. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_FIFO_EMPTY_EN            ((u32)0x00000001 << 5)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when a FIFO empty state takes place. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_FIFO_OVER_EN             ((u32)0x00000001 << 4)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when a FIFO overflow state takes place. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_FIFO_FULL_EN             ((u32)0x00000001 << 3)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when a FIFO full state takes place. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_CHCV_END_EN              ((u32)0x00000001 << 2)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when a particular channel conversion is done. Please refer to reg_adc_it_chno_con 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_CV_END_EN                ((u32)0x00000001 << 1)          /*!<R/W 0h  This bit controls the interrupt is enabled or not every time a conversion is done. No matter ADC module is in what kind of operation mode. Every time a conversion is executed, ADC module would notify system if This bit is set. 0: This interrupt is disabled. 1: This interrupt is enabled. */
#define ADC_BIT_IT_CVLIST_END_EN            ((u32)0x00000001 << 0)          /*!<R/W 0h  This bit controls the interrupt is enabled or not when a conversion of the last channel in the list is done. For example, in automatic mode conversions would be executed continuously. Every time the last channel conversion is done, which means all channel conversions in the list is done, ADC could notify system if This bit is set. 0: This interrupt is disabled. 1: This interrupt is enabled. */
/** @} */

/**************************************************************************//**
 * @defgroup ADC_INTR_STS
 * @brief ADC Interrupt Status Register
 * @{
 *****************************************************************************/
#define ADC_BIT_IT_COMP_CH9_STS             ((u32)0x00000001 << 17)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH8_STS             ((u32)0x00000001 << 16)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH7_STS             ((u32)0x00000001 << 15)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH6_STS             ((u32)0x00000001 << 14)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH5_STS             ((u32)0x00000001 << 13)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH4_STS             ((u32)0x00000001 << 12)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH3_STS             ((u32)0x00000001 << 11)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH2_STS             ((u32)0x00000001 << 10)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH1_STS             ((u32)0x00000001 << 9)          /*!<RW1C 0h   */
#define ADC_BIT_IT_COMP_CH0_STS             ((u32)0x00000001 << 8)          /*!<RW1C 0h   */
#define ADC_BIT_IT_ERR_STS                  ((u32)0x00000001 << 7)          /*!<RW1C 0h   */
#define ADC_BIT_IT_DAT_OVW_STS              ((u32)0x00000001 << 6)          /*!<RW1C 0h   */
#define ADC_BIT_IT_FIFO_EMPTY_STS           ((u32)0x00000001 << 5)          /*!<RW1C 0h   */
#define ADC_BIT_FIFO_OVER_STS               ((u32)0x00000001 << 4)          /*!<RW1C 0h   */
#define ADC_BIT_FIFO_FULL_STS               ((u32)0x00000001 << 3)          /*!<RW1C 0h   */
#define ADC_BIT_CHCV_END_STS                ((u32)0x00000001 << 2)          /*!<RW1C 0h   */
#define ADC_BIT_CV_END_STS                  ((u32)0x00000001 << 1)          /*!<RW1C 0h   */
#define ADC_BIT_CVLIST_END_STS              ((u32)0x00000001 << 0)          /*!<RW1C 0h   */
/** @} */

/**************************************************************************//**
 * @defgroup ADC_DATA_GLOBAL
 * @brief ADC Global Data Register
 * @{
 *****************************************************************************/
#define ADC_MASK_DAT_CH                     ((u32)0x0000000F << 18)          /*!<R 0  This field indicates which channel data is procedd right now */
#define ADC_DAT_CH(x)                       ((u32)(((x) & 0x0000000F) << 18))
#define ADC_GET_DAT_CH(x)                   ((u32)(((x >> 18) & 0x0000000F)))
#define ADC_BIT_DAT_RDY_GLOBAL              ((u32)0x00000001 << 17)          /*!<R 0  This bit indicates that a conversion is done. Every time a conversion is done, this bit should be set to 1 and it would be cleared to 0 when a read operation of reg_adc_data_global */
#define ADC_BIT_DAT_OVW_GLOBAL              ((u32)0x00000001 << 16)          /*!<R 0  This bit indicates that there is a data overwritten situation in bit_adc_data_global takes place. A data overwritten situation is that a former conversion data is NOT read before a new conversion is written to data field. 0: there is no data overwritten case. 1: there is a data overwritten case. */
#define ADC_MASK_DATA_GLOBAL                ((u32)0x0000FFFF << 0)          /*!<R 0  This field contains the newsest conversion data of channel in the list. [15:12]: which channel the data belongs to, only valid when bit_adc_ch_unmask=1 [11:0]: newest data */
#define ADC_DATA_GLOBAL(x)                  ((u32)(((x) & 0x0000FFFF) << 0))
#define ADC_GET_DATA_GLOBAL(x)              ((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup ADC_FLR
 * @brief ADC FIFO Level Register
 * @{
 *****************************************************************************/
#define ADC_MASK_FLR                        ((u32)0x0000001F << 0)          /*!<R 0  This field records the current ADC FIFO entry number. */
#define ADC_FLR(x)                          ((u32)(((x) & 0x0000001F) << 0))
#define ADC_GET_FLR(x)                      ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup RTK_ADC_CLK_DIV
 * @brief ADC Clock Divider Register
 * @{
 *****************************************************************************/
#define ADC_MASK_CLK_DIV                    ((u32)0x00000007 << 0)          /*!<R/W 3h  This field defines clock driver level of ADC module. A value of 0 is for clock divided by 2. A value of 1 is for clock divided by 4. A value of 2 is for clock divided by 8. A value of 3 is for clock divided by 12. A value of 4 is for clock divided by 16. A value of 5 is for clock divided by 32. A value of 6 is for clock divided by 64. */
#define ADC_CLK_DIV(x)                      ((u32)(((x) & 0x00000007) << 0))
#define ADC_GET_CLK_DIV(x)                  ((u32)(((x >> 0) & 0x00000007)))
/** @} */

/**************************************************************************//**
 * @defgroup ADC_DELAY_CNT
 * @brief ADC Delay Count Register
 * @{
 *****************************************************************************/
#define ADC_BIT_CH_UNMASK                   ((u32)0x00000001 << 31)          /*!<R/W 0  Enable BIT_ADC_DAT_CHID */
/** @} */

/* Registers for ADC */
#define RTK_ADC_CONF					0x00 /*!< ADC CONFIGURATION REGISTER */
#define RTK_ADC_IN_TYPE				0x04 /*!< ADC INPUT TYPE REGISTER */
#define RTK_ADC_COMP_TH_CHx			0x08 /*!< ADC CHANNEL 0 COMPARISON THRESHOLD REGISTER */
#define RTK_ADC_COMP_CTRL				0x30 /*!< ADC COMPARISON CONTROL REGISTER */
#define RTK_ADC_COMP_STS				0x34 /*!< ADC COMPARISON STATUS REGISTER */
#define RTK_ADC_CHSW_LIST_0			0x38 /*!< ADC CHANNEL SWITCH LIST 0 REGISTER */
#define RTK_ADC_CHSW_LIST_1			0x3C /*!< ADC CHANNEL SWITCH LIST 1 REGISTER */
#define RTK_ADC_RST_LIST				0x40 /*!< ADC RESET CHANNEL LIST REGISTER */
#define RTK_ADC_AUTO_CSW_CTRL		0x44 /*!< ADC AUTOMATIC CHANNEL SWITCH CONTROL REGISTER */
#define RTK_ADC_SW_TRIG				0x48 /*!< ADC SOFTWARE TRIGGER REGISTER */
#define RTK_ADC_LAST_CH				0x4C /*!< ADC LAST CHANNEL REGISTER */
#define RTK_ADC_BUSY_STS				0x50 /*!< ADC BUSY STATUS REGISTER */
#define RTK_ADC_INTR_CTRL				0x54 /*!< ADC INTERRUPT CONTROL REGISTER */
#define RTK_ADC_INTR_RAW_STS			0x58 /*!< ADC INTERRUPT RAW STATUS REGISTER */
#define RTK_ADC_INTR_STS				0x5C /*!< ADC INTERRUPT STATUS REGISTER */
#define RTK_ADC_IT_CHNO_CON			0x60 /*!< ADC INTERRUPT CHANNEL NUMBER CONFIGURATION REGISTER */
#define RTK_ADC_FULL_LVL				0x64 /*!< ADC FIFO FULL THRESHOLD REGISTER */
#define RTK_ADC_EXT_TRIG_TIMER_SEL	0x68 /*!< ADC EXTERNAL TRIGGER TIMER SELECT REGISTER */
#define RTK_ADC_DATA_CHx				0x6C /*!< ADC CHANNEL 0 DATA REGISTER */
#define RTK_ADC_DATA_GLOBAL			0x94 /*!< ADC GLOBAL DATA REGISTER */
#define RTK_ADC_FLR					0x98 /*!< ADC FIFO LEVEL REGISTER */
#define RTK_ADC_CLR_FIFO				0x9C /*!< ADC CLEAR FIFO REGISTER */
#define RTK_ADC_CLK_DIV				0xA0 /*!< ADC CLOCK DIVIDER REGISTER */
#define RTK_ADC_DELAY_CNT				0xA4 /*!< ADC DELAY COUNT REGISTER */
#define RTK_ADC_PWR_CTRL				0xA8 /*!< ADC POWER CONTROL REGISTER */

/* Definitions for ADC */
#define RTK_ADC_CH_NUM					(7)
#define RTK_ADC_MAX_DIFF_CHAN_PAIRS		(3)
#define RTK_ADC_BUF_SIZE					(32)
#define ADC_CH_BIT(x)				((u32)0x00000001 << (x))
#define ADC_MASK_CHSW                     			(0xF)
#define ADC_SHIFT_CHSW0(x)				(4*x)
#define ADC_SHIFT_CHSW1(x)				(4*(x - 8))
/* Definitions for ADC operation mode */
#define ADC_SW_TRI_MODE				(0x00)	/*!< ADC software-trigger mode */
#define ADC_AUTO_MODE					(0x01)	/*!< ADC automatic mode */
#define ADC_TIM_TRI_MODE				(0x02)	/*!< ADC timer-trigger mode */
#define ADC_COMP_ASSIST_MODE			(0x03)	/*!< ADC comparator-assist mode */

/* ADC calibration data addresses in OTP */
#define ADC_NORMAL_CH_CAL_OTPADDR   	0x704
#define ADC_VBAT_CH_CAL_OTPADDR     	0x70A
#define EFUSE_BUF_LEN					6

/* OTP Calibration offsets macros */
#define OTP_CALIB_COEFF_MAX				0xFFFF
#define OTP_CALIB_NORMAL_ACOEFF_MAX		0xFED9
#define OTP_CALIB_VBAT_ACOEFF_MAX		0xFF1C
#define OTP_CALIB_ACOEFF_DIV_PWR		26
#define OTP_CALIB_ACOEFF_DIVISOR		BIT(OTP_CALIB_ACOEFF_DIV_PWR)
#define OTP_CALIB_NORMAL_BCOEFF_MAX		0x6D6C
#define OTP_CALIB_VBAT_BCOEFF_MAX		0xA4D1
#define OTP_CALIB_BCOEFF_DIV_PWR		15
#define OTP_CALIB_BCOEFF_DIVISOR		BIT(OTP_CALIB_BCOEFF_DIV_PWR)
#define OTP_CALIB_NORMAL_CCOEFF_MAX		0xFD1D
#define OTP_CALIB_VBAT_CCOEFF_MAX		0xFD80
#define OTP_CALIB_CCOEFF_DIV_PWR		6
#define OTP_CALIB_CCOEFF_DIVISOR		BIT(OTP_CALIB_CCOEFF_DIV_PWR)
#define OTP_CALIB_COEFF_SIGN_BIT		0x8000
#define ADC_DIFF_CH_DEFAULT_OFFSET		900	/* Default: 1650 for a,b-cut; 900 for c-cut. */
#define ADC_VBAT_CHANNEL_NUM			6
#define POWER_OF_TEN_MULTIPLIER			1000000000LL
#define ADC_RESOLUTION_BITS				12
#define ADC_STORAGE_BITS				16
#define ADC_NORMAL_CH_SCALE_FACTOR		GENMASK((ADC_RESOLUTION_BITS - 1),0)
#define ADC_VBAT_CH_SCALE_FACTOR		5000


/**
 * struct realtek_adc_info - ADC config data
 * @clk_div: ADC clock divider
 * @rx_level: receive FIFO threshold level
 * @chid_en: specifies whether ADC enables BIT_ADC_DAT_CHID or not
 * @special_ch: ADC particular channel
 * @timer_idx: external timer index select
 * @period: period of trigger timer(ns)
 */
struct realtek_adc_info {
	u32 clk_div;
	u32 rx_level;
	u32 chid_en;
	u32 special_ch;
	u32 timer_idx;
	u64 period;
};

/**
 * struct realtek_adc_diff_channel - ADC differential channels
 * @vinp: positive input channel
 * @vinn: negative input channel
 */
struct realtek_adc_diff_channel {
	u32 vinp;
	u32 vinn;
};

/**
 * struct realtek_adc_data - realtek ADC driver data
 * @base: control registers base cpu addr
 * @comp_base: comparator registers base cpu addr
 * @irq: interrupt for this adc instance
 * @lock: spinlock
 * @mode: adc mode, auto, software-trigger, timer-trigger, comparator-assist
 * @buffer: adc data buffer
 * @buf_index: index
 * @num_conv: expected number of scan conversions
 * @adc_clk: adc clock
 * @ctc_clk: captouch clock
 * @k_coeff_normal: quadritic fitting A, B, C calibration params for normal channels
 * @k_coeff_vbat: quadritic fitting A, B, C calibration params for VBAT channel
 * @cal_offset: calibration offsets for all 7 channels
 * @diff_ch_offset: offset for differential channel which corresponds to 0V
 * input. To be read from OTP once calibration OTP params are added for
 * differential channel mode as well. For now, fixing it to 1650.
 * @is_calibdata_read: bool flag to indicate if calibdata for both normal & VBAT
 *  channels is read out from OTP or not.
 */
struct realtek_adc_data {
	void __iomem *base;
	void __iomem *comp_base;
	void __iomem *path_base;
	struct completion completion;
	int irq;
	spinlock_t lock;		/* interrupt lock */
	int mode;
	u16 buffer[RTK_ADC_BUF_SIZE];
	u32 buf_index;
	u32 num_conv;
	struct clk *adc_clk;
	struct clk *adc_parent_ls_apb;
	struct clk *adc_parent_osc_2m;
	struct clk *clk_sl;

	struct clk *ctc_clk;
	int k_coeff_normal[3];
	int k_coeff_vbat[3];
	int cal_offset[7];
	int diff_ch_offset;
	bool is_calibdata_read;
};

extern void realtek_adc_cmd(struct realtek_adc_data *adc, bool state);
extern void realtek_adc_clear_fifo(struct realtek_adc_data *adc);
#endif
