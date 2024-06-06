// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <stddef.h>

#include "ameba_audio_clock.h"

#define MAX_PLL_DIV 8

struct audio_clock_component* acc = NULL;

struct audio_clock_component* create_audio_clock_component(struct device *dev)
{
	if (acc != NULL)
		return acc;

	acc = devm_kzalloc(dev, sizeof(struct audio_clock_component), GFP_KERNEL);
	if (acc == NULL){
		pr_info("alloc component fail");
		return NULL;
	} else {
		acc->dev = dev;
		acc->pll_clock[0] = PLL_CLOCK_24P576M;
		acc->pll_clock[1] = PLL_CLOCK_98P304M;
		acc->pll_clock[2] = PLL_CLOCK_45P1584M;
		acc->sport_mclk_div[0] = 1;
		acc->sport_mclk_div[1] = 2;
		acc->sport_mclk_div[2] = 4;
		return acc;
	}
}
EXPORT_SYMBOL(create_audio_clock_component);

void * get_audio_clock_data(struct audio_clock_component* audio_clock_comp)
{
	return dev_get_drvdata(audio_clock_comp->dev);
}
EXPORT_SYMBOL(get_audio_clock_data);

int register_audio_clock(const struct audio_clock_driver* clk_driver)
{
	acc->clock_driver = clk_driver;
	return 0;
}
EXPORT_SYMBOL(register_audio_clock);

static bool is_sport_ni_mi_supported(unsigned int clock, unsigned int sr, unsigned int channel_count, unsigned int chn_len)
{
	int ni = 1;
	int mi = 1;
	int max_ni = 32767;
	int max_mi = 65535;
	bool ni_mi_found = false;

	for (; ni <= max_ni ; ni++) {
		if (clock * ni % (channel_count * chn_len * sr) == 0) {
			mi = clock * ni / (channel_count * chn_len * sr);
			//pr_info("check founded: ni : %d, mi: %d \n", ni, mi);
			if (mi < 2 * ni) {
				//pr_info("mi <= ni, check fail, try another pll divider or sport mclk divider \n");
				break;
			}
			if (mi <= max_mi) {
				//pr_info("check ni : %d, mi: %d success\n", ni, mi);
				ni_mi_found = true;
				break;
			}
		}
	}
	return ni_mi_found;
}

int choose_pll_clock(unsigned int channel_count, unsigned int channel_len, unsigned int rate,
					unsigned int codec_multiplier_with_rate, unsigned int sport_mclk_fixed_max, struct clock_params* params)
{
	int ret = 0;
	unsigned int sport_mclk_div_index = 0;
	unsigned int pll_clock_index = 0;
	unsigned int pll_div = 1;
	bool choose_done = false;

	if (codec_multiplier_with_rate && sport_mclk_fixed_max) {
		pr_info("not supported multi:%u, fixed_max:%u", codec_multiplier_with_rate, sport_mclk_fixed_max);
	}

	// external codec needs mclk to be codec_multiplier_with_rate * fs.
	if (codec_multiplier_with_rate) {
		for (; sport_mclk_div_index < SPORT_MCLK_DIV_MAX_NUM; sport_mclk_div_index++) {
			for (; pll_clock_index < PLL_CLOCK_MAX_NUM; pll_clock_index++) {
				if ((acc->pll_clock[pll_clock_index] / acc->sport_mclk_div[sport_mclk_div_index]) % (codec_multiplier_with_rate * rate) != 0) {
					continue;
				} else {
					pll_div = (acc->pll_clock[pll_clock_index] / acc->sport_mclk_div[sport_mclk_div_index]) / (codec_multiplier_with_rate * rate);
					if (pll_div <= 8 && is_sport_ni_mi_supported((acc->pll_clock[pll_clock_index]/pll_div), rate, channel_count, channel_len)) {
						//pr_info("find the right clock:%d, sport_mclk_div:%d, pll_div:%d", acc->pll_clock[pll_clock_index], acc->sport_mclk_div[sport_mclk_div_index], pll_div);
						choose_done = true;
						break;
					} else {
						continue;
					}
				}
			}
			if (choose_done)
				break;

			pll_clock_index = 0;
		}
	}

	// external codec needs mclk to be less than sport_mclk_fixed_max.
	if (sport_mclk_fixed_max) {
		if (!(rate % 4000)) {
			for (; pll_clock_index < PLL_CLOCK_MAX_NUM; pll_clock_index++) {
				for (pll_div = 1; pll_div < MAX_PLL_DIV + 1; pll_div++) {
					for (; sport_mclk_div_index < SPORT_MCLK_DIV_MAX_NUM; sport_mclk_div_index++) {
						if (acc->pll_clock[pll_clock_index] / pll_div / acc->sport_mclk_div[sport_mclk_div_index] <= sport_mclk_fixed_max) {
							if (is_sport_ni_mi_supported((acc->pll_clock[pll_clock_index]/pll_div), rate, channel_count, channel_len)) {
								//pr_info("find the fixed mclk clock:%u, sport_mclk_div:%u, pll_div:%u", acc->pll_clock[pll_clock_index], acc->sport_mclk_div[sport_mclk_div_index], pll_div);
								choose_done = true;
								goto end;
							} else {
								continue;
							}
						} else {
							continue;
						}
					}
					sport_mclk_div_index = 0;
				}
			}
		} else if (!(rate % 11025)) {
			for (pll_div = 1; pll_div < MAX_PLL_DIV + 1; pll_div++) {
				for (; sport_mclk_div_index < SPORT_MCLK_DIV_MAX_NUM; sport_mclk_div_index++) {
					if (PLL_CLOCK_45P1584M / pll_div / acc->sport_mclk_div[sport_mclk_div_index] <= sport_mclk_fixed_max) {
						if (is_sport_ni_mi_supported((PLL_CLOCK_45P1584M), rate, channel_count, channel_len)) {
							//pr_info("find the fixed mclk clock:%u, sport_mclk_div:%u, pll_div:%u", PLL_CLOCK_45P1584M, acc->sport_mclk_div[sport_mclk_div_index], pll_div);
							choose_done = true;
							pll_clock_index = 2;
							goto end;
						} else {
							continue;
						}
					} else {
						continue;
					}
				}
				sport_mclk_div_index = 0;
			}
		}
	}

end:
	if (!choose_done) {
		pr_info("can't find proper clock for the current rate:%u", rate);
		ret = -EINVAL;
		return ret;
	}

	params->clock = acc->pll_clock[pll_clock_index];
	params->pll_div = pll_div;
	params->sport_mclk_div = acc->sport_mclk_div[sport_mclk_div_index];

	return ret;
}
EXPORT_SYMBOL(choose_pll_clock);

int update_98MP304M_input_clock_status(bool enabled)
{
	//pr_info("%s:%d \n", __func__, enabled);
	if (acc->clock_driver != NULL)
		return acc->clock_driver->update_98MP304M_input_clock_status(acc, enabled);
	else
		return -EPERM;

}
EXPORT_SYMBOL(update_98MP304M_input_clock_status);

int update_45MP158_input_clock_status(bool enabled)
{
	//pr_info("%s:%d \n", __func__, enabled);
	if (acc->clock_driver != NULL)
		return acc->clock_driver->update_45MP158_input_clock_status(acc, enabled);
	else
		return -EPERM;

}
EXPORT_SYMBOL(update_45MP158_input_clock_status);

int update_24MP576_input_clock_status(bool enabled)
{
	//pr_info("%s:%d \n", __func__, enabled);
	if (acc->clock_driver != NULL)
		return acc->clock_driver->update_24MP576_input_clock_status(acc, enabled);
	else
		return -EPERM;

}
EXPORT_SYMBOL(update_24MP576_input_clock_status);

int pll_i2s_98P304M_clk_tune(u32 ppm, u32 action)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->pll_i2s_98P304M_clk_tune(acc, ppm, action);
	else
		return -EPERM;
}
EXPORT_SYMBOL(pll_i2s_98P304M_clk_tune);

int pll_i2s_24P576M_clk_tune(u32 ppm, u32 action)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->pll_i2s_24P576M_clk_tune(acc, ppm, action);
	else
		return -EPERM;
}
EXPORT_SYMBOL(pll_i2s_24P576M_clk_tune);

int pll_i2s_45P1584M_clk_tune(u32 ppm, u32 action)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->pll_i2s_45P1584M_clk_tune(acc, ppm, action);
	else
		return -EPERM;
}
EXPORT_SYMBOL(pll_i2s_45P1584M_clk_tune);

int get_audio_clock_info_for_sport(unsigned int sport_index)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->get_audio_clock_info_for_sport(acc, sport_index);
	else
		return -EPERM;
}
EXPORT_SYMBOL(get_audio_clock_info_for_sport);

int choose_input_clock_for_sport_index(unsigned int sport_index, unsigned int clock)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->choose_input_clock_for_sport_index(acc, sport_index, clock);
	else
		return -EPERM;
}
EXPORT_SYMBOL(choose_input_clock_for_sport_index);

int update_fen_cke_sport_status(unsigned int sport_index, bool enabled)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->update_fen_cke_sport_status(acc, sport_index, enabled);
	else
		return -EPERM;
}
EXPORT_SYMBOL(update_fen_cke_sport_status);

int set_rate_divider(unsigned int sport_index, unsigned int divider)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->set_rate_divider(acc, sport_index, divider);
	else
		return -EPERM;
}
EXPORT_SYMBOL(set_rate_divider);

int get_rate_divider(void)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->get_rate_divider(acc);
	else
		return -EPERM;
}
EXPORT_SYMBOL(get_rate_divider);

int set_rate(unsigned int rate)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->set_rate(acc, rate);
	else
		return -EPERM;
}
EXPORT_SYMBOL(set_rate);

int get_rate(void)
{
	if (acc->clock_driver != NULL)
		return acc->clock_driver->get_rate(acc);
	else
		return -EPERM;
}
EXPORT_SYMBOL(get_rate);

void audio_clock_dump(void)
{
	if (acc->clock_driver != NULL)
		acc->clock_driver->dump(acc);
}
EXPORT_SYMBOL(audio_clock_dump);