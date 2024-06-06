// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek LEDC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/
#include "leds-rtk-ws28xxx.h"

#define to_rtk_ws28xxx_led(ldev) container_of(ldev, struct rtk_ws28xxx_led, ldev)

/*************************************************************************/
/*************************** LEDs Hardware Layer *************************/
/*************************************************************************/

static void rtk_ledc_writel(
	void __iomem *ptr, u32 reg, u32 value)
{
	writel(value, (void __iomem *)((u32)ptr + reg));
}

static u32 rtk_ledc_readl(void __iomem *ptr, u32 reg)
{
	return readl((void __iomem *)((u32)ptr + reg));
}

static void rtk_get_dts_info(
	struct rtk_ws28xxx_led_priv *priv,
	struct device *dev,
	struct device_node *np,
	int *param_to_set, int default_value,
	char *dts_name)
{
	int nr_requests, ret;

	/* Get DTS params. */
	ret = of_property_read_u32(np, dts_name, &nr_requests);
	if (ret) {
		dev_warn(dev, "Can't get DTS property %s, set it to default value %d\n", dts_name, default_value);
		*param_to_set = default_value;
	} else {
		dev_dbg(dev, "Get DTS property %s = %d\n", dts_name, nr_requests);
		*param_to_set = nr_requests;
	}
}

/**
  * @brief  Fills each LEDC_InitStruct member with its default value.
  * @param  LEDC_InitStruct: pointer to an LEDC_InitTypeDef structure which will be initialized.
  * @retval None
  */
void rtk_ledc_struct_init(
	struct rtk_ws28xxx_led_priv *priv,
	struct device *dev,
	struct rtk_ws28xxx_ledc_init *init_struct_ledc)
{
	struct device_node *np = dev->of_node;

	/* LEDC DTS defined params. */
	char s1[] = "reg";
	char s2[] = "rtk,led-nums";
	char s4[] = "rtk,wait-data-timeout";
	char s5[] = "rtk,output-RGB-mode";
	char s6[] = "rtk,data-tx-time0h";
	char s7[] = "rtk,data-tx-time0l";
	char s8[] = "rtk,data-tx-time1h";
	char s9[] = "rtk,data-tx-time1l";
	char s10[] = "rtk,refresh-time";

	rtk_get_dts_info(priv, dev, np, &priv->ledc_phy_addr, LEDC_REG_BASE, s1);
	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->led_count, 1, s2);
	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->wait_data_time_ns, 0x7FFF, s4);
	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->output_RGB_mode, LEDC_OUTPUT_GRB, s5);

	init_struct_ledc->auto_dma_thred = 32;
	init_struct_ledc->ledc_trans_mode = LEDC_CPU_MODE;
	/* If ledc fifo level is too large, dma may slow down and appear refresh-time. Set 7 here. */
	init_struct_ledc->ledc_fifo_level = 0x7;

	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->t0h_ns, 0xd, s6);
	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->t0l_ns, 0x27, s7);
	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->t1h_ns, 0x27, s8);
	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->t1l_ns, 0xd, s9);
	rtk_get_dts_info(priv, dev, np, &init_struct_ledc->reset_ns, 0x3A97, s10); //375000ns

	init_struct_ledc->data_length = LEDC_DEFAULT_LED_NUM;
	init_struct_ledc->ledc_polarity = LEDC_IDLE_POLARITY_LOW;
	init_struct_ledc->wait_time0_en = ENABLE;
	init_struct_ledc->wait_time1_en = ENABLE;
	init_struct_ledc->wait_time0_ns = 0xEF; //6us
	init_struct_ledc->wait_time1_ns = 0x61A7; //625000ns

}

/**
  * @brief  Set LEDC soft reset, Force LEDC Enters IDLE State
  * @param  LEDCx: selected LEDC peripheral.
  * @retval None
  *
  * @note LEDC soft reset will clear to 0 after set 1; after LEDC soft reset, LEDC enters idle state, which
  *   brings all interrupt status cleared, FIFO read write pointer cleared, finished data count is 0 and LEDC_EN bit is
  *   cleared to 0.
  */
void rtk_ledc_soft_reset(struct rtk_ws28xxx_led_priv *priv)
{
	u32 temp_ctrl_reg;

	temp_ctrl_reg = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);
	rtk_ledc_writel(priv->base, LEDC_CTRL_REG, temp_ctrl_reg | LEDC_BIT_SOFT_RESET);
}

/**
  * @brief  Enable or disable LEDC peripheral.
  * @param  LEDCx: selected LEDC peripheral.
  * @param  NewState: new state of the operation mode.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void rtk_ledc_enable_cmd(struct rtk_ws28xxx_led_priv *priv, u8 new_state)
{
	u32 temp_ctrl_reg;

	temp_ctrl_reg = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);

	if (new_state == DISABLE) {
		rtk_ledc_writel(priv->base, LEDC_CTRL_REG, temp_ctrl_reg & ~ LEDC_BIT_EN);
	} else {
		rtk_ledc_soft_reset(priv);
		msleep(1);
		rtk_ledc_writel(priv->base, LEDC_CTRL_REG, temp_ctrl_reg | LEDC_BIT_EN);
	}
}

/**
  * @brief  ENABLE/DISABLE LEDC's interrupt bits.
  * @param  LEDCx: selected LEDC peripheral.
  * @param  LEDC_IT: specifies the LEDCx interrupt sources to be enabled or disabled.
  *   This parameter can be one or combinations of the following values:
  *            @arg BIT_LEDC_FIFO_OVERFLOW_INT_EN: LEDC FIFO Overflow Interrupt
  *            @arg BIT_LEDC_WAITDATA_TIMEOUT_INT_EN: LEDC Wait Data Timeout Interrupt
  *            @arg BIT_LEDC_CPUREQ_FIFO_INT_EN: LEDC CPU Requset Interrupt
  *            @arg BIT_LEDC_TRANS_FINISH_INT_EN: LEDC Transfer Finishi Interrupt
  * @param  NewState: specifies the state of the interrupt.
  *   This parameter can be one of the following values:
  *            @arg ENABLE
  *            @arg DISABLE
  * @retval None
  */
void rtk_ledc_interrupt_config(
	struct rtk_ws28xxx_led_priv *priv,
	u32 ledc_it, u32 new_state)
{
	u32 reg_val = 0;
	u32 temp_int_reg;

	if (!IS_LEDC_INTERRUPT(ledc_it)) {
		dev_err(priv->dev, "Illegal parameter: interrupt config bits\n");
		return;
	}
	temp_int_reg = rtk_ledc_readl(priv->base, LEDC_LED_INTERRUPT_CTRL_REG);

	if (new_state == ENABLE) {
		reg_val = LEDC_BIT_GLOBAL_INT_EN | ledc_it;
		rtk_ledc_writel(priv->base, LEDC_LED_INTERRUPT_CTRL_REG, temp_int_reg | reg_val);
	} else {
		rtk_ledc_writel(priv->base, LEDC_LED_INTERRUPT_CTRL_REG, temp_int_reg & ~ ledc_it);
	}
}

/**
  * @brief  Clear the LEDC's interrupt bits.
  * @param  LEDCx: selected LEDC peripheral.
  * @param  LEDC_IT: specifies the LEDCx interrupt sources to be enabled or disabled.
  *   This parameter can be one or combinations of the following values:
  *            @arg BIT_LEDC_FIFO_OVERFLOW_INT_EN: LEDC FIFO Overflow Interrupt
  *            @arg BIT_LEDC_WAITDATA_TIMEOUT_INT_EN: LEDC Wait Data Timeout Interrupt
  *            @arg BIT_LEDC_CPUREQ_FIFO_INT_EN: LEDC CPU Requset Interrupt
  *            @arg BIT_LEDC_TRANS_FINISH_INT_EN: LEDC Transfer Finishi Interrupt
  * @retval None
  */
void rtk_ledc_clear_interrupt(
	struct rtk_ws28xxx_led_priv *priv, u32 ledc_it)
{
	u32 temp_int_reg;
	if (!IS_LEDC_INTERRUPT(ledc_it)) {
		dev_err(priv->dev, "Illegal parameter: interrupt clear bits\n");
		return;

	}
	temp_int_reg = rtk_ledc_readl(priv->base, LEDC_LED_INT_STS_REG);
	rtk_ledc_writel(priv->base, LEDC_LED_INT_STS_REG, temp_int_reg | ledc_it);
}

/**
  * @brief  Get LEDC's interrupt status.
  * @param  LEDCx: selected LEDC peripheral.
  * @retval LEDC interrupt status.
  *   each bit of the interrupt status shows as follows:
  *            - bit 17 : FIFO empty status
  *            - bit 16 : FIFO full status
  *            - bit 3 : FIFO overflow interrupt happens when FIFO overflow
  *            - bit 2 : Wait data timeout interrupt happens when FIFO is empty after LEDC_WAIT_DATA_TIME
  *            - bit 1 : CPU Request interrupt happens when FIFO left less than FIFO threshold
  *            - bit 0 : Transfer done interrupt happens when transfer complete.
  */
u32 rtk_ledc_get_interrupt(struct rtk_ws28xxx_led_priv *priv)
{
	return rtk_ledc_readl(priv->base, LEDC_LED_INT_STS_REG);
}

/**
  * @brief  Set LEDC transfer mode
  * @param  LEDCx: selected LEDC peripheral.
  * @param  mode:This parameter can be one of the following values:
  *            @arg LEDC_CPU_MODE
  *            @arg LEDC_DMA_MODE
  * @retval None
  */
void rtk_ledc_set_trans_mode(
	struct rtk_ws28xxx_led_priv *priv,
	u32 mode)
{
	u32 temp_ctrl_reg;
	u32 temp_int_reg;

	temp_ctrl_reg = rtk_ledc_readl(priv->base, LEDC_DMA_CTRL_REG);
	temp_int_reg = rtk_ledc_readl(priv->base, LEDC_LED_INTERRUPT_CTRL_REG);

	if (!IS_LEDC_TRANS_MODE(mode)) {
		dev_err(priv->dev, "Illegal parameter: transfer mode\n");
		return;
	}

	if (mode) {
		rtk_ledc_writel(priv->base, LEDC_DMA_CTRL_REG, temp_ctrl_reg | LEDC_BIT_DMA_EN);
		rtk_ledc_writel(priv->base, LEDC_LED_INTERRUPT_CTRL_REG, temp_int_reg & ~LEDC_BIT_FIFO_CPUREQ_INT_EN);
	} else {
		rtk_ledc_writel(priv->base, LEDC_DMA_CTRL_REG, temp_ctrl_reg & ~LEDC_BIT_DMA_EN);
		rtk_ledc_writel(priv->base, LEDC_LED_INTERRUPT_CTRL_REG, temp_int_reg | LEDC_BIT_GLOBAL_INT_EN | LEDC_BIT_FIFO_CPUREQ_INT_EN);
	}
}

/**
  * @brief  Get LEDC transfer mode
  * @param  LEDCx: selected LEDC peripheral.
  * @retval LEDC transfer mode.
  *   the result can be one of the folling values:
  *            - LEDC_DMA_MODE
  *            - LEDC_CPU_MODE
  */
u32 rtk_ledc_get_trans_mode(struct rtk_ws28xxx_led_priv *priv)
{
	u32 temp_ctrl_reg;

	temp_ctrl_reg = rtk_ledc_readl(priv->base, LEDC_DMA_CTRL_REG);

	if (temp_ctrl_reg & LEDC_BIT_DMA_EN) {
		return LEDC_DMA_MODE;
	} else {
		return LEDC_CPU_MODE;
	}
}

/**
  * @brief  Set LEDC's FIFO Level
  * @param  LEDCx: selected LEDC peripheral.
  * @param  FifoLevel: LEDCx FIFO level. Value range: 0 to 31.
  *   When the number of transmit FIFO entries is greater or equal to this value,
  *         DMA request or BIT_LEDC_CPUREQ_FIFO_INT_EN happens.
  *         This parameter is recommaned to 7 or 15.
  *
  * @retval None
  */
void rtk_ledc_set_fifo_level(
	struct rtk_ws28xxx_led_priv *priv, u8 FifoLevel)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_DMA_CTRL_REG);
	reg_val &= ~LEDC_MASK_FIFO_TRIG_LEVEL;
	reg_val |= LEDC_FIFO_TRIG_LEVEL(FifoLevel);
	rtk_ledc_writel(priv->base, LEDC_DMA_CTRL_REG, reg_val);
}

void rtk_ledc_cpu_deinit(struct rtk_ws28xxx_led_priv *priv)
{
	rtk_ledc_interrupt_config(priv, LEDC_BIT_GLOBAL_INT_EN, DISABLE);
	if (priv->cpu_params.pattern_data_buff) {
		kfree(priv->cpu_params.pattern_data_buff);
		priv->cpu_params.pattern_data_buff = NULL;
	}
}

void rtk_ledc_dma_deinit(struct rtk_ledc_dmac_parameters *dma_params)
{
	if (dma_params->dma_buf_addr) {
		dma_free_coherent(dma_params->dma_dev, dma_params->dma_length, dma_params->dma_buf_addr, dma_params->dma_buf_phy_addr);
		dma_params->dma_buf_addr = NULL;
	}

	if (dma_params->chan) {
		dma_release_channel(dma_params->chan);
		dma_params->chan = NULL;
	}

	if (dma_params->config) {
		kfree(dma_params->config);
		dma_params->config = NULL;
	}
}

/**
  * @brief  Get LEDC FIFO Level
  * @param  LEDCx: selected LEDC peripheral.
  * @retval LEDC FIFO level
  */
u32 rtk_ledc_get_fifo_level(struct rtk_ws28xxx_led_priv *priv)
{
	u32 fifo_level;

	fifo_level = LEDC_GET_FIFO_TRIG_LEVEL(rtk_ledc_readl(priv->base, LEDC_DMA_CTRL_REG)) + 1;

	return fifo_level;
}

void rtk_ledc_gdma_tx_periodic(
	struct rtk_ledc_dmac_parameters *dma_params)
{
	u32 ret;

	ret = dmaengine_slave_config(dma_params->chan, dma_params->config);
	if (ret != 0) {
		dev_err(dma_params->dma_dev, "DMA engine slave config fail\n");
		goto out;
	}

	dma_params->txdesc = dmaengine_prep_dma_cyclic(dma_params->chan, dma_params->config->src_addr, dma_params->dma_length,
						 dma_params->config->dst_addr_width * dma_params->config->dst_maxburst, DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT);
	dma_params->txdesc->callback = rtk_ledc_dma_done_callback;
	dma_params->txdesc->callback_param = dma_params;

	dmaengine_submit(dma_params->txdesc);
	dma_async_issue_pending(dma_params->chan);

	return;

out:
	dma_release_channel(dma_params->chan);

}

void rtk_ledc_dma_done_callback(void *data)
{
	struct rtk_ledc_dmac_parameters *dma_params;
	dma_params = data;

	dma_params->gdma_done = 1;
}

/**
  * @brief  Init and Enable LEDC TX GDMA.
  * @param  LEDCx: selected LEDC peripheral.
  * @param  GDMA_InitStruct: pointer to a GDMA_InitTypeDef structure that contains
  *   the configuration information for the GDMA peripheral.
  * @param  CallbackData: GDMA callback data.
  * @param  CallbackFunc: GDMA callback function.
  * @param  pTxData: Tx Buffer.
  * @param  Length: Tx Count.
  * @retval   TRUE/FLASE
  */
void rtk_ledc_gdma_tx(
	struct rtk_ws28xxx_led_priv *priv,
	struct device *dev,
	struct rtk_ledc_dmac_parameters *dma_params,
	u32 length)
{
	u32 ledc_fifo_threshold;

	u8 length_long_enough = 0;

	dma_params->config = kmalloc(sizeof(*dma_params->config), GFP_KERNEL);
	dma_params->dma_length = length * 4; // each leds pattern is 32 bits.

	if (dma_params->dma_length >= 64) {
		length_long_enough = 1;
	}

	/* Fill in config parameters. */
	dma_params->config->device_fc = 1;
	dma_params->config->direction = DMA_MEM_TO_DEV;
	dma_params->config->dst_addr = priv->ledc_phy_addr + LEDC_DATA_REG;

	dma_params->config->dst_port_window_size = 0;
	dma_params->config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_params->config->slave_id = GDMA_HANDSHAKE_INTERFACE_LEDC_TX;
	dma_params->config->src_addr = dma_params->dma_buf_phy_addr;
	dma_params->config->src_port_window_size = 0;
	dma_params->config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

	ledc_fifo_threshold = rtk_ledc_get_fifo_level(priv);

	if (length_long_enough) {
		dma_params->config->dst_maxburst = 16;
		dma_params->config->src_maxburst = 16;
	} else {
		dma_params->config->dst_maxburst = 8;
		dma_params->config->src_maxburst = 8;
	}

	if (ledc_fifo_threshold == 0x08) {
		dma_params->config->dst_maxburst = 8;
		dma_params->config->src_maxburst = 8;
	}

	dma_params->gdma_count++;
	rtk_ledc_gdma_tx_periodic(dma_params);

	/* enable led. */
	rtk_ledc_enable_cmd(priv, ENABLE);
}

/**
  * @brief  Set LEDC Total Data Length
  * @param  LEDCx: selected LEDC peripheral.
  * @param  len: LEDCx data len. Value range: 0 to 8023.
  *   When the number of transmit FIFO entries is greater or equal to this value, MA request or
  *   BIT_LEDC_CPUREQ_FIFO_INT_EN happens. This parameter is recommaned to 7 or 15.
  * @retval None
  */
void rtk_ledc_set_total_len(
	struct rtk_ws28xxx_led_priv *priv, u32 total_data)
{
	u32 reg_val;

	if (!IS_LEDC_DATA_LENGTH(total_data)) {
		dev_err(priv->dev, "Illegal parameter: total data length\n");
		return;
	}

	reg_val = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);
	reg_val &= ~ LEDC_MASK_TOTAL_DATA_LENGTH;
	reg_val |= LEDC_TOTAL_DATA_LENGTH(total_data - 1);
	rtk_ledc_writel(priv->base, LEDC_CTRL_REG, reg_val);

	pr_debug("Check immediately ctl reg for total length = 0x%08X (bit 16~28)\n", rtk_ledc_readl(priv->base, LEDC_CTRL_REG));
}

/**
  * @brief  Set LEDC LED Number
  * @param  LEDCx: selected LEDC peripheral.
  * @param  Num: LEDCx LED number. Value range: 0 to 1023.
  * @retval None
  */
void rtk_ledc_set_led_num(struct rtk_ws28xxx_led_priv *priv, u32 num)
{
	u32 reg_val;

	if (!IS_LEDC_LED_NUM(num)) {
		dev_err(priv->dev, "Illegal parameter: LED number\n");
		return;
	}

	reg_val = rtk_ledc_readl(priv->base, LEDC_LED_RESET_TIMING_CTRL_REG);
	reg_val &= ~ LEDC_MASK_LED_NUM;
	reg_val |= LEDC_LED_NUM(num - 1);
	rtk_ledc_writel(priv->base, LEDC_LED_RESET_TIMING_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Idle Output Level
  * @param  LEDCx: selected LEDC peripheral.
  * @param  Pol: LEDCx Output level.
  *   This parameter can be one of the following values:
  *            @arg LEDC_IDLE_POLARITY_HIGH: LEDC Idle output High Level
  *            @arg LEDC_IDLE_POLARITY_LOW: LEDC Idle output Low Level
  * @retval None
  */
void rtk_ledc_set_polarity(struct rtk_ws28xxx_led_priv *priv, u32 Pol)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);
	if (Pol) {
		reg_val |= LEDC_BIT_LED_POLARITY;
	} else {
		reg_val &= ~LEDC_BIT_LED_POLARITY;
	}

	rtk_ledc_writel(priv->base, LEDC_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Output RGB Mode
  * @param  LEDCx: selected LEDC peripheral.
  * @param  mode: LEDCx Output mode with input GBR mode order.
  *   This parameter can be one of the following values:
  *            @arg LEDC_OUTPUT_GRB: LEDC Output in order of GRB
  *            @arg LEDC_OUTPUT_GBR: LEDC Output in order of GBR
  *            @arg LEDC_OUTPUT_RGB: LEDC Output in order of RGB
  *            @arg LEDC_OUTPUT_BGR: LEDC Output in order of BGR
  *            @arg LEDC_OUTPUT_RBG: LEDC Output in order of RBG
  *            @arg LEDC_OUTPUT_BRG: LEDC Output in order of BRG
  * @retval None
  */
void rtk_ledc_set_output_mode(
	struct rtk_ws28xxx_led_priv *priv, u32 mode)
{
	u32 reg_val;

	if (!IS_LEDC_OUTPUT_MODE(mode)) {
		dev_err(priv->dev, "Illegal parameter: output RGB mode\n");
		return;
	}

	reg_val = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);
	reg_val &= ~LEDC_MASK_LED_RGB_MODE;
	reg_val |= LEDC_LED_RGB_MODE(mode);
	rtk_ledc_writel(priv->base, LEDC_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Input data with MSB or LSB order
  * @param  LEDCx: selected LEDC peripheral.
  *   This parameter can be one of the following values:
  *            @arg LEDC_INPUT_LSB: LEDC input data in order of GRB
  *            @arg LEDC_INPUT_MSB: LEDC input data in order of GBR
  * @retval None
  */
void rtk_ledc_set_output_order(
	struct rtk_ws28xxx_led_priv *priv, u8 order)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);
	reg_val &= ~LEDC_OUTPUT_ORDER_MASK;
	reg_val |= LEDC_OUTPUT_ORDER(order);
	rtk_ledc_writel(priv->base, LEDC_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Input MSB or LSB order
  * @param  LEDCx: selected LEDC peripheral.
  * @param  order: LEDCx input order with BYTE2:BYTE1:BYTE0.
  *   Value Range is 0 to 7. 1b`0 is LSB; 1b`1 is MSB.
  * @retval None
  */
void rtk_ledc_set_input_mode(struct rtk_ws28xxx_led_priv *priv, u8 order)
{
	u32 reg_val;

	if (!IS_LEDC_INPUT_ORDER(order)) {
		dev_err(priv->dev, "Illegal parameter: input order\n");
		return;
	}

	reg_val = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);
	if (order) {
		reg_val |= LEDC_BIT_LED_MSB_TOP;
	} else {
		reg_val &= ~LEDC_BIT_LED_MSB_TOP;
	}

	rtk_ledc_writel(priv->base, LEDC_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Send LED refresh sinal
  * @param  LEDCx: selected LEDC peripheral.
  * @retval None
  */
void rtk_ledc_reset(struct rtk_ws28xxx_led_priv *priv)
{
	u32 temp_ctrl_reg;

	temp_ctrl_reg = rtk_ledc_readl(priv->base, LEDC_CTRL_REG);
	rtk_ledc_writel(priv->base, LEDC_CTRL_REG, temp_ctrl_reg | LEDC_BIT_RESET_LED_EN);
}

/**
  * @brief  Write Data to LEDC FIFO
  * @param  LEDCx: selected LEDC peripheral.
  * @param  data: LEDCx input data, only the lower 24bits valid.
  * @retval None
  */
void rtk_ledc_write_data(struct rtk_ws28xxx_led_priv *priv, u32 data)
{
	rtk_ledc_writel(priv->base, LEDC_DATA_REG, LEDC_DATA(data));
}

/**
  * @brief  Send Specific Data to LEDC FIFO
  * @param  LEDCx: selected LEDC peripheral.
  * @param  data: LEDCx input source data, only the lower 24bits valid.
  * @param  Len: data length to send.
  * @retval None
  */
void rtk_ledc_send_data(
	struct rtk_ws28xxx_led_priv *priv, u32 *data, u32 len)
{
	u32 fifo_full;
	u32 tx_max;
	u32 data_len = len;

	if (!IS_LEDC_DATA_LENGTH(len)) {
		dev_err(priv->dev, "Illegal parameter: data length\n");
		return;
	}

	fifo_full = rtk_ledc_get_interrupt(priv) & LEDC_BIT_FIFO_FULL;
	tx_max = LEDC_FIFO_DEPTH - rtk_ledc_get_fifo_level(priv);

	/* What if break in by interrupt, mask all interrupts. */
	if (!fifo_full) {
		while (tx_max--) {
			if (data != NULL) {
				rtk_ledc_write_data(priv, *data);
				data += 1;
			} else {
				break;
			}
			data_len --;

			if (data_len == 0) {
				break;
			}
		}
	}

	priv->cpu_params.data_current_p = data;
	priv->cpu_params.left_u32 = data_len;
}

/**
  * @brief  Set LEDC Reset Time Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  RstVal: LEDC reset time Value. Value Range is 1 to 0x3FFF
  * @retval None
  */
void rtk_ledc_set_reset_val(
	struct rtk_ws28xxx_led_priv *priv, u32 RstVal)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_LED_RESET_TIMING_CTRL_REG);
	reg_val &= ~LEDC_MASK_RESET_TIME;
	reg_val |= LEDC_RESET_TIME(RstVal);
	rtk_ledc_writel(priv->base, LEDC_LED_RESET_TIMING_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Reset Time Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  T1hVal: LEDC logic 1 high level time. Value Range is 0 to 0x7F.
  * @retval None
  */
void rtk_ledc_set_t1h_val(struct rtk_ws28xxx_led_priv *priv, u32 T1hVal)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_LED_T01_TIMING_CTRL_REG);
	reg_val &= ~LEDC_MASK_T1H_CNT;
	reg_val |= LEDC_T1H_CNT(T1hVal);
	rtk_ledc_writel(priv->base, LEDC_LED_T01_TIMING_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Reset Time Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  T1lVal: LEDC logic 1 low level time. Value Range is 0 to 0x7F.
  * @retval None
  */
void rtk_ledc_set_t1l_val(struct rtk_ws28xxx_led_priv *priv, u32 T1lVal)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_LED_T01_TIMING_CTRL_REG);
	reg_val &= ~LEDC_MASK_T1L_CNT;
	reg_val |= LEDC_T1L_CNT(T1lVal);
	rtk_ledc_writel(priv->base, LEDC_LED_T01_TIMING_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Reset Time Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  T0hVal: LEDC logic 0 high level time. Value Range is 0 to 0x7F.
  * @retval None
  */
void rtk_ledc_set_t0h_val(struct rtk_ws28xxx_led_priv *priv, u32 T0hVal)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_LED_T01_TIMING_CTRL_REG);
	reg_val &= ~LEDC_MASK_T0H_CNT;
	reg_val |= LEDC_T0H_CNT(T0hVal);
	rtk_ledc_writel(priv->base, LEDC_LED_T01_TIMING_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Reset Time Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  T0lVal: LEDC logic 0 low level time. Value Range is 0 to 0x7F.
  * @retval None
  */
void rtk_ledc_set_t0l_val(struct rtk_ws28xxx_led_priv *priv, u32 T0lVal)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_LED_T01_TIMING_CTRL_REG);
	reg_val &= ~LEDC_MASK_T0L_CNT;
	reg_val |= LEDC_T0L_CNT(T0lVal);
	rtk_ledc_writel(priv->base, LEDC_LED_T01_TIMING_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Wait data Time Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  WaitDataVal: LEDC wait data. Value Range is 0 to 0x7FFF.
  * @retval None
  */
void rtk_ledc_set_wait_data_time_val(
	struct rtk_ws28xxx_led_priv *priv, u32 WaitDataVal)
{
	u32 reg_val;

	reg_val = rtk_ledc_readl(priv->base, LEDC_DATA_FINISH_CNT_REG);
	reg_val &= ~LEDC_MASK_LED_WAIT_DATA_TIME;
	reg_val |= LEDC_LED_WAIT_DATA_TIME(WaitDataVal);
	rtk_ledc_writel(priv->base, LEDC_DATA_FINISH_CNT_REG, reg_val);
}

/**
  * @brief  Set LEDC Wait between Two Packages Time0 Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  WaitTime0: LEDC wait time0. Value Range is 0 to 0x1FF.
  * @param  NewState: enable or disable wait time between tow packages.
  *   This parameter can be one of the following values:
  *            @arg ENABLE
  *            @arg DISABLE
  * @retval None
  */
void rtk_ledc_set_wait_time0_val(struct rtk_ws28xxx_led_priv *priv, u32 WaitTime0, u8 NewState)
{
	u32 reg_val = 0;

	if (NewState == ENABLE) {
		reg_val = LEDC_BIT_WAIT_TIME0_EN | LEDC_TOTAL_WAIT_TIME0(WaitTime0);
	} else {
		reg_val |= ~LEDC_BIT_WAIT_TIME0_EN;
	}

	rtk_ledc_writel(priv->base, LEDC_WAIT_TIME0_CTRL_REG, reg_val);
}

/**
  * @brief  Set LEDC Wait between Two Frames Time1 Value
  * @param  LEDCx: selected LEDC peripheral.
  * @param  WaitTime1: LEDC wait time1. Value Range is 0 to 0x7FFFFFFF.
  * @param  NewState: enable or disable wait time between tow frames.
  *   This parameter can be one of the following values:
  *            @arg ENABLE
  *            @arg DISABLE
  * @retval None
  */
void rtk_ledc_set_wait_time1_val(
	struct rtk_ws28xxx_led_priv *priv, u32 WaitTime1, u8 NewState)
{
	u32 reg_val = 0;

	if (NewState == ENABLE) {
		reg_val = LEDC_BIT_WAIT_TIME1_EN | LEDC_TOTAL_WAIT_TIME1(WaitTime1);
	} else {
		reg_val |= ~LEDC_BIT_WAIT_TIME1_EN;
	}

	rtk_ledc_writel(priv->base, LEDC_WAIT_TIME1_CTRL_REG, reg_val);
}

/*************************************************************************/
/*********************** Linux LEDs Software Layer ***********************/
/*************************************************************************/
u32 ledc_irq_handler(struct rtk_ws28xxx_led_priv *priv, u32 InterruptStatus)
{
	if (InterruptStatus & LEDC_BIT_LED_TRANS_FINISH_INT) {
		rtk_ledc_clear_interrupt(priv, LEDC_BIT_LED_TRANS_FINISH_INT);
		dev_dbg(priv->dev, "Transfer done\n");

		if (rtk_ledc_get_trans_mode(priv) == LEDC_DMA_MODE) {
			priv->dma_params.gdma_done = 1;
		} else {
			priv->cpu_params.cpu_trans_done = 1;
		}
		priv->irq_result = RESULT_COMPLETE;
		rtk_ledc_soft_reset(priv);
	}

	if (InterruptStatus & LEDC_BIT_WAITDATA_TIMEOUT_INT) {
		rtk_ledc_clear_interrupt(priv, LEDC_BIT_WAITDATA_TIMEOUT_INT);

		priv->irq_result = RESULT_ERR;
		rtk_ledc_soft_reset(priv);
		if (rtk_ledc_get_trans_mode(priv) == LEDC_DMA_MODE) {
			priv->dma_params.gdma_done = 1;
		} else {
			priv->cpu_params.cpu_trans_done = 1;
		}
	}

	if (InterruptStatus & LEDC_BIT_FIFO_OVERFLOW_INT) {
		rtk_ledc_clear_interrupt(priv, LEDC_BIT_FIFO_OVERFLOW_INT);
		priv->irq_result = RESULT_ERR;
		rtk_ledc_soft_reset(priv);
	}

	if (InterruptStatus & LEDC_BIT_FIFO_CPUREQ_INT) {
		if (priv->cpu_params.data_current_p == NULL) {
			dev_err(priv->dev, "TX data error\n");
			rtk_ledc_clear_interrupt(priv, LEDC_BIT_FIFO_CPUREQ_INT);
			return -1;
		} else {
			if (!priv->cpu_params.left_u32) {
				priv->cpu_params.cpu_trans_done = 1;
				dev_dbg(priv->dev, "No more data to TX\n");
				rtk_ledc_clear_interrupt(priv, LEDC_BIT_FIFO_CPUREQ_INT);
			} else {
				rtk_ledc_send_data(priv, priv->cpu_params.data_current_p, priv->cpu_params.left_u32);
			}
		}
	}

	return 0;
}

static irqreturn_t rtk_ledc_interrupt(int irq, void *dev_id)
{
	struct rtk_ws28xxx_led_priv *priv = dev_id;
	u32 ledc_interrupt_res;

	ledc_interrupt_res = rtk_ledc_readl(priv->base, LEDC_LED_INT_STS_REG);
	priv->irq_result = ledc_interrupt_res & BIT(0);

	if (ledc_interrupt_res) {
		ledc_irq_handler(priv, ledc_interrupt_res);
	} else {
		dev_warn(priv->dev, "Expired interrupt\n");
	}

	ledc_interrupt_res = rtk_ledc_readl(priv->base, LEDC_LED_INTERRUPT_CTRL_REG);
	dev_dbg(priv->dev, "Check interrupt mask = 0x%08X\n", ledc_interrupt_res);

	return IRQ_HANDLED;
}

/**
  * @brief  Initializes the LEDC peripheral according to the specified
  *   parameters in the LEDC_InitStruct
  * @param  LEDCx: selected LEDC peripheral.
  * @param  LEDC_InitStruct: pointer to a LEDC_InitTypeDef structure that
  *   contains the configuration information for the specified LEDC peripheral
  * @retval None
  */
static void rtk_ws28xxx_ledc_hw_set(struct rtk_ws28xxx_led_priv *priv)
{
	rtk_ledc_set_reset_val(priv, priv->ledc_params.reset_ns);
	rtk_ledc_set_t0h_val(priv, priv->ledc_params.t0h_ns);
	rtk_ledc_set_t0l_val(priv, priv->ledc_params.t0l_ns);
	rtk_ledc_set_t1h_val(priv, priv->ledc_params.t1h_ns);
	rtk_ledc_set_t1l_val(priv, priv->ledc_params.t1l_ns);
	rtk_ledc_set_wait_data_time_val(priv, priv->ledc_params.wait_data_time_ns);
	rtk_ledc_set_wait_time0_val(priv, priv->ledc_params.wait_time0_ns, priv->ledc_params.wait_time0_en);
	rtk_ledc_set_wait_time1_val(priv, priv->ledc_params.wait_time1_ns, priv->ledc_params.wait_time1_en);

	rtk_ledc_set_polarity(priv, priv->ledc_params.ledc_polarity);
	rtk_ledc_set_output_mode(priv, priv->ledc_params.output_RGB_mode);

	rtk_ledc_set_trans_mode(priv, priv->ledc_params.ledc_trans_mode);
	rtk_ledc_set_fifo_level(priv, priv->ledc_params.ledc_fifo_level);
	rtk_ledc_set_led_num(priv, priv->ledc_params.led_count);
	rtk_ledc_set_total_len(priv, priv->ledc_params.data_length);

	rtk_ledc_interrupt_config(priv, LEDC_BIT_GLOBAL_INT_EN, ENABLE);

}

static void rtk_ws28xxx_led_enable(struct rtk_ws28xxx_led *leds, enum led_brightness value)
{
	rtk_ledc_enable_cmd(leds->priv, ENABLE);

	if (!(rtk_ledc_get_interrupt(leds->priv) & LEDC_BIT_FIFO_FULL)) {
		rtk_ledc_write_data(leds->priv, value);
	}
}

static void rtk_ws28xxx_led_disable(struct rtk_ws28xxx_led *leds)
{
	rtk_ledc_enable_cmd(leds->priv, DISABLE);
}

static int rtk_ws28xxx_led_set(
	struct led_classdev *ldev, enum led_brightness value)
{
	struct rtk_ws28xxx_led *leds = to_rtk_ws28xxx_led(ldev);

	mutex_lock(&leds->priv->lock);
	if (value == LED_OFF) {
		rtk_ws28xxx_led_disable(leds);
	} else {
		rtk_ws28xxx_led_enable(leds, value);
	}
	mutex_unlock(&leds->priv->lock);

	return 0;
}

static int rtk_ws28xxx_led_pattern_clear(struct led_classdev *ldev)
{
	struct rtk_ws28xxx_led *leds = to_rtk_ws28xxx_led(ldev);
	int err = 0;

	mutex_lock(&leds->priv->lock);
	rtk_ledc_soft_reset(leds->priv);
	mutex_unlock(&leds->priv->lock);

	return err;
}

static void rtk_ws28xxx_led_hw_prepare(
	struct rtk_ws28xxx_led_priv *priv, u32 len)
{
	/* Fresh LEDC reg paramters */
	priv->ledc_params.data_length = len;

	rtk_ledc_enable_cmd(priv, DISABLE);

	/* Dynamic setting of transfer-mode. */
	rtk_ws28xxx_ledc_hw_set(priv);

	rtk_ledc_set_total_len(priv, priv->ledc_params.data_length);
	rtk_ledc_interrupt_config(priv, LEDC_BIT_GLOBAL_INT_EN, ENABLE);
	rtk_ledc_interrupt_config(priv, LEDC_BIT_LED_TRANS_FINISH_INT_EN, ENABLE);
	rtk_ledc_interrupt_config(priv, LEDC_BIT_WAITDATA_TIMEOUT_INT_EN, ENABLE);
	rtk_ledc_interrupt_config(priv, LEDC_BIT_FIFO_OVERFLOW_INT_EN, ENABLE);

	dev_dbg(priv->dev, "Trans mode = %s\n", priv->ledc_params.ledc_trans_mode ? "DMA" : "CPU");
	dev_dbg(priv->dev, "Total data len set in ctl reg = 0x%08X (bit 16~28)\n", rtk_ledc_readl(priv->base, LEDC_CTRL_REG));
	dev_dbg(priv->dev, "LED num = 0x%08X (bit 0~9)\n", rtk_ledc_readl(priv->base, LEDC_LED_RESET_TIMING_CTRL_REG));
}

static int rtk_ws28xxx_led_pattern_set_cpu(struct rtk_ws28xxx_led_priv *priv, u32 len)
{
	int wait_timeout = 0;

	priv->cpu_params.cpu_trans_done = 0;
	priv->cpu_params.u32_in_total = len;
	priv->cpu_params.left_u32 = len;
	priv->cpu_params.data_start_p = priv->cpu_params.pattern_data_buff;
	priv->cpu_params.data_current_p = priv->cpu_params.pattern_data_buff;
	rtk_ledc_interrupt_config(priv, LEDC_BIT_FIFO_CPUREQ_INT_EN, ENABLE);
	rtk_ledc_enable_cmd(priv, ENABLE);
	while (wait_timeout < LEDC_TRANS_FAIL_ATTEMPS) {
		if (wait_timeout % 100 == 0) {
			dev_dbg(priv->dma_params.dma_dev, "LEDC_LED_INT_STS_REG = 0x%08X\n", rtk_ledc_readl(priv->base, LEDC_LED_INT_STS_REG));
		}
		wait_timeout++;
		if (priv->cpu_params.cpu_trans_done) {
			return 0;
		}
		msleep(2);
	}

	dev_err(priv->dma_params.dma_dev, "CPU-Send timeout\n");
	return -1;
}

static int rtk_ws28xxx_led_pattern_set_dma(
	struct rtk_ws28xxx_led_priv *priv, u32 len)
{
	int wait_timeout = 0;

	/* For test only */
	int repeat = 1;

	priv->dma_params.gdma_done = 0;
	priv->dma_params.gdma_count = 0;
	priv->dma_params.gdma_loop = repeat;

	rtk_ledc_gdma_tx(priv, priv->dma_params.dma_dev, &priv->dma_params, len);

	while (!priv->dma_params.gdma_done) {
		if (wait_timeout % 100 == 0) {
			dev_dbg(priv->dma_params.dma_dev, "LEDC_LED_INT_STS_REG = 0x%08X\n", rtk_ledc_readl(priv->base, LEDC_LED_INT_STS_REG));
		}
		if (wait_timeout > LEDC_TRANS_FAIL_ATTEMPS) {
			dev_err(priv->dma_params.dma_dev, "DMA-Send timeout\n");
			return -1;
		}
		msleep(2);
		wait_timeout++;
	}

	return 0;
}

static int rtk_ws28xxx_get_dma_chan(struct rtk_ws28xxx_led_priv *priv, struct device *dev)
{
	const char *name = "dma_leds_tx";
	struct rtk_ledc_dmac_parameters *dma_params = &priv->dma_params;

	dma_params->chan = dma_request_chan(dev, name);
	if (!dma_params->chan) {
		dev_err(dev, "Failed to request LEDC dma channel\n");
		dma_params->gdma_done = 1;
		return -EBUSY;
	}
	dev_dbg(dev, "TX virtual chan = %d\n", dma_params->chan->chan_id);
	return 0;
}

static int rtk_ws28xxx_led_pattern_set(
	struct led_classdev *ldev,
	struct led_pattern *pattern,
	u32 len, int repeat)
{
	struct rtk_ws28xxx_led *leds = to_rtk_ws28xxx_led(ldev);
	int i, ret = 0;

	mutex_lock(&leds->priv->lock);

	if (len > leds->priv->ledc_params.auto_dma_thred) {
		dev_dbg(leds->priv->dev, "Auto-dma enabled by threshold %d. Input Data Length: %d.\n", leds->priv->ledc_params.auto_dma_thred, len);
		if (rtk_ws28xxx_get_dma_chan(leds->priv, leds->priv->dma_params.dma_dev) < 0) {
			dev_err(leds->priv->dev, "Allocate dma channel fail, Still use LEDC_CPU_MODE. But transfer burst can only be below 32.");
			len = 32;
			leds->priv->ledc_params.ledc_trans_mode = LEDC_CPU_MODE;
		} else {
			leds->priv->ledc_params.ledc_trans_mode = LEDC_DMA_MODE;
		}
	} else {
		leds->priv->ledc_params.ledc_trans_mode = LEDC_CPU_MODE;
	}

	/* Get patterns */
	if (leds->priv->ledc_params.ledc_trans_mode == LEDC_DMA_MODE) {
		leds->priv->dma_params.dma_buf_addr = dma_alloc_coherent(leds->priv->dma_params.dma_dev, len * 4, &leds->priv->dma_params.dma_buf_phy_addr, GFP_KERNEL);
		for (i = 0; i < len * 4; i += 4) {
			*(leds->priv->dma_params.dma_buf_addr + i) = pattern[i / 4].brightness & 0xFF;
			*(leds->priv->dma_params.dma_buf_addr + i + 1) = (pattern[i / 4].brightness & 0xFF00) >> 8;
			*(leds->priv->dma_params.dma_buf_addr + i + 2) = (pattern[i / 4].brightness & 0xFF0000) >> 16;
		}
	} else {
		leds->priv->cpu_params.pattern_data_buff = kmalloc(len * 4, GFP_KERNEL);
		for (i = 0; i < len; i++) {
			*(leds->priv->cpu_params.pattern_data_buff + i) = pattern[i].brightness;
		}
	}

	rtk_ws28xxx_led_hw_prepare(leds->priv, len);

	if (leds->priv->ledc_params.ledc_trans_mode == LEDC_DMA_MODE) {
		ret = rtk_ws28xxx_led_pattern_set_dma(leds->priv, len);
		rtk_ledc_dma_deinit(&leds->priv->dma_params);
		goto PATTERN_OUT;

	} else {
		ret = rtk_ws28xxx_led_pattern_set_cpu(leds->priv, len);
		rtk_ledc_cpu_deinit(leds->priv);
		goto PATTERN_OUT;
	}

PATTERN_OUT:
	mutex_unlock(&leds->priv->lock);

	return ret;
}

static int rtk_ws28xxx_led_register(struct device *dev,
									struct rtk_ws28xxx_led_priv *priv)
{
	int i, err;

	for (i = 0; i < LEDC_TO_LINUX_LED_NUM; i++) {
		struct rtk_ws28xxx_led *led = &priv->leds[i];
		struct led_init_data init_data = {};

		if (!led->active) {
			continue;
		}

		led->line = i;
		led->priv = priv;
		led->ldev.brightness_set_blocking = rtk_ws28xxx_led_set;
		led->ldev.pattern_set = rtk_ws28xxx_led_pattern_set;
		led->ldev.pattern_clear = rtk_ws28xxx_led_pattern_clear;
		led->ldev.default_trigger = "pattern";

		init_data.fwnode = led->fwnode;
		init_data.devicename = "rtk_ws28xxx";
		init_data.default_label = ":";

		err = devm_led_classdev_register_ext(dev, &led->ldev, &init_data);
		if (err) {
			return err;
		}

		rtk_ws28xxx_ledc_hw_set(led->priv);
	}

	return 0;
}

static const struct of_device_id rtk_ws28xxx_led_of_match[] = {
	{ .compatible = "realtek,ameba-ws28xxx-led", },
	{ }
};
MODULE_DEVICE_TABLE(of, rtk_ws28xxx_led_of_match);

static int rtk_ws28xxx_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child = NULL;
	struct rtk_ws28xxx_led_priv *priv;
	struct resource *res;
	u32 count, reg;
	int err, ret;
	const struct of_device_id *of_id = NULL;

	of_id = of_match_device(rtk_ws28xxx_led_of_match, &pdev->dev);
	if (!of_id || strcmp(of_id->compatible, rtk_ws28xxx_led_of_match->compatible)) {
		return -EINVAL;
	}

	count = of_get_child_count(np);
	if (!count || count > LEDC_MAX_LED_NUM) {
		return -EINVAL;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, priv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		return -EINVAL;
	}

	mutex_init(&priv->lock);
	priv->base = devm_ioremap_resource(&pdev->dev, res);

	/* Init default params */
	rtk_ledc_struct_init(priv, &pdev->dev, &priv->ledc_params);

	for_each_child_of_node(np, child) {
		err = of_property_read_u32(child, "reg", &reg);
		if (err) {
			of_node_put(child);
			mutex_destroy(&priv->lock);
			return err;
		}

		if (reg >= LEDC_TO_LINUX_LED_NUM || priv->leds[reg].active) {
			of_node_put(child);
			mutex_destroy(&priv->lock);
			return -EINVAL;
		}

		priv->leds[reg].fwnode = of_fwnode_handle(child);
		priv->leds[reg].active = true;
	}
	priv->dev = dev;
	priv->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, priv->irq, rtk_ledc_interrupt, 0, dev_name(&pdev->dev), priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		return ret;
	}

	/* Enable LEDC Clock */
	priv->clock = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clock)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		return PTR_ERR(priv->clock);
	}

	clk_prepare_enable(priv->clock);

	err = rtk_ws28xxx_led_register(dev, priv);
	if (err) {
		mutex_destroy(&priv->lock);
	}

	priv->dma_params.dma_dev = &pdev->dev;
	priv->cpu_params.cpu_dev = &pdev->dev;

	return err;
}

static int rtk_ws28xxx_led_remove(struct platform_device *pdev)
{
	struct rtk_ws28xxx_led_priv *priv = platform_get_drvdata(pdev);

	mutex_destroy(&priv->lock);
	return 0;
}

static struct platform_driver rtk_ws28xxx_led_driver = {
	.driver = {
		.name = "realtek-ameba-ws28xxx-ledc",
		.of_match_table = rtk_ws28xxx_led_of_match,
	},
	.probe = rtk_ws28xxx_led_probe,
	.remove = rtk_ws28xxx_led_remove,
};

module_platform_driver(rtk_ws28xxx_led_driver);

MODULE_DESCRIPTION("Realtek Ameba LEDC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
