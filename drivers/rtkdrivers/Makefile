# SPDX-License-Identifier: GPL-2.0-only
# drm/tegra depends on host1x, so if both drivers are built-in care must be
# taken to initialize them in the correct order. Link order is the only way
# to ensure this currently.

obj-$(CONFIG_OTP_RTK_AMEBA)		        += otp/
obj-$(CONFIG_MFD_RTK_AMEBA_TIMERS)		+= mfd_timer/
obj-$(CONFIG_DRM_RTK_AMEBA)				+= drm/
obj-$(CONFIG_IPC_RTK_AMEBA)				+= ipc/
obj-$(CONFIG_LEDC_RTK_WS28XXX)			+= ledc/
obj-$(CONFIG_CLK_RTK_AMEBA_RCC)			+= clk/
obj-$(CONFIG_USB_RTK_AMEBA_USB20PHY)	+= usb_phy/
obj-$(CONFIG_ADC_RTK_AMEBA)				+= adc/
obj-$(CONFIG_CAPTOUCH_RTK_AMEBA)		+= captouch/
obj-$(CONFIG_CRYPTO_RTK_AMEBA)			+= crypto/
obj-$(CONFIG_GPIO_RTK_AMEBA)			+= gpio/
obj-$(CONFIG_THERMAL_RTK_AMEBA)			+= thermal/
obj-$(CONFIG_PINCTRL_RTK_AMEBA)			+= pinctrl/
obj-$(CONFIG_WATCHDOG_RTK_AMEBA)		+= watchdog/
obj-$(CONFIG_I2C_RTK_AMEBA)				+= i2c/
obj-$(CONFIG_I2C_SLAVE_RTK_AMEBA)		+= i2c/
obj-$(CONFIG_DMAC_RTK_AMEBA)			+= dma/
obj-$(CONFIG_SERIAL_RTK_AMEBA)			+= tty_serial/
obj-$(CONFIG_PWM_RTK_AMEBA)				+= pwm/
obj-$(CONFIG_RTC_RTK_AMEBA)   			+= rtc/
obj-$(CONFIG_MMC_RTK_AMEBA_SDIOH)		+= mmc/
obj-$(CONFIG_TOUCHSCREEN_RTK_AMEBA)		+= touchscreen/
obj-$(CONFIG_SPI_RTK_AMEBA)				+= spi/
obj-$(CONFIG_SPIC_RTK_AMEBA_RXI312)		+= spic/
obj-$(CONFIG_CPUFREQ_RTK_AMEBA)			+= cpufreq/
obj-$(CONFIG_IR_RTK_AMEBA)				+= ir/
obj-$(CONFIG_CLOCKSOURCE_RTK_AMEBA)		+= clocksource/
obj-$(CONFIG_RTK_MRDUMP)				+= mrdump/
obj-$(CONFIG_MISC_RTK_AMEBA)			+= misc/
obj-$(CONFIG_KM4_CONSOLE_AMEBA)			+= km4_console/
obj-$(CONFIG_FULLMAC_RTK_RTL8730E)		+= net/wireless/cfg80211_fullmac/rtl8730e/
obj-$(CONFIG_BT_IOCTL_RTK_AMEBA)		+= bluetooth/
obj-$(CONFIG_OTP_IPC_RTK_AMEBA_CDEV)	+= otp_ipc/
