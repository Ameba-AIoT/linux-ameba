// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#ifndef __SND_SOC_REALTEK_GDMA_H_
#define __SND_SOC_REALTEK_GDMA_H_

#define MAX_IDMA_PERIOD (128 * 1024)
#define MAX_IDMA_BUFFER (160 * 1024)
#define DMA_BURST_AGGREGATION 16

//dma fifo infos.
struct ameba_pcm_dma_params {
	dma_addr_t dev_phys_0;
	dma_addr_t dev_phys_1;
	u32 datawidth;
	u32 src_maxburst;
	u32 handshaking_0;
	u32 handshaking_1;
	const char *chan_name;
	//means the total counter calculated by irq interrupt.
	//however it's not the real current time, need to add the delta
	//counter value from last interrupt->now.
	u64 total_sport_counter;
	//means the current substream's sport's base addr.
	void __iomem * sport_base_addr;
	bool use_mmap;
};

struct dma_callback_params {
	struct snd_pcm_substream *substream;
	unsigned int dma_id;
};

void ameba_alsa_callback_handle(struct dma_callback_params *params);

#endif
