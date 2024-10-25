// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek SPI support
*
* @attention
*		- for master tx, slave rx, slave prepare first and wait for master's tx data.
*		- for master rx, slave tx, slave tx to its fifo first and wait master's rx signal and dummy data.
*		- spi master trx can choose interrupt mode or poll mode by configuring dts.
*		- spi slave rx support interrupt mode only, so tx is also in interrupt mode.
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include "spi-realtek-general.h"
#include <linux/of_gpio.h>

/* Mapping address used by both SPI0 and SPI1. */
static void __iomem *role_set_base = 0;

static void rtk_spi_writel(
	void __iomem *ptr, u32 reg, u32 value)
{
	writel(value, (void __iomem *)((u32)ptr + reg));
}

static u32 rtk_spi_readl(void __iomem *ptr, u32 reg)
{
	return readl((void __iomem *)((u32)ptr + reg));
}

void rtk_spi_reg_update(
	void __iomem *ptr,
	u32 reg, u32 mask, u32 value)
{
	u32 temp;

	temp = rtk_spi_readl(ptr, reg);
	temp |= mask;
	temp &= (~mask | value);
	rtk_spi_writel(ptr, reg, temp);
}

void rtk_spi_interrupt_config(
	struct rtk_spi_controller *rtk_spi, u32 spi_int, u32 new_state)
{
	if (new_state == ENABLE) {
		/* Enable the selected SSI interrupts */
		rtk_spi_reg_update(rtk_spi->base, SPI_IMR, spi_int, spi_int);
	} else {
		/* Disable the selected SSI interrupts */
		rtk_spi_reg_update(rtk_spi->base, SPI_IMR, spi_int, ~spi_int);
	}
}

void rtk_spi_reg_dump(struct rtk_spi_controller *rtk_spi)
{
#if RTK_SPI_REG_DUMP
	dev_dbg(rtk_spi->dev, "SPI_CTRLR0[0x%04X] = 0x%08X\n", SPI_CTRLR0, rtk_spi_readl(rtk_spi->base, SPI_CTRLR0));
	dev_dbg(rtk_spi->dev, "SPI_CTRLR1[0x%04X] = 0x%08X\n", SPI_CTRLR1, rtk_spi_readl(rtk_spi->base, SPI_CTRLR1));
	dev_dbg(rtk_spi->dev, "SPI_SSIENR[0x%04X] = 0x%08X\n", SPI_SSIENR, rtk_spi_readl(rtk_spi->base, SPI_SSIENR));
	dev_dbg(rtk_spi->dev, "RSVD0[0x%04X] = 0x%08X\n", RSVD0, rtk_spi_readl(rtk_spi->base, RSVD0));
	dev_dbg(rtk_spi->dev, "SPI_SER[0x%04X] = 0x%08X\n", SPI_SER, rtk_spi_readl(rtk_spi->base, SPI_SER));
	dev_dbg(rtk_spi->dev, "SPI_BAUDR[0x%04X] = 0x%08X\n", SPI_BAUDR, rtk_spi_readl(rtk_spi->base, SPI_BAUDR));
	dev_dbg(rtk_spi->dev, "SPI_TXFTLR[0x%04X] = 0x%08X\n", SPI_TXFTLR, rtk_spi_readl(rtk_spi->base, SPI_TXFTLR));
	dev_dbg(rtk_spi->dev, "SPI_RXFTLR[0x%04X] = 0x%08X\n", SPI_RXFTLR, rtk_spi_readl(rtk_spi->base, SPI_RXFTLR));
	dev_dbg(rtk_spi->dev, "SPI_TXFLR[0x%04X] = 0x%08X\n", SPI_TXFLR, rtk_spi_readl(rtk_spi->base, SPI_TXFLR));
	dev_dbg(rtk_spi->dev, "SPI_RXFLR[0x%04X] = 0x%08X\n", SPI_RXFLR, rtk_spi_readl(rtk_spi->base, SPI_RXFLR));
	dev_dbg(rtk_spi->dev, "RTK_SPI_SR[0x%04X] = 0x%08X\n", RTK_SPI_SR, rtk_spi_readl(rtk_spi->base, RTK_SPI_SR));
	dev_dbg(rtk_spi->dev, "SPI_IMR[0x%04X] = 0x%08X\n", SPI_IMR, rtk_spi_readl(rtk_spi->base, SPI_IMR));
	dev_dbg(rtk_spi->dev, "SPI_ISR[0x%04X] = 0x%08X\n", SPI_ISR, rtk_spi_readl(rtk_spi->base, SPI_ISR));
	dev_dbg(rtk_spi->dev, "SPI_RISR[0x%04X] = 0x%08X\n", SPI_RISR, rtk_spi_readl(rtk_spi->base, SPI_RISR));
#endif // RTK_SPI_REG_DUMP
}

static void rtk_get_dts_info(
	struct rtk_spi_controller *rtk_spi,
	struct device_node *np,
	int *param_to_set, int default_value,
	char *dts_name)
{
	int nr_requests, ret;

	/* Get DTS params. */
	ret = of_property_read_u32(np, dts_name, &nr_requests);
	if (ret) {
		dev_warn(rtk_spi->dev,  "Can't get DTS property %s, set it to default value %d\n", dts_name, default_value);
		*param_to_set = default_value;
	} else {
		dev_dbg(rtk_spi->dev, "Get DTS property %s = %d\n", dts_name, nr_requests);
		*param_to_set = nr_requests;
	}
}

void rtk_spi_struct_init(
	struct rtk_spi_controller *rtk_spi,
	struct device_node *np)
{
	/* DTS defined params. */
	char s0[] = "reg";
	char s1[] = "rtk,spi-default-cs";
	char s2[] = "rtk,spi-slave-mode";
	char s3[] = "rtk,max-cs-num";
	char s4[] = "rtk,spi-index";
	char s5[] = "rtk,spi-dma-en";
	char s6[] = "rtk,spi-master-poll-mode";
	char s7[] = "rtk,spi-for-kernel";
	int ret = 0;
	enum of_gpio_flags flags;

	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.dma_params.spi_phy_addr, 0, s0);
	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.spi_default_cs, 0, s1);
	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.is_slave, 0, s2);
	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.max_cs_num, 0xFFFF, s3);
	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.spi_index, 0, s4);
	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.dma_enabled, 0, s5);
	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.spi_poll_mode, 0, s6);
	rtk_get_dts_info(rtk_spi, np, &rtk_spi->spi_manage.spi_for_kernel, 0, s7);

	rtk_spi->spi_param.rx_threshold_level = 0;
	rtk_spi->spi_param.tx_threshold_level = 1;

	rtk_spi->spi_param.dma_rx_data_level = 3;
	rtk_spi->spi_param.dma_tx_data_level = 56;

	rtk_spi->spi_param.slave_select_enable = 0;
	rtk_spi->spi_param.data_frame_num = 0;
	rtk_spi->spi_param.clock_divider = 2;
	rtk_spi->spi_param.data_frame_format = FRF_MOTOROLA_SPI;
	rtk_spi->spi_param.data_frame_size = DFS_8_BITS;
	rtk_spi->spi_param.interrupt_mask = 0x0;
	rtk_spi->spi_param.transfer_mode = TMOD_TR;
	/* Should keep the same for master and slave. */
	rtk_spi->spi_param.sclk_phase = SCPH_TOGGLES_IN_MIDDLE;
	rtk_spi->spi_param.sclk_polarity = SCPOL_INACTIVE_IS_LOW;

	rtk_spi->spi_manage.dma_params.rx_gdma_status = RTK_SPI_GDMA_UNPREPARED;
	rtk_spi->spi_manage.dma_params.tx_gdma_status = RTK_SPI_GDMA_UNPREPARED;

	if (!rtk_spi->spi_manage.is_slave) {
		rtk_spi->spi_manage.spi_cs_pin = of_get_named_gpio_flags(np, "rtk,spi-cs-gpios", 0, &flags);
		ret = gpio_request(rtk_spi->spi_manage.spi_cs_pin, NULL);
		if (ret != 0) {
			dev_err(rtk_spi->dev, "Failed to request SPI CS pin\n");
			goto fail;
		}

		ret = gpio_direction_output(rtk_spi->spi_manage.spi_cs_pin, 1);
		if (IS_ERR_VALUE(ret)) {
			dev_err(rtk_spi->dev, "Failed to set SPI CS to output direction\n");
			goto fail;
		}

		gpio_set_value(rtk_spi->spi_manage.spi_cs_pin, 1);
	}

	rtk_spi->spi_manage.dma_params.rx_chan = NULL;
	rtk_spi->spi_manage.dma_params.tx_chan = NULL;
	rtk_spi->spi_manage.dma_params.rx_config = NULL;
	rtk_spi->spi_manage.dma_params.tx_config = NULL;
	init_completion(&rtk_spi->spi_manage.dma_params.dma_rx_completion);
	init_completion(&rtk_spi->spi_manage.dma_params.dma_tx_completion);
	init_completion(&rtk_spi->spi_manage.txrx_completion);

	rtk_spi->spi_manage.dma_params.rx_dma_addr = 0;
	rtk_spi->spi_manage.dma_params.tx_dma_addr = 0;

	/* Disable all interrupts */
	rtk_spi_interrupt_config(rtk_spi, 0xFF, DISABLE);

fail:
	gpio_free(rtk_spi->spi_manage.spi_cs_pin);
}

void rtk_spi_enable_cmd(
	struct rtk_spi_controller *rtk_spi, u32 new_status)
{
	if (new_status != DISABLE) {
		rtk_spi_reg_update(rtk_spi->base, SPI_SSIENR, SPI_BIT_SSI_EN, SPI_BIT_SSI_EN);
	} else {
		rtk_spi_reg_update(rtk_spi->base, SPI_SSIENR, SPI_BIT_SSI_EN, ~SPI_BIT_SSI_EN);
	}
}

u32 rtk_spi_get_status(
	struct rtk_spi_controller *rtk_spi)
{
	return rtk_spi_readl(rtk_spi->base, RTK_SPI_SR);
}

u32 rtk_spi_busy_check(
	struct rtk_spi_controller *rtk_spi)
{
	return (((rtk_spi_get_status(rtk_spi) & SPI_BIT_BUSY) != 0) ? 1 : 0);
}

void rtk_spi_set_slave_enable(
	struct rtk_spi_controller *rtk_spi, u32 slave_index)
{
	rtk_spi_writel(rtk_spi->base, SPI_SER, 1 << slave_index);
}

void rtk_spi_set_dma_level(
	struct rtk_spi_controller *rtk_spi, u32 tx_level, u32 rx_level)
{
	/* Set TX FIFO water level to trigger Tx DMA transfer */
	rtk_spi_writel(rtk_spi->base, SPI_DMATDLR, tx_level);

	/* Set RX FIFO water level to trigger Rx DMA transfer */
	rtk_spi_writel(rtk_spi->base, SPI_DMARDLR, rx_level);
}

void rtk_spi_set_role(
	struct rtk_spi_controller *rtk_spi)
{
	if (rtk_spi->spi_manage.spi_index == 0) {
		if (!rtk_spi->spi_manage.is_slave) {
			/* Set spi0 master mode. */
			rtk_spi_reg_update(role_set_base, REG_HSYS_HPLAT_CTRL, HSYS_BIT_SPI0_MST, HSYS_BIT_SPI0_MST);
		} else {
			/* Set spi0 slave mode. */
			rtk_spi_reg_update(role_set_base, REG_HSYS_HPLAT_CTRL, HSYS_BIT_SPI0_MST, ~HSYS_BIT_SPI0_MST);
		}
	} else if (rtk_spi->spi_manage.spi_index == 1) {
		if (!rtk_spi->spi_manage.is_slave) {
			/* Set spi1 master mode. */
			rtk_spi_reg_update(role_set_base, REG_HSYS_HPLAT_CTRL, HSYS_BIT_SPI1_MST, HSYS_BIT_SPI1_MST);
		} else {
			/* Set spi1 slave mode. */
			rtk_spi_reg_update(role_set_base, REG_HSYS_HPLAT_CTRL, HSYS_BIT_SPI1_MST, ~HSYS_BIT_SPI1_MST);
		}
	} else {
		dev_err(rtk_spi->dev, "Invalid SPI index %d\n", rtk_spi->spi_manage.spi_index);
	}
}

void rtk_spi_set_clk_polarity(
	struct rtk_spi_controller *rtk_spi, u32 sclk_polarity)
{
	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_BIT_SCPOL, sclk_polarity << 7);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);
}

void rtk_spi_set_clk_phase(
	struct rtk_spi_controller *rtk_spi, u32 sclk_phase)
{
	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_BIT_SCPH, sclk_phase << 6);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);
}

void rtk_spi_set_frame_size(
	struct rtk_spi_controller *rtk_spi, u32 data_frame_size)
{
	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_MASK_DFS, data_frame_size);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);
}

void rtk_spi_set_read_len(
	struct rtk_spi_controller *rtk_spi, u32 data_frame_num)
{
	if ((data_frame_num < 1) || (data_frame_num >  0x10000)) {
		dev_err(rtk_spi->dev, "Illegal data_frame_num = 0x%08X\n", data_frame_num);
		return;
	}
	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_writel(rtk_spi->base, SPI_CTRLR1, data_frame_num - 1);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);
}

void rtk_spi_set_baud_div(
	struct rtk_spi_controller *rtk_spi, u32 clk_divider)
{
	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_writel(rtk_spi->base, SPI_BAUDR, clk_divider & SPI_MASK_SCKDV);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);
}

void rtk_spi_set_dma_enable(
	struct rtk_spi_controller *rtk_spi,
	u32 new_state, u8 direction)
{
	if (new_state == DISABLE) {
		if (direction == SPI_TX_MODE) {
			rtk_spi_reg_update(rtk_spi->base, SPI_DMACR, SPI_BIT_TDMAE, ~SPI_BIT_TDMAE);
		} else {
			rtk_spi_reg_update(rtk_spi->base, SPI_DMACR, SPI_BIT_RDMAE, ~SPI_BIT_RDMAE);
		}
	} else {
		if (direction == SPI_TX_MODE) {
			rtk_spi_reg_update(rtk_spi->base, SPI_DMACR, SPI_BIT_TDMAE, SPI_BIT_TDMAE);
		} else {
			rtk_spi_reg_update(rtk_spi->base, SPI_DMACR, SPI_BIT_RDMAE, SPI_BIT_RDMAE);
		}
	}
}

void rtk_spi_init_hw(struct rtk_spi_controller *rtk_spi)
{
	u32 temp_value	= 0;

	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_set_role(rtk_spi);

	/* REG_DW_SSI_CTRLR0 */
	temp_value |= rtk_spi->spi_param.data_frame_size;
	temp_value |= (rtk_spi->spi_param.data_frame_format << 4);
	temp_value |= (rtk_spi->spi_param.sclk_phase << 6);
	temp_value |= (rtk_spi->spi_param.sclk_polarity << 7);
	temp_value |= (rtk_spi->spi_param.transfer_mode << 8);
	temp_value &= ~SPI_BIT_SLV_OE;
	rtk_spi_writel(rtk_spi->base, SPI_CTRLR0, temp_value);
	rtk_spi_writel(rtk_spi->base, SPI_TXFTLR, rtk_spi->spi_param.tx_threshold_level);
	rtk_spi_writel(rtk_spi->base, SPI_RXFTLR, rtk_spi->spi_param.rx_threshold_level);

	/* Master Only:REG_DW_SSI_CTRLR1, REG_DW_SSI_SER, REG_DW_SSI_BAUDR*/
	if (rtk_spi->spi_param.spi_role & SSI_MASTER) {
		rtk_spi_writel(rtk_spi->base, SPI_CTRLR1, rtk_spi->spi_param.data_frame_num);
		rtk_spi_set_slave_enable(rtk_spi, rtk_spi->spi_param.slave_select_enable);
		rtk_spi_writel(rtk_spi->base, SPI_BAUDR, rtk_spi->spi_param.clock_divider);
	}

	/* REG_DW_SSI_IMR */
	rtk_spi_writel(rtk_spi->base, SPI_IMR, rtk_spi->spi_param.interrupt_mask);

	/*DMA level set */
	rtk_spi_set_dma_level(rtk_spi, rtk_spi->spi_param.dma_tx_data_level, rtk_spi->spi_param.dma_rx_data_level);

	rtk_spi_enable_cmd(rtk_spi, ENABLE);

	rtk_spi_set_clk_polarity(rtk_spi, rtk_spi->spi_param.sclk_polarity);
	rtk_spi_set_clk_phase(rtk_spi, rtk_spi->spi_param.sclk_phase);
	rtk_spi_set_baud_div(rtk_spi, rtk_spi->spi_param.clock_divider);
}

void rtk_spi_clean_interrupt(
	struct rtk_spi_controller *rtk_spi, u32 int_status)
{
	if (int_status & SPI_BIT_TXOIS) {
		rtk_spi_readl(rtk_spi->base, SPI_TXOICR);
	}

	if (int_status & SPI_BIT_RXUIS) {
		rtk_spi_readl(rtk_spi->base, SPI_RXUICR);
	}

	if (int_status & SPI_BIT_RXOIS) {
		rtk_spi_readl(rtk_spi->base, SPI_RXOICR);
	}

	if (int_status & SPI_BIT_MSTIS_FAEIS) {
		/* Another master is actively transferring data */
		rtk_spi_readl(rtk_spi->base, SPI_MSTICR_FAEICR);
	}

	if (int_status & SPI_BIT_TXUIS) {
		/* For slave only. This register is used as TXUICR in slave mode */
		rtk_spi_readl(rtk_spi->base, SPI_TXUICR);
	}

	if (int_status & SPI_BIT_SSRIS) {
		/* For slave only. This register is used as SSRICR in slave mode */
		rtk_spi_readl(rtk_spi->base, SPI_SSRICR);
	}
}

void rtk_spi_write_data(
	struct rtk_spi_controller *rtk_spi, u32 value)
{
	rtk_spi_writel(rtk_spi->base, SPI_DATA_FIFO_ENRTY, value & SPI_MASK_DR);
}

void rtk_spi_set_rx_fifo_level(
	struct rtk_spi_controller *rtk_spi, u32 rx_threshold_level)
{
	rtk_spi_writel(rtk_spi->base, SPI_RXFTLR, rx_threshold_level);
}

void rtk_spi_set_tx_fifo_level(
	struct rtk_spi_controller *rtk_spi, u32 tx_threshold_level)
{
	rtk_spi_writel(rtk_spi->base, SPI_TXFTLR, tx_threshold_level);
}

u32 rtk_spi_read_data(
	struct rtk_spi_controller *rtk_spi)
{
	u32 temp;
	temp = rtk_spi_readl(rtk_spi->base, SPI_DATA_FIFO_ENRTY);
	return temp;
}

u32 rtk_spi_get_rx_cnt(
	struct rtk_spi_controller *rtk_spi)
{
	return rtk_spi_readl(rtk_spi->base, SPI_RXFLR) & SPI_MASK_RXTFL;
}

u32 rtk_spi_get_tx_cnt(
	struct rtk_spi_controller *rtk_spi)
{
	return rtk_spi_readl(rtk_spi->base, SPI_TXFLR) & SPI_MASK_TXTFL;
}

u32 rtk_spi_get_data_frame_size(
	struct rtk_spi_controller *rtk_spi)
{
	return ((rtk_spi_readl(rtk_spi->base, SPI_CTRLR0) & SPI_MASK_DFS) + 1);
}

void rtk_spi_set_sample_delay(
	struct rtk_spi_controller *rtk_spi, u32 sample_delay)
{
	rtk_spi_writel(rtk_spi->base, SPI_RX_SAMPLE_DLY, sample_delay & SPI_MASK_RSD);
}

u32 rtk_spi_get_interrupt(
	struct rtk_spi_controller *rtk_spi)
{
	return rtk_spi_readl(rtk_spi->base, SPI_ISR);
}

#if RTK_SPI_HW_CONTROL_FOR_FUTURE_USE
void rtk_spi_set_toggle_phase(
	struct rtk_spi_controller *rtk_spi, u32 toggle_phase)
{
	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_BIT_SS_T, toggle_phase << 31);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);
}

void rtk_spi_set_data_swap(
	struct rtk_spi_controller *rtk_spi, u32 swap_status, u32 new_state)
{
	rtk_spi_enable_cmd(rtk_spi, DISABLE);

	if (new_state == ENABLE) {
		rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, swap_status, swap_status);
	} else {
		rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, swap_status, ~swap_status);
	}

	rtk_spi_enable_cmd(rtk_spi, ENABLE);
}

u32 rtk_spi_get_raw_interrupt(
	struct rtk_spi_controller *rtk_spi)
{
	return rtk_spi_readl(rtk_spi->base, SPI_RISR);
}

u32 rtk_spi_check_slave_enable(
	struct rtk_spi_controller *rtk_spi)
{
	return rtk_spi_readl(rtk_spi->base, SPI_SER);
}
#endif // RTK_SPI_HW_CONTROL_FOR_FUTURE_USE

u32 rtk_spi_get_readable(
	struct rtk_spi_controller *rtk_spi)
{
	u32 status = rtk_spi_get_status(rtk_spi);
	u32 readable = (((status & SPI_BIT_RFNE) != 0) ? 1 : 0);

	dev_dbg(rtk_spi->dev, "RX FIFO %s empty\n", readable ? "is not" : "is");
	return readable;
}

u32 rtk_spi_get_writeable(
	struct rtk_spi_controller *rtk_spi)
{
	u8 status = 0;
	u32 value = 0;

	value = rtk_spi_get_status(rtk_spi);
	status = (((value & SPI_BIT_TFNF) != 0) ? 1 : 0);

	return status;
}

static int rtk_spi_wait_for_completion(struct spi_controller *controller,
									   struct spi_transfer *transfer)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);
	unsigned long long ms = 1;

	if (rtk_spi->spi_manage.is_slave) {
		if (wait_for_completion_interruptible(&rtk_spi->spi_manage.txrx_completion)) {
			dev_err(rtk_spi->dev, "Wait for slave transfer interrupt\n");
			return -EINTR;
		}
	} else {
		if (transfer->speed_hz) {
			ms = SPI_SHIFT_WAIT_MSECS * transfer->len;
			do_div(ms, transfer->speed_hz);
			ms += ms + SPI_WAIT_TOLERANCE; /* some tolerance */
		} else {
			ms = 2000; // 2s for default when speed_hz = 0.
		}

		if (ms > UINT_MAX) {
			ms = UINT_MAX;
		}

		if (!wait_for_completion_timeout(&rtk_spi->spi_manage.txrx_completion, msecs_to_jiffies(ms))) {
			dev_err(rtk_spi->dev, "Wait for master transfer completion timeout\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

void rtk_spi_status_switch(
	struct rtk_spi_controller *rtk_spi,
	enum rtk_spi_master_trans_status to_set)
{
	switch (to_set) {
	case RTK_SPI_RX_DONE:
		if (rtk_spi->spi_manage.transfer_status == RTK_SPI_ONGOING) {
			rtk_spi->spi_manage.transfer_status = RTK_SPI_RX_DONE;
		}

		if (rtk_spi->spi_manage.transfer_status == RTK_SPI_TX_DONE) {
			rtk_spi->spi_manage.transfer_status = RTK_SPI_FULL_DUPLEX_DONE;
		}
		break;
	case RTK_SPI_TX_DONE:
		if (rtk_spi->spi_manage.transfer_status == RTK_SPI_ONGOING) {
			rtk_spi->spi_manage.transfer_status = RTK_SPI_TX_DONE;
		}

		if (rtk_spi->spi_manage.transfer_status == RTK_SPI_RX_DONE) {
			rtk_spi->spi_manage.transfer_status = RTK_SPI_FULL_DUPLEX_DONE;
		}
		break;
	default:
		rtk_spi->spi_manage.transfer_status = to_set;
		break;
	}

	dev_dbg(rtk_spi->dev, "Transfer status = %d\n", rtk_spi->spi_manage.transfer_status);
}

u32 rtk_spi_receive_data(
	struct rtk_spi_controller *rtk_spi,
	void *rx_data,
	u32 length)
{
	u32 receive_level;
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);
	u32 readable = rtk_spi_get_readable(rtk_spi);
	u32 left_data = length;

	while (readable) {
		receive_level = rtk_spi_get_rx_cnt(rtk_spi);
		while (receive_level--) {
			if (rx_data != NULL) {
				if (data_frame_size > 8) {
					/*  16~9 bits mode */
					*((u16 *)(rx_data)) = (u16)rtk_spi_read_data(rtk_spi);
					rx_data = (void *)(((u16 *)rx_data) + 1);
				} else {
					/*  8~4 bits mode */
					*((u8 *)(rx_data)) = (u8)rtk_spi_read_data(rtk_spi);
					rx_data = (void *)(((u8 *)rx_data) + 1);
				}
			}

			if (left_data > 0) {
				left_data--;
			}
			if (left_data == 0) {
				break;
			}
		}

		if (left_data == 0) {
			break;
		}

		readable = rtk_spi_get_readable(rtk_spi);
	}

	if (left_data) {
		if (left_data <= RTK_SPI_FIFO_ALL) {
			rtk_spi_set_rx_fifo_level(rtk_spi, left_data - 1);
		} else {
			rtk_spi_set_rx_fifo_level(rtk_spi, RTK_SPI_FIFO_ALL - 1);
		}
	}

	return (length - left_data);
}

u32 rtk_spi_send_data(
	struct rtk_spi_controller *rtk_spi,
	const void *tx_data,
	u32 length,
	u32 is_slave)
{
	u32 writeable = rtk_spi_get_writeable(rtk_spi);
	u32 tx_write_max = SSI_TX_FIFO_DEPTH - rtk_spi_get_tx_cnt(rtk_spi);
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);
	u32 tx_length = length;

	if (writeable) {
		while (tx_write_max--) {
			if (tx_length == 0) {
				break;
			}
			if (data_frame_size > 8) {
				// 16~9 bits mode
				if (tx_data != NULL) {
					rtk_spi_write_data(rtk_spi, *((u16 *)(tx_data)));
					tx_data = (void *)(((u16 *)tx_data) + 1);
				}
			} else {
				// 8~4 bits mode
				if (tx_data != NULL) {
					rtk_spi_write_data(rtk_spi, *((u8 *)(tx_data)));
					tx_data = (void *)(((u8 *)tx_data) + 1);
				}
			}

			if (tx_length) {
				tx_length--;
			}
		}
	}

	if (tx_length) {
		if (tx_length < RTK_SPI_FIFO_ALL) {
			rtk_spi_set_tx_fifo_level(rtk_spi, tx_length);
		} else {
			rtk_spi_set_tx_fifo_level(rtk_spi, RTK_SPI_FIFO_ALL - 1);
		}
	}

	return (length - tx_length);
}

static irqreturn_t rtk_spi_interrupt_handler(int irq, void *dev_id)
{
	struct rtk_spi_controller *rtk_spi = dev_id;

	u32 int_status = rtk_spi_get_interrupt(rtk_spi);
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);
	u32 trans_len = 0;

	rtk_spi_clean_interrupt(rtk_spi, int_status);

	if (int_status & (SPI_BIT_TXOIS | SPI_BIT_RXUIS | SPI_BIT_RXOIS | SPI_BIT_TXUIS)) {
		dev_dbg(rtk_spi->dev, "TX and RX warning = 0x%08X\n", int_status);
	}

	if (int_status & SPI_BIT_SSRIS) {
		dev_dbg(rtk_spi->dev, "SS_N rising edge detected\n");
		rtk_spi_interrupt_config(rtk_spi, SPI_BIT_SSRIM, DISABLE);
		if (rtk_spi->spi_manage.is_slave) {
			if (rtk_spi->controller && rtk_spi->controller->cur_msg_prepared) {
				complete(&rtk_spi->spi_manage.txrx_completion);
			}

			return IRQ_HANDLED;
		}
	}

	if (int_status & SPI_BIT_MSTIS_FAEIS) {
		dev_dbg(rtk_spi->dev, "Multi master contention interrupt or Slave frame alignment interrupt\n");
	}

	if ((int_status & SPI_BIT_RXFIS)) {

		rtk_spi_interrupt_config(rtk_spi, (SPI_BIT_RXFIM | SPI_BIT_RXOIM | SPI_BIT_RXUIM), DISABLE);
		trans_len = rtk_spi_receive_data(rtk_spi, rtk_spi->spi_manage.rx_info.p_data_buf, rtk_spi->spi_manage.rx_info.data_len);
		rtk_spi->spi_manage.rx_info.data_len -= trans_len;
		if (data_frame_size > 8) {
			// 16~9 bits mode
			rtk_spi->spi_manage.rx_info.p_data_buf = (void *)(((u16 *)rtk_spi->spi_manage.rx_info.p_data_buf) + trans_len);
		} else {
			// 8~4 bits mode
			rtk_spi->spi_manage.rx_info.p_data_buf = (void *)(((u8 *)rtk_spi->spi_manage.rx_info.p_data_buf) + trans_len);
		}

		if (!rtk_spi->spi_manage.rx_info.data_len) {
			rtk_spi_status_switch(rtk_spi, RTK_SPI_RX_DONE);
			if (rtk_spi->spi_manage.transfer_status == RTK_SPI_FULL_DUPLEX_DONE) {
				complete(&rtk_spi->spi_manage.txrx_completion);
			}
		} else {
			rtk_spi_interrupt_config(rtk_spi, (SPI_BIT_RXFIM | SPI_BIT_RXOIM | SPI_BIT_RXUIM | SPI_BIT_SSRIS), ENABLE);
		}
	}

	if (int_status & SPI_BIT_TXEIS) {

		rtk_spi_interrupt_config(rtk_spi, SPI_BIT_TXEIM, DISABLE);
		if (!rtk_spi->spi_manage.tx_info.data_len) {
			rtk_spi_status_switch(rtk_spi, RTK_SPI_TX_DONE);
			if (rtk_spi->spi_manage.transfer_status == RTK_SPI_FULL_DUPLEX_DONE) {
				complete(&rtk_spi->spi_manage.txrx_completion);
			}
		} else {
			trans_len = rtk_spi_send_data(rtk_spi, rtk_spi->spi_manage.tx_info.p_data_buf, rtk_spi->spi_manage.tx_info.data_len, rtk_spi->spi_manage.is_slave);
			rtk_spi->spi_manage.tx_info.data_len -= trans_len;
			if (data_frame_size > 8) {
				// 16~9 bits mode
				rtk_spi->spi_manage.tx_info.p_data_buf = (void *)(((u16 *)rtk_spi->spi_manage.tx_info.p_data_buf) + trans_len);
			} else {
				// 8~4 bits mode
				rtk_spi->spi_manage.tx_info.p_data_buf = (void *)(((u8 *)rtk_spi->spi_manage.tx_info.p_data_buf) + trans_len);
			}

			rtk_spi_interrupt_config(rtk_spi, SPI_BIT_TXEIM | SPI_BIT_SSRIS, ENABLE);
		}
	}

	return IRQ_HANDLED;
}

void rtk_spi_interrupt_read(
	struct rtk_spi_controller *rtk_spi,
	void *rx_data, u32 length)
{
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);

	if (length == 0) {
		dev_err(rtk_spi->dev, "Illegal read length\n");
		return;
	}

	if (data_frame_size > 8) {
		/*  16~9 bits mode */
		rtk_spi->spi_manage.rx_info.data_len = length >> 1;    // 2 bytes(16 bit) every transfer
	} else {
		/*  8~4 bits mode */
		rtk_spi->spi_manage.rx_info.data_len = length;    // 1 byte(8 bit) every transfer
	}

	rtk_spi->spi_manage.rx_info.p_data_buf = rx_data;
	rtk_spi_interrupt_config(rtk_spi, (SPI_BIT_RXFIM | SPI_BIT_RXOIM | SPI_BIT_RXUIM | SPI_BIT_SSRIS), ENABLE);
}

void rtk_spi_interrupt_write(
	struct rtk_spi_controller *rtk_spi,
	const void *ptx_data, u32 length)
{
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);

	if (length == 0) {
		dev_err(rtk_spi->dev, "Illegal write length\n");
		return;
	}

	rtk_spi_reg_dump(rtk_spi);

	if (data_frame_size > 8) {
		/*  16~9 bits mode */
		rtk_spi->spi_manage.tx_info.data_len = length >> 1;    // 2 bytes(16 bit) every transfer
	} else {
		/*  8~4 bits mode */
		rtk_spi->spi_manage.tx_info.data_len = length;    // 1 byte(8 bit) every transfer
	}

	rtk_spi->spi_manage.tx_info.p_data_buf = (void *)ptx_data;
	rtk_spi_interrupt_config(rtk_spi, (SPI_BIT_TXOIM | SPI_BIT_TXEIM | SPI_BIT_SSRIS), ENABLE);
}

u32 rtk_spi_poll_send(
	struct rtk_spi_controller *rtk_spi,
	const void *ptx_data, u32 length)
{
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);

	while (length) {
		if (rtk_spi_get_status(rtk_spi) & SPI_BIT_TFNF) {
			if (data_frame_size > 8) {
				rtk_spi_write_data(rtk_spi, *(u16 *)(ptx_data));
				ptx_data = (void *)(((u16 *)ptx_data) + 1);
			} else {
				rtk_spi_write_data(rtk_spi, *(u8 *)(ptx_data));
				ptx_data = (void *)(((u8 *)ptx_data) + 1);
			}
			length--;
		}
	}

	/* Wait for SPI busy bit to clear */
	while (rtk_spi_busy_check(rtk_spi)) {
		dev_dbg(rtk_spi->dev, "SPI is still busy\n");
		schedule_timeout_interruptible(msecs_to_jiffies(SPI_BUSBUSY_WAIT_TIMESLICE));
	}

	rtk_spi_status_switch(rtk_spi, RTK_SPI_TX_DONE);

	return 0;
}

int rtk_spi_poll_receive(
	struct rtk_spi_controller *rtk_spi,
	void *prx_data, u32 length)
{
	int cnt_to_rx;
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);

	while (length) {
		if ((rtk_spi_get_status(rtk_spi)) & SPI_BIT_RFNE) {
			cnt_to_rx = rtk_spi_get_rx_cnt(rtk_spi);
			while (cnt_to_rx--) {
				if (data_frame_size > 8) {
					/*  16~9 bits mode */
					*((u16 *)(prx_data)) = (u16)rtk_spi_read_data(rtk_spi);
					prx_data = (void *)(((u16 *)prx_data) + 1);
				} else {
					/*  8~4 bits mode */
					*((u8 *)(prx_data)) = (u8)rtk_spi_read_data(rtk_spi);
					prx_data = (void *)(((u8 *)prx_data) + 1);
				}
				length--;
				if (length == 0) {
					rtk_spi_status_switch(rtk_spi, RTK_SPI_RX_DONE);
					return 0;
				}
			}
		}
	}
	rtk_spi->spi_manage.rx_info.p_data_buf = prx_data;

	return length;
}

u32 rtk_spi_poll_trx(
	struct rtk_spi_controller *rtk_spi,
	void *prx_data, const void *ptx_data, u32 length)
{
	u32 rx_tmp_cnt, tx_cnt = 0, rx_cnt = 0;
	u32 data_frame_size = rtk_spi_get_data_frame_size(rtk_spi);

	while (tx_cnt < length || rx_cnt < length) {
		if (tx_cnt < length) {
			if (rtk_spi_get_status(rtk_spi) & SPI_BIT_TFNF) {
				if (data_frame_size > 8) {
					rtk_spi_write_data(rtk_spi, *(u16 *)(ptx_data));
					ptx_data = (void *)(((u16 *)ptx_data) + 1);
				} else {
					rtk_spi_write_data(rtk_spi, *(u8 *)(ptx_data));
					ptx_data = (void *)(((u8 *)ptx_data) + 1);
				}
				tx_cnt++;
			}
		}

		if (rx_cnt < length) {
			while (rtk_spi_get_status(rtk_spi) & SPI_BIT_RFNE) {
				rx_tmp_cnt = rtk_spi_get_rx_cnt(rtk_spi);
				while (rx_tmp_cnt) {
					if (data_frame_size > 8) {
						*(u16 *)(prx_data) = (u16)rtk_spi_read_data(rtk_spi);
						prx_data = (void *)(((u16 *)prx_data) + 1);
					} else {
						*(u8 *)(prx_data) = (u8)rtk_spi_read_data(rtk_spi);
						prx_data = (void *)(((u8 *)prx_data) + 1);
					}

					rx_cnt++;
					rx_tmp_cnt--;
				}
			}
		}
	}

	rtk_spi_status_switch(rtk_spi, RTK_SPI_TX_DONE);
	return 0;
}

void rtk_spi_dma_rx_done_callback(void *data)
{
	struct rtk_spi_controller *rtk_spi = data;

	rtk_spi_set_dma_enable(rtk_spi, DISABLE, SPI_RX_MODE);
	rtk_spi->spi_manage.dma_params.rx_gdma_status = RTK_SPI_GDMA_DONE;
	complete(&rtk_spi->spi_manage.dma_params.dma_rx_completion);
}

void rtk_spi_dma_tx_done_callback(void *data)
{
	struct rtk_spi_controller *rtk_spi = data;

	rtk_spi_set_dma_enable(rtk_spi, DISABLE, SPI_TX_MODE);
	rtk_spi->spi_manage.dma_params.tx_gdma_status = RTK_SPI_GDMA_DONE;
	complete(&rtk_spi->spi_manage.dma_params.dma_tx_completion);
}

int rtk_spi_gdma_prepare(struct rtk_spi_controller *rtk_spi)
{
	int ret = 0;
	char name[10];

	snprintf(name, 10, "spi%d-rx", rtk_spi->spi_manage.spi_index);
	if (!rtk_spi->spi_manage.dma_params.rx_chan) {
		rtk_spi->spi_manage.dma_params.rx_chan = dma_request_chan(rtk_spi->dev, name);
	}
	if (!rtk_spi->spi_manage.dma_params.rx_chan) {
		dev_err(rtk_spi->dev, "Failed to request SPI DMA channel\n");
		rtk_spi->spi_manage.dma_params.rx_gdma_status = RTK_SPI_GDMA_UNPREPARED;
		return -1;
	}
	snprintf(name, 10, "spi%d-tx", rtk_spi->spi_manage.spi_index);
	if (!rtk_spi->spi_manage.dma_params.tx_chan) {
		rtk_spi->spi_manage.dma_params.tx_chan = dma_request_chan(rtk_spi->dev, name);
	}
	if (!rtk_spi->spi_manage.dma_params.tx_chan) {
		dev_err(rtk_spi->dev, "Failed to request SPI DMA channel\n");
		rtk_spi->spi_manage.dma_params.tx_gdma_status = RTK_SPI_GDMA_UNPREPARED;
		return -1;
	}

	return ret;
}

void rtk_spi_gdma_deinit(struct rtk_spi_controller *rtk_spi)
{

	if (rtk_spi->spi_manage.dma_params.tx_config) {
		kfree(rtk_spi->spi_manage.dma_params.tx_config);
		rtk_spi->spi_manage.dma_params.tx_config = NULL;
	}
	if (rtk_spi->spi_manage.dma_params.tx_dma_addr) {
		dma_unmap_single(rtk_spi->dev,
						 rtk_spi->spi_manage.dma_params.tx_dma_addr,
						 rtk_spi->spi_manage.dma_params.tx_dma_length,
						 DMA_MEM_TO_DEV);
		rtk_spi->spi_manage.dma_params.tx_dma_addr = 0;
	}
	if (rtk_spi->spi_manage.dma_enabled && rtk_spi->spi_manage.dma_params.tx_chan) {
		dma_release_channel(rtk_spi->spi_manage.dma_params.tx_chan);
		rtk_spi->spi_manage.dma_params.tx_chan = NULL;
		rtk_spi->spi_manage.dma_params.tx_gdma_status = RTK_SPI_GDMA_UNPREPARED;
	}
	complete(&rtk_spi->spi_manage.dma_params.dma_tx_completion);
	if (rtk_spi->spi_manage.dma_params.rx_config) {
		kfree(rtk_spi->spi_manage.dma_params.rx_config);
		rtk_spi->spi_manage.dma_params.rx_config = NULL;
	}
	if (rtk_spi->spi_manage.dma_params.rx_dma_addr) {
		dma_unmap_single(rtk_spi->dev,
						 rtk_spi->spi_manage.dma_params.rx_dma_addr,
						 rtk_spi->spi_manage.dma_params.rx_dma_length,
						 DMA_DEV_TO_MEM);
		rtk_spi->spi_manage.dma_params.rx_dma_addr = 0;
	}
	if (rtk_spi->spi_manage.dma_enabled && rtk_spi->spi_manage.dma_params.rx_chan) {
		dma_release_channel(rtk_spi->spi_manage.dma_params.rx_chan);
		rtk_spi->spi_manage.dma_params.rx_chan = NULL;
		rtk_spi->spi_manage.dma_params.rx_gdma_status = RTK_SPI_GDMA_UNPREPARED;
	}
	complete(&rtk_spi->spi_manage.dma_params.dma_rx_completion);
	rtk_spi_set_dma_enable(rtk_spi, DISABLE, SPI_TX_MODE);
	rtk_spi_set_dma_enable(rtk_spi, DISABLE, SPI_RX_MODE);
}

static bool rtk_spi_can_dma(struct spi_controller *controller,
							struct spi_device *spi,
							struct spi_transfer *transfer)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);

	if (transfer->len > RTK_SPI_FIFO_ALL) {
		rtk_spi->spi_manage.dma_enabled = 1;
		return true;
	} else {
		rtk_spi->spi_manage.dma_enabled = 0;
		return false;
	}
}

static int rtk_spi_calculate_dmatimeout(struct spi_transfer *transfer)
{
	unsigned long timeout = 0;
	int ret = 0;

	if (transfer->speed_hz) {
		timeout = SPI_DMASHIFT_WAIT_MSECS * transfer->len / transfer->speed_hz;
		timeout += SPI_DMAWAIT_TOLERANCE;
		ret = msecs_to_jiffies(SPI_DMAWAIT_TOLERANCE * timeout * MSEC_PER_SEC);
	} else {
		ret = msecs_to_jiffies(2000); // 2s for default when speed_hz = 0.
	}
	return ret;
}

/* Accept only 8-bit-rule data here. */
int rtk_spi_do_dma_transfer(
	struct spi_controller *controller,
	struct spi_device *spi,
	struct spi_transfer *transfer)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);
	struct rtk_spi_gdma_parameters	*dma_params;
	unsigned long transfer_timeout, timeout;
	int ret = -EIO;

	if (transfer->len > MAX_DMA_LENGTH) {
		return ret;
	}

	dma_params = &rtk_spi->spi_manage.dma_params;
	transfer_timeout = rtk_spi_calculate_dmatimeout(transfer);

	if (rtk_spi->spi_manage.dma_enabled && (dma_params->rx_gdma_status == RTK_SPI_GDMA_UNPREPARED) && (dma_params->tx_gdma_status == RTK_SPI_GDMA_UNPREPARED)) {
		ret = rtk_spi_gdma_prepare(rtk_spi);
		if (ret < 0) {
			dev_err(rtk_spi->dev, "DMA transfer is not prepared for SPI%d\n", rtk_spi->spi_manage.spi_index);
			dma_params->rx_gdma_status = RTK_SPI_GDMA_UNPREPARED;
			dma_params->tx_gdma_status = RTK_SPI_GDMA_UNPREPARED;
			goto cannot_dma;
		}
		dma_params->rx_gdma_status = RTK_SPI_GDMA_PREPARED;
		dma_params->tx_gdma_status = RTK_SPI_GDMA_PREPARED;
	}

	if ((dma_params->rx_gdma_status == RTK_SPI_GDMA_UNPREPARED) || (dma_params->tx_gdma_status == RTK_SPI_GDMA_UNPREPARED)) {
		dev_err(rtk_spi->dev, "DMA has not been prepared for SPI%d\n", rtk_spi->spi_manage.spi_index);
		goto cannot_dma;
	} else if ((dma_params->rx_gdma_status == RTK_SPI_GDMA_ONGOING) || (dma_params->tx_gdma_status == RTK_SPI_GDMA_ONGOING)) {
		dev_err(rtk_spi->dev, "Last DMA is ongoing for SPI%d, please check\n", rtk_spi->spi_manage.spi_index);
		goto cannot_dma;
	}
	if (!dma_params->rx_config) {
		dma_params->rx_config = kmalloc(sizeof(*dma_params->rx_config), GFP_KERNEL);
	}
	if (!dma_params->tx_config) {
		dma_params->tx_config = kmalloc(sizeof(*dma_params->tx_config), GFP_KERNEL);
	}
	dma_params->rx_gdma_status = RTK_SPI_GDMA_ONGOING;
	dma_params->rx_config->device_fc = 1;
	dma_params->rx_dma_length = transfer->len;
	dma_params->rx_config->dst_port_window_size = 0;
	dma_params->rx_config->src_port_window_size = 0;
	dma_params->tx_gdma_status = RTK_SPI_GDMA_ONGOING;
	dma_params->tx_config->device_fc = 1;
	dma_params->tx_dma_length = transfer->len;
	dma_params->tx_config->dst_port_window_size = 0;
	dma_params->tx_config->src_port_window_size = 0;

	transfer->rx_dma = dma_map_single(rtk_spi->dev, transfer->rx_buf, transfer->len, DMA_DEV_TO_MEM);
	if (!transfer->rx_dma) {
		goto cannot_dma;
	}

	dma_params->rx_dma_addr = transfer->rx_dma;

	transfer->tx_dma = dma_map_single(rtk_spi->dev, (void *)transfer->tx_buf, transfer->len, DMA_MEM_TO_DEV);
	if (!transfer->tx_dma) {
		goto cannot_dma;
	}

	dma_params->tx_dma_addr = transfer->tx_dma;

	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_set_dma_level(rtk_spi, 1, 3);
	rtk_spi_set_sample_delay(rtk_spi, 1);
	rtk_spi_set_read_len(rtk_spi, transfer->len);
	if (!rtk_spi->spi_manage.is_slave) {
		rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_MASK_TMOD, SPI_TMOD(0));
	}

	dma_params->rx_config->src_addr = rtk_spi->spi_manage.dma_params.spi_phy_addr + SPI_DATA_FIFO_ENRTY;
	dma_params->rx_config->direction = DMA_DEV_TO_MEM;
	dma_params->rx_config->dst_addr = transfer->rx_dma;
	dma_params->rx_config->dst_port_window_size = 0;
	dma_params->rx_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_params->rx_config->src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma_params->rx_config->dst_maxburst = 1;
	dma_params->rx_config->src_maxburst = 4;

	if (rtk_spi->spi_manage.spi_index == 0) {
		dma_params->rx_config->slave_id = GDMA_HANDSHAKE_INTERFACE_SPI0_RX;
	} else {
		dma_params->rx_config->slave_id = GDMA_HANDSHAKE_INTERFACE_SPI1_RX;
	}

	dma_params->tx_config->src_addr = transfer->tx_dma;
	dma_params->tx_config->direction = DMA_MEM_TO_DEV;
	dma_params->tx_config->dst_addr = dma_params->spi_phy_addr + SPI_DATA_FIFO_ENRTY;
	dma_params->tx_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma_params->tx_config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_params->tx_config->dst_maxburst = 4;
	dma_params->tx_config->src_maxburst = 1;

	if (rtk_spi->spi_manage.spi_index == 0) {
		dma_params->tx_config->slave_id = GDMA_HANDSHAKE_INTERFACE_SPI0_TX;
	} else {
		dma_params->tx_config->slave_id = GDMA_HANDSHAKE_INTERFACE_SPI1_TX;
	}

	ret = dmaengine_slave_config(dma_params->rx_chan, dma_params->rx_config);
	if (ret < 0) {
		dev_err(rtk_spi->dev, "DMA engine slave config for RX fail\n");
		goto cannot_dma;
	}
	ret = dmaengine_slave_config(dma_params->tx_chan, dma_params->tx_config);
	if (ret < 0) {
		dev_err(rtk_spi->dev, "DMA engine slave config for TX fail\n");
		goto cannot_dma;
	}
	dma_params->rxdesc = dmaengine_prep_dma_cyclic(dma_params->rx_chan,
						 dma_params->rx_config->dst_addr,
						 dma_params->rx_dma_length, transfer->len,
						 dma_params->rx_config->direction,
						 DMA_PREP_INTERRUPT);

	dma_params->rxdesc->callback = rtk_spi_dma_rx_done_callback;
	dma_params->rxdesc->callback_param = rtk_spi;

	dmaengine_submit(dma_params->rxdesc);
	reinit_completion(&dma_params->dma_rx_completion);

	dma_async_issue_pending(dma_params->rx_chan);

	dma_params->txdesc = dmaengine_prep_dma_cyclic(dma_params->tx_chan,
						 dma_params->tx_config->src_addr,
						 dma_params->tx_dma_length, transfer->len,
						 dma_params->tx_config->direction,
						 DMA_PREP_INTERRUPT);

	dma_params->txdesc->callback = rtk_spi_dma_tx_done_callback;
	dma_params->txdesc->callback_param = rtk_spi;

	dmaengine_submit(dma_params->txdesc);
	reinit_completion(&dma_params->dma_tx_completion);

	dma_async_issue_pending(dma_params->tx_chan);

	if (rtk_spi->spi_manage.is_slave) {
		rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_BIT_SLV_OE, ~SPI_BIT_SLV_OE);
	}

	rtk_spi_enable_cmd(rtk_spi, ENABLE);

	if (rtk_spi->spi_manage.is_slave) {
		rtk_spi_set_dma_enable(rtk_spi, ENABLE, SPI_RX_MODE);
		rtk_spi_set_dma_enable(rtk_spi, ENABLE, SPI_TX_MODE);
	} else {
		rtk_spi_set_dma_enable(rtk_spi, ENABLE, SPI_TX_MODE);
		rtk_spi_set_dma_enable(rtk_spi, ENABLE, SPI_RX_MODE);
	}

	if (!rtk_spi->spi_manage.is_slave) {
		timeout = wait_for_completion_timeout(&dma_params->dma_tx_completion,
											  transfer_timeout);
		if (!timeout) {
			dev_err(rtk_spi->dev, "Timeout error in DMA TX\n");
			rtk_spi_gdma_deinit(rtk_spi);
			return -ETIMEDOUT;
		}
	} else {
		if (wait_for_completion_interruptible(&dma_params->dma_tx_completion)) {
			dev_err(rtk_spi->dev, "Interrupt error in DMA TX\n");
			rtk_spi_gdma_deinit(rtk_spi);
			return -EINTR;
		}
	}
	/* Wait for SPI busy bit to clear */
	while (rtk_spi_busy_check(rtk_spi)) {
		dev_dbg(rtk_spi->dev, "SPI is still busy\n");
		schedule_timeout_interruptible(msecs_to_jiffies(SPI_BUSBUSY_WAIT_TIMESLICE));
	}
	if (!rtk_spi->spi_manage.is_slave) {

		timeout = wait_for_completion_timeout(&dma_params->dma_rx_completion,
											  transfer_timeout);
		if (!timeout) {
			dev_err(rtk_spi->dev, "Timeout error in DMA RX\n");
			rtk_spi_gdma_deinit(rtk_spi);
			return -ETIMEDOUT;
		}
	} else {
		if (wait_for_completion_interruptible(&dma_params->dma_rx_completion)) {
			dev_err(rtk_spi->dev, "Interrupt error in DMA RX\n");
			rtk_spi_gdma_deinit(rtk_spi);
			return -EINTR;
		}
	}

	rtk_spi_gdma_deinit(rtk_spi);

	return 0;

cannot_dma:
	dev_err(rtk_spi->dev, "DMA transfer failed\n");
	rtk_spi_gdma_deinit(rtk_spi);
	return ret;
}

int rtk_spi_transfer_slave(struct rtk_spi_controller *rtk_spi,
						   struct spi_transfer *transfer, u32 transfer_len,
						   u32 transfer_done)
{
	int ret = 0;

	rtk_spi->spi_manage.transfer_status = RTK_SPI_ONGOING;
	reinit_completion(&rtk_spi->spi_manage.txrx_completion);
	rtk_spi_enable_cmd(rtk_spi, DISABLE);
	rtk_spi_set_tx_fifo_level(rtk_spi, rtk_spi->spi_param.tx_threshold_level);
	rtk_spi_set_rx_fifo_level(rtk_spi, rtk_spi->spi_param.rx_threshold_level);

	if (transfer_len < RTK_SPI_FIFO_ALL) {
		rtk_spi_set_tx_fifo_level(rtk_spi, transfer_len);
	} else {
		rtk_spi_set_tx_fifo_level(rtk_spi, RTK_SPI_FIFO_ALL - 1);
	}

	if (transfer_len <= RTK_SPI_FIFO_ALL) {
		rtk_spi_set_rx_fifo_level(rtk_spi, transfer_len - 1);
	} else {
		rtk_spi_set_rx_fifo_level(rtk_spi, RTK_SPI_FIFO_ALL - 1);
	}

	rtk_spi_set_read_len(rtk_spi, transfer_len);
	rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_BIT_SLV_OE, ~SPI_BIT_SLV_OE);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);

	rtk_spi->spi_manage.tx_info.p_data_buf = transfer->tx_buf + transfer_done;
	rtk_spi->spi_manage.tx_info.data_len = transfer_len;
	rtk_spi->spi_manage.rx_info.p_data_buf = transfer->rx_buf + transfer_done;
	rtk_spi->spi_manage.rx_info.data_len = transfer_len;

	rtk_spi_interrupt_read(rtk_spi, transfer->rx_buf + transfer_done, transfer_len);
	rtk_spi_interrupt_write(rtk_spi, transfer->tx_buf + transfer_done, transfer_len);

	rtk_spi_reg_dump(rtk_spi);

	ret = rtk_spi_wait_for_completion(rtk_spi->controller, transfer);
	if (ret) {
		dev_err(rtk_spi->dev, "Slave interrupt mode transfer failed\n");
		rtk_spi_enable_cmd(rtk_spi, DISABLE);
		return ret;
	}

	/* Wait for SPI busy bit to clear */
	while (rtk_spi_busy_check(rtk_spi)) {
		dev_dbg(rtk_spi->dev, "SPI is still busy\n");
		schedule_timeout_interruptible(msecs_to_jiffies(SPI_BUSBUSY_WAIT_TIMESLICE));
	}

	rtk_spi_reg_dump(rtk_spi);

	return ret;
}

int rtk_spi_transfer_master(struct rtk_spi_controller *rtk_spi,
							struct spi_transfer *transfer, u32 transfer_len,
							u32 transfer_done)
{
	int ret = 0;

	rtk_spi->spi_manage.transfer_status = RTK_SPI_ONGOING;
	rtk_spi_enable_cmd(rtk_spi, DISABLE);

	if (transfer_len < RTK_SPI_FIFO_ALL) {
		rtk_spi_set_tx_fifo_level(rtk_spi, transfer_len);
	} else {
		rtk_spi_set_tx_fifo_level(rtk_spi, RTK_SPI_FIFO_ALL - 1);
	}

	if (transfer_len <= RTK_SPI_FIFO_ALL) {
		rtk_spi_set_rx_fifo_level(rtk_spi, transfer_len - 1);
	} else {
		rtk_spi_set_rx_fifo_level(rtk_spi, RTK_SPI_FIFO_ALL - 1);
	}

	rtk_spi_reg_update(rtk_spi->base, SPI_CTRLR0, SPI_MASK_TMOD, SPI_TMOD(0));
	rtk_spi_set_read_len(rtk_spi, transfer_len);
	rtk_spi_enable_cmd(rtk_spi, ENABLE);

	rtk_spi->spi_manage.tx_info.p_data_buf = transfer->tx_buf + transfer_done;
	rtk_spi->spi_manage.tx_info.data_len = transfer_len;
	rtk_spi->spi_manage.rx_info.p_data_buf = transfer->rx_buf + transfer_done;
	rtk_spi->spi_manage.rx_info.data_len = transfer_len;
	if (rtk_spi->spi_manage.spi_poll_mode) {
		ret = rtk_spi_poll_trx(rtk_spi, transfer->rx_buf + transfer_done, transfer->tx_buf + transfer_done, transfer_len);
	} else {
		rtk_spi_interrupt_read(rtk_spi, transfer->rx_buf + transfer_done, transfer_len);
		rtk_spi_interrupt_write(rtk_spi, transfer->tx_buf + transfer_done, transfer_len);
		ret = rtk_spi_wait_for_completion(rtk_spi->controller, transfer);
		if (ret) {
			dev_err(rtk_spi->dev, "Master interrupt mode transfer failed\n");
			rtk_spi_enable_cmd(rtk_spi, DISABLE);
			return ret;
		}

		/* Wait for SPI busy bit to clear */
		while (rtk_spi_busy_check(rtk_spi)) {
			dev_dbg(rtk_spi->dev, "SPI is still busy\n");
			schedule_timeout_interruptible(msecs_to_jiffies(SPI_BUSBUSY_WAIT_TIMESLICE));
		}

	}
	rtk_spi_reg_dump(rtk_spi);

	return ret;
}

int rtk_spi_transfer_one(
	struct spi_controller *controller,
	struct spi_device *spi,
	struct spi_transfer *transfer)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);
	u32 fifo_chunks = 0, bytes_per_frame = 0;
	u32 fifo_entries = 0, fifo_residual = 0;
	u32 transfer_done = 0;
	int ret = -EINVAL;

	if (transfer->bits_per_word <= SPI_BITS_PER_WORD_MAX) {
		rtk_spi_set_frame_size(rtk_spi, transfer->bits_per_word - 1);
	} else {
		dev_err(rtk_spi->dev, "Invalid bits_per_word %d for SPI%d\n", transfer->bits_per_word, rtk_spi->spi_manage.spi_index);
		return -EINVAL;
	}

	if (transfer->speed_hz) {
		rtk_spi->spi_param.clock_divider = MAX_SSI_CLOCK / transfer->speed_hz + (MAX_SSI_CLOCK % transfer->speed_hz == 0 ? 0 : 1);
		if (rtk_spi->spi_param.clock_divider % 2) {
			rtk_spi->spi_param.clock_divider++;
		}
		dev_dbg(rtk_spi->dev, "Upper level given speed_hz = %d\n", transfer->speed_hz);
		dev_dbg(rtk_spi->dev, "Clock divider caculated = %d\n", rtk_spi->spi_param.clock_divider);
		dev_dbg(rtk_spi->dev, "Actual speed_hz = %d\n", MAX_SSI_CLOCK / rtk_spi->spi_param.clock_divider);
		rtk_spi_set_baud_div(rtk_spi, rtk_spi->spi_param.clock_divider);
	}

	dev_dbg(&spi->dev, "Transfer settings: bpw = %d, CPOL = %d, CPHA = %d, speed = %dHz, %s, transfer_len = %d\n",
			transfer->bits_per_word,
			rtk_spi->spi_param.sclk_polarity,
			rtk_spi->spi_param.sclk_phase,
			MAX_SSI_CLOCK / rtk_spi->spi_param.clock_divider,
			rtk_spi->spi_manage.dma_enabled ? "DMA Mode" : "Interrupt Mode",
			transfer->len);

	if (rtk_spi->spi_manage.dma_enabled) {
		return rtk_spi_do_dma_transfer(controller, spi, transfer);
	}

	bytes_per_frame = (transfer->bits_per_word > 8) ? 2 : 1;
	fifo_entries = transfer->len / bytes_per_frame;
	fifo_chunks = fifo_entries / RTK_SPI_FIFO_ALL;
	fifo_residual = fifo_entries % RTK_SPI_FIFO_ALL;

	if (rtk_spi->spi_manage.is_slave) {
		/* Slave mode - Non DMA */
		return rtk_spi_transfer_slave(rtk_spi, transfer, transfer->len, 0);
	}

	/* Master mode - Non DMA */
	while (fifo_chunks > 0) {
		ret = rtk_spi_transfer_master(rtk_spi, transfer, RTK_SPI_FIFO_ALL, transfer_done);
		if (ret) {
			dev_err(rtk_spi->dev, "SPI master transfer failed\n");
			return ret;
		} else {
			fifo_chunks -= 1;
			transfer_done += RTK_SPI_FIFO_ALL;
		}
	}
	/* Transfer residual data */
	if (fifo_residual) {
		ret = rtk_spi_transfer_master(rtk_spi, transfer, fifo_residual, transfer_done);
		if (ret) {
			dev_err(rtk_spi->dev, "SPI master transfer failed\n");
		}
	}

	return ret;
}

static void spi_init_board_info(
	struct rtk_spi_controller *rtk_spi,
	struct spi_board_info *rtk_spi_chip)
{
	strcpy(rtk_spi_chip->modalias, "spidev");
	rtk_spi_chip->chip_select = rtk_spi->spi_manage.spi_default_cs;
	rtk_spi_chip->bus_num = rtk_spi->spi_manage.spi_index;
	rtk_spi_chip->controller_data = rtk_spi->controller;
	rtk_spi_chip->max_speed_hz = 100000000;
	rtk_spi_chip->mode = 0;
	rtk_spi_chip->platform_data = rtk_spi;
}

static int rtk_spi_slave_abort(struct spi_controller *controller)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);

	if (rtk_spi->controller && rtk_spi->controller->cur_msg_prepared) {
		/* Slave transfer abort manually. */
		if (rtk_spi->spi_manage.dma_enabled) {
			rtk_spi_gdma_deinit(rtk_spi);
		}

		complete(&rtk_spi->spi_manage.txrx_completion);
	}

	return 0;
}

static void rtk_spi_handle_err(struct spi_controller *controller,
							   struct spi_message *msg)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);
	rtk_spi->spi_manage.transfer_status = RTK_SPI_DONE_WITH_ERROR;
	if (rtk_spi->controller && rtk_spi->controller->cur_msg_prepared) {
		/* Slave transfer abort manually. */
		if (rtk_spi->spi_manage.dma_enabled) {
			rtk_spi_gdma_deinit(rtk_spi);
		}

		complete(&rtk_spi->spi_manage.txrx_completion);
	}

}

/* spi.c spi_set_cs: input signal enbale: 0 means enable, 1 means disable. */
void rtk_spi_set_cs(
	struct spi_device *spi,
	bool enable)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(spi->controller);

	if (rtk_spi->spi_manage.is_slave) {
		return;
	}

	dev_dbg(rtk_spi->dev, "Set CS %s\n", !enable ? "enable" : "disable");
	if (spi->chip_select) {
		dev_warn(rtk_spi->dev, "Set CS id = %d\n", spi->chip_select);
		dev_warn(rtk_spi->dev, "The hardware slave-select line is dedicated for general SPI. One master supports only one slave actually\n");
	}

	if (!enable) {
		rtk_spi_set_slave_enable(rtk_spi, spi->chip_select);
		gpio_set_value(rtk_spi->spi_manage.spi_cs_pin, 0);
	} else {
		/* Disbale CS is not recommended, change CS id directly. */
		gpio_set_value(rtk_spi->spi_manage.spi_cs_pin, 1);
	}
}

static int rtk_spi_prepare_message(
	struct spi_controller *controller,
	struct spi_message *msg)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);
	int ret = 0;

	rtk_spi_init_hw(rtk_spi);
	rtk_spi->controller->cur_msg_prepared = 1;
	return ret;
}

static int rtk_spi_unprepare_message(
	struct spi_controller *controller,
	struct spi_message *msg)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(controller);

	if (rtk_spi->spi_manage.dma_enabled) {
		rtk_spi_gdma_deinit(rtk_spi);
	}

	if (rtk_spi->controller && rtk_spi->controller->cur_msg_prepared) {
		rtk_spi->controller->cur_msg_prepared = 0;
	}

	return 0;
}

static int rtk_spi_setup(struct spi_device *spi)
{
	struct rtk_spi_controller *rtk_spi = spi_controller_get_devdata(spi->controller);
	int ret = 0;

	/* Set CPHA & CPOL as requested by user - default is mode-0 SCPOL=0 SCPH=0 */
	rtk_spi->spi_param.sclk_polarity = spi->mode & SPI_CPOL ? SCPOL_INACTIVE_IS_HIGH : SCPOL_INACTIVE_IS_LOW;
	rtk_spi->spi_param.sclk_phase = spi->mode & SPI_CPHA ? SCPH_TOGGLES_AT_START : SCPH_TOGGLES_IN_MIDDLE;

	rtk_spi_init_hw(rtk_spi);
	return ret;
}

static int rtk_spi_probe(struct platform_device *pdev)
{
	struct device			*dev = &pdev->dev;
	struct rtk_spi_controller	*rtk_spi;
	struct spi_controller		*controller;
	struct resource			*res;
	struct device_node		*np = pdev->dev.of_node;
	int ret, status;

	rtk_spi = devm_kzalloc(&pdev->dev, sizeof(struct rtk_spi_controller), GFP_KERNEL);
	if (!rtk_spi) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtk_spi->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtk_spi->base)) {
		return PTR_ERR(rtk_spi->base);
	}

	if (!role_set_base) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		role_set_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(role_set_base)) {
			return PTR_ERR(role_set_base);
		}
	}

	rtk_spi->clk = devm_clk_get(&pdev->dev, "rtk_spi_clk");
	if (IS_ERR(rtk_spi->clk)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		return PTR_ERR(rtk_spi->clk);
	}

	ret = clk_prepare_enable(rtk_spi->clk);
	if (ret) {
		dev_err(rtk_spi->dev, "Failed to enable clock\n");
		return ret;
	}

	rtk_spi->dev = &pdev->dev;
	rtk_spi_struct_init(rtk_spi, np);

	if (rtk_spi->spi_manage.is_slave) {
		controller = spi_alloc_slave(dev, sizeof(struct rtk_spi_controller));
	} else {
		controller = spi_alloc_master(dev, sizeof(struct rtk_spi_controller));
	}

	if (!controller) {
		dev_err(&pdev->dev, "Failed to alloc SPI controller\n");
		if (rtk_spi->spi_manage.is_slave) {
			dev_err(&pdev->dev, "Please confirm SPI_SLAVE has been enabled.\n");
		}
		clk_disable_unprepare(rtk_spi->clk);
		return -ENOMEM;
	}

	rtk_spi->controller = controller;
	controller->dev.of_node = pdev->dev.of_node;
	/* the spi->mode bits understood by this driver: */
	controller->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LOOP;
	controller->bus_num = rtk_spi->spi_manage.spi_index;
	controller->dma_alignment = DMA_ALIGNMENT;
	controller->flags = SPI_MASTER_MUST_RX | SPI_MASTER_MUST_TX;
	controller->setup = rtk_spi_setup;
	controller->set_cs = rtk_spi_set_cs;
	controller->transfer_one = rtk_spi_transfer_one;
	controller->slave_abort = rtk_spi_slave_abort;
	controller->handle_err = rtk_spi_handle_err;
	controller->prepare_message = rtk_spi_prepare_message;
	controller->unprepare_message = rtk_spi_unprepare_message;
	controller->auto_runtime_pm = false;
	controller->num_chipselect = rtk_spi->spi_manage.max_cs_num;
	controller->can_dma = rtk_spi_can_dma;
	controller->max_speed_hz = 50000000;
	controller->min_speed_hz = 762;

	rtk_spi->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, rtk_spi->irq, rtk_spi_interrupt_handler, 0, dev_name(&pdev->dev), rtk_spi);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		clk_disable_unprepare(rtk_spi->clk);
		return ret;
	}

	/* Register with the SPI framework */
	platform_set_drvdata(pdev, rtk_spi);
	spi_controller_set_devdata(controller, rtk_spi);

	/* Attention!!! If spi controller is registered for kernel device, register board inifo in child-node probe. */
	if (!rtk_spi->spi_manage.spi_for_kernel) {
		/* Register board info for userspace spidev. */
		spi_init_board_info(rtk_spi, &rtk_spi->rtk_spi_chip);
		spi_register_board_info(&rtk_spi->rtk_spi_chip, 1);
	}

	status = spi_register_controller(controller);
	if (status != 0) {
		dev_err(&pdev->dev, "Failed to register SPI controller\n");
		goto out_error_controller_alloc;
	}
	dev_info(&pdev->dev, "SPI%d initialized successfully\n", controller->bus_num);

	return status;

out_error_controller_alloc:
	spi_controller_put(controller);
	clk_disable_unprepare(rtk_spi->clk);
	return status;
}

static int rtk_spi_remove(struct platform_device *pdev)
{
	struct rtk_spi_controller *rtk_spi = platform_get_drvdata(pdev);

	if (!rtk_spi) {
		return 0;
	}

	rtk_spi_enable_cmd(rtk_spi, DISABLE);

	if (rtk_spi->spi_manage.dma_enabled) {
		rtk_spi_gdma_deinit(rtk_spi);
	}

	if (rtk_spi->controller) {
		spi_unregister_controller(rtk_spi->controller);
	}

	clk_disable_unprepare(rtk_spi->clk);

	return 0;
}

static const struct of_device_id rtk_spi_of_match[] = {
	{.compatible = "realtek,ameba-spi"},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_spi_of_match);

static struct platform_driver rtk_spi_driver = {
	.driver = {
		.name	= "realtek-ameba-spi",
		.of_match_table = of_match_ptr(rtk_spi_of_match),
	},
	.probe = rtk_spi_probe,
	.remove = rtk_spi_remove,
};

module_platform_driver(rtk_spi_driver);

MODULE_DESCRIPTION("Realtek Ameba SPI driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
