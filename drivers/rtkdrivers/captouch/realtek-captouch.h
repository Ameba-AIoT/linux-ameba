// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Captouch support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __REALTEK_CAPTOUCH_H
#define __REALTEK_CAPTOUCH_H

/**************************************************************************//**
 * @defgroup CT_CTC_CTRL
 * @brief Captouch Control Register
 * @{
 *****************************************************************************/
#define CT_BIT_BASELINE_INI              ((u32)0x00000001 << 8)          /*!<R/W/EC 1h  1: Baseline initial function enable, HW will clear this bit to "0" after baseline initial */
#define CT_MASK_DEBOUNCE_TIME            ((u32)0x00000003 << 5)          /*!<R/W 0h  2'b00: 2 times 2'b01: 3 times 2'b10: 4 times 2'b11: 5 times */
#define CT_DEBOUNCE_TIME(x)              ((u32)(((x) & 0x00000003) << 5))
#define CT_GET_DEBOUNCE_TIME(x)          ((u32)(((x >> 5) & 0x00000003)))
#define CT_BIT_DEBOUNCE_EN               ((u32)0x00000001 << 4)          /*!<R/W 0h  0: Disable 1: Enable */
#define CT_BIT_ENABLE                    ((u32)0x00000001 << 0)          /*!<R/W 0h  0: CTC disable 1: CTC enable */
/** @} */

/**************************************************************************//**
 * @defgroup CT_SCAN_PERIOD
 * @brief Scan Parameters Register
 * @{
 *****************************************************************************/
#define CT_MASK_SAMPLE_AVE_CTRL          ((u32)0x00000007 << 16)          /*!<R/W 6h  ADC sampled number for average function average number=2^(sample_ave_ctrl+2) */
#define CT_SAMPLE_AVE_CTRL(x)            ((u32)(((x) & 0x00000007) << 16))
#define CT_GET_SAMPLE_AVE_CTRL(x)        ((u32)(((x >> 16) & 0x00000007)))
#define CT_MASK_SCAN_INTERVAL            ((u32)0x00000FFF << 0)          /*!<R/W 3Ch  Code: 0~4095 (0~0xFFF), unit is 1.024KHz cycle (1/32 32.768KHz). When this register is set to 0 or 1, HW will scan continuously and have no sleep time. Recommend value: 60~480ms */
#define CT_SCAN_INTERVAL(x)              ((u32)(((x) & 0x00000FFF) << 0))
#define CT_GET_SCAN_INTERVAL(x)          ((u32)(((x >> 0) & 0x00000FFF)))
/** @} */

/**************************************************************************//**
 * @defgroup CT_ETC_CTRL
 * @brief Environment Tracking Control Register
 * @{
 *****************************************************************************/
#define CT_MASK_BASELINE_UPD_STEP        ((u32)0x0000000F << 12)          /*!<R/W 1h  Baseline update step for all channel */
#define CT_BASELINE_UPD_STEP(x)          ((u32)(((x) & 0x0000000F) << 12))
#define CT_GET_BASELINE_UPD_STEP(x)      ((u32)(((x >> 12) & 0x0000000F)))
#define CT_MASK_BASELINE_WT_FACTOR       ((u32)0x0000000F << 8)          /*!<R/W 2h  Baseline update weight factor (ETC_factor_th) for all channel ETC_factor_th=2^baseline_wt_factor */
#define CT_BASELINE_WT_FACTOR(x)         ((u32)(((x) & 0x0000000F) << 8))
#define CT_GET_BASELINE_WT_FACTOR(x)     ((u32)(((x >> 8) & 0x0000000F)))
#define CT_MASK_ETC_SCAN_INTERVAL        ((u32)0x0000007F << 1)          /*!<R/W 2h  ETC update interval between scan period (sleep time) for all channel Interval=(etc_scan_interval+1)*scan_period */
#define CT_ETC_SCAN_INTERVAL(x)          ((u32)(((x) & 0x0000007F) << 1))
#define CT_GET_ETC_SCAN_INTERVAL(x)      ((u32)(((x >> 1) & 0x0000007F)))
#define CT_BIT_ETC_FUNC_CTRL             ((u32)0x00000001 << 0)          /*!<R/W 0h  Environmental cap tracking calibration function 0: Disable 1: Enable */
/** @} */

/**************************************************************************//**
* @defgroup CT_SNR_INF
* @brief Signal Noise Ratio Register
* @{
*****************************************************************************/
#define CT_BIT_SNR_UPD_ALWAYS            ((u32)0x00000001 << 31)          /*!<R/W 0  1: Update SNR info no matter touch or not (used only in debug function) */
#define CT_MASK_SNR_NOISE_DATA           ((u32)0x00000FFF << 16)          /*!<R/W/ES 0  Noise peak to peak signal raw data for SNR monitor */
#define CT_SNR_NOISE_DATA(x)             ((u32)(((x) & 0x00000FFF) << 16))
#define CT_GET_SNR_NOISE_DATA(x)         ((u32)(((x >> 16) & 0x00000FFF)))
#define CT_MASK_SNR_TOUCH_DATA           ((u32)0x00000FFF << 0)          /*!<R/W/ES 0h  Raw data of touch(signal-baseline)for SNR monitor */
#define CT_SNR_TOUCH_DATA(x)             ((u32)(((x) & 0x00000FFF) << 0))
#define CT_GET_SNR_TOUCH_DATA(x)         ((u32)(((x >> 0) & 0x00000FFF)))
/** @} */

/**************************************************************************//**
 * @defgroup CT_DEBUG_MODE_CTRL
 * @brief Debug and Channel Swith Mode Register
 * @{
 *****************************************************************************/
#define CT_MASK_CH_CTRL                  ((u32)0x0000000F << 5)          /*!<R/W 0h  Scan channel control 0000: channel 0 0001: channel 1 0010: channel 2 0011: channel 3 0100: channel 4 0101: channel 5 0110: channel 6 0111: channel 7 1000: channel 8 (guard sensor) */
#define CT_CH_CTRL(x)                    ((u32)(((x) & 0x0000000F) << 5))
#define CT_GET_CH_CTRL(x)                ((u32)(((x >> 5) & 0x0000000F)))
#define CT_BIT_CH_SWITCH_CTRL            ((u32)0x00000001 << 4)          /*!<R/W 1h  Scan channel switch control 0: Auto switch function disable (manual switch by ch_ctrl) 1: Auto switch to next channel (channel 0 --> channel 3 --> channel 0) */
#define CT_BIT_DEBUG_EN                  ((u32)0x00000001 << 0)          /*!<R/W 0  Debug mode enable 0: Disable 1: Enable */
/** @} */

/**************************************************************************//**
* @defgroup CT_RAW_CODE_FIFO_STATUS
* @brief FIFO Status Register
* @{
*****************************************************************************/
#define CT_MASK_AFIFO_VALID_CNT          ((u32)0x00000007 << 4)          /*!<R 0h  Raw code FIFO valid cnt(push unit number which can be read) */
#define CT_AFIFO_VALID_CNT(x)            ((u32)(((x) & 0x00000007) << 4))
#define CT_GET_AFIFO_VALID_CNT(x)        ((u32)(((x >> 4) & 0x00000007)))
#define CT_BIT_AFIFO_EMPTY               ((u32)0x00000001 << 1)          /*!<R 1  0: Raw code FIFO not empty 1: Raw code FIFO empty */
#define CT_BIT_AFIFO_FULL                ((u32)0x00000001 << 0)          /*!<R 0  0: Raw code FIFO not full 1: Raw code FIFO full */
/** @} */

/**************************************************************************//**
* @defgroup CT_RAW_CODE_FIFO_READ
* @brief FIFO Read Register
* @{
*****************************************************************************/
#define CT_BIT_AFIFO_RD_DATA_VLD         ((u32)0x00000001 << 31)          /*!<R 0h  Read data from raw code FIFO valid */
#define CT_MASK_AFIFO_RD_DATA            ((u32)0x00000FFF << 0)          /*!<RP 0h  Read data from raw code FIFO */
#define CT_AFIFO_RD_DATA(x)              ((u32)(((x) & 0x00000FFF) << 0))
#define CT_GET_AFIFO_RD_DATA(x)          ((u32)(((x >> 0) & 0x00000FFF)))
/** @} */

/**************************************************************************//**
 * @defgroup CT_INTERRUPT_ENABLE
 * @brief Interrupt Enable Register
 * @{
 *****************************************************************************/
#define CT_BIT_SCAN_END_INTR_EN          ((u32)0x00000001 << 21)          /*!<R/W 0h  Scan end interrupt enable */
#define CT_BIT_GUARD_RELEASE_INTR_EN     ((u32)0x00000001 << 20)          /*!<R/W 0h  Guard sensor release enable */
#define CT_BIT_GUARD_PRESS_INTR_EN       ((u32)0x00000001 << 19)          /*!<R/W 0h  Guard sensor press enable */
#define CT_BIT_OVER_N_NOISE_TH_INTR_EN   ((u32)0x00000001 << 18)          /*!<R/W 0h  Negative noise threshold overflow enable */
#define CT_BIT_AFIFO_OVERFLOW_INTR_EN    ((u32)0x00000001 << 17)          /*!<R/W 0h  Raw code FIFO overflow enable */
#define CT_BIT_OVER_P_NOISE_TH_INTR_EN   ((u32)0x00000001 << 16)          /*!<R/W 0h  Positive noise threshold overflow enable */
#define CT_MASK_TOUCH_RELEASE_INTR_EN    ((u32)0x000000FF << 8)          /*!<R/W 0h  Channelx single touch release enable, x is 0~7 */
#define CT_TOUCH_RELEASE_INTR_EN(x)      ((u32)(((x) & 0x000000FF) << 8))
#define CT_GET_TOUCH_RELEASE_INTR_EN(x)  ((u32)(((x >> 8) & 0x000000FF)))
#define CT_MASK_TOUCH_PRESS_INTR_EN      ((u32)0x000000FF << 0)          /*!<R/W 0h  Channelx single touch press enable, x is 0~7 */
#define CT_TOUCH_PRESS_INTR_EN(x)        ((u32)(((x) & 0x000000FF) << 0))
#define CT_GET_TOUCH_PRESS_INTR_EN(x)    ((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup CT_INTERRUPT_STATUS
 * @brief interrupt status register
 * @{
 *****************************************************************************/
#define CT_BIT_GUARD_RELEASE_INTR        ((u32)0x00000001 << 20)          /*!<R 0h  Guard sensor release interrupt */
#define CT_BIT_GUARD_PRESS_INTR          ((u32)0x00000001 << 19)          /*!<R 0h  Guard sensor press interrupt */
#define CT_BIT_OVER_N_NOISE_TH_INTR      ((u32)0x00000001 << 18)          /*!<R 0h  Negative noise threshold overflow interrupt */
#define CT_BIT_AFIFO_OVERFLOW_INTR       ((u32)0x00000001 << 17)          /*!<R 0h  Raw code FIFO overflow interrupt */
#define CT_BIT_OVER_P_NOISE_TH_INTR      ((u32)0x00000001 << 16)          /*!<R 0h  Positive noise threshold overflow interrupt */
#define CT_MASK_TOUCH_RELEASE_INTR       ((u32)0x000000FF << 8)          /*!<R 0h  Channelx single touch release interrupt, x is 0~7 */
#define CT_TOUCH_RELEASE_INTR(x)         ((u32)(((x) & 0x000000FF) << 8))
#define CT_GET_TOUCH_RELEASE_INTR(x)     ((u32)(((x >> 8) & 0x000000FF)))
#define CT_MASK_TOUCH_PRESS_INTR         ((u32)0x000000FF << 0)          /*!<R 0h  Channelx single touch press interrupt, x is 0~7 */
#define CT_TOUCH_PRESS_INTR(x)           ((u32)(((x) & 0x000000FF) << 0))
#define CT_GET_TOUCH_PRESS_INTR(x)       ((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup CT_INTERRUPT_ALL_CLR
 * @brief Interrupt All Clear Register
 * @{
 *****************************************************************************/
#define CT_BIT_INTR_ALL_CLR              ((u32)0x00000001 << 0)          /*!<R/WA0 0h  Write "1" to this register to clear the combined interrupts, all individual interrupts, interrupt status register and raw interrupt status register. */
/** @} */

/**************************************************************************//**
 * @defgroup CT_CHx_CTRL
 * @brief Channel x Control Register
 * @{
 *****************************************************************************/
#define CT_MASK_CHx_D_TOUCH_TH           ((u32)0x00000FFF << 16)          /*!<R/W 64h  Difference threshold data of touch judgement for channel x. It needs to be configured during development, (0x0~0xFFF) (0~4095). Init number=0x1E Recommend data=80%*(signal-baseline) */
#define CT_CHx_D_TOUCH_TH(x)             ((u32)(((x) & 0x00000FFF) << 16))
#define CT_GET_CHx_D_TOUCH_TH(x)         ((u32)(((x >> 16) & 0x00000FFF)))
#define CT_MASK_CHx_BASELINE             ((u32)0x00000FFF << 4)          /*!<R/W/ES 0h  Digital baseline data of channel x. Init number=0x0 It can be initialed by HW when baseline initial function enabled, and can be updated automatically by HW when ETC function enabled. It could be configured by manual tuning. */
#define CT_CHx_BASELINE(x)               ((u32)(((x) & 0x00000FFF) << 4))
#define CT_GET_CHx_BASELINE(x)           ((u32)(((x >> 4) & 0x00000FFF)))
#define CT_BIT_CHx_EN                    ((u32)0x00000001 << 0)          /*!<R/W 0h  Cap sensor activity control of channel x 0: Disable 1: Enable */
/** @} */

/**************************************************************************//**
 * @defgroup CT_CHx_NOISE_TH
 * @brief Channel x Noise Threshold Register
 * @{
 *****************************************************************************/
#define CT_MASK_CHx_N_ENT                ((u32)0x00000FFF << 16)          /*!<R/W 28h  The environmental negative noise threshold: the negative maximum capacitance change of raw data that is still considered an environment change. */
#define CT_CHx_N_ENT(x)                  ((u32)(((x) & 0x00000FFF) << 16))
#define CT_GET_CHx_N_ENT(x)              ((u32)(((x >> 16) & 0x00000FFF)))
#define CT_MASK_CHx_P_ENT                ((u32)0x00000FFF << 0)          /*!<R/W 28h  The environmental positive noise threshold: the positive maximum capacitance change of raw data that is still considered an environment change. */
#define CT_CHx_P_ENT(x)                  ((u32)(((x) & 0x00000FFF) << 0))
#define CT_GET_CHx_P_ENT(x)              ((u32)(((x >> 0) & 0x00000FFF)))
/** @} */

/**************************************************************************//**
 * @defgroup CT_CHx_MBIAS_ATH
 * @brief Channel x Mbias
 * @{
 *****************************************************************************/
#define CT_MASK_CHx_A_TOUCH_TH           ((u32)0x00000FFF << 16)          /*!<R/W/ES 0h  Absolute threshold data of touch judgement for channel x. It can be updated by HW when baseline initial function or ETC function enabled, a_touch_th=baseline-d_touch_th, manual tuning when ETC disabled, (0x0~0xFFF). Recommended data=80%*signal */
#define CT_CHx_A_TOUCH_TH(x)             ((u32)(((x) & 0x00000FFF) << 16))
#define CT_GET_CHx_A_TOUCH_TH(x)         ((u32)(((x >> 16) & 0x00000FFF)))
#define CT_MASK_CHx_MBIAS                ((u32)0x0000003F << 0)          /*!<R/W 20h  Touch mbias current [5:0] 8u/4u/2u/1u/0.5u/0.25u for channel x */
#define CT_CHx_MBIAS(x)                  ((u32)(((x) & 0x0000003F) << 0))
#define CT_GET_CHx_MBIAS(x)              ((u32)(((x >> 0) & 0x0000003F)))
/** @} */

/**************************************************************************//**
* @defgroup CT_CHx_DATA_INF
* @brief Channel x Dtouch Register
* @{
*****************************************************************************/
#define CT_BIT_CHx_DIFF_DATA_POLARITY    ((u32)0x00000001 << 28)          /*!<R/W/ES 0h  Polarity of chx_diff_data 0: chx_data_ave<baseline 1: chx_data_ave>=baseline */
#define CT_MASK_CHx_DIFF_DATA            ((u32)0x00000FFF << 16)          /*!<R/W/ES 0h  Difference digital data of channelx between chx_data_ave and baseline */
#define CT_CHx_DIFF_DATA(x)              ((u32)(((x) & 0x00000FFF) << 16))
#define CT_GET_CHx_DIFF_DATA(x)          ((u32)(((x >> 16) & 0x00000FFF)))
#define CT_MASK_CHx_DATA_AVE             ((u32)0x00000FFF << 0)          /*!<R/W/ES 0h  Average of channel x raw code */
#define CT_CHx_DATA_AVE(x)               ((u32)(((x) & 0x00000FFF) << 0))
#define CT_GET_CHx_DATA_AVE(x)           ((u32)(((x >> 0) & 0x00000FFF)))
/** @} */

/* Registers for captouch */
#define RTK_CT_CTC_CTRL						0x000 /*!< CAPTOUCH CONTROL REGISTER */
#define RTK_CT_SCAN_PERIOD					0x004 /*!< SCAN PARAMETERS REGISTER */
#define RTK_CT_ETC_CTRL					0x008 /*!< ENVIRONMENT TRACKING CONTROL REGISTER */
#define RTK_CT_SNR_INF						0x00C /*!< SIGNAL NOISE RATIO REGISTER */
#define RTK_CT_DEBUG_MODE_CTRL			0x010 /*!< DEBUG AND CHANNEL SWITH MODE REGISTER */
#define RTK_CT_RAW_CODE_FIFO_STATUS			0x014 /*!< FIFO STATUS REGISTER */
#define RTK_CT_RAW_CODE_FIFO_READ			0x018 /*!< FIFO READ REGISTER */
#define RTK_CT_INTERRUPT_ENABLE				0x020 /*!< INTERRUPT ENABLE REGISTER */
#define RTK_CT_INTERRUPT_STATUS				0x024 /*!< INTERRUPT STATUS REGISTER */
#define RTK_CT_RAW_INTERRUPT_STATUS			0x028 /*!< RAW INTERRUPT STATUS REGISTER */
#define RTK_CT_INTERRUPT_ALL_CLR			0x030 /*!< INTERRUPT ALL CLEAR REGISTER */
#define RTK_CT_INTERRUPT_STATUS_CLR			0x034 /*!< INTERRUPT CLEAR REGISTER */
#define RTK_CT_DEBUG_SEL					0x040 /*!< DEBUG SELERTK_CT REGISTER */
#define RTK_CT_DEBUG_PORT					0x044 /*!< DEBUG REGISTER */
#define RTK_CT_ECO_USE0						0x048 /*!< ECO USE0 REGISTER */
#define RTK_CT_ECO_USE1						0x04C /*!< ECO USE1 REGISTER */
#define RTK_CT_CTC_COMP_VERSION			0x050 /*!< VERSION REGISTER */
#define RTK_CT_CHx_CTRL						0x100 /*!< CHANNEL x CONTROL REGISTER */
#define RTK_CT_CHx_NOISE_TH					0x104 /*!< CHANNEL x NOISE THRESHOLD REGISTER */
#define RTK_CT_CHx_MBIAS_ATH				0x108 /*!< CHANNEL x MBIAS Register */
#define RTK_CT_CHx_DATA_INF					0x10C /*!< CHANNEL x DTOUCH REGISTER */
#define RTK_CT_ANA_ADC_REG0X_LPAD			0x400 /*!< ANALOG ADC REG0X_LPAD REGISTER */
#define RTK_CT_ANA_ADC_REG1X_LPAD			0x404 /*!< ANALOG ADC REG1X_LPAD REGISTER */
#define RTK_CT_ANA_ADC_REG0X_LPSD			0x408 /*!< ANALOG ADC REG0X_LPSD REGISTER */
#define RTK_CT_ANA_ADC_TIME_SET				0x40C /*!< ANALOG ADC TIME REGISTER */

/* Definitions for captouch */
#define  CT_CHANNEL_NUM					9
/* Definitions for captouch interrupt  */
#define  CT_CHX_PRESS_INT(x)			((u32)0x00000001 << (x))
#define  CT_CHX_RELEASE_INT(x)			((u32)0x00000001 << ((x) + 8))

#define CT_ALL_INT_EN					(CT_MASK_TOUCH_PRESS_INTR_EN | \
											CT_MASK_TOUCH_RELEASE_INTR_EN | \
											CT_BIT_OVER_P_NOISE_TH_INTR_EN |\
											CT_BIT_AFIFO_OVERFLOW_INTR_EN |\
											CT_BIT_OVER_N_NOISE_TH_INTR_EN |\
											CT_BIT_GUARD_PRESS_INTR_EN |\
											CT_BIT_GUARD_RELEASE_INTR_EN|\
											CT_BIT_SCAN_END_INTR_EN)

#define IS_CT_INT_EN(IT)				(((IT) & ~CT_ALL_INT_EN) == 0)
#define IS_CT_INT_CLR(IT)				(((IT) & ~CT_ALL_INT_EN) == 0)

/**
 * struct realtek_ct_chinit_para - captouch channel initialization parameters
 * @diff_thr: Difference threshold data of touch judgement for channelx
 *			1. Configured during development; (0x0~0xFFF) (0~4095)
 *			2. Init number=0x0, need to be configured
 *			3. recommend data=80%*(signal-baseline);
 * @mbias_current: Channelx mbias current tuning(sensitivity tuning).
 * @nnoise_thr: Negetive noise threshold of ETC.
 * @pnoise_thr: Positive threshold of ETC.
 * @ch_ena: The channel is enable or not
 */
struct realtek_ct_chinit_para {
	u16 diff_thr;
	u8 mbias_current;
	u16 nnoise_thr;
	u16 pnoise_thr;
	u8 ch_ena;
};

/**
 * struct realtek_ct_init_para - captouch initial parameters
 * @debounce_ena: CapTouch press event Debounce Enable.
 * @sample_cnt: sample count for average function,sample count = 2*exp(sample_cnt+2).
 * @scan_interval: Scan interval of every key.
 * @step: Baseline update setp of ETC.
 * @factor: CapTouch ETC Factor.
 * @etc_scan_interval: Baseline update step of ETC.
 * @ch: Initialization parameters for each channel.
 */
struct realtek_ct_init_para {
	u32 debounce_ena;
	u32 sample_cnt;
	u32 scan_interval;
	u32 step;
	u32 factor;
	u32 etc_scan_interval;
	struct realtek_ct_chinit_para ch[CT_CHANNEL_NUM];

};

/**
 * struct realtek_captouch_data - realtek captouch driver data
 * @dev: captouch device
 * @input: captouch input device
 * @base: control registers base cpu addr
 * @irq: interrupt for this adc instance
 * @adc_clk: adc clock
 * @ctc_clk: captouch clock
 * @lock: spinlock
 */
struct realtek_captouch_data {
	struct device *dev;
	struct input_dev *input;
	struct realtek_ct_chinit_para ch_init[CT_CHANNEL_NUM];
	u32 keycode[CT_CHANNEL_NUM];
	void __iomem *base;
	void __iomem *path_base;
	int irq;
	struct clk *adc_clk;
	struct clk *ctc_clk;
	struct clk *ctc_parent_osc_131k;
	struct clk *ctc_parent_ls_apb;
	struct clk *clk_sl;
	struct pinctrl *pinctrl;
	struct pinctrl_state *ctc_state_active;
	struct pinctrl_state *ctc_state_sleep;
	spinlock_t lock;		/* interrupt lock */
};

extern const struct attribute_group *captouch_configs[];
extern struct realtek_captouch_data *captouch;
extern void realtek_captouch_set_en(bool state);
#endif
