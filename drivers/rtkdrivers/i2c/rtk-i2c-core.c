// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek I2C support
*
*       - I2C Master mode:
*             1) rtk_i2c_xfer: to send or receive i2c messages as i2c master.
*       - I2C Slave mode:
*             1) rtk_i2c_reg_slave: to register an i2c slave,
*             2) rtk_i2c_unreg_slave: to unregister an i2c slave.
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include "i2c-realtek.h"

/*Below parameters are used for I2C speed fine-tune*/
u32 IC_SS_SCL_HCNT_TRIM = 0;
u32 IC_SS_SCL_LCNT_TRIM = 0;
u32 IC_FS_SCL_HCNT_TRIM = 0;
u32 IC_FS_SCL_LCNT_TRIM = 0;
u32 IC_HS_SCL_HCNT_TRIM = 0;
u32 IC_HS_SCL_LCNT_TRIM = 0;

void rtk_i2c_writel(void __iomem *ptr, u32 reg, u32 value)
{
	writel(value, (void __iomem *)((u32)ptr + reg));
}

u32 rtk_i2c_readl(void __iomem *ptr, u32 reg)
{
	return readl((void __iomem *)((u32)ptr + reg));
}

void rtk_i2c_reg_update(void __iomem *ptr, u32 reg, u32 mask, u32 value)
{
	u32 temp;

	temp = rtk_i2c_readl(ptr, reg);
	temp |= mask;
	temp &= (~mask | value);
	rtk_i2c_writel(ptr, reg, temp);
}

void rtk_i2c_enable_cmd(struct rtk_i2c_hw_params *i2c_param, u8 new_state)
{
	if (new_state == ENABLE) {
		/* Enable the selected I2C peripheral */
		rtk_i2c_reg_update(i2c_param->i2c_dev->base, IC_ENABLE, I2C_BIT_ENABLE, I2C_BIT_ENABLE);
	} else if (new_state == DISABLE) {
		/* Disable the selected I2C peripheral */
		rtk_i2c_reg_update(i2c_param->i2c_dev->base, IC_ENABLE, I2C_BIT_ENABLE, ~I2C_BIT_ENABLE);
	} else {
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_ENABLE, I2C_BIT_FORCE);
	}
}

static void rtk_get_dts_info(
	struct rtk_i2c_dev *i2c_dev,
	struct device_node *np,
	u32 *param_to_set, int default_value,
	char *dts_name)
{
	int nr_requests, ret;

	/* Get DTS params. */
	ret = of_property_read_u32(np, dts_name, &nr_requests);
	if (ret) {
		dev_warn(i2c_dev->dev, "Can't get DTS property %s, set it to default value %d\n", dts_name, default_value);
		*param_to_set = default_value;
	} else {
		dev_dbg(i2c_dev->dev, "Get DTS property %s = %d\n", dts_name, nr_requests);
		*param_to_set = nr_requests;
	}
}

static void rtk_i2c_struct_init(
	struct rtk_i2c_dev *i2c_dev,
	struct device_node *np,
	struct rtk_i2c_hw_params *i2c_param)
{
	char s1[] = "rtk,i2c-index";
	char s2[] = "rtk,i2c-clk";
	char s3[] = "rtk,wait-timeout";
	char s4[] = "rtk,use-poll-type";
	char s5[] = "rtk,i2c-reg-slave-num";
	u32 user_set_poll_mode;

	rtk_get_dts_info(i2c_dev, np, &i2c_param->i2c_index, 0, s1);
	rtk_get_dts_info(i2c_dev, np, &i2c_dev->i2c_manage.user_set_timeout, 0x1000, s3);
	rtk_get_dts_info(i2c_dev, np, &user_set_poll_mode, 0, s4);
	if (user_set_poll_mode) {
		i2c_dev->i2c_manage.operation_type = I2C_POLL_TYPE;
	} else {
		i2c_dev->i2c_manage.operation_type = I2C_INTR_TYPE;
	}
	rtk_get_dts_info(i2c_dev, np, &i2c_dev->nr_slaves, 0, s5);
	if (i2c_dev->nr_slaves > 2) {
		dev_warn(i2c_dev->dev, "Realtek I2C slave only support no more than 2 devices\n");
	}

	/* Load HAL initial data structure default value */
	i2c_param->i2c_master = I2C_MASTER_MODE;
	i2c_param->i2c_addr_mode = I2C_ADDR_7BIT;
	i2c_param->i2c_speed_mode = I2C_SS_MODE;
	i2c_param->i2c_ack_addr = 0x11;
	i2c_param->i2c_sda_hold = 2;
	i2c_param->i2c_slv_setup = 0x3;
	i2c_param->i2c_rx_thres = 0x00;
	i2c_param->i2c_tx_thres = 0x10;
	i2c_param->i2c_mst_restart_en = DISABLE;
	i2c_param->i2c_mst_gen_call = DISABLE;
	i2c_param->i2c_mst_startb = DISABLE;
	i2c_param->i2c_slv_nack = DISABLE;
	i2c_param->i2c_filter = 0x101;
	i2c_param->i2c_tx_dma_empty_level = 0x09;
	i2c_param->i2c_rx_dma_full_level = 0x03;
	i2c_param->i2c_ack_addr1 = 0x12;

	if (i2c_dev->i2c_param.i2c_index == 0) {
		i2c_dev->i2c_param.i2c_ip_clk = 20000000;	// 20M ip clock for i2c-0 (smart mp).
	} else {
		i2c_dev->i2c_param.i2c_ip_clk = 100000000;	// 100M ip clock for i2c-1 and i2c-2.
	}
	rtk_get_dts_info(i2c_dev, np, &i2c_param->i2c_clk, 100, s2);

	if (i2c_param->i2c_clk > i2c_dev->i2c_param.i2c_ip_clk) {
		dev_warn(i2c_dev->dev, "Invalid clock for I2C %d\n", i2c_param->i2c_index);
		i2c_param->i2c_clk = i2c_dev->i2c_param.i2c_ip_clk;
	}

	if ((i2c_param->i2c_index == 0) && (i2c_param->i2c_clk >= 400000)) {
		dev_warn(i2c_dev->dev, "This speed is too large for I2C0. May try I2C1 or I2C2 for this slave. Speed will be cut down to 400K\n");
		i2c_param->i2c_clk = 400000;
		if (i2c_param->i2c_clk >= 400000) {
			i2c_param->i2c_speed_mode = I2C_FS_MODE;
		} else {
			i2c_param->i2c_speed_mode = I2C_SS_MODE;
		}
	}

	if ((i2c_param->i2c_index != 0) && (i2c_param->i2c_clk > 4000000)) {
		dev_warn(i2c_dev->dev, "This speed is too large for I2C%d. Speed will be cut down to 4M\n", i2c_param->i2c_index);
		i2c_param->i2c_clk = 4000000;
		if (i2c_param->i2c_clk >= 1700000) {
			dev_warn(i2c_dev->dev, "Only <one-master-and-one-slave> mode is supported by this speed\n");
			dev_warn(i2c_dev->dev, "Please confirm your devices and delete hs_lock in code manually to use HS Mode\n");
			dev_warn(i2c_dev->dev, "Otherwise, the speed will be cut down to 1M\n");
			i2c_param->i2c_clk = 1000000;
			i2c_param->i2c_speed_mode = I2C_FS_MODE;
		} else if (i2c_param->i2c_clk >= 400000) {
			i2c_param->i2c_speed_mode = I2C_FS_MODE;
		} else {
			i2c_param->i2c_speed_mode = I2C_SS_MODE;
		}
	}
}

static void rtk_i2c_set_hw_speed(
	struct rtk_i2c_hw_params *i2c_param,
	u32 speed_mode, u32 i2c_clk, u32 i2c_ip_clk)
{
	u32 ich_cnt;
	u32 ichl_cnt;
	u32 ichh_cnt;

	ich_cnt = i2c_ip_clk / i2c_clk;
	switch (speed_mode) {
	case I2C_SS_MODE: {
		ichl_cnt = (ich_cnt * I2C_SS_MIN_SCL_LTIME) / (I2C_SS_MIN_SCL_HTIME + I2C_SS_MIN_SCL_LTIME);
		ichh_cnt = (ich_cnt * I2C_SS_MIN_SCL_HTIME) / (I2C_SS_MIN_SCL_HTIME + I2C_SS_MIN_SCL_LTIME);

		if (ichl_cnt > IC_SS_SCL_HCNT_TRIM) {
			ichl_cnt -= IC_SS_SCL_HCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_SS_SCL_LCNT, ichl_cnt + 1);

		if (ichh_cnt > IC_SS_SCL_LCNT_TRIM) {
			ichh_cnt -= IC_SS_SCL_LCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_SS_SCL_HCNT, ichh_cnt);

		break;
	}

	case I2C_FS_MODE: {
		ichl_cnt = (ich_cnt * I2C_FS_MIN_SCL_LTIME) / (I2C_FS_MIN_SCL_HTIME + I2C_FS_MIN_SCL_LTIME);
		ichh_cnt = (ich_cnt * I2C_FS_MIN_SCL_HTIME) / (I2C_FS_MIN_SCL_HTIME + I2C_FS_MIN_SCL_LTIME);

		if (ichl_cnt > IC_FS_SCL_HCNT_TRIM) {
			ichl_cnt -= IC_FS_SCL_HCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_FS_SCL_LCNT, ichl_cnt + 1);

		if (ichh_cnt > IC_FS_SCL_LCNT_TRIM) {
			ichh_cnt -= IC_FS_SCL_LCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_FS_SCL_HCNT, ichh_cnt);

		break;
	}

	case I2C_HS_MODE: {
		/* Set fast mode count for master code */
		ichl_cnt = (ich_cnt * I2C_FS_MIN_SCL_LTIME) / (I2C_FS_MIN_SCL_HTIME + I2C_FS_MIN_SCL_LTIME);
		ichh_cnt = (ich_cnt * I2C_FS_MIN_SCL_HTIME) / (I2C_FS_MIN_SCL_HTIME + I2C_FS_MIN_SCL_LTIME);

		if (ichl_cnt > IC_FS_SCL_HCNT_TRIM) {
			ichl_cnt -= IC_FS_SCL_HCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_FS_SCL_LCNT, ichl_cnt + 1);

		if (ichh_cnt > IC_FS_SCL_LCNT_TRIM) {
			ichh_cnt -= IC_FS_SCL_LCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_FS_SCL_HCNT, ichh_cnt);

		ichl_cnt = (ich_cnt * I2C_HS_MIN_SCL_LTIME_100) / (I2C_HS_MIN_SCL_HTIME_100 + I2C_HS_MIN_SCL_LTIME_100);
		ichh_cnt = (ich_cnt * I2C_HS_MIN_SCL_HTIME_100) / (I2C_HS_MIN_SCL_HTIME_100 + I2C_HS_MIN_SCL_LTIME_100);

		if (ichl_cnt > IC_HS_SCL_HCNT_TRIM) {
			ichl_cnt -= IC_HS_SCL_HCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_HS_SCL_LCNT, ichl_cnt + 1);

		if (ichh_cnt > IC_HS_SCL_LCNT_TRIM) {
			ichh_cnt -= IC_HS_SCL_LCNT_TRIM;
		}
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_HS_SCL_HCNT, ichh_cnt);

		break;
	}

	default:
		break;
	}

	dev_dbg(i2c_param->i2c_dev->dev, "Set current mode = %d\n", speed_mode);
	dev_dbg(i2c_param->i2c_dev->dev, "Set high level count = %d\n", ichh_cnt);
	dev_dbg(i2c_param->i2c_dev->dev, "Set low level count = %d\n", ichl_cnt + 1);
}

u8 rtk_i2c_check_flag_state(struct rtk_i2c_hw_params *i2c_param, u32 i2c_flag)
{
	u8 bit_status = 0;

	/* Poll I2C flag state! */
	if ((rtk_i2c_readl(i2c_param->i2c_dev->base, IC_STATUS) & i2c_flag) != 0) {
		/* I2C_FLAG is set */
		bit_status = 1;
	}

	/* Return the I2C_FLAG status */
	return  bit_status;
}

void rtk_i2c_interrupt_config(
	struct rtk_i2c_hw_params *i2c_param,
	u32 i2c_interrupt, u32 new_state)
{
	u32 temp_val;

	temp_val = rtk_i2c_readl(i2c_param->i2c_dev->base, IC_INTR_MASK);
	if (new_state == ENABLE) {
		temp_val |= i2c_interrupt;
	} else {
		temp_val &= ~i2c_interrupt;
	}

	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_INTR_MASK, temp_val);
}

void rtk_i2c_clear_interrupt(
	struct rtk_i2c_hw_params *i2c_param, u32 interrupt_bit)
{
	switch (interrupt_bit) {
	case I2C_BIT_R_LP_WAKE_2:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_ADDR_MATCH);
		break;
	case I2C_BIT_R_LP_WAKE_1:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_ADDR_MATCH);
		break;
	case I2C_BIT_R_GEN_CALL:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_GEN_CALL);
		break;
	case I2C_BIT_R_START_DET:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_START_DET);
		break;
	case I2C_BIT_R_STOP_DET:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_STOP_DET);
		break;
	case I2C_BIT_R_ACTIVITY:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_ACTIVITY);
		break;
	case I2C_BIT_R_RX_DONE:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_RX_DONE);
		break;
	case I2C_BIT_R_TX_ABRT:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_TX_ABRT);
		break;
	case I2C_BIT_R_RD_REQ:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_RD_REQ);
		break;
	case I2C_BIT_R_TX_OVER:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_TX_OVER);
		break;
	case I2C_BIT_R_RX_OVER:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_RX_OVER);
		break;
	case I2C_BIT_R_RX_UNDER:
		rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_RX_UNDER);
		break;
	case I2C_BIT_R_TX_EMPTY:
	case I2C_BIT_R_RX_FULL:
	default:
		break;
	}
}

void rtk_i2c_clear_all_interrupts(struct rtk_i2c_hw_params *i2c_param)
{
	rtk_i2c_readl(i2c_param->i2c_dev->base, IC_CLR_INTR);
}

u32 rtk_i2c_get_raw_interrupt(struct rtk_i2c_hw_params *i2c_param)
{
	return rtk_i2c_readl(i2c_param->i2c_dev->base, IC_RAW_INTR_STAT);
}

u32 rtk_i2c_get_interrupt(struct rtk_i2c_hw_params *i2c_param)
{
	return rtk_i2c_readl(i2c_param->i2c_dev->base, IC_INTR_STAT);
}

void rtk_i2c_init_hw(struct rtk_i2c_hw_params *i2c_param)
{
	u8 specical;

	/* Check the parameters */
	if (!IS_I2C_ADDR_MODE(i2c_param->i2c_addr_mode) || !IS_I2C_SPEED_MODE(i2c_param->i2c_speed_mode)) {
		dev_err(i2c_param->i2c_dev->dev, "Init HW with illegal parameter\n");
		return;
	}

	/* Disable the I2C first */
	rtk_i2c_enable_cmd(i2c_param, DISABLE);

	/* Master case*/
	if (i2c_param->i2c_master == I2C_MASTER_MODE) {
		/*RESTART MUST be set in these condition in Master mode.
		But it might be NOT compatible in old slaves.*/
		if ((i2c_param->i2c_addr_mode == I2C_ADDR_10BIT)
			|| (i2c_param->i2c_speed_mode == I2C_HS_MODE)
			|| (i2c_param->i2c_mst_startb != 0)) {
			i2c_param->i2c_mst_restart_en = ENABLE;
		}

		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_CON,
					   I2C_BIT_IC_SLAVE_DISABLE_1 |
					   I2C_BIT_IC_SLAVE_DISABLE_0 |
					   (i2c_param->i2c_mst_restart_en << 5) |
					   (i2c_param->i2c_speed_mode << 1) |
					   (i2c_param->i2c_master));

		/* To set target addr.*/
		specical = 0;
		if ((i2c_param->i2c_mst_gen_call != 0) ||
			(i2c_param->i2c_mst_startb != 0)) {
			specical = 1;
		}

		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_TAR,
					   (i2c_param->i2c_addr_mode << 12) |
					   (specical << 11) |
					   (i2c_param->i2c_mst_startb << 10) |
					   (i2c_param->i2c_ack_addr & I2C_MASK_IC_TAR));

		/* To Set I2C clock*/
		rtk_i2c_set_hw_speed(i2c_param,
							 i2c_param->i2c_speed_mode,
							 i2c_param->i2c_clk,
							 i2c_param->i2c_ip_clk);
	} else {   // I2C_SLAVE_MODE
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_CON,
					   (i2c_param->i2c_master << 7) |
					   (i2c_param->i2c_master << 6) |
					   (i2c_param->i2c_addr_mode << 3) |
					   (i2c_param->i2c_speed_mode << 1) |
					   (i2c_param->i2c_master));

		/* To set slave0 addr. */
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_SAR, i2c_param->i2c_ack_addr & I2C_MASK_IC_SAR);
		/* To set slave1 addr. */
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_SAR2, i2c_param->i2c_ack_addr1 & I2C_MASK_IC_SAR2);
		/* To set slave no ack */
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_SLV_DATA_NACK_ONLY, i2c_param->i2c_slv_nack);
		/* Set ack general call. */
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_ACK_GENERAL_CALL, i2c_param->i2c_slv_ack_gen_call & I2C_BIT_ACK_GEN_CALL);
		/* to set SDA setup time */
		rtk_i2c_writel(i2c_param->i2c_dev->base, IC_SDA_SETUP, i2c_param->i2c_slv_setup & I2C_MASK_IC_SDA_SETUP);
	}
	/* To set SDA hold time */
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_SDA_HOLD, i2c_param->i2c_sda_hold & I2C_MASK_IC_SDA_HOLD);

	/* To set TX_Empty Level */
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_TX_TL, i2c_param->i2c_tx_thres);

	/* To set RX_Full Level */
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_RX_TL, i2c_param->i2c_rx_thres);

	/* To set IC_FILTER */
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_FILTER, i2c_param->i2c_filter);

	/* To set TX/RX FIFO level */
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DMA_TDLR, i2c_param->i2c_tx_dma_empty_level);
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DMA_RDLR, i2c_param->i2c_rx_dma_full_level);

	/*I2C Clear all interrupts first*/
	rtk_i2c_clear_all_interrupts(i2c_param);

	/*I2C Disable all interrupts first*/
	rtk_i2c_interrupt_config(i2c_param, 0xFFFFFFFF, DISABLE);
}

u8 rtk_i2c_receive_data(
	struct rtk_i2c_hw_params *i2c_param)
{
	u8 data_recved;

	data_recved = rtk_i2c_readl(i2c_param->i2c_dev->base, IC_DATA_CMD);

	/* Return the data in the DR register */
	return data_recved;
}

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
static void rtk_i2c_isr_handle_tx_abort(
	struct rtk_i2c_dev *i2c_dev
)
{
	u32 i2c_local_temp    = 0;
	u8 i2c_stop;

	/* Clear I2C Interrupt */
	i2c_local_temp = i2c_dev->i2c_manage.dev_status;

	/* Update I2C device status */
	i2c_dev->i2c_manage.dev_status  = I2C_STS_ERROR;

	if (0) {
		/* Invoke I2C error callback */
	}

	if (i2c_dev->i2c_manage.i2c_extend & I2C_EXD_MTR_ADDR_RTY) {
		if (i2c_dev->i2c_param.i2c_master == I2C_MASTER_MODE) {
			if ((i2c_local_temp == I2C_STS_RX_READY) || (i2c_local_temp == I2C_STS_RX_ING)) {
				/* Update I2C device status */
				i2c_dev->i2c_manage.dev_status = I2C_STS_RX_ING;

				if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFNF)) {
					if (i2c_dev->i2c_manage.master_rd_cmd_cnt > 0) {
						i2c_stop = I2C_STOP_DIS;
						if ((i2c_dev->i2c_manage.master_rd_cmd_cnt == 1) && ((i2c_dev->i2c_manage.i2c_extend & I2C_EXD_MTR_HOLD_BUS) == 0)) {
							i2c_stop = I2C_STOP_EN;
						}
						i2c_dev->i2c_manage.master_rd_cmd_cnt--;
						rtk_i2c_master_send(&i2c_dev->i2c_param, i2c_dev->i2c_manage.rx_info.p_data_buf, I2C_READ_CMD, i2c_stop, 0);
					}
				}
			} else if ((i2c_local_temp == I2C_STS_TX_READY) || (i2c_local_temp == I2C_STS_TX_ING)) {
				/* Update I2C device status */
				i2c_dev->i2c_manage.dev_status  = I2C_STS_TX_ING;

				if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFNF)) {
					i2c_stop = I2C_STOP_DIS;
					if ((i2c_dev->i2c_manage.tx_info.data_len == 1) && ((i2c_dev->i2c_manage.i2c_extend & I2C_EXD_MTR_HOLD_BUS) == 0)) {
						i2c_stop = I2C_STOP_EN;
					}

					rtk_i2c_master_send(&i2c_dev->i2c_param, i2c_dev->i2c_manage.tx_info.p_data_buf, I2C_WRITE_CMD, i2c_stop, 0);

					i2c_dev->i2c_manage.tx_info.p_data_buf++;
					i2c_dev->i2c_manage.tx_info.data_len--;
				}
			}
		}
	}
}
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

hal_status rtk_i2c_flow_init(struct rtk_i2c_dev *i2c_dev)
{
	dev_dbg(i2c_dev->dev, "I2C mode = %s\n", i2c_dev->i2c_param.i2c_master ? "master" : "slave");

	/* I2C Device Status Update */
	i2c_dev->i2c_manage.dev_status = I2C_STS_INITIALIZED;

	/* I2C Enable Module */
	rtk_i2c_enable_cmd(&i2c_dev->i2c_param, ENABLE);

	return HAL_OK;
}

hal_status rtk_i2c_flow_deinit(struct rtk_i2c_dev *i2c_dev)
{
	/* I2C HAL DeInitialization */
	rtk_i2c_enable_cmd(&i2c_dev->i2c_param, DISABLE);

	/* I2C Device Status Update */
	i2c_dev->i2c_manage.dev_status = I2C_STS_UNINITIAL;

	return HAL_OK;
}

static void rtk_i2c_isr_handle_tx_empty(struct rtk_i2c_dev *i2c_dev)
{
	if (i2c_dev->i2c_param.i2c_master == I2C_MASTER_MODE) {
		rtk_i2c_isr_master_handle_tx_empty(i2c_dev);
	}
}

static void rtk_i2c_isr_handle_tx_over(struct rtk_i2c_dev *i2c_dev)
{
	/* Update I2C device status */
	i2c_dev->i2c_manage.dev_status = I2C_STS_ERROR;
}

static void rtk_i2c_isr_handle_rx_full(struct rtk_i2c_dev *i2c_dev)
{
	if (i2c_dev->i2c_param.i2c_master == I2C_MASTER_MODE) {
		rtk_i2c_isr_master_handle_rx_full(i2c_dev);
	} else {
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		rtk_i2c_isr_slave_handle_rx_full(i2c_dev);
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
	}
}

static irqreturn_t rtk_i2c_isr_event(int irq, void *data)
{
	struct rtk_i2c_dev *i2c_dev = data;
	u32 intr_status = 0;

	/* Time-Out check */
	if (i2c_dev->i2c_param.i2c_master == I2C_MASTER_MODE) {
		if (rtk_i2c_master_irq_wait_timeout(i2c_dev)) {
			return IRQ_HANDLED;
		}
	}

	intr_status = rtk_i2c_get_interrupt(&i2c_dev->i2c_param);
	/* I2C ADDR MATCH Intr*/
	if (intr_status & I2C_BIT_R_LP_WAKE_2) {
		/* Clear I2C interrupt */
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		if (i2c_dev->i2c_param.i2c_master == I2C_SLAVE_MODE) {
			rtk_i2c_isr_slave_handle_sar2_wake(i2c_dev);
		}
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_LP_WAKE_2);
	}
	/* I2C ADDR MATCH Intr*/
	if (intr_status & I2C_BIT_R_LP_WAKE_1) {
		/* Clear I2C interrupt */
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		if (i2c_dev->i2c_param.i2c_master == I2C_SLAVE_MODE) {
			rtk_i2c_isr_slave_handle_sar1_wake(i2c_dev);
		}
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_LP_WAKE_1);
	}
	/* I2C General Call Intr*/
	if (intr_status & I2C_BIT_R_GEN_CALL) {
		/* Clear I2C interrupt */
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		if (i2c_dev->i2c_param.i2c_master == I2C_SLAVE_MODE) {
			rtk_i2c_isr_slave_handle_generall_call(i2c_dev);
		}
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_GEN_CALL);
	}

	/* I2C START DET Intr */
	if (intr_status & I2C_BIT_R_START_DET) {
		/* Clear I2C interrupt */
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_START_DET);
	}

	/* I2C STOP DET Intr */
	if (intr_status & I2C_BIT_R_STOP_DET) {
		/* Clear I2C interrupt */
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_STOP_DET);
	}

	/* I2C Activity Intr */
	if (intr_status & I2C_BIT_R_ACTIVITY) {
		/* Clear I2C interrupt */
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_ACTIVITY);
	}

	/* I2C RX Done Intr */
	if (intr_status & I2C_BIT_R_RX_DONE) {
		//Slave-transmitter and master not ACK it, This occurs on the last byte of
		//the transmission, indicating that the transmission is done.
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		if (i2c_dev->i2c_param.i2c_master == I2C_SLAVE_MODE) {
			rkt_i2c_isr_slave_handle_res_done(i2c_dev);
		}
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_RX_DONE);
	}

	/* I2C STOP DET Intr */
	if (intr_status & I2C_BIT_M_STOP_DET) {
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		if (i2c_dev->i2c_param.i2c_master == I2C_SLAVE_MODE) {
			rkt_i2c_isr_slave_handle_res_done(i2c_dev);
		}
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_M_STOP_DET);
	}

	/* I2C TX Abort Intr */
	if (intr_status & I2C_BIT_R_TX_ABRT) {
		/* If not recv enough bytes, trigger I2C recv agian automatically by driver.*/
		if ((rtk_i2c_readl(i2c_dev->base, IC_TX_ABRT_SOURCE)) & I2C_BIT_ABRT_7B_ADDR_NOACK) {
			/* Read mode: trigger hw retry. */
			if (((i2c_dev->i2c_manage.dev_status == I2C_STS_RX_ING)
				 || (i2c_dev->i2c_manage.dev_status == I2C_STS_RX_READY))
				&& (i2c_dev->i2c_manage.rx_info.data_len)) {
				rtk_i2c_receive_master(i2c_dev);
			}
		}

		if ((rtk_i2c_readl(i2c_dev->base, IC_TX_ABRT_SOURCE)) & I2C_BIT_ARBT_LOST) {
			/*Acquire the data number in TX FIFO*/
			i2c_dev->i2c_manage.tx_info.p_data_buf -= rtk_i2c_readl(i2c_dev->base, IC_TXFLR);
			i2c_dev->i2c_manage.tx_info.data_len += rtk_i2c_readl(i2c_dev->base, IC_TXFLR);

			/* Deinit I2C first */
			rtk_i2c_enable_cmd(&i2c_dev->i2c_param, DISABLE);
			rtk_i2c_enable_cmd(&i2c_dev->i2c_param, ENABLE);
		}
		if ((rtk_i2c_readl(i2c_dev->base, IC_TX_ABRT_SOURCE) & 0xF)) {
			/* Return to the former transfer status */
			i2c_dev->i2c_manage.tx_info.p_data_buf--;
			i2c_dev->i2c_manage.tx_info.data_len++;
		}
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_TX_ABRT);
	}

	/* I2C RD REQ Intr. It is only for slave. */
	if (intr_status & I2C_BIT_R_RD_REQ) {
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		if (i2c_dev->i2c_param.i2c_master == I2C_SLAVE_MODE) {
			rtk_i2c_isr_slave_handle_rd_req(i2c_dev);
		}
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
	}

	/* I2C TX Empty Intr */
	if (intr_status & I2C_BIT_R_TX_EMPTY) {
		rtk_i2c_isr_handle_tx_empty(i2c_dev);
	}

	/* I2C TX Over Run Intr */
	if (intr_status & I2C_BIT_R_TX_OVER) {
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_TX_OVER);
		rtk_i2c_isr_handle_tx_over(i2c_dev);
	}

	/* I2C RX Full Intr */
	if (intr_status & I2C_BIT_R_RX_FULL ||
		intr_status & I2C_BIT_R_RX_OVER) {
		/*I2C RX Over Run Intr*/
		if (intr_status & I2C_BIT_R_RX_OVER) {
			rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_RX_OVER);
		}
		rtk_i2c_isr_handle_rx_full(i2c_dev);
	}

	/*I2C RX Under Run Intr*/
	if (intr_status & I2C_BIT_R_RX_UNDER) {
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_RX_UNDER);
	}

	dev_dbg(i2c_dev->dev, "REG IC_INTR_MASK = 0x%08X\n", rtk_i2c_readl(i2c_dev->base, IC_INTR_MASK));

	return IRQ_HANDLED;
}

static int rtk_i2c_probe(struct platform_device *pdev)
{
	struct rtk_i2c_dev		*i2c_dev;
	struct resource			*res;
	struct device_node		*np = pdev->dev.of_node;
	struct                  i2c_adapter *adap;
	int ret;

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(struct rtk_i2c_dev), GFP_KERNEL);
	if (!i2c_dev) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i2c_dev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c_dev->base)) {
		return PTR_ERR(i2c_dev->base);
	}

	i2c_dev->clock = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(i2c_dev->clock)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		return PTR_ERR(i2c_dev->clock);
	}

	ret = clk_prepare_enable(i2c_dev->clock);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable clock\n");
		return ret;
	}

	platform_set_drvdata(pdev, i2c_dev);
	i2c_dev->dev = &pdev->dev;
	i2c_dev->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, i2c_dev->irq, rtk_i2c_isr_event, 0, dev_name(&pdev->dev), i2c_dev);

	rtk_i2c_struct_init(i2c_dev, np, &i2c_dev->i2c_param);
	adap = &i2c_dev->adap;
	i2c_set_adapdata(adap, i2c_dev);
	snprintf(adap->name, sizeof(adap->name), "RTK I2C(%pa)", &res->start);
	adap->owner = THIS_MODULE;
	i2c_dev->i2c_manage.timeout = i2c_dev->i2c_manage.user_set_timeout;
	adap->timeout = i2c_dev->i2c_manage.user_set_timeout;
	adap->retries = 3;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;
	adap->nr = i2c_dev->i2c_param.i2c_index;
	i2c_dev->i2c_param.i2c_dev = i2c_dev;
	i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;

	if (i2c_dev->nr_slaves) {
#if IS_ENABLED(CONFIG_I2C_SLAVE)
		ret = rtk_i2c_slave_probe(pdev, i2c_dev, adap);
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
	} else {
		ret = rtk_i2c_master_probe(pdev, i2c_dev, adap);
	}
	if (ret < 0) {
		goto clk_free;
	}

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	pm_runtime_set_autosuspend_delay(i2c_dev->dev, 2000); /* ms to delay then idle. */
	pm_runtime_use_autosuspend(i2c_dev->dev);
	pm_runtime_set_active(i2c_dev->dev);
	pm_runtime_enable(i2c_dev->dev);
	pm_runtime_get_noresume(&pdev->dev);
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

	ret = i2c_add_numbered_adapter(adap);
	if (ret) {
		goto pm_disable;
	}

	dev_info(i2c_dev->dev, "I2C %d adapter registered\n", adap->nr);

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	return 0;

pm_disable:
#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	pm_runtime_put_noidle(i2c_dev->dev);
	pm_runtime_disable(i2c_dev->dev);
	pm_runtime_set_suspended(i2c_dev->dev);
	pm_runtime_dont_use_autosuspend(i2c_dev->dev);
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

clk_free:
	clk_disable_unprepare(i2c_dev->clock);
	return ret;
}

static int rtk_i2c_remove(struct platform_device *pdev)
{
	struct rtk_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c_dev->adap);
#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	pm_runtime_get_sync(i2c_dev->dev);
	pm_runtime_put_noidle(i2c_dev->dev);
	pm_runtime_disable(i2c_dev->dev);
	pm_runtime_set_suspended(i2c_dev->dev);
	pm_runtime_dont_use_autosuspend(i2c_dev->dev);
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

	clk_disable_unprepare(i2c_dev->clock);
	return 0;
}

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
static int rtk_i2c_runtime_suspend(struct device *dev)
{
	struct rtk_i2c_dev *i2c_dev = dev_get_drvdata(dev);
	return 0;
}

static int rtk_i2c_runtime_resume(struct device *dev)
{
	struct rtk_i2c_dev *i2c_dev = dev_get_drvdata(dev);
	return 0;
}

static int rtk_i2c_runtime_idle(struct device *dev)
{
	struct rtk_i2c_dev *i2c_dev = dev_get_drvdata(dev);

	/* If i2c slave registered, not idle for ever. */
	return 0;
}
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

static int rtk_i2c_suspend(struct device *dev)
{
	struct rtk_i2c_dev *i2c_dev = dev_get_drvdata(dev);

	i2c_mark_adapter_suspended(&i2c_dev->adap);
	return 0;
}

static int rtk_i2c_resume(struct device *dev)
{
	struct rtk_i2c_dev *i2c_dev = dev_get_drvdata(dev);

	/* PG MODE: */
	/* For kernel space i2c-slave client, keep system active if waiting for instructions. */
	/* When system sleep, unreg the i2c slave. After system resume, reg the slave again. */

	/* CG MODE: slave need do nothing here. */

	i2c_mark_adapter_resumed(&i2c_dev->adap);
	return 0;
}

static const struct dev_pm_ops rtk_i2c_pm_ops = {
	/* system syspend/resume call: */
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(rtk_i2c_suspend, rtk_i2c_resume)
#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	/* i2c runtime idle suspend/resume call: */
	SET_RUNTIME_PM_OPS(rtk_i2c_runtime_suspend, rtk_i2c_runtime_resume, rtk_i2c_runtime_idle)
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
};

static const struct of_device_id rtk_i2c_match[] = {
	{ .compatible = "realtek,ameba-i2c"},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_i2c_match);

static struct platform_driver rtk_i2c_driver = {
	.driver = {
		.name = "realtek-ameba-i2c",
		.of_match_table = rtk_i2c_match,
		.pm = &rtk_i2c_pm_ops,
	},
	.probe = rtk_i2c_probe,
	.remove = rtk_i2c_remove,
};

module_platform_driver(rtk_i2c_driver);

MODULE_DESCRIPTION("Realtek Ameba I2C driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
