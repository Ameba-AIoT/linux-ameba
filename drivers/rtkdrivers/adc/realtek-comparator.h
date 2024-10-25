// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ADC comparator support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/


#ifndef __REALTEK_COMPARATOR_H
#define __REALTEK_COMPARATOR_H

#include "realtek-adc.h"

/**************************************************************************//**
 * @defgroup COMP_REF_CHx
 * @brief Comparator Channel x Reference Voltage Register
 * @{
 *****************************************************************************/
#define COMP_MASK_REF1_CHx               ((u32)0x0000001F << 16)          /*!<R/W 0h  This field controls the comparator channel x internal reference voltage 1. Vref1 is equal to (the value of bit_comp_ref1)*0.1V. Therefore, when bit_comp_ref1 is 5, Vref1 is 0.5V. */
#define COMP_REF1_CHx(x)                 ((u32)(((x) & 0x0000001F) << 16))
#define COMP_GET_REF1_CHx(x)             ((u32)(((x >> 16) & 0x0000001F)))
#define COMP_MASK_REF0_CHx               ((u32)0x0000001F << 0)          /*!<R/W 0h  This field controls the comparator channel x internal reference voltage 0. Vref0 is equal to (the value of bit_comp_ref0)*0.1V. Therefore, when bit_comp_ref0 is 5, Vref0 is 0.5V. */
#define COMP_REF0_CHx(x)                 ((u32)(((x) & 0x0000001F) << 0))
#define COMP_GET_REF0_CHx(x)             ((u32)(((x >> 0) & 0x0000001F)))
/** @} */

/**************************************************************************//**
 * @defgroup COMP_AUTO_SHUT
 * @brief Comparator Automatic Shut Register
 * @{
 *****************************************************************************/
#define COMP_BIT_AUTO_SHUT               ((u32)0x00000001 << 0)          /*!<R/W 0h  This bit controls the comparator to disable the analog module and mux automatically or not after the analog conversion is done. 0: The analog module and mux will NOT be disabled. 1: The analog module and mux will be disabled automatically after the analog conversion is done. */
/** @} */

/**************************************************************************//**
 * @defgroup COMP_EXT_TRIG_CTRL
 * @brief Comparator External Trigger Control Register
 * @{
 *****************************************************************************/
#define COMP_BIT_EXT_WK_TIMER            ((u32)0x00000001 << 0)          /*!<R/W 0h  This bit controls the external trigger source of general timer enabled or not. If it's enabled, the comparator would execute comparison process when the timer event sends to comparator module. 0: Disable external timer trigger. 1: Enable external timer trigger. */
/** @} */

/**************************************************************************//**
 * @defgroup COMP_EXT_TRIG_TIMER_SEL
 * @brief Comparator External Trigger Timer Select Register
 * @{
 *****************************************************************************/
#define COMP_MASK_EXT_WK_TIMER_SEL       ((u32)0x00000007 << 0)          /*!<R/W 0h  This field defines which timer channel would be used to wake up the comparator. 0: timer module 0 is used as the comparator external trigger source. 1:timer module 1 is used as the comparator external trigger source. ...so on... 7:timer module 7 is used as the comparator external trigger source. */
#define COMP_EXT_WK_TIMER_SEL(x)         ((u32)(((x) & 0x00000007) << 0))
#define COMP_GET_EXT_WK_TIMER_SEL(x)     ((u32)(((x >> 0) & 0x00000007)))
/** @} */

/**************************************************************************//**
 * @defgroup COMP_AUTOSW_EN
 * @brief Comparator Automatic Channel Switch Enable Register
 * @{
 *****************************************************************************/
#define COMP_BIT_AUTOSW_EN               ((u32)0x00000001 << 0)          /*!<R/W 0h  This bit controls the automatic channel switch enabled or disabled. 0: Disable the automatic channel switch. If an automatic channel switch is in process, writing 0 will terminate the automatic channel switch. 1: Enable the automatic channel switch. When setting this bit, an automatic channel switch starts from the first channel in the channel switch list. */
/** @} */

/**************************************************************************//**
 * @defgroup COMP_EN_TRIG
 * @brief Comparator Enable Trigger Register
 * @{
 *****************************************************************************/
#define COMP_BIT_DBG_EN                  ((u32)0x00000001 << 2)          /*!<R/W 0h  The comparator IP debug port enable. */
#define COMP_BIT_EN_TRIG                 ((u32)0x00000001 << 1)          /*!<R/W 0h  This bit controls the analog module and the analog mux of comparator to be enabled or not. Every time this bit is written, the comparator switches to a new channel and does one comparison. After this, it shuts down the analog module according to REG_COMP_AUTO_SHUT. Therefore, this bit can be used as a start-compare event which is controlled by software. Every time a comparison is done, software must clear this bit manually. 0: Disable the analog module and analog mux. 1: Enable the analog module and analog mux, then start a new channel comparison. */
#define COMP_BIT_ENABLE                  ((u32)0x00000001 << 0)          /*!<R/W 0h  The comparator IP internal enable. */
/** @} */

/**************************************************************************//**
 * @defgroup COMP_LPSD
 * @brief Comparator Analog Register
 * @{
 *****************************************************************************/
#define COMP_BIT_LPSD_1                  ((u32)0x00000001 << 1)          /*!<R/W 0h  SD_POSEDGE, 1: Vin > Vref output high */
/** @} */

/* Registers for comparator */
#define RTK_COMP_REF_CHx					0x00 /*!< COMPARATOR CHANNEL x REFERENCE VOLTAGE REGISTER */
#define RTK_COMP_INTR_CTRL					0x10 /*!< COMPARATOR INTERRUPT CONTROL REGISTER */
#define RTK_COMP_WK_STS					0x14 /*!< COMPARATOR WAKEUP ADC/SYS STATUS REGISTER */
#define RTK_COMP_WK_STS_RAW				0x18 /*!< COMPARATOR WAKEUP REGISTER */
#define RTK_COMP_CHSW_LIST				0x1C /*!< COMPARATOR CHANNEL SWITCH LIST REGISTER */
#define RTK_COMP_LAST_CH					0x20 /*!< COMPARATOR LAST CHANNEL REGISTER */
#define RTK_COMP_BUSY_STS					0x24 /*!< COMPARATOR BUSY STATUS REGISTER */
#define RTK_COMP_CH_STS					0x28 /*!< COMPARATOR CHANNEL STATUS REGISTER */
#define RTK_COMP_AUTO_SHUT				0x2C /*!< COMPARATOR AUTOMATIC SHUT REGISTER */
#define RTK_COMP_EXT_TRIG_CTRL			0x30 /*!< COMPARATOR EXTERNAL TRIGGER CONTROL REGISTER */
#define RTK_COMP_EXT_TRIG_TIMER_SEL		0x34 /*!< COMPARATOR EXTERNAL TRIGGER TIMER SELECT REGISTER */
#define RTK_COMP_RST_LIST					0x38 /*!< COMPARATOR RESET CHANNEL LIST REGISTER */
#define RTK_COMP_AUTOSW_EN				0x3C /*!< COMPARATOR AUTOMATIC CHANNEL SWITCH ENABLE REGISTER */
#define RTK_COMP_EN_TRIG					0x40 /*!< COMPARATOR ENABLE TRIGGER REGISTER */
#define RTK_COMP_CTRL_CNT					0x44 /*!< COMPARATOR EXTERNAL WAKE SHUT COUNT REGISTER */
#define RTK_COMP_LPSD						0x48 /*!< COMPARATOR ANALOG REGISTER */


/* Definitions for comparator */
#define COMP_CH_NUM					(4)
/* Definitions for comparator channel list shift */
#define COMP_SHIFT_CHSW(x)				(4*x)
/* Definitions for comparator operation mode */
#define COMP_AUTO_MODE					(0x00)	/*!< comparator automatic mode */
#define COMP_TIM_TRI_MODE				(0x01)	/*!< comparator timer-trigger mode */
#define COMP_SW_TRI_MODE				(0x02)	/*!< comparator software-trigger mode */
/* Definitions for comparator wakeup type */
#define COMP_WK_SYS						((u8)0x02)
#define COMP_WK_ADC						((u8)0x01)
#define COMP_WK_NONE					((u8)0x00)
/* Definitions for comparator wakeup system control */
#define COMP_SHIFT_WK_SYS_CTRL(x)			(17 + 3*x)
#define COMP_MASK_WK_SYS_CTRL(x)			(u32)(0x00000003 << COMP_SHIFT_WK_SYS_CTRL(x))
#define COMP_SHIFT_WK_SYS_EN(x)				(16 + 3*x)
#define COMP_WK_SYS_EN(x)					(u32)(0x00000001 << COMP_SHIFT_WK_SYS_EN(x))
/* Definitions for comparator wakeup adc control */
#define COMP_SHIFT_WK_ADC_CTRL(x)			(1 + 3*x)
#define COMP_MASK_WK_ADC_CTRL(x)			(u32)(0x00000003 << COMP_SHIFT_WK_ADC_CTRL(x))
#define COMP_SHIFT_WK_ADC_EN(x)				(3*x)
#define COMP_WK_ADC_EN(x)					(u32)(0x00000001 << COMP_SHIFT_WK_ADC_EN(x))
/* Definitions for comparator wakeup status mask */
#define COMP_MASK_WK_STS					(u32)(0x02490249)

/**
 * struct realtek_comp_priv - comparator config priv data
 * @mode: comparator operation mode
 * @timer_idx: external timer index select
 * @period: period of trigger timer(ns)
 */
struct realtek_comp_priv {
	u32 mode;
	u32 timer_idx;
	u64 period;
};

/**
 * struct realtek_comp_info - comparator config data
 * @wake_type: wakeup type
 * @wake_sys_ctrl: criteria of when comparator channel should wakeup system
 * @wake_adc_ctrl: criteria of when comparator channel should wakeup adc
 */
struct realtek_comp_info {
	u32 wake_type;
	u32 wake_sys_ctrl;
	u32 wake_adc_ctrl;
};

/**
 * struct realtek_comp_data - realtek comparator driver data
 * @adc: realtek adc driver data
 * @dev: device
 * @base: control registers base cpu addr
 * @irq: interrupt for this adc instance
 * @lock: spinlock
 * @ref0: internal reference voltage0, Vref0 = ref0*0.1v
 * @ref1: internal reference voltage1, Vref1 = ref1*0.1v
 */
struct realtek_comp_data {
	struct realtek_adc_data *adc;
	struct device	*dev;
	void __iomem *base;
	int irq;
	spinlock_t lock;		/* interrupt lock */
	u32 ref0;
	u32 ref1;
};

#endif
