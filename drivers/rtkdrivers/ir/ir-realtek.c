// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek IR support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include "ir-realtek.h"

#if IS_ENABLED(CONFIG_IR_REALTEK_DECODER)
#include "ir-realtek-decoder.c"
#endif // IS_ENABLED(CONFIG_IR_REALTEK_DECODER)

static void rtk_ir_writel(
	void __iomem *ptr, u32 reg, u32 value)
{
	writel(value, (void __iomem *)((u32)ptr + reg));
}

static u32 rtk_ir_readl(void __iomem *ptr, u32 reg)
{
	return readl((void __iomem *)((u32)ptr + reg));
}

void rtk_ir_reg_update(
	void __iomem *ptr,
	u32 reg, u32 mask, u32 value)
{
	u32 temp;

	temp = rtk_ir_readl(ptr, reg);
	temp |= mask;
	temp &= (~mask | value);
	rtk_ir_writel(ptr, reg, temp);
}

void rtk_ir_reg_dump(struct rtk_ir_dev *ir_rtk)
{
#if RTK_IR_REG_DUMP
	dev_dbg(ir_rtk->dev, "IR_CLK_DIV[0x%04X] = 0x%08X\n", IR_CLK_DIV, rtk_ir_readl(ir_rtk->base, IR_CLK_DIV));
	dev_dbg(ir_rtk->dev, "IR_TX_CONFIG[0x%04X] = 0x%08X\n", IR_TX_CONFIG, rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG));
	dev_dbg(ir_rtk->dev, "IR_TX_SR[0x%04X] = 0x%08X\n", IR_TX_SR, rtk_ir_readl(ir_rtk->base, IR_TX_SR));
	dev_dbg(ir_rtk->dev, "IR_TX_COMPE_DIV[0x%04X] = 0x%08X\n", IR_TX_COMPE_DIV, rtk_ir_readl(ir_rtk->base, IR_TX_COMPE_DIV));
	dev_dbg(ir_rtk->dev, "IR_RX_CONFIG[0x%04X] = 0x%08X\n", IR_RX_CONFIG, rtk_ir_readl(ir_rtk->base, IR_RX_CONFIG));
	dev_dbg(ir_rtk->dev, "IR_RX_SR[0x%04X] = 0x%08X\n", IR_RX_SR, rtk_ir_readl(ir_rtk->base, IR_RX_SR));
	dev_dbg(ir_rtk->dev, "IR_RX_CNT_INT_SEL[0x%04X] = 0x%08X\n", IR_RX_CNT_INT_SEL, rtk_ir_readl(ir_rtk->base, IR_RX_CNT_INT_SEL));
	dev_dbg(ir_rtk->dev, "IR_VERSION[0x%04X] = 0x%08X\n", IR_VERSION, rtk_ir_readl(ir_rtk->base, IR_VERSION));
#endif // RTK_IR_REG_DUMP
}

void rtk_ir_init_hw(struct rtk_ir_dev *ir_rtk)
{
	/* Configure IR clock divider. Formula: IR_CLK = IO_CLK/(1+IR_CLK_DIV) */
	rtk_ir_writel(ir_rtk->base, IR_CLK_DIV, (ir_rtk->ir_param.ir_clock) / (ir_rtk->ir_param.ir_freq) - 1);

	if (ir_rtk->ir_param.ir_mode == IR_MODE_TX) {
		/* Check the parameters in TX mode */
		if (!IS_IR_FREQUENCY(ir_rtk->ir_param.ir_freq)) {
			dev_err(ir_rtk->dev, "Illegal parameter: ir_freq\n");
			return;
		}
		/* Save IR TX interrupt configuration */
		rtk_ir_writel(ir_rtk->base, IR_TX_CONFIG, 0x3F);

		/* Configure TX mode parameters and disable all TX interrupt */
		rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG,
						  IR_TX_DUTY_NUM(0xFFF) | IR_TX_DATA_TYPE_SET(1) | IR_TX_FIFO_LEVEL_TH(0x1F),
						  (IR_TX_DUTY_NUM((rtk_ir_readl(ir_rtk->base, IR_CLK_DIV) + 1) / (ir_rtk->ir_param.ir_duty_cycle))) |
						  (IR_TX_DATA_TYPE_SET(ir_rtk->ir_param.ir_tx_inverse)) |
						  (IR_TX_FIFO_LEVEL_TH(ir_rtk->ir_param.ir_tx_fifo_threshold)));

		if (ir_rtk->ir_param.ir_tx_idle_level) {
			rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG,
							  IR_BIT_TX_IDLE_STATE, IR_BIT_TX_IDLE_STATE);
		} else {
			rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG,
							  IR_BIT_TX_IDLE_STATE, ~IR_BIT_TX_IDLE_STATE);
		}

		/* Clear all TX interrupt and TX FIFO */
		rtk_ir_writel(ir_rtk->base, IR_TX_INT_CLR, IR_TX_INT_ALL_CLR | IR_BIT_TX_FIFO_CLR);

		/* Configure IR tx compensation clock divisor. */
		rtk_ir_writel(ir_rtk->base, IR_TX_COMPE_DIV,
					  (ir_rtk->ir_param.ir_clock) / (ir_rtk->ir_param.ir_tx_comp_clk) - 1);
	} else {
		/* Check the parameters in RX mode */
		if (!IS_IR_RX_TRIGGER_EDGE(ir_rtk->ir_param.ir_rx_trigger_mode)) {
			dev_err(ir_rtk->dev, "Illegal parameter: ir_rx_trigger_mode\n");
			return;
		}

		/* Enable RX mode */
		rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG, IR_BIT_MODE_SEL, IR_BIT_MODE_SEL);
		dev_dbg(ir_rtk->dev, "Set RX mode result = 0x%08X\n", IR_MODE(rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG)));
		/* Configure RX mode parameters and disable all RX interrupt */
		if (ir_rtk->ir_param.ir_rx_auto) {
			rtk_ir_writel(ir_rtk->base, IR_RX_CONFIG, IR_BIT_RX_START_MODE |
						  (IR_RX_TRIGGER_MODE(ir_rtk->ir_param.ir_rx_trigger_mode)) |
						  (IR_RX_FILTER_STAGETX(ir_rtk->ir_param.ir_rx_filter_time)) |
						  (IR_RX_FIFO_LEVEL_TH(ir_rtk->ir_param.ir_rx_fifo_threshold)));
		} else {
			rtk_ir_writel(ir_rtk->base, IR_RX_CONFIG,
						  (IR_RX_TRIGGER_MODE(ir_rtk->ir_param.ir_rx_trigger_mode)) |
						  (IR_RX_FILTER_STAGETX(ir_rtk->ir_param.ir_rx_filter_time)) |
						  (IR_RX_FIFO_LEVEL_TH(ir_rtk->ir_param.ir_rx_fifo_threshold)));
		}

		if (ir_rtk->ir_param.ir_rx_fifo_full_ctrl) {
			rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG,
							  IR_BIT_RX_FIFO_DISCARD_SET, IR_BIT_RX_FIFO_DISCARD_SET);
		} else {
			rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG,
							  IR_BIT_RX_FIFO_DISCARD_SET, ~IR_BIT_RX_FIFO_DISCARD_SET);
		}

		/* Configure IR RX counter threshold parameters */
		if (ir_rtk->ir_param.ir_rx_cnt_thr_type) {
			rtk_ir_writel(ir_rtk->base, IR_RX_CNT_INT_SEL,
						  IR_BIT_RX_CNT_THR_TRIGGER_LV |
						  IR_RX_CNT_THR(ir_rtk->ir_param.ir_rx_cnt_thr));
		} else {
			rtk_ir_writel(ir_rtk->base, IR_RX_CNT_INT_SEL,
						  IR_RX_CNT_THR(ir_rtk->ir_param.ir_rx_cnt_thr));
		}

		/* Clear all RX interrupt and RX FIFO */
		rtk_ir_writel(ir_rtk->base, IR_RX_INT_CLR,
					  IR_RX_INT_ALL_CLR | IR_BIT_RX_FIFO_CLR);
	}
}

static void rtk_get_dts_info(
	struct rtk_ir_dev *ir_rtk,
	struct device_node *np,
	u32 *param_to_set, int default_value,
	char *dts_name)
{
	int nr_requests, ret;

	/* Get DTS params. */
	ret = of_property_read_u32(np, dts_name, &nr_requests);
	if (ret) {
		dev_warn(ir_rtk->dev, "Can't get DTS property %s, set it to default value %d\n", dts_name, default_value);
		*param_to_set = default_value;
	} else {
		dev_dbg(ir_rtk->dev, "Get DTS property %s = %d\n", dts_name, nr_requests);
		*param_to_set = nr_requests;
	}
}

void rtk_ir_struct_init(
	struct rtk_ir_dev *ir_rtk,
	struct device_node *np)
{
	char s1[] = "rtk,ir-receiver";
	char s2[] = "rtk,ir-tx-encode";
	char s3[] = "rtk,ir-rx-auto";
	char s4[] = "rtk,ir-cnt-threshold";
	char s5[] = "rtk,ir-rx-trigger-mode";
	char s6[] = "rtk,ir-cnt-thred-type";
	char s7[] = "rtk,ir-idle-level";
	char s8[] = "rtk,ir_rx_inverse";

	ir_rtk->ir_manage.duty_cycle = 50;

	if (np) {
		rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_rx_enable_mode, 0, s1);
	}
	if (ir_rtk->ir_param.ir_rx_enable_mode) {
		ir_rtk->ir_param.ir_mode = IR_MODE_RX;
		rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_rx_auto, 1, s3);
	} else {
		ir_rtk->ir_param.ir_mode = IR_MODE_TX;
		rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_tx_encode_enable, 0, s2);
	}
	rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_rx_cnt_thr, 30000, s4);
	rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_rx_trigger_mode, IR_RX_FALL_EDGE, s5);
	rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_rx_cnt_thr_type, IR_RX_COUNT_HIGH_LEVEL, s6);
	rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_tx_idle_level, IR_IDLE_OUTPUT_LOW, s7);
	rtk_get_dts_info(ir_rtk, np, &ir_rtk->ir_param.ir_rx_inverse, 0, s8);

	ir_rtk->ir_param.ir_clock = 100000000; // 100MHz
	ir_rtk->ir_param.ir_freq = 38000; // 38kHz
	ir_rtk->ir_param.ir_duty_cycle = 3;
	ir_rtk->ir_param.ir_tx_inverse = IR_TX_DATA_NORMAL_CARRIER_NORMAL;
	ir_rtk->ir_param.ir_tx_fifo_threshold = 0;
	ir_rtk->ir_param.ir_tx_comp_clk = 1000000; // 1M
	ir_rtk->ir_param.ir_rx_fifo_threshold = 0;
	ir_rtk->ir_param.ir_rx_fifo_full_ctrl = IR_RX_FIFO_FULL_DISCARD_NEWEST;
	ir_rtk->ir_param.ir_rx_filter_time = IR_RX_FILTER_TIME_50NS;
}

void rtk_ir_enable_cmd(
	struct rtk_ir_dev *ir_rtk, u32 mode, u32 new_state)
{
	if (new_state == ENABLE) {
		if (mode == IR_MODE_TX) {
			rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG, IR_BIT_TX_START, IR_BIT_TX_START);
		} else {
			rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG, IR_BIT_RX_START, IR_BIT_RX_START);
		}
	} else if (new_state == DISABLE) {
		if (mode == IR_MODE_TX) {
			rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG, IR_BIT_TX_START, ~IR_BIT_TX_START);
		} else {
			rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG, IR_BIT_RX_START, ~IR_BIT_RX_START);
		}
	}
}

void rtk_ir_send_buf(
	struct rtk_ir_dev *ir_rtk,
	u32 *pbuf, u32 len, u32 is_last_pkt)
{
	if (len == 0) {
		return;
	}

	while (--len) {
		rtk_ir_writel(ir_rtk->base, IR_TX_FIFO, *pbuf++);
	}

	/* If send the last IR packet, set the following bit */
	if (is_last_pkt) {
		rtk_ir_writel(ir_rtk->base, IR_TX_FIFO, *pbuf | IR_BIT_TX_DATA_END_FLAG);
	} else {
		rtk_ir_writel(ir_rtk->base, IR_TX_FIFO, *pbuf);
	}

}

void rtk_ir_interrupt_config(
	struct rtk_ir_dev *ir_rtk, u32 ir_int, u32 new_state)
{
	if (ir_rtk->ir_param.ir_mode == IR_MODE_RX) {
		if (new_state == ENABLE) {
			rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG, ir_int, ir_int);
		} else {
			rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG, ir_int, ~ir_int);
		}
	} else {
		if (new_state == ENABLE) {
			rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG, ir_int, ir_int);
		} else {
			rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG, ir_int, ~ir_int);
		}
	}
}

u32 rtk_ir_get_int_status(struct rtk_ir_dev *ir_rtk)
{
	u32 status;

	if (IR_MODE(rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG))) {
		status = rtk_ir_readl(ir_rtk->base, IR_RX_SR);
	} else {
		status = rtk_ir_readl(ir_rtk->base, IR_TX_SR);
	}

	return status;
}

u32 rtk_ir_get_mask_type(struct rtk_ir_dev *ir_rtk)
{
	u32 mask;

	if (IR_MODE(rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG))) {
		mask = rtk_ir_readl(ir_rtk->base, IR_RX_CONFIG) & IR_RX_INT_ALL_MASK;
	} else {
		mask = rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG) & IR_TX_INT_ALL_MASK;
	}

	return  mask;
}

void rtk_ir_clear_int_pending_bit(
	struct rtk_ir_dev *ir_rtk, u32 int_to_clear)
{
	if (IR_MODE(rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG))) {
		if (!IS_RX_INT_CLR(int_to_clear)) {
			dev_err(ir_rtk->dev, "Illegal RX clear bit\n");
			return;
		}
		rtk_ir_reg_update(ir_rtk->base, IR_RX_INT_CLR, int_to_clear, int_to_clear);
	} else {
		if (!IS_TX_INT_CLR(int_to_clear)) {
			dev_err(ir_rtk->dev, "Illegal TX clear bit\n");
			return;
		}
		rtk_ir_reg_update(ir_rtk->base, IR_TX_INT_CLR, int_to_clear, int_to_clear);
	}
}

u32 rtk_ir_get_tx_fifo_free_len(
	struct rtk_ir_dev *ir_rtk)
{
	return (IR_TX_FIFO_SIZE - IR_GET_TX_FIFO_OFFSET(rtk_ir_readl(ir_rtk->base, IR_TX_SR)));
}

u32 rtk_ir_get_rx_fifo_free_len(
	struct rtk_ir_dev *ir_rtk)
{
	return (IR_GET_RX_FIFO_OFFSET(rtk_ir_readl(ir_rtk->base, IR_RX_SR)));
}

u32 rtk_ir_receive_data(struct rtk_ir_dev *ir_rtk)
{
	u32 temp;
	temp = rtk_ir_readl(ir_rtk->base, IR_RX_FIFO);
	dev_dbg(ir_rtk->dev, "RX %s cnt = %d\n", temp & IR_BIT_RX_LEVEL ? "pulse" : "space", temp & IR_MASK_RX_CNT);
	return temp;
}

void rtk_ir_clear_rx_fifo(struct rtk_ir_dev *ir_rtk)
{
	rtk_ir_writel(ir_rtk->base, IR_RX_INT_CLR, IR_BIT_RX_FIFO_CLR);
}

void rtk_ir_start_manual_rx_trigger(
	struct rtk_ir_dev *ir_rtk, u32 new_state)
{
	/* Start RX manual mode */
	if (new_state == ENABLE) {
		rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG, IR_BIT_RX_MAN_START, IR_BIT_RX_MAN_START);
	} else {
		rtk_ir_reg_update(ir_rtk->base, IR_RX_CONFIG, IR_BIT_RX_MAN_START, ~IR_BIT_RX_MAN_START);
	}
}

void rtk_ir_set_rx_cnt_threshlod(
	struct rtk_ir_dev *ir_rtk,
	u32 ir_rx_cnt_thr_type,
	u32 ir_rx_cnt_thr)
{
	/* Configure IR RX counter threshold parameters */
	rtk_ir_writel(ir_rtk->base, IR_RX_CNT_INT_SEL, (ir_rx_cnt_thr_type) | IR_RX_CNT_THR(ir_rx_cnt_thr));
}

void rtk_ir_clear_tx_fifo(struct rtk_ir_dev *ir_rtk)
{
	rtk_ir_writel(ir_rtk->base, IR_TX_INT_CLR, IR_BIT_TX_FIFO_CLR);
}

#if RTK_IR_HW_CONTROL_FOR_FUTURE_USE
void rtk_ir_receive_buf(
	struct rtk_ir_dev *ir_rtk, u32 *pbuf, u32 len)
{
	while (len--) {
		*pbuf++ = rtk_ir_readl(ir_rtk->base, IR_RX_FIFO);
	}
}

u32 rtk_ir_fsm_running(struct rtk_ir_dev *ir_rtk)
{
	u32 status = FALSE;

	if (IR_MODE(rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG))) {
		if (rtk_ir_readl(ir_rtk->base, IR_RX_SR) & IR_BIT_RX_STATE) {
			status = TRUE;
		}
	} else {
		if (rtk_ir_readl(ir_rtk->base, IR_TX_SR) & IR_BIT_TX_STATUS) {
			status = TRUE;
		}
	}

	return status;
}

void rtk_ir_set_tx_threshold(
	struct rtk_ir_dev *ir_rtk, uint8_t thd)
{
	u32 tx_config;

	tx_config = rtk_ir_readl(ir_rtk->base, IR_TX_CONFIG);
	tx_config &= ~IR_MASK_TX_FIFO_LEVEL_TH;
	tx_config |= IR_TX_FIFO_LEVEL_TH(thd);
	rtk_ir_writel(ir_rtk->base, IR_TX_CONFIG, tx_config);
}

void rtk_ir_set_rx_threshold(
	struct rtk_ir_dev *ir_rtk, uint8_t thd)
{
	u32 rx_config;

	rx_config = rtk_ir_readl(ir_rtk->base, IR_RX_CONFIG);
	rx_config &= ~IR_MASK_RX_FIFO_LEVEL_TH;
	rx_config |= IR_RX_FIFO_LEVEL_TH(thd);
	rtk_ir_writel(ir_rtk->base, IR_RX_CONFIG, rx_config);
}

void rtk_ir_send_data(
	struct rtk_ir_dev *ir_rtk, u32 data)
{
	rtk_ir_writel(ir_rtk->base, IR_TX_FIFO, data);
}

#endif // RTK_IR_HW_CONTROL_FOR_FUTURE_USE

void ir_rx_prepare(struct rtk_ir_dev *ir_rtk)
{
	/* Initialize IR */
	ir_rtk->ir_param.ir_mode = IR_MODE_RX;
	ir_rtk->ir_param.ir_freq = 5000000;
	ir_rtk->ir_param.ir_rx_fifo_threshold = 5;

	rtk_ir_enable_cmd(ir_rtk, IR_MODE_RX, DISABLE);
	rtk_ir_init_hw(ir_rtk);

	/* Enable all interrupts and disable all masks. */
	rtk_ir_interrupt_config(ir_rtk, IR_RX_INT_ALL_EN, ENABLE);
	rtk_ir_interrupt_config(ir_rtk, IR_RX_INT_ALL_MASK, DISABLE);

	/* Mask the first idle before IR recv wave. */
	rtk_ir_interrupt_config(ir_rtk, IR_BIT_RX_CNT_THR_INT_EN, DISABLE);
	dev_dbg(ir_rtk->dev, "IR_RX_CONFIG= 0x%08X\n", rtk_ir_readl(ir_rtk->base, IR_RX_CONFIG));
	rtk_ir_enable_cmd(ir_rtk, IR_MODE_RX, ENABLE);
}

void ir_recv_end(struct rtk_ir_dev *ir_rtk)
{
	int i, us_duration = 0;
	struct ir_raw_event rx_ev;

	ir_raw_event_reset(ir_rtk->rcdev);

	ir_rtk->rcdev->raw->dev->enabled_protocols = RC_PROTO_BIT_NEC;
	rx_ev.carrier = ir_rtk->ir_param.ir_freq;
	rx_ev.duty_cycle = 1 / ir_rtk->ir_param.ir_duty_cycle;
	rx_ev.reset = 0;
	rx_ev.carrier_report = 0;

	for (i = 0; i < ir_rtk->ir_manage.wbuf_index; i++) {
		if (ir_rtk->ir_manage.wbuf[i] & IR_BIT_RX_LEVEL) {
			us_duration =  1000 * (ir_rtk->ir_manage.wbuf[i] & IR_MASK_RX_CNT) / (ir_rtk->ir_param.ir_freq / 1000);
			rx_ev.duration = US_TO_NS(us_duration);
			rx_ev.pulse = ir_rtk->ir_param.ir_rx_inverse? 0 : 1;
			ir_raw_event_store(ir_rtk->rcdev, &rx_ev);
		} else {
			us_duration =  1000 * ir_rtk->ir_manage.wbuf[i] / (ir_rtk->ir_param.ir_freq / 1000);
			rx_ev.duration = US_TO_NS(us_duration);
			rx_ev.pulse = ir_rtk->ir_param.ir_rx_inverse? 1 : 0;
			ir_raw_event_store(ir_rtk->rcdev, &rx_ev);
		}
	}

	/* Start NEC Decode. */
	/* NEC decode has a big edian to little edian change. */
	/* In test, tx:   0101 0111  0010 0011  0101 0000  1010 1111*/
	/* RTK-IR recved: 1110 1010  1100 0100  0000 1010  1111 0101 */
	ir_raw_event_handle(ir_rtk->rcdev);

	rtk_ir_clear_rx_fifo(ir_rtk);

	ir_rtk->ir_manage.wbuf_index = 0;
	ir_rtk->ir_manage.pulse_duration = 0;
}

int ir_rx_recv(void *input_param)
{
	u32 reg_data;
	u32 data;
	u32 len, i;

	struct rtk_ir_dev *ir_rtk = input_param;

	len = rtk_ir_get_rx_fifo_free_len(ir_rtk);

	while (len) {
		if ((ir_rtk->ir_manage.wbuf_index + len) >= (MAX_BUF_LEN - 1)) {
			dev_info(ir_rtk->dev, "Software buffer is full, drop newest data and refresh RX hardware\n");
			rtk_ir_enable_cmd(ir_rtk, IR_MODE_RX, DISABLE);
			ir_recv_end(ir_rtk);
			rtk_ir_enable_cmd(ir_rtk, IR_MODE_RX, ENABLE);
			return 0;
		}
		/*****************************************************************************************/
		/*** Attention: demodulator may recv inverse wave, see debug info in ir-nec-decoder.c ****/
		/*** Configure "rtk,ir_rx_inverse = <1>; rtk,ir-cnt-thred-type = <1>;" in dts ************/
		/*****************************************************************************************/
		/*****************************************************************************************/
		/* Attention: configurations of wire-to-wire are as follow. See instructions for ir-led. */
		/*** Set "rtk,ir-cnt-thred-type = <0>; rtk,rtk,ir-rx-trigger-mode = <1>;" in dts *********/
		/*****************************************************************************************/
		for (i = 0; i < len; i++) {
			reg_data = rtk_ir_receive_data(ir_rtk);
			data = reg_data & IR_MASK_RX_CNT;
			if (data > RTK_IR_MAX_CARRIER) {
				if (ir_rtk->ir_manage.pulse_duration) {
					dev_dbg(ir_rtk->dev, "IR LED: pulse = %d, space = %d.\n",
						1000 * ir_rtk->ir_manage.pulse_duration / (ir_rtk->ir_param.ir_freq / 1000),
						1000 * data / (ir_rtk->ir_param.ir_freq / 1000));
					/* ir led pulse total. */
					ir_rtk->ir_manage.wbuf[ir_rtk->ir_manage.wbuf_index] = IR_BIT_RX_LEVEL | ir_rtk->ir_manage.pulse_duration;
					ir_rtk->ir_manage.wbuf_index++;
					/* ir led space duration. */
					ir_rtk->ir_manage.wbuf[ir_rtk->ir_manage.wbuf_index] = data;
					ir_rtk->ir_manage.wbuf_index++;
				} else if (reg_data & IR_BIT_RX_LEVEL) {
					dev_dbg(ir_rtk->dev, "IR demodulator: pulse = %d\n", 1000 * data / (ir_rtk->ir_param.ir_freq / 1000));
					/* ir demodulator pulse. */
					ir_rtk->ir_manage.wbuf[ir_rtk->ir_manage.wbuf_index] = IR_BIT_RX_LEVEL | data;
					ir_rtk->ir_manage.wbuf_index++;
				} else {
					/* first space, or ir demodulator space. */
					dev_dbg(ir_rtk->dev, "IR demodulator: space = %d\n", 1000 * data / (ir_rtk->ir_param.ir_freq / 1000));
					ir_rtk->ir_manage.wbuf[ir_rtk->ir_manage.wbuf_index] = data;
					ir_rtk->ir_manage.wbuf_index++;
				}
				ir_rtk->ir_manage.pulse_duration = 0;
			} else {
				/* ir led small pulse. */
				ir_rtk->ir_manage.pulse_duration += data;
			}
		}

		len = rtk_ir_get_rx_fifo_free_len(ir_rtk);
	}
	return 0;

}

void ir_tx_start(struct rtk_ir_dev *ir_rtk)
{
	u32 tx_count = 0;
	int retry_count = 0;

	/* Initialize IR */
	ir_rtk->ir_param.ir_mode = IR_MODE_TX;
	ir_rtk->ir_param.ir_freq = 38000;
	rtk_ir_enable_cmd(ir_rtk, IR_MODE_TX, DISABLE);
	rtk_ir_init_hw(ir_rtk);

	rtk_ir_enable_cmd(ir_rtk, IR_MODE_TX, ENABLE);
	rtk_ir_clear_tx_fifo(ir_rtk);

	rtk_ir_send_buf(ir_rtk, ir_rtk->ir_manage.wbuf, IR_TX_FIFO_SIZE, 0);
	tx_count += IR_TX_FIFO_SIZE;

	dev_dbg(ir_rtk->dev, "Start TX, count = %d, left = %d\n", tx_count, ir_rtk->ir_manage.wbuf_len - tx_count);

	while (ir_rtk->ir_manage.wbuf_len - tx_count > 0) {
		while (rtk_ir_get_tx_fifo_free_len(ir_rtk) < RTK_IR_TX_THRESHOLD) {
			retry_count++;
			if (retry_count >= RTK_IR_TX_RETRY_TIMES) {
				goto ir_tx_end;
			}
		}
		retry_count = 0;

		if (ir_rtk->ir_manage.wbuf_len - tx_count > RTK_IR_TX_THRESHOLD) {
			rtk_ir_send_buf(ir_rtk, ir_rtk->ir_manage.wbuf + tx_count, RTK_IR_TX_THRESHOLD, 0);
			tx_count += RTK_IR_TX_THRESHOLD;
		} else {
			rtk_ir_send_buf(ir_rtk, ir_rtk->ir_manage.wbuf + tx_count, ir_rtk->ir_manage.wbuf_len - tx_count, 1);
			tx_count = ir_rtk->ir_manage.wbuf_len;
		}
	}

ir_tx_end:
	ir_rtk->ir_manage.ir_status = RTK_IR_TX_DONE;
}

static irqreturn_t rtk_ir_isr_event(int irq, void *data)
{
	struct rtk_ir_dev *ir_rtk = data;
	u32 int_status, int_mask;

	int_status = rtk_ir_get_int_status(ir_rtk);
	int_mask = rtk_ir_get_mask_type(ir_rtk);

	if (ir_rtk->ir_param.ir_mode == IR_MODE_RX) {
		rtk_ir_interrupt_config(ir_rtk, IR_RX_INT_ALL_EN, DISABLE);
		rtk_ir_interrupt_config(ir_rtk, IR_RX_INT_ALL_MASK, ENABLE);

		if (int_status & IR_BIT_RX_FIFO_FULL_INT_STATUS) {
			dev_dbg(ir_rtk->dev, "RX fifo full\n");
			rtk_ir_clear_int_pending_bit(ir_rtk, IR_BIT_RX_FIFO_FULL_INT_CLR);
		}

		if (int_status & IR_BIT_RX_FIFO_LEVEL_INT_STATUS) {
			dev_dbg(ir_rtk->dev, "Start RX interrupt\n");
			ir_rx_recv(ir_rtk);
			rtk_ir_clear_int_pending_bit(ir_rtk, IR_BIT_RX_FIFO_LEVEL_INT_CLR);
		}

		if (int_status & IR_BIT_RX_CNT_OF_INT_STATUS) {
			rtk_ir_clear_int_pending_bit(ir_rtk, IR_BIT_RX_CNT_OF_INT_CLR);
		}

		if (int_status & IR_BIT_RX_FIFO_OF_INT_STATUS) {
			dev_dbg(ir_rtk->dev, "RX fifo overflow\n");
			rtk_ir_clear_int_pending_bit(ir_rtk, IR_BIT_RX_FIFO_OF_INT_CLR);
		}

		if (int_status & IR_BIT_RX_CNT_THR_INT_STATUS) {
			dev_dbg(ir_rtk->dev, "RX count threshold interrupt. RX end\n");
			ir_rx_recv(ir_rtk);
			ir_recv_end(ir_rtk);
			rtk_ir_clear_int_pending_bit(ir_rtk, IR_BIT_RX_CNT_THR_INT_CLR);
		}

		if (int_status & IR_BIT_RX_FIFO_ERROR_INT_STATUS) {
			rtk_ir_clear_int_pending_bit(ir_rtk, IR_BIT_RX_FIFO_ERROR_INT_CLR);
		}

		rtk_ir_interrupt_config(ir_rtk, IR_RX_INT_ALL_EN, ENABLE);
		rtk_ir_interrupt_config(ir_rtk, IR_RX_INT_ALL_MASK, DISABLE);

	}

	return IRQ_HANDLED;
}

static inline void ir_rtk_on(struct rtk_ir_dev *ir_rtk)
{
	if (ir_rtk->ir_param.ir_rx_enable_mode) {
		ir_rx_prepare(ir_rtk);
	}
}

static inline void ir_rtk_off(struct rtk_ir_dev *ir_rtk)
{
	if (ir_rtk->ir_param.ir_rx_enable_mode) {
		rtk_ir_interrupt_config(ir_rtk, IR_RX_INT_ALL_MASK, DISABLE);
		rtk_ir_enable_cmd(ir_rtk, IR_MODE_RX, DISABLE);
	}
}

int ir_rtk_set_tx_duty_cycle(
	struct rc_dev *dev, u32 duty_cycle)
{
	struct rtk_ir_dev *ir_rtk = dev->priv;

	ir_rtk->ir_param.ir_duty_cycle = duty_cycle;
	rtk_ir_reg_update(ir_rtk->base, IR_TX_CONFIG, IR_TX_DUTY_NUM(0xFFF),
					  IR_TX_DUTY_NUM((rtk_ir_readl(ir_rtk->base, IR_CLK_DIV) + 1) / (ir_rtk->ir_param.ir_duty_cycle)));
	return 0;
}

static int ir_rtk_tx(struct rc_dev *dev, unsigned int *buffer,
					 unsigned int count)
{
	struct rtk_ir_dev *ir_rtk = dev->priv;
	int i;

	if (ir_rtk->ir_manage.ir_status == RTK_IR_TX_ONGOING) {
		return -EINVAL;
	}

	if (count > MAX_BUF_LEN) {
		return -EINVAL;
	}

	dev_dbg(ir_rtk->dev, "Get TX count = %d\n", count);
	for (i = 0; i < count; i++) {
		dev_dbg(ir_rtk->dev, "Dump TX buf[%d] = 0x%08X\n", i, *(buffer + i));
	}

	ir_rtk->ir_manage.ir_status = RTK_IR_TX_ONGOING;

	for (i = 0; i < count; i++) {
		if (i % 2) {
			/* NEC space. */
			ir_rtk->ir_manage.wbuf[i] = *(buffer + i) * ir_rtk->ir_param.ir_freq / 1000000;
		} else {
			/* NEC pulse. */
			ir_rtk->ir_manage.wbuf[i] = (*(buffer + i) * ir_rtk->ir_param.ir_freq / 1000000) | IR_BIT_TX_DATA_TYPE;
		}
	}

	ir_rtk->ir_manage.wbuf_index = 0;
	ir_rtk->ir_manage.wbuf_len = count;

	ir_tx_start(ir_rtk);
	return count;
}

static int ir_rtk_open(struct rc_dev *dev)
{
	struct rtk_ir_dev *ir_rtk = dev->priv;

	if (test_and_set_bit(1, &ir_rtk->ir_manage.device_is_open)) {
		return -EBUSY;
	}

	/* When IR device is open, ready to RX. */
	if (ir_rtk->ir_param.ir_rx_enable_mode) {
		ir_rtk->ir_manage.wbuf_index = 0;
		ir_rtk->ir_manage.pulse_duration = 0;
		ir_rx_prepare(ir_rtk);
	}

	rtk_ir_reg_dump(ir_rtk);

	return 0;
}

static void ir_rtk_release(struct rc_dev *dev)
{
	struct rtk_ir_dev *ir_rtk = dev->priv;

	if (!ir_rtk) {
		return;
	}

	if (ir_rtk->ir_param.ir_rx_enable_mode) {
		if (!ir_rtk->ir_param.ir_rx_auto) {
			rtk_ir_start_manual_rx_trigger(ir_rtk, DISABLE);
		}
	}
	ir_rtk_off(ir_rtk);

	clear_bit(1, &ir_rtk->ir_manage.device_is_open);
}

static int ir_rtk_set_duty_cycle(
	struct rc_dev *dev, u32 duty)
{
	struct rtk_ir_dev *ir_rtk = dev->priv;

	ir_rtk->ir_manage.duty_cycle = duty;

	return 0;
}

static int ir_rtk_tx_carrier_send(
	struct rc_dev *dev, u32 carrier)
{
	struct rtk_ir_dev *ir_rtk = dev->priv;

	if (carrier > 500000 || carrier < 20000) {
		return -EINVAL;
	}

	ir_rtk->ir_manage.freq = carrier;
	ir_rtk->ir_param.ir_freq = carrier;

	return 0;
}

#define ir_rtk_suspend	NULL
#define ir_rtk_resume	NULL

static const struct of_device_id rtk_ir_match[] = {
	{
		.compatible = "realtek,ameba-ir",
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_ir_match);

static int ir_rtk_probe(struct platform_device *pdev)
{
	//struct pwm_device		*pwm;
	struct rc_dev			*rcdev;
	struct rtk_ir_dev		*ir_rtk;
	struct resource			*res;
	struct device_node		*np = pdev->dev.of_node;
	int ret;
	const struct of_device_id *of_id = NULL;

	of_id = of_match_device(rtk_ir_match, &pdev->dev);
	if (!of_id || strcmp(of_id->compatible, rtk_ir_match->compatible)) {
		return -EINVAL;
	}

	ir_rtk = devm_kzalloc(&pdev->dev, sizeof(struct rtk_ir_dev), GFP_KERNEL);
	if (!ir_rtk) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ir_rtk->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ir_rtk->base)) {
		return PTR_ERR(ir_rtk->base);
	}

	/* Enable IR Clock. */
	ir_rtk->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(ir_rtk->clk)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		return PTR_ERR(ir_rtk->clk);
	}
	clk_prepare_enable(ir_rtk->clk);

	ir_rtk->dev = &pdev->dev;
	ir_rtk->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, ir_rtk->irq, rtk_ir_isr_event, 0, dev_name(&pdev->dev), ir_rtk);

	ir_rtk->dev = &pdev->dev;

	rtk_ir_struct_init(ir_rtk, np);

	if (ir_rtk->ir_param.ir_rx_enable_mode) {
		dev_dbg(ir_rtk->dev, "Register IR raw\n");
		rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW);
	} else if (!ir_rtk->ir_param.ir_rx_enable_mode && !ir_rtk->ir_param.ir_tx_encode_enable) {
		dev_dbg(ir_rtk->dev, "Register IR raw TX\n");
		rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW_TX);
	} else {
		dev_dbg(ir_rtk->dev, "Register IR scancode\n");
		rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_SCANCODE);
	}

	if (!rcdev) {
		return -ENOMEM;
	}

	rcdev->priv = ir_rtk;
	rcdev->open = ir_rtk_open;
	rcdev->close = ir_rtk_release;
	rcdev->tx_ir = ir_rtk_tx;
	rcdev->s_tx_duty_cycle = ir_rtk_set_duty_cycle;
	rcdev->s_tx_carrier = ir_rtk_tx_carrier_send;
	rcdev->driver_name = KBUILD_MODNAME;
	rcdev->s_tx_duty_cycle = ir_rtk_set_tx_duty_cycle;
	rcdev->device_name = "ir_rtk";
	rcdev->map_name = "";
	rcdev->tx_resolution = ir_rtk->ir_param.ir_freq;
	rcdev->rx_resolution = ir_rtk->ir_param.ir_freq;
	rcdev->allowed_protocols = RC_PROTO_BIT_NEC | RC_PROTO_BIT_NECX |
							   RC_PROTO_BIT_NEC32;
	rcdev->enabled_protocols = RC_PROTO_BIT_NEC | RC_PROTO_BIT_NECX |
							   RC_PROTO_BIT_NEC32;

	ret = ir_raw_event_prepare(rcdev);
	if (ret < 0) {
		dev_err(ir_rtk->dev, "Failed to prepare raw event\n");
	}

	rcdev->raw->dev->enabled_protocols = RC_PROTO_BIT_NEC | RC_PROTO_BIT_NECX | RC_PROTO_BIT_NEC32;

	rtk_ir_init_hw(ir_rtk);
	ir_rtk->rcdev = rcdev;

	return devm_rc_register_device(&pdev->dev, ir_rtk->rcdev);
}

static int ir_rtk_remove(struct platform_device *pdev)
{
	struct rtk_ir_dev *ir_rtk = platform_get_drvdata(pdev);

	clk_disable_unprepare(ir_rtk->clk);

	return 0;
}

static struct platform_driver rtk_ir_platform_driver = {
	.probe		= ir_rtk_probe,
	.remove		= ir_rtk_remove,
	.driver		= {
		.name	= KBUILD_MODNAME,
		.of_match_table = of_match_ptr(rtk_ir_match),
	},
};
module_platform_driver(rtk_ir_platform_driver);

MODULE_DESCRIPTION("Realtek Ameba IR driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
