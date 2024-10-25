// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek CPU frequency support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/cpu_cooling.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

struct rtk_cpufreq {
	struct device *cpu;

	struct clk *apll_clk;
	struct clk *npll_clk;
	struct clk *div_clk;
	struct clk *mux_clk;
	struct clk *cpu_clk;

	u32 cpu_init_rate;
	u32 transition_latency;
	struct cpufreq_frequency_table *freq_tab;
	u32 cpu_cur_rate;
};


static int rtk_cpufreq_apll(struct cpufreq_policy *policy, unsigned int index)
{
	u32 new_freq;
	int ret;
	struct rtk_cpufreq *rtk_data;
	rtk_data = policy->driver_data;
	new_freq = rtk_data->freq_tab[index].frequency;

	/* Reparent mux clock to NP PLL clock*/
	ret = clk_set_parent(rtk_data->mux_clk, rtk_data->npll_clk);
	if (ret) {
		pr_err("CPU%d: Failed to re-parent CPU clock\n", policy->cpu);
		return ret;
	}

	/* Set the AP PLL to target rate */
	ret = clk_set_rate(rtk_data->apll_clk, new_freq * 1000);
	if (ret) {
		pr_err("CPU%d: Failed to scale CPU clock rate\n", policy->cpu);
		clk_set_parent(rtk_data->mux_clk, rtk_data->apll_clk);
		return ret;
	}

	/* Set parent of mux clock back to the AP PLL clock */
	ret = clk_set_parent(rtk_data->mux_clk, rtk_data->apll_clk);
	if (ret) {
		pr_err("CPU%d: Failed to re-parent CPU clock\n", policy->cpu);
		return ret;
	}

	return ret;
}



static int rtk_cpufreq_target_index(struct cpufreq_policy *policy, unsigned int index)
{
	u32 new_freq;
	int ret;
	struct rtk_cpufreq *rtk_data;
	new_freq = policy->freq_table[index].frequency;
	rtk_data = policy->driver_data;

	/* method 1: tune AP clock divider.
	   method 2: tune APLL clock rate.
	   Now we choose method 1. */
	ret = clk_set_rate(rtk_data->div_clk, new_freq * 1000);
	if (ret) {
		pr_err("Failed to set CPU clock to %dKHz\n", new_freq);
		return ret;
	}

	return ret;
}



static int rtk_cpufreq_init(struct cpufreq_policy *policy)
{
	struct device_node *np;
	struct rtk_cpufreq *rtk_data;
	const struct property *prop;
	struct cpufreq_frequency_table *freq_tab;
	int ret, cnt, i;
	const __be32 *val;
	struct device *cpu = get_cpu_device(policy->cpu);
	int match_flag = 0;

	rtk_data = kmalloc(sizeof(struct rtk_cpufreq), GFP_KERNEL);
	if (!rtk_data) {
		ret = -ENOMEM;
		return ret;
	}

	memset(rtk_data, 0, sizeof(struct rtk_cpufreq));

	np = of_node_get(cpu->of_node);
	if (!np) {
		pr_err("No CPU node found\n");
		ret = -ENODEV;
		goto out1;
	}

	ret = of_property_read_u32(np, "clock-frequency", &rtk_data->cpu_init_rate);
	if (ret) {
		rtk_data->cpu_init_rate = 0;
	}

	ret = of_property_read_u32(np, "clock-latency", &rtk_data->transition_latency);
	if (ret) {
		rtk_data->transition_latency = CPUFREQ_ETERNAL;
	}

	prop = of_find_property(np, "freq_tab", NULL);
	if (!prop || !prop->value) {
		pr_err("Invalid cpufreq table\n");
		ret = -ENODEV;
		goto out2;
	}

	cnt = prop->length / sizeof(u32);
	val = prop->value;

	freq_tab = kmalloc(sizeof(struct cpufreq_frequency_table) * (cnt + 1), GFP_KERNEL);
	if (!freq_tab) {
		ret = -ENOMEM;
		goto out2;
	}

	memset(freq_tab, 0, sizeof(struct cpufreq_frequency_table) * (cnt + 1));

	for (i = 0; i < cnt; i++) {
		freq_tab[i].driver_data = i;
		freq_tab[i].frequency = be32_to_cpup(val++);
		if (freq_tab[i].frequency == (rtk_data->cpu_init_rate / 1000)) {
			match_flag = 1;
		}
	}

	freq_tab[i].driver_data = i;
	freq_tab[i].frequency = CPUFREQ_TABLE_END;

	rtk_data->cpu_clk = of_clk_get_by_name(np, "cpu");
	if (IS_ERR_OR_NULL(rtk_data->cpu_clk)) {
		pr_err("Unable to get CPU clock\n");
		ret = PTR_ERR(rtk_data->cpu_clk);
		goto out3;
	}

	rtk_data->div_clk = of_clk_get_by_name(np, "cpu-div");
	if (IS_ERR_OR_NULL(rtk_data->div_clk)) {
		pr_err("Unable to get cpu-div clock\n");
		ret = PTR_ERR(rtk_data->div_clk);
		goto out4;
	}

	rtk_data->mux_clk = of_clk_get_by_name(np, "cpu-mux");
	if (IS_ERR_OR_NULL(rtk_data->mux_clk)) {
		pr_err("Unable to get cpu-mux clock\n");
		ret = PTR_ERR(rtk_data->mux_clk);
		goto out5;
	}

	rtk_data->npll_clk = of_clk_get_by_name(np, "np-pll");
	if (IS_ERR_OR_NULL(rtk_data->npll_clk)) {
		pr_err("Unable to get np-pll clock\n");
		ret = PTR_ERR(rtk_data->npll_clk);
		goto out6;
	}

	rtk_data->apll_clk = of_clk_get_by_name(np, "ap-pll");
	if (IS_ERR_OR_NULL(rtk_data->apll_clk)) {
		pr_err("Unable to get ap-pll clock\n");
		ret = PTR_ERR(rtk_data->apll_clk);
		goto out7;
	}

	policy->clk = rtk_data->cpu_clk;
	rtk_data->cpu = cpu;
	policy->driver_data = rtk_data;
	rtk_data->freq_tab = freq_tab;

	/* if cpu initial clock rate is not one of available frequency,
	   tune cpu clock to the max available frequency*/
	if (match_flag != 1) {
		rtk_cpufreq_apll(policy, cnt - 1);
	}

	cpufreq_generic_init(policy, freq_tab, rtk_data->transition_latency);

	return 0;

out7:
	clk_put(rtk_data->npll_clk);
out6:
	clk_put(rtk_data->mux_clk);
out5:
	clk_put(rtk_data->div_clk);
out4:
	clk_put(rtk_data->cpu_clk);
out3:
	kfree(freq_tab);
out2:
	of_node_put(np);
out1:
	kfree(rtk_data);

	return ret;
}



static int rtk_cpufreq_suspend(struct cpufreq_policy *policy)
{
	struct rtk_cpufreq *rtk_data = policy->driver_data;
	rtk_data->cpu_cur_rate = cpufreq_generic_get(policy->cpu);

	return 0;
}



static int rtk_cpufreq_resume(struct cpufreq_policy *policy)
{
	struct rtk_cpufreq *rtk_data = policy->driver_data;
	unsigned int index = 0;
	struct cpufreq_frequency_table *freq_tab = rtk_data->freq_tab;

	for (; freq_tab->frequency != CPUFREQ_TABLE_END; freq_tab++) {
		if (rtk_data->cpu_cur_rate == freq_tab->frequency) {
			break;
		} else if (freq_tab->frequency == CPUFREQ_TABLE_END) {
			return -ENODATA;
		}

		index ++;
	}

	return rtk_cpufreq_target_index(policy, index);
}



static struct cpufreq_driver rtk_cpufreq_driver = {
	.name         = "realtek-ameba-cpufreq",
	.flags        = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.init         = rtk_cpufreq_init,
	.get          = cpufreq_generic_get,
	.target_index = rtk_cpufreq_target_index,
	.verify       = cpufreq_generic_frequency_table_verify,
	.suspend      = rtk_cpufreq_suspend,
	.resume       = rtk_cpufreq_resume,
};



static int __init rtk_cpufreq_initcall(void)
{
	int ret;

	ret = cpufreq_register_driver(&rtk_cpufreq_driver);
	if (ret) {
		pr_err("CPUFFREQ: Failed register driver\n");
	}

	return ret;
}



static void __exit rtk_cpufreq_exitcall(void)
{
	cpufreq_unregister_driver(&rtk_cpufreq_driver);
}


module_init(rtk_cpufreq_initcall);
module_exit(rtk_cpufreq_exitcall);

MODULE_DESCRIPTION("Realtek Ameba CPU frequency driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
