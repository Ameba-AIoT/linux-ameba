// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek SPIC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/ctype.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/spi-nor.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include "spi-rxi312-nor.h"


static char *channels;
module_param(channels, charp, 0444);
MODULE_PARM_DESC(channels,
				 "spi channel mode, fast/dual/quad/qpi");

static inline u32 rts_readl(struct rts_qspi *rqspi, u32 reg)
{
	return readl(rqspi->regs + reg);
}

static inline u16 rts_readw(struct rts_qspi *rqspi, u32 reg)
{
	return readw(rqspi->regs + reg);
}

static inline u8 rts_readb(struct rts_qspi *rqspi, u32 reg)
{
	return readb(rqspi->regs + reg);
}

static inline void rts_writel(struct rts_qspi *rqspi, u32 reg, u32 val)
{
	writel(val, rqspi->regs + reg);
}

static inline void rts_writew(struct rts_qspi *rqspi, u32 reg, u16 val)
{
	writew(val, rqspi->regs + reg);
}

static inline void rts_writeb(struct rts_qspi *rqspi, u32 reg, u8 val)
{
	writeb(val, rqspi->regs + reg);
}

static void addr2cmd(int addr_width, u32 addr, u8 *cmd)
{
	cmd[1] = addr >> (addr_width * 8 - 8);
	cmd[2] = addr >> (addr_width * 8 - 16);
	cmd[3] = addr >> (addr_width * 8 - 24);
	cmd[4] = addr >> (addr_width * 8 - 32);
}

static int rts_qspi_controller_ready(struct rts_qspi *rqspi)
{
	u32 cnt;
	u32 reg;

	for (cnt = 0; cnt < 1000; cnt++) {
		reg = rts_readl(rqspi, REG_SSIENR);
		if (!(reg & BIT_SPIC_EN)) {
			return 0;
		}
		udelay(1);
	}
	return -EBUSY;
}

static inline int rts_qspi_set_dummy(struct rts_qspi *rqspi, u32 cycle)
{
	u32 baud;
	u32 dummy;
	u32 reg;
	u32 internal_dummy;

	internal_dummy = 0;

	if (cycle == 0) {
		dummy = 0;
	} else {
		baud = rts_readl(rqspi, REG_BAUDR);
		dummy = baud * cycle * 2 + internal_dummy;
	}

	if (dummy > MASK_USER_RD_DUMMY_LENGTH) {
		return -EINVAL;
	}

	reg = rts_readl(rqspi, REG_USER_LENGTH);
	reg = (reg & ~MASK_USER_RD_DUMMY_LENGTH) | dummy;
	rts_writel(rqspi, REG_USER_LENGTH, reg);

	return 0;
}

static void rts_user_mode_En(struct rts_qspi *rqspi, u8 enable)
{
	u32 reg;
	u32 BusyCheck = 0;

	/* Wait SPIC busy done before switch mode */
	do {
		BusyCheck = (rts_readl(rqspi, REG_SR) & BIT_BUSY);

		if (!BusyCheck) {
			break;
		}
	} while (1);

	reg = rts_readl(rqspi, REG_CTRLR0);

	if (enable) {
		reg |= BIT_USER_MODE;
	} else {
		reg &= ~BIT_USER_MODE;
	}
	rts_writel(rqspi, REG_CTRLR0, reg);
}

static int rts_qspi_read_xfer(struct spi_nor *nor,
							  struct spi_nor_xfer_cfg *cfg, u8 *buf, size_t len)
{
	struct rts_qspi *rqspi = nor->priv;
	int ret, cnt;
	u32 reg;

	rts_user_mode_En(rqspi, 1);

	ret = rts_qspi_set_dummy(rqspi, cfg->dummy_cycles);
	if (ret) {
		goto end;
	}

	reg = rts_readl(rqspi, REG_CTRLR0);
	reg &= ~(MASK_TMOD | MASK_ADDR_CH | MASK_DATA_CH | MASK_CMD_CH);
	reg |= TMOD(cfg->mode) | ADDR_CH(cfg->addr_pins >> 1) | DATA_CH(cfg->mode_pins >> 1) | CMD_CH(cfg->cmd_pins >> 1);
	rts_writel(rqspi, REG_CTRLR0, reg);

	if (cfg->addr_width) {
		u8 cmd[6], addr_len = cfg->addr_width;
		int cnt;

		cmd[0] = cfg->cmd;
		addr2cmd(addr_len, cfg->addr, cmd);

		if (cfg->dum_en) {
			addr_len ++;
			cmd[addr_len] = 0;
		}

		for (cnt = 0; cnt < 1 + addr_len; cnt++) {
			rts_writeb(rqspi, REG_DR, cmd[cnt]);
		}

		reg = rts_readl(rqspi, REG_USER_LENGTH);
		reg &= ~(MASK_USER_CMD_LENGHT | MASK_USER_ADDR_LENGTH);
		reg |= USER_ADDR_LENGTH(addr_len);
		reg |= USER_CMD_LENGHT(1);
		rts_writel(rqspi, REG_USER_LENGTH, reg);
	} else {
		rts_writeb(rqspi, REG_DR, cfg->cmd);
		reg = rts_readl(rqspi, REG_USER_LENGTH);
		reg &= ~(MASK_USER_CMD_LENGHT | MASK_USER_ADDR_LENGTH);
		reg |= USER_CMD_LENGHT(1);
		rts_writel(rqspi, REG_USER_LENGTH, reg);
	}

	rts_writel(rqspi, REG_TX_NDF, 0);
	rts_writel(rqspi, REG_RX_NDF, len);
	rts_writel(rqspi, REG_SSIENR, 1);

	for (cnt = 0; cnt < len;) {
		if (rts_readl(rqspi, REG_SR) & BIT_RFNE) {
			buf[cnt++] = rts_readb(rqspi, REG_DR);
		}
	}

	ret = rts_qspi_controller_ready(rqspi);
	if (ret) {
		dev_err(nor->dev, "Controller is busy\n");
		goto end;
	}

end:
	rts_user_mode_En(rqspi, 0);

	if (ret != 0) {
		dev_err(nor->dev, "Read failed: %d\n", ret);
	}

	return ret;
}

static int rts_qspi_write_xfer(struct spi_nor *nor,
							   struct spi_nor_xfer_cfg *cfg, u8 *buf, size_t len)
{
	struct rts_qspi *rqspi = nor->priv;
	int ret, cnt;
	u32 reg;

	rts_user_mode_En(rqspi, 1);

	ret = rts_qspi_set_dummy(rqspi, cfg->dummy_cycles);
	if (ret) {
		goto end;
	}

	reg = rts_readl(rqspi, REG_CTRLR0);
	reg &= ~(MASK_TMOD | MASK_ADDR_CH | MASK_DATA_CH | MASK_CMD_CH);
	reg |= TMOD(cfg->mode) | ADDR_CH(cfg->addr_pins >> 1) | DATA_CH(cfg->mode_pins >> 1) | CMD_CH(cfg->cmd_pins >> 1);
	rts_writel(rqspi, REG_CTRLR0, reg);

	/* When transmited bytes are not greater than 4, use ADDR_LENGTH
	 * to indicate non-cmd bytes. When len equals zero, we don't
	 * push data into FIFO, just ignore it.
	 */
	if (cfg->addr_width == 0) {
		/* nor->write_reg */
		rts_writeb(rqspi, REG_DR, cfg->cmd);

		reg = rts_readl(rqspi, REG_USER_LENGTH);
		reg &= ~(MASK_USER_CMD_LENGHT | MASK_USER_ADDR_LENGTH);
		reg |= USER_CMD_LENGHT(1);
		rts_writel(rqspi, REG_USER_LENGTH, reg);

	} else {
		/* nor->write */
		u8 cmd[5];
		u8 cmd_len;

		cmd[0] = cfg->cmd;
		addr2cmd(cfg->addr_width, cfg->addr, cmd);
		cmd_len = cfg->addr_width + 1;

		for (cnt = 0; cnt < cmd_len; cnt++) {
			rts_writeb(rqspi, REG_DR, cmd[cnt]);
		}

		reg = rts_readl(rqspi, REG_USER_LENGTH);
		reg &= ~(MASK_USER_CMD_LENGHT | MASK_USER_ADDR_LENGTH);
		reg |= USER_ADDR_LENGTH(cfg->addr_width);
		reg |= USER_CMD_LENGHT(1);
		rts_writel(rqspi, REG_USER_LENGTH, reg);

	}
	rts_writel(rqspi, REG_RX_NDF, 0);
	rts_writel(rqspi, REG_TX_NDF, len);
	rts_writel(rqspi, REG_SSIENR, 1);

	for (cnt = 0; cnt < len;) {
		if (rts_readl(rqspi, REG_SR) & BIT_TFNF) {
			rts_writeb(rqspi, REG_DR, buf[cnt++]);
		}
	}

	ret = rts_qspi_controller_ready(rqspi);
	if (ret) {
		dev_err(nor->dev, "Controller is busy\n");
		goto end;
	}

end:
	rts_user_mode_En(rqspi, 0);

	if (ret != 0) {
		dev_err(nor->dev, "Write failed: %d\n", ret);
	}

	return ret;
}

static int rts_qspi_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf, int len)
{
	struct spi_nor_xfer_cfg cfg;

	cfg.wren = 0;
	cfg.cmd = opcode;
	cfg.cmd_pins = 1;
	cfg.addr = 0;
	cfg.addr_width = 0;
	cfg.addr_pins = 1;
	cfg.mode = RECEIVE_MODE;
	cfg.mode_pins = 1;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = 0;
	cfg.dum_en = 0;

	if (nor->read_proto == SNOR_PROTO_4_4_4) {
		cfg.cmd_pins = 4;
		cfg.addr_pins = 4;
		cfg.mode_pins = 4;
	}

	return rts_qspi_read_xfer(nor, &cfg, buf, len);
}

static int rts_qspi_write_reg(struct spi_nor *nor, u8 opcode, u8 *buf, int len)
{
	struct spi_nor_xfer_cfg cfg;

	cfg.wren = 0;
	cfg.cmd = opcode;
	cfg.cmd_pins = 1;
	cfg.addr = 0;
	cfg.addr_width = 0;
	cfg.addr_pins = 1;
	cfg.mode = TRANSMIT_MODE;
	cfg.mode_pins = 1;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = 0;

	if (nor->write_proto == SNOR_PROTO_4_4_4) {
		cfg.cmd_pins = 4;
		cfg.addr_pins = 4;
		cfg.mode_pins = 4;
	}

	return rts_qspi_write_xfer(nor, &cfg, buf, len);
}

static ssize_t rts_qspi_read(struct spi_nor *nor, loff_t from,
							 size_t len, u_char *buf)
{
	struct spi_nor_xfer_cfg cfg;
	u8 cmd_pins, addr_pins, mode_pins;
	u8 dum_en = 0, db_cycles = 0;
	int ret;

	/* Set channel */
	switch (nor->read_proto) {
	case SNOR_PROTO_4_4_4:
		cmd_pins = 4;
		addr_pins = 4;
		mode_pins = 4;
		break;
	case SNOR_PROTO_1_4_4:
		cmd_pins = 1;
		addr_pins = 4;
		mode_pins = 4;
		dum_en = 1;
		db_cycles = 2;
		break;
	case SNOR_PROTO_1_1_4:
		cmd_pins = 1;
		addr_pins = 1;
		mode_pins = 4;
		break;
	case SNOR_PROTO_1_2_2:
		cmd_pins = 1;
		addr_pins = 2;
		mode_pins = 2;
		dum_en = 1;
		db_cycles = 4;
		break;
	case SNOR_PROTO_1_1_2:
		cmd_pins = 1;
		addr_pins = 1;
		mode_pins = 2;
		break;
	case SNOR_PROTO_1_1_1:
		cmd_pins = 1;
		addr_pins = 1;
		mode_pins = 1;
		break;
	default:
		dev_err(nor->dev, "Invalid read opcode\n");
		return -EINVAL;
	}

	cfg.wren = 0;
	cfg.cmd = nor->read_opcode;
	cfg.cmd_pins = cmd_pins;
	cfg.addr = from;
	cfg.addr_width = nor->addr_width;
	cfg.addr_pins = addr_pins;
	cfg.mode = RECEIVE_MODE;
	cfg.mode_pins = mode_pins;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = nor->read_dummy - db_cycles;
	cfg.dum_en = dum_en;

	ret = rts_qspi_read_xfer(nor, &cfg, buf, len);
	if (ret) {
		return ret;
	}

	return len;
}

static ssize_t rts_qspi_write(struct spi_nor *nor, loff_t to,
							  size_t len, const u_char *buf)
{
	struct spi_nor_xfer_cfg cfg;
	int ret;
	int cmd_pins, addr_pins, mode_pins;

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
	struct rts_qspi *rqspi = nor->priv;
	struct completion comp;
	long timeleft;

	spin_lock_irqsave(&rqspi->__lock, rqspi->__lock_flags);
	init_completion(&comp);
	rqspi->comp = &comp;
#endif

	/* set channel */
	switch (nor->write_proto) {
	case SNOR_PROTO_1_1_1:
		cmd_pins = 1;
		addr_pins = 1;
		mode_pins = 1;
		break;
	case SNOR_PROTO_1_1_4:
		cmd_pins = 1;
		addr_pins = 1;
		mode_pins = 4;
		break;
	case SNOR_PROTO_1_4_4:
		cmd_pins = 1;
		addr_pins = 4;
		mode_pins = 4;
		break;
	case SNOR_PROTO_4_4_4:
		cmd_pins = 4;
		addr_pins = 4;
		mode_pins = 4;
		break;
	default:
		dev_err(nor->dev, "Invalid page program opcode\n");
		return -EINVAL;
	}

	cfg.wren = 0;
	cfg.cmd = nor->program_opcode;
	cfg.cmd_pins = cmd_pins;
	cfg.addr = to;
	cfg.addr_width = nor->addr_width;
	cfg.addr_pins = addr_pins;
	cfg.mode = TRANSMIT_MODE;
	cfg.mode_pins = mode_pins;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = 0;

	ret = rts_qspi_write_xfer(nor, &cfg, (u_char *)buf, len);
	if (ret) {
		goto FAIL;
	}

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
	spin_unlock_irqrestore(&rqspi->__lock, rqspi->__lock_flags);
	rts_writel(rqspi, REG_TX_NDF, 0);
	rts_writel(rqspi, REG_USER_LENGTH, 0);
	rts_writel(rqspi, REG_SSIENR, BIT_SPIC_EN | BIT_ATCK_CMD);

	timeleft = wait_for_completion_interruptible_timeout(
				   rqspi->comp, msecs_to_jiffies(rqspi->timeout_ms));
	if (timeleft <= 0) {
		ret = -ETIMEDOUT;
		goto FAIL;
	}
#endif

	return len;

FAIL:
	dev_err(nor->dev, "Write failed: %d\n", ret);
	return ret;
}

static int rts_qspi_erase(struct spi_nor *nor, loff_t offs)
{
	int ret;
	u8 cmd_buf[8];

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
	struct completion comp;
	long timeleft;
	struct rts_qspi *rqspi = nor->priv;
	u32 baudr;

	spin_lock_irqsave(&rqspi->__lock, rqspi->__lock_flags);
	init_completion(&comp);
	rqspi->comp = &comp;
#endif

	cmd_buf[0] = offs >> (nor->addr_width * 8 - 8);
	cmd_buf[1] = offs >> (nor->addr_width * 8 - 16);
	cmd_buf[2] = offs >> (nor->addr_width * 8 - 24);
	cmd_buf[3] = offs >> (nor->addr_width * 8 - 32);

	ret = nor->write_reg(nor, nor->erase_opcode,
						 cmd_buf, nor->addr_width);
	if (ret) {
		goto FAIL;
	}

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
	spin_unlock_irqrestore(&rqspi->__lock, rqspi->__lock_flags);
	rts_writel(rqspi, REG_TX_NDF, 0);
	rts_writel(rqspi, REG_USER_LENGTH, 0);
	baudr = rts_readl(rqspi, REG_BAUDR);

	/* make auto check timeout longer */
	rts_writel(rqspi, REG_BAUDR, baudr * 20);
	rts_writel(rqspi, REG_SSIENR, BIT_SPIC_EN | BIT_ATCK_CMD);

	timeleft = wait_for_completion_interruptible_timeout(
				   rqspi->comp, msecs_to_jiffies(rqspi->timeout_ms));
	rts_writel(rqspi, REG_SSIENR, 0);
	rts_writel(rqspi, REG_BAUDR, baudr);

	if (timeleft <= 0) {
		ret = -ETIMEDOUT;
		goto FAIL;
	}
#endif

	return 0;

FAIL:
	dev_err(nor->dev, "Erase failed: %d\n", ret);
	return ret;
}

static int rts_qspi_setup(struct rts_qspi *rqspi)
{
	struct spi_board_info	*bi = rqspi->bi;
	u32			reg;
#if 0
	u32			speed_hz, baudr;
	struct platform_device	*pdev = rqspi->pdev;
#endif
	/* User can't program some control register if SSIENR
	 * is enabled. So disable it before init registers
	 */
	rts_writel(rqspi, REG_SSIENR, 0);	/* Disable SPIC */

	/* SPI mode */
	reg = 0;
	if (bi->mode & SPI_CPOL) {
		reg |= BIT_SCPOL;
	}
	if (bi->mode && SPI_CPHA) {
		reg |= BIT_SCPH;
	}

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
	reg |= CK_MTIMES(0x1f);
#endif

	/* SPIC stay in BUSY state when there are more than 0x1_0000 cycles between two input data.
	 * Disable DREIR to avoid that interrupt hanler time is lager than 0x1_0000 SPIC cycles.
	 */
	reg |= BIT_SPI_DREIR_R_DIS;
	rts_writel(rqspi, REG_CTRLR0, reg);
#if 0
	/* Set clock ratio
	 * F(spi_sclk) = F(bus) / (2 * baudr)
	 */
	speed_hz = bi->max_speed_hz;
	if ((speed_hz == 0) || (speed_hz > rqspi->max_speed_hz)) {
		speed_hz = rqspi->max_speed_hz;
		dev_warn(&pdev->dev, "request %d Hz, force to set %d Hz\n",
				 bi->max_speed_hz, rqspi->max_speed_hz);
	}

	if (speed_hz < rqspi->min_speed_hz) {
		dev_err(&pdev->dev, "requested speed too low %d Hz\n",
				bi->max_speed_hz);
		return -EINVAL;
	}

	baudr = DIV_ROUND_UP(rqspi->spiclk_hz, speed_hz) / 2;
	if (baudr > MASK_SCKDV) {
		dev_err(&pdev->dev, "invalid baud reg: %08X\n", baudr);
		return -EINVAL;
	}
	rts_writel(rqspi, REG_BAUDR, baudr);

	/* disable DUM_EN */
	reg = rts_readl(rqspi, REG_VALID_CMD);
	reg &= ~(BIT_DUM_EN);
	rts_writel(rqspi, REG_VALID_CMD, reg);
#endif
	/* Pin route */
	reg = rts_readl(rqspi, REG_CTRLR2);
	reg &= ~(BIT_SO_DNUM | BIT_WPN_DNUM);
	reg |= BIT_SO_DNUM;
	rts_writel(rqspi, REG_CTRLR2, reg);

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
	rts_writel(rqspi, REG_IMR, BIT_ACEIM | BIT_ACSIM);	/* Enable ACSIM interrupt */
	rqspi->timeout_ms = 5000;
#endif
#ifdef CONFIG_SPI_RTS_QUADSPI_POLLING
	rts_writel(rqspi, REG_IMR, 0);	/* Disable all interrupt */
#endif
	rts_writel(rqspi, REG_SER, 1);	/* CS actived */

	rts_writel(rqspi, REG_WRITE_SINGLE, 0xBB);

	return 0;
}

static void rts_qspi_reset(struct rts_qspi *rqspi)
{
	struct spi_nor *nor = &rqspi->nor;

	nor->write_reg(nor, SPINOR_OP_RSTEN, NULL, 0);
	nor->write_reg(nor, SPINOR_OP_RST, NULL, 0);
}

static int rts_channel_map(char *str, const char *const *map)
{
	char lower[10] = {0};
	char *p;
	int index;

	strncpy(lower, str, sizeof(lower) - 1);

	for (p = lower; *p; p++) {
		*p = tolower(*p);
	}

	index = 0;
	for (; *map; map++) {
		if (!strcmp(lower, *map)) {
			return index;
		}
		index++;
	}

	return -EINVAL;
}

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
static irqreturn_t rtsx_qspi_isr(int irq, void *dev_id)
{
	struct rts_qspi *rqspi = dev_id;
	u32 int_reg;

	if (!rqspi) {
		return IRQ_NONE;
	}

	spin_lock(&(rqspi->__lock));

	int_reg = rts_readl(rqspi, REG_ISR);
	if (!int_reg) {
		spin_unlock(&(rqspi->__lock));
		return IRQ_NONE;
	}
	rts_writel(rqspi, REG_ICR, 0); /* clear ICR */

	if (int_reg & (BIT_ACEIS | BIT_ACSIS)) {
		if (int_reg & BIT_ACEIS) {
			rqspi->auto_check_status = STATUS_TIMEOUT;
		} else {
			rqspi->auto_check_status = STATUS_OK;
		}

		if (rqspi->comp) {
			complete(rqspi->comp);
		}
	}

	spin_unlock(&(rqspi->__lock));

	return IRQ_HANDLED;
}

static int rtsx_qspi_acquire_irq(struct rts_qspi *rqspi)
{
	int err = 0;

	spin_lock_init(&rqspi->__lock);
	err = request_irq(rqspi->irq, rtsx_qspi_isr, IRQF_SHARED,
					  RTSX_QSPI_DRV_NAME, rqspi);
	if (err)
		dev_err(&(rqspi->pdev->dev), "Failed to request IRQ %d\n",
				rqspi->irq);

	return err;
}
#endif

static const struct of_device_id rts_qspi_dt_ids[] = {
	{ .compatible = "realtek,rxi312-nor" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rts_qspi_dt_ids);

static struct flash_platform_data rts_spiflash_data = {
	.name		= "m25p80",
	.type		= "mx25l12835f",
};

static struct spi_board_info rts_spi_board_info[] = {
	{
		.modalias		= "m25p80",
		.platform_data		= &rts_spiflash_data,
		.mode			= SPI_MODE_0,
		.max_speed_hz		= 100000000,
		.bus_num		= 0,
		.chip_select		= 0,
		.controller_data	= (void *)SNOR_PROTO_1_1_4,
	},
};

static int rts_qspi_probe(struct platform_device *pdev)
{
	struct resource				*res;
	struct rts_qspi				*rqspi;
	struct spi_nor				*nor;
	struct spi_board_info			*board_info;
	struct mtd_info				*mtd;
	const struct flash_platform_data	*data;
	int					irq, ret;
	u32					spiclk_hz;
	void __iomem				*regs;
	enum spi_nor_protocol			mode;
	const struct of_device_id *of_id;

	const struct spi_nor_hwcaps hwcaps = { .mask = SNOR_HWCAPS_ALL };
	struct device_node *np = pdev->dev.of_node;

	of_id = of_match_device(rts_qspi_dt_ids, &pdev->dev);
	if (!of_id) {
		return -ENODEV;
	}

	rqspi = devm_kzalloc(&pdev->dev, sizeof(*rqspi), GFP_KERNEL);
	if (!rqspi) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get resource\n");
		return -EINVAL;
	}

	regs = devm_ioremap_resource(&pdev->dev, res);
	if (!regs) {
		dev_err(&pdev->dev, "Failed to ioremap address\n");
		return -ENOMEM;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return -ENXIO;
	}

	spiclk_hz = 40000000;	//default 40M or PLL
	board_info = rts_spi_board_info;
	data = board_info->platform_data;
	if (!(data && data->type)) {
		dev_err(&pdev->dev, "Invalid flash data info\n");
		return -EINVAL;
	}
	rqspi->bi = board_info;
	mode = (int) board_info->controller_data;
	if (mode <= 0 || mode > SNOR_PROTO_4_4_4) {
		dev_err(&pdev->dev, "Invalid SPI channel info\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "spi-max-frequency",
							   &(rqspi->bi->max_speed_hz));
	if (ret) {
		dev_err(&pdev->dev, "No spi-max-frequency property in DTS\n");
		return -EINVAL;
	}

	if (channels) {
		int channel_mode;
		static const char *const channel_str[] = {
			[SNOR_PROTO_1_1_1] = "normal",
			[SNOR_PROTO_1_1_2] = "dual",
			[SNOR_PROTO_1_1_4] = "quad",
			[SNOR_PROTO_4_4_4] = "qpi",
			NULL,
		};

		channel_mode = rts_channel_map(channels, channel_str);
		if (channel_mode <= 0) {
			dev_err(&pdev->dev,
					"Invalid channel parameter: %s\n", channels);
			return -EINVAL;
		} else if (channel_mode != mode) {
			dev_info(&pdev->dev,
					 "Force to set channels from %s mode to %s mode\n",
					 channel_str[mode], channel_str[channel_mode]);
			mode = channel_mode;
		}
	}

	/* Work around for spi_nor_scan check */
	pdev->dev.platform_data = (struct flash_platform_data *)data;

	platform_set_drvdata(pdev, rqspi);
	rqspi->irq = irq;
	rqspi->regs = regs;
	rqspi->phybase = res->start;
	rqspi->use_dma = false;
	rqspi->spiclk_hz = spiclk_hz;
	rqspi->max_speed_hz = DIV_ROUND_UP(spiclk_hz, 1) / 2;
	rqspi->min_speed_hz =
		DIV_ROUND_UP(spiclk_hz, MASK_SCKDV) / 2;
	rqspi->pdev = pdev;
	nor = &rqspi->nor;
	nor->dev = &pdev->dev;
	nor->priv = rqspi;
	mtd = &nor->mtd;
	mtd->priv = nor;
#define SNOR_F_EN_SWP BIT(2)
	if (of_property_read_bool(np, "swp_enable")) {
		nor->flags |= SNOR_F_EN_SWP;
	}

#ifdef CONFIG_SPI_RTS_QUADSPI_IRQ
	ret = rtsx_qspi_acquire_irq(rqspi);
	if (ret < 0) {
		return ret;
	}
	synchronize_irq(rqspi->irq);
#endif

	/* Initialize the hardware */
	ret = rts_qspi_setup(rqspi);
	if (ret) {
		return ret;
	}

	/* Fill the hooks */
	nor->read_reg = rts_qspi_read_reg;
	nor->write_reg = rts_qspi_write_reg;
	nor->read = rts_qspi_read;
	nor->write = rts_qspi_write;
	if (!nor->erase) {
		nor->erase = rts_qspi_erase;
	}

	spi_nor_set_flash_node(nor, np->child);

	ret = spi_nor_scan(nor, data->type, &hwcaps);
	if (ret) {
		return ret;
	}

	ret = mtd_device_register(mtd, NULL, 0);
	if (ret) {
		return ret;
	}

	/* Restore platform_data */
	pdev->dev.platform_data = board_info;

	return 0;
}

static int rts_qspi_remove(struct platform_device *pdev)
{
	struct rts_qspi *rqspi = platform_get_drvdata(pdev);

	rts_qspi_reset(rqspi);

	mtd_device_unregister(&rqspi->nor.mtd);

	return 0;
}

static struct platform_driver rts_qspi_driver = {
	.driver = {
		.name	= "realtek-rxi312-nor",
		.owner	= THIS_MODULE,
		.of_match_table = rts_qspi_dt_ids,
	},
	.probe		= rts_qspi_probe,
	.remove		= rts_qspi_remove,
};
module_platform_driver(rts_qspi_driver);

MODULE_ALIAS("platform:realtek-rxi312-nor");
MODULE_DESCRIPTION("Realtek Ameba Quad SPIC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
