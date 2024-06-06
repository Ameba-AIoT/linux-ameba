// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ADC comparator support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/mfd/rtk-timer.h>
#include <linux/clk.h>
#include <linux/pm_wakeirq.h>
#include <linux/suspend.h>

#include "realtek-comparator.h"

/* comparator configuration data */
static const struct realtek_comp_priv realtek_comp_priv_info = {
	.mode = COMP_AUTO_MODE,
	.timer_idx = 1,
	.period = 100000000,
};

static const struct realtek_comp_info realtek_comp_cfg[] = {
	{
		.wake_type = (COMP_WK_SYS | COMP_WK_ADC),
		.wake_sys_ctrl = 2,
		.wake_adc_ctrl = 2,
	},
	{
		.wake_type = (COMP_WK_SYS | COMP_WK_ADC),
		.wake_sys_ctrl = 2,
		.wake_adc_ctrl = 2,
	},
	{
		.wake_type = (COMP_WK_SYS | COMP_WK_ADC),
		.wake_sys_ctrl = 2,
		.wake_adc_ctrl = 2,
	},
	{
		.wake_type = (COMP_WK_SYS | COMP_WK_ADC),
		.wake_sys_ctrl = 2,
		.wake_adc_ctrl = 2,
	},
};

/**
  * realtek_comp_cmd - Enable or Disable the comparator peripheral.
  * @param  comparator: comparator driver data.
  * @param  state: state of the comparator.
  *   			This parameter can be: true or false.
  * @retval None
  */
static void realtek_comp_cmd(struct realtek_comp_data *comparator, bool state)
{
	u32 reg_value;

	reg_value = readl(comparator->base + RTK_COMP_EN_TRIG);
	if (state) {
		reg_value |= COMP_BIT_ENABLE;
	} else {
		reg_value &= ~COMP_BIT_ENABLE;
	}
	writel(reg_value, comparator->base + RTK_COMP_EN_TRIG);
}

/**
  * realtek_comp_swtrig_cmd - Enabled or disabled software-trigger channel switch in Comparator Software-Trigger Mode.
  * @param  comparator: comparator driver data.
  * @param  state: can be one of the following value:
  *			@arg true: Enable the software-trigger channel switch. When setting this bit, do one channel switch in the channel switch list.
  *			@arg false: Disable the software-trigger channel switch.
  * @retval  None.
  */
static void realtek_comp_swtrig_cmd(struct realtek_comp_data *comparator, bool state)
{
	u32 reg_val;

	reg_val = readl(comparator->base + RTK_COMP_EN_TRIG);
	if (state) {
		reg_val |= COMP_BIT_EN_TRIG;
	} else {
		reg_val &= ~COMP_BIT_EN_TRIG;
	}
	writel(reg_val, comparator->base + RTK_COMP_EN_TRIG);
}

/**
  * realtek_comp_auto_cmd - Controls the automatic channel switch enabled or disabled.
  * @param  comparator: comparator driver data.
  * @param  state: can be one of the following value:
  *		@arg true: Enable the automatic channel switch.
  *			When setting this bit, an automatic channel switch starts from the first channel in the channel switch list.
  *		@arg false:  Disable the automatic channel switch.
  *			If an automatic channel switch is in progess, writing 0 will terminate the automatic channel switch.
  * @retval  None.
  * @note  Used in Automatic Mode
  */
static void realtek_comp_auto_cmd(struct realtek_comp_data *comparator, bool state)
{
	u32 reg_val;

	reg_val = readl(comparator->base + RTK_COMP_EN_TRIG);
	if (state) {
		writel(COMP_BIT_AUTOSW_EN, comparator->base + RTK_COMP_AUTOSW_EN);
		reg_val |= COMP_BIT_EN_TRIG;
	} else {
		writel(0, comparator->base + RTK_COMP_AUTOSW_EN);
		reg_val &= ~COMP_BIT_EN_TRIG;
	}
	writel(reg_val, comparator->base + RTK_COMP_EN_TRIG);
}

/**
  * realtek_comp_timtrig_cmd - Initialize the trigger timer when in comparator Timer-Trigger Mode.
  * @param  comparator: comparator driver data.
  * @param  Tim_Idx: The timer index would be used to make comparator module do a conversion.
  * @param  PeriodNs: Indicate the period of trigger timer.
  * @param  state: can be one of the following value:
  *			@arg true: Enable the comparator timer trigger mode.
  *			@arg false: Disable the comparator timer trigger mode.
  * @retval  None.
  * @note  Used in Timer-Trigger Mode
  */
static void realtek_comp_timtrig_cmd(struct realtek_comp_data *comparator, u8 Tim_Idx, u64 PeriodNs, bool state)
{
	writel(Tim_Idx, comparator->base + RTK_COMP_EXT_TRIG_TIMER_SEL);

	if (state) {
		rtk_gtimer_init(Tim_Idx, PeriodNs, NULL, NULL);
		rtk_gtimer_start(Tim_Idx, state);
		writel(COMP_BIT_EXT_WK_TIMER, comparator->base + RTK_COMP_EXT_TRIG_CTRL);
	} else {
		rtk_gtimer_start(Tim_Idx, state);
		//rtk_gtimer_deinit(Tim_Idx);	// timer deinit outside IRQ Context
		writel(0, comparator->base + RTK_COMP_EXT_TRIG_CTRL);
	}
}

static irqreturn_t realtek_comp_isr(int irq, void *data)
{
	struct realtek_comp_data *comparator = data;
	u32 status = readl(comparator->base + RTK_COMP_WK_STS);
	u16 adc_data;

	spin_lock(&comparator->lock);

	if (status & COMP_MASK_WK_STS) {
		while ((ADC_GET_FLR(readl(comparator->adc->base + RTK_ADC_FLR))) == 0);
		adc_data = (u16)ADC_GET_DATA_GLOBAL(readl(comparator->adc->base + RTK_ADC_DATA_GLOBAL));
		dev_dbg(comparator->dev, "INT status = 0x%08X, ADC sample data = 0x%04X\n", status, adc_data);
		writel(status, comparator->base + RTK_COMP_WK_STS);
		realtek_adc_clear_fifo(comparator->adc);

		spin_unlock(&comparator->lock);
		return IRQ_HANDLED;
	}

	spin_unlock(&comparator->lock);
	return IRQ_NONE;
}

static void realtek_comp_start(struct realtek_comp_data *comparator)
{
	if (realtek_comp_priv_info.mode == COMP_AUTO_MODE) {
		realtek_comp_auto_cmd(comparator, true);
	} else if (realtek_comp_priv_info.mode == COMP_TIM_TRI_MODE) {
		realtek_comp_timtrig_cmd(comparator, realtek_comp_priv_info.timer_idx, realtek_comp_priv_info.period, true);
	} else {
		realtek_comp_swtrig_cmd(comparator, true);
		while (1) {
			mdelay(200);
			realtek_comp_swtrig_cmd(comparator, false);
			mdelay(100);
			realtek_comp_swtrig_cmd(comparator, true);
		}
	}
}

static void realtek_comp_hw_init(struct realtek_comp_data *comparator)
{
	u32 reg_value, i;
	u32 val_ch = 0;

	realtek_adc_cmd(comparator->adc, false);
	reg_value = readl(comparator->adc->base + RTK_ADC_CONF);
	reg_value &= ~(ADC_MASK_OP_MOD);
	reg_value |= ADC_OP_MOD(ADC_COMP_ASSIST_MODE);
	writel(reg_value, comparator->adc->base + RTK_ADC_CONF);
	realtek_adc_cmd(comparator->adc, true);

	realtek_comp_cmd(comparator, false);
	//set comparator mode and channel list len
	reg_value = readl(comparator->base + RTK_COMP_INTR_CTRL);
	for (i = 0; i < COMP_CH_NUM; i++) {
		val_ch |= (u32)(i << COMP_SHIFT_CHSW(i));
		writel(COMP_REF0_CHx(comparator->ref0) | COMP_REF1_CHx(comparator->ref1), comparator->base + RTK_COMP_REF_CHx + i * 0x4);

		if (realtek_comp_cfg[i].wake_type & COMP_WK_ADC) {
			reg_value &= ~ COMP_MASK_WK_ADC_CTRL(i);
			reg_value |= (realtek_comp_cfg[i].wake_adc_ctrl << COMP_SHIFT_WK_ADC_CTRL(i)) | COMP_WK_ADC_EN(i);
		}

		if (realtek_comp_cfg[i].wake_type & COMP_WK_SYS) {
			reg_value &= ~ COMP_MASK_WK_SYS_CTRL(i);
			reg_value |= (realtek_comp_cfg[i].wake_sys_ctrl << COMP_SHIFT_WK_SYS_CTRL(i)) | COMP_WK_SYS_EN(i);
		}
	}
	writel(reg_value, comparator->base + RTK_COMP_INTR_CTRL);
	// set channel switch list
	writel(val_ch, comparator->base + RTK_COMP_CHSW_LIST);
	// set lpsd
	writel(COMP_BIT_LPSD_1, comparator->base + RTK_COMP_LPSD);
	// set auto shut
	writel(COMP_BIT_AUTO_SHUT, comparator->base + RTK_COMP_AUTO_SHUT);
	realtek_comp_cmd(comparator, true);
}

static void realtek_comp_hw_deinit(struct realtek_comp_data *comparator)
{
	writel(0, comparator->base + RTK_COMP_INTR_CTRL);
	writel(COMP_MASK_WK_STS, comparator->base + RTK_ADC_INTR_STS);
	realtek_comp_cmd(comparator, false);
}

static int realtek_comp_probe(struct platform_device *pdev)
{
	struct realtek_comp_data *comparator;
	int ret;

	if (!pdev->dev.of_node) {
		return -ENODEV;
	}

	comparator = devm_kzalloc(&pdev->dev, sizeof(*comparator), GFP_KERNEL);
	if (!comparator) {
		return -ENOMEM;
	}

	comparator->adc = dev_get_drvdata(pdev->dev.parent);
	comparator->dev = &pdev->dev;

	comparator->base = comparator->adc->comp_base;

	spin_lock_init(&comparator->lock);
	platform_set_drvdata(pdev, comparator);

	comparator->irq = platform_get_irq(pdev, 0);
	if (comparator->irq < 0) {
		return comparator->irq;
	}

	ret = devm_request_irq(&pdev->dev, comparator->irq, realtek_comp_isr, 0, pdev->name, comparator);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		return ret;
	}

	if (of_property_read_u32(pdev->dev.of_node, "rtk,cmp-ref0", &comparator->ref0)) {
		dev_err(&pdev->dev, "Comparator mode property not found\n");
		return -EINVAL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "rtk,cmp-ref1", &comparator->ref1)) {
		dev_err(&pdev->dev, "Comparator mode property not found\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "ADC comparator mode ref0 = 0x%08X, ref1 = 0x%08X\n", comparator->ref0, comparator->ref1);

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		device_init_wakeup(&pdev->dev, true);
		dev_pm_set_wake_irq(&pdev->dev, comparator->irq);
	}

	realtek_comp_hw_init(comparator);
	realtek_comp_start(comparator);

	return 0;
}

static int realtek_comp_remove(struct platform_device *pdev)
{
	struct realtek_comp_data *comparator = platform_get_drvdata(pdev);

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		dev_pm_clear_wake_irq(&pdev->dev);
		device_init_wakeup(&pdev->dev, false);
	}

	realtek_comp_hw_deinit(comparator);

	return 0;
}

#ifdef CONFIG_PM
static int realtek_comp_suspend(struct device *dev)
{
	struct realtek_adc_data *comp_adc = dev_get_drvdata(dev->parent);

	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		clk_set_parent(comp_adc->clk_sl, comp_adc->adc_parent_osc_2m);
	}
	return 0;
}

static int realtek_comp_resume(struct device *dev)
{
	struct realtek_adc_data *comp_adc = dev_get_drvdata(dev->parent);

	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		clk_set_parent(comp_adc->clk_sl, comp_adc->adc_parent_ls_apb);
	}
	return 0;
}

static const struct dev_pm_ops realtek_ameba_comp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(realtek_comp_suspend, realtek_comp_resume)
};
#endif

static const struct of_device_id realtek_comp_match[] = {
	{.compatible = "realtek,ameba-comparator",},
	{},
};
MODULE_DEVICE_TABLE(of, realtek_comp_match);

static struct platform_driver realtek_comp_driver = {
	.probe	= realtek_comp_probe,
	.remove	= realtek_comp_remove,
	.driver = {
		.name = "realtek-ameba-comparator",
		.of_match_table = of_match_ptr(realtek_comp_match),
#ifdef CONFIG_PM
		.pm = &realtek_ameba_comp_pm_ops,
#endif
	},
};

builtin_platform_driver(realtek_comp_driver);

MODULE_DESCRIPTION("Realtek Ameba ADC comparator driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
