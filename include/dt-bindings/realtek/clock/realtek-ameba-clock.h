// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Clock support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _DT_BINDINGS_REALTEK_AMEBA_CLOCK_H
#define _DT_BINDINGS_REALTEK_AMEBA_CLOCK_H

/* 
 * AON clocks
 */

/* Gate clocks */
#define RTK_CKE_ATIM			0	/* CKE_ATIM */
#define RTK_CKE_RTC				1	/* CKE_RTC */
#define RTK_CKE_LP4M			2	/* CKE_LP4M, only for RTL8730E */
#define RTK_CKE_SDM32K			3	/* CKE_SDM32K */
#define RTK_CKE_OSC4M_FORCE0	4	/* CKE_OSC4M_FORCE[0]: OSC 4M clock used for OTPC, only for RTL8730E */
#define RTK_CKE_OSC4M_FORCE1	5	/* CKE_OSC4M_FORCE[1]: OSC 4M clock used for AIP XTAL, only for RTL8730E */
#define RTK_CKE_OSC4M_FORCE2	6	/* CKE_OSC4M_FORCE[2]: OSC 4M clock used for peripherals, only for RTL8730E */
#define RTK_CKE_OSC4M_FORCE3	7	/* CKE_OSC4M_FORCE[3]: OSC 4M clock used for platform, only for RTL8730E */
#define RTK_CKE_OSC4M_FORCE4	8	/* CKE_OSC4M_FORCE[4]: OSC 4M clock used for VAD, only for RTL8730E */
#define RTK_CKE_OSC4M_FORCE5	9	/* CKE_OSC4M_FORCE[5]: OSC 4M clock used for WLAN / BT, only for RTL8730E */
#define RTK_CKE_OSC4M_FORCE6	10	/* CKE_OSC4M_FORCE[6]: OSC 4M clock used for SW reserved, only for RTL8730E */
#define RTK_CKE_OSC4M_FORCE7	11	/* CKE_OSC4M_FORCE[7]: OSC 4M clock used for SW reserved, only for RTL8730E */

/* MUX clocks */
#define RTK_CKSL_OTP			20	/* CKSL_OTP */
#define RTK_CKSL_32K			21	/* CKSL_32K */

/*
 * LSYS clocks
 */

/* Gate clocks */
#define RTK_CKE_HPON			30	/* CKE_HPON */
#define RTK_CKE_SIC				31	/* CKE_SIC */
#define RTK_CKE_LPLFM			32	/* CKE_LPLFM */
#define RTK_CKE_HPLFM			33	/* CKE_HPLFM */
#define RTK_CKE_LP				34	/* CKE_LP */
#define RTK_CKE_NP				35	/* CKE_NP */
#define RTK_CKE_SPIC			36	/* CKE_SPIC */
#define RTK_CKE_GPIO			37	/* CKE_GPIO */
#define RTK_CKE_WLON			38	/* CKE_WLON */
#define RTK_CKE_BTON			39	/* CKE_BTON */
#define RTK_CKE_PX				40	/* CKE_PX */
#define RTK_CKE_IPC_LP			41	/* CKE_IPC_LP */
#define RTK_CKE_LOGUART			42	/* CKE_LOGUART */
#define RTK_CKE_CTC				43	/* CKE_CTC */
#define RTK_CKE_ADC				44	/* CKE_ADC */
#define RTK_CKE_THM				45	/* CKE_THM */
#define RTK_CKE_AIP				46	/* CKE_AIP */
#define RTK_CKE_ED25519			47	/* CKE_ED25519 */
#define RTK_CKE_ECDSA			48	/* CKE_ECDSA */
#define RTK_CKE_DTIM			49	/* CKE_DTIM */
#define RTK_CKE_I2C0			50	/* CKE_I2C0 */
#define RTK_CKE_I2C1			51	/* CKE_I2C1 */
#define RTK_CKE_I2C2			52	/* CKE_I2C2 */
#define RTK_CKE_HPERI			53	/* CKE_HPERI */
#define RTK_CKE_LX1				54	/* CKE_LX1 */
#define RTK_CKE_IPSEC			55	/* CKE_IPSEC */
#define RTK_CKE_LCDCMIPI		56	/* CKE_LCDCMIPI */
#define RTK_CKE_GDMA			57	/* CKE_GDMA */
#define RTK_CKE_WMAC			58	/* CKE_WMAC */
#define RTK_CKE_SPI0			59	/* CKE_SPI0 */
#define RTK_CKE_SPI1			60	/* CKE_SPI1 */
#define RTK_CKE_USB				61	/* CKE_USB */
#define RTK_CKE_LEDC			62	/* CKE_LEDC */
#define RTK_CKE_TRNG			63	/* CKE_TRNG */
#define RTK_CKE_SDH				64	/* CKE_SDH */
#define RTK_CKE_IPC_HP			65	/* CKE_IPC_HP */
#define RTK_CKE_IRDA			66	/* CKE_IRDA */
#define RTK_CKE_DDRC			67	/* CKE_DDRC */
#define RTK_CKE_DDRP			68	/* CKE_DDRP */
#define RTK_CKE_AC				69	/* CKE_AC */
#define RTK_CKE_PSRAM			70	/* CKE_PSRAM */
#define RTK_CKE_ZGB				71	/* CKE_ZGB */
#define RTK_CKE_UART0			72	/* CKE_UART[0] */
#define RTK_CKE_UART1			73	/* CKE_UART[1] */
#define RTK_CKE_UART2			74	/* CKE_UART[2] */
#define RTK_CKE_UART3			75	/* CKE_UART[3] */
#define RTK_CKE_SPORT0			76	/* CKE_SPORT[0] */
#define RTK_CKE_SPORT1			77	/* CKE_SPORT[1] */
#define RTK_CKE_SPORT2			78	/* CKE_SPORT[2] */
#define RTK_CKE_SPORT3			79	/* CKE_SPORT[3] */
#define RTK_CKE_TIM0			80	/* CKE_TIM[0] */
#define RTK_CKE_TIM1			81	/* CKE_TIM[1] */
#define RTK_CKE_TIM2			82	/* CKE_TIM[2] */
#define RTK_CKE_TIM3			83	/* CKE_TIM[3] */
#define RTK_CKE_TIM4			84	/* CKE_TIM[4] */
#define RTK_CKE_TIM5			85	/* CKE_TIM[5] */
#define RTK_CKE_TIM6			86	/* CKE_TIM[6] */
#define RTK_CKE_TIM7			87	/* CKE_TIM[7] */
#define RTK_CKE_TIM8			88	/* CKE_TIM[8], only for RTL8730E */
#define RTK_CKE_TIM9			89	/* CKE_TIM[9], only for RTL8730E */
#define RTK_CKE_TIM10			90	/* CKE_TIM[10], only for RTL8730E */
#define RTK_CKE_TIM11			91	/* CKE_TIM[11], only for RTL8730E */
#define RTK_CKE_TIM12			92	/* CKE_TIM[12], only for RTL8730E */
#define RTK_CKE_TIM13			93	/* CKE_TIM[13], only for RTL8730E */
#define RTK_CKE_XTAL_GRP0		94	/* CKE_XTAL_GRP[0]: XTAL 40M clock used for LP system */
#define RTK_CKE_XTAL_GRP1		95	/* CKE_XTAL_GRP[1]: XTAL 40M clock used for 40M peripherals */
#define RTK_CKE_XTAL_GRP2		96	/* CKE_XTAL_GRP[2]: XTAL 40M clock used for 2M peripherals */
#define RTK_CKE_XTAL_GRP3		97	/* CKE_XTAL_GRP[3]: XTAL 40M clock used for NP when XTAL in LPS mode */
#define RTK_CKE_XTAL_LPS_GRP0	98	/* CKE_XTAL_LPS_GRP[0]: XTAL LPS clock used for system SDM 32K */
#define RTK_CKE_XTAL_LPS_GRP1	99	/* CKE_XTAL_LPS_GRP[1]: XTAL LPS clock used for BT LPS clock */

/* MUX clocks */
#define RTK_CKSL_LSOC			130	/* CKSL_LSOC */
#define RTK_CKSL_SPIC			131	/* CKSL_SPIC */
#define RTK_CKSL_GPIO			132	/* CKSL_GPIO */
#define RTK_CKSL_ADC			133	/* CKSL_ADC */
#define RTK_CKSL_CTC			134	/* CKSL_CTC */
#define RTK_CKSL_NP				135	/* CKSL_NP */
#define RTK_CKSL_HBUS			136	/* CKSL_HBUS, only for RTL8730E */
#define RTK_CKSL_LOGUART		137	/* CKSL_LOGUART, only for RTL8730E */
#define RTK_CKSL_LOGUART2M		138	/* CKSL_LOGUART2M, only for RTL8730E */
#define RTK_CKSL_AC				139	/* CKSL_AC */
#define RTK_CKSL_VADM			140	/* CKSL_VADM */
#define RTK_CKSL_SDM			141	/* CKSL_SDM */
#define RTK_CKSL_UART0			142	/* CKSL_UART[0] */
#define RTK_CKSL_UART1			143	/* CKSL_UART[1] */
#define RTK_CKSL_UART2			144	/* CKSL_UART[2] */
#define RTK_CKSL_UART2M			145	/* CKSL_UART2M */
#define RTK_CKSL_XTAL			146	/* CKSL_XTAL */
#define RTK_CKSL_PSRAM			147	/* CKSL_PSRAM */
#define RTK_CKSL_ADC2M			148	/* CKSL_ADC2M, only for RTL8730E */
#define RTK_CKSL_VAD			149	/* CKSL_VAD, only for RTL8730E */
#define RTK_CKSL_HIPC			150 /* CKSL_HIPC, only for RTL8730E */
#define RTK_CKSL_I2S0			151	/* CKSL_I2S0 */
#define RTK_CKSL_I2S1			152	/* CKSL_I2S1 */
#define RTK_CKSL_I2S2			153	/* CKSL_I2S2 */
#define RTK_CKSL_I2S3			154	/* CKSL_I2S3 */
#define RTK_CKSL_SPORT0			155	/* CKSL_SPORT[0] */
#define RTK_CKSL_SPORT1			156	/* CKSL_SPORT[1] */
#define RTK_CKSL_SPORT2			157	/* CKSL_SPORT[2] */
#define RTK_CKSL_SPORT3			158	/* CKSL_SPORT[3] */

/* Divider clocks */
#define RTK_CKD_NP				180	/* CKD_NP */
#define RTK_CKD_PLFM			181	/* CKD_PLFM */
#define RTK_CKD_PSRAM			182	/* CKD_PSRAM */
#define RTK_CKD_HBUS			183 /* CKD_HBUS, only for RTL8730E */
#define RTK_CKD_AC				184 /* CKD_AC, only for RTL8730E */
#define RTK_CKD_HPERI			185 /* CKD_HPERI, only for RTL8730E */
#define RTK_CKD_MIPI			186 /* CKD_MIPI, only for RTL8730E */
#define RTK_CKD_SPORT0			187	/* CKD_SPORT0 */
#define RTK_CKD_SPORT1			188	/* CKD_SPORT1 */
#define RTK_CKD_SPORT2			189	/* CKD_SPORT2 */
#define RTK_CKD_SPORT3			190	/* CKD_SPORT3 */

/*
 * LSYS clocks
 */

#define RTK_CKD_AP				200	/* CKD_AP */
#define RTK_CKSL_AP				201	/* CKSL_AP */
#define RTK_CLK_AP_PLL			202	/* CLK_APLL */
#define RTK_CKE_AP				203 /* CKE_AP */

/*
 * hSYS clocks
 */

/* MUX clocks */
#define RTK_CKSL_TIM9			204

/*
 * Fixed-factor clocks
 */

#define RTK_CLK_LBUS			220	/* LS_APB_CLK */
#define RTK_CLK_LAPB			221	/* LS_APB_CLK */
#define RTK_CLK_HAPB			222	/* HP_APB_CLK */
#define RTK_CLK_HBUS			223	/* HBUS_CLK */
#define RTK_CLK_PLFM			224 /* PLFM_CLK */


#define RTK_CLK_MAX_NUM			RTK_CLK_PLFM

#endif /* _DT_BINDINGS_REALTEK_AMEBA_CLOCK_H */
