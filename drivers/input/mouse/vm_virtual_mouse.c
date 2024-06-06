/*
 * Realtek Semiconductor Corp.
 *
 * vm_virtual_keyboard.h:
 *      RLX-VM virtual mouse driver.
 *
 * Copyright (C) 2013-2015 Pope Tang (donpope_tang@realsil.com.cn)
 * Copyright (C) 2019      Mickey Zhu(mickey_zhu@realsil.com.cn)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "../sdl_events.h"

struct rlxvm_mouse {
	struct device	*dev;
	void __iomem	*mmio;
	int		irq;
	struct input_dev	*idev;
};


static ssize_t rlxvm_mouse_store_mouse(struct device *dev,
				        struct device_attribute *attr,
				        const char *buf, size_t count)
{
	struct rlxvm_mouse *mouse = dev_get_drvdata(dev);
	int x, y;

	sscanf(buf, "%d%d", &x, &y);

	input_report_rel(mouse->idev, REL_X, x);
	input_report_rel(mouse->idev, REL_Y, y);
	printk(KERN_DEBUG "mouse write:%d, %d\n", x, y);

	input_sync(mouse->idev);

	return count;
}

static DEVICE_ATTR(mouse, S_IWUSR | S_IRUGO,
		    NULL,
		    rlxvm_mouse_store_mouse);

static struct attribute *rlxvm_mouse_attrs[] = {
	&dev_attr_mouse.attr,
	NULL
};

static struct attribute_group rlxvm_mouse_attr_group = {
	.attrs = rlxvm_mouse_attrs,
};

static irqreturn_t rlxvm_mouse_isr(int irq, void *args)
{
	struct rlxvm_mouse *mouse = args;
	SDL_Event *event = (SDL_Event *)(mouse->mmio + 0x100);
	const SDL_MouseMotionEvent *motion = &(event->motion);
	const SDL_MouseButtonEvent *button = &(event->button);

	switch (event->type) {
	case SDL_MOUSEMOTION:
		input_report_rel(mouse->idev, REL_X, motion->xrel);
		input_report_rel(mouse->idev, REL_Y, motion->yrel);
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (SDL_BUTTON(button->button) & SDL_BUTTON_LMASK)
			input_report_key(mouse->idev, BTN_LEFT, 1);
		if (SDL_BUTTON(button->button) & SDL_BUTTON_RMASK)
			input_report_key(mouse->idev, BTN_RIGHT, 1);
		if (SDL_BUTTON(button->button) & SDL_BUTTON_MMASK)
			input_report_key(mouse->idev, BTN_MIDDLE, 1);
		break;
	case SDL_MOUSEBUTTONUP:
		if (!(SDL_BUTTON(button->button) & SDL_BUTTON_LMASK))
			input_report_key(mouse->idev, BTN_LEFT, 0);
		if (!(SDL_BUTTON(button->button) & SDL_BUTTON_RMASK))
			input_report_key(mouse->idev, BTN_RIGHT, 0);
		if (!(SDL_BUTTON(button->button) & SDL_BUTTON_MMASK))
			input_report_key(mouse->idev, BTN_MIDDLE, 0);
		break;
	case SDL_MOUSEWHEEL:
		input_report_rel(mouse->idev, REL_WHEEL, event->wheel.y);
		break;
	default:
		break;
	}

	input_sync(mouse->idev);

	/* Clear inerrupt flag */
	readb(mouse->mmio + 0x10);

	return IRQ_HANDLED;
}

static int rlxvm_mouse_probe(struct platform_device *pdev)
{
	struct rlxvm_mouse *mouse;
	struct input_dev *input;
	struct resource *res;
	int err;

	mouse = devm_kzalloc(&pdev->dev, sizeof(*mouse), GFP_KERNEL);
	if (!mouse) {
		dev_err(&pdev->dev, "failed to alloc memory for rlxvm_mouse\n");
		return -ENOMEM;
	}

	mouse->irq = platform_get_irq(pdev, 0);
	if (mouse->irq < 0) {
		dev_err(&pdev->dev, "failed to get keyboard IRQ\n");
		return -ENXIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mouse->mmio = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mouse->mmio))
		return PTR_ERR(mouse->mmio);

	input = devm_input_allocate_device(&pdev->dev);
	if (!input) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	mouse->dev = &pdev->dev;
	mouse->idev = input;
	input_set_drvdata(input, mouse);

	input->name = pdev->name;
	input->dev.parent = &pdev->dev;

	input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input->rep[REP_DELAY] = 250;
	input->rep[REP_PERIOD] = 33;

	set_bit(EV_REL, input->evbit);
	set_bit(REL_X, input->relbit);
	set_bit(REL_Y, input->relbit);
	set_bit(REL_WHEEL, input->relbit);

	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_LEFT, input->keybit);
	set_bit(BTN_RIGHT, input->keybit);
	set_bit(BTN_MIDDLE, input->keybit);

	err = input_register_device(input);
	if (err) {
		dev_err(&pdev->dev, "failed to register input device\n");
		return err;
	}

	err = devm_request_irq(&pdev->dev, mouse->irq, rlxvm_mouse_isr,
			       IRQF_TRIGGER_RISING, pdev->name, mouse);
	if (err) {
		dev_err(&pdev->dev, "failed to request keyboard IRQ\n");
		return err;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &rlxvm_mouse_attr_group);
	if (err) {
		dev_err(&pdev->dev, "Unable to export keys\n");
		sysfs_remove_group(&pdev->dev.kobj, &rlxvm_mouse_attr_group);
		return err;
	}

	platform_set_drvdata(pdev, mouse);

	/* Enable mouse module */
	readb(mouse->mmio + 0x20);

	return 0;
}

static int rlxvm_mouse_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &rlxvm_mouse_attr_group);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id rlxvm_mouse_dt_match_table[] = {
	{ .compatible = "realtek,rlxvm_mouse" },
	{},
};
MODULE_DEVICE_TABLE(of, rlxvm_mouse_dt_match_table);
#endif

static struct platform_driver rlxvm_mouse_driver = {
	.probe		= rlxvm_mouse_probe,
	.remove		= rlxvm_mouse_remove,
	.driver		= {
		.name	= "rlxvm_mouse",
		.of_match_table = of_match_ptr(rlxvm_mouse_dt_match_table),
	},
};
module_platform_driver(rlxvm_mouse_driver);

MODULE_AUTHOR("Realtek PSP software");
MODULE_DESCRIPTION("RLXVM Virtual Mouse Driver");
MODULE_LICENSE("GPL");
