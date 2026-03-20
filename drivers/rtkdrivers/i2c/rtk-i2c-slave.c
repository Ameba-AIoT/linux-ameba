// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek I2C slave support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#if IS_ENABLED(CONFIG_I2C_SLAVE)
#include "i2c-realtek.h"

void rtk_i2c_slave_send(
	struct rtk_i2c_hw_params *i2c_param, u8 data)
{
	rtk_i2c_writel(i2c_param->i2c_dev->base, IC_DATA_CMD, data);
}

void rtk_i2c_isr_slave_handle_sar2_wake(struct rtk_i2c_dev *i2c_dev)
{
	i2c_dev->slave_dev->broadcast = 0;
	i2c_dev->slave_dev->current_slave = &i2c_dev->slave_dev->reg_slaves[1];
}

void rtk_i2c_isr_slave_handle_sar1_wake(struct rtk_i2c_dev *i2c_dev)
{
	i2c_dev->slave_dev->broadcast = 0;
	i2c_dev->slave_dev->current_slave = &i2c_dev->slave_dev->reg_slaves[0];
}

void rtk_i2c_isr_slave_handle_generall_call(struct rtk_i2c_dev *i2c_dev)
{
	i2c_dev->slave_dev->broadcast = 1;
}

void rtk_i2c_isr_slave_handle_rx_full(struct rtk_i2c_dev *i2c_dev)
{
	int i = 0;
	u8 data_recved;

	if (!i2c_dev->slave_dev->current_slave) {
		dev_dbg(i2c_dev->dev, "%s: Slave-end quit\n", __func__);
		return;
	}

	/* If no slave address matches the aim address. If not a generall call. */
	if (!i2c_dev->slave_dev->current_slave->slave && !i2c_dev->slave_dev->broadcast) {
		dev_err(i2c_dev->dev, "Unknown current slave state\n");
		return;
	}

	if (i2c_dev->i2c_manage.dev_status != I2C_STS_RX_ING) {
		if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, (BIT_RFNE | BIT_RFF))) {
			data_recved = rtk_i2c_receive_data(&i2c_dev->i2c_param);
		}
		if (i2c_dev->slave_dev->broadcast) {
			for (i = 0; i < i2c_dev->nr_slaves; i++) {
				if (i2c_dev->slave_dev->reg_slaves[i].slave &&
					(i2c_dev->slave_dev->reg_slaves[i].slave->flags & I2C_CLIENT_WAKE)) {
					if (i2c_slave_event(i2c_dev->slave_dev->reg_slaves[i].slave,
										I2C_SLAVE_WRITE_REQUESTED, NULL)) {
						i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
						return;
					} else if (i2c_slave_event(i2c_dev->slave_dev->reg_slaves[i].slave,
											   I2C_SLAVE_WRITE_RECEIVED, &data_recved) < 0) {
						i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
						return;
					} else {
						i2c_dev->i2c_manage.dev_status = I2C_STS_RX_ING;
					}
				}
			}
		} else {
			if (i2c_dev->slave_dev->current_slave->slave) {
				if (i2c_slave_event(i2c_dev->slave_dev->current_slave->slave,
									I2C_SLAVE_WRITE_REQUESTED, NULL) < 0) {
					/* Current slave reject the data. */
					i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
					return;
				} else if (i2c_slave_event(i2c_dev->slave_dev->current_slave->slave,
										   I2C_SLAVE_WRITE_RECEIVED, &data_recved) < 0) {
					i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
					return;
				} else {
					i2c_dev->i2c_manage.dev_status = I2C_STS_RX_ING;
				}
			} else {
				i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
				return;
			}
		}
	}

	while (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, (BIT_RFNE | BIT_RFF))) {
		data_recved = rtk_i2c_receive_data(&i2c_dev->i2c_param);
		if (i2c_dev->slave_dev->broadcast) {
			for (i = 0; i < i2c_dev->nr_slaves; i++) {
				if (i2c_dev->slave_dev->reg_slaves[i].slave &&
					(i2c_dev->slave_dev->reg_slaves[i].slave->flags & I2C_CLIENT_WAKE)) {
					i2c_slave_event(i2c_dev->slave_dev->reg_slaves[i].slave,
									I2C_SLAVE_WRITE_RECEIVED, &data_recved);
				}
			}
		} else {
			if (i2c_slave_event(i2c_dev->slave_dev->current_slave->slave,
								I2C_SLAVE_WRITE_RECEIVED, &data_recved) < 0) {
				i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
				return;
			}
		}
	}
}

/* Master inform slave that (master)rx-(slave)tx has been finished with NAK. */
/* Add I2C_BIT_M_STOP_DET to indicate i2c transfer is stopped. */
void rkt_i2c_isr_slave_handle_res_done(struct rtk_i2c_dev *i2c_dev)
{
	if (!i2c_dev->slave_dev->current_slave) {
		dev_dbg(i2c_dev->dev, "%s: Slave-end quit\n", __func__);
		goto exit;
	}
	if (i2c_dev->slave_dev->current_slave->slave) {
		i2c_slave_event(i2c_dev->slave_dev->current_slave->slave, I2C_SLAVE_STOP, NULL);
	}

	/* Master-end stop request data from slave, clear RD_REQ irq. */
	if (i2c_dev->i2c_manage.dev_status == I2C_STS_TX_ING) {
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_RD_REQ);
	}

exit:
	rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_M_STOP_DET);
	i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
	i2c_dev->slave_dev->current_slave = NULL;
	i2c_dev->slave_dev->broadcast = 0;
}

/* Master inform slave to tx data. */
void rtk_i2c_isr_slave_handle_rd_req(struct rtk_i2c_dev *i2c_dev)
{
	u8 data_for_send;

	if (!i2c_dev->slave_dev->current_slave) {
		dev_dbg(i2c_dev->dev, "%s: Slave-end quit\n", __func__);
		goto exit;
	}

	if (i2c_dev->i2c_manage.dev_status != I2C_STS_TX_ING &&
		i2c_dev->slave_dev->current_slave->slave) {
		if (i2c_slave_event(i2c_dev->slave_dev->current_slave->slave,
							I2C_SLAVE_READ_REQUESTED, &data_for_send) < 0) {
			i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
			/* Current slave reject the data. */
			goto exit;
		}
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_RD_REQ);
		rtk_i2c_slave_send(&i2c_dev->i2c_param, data_for_send);
		i2c_dev->i2c_manage.dev_status = I2C_STS_TX_ING;
		return;
	}

	i2c_dev->i2c_manage.dev_status = I2C_STS_TX_ING;
	if (rtk_i2c_check_flag_state(&i2c_dev->i2c_param, BIT_TFNF)) {
		/* Check master rx done for slave first, then callback the slave send. */
		if (i2c_slave_event(i2c_dev->slave_dev->current_slave->slave,
							I2C_SLAVE_READ_PROCESSED, &data_for_send) < 0) {
			goto exit;
		}
		rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_RD_REQ);
		rtk_i2c_slave_send(&i2c_dev->i2c_param, data_for_send);
		return;
	}

exit:
	dev_err(i2c_dev->dev, "Abort slave tx by no data. Reset i2c slave-end.");
	rtk_i2c_clear_interrupt(&i2c_dev->i2c_param, I2C_BIT_R_RD_REQ);
	rtk_i2c_enable_cmd(&i2c_dev->i2c_param, DISABLE);
	rtk_i2c_enable_cmd(&i2c_dev->i2c_param, ENABLE);
	i2c_dev->i2c_manage.dev_status = I2C_STS_IDLE;
	i2c_dev->slave_dev->current_slave = NULL;
	i2c_dev->slave_dev->broadcast = 0;
}

hal_status rtk_i2c_prepare_slave_interrupts(struct rtk_i2c_dev *i2c_dev)
{
	rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);
	rtk_i2c_interrupt_config(&i2c_dev->i2c_param,
							 (I2C_BIT_M_LP_WAKE_1 |
							  I2C_BIT_M_LP_WAKE_2 |
							  I2C_BIT_M_GEN_CALL |
							  I2C_BIT_M_RX_FULL |
							  I2C_BIT_M_RX_DONE |
							  I2C_BIT_M_STOP_DET |
							  I2C_BIT_M_TX_ABRT |
							  I2C_BIT_M_TX_OVER |
							  I2C_BIT_M_RD_REQ |
							  I2C_BIT_M_RX_UNDER), ENABLE);
	return HAL_OK;
}

static int rtk_i2c_slave_register(
	struct rtk_i2c_dev *i2c_dev,
	struct i2c_client *slave, int *id)
{
	struct device *dev = i2c_dev->dev;
	int i;

	for (i = 0; i < i2c_dev->nr_slaves; i++) {
		if (!i2c_dev->slave_dev->reg_slaves[i].slave) {
			i2c_dev->slave_dev->reg_slaves[i].slave = slave;
			*id = i2c_dev->slave_dev->reg_slaves[i].slave_id;
			if (i && (slave->flags & I2C_CLIENT_TEN)) {
				dev_err(dev, "Slave 0x%04X could not be registered\n", slave->addr);
				dev_err(dev, "Hardware: only slave0 support the 10-bit mode. But slave0 has already been taken\n");
				return -EINVAL;
			}
			return 0;
		}
	}

	dev_err(dev, "Failed to register slave 0x%04X, try increasing rtk,i2c-reg-slave-num in DTS\n", slave->addr);
	return -EINVAL;
}

static bool rtk_i2c_slave_offline_check(struct rtk_i2c_dev *i2c_dev)
{
	int i;

	for (i = 0; i < i2c_dev->nr_slaves; i++) {
		if (!i2c_dev->slave_dev->reg_slaves[i].slave) {
			return false;
		}
	}
	return true;
}

/* Can only register one slave. */
static int rtk_i2c_reg_slave(struct i2c_client *slave)
{
	struct rtk_i2c_dev *i2c_dev = i2c_get_adapdata(slave->adapter);
	int id, ret;

	if (slave->flags & I2C_CLIENT_PEC) {
		dev_err(i2c_dev->dev, "SMBus PEC not supported in slave mode\n");
		return -EINVAL;
	}

	if (i2c_dev->i2c_manage.dev_status == I2C_STS_RX_ING ||
		i2c_dev->i2c_manage.dev_status == I2C_STS_TX_ING) {
		dev_err(i2c_dev->dev, "Another slave device is transferring, wait and try again\n");
		return -EBUSY;
	}

	ret = rtk_i2c_slave_register(i2c_dev, slave, &id);
	if (ret) {
		return ret;
	}

	if (slave->flags & I2C_CLIENT_WAKE) {
		i2c_dev->i2c_param.i2c_slv_ack_gen_call = ENABLE;
	} else {
		i2c_dev->i2c_param.i2c_slv_ack_gen_call = DISABLE;
	}

	if (!id) {
		i2c_dev->i2c_param.i2c_ack_addr = slave->addr;
	} else {
		i2c_dev->i2c_param.i2c_ack_addr1 = slave->addr;
	}

	dev_dbg(i2c_dev->dev, "I2C slave 0x%04X registered\n", slave->addr);

	/* Import new params. */
	rtk_i2c_init_hw(&i2c_dev->i2c_param);
	rtk_i2c_flow_init(i2c_dev);

	rtk_i2c_prepare_slave_interrupts(i2c_dev);

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	ret = pm_runtime_get_sync(i2c_dev->dev);
	if (ret < 0) {
		return ret;
	}

	ret = 0;
pm_free:
	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	return ret;
}

static int rtk_i2c_unreg_slave(struct i2c_client *slave)
{
	struct rtk_i2c_dev *i2c_dev = i2c_get_adapdata(slave->adapter);
	int i;

	/* Recover current slave. */
	if (i2c_dev->slave_dev->current_slave && (slave == i2c_dev->slave_dev->current_slave->slave)) {
		rtk_i2c_interrupt_config(&i2c_dev->i2c_param, 0x1FFF, DISABLE);
		rtk_i2c_clear_all_interrupts(&i2c_dev->i2c_param);
		rtk_i2c_flow_deinit(i2c_dev);
		if (!rtk_i2c_slave_offline_check(i2c_dev)) {
			rtk_i2c_init_hw(&i2c_dev->i2c_param);
			rtk_i2c_prepare_slave_interrupts(i2c_dev);
		}
	}

	for (i = 0; i < i2c_dev->nr_slaves; i++) {
		if (i2c_dev->slave_dev->reg_slaves[i].slave == slave) {
			i2c_dev->slave_dev->reg_slaves[i].slave = NULL;
		}
	}

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	int ret;
	ret = pm_runtime_get_sync(i2c_dev->dev);
	if (ret < 0) {
		return ret;
	}
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

	if (rtk_i2c_slave_offline_check(i2c_dev)) {
		rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_RX_FULL |
								 I2C_BIT_M_RX_OVER |
								 I2C_BIT_M_RX_UNDER), DISABLE);
		rtk_i2c_interrupt_config(&i2c_dev->i2c_param, (I2C_BIT_M_TX_ABRT |
								 I2C_BIT_M_TX_EMPTY |
								 I2C_BIT_M_TX_OVER), DISABLE);
		rtk_i2c_flow_deinit(i2c_dev);
		rtk_i2c_init_hw(&i2c_dev->i2c_param);
	}

#if defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME
	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);
#endif // defined(RTK_I2C_PM_RUNTIME) && RTK_I2C_PM_RUNTIME

	return 0;
}

static u32 rtk_i2c_slave_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_10BIT_ADDR | I2C_FUNC_SLAVE;
}

static const struct i2c_algorithm rtk_i2c_slave_algo = {
	.functionality = rtk_i2c_slave_func,
	.reg_slave = rtk_i2c_reg_slave,
	.unreg_slave = rtk_i2c_unreg_slave,
};

int rtk_i2c_slave_probe(
	struct platform_device *pdev, struct rtk_i2c_dev *i2c_dev, struct i2c_adapter *adap)
{
	int i;

	i2c_dev->slave_dev = devm_kzalloc(&pdev->dev, sizeof(struct rtk_i2c_slave_dev), GFP_KERNEL);
	if (!i2c_dev->slave_dev) {
		return -ENOMEM;
	}

	i2c_dev->i2c_param.i2c_master = I2C_SLAVE_MODE;
	adap->algo = &rtk_i2c_slave_algo;

	i2c_dev->slave_dev->reg_slaves = devm_kcalloc(&pdev->dev,
									 i2c_dev->nr_slaves, sizeof(struct rtk_i2c_slave), GFP_KERNEL);
	if (!i2c_dev->slave_dev->reg_slaves) {
		return -ENOMEM;
	}

	/* Init slave parameters. */
	for (i = 0; i < i2c_dev->nr_slaves; i++) {
		i2c_dev->slave_dev->reg_slaves[i].slave = NULL;
		i2c_dev->slave_dev->reg_slaves[i].slave_id = i;
		i2c_dev->slave_dev->current_slave = NULL;
		i2c_dev->slave_dev->broadcast = 0;
	}
	return 0;
}
#endif // IS_ENABLED(CONFIG_I2C_SLAVE)
