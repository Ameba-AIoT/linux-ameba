// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
* Realtek Audio support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __SND_SOC_REALTEK_AUDIO_PLL_H_
#define __SND_SOC_REALTEK_AUDIO_PLL_H_

#include <linux/types.h>

#define PLL_24P576M			0
#define PLL_45P1584M			1
#define PLL_98P304M			2
#define PLL_XTAL40M			3
#define PLL_AUTO			0
#define PLL_FASTER			1
#define PLL_SLOWER			2

#define MAGIC    'x'
#define AUDIO_PLL_MICRO_ADJUST           _IOW(MAGIC, 0, struct micro_adjust_params)
#define AUDIO_PLL_GET_CLOCK_FOR_SPORT    _IOW(MAGIC, 1, struct audio_clock_info)

struct micro_adjust_params {
	/*can be PLL_24P576M, PLL_45P1584M, or PLL_98P304M*/
	unsigned int clock;
	/*can be #define PLL_AUTO, PLL_FASTER, or PLL_SLOWER*/
	unsigned int action;
	/*can be any value between 0-1000*/
	unsigned int ppm;
};

struct audio_clock_info {
	/*user sets sport_index, to tell kernel which sport's clock info it needs*/
	unsigned int sport_index;
	/*user gets clock info from kernel for the sport_index he sets*/
	unsigned int clock;
};

#endif
