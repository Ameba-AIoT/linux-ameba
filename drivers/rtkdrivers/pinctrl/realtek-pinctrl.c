// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Pinctrl support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/clk.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "core.h"
#include "pinconf.h"
#include "pinctrl-utils.h"
#include "realtek-pinctrl.h"

static const char *const realtek_gpio_functions[] = {
	"gpio", "uart_data", "log_uart_rts_cts",
	"spi", "rtc", "ir",
	"spi_flash", "i2c", "sdio",
	"ledc", "pwm", "swd",
	"audio", "i2s0_1", "i2s2",
	"i2s3", "spk_auxin", "dmic",
	"captouch_aux_adc", "sic", "mipi",
	"usb", "rf_ctrl", "ext_zigbee",
	"bt_uart", "bt_gpio", "bt_rf",
	"dbg_btcoex_gnt", "hs_timer_trig", "debug_port",
	"wakeup",
};

struct realtek_pinctrl_group {
	const char *name;
	unsigned long config;
	unsigned pin;
};

struct realtek_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pctl_dev;
	struct pinctrl_desc pctl_desc;
	struct realtek_pinctrl_group *groups;
	unsigned ngroups;
	const char **grp_names;
	const struct realtek_pinctrl_match_data *match_data;

	struct hwspinlock *hwlock;
	struct realtek_desc_pin *pins;
	u32 npins;
	u16 irqmux_map;
	spinlock_t			lock;
	void __iomem	*membase;
};

/* Custom pinconf parameters */
#define PIN_CONFIG_SWD_DISABLE	(PIN_CONFIG_END + 1)
#define PIN_CONFIG_AUDIO_SHARE_ENABLE	(PIN_CONFIG_END + 2)
static const struct pinconf_generic_params realtek_custom_bindings[] = {
	{"swd-disable", PIN_CONFIG_SWD_DISABLE, 0},
	{"audio-share-enable", PIN_CONFIG_AUDIO_SHARE_ENABLE, 1},
};

/* Pinctrl functions */
static struct realtek_pinctrl_group *
realtek_pctrl_find_group_by_pin(struct realtek_pinctrl *pctl, u32 pin)
{
	int i;

	for (i = 0; i < pctl->ngroups; i++) {
		struct realtek_pinctrl_group *grp = pctl->groups + i;

		if (grp->pin == pin) {
			return grp;
		}
	}

	return NULL;
}

static bool realtek_pctrl_is_function_valid(struct realtek_pinctrl *pctl,
		u32 pin_num, u32 fnum)
{
	int i;

	for (i = 0; i < pctl->npins; i++) {
		const struct realtek_desc_pin *pin = pctl->pins + i;
		const struct realtek_desc_function *func = pin->functions;

		if (pin->pin.number != pin_num) {
			continue;
		}

		while (func && func->name) {
			if (func->func_id == fnum) {
				return true;
			}
			func++;
		}

		break;
	}

	return false;
}

static int realtek_pctrl_dt_node_to_map_func(struct realtek_pinctrl *pctl,
		u32 pin, u32 fnum, struct realtek_pinctrl_group *grp,
		struct pinctrl_map **map, unsigned *reserved_maps,
		unsigned *num_maps)
{
	if (*num_maps == *reserved_maps) {
		return -ENOSPC;
	}

	(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)[*num_maps].data.mux.group = grp->name;

	if (!realtek_pctrl_is_function_valid(pctl, pin, fnum)) {
		dev_err(pctl->dev, "Invalid function %d on pin %d\n",
				fnum, pin);
		return -EINVAL;
	}

	(*map)[*num_maps].data.mux.function = realtek_gpio_functions[fnum];
	(*num_maps)++;

	return 0;
}

static int realtek_pctrl_dt_subnode_to_map(struct pinctrl_dev *pctldev,
		struct device_node *node,
		struct pinctrl_map **map,
		unsigned *reserved_maps,
		unsigned *num_maps)
{
	struct realtek_pinctrl *pctl;
	struct realtek_pinctrl_group *grp;
	struct property *pins;
	u32 pinfunc, pin, func;
	unsigned long *configs;
	unsigned int num_configs;
	bool has_config = 0;
	unsigned reserve = 0;
	int num_pins, num_funcs, maps_per_pin, i, err = 0;

	pctl = pinctrl_dev_get_drvdata(pctldev);

	pins = of_find_property(node, "pinmux", NULL);
	if (!pins) {
		dev_err(pctl->dev, "No pinmux property in DTS\n",
				node);
		return -EINVAL;
	}

	err = pinconf_generic_parse_dt_config(node, pctldev, &configs,
										  &num_configs);
	if (err) {
		return err;
	}

	if (num_configs) {
		has_config = 1;
	}

	num_pins = pins->length / sizeof(u32);
	num_funcs = num_pins;
	maps_per_pin = 0;
	if (num_funcs) {
		maps_per_pin++;
	}
	if (has_config && num_pins >= 1) {
		maps_per_pin++;
	}

	if (!num_pins || !maps_per_pin) {
		err = -EINVAL;
		goto exit;
	}

	reserve = num_pins * maps_per_pin;

	err = pinctrl_utils_reserve_map(pctldev, map,
									reserved_maps, num_maps, reserve);
	if (err) {
		goto exit;
	}

	for (i = 0; i < num_pins; i++) {
		err = of_property_read_u32_index(node, "pinmux",
										 i, &pinfunc);
		if (err) {
			goto exit;
		}

		pin = REALTEK_GET_PIN_NO(pinfunc);
		func = REALTEK_GET_PIN_FUNC(pinfunc);

		if (!realtek_pctrl_is_function_valid(pctl, pin, func)) {
			dev_err(pctl->dev, "Invalid function %d for pin %d\n", func, pin);
			err = -EINVAL;
			goto exit;
		}

		grp = realtek_pctrl_find_group_by_pin(pctl, pin);
		if (!grp) {
			dev_err(pctl->dev, "Unable to match pin %d to group\n",
					pin);
			err = -EINVAL;
			goto exit;
		}

		err = realtek_pctrl_dt_node_to_map_func(pctl, pin, func, grp, map,
												reserved_maps, num_maps);
		if (err) {
			goto exit;
		}

		if (has_config) {
			err = pinctrl_utils_add_map_configs(pctldev, map,
												reserved_maps, num_maps, grp->name,
												configs, num_configs,
												PIN_MAP_TYPE_CONFIGS_GROUP);
			if (err) {
				goto exit;
			}
		}
	}

exit:
	kfree(configs);
	return err;
}

static int realtek_pctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
										struct device_node *np_config,
										struct pinctrl_map **map, unsigned *num_maps)
{
	struct device_node *np = NULL;
	unsigned reserved_maps;
	int ret;

	*map = NULL;
	*num_maps = 0;
	reserved_maps = 0;

	for_each_child_of_node(np_config, np) {
		ret = realtek_pctrl_dt_subnode_to_map(pctldev, np, map,
											  &reserved_maps, num_maps);
		if (ret < 0) {
			pinctrl_utils_free_map(pctldev, *map, *num_maps);
			of_node_put(np);
			return ret;
		}
	}

	return 0;
}

static int realtek_pctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->ngroups;
}

static const char *realtek_pctrl_get_group_name(struct pinctrl_dev *pctldev,
		unsigned group)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[group].name;
}

static int realtek_pctrl_get_group_pins(struct pinctrl_dev *pctldev,
										unsigned group,
										const unsigned **pins,
										unsigned *num_pins)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*pins = (unsigned *)&pctl->groups[group].pin;
	*num_pins = 1;

	return 0;
}

static const struct pinctrl_ops realtek_pctrl_ops = {
	.dt_node_to_map		= realtek_pctrl_dt_node_to_map,
	.dt_free_map		= pinctrl_utils_free_map,
	.get_groups_count	= realtek_pctrl_get_groups_count,
	.get_group_name		= realtek_pctrl_get_group_name,
	.get_group_pins		= realtek_pctrl_get_group_pins,
};

/* Pinmux functions */

static int realtek_pmx_get_funcs_cnt(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(realtek_gpio_functions);
}

static const char *realtek_pmx_get_func_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	return realtek_gpio_functions[selector];
}

static int realtek_pmx_get_func_groups(struct pinctrl_dev *pctldev,
									   unsigned function,
									   const char *const **groups,
									   unsigned *const num_groups)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctl->grp_names;
	*num_groups = pctl->ngroups;

	return 0;
}

static int realtek_pmx_set_mux(struct pinctrl_dev *pctldev,
							   unsigned function,
							   unsigned group)
{
	bool ret;
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct realtek_pinctrl_group *g = pctl->groups + group;
	unsigned long flags;
	u32 val;
	int err = 0;

	ret = realtek_pctrl_is_function_valid(pctl, g->pin, function);
	if (!ret) {
		dev_err(pctl->dev, "Invalid function %d on group %d\n",
				function, group);
		return -EINVAL;
	}

	/* Should change according to our own code*/
	spin_lock_irqsave(&pctl->lock, flags);

	/* Get PADCTR */
	val = readl(pctl->membase + (g->pin * 0x4));

	/* Set needs function */
	val &= ~PAD_MASK_GPIOx_SEL;
	val |= PAD_GPIOx_SEL(function);

	/* Set PADCTR register */
	writel(val, pctl->membase + (g->pin * 0x4));

	spin_unlock_irqrestore(&pctl->lock, flags);

	return err;
}

static const struct pinmux_ops realtek_pmx_ops = {
	.get_functions_count	= realtek_pmx_get_funcs_cnt,
	.get_function_name	= realtek_pmx_get_func_name,
	.get_function_groups	= realtek_pmx_get_func_groups,
	.set_mux		= realtek_pmx_set_mux,
	.strict			= true,
};

/* Pinconf functions */
static int realtek_pconf_set_driving(struct pinctrl_dev *pctldev,
									 unsigned int pin, u32 drive)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	u32 val;
	int err = 0;
	u32 offset = pin * 0x4;

	/* Should change according to our own code*/
	val = readl(pctl->membase + offset);

	if (drive == PAD_DRV_ABILITITY_LOW) {
		val &= ~PAD_BIT_GPIOx_E2;
	} else {
		val |= PAD_BIT_GPIOx_E2;
	}

	writel(val, pctl->membase + offset);

	return err;
}

static int realtek_pconf_set_speed(struct pinctrl_dev *pctldev,
								   unsigned int pin, u32 speed)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	u32 val;
	int err = 0;
	u32 offset = pin * 0x4;

	/* Should change according to our own code*/
	val = readl(pctl->membase + offset);

	if (speed == PAD_Slew_Rate_LOW) {
		val &= ~PAD_BIT_GPIOx_SR;
	} else {
		val |= PAD_BIT_GPIOx_SR;
	}

	writel(val, pctl->membase + offset);

	return err;
}

static int realtek_pconf_set_bias(struct pinctrl_dev *pctldev,
								  unsigned int pin, u32 pull_type)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	u32 val;
	int err = 0;
	u32 offset = pin * 0x4;

	/* Should change according to our own code*/
	val = readl(pctl->membase + offset);

	/* Clear Pin_Num Pull contrl */
	val &= ~(PAD_BIT_GPIOx_PU | PAD_BIT_GPIOx_PD | PAD_BIT_GPIOx_PD_SLP | PAD_BIT_GPIOx_PU_SLP);

	/* Set needs Pull contrl */
	if (pull_type == GPIO_PuPd_DOWN) {
		val |= PAD_BIT_GPIOx_PD;
		val |= PAD_BIT_GPIOx_PD_SLP;
	} else if (pull_type == GPIO_PuPd_UP) {
		val |= PAD_BIT_GPIOx_PU;
		val |= PAD_BIT_GPIOx_PU_SLP;
	}

	writel(val, pctl->membase + offset);

	return err;
}

static int realtek_pconf_swd_off(struct pinctrl_dev *pctldev, unsigned int pin)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	u32 val;

#ifdef CONFIG_SOC_CPU_ARMA7
	if ((pin != 3) && (pin != 4)) {
		dev_err(pctl->dev, "Pin%d is out of range\n", pin);
		return -EINVAL;
	}
#endif
#ifdef CONFIG_SOC_CPU_ARMA32
	if ((pin != 13) && (pin != 14)) {
		dev_err(pctl->dev, "Pin%d is out of range\n", pin);
		return -EINVAL;
	}
#endif

	/* Should change according to our own code*/
	val = readl(pctl->membase + REG_SWD_SDD_CTRL);
	val &= ~(PAD_BIT_SWD_PMUX_EN);
	writel(val, pctl->membase + REG_SWD_SDD_CTRL);

	return 0;
}

static int realtek_pconf_audio_share_enable(struct pinctrl_dev *pctldev, unsigned int pin)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	u32 val;

	if ((pin < 18) || (pin > 39)) {
		dev_err(pctl->dev, "Pin%d is out of range\n", pin);
		return -EINVAL;
	}

	/* Should change according to our own code*/
	val = readl(pctl->membase + REG_PAD_AUD_PAD_CTRL);
	val |= BIT(pin - 18);
	writel(val, pctl->membase + REG_PAD_AUD_PAD_CTRL);

	return 0;
}

/* Should change accoring to pin not GPIO*/
static int realtek_pconf_parse_conf(struct pinctrl_dev *pctldev,
									unsigned int pin, enum pin_config_param param,
									enum pin_config_param arg)
{
	int ret = 0;

	switch ((unsigned int)param) {
	case PIN_CONFIG_DRIVE_STRENGTH:
		ret = realtek_pconf_set_driving(pctldev, pin, arg);
		break;
	case PIN_CONFIG_SLEW_RATE:
		ret = realtek_pconf_set_speed(pctldev, pin, arg);
		break;
	case PIN_CONFIG_BIAS_DISABLE:
		ret = realtek_pconf_set_bias(pctldev, pin, GPIO_PuPd_NOPULL);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		ret = realtek_pconf_set_bias(pctldev, pin, GPIO_PuPd_UP);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		ret = realtek_pconf_set_bias(pctldev, pin, GPIO_PuPd_DOWN);
		break;
	case PIN_CONFIG_SWD_DISABLE:
		ret = realtek_pconf_swd_off(pctldev, pin);
		break;
	case PIN_CONFIG_AUDIO_SHARE_ENABLE:
		ret = realtek_pconf_audio_share_enable(pctldev, pin);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int realtek_pconf_get(struct pinctrl_dev *pctldev, unsigned pin,
							 unsigned long *config)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned int param = (unsigned int)pinconf_to_config_param(*config);
	u32 val;
	u16 arg;
	unsigned long flags;

	spin_lock_irqsave(&pctl->lock, flags);

	val = readl(pctl->membase + (pin * 0x4));

	switch (param) {
	case PIN_CONFIG_DRIVE_STRENGTH:
		arg = !!(val & PAD_BIT_GPIOx_E2);
		break;

	case PIN_CONFIG_SLEW_RATE:
		arg = !!(val & PAD_BIT_GPIOx_SR);
		break;

	case PIN_CONFIG_BIAS_PULL_UP:
		arg = !!(val & PAD_BIT_GPIOx_PU_SLP);
		break;

	case PIN_CONFIG_BIAS_PULL_DOWN:
		arg = !!(val & PAD_BIT_GPIOx_PD_SLP);
		break;

	case PIN_CONFIG_BIAS_DISABLE:
		if (val & (PAD_BIT_GPIOx_PU_SLP | PAD_BIT_GPIOx_PD_SLP)) {
			spin_unlock_irqrestore(&pctl->lock, flags);
			return -EINVAL;
		}
		arg = 0;
		break;

	case PIN_CONFIG_SWD_DISABLE:
		arg = !!(readl(pctl->membase + REG_SWD_SDD_CTRL) & PAD_BIT_SWD_PMUX_EN);
		break;

	case PIN_CONFIG_AUDIO_SHARE_ENABLE:
		arg = !!(readl(pctl->membase + REG_PAD_AUD_PAD_CTRL) & PAD_MASK_GPIOC_IE);
		break;

	default:
		WARN_ON(1);
		spin_unlock_irqrestore(&pctl->lock, flags);
		return -ENOTSUPP;
	}

	spin_unlock_irqrestore(&pctl->lock, flags);

	*config = pinconf_to_config_packed(param, arg);

	return 0;
}

static int realtek_pconf_group_get(struct pinctrl_dev *pctldev,
								   unsigned group,
								   unsigned long *config)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;

	spin_lock_irqsave(&pctl->lock, flags);

	*config = pctl->groups[group].config;

	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static int realtek_pconf_set(struct pinctrl_dev *pctldev, unsigned pin,
							 unsigned long *configs, unsigned num_configs)
{
	int i, ret;
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;

	spin_lock_irqsave(&pctl->lock, flags);

	for (i = 0; i < num_configs; i++) {
		ret = realtek_pconf_parse_conf(pctldev, pin,
									   pinconf_to_config_param(configs[i]),
									   pinconf_to_config_argument(configs[i]));
		if (ret < 0) {
			spin_unlock_irqrestore(&pctl->lock, flags);
			return ret;
		}
	}

	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static int realtek_pconf_group_set(struct pinctrl_dev *pctldev, unsigned group,
								   unsigned long *configs, unsigned num_configs)
{
	struct realtek_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct realtek_pinctrl_group *g = &pctl->groups[group];
	int i, ret;
	unsigned long flags;

	spin_lock_irqsave(&pctl->lock, flags);

	for (i = 0; i < num_configs; i++) {
		ret = realtek_pconf_parse_conf(pctldev, g->pin,
									   pinconf_to_config_param(configs[i]),
									   pinconf_to_config_argument(configs[i]));
		if (ret < 0) {
			spin_unlock_irqrestore(&pctl->lock, flags);
			return ret;
		}

		g->config = configs[i];
	}

	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static const struct pinconf_ops realtek_pconf_ops = {
	.is_generic		= true,
	.pin_config_get		= realtek_pconf_get,
	.pin_config_set		= realtek_pconf_set,
	.pin_config_group_get	= realtek_pconf_group_get,
	.pin_config_group_set	= realtek_pconf_group_set,
};

static int realtek_pctrl_build_state(struct platform_device *pdev)
{
	struct realtek_pinctrl *pctl = platform_get_drvdata(pdev);
	int i;

	pctl->ngroups = pctl->npins;

	/* Allocate groups */
	pctl->groups = devm_kcalloc(&pdev->dev, pctl->ngroups,
								sizeof(*pctl->groups), GFP_KERNEL);
	if (!pctl->groups) {
		return -ENOMEM;
	}

	/* We assume that one pin is one group, use pin name as group name. */
	pctl->grp_names = devm_kcalloc(&pdev->dev, pctl->ngroups,
								   sizeof(*pctl->grp_names), GFP_KERNEL);
	if (!pctl->grp_names) {
		return -ENOMEM;
	}

	for (i = 0; i < pctl->npins; i++) {
		const struct realtek_desc_pin *pin = pctl->pins + i;
		struct realtek_pinctrl_group *group = pctl->groups + i;

		group->name = pin->pin.name;
		group->pin = pin->pin.number;
		pctl->grp_names[i] = pin->pin.name;
	}

	return 0;
}

static int realtek_pctrl_create_pins_tab(struct realtek_pinctrl *pctl,
		struct realtek_desc_pin *pins)
{
	const struct realtek_desc_pin *p;
	int i = 0;

	for (i = 0; i < pctl->match_data->npins; i++) {
		p = pctl->match_data->pins + i;
		pins->pin = p->pin;
		pins->functions = p->functions;
		pins++;
	}

	pctl->npins = pctl->match_data->npins;

	return 0;
}

int realtek_pctl_probe(struct platform_device *pdev)
{
	struct realtek_pinctrl *pctl;
	struct pinctrl_pin_desc *pins;
	int i, ret;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;

	if (!np) {
		return -EINVAL;
	}

	match = of_match_device(dev->driver->of_match_table, dev);
	if (!match || !match->data) {
		return -EINVAL;
	}

	pctl = devm_kzalloc(dev, sizeof(*pctl), GFP_KERNEL);
	if (!pctl) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, pctl);
	pctl->dev = dev;

	spin_lock_init(&pctl->lock);

	pctl->match_data = match->data;

	/* Get memory base from device tree and ioremap it*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pctl->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pctl->membase)) {
		return PTR_ERR(pctl->membase);
	}

	pctl->pins = devm_kcalloc(pctl->dev, pctl->match_data->npins,
							  sizeof(*pctl->pins), GFP_KERNEL);
	if (!pctl->pins) {
		return -ENOMEM;
	}

	ret = realtek_pctrl_create_pins_tab(pctl, pctl->pins);
	if (ret) {
		return ret;
	}

	ret = realtek_pctrl_build_state(pdev);
	if (ret) {
		dev_err(dev, "Failed to build state: %d\n", ret);
		return -EINVAL;
	}

	pins = devm_kcalloc(&pdev->dev, pctl->npins, sizeof(*pins),
						GFP_KERNEL);
	if (!pins) {
		return -ENOMEM;
	}

	for (i = 0; i < pctl->npins; i++) {
		pins[i] = pctl->pins[i].pin;
	}

	pctl->pctl_desc.name = dev_name(&pdev->dev);
	pctl->pctl_desc.owner = THIS_MODULE;
	pctl->pctl_desc.pins = pins;
	pctl->pctl_desc.npins = pctl->npins;
	pctl->pctl_desc.link_consumers = true;
	pctl->pctl_desc.confops = &realtek_pconf_ops;
	pctl->pctl_desc.pctlops = &realtek_pctrl_ops;
	pctl->pctl_desc.pmxops = &realtek_pmx_ops;
	pctl->pctl_desc.num_custom_params = ARRAY_SIZE(realtek_custom_bindings);
	pctl->pctl_desc.custom_params = realtek_custom_bindings;
	pctl->dev = &pdev->dev;

	pctl->pctl_dev = devm_pinctrl_register(&pdev->dev, &pctl->pctl_desc,
										   pctl);

	if (IS_ERR(pctl->pctl_dev)) {
		dev_err(&pdev->dev, "Failed to register pinctrl\n");
		return PTR_ERR(pctl->pctl_dev);
	}

	dev_info(dev, "Initialized successfully\n");

	return 0;
}

