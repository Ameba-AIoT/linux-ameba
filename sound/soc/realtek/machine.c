// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

enum {
	DAI_LINK_PLAYBACK,
	DAI_LINK_CAPTURE,
};

SND_SOC_DAILINK_DEFS(aif1,
	DAILINK_COMP_ARRAY(COMP_CPU("4100d000.sport")),
	DAILINK_COMP_ARRAY(COMP_CODEC("4100b000.codec", "ameba-aif1")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audiodma@0")));

SND_SOC_DAILINK_DEFS(aif2,
	DAILINK_COMP_ARRAY(COMP_CPU("4100e000.sport")),
	DAILINK_COMP_ARRAY(COMP_CODEC("4100b000.codec", "ameba-aif2")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audiodma@1")));

SND_SOC_DAILINK_DEFS(aif3,
	DAILINK_COMP_ARRAY(COMP_CPU("4100f000.sport")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audiodma@2")));

SND_SOC_DAILINK_DEFS(aif4,
	DAILINK_COMP_ARRAY(COMP_CPU("41010000.sport")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audiodma@3")));

static int ameba_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	return 0;
}

static struct snd_soc_ops ameba_ops = {
	.hw_params = ameba_hw_params,
};

static struct snd_soc_dai_link ameba_dai[] = {
	{
		.name = "codec AIF1",
		.stream_name = "Pri_Dai",
		.ops = &ameba_ops,
		.dai_fmt = SND_SOC_DAI_FORMAT_LEFT_J,
		SND_SOC_DAILINK_REG(aif1),
	},
	{
		.name = "codec AIF2",
		.stream_name = "Sec_Dai",
		.ops = &ameba_ops,
		.dai_fmt = SND_SOC_DAI_FORMAT_LEFT_J,
		SND_SOC_DAILINK_REG(aif2),
	},
	{
		.name = "codec AIF3",
		.stream_name = "SPORT2_I2s_Dai",
		.ops = &ameba_ops,
		.dai_fmt = SND_SOC_DAI_FORMAT_LEFT_J,
		SND_SOC_DAILINK_REG(aif3),
	},
	{
		.name = "codec AIF4",
		.stream_name = "SPORT3_I2S_Dai",
		.ops = &ameba_ops,
		.dai_fmt = SND_SOC_DAI_FORMAT_LEFT_J,
		SND_SOC_DAILINK_REG(aif4),
	},
};

static struct snd_soc_card ameba_snd = {
	.name = "Ameba-snd",
	.owner = THIS_MODULE,
	.dai_link = ameba_dai,
	.num_links = ARRAY_SIZE(ameba_dai),
};

static int ameba_audio_probe(struct platform_device *pdev)
{
	int ret;

	//struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &ameba_snd;
	card->dev = &pdev->dev;

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	return 0;
}

static const struct of_device_id ameba_audio_of_match[] = {
	{ .compatible = "realtek,ameba-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, ameba_audio_of_match);


static struct platform_driver ameba_audio_driver = {
	.driver		= {
		.name	= "ameba-audio",
		.of_match_table = of_match_ptr(ameba_audio_of_match),
		.pm	= &snd_soc_pm_ops,
	},
	.probe		= ameba_audio_probe,
};

module_platform_driver(ameba_audio_driver);

MODULE_DESCRIPTION("Realtek Ameba ALSA driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
