// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/dmaengine.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "dma.h"
#include "ameba_sport.h"
#include "ameba_gdma.h"
#include "ameba_audio_clock.h"

#define NO_MICRO_ADJUST 1
#define IRQ_COMPENSATE_BASING_ON_DELTA_DELAY 1
#define COUNTER_COMPENSATE_BASING_ON_DELTA_DELAY 1
#define MAX_SPORT_IRQ_X 134217727
#define USING_COUNTER 1

#define PLAY_IN_USE 0x01
#define RECORD_IN_USE 0x02

struct sport_dai {
	/* Platform device for this DAI */
	struct platform_device *pdev;

	/* IOREMAP'd SFRs */
	void __iomem *addr;
	/* Physical base address of SFRs */
	u32 base;
	u32 reg_size;
	/* Frame clock */
	unsigned frmclk;
	unsigned cap_frmclk;

	int id;
	u32 clock_in_from_dts;

	/* DMA parameters */
	struct ameba_pcm_dma_params dma_playback;
	struct ameba_pcm_dma_params dma_capture;

	int sport_debug;

	struct clk* clock;
	struct clock_params * audio_clock_params;
	unsigned int sport_mclk_multiplier;
	unsigned int sport_fixed_mclk_max;
	unsigned int sport_multi_io;
	unsigned int sport_mode;
	int	irq;
	int clock_enabled;

	/* total frames delivered through LRCLK */
	u64 total_rx_counter;
	u64 total_tx_counter;
	/* total frames delivered through LRCLK */
	u64 total_rx_counter_boundary;
	u64 total_tx_counter_boundary;
	/* total irq counts, if LRCLK delivers dma buffer_size(tx_sport_compare_val)
	*  (rx_sport_compare_val) then triggers one sport irq */
	u64 irq_tx_count;
	u64 irq_rx_count;
	/* size that LRCLK delivers, that triggers one sport irq */
	u64 tx_sport_compare_val;
	u64 rx_sport_compare_val;
	u64 last_total_delay;

	unsigned int dai_fmt;
	u32 fifo_num;

	spinlock_t lock;

	/* bit0: play in use, bit1: record in use*/
	u8 play_record_in_use;
	/* guard play_record_in_use */
	struct mutex state_mutex;
};

#define sport_info(mask,dev, ...)						\
do{									\
	if (((struct sport_dai *)(dev_get_drvdata(dev))) != NULL)					\
		if (((((struct sport_dai *)(dev_get_drvdata(dev)))->sport_debug) & mask) >= 1)		\
			dev_info(dev, ##__VA_ARGS__); \
}while(0)

#define sport_verbose(mask,dev, ...)						\
do{									\
	if (((struct sport_dai *)(dev_get_drvdata(dev))) != NULL)					\
		if (((((struct sport_dai *)(dev_get_drvdata(dev)))->sport_debug) & mask) == 2)		\
			dev_info(dev, ##__VA_ARGS__); \
}while(0)


static ssize_t sport_debug_show(struct device *dev, struct device_attribute*attr, char *buf)
{
		struct sport_dai *data = dev_get_drvdata(dev);
        return sprintf(buf, "sport_debug=%d\n", data->sport_debug);
}

static ssize_t sport_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sport_dai *data = dev_get_drvdata(dev);
	data->sport_debug = buf[0] - '0';
    return count;
}

static DEVICE_ATTR(sport_debug, S_IWUSR |S_IRUGO, sport_debug_show, sport_debug_store);

static struct attribute *sport_debug_attrs[] = {
        &dev_attr_sport_debug.attr,
        NULL
};

static const struct attribute_group sport_debug_attr_grp = {
        .attrs = sport_debug_attrs,
};

void dumpSportRegs(struct device *dev,void __iomem * sportx){
	u32 tmp;
	struct sport_dai *sport = dev_get_drvdata(dev);
	if (sport -> sport_debug >= 1) {
		tmp = readl(sportx + REG_SP_REG_MUX);
		sport_info(1, dev, "REG_SP_REG_MUX:%x",tmp);
		tmp = readl(sportx + REG_SP_CTRL0);
		sport_info(1, dev, "REG_SP_CTRL0:%x",tmp);
		tmp = readl(sportx + REG_SP_CTRL1);
		sport_info(1, dev, "REG_SP_CTRL1:%x",tmp);
		tmp = readl(sportx + REG_SP_INT_CTRL);
		sport_info(1, dev, "REG_SP_INT_CTRL:%x",tmp);
		tmp = readl(sportx + REG_RSVD0);
		sport_info(1, dev, "REG_RSVD0:%x",tmp);
		tmp = readl(sportx + REG_SP_TRX_COUNTER_STATUS);
		sport_info(1, dev, "REG_SP_TRX_COUNTER_STATUS:%x",tmp);
		tmp = readl(sportx + REG_SP_ERR);
		sport_info(1, dev, "REG_SP_ERR:%x",tmp);
		tmp = readl(sportx + REG_SP_SR_TX_BCLK);
		sport_info(1, dev, "REG_SP_SR_TX_BCLK:%x",tmp);
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		sport_info(1, dev, "REG_SP_TX_LRCLK:%x",tmp);
		tmp = readl(sportx + REG_SP_FIFO_CTRL);
		sport_info(1, dev, "REG_SP_FIFO_CTRL:%x",tmp);
		tmp = readl(sportx + REG_SP_FORMAT);
		sport_info(1, dev, "REG_SP_FORMAT:%x",tmp);
		tmp = readl(sportx + REG_SP_RX_BCLK);
		sport_info(1, dev, "REG_SP_RX_BCLK:%x",tmp);
		tmp = readl(sportx + REG_SP_RX_LRCLK);
		sport_info(1, dev, "REG_SP_RX_LRCLK:%x",tmp);
		tmp = readl(sportx + REG_SP_DSP_COUNTER);
		sport_info(1, dev, "REG_SP_DSP_COUNTER:%x",tmp);
		tmp = readl(sportx + REG_RSVD1);
		sport_info(1, dev, "REG_RSVD1:%x",tmp);
		tmp = readl(sportx + REG_SP_DIRECT_CTRL0);
		sport_info(1, dev, "REG_SP_DIRECT_CTRL0:%x",tmp);
		tmp = readl(sportx + REG_RSVD2);
		sport_info(1, dev, "REG_RSVD2:%x",tmp);
		tmp = readl(sportx + REG_SP_FIFO_IRQ);
		sport_info(1, dev, "REG_SP_FIFO_IRQ:%x",tmp);
		tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
		sport_info(1, dev, "REG_SP_DIRECT_CTRL1:%x",tmp);
		tmp = readl(sportx + REG_SP_DIRECT_CTRL2);
		sport_info(1, dev, "REG_SP_DIRECT_CTRL2:%x",tmp);
		tmp = readl(sportx + REG_RSVD3);
		sport_info(1, dev, "REG_RSVD3:%x",tmp);
		tmp = readl(sportx + REG_SP_DIRECT_CTRL3);
		sport_info(1, dev, "REG_SP_DIRECT_CTRL3:%x",tmp);
		tmp = readl(sportx + REG_SP_DIRECT_CTRL4);
		sport_info(1, dev, "REG_SP_DIRECT_CTRL4:%x",tmp);
		tmp = readl(sportx + REG_SP_RX_COUNTER1);
		sport_info(1, dev, "REG_SP_RX_COUNTER1:%x",tmp);
		tmp = readl(sportx + REG_SP_RX_COUNTER2);
		sport_info(1, dev, "REG_SP_RX_COUNTER2:%x",tmp);
		tmp = readl(sportx + REG_SP_TX_FIFO_0_WR_ADDR);
		sport_info(1, dev, "REG_SP_TX_FIFO_0_WR_ADDR:%x",tmp);
		tmp = readl(sportx + REG_SP_RX_FIFO_0_RD_ADDR);
		sport_info(1, dev, "REG_SP_RX_FIFO_0_RD_ADDR:%x",tmp);
		tmp = readl(sportx + REG_SP_TX_FIFO_1_WR_ADDR);
		sport_info(1, dev, "REG_SP_TX_FIFO_1_WR_ADDR:%x",tmp);
		tmp = readl(sportx + REG_SP_RX_FIFO_1_RD_ADDR);
		sport_info(1, dev, "REG_SP_RX_FIFO_1_RD_ADDR:%x",tmp);
	}
}

static const struct of_device_id ameba_sport_match[] = {
	{ .compatible = "realtek,ameba-sport", },
	{},
};
MODULE_DEVICE_TABLE(of, ameba_sport_match);

static int sport_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	int i;
	int fifo_bytes = 128;
	sport_info(1,&sport->pdev->dev,"%s",__func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

		mutex_lock(&sport->state_mutex);
		if (is_playback) {
			sport->play_record_in_use |= PLAY_IN_USE;
		} else {
			sport->play_record_in_use |= RECORD_IN_USE;
		}
		mutex_unlock(&sport->state_mutex);

		audio_sp_dma_cmd(sport->addr, true);

		if (is_playback) {
			/*should init last_total_delay as dma buf size, so that irq compensate will succeed*/
			sport->last_total_delay = sport->tx_sport_compare_val;
			sport->total_tx_counter_boundary = substream->runtime->boundary;
			while ((sport->tx_sport_compare_val * 2 <= sport->total_tx_counter_boundary) && (sport->tx_sport_compare_val * 2 <= MAX_SPORT_IRQ_X) )
				sport->tx_sport_compare_val *= 2;
			sport->total_tx_counter = 0;
			sport->irq_tx_count = 0;
			sport->dma_playback.total_sport_counter = 0;
			sport_info(1,&sport->pdev->dev,"tx X:%llu, counter boundary:%llu", sport->tx_sport_compare_val, sport->total_tx_counter_boundary);
			audio_sp_tx_set_fifo(sport->addr, sport->fifo_num, true);
			audio_sp_tx_start(sport->addr, true);
			audio_sp_set_tx_count(sport->addr, (u32)(sport->tx_sport_compare_val));
		} else {
			/*should init last_total_delay as dma buf size, so that irq compensate will succeed*/
			sport->last_total_delay = sport->rx_sport_compare_val;
			sport->total_rx_counter_boundary = substream->runtime->boundary;
			while ((sport->rx_sport_compare_val * 2 <= sport->total_rx_counter_boundary) && (sport->rx_sport_compare_val * 2 <= MAX_SPORT_IRQ_X) )
				sport->rx_sport_compare_val *= 2;
			sport->total_rx_counter = 0;
			sport->irq_rx_count = 0;
			sport->dma_capture.total_sport_counter = 0;
			sport_info(1,&sport->pdev->dev,"rx X:%llu, counter boundary:%llu", sport->rx_sport_compare_val, sport->total_rx_counter_boundary);
			audio_sp_rx_set_fifo(sport->addr, sport->fifo_num, true);
			audio_sp_rx_start(sport->addr, true);
			audio_sp_set_rx_count(sport->addr, (u32)(sport->rx_sport_compare_val));
		}

		if (sport->sport_debug == 2 ){
			msleep(8); //sleep 8ms to wait for rx data to debug
			if (!is_playback ){
				for (i = 0;i < fifo_bytes; i++)
					sport_verbose(2,&sport->pdev->dev,"trigger sport1 fifo0: %x",readb(sport->addr + REG_SP_RX_FIFO_0_RD_ADDR + i));
			}
			sport_info(1, &sport->pdev->dev,"%s start done\n",__func__);
		}

		dumpSportRegs(&sport->pdev->dev, sport->addr);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		mutex_lock(&sport->state_mutex);
		if (is_playback) {
			sport->play_record_in_use &= ~PLAY_IN_USE;
		} else {
			sport->play_record_in_use &= ~RECORD_IN_USE;
		}
		mutex_unlock(&sport->state_mutex);

		if (sport->play_record_in_use & 0x03 == 0)
			audio_sp_dma_cmd(sport->addr, false);

		if (is_playback) {
			audio_sp_tx_start(sport->addr, false);
			audio_sp_disable_tx_count(sport->addr);
			audio_sp_tx_set_fifo(sport->addr, sport->fifo_num, false);
		} else {
			audio_sp_rx_start(sport->addr, false);
			audio_sp_disable_rx_count(sport->addr);
			audio_sp_rx_set_fifo(sport->addr, sport->fifo_num, false);
		}

		sport_info(1, &sport->pdev->dev,"%s stop done \n",__func__);
		break;
	}

	return 0;
}

static int enable_audio_source_clock(struct clock_params * params, int id, bool enabled)
{
	int ret = 0;
	if (params->clock == PLL_CLOCK_98P304M) {
#if (!I2S_PLL_ALWAYS_ON)
		update_98MP304M_input_clock_status(enabled);
#endif
		if (enabled)
			choose_input_clock_for_sport_index(id, CKSL_I2S_PLL98M);
	} else if (params->clock == PLL_CLOCK_45P1584M) {
#if (!I2S_PLL_ALWAYS_ON)
		update_45MP158_input_clock_status(enabled);
#endif
		if (enabled)
			choose_input_clock_for_sport_index(id, CKSL_I2S_PLL45M);
	} else if (params->clock == PLL_CLOCK_24P576M) {
#if (!I2S_PLL_ALWAYS_ON)
		update_24MP576_input_clock_status(enabled);
#endif
		if (enabled)
			choose_input_clock_for_sport_index(id, CKSL_I2S_PLL24M);
	}
	return ret;
}

/*
 * calculate the runtime delay
 * to notice that: for alsa fwk, the boundary is calculated as follows:
 * LONG_MAX:0x7fffffffUL
 * runtime->boundary = runtime->buffer_size;
 * while (runtime->boundary * 2 <= LONG_MAX - runtime->buffer_size)
 *		runtime->boundary *= 2;
 * this means the runtime->boundary is always the interger multiple of buffer_size.
 * sport->tx_sport_compare_val is set as buffer_size, so the runtime->boundary is multiple
 * of tx_sport_compare_val or rx_sport_compare_val.
 */
static snd_pcm_sframes_t sport_delay(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	u64 counter = 0;
	/* frames totally delivered through LRCLK */
	u64 total_counter = 0;
	/* delay(frames) is the value of substream->runtime->delay */
	u64 delay = 0;
	/* delay(frames) of dma buffer */
	u64 dma_buf_delay = 0;
	u64 total_delay = 0;
	bool is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct sport_dai *sport;
	sport = snd_soc_dai_get_drvdata(dai);

	if (is_playback) {
		if (sport->dma_playback.use_mmap == true)
			return 0;
	} else {
		if (sport->dma_capture.use_mmap == true)
			return 0;
	}

	spin_lock(&sport->lock);
	if (is_playback) {
		audio_sp_set_phase_latch(sport->addr);
		counter = audio_sp_get_tx_count(sport->addr);
		total_counter = counter + sport->irq_tx_count * sport->tx_sport_compare_val;
		dma_buf_delay = snd_pcm_playback_hw_avail(substream->runtime);
	} else {
		audio_sp_set_phase_latch(sport->addr);
		counter = audio_sp_get_rx_count(sport->addr);
		total_counter = counter + sport->irq_rx_count * sport->rx_sport_compare_val;
		dma_buf_delay = snd_pcm_capture_avail(substream->runtime);
	}

	if (is_playback) {
		/* application out of boundary, yet LRCLK frames not out of boundary */
		if (substream->runtime->control->appl_ptr < total_counter) {
			delay = substream->runtime->boundary - total_counter + substream->runtime->control->appl_ptr - dma_buf_delay;
			total_delay = substream->runtime->boundary - total_counter + substream->runtime->control->appl_ptr;
		} else {
			delay = substream->runtime->control->appl_ptr - total_counter - dma_buf_delay;
			total_delay = substream->runtime->control->appl_ptr - total_counter;
		}
	} else {
		if (substream->runtime->control->appl_ptr > total_counter) {
			delay = substream->runtime->boundary - substream->runtime->control->appl_ptr + total_counter - dma_buf_delay;
			total_delay = substream->runtime->boundary - substream->runtime->control->appl_ptr + total_counter;
		} else {
			delay = total_counter - substream->runtime->control->appl_ptr - dma_buf_delay;
			total_delay = total_counter - substream->runtime->control->appl_ptr;
		}
	}

#if COUNTER_COMPENSATE_BASING_ON_DELTA_DELAY
	/*
	 * Notice that if user add log in dma irq function, then dma may run slower than it should be, so appl_ptr will finally
	 * be less than total_counter, which will cause problem.
	 */
	if ((substream->runtime->control->appl_ptr < total_counter) && (total_delay > substream->runtime->boundary / 2)) {
		pr_info("total_counter ahead of appl_ptr, make sure no log in dma irq func.");
		delay = 0 - dma_buf_delay;
		goto delay_end;
	}
#endif

#if IRQ_COMPENSATE_BASING_ON_DELTA_DELAY
	/* See workaround here.If wifi can fix this, remove workaround in sport here.
	 * Sometimes if wifi marks all interrupts, then sport interrupt may loss, in this case we need to compensate sport irq predictly.
	 * If sport irq loss, then the i2s counter will be less than the real value for one counter_boundary size, which is one dma buffer size.
	 * If the current sw+hw latency suddenly changes big value, we compensate the irq.
	 * The delay function is always called when (1)appl_ptr is updated, (2)hw_ptr is update. So it should be the prop way to compensate
	 * sport irq in delay function.
	 */
	if (is_playback) {
		if (substream->runtime->control->appl_ptr >= sport->tx_sport_compare_val) {
			if ((total_delay > sport->last_total_delay) && (total_delay - sport->last_total_delay > (div_u64(sport->tx_sport_compare_val, 5) * 4))) {
				//pr_info("may loss tx sport irq, manually compensate");
				sport->irq_tx_count += 1;
				sport->total_tx_counter += sport->tx_sport_compare_val;
				if (sport->total_tx_counter >= sport->total_tx_counter_boundary) {
					sport->total_tx_counter -= sport->total_tx_counter_boundary;
					sport->irq_tx_count = 0;
				}
				sport->dma_playback.total_sport_counter = sport->total_tx_counter;
				total_delay -= sport->tx_sport_compare_val;
				delay -= sport->tx_sport_compare_val;
			}
		}
	} else {
		if (substream->runtime->control->appl_ptr >= sport->rx_sport_compare_val) {
			if ((total_delay > sport->last_total_delay) && (total_delay - sport->last_total_delay > (div_u64(sport->rx_sport_compare_val, 5) * 4))) {
				//pr_info("may loss rx sport irq, manually compensate");
				sport->irq_rx_count += 1;
				sport->total_rx_counter += sport->rx_sport_compare_val;
				if (sport->total_rx_counter >= sport->total_rx_counter_boundary) {
					sport->total_rx_counter -= sport->total_rx_counter_boundary;
					sport->irq_rx_count = 0;
				}
				sport->dma_capture.total_sport_counter = sport->total_rx_counter;
				total_delay -= sport->rx_sport_compare_val;
				delay -= sport->rx_sport_compare_val;
			}
		}
	}
	sport->last_total_delay = total_delay;
#endif

delay_end:
	spin_unlock(&sport->lock);
	//pr_info("%s counter:%llu, appl_ptr:%lu total_counter:%llu sport->irq_tx_count:%llu dma_buf_delay:%llu total_delay:%llu, delay:%lld\n", __func__, counter, substream->runtime->control->appl_ptr, total_counter, sport->irq_tx_count, dma_buf_delay, total_delay, delay);

	return delay;
}

static unsigned int get_sp_df_for_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct sport_dai *sport;
	unsigned int sp_df = SP_DF_LEFT;

	sport = snd_soc_dai_get_drvdata(dai);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
	case SND_SOC_DAI_FORMAT_I2S:
		sp_df = SP_DF_I2S;
		break;
	case SND_SOC_DAI_FORMAT_LEFT_J:
		sp_df = SP_DF_LEFT;
		break;
	case SND_SOC_DAI_FORMAT_DSP_A:
		sp_df = SP_DF_PCM_A;
		break;
	case SND_SOC_DAI_FORMAT_DSP_B:
		sp_df = SP_DF_PCM_B;
		break;
	default:
		dev_err(&sport->pdev->dev,"unsupported fmt:%d", fmt);
		break;
	}

	return sp_df;
}

static int sport_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	sport_init_params sp_tx_init;
	sport_init_params sp_rx_init;
	unsigned int channel_count;
	unsigned int channel_length = 32;
	struct sport_dai *sport;
	channel_count = params_channels(params);
	sport = snd_soc_dai_get_drvdata(dai);

	sport_info(1,&sport->pdev->dev,"%s: sport addr:%p,channel:%d,rate:%dhz,width:%d",__func__,sport->addr,channel_count,params_rate(params),params_width(params));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		sport->tx_sport_compare_val = params_buffer_size(params);
		sp_tx_init.sp_sel_data_format = get_sp_df_for_fmt(dai, sport->dai_fmt);
		sp_tx_init.sp_sel_ch = SP_TX_CH_LR;
		sp_tx_init.sp_sr = SP_16K;
		sp_tx_init.sp_in_clock = sport->clock_in_from_dts;
		sp_tx_init.sp_sel_tdm = SP_TX_NOTDM;
		sp_tx_init.sp_sel_fifo = SP_TX_FIFO2;
		sp_tx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
		sp_tx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;

		if (sport->sport_multi_io == 1)
			sp_tx_init.sp_set_multi_io = SP_TX_MULTIIO_EN;
		else
			sp_tx_init.sp_set_multi_io = SP_TX_MULTIIO_DIS;

		/* this means the external codec MCLK needs to be imported by sport_mclk_multiplier*sport_MCLK */
		if (sport->sport_mclk_multiplier != 0 || sport->sport_fixed_mclk_max != 0) {
			if (sport->sport_multi_io == 1) {
				if (choose_pll_clock(2, channel_length, params_rate(params), sport->sport_mclk_multiplier, sport->sport_fixed_mclk_max, sport->audio_clock_params)!= 0)
					return -EINVAL;
			} else {
				if (choose_pll_clock(channel_count, channel_length, params_rate(params), sport->sport_mclk_multiplier, sport->sport_fixed_mclk_max, sport->audio_clock_params) != 0)
					return -EINVAL;
			}
			enable_audio_source_clock(sport->audio_clock_params, sport->id, true);
			update_fen_cke_sport_status(sport->id, true);
#if NO_MICRO_ADJUST
			set_rate_divider(sport->id, sport->audio_clock_params->pll_div);
#endif
			sp_tx_init.sp_in_clock = sport->audio_clock_params->clock / sport->audio_clock_params->pll_div;
#if NO_MICRO_ADJUST
			audio_sp_set_mclk_clk_div(sport->addr, sport->audio_clock_params->sport_mclk_div);
#else
			audio_sp_set_mclk_clk_div(sport->addr, 1);
#endif
		}

		switch (channel_count) {     //params_channels - Get the number of channels from the hw params
			case 8:
				sport->dma_playback.datawidth = 4;     //4 byte:32bit
				sp_tx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_tx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				sp_tx_init.sp_sel_tdm = SP_TX_TDM8;
				sp_tx_init.sp_sel_fifo = SP_TX_FIFO8;
				break;
			case 6:
				sport->dma_playback.datawidth = 4;     //4 byte:32bit
				sp_tx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_tx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				sp_tx_init.sp_sel_tdm = SP_TX_TDM6;
				sp_tx_init.sp_sel_fifo = SP_TX_FIFO6;
				break;
			case 4:
				sport->dma_playback.datawidth = 4;     //4 byte:32bit
				sp_tx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_tx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				sp_tx_init.sp_sel_tdm = SP_TX_TDM4;
				sp_tx_init.sp_sel_fifo = SP_TX_FIFO4;
				break;
			case 2:
				sport->dma_playback.datawidth = 4;     //4 byte:32bit
				sp_tx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_tx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				break;
			case 1:
				sport->dma_playback.datawidth = 4;
				sp_tx_init.sp_sel_i2s0_mono_stereo = SP_CH_MONO;
				sp_tx_init.sp_sel_i2s1_mono_stereo = SP_CH_MONO;
				break;
			default:
				dev_err(&sport->pdev->dev, "%d channels not supported\n",
						params_channels(params));
			return -EINVAL;
		}

		switch (params_width(params)) {     //params_width - get the number of bits of the sample format,like 16LS
			//case 8 is supported in spec, yet fwlib not support it now.
			case 16:
				sp_tx_init.sp_sel_word_len = SP_TXWL_16;
				sp_tx_init.sp_sel_ch_len = SP_TXCL_32;
				break;
			case 20:
				sp_tx_init.sp_sel_word_len = SP_TXWL_20;
				sp_tx_init.sp_sel_ch_len = SP_TXCL_32;
				break;
			case 24:
				sp_tx_init.sp_sel_word_len = SP_TXWL_24;
				sp_tx_init.sp_sel_ch_len = SP_TXCL_32;
				break;
			case 32:
				sp_tx_init.sp_sel_word_len = SP_TXWL_32;
				sp_tx_init.sp_sel_ch_len = SP_TXCL_32;
				break;
			default:
				dev_err(&sport->pdev->dev, "Format(%d) not supported\n",
						params_format(params));
				return -EINVAL;
		}

		if (IS_RATE_SUPPORTED(params_rate(params))) {
			sport->frmclk = params_rate(params);
			sp_tx_init.sp_sr = params_rate(params);
		} else {
			dev_err(&sport->pdev->dev,"unsupported samplerate:%d",sport->frmclk);
			return -EINVAL;
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE){
		sport->rx_sport_compare_val = params_buffer_size(params);
		sp_rx_init.sp_sel_data_format = get_sp_df_for_fmt(dai, sport->dai_fmt);
		sp_rx_init.sp_sel_ch = SP_RX_CH_LR;
		sp_rx_init.sp_sr = SP_16K;
		sp_rx_init.sp_in_clock = sport->clock_in_from_dts;
		sp_rx_init.sp_sel_tdm = SP_RX_NOTDM;
		sp_rx_init.sp_sel_fifo = SP_RX_FIFO2;
		sp_rx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
		sp_rx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;

		if (sport->sport_multi_io == 1)
			sp_rx_init.sp_set_multi_io = SP_RX_MULTIIO_EN;
		else
			sp_rx_init.sp_set_multi_io = SP_RX_MULTIIO_DIS;

		if (sport->sport_mclk_multiplier != 0 || sport->sport_fixed_mclk_max != 0) {
			if (sport->sport_multi_io == 1) {
				if (choose_pll_clock(2, channel_length, params_rate(params), sport->sport_mclk_multiplier, sport->sport_fixed_mclk_max, sport->audio_clock_params) != 0)
					return -EINVAL;
			} else {
				if (choose_pll_clock(channel_count, channel_length, params_rate(params), sport->sport_mclk_multiplier, sport->sport_fixed_mclk_max, sport->audio_clock_params) != 0)
					return -EINVAL;
			}
			enable_audio_source_clock(sport->audio_clock_params, sport->id, true);
			update_fen_cke_sport_status(sport->id, true);
#if NO_MICRO_ADJUST
			set_rate_divider(sport->id, sport->audio_clock_params->pll_div);
#endif
			sp_rx_init.sp_in_clock = sport->audio_clock_params->clock / sport->audio_clock_params->pll_div;
#if NO_MICRO_ADJUST
			audio_sp_set_mclk_clk_div(sport->addr, sport->audio_clock_params->sport_mclk_div);
#else
			audio_sp_set_mclk_clk_div(sport->addr, 1);
#endif
		}

		switch (channel_count) {     //params_channels - Get the number of channels from the hw params
			case 8:
				sport->dma_capture.datawidth = 4;
				sp_rx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_rx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				sp_rx_init.sp_sel_tdm = SP_RX_TDM8;
				sp_rx_init.sp_sel_fifo = SP_RX_FIFO8;
				break;
			case 6:
				sport->dma_capture.datawidth = 4;
				sp_rx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_rx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				sp_rx_init.sp_sel_tdm = SP_RX_TDM6;
				sp_rx_init.sp_sel_fifo = SP_RX_FIFO6;
				break;
			case 4:
				sport->dma_capture.datawidth = 4;
				sp_rx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_rx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				sp_rx_init.sp_sel_tdm = SP_RX_TDM4;
				sp_rx_init.sp_sel_fifo = SP_RX_FIFO4;
				break;
			case 2:
				sport->dma_capture.datawidth = 4;
				sp_rx_init.sp_sel_i2s0_mono_stereo = SP_CH_STEREO;
				sp_rx_init.sp_sel_i2s1_mono_stereo = SP_CH_STEREO;
				break;
			case 1:
				sport->dma_capture.datawidth = 4;
				sp_rx_init.sp_sel_i2s0_mono_stereo = SP_CH_MONO;
				sp_rx_init.sp_sel_i2s1_mono_stereo = SP_CH_MONO;
				break;
			default:
				dev_err(&sport->pdev->dev, "%d channels not supported\n",
						params_channels(params));
			return -EINVAL;
		}

		switch (params_width(params)) {     //params_width - get the number of bits of the sample format,like 16LS
			//case 8 is supported in spec, yet fwlib not support it now.
			case 16:
				sp_rx_init.sp_sel_word_len = SP_RXWL_16;
				sp_rx_init.sp_sel_ch_len = SP_RXCL_32;
				break;
			case 20:
				sp_rx_init.sp_sel_word_len = SP_RXWL_20;
				sp_rx_init.sp_sel_ch_len = SP_RXCL_32;
				break;
			case 24:
				sp_rx_init.sp_sel_word_len = SP_RXWL_24;
				sp_rx_init.sp_sel_ch_len = SP_RXCL_32;
				break;
			case 32:
				sp_rx_init.sp_sel_word_len = SP_RXWL_32;
				sp_rx_init.sp_sel_ch_len = SP_RXCL_32;
				break;
			default:
				dev_err(&sport->pdev->dev, "Format(%d) not supported\n",
					params_format(params));
			return -EINVAL;
		}

		if (IS_RATE_SUPPORTED(params_rate(params))) {
			sport->cap_frmclk = params_rate(params);
			sp_rx_init.sp_sr = params_rate(params);
		} else {
			dev_err(&sport->pdev->dev,"unsupported samplerate:%d",sport->cap_frmclk);
			return -EINVAL;
		}
	}

	snd_soc_dai_init_dma_data(dai, &sport->dma_playback, &sport->dma_capture);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		audio_sp_tx_init(sport->addr, &sp_tx_init);
		sport->fifo_num = sp_tx_init.sp_sel_fifo;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		audio_sp_rx_init(sport->addr, &sp_rx_init);
		sport->fifo_num = sp_rx_init.sp_sel_fifo;
	}

	audio_sp_set_i2s_mode(sport->addr, sport->sport_mode);

	return 0;
}

static int sport_hw_free(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);

	sport_info(1,&sport->pdev->dev,"%s",__func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sport->total_tx_counter = 0;
		sport->irq_tx_count = 0;
		sport->tx_sport_compare_val = 6144;
		sport->total_tx_counter_boundary = 0;
	} else {
		sport->total_rx_counter = 0;
		sport->total_rx_counter_boundary = 0;
		sport->irq_rx_count = 0;
		sport->rx_sport_compare_val = 6144;
	}

	return 0;
}

static int sport_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);
	sport->dai_fmt = fmt;

	return 0;
}

static int sport_startup(struct snd_pcm_substream *substream,
	  struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);
	int ret;
	sport_info(1,&sport->pdev->dev,"%s",__func__);

	/* enable sport clock */
	ret = clk_prepare_enable(sport->clock);
	if (ret < 0) {
		dev_err(&sport->pdev->dev, "Fail to enable clock %d\n", ret);
	}

	sport->clock_enabled = 1;
	if ((sport -> sport_debug) >= 1 && (sport->sport_mclk_multiplier != 0 || sport->sport_fixed_mclk_max != 0))
		audio_clock_dump();

	return 0;
}

static void sport_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);

	sport_info(1,&sport->pdev->dev,"%s",__func__);

	if (sport->play_record_in_use & 0x03 == 0) {
		clk_disable_unprepare(sport->clock);
		sport->clock_enabled = 0;
		//below to be done
		if (sport->sport_mclk_multiplier != 0 || sport->sport_fixed_mclk_max != 0) {
			/* disable sport clock */
			update_fen_cke_sport_status(sport->id, false);
			if (sport -> sport_debug >= 1)
				audio_clock_dump();
			//enable_audio_source_clock(sport->audio_clock_params, sport->id, false);
		}
	}

	sport_info(1, &sport->pdev->dev,"close");
}

#ifdef CONFIG_PM
static int ameba_sport_suspend(struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);

	sport_info(1, &sport->pdev->dev, "%s, clock enabled: %d", __func__, sport->clock_enabled);

	if (sport->clock_enabled) {
		clk_disable_unprepare(sport->clock);
		sport->clock_enabled = 0;
	}

	//below to be done
	if (sport->sport_mclk_multiplier != 0 || sport->sport_fixed_mclk_max != 0) {
		/* disable sport clock */
		update_fen_cke_sport_status(sport->id, false);
	}

	return 0;
}

static int ameba_sport_resume(struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);
	int ret;

	sport_info(1, &sport->pdev->dev, "%s", __func__);

	/* enable sport clock */
	ret = clk_prepare_enable(sport->clock);
	if (ret < 0) {
		dev_err(&sport->pdev->dev, "Fail to enable clock %d\n", ret);
	}

	sport->clock_enabled = 1;

	return ret;
}
#else
#define ameba_sport_suspend NULL
#define ameba_sport_resume NULL
#endif

static const struct snd_soc_dai_ops ameba_sport_dai_ops = {
	.trigger   = sport_trigger,
	#if USING_COUNTER
	.delay     = sport_delay,
	#endif
	.hw_params = sport_hw_params,
	.hw_free   = sport_hw_free,
	.set_fmt   = sport_set_fmt,
	.startup   = sport_startup,
	.shutdown  = sport_shutdown,
};


static const struct snd_kcontrol_new sport_controls[] = {

};

static int ameba_sport_dai_probe(struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);

	int i = 0;
	int fifo_byte = 128;

	sport_info(1,&sport->pdev->dev,"%s",__func__);

	sport->addr = ioremap(sport->base, sport->reg_size);
	if (sport->addr == NULL) {
		dev_err(&sport->pdev->dev, "cannot ioremap registers\n");
		return -ENXIO;
	}

	if (sport->sport_debug == 2){
		for (i = 0;i < fifo_byte; i++)
			sport_verbose(2,&sport->pdev->dev,"sp1 ff0: %x",readb(sport->addr + REG_SP_RX_FIFO_0_RD_ADDR + i));
	}

	/* disable rx, tx counter irq */
	audio_sp_disable_rx_tx_sport_irq(sport->addr);

	sport->dma_playback.dev_phys_0 = sport->base + REG_SP_TX_FIFO_0_WR_ADDR;
	sport->dma_playback.dev_phys_1 = sport->base + REG_SP_TX_FIFO_1_WR_ADDR;
	sport->dma_playback.datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;    //32bit
	sport->dma_playback.src_maxburst = SP_TX_FIFO_DEPTH/2;    //deliver 8 samples to fifo one time, because fifo is 16 sample depth
	sport->dma_playback.chan_name = "tx";
	sport->dma_playback.total_sport_counter = 0;
	sport->dma_playback.sport_base_addr = sport->addr;
	sport->dma_playback.use_mmap = false;

	sport->dma_capture.dev_phys_0 = sport->base + REG_SP_RX_FIFO_0_RD_ADDR;
	sport->dma_capture.dev_phys_1 = sport->base + REG_SP_RX_FIFO_1_RD_ADDR;
	sport->dma_capture.datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;    //32bit
	sport->dma_capture.src_maxburst = SP_RX_FIFO_DEPTH/2;    //deliver 8 samples to fifo one time, because fifo is 16 sample depth
	sport->dma_capture.chan_name = "rx";
	sport->dma_capture.total_sport_counter = 0;
	sport->dma_capture.sport_base_addr = sport->addr;
	sport->dma_capture.use_mmap = false;

	switch (sport->id)
	{
	case 0:
		sport->dma_playback.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT0F0_TX;
		sport->dma_capture.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT0F0_RX;
		sport->dma_capture.handshaking_1 = GDMA_HANDSHAKE_INTERFACE_SPORT0F1_RX;
		break;
	case 1:
		sport->dma_playback.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT1F0_TX;
		sport->dma_capture.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT1F0_RX;
		sport->dma_capture.handshaking_1 = GDMA_HANDSHAKE_INTERFACE_SPORT1F1_RX;
		break;
	case 2:
		sport->dma_playback.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT2F0_TX;
		sport->dma_capture.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT2F0_RX;
		sport->dma_playback.handshaking_1 = GDMA_HANDSHAKE_INTERFACE_SPORT2F1_TX;
		sport->dma_capture.handshaking_1 = GDMA_HANDSHAKE_INTERFACE_SPORT2F1_RX;
		break;
	case 3:
		sport->dma_playback.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT3F0_TX;
		sport->dma_capture.handshaking_0 = GDMA_HANDSHAKE_INTERFACE_SPORT3F0_RX;
		sport->dma_playback.handshaking_1 = GDMA_HANDSHAKE_INTERFACE_SPORT3F1_TX;
		sport->dma_capture.handshaking_1 = GDMA_HANDSHAKE_INTERFACE_SPORT3F1_RX;
		break;
	default:
		dev_err(&sport->pdev->dev, " sport id:%d not supported \n", sport->id);
		break;
	}

	sport->clock_enabled = 0;

	sport_info(1,&sport->pdev->dev,"%s, play phyaddr:%x,cap phyaddr:%x\n",__func__,sport->dma_playback.dev_phys_0,sport->dma_capture.dev_phys_0);

	snd_soc_add_component_controls(dai->component, sport_controls,
				       ARRAY_SIZE(sport_controls));
	return 0;
}

static int ameba_sport_dai_remove(struct snd_soc_dai *dai)
{
	struct sport_dai *sport = snd_soc_dai_get_drvdata(dai);
	iounmap(sport->addr);
	return 0;
}

static struct snd_soc_dai_driver ameba_sport_dai_drv[] = {
	{
		.name = "4100d000.sport",
		.id = 0,
        .playback = {
                .channels_min = 1,
                .channels_max = 8,
                .rates = SNDRV_PCM_RATE_8000_192000,
                .rate_min = 8000,
                .rate_max = 192000,
                .formats = SNDRV_PCM_FMTBIT_S16_LE |
							SNDRV_PCM_FORMAT_U16_LE |
							SNDRV_PCM_FORMAT_S20_LE |
							SNDRV_PCM_FORMAT_U20_LE |
							SNDRV_PCM_FMTBIT_S24_LE |
							SNDRV_PCM_FORMAT_U24_LE |
							SNDRV_PCM_FMTBIT_S32_LE |
							SNDRV_PCM_FMTBIT_U32_LE,
        },
		.probe = ameba_sport_dai_probe,
		.remove = ameba_sport_dai_remove,
		.ops = &ameba_sport_dai_ops,
		.suspend = ameba_sport_suspend,
		.resume = ameba_sport_resume,
	},
	{
		.name = "4100e000.sport",
		.id = 1,
		.capture = {
                .channels_min = 1,
                .channels_max = 8,
                .rates = SNDRV_PCM_RATE_8000_192000,
                .rate_min = 8000,
                .rate_max = 192000,
                .formats = SNDRV_PCM_FMTBIT_S16_LE |
							SNDRV_PCM_FORMAT_U16_LE |
							SNDRV_PCM_FORMAT_S20_LE |
							SNDRV_PCM_FORMAT_U20_LE |
							SNDRV_PCM_FMTBIT_S24_LE |
							SNDRV_PCM_FORMAT_U24_LE |
							SNDRV_PCM_FMTBIT_S32_LE |
							SNDRV_PCM_FMTBIT_U32_LE,
        },
		.probe = ameba_sport_dai_probe,
		.remove = ameba_sport_dai_remove,
		.ops = &ameba_sport_dai_ops,
		.suspend = ameba_sport_suspend,
		.resume = ameba_sport_resume,
	},
	{
		.name = "4100f000.sport",
		.id = 2,
        .playback = {
                .channels_min = 1,
                .channels_max = 8,
                .rates = SNDRV_PCM_RATE_8000_192000,
                .rate_min = 8000,
                .rate_max = 192000,
                .formats = SNDRV_PCM_FMTBIT_S16_LE |
							SNDRV_PCM_FORMAT_U16_LE |
							SNDRV_PCM_FORMAT_S20_LE |
							SNDRV_PCM_FORMAT_U20_LE |
							SNDRV_PCM_FMTBIT_S24_LE |
							SNDRV_PCM_FORMAT_U24_LE |
							SNDRV_PCM_FMTBIT_S32_LE |
							SNDRV_PCM_FMTBIT_U32_LE,
        },
		.capture = {
                .channels_min = 1,
                .channels_max = 8,
                .rates = SNDRV_PCM_RATE_8000_192000,
                .rate_min = 8000,
                .rate_max = 192000,
                .formats = SNDRV_PCM_FMTBIT_S16_LE |
							SNDRV_PCM_FORMAT_U16_LE |
							SNDRV_PCM_FORMAT_S20_LE |
							SNDRV_PCM_FORMAT_U20_LE |
							SNDRV_PCM_FMTBIT_S24_LE |
							SNDRV_PCM_FORMAT_U24_LE |
							SNDRV_PCM_FMTBIT_S32_LE |
							SNDRV_PCM_FMTBIT_U32_LE,
        },
		.probe = ameba_sport_dai_probe,
		.remove = ameba_sport_dai_remove,
		.ops = &ameba_sport_dai_ops,
		.suspend = ameba_sport_suspend,
		.resume = ameba_sport_resume,
	},
	{
		.name = "41010000.sport",
		.id = 3,
        .playback = {
                .channels_min = 1,
                .channels_max = 8,
                .rates = SNDRV_PCM_RATE_8000_192000,
                .rate_min = 8000,
                .rate_max = 192000,
                .formats = SNDRV_PCM_FMTBIT_S16_LE |
							SNDRV_PCM_FORMAT_U16_LE |
							SNDRV_PCM_FORMAT_S20_LE |
							SNDRV_PCM_FORMAT_U20_LE |
							SNDRV_PCM_FMTBIT_S24_LE |
							SNDRV_PCM_FORMAT_U24_LE |
							SNDRV_PCM_FMTBIT_S32_LE |
							SNDRV_PCM_FMTBIT_U32_LE,
        },
		.capture = {
                .channels_min = 1,
                .channels_max = 8,
                .rates = SNDRV_PCM_RATE_8000_192000,
                .rate_min = 8000,
                .rate_max = 192000,
                .formats = SNDRV_PCM_FMTBIT_S16_LE |
							SNDRV_PCM_FORMAT_U16_LE |
							SNDRV_PCM_FORMAT_S20_LE |
							SNDRV_PCM_FORMAT_U20_LE |
							SNDRV_PCM_FMTBIT_S24_LE |
							SNDRV_PCM_FORMAT_U24_LE |
							SNDRV_PCM_FMTBIT_S32_LE |
							SNDRV_PCM_FMTBIT_U32_LE,
        },
		.probe = ameba_sport_dai_probe,
		.remove = ameba_sport_dai_remove,
		.ops = &ameba_sport_dai_ops,
		.suspend = ameba_sport_suspend,
		.resume = ameba_sport_resume,
	},
};

static const struct snd_soc_component_driver ameba_sport_component = {
	.name = "ameba,sport",
};


struct sport_dai *sport_alloc_dai(struct platform_device *pdev)
{
	struct sport_dai *sport;

	sport = devm_kzalloc(&pdev->dev, sizeof(struct sport_dai), GFP_KERNEL);
	if (sport == NULL)
		return NULL;

	sport->pdev = pdev;

	return sport;
}

/*
 * when sport LRCLK delivered tx_sport_compare_val or rx_sport_compare_val frames,
 * the interrupt callback is triggered.
 * make sure the max size of total counter matches the substream->runtime->boundary.
 */
static irqreturn_t rtk_sport_interrupt(int irq, void * dai)
{
	struct sport_dai *sport = dai;
	spin_lock(&sport->lock);

	if (audio_sp_is_rx_sport_irq(sport->addr)) {
		sport->irq_rx_count++;
		sport->total_rx_counter += sport->rx_sport_compare_val;
		if (sport->total_rx_counter >= sport->total_rx_counter_boundary) {
			sport->total_rx_counter -= sport->total_rx_counter_boundary;
			sport->irq_rx_count = 0;
		}
		sport->dma_capture.total_sport_counter = sport->total_rx_counter;
		audio_sp_clear_rx_sport_irq(sport->addr);
	} else if (audio_sp_is_tx_sport_irq(sport->addr)) {
		sport->irq_tx_count++;
		//pr_info("irq_tx_count:%llu", sport->irq_tx_count);
		sport->total_tx_counter += sport->tx_sport_compare_val;
		if (sport->total_tx_counter >= sport->total_tx_counter_boundary) {
			sport->total_tx_counter -= sport->total_tx_counter_boundary;
			sport->irq_tx_count = 0;
		}
		sport->dma_playback.total_sport_counter = sport->total_tx_counter;
		audio_sp_clear_tx_sport_irq(sport->addr);
	}

	spin_unlock(&sport->lock);
	return IRQ_HANDLED;
}


/*
* for clk set, get clk first, then set parent relationship ,then enable from
* parent to child. To notice: cke_ac is enabled in codec. sport using clk from cke_ac
*/
int set_sport_clk(struct platform_device *pdev, struct sport_dai *sport){
	int ret = 0;

	sport_info(1, &pdev->dev,"%s",__func__);

	/* get sport clock */
	sport->clock = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sport->clock)) {
		dev_err(&pdev->dev, "Fail to get clock %d\n",__LINE__);
		ret = -1;
		goto err;
	}

	sport->clock_in_from_dts = clk_get_rate(sport->clock);
	sport_info(1, &pdev->dev,"(%d)current clk rate is :%d", sport->id, sport->clock_in_from_dts);

err:
	return ret;
}


static int ameba_sport_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	int ret;
	u32 sportx_regs_base;
	struct sport_dai *sport = NULL;

	int sportx_reg_size;
	struct resource *resource_reg;

	resource_reg = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	sportx_regs_base = resource_reg->start;
	sportx_reg_size = resource_reg->end - resource_reg->start + 1;

	sport = sport_alloc_dai(pdev);
	if (!sport) {
		dev_err(&pdev->dev, "Unable to alloc sport_pri\n");
		ret = -ENOMEM;
		return ret;
	}

	sport->base = sportx_regs_base;
	sport->reg_size = sportx_reg_size;

	sport->sport_debug = 0;
	if (sysfs_create_group(&pdev->dev.kobj,&sport_debug_attr_grp))
		dev_warn(&pdev->dev, "Error creating sysfs entry\n");

	sport_info(1,&pdev->dev,"%s,sportx_reg_base:%x,sportx_reg_size:%x \n",__func__,sportx_regs_base,sportx_reg_size);

	if (!request_mem_region(sport->base, sportx_reg_size,
							"ameba-sport")) {
		dev_err(&pdev->dev, "Unable to request SFR region\n");
		ret = -EBUSY;
		goto err;
	}

	/* Reads the channel id from the device tree and stores in data->id */
	ret = of_property_read_u32_index(dev->of_node, "id", 0, &sport->id);
	if (ret < 0) {
		dev_err(&pdev->dev, "id property reading fail\n");
		goto err;
	}

	/* default multiplier 0 */
	sport->sport_mclk_multiplier = 0;
	/* Reads the mclk mutiplier value from the device tree and stores in data->sport_mclk_multiplier */
	of_property_read_u32_index(dev->of_node, "rtk,sport-mclk-multiplier", 0, &sport->sport_mclk_multiplier);

	sport->sport_fixed_mclk_max = 0;
	/* Reads the fixed mclk max value from the device tree and stores in data->sport_fixed_mclk_max */
	of_property_read_u32_index(dev->of_node, "rtk,sport-mclk-fixed-max", 0, &sport->sport_fixed_mclk_max);

	/* default sport_multi_io 0 */
	sport->sport_multi_io = 0;
	/* Reads the sport_multi_io value from the device tree and stores in data->sport_multi_io */
	of_property_read_u32_index(dev->of_node, "rtk,sport-multi-io", 0, &sport->sport_multi_io);

	/* default sport_mode 0(master) */
	sport->sport_mode = 0;
	/* Reads the sport_mode value from the device tree and stores in data->sport_mode */
	of_property_read_u32_index(dev->of_node, "rtk,sport-mode", 0, &sport->sport_mode);

	/* Read irq of sport */
	sport->irq = platform_get_irq(pdev, 0);

	/* request irq with irq func. */
	ret = devm_request_irq(&pdev->dev, sport->irq, rtk_sport_interrupt, 0, dev_name(&pdev->dev), sport);
	sport->total_tx_counter = 0;
	sport->total_rx_counter = 0;
	sport->total_tx_counter_boundary = 0;
	sport->total_rx_counter_boundary = 0;
	sport->irq_tx_count = 0;
	sport->irq_rx_count = 0;
	sport->tx_sport_compare_val = 6144;
	sport->rx_sport_compare_val = 6144;
	sport->dai_fmt = SND_SOC_DAIFMT_LEFT_J;
	sport->fifo_num = 0;
	sport->clock_enabled = 0;
	sport->play_record_in_use = 0x00;
	mutex_init(&sport->state_mutex);

	spin_lock_init(&sport->lock);

	/*for internal sport0, and sport1, using 40M to support all kinds of samplerates below 192k*/
	ret = set_sport_clk(pdev, sport);
	if (ret < 0){
		dev_err(&pdev->dev, "set sport clock err");
		goto err;
	}

	/*for external sport2, and sport3, using rcc node to control clocks*/
	if (sport->sport_mclk_multiplier != 0 || sport->sport_fixed_mclk_max != 0) {
		sport->audio_clock_params = devm_kzalloc(dev, sizeof(struct clock_params), GFP_KERNEL);
		if (sport->audio_clock_params == NULL){
			dev_err(&pdev->dev, "alloc audio_clock_params fail");
			ret = -ENOMEM;
			goto err;
    	}

	}

	/* Pre-assign snd_soc_dai_set_drvdata */
	dev_set_drvdata(&sport->pdev->dev, sport);       // set drv data, for sport_startup to get

	ret = devm_snd_soc_register_component(&pdev->dev,
					&ameba_sport_component,
					&ameba_sport_dai_drv[sport->id], 1);
	return 0;

err:
	release_mem_region(sport->base, sportx_reg_size);
	return ret;
}

static int ameba_sport_remove(struct platform_device *pdev)
{
    //do we need to release sport here?? need check
	struct sport_dai *sport;

	sport_info(1,&pdev->dev,"%s\n",__func__);

	sport = dev_get_drvdata(&pdev->dev);

	release_mem_region(sport->base, sport->reg_size);

	sysfs_remove_group(&pdev->dev.kobj,&sport_debug_attr_grp);

	return 0;
}


static struct platform_driver ameba_sport_driver = {
	.probe  = ameba_sport_probe,
	.remove = ameba_sport_remove,
	.driver = {
		.name = "ameba-sport",
		.of_match_table = of_match_ptr(ameba_sport_match),
	},
};

module_platform_driver(ameba_sport_driver);

/* Module information */
MODULE_DESCRIPTION("Realtek Ameba ALSA driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
