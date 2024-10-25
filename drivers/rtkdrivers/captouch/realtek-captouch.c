// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Captouch support
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
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/pm_wakeirq.h>
#include <linux/suspend.h>
#include <linux/clk-provider.h>
#include <linux/pinctrl/consumer.h>
#include <linux/nvmem-consumer.h>

#include "realtek-captouch.h"

struct realtek_captouch_data *captouch;

/**
  * @brief  Set CapTouch initialization parameters.
  * @param  captouch: CapTouch driver data
  * @param  ct_init: CapTouch initialization parameters.
  * @retval None
  */
static void realtek_captouch_para_set(struct realtek_ct_init_para *ct_init)
{
	ct_init->debounce_ena = 1;
	ct_init->sample_cnt = 6;
	ct_init->scan_interval = 60;
	ct_init->step = 1;
	ct_init->factor = 4;
	ct_init->etc_scan_interval = 3;

	memcpy((void *)ct_init->ch, (void *)captouch->ch_init, CT_CHANNEL_NUM * sizeof(struct realtek_ct_chinit_para));
}

/**
  * @brief  Check CapTouch initialization parameters.
  * @param  ct_init: CapTouch initialization parameters.
  * @retval Status:0-pass, -1-fail
  */
static int realtek_captouch_para_chk(struct realtek_ct_init_para *ct_init)
{
	u8 i;

	if (ct_init->debounce_ena > 1) {
		return -1;
	}
	if (ct_init->sample_cnt > 7) {
		return -1;
	}
	if (ct_init->scan_interval > 0xFFF) {
		return -1;
	}
	if (ct_init->step > 0xF) {
		return -1;
	}
	if (ct_init->factor > 0xF) {
		return -1;
	}
	if (ct_init->etc_scan_interval > 0x7F) {
		return -1;
	}

	for (i = 0; i < CT_CHANNEL_NUM ; i++) {
		if (ct_init->ch[i].diff_thr > 0xFFF) {
			return -1;
		}
		if (ct_init->ch[i].mbias_current > 0x3F) {
			return -1;
		}
		if (ct_init->ch[i].nnoise_thr > 0xFFF) {
			return -1;
		}
		if (ct_init->ch[i].pnoise_thr > 0xFFF) {
			return -1;
		}
		if (ct_init->ch[i].ch_ena > 1) {
			return -1;
		}
	}
	return 0;
}



/**
  * @brief  Enables or disables the specified CapTouch interrupts.
  * @param  captouch: captouch driver data.
  * @param  ct_int: specifies the CapTouch interrupt to be enabled or masked.
  *          This parameter can be one or combinations of the following values:
  *            @arg CT_BIT_GUARD_RELEASE_INTR_EN: CapTouch guard sensor release interrupt
  *            @arg CT_BIT_GUARD_PRESS_INTR_EN: CapTouch guard sensor press interrupt
  *            @arg CT_BIT_OVER_N_NOISE_TH_INTR_EN: CapTouch negetive noise overflow interrupt
  *            @arg CT_BIT_AFIFO_OVERFLOW_INTR_EN: CapTouch FIFO overflow interrupt
  *            @arg CT_BIT_OVER_P_NOISE_TH_INTR_EN:CapTouch positive noise overflow interrupt
  *            @arg CT_CHX_PRESS_INT(x): CapTouch channel(x) press interrupt, where x can be 0~7
  *            @arg CT_CHX_RELEASE_INT(x): CapTouch channel(x) release interrupt, where x can be 0~7
  * @param  state: state of the specified CapTouch interrupts mask.
  *   This parameter can be: true or false.
  * @retval None
  */
static void realtek_captouch_int_config(u32 ct_int, bool state)
{
	u32 reg_value;

	reg_value = readl(captouch->base + RTK_CT_INTERRUPT_ENABLE);
	if (state) {
		reg_value |= ct_int;
	} else {
		reg_value &= (~ct_int);
	}
	writel(reg_value, captouch->base + RTK_CT_INTERRUPT_ENABLE);
}

/**
  * @brief  Initializes the CapTouch peripheral.
  * @param  captouch: captouch driver data.
  * @param  ct_init: CapTouch initialization parameters.
  * @retval Init status
  */
static int realtek_captouch_init(struct realtek_ct_init_para *ct_init)
{
	u32 reg_value;
	int ret;
	u8 i;

	ret = realtek_captouch_para_chk(ct_init);
	if (ret) {
		return -1;
	}

	realtek_captouch_set_en(false);

	//clear pending interrupt
	writel(0, captouch->base + RTK_CT_INTERRUPT_ENABLE);
	writel(CT_BIT_INTR_ALL_CLR, captouch->base + RTK_CT_INTERRUPT_ALL_CLR);
	//set captouch control register
	reg_value = CT_BIT_BASELINE_INI;
	if (ct_init->debounce_ena == 1) {
		reg_value |= CT_BIT_DEBOUNCE_EN;
	}
	writel(reg_value, captouch->base + RTK_CT_CTC_CTRL);
	reg_value = CT_SAMPLE_AVE_CTRL(ct_init->sample_cnt) | ct_init->scan_interval;
	writel(reg_value, captouch->base + RTK_CT_SCAN_PERIOD);
	reg_value = CT_BASELINE_UPD_STEP(ct_init->step) | \
				CT_BASELINE_WT_FACTOR(ct_init->factor) | \
				CT_ETC_SCAN_INTERVAL(ct_init->etc_scan_interval) | \
				CT_BIT_ETC_FUNC_CTRL;
	writel(reg_value, captouch->base + RTK_CT_ETC_CTRL);
	writel(CT_BIT_CH_SWITCH_CTRL, captouch->base + RTK_CT_DEBUG_MODE_CTRL);

	reg_value = readl(captouch->path_base);
	/* Configure each channel */
	for (i = 0; i < CT_CHANNEL_NUM ; i++) {
		if (ct_init->ch[i].ch_ena) {
			writel((CT_CHx_D_TOUCH_TH(ct_init->ch[i].diff_thr) | CT_BIT_CHx_EN), captouch->base + RTK_CT_CHx_CTRL + i * 0x10);
			writel(CT_CHx_MBIAS(ct_init->ch[i].mbias_current), captouch->base + RTK_CT_CHx_MBIAS_ATH + i * 0x10);
			writel((CT_CHx_N_ENT(ct_init->ch[i].nnoise_thr) | CT_CHx_P_ENT(ct_init->ch[i].pnoise_thr)), captouch->base + RTK_CT_CHx_NOISE_TH + i * 0x10);
			reg_value &= ~(1 << i);
		}
	}
	// Disable digital path input for captouch
	writel(reg_value, captouch->path_base);

	realtek_captouch_set_en(true);

	return 0;
}

static irqreturn_t realtek_captouch_irq(int irq, void *dev_id)
{
	struct realtek_captouch_data *captouch = dev_id;
	u8 i;
	u32 IntStatus;

	spin_lock(&captouch->lock);

	IntStatus  = readl(captouch->base + RTK_CT_INTERRUPT_STATUS);

	for (i = 0; i < (CT_CHANNEL_NUM - 1); i++) {
		if (IntStatus & CT_CHX_PRESS_INT(i)) {
			dev_info(captouch->dev, "Key %d press\n", i);
			input_report_key(captouch->input, captouch->keycode[i], 1);
		} else if (IntStatus & CT_CHX_RELEASE_INT(i)) {
			dev_info(captouch->dev, "Key %d release\n", i);
			input_report_key(captouch->input, captouch->keycode[i], 0);
		}
	}

	if (IntStatus & CT_BIT_AFIFO_OVERFLOW_INTR) {
		dev_dbg(captouch->dev, "CT_BIT_AFIFO_OVERFLOW_INTR\n");
	}

	if (IntStatus & CT_BIT_OVER_P_NOISE_TH_INTR) {
		dev_dbg(captouch->dev, "CT_BIT_OVER_P_NOISE_TH_INTR\n");
		realtek_captouch_set_en(false);
		realtek_captouch_set_en(true);

		goto exit;
	}

	if (IntStatus & CT_BIT_GUARD_PRESS_INTR) {
		dev_dbg(captouch->dev, "Guard sensor press\n");
		input_report_key(captouch->input, captouch->keycode[CT_CHANNEL_NUM - 1], 1);
	}

	if (IntStatus & CT_BIT_GUARD_RELEASE_INTR) {
		dev_dbg(captouch->dev, "Guard sensor release\n");
		input_report_key(captouch->input, captouch->keycode[CT_CHANNEL_NUM - 1], 0);
	}

exit:
	input_sync(captouch->input);
	writel(IntStatus, captouch->base + RTK_CT_INTERRUPT_STATUS_CLR);
	spin_unlock(&captouch->lock);

	return IRQ_HANDLED;
}

static int realtek_captouch_get_calibration(struct device *dev) {
	int ret;
	struct nvmem_cell *cell;
	unsigned char *efuse_buf;
	u32 i;

	cell = nvmem_cell_get(dev, "ctc_mbias");
	if (IS_ERR(cell)) {
		dev_err(dev, "Fail to get captouch efuse cell.\n");
		return -EINVAL;
	}

	efuse_buf = nvmem_cell_read(cell, &ret);
	nvmem_cell_put(cell);

	if (IS_ERR(efuse_buf)) {
		dev_err(dev, "Fail to read captouch efuse cell.\n");
		return -EINVAL;
	}

	for (i = 0; i < CT_CHANNEL_NUM; i++) {
		if (efuse_buf[i] > 0 && efuse_buf[i] < 64 && (captouch->ch_init[i].ch_ena == 1)) {
			captouch->ch_init[i].mbias_current = (u8)efuse_buf[i];
		} else {
			if (efuse_buf[i] != 0xff) {
				dev_err(dev, "Bad captouch calibration value [%d]=%d.\n", i, efuse_buf[i]);
			}
		}
	}

	return 0;
}

static int realtek_captouch_pase_dt(struct device *dev)
{
	struct device_node *node = dev->of_node;
	int ret;
	u32 val[CT_CHANNEL_NUM];
	u32 i;

	ret = of_property_count_u32_elems(node, "rtk,ctc-diffthr");
	if (ret > CT_CHANNEL_NUM) {
		dev_err(dev, "Bad captouch diff-threshold number\n");
		return -EINVAL;
	}
	if (of_property_read_u32_array(node, "rtk,ctc-diffthr", val, ret)) {
		dev_err(dev, "Read captouch diff-threshold fail\n");
		return -EINVAL;
	}
	for (i = 0; i < ret; i++) {
		captouch->ch_init[i].diff_thr = (u16)val[i];
	}

	ret = of_property_count_u32_elems(node, "rtk,ctc-mbias");
	if (ret > CT_CHANNEL_NUM) {
		dev_err(dev, "Bad captouch mbias number\n");
		return -EINVAL;
	}
	if (of_property_read_u32_array(node, "rtk,ctc-mbias", val, ret)) {
		dev_err(dev, "Read captouch mbias fail\n");
		return -EINVAL;
	}
	for (i = 0; i < ret; i++) {
		captouch->ch_init[i].mbias_current = (u8)val[i];
	}

	ret = of_property_count_u32_elems(node, "rtk,ctc-nnoise");
	if (ret > CT_CHANNEL_NUM) {
		dev_err(dev, "Bad captouch n-noise number\n");
		return -EINVAL;
	}
	if (of_property_read_u32_array(node, "rtk,ctc-nnoise", val, ret)) {
		dev_err(dev, "Read captouch n-noise fail\n");
		return -EINVAL;
	}
	for (i = 0; i < ret; i++) {
		captouch->ch_init[i].nnoise_thr = (u16)val[i];
	}

	ret = of_property_count_u32_elems(node, "rtk,ctc-pnoise");
	if (ret > CT_CHANNEL_NUM) {
		dev_err(dev, "Bad captouch p-noise number\n");
		return -EINVAL;
	}
	if (of_property_read_u32_array(node, "rtk,ctc-pnoise", val, ret)) {
		dev_err(dev, "Read captouch p-noise fail\n");
		return -EINVAL;
	}
	for (i = 0; i < ret; i++) {
		captouch->ch_init[i].pnoise_thr = (u16)val[i];
	}

	ret = of_property_count_u32_elems(node, "rtk,ctc-ch-status");
	if (ret > CT_CHANNEL_NUM) {
		dev_err(dev, "Bad captouch channel enable state number\n");
		return -EINVAL;
	}
	if (of_property_read_u32_array(node, "rtk,ctc-ch-status", val, ret)) {
		dev_err(dev, "Read captouch channel enable state fail\n");
		return -EINVAL;
	}
	for (i = 0; i < ret; i++) {
		captouch->ch_init[i].ch_ena = (u8)val[i];
	}

	ret = of_property_count_u32_elems(node, "rtk,ctc-keycodes");
	if (ret > CT_CHANNEL_NUM) {
		dev_err(dev, "Bad captouch keycode number\n");
		return -EINVAL;
	}
	if (of_property_read_u32_array(node, "rtk,ctc-keycodes", captouch->keycode, ret)) {
		dev_err(dev, "Read captouch keycode fail\n");
		return -EINVAL;
	}

	return 0;
}

static int realtek_captouch_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct realtek_ct_init_para *ct_init;
	u8 i;
	int ret;
	const struct clk_hw *hwclk;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Device tree node not found\n");
		return -EINVAL;
	}

	ct_init = devm_kzalloc(&pdev->dev, sizeof(*ct_init), GFP_KERNEL);
	if (!ct_init) {
		return -ENOMEM;
	}

	captouch = devm_kzalloc(&pdev->dev, sizeof(*captouch), GFP_KERNEL);
	if (!captouch) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, captouch);
	captouch->dev = &pdev->dev;

	spin_lock_init(&captouch->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	captouch->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(captouch->base)) {
		return PTR_ERR(captouch->base);
	}

	captouch->adc_clk = devm_clk_get(&pdev->dev, "rtk_adc_clk");
	if (IS_ERR(captouch->adc_clk)) {
		dev_err(&pdev->dev, "Failed to get ADC clock\n");
		return PTR_ERR(captouch->adc_clk);
	}

	captouch->ctc_clk = devm_clk_get(&pdev->dev, "rtk_ctc_clk");
	if (IS_ERR(captouch->ctc_clk)) {
		dev_err(&pdev->dev, "Failed to get CTC clock\n");
		return PTR_ERR(captouch->ctc_clk);
	}

	ret = clk_prepare_enable(captouch->adc_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable ADC clock %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(captouch->ctc_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable CTC clock %d\n", ret);
		goto clk_fail;
	}

	captouch->path_base = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(captouch->path_base)) {
		goto err_fail;
	}

	captouch->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(captouch->pinctrl)) {
		dev_err(&pdev->dev, "Pinctrl not defined\n");
		goto map_fail;
	} else {
		captouch->ctc_state_active  = pinctrl_lookup_state(captouch->pinctrl, PINCTRL_STATE_DEFAULT);
		if (IS_ERR_OR_NULL(captouch->ctc_state_active)) {
			dev_err(&pdev->dev, "Failed to lookup pinctrl default state %d\n", ret);
			goto map_fail;
		}
		captouch->ctc_state_sleep = pinctrl_lookup_state(captouch->pinctrl, PINCTRL_STATE_SLEEP);
		if (IS_ERR_OR_NULL(captouch->ctc_state_sleep)) {
			dev_err(&pdev->dev, "Failed to lookup pinctrl sleep state %d\n", ret);
			goto map_fail;
		}
	}

	ret = realtek_captouch_pase_dt(&pdev->dev);
	if (ret < 0) {
		goto map_fail;
	}

	ret = realtek_captouch_get_calibration(&pdev->dev);
	if (ret < 0) {
		goto map_fail;
	}

	captouch->input = devm_input_allocate_device(&pdev->dev);
	if (!captouch->input) {
		ret = -ENOMEM;
		goto map_fail;
	}

	captouch->input->name = pdev->name;
	captouch->input->phys = "realtek-captouch/input0";
	captouch->input->id.bustype = BUS_HOST;
	captouch->input->id.vendor = 0x0001;
	captouch->input->id.product = 0x0001;
	captouch->input->id.version = 0x0100;

	__set_bit(EV_KEY, captouch->input->evbit);
	for (i = 0; i < CT_CHANNEL_NUM; i++) {
		__set_bit(captouch->keycode[i], captouch->input->keybit);
	}

	input_set_drvdata(captouch->input, captouch);


	/* Register IRQ into GIC */
	captouch->irq = platform_get_irq(pdev, 0);
	if (captouch->irq < 0) {
		dev_err(&pdev->dev, "Unable to find IRQ\n");
		ret = -EBUSY;
		goto map_fail;
	}

	ret = devm_request_irq(&pdev->dev, captouch->irq, realtek_captouch_irq, 0, pdev->dev.driver->name, captouch);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register IRQ %d\n", captouch->irq);
		goto map_fail;
	}

	realtek_captouch_para_set(ct_init);
	ret = realtek_captouch_init(ct_init);
	if (ret) {
		dev_err(&pdev->dev, "Failed to init captouch: parameter error\n");
		goto map_fail;
	}
	realtek_captouch_int_config(CT_ALL_INT_EN, true);
	realtek_captouch_int_config(CT_BIT_SCAN_END_INTR_EN, false);

	ret = input_register_device(captouch->input);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register input device\n");
		goto map_fail;
	}

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		device_init_wakeup(&pdev->dev, true);
		dev_pm_set_wake_irq(&pdev->dev, captouch->irq);
	}

	captouch->clk_sl = clk_get_parent(captouch->ctc_clk);
	hwclk = __clk_get_hw(captouch->clk_sl);
	captouch->ctc_parent_osc_131k = clk_hw_get_parent_by_index(hwclk, 0)->clk;
	captouch->ctc_parent_ls_apb = clk_hw_get_parent_by_index(hwclk, 1)->clk;

	return 0;

map_fail:
	iounmap(captouch->path_base);
err_fail:
	clk_disable_unprepare(captouch->ctc_clk);
clk_fail:
	clk_disable_unprepare(captouch->adc_clk);
	return ret;
}

static int realtek_captouch_remove(struct platform_device *pdev)
{
	struct realtek_captouch_data *captouch = platform_get_drvdata(pdev);

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		dev_pm_clear_wake_irq(&pdev->dev);
		device_init_wakeup(&pdev->dev, false);
	}

	clk_disable_unprepare(captouch->adc_clk);
	clk_disable_unprepare(captouch->ctc_clk);
	iounmap(captouch->path_base);
	input_unregister_device(captouch->input);
	realtek_captouch_set_en(false);

	return 0;
}

#ifdef CONFIG_PM
static int realtek_ctc_suspend(struct device *dev)
{
	struct realtek_captouch_data *captouch = dev_get_drvdata(dev);
	int ret;

	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		ret = pinctrl_select_state(captouch->pinctrl, captouch->ctc_state_sleep);
		if (ret) {
			dev_err(dev, "Failed to select suspend state\n");
		}
		clk_set_parent(captouch->clk_sl, captouch->ctc_parent_osc_131k);
	}
	return 0;
}

static int realtek_ctc_resume(struct device *dev)
{
	struct realtek_captouch_data *captouch = dev_get_drvdata(dev);
	int ret;

	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		clk_set_parent(captouch->clk_sl, captouch->ctc_parent_ls_apb);
		ret = pinctrl_select_state(captouch->pinctrl, captouch->ctc_state_active);
		if (ret) {
			dev_err(dev, "Failed to select active state\n");
		}
	}
	return 0;
}

static const struct dev_pm_ops realtek_ameba_ctc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(realtek_ctc_suspend, realtek_ctc_resume)
};
#endif

static const struct of_device_id realtek_captouch_match[] = {
	{ .compatible = "realtek,ameba-captouch"},
	{ }
};
MODULE_DEVICE_TABLE(of, realtek_captouch_match);

static struct platform_driver realtek_captouch_driver = {
	.driver = {
		.name	= "realtek-ameba-captouch",
		.of_match_table = realtek_captouch_match,
#ifdef CONFIG_PM
		.pm = &realtek_ameba_ctc_pm_ops,
#endif
		.dev_groups = captouch_configs,
	},
	.probe		= realtek_captouch_probe,
	.remove		= realtek_captouch_remove,
};

builtin_platform_driver(realtek_captouch_driver);

MODULE_DESCRIPTION("Realtek Ameba Captouch driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
