// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <stddef.h>
#include <uapi/sound/rtk_audio_pll.h>

#include "ameba_audio_clock.h"
#include "ameba_audio_clock_driver.h"

#define SPORT_INDEX_MAX 4

#define DEV_NAME audio_pll_clk
int audio_pll_clk_open(struct inode *inode, struct file *filp);
int audio_pll_clk_release(struct inode *indoe, struct file *filp);
long int audio_pll_clk_ioctl (struct file *filp, unsigned int cmd, unsigned long args);

struct cdev *audio_pll_cdev;
struct class *realtek_audio_pll_class;
dev_t devno;

struct audio_clock_data {
	struct platform_device *pdev;
	void __iomem *addr;
	/* Physical base address of SFRs */
	u32 base;
	u32 reg_size;
	void __iomem *system_control_base_lp;
	u32 pll_clk_for_sport[SPORT_INDEX_MAX];
};

static int audio_pll_micro_adjust(struct micro_adjust_params *args)
{
	int res = 0;
	struct micro_adjust_params tmp;
	struct micro_adjust_params * p_tmp = &tmp;
	if (copy_from_user((unsigned char*)p_tmp, (unsigned char*)args, sizeof(struct micro_adjust_params))){
		pr_info("error copy from user");
		return -EFAULT;
	}
	if (p_tmp->clock == PLL_24P576M)
		res = pll_i2s_24P576M_clk_tune(p_tmp->ppm, p_tmp->action);
	else if (p_tmp->clock == PLL_45P1584M)
		res = pll_i2s_45P1584M_clk_tune(p_tmp->ppm, p_tmp->action);
	else if (p_tmp->clock == PLL_98P304M)
		res = pll_i2s_98P304M_clk_tune(p_tmp->ppm, p_tmp->action);

    //pr_info("AUDIO_PLL_MICRO_ADJUST clock:%d, action:%d, ppm:%d ", p_tmp->clock, p_tmp->action, p_tmp->ppm);

	return res;
}

static int audio_pll_get_clock_info(struct audio_clock_info * args)
{
	u32 sport_index = 0;
	struct audio_clock_info info;
	struct audio_clock_info * p_info = &info;

	if (get_user(sport_index, (unsigned __user *)(&args->sport_index)))
		return -EFAULT;

	info.sport_index = sport_index;
	info.clock = get_audio_clock_info_for_sport(sport_index);
	//pr_info("sport_index:%d clock info:%d", sport_index, info.clock);

	if (copy_to_user((struct audio_clock_info*)args, p_info, sizeof(struct audio_clock_info))) {
		pr_err("AUDIO_PLL_GET_CLOCK_FOR_SPORT copy to user fail");
		return -EFAULT;
	}
	return 0;
}

long int audio_pll_clk_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	int res = 0;
    switch (cmd) {
        case AUDIO_PLL_MICRO_ADJUST:
			res = audio_pll_micro_adjust((struct micro_adjust_params *)args);
			break;
		case AUDIO_PLL_GET_CLOCK_FOR_SPORT:
			res = audio_pll_get_clock_info((struct audio_clock_info *)args);
			break;
        default:
            return -EFAULT;
    }
    return res;
}

//Open System Call
int audio_pll_clk_open(struct inode *inode, struct file *filp)
{
    return 0;
}

//Close System Call
int audio_pll_clk_release(struct inode *indoe, struct file *filp)
{
    return 0;
}

//Structure that defines the operations that the driver provides
struct file_operations fops =
{
    .owner   = THIS_MODULE,
    .open    = audio_pll_clk_open,
    .unlocked_ioctl  = audio_pll_clk_ioctl,
    .release = audio_pll_clk_release,
};

/**
  * @brief  I2S1 PLL output adjust by ppm.
  * @param  ppm: ~1.55ppm per FOF step
  * @param  action: slower or faster or reset
  *          This parameter can be one of the following values:
  *            @arg PLL_AUTO: reset to auto 98.304M
  *            @arg PLL_FASTER: pll clock faster than 98.304M
  *            @arg PLL_SLOWER: pll clock slower than 98.304M
  * real_diff(M) = out_clock - auto(0)out_clock
  * ppm_expected_diff = 98.304 * ppm / 1000000
  * our experiment result for example:
  * 	   ppm	out_clock	real_diff(M)	ppm_expected_diff
  * Auto	0	98.3053
  * Faster	800	98.3835  	0.0782	        0.0786432
  * 	    500	98.3542	    0.0489	        0.049152
  * 	    200	98.3249	    0.0196	        0.0196608
  * Slower	800	98.2272	    0.0781	        0.0786432
  * 	    500	98.2565	    0.0488	        0.049152
  * 	    200	98.2858	    0.0195	        0.0196608
  */

static int ameba_pll_i2s_98P304M_clk_tune(struct audio_clock_component* acc, u32 ppm, u32 action)
{
	u32 F0F_new;
	u32 tmp;
	struct audio_clock_data* data = get_audio_clock_data(acc);

	if (ppm > 1000) {
		pr_info("invalid ppm:%d", ppm);
		return -EINVAL;
	}

	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp &= (~PLL_MASK_IPLL1_DIVN_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp |= (PLL_IPLL1_DIVN_SDM(7));
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

	if (action == PLL_FASTER) {
		F0F_new = 5269 + ppm * 64 / 100;
	} else if (action == PLL_SLOWER) {
		F0F_new = 5269 - ppm * 64 / 100;
	} else {
		F0F_new = 5269;
	}

	//pr_info("%s, acc:0x%x, data->addr:0x%x, ppm:%d, action:%d, F0F_new:%d", __func__, acc, data->addr, ppm, action, F0F_new);
	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp &= (~PLL_MASK_IPLL1_F0F_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp |= (PLL_IPLL1_F0F_SDM(F0F_new));
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);

	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp &= (~PLL_MASK_IPLL1_F0N_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp |= (PLL_IPLL1_F0N_SDM(6));
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);

	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp |= (PLL_BIT_IPLL1_TRIG_FREQ);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp &= (~PLL_BIT_IPLL1_TRIG_FREQ);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

	return 0;
}

/**
  * @brief  I2S1 PLL output adjust by ppm.
  * @param  ppm: ~0.3875ppm per FOF step
  * @param  action: slower or faster or reset
  *          This parameter can be one of the following values:
  *            @arg PLL_AUTO: reset to auto 24.576M
  *            @arg PLL_FASTER: pll clock faster than 24.576M
  *            @arg PLL_SLOWER: pll clock slower than 24.576M
  * Actually 24.576M clock is from 98.304 in hardware.So if user wants to micro adjust 24.576M
  * we actually adjust 98.304M.
  * For example, if user wants to set ppm 200, basing on 24.576M, which ppm expected diff:200/1000000*24.576
  * We can achieve this by setting 200 /1000000 * (98.304/4), so for 98.304,also set ppm 200
  * real_diff(M) = out_clock - auto(0)out_clock
  * ppm_expected_diff = 24.576 * ppm / 1000000
  *	       ppm  	out_clock	real_diff(M)	ppm_expected_diff
  * Auto	0	    24.5763
  * Faster	800	    24.5959	    0.0196	        0.0196608
  *	        500	    24.5885	    0.0122	        0.012288
  *	        200	    24.5812	    0.0049	        0.0049152
  * Slower	800	    24.5568	    0.0195	        0.0196608
  *	        500	    24.5641	    0.0122	        0.012288
  *	        200	    24.5714	    0.0049	        0.0049152
  */
static int ameba_pll_i2s_24P576M_clk_tune(struct audio_clock_component* acc, u32 ppm, u32 action)
{
	u32 F0F_new;
	u32 tmp;
	struct audio_clock_data* data = get_audio_clock_data(acc);

	if (ppm > 1000) {
		pr_info("invalid ppm:%d", ppm);
		return -EINVAL;
	}

	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp &= (~PLL_MASK_IPLL1_DIVN_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp |= (PLL_IPLL1_DIVN_SDM(7));
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

	if (action == PLL_FASTER) {
		F0F_new = 5269 + ppm * 64 / 100;
	} else if (action == PLL_SLOWER) {
		F0F_new = 5269 - ppm * 64 / 100;
	} else {
		F0F_new = 5269;
	}

	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp &= (~PLL_MASK_IPLL1_F0F_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp |= (PLL_IPLL1_F0F_SDM(F0F_new));
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);

	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp &= (~PLL_MASK_IPLL1_F0N_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);
	tmp |= (PLL_IPLL1_F0N_SDM(6));
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL3);

	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp |= (PLL_BIT_IPLL1_TRIG_FREQ);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);
	tmp &= (~PLL_BIT_IPLL1_TRIG_FREQ);
	writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

	return 0;
}

/**
  * @brief  I2S2 PLL output adjust by ppm.
  * @param  ppm: ~1.69ppm per FOF step
  * @param  action: slower or faster or reset
  *          This parameter can be one of the following values:
  *            @arg PLL_AUTO: reset to auto 45.1584M
  *            @arg PLL_FASTER: pll clock faster than 45.1584M
  *            @arg PLL_SLOWER: pll clock slower than 45.1584M
  * real_diff(M) = out_clock - auto(0)out_clock
  * ppm_expected_diff = 45.1584 * ppm / 1000000
  *	       ppm	out_clock	real_diff(M)	ppm_expected_diff
  *Auto	   0	45.1586
  *Faster  800	45.1946	    0.036	        0.0361267
  *	       500	45.1811	    0.0225	        0.0225792
  *	       200	45.1676	    0.009	        0.0090317
  *Slower  800	45.1226	    0.036	        0.0361267
  *	       500	45.1361	    0.0225	        0.0225792
  *	       200	45.1496	    0.009	        0.0090317

  */
static int ameba_pll_i2s_45P1584M_clk_tune(struct audio_clock_component* acc, u32 ppm, u32 action)
{

	u32 F0F_new;
	u32 tmp;
	struct audio_clock_data* data = get_audio_clock_data(acc);

	if (ppm > 1000) {
		pr_info("invalid ppm:%d", ppm);
		return -EINVAL;
	}

	tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL1);
	tmp &= (~PLL_MASK_IPLL2_DIVN_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL1);
	tmp |= (PLL_IPLL2_DIVN_SDM(7));
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL1);

	if (action == PLL_FASTER) {
		F0F_new = 2076 + ppm * 59 / 100;
	} else if (action == PLL_SLOWER) {
		F0F_new = 2076 - ppm * 59 / 100;
	} else {
		F0F_new = 2076;
	}

	tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL3);
	tmp &= (~PLL_MASK_IPLL2_F0F_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL3);
	tmp |= (PLL_IPLL2_F0F_SDM(F0F_new));
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL3);

	tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL3);
	tmp &= (~PLL_MASK_IPLL2_F0N_SDM);
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL3);
	tmp |= (PLL_IPLL2_F0N_SDM(0));
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL3);

	tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL1);
	tmp |= (PLL_BIT_IPLL2_TRIG_FREQ);
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL1);
	tmp &= (~PLL_BIT_IPLL2_TRIG_FREQ);
	writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL1);

	return 0;
}

static int ameba_update_98P304M_input_clock_status(struct audio_clock_component* acc, bool enabled)
{
	int res = 0;
	u32 tmp;
	struct audio_clock_data* data = get_audio_clock_data(acc);
	//pr_info("%s enabled:%d", __func__, enabled);

	//PLL_TypeDef *pll = data->addr;

	if (enabled == true) {
		//avoid repeated enabling operations
		tmp = readl(data->addr + REG_PLL_STATE);
		if (tmp & PLL_BIT_CKRDY_I2S1)
			return res;

		//Check BandGap power on
		tmp = readl(data->addr + REG_PLL_AUX_BG);
		if ((tmp & PLL_BG_POW_MASK) == 0) {
			tmp |= (PLL_BIT_POW_BG | PLL_BIT_POW_I | PLL_BIT_POW_MBIAS);
			writel(tmp, data->addr + REG_PLL_AUX_BG);
			//DelayUs(160);
			 usleep_range(160,170);
		}

		// erc enable
		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp |= PLL_BIT_IPLL1_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);

		//DelayUs(1);
		usleep_range(10,20);

		// PLL power on
		tmp = readl(data->addr + REG_PLL_PS);
		tmp |= PLL_BIT_POW_CKGEN;
		writel(tmp, data->addr + REG_PLL_PS);

		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp |= PLL_BIT_IPLL1_POW_PLL | PLL_BIT_IPLL1_CK_EN | PLL_BIT_IPLL1_CK_EN_D4;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);

		// Select I2S1
		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
		tmp &= ~PLL_MASK_IPLL1_CK_OUT_SEL;
		tmp |= PLL_IPLL1_CK_OUT_SEL(14);
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

		// Wait ready
		//to be checked, if no sleep here, pll will stuck here
		msleep(10);
		while (!(readl(data->addr + REG_PLL_STATE) & PLL_BIT_CKRDY_I2S1));

	} else {
		//disable not supported right now..
		// old flow
/*		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp &= ~ PLL_BIT_IPLL1_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);

		// PLL power on
		tmp = readl(data->addr + REG_PLL_PS);
		tmp &= ~ PLL_BIT_POW_CKGEN;
		writel(tmp, data->addr + REG_PLL_PS);

		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp &= ~(PLL_BIT_IPLL1_POW_PLL | PLL_BIT_IPLL1_CK_EN | PLL_BIT_IPLL1_CK_EN_D4);
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);

		// Select I2S1
		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
		tmp &= ~PLL_MASK_IPLL1_CK_OUT_SEL;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

		//to be checked, if no sleep here, pll will stuck here
		msleep(10);
		while (readl(data->addr + REG_PLL_STATE) & PLL_BIT_CKRDY_I2S1);*/
		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp &= ~ PLL_BIT_IPLL1_POW_PLL;
		tmp &= ~ PLL_BIT_IPLL1_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);
	}
	return res;
}

static int ameba_update_45P1584_input_clock_status(struct audio_clock_component* acc, bool enabled)
{
	int res = 0;
	u32 tmp;
	struct audio_clock_data* data = get_audio_clock_data(acc);

	//the default I2S2 clock is 45.158M, tune to 45.1584 here.
	ameba_pll_i2s_45P1584M_clk_tune(acc, 0, 0);

	if (enabled == true) {
		//avoid repeated enabling operations
		if ((readl(data->addr + REG_PLL_STATE)) & PLL_BIT_CKRDY_I2S2) {
			return res;
		}

		//Check BandGap power on
		tmp = readl(data->addr + REG_PLL_AUX_BG);
		if ((tmp & PLL_BG_POW_MASK) == 0) {
			tmp |= (PLL_BIT_POW_BG | PLL_BIT_POW_I | PLL_BIT_POW_MBIAS);
			writel(tmp, data->addr + REG_PLL_AUX_BG);
			//DelayUs(160);
			usleep_range(160,170);
		}

		// erc enable
		tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL0);
		tmp |= PLL_BIT_IPLL2_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL0);

		//DelayUs(1);
		usleep_range(1,2);

		// PLL power on
		tmp = readl(data->addr + REG_PLL_PS);
		tmp |= PLL_BIT_POW_CKGEN;
		writel(tmp, data->addr + REG_PLL_PS);

		tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL0);
		tmp |= PLL_BIT_IPLL2_POW_PLL | PLL_BIT_IPLL2_CK_EN | PLL_BIT_IPLL2_CK_EN_D4;
		writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL0);

		// Select I2S1
		tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL1);
		tmp &= ~PLL_MASK_IPLL2_CK_OUT_SEL;
		tmp |= PLL_IPLL2_CK_OUT_SEL(12);
		writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL1);

		// Wait ready
		//to be checked, if no sleep here, pll will stuck here
		msleep(10);
		while (!(readl(data->addr + REG_PLL_STATE) & PLL_BIT_CKRDY_I2S2));
	} else {
		//disable not supported right now..
		// old flow
		/*tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL0);
		tmp &= ~ PLL_BIT_IPLL2_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL0);

		// PLL power on
		tmp = readl(data->addr + REG_PLL_PS);
		tmp &= ~ PLL_BIT_POW_CKGEN;
		writel(tmp, data->addr + REG_PLL_PS);
		tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL0);
		tmp &= ~(PLL_BIT_IPLL2_POW_PLL | PLL_BIT_IPLL2_CK_EN | PLL_BIT_IPLL2_CK_EN_D4);
		writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL0);

		// Select I2S1
		tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL1);
		tmp &= ~PLL_MASK_IPLL2_CK_OUT_SEL;
		writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL1);

		//to be checked, if no sleep here, pll will stuck here
		msleep(10);
		while (readl(data->addr + REG_PLL_STATE) & PLL_BIT_CKRDY_I2S2);*/
		tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL0);
		tmp &= ~ PLL_BIT_IPLL2_POW_PLL;
		tmp &= ~ PLL_BIT_IPLL2_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL2_CTRL0);
	}
	return res;
}

static int ameba_update_24P576_input_clock_status(struct audio_clock_component* acc, bool enabled)
{
	int res = 0;
	u32 tmp;

	struct audio_clock_data* data = get_audio_clock_data(acc);
	//pr_info("%s enabled:%d", __func__, enabled);

	if (enabled == true) {
		//avoid repeated enabling operations
		if ((readl(data->addr + REG_PLL_STATE)) & PLL_BIT_CKRDY_I2S1) {
			return res;
		}

		//Check BandGap power on
		tmp = readl(data->addr + REG_PLL_AUX_BG);
		if ((tmp & PLL_BG_POW_MASK) == 0) {
			tmp |= (PLL_BIT_POW_BG | PLL_BIT_POW_I | PLL_BIT_POW_MBIAS);
			writel(tmp, data->addr + REG_PLL_AUX_BG);
			//DelayUs(160);
			usleep_range(160,170);
		}

		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
		tmp &= ~PLL_MASK_IPLL1_CK_OUT_SEL;
		tmp |= PLL_IPLL1_CK_OUT_SEL(0);
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

		// erc enable
		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp |= PLL_BIT_IPLL1_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);
		//DelayUs(1);
		usleep_range(1,2);

		// PLL power on
		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp |= PLL_BIT_IPLL1_POW_PLL;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);

		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
		tmp &= ~PLL_MASK_IPLL1_CK_OUT_SEL;
		tmp |= PLL_NPLL_CK_OUT_SEL(14);
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL1);

		// Wait ready
		//to be checked, if no sleep here, pll will stuck here
		msleep(10);
		while (!(readl(data->addr + REG_PLL_STATE) & PLL_BIT_CKRDY_I2S1));

	} else {
		//disable not supported right now..
		// old flow
		/*tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp &= ~ PLL_BIT_IPLL1_POW_PLL;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);

		// PLL power on
		tmp = readl(data->addr + REG_PLL_PS);
		tmp &= ~ PLL_BIT_POW_CKGEN;
		writel(tmp, data->addr + REG_PLL_PS);

		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp &= ~(PLL_BIT_IPLL1_POW_ERC);
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);

		//to be checked, if no sleep here, pll will stuck here
		msleep(10);*/
		tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
		tmp &= ~ PLL_BIT_IPLL1_POW_PLL;
		tmp &= ~ PLL_BIT_IPLL1_POW_ERC;
		writel(tmp, data->addr + REG_PLL_I2SPLL1_CTRL0);
	}
	return res;
}

static int ameba_get_audio_clock_info_for_sport(struct audio_clock_component* acc, unsigned int sport_index)
{
	struct audio_clock_data* data = get_audio_clock_data(acc);
	if (sport_index < 0 || sport_index > SPORT_INDEX_MAX) {
		pr_err("invalid sport_index:%d", sport_index);
		return -EINVAL;
	}
	return data->pll_clk_for_sport[sport_index];
}

static int ameba_choose_input_clock_for_sport_index(struct audio_clock_component* acc, unsigned int sport_index, unsigned int clock)
{
	int res = 0;

	u32 RegValue = 0;
	u32 RegValue2 = 0;
	u32 SPORTClk = 0;
	void __iomem *system_control_base_lp;

	struct audio_clock_data* data = get_audio_clock_data(acc);
	system_control_base_lp = data->system_control_base_lp;

	RegValue = readl(system_control_base_lp + REG_LSYS_CKSL_GRP0);
	RegValue2 = readl(system_control_base_lp + REG_LSYS_SPORT_CLK);
	SPORTClk = LSYS_GET_CKSL_SPORT(RegValue);

	if (clock == CKSL_I2S_XTAL40M) {
		SPORTClk &= ~(BIT(sport_index));
	} else {
		SPORTClk |= (BIT(sport_index));
		switch (sport_index) {
		case 0:
			RegValue2 &= ~(LSYS_MASK_CKSL_I2S0);
			RegValue2 |= LSYS_CKSL_I2S0(clock);
			break;
		case 1:
			RegValue2 &= ~(LSYS_MASK_CKSL_I2S1);
			RegValue2 |= LSYS_CKSL_I2S1(clock);
			break;
		case 2:
			RegValue2 &= ~(LSYS_MASK_CKSL_I2S2);
			RegValue2 |= LSYS_CKSL_I2S2(clock);
			break;
		case 3:
			RegValue2 &= ~(LSYS_MASK_CKSL_I2S3);
			RegValue2 |= LSYS_CKSL_I2S3(clock);
			break;
		default:
			break;
		}
	}

	RegValue &= ~LSYS_MASK_CKSL_SPORT;
	RegValue |= LSYS_CKSL_SPORT(SPORTClk);

	writel(RegValue, system_control_base_lp + REG_LSYS_CKSL_GRP0);
	writel(RegValue2, system_control_base_lp + REG_LSYS_SPORT_CLK);

	switch (clock)
	{
	case CKSL_I2S_PLL98M:
		data->pll_clk_for_sport[sport_index] = PLL_98P304M;
		break;
	case CKSL_I2S_PLL45M:
		data->pll_clk_for_sport[sport_index] = PLL_45P1584M;
		break;
	case CKSL_I2S_PLL24M:
		data->pll_clk_for_sport[sport_index] = PLL_24P576M;
		break;
	default:
		break;
	}

	return res;
}

static int ameba_update_fen_cke_sport_status(struct audio_clock_component* acc, unsigned int sport_index, bool enabled)
{
	int res = 0;
	u32 tmp;
	void __iomem *system_control_base_lp;

	struct audio_clock_data* data = get_audio_clock_data(acc);
	system_control_base_lp = data->system_control_base_lp;

	if (enabled == true) {
		tmp = readl(system_control_base_lp + REG_LSYS_CKE_GRP2);
		tmp |= LSYS_CKE_SPORT(1 << sport_index);
		writel(tmp, system_control_base_lp + REG_LSYS_CKE_GRP2);
		tmp = readl(system_control_base_lp + REG_LSYS_FEN_GRP2);
		tmp |= LSYS_FEN_SPORT(1 << sport_index);
		writel(tmp, system_control_base_lp + REG_LSYS_FEN_GRP2);
	} else {
		tmp = readl(system_control_base_lp + REG_LSYS_CKE_GRP2);
		tmp &= ~LSYS_CKE_SPORT(1 << sport_index);
		writel(tmp, system_control_base_lp + REG_LSYS_CKE_GRP2);
		tmp = readl(system_control_base_lp + REG_LSYS_FEN_GRP2);
		tmp &= ~LSYS_FEN_SPORT(1 << sport_index);
		writel(tmp, system_control_base_lp + REG_LSYS_FEN_GRP2);
	}
	return res;
}

static int ameba_set_rate_divider(struct audio_clock_component* acc, unsigned int sport_index, unsigned int divider)
{
	int res = 0;
	u32 tmp = 0;
	void __iomem *system_control_base_lp;

	struct audio_clock_data* data = get_audio_clock_data(acc);
	system_control_base_lp = data->system_control_base_lp;

	if (divider < 1 || divider > 8) {
		pr_info("invalid divider:%d", divider);
		return -EINVAL;
	}

	tmp = readl(system_control_base_lp + REG_LSYS_SPORT_CLK);

	switch (sport_index) {
	case 0:
		tmp &= ~(LSYS_MASK_CKD_SPORT0);
		tmp |= LSYS_CKD_SPORT0(divider - 1);
		break;
	case 1:
		tmp &= ~(LSYS_MASK_CKD_SPORT1);
		tmp |= LSYS_CKD_SPORT1(divider - 1);
		break;
	case 2:
		tmp &= ~(LSYS_MASK_CKD_SPORT2);
		tmp |= LSYS_CKD_SPORT2(divider - 1);
		break;
	case 3:
		tmp &= ~(LSYS_MASK_CKD_SPORT3);
		tmp |= LSYS_CKD_SPORT3(divider - 1);
		break;
	}

	writel(tmp, system_control_base_lp + REG_LSYS_SPORT_CLK);

	return res;
}

static int ameba_get_rate_divider(struct audio_clock_component* acc)
{
	int res = 0;
	return res;
}

static int ameba_set_rate(struct audio_clock_component* acc,unsigned int rate)
{
	int res = 0;
	return res;
}

static int ameba_get_rate(struct audio_clock_component* acc)
{
	int res = 0;
	return res;
}

static void ameba_clock_dump(struct audio_clock_component* acc)
{
	u32 tmp = 0;
	void __iomem *system_control_base_lp;

	struct audio_clock_data* data = get_audio_clock_data(acc);
	system_control_base_lp = data->system_control_base_lp;

	tmp = readl(system_control_base_lp + REG_LSYS_CKD_GRP0);
	pr_info("sys REG_LSYS_CKD_GRP0:0x%x", tmp);
	tmp = readl(system_control_base_lp + REG_LSYS_CKE_GRP2);
	pr_info("sys REG_LSYS_CKE_GRP2:0x%x", tmp);
	tmp = readl(system_control_base_lp + REG_LSYS_FEN_GRP2);
	pr_info("sys REG_LSYS_FEN_GRP2:0x%x", tmp);
	tmp = readl(system_control_base_lp + REG_LSYS_CKSL_GRP0);
	pr_info("sys REG_LSYS_CKSL_GRP0:0x%x", tmp);
	tmp = readl(system_control_base_lp + REG_LSYS_CKD_GRP1);
	pr_info("sys REG_LSYS_CKD_GRP1:0x%x", tmp);

	tmp = readl(data->addr + REG_PLL_STATE);
	pr_info("pll REG_PLL_STATE:0x%x", tmp);
	tmp = readl(data->addr + REG_PLL_AUX_BG);
	pr_info("pll REG_PLL_AUX_BG:0x%x", tmp);
	tmp = readl(data->addr + REG_PLL_PS);
	pr_info("pll REG_PLL_PS:0x%x", tmp);
	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL0);
	pr_info("pll REG_PLL_I2SPLL1_CTRL0:0x%x", tmp);
	tmp = readl(data->addr + REG_PLL_I2SPLL1_CTRL1);
	pr_info("pll REG_PLL_I2SPLL1_CTRL1:0x%x", tmp);
	tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL0);
	pr_info("pll REG_PLL_I2SPLL2_CTRL0:0x%x", tmp);
	tmp = readl(data->addr + REG_PLL_I2SPLL2_CTRL1);
	pr_info("pll REG_PLL_I2SPLL2_CTRL1:0x%x", tmp);

}

static const struct audio_clock_driver clk_driver = {
	.update_98MP304M_input_clock_status = ameba_update_98P304M_input_clock_status,
	.update_45MP158_input_clock_status = ameba_update_45P1584_input_clock_status,
	.update_24MP576_input_clock_status = ameba_update_24P576_input_clock_status,
	.pll_i2s_98P304M_clk_tune = ameba_pll_i2s_98P304M_clk_tune,
	.pll_i2s_24P576M_clk_tune = ameba_pll_i2s_24P576M_clk_tune,
	.pll_i2s_45P1584M_clk_tune = ameba_pll_i2s_45P1584M_clk_tune,
	.get_audio_clock_info_for_sport = ameba_get_audio_clock_info_for_sport,
	.choose_input_clock_for_sport_index = ameba_choose_input_clock_for_sport_index,
	.update_fen_cke_sport_status = ameba_update_fen_cke_sport_status,
	.set_rate_divider = ameba_set_rate_divider,
	.get_rate_divider = ameba_get_rate_divider,
	.set_rate = ameba_set_rate,
	.get_rate = ameba_get_rate,
	.dump = ameba_clock_dump,
};

static int ameba_audio_clock_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct audio_clock_data *clock_priv;
	struct resource *resource_reg;
	int ret = 0;
	struct device_node * rcc_node;
	struct resource res;
	int sport_index = 0;

	/*alloc private data for this driver*/
	clock_priv = devm_kzalloc(&pdev->dev, sizeof(struct audio_clock_data),
							GFP_KERNEL);
	if (clock_priv == NULL)
		return -ENOMEM;

	/*get reg information from dts*/
	resource_reg = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	clock_priv->base = resource_reg->start;
	clock_priv->reg_size = resource_reg->end - resource_reg->start + 1;
	clock_priv->addr = devm_ioremap_resource(&pdev->dev, resource_reg);
	if (clock_priv->addr == NULL) {
		dev_err(&pdev->dev, "cannot ioremap registers\n");
		return -ENXIO;
	}

	rcc_node = of_parse_phandle(dev->of_node, "rtk,sport-clock", 0);

	if (of_address_to_resource(rcc_node, 1, &res)) {
		dev_err(&pdev->dev, "get resource for sport clock error\n");
		ret = -ENOMEM;
		return ret;
	}

	clock_priv->system_control_base_lp = ioremap(res.start, resource_size(&res));
	if (!clock_priv->system_control_base_lp) {
			dev_err(&pdev->dev, "ioremap for sport clock error\n");
			ret = -ENOMEM;
			return ret;
	}

	for (; sport_index < SPORT_INDEX_MAX; sport_index++) {
		//-1 means sport doesn't choose pll for clock, it may only choose 40M XTAL for clock
		clock_priv->pll_clk_for_sport[sport_index] = PLL_XTAL40M;
	}

	create_audio_clock_component(dev);
	register_audio_clock(&clk_driver);

	platform_set_drvdata(pdev, clock_priv);
	dev_set_drvdata(&pdev->dev, clock_priv);

#if I2S_PLL_ALWAYS_ON
	update_98MP304M_input_clock_status(true);
	update_45MP158_input_clock_status(true);
	update_24MP576_input_clock_status(true);
#endif

#define DEBUG_PLL_MICRO_ADJUST 1
#if DEBUG_PLL_MICRO_ADJUST
	set_rate_divider(2, 1);
	set_rate_divider(3, 1);
#endif

	ret = alloc_chrdev_region(&devno, 0, 1, "audio_pll_clk");
	if (ret < 0) {
		return ret;
	}

	audio_pll_cdev = cdev_alloc();

	cdev_init(audio_pll_cdev, &fops);
	audio_pll_cdev->owner = THIS_MODULE;
	audio_pll_cdev->ops = &fops;
	ret = cdev_add(audio_pll_cdev, devno, 1);
	if (ret) {
		pr_err("Error %d add realtek audio pll %d", ret, 0);
		return ret;
	}

	realtek_audio_pll_class = class_create(THIS_MODULE, "audio_pll_clk");
	device_create(realtek_audio_pll_class, NULL, devno, NULL, "audio_pll_clk");

	return 0;
}

static int ameba_audio_clock_remove(struct platform_device *pdev)
{
	int res = 0;
	device_destroy(realtek_audio_pll_class, devno);
	class_destroy(realtek_audio_pll_class);
	realtek_audio_pll_class = NULL;
	cdev_del(audio_pll_cdev);
	audio_pll_cdev = NULL;
	unregister_chrdev_region(devno, 1);

	return res;
}

#ifdef CONFIG_PM
static int ameba_audio_clock_suspend(struct device *dev)
{
	update_98MP304M_input_clock_status(false);
	update_45MP158_input_clock_status(false);
	update_24MP576_input_clock_status(false);

	return 0;
}

static int ameba_audio_clock_resume(struct device *dev)
{
	update_98MP304M_input_clock_status(true);
	update_45MP158_input_clock_status(true);
	update_24MP576_input_clock_status(true);

	return 0;
}
#else
#define ameba_audio_clock_suspend NULL
#define ameba_audio_clock_resume NULL
#endif

static const struct dev_pm_ops ameba_audio_clock_pm_ops = {
	.suspend = ameba_audio_clock_suspend,
	.resume = ameba_audio_clock_resume,
};

static const struct of_device_id ameba_audio_clock_match[] = {
	{ .compatible = "realtek,ameba-audio-clock", },
	{},
};

static struct platform_driver ameba_audio_clock_driver = {
	.probe  = ameba_audio_clock_probe,
	.remove = ameba_audio_clock_remove,
	.driver = {
		.name = "ameba-audio-clock",
		.of_match_table = of_match_ptr(ameba_audio_clock_match),
		.pm = &ameba_audio_clock_pm_ops,
	},
};

module_platform_driver(ameba_audio_clock_driver);

/* Module information */
MODULE_DESCRIPTION("Realtek Ameba ALSA driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
