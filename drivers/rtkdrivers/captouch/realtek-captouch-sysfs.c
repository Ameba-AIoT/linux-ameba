// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Captouch sysfs support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "realtek-captouch.h"

/**
  * @brief  Enable or Disable the captouch peripheral.
  * @param  captouch: captouch driver data.
  * @param  state: state of the captouch.
  *   			This parameter can be: true or false.
  * @retval None
  */
void realtek_captouch_set_en(bool state)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_CTC_CTRL);
	if (state) {
		reg_value |= (CT_BIT_ENABLE | CT_BIT_BASELINE_INI);
	} else {
		reg_value &= ~CT_BIT_ENABLE;
	}

	writel(reg_value, captouch->base + RTK_CT_CTC_CTRL);
}
EXPORT_SYMBOL(realtek_captouch_set_en);

/**
  * @brief  Set CapTouch Scan interval.
  * @param  captouch: captouch driver data.
  * @param Interval: scan interval in units of ms
  * @retval None
  */
static void ctc_set_scan_interval(u32 Interval)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base + RTK_CT_SCAN_PERIOD);
	reg_value &= ~CT_MASK_SCAN_INTERVAL;
	reg_value |= Interval;

	writel(reg_value, captouch->base + RTK_CT_SCAN_PERIOD);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_scan_interval(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_SCAN_PERIOD);

	return CT_GET_SCAN_INTERVAL(reg_value);
}

/* set sample number = 2^(SampleCtrl + 2) */
/*  SampleAveCtrl = 0, sample number = 4;
*   SampleAveCtrl = 1, sample number = 8;
*   SampleAveCtrl = 2, sample number = 16;
*   SampleAveCtrl = 3, sample number = 32;
*   SampleAveCtrl = 4, sample number = 64;
*   SampleAveCtrl = 5, sample number = 128;
*   SampleAveCtrl = 6, sample number = 256;
*   SampleAveCtrl = 7, sample number = 512;
*/
static void ctc_set_sample_num(u8 SampleAveCtrl)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base + RTK_CT_SCAN_PERIOD);
	reg_value &= ~CT_MASK_SAMPLE_AVE_CTRL;
	reg_value |= CT_SAMPLE_AVE_CTRL(SampleAveCtrl);

	writel(reg_value, captouch->base + RTK_CT_SCAN_PERIOD);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_sample_num(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_SCAN_PERIOD);

	return CT_GET_SAMPLE_AVE_CTRL(reg_value);
}

static void ctc_set_debounce_en(u32 Sel)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base + RTK_CT_CTC_CTRL);
	reg_value |= CT_BIT_DEBOUNCE_EN;

	writel(reg_value, captouch->base + RTK_CT_CTC_CTRL);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_debounce_en(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_CTC_CTRL);

	return ((reg_value & CT_BIT_DEBOUNCE_EN) == 0) ? 0 : 1;
}

/* set debounce times */
/*  Sel = 0, debounce times = 2;
*   Sel = 1, debounce times = 3;
*   Sel = 2, debounce times = 4;
*   Sel = 3, debounce times = 5;
*/
static void ctc_set_debounce_times(u32 Sel)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base + RTK_CT_CTC_CTRL);
	reg_value &= ~CT_MASK_DEBOUNCE_TIME;
	reg_value |= CT_DEBOUNCE_TIME(Sel);

	writel(reg_value, captouch->base + RTK_CT_CTC_CTRL);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_debounce_times(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_CTC_CTRL);

	return CT_GET_DEBOUNCE_TIME(reg_value);
}

static u32 ctc_get_snr_info(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_SNR_INF);

	return reg_value;
}

/* set ETC interval */
/* Interval=(etc_scan_interval+1)*scan_period
* etc_scan_interval: 0-127
*/
static void ctc_set_etc_interval(u32 Interval)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);
	reg_value &= ~CT_MASK_ETC_SCAN_INTERVAL;
	reg_value |= CT_ETC_SCAN_INTERVAL(Interval);

	writel(reg_value, captouch->base + RTK_CT_ETC_CTRL);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_etc_interval(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);

	return CT_GET_ETC_SCAN_INTERVAL(reg_value);
}

/* set ETC baseline weight factor */
/* ETC_factor_th=2^baseline_wt_factor
* baseline_wt_factor: 0-15
*/
static void ctc_set_etc_factor(u32 Factor)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);
	reg_value &= ~CT_MASK_BASELINE_WT_FACTOR;
	reg_value |= CT_BASELINE_WT_FACTOR(Factor);

	writel(reg_value, captouch->base + RTK_CT_ETC_CTRL);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_etc_factor(void)
{
	u32 reg_value;
	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);
	return CT_GET_BASELINE_WT_FACTOR(reg_value);
}

/* set ETC baseline update step */
/*baseline_upd_step: 0~15*/
static void ctc_set_etc_step(u32 Step)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);
	reg_value &= ~CT_MASK_BASELINE_UPD_STEP;
	reg_value |= CT_BASELINE_UPD_STEP(Step);

	writel(reg_value, captouch->base + RTK_CT_ETC_CTRL);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_etc_step(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);

	return CT_GET_BASELINE_UPD_STEP(reg_value);
}

/* ETC enable or disable */
static void  ctc_set_etc_en(bool state)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);

	if (state) {
		/* Enable ETC */
		reg_value |= CT_BIT_ETC_FUNC_CTRL;
	} else {
		/* Disable ETC */
		reg_value &= ~CT_BIT_ETC_FUNC_CTRL;
	}

	writel(reg_value, captouch->base + RTK_CT_ETC_CTRL);
}

static u32 ctc_get_etc_en(void)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_ETC_CTRL);

	return ((reg_value & CT_BIT_ETC_FUNC_CTRL) == 0) ? 0 : 1;
}

/**
  * @brief  Enable or disable specified channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @param  NewState: new state of the specified channel.
  *   			This parameter can be: true or false.
  * @retval None
  */
static void  ctc_set_chx_en(u8 Channel, bool state)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_CTRL + Channel * 0x10);

	if (state) {
		/* Enable the CapTouch crossponding channel */
		reg_value |= CT_BIT_CHx_EN ;
	} else {
		/* Disable the CapTouch channel */
		reg_value &= ~CT_BIT_CHx_EN;
	}

	writel(reg_value, captouch->base + RTK_CT_CHx_CTRL + Channel * 0x10);
}

static u32 ctc_get_chx_en(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_CTRL + Channel * 0x10);

	return ((reg_value & CT_BIT_CHx_EN) == 0) ? 0 : 1;
}

/**
  * @brief  Set Mbias current for specified channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel.
  * @param  Mbias: Mbias value, relate current = 0.25*Mbias.
  * @retval None
  */
static void ctc_set_mbias(u8 Channel, u8 Mbias)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base +  RTK_CT_CHx_MBIAS_ATH + Channel * 0x10);
	reg_value &= ~CT_MASK_CHx_MBIAS;
	reg_value |= CT_CHx_MBIAS(Mbias);

	writel(reg_value, captouch->base + RTK_CT_CHx_MBIAS_ATH + Channel * 0x10);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_mbias(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_MBIAS_ATH + Channel * 0x10);

	return CT_GET_CHx_MBIAS(reg_value);
}

/**
  * @brief  Set relative touch threshold for related channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @param  Threshold: Related Threshold value.
  * @retval None
  */
static void ctc_set_diff_thres(u8 Channel, u32 Threshold)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base +  RTK_CT_CHx_CTRL + Channel * 0x10);
	reg_value &= ~CT_MASK_CHx_D_TOUCH_TH;
	reg_value |= CT_CHx_D_TOUCH_TH(Threshold);

	writel(reg_value, captouch->base + RTK_CT_CHx_CTRL + Channel * 0x10);

	realtek_captouch_set_en(true);
}

/**
  * @brief  Get relative threshold of touch judgement for specified channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @retval Difference threshold of specified channel.
  */
static u32 ctc_get_diff_thres(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_CTRL + Channel * 0x10);

	return CT_GET_CHx_D_TOUCH_TH(reg_value);
}

/* set absolute threshold include guard channel-> guard is channel 8 */
/* note: when ETC is disabled, manual set this value, otherwise it is updated by HW */
static void ctc_set_abs_thres(u8 Channel, u16 Threshold)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_MBIAS_ATH + Channel * 0x10);
	reg_value &= ~CT_MASK_CHx_A_TOUCH_TH;
	reg_value |= CT_CHx_A_TOUCH_TH(Threshold);

	writel(reg_value, captouch->base + RTK_CT_CHx_CTRL + Channel * 0x10);
}

/**
  * @brief  Get Absolute  threshold of touch judgement for specified channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @retval Difference threshold of specified channel.
  */
static u32 ctc_get_abs_thres(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_MBIAS_ATH + Channel * 0x10);

	return CT_GET_CHx_A_TOUCH_TH(reg_value);
}

/**
  * @brief  Set N-noise threshold for related channel.
  * @param  captouch: captouch driver data.
  * @param  ct_init: CapTouch initialization parameters.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @param  Threshold: Related N-noise Threshold value.
  * @retval None
  */
static void ctc_set_n_noise_thres(u8 Channel, u16 Threshold)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base +  RTK_CT_CHx_NOISE_TH + Channel * 0x10);
	reg_value &= ~CT_MASK_CHx_N_ENT;
	reg_value |= CT_CHx_N_ENT(Threshold);

	writel(reg_value, captouch->base + RTK_CT_CHx_NOISE_TH + Channel * 0x10);

	realtek_captouch_set_en(true);

}

/**
  * @brief  Get positive or negative noise threshold for specified channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @param  type: can be P_NOISE_THRES or N_NOISE_THRES
  * @retval  Noise threshold of specified channel.
  */
static u32 ctc_get_n_noise_thres(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_NOISE_TH + Channel * 0x10);

	return  CT_GET_CHx_N_ENT(reg_value);
}

/**
  * @brief  Set P-noise threshold for related channel.
  * @param  captouch: captouch driver data.
  * @param  ct_init: CapTouch initialization parameters.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @param  Threshold: Related P-noise Threshold value.
  * @retval None
  */
static void ctc_set_p_noise_thres(u8 Channel, u16 Threshold)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base +  RTK_CT_CHx_NOISE_TH + Channel * 0x10);
	reg_value &= ~CT_MASK_CHx_P_ENT;
	reg_value |= CT_CHx_P_ENT(Threshold);

	writel(reg_value, captouch->base + RTK_CT_CHx_NOISE_TH + Channel * 0x10);

	realtek_captouch_set_en(true);
}

static u32 ctc_get_p_noise_thres(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_NOISE_TH + Channel * 0x10);

	return CT_GET_CHx_P_ENT(reg_value);
}

static void ctc_set_baseline(u8 Channel, u16 baseline)
{
	u32 reg_value;

	realtek_captouch_set_en(false);

	reg_value = readl(captouch->base +  RTK_CT_CHx_CTRL + Channel * 0x10);
	reg_value &= ~CT_MASK_CHx_BASELINE;
	reg_value |= CT_CHx_BASELINE(baseline);

	writel(reg_value, captouch->base + RTK_CT_CHx_CTRL + Channel * 0x10);

	realtek_captouch_set_en(true);
}

/**
  * @brief  Read Baseline data from specified channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @retval Baseline data
  */
static u32 ctc_get_baseline(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_CTRL + Channel * 0x10);

	return CT_GET_CHx_BASELINE(reg_value);
}

/**
  * @brief  Read average data from specified channel.
  * @param  captouch: captouch driver data.
  * @param  Channel: specified channel index, which can be 0~8
  * @note     Channel 8 is guard sensor channel
  * @retval Average data
  */
static u32 ctc_get_avedata(u8 Channel)
{
	u32 reg_value;

	reg_value = readl(captouch->base +  RTK_CT_CHx_DATA_INF + Channel * 0x10);

	return CT_GET_CHx_DATA_AVE(reg_value);
}

static int ch_num = 0;
static ssize_t ch_num_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	if (val > 8) {
		return -1;
	}

	ch_num = val;

	return size;
}
DEVICE_ATTR_WO(ch_num);

static ssize_t chx_enable_show(struct device *dev,
							   struct device_attribute *attr,
							   char *buf)
{
	return sprintf(buf, "%d\n", ctc_get_chx_en(ch_num));
}

static ssize_t chx_enable_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);//"0x10"->16
	if (ret) {
		return ret;
	}

	if (val == 0) {
		ctc_set_chx_en(ch_num, false);
	} else {
		ctc_set_chx_en(ch_num, true);
	}

	return size;
}

DEVICE_ATTR_RW(chx_enable);
static ssize_t chx_mbias_show(struct device *dev,
							  struct device_attribute *attr,
							  char *buf)
{
	return sprintf(buf, "%d\n", ctc_get_mbias(ch_num));
}

static ssize_t chx_mbias_store(struct device *dev,
							   struct device_attribute *attr,
							   const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_mbias(ch_num, val);

	return size;
}
DEVICE_ATTR_RW(chx_mbias);

static ssize_t chx_diff_th_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_diff_thres(ch_num));
}

static ssize_t chx_diff_th_store(struct device *dev,
								 struct device_attribute *attr,
								 const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_diff_thres(ch_num, val);

	return size;
}
DEVICE_ATTR_RW(chx_diff_th);

static ssize_t chx_abs_th_show(struct device *dev,
							   struct device_attribute *attr,
							   char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_abs_thres(ch_num));
}

static ssize_t chx_abs_th_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_abs_thres(ch_num, val);

	return size;
}
DEVICE_ATTR_RW(chx_abs_th);

static ssize_t chx_n_noise_th_show(struct device *dev,
								   struct device_attribute *attr,
								   char *buf)
{
	return sprintf(buf, "%d\n", ctc_get_n_noise_thres(ch_num));
}

static ssize_t chx_n_noise_th_store(struct device *dev,
									struct device_attribute *attr,
									const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_n_noise_thres(ch_num, val);

	return size;
}
DEVICE_ATTR_RW(chx_n_noise_th);

static ssize_t chx_p_noise_th_show(struct device *dev,
								   struct device_attribute *attr,
								   char *buf)
{
	return sprintf(buf, "%d\n", ctc_get_p_noise_thres(ch_num));
}

static ssize_t chx_p_noise_th_store(struct device *dev,
									struct device_attribute *attr,
									const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_p_noise_thres(ch_num, val);

	return size;
}

DEVICE_ATTR_RW(chx_p_noise_th);

static ssize_t chx_baseline_show(struct device *dev,
								 struct device_attribute *attr,
								 char *buf)
{
	return sprintf(buf, "%d\n", ctc_get_baseline(ch_num));
}

static ssize_t chx_baseline_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_baseline(ch_num, val);

	return size;
}

DEVICE_ATTR_RW(chx_baseline);

static ssize_t chx_ave_data_show(struct device *dev,
								 struct device_attribute *attr,
								 char *buf)
{
	return sprintf(buf, "%d\n", ctc_get_avedata(ch_num));
}
DEVICE_ATTR_RO(chx_ave_data);

static struct attribute *chx_attrs[] = {
	&dev_attr_ch_num.attr,
	&dev_attr_chx_enable.attr,
	&dev_attr_chx_mbias.attr,
	&dev_attr_chx_diff_th.attr,
	&dev_attr_chx_abs_th.attr,
	&dev_attr_chx_n_noise_th.attr,
	&dev_attr_chx_p_noise_th.attr,
	&dev_attr_chx_baseline.attr,
	&dev_attr_chx_ave_data.attr,
	NULL,
};

static const struct attribute_group chx_group = {
	.name = "chx_config",
	.attrs = chx_attrs,
};

static ssize_t etc_enable_show(struct device *dev,
							   struct device_attribute *attr,
							   char *buf)
{
	return sprintf(buf, "%d\n", ctc_get_etc_en());
}

static ssize_t etc_enable_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}

	if (val != 0) {
		ctc_set_etc_en(true);
	} else {
		ctc_set_etc_en(false);
	}

	return size;
}
DEVICE_ATTR_RW(etc_enable);

static ssize_t etc_interval_show(struct device *dev,
								 struct device_attribute *attr,
								 char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_etc_interval());
}

static ssize_t etc_interval_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_etc_interval(val);

	return size;
}
DEVICE_ATTR_RW(etc_interval);

static ssize_t etc_factor_show(struct device *dev,
							   struct device_attribute *attr,
							   char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_etc_factor());
}

static ssize_t etc_factor_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_etc_factor(val);

	return size;
}
DEVICE_ATTR_RW(etc_factor);

static ssize_t etc_step_show(struct device *dev,
							 struct device_attribute *attr,
							 char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_etc_step());
}

static ssize_t etc_step_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_etc_step(val);

	return size;
}
DEVICE_ATTR_RW(etc_step);

static struct attribute *etc_attrs[] = {
	&dev_attr_etc_enable.attr,
	&dev_attr_etc_interval.attr,
	&dev_attr_etc_factor.attr,
	&dev_attr_etc_step.attr,
	NULL,
};
static const struct attribute_group etc_group = {
	.name = "etc_config",
	.attrs = etc_attrs,
};

static ssize_t ctc_enable_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}

	if (val != 0) {
		realtek_captouch_set_en(true);
	} else {
		realtek_captouch_set_en(false);
	}

	return size;
}
DEVICE_ATTR_WO(ctc_enable);

static ssize_t scan_interval_show(struct device *dev,
								  struct device_attribute *attr,
								  char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_scan_interval());
}

static ssize_t scan_interval_store(struct device *dev,
								   struct device_attribute *attr,
								   const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_scan_interval(val);

	return size;
}
DEVICE_ATTR_RW(scan_interval);

static ssize_t sample_num_show(struct device *dev,
							   struct device_attribute *attr,
							   char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_sample_num());
}

static ssize_t sample_num_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_sample_num(val);

	return size;
}
DEVICE_ATTR_RW(sample_num);

static ssize_t debounce_en_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_debounce_en());
}

static ssize_t debounce_en_store(struct device *dev,
								 struct device_attribute *attr,
								 const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_debounce_en(val);

	return size;
}
DEVICE_ATTR_RW(debounce_en);

static ssize_t debounce_times_show(struct device *dev,
								   struct device_attribute *attr,
								   char *buf)
{
	return sprintf(buf, "%u\n", ctc_get_debounce_times());
}

static ssize_t debounce_times_store(struct device *dev,
									struct device_attribute *attr,
									const char *buf, size_t size)
{
	int ret;
	u32 val;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}
	ctc_set_debounce_times(val);

	return size;
}
DEVICE_ATTR_RW(debounce_times);

static ssize_t snr_touch_show(struct device *dev,
							  struct device_attribute *attr,
							  char *buf)
{
	return sprintf(buf, "%u\n", CT_GET_SNR_TOUCH_DATA(ctc_get_snr_info()));
}
DEVICE_ATTR_RO(snr_touch);

static ssize_t snr_noise_show(struct device *dev,
							  struct device_attribute *attr,
							  char *buf)
{
	return sprintf(buf, "%u\n", CT_GET_SNR_NOISE_DATA(ctc_get_snr_info()));
}
DEVICE_ATTR_RO(snr_noise);

static struct attribute *common_attrs[] = {
	&dev_attr_ctc_enable.attr,
	&dev_attr_scan_interval.attr,
	&dev_attr_sample_num.attr,
	&dev_attr_debounce_en.attr,
	&dev_attr_debounce_times.attr,
	&dev_attr_snr_touch.attr,
	&dev_attr_snr_noise.attr,
	NULL,
};

static const struct attribute_group common_group = {
	.name = "common_config",
	.attrs = common_attrs,
};

const struct attribute_group *captouch_configs[] = {
	&common_group,
	&chx_group,
	&etc_group,
	NULL,
};
