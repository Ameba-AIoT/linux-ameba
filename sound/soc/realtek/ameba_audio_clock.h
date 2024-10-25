// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#ifndef _AMEBA_AUDIO_CLOCK_H_
#define _AMEBA_AUDIO_CLOCK_H_

#define CKSL_I2S_PLL98M				0x00
#define CKSL_I2S_PLL45M				0x01
#define CKSL_I2S_PLL24M				0x03
#define CKSL_I2S_XTAL40M			0x04

#define PLL_CLOCK_24P576M           24576000
#define PLL_CLOCK_45P1584M           45158400
#define PLL_CLOCK_98P304M           98304000

#define SPORT_MCLK_DIV_MAX_NUM      3
#define PLL_CLOCK_MAX_NUM           3

#define I2S_PLL_ALWAYS_ON           1

struct audio_clock_component {
	const struct audio_clock_driver* clock_driver;
	struct device *dev;
	unsigned int pll_clock[3];
	unsigned int sport_mclk_div[3];
};

struct audio_clock_driver {
	int (*update_98MP304M_input_clock_status)(struct audio_clock_component* acc, bool enabled);
	int (*update_45MP158_input_clock_status)(struct audio_clock_component* acc, bool enabled);
	int (*update_24MP576_input_clock_status)(struct audio_clock_component* acc, bool enabled);
	int (*pll_i2s_98P304M_clk_tune)(struct audio_clock_component* acc, u32 ppm, u32 action);
	int (*pll_i2s_24P576M_clk_tune)(struct audio_clock_component* acc, u32 ppm, u32 action);
	int (*pll_i2s_45P1584M_clk_tune)(struct audio_clock_component* acc, u32 ppm, u32 action);
	int (*get_audio_clock_info_for_sport)(struct audio_clock_component* acc, unsigned int sport_index);
	int (*choose_input_clock_for_sport_index)(struct audio_clock_component* acc, unsigned int sport_index, unsigned int clock);
	int (*update_fen_cke_sport_status)(struct audio_clock_component* acc, unsigned int sport_index, bool enabled);
	int (*set_rate_divider)(struct audio_clock_component* acc, unsigned int sport_index, unsigned int divider);
	int (*get_rate_divider)(struct audio_clock_component* acc);
	int (*set_rate)(struct audio_clock_component* acc, unsigned int rate);
	int (*get_rate)(struct audio_clock_component* acc);
	void (*dump)(struct audio_clock_component* acc);
};

struct clock_params {
	unsigned int clock;
	unsigned int pll_div;
	unsigned int sport_mclk_div;
};

/*apis interact with audio clock driver*/
struct audio_clock_component* create_audio_clock_component(struct device *dev);
void * get_audio_clock_data(struct audio_clock_component* audio_clock_comp);
int    register_audio_clock(const struct audio_clock_driver* clk_driver);

/*apis interact with audio clock clients*/
int update_98MP304M_input_clock_status(bool enabled);
int update_45MP158_input_clock_status(bool enabled);
int update_24MP576_input_clock_status(bool enabled);
int pll_i2s_98P304M_clk_tune(u32 ppm, u32 action);
int pll_i2s_24P576M_clk_tune(u32 ppm, u32 action);
int pll_i2s_45P1584M_clk_tune(u32 ppm, u32 action);
int get_audio_clock_info_for_sport(unsigned int sport_index);
int choose_pll_clock(unsigned int channel_count, unsigned int channel_len, unsigned int rate,
					unsigned int codec_multiplier_with_rate, unsigned int sport_mclk_fixed_max, struct clock_params* params);
int update_fen_cke_sport_status(unsigned int sport_index, bool enabled);
int choose_input_clock_for_sport_index(unsigned int sport_index, unsigned int clock);
int set_rate_divider(unsigned int sport_index, unsigned int divider);
int get_rate_divider(void);
int set_rate(unsigned int rate);
int get_rate(void);
void audio_clock_dump(void);

#endif