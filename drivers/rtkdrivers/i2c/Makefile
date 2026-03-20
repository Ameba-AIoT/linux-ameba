obj-$(CONFIG_I2C_RTK_AMEBA)	+= i2c-realtek.o
i2c-realtek-objs			:= rtk-i2c-core.o rtk-i2c-master.o
ifeq ($(CONFIG_I2C_SLAVE_RTK_AMEBA),y)
	i2c-realtek-objs		+= rtk-i2c-slave.o
endif
