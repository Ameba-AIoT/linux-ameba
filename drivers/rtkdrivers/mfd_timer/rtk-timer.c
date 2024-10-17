// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Timer support
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
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/mfd/rtk-timer.h>
#include <linux/pm_wakeirq.h>

#define TIMER_DISABLE		0
#define TIMER_ENABLE		1

#define TIMER_IDLE			2
#define TIMER_TAKEN			3

#define RTK_NR_TIMERS		10

static u8 timer_manage[RTK_NR_TIMERS] = {
	TIMER_TAKEN, TIMER_TAKEN, TIMER_DISABLE,
	TIMER_DISABLE, TIMER_DISABLE, TIMER_DISABLE,
	TIMER_DISABLE, TIMER_DISABLE, TIMER_TAKEN, TIMER_TAKEN
};
spinlock_t	lock;

static struct rtk_tim gtimer[RTK_NR_TIMERS];

static int dynamic_allocate_timer(void)
{
	int i;

	spin_lock(&lock);
	/* Only dynamically allocate timers between 2~7. */
	for (i = 2; i < RTK_NR_TIMERS - 2; i++) {
		if (timer_manage[i] == TIMER_IDLE) {
			timer_manage[i] = TIMER_TAKEN;
			spin_unlock(&lock);
			return i;
		}
	}
	spin_unlock(&lock);
	return -1;
}

static int is_timer_invalid(u32 index)
{
	struct rtk_tim *tim;

	if (index > (RTK_NR_TIMERS - 1)) {
		return -1;
	}

	tim = &gtimer[index];
	if (tim->valid != 1) {
		return -1;
	}

	return 0;
}


static irqreturn_t rtk_gtimer_irq(int irq, void *dev_id)
{
	struct rtk_tim *tim = (struct rtk_tim *)dev_id;

	rtk_gtimer_int_clear(tim->index);
	pm_wakeup_event(tim->dev, 0);

	if (tim->intr_handler) {
		tim->intr_handler(tim->cbdata);
	}

	return IRQ_HANDLED;
}

/**
  * @brief  Change timer period.
  * @param  index: timer index, can be 0~13.
  * @param  period_ns: timer preiod and its unit is nanosecond.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_change_period(u32 index, u64 period_ns)
{
	u32 reg, arr, psc;
	u64 div, prd;
	struct rtk_tim *tim;
	void __iomem *base;

	if (is_timer_invalid(index)) {
		return  -ENODEV;
	}

	tim = &gtimer[index];
	base = tim->base;

	/* Calculate arr & psc*/
	div = (u64)tim->clk_rate * period_ns;
	do_div(div, NSEC_PER_SEC);

	arr = (u32)div;
	psc = 0;

	if (8 == index || 9 == index) {
		/* Period and prescaler values depends on clock rate */
		div = (unsigned long long)tim->clk_rate * period_ns;

		/* Start to compute arr and prescaler*/
		do_div(div, NSEC_PER_SEC);
		prd = div;

		/* Max count is UINT32_MAX because REG_TIM_CNT is 32bit*/
		while (div > U16_MAX) {
			psc++;
			div = prd;
			do_div(div, psc + 1);
		}

		prd = div;
		arr = prd - 1;

		/* Prescaler is 16bit*/
		if (psc > U16_MAX || prd > U16_MAX) {
			return -EINVAL;
		}
	}

	/* Reset the ARR Preload Bit */
	reg = readl_relaxed(base + REG_TIM_CR);
	reg &= ~TIM_BIT_ARPE;
	writel_relaxed(reg, base + REG_TIM_CR);

	/* Set ARR*/
	writel_relaxed(arr, base + REG_TIM_ARR);

	if (psc != 0) {
		writel_relaxed(psc, base + REG_TIM_PSC);
	}

	/* Generate an update event */
	writel_relaxed(TIM_PSCReloadMode_Immediate, base + REG_TIM_EGR);

	/* Poll EGR UG done */
	while (1) {
		if (readl_relaxed(base + REG_TIM_SR) & TIM_BIT_UG_DONE) {
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(rtk_gtimer_change_period);

/**
  * @brief  Start or stop timer(counter will not reset to 0).
  * @param  index: timer index, can be 0~13.
  * @param  NewState: can be 0 or 1, indicate start and stop counter.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_start(u32 index, u32 NewState)
{
	u32 reg;
	struct rtk_tim *tim;
	void __iomem *base;

	if (is_timer_invalid(index)) {
		return -ENODEV;
	}

	tim = &gtimer[index];
	base = tim->base;

	if (NewState != TIMER_DISABLE) {
		/* Enable the TIM Counter, dont do this if timer is RUN */
		reg = readl_relaxed(base + REG_TIM_EN);
		if ((readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) == 0) {
			writel_relaxed(TIM_BIT_CNT_START, base + REG_TIM_EN);
		}

		/* Poll if cnt is running, 3*32k cycles */
		while (1) {
			if (readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) {
				break;
			}
		}
	} else {
		/* Disable the TIM Counter, dont do this if timer is not RUN */
		/* This action need sync to 32k domain for 100us */
		if (readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) {
			writel_relaxed(TIM_BIT_CNT_STOP, base + REG_TIM_EN);
		}

		/* Poll if cnt is running, aout 100us */
		while (1) {
			if ((readl_relaxed(base + REG_TIM_EN) & TIM_BIT_CEN) == 0) {
				break;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(rtk_gtimer_start);

/**
  * @brief  Clear interrupt flag.
  * @param  index: timer index, can be 0~13.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_int_clear(u32 index)
{
	u32 CounterIndex = 0;
	u32 reg;
	struct rtk_tim *tim;
	void __iomem *base;

	if (is_timer_invalid(index)) {
		return  -ENODEV;
	}

	tim = &gtimer[index];
	base = tim->base;

	/* Clear the all IT pending Bits */
	reg = readl_relaxed(base + REG_TIM_SR);
	writel_relaxed(reg, base + REG_TIM_SR);

	/* Make sure write ok, because bus delay */
	while (1) {
		CounterIndex++;
		if (CounterIndex >= 300) {
			break;
		}

		if (((readl_relaxed(base + REG_TIM_SR)) & 0xFFFFFF) == 0) {
			break;
		}
	}

	return 0;
}

EXPORT_SYMBOL(rtk_gtimer_int_clear);

/**
  * @brief  Enable or disable timer update interrupt.
  * @param  index: timer index, can be 0~13.
  * @param  NewState: can be 0 or 1, indicate enable and disable interrupt.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_int_config(u32 index, u32 NewState)
{
	u32 reg;
	struct rtk_tim *tim;
	void __iomem *base;

	if (is_timer_invalid(index)) {
		return  -ENODEV;
	}

	tim = &gtimer[index];
	base = tim->base;

	reg = readl_relaxed(base + REG_TIM_DIER);
	if (NewState != TIMER_DISABLE) {
		reg |= TIM_IT_Update;
		writel_relaxed(reg, base + REG_TIM_DIER);
	} else {
		reg &= ~TIM_IT_Update;
		writel_relaxed(reg, base + REG_TIM_DIER);
	}

	return 0;
}

EXPORT_SYMBOL(rtk_gtimer_int_config);



/**
  * @brief  Deinitilize timer.
  * @param  index: timer index, can be 0~13.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_deinit(u32 index)
{
	struct rtk_tim *tim;

	if (is_timer_invalid(index)) {
		return  -ENODEV;
	}

	tim = &gtimer[index];

	rtk_gtimer_start(index, TIMER_DISABLE);
	tim->intr_handler = NULL;
	tim->cbdata = NULL;

	spin_lock(&lock);
	timer_manage[index] = TIMER_IDLE;
	spin_unlock(&lock);

	return 0;
}

EXPORT_SYMBOL(rtk_gtimer_deinit);

/**
  * @brief  Initilize timer.
  * @param  index: timer index, can be 0~13.
  * @param  period_ns: timer preiod and its unit is nanosecond.
  * @param  cbhandler: callback handler registered.
  * @param  cbdata: callback data of handler.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_init(u32 index, u64 period_ns, void *cbhandler, void *cbdata)
{
	u32 reg, arr, psc;
	u64 div, prd;
	struct rtk_tim *tim;
	void __iomem *base;

	if (is_timer_invalid(index)) {
		return  -ENODEV;
	}

	spin_lock(&lock);
	tim = &gtimer[index];
	timer_manage[index] = TIMER_TAKEN;
	spin_unlock(&lock);
	base = tim->base;

	/* Calculate arr & psc*/
	div = (u64)tim->clk_rate * period_ns;
	do_div(div, NSEC_PER_SEC);

	arr = (u32)div;
	psc = 0;

	if (8 == index || 9 == index) {
		/* Period and prescaler values depends on clock rate */
		div = (u64)tim->clk_rate * period_ns;

		/* Start to compute arr and prescaler*/
		do_div(div, NSEC_PER_SEC);
		prd = div;

		/* Max count is UINT32_MAX because REG_TIM_CNT is 32bit*/
		while (div > U16_MAX) {
			psc++;
			div = prd;
			do_div(div, psc + 1);
		}

		prd = div;
		arr = prd - 1;

		/* Prescaler is 16bit*/
		if (psc > U16_MAX || prd > U16_MAX) {
			return -EINVAL;
		}
	}

	/* Disble timer*/
	rtk_gtimer_start(index, TIMER_DISABLE);

	/* Disable interrupt*/
	writel_relaxed(0, base + REG_TIM_DIER);

	/* Clear all pending bits*/
	reg = readl_relaxed(base + REG_TIM_SR);
	writel_relaxed(reg, base + REG_TIM_SR);
	reg = readl_relaxed(base + REG_TIM_SR);

	/* Set ARR*/
	writel_relaxed(arr, base + REG_TIM_ARR);

	if (psc != 0) {
		writel_relaxed(psc, base + REG_TIM_PSC);
	}

	/* Set CR*/
	reg = readl_relaxed(base + REG_TIM_CR);
	reg |= TIM_BIT_ARPE | TIM_BIT_URS;
	reg &= ~TIM_BIT_UDIS;
	writel_relaxed(reg, base + REG_TIM_CR);

	/* Generate an update event*/
	writel_relaxed(TIM_PSCReloadMode_Immediate, base + REG_TIM_EGR);

	while (1) {
		if (readl_relaxed(base + REG_TIM_SR) & TIM_BIT_UG_DONE) {
			break;
		}
	}

	/* Clear all flags*/
	reg = readl_relaxed(base + REG_TIM_SR);
	writel_relaxed(reg, base + REG_TIM_SR);

	tim->intr_handler = cbhandler;
	tim->cbdata = cbdata;

	return 0;
}

EXPORT_SYMBOL(rtk_gtimer_init);

int rtk_gtimer_dynamic_init(u64 period_ns, void *cbhandler, void *cbdata)
{
	u32 reg, arr, psc;
	u64 div;
	int ret;
	struct rtk_tim *tim;
	void __iomem *base;
	u32 index;

	ret = dynamic_allocate_timer();
	if (ret < 0) {
		return  -ENODEV;
	} else {
		index = ret;
	}

	tim = &gtimer[index];
	base = tim->base;

	/* Calculate arr & psc*/
	div = (u64)tim->clk_rate * period_ns;
	do_div(div, NSEC_PER_SEC);

	arr = (u32)div;
	psc = 0;

	/* Disble timer*/
	rtk_gtimer_start(index, TIMER_DISABLE);

	/* Disable interrupt*/
	writel_relaxed(0, base + REG_TIM_DIER);

	/* Clear all pending bits*/
	reg = readl_relaxed(base + REG_TIM_SR);
	writel_relaxed(reg, base + REG_TIM_SR);
	reg = readl_relaxed(base + REG_TIM_SR);

	/* Set ARR*/
	writel_relaxed(arr, base + REG_TIM_ARR);

	if (psc != 0) {
		writel_relaxed(psc, base + REG_TIM_PSC);
	}

	/* Set CR*/
	reg = readl_relaxed(base + REG_TIM_CR);
	reg |= TIM_BIT_ARPE | TIM_BIT_URS;
	reg &= ~TIM_BIT_UDIS;
	writel_relaxed(reg, base + REG_TIM_CR);

	/* Generate an update event*/
	writel_relaxed(TIM_PSCReloadMode_Immediate, base + REG_TIM_EGR);

	while (1) {
		if (readl_relaxed(base + REG_TIM_SR) & TIM_BIT_UG_DONE) {
			break;
		}
	}

	/* Clear all flags*/
	reg = readl_relaxed(base + REG_TIM_SR);
	writel_relaxed(reg, base + REG_TIM_SR);

	tim->intr_handler = cbhandler;
	tim->cbdata = cbdata;

	return index;
}
EXPORT_SYMBOL(rtk_gtimer_dynamic_init);

static int rtk_gtimer_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct rtk_tim *tim;
	int ret;

	if (pdev->dev.of_node) {
		pdev->id = of_alias_get_id(pdev->dev.of_node, "tim");
	} else {
		dev_err(&pdev->dev, "Invalid node\n");
		return -ENODEV;
	}

	if (pdev->id < 0 || pdev->id >= RTK_NR_TIMERS) {
		dev_err(&pdev->dev, "Wrong device id\n");
		return -EINVAL;
	}

	tim = &gtimer[pdev->id];
	timer_manage[pdev->id] = TIMER_IDLE;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tim->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tim->base)) {
		return PTR_ERR(tim->base);
	}

	tim->tim_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(tim->tim_clk)) {
		ret =  PTR_ERR(tim->tim_clk);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(tim->tim_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable clock: %d\n", ret);
		return ret;
	}

	tim->irq = platform_get_irq(pdev, 0);
	if (tim->irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return tim->irq;
	}

	tim->dev = &pdev->dev;
	ret = request_irq(tim->irq, (irq_handler_t) rtk_gtimer_irq, 0, pdev->name, tim);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		return ret;
	}

	tim->valid = 1;
	tim->index = pdev->id;

	if (0 <= pdev->id && 7 >= pdev->id) {
		tim->clk_rate = 32768;
	} else if (8 <= pdev->id && 9 >= pdev->id) {
		tim->clk_rate = 40000000;
	} else {
		tim->clk_rate = 1000000;
	}

	platform_set_drvdata(pdev, tim);

	if (pdev->id == 8) {
		ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to populate DT children\n");
			return ret;
		}
	}
	dev_info(&pdev->dev, "Timer %d init done, clock rate is %d\n",pdev->id, tim->clk_rate);

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		device_init_wakeup(&pdev->dev, true);
		dev_pm_set_wake_irq(&pdev->dev, tim->irq);
	}

	spin_lock_init(&lock);

	return 0;
}

static void timer_cb_handler(void *cbdata)
{
	struct rtk_tim *tim = (struct rtk_tim *) cbdata;

	if (tim->mode == 1) {
		rtk_gtimer_start(tim->index, 0);
	}
}

static ssize_t mode_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t size)
{
	struct rtk_tim *tim = dev->driver_data;

	if (tim->valid != 1) {
		dev_err(dev, "This timer is not valid\n");
		return -1;
	}

	if (sysfs_streq(buf, "oneshot")) {
		tim->mode = 1;
	} else if (sysfs_streq(buf, "periodic")) {
		tim->mode = 0;
	} else {
		return -EINVAL;
	}

	return size;
}

static ssize_t mode_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	struct rtk_tim *tim = dev->driver_data;
	const char *polarity = "unknown";

	if (tim->valid != 1) {
		dev_err(dev,"This timer is not valid\n");
		return -1;
	}

	switch (tim->mode) {
	case 0:
		polarity = "periodic";
		break;

	case 1:
		polarity = "oneshot";
		break;
	}

	return sprintf(buf, "%s\n", polarity);
}

static ssize_t enable_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t size)
{
	struct rtk_tim *tim = dev->driver_data;
	int ret = 0;
	u32 val;

	if (tim->valid != 1) {
		dev_err(dev, "This timer is not valid\n");
		return -1;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}

	if (val != 0) {
		tim->enable = 1;
		if (tim->period == 0) {
			tim->period = 1000000;
		}
		rtk_gtimer_init(tim->index, (u64) tim->period * 1000000, timer_cb_handler, tim);
		rtk_gtimer_int_config(tim->index, 1);
		rtk_gtimer_start(tim->index, 1);
	} else {
		tim->enable = 0;
		rtk_gtimer_deinit(tim->index);
	}

	return size;
}

static ssize_t enable_show(struct device *dev,
						   struct device_attribute *attr,
						   char *buf)
{
	struct rtk_tim *tim = dev->driver_data;

	if (tim->valid != 1) {
		dev_err(dev, "This timer is not valid\n");
		return -1;
	}

	return sprintf(buf, "%u\n", tim->enable);;
}

static ssize_t time_ms_store(struct device *dev,
							 struct device_attribute *attr,
							 const char *buf, size_t size)
{
	struct rtk_tim *tim = dev->driver_data;

	int val;
	int ret = 0;

	if (tim->valid != 1) {
		dev_err(dev, "This timer is not valid\n");
		return -1;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		return ret;
	}

	tim->period = val;

	if (tim->enable == 1) {
		rtk_gtimer_change_period(tim->index, (u64) tim->period * 1000000);
	};

	return size;
}

static ssize_t time_ms_show(struct device *dev,
							struct device_attribute *attr,
							char *buf)
{
	struct rtk_tim *tim = dev->driver_data;

	if (tim->valid != 1) {
		dev_err(dev, "This timer is not valid\n");
		return -1;
	}

	return sprintf(buf, "%u\n", tim->period);
}


DEVICE_ATTR_RW(mode);
DEVICE_ATTR_RW(time_ms);
DEVICE_ATTR_RW(enable);


static struct attribute *timer_config_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_time_ms.attr,
	&dev_attr_enable.attr,
	NULL,
};


ATTRIBUTE_GROUPS(timer_config);

static const struct of_device_id rtk_timer_of_match[] = {
	{ .compatible = "realtek,ameba-timer",	},
	{ /* end node */ },
};

static struct platform_driver rtk_timer_driver = {
	.probe	= rtk_gtimer_probe,
	.driver	= {
		.name = "realtek-ameba-timer",
		.of_match_table = rtk_timer_of_match,
		.dev_groups = timer_config_groups,
	},
};


builtin_platform_driver(rtk_timer_driver);

MODULE_DESCRIPTION("Realtek Ameba Timer driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
