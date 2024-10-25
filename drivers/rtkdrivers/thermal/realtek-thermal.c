// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Thermal support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/clk.h>

#include "thermal_core.h"
#include "thermal_hwmon.h"

#include "realtek-thermal.h"

/* Thermal configuration data */
static const struct realtek_thermal_info realtek_thermal_cfg = {
	.down_rate = 0,
	.clk_div = 1,
	.low_tmp_th = 0x1D8,
	.period = 15,
};

static irqreturn_t realtek_thermal_alarm_irq_thread(int irq, void *sdata)
{
	u32 value;
	struct realtek_thermal_data *thermal = sdata;

	spin_lock(&thermal->lock);

	/* Read IT reason in SR and clear flags */
	value = readl(thermal->base + RTK_TM_INTR_STS);

	if (value & TM_BIT_ISR_TM_LOW_WT) {
		dev_info(thermal->dev, "Low warning temperature detected\n");
	}

	if (value & TM_BIT_ISR_TM_HIGH_WT) {
		dev_info(thermal->dev, "High warning temperature detected\n");
	}

	writel(value, thermal->base + RTK_TM_INTR_STS);

	thermal_zone_device_update(thermal->th_dev, THERMAL_EVENT_UNSPECIFIED);

	spin_unlock(&thermal->lock);
	return IRQ_HANDLED;
}

/**
  * @brief  Enable or Disable the thermal peripheral.
  * @param  thermal: thermal driver data.
  * @param  state: state of the thermal.
  *   			This parameter can be: true or false.
  * @retval None
  */
static void realtek_thermal_cmd(struct realtek_thermal_data *thermal, bool state)
{
	u32 reg_value;

	reg_value = readl(thermal->base + RTK_TM_CTRL);
	if (state) {
		reg_value |= (TM_BIT_POWCUT | TM_BIT_POW | TM_BIT_RSTB);
	} else {
		reg_value &= ~(TM_BIT_POWCUT | TM_BIT_POW | TM_BIT_RSTB);
	}
	writel(reg_value, thermal->base + RTK_TM_CTRL);
}

/* Callback to get temperature from HW */
static int realtek_thermal_get_temp(void *data, int *temp)
{
	struct realtek_thermal_data *thermal = data;
	u32 temp_result;

	if (thermal->mode != THERMAL_DEVICE_ENABLED) {
		return -EAGAIN;
	}

	/* Retrieve temperature result */
	temp_result = TM_GET_OUT(readl(thermal->base + RTK_TM_RESULT));

	if (temp_result >= 0x40000) {
		*temp = -(int)(mcelsius(0x80000 - temp_result) >> 10);
	} else {
		*temp = (int)(mcelsius(temp_result) >> 10);
	}

	return 0;
}

#ifdef CONFIG_SOC_CPU_ARMA7
/**
  * @brief  Enable or Disable the thermal latch.
  * @param  thermal: thermal driver data.
  * @param  state: state of the thermal latch.
  *   			This parameter can be: true or false.
  * @retval None
  */
static void realtek_thermal_set_latch(struct realtek_thermal_data *thermal, bool state)
{
	u32 reg_value;

	reg_value = readl(thermal->base + RTK_TM_CTRL);
	if (state) {
		reg_value |= TM_BIT_EN_LATCH;
	} else {
		reg_value &= ~TM_BIT_EN_LATCH;
	}
	writel(reg_value, thermal->base + RTK_TM_CTRL);
}

static void realtek_thermal_init(struct realtek_thermal_data *thermal)
{
	u32 reg_value;

	realtek_thermal_cmd(thermal, false);
	/* Set thermal control register */
	reg_value = readl(thermal->base + RTK_TM_CTRL);
	reg_value &= ~TM_MASK_OSR;
	reg_value |= TM_OSR(realtek_thermal_cfg.down_rate);
	if (realtek_thermal_cfg.clk_div == TM_ADC_CLK_DIV_128) {
		reg_value |= TM_BIT_ADCCKSEL;
	} else {
		reg_value &= ~TM_BIT_ADCCKSEL;
	}
	reg_value |= TM_BIT_EN_LATCH;
	writel(reg_value, thermal->base + RTK_TM_CTRL);
	/* Set thermal threshold */
	reg_value = (TM_HIGH_PT_THR(thermal->temp_critical) |
				 TM_BIT_HIGHCMP_WT_EN |
				 TM_HIGH_WT_THR(thermal->temp_passive) |
				 TM_BIT_LOWCMP_WT_EN |
				 TM_LOW_WT_THR(realtek_thermal_cfg.low_tmp_th));
	writel(reg_value, thermal->base + RTK_TM_TH_CTRL);
	/* Max and min clear */
	reg_value = readl(thermal->base + RTK_TM_MAX_CTRL);
	reg_value |= TM_BIT_MAX_CLR;
	reg_value &= ~TM_BIT_MAX_CLR;
	writel(reg_value, thermal->base + RTK_TM_MAX_CTRL);
	reg_value = readl(thermal->base + RTK_TM_MIN_CTRL);
	reg_value |= TM_BIT_MIN_CLR;
	reg_value &= ~TM_BIT_MIN_CLR;
	writel(reg_value, thermal->base + RTK_TM_MIN_CTRL);
	/* Clear all interrupt */
	writel((TM_BIT_ISR_TM_LOW_WT | TM_BIT_ISR_TM_HIGH_WT), thermal->base + RTK_TM_INTR_STS);
	/* Set timer period */
	writel(TM_TIME_PERIOD(realtek_thermal_cfg.period), thermal->base + RTK_TM_TIMER);

	realtek_thermal_cmd(thermal, true);
	mdelay(500);
	realtek_thermal_set_latch(thermal, false);
	/* Enable interrupt */
	writel((TM_BIT_IMR_TM_HIGH_WT | TM_BIT_IMR_TM_LOW_WT), thermal->base + RTK_TM_INTR_CTRL);
}
#endif

#ifdef CONFIG_SOC_CPU_ARMA32
static void realtek_thermal_init(struct realtek_thermal_data *thermal)
{
	u32 reg_value;

	writel(0, thermal->base + RTK_TM_INTR_CTRL);
	realtek_thermal_cmd(thermal, false);
	/* Set thermal control register */
	reg_value = readl(thermal->base + RTK_TM_CTRL);
	reg_value &= ~TM_MASK_OSR;
	reg_value |= TM_OSR(realtek_thermal_cfg.down_rate);
	if (realtek_thermal_cfg.clk_div == TM_ADC_CLK_DIV_128) {
		reg_value |= TM_BIT_ADCCKSEL;
	} else {
		reg_value &= ~TM_BIT_ADCCKSEL;
	}

	/* Max and min clear */
	reg_value = readl(thermal->base + RTK_TM_MAX_CTRL);
	reg_value |= TM_BIT_MAX_CLR;
	reg_value &= ~TM_BIT_MAX_CLR;
	writel(reg_value, thermal->base + RTK_TM_MAX_CTRL);
	reg_value = readl(thermal->base + RTK_TM_MIN_CTRL);
	reg_value |= TM_BIT_MIN_CLR;
	reg_value &= ~TM_BIT_MIN_CLR;
	writel(reg_value, thermal->base + RTK_TM_MIN_CTRL);

	/* Set timer period */
	writel(TM_TIME_PERIOD(realtek_thermal_cfg.period), thermal->base + RTK_TM_TIMER);

	realtek_thermal_cmd(thermal, true);

	/* Clear all interrupt */
	writel((TM_BIT_ISR_TM_LOW_WT | TM_BIT_ISR_TM_HIGH_WT), thermal->base + RTK_TM_INTR_STS);
	/* Set thermal threshold */
	reg_value = (TM_HIGH_PT_THR(thermal->temp_critical) |
				 TM_BIT_HIGHCMP_WT_EN |
				 TM_HIGH_WT_THR(thermal->temp_passive) |
				 TM_BIT_LOWCMP_WT_EN |
				 TM_LOW_WT_THR(realtek_thermal_cfg.low_tmp_th));
	writel(reg_value, thermal->base + RTK_TM_TH_CTRL);
	writel((TM_BIT_IMR_TM_HIGH_WT | TM_BIT_IMR_TM_LOW_WT), thermal->base + RTK_TM_INTR_CTRL);
}
#endif

static const struct thermal_zone_of_device_ops realtek_tz_ops = {
	.get_temp	= realtek_thermal_get_temp,
};

static int realtek_thermal_probe(struct platform_device *pdev)
{
	struct realtek_thermal_data *thermal;
	struct resource *res;
	const struct thermal_trip *trip;
	int ret, i;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Invalid device node\n");
		return -EINVAL;
	}

	thermal = devm_kzalloc(&pdev->dev, sizeof(*thermal), GFP_KERNEL);
	if (!thermal) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, thermal);
	thermal->dev = &pdev->dev;
	spin_lock_init(&thermal->lock);

	/* Populate thermal */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	thermal->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(thermal->base)) {
		return PTR_ERR(thermal->base);
	}

	/* Register IRQ into GIC */
	thermal->irq = platform_get_irq(pdev, 0);
	if (thermal->irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return thermal->irq;
	}

	ret = devm_request_threaded_irq(&pdev->dev, thermal->irq, NULL, realtek_thermal_alarm_irq_thread,
									IRQF_ONESHOT, pdev->dev.driver->name, thermal);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register IRQ %d\n", thermal->irq);
		return ret;
	}

	thermal->th_dev = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, thermal, &realtek_tz_ops);
	if (IS_ERR(thermal->th_dev)) {
		dev_err(&pdev->dev, "Failed to register thermal\n");
		ret = PTR_ERR(thermal->th_dev);
		return ret;
	}

	if (!thermal->th_dev->ops->get_crit_temp) {
		/* Critical point must be provided */
		ret = -EINVAL;
		goto err_tz;
	}

	ret = thermal->th_dev->ops->get_crit_temp(thermal->th_dev, &thermal->temp_critical);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read critical_temp: %d\n", ret);
		goto err_tz;
	}

	thermal->temp_critical = celsius(thermal->temp_critical);

	trip = of_thermal_get_trip_points(thermal->th_dev);
	thermal->num_trips = of_thermal_get_ntrips(thermal->th_dev);

	/* Find out passive temperature if it exists */
	for (i = (thermal->num_trips - 1); i >= 0;  i--) {
		if (trip[i].type == THERMAL_TRIP_PASSIVE) {
			thermal->temp_passive = celsius(trip[i].temperature);
		}
	}

	thermal->atim_clk = devm_clk_get(&pdev->dev, "rtk_aon_tim_clk");
	if (IS_ERR(thermal->atim_clk)) {
		dev_err(&pdev->dev, "Failed to get AON timer clock\n");
		return PTR_ERR(thermal->atim_clk);
	}

	thermal->thm_clk = devm_clk_get(&pdev->dev, "rtk_thermal_clk");
	if (IS_ERR(thermal->thm_clk)) {
		dev_err(&pdev->dev, "Failed to get thermal clock\n");
		return PTR_ERR(thermal->thm_clk);
	}

	ret = clk_prepare_enable(thermal->atim_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable AON timer clock: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(thermal->thm_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable thermal clock: %d\n", ret);
		goto clk_fail;
	}

	/* Configure and enable HW thermal */
	realtek_thermal_init(thermal);

	/*
	 * Thermal_zone doesn't enable hwmon as default,
	 * enable it here
	 */
	thermal->th_dev->tzp->no_hwmon = false;
	ret = thermal_add_hwmon_sysfs(thermal->th_dev);
	if (ret) {
		goto err_tz;
	}

	thermal->mode = THERMAL_DEVICE_ENABLED;

	dev_info(&pdev->dev, "Initialized successfully\n");

	return 0;

err_tz:
	thermal_zone_of_sensor_unregister(&pdev->dev, thermal->th_dev);
	clk_disable_unprepare(thermal->thm_clk);
clk_fail:
	clk_disable_unprepare(thermal->atim_clk);
	return ret;
}

static int realtek_thermal_remove(struct platform_device *pdev)
{
	struct realtek_thermal_data *thermal = platform_get_drvdata(pdev);

	realtek_thermal_cmd(thermal, false);
	thermal_remove_hwmon_sysfs(thermal->th_dev);
	thermal_zone_of_sensor_unregister(&pdev->dev, thermal->th_dev);
	clk_disable_unprepare(thermal->thm_clk);
	clk_disable_unprepare(thermal->atim_clk);

	return 0;
}

static const struct of_device_id realtek_thermal_match[] = {
	{ .compatible = "realtek,ameba-thermal"},
	{ },
};
MODULE_DEVICE_TABLE(of, realtek_thermal_match);

static struct platform_driver realtek_thermal_driver = {
	.driver = {
		.name	= "realtek-ameba-thermal",
		.of_match_table = realtek_thermal_match,
	},
	.probe		= realtek_thermal_probe,
	.remove		= realtek_thermal_remove,
};

builtin_platform_driver(realtek_thermal_driver);

MODULE_DESCRIPTION("Realtek Ameba Thermal driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
