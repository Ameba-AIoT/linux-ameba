// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek RTC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/module.h>
#include <linux/err.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>

#include <linux/io.h>
#include <linux/module.h>
#include <linux/sizes.h>
#include <linux/of.h>

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/time.h>

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeirq.h>



#define RTK_RTC_TR			0x000
#define RTK_RTC_CR			0x004
#define RTK_RTC_ISR			0x008
#define RTK_RTC_PRER		0x00C
#define RTK_RTC_CALIBR		0x010
#define RTK_RTC_ALMR1L		0x014
#define RTK_RTC_ALMR1H		0x018
#define RTK_RTC_WPR			0x01C
#define RTK_RTC_YEAR		0x020
#define RTK_RTC_WUTR		0x024

#define RTC_MASK_DAY        ((u32)0x000001FF << 23)
#define RTC_DAY(x)          ((u32)(((x) & 0x000001FF) << 23))
#define RTC_GET_DAY(x)      ((u32)(((x >> 23) & 0x000001FF)))
#define RTC_BIT_PM          ((u32)0x00000001 << 22)
#define RTC_PM(x)           ((u32)(((x) & 0x00000001) << 22))
#define RTC_MASK_HT         ((u32)0x00000003 << 20)
#define RTC_HT(x)           ((u32)(((x) & 0x00000003) << 20))
#define RTC_GET_HT(x)       ((u32)(((x >> 20) & 0x00000003)))
#define RTC_MASK_HU         ((u32)0x0000000F << 16)
#define RTC_HU(x)           ((u32)(((x) & 0x0000000F) << 16))
#define RTC_GET_HU(x)       ((u32)(((x >> 16) & 0x0000000F)))
#define RTC_MASK_MNT        ((u32)0x00000007 << 12)
#define RTC_MNT(x)          ((u32)(((x) & 0x00000007) << 12))
#define RTC_GET_MNT(x)      ((u32)(((x >> 12) & 0x00000007)))
#define RTC_MASK_MNU        ((u32)0x0000000F << 8)
#define RTC_MNU(x)          ((u32)(((x) & 0x0000000F) << 8))
#define RTC_GET_MNU(x)      ((u32)(((x >> 8) & 0x0000000F)))
#define RTC_MASK_ST         ((u32)0x00000007 << 4)
#define RTC_ST(x)           ((u32)(((x) & 0x00000007) << 4))
#define RTC_GET_ST(x)       ((u32)(((x >> 4) & 0x00000007)))
#define RTC_MASK_SU         ((u32)0x0000000F << 0)
#define RTC_SU(x)           ((u32)(((x) & 0x0000000F) << 0))
#define RTC_GET_SU(x)       ((u32)(((x >> 0) & 0x0000000F)))

#define RTC_MASK_DAY_THRES  ((u32)0x000001FF << 23)
#define RTC_DAY_THRES(x)    ((u32)(((x) & 0x000001FF) << 23))
#define RTC_GET_DAY_THRES(x) ((u32)(((x >> 23) & 0x000001FF)))
#define RTC_BIT_DOVTHIE     ((u32)0x00000001 << 16)
#define RTC_DOVTHIE(x)      ((u32)(((x) & 0x00000001) << 16))
#define RTC_BIT_WUTIE       ((u32)0x00000001 << 14)
#define RTC_WUTIE(x)        ((u32)(((x) & 0x00000001) << 14))
#define RTC_BIT_ALMIE       ((u32)0x00000001 << 12)
#define RTC_ALMIE(x)        ((u32)(((x) & 0x00000001) << 12))
#define RTC_BIT_WUTE        ((u32)0x00000001 << 10)
#define RTC_WUTE(x)         ((u32)(((x) & 0x00000001) << 10))
#define RTC_BIT_ALME        ((u32)0x00000001 << 8)
#define RTC_ALME(x)         ((u32)(((x) & 0x00000001) << 8))
#define RTC_BIT_FMT         ((u32)0x00000001 << 7)
#define RTC_FMT(x)          ((u32)(((x) & 0x00000001) << 7))
#define RTC_MASK_OSEL       ((u32)0x00000003 << 5)
#define RTC_OSEL(x)         ((u32)(((x) & 0x00000003) << 5))
#define RTC_GET_OSEL(x)     ((u32)(((x >> 5) & 0x00000003)))
#define RTC_BIT_BYPSHAD     ((u32)0x00000001 << 3)
#define RTC_BKP(x)          ((u32)(((x) & 0x00000001) << 2))
#define RTC_BIT_SUB1H       ((u32)0x00000001 << 1)
#define RTC_SUB1H(x)        ((u32)(((x) & 0x00000001) << 1))
#define RTC_BIT_ADD1H       ((u32)0x00000001 << 0)
#define RTC_ADD1H(x)        ((u32)(((x) & 0x00000001) << 0))

#define RTC_BIT_RECALPF     ((u32)0x00000001 << 16)
#define RTC_RECALPF(x)      ((u32)(((x) & 0x00000001) << 16))
#define RTC_BIT_DOVTHF      ((u32)0x00000001 << 15)
#define RTC_DOVTHF(x)       ((u32)(((x) & 0x00000001) << 15))
#define RTC_BIT_WUTF        ((u32)0x00000001 << 10)
#define RTC_WUTF(x)         ((u32)(((x) & 0x00000001) << 10))
#define RTC_BIT_ALMF        ((u32)0x00000001 << 8)
#define RTC_ALMF(x)         ((u32)(((x) & 0x00000001) << 8))
#define RTC_BIT_INIT        ((u32)0x00000001 << 7)
#define RTC_INIT(x)         ((u32)(((x) & 0x00000001) << 7))
#define RTC_BIT_INITF       ((u32)0x00000001 << 6)
#define RTC_INITF(x)        ((u32)(((x) & 0x00000001) << 6))
#define RTC_BIT_RSF         ((u32)0x00000001 << 5)
#define RTC_RSF(x)          ((u32)(((x) & 0x00000001) << 5))
#define RTC_BIT_INITS       ((u32)0x00000001 << 4)
#define RTC_INITS(x)        ((u32)(((x) & 0x00000001) << 4))
#define RTC_BIT_WUTWF       ((u32)0x00000001 << 2)
#define RTC_WUTWF(x)        ((u32)(((x) & 0x00000001) << 2))
#define RTC_BIT_WUTRSF      ((u32)0x00000001 << 1)
#define RTC_WUTRSF(x)       ((u32)(((x) & 0x00000001) << 1))
#define RTC_BIT_ALMWF       ((u32)0x00000001 << 0)

#define RTC_MASK_PREDIV_A   ((u32)0x000001FF << 16)
#define RTC_PREDIV_A(x)     ((u32)(((x) & 0x000001FF) << 16))
#define RTC_GET_PREDIV_A(x) ((u32)(((x >> 16) & 0x000001FF)))
#define RTC_MASK_PREDIV_S   ((u32)0x000001FF << 0)
#define RTC_PREDIV_S(x)     ((u32)(((x) & 0x000001FF) << 0))
#define RTC_GET_PREDIV_S(x) ((u32)(((x >> 0) & 0x000001FF)))

#define RTC_MASK_CALP       ((u32)0x00000007 << 16)
#define RTC_CALP(x)         ((u32)(((x) & 0x00000007) << 16))
#define RTC_GET_CALP(x)     ((u32)(((x >> 16) & 0x00000007)))
#define RTC_BIT_DCE         ((u32)0x00000001 << 15)
#define RTC_DCE(x)          ((u32)(((x) & 0x00000001) << 15))
#define RTC_BIT_DCS         ((u32)0x00000001 << 14)
#define RTC_DC(x)           ((u32)(((x) & 0x0000007F) << 0))
#define RTC_GET_DC(x)       ((u32)(((x >> 0) & 0x0000007F)))

#define RTC_BIT_MSK2        ((u32)0x00000001 << 23)
#define RTC_MSK2(x)         ((u32)(((x) & 0x00000001) << 23))
#define RTC_BIT_ALR_PM      ((u32)0x00000001 << 22)
#define RTC_ALR_PM(x)       ((u32)(((x) & 0x00000001) << 22))
#define RTC_MASK_ALR_HT     ((u32)0x00000003 << 20)
#define RTC_ALR_HT(x)       ((u32)(((x) & 0x00000003) << 20))
#define RTC_GET_ALR_HT(x)   ((u32)(((x >> 20) & 0x00000003)))
#define RTC_MASK_ALR_HU     ((u32)0x0000000F << 16)
#define RTC_ALR_HU(x)       ((u32)(((x) & 0x0000000F) << 16))
#define RTC_GET_ALR_HU(x)   ((u32)(((x >> 16) & 0x0000000F)))
#define RTC_BIT_MSK1        ((u32)0x00000001 << 15)
#define RTC_MSK1(x)         ((u32)(((x) & 0x00000001) << 15))
#define RTC_MASK_ALR_MNT    ((u32)0x00000007 << 12)
#define RTC_ALR_MNT(x)      ((u32)(((x) & 0x00000007) << 12))
#define RTC_GET_ALR_MNT(x)  ((u32)(((x >> 12) & 0x00000007)))
#define RTC_MASK_ALR_MNU    ((u32)0x0000000F << 8)
#define RTC_ALR_MNU(x)      ((u32)(((x) & 0x0000000F) << 8))
#define RTC_GET_ALR_MNU(x)  ((u32)(((x >> 8) & 0x0000000F)))
#define RTC_BIT_MSK0        ((u32)0x00000001 << 7)
#define RTC_MSK0(x)         ((u32)(((x) & 0x00000001) << 7))
#define RTC_MASK_ALR_ST     ((u32)0x00000007 << 4)
#define RTC_ALR_ST(x)       ((u32)(((x) & 0x00000007) << 4))
#define RTC_GET_ALR_ST(x)   ((u32)(((x >> 4) & 0x00000007)))
#define RTC_MASK_ALR_SU     ((u32)0x0000000F << 0)
#define RTC_ALR_SU(x)       ((u32)(((x) & 0x0000000F) << 0))
#define RTC_GET_ALR_SU(x)   ((u32)(((x >> 0) & 0x0000000F)))

#define RTC_BIT_MSK3        ((u32)0x00000001 << 9)
#define RTC_MSK3(x)         ((u32)(((x) & 0x00000001) << 9))
#define RTC_MASK_ALR_DAY    ((u32)0x000001FF << 0)
#define RTC_ALR_DAY(x)      ((u32)(((x) & 0x000001FF) << 0))
#define RTC_GET_ALR_DAY(x)  ((u32)(((x >> 0) & 0x000001FF)))

#define RTC_MASK_KEY        ((u32)0x000000FF << 0)
#define RTC_KEY(x)          ((u32)(((x) & 0x000000FF) << 0))
#define RTC_GET_KEY(x)      ((u32)(((x >> 0) & 0x000000FF)))

#define RTC_BIT_RESTORE     ((u32)0x00000001 << 31)
#define RTC_RESTORE(x)      ((u32)(((x) & 0x00000001) << 31))
#define RTC_MASK_YEAR       ((u32)0x000000FF << 0)
#define RTC_YEAR(x)         ((u32)(((x) & 0x000000FF) << 0))
#define RTC_GET_YEAR(x)     ((u32)(((x >> 0) & 0x000000FF)))

#define RTC_MASK_WUT        ((u32)0x0001FFFF << 0)
#define RTC_WUT(x)          ((u32)(((x) & 0x0001FFFF) << 0))
#define RTC_GET_WUT(x)      ((u32)(((x >> 0) & 0x0001FFFF)))

#define RTC_HourFormat_24			((u32)0x00000000)
#define RTC_HourFormat_12			((u32)0x00000080)

#define RTC_DAYTHRES_MSK			((u32)0xFF800000)
#define RTC_BASE_YEAR		((u16)1900)

#define RTC_Alarm				((u32)0x00000100)
#define RTC_Alarm_IntEn				((u32)0x00001000)

#define RTC_TR_RESERVED_MASK	((u32)0xFFFF7F7F)
#define INITMODE_TIMEOUT	((u32) 0x00010000)
#define SYNCHRO_TIMEOUT	((u32) 0x00020000)
#define RECALPF_TIMEOUT	((u32) 0x00020000)
#define ALARMDIS_TIMEOUT	((u32) 0x00020000)
#define WUTDIS_TIMEOUT	((u32) 0x00020000)

#define RTC_DISABLE 0
#define RTC_ENABLE 1


struct rtk_rtc {
	struct rtc_device *rtc_dev;
	void __iomem *ioaddr;
	struct clk *rtc_ck;
	int irq_alarm;
};


static inline bool rtk_is_leap_year(unsigned int year)
{
	u32 full_year = year + RTC_BASE_YEAR;
	return (!(full_year % 4) && (full_year % 100)) || !(full_year % 400);
}


static u8 days_in_month(u8 month, u8 year)
{
	u8 dim[12] = {31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	u8 ret = 0;

	if (month > 11) {
		pr_err("Out of month table.\n");
		return 0; //Illegal month
	}

	ret = dim[month];

	if (ret == 0) {
		ret = rtk_is_leap_year(year) ? 29 : 28;
	}
	return ret;
}


static void calculate_yday(int year, int mon, int mday, int *yday)
{
	int i;
	u8 dim[12] = {31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	dim[1] = rtk_is_leap_year(year) ? 29 : 28;
	*yday = 0;

	for (i = 0; i < mon; i++) {
		(*yday) += dim[i];
	}

	(*yday) += (mday - 1);
}


static void calculate_mday(int year, int yday, int *mon, int *mday)
{
	int t_mon = -1, t_yday = yday + 1;

	while (t_yday > 0) {
		t_mon ++;
		t_yday -= days_in_month(t_mon, year);
	}

	*mon = t_mon;
	*mday = t_yday + days_in_month(t_mon, year);
}


static void calculate_wday(int year, int mon, int mday, int *wday)
{
	int t_year = year + RTC_BASE_YEAR, t_mon = mon + 1;
	int c, y, week;

	if (t_mon == 1 || t_mon == 2) {
		t_year --;
		t_mon += 12;
	}

	c = t_year / 100;
	y = t_year % 100;
	week = (c / 4) - 2 * c + (y + y / 4) + (26 * (t_mon + 1) / 10) + mday - 1;

	while (week < 0) {
		week += 7;
	}
	week %= 7;

	*wday = week;
}


static inline void rtk_rtc_write_proctect(struct device *dev, u32 status)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;

	if (status == RTC_DISABLE) {
		writel(RTC_KEY(0xCA), base + RTK_RTC_WPR);
		writel(RTC_KEY(0x53), base + RTK_RTC_WPR);
	} else {
		writel(RTC_KEY(0xFF), base + RTK_RTC_WPR);
	}
}


static u32 rtk_rtc_enter_init_mode(struct device *dev)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 reg;
	u32 counter = 0x00;
	u32 status = 0;
	u32 initstatus = 0x00;

	/* Check if the initialization mode is set */
	if ((readl(base + RTK_RTC_ISR) & RTC_BIT_INITF) == 0) {
		/* Set the initialization mode */
		reg = readl(base + RTK_RTC_ISR);
		reg |= (u32)RTC_BIT_INIT;
		writel(reg, base + RTK_RTC_ISR);

		/* Wait till RTC is in init state and if time out is reached exit */
		do {
			/* Check Calendar registers update is allowed */
			initstatus = readl(base + RTK_RTC_ISR) & RTC_BIT_INITF;
			counter++;
		} while ((counter != INITMODE_TIMEOUT * 10) && (initstatus == 0x00));

		if (readl(base + RTK_RTC_ISR) & RTC_BIT_INITF) {
			status = 1;
		} else {
			status = 0;
		}
	} else {
		status = 1;
	}

	return (status);
}


static void rtk_rtc_exit_init_mode(struct device *dev)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 reg;

	reg = readl(base + RTK_RTC_ISR);
	/* Exit Initialization mode */
	reg &= (u32)(~RTC_BIT_INIT);
	writel(reg, base + RTK_RTC_ISR);
}


static u32 rtk_rtc_wait_for_synchro(struct device *dev)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 counter = 0;

	u32 status = 0;
	u32 synchrostatus = 0x00;
	u32 reg;

	/* We can not poll RTC_ISR_RSF when RTC_CR_BYPSHAD */
	if (readl(base + RTK_RTC_CR) & RTC_BIT_BYPSHAD) {
		return 1;
	}

	/* Disable the write protection for RTC registers */
	rtk_rtc_write_proctect(dev, RTC_DISABLE);

	/* Write 1 to Clear RSF flag */
	reg = readl(base + RTK_RTC_ISR) | RTC_BIT_RSF;
	writel(reg, base + RTK_RTC_ISR);

	/* Wait the registers to be synchronised */
	do {
		synchrostatus = readl(base + RTK_RTC_ISR) & RTC_BIT_RSF; /* 6bus cycle one time */
		counter++;
	} while ((counter != SYNCHRO_TIMEOUT * 10) && (synchrostatus == 0x00));

	if (readl(base + RTK_RTC_ISR) & RTC_BIT_RSF) {
		status = 1;
	} else {
		status = 0;
	}

	/* Enable the write protection for RTC registers */
	rtk_rtc_write_proctect(dev, RTC_ENABLE);

	return (status);
}


static irqreturn_t rtk_rtc_alarm_irq(int irq, void *dev_id)
{
	struct rtk_rtc *rtc = (struct rtk_rtc *)dev_id;
	void __iomem *base = rtc->ioaddr;
	u32 reg;
	u32 counter = 0;

	mutex_lock(&rtc->rtc_dev->ops_lock);

	dev_dbg(&rtc->rtc_dev->dev, "Alarm occurred\n");

	/* Pass event to the kernel */
	rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);

	reg = readl(base + RTK_RTC_ISR);
	reg &= ~(RTC_BIT_DOVTHF | RTC_BIT_WUTF);
	reg |= RTC_BIT_ALMF;
	writel(reg, base + RTK_RTC_ISR);

	while (1) {
		/* Check alarm flag clear success */
		if ((readl(base + RTK_RTC_ISR) & RTC_BIT_ALMF) == 0) {
			break;
		}
		if (counter >= ALARMDIS_TIMEOUT) {
			break;
		}
		counter++;
	}

	mutex_unlock(&rtc->rtc_dev->ops_lock);

	return IRQ_HANDLED;
}


static int rtk_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 reg;
	volatile u32 year_flag = 0;
	struct rtc_time tm_temp = {0};
	u32 alarm_en;
	u32 ydays = 365;

	reg = readl(base + RTK_RTC_TR) & RTC_TR_RESERVED_MASK;

	/* Hour, min, sec need convert to binary from BCD*/
	tm_temp.tm_hour = RTC_GET_HU(reg) + 10 * RTC_GET_HT(reg);
	tm_temp.tm_min = RTC_GET_MNU(reg) + 10 * RTC_GET_MNT(reg);
	tm_temp.tm_sec = RTC_GET_SU(reg) + 10 * RTC_GET_ST(reg);
	tm_temp.tm_yday = RTC_GET_DAY(reg);
	tm_temp.tm_year = RTC_GET_YEAR(readl(base + RTK_RTC_YEAR));

	if (tm_temp.tm_yday >= 365) {
		if (rtk_is_leap_year(tm_temp.tm_year)) {
			ydays = 366;
			if (tm_temp.tm_yday > 365) {
				tm_temp.tm_yday -= ydays;
				tm_temp.tm_year += 1;
				year_flag = 1;
			}
		} else {
			ydays = 365;
			tm_temp.tm_yday -= ydays;
			tm_temp.tm_year += 1;
			year_flag = 1;
		}
	}

	if (year_flag == 1) {

		rtk_rtc_write_proctect(dev, RTC_DISABLE);

		reg = readl(base + RTK_RTC_TR);
		reg &= ~RTC_MASK_DAY;
		reg |= tm_temp.tm_yday;
		writel(reg, base + RTK_RTC_TR);

		reg = readl(base + RTK_RTC_YEAR);
		reg &= ~RTC_MASK_YEAR;
		reg |= tm_temp.tm_year;
		writel(reg, base + RTK_RTC_YEAR);

		alarm_en = ((readl(base + RTK_RTC_CR) & RTC_BIT_ALMIE) && (readl(base + RTK_RTC_CR) & RTC_BIT_ALME)) ? 1 : 0;
		if (alarm_en && ((readl(base + RTK_RTC_ALMR1H) & RTC_MASK_ALR_DAY) > ydays)) {
			reg = readl(base + RTK_RTC_ALMR1H);
			reg &= ~RTC_MASK_ALR_DAY;
			reg |= (readl(base + RTK_RTC_ALMR1H) & RTC_MASK_ALR_DAY) - ydays;
			writel(reg, base + RTK_RTC_ALMR1H);
		}

		rtk_rtc_write_proctect(dev, RTC_ENABLE);
	}

	calculate_mday(tm_temp.tm_year, tm_temp.tm_yday, &tm_temp.tm_mon, &tm_temp.tm_mday);
	calculate_wday(tm_temp.tm_year, tm_temp.tm_mon, tm_temp.tm_mday, &tm_temp.tm_wday);

	/* RTC_DAY in reg = tm_yday in struct rtc_time - 1*/
	tm_temp.tm_yday += 1;

	memcpy((void *)tm, (void *)&tm_temp, sizeof(struct tm));

	return 0;
}


static int rtk_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 reg, hourt, mint, sect, houru, minu, secu;
	u32 year_day;   //0 means Jan 1st

	calculate_yday(tm->tm_year + 1900, tm->tm_mon, tm->tm_mday, &year_day);

	rtk_rtc_write_proctect(dev, RTC_DISABLE);

	if (rtk_rtc_enter_init_mode(dev)) {

		/* Hour, min, sec need convert to BCD*/
		hourt = tm->tm_hour / 10;
		houru = tm->tm_hour % 10;
		mint = tm->tm_min / 10;
		minu = tm->tm_min % 10;
		sect = tm->tm_sec / 10;
		secu = tm->tm_sec % 10;

		reg = readl(base + RTK_RTC_TR);
		reg &= (~RTC_MASK_HT) & (~RTC_MASK_HU) & (~RTC_MASK_MNT) & \
			   (~RTC_MASK_MNU) & (~RTC_MASK_ST) & (~RTC_MASK_SU);

		reg |= RTC_HT(hourt) | RTC_HU(houru) | RTC_MNT(mint) | \
			   RTC_MNU(minu) | RTC_ST(sect) | RTC_SU(secu);

		reg &= ~RTC_MASK_DAY;
		reg |= RTC_DAY(year_day);
		writel(reg, base + RTK_RTC_TR);

		reg = readl(base + RTK_RTC_YEAR);
		reg &= (~RTC_MASK_YEAR);
		reg |= RTC_YEAR(tm->tm_year);
		writel(reg, base + RTK_RTC_YEAR);

		rtk_rtc_exit_init_mode(dev);

		rtk_rtc_wait_for_synchro(dev);
	}

	rtk_rtc_write_proctect(dev, RTC_ENABLE);

	return 0;
}


static int rtk_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 reg;

	reg = readl(base + RTK_RTC_ISR);

	alrm->pending = (readl(base + RTK_RTC_ISR) & RTC_BIT_ALMF) ? 1 : 0;
	alrm->enabled = ((readl(base + RTK_RTC_CR) & RTC_BIT_ALMIE) && (readl(base + RTK_RTC_CR) & RTC_BIT_ALME)) ? 1 : 0;

	/* Year alarm not support */
	alrm->time.tm_year = -1;

	/* Day and month alarm */
	if (readl(base + RTK_RTC_ALMR1H) & RTC_BIT_MSK3) {
		alrm->time.tm_yday = -1;
		alrm->time.tm_mon = -1;
		alrm->time.tm_mday = -1;
		alrm->time.tm_wday = -1;
	} else {
		alrm->time.tm_yday = (readl(base + RTK_RTC_ALMR1H) & RTC_MASK_ALR_DAY) + 1;

		reg = readl(base + RTK_RTC_YEAR);
		calculate_mday(reg & RTC_MASK_YEAR, alrm->time.tm_yday, &alrm->time.tm_mon, &alrm->time.tm_mday);
		calculate_wday(reg & RTC_MASK_YEAR, alrm->time.tm_mon, alrm->time.tm_mday, &alrm->time.tm_wday);
	}

	if (readl(base + RTK_RTC_ALMR1L) & RTC_BIT_MSK0) {
		alrm->time.tm_sec = -1;
	} else {
		reg = readl(base + RTK_RTC_ALMR1L);
		alrm->time.tm_sec = RTC_GET_ALR_SU(reg) + 10 * RTC_GET_ALR_ST(reg);
	}

	if (readl(base + RTK_RTC_ALMR1L) & RTC_BIT_MSK1) {
		alrm->time.tm_min = -1;
	} else {
		reg = readl(base + RTK_RTC_ALMR1L);
		alrm->time.tm_min = RTC_GET_ALR_MNU(reg) + 10 * RTC_GET_ALR_MNT(reg);
	}

	if (readl(base + RTK_RTC_ALMR1L) & RTC_BIT_MSK2) {
		alrm->time.tm_hour = -1;
	} else {
		reg = readl(base + RTK_RTC_ALMR1L);
		alrm->time.tm_hour = RTC_GET_ALR_HU(reg) + 10 * RTC_GET_ALR_HT(reg);
	}

	return 0;
}


static int rtk_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 reg;
	u32 counter = 0;

	rtk_rtc_write_proctect(dev, RTC_DISABLE);

	/* Configure the alarm state */
	if (enabled != RTC_DISABLE) {
		reg = readl(base + RTK_RTC_ISR) | RTC_BIT_ALMF;
		writel(reg, base + RTK_RTC_ISR);

		reg = readl(base + RTK_RTC_CR) | (RTC_Alarm | RTC_Alarm_IntEn);
		writel(reg, base + RTK_RTC_CR);

		/* We should wait shadow reigster sync ok */
		rtk_rtc_wait_for_synchro(dev);
	} else {
		/* Clear ISR, or set will fail */
		reg = readl(base + RTK_RTC_ISR) | RTC_BIT_ALMF;
		writel(reg, base + RTK_RTC_ISR);

		/* Disable the alarm in RTK_RTC_CR register */
		reg = readl(base + RTK_RTC_CR) & ((u32)~(RTC_Alarm | RTC_Alarm_IntEn));
		writel(reg, base + RTK_RTC_CR);

		/* Wait alarm disable */
		while (1) {
			/* Check alarm update allowed */
			if ((readl(base + RTK_RTC_ISR) & RTC_BIT_ALMWF) && (!(readl(base + RTK_RTC_CR) & RTC_BIT_ALME))) {
				break;
			}

			if (counter >= ALARMDIS_TIMEOUT) {
				break;
			}

			counter++;
		}
	}

	rtk_rtc_write_proctect(dev, RTC_ENABLE);

	return 0;
}


static int rtk_rtc_check_alarm(struct device *dev, struct rtc_time *alarm)
{
	struct rtc_time tm;
	u32 ydays;

	rtk_rtc_read_time(dev, &tm);

	if (alarm->tm_year < tm.tm_year || (alarm->tm_year - tm.tm_year) > 1) {
		return -EINVAL;
	} else if ((alarm->tm_year - tm.tm_year) == 1) {
		if (rtk_is_leap_year(tm.tm_year)) {
			ydays = 366;
		} else {
			ydays = 365;
		}

		if ((alarm->tm_yday + ydays) > 511) {
			return -EINVAL;
		} else {
			alarm->tm_yday += ydays;
		}
	}

	return 0;
}


static int rtk_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 counter = 0;
	u32 reg, hourt, mint, sect, houru, minu, secu;
	struct rtc_time *tm = &alrm->time;

	if (rtk_rtc_check_alarm(dev, tm)) {
		dev_err(dev, "Cannot set alarm because of the invalid alarm time\n");
		return -EINVAL;
	}

	/*hour, min, sec need convert to BCD*/
	hourt = tm->tm_hour / 10;
	houru = tm->tm_hour % 10;
	mint = tm->tm_min / 10;
	minu = tm->tm_min % 10;
	sect = tm->tm_sec / 10;
	secu = tm->tm_sec % 10;

	rtk_rtc_write_proctect(dev, RTC_DISABLE);

	/* Disable alarm */
	reg = readl(base + RTK_RTC_CR) & (~RTC_Alarm);
	writel(reg, base + RTK_RTC_CR);

	while (1) {
		/* Check alarm update allowed */
		if (readl(base + RTK_RTC_ISR) & RTC_BIT_ALMWF) {
			break;
		}
		if (counter >= ALARMDIS_TIMEOUT) {
			break;
		}

		counter++;
	}

	/* Clear ISR, or set wll fail */
	reg = readl(base + RTK_RTC_ISR) | RTC_BIT_ALMF;
	writel(reg, base + RTK_RTC_ISR);

	if (readl(base + RTK_RTC_ISR) & RTC_BIT_ALMWF) {
		/* Configure the alarm1 register H:M:S */
		reg = readl(base + RTK_RTC_ALMR1L);
		reg &= RTC_BIT_ALR_PM;
		reg |= RTC_ALR_HT(hourt) | RTC_ALR_HU(houru) | RTC_ALR_MNT(mint) | \
			   RTC_ALR_MNU(minu) | RTC_ALR_ST(sect) | RTC_ALR_SU(secu);
		writel(reg, base + RTK_RTC_ALMR1L);

		/* Configure the alarm2 register D */
		reg = readl(base + RTK_RTC_ALMR1H);
		reg &= ~RTC_MASK_ALR_DAY;
		reg |= tm->tm_yday - 1;
		writel(reg, base + RTK_RTC_ALMR1H);

	} else {
		rtk_rtc_write_proctect(dev, RTC_ENABLE);

		return -1;
	}

	rtk_rtc_write_proctect(dev, RTC_ENABLE);

	rtk_rtc_alarm_irq_enable(dev, alrm->enabled);

	return 0;
}


static const struct rtc_class_ops rtk_rtc_ops = {
	.read_time	= rtk_rtc_read_time,
	.set_time	= rtk_rtc_set_time,
	.read_alarm	= rtk_rtc_read_alarm,
	.set_alarm	= rtk_rtc_set_alarm,
	.alarm_irq_enable = rtk_rtc_alarm_irq_enable,
};


static int rtk_rtc_init(struct device *dev)
{
	struct rtk_rtc *rtc = dev_get_drvdata(dev);
	void __iomem *base = rtc->ioaddr;
	u32 status = 0;
	u32 reg;

	rtk_rtc_write_proctect(dev, RTC_DISABLE);

	if (rtk_rtc_enter_init_mode(dev)) {
		/* Configure the RTC PRER to provide 1Hz to the calendar */
		reg = 0x007F00FF;
		writel(reg, base + RTK_RTC_PRER);

		/* Set RTC CR FMT Bit */
		reg = readl(base + RTK_RTC_CR);
		reg &= ((u32)~(RTC_BIT_FMT | RTC_DAYTHRES_MSK | RTC_BIT_DOVTHIE));
		reg |= RTC_HourFormat_24 | (0x1FF << 23);

		/* Reset the BYPSHAD bit, get counter from shadow registger */
		reg &= ~RTC_BIT_BYPSHAD;
		writel(reg, base + RTK_RTC_CR);

		rtk_rtc_exit_init_mode(dev);

		if (rtk_rtc_wait_for_synchro(dev)) {
			status = 1;
		} else {
			status = 0;
		}

	} else {
		status = 0;
	}

	rtk_rtc_write_proctect(dev, RTC_ENABLE);

	return status;
}


static int rtk_rtc_probe(struct platform_device *pdev)
{
	struct rtk_rtc *rtc;
	struct resource *res;
	int ret;
	struct rtc_time tm;

	rtc = (struct rtk_rtc *) devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to malloc: %d\n", ret);
		goto fail;
	}

	platform_set_drvdata(pdev, rtc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->ioaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc->ioaddr)) {
		ret =  PTR_ERR(rtc->ioaddr);
		dev_err(&pdev->dev, "Failed to get resource: %d\n", ret);
		goto fail;
	}

	rtc->rtc_ck = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(rtc->rtc_ck)) {
		ret =  PTR_ERR(rtc->rtc_ck);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		goto fail;
	}

	ret = clk_prepare_enable(rtc->rtc_ck);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable clock: %d\n", ret);
		goto fail;
	}

	rtk_rtc_init(&pdev->dev);

	rtc->irq_alarm = platform_get_irq(pdev, 0);
	if (rtc->irq_alarm <= 0) {
		ret = rtc->irq_alarm;
		dev_err(&pdev->dev, "Failed to get IRQ: %d\n", ret);
		goto fail;
	}

	/* Handle RTC alarm interrupts */
	ret = devm_request_threaded_irq(&pdev->dev, rtc->irq_alarm, NULL,
									rtk_rtc_alarm_irq, IRQF_ONESHOT,
									pdev->name, rtc);
	if (ret) {
		dev_err(&pdev->dev, "IRQ%d (alarm interrupt) already claimed\n",
				rtc->irq_alarm);
		goto fail;
	}

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		device_init_wakeup(&pdev->dev, true);
		dev_pm_set_wake_irq(&pdev->dev, rtc->irq_alarm);
	}

	rtc->rtc_dev = devm_rtc_device_register(&pdev->dev, pdev->name,
											&rtk_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		ret = PTR_ERR(rtc->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n",
				ret);
		goto fail;
	}

	/*Set time to 1970-01-01 00:00:00*/
	rtc_time64_to_tm(0, &tm);
	rtk_rtc_set_time(&pdev->dev, &tm);

	return 0;

fail:
	return ret;
}


static const struct of_device_id rtk_rtc_of_match[] = {
	{ .compatible = "realtek,ameba-rtc",	},
	{ /* end node */ },
};


static struct platform_driver rtk_rtc_driver = {
	.probe	= rtk_rtc_probe,
	.driver	= {
		.name = "realtek-ameba-rtc",
		.of_match_table = rtk_rtc_of_match,
	},
};


builtin_platform_driver(rtk_rtc_driver);


MODULE_DESCRIPTION("Realtek Ameba RTC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
