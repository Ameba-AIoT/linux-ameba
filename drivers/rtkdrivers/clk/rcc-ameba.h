// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek RCC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _RCC_AMEBA_H
#define _RCC_AMEBA_H

/******************************
 * Ameda System Definitions *
 *****************************/
#define RTK_SYS_NO							(0x00 << 24)
#define RTK_SYS_AON							(0x01 << 24)
#define RTK_SYS_LS							(0x02 << 24)
#define RTK_SYS_AP							(0x03 << 24)
#define RTK_SYS_HP							(0x04 << 24)

#define RTK_SYS_MASK						(0xFF << 24)

#define RTK_REG_GET_SYS(_reg)				((_reg) & (RTK_SYS_MASK))
#define RTK_REG_IS_SYS_AON(_reg)			(((_reg) & (RTK_SYS_MASK)) == (RTK_SYS_AON))
#define RTK_REG_IS_SYS_LS(_reg)				(((_reg) & (RTK_SYS_MASK)) == (RTK_SYS_LS))
#define RTK_REG_IS_SYS_AP(_reg)				(((_reg) & (RTK_SYS_MASK)) == (RTK_SYS_AP))
#define RTK_REG_IS_SYS_NO(_reg)				(((_reg) & (RTK_SYS_MASK)) == (RTK_SYS_NO))
#define RTK_REG_GET_OFFSET(_reg)			((_reg) & ~(RTK_SYS_MASK))

/******************************
 * Ameda AON RCC registers  *
 *****************************/
#define AMEBA_REG_AON_PWC					(RTK_SYS_AON | 0x00)
#define AMEBA_REG_AON_ISO					(RTK_SYS_AON | 0x04)
#define AMEBA_REG_AON_ROR					(RTK_SYS_AON | 0x06)
#define AMEBA_REG_AON_FEN					(RTK_SYS_AON | 0x08)
#define AMEBA_REG_AON_CLK					(RTK_SYS_AON | 0x0C)
#define AMEBA_REG_AON_CKSL				(RTK_SYS_AON | 0x10)

/* REG_AON_ROR (AON Power Reset Register) */
#define POR_SYSON							0
#define POR_AON_OTP							1

/* REG_AON_FEN (AON Domain Function Enable Register) */
#define FEN_BOR								0
#define FEN_BOR_ACC							1
#define FEN_IWDG							2
#define FEN_ATIM							3
#define FEN_LPON							16
#define FEN_PX								17
#define FEN_RTC								19
#define FEN_OTP								23
#define FEN_SDM32K							26

/* REG_AON_CLK (AON Domain Clock Enable Register) */
#define CKE_AON								0
#define CKE_ATIM							1
#define CKE_RTC								3
#define CKE_LP4M							8
#define CKE_SDM32K							10
#define CKE_OSC4M_FORCE0					24	// OTPC
#define CKE_OSC4M_FORCE1					25	// API XTAL
#define CKE_OSC4M_FORCE2					26	// Periphals
#define CKE_OSC4M_FORCE3					27	// Platform
#define CKE_OSC4M_FORCE4					28	// VAD
#define CKE_OSC4M_FORCE5					29	// WLAN & BT
#define CKE_OSC4M_FORCE6					30	// Reserved for SW
#define CKE_OSC4M_FORCE7					31	// Reserved for SW

/* REG_AON_CKSL (AON Domain Clock Select Register) */
#define CKSL_OTP_SHIFT						0
#define CKSL_OTP_MASK						1
#define CKSL_32K_SHIFT						4
#define CKSL_32K_MASK						2

/******************************
 * Ameda LSYS RCC registers *
 *****************************/
#define AMEBA_REG_LSYS_PWC				(RTK_SYS_LS | 0x00)
#define AMEBA_REG_LSYS_POR				(RTK_SYS_LS | 0x02)
#define AMEBA_REG_LSYS_ISO				(RTK_SYS_LS | 0x04)
#define AMEBA_REG_LSYS_FEN_GRP0			(RTK_SYS_LS | 0x08)
#define AMEBA_REG_LSYS_FEN_GRP1			(RTK_SYS_LS | 0x0C)
#define AMEBA_REG_LSYS_FEN_GRP2			(RTK_SYS_LS | 0x10)
#define AMEBA_REG_LSYS_CKE_GRP0			(RTK_SYS_LS | 0x14)
#define AMEBA_REG_LSYS_CKE_GRP1			(RTK_SYS_LS | 0x18)
#define AMEBA_REG_LSYS_CKE_GRP2			(RTK_SYS_LS | 0x1C)
#define AMEBA_REG_LSYS_CKE_GRP			(RTK_SYS_LS | 0x20)
#define AMEBA_REG_LSYS_CKSL_GRP0			(RTK_SYS_LS | 0x24)
#define AMEBA_REG_LSYS_CKD_GRP0			(RTK_SYS_LS | 0x28)
#define AMEBA_REG_LSYS_CKD_GRP1			(RTK_SYS_LS | 0x2C)
#define AMEBA_REG_SPORT_CLK				(RTK_SYS_LS | 0x4C)

/* REG_LSYS_POR (LSYS Function Power Reset Register) */
#define POR_LP_PLAT							1
#define POR_HP_PLAT							3
#define POR_LP_BTON							9
#define POR_HP_DDRPHY						12

/* REG_LSYS_FEN_GRP0 (LSYS Function Enable Register Group0) */
#define FEN_HPON							1
#define FEN_SIC								2
#define FEN_LPLFM							3
#define FEN_HPLFM							4
#define FEN_LP								5
#define FEN_NP								6
#define FEN_WLON							7
#define FEN_SPIC							8
#define FEN_SCE								9
#define FEN_DTIM							10
#define FEN_THM								11
#define FEN_AIP								12
#define FEN_BTON							13
#define FEN_IPC_LP							16
#define FEN_LOGUART							19
#define FEN_ADC								20
#define FEN_CTC								21
#define FEN_GPIO							23
#define FEN_WLAFE_CTRL						24
#define FEN_I2C0							25
#define FEN_I2C1							26
#define FEN_I2C2							27

/* REG_LSYS_FEN_GRP1 (LSYS Function Enable Register Group1) */
#define FEN_LX1								2
#define FEN_IPSEC							3
#define FEN_LCDC							4
#define FEN_GDMA							5
#define FEN_SPI0							9
#define FEN_SPI1							10
#define FEN_SDH								11
#define FEN_USB								12
#define FEN_TRNG							14
#define FEN_LEDC							15
#define FEN_IPC_HP							18
#define FEN_RSA								19
#define FEN_ED25519							20
#define FEN_ECDSA							21
#define FEN_IRDA							22
#define FEN_DDRC							23
#define FEN_DDRP							24
#define FEN_AC								25
#define FEN_AUDIO							26
#define FEN_PSRAM							27
#define FEN_ZGB								28

/* REG_LSYS_FEN_GRP2 (LSYS Function Enable Register Group2) */
#define FEN_UART0							4
#define FEN_UART1							5
#define FEN_UART2							6
#define FEN_UART3							7
#define FEN_SPORT0							8
#define FEN_SPORT1							9
#define FEN_SPORT2							10
#define FEN_SPORT3							11
#define FEN_TIM0							16
#define FEN_TIM1							17
#define FEN_TIM2							18
#define FEN_TIM3							19
#define FEN_TIM4							20
#define FEN_TIM5							21
#define FEN_TIM6							22
#define FEN_TIM7							23
#define FEN_TIM8							24
#define FEN_TIM9							25
#define FEN_TIM10							26
#define FEN_TIM11							27
#define FEN_TIM12							28
#define FEN_TIM13							29

/* REG_LSYS_CKE_GRP0 (LSYS Clock Enable Register Group0) */
#define CKE_HPON							1
#define CKE_SIC								2
#define CKE_LPLFM							3
#define CKE_HPLFM							4
#define CKE_LP								5
#define CKE_NP								6
#define CKE_SPIC							7
#define CKE_GPIO							8
#define CKE_WLON							9
#define CKE_BTON							10
#define CKE_PX								11
#define CKE_IPC_LP							12
#define CKE_LOGUART							13
#define CKE_CTC								14
#define CKE_ADC								15
#define CKE_THM								17
#define CKE_AIP								18
#define CKE_ED25519							20
#define CKE_ECDSA							21
#define CKE_DTIM							23
#define CKE_I2C0							25
#define CKE_I2C1							26
#define CKE_I2C2							27

/* REG_LSYS_CKE_GRP1 (LSYS Clock Enable Register Group1) */
#define CKE_HPERI							0
#define CKE_LX1								1
#define CKE_IPSEC							2
#define CKE_LCDCMIPI						3
#define CKE_GDMA							4
#define CKE_WMAC							5
#define CKE_SPI0							8
#define CKE_SPI1							9
#define CKE_USB								10
#define CKE_LEDC							11
#define CKE_TRNG							12
#define CKE_SDH								13
#define CKE_IPC_HP							16
#define CKE_IRDA							17
#define CKE_DDRC							18
#define CKE_DDRP							19
#define CKE_AC								24
#define CKE_PSRAM							26
#define CKE_ZGB								27

/* REG_LSYS_CKE_GRP2 (LSYS Clock Enable Register Group2) */
#define CKE_UART0							0
#define CKE_UART1							1
#define CKE_UART2							2
#define CKE_UART3							3
#define CKE_SPORT0							4
#define CKE_SPORT1							5
#define CKE_SPORT2							6
#define CKE_SPORT3							7
#define CKE_TIM0							16
#define CKE_TIM1							17
#define CKE_TIM2							18
#define CKE_TIM3							19
#define CKE_TIM4							20
#define CKE_TIM5							21
#define CKE_TIM6							22
#define CKE_TIM7							23
#define CKE_TIM8							24
#define CKE_TIM9							25
#define CKE_TIM10							26
#define CKE_TIM11							27
#define CKE_TIM12							28
#define CKE_TIM13							29

/* REG_LSYS_CKE_GRP (LSYS Clock Enable Register Group) */
#define CKE_XTAL_GRP0						8	// LP system
#define CKE_XTAL_GRP1						9	// 40M periphals
#define CKE_XTAL_GRP2						10	// 2M periphals
#define CKE_XTAL_GRP3						11	// 40M periphals for NP whem XTAL is in LPS mode
#define CKE_XTAL_LPS_GRP0					16	// Sytem SDM 32K
#define CKE_XTAL_LPS_GRP1					17	// BT LPS clock

/* REG_LSYS_CKSL_GRP0 (LSYS Clock Select Register Group0) */
#define CKSL_LSOC_SHIFT						0
#define CKSL_LSOC_MASK						2
#define CKSL_SPIC_SHIFT						2
#define CKSL_SPIC_MASK						2
#define CKSL_GPIO_SHIFT						4
#define CKSL_GPIO_MASK						1
#define CKSL_ADC_SHIFT						5
#define CKSL_ADC_MASK						1
#define CKSL_CTC_SHIFT						6
#define CKSL_CTC_MASK						1
#define CKSL_NP_SHIFT						8
#define CKSL_NP_MASK						1
#define CKSL_HBUS_SHIFT						10
#define CKSL_HBUS_MASK						1
#define CKSL_LOGUART_SHIFT					11
#define CKSL_LOGUART_MASK					1
#define CKSL_LOGUART2M_SHIFT				12
#define CKSL_LOGUART2M_MASK					1
#define CKSL_AC_SHIFT						13
#define CKSL_AC_MASK						1
#define CKSL_VADM_SHIFT						14
#define CKSL_VADM_MASK						1
#define CKSL_SDM_SHIFT						15
#define CKSL_SDM_MASK						1
#define CKSL_UART0_SHIFT					16
#define CKSL_UART0_MASK						1
#define CKSL_UART1_SHIFT					17
#define CKSL_UART1_MASK						1
#define CKSL_UART2_SHIFT					18
#define CKSL_UART2_MASK						1
#define CKSL_UART2M_SHIFT					19
#define CKSL_UART2M_MASK					1
#define CKSL_XTAL_SHIFT						20
#define CKSL_XTAL_MASK						4
#define CKSL_SPORT0_SHIFT					24
#define CKSL_SPORT0_MASK					1
#define CKSL_SPORT1_SHIFT					25
#define CKSL_SPORT1_MASK					1
#define CKSL_SPORT2_SHIFT					26
#define CKSL_SPORT2_MASK					1
#define CKSL_SPORT3_SHIFT					27
#define CKSL_SPORT3_MASK					1
#define CKSL_ADC2M_SHIFT					28
#define CKSL_ADC2M_MASK						1
#define CKSL_VAD_SHIFT						29
#define CKSL_VAD_MASK						1
#define CKSL_HIPC_SHIFT						30
#define CKSL_HIPC_MASK						1
#define CKSL_PSRAM_SHIFT					31
#define CKSL_PSRAM_MASK						1

/* REG_LSYS_CKD_GRP0 (LSYS Clock Divider Register Group0) */
#define CKD_NP_SHIFT						0
#define CKD_NP_WIDTH						3
#define CKD_PLFM_SHIFT						4
#define CKD_PLFM_WIDTH						3
#define CKD_PSRAM_SHIFT						8
#define CKD_PSRAM_WIDTH						3
#define CKD_HBUS_SHIFT						12
#define CKD_HBUS_WIDTH						4
#define CKD_AC_SHIFT						16
#define CKD_AC_WIDTH						4
#define CKD_HPERI_SHIFT						20
#define CKD_HPERI_WIDTH						3
#define CKD_MIPI_SHIFT						24
#define CKD_MIPI_WIDTH						6

/* REG_LSYS_CKD_GRP1 (LSYS Clock Divider Register Group1) */
#define CKD_XTAL_2M_EN						0

/* REG_SPORT_CLK (LSYS SPORT Clock) */
#define CKSL_I2S0_SHIFT						0
#define CKSL_I2S0_MASK						2
#define CKSL_I2S1_SHIFT						2
#define CKSL_I2S1_MASK						2
#define CKSL_I2S2_SHIFT						4
#define CKSL_I2S2_MASK						2
#define CKSL_I2S3_SHIFT						6
#define CKSL_I2S3_MASK						2
#define CKD_SPORT0_SHIFT					16
#define CKD_SPORT0_WIDTH					3
#define CKD_SPORT1_SHIFT					20
#define CKD_SPORT1_WIDTH					3
#define CKD_SPORT2_SHIFT					24
#define CKD_SPORT2_WIDTH					3
#define CKD_SPORT3_SHIFT					28
#define CKD_SPORT3_WIDTH					3

/*******************************
 * Ameda CA32 RCC registers *
 ******************************/
#define AMEBA_REG_APLL_BASE			(RTK_SYS_AP | 0x100)


#define REG_APLL_CTRL0		0x0
#define REG_APLL_CTRL1		0x04
#define REG_APLL_CTRL5		0x14

#define APLL_POW_PI_BIT		BIT(3)
#define APLL_POW_PLL_BIT	BIT(0)
#define APLL_CK_RDY_BIT		BIT(31)
#define APLL_SDM_DIVM_MSK	(0xff << 0)


/*******************************
 * Ameda HP RCC registers *
 ******************************/
#define AMEBA_REG_HP_CKE				(RTK_SYS_HP | 0xC)
#define AMEBA_REG_HP_CKSL				(RTK_SYS_HP | 0x10)
#define AMEBA_REG_DUMMY_1E0			(RTK_SYS_HP | 0x1E0)

/* AMEBA_REG_HP_CKE */
#define CKE_AP_CPU							0

/* AMEBA_REG_HP_CKSL */
#define CKSL_AP_CPU_SHIFT					2
#define CKSL_AP_CPU_MASK					1
#define CKD_AP_CPU_SHIFT					0
#define CKD_AP_CPU_WIDTH					2


/* AMEBA_REG_DUMMY_1E0 */
#define CKSL_TIM9_SHIFT		2
#define CKSL_TIM9_MASK		1

/* Ameda PLL registers */

/* Ameda SDM registers */

/* Ameda Regulator registers */

/* Ameda SWR registers */

/* Ameda XTAL registers */

#endif /* _RCC_AMEBA_H */
