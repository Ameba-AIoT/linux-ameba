// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek RCC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>
#include <linux/spinlock.h>

#include <dt-bindings/realtek/clock/realtek-ameba-clock.h>
#ifdef CONFIG_RESET_CONTROLLER
#include <dt-bindings/realtek/reset/realtek-ameba-reset.h>
#endif

#include "rcc-realtek.h"
#include "rcc-ameba.h"

static void __iomem *aon_rcc_base;
static void __iomem *lsys_rcc_base;
static void __iomem *hsys_rcc_base;
static void __iomem *apsys_rcc_base;

static DEFINE_SPINLOCK(ameba_clk_lock);
#ifdef CONFIG_RESET_CONTROLLER
static DEFINE_SPINLOCK(ameba_reset_lock);
#endif

static const char *cksl_otp_parents[2]			= {"OSC_100K", "LBUS_CLK"};
static const char *cksl_32k_parents[4]			= {"CKE_SDM32K", "CKE_SDM32K", "XTAL_32K", "EXT_32K"};
static const char *cksl_lsoc_parents[4]			= {"CKE_LP4M", "CKE_LP4M", "CKE_XTAL_GRP0", "CKE_XTAL_GRP0"};
static const char *cksl_spic_parents[4]			= {"CKE_LP4M", "CKE_LP4M", "CKE_XTAL_GRP0", "NP_SYSPLL"};	// NP_SYSPLL_G3
static const char *cksl_gpio_parents[2]			= {"LAPB_CLK", "CKSL_32K"};
static const char *cksl_adc_parents[2]			= {"LAPB_CLK", "OSC_2M"};
static const char *cksl_ctc_parents[2]			= {"OSC_131K", "LAPB_CLK"};
static const char *cksl_np_parents[2]			= {"CKE_XTAL_GRP1", "CKD_NP"};
static const char *cksl_hbus_parents[2]			= {"CKE_XTAL_GRP1", "CKD_HPERI"};
static const char *cksl_loguart_parents[2]		= {"CKE_XTAL_GRP0", "CKSL_LOGUART2M"};
static const char *cksl_loguart2m_parents[2]	= {"OSC_2M", "CKE_XTAL_GRP2"};
static const char *cksl_ac_parents[2]			= {"CKD_AC", "CKE_XTAL_GRP1"};
static const char *cksl_vadm_parents[2]			= {"CKE_HPLFM", "CKSL_VAD"};
static const char *cksl_sdm_parents[2]			= {"OSC_131K", "XTAL_LPS"};
static const char *cksl_uart_parents[2]			= {"CKE_XTAL_GRP1", "CKSL_UART2M"};
static const char *cksl_uart2m_parents[2]		= {"OSC_2M", "CKE_XTAL_GRP2"};
static const char *cksl_sport0_parents[2]		= {"CKSL_AC", "CKD_SPORT0"};
static const char *cksl_sport1_parents[2]		= {"CKSL_AC", "CKD_SPORT1"};
static const char *cksl_sport2_parents[2]		= {"CKSL_AC", "CKD_SPORT2"};
static const char *cksl_sport3_parents[2]		= {"CKSL_AC", "CKD_SPORT3"};
static const char *cksl_adc2m_parents[2]		= {"OSC_2M", "CKE_XTAL_GRP2"};
static const char *cksl_vad_parents[2]			= {"OSC_4M", "CKE_XTAL_GRP3"};
static const char *cksl_hipc_parents[2]			= {"CLK_HAPB", "CKE_LP4M"};
static const char *cksl_psram_parents[2]		= {"CKE_XTAL_GRP1", "CKD_PSRAM"};
static const char *cksl_i2s_parents[4] 			= {"I2S_PLL0", "I2S_PLL1", "I2S_PLL2", "I2S_PLL2"};
static const char *cksl_ap_cpu_parents[2]		= {"AP_SYSPLL", "NP_SYSPLL"};
static const char *cksl_tim9_parents[2]			= {"CKE_TIM9", "I2S_PLL2"};


static const struct clk_div_table ckd_ac_div_table[] = {
	{ 3, 20 }, { 4, 21 }, { 5, 22 }, { 6, 23 }, { 7, 24 }, { 8, 25 }, { 9, 26 },
	{ 10, 27 }, { 11, 28 }, { 12, 29 }, { 13, 30 }, { 14, 31 }, { 15, 32 }
};

static const struct clk_div_table ckd_ap_cpu_div_table[] = {
	{0, 1}, {1, 2}, {2, 3}, {3, 4}
};

/* AON clocks */
RTK_GATE_CLK(aon_gate_timer,									"CKE_ATIM",				"OSC_100K",
			 AMEBA_REG_AON_CLK,	CKE_ATIM,					RTK_NO_GATE_FLAGS,		AMEBA_REG_AON_FEN,	FEN_ATIM,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_rtc,										"CKE_RTC",				"CKSL_32K",
			 AMEBA_REG_AON_CLK,	CKE_RTC,					RTK_NO_GATE_FLAGS,		AMEBA_REG_AON_FEN,	FEN_RTC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_sdm_32k,									"CKE_SDM32K",			"CKSL_SDM",
			 AMEBA_REG_AON_CLK,	CKE_SDM32K,					RTK_NO_GATE_FLAGS,		AMEBA_REG_AON_FEN,	FEN_SDM32K,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_lp4m,										"CKE_LP4M",				"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_LP4M,					RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force0,								"CKE_OSC4M_FORCE0",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE0,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force1,								"CKE_OSC4M_FORCE1",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE1,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force2,								"CKE_OSC4M_FORCE2",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE2,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force3,								"CKE_OSC4M_FORCE3",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE3,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force4,								"CKE_OSC4M_FORCE4",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE4,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force5,								"CKE_OSC4M_FORCE5",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE5,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force6,								"CKE_OSC4M_FORCE6",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE6,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(aon_gate_osc4m_force7,								"CKE_OSC4M_FORCE7",		"OSC_4M",
			 AMEBA_REG_AON_CLK,	CKE_OSC4M_FORCE7,			RTK_NO_GATE_FLAGS,		0,						0,				CLK_IGNORE_UNUSED);

RTK_MUX_CLK(aon_mux_otp,										"CKSL_OTP",				cksl_otp_parents,
			AMEBA_REG_AON_CKSL,	CKSL_OTP_SHIFT,				CKSL_OTP_MASK,			RTK_NO_MUX_FLAGS,		RTK_NO_FLAGS);
RTK_MUX_CLK(aon_mux_32k,										"CKSL_32K",				cksl_32k_parents,
			AMEBA_REG_AON_CKSL,	CKSL_32K_SHIFT,				CKSL_32K_MASK,			RTK_NO_MUX_FLAGS,		RTK_NO_FLAGS);

/* LSYS clocks */

RTK_GATE_CLK(lsys_gate_hpon,									"CKE_HPON",				"HAPB_CLK",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_HPON,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_HPON,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_sic,										"CKE_SIC",				"LBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_SIC,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_SIC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_lplfm,									"CKE_LPLFM",			"CKE_XTAL_GRP0",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_LPLFM,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_LPLFM,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_hplfm,									"CKE_HPLFM",			"CKE_XTAL_GRP1",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_HPLFM,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_HPLFM,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_lp,										"CKE_LP",				"LBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_LP,					RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_LP,			CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_np,										"CKE_NP",				"CKSL_NP",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_NP,					RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_NP,			CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_spic,									"CKE_SPIC",				"CKSL_SPIC",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_SPIC,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_SPIC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_gpio,									"CKE_GPIO",				"CKSL_GPIO",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_GPIO,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_GPIO,		CLK_IGNORE_UNUSED);
/*
RTK_GATE_CLK(lsys_gate_wlon,									"CKE_WLON",				NULL,	// TBD
	AMEBA_REG_LSYS_CKE_GRP0,	CKE_WLON,						RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_WLON,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_bton,									"CKE_BTON",				NULL,	// TBD
	AMEBA_REG_LSYS_CKE_GRP0,	CKE_BTON,						RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_BTON,		CLK_IGNORE_UNUSED);
*/
RTK_GATE_CLK(lsys_gate_px,										"CKE_PX",				"CKE_LP4M",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_PX,					RTK_NO_GATE_FLAGS,		AMEBA_REG_AON_FEN,		FEN_PX,			CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ipc_lp,									"CKE_IPC_LP",			"CKE_LP4M",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_IPC_LP,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_IPC_LP,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_loguart,									"CKE_LOGUART",			"CKSL_LOGUART",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_LOGUART,			RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_LOGUART,	CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ctc,										"CKE_CTC",				"CKSL_CTC",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_CTC,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_CTC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_adc,										"CKE_ADC",				"CKSL_ADC",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_ADC,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_ADC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_thm,										"CKE_THM",				"LAPB_CLK",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_THM,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_THM,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_aip,										"CKE_AIP",				"OSC_4M",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_AIP,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_AIP,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ed25519,									"CKE_ED25519",			"CKE_HPERI",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_ED25519,			RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_ED25519,	CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ecdsa,									"CKE_ECDSA",			"CKE_HPERI",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_AIP,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_ECDSA,		CLK_IGNORE_UNUSED);
/*
RTK_GATE_CLK(lsys_gate_dtim,									"CKE_DTIM",				NULL,	// TBD: the parent clk is acutally a mux(CKSL_32K, CKE_XTAL_GRP0) within DTIM
	AMEBA_REG_LSYS_CKE_GRP0,	CKE_DTIM,						RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_DTIM,		CLK_IGNORE_UNUSED);
*/
RTK_GATE_CLK(lsys_gate_i2c0,									"CKE_I2C0",				"LAPB_CLK",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_I2C0,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_I2C0,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_i2c1,									"CKE_I2C1",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_I2C1,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_I2C1,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_i2c2,									"CKE_I2C2",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP0,	CKE_I2C2,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP0,	FEN_I2C2,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_hperi,									"CKE_HPERI",			"CKD_HPERI",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_HPERI,				RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_lx1,										"CKE_LX1",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_LX1,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_LX1,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ipsec,									"CKE_IPSEC",			"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_IPSEC,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_IPSEC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_lcdcmipi,								"CKE_LCDCMIPI",			"CKE_HPERI",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_LCDCMIPI,			RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_LCDC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_gdma,									"CKE_GDMA",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_GDMA,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_GDMA,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_wmac,									"CKE_WMAC",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_WMAC,				RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_spi0,									"CKE_SPI0",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_SPI0,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_SPI0,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_spi1,									"CKE_SPI1",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_SPI1,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_SPI1,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_usb,										"CKE_USB",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_USB,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_USB,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ledc,									"CKE_LEDC",				"CKE_XTAL_GRP1",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_LEDC,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_LEDC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_trng,									"CKE_TRNG",				"NP_SYSPLL",	// NP_SYSPLL_G8, div=1
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_TRNG,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_TRNG,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_sdh,										"CKE_SDH",				"CKE_HPERI",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_SDH,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_SDH,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ipc_hp,									"CKE_IPC_HP",			"HAPB_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_IPC_HP,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_IPC_HP,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_irda,									"CKE_IRDA",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_IRDA,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_IRDA,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ddrc,									"CKE_DDRC",				"DDR_PLL_533M",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_DDRC,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_DDRC,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ddrp,									"CKE_DDRP",				"DDR_PLL_533M",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_DDRP,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_DDRP,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_ac,										"CKE_AC",				"CKSL_AC",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_AC,					RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_AC,			CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_psram,									"CKE_PSRAM",			"CKSL_PSRAM",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_PSRAM,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_PSRAM,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_zgb,										"CKE_ZGB",				"HBUS_CLK",
			 AMEBA_REG_LSYS_CKE_GRP1,	CKE_ZGB,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP1,	FEN_ZGB,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_uart0,									"CKE_UART0",			"CKSL_UART0",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_UART0,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_UART0,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_uart1,									"CKE_UART1",			"CKSL_UART1",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_UART1,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_UART1,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_uart2,									"CKE_UART2",			"CKSL_UART2",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_UART2,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_UART2,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_uart3,									"CKE_UART3",			"CKSL_UART3",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_UART3,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_UART3,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_sport0,									"CKE_SPORT0",			"CKSL_SPORT0",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_SPORT0,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_SPORT0,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_sport1,									"CKE_SPORT1",			"CKSL_SPORT1",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_SPORT1,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_SPORT1,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_sport2,									"CKE_SPORT2",			"CKSL_SPORT2",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_SPORT2,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_SPORT2,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_sport3,									"CKE_SPORT3",			"CKSL_SPORT3",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_SPORT3,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_SPORT3,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim0,									"CKE_TIM0",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM0,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM0,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim1,									"CKE_TIM1",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM1,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM1,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim2,									"CKE_TIM2",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM2,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM2,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim3,									"CKE_TIM3",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM3,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM3,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim4,									"CKE_TIM4",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM4,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM4,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim5,									"CKE_TIM5",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM5,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM5,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim6,									"CKE_TIM6",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM6,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM6,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim7,									"CKE_TIM7",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM7,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM7,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim8,									"CKE_TIM8",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM8,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM8,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim9,									"CKE_TIM9",				"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM9,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM9,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim10,									"CKE_TIM10",			"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM10,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM10,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim11,									"CKE_TIM11",			"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM11,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM11,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim12,									"CKE_TIM12",			"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM12,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM12,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_tim13,									"CKE_TIM13",			"CKSL_32K",
			 AMEBA_REG_LSYS_CKE_GRP2,	CKE_TIM13,				RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_FEN_GRP2,	FEN_TIM13,		CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_xtal_grp0,								"CKE_XTAL_GRP0",		"XTAL_40M",
			 AMEBA_REG_LSYS_CKE_GRP,	CKE_XTAL_GRP0,			RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_xtal_grp1,								"CKE_XTAL_GRP1",		"XTAL_40M",
			 AMEBA_REG_LSYS_CKE_GRP,	CKE_XTAL_GRP0,			RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_xtal_grp2,								"CKE_XTAL_GRP2",		"XTAL_40M",
			 AMEBA_REG_LSYS_CKE_GRP,	CKE_XTAL_GRP0,			RTK_NO_GATE_FLAGS,		AMEBA_REG_LSYS_CKD_GRP1,	CKD_XTAL_2M_EN,	CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_xtal_grp3,								"CKE_XTAL_GRP3",		"XTAL_40M",
			 AMEBA_REG_LSYS_CKE_GRP,	CKE_XTAL_GRP0,			RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_xtal_lps_grp0,							"CKE_XTAL_LPS_GRP0",	"XTAL_LPS",
			 AMEBA_REG_LSYS_CKE_GRP,	CKE_XTAL_LPS_GRP0,		RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(lsys_gate_xtal_lps_grp1,							"CKE_XTAL_LPS_GRP1",	"XTAL_LPS",
			 AMEBA_REG_LSYS_CKE_GRP,	CKE_XTAL_LPS_GRP1,		RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);
RTK_GATE_CLK(apsys_gate_ap_cpu,									"CKE_AP_CPU",			"CKD_AP_CPU",
			 AMEBA_REG_HP_CKE,	CKE_AP_CPU,				RTK_NO_GATE_FLAGS,		0,							0,				CLK_IGNORE_UNUSED);


RTK_MUX_CLK(lsys_mux_lsoc,										"CKSL_LSOC",			cksl_lsoc_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_LSOC_SHIFT,		CKSL_LSOC_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_spic,										"CKSL_SPIC",			cksl_spic_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_SPIC_SHIFT,		CKSL_SPIC_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_gpio,										"CKSL_GPIO",			cksl_gpio_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_GPIO_SHIFT,		CKSL_GPIO_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_adc,										"CKSL_ADC",				cksl_adc_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_ADC_SHIFT,			CKSL_ADC_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_ctc,										"CKSL_CTC",				cksl_ctc_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_CTC_SHIFT,			CKSL_CTC_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_np,										"CKSL_NP",				cksl_np_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_NP_SHIFT,			CKSL_NP_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_hbus,										"CKSL_HBUS",			cksl_hbus_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_HBUS_SHIFT,		CKSL_HBUS_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_loguart,									"CKSL_LOGUART",			cksl_loguart_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_LOGUART_SHIFT,		CKSL_LOGUART_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_loguart2m,									"CKSL_LOGUART2M",		cksl_loguart2m_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_LOGUART2M_SHIFT,	CKSL_LOGUART2M_MASK,	RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_ac,										"CKSL_AC",				cksl_ac_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_AC_SHIFT,			CKSL_AC_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_vadm,										"CKSL_VADM",			cksl_vadm_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_VADM_SHIFT,		CKSL_VADM_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_sdm,										"CKSL_SDM",				cksl_sdm_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_SDM_SHIFT,			CKSL_SDM_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_uart0,										"CKSL_UART0",			cksl_uart_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_UART0_SHIFT,		CKSL_UART0_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_uart1,										"CKSL_UART1",			cksl_uart_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_UART1_SHIFT,		CKSL_UART1_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_uart2,										"CKSL_UART2",			cksl_uart_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_UART2_SHIFT,		CKSL_UART2_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_uart2m,									"CKSL_UART2M",			cksl_uart2m_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_UART2M_SHIFT,		CKSL_UART2M_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_sport0,									"CKSL_SPORT0",			cksl_sport0_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_SPORT0_SHIFT,		CKSL_SPORT0_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_sport1,									"CKSL_SPORT1",			cksl_sport1_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_SPORT1_SHIFT,		CKSL_SPORT1_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_sport2,									"CKSL_SPORT2",			cksl_sport2_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_SPORT2_SHIFT,		CKSL_SPORT2_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_sport3,									"CKSL_SPORT3",			cksl_sport3_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_SPORT3_SHIFT,		CKSL_SPORT3_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_adc2m,										"CKSL_ADC2M",			cksl_adc2m_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_ADC2M_SHIFT,		CKSL_ADC2M_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_vad,										"CKSL_VAD",				cksl_vad_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_VAD_SHIFT,			CKSL_VAD_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_hipc,										"CKSL_HIPC",			cksl_hipc_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_HIPC_SHIFT,		CKSL_HIPC_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_psram,										"CKSL_PSRAM",			cksl_psram_parents,
			AMEBA_REG_LSYS_CKSL_GRP0,	CKSL_PSRAM_SHIFT,		CKSL_PSRAM_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(apsys_mux_ap_cpu,									"CKSL_AP_CPU",			cksl_ap_cpu_parents,
			AMEBA_REG_HP_CKSL,	CKSL_AP_CPU_SHIFT,		CKSL_AP_CPU_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(hsys_mux_tim9,										"CKSL_TIM9",			cksl_tim9_parents,
			AMEBA_REG_DUMMY_1E0,		CKSL_TIM9_SHIFT,		CKSL_TIM9_MASK,		RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);


RTK_DIV_CLK(lsys_div_np,										"CKD_NP",				"NP_SYSPLL",	// NP_SYSPLL_G0
			AMEBA_REG_LSYS_CKD_GRP0,	CKD_NP_SHIFT,			CKD_NP_WIDTH,			RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_plfm,										"CKD_PLFM",				"CKSL_NP",
			AMEBA_REG_LSYS_CKD_GRP0,	CKD_PLFM_SHIFT,			CKD_PLFM_WIDTH,			RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_psram,										"CKD_PSRAM",			"NP_SYSPLL",	// NP_SYSPLL_G2
			AMEBA_REG_LSYS_CKD_GRP0,	CKD_PSRAM_SHIFT,		CKD_PSRAM_WIDTH,		RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(apsys_div_ap_cpu,									"CKD_AP_CPU",			"CKSL_AP_CPU",
			AMEBA_REG_HP_CKSL,	CKD_AP_CPU_SHIFT,		CKD_AP_CPU_WIDTH,		ckd_ap_cpu_div_table,		RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);

RTK_DIV_CLK(lsys_div_hbus,										"CKD_HBUS",				"NP_SYSPLL",	// NP_SYSPLL_G1
			AMEBA_REG_LSYS_CKD_GRP0,	CKD_HBUS_SHIFT,			CKD_HBUS_WIDTH,			RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_ac,										"CKD_AC",				"NP_SYSPLL",	// NP_SYSPLL_G7
			AMEBA_REG_LSYS_CKD_GRP0,	CKD_AC_SHIFT,			CKD_AC_WIDTH,			ckd_ac_div_table,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_hperi,										"CKD_HPERI",			"NP_SYSPLL",	// NP_SYSPLL_G6
			AMEBA_REG_LSYS_CKD_GRP0,	CKD_HPERI_SHIFT,		CKD_HPERI_WIDTH,		RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_mipi,										"CKD_MIPI",				"NP_SYSPLL",
			AMEBA_REG_LSYS_CKD_GRP0,	CKD_MIPI_SHIFT,			CKD_MIPI_WIDTH,			RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);

RTK_MUX_CLK(lsys_mux_i2s0,										"CKSL_I2S0",			cksl_i2s_parents,
			AMEBA_REG_SPORT_CLK,		CKSL_I2S0_SHIFT,		CKSL_I2S0_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_i2s1,										"CKSL_I2S1",			cksl_i2s_parents,
			AMEBA_REG_SPORT_CLK,		CKSL_I2S1_SHIFT,		CKSL_I2S1_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_i2s2,										"CKSL_I2S2",			cksl_i2s_parents,
			AMEBA_REG_SPORT_CLK,		CKSL_I2S2_SHIFT,		CKSL_I2S2_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_MUX_CLK(lsys_mux_i2s3,										"CKSL_I2S3",			cksl_i2s_parents,
			AMEBA_REG_SPORT_CLK,		CKSL_I2S3_SHIFT,		CKSL_I2S3_MASK,			RTK_NO_MUX_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_sport0,									"CKD_SPORT0",			"CKSL_I2S0",
			AMEBA_REG_SPORT_CLK,		CKD_SPORT0_SHIFT,		CKD_SPORT0_WIDTH,		RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_sport1,									"CKD_SPORT1",			"CKSL_I2S1",
			AMEBA_REG_SPORT_CLK,		CKD_SPORT1_SHIFT,		CKD_SPORT1_WIDTH,		RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_sport2,									"CKD_SPORT2",			"CKSL_I2S2",
			AMEBA_REG_SPORT_CLK,		CKD_SPORT2_SHIFT,		CKD_SPORT2_WIDTH,		RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);
RTK_DIV_CLK(lsys_div_sport3,									"CKD_SPORT3",			"CKSL_I2S3",
			AMEBA_REG_SPORT_CLK,		CKD_SPORT3_SHIFT,		CKD_SPORT3_WIDTH,		RTK_NO_DIV_TABLE,			RTK_NO_DIV_FLAGS,			RTK_NO_FLAGS);

/* Fixed-factor clocks */
RTK_FIXED_FACTOR_CLK(lsys_ff_lbus_clk,					"LBUS_CLK",				"CKSL_LSOC",	1,		1,		RTK_NO_FLAGS);
RTK_FIXED_FACTOR_CLK(lsys_ff_lapb_clk,					"LAPB_CLK",				"LBUS_CLK",		1,		2,		RTK_NO_FLAGS);
RTK_FIXED_FACTOR_CLK(lsys_ff_hapb_clk,					"HAPB_CLK",				"HBUS_CLK",		1,		2,		RTK_NO_FLAGS);
RTK_FIXED_FACTOR_CLK(lsys_ff_hbus_clk,					"HBUS_CLK",				"CKSL_HBUS",	1,		1,		RTK_NO_FLAGS);
RTK_FIXED_FACTOR_CLK(lsys_ff_plfm_clk,					"PLFM_CLK",				"CKD_NP",		1,		2,		RTK_NO_FLAGS);

/*AP SYSPLL clock*/
RTK_APLL_CLK(apsys_apll_clk,			"AP_SYSPLL",			AMEBA_REG_APLL_BASE,			RTK_NO_FLAGS);


static const struct rtk_clk *ameba_clk_data[] = {
	[RTK_CKE_ATIM]				= &aon_gate_timer,
	[RTK_CKE_RTC]				= &aon_gate_rtc,
	[RTK_CKE_SDM32K]			= &aon_gate_sdm_32k,
	[RTK_CKE_LP4M]				= &aon_gate_lp4m,
	[RTK_CKE_OSC4M_FORCE0]		= &aon_gate_osc4m_force0,
	[RTK_CKE_OSC4M_FORCE1]		= &aon_gate_osc4m_force1,
	[RTK_CKE_OSC4M_FORCE2]		= &aon_gate_osc4m_force2,
	[RTK_CKE_OSC4M_FORCE3]		= &aon_gate_osc4m_force3,
	[RTK_CKE_OSC4M_FORCE4]		= &aon_gate_osc4m_force4,
	[RTK_CKE_OSC4M_FORCE5]		= &aon_gate_osc4m_force5,
	[RTK_CKE_OSC4M_FORCE6]		= &aon_gate_osc4m_force6,
	[RTK_CKE_OSC4M_FORCE7]		= &aon_gate_osc4m_force7,

	[RTK_CKSL_OTP]				= &aon_mux_otp,
	[RTK_CKSL_32K]				= &aon_mux_32k,

	[RTK_CKE_HPON]				= &lsys_gate_hpon,
	[RTK_CKE_SIC]				= &lsys_gate_sic,
	[RTK_CKE_LPLFM] 			= &lsys_gate_lplfm,
	[RTK_CKE_HPLFM] 			= &lsys_gate_hplfm,
	[RTK_CKE_LP]				= &lsys_gate_lp,
	[RTK_CKE_NP]				= &lsys_gate_np,
	[RTK_CKE_SPIC]				= &lsys_gate_spic,
	[RTK_CKE_GPIO]				= &lsys_gate_gpio,
	//[RTK_CKE_WLON]				= &lsys_gate_wlon,
	//[RTK_CKE_BTON]				= &lsys_gate_bton,
	[RTK_CKE_PX]				= &lsys_gate_px,
	[RTK_CKE_IPC_LP]			= &lsys_gate_ipc_lp,
	[RTK_CKE_LOGUART]			= &lsys_gate_loguart,
	[RTK_CKE_CTC]				= &lsys_gate_ctc,
	[RTK_CKE_ADC]				= &lsys_gate_adc,
	[RTK_CKE_THM]				= &lsys_gate_thm,
	[RTK_CKE_AIP]				= &lsys_gate_aip,
	[RTK_CKE_ED25519]			= &lsys_gate_ed25519,
	[RTK_CKE_ECDSA] 			= &lsys_gate_ecdsa,
	//[RTK_CKE_DTIM]				= &lsys_gate_dtim,
	[RTK_CKE_I2C0]				= &lsys_gate_i2c0,
	[RTK_CKE_I2C1]				= &lsys_gate_i2c1,
	[RTK_CKE_I2C2]				= &lsys_gate_i2c2,
	[RTK_CKE_HPERI] 			= &lsys_gate_hperi,
	[RTK_CKE_LX1]				= &lsys_gate_lx1,
	[RTK_CKE_IPSEC] 			= &lsys_gate_ipsec,
	[RTK_CKE_LCDCMIPI]			= &lsys_gate_lcdcmipi,
	[RTK_CKE_GDMA]				= &lsys_gate_gdma,
	[RTK_CKE_WMAC]				= &lsys_gate_wmac,
	[RTK_CKE_SPI0]				= &lsys_gate_spi0,
	[RTK_CKE_SPI1]				= &lsys_gate_spi1,
	[RTK_CKE_USB]				= &lsys_gate_usb,
	[RTK_CKE_LEDC]				= &lsys_gate_ledc,
	[RTK_CKE_TRNG]				= &lsys_gate_trng,
	[RTK_CKE_SDH]				= &lsys_gate_sdh,
	[RTK_CKE_IPC_HP]			= &lsys_gate_ipc_hp,
	[RTK_CKE_IRDA]				= &lsys_gate_irda,
	[RTK_CKE_DDRC]				= &lsys_gate_ddrc,
	[RTK_CKE_DDRP]				= &lsys_gate_ddrp,
	[RTK_CKE_AC]				= &lsys_gate_ac,
	[RTK_CKE_PSRAM] 			= &lsys_gate_psram,
	[RTK_CKE_ZGB]				= &lsys_gate_zgb,
	[RTK_CKE_UART0] 			= &lsys_gate_uart0,
	[RTK_CKE_UART1] 			= &lsys_gate_uart1,
	[RTK_CKE_UART2] 			= &lsys_gate_uart2,
	[RTK_CKE_UART3] 			= &lsys_gate_uart3,
	[RTK_CKE_SPORT0]			= &lsys_gate_sport0,
	[RTK_CKE_SPORT1]			= &lsys_gate_sport1,
	[RTK_CKE_SPORT2]			= &lsys_gate_sport2,
	[RTK_CKE_SPORT3]			= &lsys_gate_sport3,
	[RTK_CKE_TIM0]				= &lsys_gate_tim0,
	[RTK_CKE_TIM1]				= &lsys_gate_tim1,
	[RTK_CKE_TIM2]				= &lsys_gate_tim2,
	[RTK_CKE_TIM3]				= &lsys_gate_tim3,
	[RTK_CKE_TIM4]				= &lsys_gate_tim4,
	[RTK_CKE_TIM5]				= &lsys_gate_tim5,
	[RTK_CKE_TIM6]				= &lsys_gate_tim6,
	[RTK_CKE_TIM7]				= &lsys_gate_tim7,
	[RTK_CKE_TIM8]				= &lsys_gate_tim8,
	[RTK_CKE_TIM9]				= &lsys_gate_tim9,
	[RTK_CKE_TIM10]				= &lsys_gate_tim10,
	[RTK_CKE_TIM11]				= &lsys_gate_tim11,
	[RTK_CKE_TIM12]				= &lsys_gate_tim12,
	[RTK_CKE_TIM13]				= &lsys_gate_tim13,
	[RTK_CKE_XTAL_GRP0] 		= &lsys_gate_xtal_grp0,
	[RTK_CKE_XTAL_GRP1] 		= &lsys_gate_xtal_grp1,
	[RTK_CKE_XTAL_GRP2] 		= &lsys_gate_xtal_grp2,
	[RTK_CKE_XTAL_GRP3] 		= &lsys_gate_xtal_grp3,
	[RTK_CKE_XTAL_LPS_GRP0] 	= &lsys_gate_xtal_lps_grp0,
	[RTK_CKE_XTAL_LPS_GRP1] 	= &lsys_gate_xtal_lps_grp1,

	[RTK_CKSL_LSOC] 			= &lsys_mux_lsoc,
	[RTK_CKSL_SPIC] 			= &lsys_mux_spic,
	[RTK_CKSL_GPIO] 			= &lsys_mux_gpio,
	[RTK_CKSL_ADC]				= &lsys_mux_adc,
	[RTK_CKSL_CTC]				= &lsys_mux_ctc,
	[RTK_CKSL_NP]				= &lsys_mux_np,
	[RTK_CKSL_HBUS] 			= &lsys_mux_hbus,
	[RTK_CKSL_LOGUART]			= &lsys_mux_loguart,
	[RTK_CKSL_LOGUART2M]		= &lsys_mux_loguart2m,
	[RTK_CKSL_AC]				= &lsys_mux_ac,
	[RTK_CKSL_VADM] 			= &lsys_mux_vadm,
	[RTK_CKSL_SDM]				= &lsys_mux_sdm,
	[RTK_CKSL_UART0]			= &lsys_mux_uart0,
	[RTK_CKSL_UART1]			= &lsys_mux_uart1,
	[RTK_CKSL_UART2]			= &lsys_mux_uart2,
	[RTK_CKSL_UART2M]			= &lsys_mux_uart2m,
	[RTK_CKSL_SPORT0]			= &lsys_mux_sport0,
	[RTK_CKSL_SPORT1]			= &lsys_mux_sport1,
	[RTK_CKSL_SPORT2]			= &lsys_mux_sport2,
	[RTK_CKSL_SPORT3]			= &lsys_mux_sport3,
	[RTK_CKSL_ADC2M]			= &lsys_mux_adc2m,
	[RTK_CKSL_VAD]				= &lsys_mux_vad,
	[RTK_CKSL_HIPC]				= &lsys_mux_hipc,
	[RTK_CKSL_PSRAM]			= &lsys_mux_psram,

	[RTK_CKD_NP]				= &lsys_div_np,
	[RTK_CKD_PLFM]				= &lsys_div_plfm,
	[RTK_CKD_PSRAM] 			= &lsys_div_psram,
	[RTK_CKD_HBUS]				= &lsys_div_hbus,
	[RTK_CKD_AC]				= &lsys_div_ac,
	[RTK_CKD_HPERI] 			= &lsys_div_hperi,
	[RTK_CKD_MIPI]				= &lsys_div_mipi,

	[RTK_CKSL_I2S0] 			= &lsys_mux_i2s0,
	[RTK_CKSL_I2S1] 			= &lsys_mux_i2s1,
	[RTK_CKSL_I2S2] 			= &lsys_mux_i2s2,
	[RTK_CKSL_I2S3] 			= &lsys_mux_i2s3,
	[RTK_CKD_SPORT0]			= &lsys_div_sport0,
	[RTK_CKD_SPORT1]			= &lsys_div_sport1,
	[RTK_CKD_SPORT2]			= &lsys_div_sport2,
	[RTK_CKD_SPORT3]			= &lsys_div_sport3,

	[RTK_CLK_LBUS]				= &lsys_ff_lbus_clk,
	[RTK_CLK_LAPB]				= &lsys_ff_lapb_clk,
	[RTK_CLK_HAPB]				= &lsys_ff_hapb_clk,
	[RTK_CLK_HBUS]				= &lsys_ff_hbus_clk,
	[RTK_CLK_PLFM]				= &lsys_ff_plfm_clk,

	[RTK_CKD_AP]				= &apsys_div_ap_cpu,
	[RTK_CKSL_AP]				= &apsys_mux_ap_cpu,
	[RTK_CLK_AP_PLL]			= &apsys_apll_clk,
	[RTK_CKE_AP]				= &apsys_gate_ap_cpu,
	[RTK_CKSL_TIM9]				= &hsys_mux_tim9,
};

#ifdef CONFIG_RESET_CONTROLLER
static const struct rtk_reset_map ameba_reset_map[] = {
	[RTK_POR_SYSON]				= {AMEBA_REG_AON_ROR,		POR_SYSON},
	[RTK_POR_AON_OTP]			= {AMEBA_REG_AON_ROR,		POR_AON_OTP},
	[RTK_POR_LP_PLAT]			= {AMEBA_REG_LSYS_POR,	POR_LP_PLAT},
	[RTK_POR_HP_PLAT]			= {AMEBA_REG_LSYS_POR,	POR_HP_PLAT},
	[RTK_POR_LP_BTON]			= {AMEBA_REG_LSYS_POR,	POR_LP_BTON},
	[RTK_POR_HP_DDRPHY]			= {AMEBA_REG_LSYS_POR,	POR_HP_DDRPHY},
};
#endif

static const struct rtk_rcc_data ameba_rcc_data = {
	.clocks = ameba_clk_data,
	.clock_num = RTK_CLK_MAX_NUM,
#ifdef CONFIG_RESET_CONTROLLER
	.resets = ameba_reset_map,
	.reset_num = RTK_RST_MAX_NUM
#endif
};

static const struct of_device_id ameba_of_match[] = {
	{
		.compatible = "realtek,ameba-rcc",
		.data = &ameba_rcc_data
	},
	{}
};

static void __iomem *rtk_rcc_get_reg(u32 offset)
{
	void __iomem *reg = NULL;

	switch (RTK_REG_GET_SYS(offset)) {
	case RTK_SYS_AON:
		reg = aon_rcc_base + RTK_REG_GET_OFFSET(offset);
		break;
	case RTK_SYS_LS:
		reg = lsys_rcc_base + RTK_REG_GET_OFFSET(offset);
		break;
	case RTK_SYS_AP:
		reg = apsys_rcc_base + RTK_REG_GET_OFFSET(offset);
		break;
	case RTK_SYS_HP:
		reg = hsys_rcc_base + RTK_REG_GET_OFFSET(offset);
		break;
	default:
		break;
	}

	return reg;
}

static int rtk_gate_clk_enable(struct clk_hw *hw)
{
	u32 val;
	unsigned long uninitialized_var(flags);
	struct rtk_clk *clk = rtk_gate_hw_to_clk(hw);
	struct rtk_fen *fen = &clk->fen;

	spin_lock_irqsave(&ameba_clk_lock, flags);

	if (!RTK_REG_IS_SYS_NO(clk->offset)) {
		val = readl(clk->gate.reg);
		val |= BIT(clk->gate.bit_idx);
		writel(val, clk->gate.reg);
	}

	if (!RTK_REG_IS_SYS_NO(fen->offset)) {
		val = readl(fen->reg);
		val |= BIT(fen->bit);
		writel(val, fen->reg);
	}

	spin_unlock_irqrestore(&ameba_clk_lock, flags);

	return 0;
}

static void rtk_gate_clk_disable(struct clk_hw *hw)
{
	u32 val;
	unsigned long uninitialized_var(flags);
	struct rtk_clk *clk = rtk_gate_hw_to_clk(hw);
	struct rtk_fen *fen = &clk->fen;

	spin_lock_irqsave(&ameba_clk_lock, flags);

	if (!RTK_REG_IS_SYS_NO(clk->offset)) {
		val = readl(clk->gate.reg);
		val &= ~BIT(clk->gate.bit_idx);
		writel(val, clk->gate.reg);
	}

	if (!RTK_REG_IS_SYS_NO(fen->offset)) {
		val = readl(fen->reg);
		val &= ~BIT(fen->bit);
		writel(val, fen->reg);
	}

	spin_unlock_irqrestore(&ameba_clk_lock, flags);
}

static int rtk_gate_clk_is_enabled(struct clk_hw *hw)
{
	int enabled = 0;
	u32 gate_reg = 0;
	u32 fen_reg = 0;
	unsigned long uninitialized_var(flags);
	struct rtk_clk *clk = rtk_gate_hw_to_clk(hw);
	struct rtk_fen *fen = &clk->fen;

	spin_lock_irqsave(&ameba_clk_lock, flags);

	if (!RTK_REG_IS_SYS_NO(clk->offset)) {
		gate_reg = readl(clk->gate.reg);
		gate_reg &= BIT(clk->gate.bit_idx);
	}

	if (!RTK_REG_IS_SYS_NO(fen->offset)) {
		fen_reg = readl(fen->reg);
		fen_reg &= BIT(fen->bit);
	}

	if (!RTK_REG_IS_SYS_NO(clk->offset)) {
		if (!RTK_REG_IS_SYS_NO(fen->offset)) {
			enabled = (gate_reg && fen_reg) ? 1 : 0;
		} else {
			enabled = gate_reg ? 1 : 0;
		}
	} else if (!RTK_REG_IS_SYS_NO(fen->offset)) {
		enabled = fen_reg ? 1 : 0;
	}

	spin_unlock_irqrestore(&ameba_clk_lock, flags);

	return enabled;
}

const struct clk_ops rtk_gate_clk_ops = {
	.enable		= rtk_gate_clk_enable,
	.disable	= rtk_gate_clk_disable,
	.is_enabled	= rtk_gate_clk_is_enabled,
};

static long	rtk_apll_clk_round_rate(struct clk_hw *hw, unsigned long rate,
									unsigned long *parent_rate)
{
	long val;

	/* Rate should be multiple of 40M, and between 800M and 1320M*/
	if (rate < 800000000) {
		val = 800000000;
	} else if (rate > 1320000000) {
		val = 1320000000;
	} else {
		val = rate;
	}

	val = val / 40000000;
	val = val * 40000000;

	return val;

}


static unsigned long rtk_apll_clk_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct rtk_apll *apll = container_of(hw, struct rtk_apll, hw);
	void __iomem *base = apll->reg;
	u32 factor;
	unsigned long rate;
	unsigned long uninitialized_var(flags);

	spin_lock_irqsave(&ameba_clk_lock, flags);

	factor = readl(base + REG_APLL_CTRL5);
	factor &= APLL_SDM_DIVM_MSK;
	rate = (factor + 2) * 40000000;

	spin_unlock_irqrestore(&ameba_clk_lock, flags);

	return rate;
}


static int rtk_apll_clk_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct rtk_apll *apll = container_of(hw, struct rtk_apll, hw);
	void __iomem *base = apll->reg;
	u32 val, factor = rate;
	unsigned long uninitialized_var(flags);

	spin_lock_irqsave(&ameba_clk_lock, flags);

	/* Disable apll */
	val = readl(base + REG_APLL_CTRL0);
	val &= (~APLL_POW_PI_BIT & ~APLL_POW_PLL_BIT);
	writel(val, base + REG_APLL_CTRL0);


	/* Calculate and set factor. rate = (divm[7:0] + 2) * 40M */
	factor = factor / 40000000;
	factor -= 2;

	val = readl(base + REG_APLL_CTRL5);
	val &= ~APLL_SDM_DIVM_MSK;
	val |= factor;
	writel(val, base + REG_APLL_CTRL5);

	/* Enable apll */
	val = readl(base + REG_APLL_CTRL0);
	val |= (APLL_POW_PI_BIT | APLL_POW_PLL_BIT);
	writel(val, base + REG_APLL_CTRL0);


	/* Wait it stable */
	while (1) {
		if (readl(base + REG_APLL_CTRL1) & APLL_CK_RDY_BIT) {
			break;
		}
	}

	spin_unlock_irqrestore(&ameba_clk_lock, flags);

	return 0;
}


const struct clk_ops rtk_apll_clk_ops = {
	.round_rate = rtk_apll_clk_round_rate,
	.set_rate = rtk_apll_clk_set_rate,
	.recalc_rate = rtk_apll_clk_recalc_rate,
};


#ifdef CONFIG_RESET_CONTROLLER

static int rtk_reset_assert(struct reset_controller_dev *rdev, unsigned long id)
{
	u32 val;
	void __iomem *reg;
	struct rtk_reset *rst = rdev_to_rtk_reset(rdev);
	const struct rtk_reset_map *map = &rst->map[id];
	unsigned long uninitialized_var(flags);

	if (map != NULL) {
		reg = rtk_rcc_get_reg(map->offset);
		spin_lock_irqsave(&ameba_reset_lock, flags);

		val = readw(reg);
		writew(val & ~map->bit, reg);

		spin_unlock_irqrestore(&ameba_reset_lock, flags);
	}

	return 0;
}

static int rtk_reset_deassert(struct reset_controller_dev *rdev, unsigned long id)
{
	u32 val;
	void __iomem *reg;
	struct rtk_reset *rst = rdev_to_rtk_reset(rdev);
	const struct rtk_reset_map *map = &rst->map[id];
	unsigned long uninitialized_var(flags);

	if (map != NULL) {
		reg = rtk_rcc_get_reg(map->offset);
		spin_lock_irqsave(&ameba_reset_lock, flags);

		val = readw(reg);
		writew(val | map->bit, reg);

		spin_unlock_irqrestore(&ameba_reset_lock, flags);
	}

	return 0;
}

static int rtk_reset_reset(struct reset_controller_dev *rdev, unsigned long id)
{
	rtk_reset_assert(rdev, id);
	udelay(10);
	rtk_reset_deassert(rdev, id);

	return 0;
}

static int rtk_reset_status(struct reset_controller_dev *rdev, unsigned long id)
{
	u32 val;
	void __iomem *reg;
	int status = 0;
	struct rtk_reset *rst = rdev_to_rtk_reset(rdev);
	const struct rtk_reset_map *map = &rst->map[id];
	unsigned long uninitialized_var(flags);

	if (map != NULL) {
		reg = rtk_rcc_get_reg(map->offset);

		spin_lock_irqsave(&ameba_clk_lock, flags);

		/*
		 * The reset control API expects 0 if reset is not asserted,
		 * which is the opposite of what our hardware uses.
		 */
		val = readw(reg);
		status = !(map->bit & val);

		spin_unlock_irqrestore(&ameba_reset_lock, flags);
	}

	return status;
}

static const struct reset_control_ops rtk_reset_ops = {
	.assert		= rtk_reset_assert,
	.deassert	= rtk_reset_deassert,
	.reset		= rtk_reset_reset,
	.status		= rtk_reset_status,
};

#endif

static struct clk_hw *rtk_clk_probe(struct rtk_clk *clk)
{
	struct clk_hw *hw;

	if (!clk) {
		return NULL;
	}

	if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_GATE) {
		clk->gate.reg = rtk_rcc_get_reg(clk->offset);
		clk->fen.reg = rtk_rcc_get_reg(clk->fen.offset);
		hw = &clk->gate.hw;
	} else if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_MUX) {
		clk->mux.reg = rtk_rcc_get_reg(clk->offset);
		hw = &clk->mux.hw;
	} else if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_DIV) {
		clk->div.reg = rtk_rcc_get_reg(clk->offset);
		hw = &clk->div.hw;
	} else if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_FIXED_FACTOR) {
		hw = &clk->ff.hw;
	} else if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_APLL) {
		hw = &clk->apll.hw;
		clk->apll.reg = rtk_rcc_get_reg(clk->offset);
	} else {
		hw = NULL;
	}

	return hw;
}

static struct clk_hw *rtk_clk_lookup_index(int index)
{
	struct clk_hw *hw = NULL;
	struct rtk_clk *clk;

	if (index < ameba_rcc_data.clock_num) {
		clk = (struct rtk_clk *)ameba_clk_data[index];
		if (clk) {
			if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_GATE) {
				hw = &clk->gate.hw;
			} else if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_MUX) {
				hw = &clk->mux.hw;
			} else if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_DIV) {
				hw = &clk->div.hw;
			} else if ((clk->flags & RTK_CLK_MASK) == RTK_CLK_APLL) {
				hw = &clk->apll.hw;
			}
		}
	}

	return hw;
}

static struct clk_hw *rtk_clk_lookup(struct of_phandle_args *clkspec, void *data)
{
	return rtk_clk_lookup_index(clkspec->args[0]);
}

static void __init rtk_rcc_init(struct device_node *node)
{
	int ret;
	int i;
	void __iomem *base = NULL;
	struct clk_hw *hw;
	const struct of_device_id *match;
	struct rtk_clk *clk;
#ifdef CONFIG_RESET_CONTROLLER
	struct rtk_reset *reset;
#endif

	match = of_match_node(ameba_of_match, node);
	if (WARN_ON(!match)) {
		return;
	}

	base = of_iomap(node, 0);
	if (NULL == base) {
		pr_err("RCC: Failed to map AON RCC resource\n");
		return;
	}

	aon_rcc_base = base;

	base = of_iomap(node, 1);
	if (NULL == base) {
		iounmap(aon_rcc_base);
		pr_err("RCC: Failed to map LSYS RCC resource\n");
		return;
	}

	lsys_rcc_base = base;

	base = of_iomap(node, 2);
	if (NULL == base) {
		iounmap(lsys_rcc_base);
		iounmap(aon_rcc_base);
		pr_err("RCC: Failed to map APSYS RCC resource\n");
		return;
	}

	apsys_rcc_base = base;

	base = of_iomap(node, 3);
	if (NULL == base) {
		iounmap(lsys_rcc_base);
		iounmap(aon_rcc_base);
		iounmap(apsys_rcc_base);
		pr_err("RCC: Failed to map HSYS RCC resource\n");
		return;
	}

	hsys_rcc_base = base;

	for (i = 0; i < ameba_rcc_data.clock_num; ++i) {
		if (ameba_clk_data[i] != NULL) {
			clk = (struct rtk_clk *)ameba_clk_data[i];
			hw = rtk_clk_probe(clk);
			if (hw) {
				ret = of_clk_hw_register(node, hw);
				if (ret) {
					pr_err("RCC: Failed to register clock %d - %s\n", i, hw->init->name);
					goto fail_add_clk_hw_provider;
				}
			}
		}
	}

	ret = of_clk_add_hw_provider(node, rtk_clk_lookup, NULL);
	if (ret) {
		pr_err("RCC: Failed to add clock provider\n");
		goto fail_add_clk_hw_provider;
	}

	pr_info("RCC: Clocks initialized\n");

#ifdef CONFIG_RESET_CONTROLLER

	reset = (struct rtk_reset *)kzalloc(sizeof(*reset), GFP_KERNEL);
	if (!reset) {
		ret = -ENOMEM;
		goto fail_alloc_reset;
	}

	reset->rdev.of_node = node;
	reset->rdev.ops = &rtk_reset_ops;
	reset->rdev.owner = THIS_MODULE;
	reset->rdev.nr_resets = ameba_rcc_data.reset_num;
	reset->map = ameba_rcc_data.resets;

	ret = reset_controller_register(&reset->rdev);
	if (ret) {
		goto fail_register_reset;
	}

	pr_info("RCC: Resets initialized\n");

#endif

	return;

#ifdef CONFIG_RESET_CONTROLLER
fail_register_reset:
	kfree(reset);

fail_alloc_reset:
	of_clk_del_provider(node);
#endif

fail_add_clk_hw_provider:
	while (--i >= 0) {
		hw = rtk_clk_lookup_index(i);
		if (hw) {
			clk_hw_unregister(hw);
		}
	}

	iounmap(aon_rcc_base);
	iounmap(lsys_rcc_base);
	iounmap(apsys_rcc_base);
	iounmap(hsys_rcc_base);

	pr_info("RCC: init error %d\n", ret);
}

CLK_OF_DECLARE_DRIVER(ameba_rcc, "realtek,ameba-rcc", rtk_rcc_init);

