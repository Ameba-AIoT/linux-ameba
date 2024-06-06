// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Setup support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/system_misc.h>

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>

#include <linux/init.h>
#include <linux/io.h>

#include <linux/reboot.h>
#include <linux/sysfs.h>
#include <mach/hardware.h>
#include <misc/realtek-misc.h>

#define BOOTLOADER_PHYSICAL_ADDR     0x42008D04UL
#define BOOTLOADER_IMAGE2_PHY_ADDR   0x80220001UL

#define LSYS_MASK_CI_EN              ((u32)0x0000000F << 28)
#define LSYS_CI_EN(x)                ((u32)((x) & 0x0000000F) << 28)
#define LSYS_GET_RLV(x)              ((u32)(((x >> 16) & 0x0000000F)))

/* SYSTEM_CTRL_BASE_LP */
static void __iomem *plat_lsys_base = NULL;
static void __iomem *plat_lsys_base_ctrl = NULL;

/* allocate io resource */
static struct map_desc bsp_io_desc[] __initdata = {
	{
		.virtual = BSP_SMP_VADDR,
		.pfn = __phys_to_pfn(BSP_SMP_PADDR),
		.length = SZ_4,
		.type = MT_DEVICE_NONSHARED,
	},
	{
		.virtual = BSP_EARLYCON_VADDR,
		.pfn = __phys_to_pfn(BSP_EARLYCON_PADDR),
		.length = SZ_256,
		.type = MT_DEVICE,
	},
};

int rtk_misc_get_rlv(void)
{
	int result = -1;

	if (plat_lsys_base_ctrl != NULL) {
		u32 value;
		u32 value32;

		value = readl(plat_lsys_base_ctrl);
		value &= ~(LSYS_MASK_CI_EN);
		value |= LSYS_CI_EN(0xA);
		writel(value, plat_lsys_base_ctrl);

		value32 = readl(plat_lsys_base_ctrl);
		value &= ~(LSYS_MASK_CI_EN);
		writel(value, plat_lsys_base_ctrl);

		result = (int)(LSYS_GET_RLV(value32));
	}

	return result;
}

EXPORT_SYMBOL(rtk_misc_get_rlv);

static void __init plat_map_io(void)
{
	extern void plat_smp_map_io(void);

	iotable_init(bsp_io_desc, ARRAY_SIZE(bsp_io_desc));
#ifdef CONFIG_SMP
	plat_smp_map_io();
#endif
}

static void plat_arch_restart(enum reboot_mode mode, const char *cmd)
{
	if (plat_lsys_base) {
		writel(0U, plat_lsys_base + REG_AON_SYSRST_MSK);
		writel(0U, plat_lsys_base + REG_LSYS_SYSRST_MSK0);
		writel(0U, plat_lsys_base + REG_LSYS_SYSRST_MSK1);
		writel(0U, plat_lsys_base + REG_LSYS_SYSRST_MSK2);

		writel(SYS_RESET_KEY, plat_lsys_base + REG_LSYS_SW_RST_TRIG);
		writel(LSYS_BIT_LPSYS_RST << AP_CPU_ID, plat_lsys_base + REG_LSYS_SW_RST_CTRL);
		writel(SYS_RESET_TRIG, plat_lsys_base + REG_LSYS_SW_RST_TRIG);
	}
}

static void __init plat_init_machine(void)
{
	struct device_node *np;
	int rlv;

	np = of_find_compatible_node(NULL, NULL, "realtek,ameba-system-ctrl-ls");
	if (np) {
		plat_lsys_base = of_iomap(np, 0);
		if (plat_lsys_base) {
			arm_pm_restart = plat_arch_restart;
		} else {
			pr_err("%s:of_iomap(plat_lsys_base) failed\n", __func__);
		}

		plat_lsys_base_ctrl = of_iomap(np, 1);
		if (!plat_lsys_base_ctrl) {
			pr_err("%s:of_iomap(plat_lsys_base_ctrl) failed\n", __func__);
		}

		of_node_put(np);
	}

	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	rlv = rtk_misc_get_rlv();
	if (rlv >= 0) {
		pr_info("RLV: %d\n", rlv);
	}
}

#ifdef CONFIG_SMP
extern struct smp_operations plat_smp_ops;
#endif

/* Realtek Ameba DT machine */
static const char *const plat_dt_match[] __initconst = {
	"realtek,ameba",
	NULL
};

DT_MACHINE_START(RLXARM_DT, "Realtek ARM (Flattened Device Tree)")
	.dt_compat = plat_dt_match,
	.init_machine = plat_init_machine,
	.map_io = plat_map_io,
#ifdef CONFIG_SMP
	.smp = smp_ops(plat_smp_ops),
#endif
MACHINE_END
