// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek I2C master support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include "i2c-realtek.h"
#include <linux/ameba/rtk-timer.h>

#define I2C_MSG_IS_READ(msg)   ((msg)->flags & I2C_M_RD)
#define I2C_MSG_IS_WRITE(msg)  (!((msg)->flags & I2C_M_RD))

static hal_status rtk_i2c_receive_int_master(struct rtk_i2c_dev *i2c_dev);
static hal_status rtk_i2c_receive_poll_master(struct rtk_i2c_dev *i2c_dev);
static hal_status rtk_i2c_send_poll_master(struct rtk_i2c_dev *i2c_dev);

static int rtk_i2c_can_send_tx_cmd(struct rtk_i2c_hw_params *i2c_param, u8 block)
{
	int retry = 0;

	while (retry < SOWFTWARE_MAX_RETRYTIMES) {
		if (!rtk_i2c_check_flag_state(i2c_param, I2C_BIT_MST_ACTIVITY) &&
			  rtk_i2c_check_flag_state(i2c_param, BIT_TFNF)) {
			return 1;
		} else if (!block) {
			return 0;
		}

		if (rtk_i2c_get_raw_interrupt(i2c_param) & I2C_BIT_TX_ABRT) {
			dev_err(i2c_param->i2c_dev->dev,
			       "TX abort detected (retry=%d)\n", retry);
			rtk_i2c_clear_all_interrupts(i2c_param);
			return -1;
		}
		retry++;
	}

	dev_err(i2c_param->i2c_dev->dev, "TX FIFO full timeout (retry=%d)\n", retry);
	return -1;
}

static int rtk_i2c_can_read_rx_data(struct rtk_i2c_hw_params *i2c_param, u8 block)
{
	int retry = 0;

	while (retry < SOWFTWARE_MAX_RETRYTIMES) {
		if (rtk_i2c_check_flag_state(i2c_param, BIT_RFNE)) {
			return 1;
		} else if (!block) {
			return 0;
		}

		u32 raw_int = rtk_i2c_get_raw_interrupt(i2c_param);
		if (raw_int & (I2C_BIT_TX_ABRT | I2C_BIT_RX_OVER | I2C_BIT_RX_UNDER)) {
			dev_err(i2c_param->i2c_dev->dev,
			       "RX error detected: RAW_INT=0x%08X (retry=%d)\n",
			       raw_int, retry);
			rtk_i2c_clear_all_interrupts(i2c_param);
			return -1;
		}

		retry++;
	}

	dev_err(i2c_param->i2c_dev->dev, "RX FIFO empty timeout (retry=%d)\n", retry);
	return -1;
}

static void rtk_i2c_master_send(
	struct rtk_i2c_hw_params *i2c_param,
	u8 *pbuf, u8  i2c_cmd,
	u8 i2c_stop, u8  i2c_restart)
{
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD,
				   * (pbuf) | (i2c_restart << 10) |
				   (i2c_cmd << 8) | (i2c_stop << 9));
}

static void rtk_i2c_set_slave_addr(
	struct rtk_i2c_hw_params *i2c_param, u16 address)
{
	u32 tar;

	tar = rtk_i2c_readl(i2c_param->i2c_dev->base, IC_TAR) & ~(I2C_MASK_IC_TAR);

	/*Set target address to generate start signal*/
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_TAR, (address & I2C_MASK_IC_TAR) | tar);
}

static hal_status rtk_i2c_is_timeout(struct rtk_i2c_dev *i2c_dev)
{
	hal_status status;

	if (i2c_dev->i2c_manage.timeout == 0) {
		status = HAL_TIMEOUT;
	} else {
		status = HAL_OK;
	}
	return status;
}

bool rtk_i2c_master_irq_wait_timeout(struct rtk_i2c_dev *i2c_dev)
{
	if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
		dev_err(i2c_dev->dev, "I2C %d wait IRQ timeout\n", i2c_dev->i2c_param.i2c_index);
		rtk_i2c_interrupt_config(&i2c_dev->i2c_param, 0x1FFF, DISABLE);
		rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);
		rtk_i2c_flow_deinit(i2c_dev);
		rtk_i2c_reg_update(i2c_dev->base, IC_DATA_CMD, I2C_BIT_CMD_STOP, I2C_BIT_CMD_STOP);
		rtk_i2c_init_hw(&i2c_dev->i2c_param);
		i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
		return 1;
	} else {
		return 0;
	}
}

static void rtk_i2c_hw_timer_timeout(struct rtk_i2c_dev *i2c_dev)
{
	dev_err(i2c_dev->dev, "%s HW timer timeout\n", (i2c_dev->i2c_param.i2c_master == I2C_SLAVE_MODE) ? "Slave" : "Master");
	i2c_dev->i2c_manage.timeout = 0;
	rtk_gtimer_int_clear(i2c_dev->i2c_manage.timer_index);
}

static void rtk_i2c_dev_status_handler(struct rtk_i2c_dev *i2c_dev,
									   hal_status dev_status)
{
	switch (dev_status) {
	case HAL_TIMEOUT:
		dev_err(i2c_dev->dev, "System waits timeout\n");
		return;
	case HAL_ERR_HW:
		dev_err(i2c_dev->dev, "System occurs hw error\n");
		return;
	case HAL_ERR_PARA:
		dev_err(i2c_dev->dev, "System occurs parameter error\n");
		return;
	case HAL_ERR_MEM:
		dev_err(i2c_dev->dev, "System occurs memory error\n");
		return;
	case HAL_ERR_UNKNOWN:
		dev_err(i2c_dev->dev, "System occurs unknown error\n");
		return;
	default:
		dev_dbg(i2c_dev->dev, "This status %d is not expected to handle\n", dev_status);
		return;
	}
}

static int rtk_i2c_release_bus(struct i2c_adapter *i2c_adap)
{
	struct rtk_i2c_dev *i2c_dev = i2c_get_adapdata(i2c_adap);

	dev_warn(i2c_dev->dev, "Trying to recover\n");

	rtk_i2c_enable_cmd(&i2c_dev->i2c_param, DISABLE);
	rtk_i2c_init_hw(&i2c_dev->i2c_param);
	i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;

	return 0;
}

static int rtk_i2c_wait_free_bus(struct rtk_i2c_dev *i2c_dev)
{
	int ret;

	if (i2c_dev->i2c_manage.dev_status == I2C_STS_IDLE || i2c_dev->i2c_manage.dev_status == I2C_STS_UNINITIAL) {
		dev_dbg(i2c_dev->dev, "Free to use\n");
		return 0;
	}

	dev_dbg(i2c_dev->dev, "I2C is busy, current status = 0x%08X\n", i2c_dev->i2c_manage.dev_status);

	ret = rtk_i2c_release_bus(&i2c_dev->adap);
	if (ret) {
		dev_err(i2c_dev->dev, "Failed to recover bus\n");
		return ret;
	}

	return -EBUSY;
}

/* Switch to the next message. */
static int rtk_i2c_restart_switch_flow(struct rtk_i2c_dev *i2c_dev)
{
	struct i2c_management_adapter *manage = &i2c_dev->i2c_manage;
	int last_idx = manage->current_msg_id - 1;
	int curr_idx = manage->current_msg_id;
	struct i2c_msg *last_msg, *curr_msg;
	bool last_is_read, curr_is_read;
	bool dir_changed, addr_changed;

	if (last_idx < 0 || curr_idx >= manage->msg_count) {
		dev_err(i2c_dev->dev, "Invalid msg index: last=%d, curr=%d, count=%d\n",
				last_idx, curr_idx, manage->msg_count);
		return -EINVAL;
	}

	last_msg = &manage->msgs[last_idx];
	curr_msg = &manage->msgs[curr_idx];

	last_is_read = I2C_MSG_IS_READ(last_msg);
	curr_is_read = I2C_MSG_IS_READ(curr_msg);

	dir_changed = (last_is_read != curr_is_read);
	addr_changed = (last_msg->addr != curr_msg->addr);

	if (addr_changed) {
		dev_err(i2c_dev->dev, "ERROR: Address change in RESTART is not supported\n");
		return -EINVAL;
	}

	dev_dbg(i2c_dev->dev,
		"Restart: msg[%d] 0x%02x %s%d -> msg[%d] 0x%02x %s%d (dir:%s addr:%s)\n",
		last_idx, last_msg->addr, last_is_read ? "R" : "W", last_msg->len,
		curr_idx, curr_msg->addr, curr_is_read ? "R" : "W", curr_msg->len,
		dir_changed ? "changed" : "same", addr_changed ? "changed" : "same");

	if (addr_changed) {
		dev_err(i2c_dev->dev, "WARNING: address changed.");
	}

	if (I2C_MSG_IS_WRITE(last_msg) && I2C_MSG_IS_WRITE(curr_msg)) {
		dev_dbg(i2c_dev->dev, "Case 1: W→W (same direction)\n");
		manage->tx_info.data_len = curr_msg->len;
		manage->tx_info.p_data_buf = curr_msg->buf;
	} else if (I2C_MSG_IS_READ(last_msg) && I2C_MSG_IS_READ(curr_msg)) {
		dev_dbg(i2c_dev->dev, "Case 2: R→R (same direction)\n");
		manage->rx_info.data_len = curr_msg->len;
		manage->rx_info.p_data_buf = curr_msg->buf;
		if (manage->operation_type == I2C_INTR_TYPE) {
			rtk_i2c_receive_int_master(i2c_dev);
		}
	} else if (I2C_MSG_IS_WRITE(last_msg) && I2C_MSG_IS_READ(curr_msg)) {
		dev_dbg(i2c_dev->dev, "Case 3: W→R (direction changed)\n");
		manage->rx_info.data_len = curr_msg->len;
		manage->rx_info.p_data_buf = curr_msg->buf;

		/* Wait for slave hardware read/write flip. */
		udelay(50);

		if (manage->operation_type == I2C_INTR_TYPE) {
			rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_TX_ABRT | I2C_BIT_M_TX_EMPTY | I2C_BIT_M_TX_OVER), DISABLE);
			rtk_i2c_receive_int_master(i2c_dev);
		}
	} else {  // I2C_MSG_IS_READ(last_msg) && I2C_MSG_IS_WRITE(curr_msg)
		dev_dbg(i2c_dev->dev, "Case 4: R→W (direction changed)\n");
		manage->tx_info.data_len = curr_msg->len;
		manage->tx_info.p_data_buf = curr_msg->buf;
		if (manage->operation_type == I2C_INTR_TYPE) {
			rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_RX_FULL | I2C_BIT_M_RX_OVER | I2C_BIT_M_RX_UNDER), DISABLE);
			rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_TX_ABRT | I2C_BIT_M_TX_EMPTY | I2C_BIT_M_TX_OVER), ENABLE);
		}
	}

	return 0;
}

void rtk_i2c_isr_master_handle_tx_empty(struct rtk_i2c_dev *i2c_dev)
{
	struct i2c_management_adapter *manage = &i2c_dev->i2c_manage;
	int retry = 0;
	u8 i2c_stop = 0, i2c_restart = 0;;

	/* To check I2C master TX data length. If all the data are transmitted,
	mask all the interrupts and invoke the user callback */
	if (!manage->tx_info.data_len) {
		/* Stop Flow */
		retry = 0;
		while (0 == rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFE)) {
			retry++;
			if (retry > SOWFTWARE_MAX_RETRYTIMES) {
				dev_err(i2c_dev->dev, "TX retry overflow\n");
				return;
			}
		}

		/* I2C Disable TX Related Interrupts */
		rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_TX_ABRT | I2C_BIT_M_TX_EMPTY |
								I2C_BIT_M_TX_OVER), DISABLE);

		/* Clear all I2C pending interrupts */
		rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);
		/* Update I2C device status */
		manage->dev_status = I2C_STS_IDLE;
		complete(&i2c_dev->xfer_completion);
		return;
	} else if (manage->tx_info.data_len == 1) {
		/* Update I2C device status */
		manage->dev_status = I2C_STS_TX_ING;

		/* Check I2C TX FIFO status. If it's not full, one byte data will be written into it. */
		if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFNF)) {
			if ((manage->current_msg_id + 1) == (manage->msg_count)) {
				/* Stop Flow */
				i2c_stop = 1;
			} else {
				/* Restart Flow */
				i2c_restart = 1;
			}

			rtk_i2c_master_send(&i2c_dev->i2c_param, manage->tx_info.p_data_buf,
								I2C_WRITE_CMD, i2c_stop, i2c_restart);

			manage->current_msg_id++;
			if (i2c_stop) {
				/* End of message. */
				manage->tx_info.p_data_buf++;
				manage->tx_info.data_len--;
			} else {
				/* Switch to the next message. */
				rtk_i2c_restart_switch_flow(i2c_dev);
			}
		}
	} else {
		/* Update I2C device status */
		manage->dev_status = I2C_STS_TX_ING;

		/* Check I2C TX FIFO status. If it's not full, one byte data will be written into it. */
		if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFNF)) {
			rtk_i2c_master_send(&i2c_dev->i2c_param, manage->tx_info.p_data_buf,
								I2C_WRITE_CMD, 0, 0);

			manage->tx_info.p_data_buf++;
			manage->tx_info.data_len--;
		}
	}

	retry = 0;
	while (!rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFE)) {
		if (rtk_i2c_get_raw_interrupt(&i2c_dev->i2c_param) & I2C_BIT_TX_ABRT) {
			return;
		}
		retry++;
		if (retry > SOWFTWARE_MAX_RETRYTIMES) {
			dev_err(i2c_dev->dev, "TX retry overflow\n");
			return;
		}
	}
}

void rtk_i2c_isr_master_handle_rx_full(struct rtk_i2c_dev *i2c_dev)
{
	struct i2c_management_adapter *manage = &i2c_dev->i2c_manage;
	u8 i2c_stop = 0, i2c_restart = 0;
	int retry = 0;

	/* Check if the receive transfer is not finished. If it is not, check if there
	is data in the RX FIFO and move the data from RX FIFO to user data buffer*/
	if (manage->rx_info.data_len > 0) {

		/* Update I2C device status */
		manage->dev_status = I2C_STS_RX_ING;

		while (1) {
			if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, (BIT_RFNE | BIT_RFF))) {

				*(manage->rx_info.p_data_buf) = rtk_i2c_receive_data(&i2c_dev->i2c_param);

				manage->rx_info.p_data_buf++;
				manage->rx_info.data_len--;

				if (rtk_i2c_readl(i2c_dev->base, IC_RXFLR) == 0) {
					dev_dbg(i2c_dev->dev, "RX signal discontinuity\n");
					break;
				}
			} else if ((rtk_i2c_get_raw_interrupt(&i2c_dev->i2c_param)
						& (I2C_BIT_RX_OVER | I2C_BIT_RX_UNDER)) != 0) {
				break;
			} else {
				if (!rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_RFNE)) {
					break;
				}
			}
			retry++;
			if (retry > SOWFTWARE_MAX_RETRYTIMES) {
				dev_err(i2c_dev->dev, "RX retry overflow\n");
				return;
			}
		}
	}

	/* To check I2C master RX data length. If all the data are received,
	mask all the interrupts and invoke the user callback.
	Otherwise, the master should send another Read Command to slave for
	the next data byte receiving. */
	if (!manage->rx_info.data_len) {
		manage->current_msg_id++; // current msg done, next.
		if (manage->current_msg_id < (manage->msg_count)) {
			/* Restart Flow */
			rtk_i2c_restart_switch_flow(i2c_dev);
		} else {
			/* Stop Flow */
			/* I2C disable RX related interrupts */
			rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_RX_FULL | I2C_BIT_M_RX_OVER |
									I2C_BIT_M_RX_UNDER | I2C_BIT_M_TX_ABRT), DISABLE);
			/* Clear all I2C pending interrupts */
			rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);
			/* Update I2C device status */
			manage->dev_status = I2C_STS_IDLE;

			if ((manage->msg_count > 1) && I2C_MSG_IS_WRITE(&manage->msgs[0])) {
				dev_dbg(i2c_dev->dev, "First message is TX, signaling completion\n");
				complete(&i2c_dev->xfer_completion);
			}
		}
	} else {
		/* If TX FIFO is not full, another read command is written into it. */
		if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFNF)) {
			if (manage->master_rd_cmd_cnt > 0) {
				if ((manage->master_rd_cmd_cnt == 1) && ((manage->i2c_extend & I2C_EXD_MTR_HOLD_BUS) == 0)) {
					if ((manage->current_msg_id + 1) == (manage->msg_count)) {
						/* Stop Flow */
						i2c_stop = 1;
					} else {
						/* Restart Flow */
						i2c_restart = 1;
					}
				}
				manage->master_rd_cmd_cnt--;

				rtk_i2c_master_send(&i2c_dev->i2c_param, manage->rx_info.p_data_buf,
									I2C_READ_CMD, i2c_stop, i2c_restart);
			}
		}
	}
}

int rtk_i2c_master_write(
	struct rtk_i2c_hw_params *i2c_param, u8 *pbuf, int len)
{
	struct i2c_management_adapter *manage = &i2c_param->i2c_dev->i2c_manage;
	int cnt = 0;

	/* Write in the DR register the data to be sent */
	for (cnt = 0; cnt < len; cnt++) {
		if (rtk_i2c_can_send_tx_cmd(i2c_param, 1) < 0) {
			dev_err(i2c_param->i2c_dev->dev, "Write break retry overflow\n");
			return cnt;
		}

		if (cnt >= len - 1) {
			if ((manage->current_msg_id + 1) == manage->msg_count) {
				/* Generate stop signal*/
				rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD, (*pbuf++) | I2C_BIT_CMD_STOP);
			} else {
				/* Generate restart signal*/
				rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD, (*pbuf++) | I2C_BIT_CMD_RESTART);
			}
		} else {
			rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD, (*pbuf++));
		}
	}

	return cnt;
}

int rtk_i2c_master_read(
	struct rtk_i2c_hw_params *i2c_param, u8 *pbuf, int len)
{
	struct i2c_management_adapter *manage = &i2c_param->i2c_dev->i2c_manage;
	int cmd_cnt = 0;
	int read_cnt = 0;

	while (read_cnt < len) {
		while ((cmd_cnt < len) && rtk_i2c_can_send_tx_cmd(i2c_param, 0)) {
			if (cmd_cnt == (len - 1)) {
				if ((manage->current_msg_id + 1) == manage->msg_count) {
					/* Generate stop signal*/
					rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD, I2C_BIT_CMD_RW | I2C_BIT_CMD_STOP);
				} else {
					/* Generate restart signal*/
					rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD, I2C_BIT_CMD_RW | I2C_BIT_CMD_RESTART);
				}
			} else {
				rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD, I2C_BIT_CMD_RW);
			}
			cmd_cnt++;
		}

		while((read_cnt < len) && (rtk_i2c_can_read_rx_data(i2c_param, 0))) {
			*pbuf++ = (u8) rtk_i2c_readl(i2c_param->i2c_dev->base, IC_DATA_CMD);
			read_cnt++;
		}

		if ((cmd_cnt == len) && (read_cnt < len) && (rtk_i2c_can_read_rx_data(i2c_param, 1) < 0)) {
			break;
		}
	}

	return read_cnt;
}

static hal_status rtk_i2c_send_poll_master(
	struct rtk_i2c_dev *i2c_dev)
{
	struct i2c_management_adapter *manage = &i2c_dev->i2c_manage;
	u32 data_send;

	/* Send data till the TX buffer data length is zero */
	manage->dev_status = I2C_STS_TX_ING;

REOPERATION:
	data_send = rtk_i2c_master_write(&i2c_dev->i2c_param, manage->tx_info.p_data_buf,
										 manage->tx_info.data_len);
	if (0 == data_send) {
		goto REOPERATION;
	} else if (data_send < manage->tx_info.data_len) {
		dev_dbg(i2c_dev->dev, "Send %d data of %d abort: 0x%08X\n",
				data_send, manage->tx_info.data_len,
				rtk_i2c_readl(i2c_dev->base, IC_TX_ABRT_SOURCE));
		rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);
		manage->dev_status = I2C_STS_ERROR;
		return HAL_ERR_UNKNOWN;
	} else if (data_send == manage->tx_info.data_len){
		manage->current_msg_id++;
		if (manage->current_msg_id < manage->msg_count) {
			rtk_i2c_restart_switch_flow(i2c_dev);
			if (I2C_MSG_IS_READ(&manage->msgs[manage->current_msg_id])) {
				rtk_i2c_receive_poll_master(i2c_dev);
			} else {
				goto REOPERATION;
			}
		}
	} else {
		dev_err(i2c_dev->dev, "Send bytes cannot be larger than total.\n");
	}

	manage->dev_status = I2C_STS_IDLE;
	if ((manage->current_msg_id + 1) == manage->msg_count) {
		complete(&i2c_dev->xfer_completion);
	}

	return HAL_OK;
}

static hal_status
rtk_i2c_receive_poll_master(
	struct rtk_i2c_dev *i2c_dev)
{
	struct i2c_management_adapter *manage = &i2c_dev->i2c_manage;
	u32 data_recv;
	int retry = 0;

	/* Send data till the TX buffer data length is zero */
	manage->dev_status = I2C_STS_RX_ING;

REOPERATION:
	data_recv = rtk_i2c_master_read(&i2c_dev->i2c_param, manage->rx_info.p_data_buf, manage->rx_info.data_len);

	if (0 == data_recv) {
		retry++;
		if (retry > SOWFTWARE_MAX_RETRYTIMES) {
			dev_err(i2c_dev->dev, "Receive retry overflow\n");
			return HAL_TIMEOUT;
		}
		goto REOPERATION;
	} else if (data_recv < manage->rx_info.data_len) {
		dev_err(i2c_dev->dev, "Send %d data of %d abort\n", data_recv, manage->rx_info.data_len);
		manage->dev_status = I2C_STS_ERROR;
		return HAL_ERR_HW;
	} else if (data_recv == manage->rx_info.data_len) {
		manage->current_msg_id++;
		if (manage->current_msg_id < manage->msg_count) {
			rtk_i2c_restart_switch_flow(i2c_dev);
			if (I2C_MSG_IS_READ(&manage->msgs[manage->current_msg_id])) {
				goto REOPERATION;
			} else {
				rtk_i2c_send_poll_master(i2c_dev);
			}
		}
	} else {
		dev_err(i2c_dev->dev, "Receive bytes cannot be larger than total.\n");
	}

	manage->dev_status = I2C_STS_IDLE;

	if (((manage->current_msg_id + 1) == manage->msg_count) &&
		  ((manage->msg_count > 1) && I2C_MSG_IS_WRITE(&manage->msgs[0]))) {
		complete(&i2c_dev->xfer_completion);
	}

	return HAL_OK;
}

static hal_status rtk_i2c_receive_int_master(
	struct rtk_i2c_dev *i2c_dev)
{
	u8 i2c_stop = 0, i2c_restart = 0;

	/* Calculate user time out parameters */
	if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
		i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
		dev_err(i2c_dev->dev, "Receive timeout: Receive int triggered after timeout\n");
		return HAL_TIMEOUT;
	}

	/* I2C device status update */
	i2c_dev->i2c_manage.dev_status = I2C_STS_RX_READY;
	rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);

	/* To fill the master read command into TX FIFO */
	i2c_dev->i2c_manage.master_rd_cmd_cnt = i2c_dev->i2c_manage.rx_info.data_len;
	i2c_dev->i2c_manage.dev_status = I2C_STS_RX_READY;

	if (i2c_dev->i2c_manage.rx_info.data_len > 0) {
		rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_RX_FULL | I2C_BIT_M_RX_OVER |
								 I2C_BIT_M_RX_UNDER | I2C_BIT_M_TX_ABRT), ENABLE);
	}

	/* In order for the DW_apb_i2c to continue acknowledging reads, a read command should be written for */
	/* every byte that is to be received; otherwise the DW_apb_i2c will stop acknowledging. */

	//Two times read cmd:
	//flow is:
	//step 1: master request first data entry
	//step 2: slave send first data entry
	//step 3: master send seconed read cmd to ack first data and request second data
	//step 4: slave send second data
	//step 5: master rx full interrupt receive fisrt data and ack second data and request third data.
	//loop step 4 and step 5.
	//so last slave data have no ack, this is permitted by the spec.
	if (i2c_dev->i2c_manage.master_rd_cmd_cnt > 0) {
		if ((i2c_dev->i2c_manage.master_rd_cmd_cnt == 1)
			&& ((i2c_dev->i2c_manage.i2c_extend & I2C_EXD_MTR_HOLD_BUS) == 0)) {
			if ((i2c_dev->i2c_manage.current_msg_id + 1) == i2c_dev->i2c_manage.msg_count) {
				i2c_stop = 1;
			} else {
				i2c_restart = 1;
			}
		}

		if (i2c_dev->i2c_manage.master_rd_cmd_cnt > 0) {
			i2c_dev->i2c_manage.master_rd_cmd_cnt--;
		}

		rtk_i2c_master_send(&i2c_dev->i2c_param, i2c_dev->i2c_manage.rx_info.p_data_buf,
							I2C_READ_CMD, i2c_stop, i2c_restart);
	}

	return HAL_OK;
}

static hal_status rtk_i2c_send_int_master(
	struct rtk_i2c_dev *i2c_dev)
{
	/* I2C device status update */
	i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
	rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);

	/* I2C device status update */
	i2c_dev->i2c_manage.dev_status = I2C_STS_TX_ING;

	/* Check I2C TX FIFO status */
	while (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFE) == 0) {
		if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
			i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
			dev_err(i2c_dev->dev, "Send timeout: send int triggered after timeout\n");
			break;
		}
	}

	/* I2C enable TX related interrupts */
	rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_TX_ABRT | I2C_BIT_M_TX_EMPTY |
							 I2C_BIT_M_TX_OVER), ENABLE);

	return HAL_OK;
}

hal_status rtk_i2c_send_master(struct rtk_i2c_dev *i2c_dev)
{
	/* Calculate user time out parameters */
	if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
		i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
		dev_err(i2c_dev->dev, "Send timeout\n");
		return HAL_TIMEOUT;
	}

	/* Master run-time update  target address */
	if (i2c_dev->i2c_manage.i2c_extend & I2C_EXD_MTR_ADDR_UPD) {
		while (1) {
			/* Check master activity status is 0 && TX FIFO status empty */
			if (!rtk_i2c_check_flag_state(&i2c_dev->i2c_param, I2C_BIT_MST_ACTIVITY) &&
				rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFE)) {
				break;
			}
			/* Time-Out check */
			if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
				i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
				dev_err(i2c_dev->dev, "Send timeout\n");
				return HAL_TIMEOUT;
			}
		}

		/* Update master target address */
		rtk_i2c_set_slave_addr(&i2c_dev->i2c_param, i2c_dev->i2c_manage.tx_info.target_addr);
	}

	if (i2c_dev->i2c_manage.operation_type == I2C_POLL_TYPE) {
		rtk_i2c_send_poll_master(i2c_dev);
	}

	if (i2c_dev->i2c_manage.operation_type == I2C_INTR_TYPE) {
		rtk_i2c_send_int_master(i2c_dev);
	}
	return HAL_OK;
}

hal_status rtk_i2c_receive_master(struct rtk_i2c_dev *i2c_dev)
{
	/* Calculate user time out parameters */
	if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
		i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
		dev_err(i2c_dev->dev, "Receive timeout\n");
		return HAL_TIMEOUT;
	}

	/* Master run-time update target address */
	if (i2c_dev->i2c_manage.i2c_extend & I2C_EXD_MTR_ADDR_UPD) {
		while (1) {
			/* Check master activity status && Check TX FIFO status */
			if (!rtk_i2c_check_flag_state(&i2c_dev->i2c_param, I2C_BIT_MST_ACTIVITY) &&
				rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFE)) {
				break;
			}
			rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);

			/* Time-Out check */
			if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
				i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
				dev_err(i2c_dev->dev, "Receive timeout\n");
				return HAL_TIMEOUT;
			}
		}

		/* Update Master Target address */
		rtk_i2c_set_slave_addr(&i2c_dev->i2c_param, i2c_dev->i2c_manage.rx_info.target_addr);
	}

	/*I2C_POLL_OP_TYPE*/
	if (i2c_dev->i2c_manage.operation_type == I2C_POLL_TYPE) {
		rtk_i2c_receive_poll_master(i2c_dev);
	}

	/*I2C_INTR_OP_TYPE*/
	if (i2c_dev->i2c_manage.operation_type == I2C_INTR_TYPE) {
		rtk_i2c_receive_int_master(i2c_dev);
	}
	return HAL_OK;
}

/* Linux Layar. */

static void rtk_i2c_xfer_msg(
	struct rtk_i2c_dev *i2c_dev,
	struct i2c_msg *msg)
{
	/*Local variables*/
	u8 i2c_msg_addr_mode = I2C_ADDR_7BIT;
	hal_status dev_status = HAL_OK;

	i2c_dev->i2c_param.i2c_ack_addr = msg->addr;
	i2c_dev->i2c_manage.i2c_extend |= (I2C_EXD_MTR_ADDR_RTY | I2C_EXD_MTR_ADDR_UPD);

	if (msg->flags & I2C_M_TEN) {
		i2c_msg_addr_mode = I2C_ADDR_10BIT;
	}

	dev_dbg(i2c_dev->dev, "I2C speed mode: 0x%08X\n", i2c_dev->i2c_param.i2c_speed_mode);
	dev_dbg(i2c_dev->dev, "I2C addr mode: 0x%02X\n", i2c_msg_addr_mode);

	/*Assign I2C address mode based on address mode*/
	i2c_dev->i2c_param.i2c_addr_mode = i2c_msg_addr_mode;

	/* To deInitialize I2C verification master and slave */
	dev_status = rtk_i2c_flow_deinit(i2c_dev);
	if (dev_status) {
		goto ERROR_CHECK;
	}

	if (msg->flags & I2C_M_RD) {
		/* I2C read */
		i2c_dev->i2c_manage.rx_info.target_addr = msg->addr;
		i2c_dev->i2c_manage.rx_info.p_data_buf = msg->buf;
		i2c_dev->i2c_manage.rx_info.data_len = msg->len;
		dev_status = rtk_i2c_flow_init(i2c_dev);
		if (dev_status) {
			goto ERROR_CHECK;
		}
		dev_status = rtk_i2c_receive_master(i2c_dev);
		if (dev_status) {
			goto ERROR_CHECK;
		}
		while ((i2c_dev->i2c_manage.dev_status != I2C_STS_IDLE) &&
			   (i2c_dev->i2c_manage.dev_status != I2C_STS_ERROR) &&
			   (i2c_dev->i2c_manage.dev_status != I2C_STS_TIMEOUT)) {
			/* Time-Out check */
			if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
				i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
				dev_err(i2c_dev->dev, "Send timeout: cannot finish read\n");
				break;
			}
		}
	} else {
		/* I2C write */
		reinit_completion(&i2c_dev->xfer_completion);
		i2c_dev->i2c_manage.tx_info.target_addr = msg->addr;
		i2c_dev->i2c_manage.tx_info.p_data_buf = msg->buf;
		i2c_dev->i2c_manage.tx_info.data_len = msg->len;
		dev_status = rtk_i2c_flow_init(i2c_dev);
		if (dev_status) {
			goto ERROR_CHECK;
		}
		dev_status = rtk_i2c_send_master(i2c_dev);
		if (dev_status) {
			goto ERROR_CHECK;
		}
		while ((i2c_dev->i2c_manage.dev_status != I2C_STS_IDLE)
			   && (i2c_dev->i2c_manage.dev_status != I2C_STS_ERROR)
			   && (i2c_dev->i2c_manage.dev_status != I2C_STS_TIMEOUT)) {
			/* Directly sleep here will cause runtime touchscreen not smooth(us-level I2C done). */
			/* No sleep here will cause audio xrun when I2C is not wired to board.(I2C cannot transfer). */
			if (wait_for_completion_timeout(&i2c_dev->xfer_completion, msecs_to_jiffies(3000)) == 0) {
				dev_err(i2c_dev->dev, "Wait for completion timeout\n");
				i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
				break;
			}

			if (rtk_i2c_is_timeout(i2c_dev) == HAL_TIMEOUT) {
				i2c_dev->i2c_manage.dev_status = I2C_STS_TIMEOUT;
				dev_err(i2c_dev->dev, "Send timeout: cannot finish write\n");
				break;
			}
		}
	}

	if (i2c_dev->i2c_manage.dev_status == I2C_STS_ERROR) {
		dev_err(i2c_dev->dev, "I2C transfer error\n");
		return;
	} else if (i2c_dev->i2c_manage.dev_status == I2C_STS_TIMEOUT) {
		dev_err(i2c_dev->dev, "I2C transfer timeout\n");
		return;
	}
	return;

ERROR_CHECK:
	rtk_i2c_dev_status_handler(i2c_dev, dev_status);
}

static int rtk_i2c_xfer(struct i2c_adapter *i2c_adap,
						struct i2c_msg msgs[], int num)
{
	struct rtk_i2c_dev *i2c_dev = i2c_get_adapdata(i2c_adap);
	int ret;

	/* Wait for slave hardware read/write flip. */
	udelay(50);

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	ret = pm_runtime_get_sync(i2c_dev->dev);
	if (ret < 0) {
		return ret;
	}
#endif //RTK_I2C_PM_RUNTIME

	ret = rtk_i2c_wait_free_bus(i2c_dev);
	if (ret) {
		goto pm_free;
	}

	i2c_dev->i2c_manage.msgs = msgs;
	i2c_dev->i2c_manage.msg_count = num;
	i2c_dev->i2c_manage.current_msg_id = 0;

	/* Start transfer I2C msgs. */
	i2c_dev->i2c_manage.timeout = i2c_adap->timeout * 1000000;
	rtk_gtimer_change_period(i2c_dev->i2c_manage.timer_index, i2c_adap->timeout * 1000000);
	rtk_gtimer_int_config(i2c_dev->i2c_manage.timer_index, 1);
	rtk_gtimer_start(i2c_dev->i2c_manage.timer_index, 1);

	/* Give HW I2C. */
	rtk_i2c_xfer_msg(i2c_dev, &msgs[0]);

	if (i2c_dev->i2c_manage.dev_status == I2C_STS_TIMEOUT) {
		rtk_gtimer_start(i2c_dev->i2c_manage.timer_index, 0);
		i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
		rtk_i2c_flow_deinit(i2c_dev);
		dev_err(i2c_dev->dev, "Wait slave 0x%04X timeout\n", msgs[0].addr);
		return -ETIMEDOUT;
	} else if (i2c_dev->i2c_manage.dev_status == I2C_STS_ERROR) {
		dev_err(i2c_dev->dev, "Some error happened when contacting slave 0x%04X\n", msgs[0].addr);
		ret = -ETIMEDOUT;
	}
	rtk_gtimer_start(i2c_dev->i2c_manage.timer_index, 0);

pm_free:
#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

	return (ret < 0) ? ret : num;
}

static u32 rtk_i2c_master_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_10BIT_ADDR;
}

static const struct i2c_algorithm rtk_i2c_master_algo = {
	.master_xfer = rtk_i2c_xfer,
	.functionality = rtk_i2c_master_func,
};

int rtk_i2c_master_probe(
	struct platform_device *pdev, struct rtk_i2c_dev *i2c_dev, struct i2c_adapter *adap)
{
	int r;

	i2c_dev->i2c_param.i2c_master = I2C_MASTER_MODE;
	adap->algo = &rtk_i2c_master_algo;

	r = rtk_gtimer_dynamic_init(i2c_dev->i2c_manage.user_set_timeout * 1000000,
								rtk_i2c_hw_timer_timeout, i2c_dev);
	if (r < 0) {
		dev_err(i2c_dev->dev, "Failed to get timer\n");
		return r;
	} else {
		i2c_dev->i2c_manage.timer_index = r;
	}

	/* Master parameters init. */
	i2c_dev->i2c_manage.rx_info.p_data_buf = devm_kzalloc(&pdev->dev, RTK_I2C_T_RX_FIFO_MAX, GFP_KERNEL);
	i2c_dev->i2c_manage.tx_info.p_data_buf = devm_kzalloc(&pdev->dev, RTK_I2C_T_RX_FIFO_MAX, GFP_KERNEL);

	rtk_i2c_init_hw(&i2c_dev->i2c_param);
	init_completion(&i2c_dev->xfer_completion);
	return 0;
}
