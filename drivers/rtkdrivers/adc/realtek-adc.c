// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ADC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mfd/rtk-timer.h>
#include <linux/nvmem-consumer.h>

#include "realtek-adc.h"

/* adc configuration data */
static struct realtek_adc_info realtek_adc_cfg = {
	.clk_div = 3,
	.rx_level = 0,
	.chid_en = 0,
	.special_ch = 0xFF,
	.timer_idx = 4,
	.period = 100000000,	//unit: ns
};

/**
  * realtek_adc_cmd - Enable or Disable the ADC peripheral.
  * @param  adc: ADC driver data.
  * @param  state: state of the ADC.
  *   			This parameter can be: true or false.
  * @retval None
  */
void realtek_adc_cmd(struct realtek_adc_data *adc, bool state)
{
	u32 reg_value;

	reg_value = readl(adc->base + RTK_ADC_CONF);
	if (state) {
		reg_value |= ADC_BIT_ENABLE;
	} else {
		reg_value &= ~ADC_BIT_ENABLE;
		// clear fifo
		writel(1, adc->base + RTK_ADC_CLR_FIFO);
		writel(0, adc->base + RTK_ADC_CLR_FIFO);
	}
	writel(reg_value, adc->base + RTK_ADC_CONF);
}

/**
  * realtek_adc_clear_fifo - Clear ADC FIFO.
  * @param  adc: ADC driver data.
  * @retval None
  */
void realtek_adc_clear_fifo(struct realtek_adc_data *adc)
{
	writel(1, adc->base + RTK_ADC_CLR_FIFO);
	writel(0, adc->base + RTK_ADC_CLR_FIFO);
}

/**
  * realtek_adc_int_config - ENABLE/DISABLE  the ADC interrupt bits.
  * @param  adc: ADC driver data.
  * @param  ADC_IT: specifies the ADC interrupt to be setup.
  *          This parameter can be one or combinations of the following values:
  *            @arg ADC_BIT_IT_COMP_CH9_EN:	ADC channel 9 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH8_EN:	ADC channel 8 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH7_EN:	ADC channel 7 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH6_EN:	ADC channel 6 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH5_EN:	ADC channel 5 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH4_EN:	ADC channel 4 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH3_EN:	ADC channel 3 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH2_EN:	ADC channel 2 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH1_EN:	ADC channel 1 compare interrupt
  *            @arg ADC_BIT_IT_COMP_CH0_EN:	ADC channel 0 compare interrupt
  *            @arg ADC_BIT_IT_ERR_EN:			ADC error state interrupt
  *            @arg ADC_BIT_IT_DAT_OVW_EN:	ADC data overwritten interrupt
  *            @arg ADC_BIT_IT_FIFO_EMPTY_EN:	ADC FIFO empty interrupt
  *            @arg ADC_BIT_IT_FIFO_OVER_EN:	ADC FIFO overflow interrupt
  *            @arg ADC_BIT_IT_FIFO_FULL_EN:	ADC FIFO full interrupt
  *            @arg ADC_BIT_IT_CHCV_END_EN:	ADC particular channel conversion done interrupt
  *            @arg ADC_BIT_IT_CV_END_EN:		ADC conversion end interrupt
  *            @arg ADC_BIT_IT_CVLIST_END_EN:	ADC conversion list end interrupt
  * @param  state: true/false.
  * @retval  None
  */
static void realtek_adc_int_config(struct realtek_adc_data *adc, u32 ADC_IT, bool state)
{
	u32 reg_value;

	reg_value = readl(adc->base + RTK_ADC_INTR_CTRL);
	if (state) {
		reg_value |= ADC_IT;
	} else {
		reg_value &= ~ADC_IT;
	}
	writel(reg_value, adc->base + RTK_ADC_INTR_CTRL);
}

/**
  * realtek_adc_readable - Detemine ADC FIFO is empty or not.
  * @param  adc: ADC driver data.
  * @retval ADC FIFO is empty or not:
  *        - 0: Not Empty
  *        - 1: Empty
  */
u32 realtek_adc_readable(struct realtek_adc_data *adc)
{
	u32 Status = readl(adc->base + RTK_ADC_BUSY_STS);
	u32 Readable = (((Status & ADC_BIT_FIFO_EMPTY) == 0) ? 1 : 0);

	return Readable;
}

/**
  * realtek_adc_swtrig_cmd - Control the ADC module to do a conversion. Used as a start-convert event which is controlled by software.
  * @param  adc: ADC driver data.
  * @param  state: can be one of the following value:
  *			@arg true: Enable the analog module and analog mux. And then start a new channel conversion.
  *			@arg false:  Disable the analog module and analog mux.
  * @retval  None.
  * @note  1. Every time this bit is set to 1, ADC module would switch to a new channel and do one conversion.
  *			    Every time a conversion is done, software MUST clear this bit manually.
  *		  2. Used in Sotfware Trigger Mode
  */
static void realtek_adc_swtrig_cmd(struct realtek_adc_data *adc, bool state)
{
	u32 div;
	u8 sync_time[4] = {12, 16, 32, 64};

	div = readl(adc->base + RTK_ADC_CLK_DIV);
	if (state) {
		writel(ADC_BIT_SW_TRIG, adc->base + RTK_ADC_SW_TRIG);
	} else {
		writel(0, adc->base + RTK_ADC_SW_TRIG);
	}

	/* Wait 2 clock to sync signal */
	udelay(sync_time[div]);
}

/**
  * realtek_adc_auto_cmd - Controls the automatic channel switch enabled or disabled.
  * @param  adc: ADC driver data.
  * @param  state: can be one of the following value:
  *		@arg true: Enable the automatic channel switch.
  *			When setting this bit, an automatic channel switch starts from the first channel in the channel switch list.
  *		@arg false:  Disable the automatic channel switch.
  *			If an automatic channel switch is in progess, writing 0 will terminate the automatic channel switch.
  * @retval  None.
  * @note  Used in Automatic Mode
  */
static void realtek_adc_auto_cmd(struct realtek_adc_data *adc, bool state)
{
	u32 div;
	u8 sync_time[4] = {12, 16, 32, 64};

	div = readl(adc->base + RTK_ADC_CLK_DIV);
	if (state) {
		writel(ADC_BIT_AUTO_CSW_EN, adc->base + RTK_ADC_AUTO_CSW_CTRL);
	} else {
		writel(0, adc->base + RTK_ADC_AUTO_CSW_CTRL);
	}

	/* Wait 2 clock to sync signal */
	udelay(sync_time[div]);
}

/**
  * realtek_adc_timtrig_cmd - Initialize the trigger timer when in ADC Timer-Trigger Mode.
  * @param  adc: ADC driver data.
  * @param  Tim_Idx: The timer index would be used to make ADC module do a conversion.
  * @param  PeriodNs: Indicate the period of trigger timer.
  * @param  state: can be one of the following value:
  *			@arg true: Enable the ADC timer trigger mode.
  *			@arg false: Disable the ADC timer trigger mode.
  * @retval  None.
  * @note  Used in Timer-Trigger Mode
  */
static void realtek_adc_timtrig_cmd(struct realtek_adc_data *adc, u8 Tim_Idx, u64 PeriodNs, bool state)
{
	if (state) {
		rtk_gtimer_init(Tim_Idx, PeriodNs, NULL, NULL);
		writel(Tim_Idx, adc->base + RTK_ADC_EXT_TRIG_TIMER_SEL);
		rtk_gtimer_start(Tim_Idx, state);
	} else {
		rtk_gtimer_start(Tim_Idx, state);
		// free timer irq outside IRQ Context
	}
}

/**
 * realtek_adc_single_read() - Performs adc conversion
 * @indio_dev: IIO device
 * @chan: IIO channel
 * @res: conversion result
 */
static int realtek_adc_single_read(struct iio_dev *indio_dev,
								   const struct iio_chan_spec *chan,
								   int *res)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	long timeout;
	int ret = IIO_VAL_INT;
	unsigned long flags;
	u32 reg_value;

	spin_lock_irqsave(&adc->lock, flags);

	realtek_adc_cmd(adc, false);
	//set adc channel list len
	reg_value = readl(adc->base + RTK_ADC_CONF);
	reg_value &= ~(ADC_MASK_CVLIST_LEN);
	writel(reg_value, adc->base + RTK_ADC_CONF);
	// set channel switch list
	writel(chan->channel, adc->base + RTK_ADC_CHSW_LIST_0);
	realtek_adc_cmd(adc, true);

	realtek_adc_clear_fifo(adc);

	if (adc->mode == ADC_SW_TRI_MODE) {
		realtek_adc_swtrig_cmd(adc, true);
		while (realtek_adc_readable(adc) == 0);
		realtek_adc_swtrig_cmd(adc, false);
		*res = ADC_GET_DATA_GLOBAL(readl(adc->base + RTK_ADC_DATA_GLOBAL));
		ret = IIO_VAL_INT;
	} else if (adc->mode == ADC_AUTO_MODE) {
		realtek_adc_auto_cmd(adc, true);
		while (realtek_adc_readable(adc) == 0);
		realtek_adc_auto_cmd(adc, false);
		*res = ADC_GET_DATA_GLOBAL(readl(adc->base + RTK_ADC_DATA_GLOBAL));
		ret = IIO_VAL_INT;
	} else if (adc->mode == ADC_TIM_TRI_MODE) {
		reinit_completion(&adc->completion);
		realtek_adc_timtrig_cmd(adc, realtek_adc_cfg.timer_idx, realtek_adc_cfg.period, true);
		realtek_adc_int_config(adc, ADC_BIT_IT_CV_END_EN, true);
		timeout = wait_for_completion_interruptible_timeout(&adc->completion, msecs_to_jiffies(2000));
		if (timeout == 0) {
			ret = -ETIMEDOUT;
		} else if (timeout < 0) {
			ret = timeout;
		} else {
			*res = (int) adc->buffer[0];
			ret = IIO_VAL_INT;
		}
		realtek_adc_int_config(adc, ADC_BIT_IT_CV_END_EN, false);
		rtk_gtimer_deinit(realtek_adc_cfg.timer_idx);
	}

	spin_unlock_irqrestore(&adc->lock, flags);
	return ret;
}

/**
  * @brief  Read calibration params (A, B, C) from otp_addr
  * @indio_dev: IIO device
  * @param  otp_addr: Address in OTP
  * @retval 0 for success, errorcode otherwise
  */
static int realtek_adc_read_efuse_caldata(struct iio_dev *indio_dev, u16 otp_addr)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	u16 K_A, K_B, K_C;
	u8 *EfuseBuf = NULL;
	int ret = 0;
	int len = 0;
	struct nvmem_cell *cell;

	if (otp_addr == ADC_NORMAL_CH_CAL_OTPADDR) {
		cell = nvmem_cell_get(&indio_dev->dev, "normal_cal");
	} else {
		cell = nvmem_cell_get(&indio_dev->dev, "vbat_cal");
	}

	if (IS_ERR(cell)) {
		adc->k_coeff_normal[0] = adc->k_coeff_normal[1] = adc->k_coeff_normal[2] = 0;
		adc->k_coeff_vbat[0] = adc->k_coeff_vbat[1] = adc->k_coeff_vbat[2] = 0;
		dev_err(&indio_dev->dev, "OTP read fail\n");
		ret = (int)PTR_ERR(cell);
		return ret;
	}

	EfuseBuf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	K_A = EfuseBuf[1] << 8 | EfuseBuf[0];
	K_B = EfuseBuf[3] << 8 | EfuseBuf[2];
	K_C = EfuseBuf[5] << 8 | EfuseBuf[4];

	if (K_A == OTP_CALIB_COEFF_MAX) {
		K_A = (otp_addr == ADC_NORMAL_CH_CAL_OTPADDR) ? OTP_CALIB_NORMAL_ACOEFF_MAX : OTP_CALIB_VBAT_ACOEFF_MAX;
	}

	if (K_B == OTP_CALIB_COEFF_MAX) {
		K_B = (otp_addr == ADC_NORMAL_CH_CAL_OTPADDR) ? OTP_CALIB_NORMAL_BCOEFF_MAX : OTP_CALIB_VBAT_BCOEFF_MAX;
	}

	if (K_C == OTP_CALIB_COEFF_MAX) {
		K_C = (otp_addr == ADC_NORMAL_CH_CAL_OTPADDR) ? OTP_CALIB_NORMAL_CCOEFF_MAX : OTP_CALIB_VBAT_CCOEFF_MAX;
	}

	if (otp_addr == ADC_NORMAL_CH_CAL_OTPADDR) {
		/* Normal channels CH0~Ch5 */
		adc->k_coeff_normal[0] = (K_A & OTP_CALIB_COEFF_SIGN_BIT) ? -((OTP_CALIB_COEFF_MAX + 1) - K_A) : (K_A & (OTP_CALIB_COEFF_SIGN_BIT - 1));
		adc->k_coeff_normal[1] = (K_B & OTP_CALIB_COEFF_MAX);
		adc->k_coeff_normal[2] = (K_C & OTP_CALIB_COEFF_SIGN_BIT) ? -((OTP_CALIB_COEFF_MAX + 1) - K_C) : (K_C & (OTP_CALIB_COEFF_SIGN_BIT - 1));

		dev_dbg(&indio_dev->dev, "Calibration params (normal channels) read from OTP: A = %d B = %d C = %d\n",
				adc->k_coeff_normal[0], adc->k_coeff_normal[1], adc->k_coeff_normal[2]);
		/*
		 * Offset for differential channel which corresponds to 0V input.
		 * To be read from OTP once calibration OTP params are added for differential channel mode as well. (Currently, not provided by hardware.)
		 * Bcut: fixing it to 1650.
		 * Actual diff-input range: -1.65V<->0V<->+1.65V --> ADC REG count: 0V<->1.65V<->3.3V, shift back to userspace.
		 * Ccut: fixing it to 900.
		 * Actual diff-input range: -0.9V<->0V<->+0.9V --> ADC REG count: 0V<->0.9V<->1.8V, shift back to userspace.
		 */
		adc->diff_ch_offset = ADC_DIFF_CH_DEFAULT_OFFSET;
	} else {
		/* VBAT chanel 6 */
		adc->k_coeff_vbat[0] = (K_A & OTP_CALIB_COEFF_SIGN_BIT) ? -((OTP_CALIB_COEFF_MAX + 1) - K_A) : (K_A & (OTP_CALIB_COEFF_SIGN_BIT - 1));
		adc->k_coeff_vbat[1] = (K_B & OTP_CALIB_COEFF_MAX);
		adc->k_coeff_vbat[2] = (K_C & OTP_CALIB_COEFF_SIGN_BIT) ? -((OTP_CALIB_COEFF_MAX + 1) - K_C) : (K_C & (OTP_CALIB_COEFF_SIGN_BIT - 1));


		dev_dbg(&indio_dev->dev, "Calibration params (VBAT channels) read from OTP: A = %d B = %d C = %d\n",
				adc->k_coeff_vbat[0], adc->k_coeff_vbat[1], adc->k_coeff_vbat[2]);

	}

	return 0;
}

/**
  * @brief  Find & save cal offset for channel_num channel
  * @indio_dev: IIO device
  * @param  raw_adc_cnt: raw adc count read from ADC conversion
  * @param  channel_num: channel number
  * @param  is_differential: bool flag to indicate if it is a differential
  * mode channel
  * @retval calibration offset for channel_num
  */
static int realtek_adc_find_cal_offset(struct iio_dev *indio_dev, int raw_adc_cnt, int channel_num, bool is_differential)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	int cal_offset = 0;
	s64 y, tmp = 0;

	if (channel_num == ADC_VBAT_CHANNEL_NUM) {
		/* VBAT channel */
		if (adc->k_coeff_vbat[0] && adc->k_coeff_vbat[1] && adc->k_coeff_vbat[2]) {
			y = (s64)(((s64)adc->k_coeff_vbat[0] * (s64)raw_adc_cnt * (s64)raw_adc_cnt) / OTP_CALIB_ACOEFF_DIVISOR) +
				(s64)(((s64)adc->k_coeff_vbat[1] * (s64)raw_adc_cnt) / OTP_CALIB_BCOEFF_DIVISOR) +
				(s64)((s64)adc->k_coeff_vbat[2] / OTP_CALIB_CCOEFF_DIVISOR);

			dev_dbg(&indio_dev->dev, "Channel %d: X (raw ADC cnt) = %d, Y (calibrated ADC cnt) = %lld\n",
					channel_num, raw_adc_cnt, y);

			tmp = div_s64((s64)adc->k_coeff_vbat[1] * POWER_OF_TEN_MULTIPLIER, OTP_CALIB_BCOEFF_DIVISOR);
		}
	} else {
		/* Normal channels */
		if (adc->k_coeff_normal[0] && adc->k_coeff_normal[1] && adc->k_coeff_normal[2]) {
			y = (s64)(((s64)adc->k_coeff_normal[0] * (s64)raw_adc_cnt * (s64)raw_adc_cnt) / OTP_CALIB_ACOEFF_DIVISOR) +
				(s64)((s64)adc->k_coeff_normal[1] * (s64)raw_adc_cnt / OTP_CALIB_BCOEFF_DIVISOR) +
				(s64)((s64)adc->k_coeff_normal[2] / OTP_CALIB_CCOEFF_DIVISOR);
			y = is_differential ? (y - adc->diff_ch_offset) : y;

			dev_dbg(&indio_dev->dev, "Channel %d: X (raw ADC cnt) = %d, Y (calibrated ADC cnt) = %lld\n",
					channel_num, raw_adc_cnt, y);

			tmp = div_s64((s64)adc->k_coeff_normal[1] * POWER_OF_TEN_MULTIPLIER, OTP_CALIB_BCOEFF_DIVISOR);
		}
	}

	if (tmp) {
		tmp = div_s64(y * POWER_OF_TEN_MULTIPLIER, tmp);
		cal_offset = (int)tmp - raw_adc_cnt;
	}

	return cal_offset;
}

static int realtek_adc_read_raw(struct iio_dev *indio_dev,
								struct iio_chan_spec const *chan,
								int *val, int *val2, long mask)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	int ret;

	if (adc->is_calibdata_read == false) {
		adc->is_calibdata_read = true;
		if (realtek_adc_read_efuse_caldata(indio_dev, ADC_NORMAL_CH_CAL_OTPADDR)) {
			dev_err(&indio_dev->dev, "Failed to read calibration data from OTP for normal channels, no calibration supported\n");
			adc->is_calibdata_read = false;
		}

		if (realtek_adc_read_efuse_caldata(indio_dev, ADC_VBAT_CH_CAL_OTPADDR)) {
			dev_err(&indio_dev->dev, "Failed to read calibration data from OTP for VBAT channel, no calibration supported\n");
			adc->is_calibdata_read = false;
		}
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret) {
			return ret;
		}
		if (chan->type == IIO_VOLTAGE) {
			ret = realtek_adc_single_read(indio_dev, chan, val);
			/*
			 * Find out the calibration offset & store it for this channel.
			 * To be returned when IIO_CHAN_INFO_OFFSET is called for this channel.
			 */
			if (chan->differential) {
				adc->cal_offset[chan->channel] = realtek_adc_find_cal_offset(indio_dev, *val, chan->channel, true);
			} else {
				adc->cal_offset[chan->channel] = realtek_adc_find_cal_offset(indio_dev, *val, chan->channel, false);
			}
		} else {
			ret = -EINVAL;
		}
		iio_device_release_direct_mode(indio_dev);
		return ret;

	case IIO_CHAN_INFO_SCALE:
		/*
		 * As per the ADC UM, V33 normal channels can also be configured
		 * in single ended bypass mode in which case the input voltage(Vin)
		 * will have a range of 0~0.85V instead of 0~3.3V in the default
		 * single ended divided mode. But this mode selection is made
		 * programmable from captouch registers instead of ADC registers.
		 * This creates a non-std conflict/dependency in ADC & captouch
		 * drivers if user wants to configure bypass mode.
		 * For now, not exposing bypass mode configuration to ADC users
		 * to avoid this conflict.
		 */
		if (adc->is_calibdata_read == true) {
			*val = (chan->channel == ADC_VBAT_CHANNEL_NUM) ? adc->k_coeff_vbat[1] : adc->k_coeff_normal[1];
			*val2 = OTP_CALIB_BCOEFF_DIV_PWR;
		} else {
			*val = (chan->differential) ? ADC_NORMAL_CH_SCALE_FACTOR / 2 : ((chan->channel == ADC_VBAT_CHANNEL_NUM) ? ADC_VBAT_CH_SCALE_FACTOR :
					ADC_NORMAL_CH_SCALE_FACTOR);
			*val2 = chan->scan_type.realbits;
		}

		return IIO_VAL_FRACTIONAL_LOG2;

	case IIO_CHAN_INFO_OFFSET:
		/*
		 * Note: offset for any channel is only available if IIO_CHAN_INFO_OFFSET
		 * is called after the IIO_CHAN_INFO_RAW for the same channel. Standard
		 * IIO API calls is get raw adc count --> get offset --> get scale.
		 */
		*val = adc->cal_offset[chan->channel];
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int realtek_adc_update_scan_mode(struct iio_dev *indio_dev,
										const unsigned long *scan_mask)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	unsigned long flags;
	const struct iio_chan_spec *chan;
	u32 reg_value, bit = 0;
	u32 i = 0;

	spin_lock_irqsave(&adc->lock, flags);

	adc->num_conv = bitmap_weight(scan_mask, indio_dev->masklength);

	realtek_adc_cmd(adc, false);

	for_each_set_bit(bit, scan_mask, indio_dev->masklength) {
		chan = indio_dev->channels + bit;
		i++;
		if (i > RTK_ADC_CH_NUM) {
			spin_unlock_irqrestore(&adc->lock, flags);
			return -EINVAL;
		}

		dev_info(&indio_dev->dev, "Update scan mode chan %d to seq %d\n", chan->channel, i);

		/*  For channel 8~15, not used now.
			reg_value = readl(adc->base + RTK_ADC_CHSW_LIST_1);
			reg_value &= ~(ADC_MASK_CHSW << ADC_SHIFT_CHSW1((i - 1)));
			reg_value |= (u32)(chan->channel << ADC_SHIFT_CHSW1((i - 1)));
			writel(reg_value, adc->base + RTK_ADC_CHSW_LIST_1);
		*/

		/* For channel 0~7 */
		reg_value = readl(adc->base + RTK_ADC_CHSW_LIST_0);
		reg_value &= ~(ADC_MASK_CHSW << ADC_SHIFT_CHSW0((i - 1)));
		reg_value |= (u32)(chan->channel << ADC_SHIFT_CHSW0((i - 1)));
		writel(reg_value, adc->base + RTK_ADC_CHSW_LIST_0);
	}

	if (!i) {
		spin_unlock_irqrestore(&adc->lock, flags);
		return -EINVAL;
	}

	//set adc mode and channel list len
	reg_value = readl(adc->base + RTK_ADC_CONF);
	reg_value &= ~(ADC_MASK_OP_MOD | ADC_MASK_CVLIST_LEN);
	reg_value |= ADC_OP_MOD(adc->mode) | ADC_CVLIST_LEN((i - 1));
	writel(reg_value, adc->base + RTK_ADC_CONF);
	realtek_adc_cmd(adc, true);

	spin_unlock_irqrestore(&adc->lock, flags);

	return 0;
}

/**
 * realtek_adc_debugfs_reg_access - read or write register value
 *
 * To read a value from an ADC register:
 *   echo [ADC reg offset] > direct_reg_access
 *   cat direct_reg_access
 *
 * To write a value in a ADC register:
 *   echo [ADC_reg_offset] [value] > direct_reg_access
 */
static int realtek_adc_debugfs_reg_access(struct iio_dev *indio_dev,
		unsigned reg, unsigned writeval,
		unsigned *readval)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);

	if (!readval) {
		writel(writeval, adc->base + reg);
	} else {
		*readval = readl(adc->base + reg);
	}
	return 0;
}

static int realtek_adc_of_xlate(struct iio_dev *indio_dev,
								const struct of_phandle_args *iiospec)
{
	int i;

	for (i = 0; i < indio_dev->num_channels; i++)
		if (indio_dev->channels[i].channel == iiospec->args[0]) {
			return i;
		}

	return -EINVAL;
}

static const struct iio_info realtek_adc_iio_info = {
	.read_raw = realtek_adc_read_raw,
	.update_scan_mode = realtek_adc_update_scan_mode,
	.debugfs_reg_access = realtek_adc_debugfs_reg_access,
	.of_xlate = realtek_adc_of_xlate,
};

static irqreturn_t realtek_adc_isr(int irq, void *data)
{
	struct realtek_adc_data *adc = data;
	struct iio_dev *indio_dev = iio_priv_to_dev(adc);
	u32 status = readl(adc->base + RTK_ADC_INTR_STS);
	u32 cnt;
	int ret = 0;

	if (status & ADC_BIT_CV_END_STS) {
		/* ADC timer mode end irq. */
		if ((adc->mode != ADC_TIM_TRI_MODE) || !ADC_GET_FLR(readl(adc->base + RTK_ADC_FLR))) {
			writel(status, adc->base + RTK_ADC_INTR_STS);
			return IRQ_HANDLED;
		}
		if (iio_buffer_enabled(indio_dev)) {
			/* Buffer trigger mode. Scan list triggered. */
			for (cnt = 0; cnt < ((indio_dev->scan_bytes) / 2); cnt++) {
				adc->buffer[adc->buf_index] = ADC_GET_DATA_GLOBAL(readl(adc->base + RTK_ADC_DATA_GLOBAL));
				adc->buf_index++;
			}
			ret = iio_push_to_buffers_with_timestamp(indio_dev, adc->buffer, NULL);
			adc->buf_index = 0;
			if (ret < 0) {
				dev_dbg(&indio_dev->dev, "System buffer is full.");
				iio_trigger_notify_done(indio_dev->trig);
				realtek_adc_int_config(adc, (ADC_BIT_IT_CV_END_EN), false);
				rtk_gtimer_deinit(realtek_adc_cfg.timer_idx);
			}
		} else {
			/* Single trigger mode. Only 1 channel triggered. */
			adc->buffer[0] = ADC_GET_DATA_GLOBAL(readl(adc->base + RTK_ADC_DATA_GLOBAL));
			realtek_adc_timtrig_cmd(adc, realtek_adc_cfg.timer_idx, 0, false);
			complete(&adc->completion);
		}
		writel(status, adc->base + RTK_ADC_INTR_STS);
		return IRQ_HANDLED;
	} else if (status & ADC_BIT_FIFO_FULL_STS) {
		/* ADC auto mode irq. */
		if (adc->mode != ADC_AUTO_MODE) {
			writel(status, adc->base + RTK_ADC_INTR_STS);
			return IRQ_HANDLED;
		}
		if (iio_buffer_enabled(indio_dev)) {
			cnt = ADC_GET_FLR(readl(adc->base + RTK_ADC_FLR));
			while (cnt--) {
				adc->buffer[adc->buf_index] = ADC_GET_DATA_GLOBAL(readl(adc->base + RTK_ADC_DATA_GLOBAL));
				adc->buf_index++;
				/* scan_bytes u8 -> buffer[i] u16 */
				if (adc->buf_index == ((indio_dev->scan_bytes) / 2)) {
					ret = iio_push_to_buffers_with_timestamp(indio_dev, adc->buffer, NULL);
					if (ret < 0) {
						dev_dbg(&indio_dev->dev, "System buffer is full.");
						iio_trigger_notify_done(indio_dev->trig);
						realtek_adc_int_config(adc, (ADC_BIT_IT_FIFO_OVER_EN | ADC_BIT_IT_FIFO_FULL_EN), false);
						realtek_adc_auto_cmd(adc, false);
						cnt = 0;
					}
					adc->buf_index = 0;
				}
			}
		} else {
			realtek_adc_int_config(adc, (ADC_BIT_IT_FIFO_OVER_EN | ADC_BIT_IT_FIFO_FULL_EN), false);
			realtek_adc_auto_cmd(adc, false);
		}
		writel(status, adc->base + RTK_ADC_INTR_STS);
		return IRQ_HANDLED;
	} else if (status & ADC_BIT_FIFO_OVER_STS) {
		dev_dbg(&indio_dev->dev, "FIFO overflow\n");
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static void realtek_adc_hw_init(struct iio_dev *indio_dev)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	u32 reg_value, i;
	u32 path_reg;

	realtek_adc_cmd(adc, false);
	// disable interrupt, clear all interrupts
	writel(0, adc->base + RTK_ADC_INTR_CTRL);
	writel(0x3FFFF, adc->base + RTK_ADC_INTR_STS);
	// set clock divider
	writel(ADC_CLK_DIV(realtek_adc_cfg.clk_div), adc->base + RTK_ADC_CLK_DIV);
	//set adc mode and channel list len
	reg_value = readl(adc->base + RTK_ADC_CONF);
	reg_value &= ~ADC_MASK_OP_MOD;
	reg_value |= ADC_OP_MOD(adc->mode);
	writel(reg_value, adc->base + RTK_ADC_CONF);

	// set adc input type
	reg_value = 0;
	path_reg = readl(adc->path_base);
	for (i = 0; i < indio_dev->num_channels; i++) {
		if (indio_dev->channels[i].differential == 1) {
			reg_value |= ADC_CH_BIT(indio_dev->channels[i].channel);
			if (indio_dev->channels[i].channel2 < 6) {
				/* PINMUX above PA6 is for captouch, do not config in adc. */
				/* ADC channel6 uses PIN-VBAT. */
				path_reg &= ~ ADC_CH_BIT(indio_dev->channels[i].channel2);
			}
		}
		if (indio_dev->channels[i].channel < 6) {
				/* PINMUX above PA6 is for captouch, do not config in adc. */
				/* ADC channel6 uses PIN-VBAT. */
				path_reg &= ~ ADC_CH_BIT(indio_dev->channels[i].channel);
		}
	}
	writel(reg_value, adc->base + RTK_ADC_IN_TYPE);
	// Disable digital path input for adc
	writel(path_reg, adc->path_base);
	// set particular channel
	if (realtek_adc_cfg.special_ch < RTK_ADC_CH_NUM) {
		writel(ADC_BIT_IT_CHCV_END_EN, adc->base + RTK_ADC_INTR_CTRL);
		writel(realtek_adc_cfg.special_ch, adc->base + RTK_ADC_IT_CHNO_CON);
	}
	// set FIFO full level
	writel(realtek_adc_cfg.rx_level, adc->base + RTK_ADC_FULL_LVL);
	// set channel ID included in data or not
	if (realtek_adc_cfg.chid_en) {
		reg_value = readl(adc->base + RTK_ADC_DELAY_CNT);
		reg_value |= ADC_BIT_CH_UNMASK;
		writel(reg_value, adc->base + RTK_ADC_DELAY_CNT);
	}
	realtek_adc_cmd(adc, true);
}

static void realtek_adc_hw_deinit(struct iio_dev *indio_dev)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);

	writel(0, adc->base + RTK_ADC_INTR_CTRL);
	writel(0x3FFFF, adc->base + RTK_ADC_INTR_STS);
	realtek_adc_cmd(adc, false);
}

static void realtek_adc_channel_cfg(struct iio_dev *indio_dev,
									struct iio_chan_spec *chan, u32 vinp,
									u32 vinn, int scan_index, bool differential)
{
	char name[10];

	chan->type = IIO_VOLTAGE;
	chan->channel = vinp;
	if (differential) {
		chan->differential = 1;
		chan->channel2 = vinn;
		snprintf(name, 10, "in%d-in%d", vinp, vinn);
	} else {
		snprintf(name, 10, "in%d", vinp);
	}
	chan->datasheet_name = name;
	chan->scan_index = scan_index;
	chan->indexed = 1;
	chan->info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) | BIT(IIO_CHAN_INFO_OFFSET);
	chan->scan_type.sign = 'u';
	chan->scan_type.realbits = ADC_RESOLUTION_BITS;
	chan->scan_type.storagebits = ADC_STORAGE_BITS;
}

static int realtek_adc_init_channel(struct iio_dev *indio_dev)
{
	struct device_node *node = indio_dev->dev.of_node;
	struct realtek_adc_diff_channel diff[RTK_ADC_MAX_DIFF_CHAN_PAIRS];
	struct property *prop = NULL;
	const __be32 *cur = NULL;
	struct iio_chan_spec *channels;
	int scan_index = 0, ret, i;
	u32 val = 0, num_channels = 0, num_diff = 0;
	int nr_requests;

	ret = of_property_count_u32_elems(node, "rtk,adc-channels");
	if (ret > RTK_ADC_CH_NUM) {
		dev_err(&indio_dev->dev, "Bad adc-channels\n");
		return -EINVAL;
	} else if (ret > 0) {
		num_channels += ret;
	}

	ret = of_property_count_elems_of_size(node, "rtk,adc-diff-channels",
										  sizeof(*diff));
	if (ret > RTK_ADC_MAX_DIFF_CHAN_PAIRS) {
		dev_err(&indio_dev->dev, "Bad adc-diff-channels\n");
		return -EINVAL;
	} else if (ret > 0) {
		int size = ret * sizeof(*diff) / sizeof(u32);

		num_diff = ret;
		num_channels += ret;
		ret = of_property_read_u32_array(node, "rtk,adc-diff-channels",
										 (u32 *)diff, size);
		if (ret) {
			return ret;
		}
	}

	ret = of_property_read_u32(node, "rtk,adc-timer-period", &nr_requests);
	if (ret) {
		dev_warn(&indio_dev->dev, "Can't get DTS property rtk,adc-timer-period, set it to default value %d\n", realtek_adc_cfg.period);
	} else {
		dev_dbg(&indio_dev->dev, "Get DTS property rtk,adc-timer-period = %d\n", nr_requests);
		realtek_adc_cfg.period = nr_requests;
	}

	if (!num_channels) {
		dev_err(&indio_dev->dev, "No channels configured\n");
		return -ENODATA;
	}

	channels = devm_kcalloc(&indio_dev->dev, num_channels,
							sizeof(struct iio_chan_spec), GFP_KERNEL);
	if (!channels) {
		return -ENOMEM;
	}

	of_property_for_each_u32(node, "rtk,adc-channels", prop, cur, val) {
		if (val >= RTK_ADC_CH_NUM) {
			dev_err(&indio_dev->dev, "Invalid channel %d\n", val);
			return -EINVAL;
		}

		/* Channel can't be configured both as single-ended & diff */
		for (i = 0; i < num_diff; i++) {
			if (val == diff[i].vinp) {
				dev_err(&indio_dev->dev,
						"Channel %d miss-configured\n",	val);
				return -EINVAL;
			}
		}
		realtek_adc_channel_cfg(indio_dev, &channels[scan_index], val, 0, scan_index, false);
		scan_index++;
	}

	for (i = 0; i < num_diff; i++) {
		if (((diff[i].vinp == 0) && (diff[i].vinn == 1)) ||
			((diff[i].vinp == 1) && (diff[i].vinn == 0)) ||
			((diff[i].vinp == 2) && (diff[i].vinn == 3)) ||
			((diff[i].vinp == 3) && (diff[i].vinn == 2)) ||
			((diff[i].vinp == 4) && (diff[i].vinn == 5)) ||
			((diff[i].vinp == 5) && (diff[i].vinn == 4))) {
			/* We only support differential with predefined channel pairs <0 1> <1 0> <2 3> <3 2> <4 5> <5 4>*/
			realtek_adc_channel_cfg(indio_dev, &channels[scan_index], diff[i].vinp, diff[i].vinn, scan_index, true);
			scan_index++;
		} else {
			dev_err(&indio_dev->dev, "Invalid channel in%d-in%d\n",
					diff[i].vinp, diff[i].vinn);
			return -EINVAL;
		}
	}

	indio_dev->num_channels = scan_index;
	indio_dev->channels = channels;

	return 0;
}

static int realtek_adc_buffer_postenable(struct iio_dev *indio_dev)
{
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	int ret;
	unsigned long flags;

	ret = iio_triggered_buffer_postenable(indio_dev);
	if (ret < 0) {
		return ret;
	}

	spin_lock_irqsave(&adc->lock, flags);

	realtek_adc_cmd(adc, false);

	adc->buf_index = 0;
	realtek_adc_clear_fifo(adc);
	writel(1, adc->base + RTK_ADC_RST_LIST);
	writel(0, adc->base + RTK_ADC_RST_LIST);
	writel(0x3FFFF, adc->base + RTK_ADC_INTR_STS);
	//realtek_adc_int_config(adc, (ADC_BIT_IT_FIFO_OVER_EN | ADC_BIT_IT_FIFO_FULL_EN), true);
	//realtek_adc_auto_cmd(adc, true);

	realtek_adc_cmd(adc, true);

	spin_unlock_irqrestore(&adc->lock, flags);

	return 0;
}

static int realtek_adc_buffer_predisable(struct iio_dev *indio_dev)
{
	int ret;
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	u32 i;
	unsigned long flags;

	ret = iio_triggered_buffer_predisable(indio_dev);
	if (ret < 0) {
		dev_err(&indio_dev->dev, "Predisable failed\n");
	}

	spin_lock_irqsave(&adc->lock, flags);
	realtek_adc_cmd(adc, false);

	for (i = 0; i < RTK_ADC_BUF_SIZE; i++) {
		dev_dbg(&indio_dev->dev, "Dump data[%d] = 0x%04X\n", i, adc->buffer[i]);
	}

	spin_unlock_irqrestore(&adc->lock, flags);
	return ret;
}

static const struct iio_buffer_setup_ops realtek_adc_buffer_setup_ops = {
	.postenable = &realtek_adc_buffer_postenable,
	.predisable = &realtek_adc_buffer_predisable,
};

static irqreturn_t realtek_adc_trigger_handler(int irq, void *private)
{
	struct iio_poll_func *pf = private;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct realtek_adc_data *adc = iio_priv(indio_dev);
	int i = 0, ret = 0;
	unsigned long flags;

	if (adc->mode == ADC_SW_TRI_MODE) {
		spin_lock_irqsave(&adc->lock, flags);
		/* scan_bytes u8 -> buffer[i] u16 */
		for (i = 0; i < (indio_dev->scan_bytes) / 2; i++) {
			realtek_adc_swtrig_cmd(adc, true);
			while (realtek_adc_readable(adc) == 0);
			adc->buffer[i] = ADC_GET_DATA_GLOBAL(readl(adc->base + RTK_ADC_DATA_GLOBAL));
			realtek_adc_swtrig_cmd(adc, false);
		}
		spin_unlock_irqrestore(&adc->lock, flags);

		ret = iio_push_to_buffers_with_timestamp(indio_dev, adc->buffer, pf->timestamp);
		if (ret < 0) {
			dev_dbg(&indio_dev->dev, "System buffer is full.");
		}
		iio_trigger_notify_done(indio_dev->trig);
	} else if (adc->mode == ADC_AUTO_MODE) {
		spin_lock_irqsave(&adc->lock, flags);
		realtek_adc_clear_fifo(adc);
		realtek_adc_int_config(adc, (ADC_BIT_IT_FIFO_OVER_EN | ADC_BIT_IT_FIFO_FULL_EN), true);
		realtek_adc_auto_cmd(adc, true);
		spin_unlock_irqrestore(&adc->lock, flags);
	} else if (adc->mode == ADC_TIM_TRI_MODE) {
		spin_lock_irqsave(&adc->lock, flags);
		realtek_adc_timtrig_cmd(adc, realtek_adc_cfg.timer_idx, realtek_adc_cfg.period, true);
		realtek_adc_int_config(adc, ADC_BIT_IT_CV_END_EN, true);
		spin_unlock_irqrestore(&adc->lock, flags);
	}

	return IRQ_HANDLED;
}

static int realtek_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;
	struct realtek_adc_data *adc;
	int ret;
	struct resource *res;
	const struct clk_hw *hwclk;

	if (!pdev->dev.of_node) {
		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc));
	if (!indio_dev) {
		return -ENOMEM;
	}

	adc = iio_priv(indio_dev);

	spin_lock_init(&adc->lock);
	init_completion(&adc->completion);
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->info = &realtek_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	platform_set_drvdata(pdev, adc);

	if (of_property_read_u32(pdev->dev.of_node, "rtk,adc-mode", &adc->mode)) {
		dev_err(&pdev->dev, "ADC mode property not found\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(adc->base)) {
		return PTR_ERR(adc->base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	adc->comp_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(adc->base)) {
		return PTR_ERR(adc->base);
	}

	adc->adc_clk = devm_clk_get(&pdev->dev, "rtk_adc_clk");
	if (IS_ERR(adc->adc_clk)) {
		dev_err(&pdev->dev, "Failed to get ADC clock\n");
		return PTR_ERR(adc->adc_clk);
	}

	adc->ctc_clk = devm_clk_get(&pdev->dev, "rtk_ctc_clk");
	if (IS_ERR(adc->ctc_clk)) {
		dev_err(&pdev->dev, "Failed to get CTC clock\n");
		return PTR_ERR(adc->ctc_clk);
	}

	ret = clk_prepare_enable(adc->adc_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable ADC clock %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(adc->ctc_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable CTC clock %d\n", ret);
		goto clk_fail;
	}

	adc->path_base = of_iomap(pdev->dev.of_node, 2);
	if (IS_ERR(adc->path_base)) {
		goto err_fail;
	}

	dev_info(&pdev->dev, "ADC mode %d\n", adc->mode);

	if (adc->mode != ADC_COMP_ASSIST_MODE) {
		adc->irq = platform_get_irq(pdev, 0);
		if (adc->irq < 0) {
			goto map_fail;
		}

		ret = devm_request_irq(&pdev->dev, adc->irq, realtek_adc_isr, 0, pdev->name, adc);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request IRQ\n");
			goto map_fail;
		}

		ret = realtek_adc_init_channel(indio_dev);
		if (ret < 0) {
			goto map_fail;
		}

		adc->is_calibdata_read = false;
		adc->k_coeff_normal[0] = adc->k_coeff_normal[1] = adc->k_coeff_normal[2] = 0;
		adc->k_coeff_vbat[0] = adc->k_coeff_vbat[1] = adc->k_coeff_vbat[2] = 0;
		memset(adc->cal_offset, 0, sizeof(adc->cal_offset));
		adc->diff_ch_offset = 0;

		ret = iio_triggered_buffer_setup(indio_dev, &iio_pollfunc_store_time, &realtek_adc_trigger_handler,
										 &realtek_adc_buffer_setup_ops);
		if (ret) {
			dev_err(&pdev->dev, "Buffer setup failed\n");
			goto map_fail;
		}

		realtek_adc_hw_init(indio_dev);
	}
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&pdev->dev, "IIO device register failed\n");
		goto map_fail;
	}

	if (adc->mode == ADC_COMP_ASSIST_MODE) {
		ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to populate DT children\n");
			goto err_dev_register;
		}
	}
	adc->clk_sl = clk_get_parent(adc->adc_clk);
	hwclk = __clk_get_hw(adc->clk_sl);
	adc->adc_parent_ls_apb = clk_hw_get_parent_by_index(hwclk, 0)->clk;
	adc->adc_parent_osc_2m = clk_hw_get_parent_by_index(hwclk, 1)->clk;

	return 0;

err_dev_register:
	iio_device_unregister(indio_dev);
	realtek_adc_hw_deinit(indio_dev);
map_fail:
	iounmap(adc->path_base);
err_fail:
	clk_disable_unprepare(adc->ctc_clk);
clk_fail:
	clk_disable_unprepare(adc->adc_clk);
	return ret;
}

static int realtek_adc_remove(struct platform_device *pdev)
{
	struct realtek_adc_data *adc = platform_get_drvdata(pdev);
	struct iio_dev *indio_dev = iio_priv_to_dev(adc);

	clk_disable_unprepare(adc->adc_clk);
	clk_disable_unprepare(adc->ctc_clk);
	iounmap(adc->path_base);
	iio_device_unregister(indio_dev);
	realtek_adc_hw_deinit(indio_dev);

	return 0;
}

static const struct of_device_id realtek_adc_match[] = {
	{.compatible = "realtek,ameba-adc",},
	{},
};
MODULE_DEVICE_TABLE(of, realtek_adc_match);

static struct platform_driver realtek_adc_driver = {
	.probe	= realtek_adc_probe,
	.remove	= realtek_adc_remove,
	.driver = {
		.name = "realtek-ameba-adc",
		.of_match_table = of_match_ptr(realtek_adc_match),
	},
};

builtin_platform_driver(realtek_adc_driver);

MODULE_DESCRIPTION("Realtek Ameba ADC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
