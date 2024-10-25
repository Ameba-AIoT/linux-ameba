// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek PM support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/suspend.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/init.h>
#include <ameba_ipc/ameba_ipc.h>
#include <uapi/linux/psci.h>
#include <linux/psci.h>
#include <linux/arm-smccc.h>
#include <asm/suspend.h>
#include <linux/init.h>
#include <linux/delay.h>

#define REG_CTRL_BASE_LP			0x42008000
#define REG_LSYS_AP_STATUS_SW 		0x266
#define LSYS_BIT_AP_RUNNING			((u32)0x00000001 << 1)

#define REG_CTRL_BASE_HP			0x41000000
#define REG_HSYS_HP_CKE				0x000C
#define HSYS_BIT_CKE_AP				((u32)0x00000001 << 0)


#define IPC_PM_CHAN_NUM			0


typedef struct {
	u32	sleep_type;
	u32	sleep_time;
	u32	dlps_enable;
	u32	rsvd[5];
} SLEEP_ParamDef;


struct aipc_ch *pm_ch;
ipc_msg_struct_t pm_msg;
SLEEP_ParamDef pm_param;
void __iomem *reg_lsys_ap_status_sw;
void __iomem *reg_hsys_hp_cke;

static u32 rtk_pm_test(aipc_ch_t *ch, ipc_msg_struct_t *pmsg)
{
	pr_info("rtk_pm_test\n");
	return 0;
}


int rtk_psci_cpu_suspend(unsigned long arg)
{
	struct arm_smccc_res res;

	arm_smccc_smc(PSCI_0_2_FN_CPU_SUSPEND, 0, __pa_symbol(cpu_resume),
				  0, 0, 0, 0, 0, &res);

	return res.a0;
}

static struct aipc_ch_ops rtk_pm_ipc_ops = {
	.channel_recv = rtk_pm_test,
};


static int  rtk_pm_valid(suspend_state_t state)
{
	return (state == PM_SUSPEND_MEM) || (state == PM_SUSPEND_CG) || (state == PM_SUSPEND_PG);
}


static int rtk_pm_enter(suspend_state_t state)
{
	int ret;

	if (!pm_ch) {
		pm_ch = ameba_ipc_alloc_ch(sizeof(SLEEP_ParamDef) + 20);
		if (!pm_ch) {
			pr_err("ipc channel alloc failed\n");
		}

		pm_ch->port_id = AIPC_PORT_LP;
		pm_ch->ch_id = IPC_PM_CHAN_NUM;
		pm_ch->ch_config = AIPC_CONFIG_NOTHING;
		pm_ch->ops = &rtk_pm_ipc_ops;
		pm_ch->priv_data = NULL;

		ret = ameba_ipc_channel_register(pm_ch);
		if (ret) {
			pr_err("ipc channel register failed\n");
		}
	}

	if (state == PM_SUSPEND_CG) {
		pr_info("sleep mode cg\n");

		/* set flag that ap is not running */
		writeb(readb(reg_lsys_ap_status_sw) & (~ LSYS_BIT_AP_RUNNING),
			   reg_lsys_ap_status_sw);

		pm_param.sleep_type = 0x1;
		pm_param.sleep_time = 0;
		pm_param.dlps_enable = 0x0;
		clean_dcache_area(&pm_param, sizeof(ipc_msg_struct_t));
		ret = ameba_ipc_channel_send(pm_ch, (ipc_msg_struct_t *)&pm_param);
		if (ret < 0) {
			pr_err("ipc channel send failed: %d\n", ret);
		}

		/* disable cke by itself */
		writel(readl(reg_hsys_hp_cke) & (~HSYS_BIT_CKE_AP),
			   reg_hsys_hp_cke);

		/* Non-wakeup-source interrupt has been disabled, so wfi is safe */
		wfi();

	} else if (state == PM_SUSPEND_PG) {
		pr_info("sleep mode pg\n");

		/* set flag that ap is not running */
		writeb(readb(reg_lsys_ap_status_sw) & (~ LSYS_BIT_AP_RUNNING),
			   reg_lsys_ap_status_sw);

		cpu_suspend(0, rtk_psci_cpu_suspend);

	} else {
		pr_info("sleep mode standby\n");
		wfi();
	}

	return 0;
}


static const struct platform_suspend_ops rtk_pm_ops = {
	.valid	= rtk_pm_valid,
	.enter	= rtk_pm_enter,
};


static int __init rtk_pm_init(void)
{
	pr_info("rtk_pm_init\n");

	reg_lsys_ap_status_sw = ioremap(REG_CTRL_BASE_LP + REG_LSYS_AP_STATUS_SW, 1);
	if (!reg_lsys_ap_status_sw) {
		pr_err("Remap register REG_LSYS_AP_STATUS_SW fail\n");
		return -ENOMEM;
	}

	reg_hsys_hp_cke = ioremap(REG_CTRL_BASE_HP + REG_HSYS_HP_CKE, 4);
	if (!reg_hsys_hp_cke) {
		pr_err("Remap register REG_HSYS_HP_CKE fail\n");
		return -ENOMEM;
	}

	suspend_set_ops(&rtk_pm_ops);

	return 0;
}

arch_initcall(rtk_pm_init);
