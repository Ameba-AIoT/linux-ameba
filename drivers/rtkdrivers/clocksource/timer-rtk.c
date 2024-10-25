// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Clocksource support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/mfd/rtk-timer.h>

#include "timer-of.h"

#define TIMER_CLK_DISABLE 0
#define TIMER_CLK_ENABLE 1

/*change timer period*/
static void rtk_timer_change_period(void __iomem *base, u32 period)
{
	u32 reg;

	/* Reset the ARR Preload Bit */
	reg = readl_relaxed(base + REG_TIM_CR);
	reg &= ~TIM_BIT_ARPE;
	writel_relaxed(reg, base + REG_TIM_CR);

	/* Set the AAR */
	writel_relaxed(period, base + REG_TIM_ARR);

	/* Generate an update event */
	writel_relaxed(TIM_PSCReloadMode_Immediate, base + REG_TIM_EGR);

	/* poll EGR UG done */
	while (1) {
		if (readl_relaxed(base + REG_TIM_SR) & TIM_BIT_UG_DONE) {
			break;
		}
	}

}

/*start or stop timer.(counter will not reset to 0)*/
static void rtk_timer_start(void __iomem *base, u32 NewState)
{
	u32 reg;

	if (NewState != TIMER_CLK_DISABLE) {
		/* Enable the TIM Counter, dont do this if timer is RUN */
		reg = readl_relaxed(base + REG_TIM_EN);
		if ((readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) == 0) {
			writel_relaxed(TIM_BIT_CNT_START, base + REG_TIM_EN);
		}

		/* poll if cnt is running, 3*32k cycles */
		while (1) {
			if (readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) {
				break;
			}
		}
	} else {
		/* Disable the TIM Counter, dont do this if timer is not RUN */
		/* this action need sync to 32k domain for 100us */
		if (readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) {
			writel_relaxed(TIM_BIT_CNT_STOP, base + REG_TIM_EN);
		}

		/* poll if cnt is running, aout 100us */
		while (1) {
			if ((readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) == 0) {
				break;
			}
		}
	}
}



/*enable/disabler interrupt*/
static void rtk_timer_int_config(void __iomem *base, u32 TIM_IT, u32 NewState)
{
	u32 reg;

	reg = readl_relaxed(base + REG_TIM_DIER);
	if (NewState != TIMER_CLK_DISABLE) {
		reg |= TIM_IT;
		writel_relaxed(reg, base + REG_TIM_DIER);
	} else {
		reg &= ~TIM_IT;
		writel_relaxed(reg, base + REG_TIM_DIER);
	}
}

/*initilize timer and start count*/
void rtk_timer_init(void __iomem *base)
{
	u32 reg;

	/*disble timer*/
	rtk_timer_start(base, TIMER_CLK_DISABLE);

	/*disable interrupt*/
	writel_relaxed(0, base + REG_TIM_DIER);

	/*clear all pending bits*/
	reg = readl_relaxed(base + REG_TIM_SR);
	writel_relaxed(reg, base + REG_TIM_SR);
	reg = readl_relaxed(base + REG_TIM_SR);

	/*set ARR to the max value*/
	writel_relaxed(UINT_MAX, base + REG_TIM_ARR);

	/*set CR*/
	reg = readl_relaxed(base + REG_TIM_CR);
	reg |= TIM_BIT_ARPE; 		//period will update immediatly
	reg |= TIM_BIT_URS;			//set URS bit
	reg &= ~TIM_BIT_UDIS;		//Set the Update Disable Bit
	writel_relaxed(reg, base + REG_TIM_CR);

	/*Generate an update event*/
	writel_relaxed(TIM_PSCReloadMode_Immediate, base + REG_TIM_EGR);

	while (1) {
		if (readl_relaxed(base + REG_TIM_SR) & TIM_BIT_UG_DONE) {
			break;
		}
	}

	/* Clear all flags*/
	reg = readl_relaxed(base + REG_TIM_SR);
	writel_relaxed(reg, base + REG_TIM_SR);

	/*start counter*/
	rtk_timer_start(base, TIMER_CLK_ENABLE);

}


static int rtk_clock_event_shutdown(struct clock_event_device *clkevt)
{
	struct timer_of *to = to_timer_of(clkevt);

	rtk_timer_int_config(timer_of_base(to), TIM_IT_Update, TIMER_CLK_DISABLE);
	rtk_timer_start(timer_of_base(to), TIMER_CLK_DISABLE);

	return 0;
}


static int rtk_clock_event_set_next_event(unsigned long evt,
		struct clock_event_device *clkevt)
{
	struct timer_of *to = to_timer_of(clkevt);

	rtk_timer_change_period(timer_of_base(to), evt);
	rtk_timer_start(timer_of_base(to), TIMER_CLK_ENABLE);
	rtk_timer_int_config(timer_of_base(to), TIM_IT_Update, TIMER_CLK_ENABLE);

	return 0;
}


static int rtk_clock_event_set_periodic(struct clock_event_device *clkevt)
{
	struct timer_of *to = to_timer_of(clkevt);

	rtk_timer_init(timer_of_base(to));

	return rtk_clock_event_set_next_event(timer_of_period(to), clkevt);
}


static int rtk_clock_event_set_oneshot(struct clock_event_device *clkevt)
{
	struct timer_of *to = to_timer_of(clkevt);

	rtk_timer_init(timer_of_base(to));

	return 0;
}


static irqreturn_t rtk_clock_event_handler(int irq, void *dev_id)
{
	u32 reg;
	struct clock_event_device *clkevt = (struct clock_event_device *)dev_id;
	struct timer_of *to = to_timer_of(clkevt);

	/*clear all pending bits*/
	reg = readl_relaxed(timer_of_base(to) + REG_TIM_SR);
	writel_relaxed(reg, timer_of_base(to) + REG_TIM_SR);
	reg = readl_relaxed(timer_of_base(to) + REG_TIM_SR);

	if (!clockevent_state_periodic(clkevt)) {
		rtk_clock_event_shutdown(clkevt);
	}

	clkevt->event_handler(clkevt);

	return IRQ_HANDLED;
}


static struct timer_of rtk_to = {

	.flags = TIMER_OF_IRQ | TIMER_OF_BASE | TIMER_OF_CLOCK,
	.clkevt = {
		.rating			= 100,
		.features		= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
		.set_state_periodic		 = rtk_clock_event_set_periodic,
		.set_state_shutdown	= rtk_clock_event_shutdown,
		.set_state_oneshot	= rtk_clock_event_set_oneshot,
		.set_next_event		= rtk_clock_event_set_next_event,
	},
	.of_irq = {
		.handler		= rtk_clock_event_handler,
	},

};


static int __init rtk_timer_probe(struct device_node *node)
{
	struct timer_of *to = &rtk_to;
	int ret;

	to->clkevt.cpumask = cpumask_of(0);
	to->of_clk.rate = 32768;

	ret = timer_of_init(node, to);
	if (ret) {
		goto err;
	}


	/*basic timer is not suitable as a clocksource but a clockevent*/
	clockevents_config_and_register(&to->clkevt, 32768, 0x1, 0xffffffff);

	return 0;

err:
	timer_of_cleanup(to);
	return ret;
}

TIMER_OF_DECLARE(rtk_timer, "realtek,ameba-timer-clk", rtk_timer_probe);

