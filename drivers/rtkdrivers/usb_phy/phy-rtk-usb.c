// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek USB PHY support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/usb/otg.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/export.h>
#include "phy-rtk-usb.h"

#define USB_CAL_OTP_EN 0

/* USB PHY defines */
#define PHY_HIGH_ADDR(x)			(((x) & 0xF0) >> 4)
#define PHY_LOW_ADDR(x)				((x) & 0x0F)
#define PHY_DATA_MASK				0xFF

#define DRIVER_NAME "realtek-otg-phy"

#define to_rtk_phy(p) container_of((p), struct rtk_phy, phy)

struct rtk_phy {
	struct usb_phy phy;
	struct clk *clk;
	void __iomem *sys_base;
	void __iomem *otpc_base;
	void __iomem *lsys_aip_ctrl1;
	u8 is_cal;
};

struct rtk_usb_phy_cal_data_t {
	u8 page;
	u8 addr;
	u8 val;
};

static const struct of_device_id rtk_phy_dt_ids[] = {
	{ .compatible = "realtek,otg-phy", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, rtk_phy_dt_ids);


#if USB_CAL_OTP_EN

/**
  * @brief  Get USB PHY calibration data
  */
static u32 rtk_phy_get_cal_data(struct usb_phy *phy)
{
	struct rtk_phy *rtk_phy = to_rtk_phy(phy);
	void __iomem *otpc_base = rtk_phy->otpc_base;
	u32 data = 0;

	data = readl(otpc_base + SEC_OTP_SYSCFG2);
	return data;
}

/**
  * @brief  Get USB PHY calibration data 2
  */
static u32 rtk_phy_get_cal_data2(struct usb_phy *phy)
{
	struct rtk_phy *rtk_phy = to_rtk_phy(phy);
	void __iomem *otpc_base = rtk_phy->otpc_base;
	u32 data = 0;

	data = readl(otpc_base + SEC_OTP_SYSCFG3);
	return data;
}

#endif

static int rtk_load_vcontrol(struct dwc2_hsotg *hsotg, u8 addr)
{
	u32 pvndctl = 0x0A300000;
	u32 count = 0;

	pvndctl |= (PHY_LOW_ADDR(addr) << USB_OTG_GPVNDCTL_VCTRL_Pos);
	writel(pvndctl, hsotg->regs + USB_OTG_GPVNDCTL);

	do {
		/* 1us timeout expected, 1ms for safe */
		usleep_range(1, 2);
		if (++count > 1000) {
			dev_err(hsotg->dev, "Vendor control timeout\n");
			return -ETIMEDOUT;
		}

		pvndctl = readl(hsotg->regs + USB_OTG_GPVNDCTL);
	} while ((pvndctl & USB_OTG_GPVNDCTL_VSTSDONE) == 0);

	return 0;
}

static int rtk_phy_write(struct dwc2_hsotg *hsotg, u8 addr, u8 val)
{
	void __iomem *addon_base = hsotg->uphy->io_priv;
	u32 tmp;
	int ret = 0;

	tmp = readl(addon_base + USB_OTG_ADDON_REG_VND_STS_OUT);
	tmp &= (~PHY_DATA_MASK);
	tmp |= val;
	writel(tmp, addon_base + USB_OTG_ADDON_REG_VND_STS_OUT);

	ret = rtk_load_vcontrol(hsotg, PHY_LOW_ADDR(addr));
	if (ret == 0) {
		ret = rtk_load_vcontrol(hsotg, PHY_HIGH_ADDR(addr));
	}

	return ret;
}

static int rtk_phy_read(struct dwc2_hsotg *hsotg, u8 addr, u8 *val)
{
	u32 pvndctl;
	int ret = 0;
	u8 addr_read = addr - 0x20;

	ret = rtk_load_vcontrol(hsotg, PHY_LOW_ADDR(addr_read));
	if (ret == 0) {
		ret = rtk_load_vcontrol(hsotg, PHY_HIGH_ADDR(addr_read));
		if (ret == 0) {
			pvndctl = readl(hsotg->regs + USB_OTG_GPVNDCTL);
			*val = (pvndctl & USB_OTG_GPVNDCTL_REGDATA_Msk) >> USB_OTG_GPVNDCTL_REGDATA_Pos;
		}
	}

	return ret;
}

static int rtk_phy_page_set(struct dwc2_hsotg *hsotg, u8 page)
{
	int ret;
	u8 reg;

	ret = rtk_phy_read(hsotg, USB_OTG_PHY_REG_F4, &reg);
	if (ret != 0) {
		return ret;
	}

	reg &= (~USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK);
	reg |= (page << USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_POS) & USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK;
	ret = rtk_phy_write(hsotg, USB_OTG_PHY_REG_F4, reg);

	return ret;
}

/**
  * @brief  USB PHY calibration
  *			Shall be called after soft disconnect
  * @param  None
  * @retval HAL status
  */
int rtk_phy_calibrate(struct dwc2_hsotg *hsotg)
{
	u8 ret = 0;
#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	u8 reg;
#endif
	u8 old_page = 0xFF;
	struct rtk_usb_phy_cal_data_t *data = NULL;
	struct rtk_usb_phy_cal_data_t *data_tmp = NULL;
	struct rtk_phy *rtk_phy = NULL;
	struct device_node *np = NULL;
	u32 *cal_data = NULL;
	u32 *cal_data_tmp = NULL;
	int prop_size = 0;
	int num_entries = 0;
	const int entry_size = 3;
	int i = 0;

	if (!hsotg || !hsotg->uphy) {
		return -EINVAL;
	}

	rtk_phy = to_rtk_phy(hsotg->uphy);
	if (rtk_phy->is_cal != 0) {
		return 0;
	}

	np = rtk_phy->phy.dev->of_node;
	if (!np) {
		dev_err(hsotg->dev, "Failed to get device node\n");
		return -EINVAL;
	}

	prop_size = of_property_count_elems_of_size(np, "rtk,cal-data", sizeof(uint32_t));
	if ((prop_size <= 0) || (prop_size % 3 != 0)) {
		dev_err(hsotg->dev, "Property 'rtk,phydata' size %d is invalid or not meet the format\n", prop_size);
		return -EINVAL;
	}

	cal_data = devm_kzalloc(hsotg->dev, prop_size * sizeof(uint32_t), GFP_KERNEL);
	if (!cal_data) {
		dev_err(hsotg->dev, "Failed to malloc cal_data\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	num_entries = prop_size / entry_size;
	data = devm_kzalloc(hsotg->dev, num_entries * sizeof(struct rtk_usb_phy_cal_data_t), GFP_KERNEL);
	if (!data){
		dev_err(hsotg->dev, "Failed to malloc data\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	ret = of_property_read_u32_array(np, "rtk,cal-data", cal_data, prop_size);
	if (ret) {
		dev_err(hsotg->dev, "Failed to read 'rtk,phydata'\n");
		goto cleanup;
	}

	for (i = 0; i < num_entries; i++) {
		data_tmp = data + i;
		cal_data_tmp = cal_data + i * entry_size;
		data_tmp->page = (u8)*(cal_data_tmp);
		data_tmp->addr = (u8)*(cal_data_tmp + 1);
		data_tmp->val = (u8)*(cal_data_tmp + 2);
	}

	devm_kfree(hsotg->dev, cal_data);

#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	ret = rtk_phy_page_set(hsotg, USB_OTG_PHY_REG_F4_BIT_PAGE1);
	if (ret != 0) {
		dev_err(hsotg->dev, "Failed to select page 1: %d\n", ret);
		goto cleanup;
	}

	ret = rtk_phy_read(hsotg, USB_OTG_PHY_REG_E1, &reg);
	if (ret != 0) {
		dev_err(hsotg->dev, "Failed to read USB_OTG_PHY_REG_E1: %d\n", ret);
		goto cleanup;
	}

	reg |= USB_OTG_PHY_REG_E1_PAGE1_BIT_EN_OTG;
	ret = rtk_phy_write(hsotg, USB_OTG_PHY_REG_E1, reg);
	if (ret != 0) {
		dev_err(hsotg->dev, "Failed to write USB_OTG_PHY_REG_E1: %d\n", ret);
		goto cleanup;
	}
#endif

	/* 3ms + 2.5us from DD, 3ms already delayed after soft disconnect */
	usleep_range(3, 4);

	i = 0;
	while (i < num_entries) {
		data_tmp = data + i;
		if (data_tmp->page != old_page) {
			ret = rtk_phy_page_set(hsotg, data_tmp->page);
			if (ret != 0) {
				dev_err(hsotg->dev, "Failed to switch to page %d: %d\n",  data_tmp->page, ret);
				goto cleanup;
			}
			old_page = data_tmp->page;
		}
		ret = rtk_phy_write(hsotg, data_tmp->addr, data_tmp->val);
		if (ret != 0) {
			dev_err(hsotg->dev, "Failed to write page %d register 0x%02X: %d\n", data_tmp->page, data_tmp->addr, ret);
			goto cleanup;
		}
		i++;
	}

	devm_kfree(hsotg->dev, data);
	rtk_phy->is_cal = 1;
	return ret;

cleanup:
	if(cal_data != NULL){
		devm_kfree(hsotg->dev, cal_data);
	}
	if(data != NULL){
		devm_kfree(hsotg->dev, data);
	}
	rtk_phy->is_cal = 0;
	return ret;
}
EXPORT_SYMBOL_GPL(rtk_phy_calibrate);

static int rtk_phy_init(struct usb_phy *phy)
{
	int ret;
	u32 reg;
	u32 count = 0;
	struct rtk_phy *rtk_phy = to_rtk_phy(phy);
	void __iomem *addon_base = rtk_phy->phy.io_priv;
	void __iomem *base = rtk_phy->sys_base;
	void __iomem *lsys_aip_ctrl1 = rtk_phy->lsys_aip_ctrl1;

	reg = readl(lsys_aip_ctrl1);
	reg |= (LSYS_BIT_BG_PWR | LSYS_BIT_BG_ON_USB2);
	reg &= ~LSYS_MASK_BG_ALL;
	reg |= LSYS_BG_ALL(0x2);
	writel(reg, lsys_aip_ctrl1);

	ret = clk_prepare_enable(rtk_phy->clk);
	if (ret) {
		return ret;
	}

	/* USB Power Sequence */
	/* USB digital pad en,dp/dm sharing GPIO PAD */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~(HSYS_BIT_USB2_DIGOTGPADEN | HSYS_BIT_USB_OTGMODE | HSYS_BIT_USB2_DIGPADEN);
#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	reg |= (HSYS_BIT_USB_OTGMODE | HSYS_BIT_OTG_ANA_EN);
#endif
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* USB PWC_UALV_EN,  PWC_UAHV_EN */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= (HSYS_BIT_PWC_UALV_EN | HSYS_BIT_PWC_UAHV_EN);
	writel(reg, base + REG_HSYS_USB_CTRL);
	usleep_range(2, 3);

	/* USB PWC_UABG_EN */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= HSYS_BIT_PWC_UABG_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);
	usleep_range(10, 15);

	/* USB ISO_USBD_EN = 0 => disable isolation output signal from PD_USBD*/
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~HSYS_BIT_ISO_USBA_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);
	usleep_range(10, 15);

	/* USBPHY_EN = 1 */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg &= ~USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_MASK;
	reg |= (0x3U << USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_POS); // Inter-packet gap 343ns, spec 399ns
	reg |= (USB_OTG_ADDON_REG_CTRL_BIT_USB_DPHY_FEN | USB_OTG_ADDON_REG_CTRL_BIT_USB_APHY_EN | USB_OTG_ADDON_REG_CTRL_BIT_LS_HST_UTMI_EN);
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);
	usleep_range(34, 40);

	/* Wait UPLL_CKRDY */
	do {
		/* 1ms timeout expected, 10ms for safe */
		usleep_range(10, 15);
		if (++count > 1000U) {
			dev_err(phy->dev, "USB PHY init timeout\n");
			return -ETIMEDOUT;
		}
	} while (!(readl(addon_base + USB_OTG_ADDON_REG_CTRL) & USB_OTG_ADDON_REG_CTRL_BIT_UPLL_CKRDY));

	/* USBOTG_EN = 1 => enable USBOTG */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg |= USB_OTG_ADDON_REG_CTRL_BIT_USB_OTG_RST;
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);

	dev_info(phy->dev, "USB PHY initialized\n");

	return ret;
}

static void rtk_phy_shutdown(struct usb_phy *phy)
{
	u32 reg = 0;
	struct rtk_phy *rtk_phy = to_rtk_phy(phy);
	void __iomem *addon_base = rtk_phy->phy.io_priv;
	void __iomem *base = rtk_phy->sys_base;
	void __iomem *lsys_aip_ctrl1 = rtk_phy->lsys_aip_ctrl1;

	/* USBOTG_EN = 0 => disable USBOTG */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg &= (~USB_OTG_ADDON_REG_CTRL_BIT_USB_OTG_RST);
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);

	/* USBPHY_EN = 0 */
	reg = readl(addon_base + USB_OTG_ADDON_REG_CTRL);
	reg &= (~(USB_OTG_ADDON_REG_CTRL_BIT_USB_DPHY_FEN | USB_OTG_ADDON_REG_CTRL_BIT_USB_APHY_EN));
	writel(reg, addon_base + USB_OTG_ADDON_REG_CTRL);

	/* USB ISO_USBD_EN = 1 => enable isolation output signal from PD_USBD*/
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= HSYS_BIT_ISO_USBA_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* USB PWC_UABG_EN = 0 */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~HSYS_BIT_PWC_UABG_EN;
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* PWC_UPHV_EN  = 0 => disable USBPHY analog 3.3V power */
	/* PWC_UPLV_EN = 0 => disable USBPHY analog 1.2V power */
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg &= ~(HSYS_BIT_PWC_UALV_EN | HSYS_BIT_PWC_UAHV_EN);
	writel(reg, base + REG_HSYS_USB_CTRL);

	/* USB digital pad disable*/
	reg = readl(base + REG_HSYS_USB_CTRL);
	reg |= (HSYS_BIT_USB2_DIGOTGPADEN | HSYS_BIT_USB2_DIGPADEN);
#if IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	reg &= ~(HSYS_BIT_USB_OTGMODE | HSYS_BIT_OTG_ANA_EN);
#endif
	writel(reg, base + REG_HSYS_USB_CTRL);

	clk_disable_unprepare(rtk_phy->clk);

	reg = readl(lsys_aip_ctrl1);
	reg &= ~LSYS_BIT_BG_ON_USB2;
	writel(reg, lsys_aip_ctrl1);

	dev_info(phy->dev, "USB PHY shutdown\n");
}

static int rtk_phy_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *base;
	void __iomem *sys_base;
	void __iomem *otpc_base;
	void __iomem *lsys_aip_ctrl1;
	struct clk *clk;
	struct rtk_phy *rtk_phy;
	int ret;
	const struct of_device_id *of_id;

	of_id = of_match_device(rtk_phy_dt_ids, &pdev->dev);
	if (!of_id) {
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		return PTR_ERR(base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	sys_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sys_base)) {
		return PTR_ERR(sys_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	otpc_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(otpc_base)) {
		return PTR_ERR(otpc_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	lsys_aip_ctrl1 = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(lsys_aip_ctrl1)) {
		return PTR_ERR(lsys_aip_ctrl1);
	}

	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		return PTR_ERR(clk);
	}

	rtk_phy = devm_kzalloc(&pdev->dev, sizeof(*rtk_phy), GFP_KERNEL);
	if (!rtk_phy) {
		dev_err(&pdev->dev, "Failed to allocate rtk_phy\n");
		return -ENOMEM;
	}

	rtk_phy->phy.io_priv = base;
	rtk_phy->phy.dev = &pdev->dev;
	rtk_phy->phy.label = DRIVER_NAME;
	//rtk_phy->phy.init = rtk_phy_init;
	//rtk_phy->phy.shutdown = rtk_phy_shutdown;
	rtk_phy->phy.type = USB_PHY_TYPE_USB2;

	rtk_phy->clk = clk;
	rtk_phy->sys_base = sys_base;
	rtk_phy->otpc_base = otpc_base;
	rtk_phy->lsys_aip_ctrl1 = lsys_aip_ctrl1;
	rtk_phy->is_cal = 0;

	platform_set_drvdata(pdev, rtk_phy);

	device_set_wakeup_capable(&pdev->dev, false);

	ret = usb_add_phy_dev(&rtk_phy->phy);

	rtk_phy_init(&rtk_phy->phy); // TBD

	return ret;
}

static int rtk_phy_remove(struct platform_device *pdev)
{
	struct rtk_phy *rtk_phy = platform_get_drvdata(pdev);

	rtk_phy_shutdown(&rtk_phy->phy); // TBD
	rtk_phy->is_cal = 0;

	usb_remove_phy(&rtk_phy->phy);

	return 0;
}

static struct platform_driver rtk_phy_driver = {
	.probe = rtk_phy_probe,
	.remove = rtk_phy_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = rtk_phy_dt_ids,
	},
};

#if 0
module_platform_driver(rtk_phy_driver);
#else
static int __init rtk_phy_module_init(void)
{
	return platform_driver_register(&rtk_phy_driver);
}
postcore_initcall(rtk_phy_module_init);

static void __exit rtk_phy_module_exit(void)
{
	platform_driver_unregister(&rtk_phy_driver);
}
module_exit(rtk_phy_module_exit);
#endif

MODULE_DESCRIPTION("Realtek Ameba USB PHY driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
