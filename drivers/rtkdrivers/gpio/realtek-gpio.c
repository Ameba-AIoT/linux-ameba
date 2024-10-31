// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek GPIO support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/clk.h>
#include <linux/gpio/driver.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/clk-provider.h>
#include "realtek-gpio.h"

#define REALTEK_GPIO_PINS_PER_BANK 32
#ifdef CONFIG_SOC_CPU_ARMA7
#define REALTEK_GPIOC_PINS_PER_BANK 7
#endif
#ifdef CONFIG_SOC_CPU_ARMA32
#define REALTEK_GPIOC_PINS_PER_BANK 8
#endif
#define REALTEK_GPIO_BANK_START(bank)		((bank) * REALTEK_GPIO_PINS_PER_BANK)

struct realtek_gpio_bank {
	void __iomem *reg_base;
	struct gpio_chip gpio_chip;
	struct irq_chip irq_chip;
	struct clk *clk;
	struct clk *clk_sl;
	struct clk *parent_ls_apb;
	struct clk *parent_sdm32k;
	int irq;
	struct irq_domain *domain;
	spinlock_t lock;
};

extern suspend_state_t pm_suspend_target_state;

static inline int realtek_gpio_pin(int gpio)
{
	return gpio % REALTEK_GPIO_PINS_PER_BANK;
}

/*Get the signal of the gpio: 0 is low, 1 is high
 *proting from GPIO_PortRead
 */
static int realtek_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);
	unsigned long flags;
	int ret;


	spin_lock_irqsave(&bank->lock, flags);

	ret = !!(readl(bank->reg_base + GPIO_EXT_PORT) & BIT(offset));

	spin_unlock_irqrestore(&bank->lock, flags);

	return ret;
}

static void realtek_gpio_set_internal(struct gpio_chip *chip, unsigned offset, int value)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);
	u32 reg_value;

	reg_value = readl(bank->reg_base + GPIO_DR);

	if (value == GPIO_PIN_LOW) {
		reg_value &= ~BIT(offset);
	} else {
		reg_value |= BIT(offset);
	}

	writel(reg_value, bank->reg_base + GPIO_DR);
}

static void realtek_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);
	unsigned long flags;

	spin_lock_irqsave(&bank->lock, flags);
	realtek_gpio_set_internal(chip, offset, value);
	spin_unlock_irqrestore(&bank->lock, flags);
}

static int realtek_gpio_set_config(struct gpio_chip *chip, unsigned offset, unsigned long config)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);
	u32 reg_value;
	u32 arg = (pinconf_to_config_argument(config) & 0x0000007F);
	unsigned long flags;

	if (pinconf_to_config_param(config) == PIN_CONFIG_INPUT_DEBOUNCE) {
		spin_lock_irqsave(&bank->lock, flags);

		reg_value = readl(bank->reg_base + GPIO_DEBOUNCE);
		reg_value |= BIT(offset);
		writel(reg_value, bank->reg_base + GPIO_DEBOUNCE);

		writel(arg, bank->reg_base + GPIO_DB_DIV_CONFIG);

		spin_unlock_irqrestore(&bank->lock, flags);
	}

	return 0;
}

static int realtek_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);
	unsigned long flags;
	u32 reg_value;

	spin_lock_irqsave(&bank->lock, flags);

	reg_value = readl(bank->reg_base + GPIO_DDR);
	reg_value &= ~BIT(offset);
	writel(reg_value, bank->reg_base + GPIO_DDR);

	spin_unlock_irqrestore(&bank->lock, flags);

	return 0;
}

static int realtek_gpio_direction_output(struct gpio_chip *chip,
		unsigned offset, int value)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);
	unsigned long flags;
	u32 reg_value;

	spin_lock_irqsave(&bank->lock, flags);
	realtek_gpio_set_internal(chip, offset, value);

	reg_value = readl(bank->reg_base + GPIO_DDR);
	reg_value |= BIT(offset);
	writel(reg_value, bank->reg_base + GPIO_DDR);
	spin_unlock_irqrestore(&bank->lock, flags);

	return 0;
}

static int realtek_gpio_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&bank->lock, flags);

	ret = !(readl(bank->reg_base + GPIO_DDR) & BIT(offset));

	spin_unlock_irqrestore(&bank->lock, flags);

	return ret;
}

static int realtek_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	if (offset < chip->ngpio) {
		return 0;
	} else {
		return -1;
	}
}

static void realtek_gpio_irq_ack(struct irq_data *data)
{
	struct realtek_gpio_bank *bank = irq_data_get_irq_chip_data(data);
	unsigned long flags;

	spin_lock_irqsave(&bank->lock, flags);

	writel(BIT(data->hwirq), bank->reg_base + GPIO_INT_EOI);

	spin_unlock_irqrestore(&bank->lock, flags);
}

static void realtek_gpio_irq_mask(struct irq_data *data)
{
	struct realtek_gpio_bank *bank = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 mask;

	spin_lock_irqsave(&bank->lock, flags);

	mask = readl(bank->reg_base + GPIO_INT_MASK);
	mask |= BIT(data->hwirq);
	writel(mask, bank->reg_base + GPIO_INT_MASK);

	spin_unlock_irqrestore(&bank->lock, flags);
}

static void realtek_gpio_irq_unmask(struct irq_data *data)
{
	struct realtek_gpio_bank *bank = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 mask;

	spin_lock_irqsave(&bank->lock, flags);

	mask = readl(bank->reg_base + GPIO_INT_MASK);
	mask &= ~BIT(data->hwirq);
	writel(mask, bank->reg_base + GPIO_INT_MASK);

	spin_unlock_irqrestore(&bank->lock, flags);
}

static void realtek_gpio_irq_enable(struct irq_data *data)
{
	struct realtek_gpio_bank *bank = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 mask;

	spin_lock_irqsave(&bank->lock, flags);

	mask = readl(bank->reg_base + GPIO_INT_EN);
	mask |= BIT(data->hwirq);
	writel(mask, bank->reg_base + GPIO_INT_EN);

	spin_unlock_irqrestore(&bank->lock, flags);

	realtek_gpio_irq_unmask(data);
}

static unsigned int realtek_gpio_irq_startup(struct irq_data *data)
{
	struct realtek_gpio_bank *bank = irq_data_get_irq_chip_data(data);

	realtek_gpio_direction_input(&bank->gpio_chip, data->hwirq);
	realtek_gpio_irq_unmask(data);
	realtek_gpio_irq_enable(data);

	return 0;
}

static int realtek_gpio_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct realtek_gpio_bank *bank = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 mask;

	spin_lock_irqsave(&bank->lock, flags);

	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_RISING:
		mask = readl(bank->reg_base + GPIO_INT_BOTHEDGE);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_BOTHEDGE);

		mask = readl(bank->reg_base + GPIO_INT_TYPE);
		mask |= BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_TYPE);

		mask = readl(bank->reg_base + GPIO_INT_POLARITY);
		mask |= BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_POLARITY);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		mask = readl(bank->reg_base + GPIO_INT_BOTHEDGE);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_BOTHEDGE);

		mask = readl(bank->reg_base + GPIO_INT_TYPE);
		mask |= BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_TYPE);

		mask = readl(bank->reg_base + GPIO_INT_POLARITY);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_POLARITY);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		mask = readl(bank->reg_base + GPIO_INT_BOTHEDGE);
		mask |= BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_BOTHEDGE);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		mask = readl(bank->reg_base + GPIO_INT_BOTHEDGE);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_BOTHEDGE);

		mask = readl(bank->reg_base + GPIO_INT_TYPE);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_TYPE);

		mask = readl(bank->reg_base + GPIO_INT_POLARITY);
		mask |= BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_POLARITY);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		mask = readl(bank->reg_base + GPIO_INT_BOTHEDGE);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_BOTHEDGE);

		mask = readl(bank->reg_base + GPIO_INT_TYPE);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_TYPE);

		mask = readl(bank->reg_base + GPIO_INT_POLARITY);
		mask &= ~BIT(data->hwirq);
		writel(mask, bank->reg_base + GPIO_INT_POLARITY);
		break;
	default:
		spin_unlock_irqrestore(&bank->lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&bank->lock, flags);

	if (type & IRQ_TYPE_LEVEL_MASK) {
		irq_set_handler_locked(data, handle_level_irq);
	} else {
		irq_set_handler_locked(data, handle_edge_irq);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int	realtek_gpio_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct realtek_gpio_bank *bank = irq_data_get_irq_chip_data(data);

	irq_set_irq_wake(bank->irq, on);

	return 0;
}
#else
#define realtek_gpio_irq_set_wake NULL
#endif

static void realtek_gpio_irq_handler(struct irq_desc *desc)
{
	struct realtek_gpio_bank *bank = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned long pending;
	unsigned int pin;
	unsigned int irq = irq_desc_get_irq(desc);

	chained_irq_enter(chip, desc);

	if (irq != bank->irq) {
		dev_err(bank->gpio_chip.parent, "GPIO irq number not match\n");
	}

	pm_wakeup_event(bank->gpio_chip.parent, 0);

	pending = readl(bank->reg_base + GPIO_INT_STATUS);

	for_each_set_bit(pin, &pending, bank->gpio_chip.ngpio)
	generic_handle_irq(irq_find_mapping(bank->domain, pin));

	chained_irq_exit(chip, desc);
}

static int realtek_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct realtek_gpio_bank *bank = gpiochip_get_data(chip);

	if (offset >= chip->ngpio) {
		return -ENXIO;
	}

	return irq_find_mapping(bank->domain, offset);
}

static const struct irq_domain_ops realtek_gpio_irq_domain_ops = {
	.xlate		= irq_domain_xlate_twocell,
};

#define GPIO_BANK(_bank, _npins)					\
	{								\
		.gpio_chip = {						\
			.label = "GPIO" #_bank,				\
			.request = realtek_gpio_request,		\
			.get_direction = realtek_gpio_get_direction,	\
			.direction_input = realtek_gpio_direction_input,	\
			.direction_output = realtek_gpio_direction_output, \
			.get = realtek_gpio_get,				\
			.set = realtek_gpio_set,				\
			.set_config = realtek_gpio_set_config,		\
			.to_irq = realtek_gpio_to_irq,		\
			.ngpio = _npins,				\
			.base = REALTEK_GPIO_BANK_START(_bank),			\
			.owner = THIS_MODULE,				\
			.can_sleep = 0,					\
		},							\
		.irq_chip = {						\
			.name = "GPIO" #_bank,				\
			.irq_startup = realtek_gpio_irq_startup,	\
			.irq_enable	= realtek_gpio_irq_enable,  \
			.irq_ack = realtek_gpio_irq_ack,		\
			.irq_mask = realtek_gpio_irq_mask,		\
			.irq_unmask = realtek_gpio_irq_unmask,		\
			.irq_set_type = realtek_gpio_irq_set_type,	\
			.irq_set_wake = realtek_gpio_irq_set_wake,	\
		},							\
	}

static struct realtek_gpio_bank realtek_gpio_banks[] = {
	GPIO_BANK(0, REALTEK_GPIO_PINS_PER_BANK),
	GPIO_BANK(1, REALTEK_GPIO_PINS_PER_BANK),
	GPIO_BANK(2, REALTEK_GPIOC_PINS_PER_BANK),
};

int realtek_gpio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct realtek_gpio_bank *bank;
	u32 id;
	int ret;
	struct resource *res;
	int i;
	const struct clk_hw *hwclk;

	if (of_property_read_u32(np, "rtk,gpio-bank", &id)) {
		dev_err(&pdev->dev, "No realtek,gpio-bank property\n");
		return -EINVAL;
	}

	if (id >= ARRAY_SIZE(realtek_gpio_banks)) {
		dev_err(&pdev->dev, "Invalid realtek,gpio-bank property\n");
		return -EINVAL;
	}

	bank = &realtek_gpio_banks[id];

	spin_lock_init(&bank->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	bank->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(bank->reg_base)) {
		return PTR_ERR(bank->reg_base);
	}

	bank->irq = platform_get_irq(pdev, 0);
	if (bank->irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return bank->irq;
	}

	bank->gpio_chip.parent = &pdev->dev;
	bank->gpio_chip.of_node = np;
	ret = gpiochip_add_data(&bank->gpio_chip, bank);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to add gpiochip %d: %d\n",
				id, ret);
		return ret;
	}

	bank->domain = irq_domain_add_linear(np, bank->gpio_chip.ngpio, &realtek_gpio_irq_domain_ops, bank);
	if (!bank->domain) {
		dev_err(&pdev->dev, "Couldn't register IRQ domain\n");
		gpiochip_remove(&bank->gpio_chip);
		return -ENOMEM;
	}

	for (i = 0; i < bank->gpio_chip.ngpio; i++) {
		int irqno = irq_create_mapping(bank->domain, i);

		irq_set_chip_and_handler(irqno, &bank->irq_chip, handle_level_irq);
		irq_set_chip_data(irqno, bank);
	}

	irq_set_chained_handler_and_data(bank->irq, realtek_gpio_irq_handler, bank);

	bank->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(bank->clk)) {
		ret =  PTR_ERR(bank->clk);
		dev_err(&pdev->dev, "Failed to get rcc clk: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "GPIO bank %d initialized\n", id);

	bank->clk_sl = clk_get_parent(bank->clk);
	hwclk = __clk_get_hw(bank->clk_sl);
	bank->parent_ls_apb = clk_hw_get_parent_by_index(hwclk, 0)->clk;
	bank->parent_sdm32k = clk_hw_get_parent_by_index(hwclk, 1)->clk;

	platform_set_drvdata(pdev, bank);

	return 0;
}

#ifdef CONFIG_PM
static int realtek_gpio_suspend(struct device *dev)
{
	struct realtek_gpio_bank *bank = (struct realtek_gpio_bank *) dev->driver_data;

	/* For sleep type CG/PG, GPIO clock need to switch from LS APB to SDM32K when suspend. */
	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		clk_set_parent(bank->clk_sl, bank->parent_sdm32k);
	}
	return 0;
}

static int realtek_gpio_resume(struct device *dev)
{
	struct realtek_gpio_bank *bank = (struct realtek_gpio_bank *) dev->driver_data;

	/* For sleep type CG/PG, GPIO clock need to switch from SDM32K to LS APB when resume. */
	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		clk_set_parent(bank->clk_sl, bank->parent_ls_apb);
	}
	return 0;
}


static const struct dev_pm_ops realtek_ameba_gpio_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(realtek_gpio_suspend, realtek_gpio_resume)
};
#endif

static const struct of_device_id realtek_ameba_gpio_of_match[] = {
	{ .compatible = "realtek,ameba-gpio", },
	{ },
};

static struct platform_driver realtek_ameba_gpio_driver = {
	.probe = realtek_gpio_probe,
	.driver = {
		.name = "realtek-ameba-gpio",
		.of_match_table = realtek_ameba_gpio_of_match,
#ifdef CONFIG_PM
		.pm = &realtek_ameba_gpio_pm_ops,
#endif
	},
};

static int __init realtek_ameba_gpio_register(void)
{
	return platform_driver_register(&realtek_ameba_gpio_driver);
}
arch_initcall(realtek_ameba_gpio_register);
