// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek GPIO support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __GPIO_REALTEK_H
#define __GPIO_REALTEK_H

#include <linux/kernel.h>

/** @defgroup GPIO_Mode_parameter_definitions
  * @{
  */
#define GPIO_Mode_IN			0x0 /*!< GPIO Input Mode */
#define GPIO_Mode_OUT			0x1 /*!< GPIO Output Mode */
#define GPIO_Mode_INT			0x2 /*!< GPIO Interrupt Mode */

/** @defgroup GPIO_Pin_State_definitions
  * @{
  */
#define GPIO_PIN_LOW			0x0 /*!< Pin state is low */
#define GPIO_PIN_HIGH			0x1 /*!< Pin state is high */

#define GPIO_DR					0x0
#define GPIO_DDR				0x4
#define GPIO_CTL				0x8
#define GPIO_INT_EN 			0x30
#define GPIO_INT_MASK 			0x34
#define GPIO_INT_TYPE 			0x38
#define GPIO_INT_POLARITY 		0x3C
#define GPIO_INT_STATUS 		0x40
#define GPIO_INT_STATUS_RAW 	0x44
#define GPIO_DEBOUNCE 			0x48
#define GPIO_INT_EOI 			0x4c
#define GPIO_EXT_PORT 			0x50
#define GPIO_ITN_LS_SYNC 		0x60
#define GPIO_ID_CODE 			0x64
#define GPIO_INT_BOTHEDGE 		0x68
#define GPIO_VER_ID_CODE 		0x6c
#define GPIO_DB_DIV_CONFIG 		0x78

extern int realtek_gpio_probe(struct platform_device *pdev);
#endif /* __GPIO_REALTEK_H */
