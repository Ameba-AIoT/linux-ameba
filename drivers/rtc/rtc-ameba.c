// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * An RTC driver for Allwinner A10/A20
 *
 * Copyright (c) 2013, Carlo Caione <carlo.caione@gmail.com>
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/types.h>

#define SUNXI_LOSC_CTRL				0x0000
#define SUNXI_LOSC_CTRL_RTC_HMS_ACC		BIT(8)
#define SUNXI_LOSC_CTRL_RTC_YMD_ACC		BIT(7)

#define SUNXI_RTC_YMD				0x0004

#define SUNXI_RTC_HMS				0x0008

#define SUNXI_ALRM_DHMS				0x000c

#define SUNXI_ALRM_EN				0x0014
#define SUNXI_ALRM_EN_CNT_EN			BIT(8)

#define SUNXI_ALRM_IRQ_EN			0x0018
#define SUNXI_ALRM_IRQ_EN_CNT_IRQ_EN		BIT(0)

#define SUNXI_ALRM_IRQ_STA			0x001c
#define SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND		BIT(0)

#define SUNXI_MASK_DH				0x0000001f
#define SUNXI_MASK_SM				0x0000003f
#define SUNXI_MASK_M				0x0000000f
#define SUNXI_MASK_LY				0x00000001
#define SUNXI_MASK_D				0x00000ffe
#define SUNXI_MASK_M				0x0000000f

#define SUNXI_GET(x, mask, shift)		(((x) & ((mask) << (shift))) \
							>> (shift))

#define SUNXI_SET(x, mask, shift)		(((x) & (mask)) << (shift))

/*
 * Get date values
 */
#define SUNXI_DATE_GET_DAY_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_DH, 0)
#define SUNXI_DATE_GET_MON_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_M, 8)
#define SUNXI_DATE_GET_YEAR_VALUE(x, mask)	SUNXI_GET(x, mask, 16)

/*
 * Get time values
 */
#define SUNXI_TIME_GET_SEC_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_SM, 0)
#define SUNXI_TIME_GET_MIN_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_SM, 8)
#define SUNXI_TIME_GET_HOUR_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_DH, 16)

/*
 * Get alarm values
 */
#define SUNXI_ALRM_GET_SEC_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_SM, 0)
#define SUNXI_ALRM_GET_MIN_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_SM, 8)
#define SUNXI_ALRM_GET_HOUR_VALUE(x)		SUNXI_GET(x, SUNXI_MASK_DH, 16)

/*
 * Set date values
 */
#define SUNXI_DATE_SET_DAY_VALUE(x)		SUNXI_DATE_GET_DAY_VALUE(x)
#define SUNXI_DATE_SET_MON_VALUE(x)		SUNXI_SET(x, SUNXI_MASK_M, 8)
#define SUNXI_DATE_SET_YEAR_VALUE(x, mask)	SUNXI_SET(x, mask, 16)
#define SUNXI_LEAP_SET_VALUE(x, shift)		SUNXI_SET(x, SUNXI_MASK_LY, shift)

/*
 * Set time values
 */
#define SUNXI_TIME_SET_SEC_VALUE(x)		SUNXI_TIME_GET_SEC_VALUE(x)
#define SUNXI_TIME_SET_MIN_VALUE(x)		SUNXI_SET(x, SUNXI_MASK_SM, 8)
#define SUNXI_TIME_SET_HOUR_VALUE(x)		SUNXI_SET(x, SUNXI_MASK_DH, 16)

/*
 * Set alarm values
 */
#define SUNXI_ALRM_SET_SEC_VALUE(x)		SUNXI_ALRM_GET_SEC_VALUE(x)
#define SUNXI_ALRM_SET_MIN_VALUE(x)		SUNXI_SET(x, SUNXI_MASK_SM, 8)
#define SUNXI_ALRM_SET_HOUR_VALUE(x)		SUNXI_SET(x, SUNXI_MASK_DH, 16)
#define SUNXI_ALRM_SET_DAY_VALUE(x)		SUNXI_SET(x, SUNXI_MASK_D, 21)

/*
 * Time unit conversions
 */
#define SEC_IN_MIN				60
#define SEC_IN_HOUR				(60 * SEC_IN_MIN)
#define SEC_IN_DAY				(24 * SEC_IN_HOUR)

/*
 * The year parameter passed to the driver is usually an offset relative to
 * the year 1900. This macro is used to convert this offset to another one
 * relative to the minimum year allowed by the hardware.
 */
#define SUNXI_YEAR_OFF(x)			((x)->min - 1900)

/*
 * min and max year are arbitrary set considering the limited range of the
 * hardware register field
 */
struct ameba_rtc_data_year {
	unsigned int min;		/* min year allowed */
	unsigned int max;		/* max year allowed */
	unsigned int mask;		/* mask for the year field */
	unsigned char leap_shift;	/* bit shift to get the leap year */
};

static const struct ameba_rtc_data_year data_year_param[] = {
	[0] = {
		.min		= 2010,
		.max		= 2073,
		.mask		= 0x3f,
		.leap_shift	= 22,
	},
	[1] = {
		.min		= 1970,
		.max		= 2225,
		.mask		= 0xff,
		.leap_shift	= 24,
	},
};

struct ameba_rtc_dev {
	struct rtc_device *rtc;
	struct device *dev;
	const struct ameba_rtc_data_year *data_year;
	void __iomem *base;
	int irq;
};

static irqreturn_t ameba_rtc_alarmirq(int irq, void *id)
{
#ifdef TODO
	struct ameba_rtc_dev *chip = (struct ameba_rtc_dev *) id;
	u32 val;
	val = readl(chip->base + SUNXI_ALRM_IRQ_STA);

	if (val & SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND) {
		val |= SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND;
		writel(val, chip->base + SUNXI_ALRM_IRQ_STA);

		rtc_update_irq(chip->rtc, 1, RTC_AF | RTC_IRQF);

		return IRQ_HANDLED;
	}
#endif
	return IRQ_NONE;
}

static void ameba_rtc_setaie(unsigned int to, struct ameba_rtc_dev *chip)
{
	u32 alrm_val = 0;
	u32 alrm_irq_val = 0;

	if (to) {
		alrm_val = readl(chip->base + SUNXI_ALRM_EN);
		alrm_val |= SUNXI_ALRM_EN_CNT_EN;

		alrm_irq_val = readl(chip->base + SUNXI_ALRM_IRQ_EN);
		alrm_irq_val |= SUNXI_ALRM_IRQ_EN_CNT_IRQ_EN;
	} else {
		writel(SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND,
				chip->base + SUNXI_ALRM_IRQ_STA);
	}

	writel(alrm_val, chip->base + SUNXI_ALRM_EN);
	writel(alrm_irq_val, chip->base + SUNXI_ALRM_IRQ_EN);
}

static int ameba_rtc_getalarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	struct ameba_rtc_dev *chip = dev_get_drvdata(dev);
	struct rtc_time *alrm_tm = &wkalrm->time;
	u32 alrm;
	u32 alrm_en;
	u32 date;

	alrm = readl(chip->base + SUNXI_ALRM_DHMS);
	date = readl(chip->base + SUNXI_RTC_YMD);

	alrm_tm->tm_sec = SUNXI_ALRM_GET_SEC_VALUE(alrm);
	alrm_tm->tm_min = SUNXI_ALRM_GET_MIN_VALUE(alrm);
	alrm_tm->tm_hour = SUNXI_ALRM_GET_HOUR_VALUE(alrm);

	alrm_tm->tm_mday = SUNXI_DATE_GET_DAY_VALUE(date);
	alrm_tm->tm_mon = SUNXI_DATE_GET_MON_VALUE(date);
	alrm_tm->tm_year = SUNXI_DATE_GET_YEAR_VALUE(date,
			chip->data_year->mask);

	alrm_tm->tm_mon -= 1;

	/*
	 * switch from (data_year->min)-relative offset to
	 * a (1900)-relative one
	 */
	alrm_tm->tm_year += SUNXI_YEAR_OFF(chip->data_year);

	alrm_en = readl(chip->base + SUNXI_ALRM_IRQ_EN);
	if (alrm_en & SUNXI_ALRM_EN_CNT_EN)
		wkalrm->enabled = 1;

	return 0;
}

static int ameba_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm)
{
#ifdef TODO
	struct ameba_rtc_dev *chip = dev_get_drvdata(dev);
	u32 date, time;
	/*
	 * read again in case it changes
	 */
	do {
		date = readl(chip->base + SUNXI_RTC_YMD);
		time = readl(chip->base + SUNXI_RTC_HMS);
	} while ((date != readl(chip->base + SUNXI_RTC_YMD)) ||
		 (time != readl(chip->base + SUNXI_RTC_HMS)));

	rtc_tm->tm_sec  = SUNXI_TIME_GET_SEC_VALUE(time);
	rtc_tm->tm_min  = SUNXI_TIME_GET_MIN_VALUE(time);
	rtc_tm->tm_hour = SUNXI_TIME_GET_HOUR_VALUE(time);

	rtc_tm->tm_mday = SUNXI_DATE_GET_DAY_VALUE(date);
	rtc_tm->tm_mon  = SUNXI_DATE_GET_MON_VALUE(date);
	rtc_tm->tm_year = SUNXI_DATE_GET_YEAR_VALUE(date,
					chip->data_year->mask);

	rtc_tm->tm_mon  -= 1;

	/*
	 * switch from (data_year->min)-relative offset to
	 * a (1900)-relative one
	 */
	rtc_tm->tm_year += SUNXI_YEAR_OFF(chip->data_year);
#endif
	return 0;
}

static int ameba_rtc_setalarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	struct ameba_rtc_dev *chip = dev_get_drvdata(dev);
	struct rtc_time *alrm_tm = &wkalrm->time;
	struct rtc_time tm_now;
#ifdef TODO
	u32 alrm;
#endif
	time64_t diff;
	unsigned long time_gap;
	unsigned long time_gap_day;
	unsigned long time_gap_hour;
	unsigned long time_gap_min;
	int ret;

	ret = ameba_rtc_gettime(dev, &tm_now);
	if (ret < 0) {
		dev_err(dev, "Error in getting time\n");
		return -EINVAL;
	}

	diff = rtc_tm_sub(alrm_tm, &tm_now);
	if (diff <= 0) {
		dev_err(dev, "Date to set in the past\n");
		return -EINVAL;
	}

	if (diff > 255 * SEC_IN_DAY) {
		dev_err(dev, "Day must be in the range 0 - 255\n");
		return -EINVAL;
	}

	time_gap = diff;
	time_gap_day = time_gap / SEC_IN_DAY;
	time_gap -= time_gap_day * SEC_IN_DAY;
	time_gap_hour = time_gap / SEC_IN_HOUR;
	time_gap -= time_gap_hour * SEC_IN_HOUR;
	time_gap_min = time_gap / SEC_IN_MIN;
	time_gap -= time_gap_min * SEC_IN_MIN;

	ameba_rtc_setaie(0, chip);
#ifdef TODO
	writel(0, chip->base + SUNXI_ALRM_DHMS);
	usleep_range(100, 300);

	alrm = SUNXI_ALRM_SET_SEC_VALUE(time_gap) |
		SUNXI_ALRM_SET_MIN_VALUE(time_gap_min) |
		SUNXI_ALRM_SET_HOUR_VALUE(time_gap_hour) |
		SUNXI_ALRM_SET_DAY_VALUE(time_gap_day);
	writel(alrm, chip->base + SUNXI_ALRM_DHMS);

	writel(0, chip->base + SUNXI_ALRM_IRQ_EN);
	writel(SUNXI_ALRM_IRQ_EN_CNT_IRQ_EN, chip->base + SUNXI_ALRM_IRQ_EN);
#endif
	ameba_rtc_setaie(wkalrm->enabled, chip);

	return 0;
}

#ifdef TODO
static int ameba_rtc_wait(struct ameba_rtc_dev *chip, int offset,
			  unsigned int mask, unsigned int ms_timeout)
{
	const unsigned long timeout = jiffies + msecs_to_jiffies(ms_timeout);
	u32 reg;

	do {
		reg = readl(chip->base + offset);
		reg &= mask;

		if (reg == mask)
			return 0;

	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}
#endif

static int ameba_rtc_settime(struct device *dev, struct rtc_time *rtc_tm)
{
	struct ameba_rtc_dev *chip = dev_get_drvdata(dev);
#ifdef TODO
	u32 date = 0;
	u32 time = 0;
#endif
	unsigned int year;

	/*
	 * the input rtc_tm->tm_year is the offset relative to 1900. We use
	 * the SUNXI_YEAR_OFF macro to rebase it with respect to the min year
	 * allowed by the hardware
	 */

	year = rtc_tm->tm_year + 1900;
	if (year < chip->data_year->min || year > chip->data_year->max) {
		dev_err(dev, "rtc only supports year in range %u - %u\n",
			chip->data_year->min, chip->data_year->max);
		return -EINVAL;
	}
#ifdef TODO
	rtc_tm->tm_year -= SUNXI_YEAR_OFF(chip->data_year);
	rtc_tm->tm_mon += 1;

	date = SUNXI_DATE_SET_DAY_VALUE(rtc_tm->tm_mday) |
		SUNXI_DATE_SET_MON_VALUE(rtc_tm->tm_mon)  |
		SUNXI_DATE_SET_YEAR_VALUE(rtc_tm->tm_year,
				chip->data_year->mask);

	if (is_leap_year(year))
		date |= SUNXI_LEAP_SET_VALUE(1, chip->data_year->leap_shift);

	time = SUNXI_TIME_SET_SEC_VALUE(rtc_tm->tm_sec)  |
		SUNXI_TIME_SET_MIN_VALUE(rtc_tm->tm_min)  |
		SUNXI_TIME_SET_HOUR_VALUE(rtc_tm->tm_hour);

	writel(0, chip->base + SUNXI_RTC_HMS);
	writel(0, chip->base + SUNXI_RTC_YMD);

	writel(time, chip->base + SUNXI_RTC_HMS);

	/*
	 * After writing the RTC HH-MM-SS register, the
	 * SUNXI_LOSC_CTRL_RTC_HMS_ACC bit is set and it will not
	 * be cleared until the real writing operation is finished
	 */

	if (ameba_rtc_wait(chip, SUNXI_LOSC_CTRL,
				SUNXI_LOSC_CTRL_RTC_HMS_ACC, 50)) {
		dev_err(dev, "Failed to set rtc time.\n");
		return -1;
	}

	writel(date, chip->base + SUNXI_RTC_YMD);

	/*
	 * After writing the RTC YY-MM-DD register, the
	 * SUNXI_LOSC_CTRL_RTC_YMD_ACC bit is set and it will not
	 * be cleared until the real writing operation is finished
	 */

	if (sunxi_rtc_wait(chip, SUNXI_LOSC_CTRL,
				SUNXI_LOSC_CTRL_RTC_YMD_ACC, 50)) {
		dev_err(dev, "Failed to set rtc time.\n");
		return -1;
	}
#endif

	return 0;
}

static int ameba_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct ameba_rtc_dev *chip = dev_get_drvdata(dev);

	if (!enabled)
		ameba_rtc_setaie(enabled, chip);

	return 0;
}

static const struct rtc_class_ops ameba_rtc_ops = {
	.read_time		= ameba_rtc_gettime,
	.set_time		= ameba_rtc_settime,
	.read_alarm		= ameba_rtc_getalarm,
	.set_alarm		= ameba_rtc_setalarm,
	.alarm_irq_enable	= ameba_rtc_alarm_irq_enable
};

static const struct of_device_id ameba_rtc_dt_ids[] = {
	{ .compatible = "realsil,ameba-rtc", .data = &data_year_param[0] },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ameba_rtc_dt_ids);

static int ameba_rtc_probe(struct platform_device *pdev)
{
	struct ameba_rtc_dev *chip;
	struct resource *res;
	int ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	platform_set_drvdata(pdev, chip);
	chip->dev = &pdev->dev;

	chip->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(chip->rtc))
		return PTR_ERR(chip->rtc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(chip->base))
		return PTR_ERR(chip->base);

	chip->irq = platform_get_irq(pdev, 0);
	if (chip->irq < 0)
		return chip->irq;
	ret = devm_request_irq(&pdev->dev, chip->irq, ameba_rtc_alarmirq,
			0, dev_name(&pdev->dev), chip);
	if (ret) {
		dev_err(&pdev->dev, "Could not request IRQ\n");
		return ret;
	}

	chip->data_year = of_device_get_match_data(&pdev->dev);
	if (!chip->data_year) {
		dev_err(&pdev->dev, "Unable to setup RTC data\n");
		return -ENODEV;
	}

#ifdef TODO
	/* clear the alarm count value */
	writel(0, chip->base + SUNXI_ALRM_DHMS);

	/* disable alarm, not generate irq pending */
	writel(0, chip->base + SUNXI_ALRM_EN);

	/* disable alarm week/cnt irq, unset to cpu */
	writel(0, chip->base + SUNXI_ALRM_IRQ_EN);

	/* clear alarm week/cnt irq pending */
	writel(SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND, chip->base +
			SUNXI_ALRM_IRQ_STA);
#endif
	chip->rtc->ops = &ameba_rtc_ops;

	return rtc_register_device(chip->rtc);
}

static struct platform_driver ameba_rtc_driver = {
	.probe		= ameba_rtc_probe,
	.driver		= {
		.name		= "ameba-rtc",
		.of_match_table = ameba_rtc_dt_ids,
	},
};

module_platform_driver(ameba_rtc_driver);

MODULE_DESCRIPTION("Realtek RTC driver");
MODULE_AUTHOR("Jingjun WU <jingjun_wu@realsil.com.cn>");
MODULE_LICENSE("GPL");
