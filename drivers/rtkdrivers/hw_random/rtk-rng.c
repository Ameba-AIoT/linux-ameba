/*
* Copyright (c) 2023 Realtek, LLC.
* All rights reserved.
*
* Licensed under the Realtek License, Version 1.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License from Realtek
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/random.h>

#define RNGC_STATUS				0x000C
#define RNGC_FIFO				0x0010
#define RNGC_SW_RESET			0x0038
#define RNGC_TIMEOUT			0xFFFF

#define RNG_FIFO_LEVEL_EMPTY	0x0
#define RNG_FIFO_LEVEL_INVALID	0xF

#define RNG_BIT_SW_RESET		((u32)0x00000001 >> 0)

#define USEC_POLL				4
#define TIMEOUT_POLL			20

#define to_rtk_rng(p)	container_of(p, struct rtk_rng, rng)

struct rtk_rng {
	void __iomem *base;
	struct hwrng rng;
};

static int rtk_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	struct rtk_rng *hrng = to_rtk_rng(rng);
	int retval = 0;
	u32 *data = buf;
	u32 counter = 0;
	unsigned int status;

	while (max >= sizeof(u32)) {
		status = readl_relaxed(hrng->base + RNGC_STATUS);
		if (status == RNG_FIFO_LEVEL_INVALID) {
			writel_relaxed(RNG_BIT_SW_RESET, hrng->base + RNGC_SW_RESET);
			if (wait)
				retval = readl_relaxed_poll_timeout_atomic(hrng->base + RNGC_STATUS, status,
							!(status & RNG_FIFO_LEVEL_INVALID), USEC_POLL,
							TIMEOUT_POLL);

			if (status == RNG_FIFO_LEVEL_INVALID) {
				dev_err((struct device *)hrng->rng.priv, "%s: bad status!\n", __func__);
				break;
			}
		}

		if (status != RNG_FIFO_LEVEL_EMPTY) {
			*data = readl_relaxed(hrng->base + RNGC_FIFO);
			data++;
			retval += sizeof(u32);
			max -= sizeof(u32);
			dev_dbg((struct device *)hrng->rng.priv, "%s data: %x!\n", __func__, *data);
		} else if (counter++ > RNGC_TIMEOUT) { //polling timeout
			/*sw reset*/
			writel_relaxed(RNG_BIT_SW_RESET, hrng->base + RNGC_SW_RESET);
			break;
		}
	}

	return retval ? retval : -EIO;
}

static int rtk_rng_probe(struct platform_device *pdev)
{
	struct rtk_rng *rng;
	struct resource *res;
	int ret;

	rng = devm_kzalloc(&pdev->dev, sizeof(*rng), GFP_KERNEL);
	if (!rng)
		return -ENOMEM;

	platform_set_drvdata(pdev, rng);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rng->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rng->base))
		return PTR_ERR(rng->base);

	rng->rng.name = pdev->name;
	// rng->rng.init = rtk_rng_init;
	// rng->rng.cleanup = rtk_rng_cleanup;
	rng->rng.read = rtk_rng_read;

	rng->rng.priv = (unsigned long) (&pdev->dev);
	rng->rng.quality = 1024;

	ret = devm_hwrng_register(&pdev->dev, &rng->rng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		return ret;
	}

	dev_info(&pdev->dev, "rtk hwrng probe success\n");

	return 0;
}

static const struct of_device_id rtk_rng_dt_ids[] = {
	{ .compatible = "rtk,ameba-trng" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtk_rng_dt_ids);

static struct platform_driver rtk_rng_driver = {
	.probe		= rtk_rng_probe,
	.driver		= {
		.name	= "rtk-rng",
		.of_match_table = of_match_ptr(rtk_rng_dt_ids),
	},
};

module_platform_driver(rtk_rng_driver);

MODULE_AUTHOR("Realtek Corporation");
MODULE_DESCRIPTION("Realtek Ameba Hardware Random driver");
MODULE_LICENSE("GPL v2");
