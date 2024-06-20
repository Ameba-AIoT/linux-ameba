// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gcd.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/of_gpio.h>

#include "ameba_audio.h"

#define SPEAKER         0
#define HEADPHONE       1
#define MAX_TDM_NUM     8
#define MAX_AMIC_NUM    5
#define MIC_TYPE_AMIC   0
#define MIC_TYPE_DMIC   1
#define MAX_DAI_NUM     2

struct ameba_priv {
	/* IOREMAP'd SFRs */
	void __iomem *digital_addr;
	/* Physical base address of SFRs */
	u32	digital_base;
	u32 digital_size;

	/* IOREMAP'd SFRs */
	void __iomem *analog_addr;
	/* Physical base address of SFRs */
	u32	analog_base;
	u32 analog_size;

	int codec_debug;

	struct clk* clock;

	int output_device;

	int mic_type;
	int amic_tdm_num;
	u8 tdm_amic_numbers[MAX_TDM_NUM];
	u8 amic_gains[MAX_AMIC_NUM];
	int dmic_tdm_num;
	u8 tdm_dmic_numbers[MAX_TDM_NUM];
	unsigned int dai_fmt[MAX_DAI_NUM];

	int gpio_index;
	bool enable_dac_asrc;
};

#define codec_info(mask,dev, ...)						\
do{									\
	if (((struct ameba_priv *)(dev_get_drvdata(dev))) != NULL)					\
		if (((((struct ameba_priv *)(dev_get_drvdata(dev)))->codec_debug) & mask) == 1)		\
			dev_info(dev, ##__VA_ARGS__); \
}while(0)

static ssize_t codec_debug_show(struct device *dev, struct device_attribute*attr, char *buf)
{
		struct ameba_priv *codec_priv = dev_get_drvdata(dev);
        return sprintf(buf, "codec_debug=%d\n", codec_priv->codec_debug);
}

static ssize_t codec_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct ameba_priv *codec_priv = dev_get_drvdata(dev);
	codec_priv->codec_debug = buf[0] - '0';
    return count;
}

static DEVICE_ATTR(codec_debug, S_IWUSR |S_IRUGO, codec_debug_show, codec_debug_store);

static struct attribute *codec_debug_attrs[] = {
        &dev_attr_codec_debug.attr,
        NULL
};

static const struct attribute_group codec_debug_attr_grp = {
        .attrs = codec_debug_attrs,
};

void dumpRegs(struct device *dev, void __iomem * audio_base_addr,void __iomem * aud_analog){
	u32 tmp;
	/***analog reg dump***/
	struct ameba_priv *codec_priv = dev_get_drvdata(dev);
	if (codec_priv -> codec_debug == 1){
		tmp = readl(aud_analog + ADDA_CTL);
		codec_info(1, dev, "ADDA_CTL:%x",tmp);
		tmp = readl(aud_analog + HPO_CTL);
		codec_info(1, dev, "HPO_CTL:%x",tmp);
		tmp = readl(aud_analog + MICBIAS_CTL0);
		codec_info(1, dev, "MICBIAS_CTL0:%x",tmp);
		tmp = readl(aud_analog + MICBIAS_CTL1);
		codec_info(1, dev, "MICBIAS_CTL1:%x",tmp);
		tmp = readl(aud_analog + MICBST_CTL0);
		codec_info(1, dev, "MICBST_CTL0:%x",tmp);
		tmp = readl(aud_analog + MICBST_CTL1);
		codec_info(1, dev, "MICBST_CTL1:%x",tmp);
		tmp = readl(aud_analog + ANALOG_RSVD0);
		codec_info(1, dev, "ANALOG_RSVD0:%x",tmp);
		tmp = readl(aud_analog + DTS_CTL);
		codec_info(1, dev, "DTS_CTL:%x",tmp);
		tmp = readl(aud_analog + MBIAS_CTL0);
		codec_info(1, dev, "MBIAS_CTL0:%x",tmp);
		tmp = readl(aud_analog + MBIAS_CTL1);
		codec_info(1, dev, "MBIAS_CTL1:%x",tmp);
		tmp = readl(aud_analog + MBIAS_CTL2);
		codec_info(1, dev, "MBIAS_CTL2:%x",tmp);
		tmp = readl(aud_analog + HPO_BIAS_CTL);
		codec_info(1, dev, "HPO_BIAS_CTL:%x",tmp);

		/***digital reg dump***/
		tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_1);
		codec_info(1, dev, "CODEC_CLOCK_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_2);
		codec_info(1, dev, "CODEC_CLOCK_CONTROL_2:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_4);
		codec_info(1, dev, "CODEC_CLOCK_CONTROL_4:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_5);
		codec_info(1, dev, "CODEC_CLOCK_CONTROL_5:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
		codec_info(1, dev, "CODEC_I2S_AD_SEL_CONTROL:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_I2S_0_CONTROL);
		codec_info(1, dev, "CODEC_I2S_0_CONTROL:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_I2S_1_CONTROL);
		codec_info(1, dev, "CODEC_I2S_1_CONTROL:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_I2S_0_CONTROL_1);
		codec_info(1, dev, "CODEC_I2S_0_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_I2S_1_CONTROL_1);
		codec_info(1, dev, "CODEC_I2S_1_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_AUDIO_CONTROL_0);
		codec_info(1, dev, "CODEC_AUDIO_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_AUDIO_CONTROL_1);
		codec_info(1, dev, "CODEC_AUDIO_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_0_CONTROL_0);
		codec_info(1, dev, "CODEC_ADC_0_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_1_CONTROL_0);
		codec_info(1, dev, "CODEC_ADC_1_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_2_CONTROL_0);
		codec_info(1, dev, "CODEC_ADC_2_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_3_CONTROL_0);
		codec_info(1, dev, "CODEC_ADC_3_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_4_CONTROL_0);
		codec_info(1, dev, "CODEC_ADC_4_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_5_CONTROL_0);
		codec_info(1, dev, "CODEC_ADC_5_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_6_CONTROL_0);
		codec_info(1, dev, "CODEC_ADC_6_CONTROL_0:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_0_CONTROL_1);
		codec_info(1, dev, "CODEC_ADC_0_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_1_CONTROL_1);
		codec_info(1, dev, "CODEC_ADC_1_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_2_CONTROL_1);
		codec_info(1, dev, "CODEC_ADC_2_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_3_CONTROL_1);
		codec_info(1, dev, "CODEC_ADC_3_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_4_CONTROL_1);
		codec_info(1, dev, "CODEC_ADC_4_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_5_CONTROL_1);
		codec_info(1, dev, "CODEC_ADC_5_CONTROL_1:%x",tmp);
		tmp = readl(audio_base_addr + CODEC_ADC_6_CONTROL_1);
		codec_info(1, dev, "CODEC_ADC_6_CONTROL_1:%x",tmp);
	}
}

static int alsa_amic_gain_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	memcpy(ucontrol->value.bytes.data, codec_priv->amic_gains, MAX_AMIC_NUM);

	return 0;
}

static int alsa_amic_gain_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);
	int amic_index = 0;

	memcpy(codec_priv->amic_gains, ucontrol->value.bytes.data, MAX_AMIC_NUM);

	for (; amic_index < MAX_AMIC_NUM; amic_index++)
		audio_codec_set_adc_gain(amic_index + 1, codec_priv->amic_gains[amic_index], codec_priv->analog_addr);

	return 0;
}

static int alsa_output_device_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_priv->output_device;

	return 0;
}

/*
 * to be noticed: before put, ap should disable external amp,first.
 * because during output_device change, there might be pop noise.
 * after put, ap can enable external amp.
*/
static int alsa_output_device_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];
	codec_priv->output_device = val;
	if (val == SPEAKER){
		audio_codec_set_hpo_diff(codec_priv->analog_addr);
		/*wait 10ms for hpo diff to be stable, otherwise,there might be pop noise*/
		msleep(10);
	}else{
		audio_codec_set_hpo_single_end(codec_priv->analog_addr);
		/*wait 10ms for hpo diff to be stable, otherwise,there might be pop noise*/
		msleep(10);
	}

	return 0;
}

static int alsa_mic_type_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_priv->mic_type;

	return 0;
}

static int alsa_mic_type_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];
	codec_priv->mic_type = val;

	return 0;
}

static int alsa_amic_number_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	memcpy(ucontrol->value.bytes.data, codec_priv->tdm_amic_numbers, MAX_TDM_NUM);

	return 0;
}

static int alsa_amic_number_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	memcpy(codec_priv->tdm_amic_numbers, ucontrol->value.bytes.data, MAX_TDM_NUM);

	return 0;
}

static int alsa_dmic_number_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	memcpy(ucontrol->value.bytes.data, codec_priv->tdm_dmic_numbers, MAX_TDM_NUM);

	return 0;
}

static int alsa_dmic_number_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	memcpy(codec_priv->tdm_dmic_numbers, ucontrol->value.bytes.data, MAX_TDM_NUM);

	return 0;
}


static const DECLARE_TLV_DB_SCALE(digital_tlv, -6562, 37, 0);  //min db: -65.625db, max db:0db,step:0.375db/step
static const struct snd_kcontrol_new common_snd_controls[] = {
	SOC_DOUBLE_R_TLV("DAC Volume", CODEC_DAC_L_CONTROL_0,
			 CODEC_DAC_R_CONTROL_0, 0, 0xaf, 0, digital_tlv),
	SOC_DOUBLE_R("DAC Mute", CODEC_DAC_L_CONTROL_1,
			 CODEC_DAC_R_CONTROL_1, 11, 0x1, 0),
	SOC_SINGLE("AD0 Volume", CODEC_ADC_0_CONTROL_1, 0, 0xaf, 0),
	SOC_SINGLE("AD1 Volume", CODEC_ADC_1_CONTROL_1, 0, 0xaf, 0),
	SOC_SINGLE("AD2 Volume", CODEC_ADC_2_CONTROL_1, 0, 0xaf, 0),
	SOC_SINGLE("AD3 Volume", CODEC_ADC_3_CONTROL_1, 0, 0xaf, 0),
	SOC_SINGLE("AD4 Volume", CODEC_ADC_4_CONTROL_1, 0, 0xaf, 0),
	SOC_SINGLE("AD5 Volume", CODEC_ADC_5_CONTROL_1, 0, 0xaf, 0),
	SOC_SINGLE("AD6 Volume", CODEC_ADC_6_CONTROL_1, 0, 0xaf, 0),
	SOC_SINGLE("AD7 Volume", CODEC_ADC_7_CONTROL_1, 0, 0xaf, 0),
	SND_SOC_BYTES_EXT("MICBST AMIC1-5 GAIN", MAX_AMIC_NUM,
				alsa_amic_gain_get,
				alsa_amic_gain_put),
	SOC_DOUBLE_R("Zero Dect TimeOut", CODEC_DAC_L_CONTROL_1,
			 CODEC_DAC_R_CONTROL_1, 2, 0x3, 0),
	SOC_SINGLE_EXT("Choice Device 0:Spk 1:HEAPHONE", 0, 0, 1, 0,
				alsa_output_device_get,
				alsa_output_device_put),
	SOC_SINGLE_EXT("Choice MIC 0:AMIC 1:DMIC", 0, 0, 1, 0,
				alsa_mic_type_get,
				alsa_mic_type_put),
	SND_SOC_BYTES_EXT("Choice AMIC tdm-8 slot", MAX_TDM_NUM,
				alsa_amic_number_get,
				alsa_amic_number_put),
	SND_SOC_BYTES_EXT("Choice DMIC tdm-8 slot", MAX_TDM_NUM,
				alsa_dmic_number_get,
				alsa_dmic_number_put),
};

static int ameba_codec_dai_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int ameba_codec_dai_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	codec_priv->dai_fmt[dai->id] = fmt;

	return 0;
}

static unsigned int get_df_for_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	unsigned int sp_df = DF_LEFT;
	struct snd_soc_component *component = dai->component;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
	case SND_SOC_DAI_FORMAT_I2S:
		sp_df = DF_I2S;
		break;
	case SND_SOC_DAI_FORMAT_LEFT_J:
		sp_df = DF_LEFT;
		break;
	case SND_SOC_DAI_FORMAT_DSP_A:
		sp_df = DF_PCM_A;
		break;
	case SND_SOC_DAI_FORMAT_DSP_B:
		sp_df = DF_PCM_B;
		break;
	default:
		dev_err(component->dev,"unsupported fmt:%d", fmt);
		break;
	}

	return sp_df;
}

static int ameba_codec_dai_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	i2s_init_params i2s_init =
    {
		.codec_sel_i2s_tx_word_len = WL_16,
		.codec_sel_i2s_tx_ch_len = CL_32,
		.codec_sel_i2s_tx_ch = CH_LR,
		.codec_sel_i2s_tx_data_format = DF_LEFT,
		.codec_sel_i2s_rx_word_len = WL_16,
		.codec_sel_i2s_rx_ch_len = CL_32,
		.codec_sel_i2s_rx_data_format = DF_LEFT,
		.codec_sel_rx_i2s_tdm = I2S_NOTDM,
	};

	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);
	int sample_rate = params_rate(params);
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	codec_info(1,component->dev,"%s,channels:%d,sample_rate:%dhz,digital_addr:%p",__func__,params_channels(params),sample_rate,codec_priv->digital_addr);

	if (is_playback){
		switch (params_width(params)) {     //params_width - get the number of bits of the sample format,like 16LS
			case 8:
				i2s_init.codec_sel_i2s_tx_word_len = WL_8;
				i2s_init.codec_sel_i2s_tx_ch_len = CL_16;
				break;
			case 16:
				i2s_init.codec_sel_i2s_tx_word_len = WL_16;
				i2s_init.codec_sel_i2s_tx_ch_len = CL_32;
				break;
			case 24:
				i2s_init.codec_sel_i2s_tx_word_len = WL_24;
				i2s_init.codec_sel_i2s_tx_ch_len = CL_32;
				break;
			//spec shows that ameba codec does not support 32bit
			default:
				dev_err(component->dev, "Format(%d) not supported\n",
						params_format(params));
				return -EINVAL;
		}
		i2s_init.codec_sel_i2s_tx_ch = CH_LR;
		i2s_init.codec_sel_i2s_tx_data_format = get_df_for_fmt(dai, codec_priv->dai_fmt[dai->id]);
		audio_codec_i2s_enable(&i2s_init,codec_priv->digital_addr,codec_priv->analog_addr, I2S0);
		audio_codec_i2s_init(&i2s_init,codec_priv->digital_addr,codec_priv->analog_addr, I2S0);

	}else{
		switch (params_width(params)) {     //params_width - get the number of bits of the sample format,like 16LS
			case 8:
				i2s_init.codec_sel_i2s_rx_word_len = WL_8;
				i2s_init.codec_sel_i2s_rx_ch_len = CL_16;
				break;
			case 16:
				i2s_init.codec_sel_i2s_rx_word_len = WL_16;
				i2s_init.codec_sel_i2s_rx_ch_len = CL_32;
				break;
			case 24:
				i2s_init.codec_sel_i2s_rx_word_len = WL_24;
				i2s_init.codec_sel_i2s_rx_ch_len = CL_32;
				break;
			default:
				dev_err(component->dev, "Format(%d) not supported\n",
						params_format(params));
				return -EINVAL;
		}

		i2s_init.codec_sel_i2s_rx_data_format = get_df_for_fmt(dai, codec_priv->dai_fmt[dai->id]);

		switch (params_channels(params)) {     //params_channels - Get the number of channels from the hw params
			case 8:
				i2s_init.codec_sel_rx_i2s_tdm = I2S_TDM8;
				break;
			case 6:
				i2s_init.codec_sel_rx_i2s_tdm = I2S_TDM6;
				break;
			case 4:
				i2s_init.codec_sel_rx_i2s_tdm = I2S_TDM4;
				break;
			case 2:
				i2s_init.codec_sel_rx_i2s_tdm = I2S_NOTDM;
				break;
			case 1:
				i2s_init.codec_sel_rx_i2s_tdm = I2S_NOTDM;
				break;
			default:
				dev_err(component->dev, "%d channels not supported\n",params_channels(params));
			return -EINVAL;
		}

		audio_codec_i2s_enable(&i2s_init,codec_priv->digital_addr,codec_priv->analog_addr, I2S1);
		audio_codec_i2s_init(&i2s_init,codec_priv->digital_addr,codec_priv->analog_addr, I2S1);

	}

	return 0;
}

static int ameba_codec_dai_aif_mute(struct snd_soc_dai *codec_dai, int mute)
{
	return 0;
}

static int ameba_codec_dai_set_fll(struct snd_soc_dai *dai, int id, int src,
			  unsigned int freq_in, unsigned int freq_out)
{
	return 0;
}

static int ameba_codec_dai_set_tristate(struct snd_soc_dai *codec_dai, int tristate)
{
	return 0;
}

static int ameba_codec_dai_startup(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{

	struct snd_soc_component *component = dai->component;
	codec_info(1,component->dev,"%s",__func__);

	return 0;
}

static snd_pcm_sframes_t ameba_codec_delay(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	//ameba codec's delay is 36 samples for all sample rates.
	return 36;
}

static int ameba_codec_dai_hw_free(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	codec_init_params codec_init;
	struct snd_soc_component *component = dai->component;
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	codec_info(1,component->dev,"%s",__func__);

	if (is_playback){
		codec_init.codec_application = APP_LINE_OUT;
		audio_codec_i2s_deinit(codec_priv->digital_addr,I2S0);
	}else{
		codec_init.codec_application = APP_AMIC_IN;
		audio_codec_i2s_deinit(codec_priv->digital_addr,I2S1);
	}

	return 0;
}


static const struct snd_soc_dai_ops ameba_aif_dai_ops = {
	.set_sysclk		= ameba_codec_dai_set_dai_sysclk,
	.set_fmt		= ameba_codec_dai_set_dai_fmt,
	.hw_params		= ameba_codec_dai_hw_params,
	.startup		= ameba_codec_dai_startup,
	.delay			= ameba_codec_delay,
	.digital_mute	= ameba_codec_dai_aif_mute,
	.set_pll		= ameba_codec_dai_set_fll,
	.set_tristate	= ameba_codec_dai_set_tristate,
	.hw_free		= ameba_codec_dai_hw_free,
};



static int ameba_codec_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =snd_soc_rtdcom_lookup(rtd, "ameba-codec");
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);
	codec_init_params codec_init =
	{
		.codec_sel_dac_i2s = I2S0,
		.codec_sel_dac_lin = I2S_L,
		.codec_sel_dac_rin = I2S_R,
		.codec_sel_adc_i2s = I2S1,
		.codec_application = APP_LINE_OUT,
		.codec_dac_sr = SR_16K,
		.codec_adc_sr = SR_16K,
		.i2s_mode = I2S_NOTDM,
	};
	int adc_channel;

	int sample_rate = params_rate(params);
	codec_info(1,component->dev,"%s,channels:%d,sample_rate:%dhz,digital_addr:%p",__func__,params_channels(params),sample_rate,codec_priv->digital_addr);

	if (is_playback){
		switch(sample_rate){
			case 192000:
				codec_init.codec_dac_sr = SR_192K;
				break;
			case 176400:
				codec_init.codec_dac_sr = SR_176P4K;
				break;
			case 96000:
				codec_init.codec_dac_sr = SR_96K;
				break;
			case 88200:
				codec_init.codec_dac_sr = SR_88P2K;
				break;
			case 48000:
				codec_init.codec_dac_sr = SR_48K;
				break;
			case 44100:
				codec_init.codec_dac_sr = SR_44P1K;
				break;
			case 32000:
				codec_init.codec_dac_sr = SR_32K;
				break;
			case 24000:
				codec_init.codec_dac_sr = SR_24K;
				break;
			case 22050:
				codec_init.codec_dac_sr = SR_22P05K;
				break;
			case 16000:
				codec_init.codec_dac_sr = SR_16K;
				break;
			case 12000:
				codec_init.codec_dac_sr = SR_12K;
				break;
			case 11025:
				codec_init.codec_dac_sr = SR_11P025K;
				break;
			case 8000:
				codec_init.codec_dac_sr = SR_8K;
				break;
			break;
		}

		codec_init.i2s_mode = I2S_NOTDM;
		codec_init.codec_sel_dac_i2s = I2S0;
		codec_init.codec_sel_dac_lin = I2S_L;
		codec_init.codec_sel_dac_rin = I2S_R;
		codec_init.codec_application = APP_LINE_OUT; //earphone and speaker can both use it.

		audio_codec_params_init(&codec_init,codec_priv->digital_addr,codec_priv->analog_addr);
		audio_codec_set_dac_asrc_rate(sample_rate, codec_priv->digital_addr);
		audio_codec_set_dac_asrc(codec_priv->enable_dac_asrc, codec_priv->digital_addr);

	}else{
		switch(sample_rate){
			case 96000:
				codec_init.codec_adc_sr = SR_96K;
				break;
			case 88200:
				codec_init.codec_adc_sr = SR_88P2K;
				break;
			case 48000:
				codec_init.codec_adc_sr = SR_48K;
				break;
			case 44100:
				codec_init.codec_adc_sr = SR_44P1K;
				break;
			case 32000:
				codec_init.codec_adc_sr = SR_32K;
				break;
			case 24000:
				codec_init.codec_adc_sr = SR_24K;
				break;
			case 22050:
				codec_init.codec_adc_sr = SR_22P05K;
				break;
			case 16000:
				codec_init.codec_adc_sr = SR_16K;
				break;
			case 12000:
				codec_init.codec_adc_sr = SR_12K;
				break;
			case 11025:
				codec_init.codec_adc_sr = SR_11P025K;
				break;
			case 8000:
				codec_init.codec_adc_sr = SR_8K;
				break;
			break;
		}
		switch (params_channels(params)) {     //params_channels - Get the number of channels from the hw params
			case 8:
				codec_init.i2s_mode = I2S_TDM8;
				if (codec_priv->mic_type == MIC_TYPE_AMIC)
					codec_priv->amic_tdm_num = 8;
				else
					codec_priv->dmic_tdm_num = 8;
				break;
			case 6:
				codec_init.i2s_mode = I2S_TDM6;
				if (codec_priv->mic_type == MIC_TYPE_AMIC)
					codec_priv->amic_tdm_num = 6;
				else
					codec_priv->dmic_tdm_num = 6;
				break;
			case 4:
				codec_init.i2s_mode = I2S_TDM4;
				if (codec_priv->mic_type == MIC_TYPE_AMIC)
					codec_priv->amic_tdm_num = 4;
				else
					codec_priv->dmic_tdm_num = 4;
				break;
			case 2:
				codec_init.i2s_mode = I2S_NOTDM;
				if (codec_priv->mic_type == MIC_TYPE_AMIC)
					codec_priv->amic_tdm_num = 2;
				else
					codec_priv->dmic_tdm_num = 2;
				break;
			case 1:
				codec_init.i2s_mode = I2S_NOTDM;
				if (codec_priv->mic_type == MIC_TYPE_AMIC)
					codec_priv->amic_tdm_num = 2;
				else
					codec_priv->dmic_tdm_num = 2;
				break;
			default:
				dev_err(component->dev, "%d channels not supported\n",params_channels(params));
			return -EINVAL;
		}

		codec_init.codec_sel_adc_i2s = I2S1;

		if (codec_priv->mic_type == MIC_TYPE_AMIC) {
			codec_init.codec_application = APP_AMIC_IN;
			audio_codec_amic_init(&codec_init, codec_priv->digital_addr, codec_priv->analog_addr);
		} else {
			codec_init.codec_application = APP_DMIC_IN;
			audio_codec_dmic_init(&codec_init, codec_priv->digital_addr, codec_priv->analog_addr);
			if (codec_init.codec_adc_sr == SR_96K) {
				audio_codec_sel_dmic_clk(DMIC_5M, codec_priv->digital_addr);
			} else {
				audio_codec_sel_dmic_clk(DMIC_2P5M, codec_priv->digital_addr);
			}
		}

		for (adc_channel = 0; adc_channel< codec_priv->amic_tdm_num; adc_channel++) {
			if (codec_priv->mic_type == MIC_TYPE_AMIC) {
			codec_info(1,component->dev,"codec_priv->tdm_amic_numbers[%d] = %d",adc_channel, codec_priv->tdm_amic_numbers[adc_channel]);
			/*unmute ad channel(AD_CHANNEL_0...) and select AMIC SDM number for the channel(ADC1...)*/
			audio_codec_mute_adc_input(false, adc_channel, codec_priv->tdm_amic_numbers[adc_channel], codec_priv->digital_addr);

			audio_codec_set_adc_mode(codec_priv->tdm_amic_numbers[adc_channel], NORMALPOWER, codec_priv->analog_addr);

			audio_codec_enable_adc_dchpf(true, adc_channel, codec_priv->digital_addr);

			audio_codec_mute_amic_in(false, codec_priv->tdm_amic_numbers[adc_channel], codec_priv->analog_addr);

			audio_codec_set_adc_mute(adc_channel, false, codec_priv->digital_addr);

			audio_codec_set_adc_gain(codec_priv->tdm_amic_numbers[adc_channel], codec_priv->amic_gains[(codec_priv->tdm_amic_numbers[adc_channel])-1], codec_priv->analog_addr);
			} else {
				codec_info(1,component->dev,"codec_priv->tdm_dmic_numbers[%d] = %d",adc_channel, codec_priv->tdm_dmic_numbers[adc_channel]);
				/*unmute ad channel(AD_CHANNEL_0...) and select DMIC number for the channel(ADC1...)*/
				audio_codec_mute_dmic_input(false, adc_channel, codec_priv->tdm_dmic_numbers[adc_channel], codec_priv->digital_addr);
				/*enable ad channel & it's fifo*/
				audio_codec_sel_dmic_channel(adc_channel, codec_priv->digital_addr);

				audio_codec_enable_dmic_dchpf(true, adc_channel, codec_priv->digital_addr);

				audio_codec_set_adc_mute(adc_channel,false,codec_priv->digital_addr);

			}
		}

		for (adc_channel = 0; adc_channel< codec_priv->amic_tdm_num; adc_channel++) {
			/*enable ad channel & it's fifo*/
			audio_codec_sel_adc_channel(adc_channel, codec_priv->digital_addr);
		}

	}

	dumpRegs(component->dev,codec_priv->digital_addr,codec_priv->analog_addr);
	return 0;

}

static int ameba_codec_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =snd_soc_rtdcom_lookup(rtd, "ameba-codec");

	codec_info(1,component->dev,"%s",__func__);

	return 0;
}

static const struct snd_pcm_ops codec_ops = {
	.hw_params	= ameba_codec_hw_params,
	.hw_free = ameba_codec_hw_free,
};

static struct snd_soc_dai_driver ameba_dai[] = {
	{
		.name = "ameba-aif1",
		.id = 0,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.rate_min = 8000,
			.rate_max = 192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
			.sig_bits = 24,
		},
		.ops = &ameba_aif_dai_ops,
	},
	{
		.name = "ameba-aif2",
		.id = 1,
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.rate_min = 8000,
			.rate_max = 192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
			.sig_bits = 24,
		},
		.ops = &ameba_aif_dai_ops,
	}
};


static int ameba_codec_component_probe(struct snd_soc_component *component)
{
	struct ameba_priv *codec_priv = snd_soc_component_get_drvdata(component);

	codec_info(1,component->dev,"%s,digital_addr:%x",__func__,(u32)codec_priv->digital_addr);

	audio_codec_enable(codec_priv->digital_addr,codec_priv->analog_addr);

	audio_codec_init(codec_priv->digital_addr,codec_priv->analog_addr);

	audio_codec_set_hpo_diff(codec_priv->analog_addr);

	audio_codec_set_dac_volume(0, MAX_VOLUME, codec_priv->digital_addr);
	audio_codec_set_dac_volume(1, MAX_VOLUME, codec_priv->digital_addr);

	audio_codec_zdet_init(DA_ZDET_TOUT_LEVEL1, codec_priv->digital_addr);

	audio_codec_amic_power(codec_priv->digital_addr,codec_priv->analog_addr);

	snd_soc_add_component_controls(component, common_snd_controls,
				       ARRAY_SIZE(common_snd_controls));

	msleep(10);

	if (codec_priv->gpio_index >= 0) {
		gpio_request(codec_priv->gpio_index, NULL);
		gpio_direction_output(codec_priv->gpio_index, 1);
		gpio_set_value(codec_priv->gpio_index, 1);
		gpio_free(codec_priv->gpio_index);
	}

	return 0;
}

static void ameba_codec_component_remove(struct snd_soc_component *component)
{
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);
	audio_codec_deinit(codec_priv->digital_addr,codec_priv->analog_addr);
	audio_codec_amic_deinit(codec_priv->digital_addr,codec_priv->analog_addr);
}

static int ameba_codec_component_suspend(struct snd_soc_component *component)
{
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	codec_info(1, component->dev,"%s",__func__);

	audio_codec_deinit(codec_priv->digital_addr,codec_priv->analog_addr);
	audio_codec_amic_deinit(codec_priv->digital_addr,codec_priv->analog_addr);
	audio_codec_disable(codec_priv->analog_addr);

	//clk_disable_unprepare(codec_priv->clock);

	return 0;
}

static int ameba_codec_component_resume(struct snd_soc_component *component)
{
	struct ameba_priv *codec_priv = snd_soc_component_get_drvdata(component);

	codec_info(1, component->dev,"%s,digital_addr:%x",__func__,(u32)codec_priv->digital_addr);

	//clk_prepare_enable(codec_priv->clock);

	audio_codec_enable(codec_priv->digital_addr,codec_priv->analog_addr);

	audio_codec_init(codec_priv->digital_addr,codec_priv->analog_addr);

	audio_codec_set_hpo_diff(codec_priv->analog_addr);

	audio_codec_set_dac_volume(0, MAX_VOLUME, codec_priv->digital_addr);
	audio_codec_set_dac_volume(1, MAX_VOLUME, codec_priv->digital_addr);

	audio_codec_zdet_init(DA_ZDET_TOUT_LEVEL1, codec_priv->digital_addr);

	audio_codec_amic_power(codec_priv->digital_addr,codec_priv->analog_addr);

	return 0;
}

static int ameba_write_reg(struct snd_soc_component *component,
		     unsigned int reg, unsigned int val)
{
	u32 tmp;
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);

	codec_info(1,component->dev,"%s,digital_addr:%x,reg:%x,value:%x",__func__,(u32)codec_priv->digital_addr,reg,val);

	tmp = readl(codec_priv->digital_addr + reg);
	tmp |= val;
	writel(val, codec_priv->digital_addr + reg);

	return 0;
}

static unsigned int ameba_read_reg(struct snd_soc_component *component,
			     unsigned int reg)
{
	struct ameba_priv * codec_priv = snd_soc_component_get_drvdata(component);
	int val = readl(codec_priv->digital_addr + reg);

	codec_info(1,component->dev,"%s,digital_addr:%x,reg:%x,value:%x",__func__,(u32)codec_priv->digital_addr,reg,val);

	return val;
}


static const struct snd_soc_component_driver soc_component_drv_codec = {
	.name = "ameba-codec",
	.probe			= ameba_codec_component_probe,
	.remove			= ameba_codec_component_remove,
	.suspend		= ameba_codec_component_suspend,
	.resume			= ameba_codec_component_resume,
	.write          = ameba_write_reg,
	.read           = ameba_read_reg,
	.ops 		    = &codec_ops,
};

/* for clk set, get clk first, then set parent relationship ,then enable from parent to child*/
int set_codec_clk(struct platform_device *pdev, struct ameba_priv *codec_priv){
	int ret = 0;

	codec_info(1,&pdev->dev,"%s",__func__);

	/* get ac clock */
	codec_priv->clock = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(codec_priv->clock)) {
		dev_err(&pdev->dev, "Fail to get clock %d\n",__LINE__);
		ret = -1;
		goto err;
	}

	/* enable ac clock */
	ret = clk_prepare_enable(codec_priv->clock);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to enable clock %d\n", ret);
		goto err;
	}

err:
	return ret;
}

static int ameba_codec_probe(struct platform_device *pdev)
{
	struct ameba_priv *codec_priv;
	struct resource *resource_reg_digital;
	struct resource *resource_reg_analog;
	struct resource *resource_reg_sys_fen_grp1;
	struct resource *resource_reg_swr_on_ctrl0;
	void __iomem *sys_fen_grp1_addr;
	void __iomem *swr_on_ctrl0_addr;
	enum of_gpio_flags flags;
	int index;
	u32 tmp;

	codec_priv = devm_kzalloc(&pdev->dev, sizeof(struct ameba_priv),
			      GFP_KERNEL);
	if (codec_priv == NULL)
		return -ENOMEM;

	resource_reg_digital = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	codec_priv->digital_base = resource_reg_digital->start;
	codec_priv->digital_size = resource_reg_digital->end - resource_reg_digital->start + 1;
	codec_priv->digital_addr = devm_ioremap_resource(&pdev->dev, resource_reg_digital);
	if (codec_priv->digital_addr == NULL) {
		dev_err(&pdev->dev, "cannot ioremap digital_base registers\n");
		return -ENXIO;
	}

	resource_reg_analog = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	codec_priv->analog_base = resource_reg_analog->start;
	codec_priv->analog_size = resource_reg_analog->end - resource_reg_analog->start + 1;
	codec_priv->analog_addr = devm_ioremap_resource(&pdev->dev, resource_reg_analog);
	if (codec_priv->analog_addr == NULL) {
		dev_err(&pdev->dev, "cannot ioremap analog_base registers\n");
		return -ENXIO;
	}

	resource_reg_sys_fen_grp1 = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	sys_fen_grp1_addr = devm_ioremap_resource(&pdev->dev, resource_reg_sys_fen_grp1);
	if (sys_fen_grp1_addr == NULL) {
		dev_err(&pdev->dev, "cannot ioremap sys_fen_grp1\n");
		return -ENXIO;
	}

	resource_reg_swr_on_ctrl0 = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	swr_on_ctrl0_addr = devm_ioremap_resource(&pdev->dev, resource_reg_swr_on_ctrl0);
	if (swr_on_ctrl0_addr == NULL) {
		dev_err(&pdev->dev, "cannot ioremap swr_on_ctrl0\n");
		return -ENXIO;
	}

	/*setting apb vad*/
	tmp = readl(sys_fen_grp1_addr);
	tmp |= APBPeriph_VAD;
	writel(tmp, sys_fen_grp1_addr);

	/*setting power for avcc*/
	tmp = readl(swr_on_ctrl0_addr);
	tmp &= ~SWR_MASK_VOL_L1;
	writel(tmp, swr_on_ctrl0_addr);
	tmp |= SWR_VOL_L1(9);
	writel(tmp, swr_on_ctrl0_addr);

	codec_priv->gpio_index = 0;
	index = of_get_named_gpio_flags(pdev->dev.of_node, "ext_amp_gpio", 0, &flags);

	codec_priv->gpio_index = index;
	codec_priv->enable_dac_asrc = of_property_read_bool(pdev->dev.of_node, "enable-dac-asrc");

	codec_priv->codec_debug = 0;
	if (sysfs_create_group(&pdev->dev.kobj,&codec_debug_attr_grp))
		dev_warn(&pdev->dev, "Error creating sysfs entry\n");

	codec_priv->mic_type = MIC_TYPE_AMIC;

	codec_priv->amic_tdm_num = 4;
	codec_priv->tdm_amic_numbers[0] = (u8)AMIC1;
	codec_priv->tdm_amic_numbers[1] = (u8)AMIC2;
	codec_priv->tdm_amic_numbers[2] = (u8)AMIC3;
	codec_priv->tdm_amic_numbers[3] = (u8)AMIC3;
	codec_priv->tdm_amic_numbers[4] = (u8)AMIC3;
	codec_priv->tdm_amic_numbers[5] = (u8)AMIC3;
	codec_priv->tdm_amic_numbers[6] = (u8)AMIC3;
	codec_priv->tdm_amic_numbers[7] = (u8)AMIC3;

	codec_priv->dmic_tdm_num = 4;
	codec_priv->tdm_dmic_numbers[0] = (u8)DMIC1;
	codec_priv->tdm_dmic_numbers[1] = (u8)DMIC2;
	codec_priv->tdm_dmic_numbers[2] = (u8)DMIC3;
	codec_priv->tdm_dmic_numbers[3] = (u8)DMIC4;
	codec_priv->tdm_dmic_numbers[4] = (u8)DMIC4;
	codec_priv->tdm_dmic_numbers[5] = (u8)DMIC4;
	codec_priv->tdm_dmic_numbers[6] = (u8)DMIC4;
	codec_priv->tdm_dmic_numbers[7] = (u8)DMIC4;

	codec_priv->amic_gains[0] = MICBST_GAIN_30DB;
	codec_priv->amic_gains[1] = MICBST_GAIN_30DB;
	codec_priv->amic_gains[2] = MICBST_GAIN_30DB;
	codec_priv->amic_gains[3] = MICBST_GAIN_30DB;
	codec_priv->amic_gains[4] = MICBST_GAIN_30DB;

	codec_priv->dai_fmt[0] = DF_LEFT;
	codec_priv->dai_fmt[1] = DF_LEFT;

	codec_info(1,&pdev->dev,"%s,digital_base:%x,digital_size:%x,analog_base:%x,analog_size:%x\n",__func__,codec_priv->digital_base,codec_priv->digital_size, codec_priv->analog_base, codec_priv->analog_size);	
	codec_info(1,&pdev->dev,"tdm_amic_0:%d, amic_1:%d, amic_2:%d, amic_3:%d", codec_priv->tdm_amic_numbers[0], codec_priv->tdm_amic_numbers[1], codec_priv->tdm_amic_numbers[2], codec_priv->tdm_amic_numbers[3]);

	if (set_codec_clk(pdev, codec_priv) < 0){
		dev_err(&pdev->dev, "can't set codec clk");
		return -ENXIO;
	}

	platform_set_drvdata(pdev, codec_priv);
	dev_set_drvdata(&pdev->dev, codec_priv);

	return devm_snd_soc_register_component(&pdev->dev, &soc_component_drv_codec,
			ameba_dai, ARRAY_SIZE(ameba_dai));
	return 0;
}
static int ameba_codec_remove(struct platform_device *pdev)
{
	struct ameba_priv *codec_priv;
    codec_info(1,&pdev->dev,"%s\n",__func__);

	codec_priv = platform_get_drvdata(pdev);

	clk_disable_unprepare(codec_priv->clock);

	sysfs_remove_group(&pdev->dev.kobj,&codec_debug_attr_grp);
	return 0;
}

static const struct of_device_id ameba_codec_of_match[] = {
	{ .compatible = "realtek,ameba-codec", },
	{},
};
MODULE_DEVICE_TABLE(of, ameba_codec_of_match);


static struct platform_driver ameba_codec_driver = {
	.driver = {
		.name = "ameba-codec",
		.of_match_table = of_match_ptr(ameba_codec_of_match),
	},
	.probe = ameba_codec_probe,
	.remove = ameba_codec_remove,
};

module_platform_driver(ameba_codec_driver);

MODULE_DESCRIPTION("Realtek Ameba ALSA driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
