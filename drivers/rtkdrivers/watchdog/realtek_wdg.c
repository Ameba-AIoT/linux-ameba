// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Watchdog support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>

enum RTK_WDG_DEVICES {
	IWDG_DEV,
	WDG1_DEV,
	WDG2_DEV,
	WDG3_DEV,
	WDG4_DEV,
	WDG5_DEV,
};

/* Registers Definitions --------------------------------------------------------*/
/**************************************************************************//**
 * @defgroup WDG_Register_Definitions WDG Register Definitions
 * @{
 *****************************************************************************/
#define WDG_MKEYR					0x000
#define WDG_CR						0x004
#define WDG_RLR						0x008
#define WDG_WINR					0x00C

/**************************************************************************//**
 * @defgroup WDG_REG
 * @{
 *****************************************************************************/
#define WDG_BIT_ENABLE				((u32)0x00000001 << 16)
#define WDG_BIT_CLEAR				((u32)0x00000001 << 24)
#define WDG_BIT_RST_MODE			((u32)0x00000001 << 30)
#define WDG_BIT_ISR_CLEAR			((u32)0x00000001 << 31)
/** @} */
/** @} */

/**************************************************************************//**
 * @defgroup WDG_MKEYR
 * @brief WDG Magic Key register
 * @{
 *****************************************************************************/
#define WDG_MASK_MKEY				((u32)0x0000FFFF << 0)          /*!<R/WPD 0h  0x6969: enable access to register WDG_CR/WDG_RLR/WDG_WINR 0x5A5A: reload WDG counter 0x3C3C: enable WDG function */
#define WDG_MKEY(x)					((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_MKEY(x)				((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup WDG_CR
 * @brief WDG Control regsietr
 * @{
 *****************************************************************************/
#define WDG_BIT_RVU					((u32)0x00000001 << 31)          /*!<R 0h  Watchdog counter update by reload value */
#define WDG_BIT_EVU					((u32)0x00000001 << 30)          /*!<R 0h  Watchdog early interrupt function update */
#define WDG_BIT_LPEN				((u32)0x00000001 << 24)          /*!<R/WE 0h  Low power enable 0: WDG will gating when system goes into sleep mode 1: WDG keep running when system goes into sleep mode */
#define WDG_BIT_EIC					((u32)0x00000001 << 17)          /*!<WA0 0h  Write '1' clear the early interrupt */
#define WDG_BIT_EIE					((u32)0x00000001 << 16)          /*!<R/WE 0h  Watchdog early interrupt enable */
#define WDG_MASK_early_int_cnt		((u32)0x0000FFFF << 0)          /*!<R/WE 0h  Early interrupt trigger threshold */
#define WDG_early_int_cnt(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_early_int_cnt(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup WDG_RLR
 * @brief WDG Relaod register
 * @{
 *****************************************************************************/
#define WDG_MASK_PRER				((u32)0x000000FF << 16)          /*!<R/WE 63h  Prescaler counter, configuration only allowed before wdg enable WDG: 0x63 System wdg: 0x1F */
#define WDG_PRER(x)					((u32)(((x) & 0x000000FF) << 16))
#define WDG_GET_PRER(x)				((u32)(((x >> 16) & 0x000000FF)))
#define WDG_MASK_RELOAD				((u32)0x0000FFFF << 0)          /*!<R/WE FFFh  Reload value for watchdog counter */
#define WDG_RELOAD(x)				((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_RELOAD(x)			((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup WDG_WINR
 * @brief WDG window Register
 * @{
 *****************************************************************************/
#define WDG_MASK_WINDOW				((u32)0x0000FFFF << 0)          /*!<R/WE FFFFh  Watchdog feed protect window register */
#define WDG_WINDOW(x)				((u32)(((x) & 0x0000FFFF) << 0))
#define WDG_GET_WINDOW(x)			((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/** @defgroup WDG_Peripheral_definitions
  * @{
  */
#define IS_WDG_ALL_PERIPH(PERIPH) 	(((PERIPH) == IWDG_DEV) ||((PERIPH) == WDG1_DEV) ||((PERIPH) == WDG2_DEV) ||((PERIPH) == WDG3_DEV) ||((PERIPH) == WDG4_DEV))
#define IS_IWDG_PERIPH(PERIPH) 		((PERIPH) == IWDG_DEV)
/**
  * @}
  */

/** @defgroup WDG_magic_key_define
  * @{
  */
#define WDG_ACCESS_EN				0x00006969
#define WDG_FUNC_EN					0x00003C3C
#define WDG_REFRESH					0x00005A5A

/**
  * @brief  WDG Init structure definition
  */
struct rtk_wdg_params {
	u16 window_ms; 			/*!< WDG parameter specifies window protection of WDG
					  This parameter must be set to a value in the 0-65535 range */

	u16 timeout; 			/*!< WDG parameter specifies WDG timeout count in units of second
					  This parameter must be set to a value in the 0-65 range */

	u16 max_timeout_ms; 	/*!< WDG parameter specifies WDG max timeout count in units of ms
					  This parameter must be set to a value in the 1-65535 range */

	u16 early_int_cnt_ms; 	/*!< WDG parameter specifies WDG early interrupt trigger threshold
					  This parameter must be set to a value in the 1-65535 range */

	u16 early_int_mode;		/*!< WDG parameter, Specifies WDG early interrupt enable or not
					  This parameter must be set to a value of 0 or 1 */
};

struct rtk_wdg {
	struct watchdog_device		wdd;
	void __iomem				*base;
	struct device				*dev;
	int							irq;
	int							wdg_index;
	struct rtk_wdg_params		wdg_params;
};

static void rtk_wdg_writel(
	void __iomem *ptr, u32 reg, u32 value)
{
	writel(value, (void __iomem *)((u32)ptr + reg));
}

static u32 rtk_wdg_readl(void __iomem *ptr, u32 reg)
{
	return readl((void __iomem *)((u32)ptr + reg));
}

static void rtk_wdg_reg_update(
	void __iomem *ptr,
	u32 reg, u32 mask, u32 value)
{
	u32 temp;

	temp = rtk_wdg_readl(ptr, reg);
	temp |= mask;
	temp &= (~mask | value);
	rtk_wdg_writel(ptr, reg, temp);
}

static struct rtk_wdg *to_rtk_wdg(struct watchdog_device *wdd)
{
	return container_of(wdd, struct rtk_wdg, wdd);
}

static void rtk_wdg_wait_busy_check(struct rtk_wdg *wdg)
{
	u32 times = 0;

	/* Wait for no update event */
	while (rtk_wdg_readl(wdg->base, WDG_CR) & (WDG_BIT_RVU | WDG_BIT_EVU)) {
		times++;
		if (times > 1000) {
			dev_warn(wdg->dev, "Check busy timeout CR = 0x%08X\n", rtk_wdg_readl(wdg->base, WDG_CR) & (WDG_BIT_RVU | WDG_BIT_EVU));
			break;
		}
	}
}

static void rtk_wdg_struct_init(struct rtk_wdg *wdg, struct device_node *np)
{
	int nr_requests, ret;

	/* Load HAL initial data structure default value from DTS. */

	ret = of_property_read_u32(np, "rtk,wdg-index", &nr_requests);
	if (ret) {
		wdg->wdg_index = 1;
		dev_warn(wdg->dev, "No rtk,wdg-index property specified in DTS, set it to %d as default\n", wdg->wdg_index);
	} else {
		wdg->wdg_index = nr_requests;
	}

	if (!IS_WDG_ALL_PERIPH(wdg->wdg_index)) {
		wdg->wdg_index = 1;
		dev_warn(wdg->dev, "Invalid rtk,wdg-index %d specified in DTS, set it to %d as default\n", wdg->wdg_index);
	}

	ret = of_property_read_u32(np, "rtk,wdg-max-timeout-ms", &nr_requests);
	if (ret) {
		wdg->wdg_params.max_timeout_ms = 0xFFFF;
		dev_warn(wdg->dev, "No rtk,wdg-max-timeout property specified in DTS, set it to 0x%04X as default\n", wdg->wdg_params.max_timeout_ms);
	} else {
		wdg->wdg_params.max_timeout_ms = nr_requests;
	}

	ret = of_property_read_u32(np, "rtk,wdg-timeout", &nr_requests);
	if (ret) {
		wdg->wdg_params.timeout = wdg->wdg_params.max_timeout_ms / 1000;
		dev_warn(wdg->dev, "No rtk,wdg-timeout property specified in DTS, set it to 0x%04X as default\n", wdg->wdg_params.timeout);
	} else {
		wdg->wdg_params.timeout = nr_requests;
	}

	ret = of_property_read_u32(np, "rtk,wdg-window-protection-ms", &nr_requests);
	if (ret) {
		wdg->wdg_params.window_ms = 0xFFFF;
		dev_warn(wdg->dev, "No rtk,wdg-window-protection property specified in DTS, set it to 0x%04X as default\n", wdg->wdg_params.window_ms);
	} else {
		wdg->wdg_params.window_ms = nr_requests;
	}

	ret = of_property_read_u32(np, "rtk,wdg-int-trigger-threshold-ms", &nr_requests);
	if (ret) {
		wdg->wdg_params.early_int_cnt_ms = 0;
		dev_warn(wdg->dev, "No rtk,wdg-int-trigger-threshold property specified in DTS, set it to 0x%04X as default\n", wdg->wdg_params.early_int_cnt_ms);
	} else {
		wdg->wdg_params.early_int_cnt_ms = nr_requests;
	}

	ret = of_property_read_u32(np, "rtk,wdg-interrupt-mode", &nr_requests);
	if (ret) {
		wdg->wdg_params.early_int_mode = 0;
		dev_warn(wdg->dev, "No rtk,wdg-interrupt-mode property specified in DTS, set it to 0x%04X as default\n", wdg->wdg_params.early_int_mode);
	} else {
		wdg->wdg_params.early_int_mode = nr_requests;
	}
}

static void rtk_wdg_init_hw(struct rtk_wdg *wdg)
{
	u32 prescaler = 0;
	u32 timeout;

	if (IS_IWDG_PERIPH(wdg->wdg_index)) {
		prescaler = 0x63;
	} else {
		prescaler = 0x1F;
	}

	timeout = clamp_t(unsigned int, wdg->wdg_params.timeout * 1000, 0, wdg->wdg_params.max_timeout_ms);

	rtk_wdg_wait_busy_check(wdg);

	/* Enable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, WDG_ACCESS_EN);

	if (wdg->wdg_params.early_int_mode) {
		rtk_wdg_writel(wdg->base, WDG_CR, WDG_BIT_EIE | WDG_early_int_cnt(wdg->wdg_params.early_int_cnt_ms));
	} else {
		rtk_wdg_writel(wdg->base, WDG_CR, 0);
	}

	rtk_wdg_writel(wdg->base, WDG_RLR, WDG_PRER(prescaler) | WDG_RELOAD(timeout));
	rtk_wdg_writel(wdg->base, WDG_WINR, wdg->wdg_params.window_ms);

	/* Disable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, 0xFFFF);

}

static void rtk_wdg_enable(struct rtk_wdg *wdg)
{
	rtk_wdg_wait_busy_check(wdg);
	rtk_wdg_writel(wdg->base, WDG_MKEYR, WDG_FUNC_EN);
}

static void rtk_wdg_refresh(struct rtk_wdg *wdg)
{
	rtk_wdg_wait_busy_check(wdg);
	rtk_wdg_writel(wdg->base, WDG_MKEYR, WDG_REFRESH);
}

static void rtk_wdg_interrupt_config(
	struct rtk_wdg *wdg,
	u32 wdg_interrupt,
	u32 new_state)
{
	rtk_wdg_wait_busy_check(wdg);

	/* Enable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, WDG_ACCESS_EN);

	if (new_state == 1) {
		rtk_wdg_reg_update(wdg->base, WDG_CR, WDG_BIT_EIE, wdg_interrupt);
	} else {
		rtk_wdg_reg_update(wdg->base, WDG_CR, WDG_BIT_EIE, ~wdg_interrupt);
	}

	/* Disable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, 0xFFFF);
}

static void rtk_wdg_clear_interrupt(struct rtk_wdg *wdg, u32 interrupt_bit)
{
	rtk_wdg_wait_busy_check(wdg);

	/* Enable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, WDG_ACCESS_EN);

	/* Clear ISR */
	rtk_wdg_reg_update(wdg->base, WDG_CR, WDG_BIT_EIC, interrupt_bit);

	/* Disable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, 0xFFFF);
}

static irqreturn_t rtk_wdg_isr_event(int irq, void *data)
{
	struct rtk_wdg *wdg = data;

	rtk_wdg_refresh(wdg);
	rtk_wdg_clear_interrupt(wdg, WDG_BIT_EIC);
	dev_dbg(wdg->dev, "WDG%d early interrupt\n", wdg->wdg_index);

	return IRQ_HANDLED;
}

static int rtk_wdg_start(struct watchdog_device *wdd)
{
	struct rtk_wdg *wdg = to_rtk_wdg(wdd);

	if (wdg->wdg_params.early_int_mode) {
		rtk_wdg_interrupt_config(wdg, WDG_BIT_EIE, 1);
	}

	rtk_wdg_enable(wdg);

	return 0;
}

static int rtk_wdg_stop(struct watchdog_device *wdd)
{
	struct rtk_wdg *wdg = to_rtk_wdg(wdd);
	dev_info(wdg->dev, "Stop watchdog is not supported by realtek-wdg\n");

	return 0;
}

/* Feed watchdog. */
static int rtk_wdg_ping(struct watchdog_device *wdd)
{
	struct rtk_wdg *wdg = to_rtk_wdg(wdd);

	rtk_wdg_refresh(wdg);

	return 0;
}

static int rtk_wdg_set_timeout(struct watchdog_device *wdd,
							   unsigned int timeout)
{
	struct rtk_wdg *wdg = to_rtk_wdg(wdd);
#ifdef RTK_WDG_SUPPORT
	u32 actual; // seconds
	u32 prescaler = 0;
#endif

	dev_info(wdg->dev, "RTK-WDG timeout-value cannot be refreshed once the WDG is enabled\n");
	dev_info(wdg->dev, "If needed, set watchdog timeout rtk,wdg-timeout in DTS\n");

#ifdef RTK_WDG_SUPPORT
	dev_dbg(wdg->dev, "Try to set watchdog timeout to %d\n", timeout);

	actual = clamp_t(unsigned int, timeout, wdd->min_timeout, wdd->max_hw_heartbeat_ms / 1000);

	dev_dbg(wdg->dev, "Watchdog timeout actually set to %d\n", actual);

	wdd->timeout = actual;

	if (IS_IWDG_PERIPH(wdg->wdg_index)) {
		prescaler = 0x63;
	} else {
		prescaler = 0x1F;
	}

	rtk_wdg_wait_busy_check(wdg);

	/* Enable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, WDG_ACCESS_EN);

	rtk_wdg_writel(wdg->base, WDG_RLR, WDG_PRER(prescaler) | WDG_RELOAD(actual * 1000));

	/* Disable Register access */
	rtk_wdg_writel(wdg->base, WDG_MKEYR, 0xFFFF);
#endif

	return 0;
}

static int rtk_wdg_restart(struct watchdog_device *wdd,
						   unsigned long action, void *data)
{
	struct rtk_wdg *wdg = to_rtk_wdg(wdd);

	dev_info(wdg->dev, "Restart watchdog is not supported by realtek-wdg, restart will only refresh the watchdog\n");

	rtk_wdg_refresh(wdg);
	rtk_wdg_clear_interrupt(wdg, WDG_BIT_EIC);

	return 0;
}

static const struct watchdog_ops rtk_wdg_ops = {
	.start		 = rtk_wdg_start,
	.stop		 = rtk_wdg_stop,
	.ping		 = rtk_wdg_ping,
	.set_timeout = rtk_wdg_set_timeout,
	.restart	 = rtk_wdg_restart,
	.owner		 = THIS_MODULE,
};

static const struct watchdog_info rtk_wdg_info = {
	.options	= WDIOF_KEEPALIVEPING,
	.identity	= KBUILD_MODNAME,
};

static const struct of_device_id rtk_wdg_of_table[] = {
	{.compatible = "realtek,ameba-watchdog"},
	{},
};

MODULE_DEVICE_TABLE(of, rtk_wdg_of_table);

static int rtk_wdg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_wdg *wdg;
	struct device_node *np;
	int ret;
	const struct of_device_id *of_id = NULL;

	of_id = of_match_device(rtk_wdg_of_table, &pdev->dev);
	if (!of_id || strcmp(of_id->compatible, rtk_wdg_of_table->compatible)) {
		return -EINVAL;
	}

	wdg = devm_kzalloc(dev, sizeof(*wdg), GFP_KERNEL);
	if (!wdg) {
		return -ENOMEM;
	}

	wdg->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdg->base)) {
		return PTR_ERR(wdg->base);
	}

	wdg->dev = &pdev->dev;
	np = dev->of_node;

	wdg->wdd.info = &rtk_wdg_info;
	wdg->wdd.ops = &rtk_wdg_ops;
	wdg->wdd.parent = dev;
	rtk_wdg_struct_init(wdg, np);

	wdg->wdd.timeout = wdg->wdg_params.timeout;

	wdg->wdd.max_hw_heartbeat_ms = wdg->wdg_params.max_timeout_ms;

	if (wdg->wdg_params.early_int_mode) {
		wdg->irq = platform_get_irq(pdev, 0);
		ret = devm_request_irq(&pdev->dev, wdg->irq, rtk_wdg_isr_event, 0, dev_name(&pdev->dev), wdg);
	}

	watchdog_init_timeout(&wdg->wdd, 0, dev);
	rtk_wdg_init_hw(wdg); /* Init to reset mode */

	dev_set_drvdata(dev, wdg);

	dev_info(dev, "Init with timeout %ds\n", wdg->wdd.timeout);

	return devm_watchdog_register_device(dev, &wdg->wdd);
}

static struct platform_driver rtk_watchdog_driver = {
	.probe = rtk_wdg_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = of_match_ptr(rtk_wdg_of_table),
	},
};

module_platform_driver(rtk_watchdog_driver);

MODULE_DESCRIPTION("Realtek Ameba Watchdog driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
