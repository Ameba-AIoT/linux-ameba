# Realtek Semiconductor Corp.
#
# bsp/Makefile
#     bsp Makefile
#
# Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
#

#
# Ameba
#
textofs-y	:= 0x00388000
core-y		+= arch/arm/mach-amebasmart/

KBUILD_CPPFLAGS += -I$(srctree)/arch/arm/mach-amebasmart/ -I$(srctree)/arch/arm/mach-amebasmart/include

obj-y := setup.o
obj-$(CONFIG_SMP) += smp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o
obj-$(CONFIG_SUSPEND) += pm.o

##
## uncomment following lines for customized linker script
##
#CPPFLAGS_vmlinux.lds := -DTEXT_OFFSET=$(TEXT_OFFSET)
#export KBUILD_LDS := arch/arm64/bsp/vmlinux.lds
#extra-y += vmlinux.lds
