// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Pinctrl support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __PINCTRL_REALTEK_H
#define __PINCTRL_REALTEK_H

#include <linux/kernel.h>
#include <linux/pinctrl/pinctrl.h>

	/**************************************************************************//**
	 * @defgroup REG_GPIOx
	 * @brief GPIO group control
	 * @{
	 *****************************************************************************/
#define PAD_BIT_GPIOx_PD_SLP           ((u32)0x00000001 << 17)          /*!<R/W 0h  PAD pull down enable when system is in sleep. */
#define PAD_BIT_GPIOx_PU_SLP           ((u32)0x00000001 << 16)          /*!<R/W 0h  PAD pull up enable when system is in sleep. */
#define PAD_BIT_GPIOx_SR               ((u32)0x00000001 << 13)          /*!<R/W 1h  PAD srew rate control */
#define PAD_BIT_GPIOx_SMT              ((u32)0x00000001 << 12)          /*!<R/W 1h  PAD Schmit control */
#define PAD_BIT_GPIOx_E2               ((u32)0x00000001 << 11)          /*!<R/W 1h  PAD driving abilitity control. 0: low 1: high The acture driving current is depend on pad type. */
#define PAD_BIT_GPIOx_PUPDC            ((u32)0x00000001 << 10)          /*!<R/W 0h  Some pad may have two type of PU/PD resistor, this bit can select it. 1: small resistor 0: big resistor */
#define PAD_BIT_GPIOx_PD               ((u32)0x00000001 << 9)          /*!<R/W 0h  PAD pull down enable when system is in active. */
#define PAD_BIT_GPIOx_PU               ((u32)0x00000001 << 8)          /*!<R/W 0h  PAD pull up enable when system is in active. */
#define PAD_MASK_GPIOx_SEL             ((u32)0x0000001F << 0)          /*!<R/W/ES 0h  PAD pinmux func id select */
#define PAD_GPIOx_SEL(x)               ((u32)(((x) & 0x0000001F) << 0))
#define PAD_GET_GPIOx_SEL(x)           ((u32)(((x >> 0) & 0x0000001F)))

/**************************************************************************//**
 * @defgroup REG_PAD_AUD_PAD_CTRL
 * @brief
 * @{
 *****************************************************************************/
#define PAD_MASK_GPIOC_IE              ((u32)0x001FFFFF << 0)          /*!<R/W 0h  Audio share pad input enable. */
#define PAD_GPIOC_IE(x)                ((u32)(((x) & 0x001FFFFF) << 0))
#define PAD_GET_GPIOC_IE(x)            ((u32)(((x >> 0) & 0x001FFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup REG_SWD_SDD_CTRL
 * @brief
 * @{
 *****************************************************************************/
#define PAD_BIT_SDD_PMUX_EN            ((u32)0x00000001 << 2)          /*!<R/W 0   */
#define PAD_BIT_SWD_LOC                ((u32)0x00000001 << 1)          /*!<R/W 0  SWD GPIO Location 0: GPIO_A 3/4 1: GPIO_G 0/1 */
#define PAD_BIT_SWD_PMUX_EN            ((u32)0x00000001 << 0)          /*!<R/W 1  1: Eanble SWD pinmux enable function 0: Disable SWD_LOC is valid only when this bit is 1 */
/** @} */

/*==========PAD Register Address Definition==========*/
#define REG_PAD_AUD_PAD_CTRL                         0x0120
#define REG_SWD_SDD_CTRL                             0x01F8

/** @defgroup PINMUX_PAD_DrvStrength_definitions
  * @{
  */
#define PAD_DRV_ABILITITY_LOW			(0)
#define PAD_DRV_ABILITITY_HIGH			(1)

/** @defgroup PINMUX_PAD_SlewRate_definitions
  * @{
  */
#define PAD_Slew_Rate_LOW			(0)
#define PAD_Slew_Rate_HIGH			(1)

/** @defgroup GPIO_Pull_parameter_definitions
  * @{
  */
#define GPIO_PuPd_NOPULL		0x0 /*!< GPIO Interrnal HIGHZ */
#define GPIO_PuPd_DOWN			0x1 /*!< GPIO Interrnal Pull DOWN */
#define GPIO_PuPd_UP			0x2 /*!< GPIO Interrnal Pull UP */
#define GPIO_PuPd_SHUTDOWN		0x3 /*!< GPIO Interrnal PAD shutdown */

#define PA_BASE					0x00
#define PB_BASE					0x20
#define PC_BASE					0x40

#define REALTEK_GET_PIN_NO(x) ((x) >> 8)
#define REALTEK_GET_PIN_FUNC(x) ((x) & 0xff)

#define REALTEK_PINCTRL_PIN(bank, pin)		\
	PINCTRL_PIN(P ## bank ## _BASE + (pin), "P" #bank #pin)

#define REALTEK_PIN(_pin, ...)					\
	{							\
		.pin = _pin,					\
		.functions = (struct realtek_desc_function[]){	\
			__VA_ARGS__, { } },			\
	}

#define REALTEK_FUNCTION(_val, _name)				\
	{							\
		.name = _name,					\
		.func_id = _val,					\
	}

struct realtek_desc_function {
	unsigned long	variant;
	const char	*name;
	u8		func_id;
	u8		irqbank;
	u8		irqnum;
};

struct realtek_desc_pin {
	struct pinctrl_pin_desc		pin;
	struct realtek_desc_function	*functions;
};

struct realtek_pinctrl_match_data {
	const struct realtek_desc_pin *pins;
	const unsigned int npins;
};

extern int realtek_pctl_probe(struct platform_device *pdev);
#endif /* __PINCTRL_REALTEK_H */
