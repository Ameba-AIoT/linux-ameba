// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/of.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

//why need sport.h in dma? Because we need to check the I2S counter for get_time_info
#include "ameba_sport.h"
#include "dma.h"

#define IS_6_8_CHANNEL(NUM) (((NUM) == 6) || \
								((NUM) == 8))
#define TDM_TWO_DMAS_SYNC 1
#define USING_COUNTER     1

struct dev_data{
	int dma_debug;
};

#define dma_info(mask,dev, ...)						\
do{									\
	if (((struct dev_data *)(dev_get_drvdata(dev))) != NULL)					\
		if ((((struct dev_data *)(dev_get_drvdata(dev)))->dma_debug) & mask)		\
			dev_info(dev, ##__VA_ARGS__); \
}while(0)


static ssize_t dma_debug_show(struct device *dev, struct device_attribute*attr, char *buf)
{
		struct dev_data *data = dev_get_drvdata(dev);
        return sprintf(buf, "dma_debug=%d\n", data->dma_debug);
}

static ssize_t dma_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct dev_data *data = dev_get_drvdata(dev);

	data->dma_debug = (buf[0] == '1')? 1:0;

    return count;
}

static DEVICE_ATTR(dma_debug, S_IWUSR |S_IRUGO, dma_debug_show, dma_debug_store);

static struct attribute *dma_debug_attrs[] = {
        &dev_attr_dma_debug.attr,
        NULL
};

static const struct attribute_group dma_debug_attr_grp = {
        .attrs = dma_debug_attrs,
};

static const struct snd_pcm_hardware gdma_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		    SNDRV_PCM_INFO_BLOCK_TRANSFER |
		    SNDRV_PCM_INFO_MMAP |
		    SNDRV_PCM_INFO_MMAP_VALID |
		    SNDRV_PCM_INFO_PAUSE |
		    SNDRV_PCM_INFO_RESUME |
			#if USING_COUNTER
			SNDRV_PCM_INFO_HAS_LINK_ATIME |
			#endif
		    SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,
	.buffer_bytes_max = MAX_IDMA_BUFFER,
	.period_bytes_min = 128,
	.period_bytes_max = MAX_IDMA_PERIOD,
	.periods_min = 1,
	.periods_max = 4096,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_U16_LE |
				SNDRV_PCM_FORMAT_S20_LE |
				SNDRV_PCM_FMTBIT_U20_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_U24_LE |
				SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_U32_LE,
};

//dma buffer infos
struct ameba_pcm_dma_data {
	struct dma_chan *chan;
	struct dma_async_tx_descriptor *desc;
	struct dma_callback_params dma_param;
	dma_addr_t phys;    //physical address for DMA buffer
	int period_bytes;   //period_bytes period bytes to use for DMA buffer, not period_size
	int period_num;     //peroid_num to use for DMA buffer
	int buffer_size;    //period_bytes*period_num;
	int channels;
	dma_cookie_t cookie;
	unsigned int be_running;
	int dma_addr_offset;
	unsigned int dma_id;  //for 2-4channel, dma_id:0; for 6-8channel in 6/8 channel case, dma_id:1;
};

struct ameba_pcm_dma_private {
	struct snd_pcm_substream *substream;
	struct ameba_pcm_dma_params *pcm_params;     //sport information to set to dma
	struct ameba_pcm_dma_data pcm_data_0;        //dma data for fifo0
	struct ameba_pcm_dma_data pcm_data_1;        //dma data for fifo1
	unsigned int sync_dma_0_1;                   //sync dma0 & dma1 for 6/8channel.
};

static u64 ameba_adjust_codec_delay(struct snd_pcm_substream *substream,
				u64 nsec)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	u64 codec_frames, codec_nsecs;

	if (!codec_dai->driver->ops->delay)
		return nsec;

	codec_frames = codec_dai->driver->ops->delay(substream, codec_dai);
	codec_nsecs = div_u64(codec_frames * 1000000000LL,
			      substream->runtime->rate);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return nsec + codec_nsecs;

	return (nsec > codec_nsecs) ? nsec - codec_nsecs : 0;
}

static u64 ameba_get_dai_counter_ntime(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct ameba_pcm_dma_params *dma_params = dma_private->pcm_params;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	u64 nsec = 0;
	//current total i2s counter of audio frames;
	u64 now_counter = 0;
	//means the delta_counter between now counter and last irq total counter.
	u64 delta_counter = 0;
	u32 phase = 0;

	if (dma_params == NULL) {
		return -EFAULT;
	}

	if (is_playback) {
		audio_sp_set_phase_latch(dma_params->sport_base_addr);
		delta_counter = audio_sp_get_tx_count(dma_params->sport_base_addr);
		phase = audio_sp_get_tx_phase_val(dma_params->sport_base_addr);
	} else {
		audio_sp_set_phase_latch(dma_params->sport_base_addr);
		delta_counter = audio_sp_get_rx_count(dma_params->sport_base_addr);
		phase = audio_sp_get_rx_phase_val(dma_params->sport_base_addr);
	}

	now_counter = dma_params->total_sport_counter + delta_counter;
	//this will cause __aeabi_uldivmod compile issue, so need to use div_u64 func.
	//nsec = now_counter * 1000000000 / runtime->rate;
	//pr_info("dma_params->total_sport_counter:%lld, delta_counter:%lld, now_counter:%lld \n", dma_params->total_sport_counter, delta_counter, now_counter);
	//1000000000*phase/32 = 31250000 * phase
	nsec = div_u64(now_counter * 1000000000LL + 31250000LL * (u64)phase, runtime->rate);

	return nsec;
}

static int ameba_get_time_info(struct snd_pcm_substream *substream,
			struct timespec *system_ts, struct timespec *audio_ts,
			struct snd_pcm_audio_tstamp_config *audio_tstamp_config,
			struct snd_pcm_audio_tstamp_report *audio_tstamp_report)
{
	u64 nsec;

	if ((substream->runtime->hw.info & SNDRV_PCM_INFO_HAS_LINK_ATIME) &&
		(audio_tstamp_config->type_requested == SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK)) {
		snd_pcm_gettime(substream->runtime, system_ts);

		nsec = ameba_get_dai_counter_ntime(substream);
		if (audio_tstamp_config->report_delay)
			nsec = ameba_adjust_codec_delay(substream, nsec);

		*audio_ts = ns_to_timespec(nsec);
		//pr_info("%s nsec:%lld", __func__, nsec);
		audio_tstamp_report->actual_type = SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK;
		audio_tstamp_report->accuracy_report = 1; /* rest of struct is valid */
		audio_tstamp_report->accuracy = 42; /* 24MHzWallClk == 42ns resolution */

	} else {
		audio_tstamp_report->actual_type = SNDRV_PCM_AUDIO_TSTAMP_TYPE_DEFAULT;
	}

	return 0;
}

static int gdma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, "ameba-gdma");

	struct device *dev = component->dev;
	struct ameba_pcm_dma_private *dma_private;

	dma_info(1,component->dev,"%s",__func__);

	snd_soc_set_runtime_hwparams(substream, &gdma_hardware);

	/* Ensure that buffer size is a multiple of period size */
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	dma_private = devm_kzalloc(dev, sizeof(*dma_private), GFP_KERNEL);

	if (!dma_private)
		return -ENOMEM;

	runtime->private_data = dma_private;
	dma_private->substream = substream;
	dma_private->sync_dma_0_1 = 0;
	//Because in soc-pcm.c soc_new_pcm:rtd->ops.get_time_info is not set.
	//substream->ops = rtd->ops, so it's get_time_info is not set,too.
	//This means add get_time_info in gdma_ops does not work.
	//Need to set it ourself.the substream->ops is const, need to force change to normal.
	#if USING_COUNTER
	((struct snd_pcm_ops*)(substream->ops))->get_time_info = ameba_get_time_info;
	#endif
	return 0;
}

static int gdma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	struct device *dev = component->dev;

	dma_info(1,component->dev,"%s",__func__);

	devm_kfree(dev, dma_private);
	return 0;
}


static int gdma_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	dma_private->pcm_params->use_mmap = true;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return remap_pfn_range(vma, vma->vm_start,
			       runtime->dma_addr >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
}

static void ameba_pcm_release_dma_channel(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");

	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct ameba_pcm_dma_data *data_0 = NULL;
	struct ameba_pcm_dma_data *data_1 = NULL;

	data_0 = &(dma_private->pcm_data_0);
	if (data_0->chan) {
		dma_info(1,component->dev,"release dma for fifo0");
		dma_release_channel(data_0->chan);
		data_0->chan = NULL;
	}

	if (IS_6_8_CHANNEL(runtime->channels)) {
		data_1 = &(dma_private->pcm_data_1);
		if (data_1->chan) {
			dma_info(1,component->dev,"release dma for fifo1");
			dma_release_channel(data_1->chan);
			data_1->chan = NULL;
		}
	}
}

static int ameba_pcm_request_dma_channel(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	struct device *dev = component->dev;
	struct ameba_pcm_dma_params *dma_params = dma_private->pcm_params;
	struct ameba_pcm_dma_data *data_0 = NULL;
	struct ameba_pcm_dma_data *data_1 = NULL;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	const char *dma_names;
	struct of_phandle_args	dma_spec;
	int i,j;
	int dma_count;
	/***check if dts channel and name set right***/
	if (dev->of_node){
		dma_count = of_property_count_strings(dev->of_node, "dma-names");
		for(j = 0; j < dma_count; j++){
			of_property_read_string_index(dev->of_node, "dma-names", j, &dma_names);
			dma_info(1,component->dev,"dts: dma_name:[%s]",dma_names);
			of_parse_phandle_with_args(dev->of_node, "dmas", "#dma-cells", j,
						       &dma_spec);
			for (i = 0;i< dma_spec.args_count;i++){
				dma_info(1,component->dev,"dts: dma-cells[%d] = %d , dma_spec.args_count:%d\n",i,dma_spec.args[i],dma_spec.args_count);
			}
		}
	}


	data_0 = &(dma_private->pcm_data_0);

	/***request channel***/
	if(is_playback){
		data_0->chan = dma_request_chan(dev,"tx-0");
	}else{
		data_0->chan = dma_request_chan(dev,"rx-0");
	}

	dma_info(1,component->dev,"chan:%d,is_playback:%d",data_0->chan->chan_id,is_playback);

	if (!data_0->chan) {
		dev_err(dev, "failed to request dma channel:%s\n",
			dma_params->chan_name);
		ameba_pcm_release_dma_channel(substream);
		return -ENODEV;
	}

	if (IS_6_8_CHANNEL(params_channels(params))) {
		/***request channel***/
		data_1 = &(dma_private->pcm_data_1);
		if(is_playback){
			data_1->chan = dma_request_chan(dev,"tx-1");
		}else{
			data_1->chan = dma_request_chan(dev,"rx-1");
		}

		dma_info(1,component->dev,"chan:%d,is_playback:%d",data_1->chan->chan_id,is_playback);

		if (!data_1->chan) {
			dev_err(dev, "failed to request dma channel:%s\n",
				dma_params->chan_name);
			ameba_pcm_release_dma_channel(substream);
			return -ENODEV;
		}
	}

	return 0;
}


static int load_dma_period(struct snd_pcm_substream *substream, struct ameba_pcm_dma_data *data)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct ameba_pcm_dma_params *dma_params;
	struct dma_chan *chan;
	struct dma_async_tx_descriptor *desc;
	struct dma_slave_config config;
	enum dma_transfer_direction dir;
	int ret;
	unsigned int dma_id = 0;

	if (data == NULL) {
		dev_err(component->dev, "%s data is NULL \n", __func__);
		return -EINVAL;
	}

	dma_id = data->dma_id;
	dma_params = dma_private->pcm_params;

	if (is_playback) {
		config.src_addr = runtime->dma_addr + data->dma_addr_offset;   //need double check
		if (dma_id == 0)
			config.dst_addr = dma_params->dev_phys_0;
		else
			config.dst_addr = dma_params->dev_phys_1;
		dir = DMA_MEM_TO_DEV;
	} else {
		if (dma_id == 0)
			config.src_addr = dma_params->dev_phys_0;
		else
			config.src_addr = dma_params->dev_phys_1;
		config.dst_addr = runtime->dma_addr + data->dma_addr_offset;
		dir = DMA_DEV_TO_MEM;
	}

	config.src_maxburst = dma_params->src_maxburst;     //deliver maxburst bytes to fifo one time
	config.dst_maxburst = dma_params->src_maxburst;     //deliver maxburst bytes to fifo one time

	config.src_addr_width = dma_params->datawidth;
	config.dst_addr_width = dma_params->datawidth;

	if (dma_id == 0)
		config.slave_id = dma_params->handshaking_0;
	else
		config.slave_id = dma_params->handshaking_1;

	config.src_port_window_size = 0;
	config.dst_port_window_size = 0;

	config.device_fc = 1;

	chan = data->chan;
	desc = data->desc;

	ret = dmaengine_slave_config(chan, &config);
	if (ret) {
		dma_info(1,component->dev,"failed to set slave configuration: %d\n", ret);
		return ret;
	}

	if (is_playback) {
		if (!runtime->no_period_wakeup)
			desc = dmaengine_prep_dma_cyclic(chan,
				config.src_addr, data->period_bytes, data->period_bytes/2, dir, DMA_PREP_INTERRUPT);
		else
			desc = dmaengine_prep_dma_cyclic(chan,
				config.src_addr, data->buffer_size, data->buffer_size, dir, 0);
	} else {
		if (!runtime->no_period_wakeup)
			desc = dmaengine_prep_dma_cyclic(chan,
				config.dst_addr, data->period_bytes, data->period_bytes/2, dir, DMA_PREP_INTERRUPT);//at lease 2 rounds for dma driver
		else
			desc = dmaengine_prep_dma_cyclic(chan,
				config.dst_addr, data->buffer_size, data->buffer_size, dir, 0);
	}

    if (!runtime->no_period_wakeup) {
		desc->callback = (dma_async_tx_callback)ameba_alsa_callback_handle;

		data->dma_param.substream = substream;
		data->dma_param.dma_id = dma_id;
		desc->callback_param = &data->dma_param;
	}

	data->desc = desc;

	return ret;
}

static void restart_dma_transfer(struct snd_pcm_substream *substream, struct ameba_pcm_dma_data *data)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	int ret;

	load_dma_period(substream, data);

	/* start to play*/
	data->cookie = dmaengine_submit(data->desc);
	ret = dma_submit_error(data->cookie);
	if (ret) {
		dma_info(1,component->dev,"error to submit:%d",ret);
	}

	dma_async_issue_pending(data->chan);
}

void ameba_alsa_callback_handle(struct dma_callback_params *params){
	struct snd_pcm_substream *substream = params->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct ameba_pcm_dma_data * data_0 = NULL;
	struct ameba_pcm_dma_data * data_1 = NULL;
	unsigned int data_0_channels = 4;
	unsigned int dma_sync_callback_count = 2;

#if TDM_TWO_DMAS_SYNC
	/*6/8 channel should wait for both dma callback, and then restart both dma, otherwise just return*/
	if (IS_6_8_CHANNEL(runtime->channels)) {
		dma_private->sync_dma_0_1 ++;
		if (dma_private->sync_dma_0_1 < dma_sync_callback_count)
			return;
		dma_private->sync_dma_0_1 = 0;
	}
#endif

#if (!TDM_TWO_DMAS_SYNC)
	if (params->dma_id == 0) {  //each dma transfer no sync with another
#endif

	data_0 = &(dma_private->pcm_data_0);

	data_0->dma_addr_offset += data_0->period_bytes * runtime->channels / data_0->channels;
	if (data_0->dma_addr_offset >= data_0->buffer_size  * runtime->channels / data_0->channels)
        data_0->dma_addr_offset = 0;

	if (!runtime->no_period_wakeup)
		snd_pcm_period_elapsed(substream);

	if (data_0->be_running)
		restart_dma_transfer(substream, data_0);

#if (!TDM_TWO_DMAS_SYNC)
	}  //each dma transfer no sync with another

	if (params->dma_id == 1) {  //each dma transfer no sync with another
#endif

	if (IS_6_8_CHANNEL(runtime->channels)) {
		data_1 = &(dma_private->pcm_data_1);

		data_1->dma_addr_offset += data_1->period_bytes * runtime->channels / data_1->channels;
		if (data_1->dma_addr_offset >= data_1->buffer_size * runtime->channels / data_1->channels)
			data_1->dma_addr_offset = data_1->period_bytes * data_0_channels / data_1->channels;

		if (data_1->be_running)
			restart_dma_transfer(substream, data_1);
	}

#if (!TDM_TWO_DMAS_SYNC)
	} //each every dma transfer no sync with another
#endif
}

static int gdma_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct ameba_pcm_dma_data *data_0 = NULL;
	struct ameba_pcm_dma_data *data_1 = NULL;
	struct ameba_pcm_dma_params *dma_params;
	unsigned int data_0_channels = 4;
	unsigned int data_1_channels = params_channels(params) - 4;
	int ret;

	data_0 = &(dma_private->pcm_data_0);
	data_0->chan = NULL;
	data_0->period_bytes = params_period_bytes(params);
	data_0->period_num = params_periods(params);
	data_0->buffer_size = params_buffer_bytes(params);
	data_0->channels = params_channels(params);
	data_0->dma_id = 0;

	if (IS_6_8_CHANNEL(params_channels(params))) {
		data_0->period_bytes = params_period_bytes(params) * data_0_channels / params_channels(params);
		data_0->period_num = params_periods(params);
		data_0->buffer_size = params_buffer_bytes(params) * data_0_channels / params_channels(params);
		data_0->channels = data_0_channels;

		data_1 = &(dma_private->pcm_data_1);
		data_1->period_bytes = params_period_bytes(params) * data_1_channels / params_channels(params);
		data_1->period_num = params_periods(params);
		data_1->buffer_size = params_buffer_bytes(params) * data_1_channels / params_channels(params);
		data_1->channels = data_1_channels;
		data_1->dma_id = 1;
		dma_info(1, component->dev,"%s, dma 1 get from user: period_bytes:%d,period_num:%d,buffer_size:%d,channels:%d,format bit:%d,format=%d; \n",__func__,data_1->period_bytes,data_1->period_num,data_1->buffer_size,data_1->channels,params_width(params),params_format(params));
	}

	dma_info(1, component->dev,"%s, dma 0 get from user: period_bytes:%d,period_num:%d,buffer_size:%d,channels:%d,format bit:%d,format=%d,rate:%d; \n",__func__,data_0->period_bytes,data_0->period_num,data_0->buffer_size,data_0->channels,params_width(params),params_format(params),params_rate(params));

	dma_params = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);     //dma data is set in sport.c
	if (!dma_params) {
		dev_warn(component->dev, "no dma parameters setting\n");
		dma_private->pcm_params = NULL;
		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
		runtime->dma_bytes = params_buffer_bytes(params);
		return 0;
	}

	if (!dma_private->pcm_params)
		dma_private->pcm_params = dma_params;

	dma_private->pcm_params->use_mmap = false;

	ret = ameba_pcm_request_dma_channel(substream, params);
	if (ret)
		return ret;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int gdma_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	ameba_pcm_release_dma_channel(substream);
	return 0;
}

static int gdma_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct ameba_pcm_dma_data *data_0 = NULL;
	struct ameba_pcm_dma_data *data_1 = NULL;

	data_0 = &(dma_private->pcm_data_0);
	data_0->be_running = 0;
	data_0->dma_addr_offset = 0;

	if (IS_6_8_CHANNEL(runtime->channels)) {
		data_1 = &(dma_private->pcm_data_1);
		data_1->be_running = 0;
		data_1->dma_addr_offset = data_1->period_bytes * 4 / data_1->channels;
	}

	return 0;
}

static int gdma_trigger_start(struct snd_pcm_substream *substream, struct ameba_pcm_dma_data *data)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	int ret;

	if (data == NULL) {
		dev_err(component->dev, "%s data is NULL \n", __func__);
		return -EINVAL;
	}

	load_dma_period(substream, data);

	if (!data->desc)
		return -EINVAL;

	if(data->be_running){
		dev_err(component->dev, "%s:already triggered\n",__func__);
		return 0;
	}

	data->cookie = dmaengine_submit(data->desc);
	ret = dma_submit_error(data->cookie);
	if (ret) {
		dev_err(component->dev,
			"failed to submit dma request: %d\n",
			ret);
		return -EINVAL;
	}

	if(NULL == (data->chan)){
		dev_err(component->dev, "%s chan is null! \n",__func__);
		return -EINVAL;
	}

	dma_async_issue_pending(data->chan);
	data->be_running = 1;
	dma_info(1, component->dev, "%s dma_id:%d \n",__func__, data->dma_id);

	return 0;
}

static int gdma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	int ret = 0;
	struct ameba_pcm_dma_data *data_0 = NULL;
	struct ameba_pcm_dma_data *data_1 = NULL;

	data_0 = &(dma_private->pcm_data_0);

	if (IS_6_8_CHANNEL(runtime->channels)) {
		data_1 = &(dma_private->pcm_data_1);
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = gdma_trigger_start(substream, data_0);

		if (IS_6_8_CHANNEL(runtime->channels)) {
			ret = gdma_trigger_start(substream, data_1);
		}
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (data_0->chan)
			dmaengine_resume(data_0->chan);

		data_0->be_running = 1;

		if (IS_6_8_CHANNEL(runtime->channels)) {
			if (data_1->chan)
				dmaengine_resume(data_1->chan);
			data_1->be_running = 1;
		}

		dma_info(1, component->dev, "%s resume \n",__func__);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		data_0->be_running = 0;
		if (data_0->chan) {
			ret = dmaengine_terminate_async(data_0->chan);   //dma terminate will stop dma immediately,now only set flag here
			if (ret) {
				dev_err(component->dev,"failed to terminate dma 0: %d\n", ret);
				return ret;
			}
		}

		if (IS_6_8_CHANNEL(runtime->channels)) {
			data_1->be_running = 0;
			if (data_1->chan) {
				ret = dmaengine_terminate_async(data_1->chan);   //dma terminate will stop dma immediately,now only set flag here
				if (ret) {
					dev_err(component->dev,"failed to terminate dma 1: %d\n", ret);
					return ret;
				}
			}
		}
		dma_info(1, component->dev, "%s stop \n",__func__);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (data_0->chan)
			dmaengine_pause(data_0->chan);
		data_0->be_running = 0;

		if (IS_6_8_CHANNEL(runtime->channels)) {
			if (data_1->chan)
				dmaengine_pause(data_1->chan);
			data_1->be_running = 0;
		}
		dma_info(1, component->dev, "%s pause \n",__func__);
		break;
	default:
		ret = -EINVAL;
		dev_err(component->dev, "%s:ret:%d \n",__func__,ret);
	}

	return ret;
}

static snd_pcm_uframes_t
	gdma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ameba_pcm_dma_private *dma_private = runtime->private_data;
	struct ameba_pcm_dma_data *data_0;
	struct dma_tx_state state;

	data_0 = &(dma_private->pcm_data_0);
	if (runtime->no_period_wakeup) {
		dmaengine_tx_status(data_0->chan, data_0->cookie, &state);
		//dma_info(1, component->dev, "delivered bytes:%d, res:%d", state.residue, res);
	}

	if (runtime->no_period_wakeup)
		return bytes_to_frames(substream->runtime, state.residue);
	else
		return bytes_to_frames(substream->runtime, data_0->dma_addr_offset);
}

/* calculate the target DMA-buffer position to be written/read
 * this function is only for interleaved data format.Because current
 * realtek ameba audio sport only support interleaved data.
 *
 * param channel here means to copy which channel, not the channel number.
 * For interleaved data, this channel always be 0.
 *
 * param hwoff means offset of bytes, not frames.
 */
static void *get_dma_ptr(struct snd_pcm_runtime *runtime,
			   int channel, unsigned long hwoff)
{
	return runtime->dma_area + hwoff +
		channel * (runtime->dma_bytes / runtime->channels);
}

/*this function is only used in channel 6/8 playback case*/
static ssize_t gdma_write(struct snd_pcm_substream *substream,
			      int channel, unsigned long hwoff,
			      void *buf, unsigned long bytes)
{
	unsigned int frame_num = 0;
	unsigned int format_bytes = 0;
	unsigned long offset_bytes = hwoff;
	unsigned long buf_offset = 0;
	unsigned int bits_per_byte = 8;
	/*channel count for first hardware dma transfer*/
	unsigned int first_channels = 4;
	/*channel count for second hardware dma transfer*/
	unsigned int last_channels = substream->runtime->channels - first_channels;
	int format_width = snd_pcm_format_width(substream->runtime->format);
	snd_pcm_sframes_t frames_to_write = bytes_to_frames(substream->runtime, bytes);

	format_bytes = format_width / bits_per_byte;
	if (format_bytes == 3)
		format_bytes = 4;

	/*write first 4 channel data to DMA buf*/
	for (; frame_num < frames_to_write; ++frame_num) {
		if (copy_from_user(get_dma_ptr(substream->runtime, channel, offset_bytes), (void __user *)buf + buf_offset, first_channels * format_bytes))
			return -EFAULT;
		offset_bytes += first_channels * format_bytes;
		buf_offset += substream->runtime->channels * format_bytes;
	}

	frame_num = 0;
	offset_bytes = hwoff + substream->runtime->period_size * first_channels * format_bytes;
	buf_offset = first_channels * format_bytes;

	/*write last 2/4 channel data to DMA buf*/
	for (; frame_num < frames_to_write; ++frame_num) {
		if (copy_from_user(get_dma_ptr(substream->runtime, channel, offset_bytes), (void __user *)buf + buf_offset, last_channels * format_bytes))
			return -EFAULT;
		offset_bytes += last_channels * format_bytes;
		buf_offset += substream->runtime->channels * format_bytes;
	}

	return 0;
}

/*this function is only used in channel 6/8 capture case*/
static ssize_t gdma_read(struct snd_pcm_substream *substream,
			      int channel, unsigned long hwoff,
			      void *buf, unsigned long bytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");
	unsigned int frame_num = 0;
	unsigned int format_bytes = 0;
	unsigned long offset_bytes = hwoff;
	unsigned long buf_offset = 0;
	unsigned int bits_per_byte = 8;
	/*channel count for first hardware dma transfer*/
	unsigned int first_channels = 4;
	/*channel count for second hardware dma transfer*/
	unsigned int last_channels = runtime->channels - first_channels;
	int format_width = snd_pcm_format_width(runtime->format);
	snd_pcm_sframes_t frames_to_read = bytes_to_frames(runtime, bytes);

	format_bytes = format_width / bits_per_byte;
	//for 24bit, we need to read 8+24bits.
	if (format_bytes == 3)
		format_bytes = 4;

	dma_info(1, component->dev,"bytes_to_read:%ld, last_channels:%d runtime->channels:%d format_bytes:%d, frames_to_read:%ld\n", bytes, last_channels, runtime->channels, format_bytes, frames_to_read);

	/*read first 4 channel data to user buf*/
	for (; frame_num < frames_to_read; ++frame_num) {
		if (copy_to_user((void __user *)buf + buf_offset, get_dma_ptr(runtime, channel, offset_bytes), first_channels * format_bytes))
			return -EFAULT;
		offset_bytes += first_channels * format_bytes;
		buf_offset += runtime->channels * format_bytes;
	}

	frame_num = 0;
	offset_bytes = hwoff + runtime->period_size * first_channels * format_bytes;
	buf_offset = first_channels * format_bytes;

	/*read last 2/4 channel data to user buf*/
	for (; frame_num < frames_to_read; ++frame_num) {
		if (copy_to_user((void __user *)buf + buf_offset, get_dma_ptr(runtime, channel, offset_bytes), last_channels * format_bytes))
			return -EFAULT;
		offset_bytes += last_channels * format_bytes;
		buf_offset += runtime->channels * format_bytes;
	}

	return 0;
}

/*
 * this function is only for interleaved data format.Because current
 * realtek ameba audio sport only support interleaved data.
 *
 * param channel here means to copy which channel, not the channel number.
 * For interleaved data, this channel always be 0.
 *
 * param hwoff means offset of bytes, not frames.
 */
static int gdma_copy(struct snd_pcm_substream *substream,
			      int channel, unsigned long hwoff,
			      void *buf, unsigned long bytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, "ameba-gdma");

	if (runtime->channels == 1 || runtime->channels == 2 || runtime->channels == 4) {
		if (is_playback) {
			if (copy_from_user(get_dma_ptr(runtime, channel, hwoff), (void __user *)buf, bytes))
				return -EFAULT;
		} else {
			if (copy_to_user((void __user *)buf, get_dma_ptr(runtime, channel, hwoff), bytes))
				return -EFAULT;
		}
	} else if (runtime->channels == 6 || runtime->channels == 8) {
		if (is_playback)
			gdma_write(substream, channel, hwoff, buf, bytes);
		else
			gdma_read(substream, channel, hwoff, buf, bytes);
	} else {
		dev_err(component->dev, " channel:%d not supported, please try 2/4/6/8 \n", channel);
		return -EFAULT;
	}

	return 0;
}

static const struct snd_pcm_ops gdma_ops = {
	.open		= gdma_open,
	.close		= gdma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.trigger	= gdma_trigger,
	.pointer	= gdma_pointer,
	.mmap		= gdma_mmap,
	.hw_params	= gdma_hw_params,
	.hw_free	= gdma_hw_free,
	.prepare	= gdma_prepare,
	.copy_user  = gdma_copy,
	#if USING_COUNTER
	.get_time_info = ameba_get_time_info,
	#endif
};

/*
 *mainly assign members'values for substream->dma_buffer,including
 *buffer addr , buffer size and so on
 */
static int gdma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_substream *substream;
	int ret;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;
	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (substream) {
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev,
									gdma_hardware.buffer_bytes_max,
									&substream->dma_buffer);
		if (ret) {
			dev_err(card->dev,
					"can't alloc playback dma buffer: %d\n", ret);
			return ret;
		}
	}

	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (substream) {
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev,
					  gdma_hardware.buffer_bytes_max,
					  &substream->dma_buffer);
		if (ret) {
			dev_err(card->dev,
				"can't alloc capture dma buffer: %d\n", ret);
			snd_dma_free_pages(&pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream->dma_buffer);
			return ret;
		}
	}
	return 0;
}

/*
 *release dma_buffer.
 */
static void gdma_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int i;

	for (i = 0; i < ARRAY_SIZE(pcm->streams); i++) {
		substream = pcm->streams[i].substream;
		if (!substream)
			return;

		buf = &substream->dma_buffer;
		if (!buf->area)
			return;

		snd_dma_free_pages(&substream->dma_buffer);
		buf->area = NULL;
		buf->addr = 0;
	}

}
static const struct snd_soc_component_driver asoc_gdma_platform = {
	.name = "ameba-gdma",
	.ops = &gdma_ops,
	.pcm_new = gdma_new,
	.pcm_free = gdma_free,
};

static int asoc_gdma_platform_probe(struct platform_device *pdev)
{
	struct dev_data * data = NULL;

	data = devm_kzalloc(&pdev->dev, sizeof(struct dev_data), GFP_KERNEL);
	if (data == NULL){
		dev_err(&pdev->dev,"%s dev_data alloc fail\n",__func__);
		return -ENOMEM;;
	}
	data->dma_debug = 0;
	dev_set_drvdata(&pdev->dev, data);

	if (sysfs_create_group(&pdev->dev.kobj,&dma_debug_attr_grp))
		dev_warn(&pdev->dev, "Error creating sysfs entry\n");

	return devm_snd_soc_register_component(&pdev->dev, &asoc_gdma_platform,
					       NULL, 0);
}

static int asoc_gdma_platform_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj,&dma_debug_attr_grp);
	return 0;
}

static const struct of_device_id ameba_gdma_of_match[] = {
	{ .compatible = "realtek,ameba-audiodma", },
	{},
};
MODULE_DEVICE_TABLE(of, ameba_gdma_of_match);

static struct platform_driver asoc_gdma_driver = {
	.driver = {
		.name = "ameba-gdma",
		.of_match_table = of_match_ptr(ameba_gdma_of_match),
	},

	.probe = asoc_gdma_platform_probe,
	.remove = asoc_gdma_platform_remove,
};

module_platform_driver(asoc_gdma_driver);

MODULE_DESCRIPTION("Realtek Ameba ALSA driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
