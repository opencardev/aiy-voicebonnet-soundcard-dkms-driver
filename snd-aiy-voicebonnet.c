/*
 * ASoC Driver for Google's AIY Voice Bonnet
 *
 * Author: Alex Van Damme <atv@google.com>
 *         Copyright 2017
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "rt5645.h"


#define PLATFORM_CLOCK 24576000

static struct snd_soc_jack headset_jack;

static struct snd_soc_jack_pin headset_jack_pin = {
  .pin = "Headphone",
  .mask = 0xFFFFF,
  .invert = 0
};

static int snd_rpi_aiy_voicebonnet_init(struct snd_soc_pcm_runtime *rtd) {
  int ret;
  struct snd_soc_dai *codec_dai = rtd->codec_dai;
  struct snd_soc_card *card = rtd->card;
  rt5645_sel_asrc_clk_src(rtd->codec,
                          RT5645_DA_STEREO_FILTER |
                          RT5645_AD_STEREO_FILTER |
                          RT5645_DA_MONO_L_FILTER |
                          RT5645_DA_MONO_R_FILTER,
                          RT5645_CLK_SEL_I2S1_ASRC);

  ret = snd_soc_dai_set_sysclk(codec_dai, RT5645_SCLK_S_MCLK, PLATFORM_CLOCK,
                               SND_SOC_CLOCK_IN);

  ret = snd_soc_card_jack_new(card, "Headphone Jack",
                              SND_JACK_HEADPHONE,
                              &headset_jack,
                              &headset_jack_pin, 1);
  if (ret) {
    dev_err(card->dev, "Setting up headphone jack failed! %d\n", ret);
    return ret;
  }

  return rt5645_set_jack_detect(rtd->codec, &headset_jack, NULL, NULL);
}

static int snd_rpi_aiy_voicebonnet_hw_params(
    struct snd_pcm_substream *substream,
    struct snd_pcm_hw_params *params) {
  int ret = 0;
  struct snd_soc_pcm_runtime *rtd = substream->private_data;
  struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
  struct snd_soc_dai *codec_dai = rtd->codec_dai;

  dev_dbg(rtd->dev, "cpu: %s codec: %s\n", cpu_dai->name, codec_dai->name);
  dev_dbg(rtd->dev, " rate: %d width: %d fmt: %d\n", params_rate(params),
         params_width(params), params_format(params));
  dev_dbg(rtd->dev, " cpu_dai: %p codec_dai; %p\n", cpu_dai, codec_dai);
  dev_dbg(rtd->dev, " rate: %d\n", params_rate(params));

  /* set codec PLL source to the 24.576MHz (MCLK) platform clock*/
  ret = snd_soc_dai_set_pll(codec_dai, 0, RT5645_PLL1_S_MCLK,
                            PLATFORM_CLOCK, params_rate(params) * 512);
  if (ret < 0) {
    dev_err(rtd->dev, "can't set codec pll: %d\n", ret);
    return ret;
  }

  ret = snd_soc_dai_set_sysclk(codec_dai, RT5645_SCLK_S_PLL1,
                               params_rate(params) * 512, SND_SOC_CLOCK_IN);
  if (ret < 0) {
    dev_err(rtd->dev, "can't set codec sysclk: %d\n", ret);
    return ret;
  }

  ret = snd_soc_dai_set_sysclk(codec_dai, RT5645_SCLK_S_PLL1,
                               params_rate(params) * 512, SND_SOC_CLOCK_OUT);
  if (ret < 0) {
    dev_err(rtd->dev, "can't set codec sysclk: %d\n", ret);
    return ret;
  }

  return ret;
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_aiy_voicebonnet_ops = {
    .hw_params = snd_rpi_aiy_voicebonnet_hw_params,
};

static const struct snd_soc_pcm_stream snd_rpi_googlevoicehat_params = {
    .stream_name = "aiy-voicebonnet",
    .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
               SNDRV_PCM_FMTBIT_S32_LE,
    .channels_min = 2,
    .channels_max = 2,
    .rate_min = 8000,
    .rate_max = 96000,
    .rates = SNDRV_PCM_RATE_8000_96000,
};

static struct snd_soc_dai_link snd_rpi_aiy_voicebonnet_dai[] = {
    {
        .name = "rt5645",
        .stream_name = "Google AIY Voice Bonnet SoundCard HiFi",
        .codec_dai_name = "rt5645-aif1",
        .dai_fmt =
            SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
        .ops = &snd_rpi_aiy_voicebonnet_ops,
        .init = snd_rpi_aiy_voicebonnet_init
    },
};

static const struct snd_soc_dapm_widget voicebonnet_widgets[] = {
    SND_SOC_DAPM_HP("Headphone", NULL),
    SND_SOC_DAPM_SPK("Speaker", NULL),
    SND_SOC_DAPM_MIC("Int Mic", NULL),
};

static const struct snd_soc_dapm_route voicebonnet_audio_map[] = {
    {"micbias2", NULL, "Int Mic"},
    {"micbias2", NULL, "Int Mic"},
    {"IN1P", NULL, "micbias2"},
    {"IN2P", NULL, "micbias2"},
    {"Headphone", NULL, "HPOR"},
    {"Headphone", NULL, "HPOL"},
    {"Speaker", NULL, "SPOL"},
    {"Speaker", NULL, "SPOR"},
};

static const struct snd_kcontrol_new voicebonnet_controls[] = {
    SOC_DAPM_PIN_SWITCH("Headphone"),
    SOC_DAPM_PIN_SWITCH("Speaker"),
    SOC_DAPM_PIN_SWITCH("Int Mic"),
};

/* audio machine driver */
static struct snd_soc_card snd_rpi_aiy_voicebonnet = {
    .name = "snd_rpi_aiy_voicebonnet",
    .owner = THIS_MODULE,
    .dai_link = snd_rpi_aiy_voicebonnet_dai,
    .num_links = ARRAY_SIZE(snd_rpi_aiy_voicebonnet_dai),
    .dapm_routes = voicebonnet_audio_map,
    .num_dapm_routes = ARRAY_SIZE(voicebonnet_audio_map),
    .dapm_widgets = voicebonnet_widgets,
    .num_dapm_widgets = ARRAY_SIZE(voicebonnet_widgets),
    .controls = voicebonnet_controls,
    .num_controls = ARRAY_SIZE(voicebonnet_controls),
    .fully_routed = true,
};

static int snd_rpi_aiy_voicebonnet_probe(
    struct platform_device *pdev) {
  int ret = 0;

  snd_rpi_aiy_voicebonnet.dev = &pdev->dev;

  if (pdev->dev.of_node) {
    struct device_node *i2s_node;
    struct snd_soc_dai_link *dai = &snd_rpi_aiy_voicebonnet_dai[0];

    dai->codec_name = NULL;
    dai->codec_of_node =
        of_parse_phandle(pdev->dev.of_node, "aiy-voicebonnet,audio-codec", 0);
    if (!dai->codec_of_node) {
      dev_err(&pdev->dev, "Couldn't parse aiy-voicebonnet,audio-codec\n");
      return -EINVAL;
    }

    i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);
    if (i2s_node) {
      dai->cpu_dai_name = NULL;
      dai->cpu_of_node = i2s_node;
      dai->platform_name = NULL;
      dai->platform_of_node = i2s_node;
    }
  }

  ret = snd_soc_of_parse_card_name(&snd_rpi_aiy_voicebonnet,
                                   "google,model");
  if (ret) dev_err(&pdev->dev, "snd_soc_parse pailed: %d\n", ret);

  ret = devm_snd_soc_register_card(&pdev->dev, &snd_rpi_aiy_voicebonnet);
  if (ret) dev_err(&pdev->dev, "devm_snd_soc_register_card() failed: %d\n", ret);

  return ret;
}

static const struct of_device_id snd_rpi_aiy_voicebonnet_of_match[] = {
    {
        .compatible = "google,aiy-voicebonnet",
    },
    {},
};
MODULE_DEVICE_TABLE(of, snd_rpi_aiy_voicebonnet_of_match);

static struct platform_driver snd_rpi_aiy_voicebonnet_driver = {
    .driver =
        {
            .name = "snd-soc-aiy-voicebonnet",
            .owner = THIS_MODULE,
            .of_match_table = snd_rpi_aiy_voicebonnet_of_match,
        },
    .probe = snd_rpi_aiy_voicebonnet_probe,
};

module_platform_driver(snd_rpi_aiy_voicebonnet_driver);

MODULE_AUTHOR("Alex Van Damme <atv@google.com>");
MODULE_DESCRIPTION("ASoC Driver for Google AIY Voice Bonnet");
MODULE_LICENSE("GPL v2");
