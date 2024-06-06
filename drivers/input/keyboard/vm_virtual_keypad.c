/*
 * Realtek Semiconductor Corp.
 *
 * vm_virtual_keyboard.h:
 *      RLX-VM virtual keypad driver.
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

struct rlxvm_kbd {
	struct device	*dev;
	void __iomem	*mmio;
	int		irq;
	struct input_dev	*idev;
};

/* Copy keycode from hid/usbhid/usbkbd.c */
static const unsigned char sdl_kbd_keycode[256] = {
	  0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	  4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
	 27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
	 65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
	105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	 72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
	122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140
};


static ssize_t rlxvm_kbd_store_keys(struct device *dev,
				        struct device_attribute *attr,
				        const char *buf, size_t count)
{
	return 0;
}

static DEVICE_ATTR(keys, S_IWUSR | S_IRUGO,
		    NULL,
		    rlxvm_kbd_store_keys);

static struct attribute *rlxvm_kbd_attrs[] = {
	&dev_attr_keys.attr,
	NULL
};

static struct attribute_group rlxvm_kbd_attr_group = {
	.attrs = rlxvm_kbd_attrs,
};

static irqreturn_t rlxvm_kbd_isr(int irq, void *args)
{
	struct rlxvm_kbd *kbd = args;
	SDL_Event *event = (SDL_Event *)(kbd->mmio + 0x100);
	const SDL_Keysym *ksym = &(event->key.keysym);

	switch (event->type) {
	case SDL_KEYDOWN:
		input_report_key(kbd->idev, sdl_kbd_keycode[ksym->scancode], 1);
		break;
	case SDL_KEYUP:
		input_report_key(kbd->idev, sdl_kbd_keycode[ksym->scancode], 0);
		break;
	default:
		break;
	}

	input_sync(kbd->idev);

	/* Clear inerrupt flag */
	readb(kbd->mmio + 0x10);

	return IRQ_HANDLED;
}

static int rlxvm_kbd_probe(struct platform_device *pdev)
{
	struct rlxvm_kbd *kbd;
	struct input_dev *input;
	struct resource *res;
	int i, err;

	kbd = devm_kzalloc(&pdev->dev, sizeof(*kbd), GFP_KERNEL);
	if (!kbd) {
		dev_err(&pdev->dev, "failed to alloc memory for rlxvm_kbd\n");
		return -ENOMEM;
	}

	kbd->irq = platform_get_irq(pdev, 0);
	if (kbd->irq < 0) {
		dev_err(&pdev->dev, "failed to get keyboard IRQ\n");
		return -ENXIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	kbd->mmio = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(kbd->mmio))
		return PTR_ERR(kbd->mmio);

	input = devm_input_allocate_device(&pdev->dev);
	if (!input) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	kbd->dev = &pdev->dev;
	kbd->idev = input;
	input_set_drvdata(input, kbd);

	input->name = pdev->name;
	input->dev.parent = &pdev->dev;

	input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input->rep[REP_DELAY] = 250;
	input->rep[REP_PERIOD] = 33;

	for (i = 0; i < 255; i++)
		set_bit(sdl_kbd_keycode[i], input->keybit);
	clear_bit(0, input->keybit);

	err = input_register_device(input);
	if (err) {
		dev_err(&pdev->dev, "failed to register input device\n");
		return err;
	}

	err = devm_request_irq(&pdev->dev, kbd->irq, rlxvm_kbd_isr,
			       IRQF_TRIGGER_RISING, pdev->name, kbd);
	if (err) {
		dev_err(&pdev->dev, "failed to request keyboard IRQ\n");
		return err;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &rlxvm_kbd_attr_group);
	if (err) {
		dev_err(&pdev->dev, "Unable to export keys\n");
		sysfs_remove_group(&pdev->dev.kobj, &rlxvm_kbd_attr_group);
		return err;
	}

	platform_set_drvdata(pdev, kbd);

	/* Enable keyboard module */
	readb(kbd->mmio + 0x20);

	return 0;
}

static int rlxvm_kbd_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &rlxvm_kbd_attr_group);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id rlxvm_kbd_dt_match_table[] = {
	{ .compatible = "realtek,rlxvm_kbd" },
	{},
};
MODULE_DEVICE_TABLE(of, rlxvm_kbd_dt_match_table);
#endif

static struct platform_driver rlxvm_kbd_driver = {
	.probe		= rlxvm_kbd_probe,
	.remove		= rlxvm_kbd_remove,
	.driver		= {
		.name	= "rlxvm_kbd",
		.of_match_table = of_match_ptr(rlxvm_kbd_dt_match_table),
	},
};
module_platform_driver(rlxvm_kbd_driver);

MODULE_AUTHOR("Realtek PSP software");
MODULE_DESCRIPTION("RLXVM Virtual Keypad Driver");
MODULE_LICENSE("GPL");
