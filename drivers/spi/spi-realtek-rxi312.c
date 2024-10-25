// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for Realtek RXI312 SPI Controller
 *
 * Copyright(c) 2023, Realtek Semiconductor Corp. All rights reserved.
 * Author: CTC PSP Software
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/spi-nor.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/of_device.h>

/* SPI RXI312 register offsets */
#define CTRLR0			0x000
#define RX_NDF			0x004
#define SSIENR			0x008
#define MWCR			0x00c
#define SER			0x010
#define BAUDR			0x014
#define TXFTLR			0x018
#define RXFTLR			0x01c
#define TXFLR			0x020
#define RXFLR			0x024
#define SR			0x028
#define IMR			0x02c
#define ISR			0x030
#define RISR			0x034
#define TXOICR			0x038
#define RXOICR			0x03c
#define RXUICR			0x040
#define MSTICR			0x044
#define ICR			0x048
#define DMACR			0x04c
#define DMATDLR			0x050
#define DMARDLR			0x054
#define IDR			0x058
#define SPIC_VERSION		0x05c
#define DR			0x060
#define READ_FAST_SINGLE	0x0e0
#define READ_DUAL_DATA		0x0e4
#define READ_DUAL_ADDR_DATA	0x0e8
#define READ_QUAD_DATA		0x0ec
#define READ_QUAD_ADDR_DATA	0x0f0
#define WRITE_SINGLE		0x0f4
#define WRITE_DUAL_DATA		0x0f8
#define WRITE_DUAL_ADDR_DATA	0x0fc
#define WRITE_QUAD_DATA		0x100
#define WRITE_QUAD_ADDR_DATA	0x104
#define WRITE_ENABLE		0x108
#define READ_STATUS		0x10c
#define CTRLR2			0x110
#define FBAUDR			0x114
#define USER_LENGTH		0x118
#define AUTO_LENGTH		0x11c
#define VALID_CMD		0x120
#define FLASH_SIZE		0x124
#define FLUSH_FIFO		0x128
#define DUM_BYTE		0x12c
#define TX_NDF			0x130
#define DEVICE_INFO		0x134
#define TPR0			0x138
#define AUTO_LENGTH2		0x13c
#define PGM_RST_FIFO		0x140
#define STFLR			0x1c0
#define RSVD2			0x1c4
#define PAGE_READ_AUTO		0x1d0 // warning: "PAGE_READ" redefined, renamed

/* Bit fields in CTRLR0 */
#define CTL0_SCPH		(1 << 6)
#define CTL0_SCPOL		(1 << 7)
#define CTL0_TRANSMIT_MODE	(0)
#define CTL0_RECEIVE_MODE	(3)
#define CTL0_TMOD(x)		(((x) & 0x3) << 8)
#define CTL0_ADDR_CH(ch)	(((ch) & 0x3) << 16)
#define CTL0_DATA_CH(ch)	(((ch) & 0x3) << 18)
#define CTL0_CMD_CH(ch)		(((ch) & 0x3) << 20)
#define CTL0_CK_MTIMES(x)	(((x) & 0x1f) << 23)
#define CTL0_UAR		(1 << 30)
#define CTL0_USER_MODE		(1 << 31)

/* Bit fields in SR */
#define SR_BUSY			(1 << 0)

/* Bit fields in IMR */
#define IMR_TXEIM		(1 << 0)
#define IMR_RXFIM		(1 << 4)

/* Bit fields in ISR */
#define ISR_TXEIS		(1 << 0)
#define ISR_RXFIS		(1 << 4)

/* Bit fields in SSIENR */
#define SSIENR_SPIC_EN		(1 << 0)
#define SSIENR_ATCK_CMD		(1 << 1)

/* Bit fields in BAUDR */
#define BAUD_SCKDV_WIDTH	12
#define BAUD_SCKDV_MASK		((1 << BAUD_SCKDV_WIDTH) - 1)

/* Bit fields in USER_LENGTH */
#define USER_RD_DUMMY_LENGTH(x)	((x) & 0xfff)
#define USER_CMD_LENGTH(x)	(((x) & 0x3) << 12)
#define USER_ADDR_LENGTH(x)	(((x) & 0xf) << 16)

/* Bit fields in CTRLR2 */
#define CTL2_SO_DNUM		(1 << 0)
#define CTL2_WPN_SET		(1 << 1)
#define CTL2_WPN_DNUM		(1 << 2)
#define CTL2_DR_FIXED		(1 << 3)
#define CTL2_TX_FIFO_MASK	(0xf << 4)
#define CTL2_TX_FIFO_ENTRY(x)	(((x) & 0xf) << 4)
#define CTL2_RX_FIFO_MASK	(0xf << 8)
#define CTL2_RX_FIFO_ENTRY(x)	(((x) & 0xf) << 8)

/* Bit fields in FLUSH_FIFO */
#define FLUSH_FIFO_ALL		(1 << 0)

/* Time out values (msec) */
#define REALTEK_SPI_TIMEOUT	5000

/* SPIC max slaves is 16(hw fixed value) */
#define REALTEK_SPI_MAX_SLAVES	16

#define REALTEK_SPI_DEFAULT_FIFO_SIZE	64

/* 0: single channel, 1: dual channels, 2: quad channels, 3: octal channels */
const u8 buswidth2ch[] = {
	[1] = 0,
	[2] = 1,
	[4] = 2,
	[8] = 3,
};

struct realtek_spi {
	struct platform_device *pdev;
	struct spi_master *master;
	void __iomem *regs;
	u32 clk_rate;
	u32 fifo_size;
	struct completion xfer_done;
	u32 timeout_ms;
	u32 nbytes;
	u32 offset;
	u8 *data;
};

/*
 * Inline functions for the SPI controller read/write
 */
static inline u32 spi_readl(struct realtek_spi *sdev, u32 offset)
{
	return readl_relaxed(sdev->regs + offset);
}

static inline u32 spi_readb(struct realtek_spi *sdev, u32 offset)
{
	return readb_relaxed(sdev->regs + offset);
}

static inline void spi_writel(struct realtek_spi *sdev, u32 offset, u32 val)
{
	writel_relaxed(val, sdev->regs + offset);
}

static inline void spi_writeb(struct realtek_spi *sdev, u32 offset, u8 val)
{
	writeb_relaxed(val, sdev->regs + offset);
}

/**
 * realtek_spi_wait_ready - Ensure user mode transaction is finished
 * @spi:	Pointer to the spi_device structure
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */
static int realtek_spi_wait_ready(struct realtek_spi *sdev)
{
	u32 cnt = 0, timeout;

	timeout = sdev->timeout_ms * 1000;
	while (spi_readl(sdev, SSIENR) & SSIENR_SPIC_EN) {
		udelay(1);
		if (cnt++ > timeout)
			return -EBUSY;
	}

	return 0;
}

/**
 * realtek_spi_chipselect_active - Select or deselect the chip select line
 * @spi:	Pointer to the spi_device structure
 * @active:	Select(1) or deselect (0) the chip select line
 */
static void realtek_spi_chipselect_active(struct spi_device *spi, bool active)
{
	struct realtek_spi *sdev = spi_master_get_devdata(spi->master);
	u32 reg;

	if (spi->cs_gpiod)
		return;

	reg = spi_readl(sdev, SER);
	if (active)
		reg |= 1 << spi->chip_select;
	else
		reg &= ~(1 << spi->chip_select);
	spi_writel(sdev, SER, reg);
}

/**
 * realtek_spi_prepare_cmd - Initiates SPI transfer command and address
 * @spi:	Pointer to the spi_device structure
 * @op:		the memory operation to execute
 * @offset	Offset to memory operation
 */
static void realtek_spi_prepare_cmd(struct realtek_spi *sdev,
				    const struct spi_mem_op *op, u64 offset)
{
	u8 count, i;
	u64 addr;

	/* Send cmd + addr */
	spi_writeb(sdev, DR, op->cmd.opcode);
	count = op->addr.nbytes;
	addr = op->addr.val + offset;
	for (i = 0; i < count; i++)
		spi_writeb(sdev, DR, addr >> (8 * (count - i - 1)));
}

/**
 * reaktek_spi_tx_mode() - SPI transmit transfer
 * @spi:	Pointer to the spi_device structure
 * @op:		the memory operation to execute
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */
static int realtek_spi_tx_mode(struct realtek_spi *sdev,
			       const struct spi_mem_op *op)
{
	struct platform_device *pdev = sdev->pdev;
	int timeout = sdev->timeout_ms, ret = 0;
	u32 imr, i, nbytes = op->data.nbytes;
	u8 *data = (u8 *)op->data.buf.out;

	dev_dbg(&pdev->dev, "transmit start\n");

	realtek_spi_prepare_cmd(sdev, op, 0);

	spi_writel(sdev, RX_NDF, 0);
	spi_writel(sdev, TX_NDF, nbytes);

	if (op->addr.nbytes == 0 || op->data.nbytes == 0) {
		/* Operation is not data program whose command transmit time is
		 * short, e.g., write enable, write status, erase sector, etc.
		 * So just wait spic transaction finished.
		 */
		dev_dbg(&pdev->dev, "command 0x%x\n", op->cmd.opcode);
		spi_writel(sdev, SSIENR, SSIENR_SPIC_EN);

		/* Write DR if commands with data */
		for (i = 0; i < nbytes; i++)
			spi_writeb(sdev, DR, data[i]);

		if (realtek_spi_wait_ready(sdev))
			ret = -ETIMEDOUT;
	} else {
		/* Write data to flash with interrupt enabled */
		sdev->data = (u8 *)op->data.buf.out;
		sdev->nbytes = nbytes;
		sdev->offset = 0;
		dev_dbg(&pdev->dev, "total transmit %d data\n", nbytes);

		/* Enable transmit empty interrupt, occurs when transmit FIFO
		 * entries <= TXFTLR, default is zero, change it could improve
		 * transmit performance.
		 */
		imr = spi_readl(sdev, IMR);
		spi_writel(sdev, IMR, imr | IMR_TXEIM);
		spi_writel(sdev, TXFTLR, 1);
		spi_writel(sdev, SSIENR, SSIENR_SPIC_EN);
		if (!wait_for_completion_timeout(&sdev->xfer_done,
						 msecs_to_jiffies(timeout)))
			ret = -ETIMEDOUT;

		/* Finished, restore interrupt mask */
		spi_writel(sdev, IMR, imr);
	}

	if (ret)
		dev_err(&pdev->dev, "transmit timeout\n");

	spi_writel(sdev, SSIENR, 0);

	dev_dbg(&pdev->dev, "transmit transfer end\n");

	return ret;
}

/**
 * reaktek_spi_rx_mode() - SPI receive transfer
 * @spi:	Pointer to the spi_device structure
 * @op:		the memory operation to execute
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */
static int realtek_spi_rx_mode(struct realtek_spi *sdev,
			       const struct spi_mem_op *op)
{
	struct platform_device *pdev = sdev->pdev;
	int timeout = sdev->timeout_ms, ret = 0;
	u32 fifo_entry = sdev->fifo_size;
	u32 imr, i, left, offset, nbytes = op->data.nbytes;
	u32 *data_u32 = (u32 *)op->data.buf.in;
	u8 *data_u8;

	dev_dbg(&pdev->dev, "receive transfer start\n");
	dev_dbg(&pdev->dev, "total receive %d bytes\n", nbytes);

	spi_writel(sdev, TX_NDF, 0);
	spi_writel(sdev, RXFTLR, fifo_entry - 1);
	imr = spi_readl(sdev, IMR);

	/* Make nbytes fifo size aligned, unaligned length is stored in left */
	left = nbytes % fifo_entry;
	nbytes -= left;
	offset = 0;
	while (offset < nbytes) {
		/* RXI311 tag 1.0.4: IMR/RX_NDF can't program when SSIENR is
		 * active, and spi_rxfir is cleared by hardware while reading
		 * data from FIFO, not by ICR. It's hard to decide RX_NDF value,
		 * so receive fifo size every time with interrupt, and IMR is
		 * cleard in interrupt handler.
		 */
		realtek_spi_prepare_cmd(sdev, op, offset);
		spi_writel(sdev, RX_NDF, fifo_entry);
		spi_writel(sdev, IMR, imr | IMR_RXFIM);
		spi_writel(sdev, SSIENR, SSIENR_SPIC_EN);

		dev_dbg(&pdev->dev, "address 0x%llx offset 0x%x\n",
			op->addr.val, offset);
		if (!wait_for_completion_timeout(&sdev->xfer_done,
						 msecs_to_jiffies(timeout))) {
			dev_err(&pdev->dev, "receive timeout\n");
			ret = -ETIMEDOUT;
			goto out;
		}

		for (i = 0; i < fifo_entry / 4; i++)
			data_u32[i] = spi_readl(sdev, DR);

		offset += fifo_entry;
		data_u32 += fifo_entry / 4;
	};

	/*
	 * Data read is always block size aligned, and left is zero. If left is
	 * existed and without address, e.g., read flash id, status check, etc.
	 * These operations are always time short, so just read these left data
	 * from receive fifo without interrupt enabled.
	 */
	if (left) {
		realtek_spi_prepare_cmd(sdev, op, offset);
		spi_writel(sdev, RXFTLR, left - 1);
		spi_writel(sdev, RX_NDF, left);
		if (op->addr.nbytes) {
			/* Use interrupt for last fifo size unaligned data */
			spi_writel(sdev, IMR, imr | IMR_RXFIM);
			spi_writel(sdev, SSIENR, SSIENR_SPIC_EN);

			dev_dbg(&pdev->dev, "address 0x%llx offset 0x%x\n",
				op->addr.val, offset);
			if (!wait_for_completion_timeout(
				    &sdev->xfer_done,
				    msecs_to_jiffies(timeout)))
				ret = -ETIMEDOUT;
		} else {
			/* Polling for no address operation */
			dev_dbg(&pdev->dev, "command 0x%x\n", op->cmd.opcode);
			spi_writel(sdev, SSIENR, SSIENR_SPIC_EN);
			if (realtek_spi_wait_ready(sdev))
				ret = -ETIMEDOUT;
		}

		if (ret) {
			dev_err(&pdev->dev, "receive %s timeout\n",
				op->addr.nbytes ? "polling" : "unaligned");
			goto out;
		}

		data_u8 = (u8 *)data_u32;
		for (i = 0; i < left; i++)
			data_u8[i] = spi_readb(sdev, DR);
	}

out:
	spi_writel(sdev, SSIENR, 0);
	spi_writel(sdev, IMR, imr);

	dev_dbg(&pdev->dev, "receive transfer end\n");

	return ret;
}

/**
 * reaktek_spi_exec_mem_op() - Initiates the SPI transfer
 * @mem:	the SPI memory
 * @op:		the memory operation to execute
 *
 * Executes a memory operation.
 *
 * This function first selects the chip and starts the memory operation.
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */
static int realtek_spi_exec_mem_op(struct spi_mem *mem,
				   const struct spi_mem_op *op)
{
	struct spi_device *spi = mem->spi;
	struct realtek_spi *sdev = spi_master_get_devdata(spi->master);
	u32 reg, tmod, baudr, dummy;
	int ret;

	realtek_spi_chipselect_active(spi, true);

	/* Clear all data in fifo */
	spi_writel(sdev, FLUSH_FIFO, 1);

	/* Set transfer mode and channel */
	tmod = op->data.dir == SPI_MEM_DATA_IN ? CTL0_RECEIVE_MODE :
						 CTL0_TRANSMIT_MODE;
	reg = spi_readl(sdev, CTRLR0);
	reg &= ~(CTL0_CMD_CH(3) | CTL0_ADDR_CH(3) | CTL0_DATA_CH(3) |
		 CTL0_TMOD(3));
	reg |= CTL0_CMD_CH(buswidth2ch[op->cmd.buswidth]) |
	       CTL0_ADDR_CH(buswidth2ch[op->addr.buswidth]) |
	       CTL0_DATA_CH(buswidth2ch[op->data.buswidth]) | CTL0_TMOD(tmod);
	spi_writel(sdev, CTRLR0, reg);

	/* Set USER_LENGTH */
	baudr = spi_readl(sdev, BAUDR) & BAUD_SCKDV_MASK;
	dummy = op->dummy.nbytes == 0 ?
			      0 :
			      op->dummy.nbytes * 16 * baudr / op->dummy.buswidth;
	reg = USER_CMD_LENGTH(1) | USER_ADDR_LENGTH(op->addr.nbytes) |
	      USER_RD_DUMMY_LENGTH(dummy);
	spi_writel(sdev, USER_LENGTH, reg);

	/* Transmit or receive data */
	if (op->data.dir == SPI_MEM_DATA_IN)
		ret = realtek_spi_rx_mode(sdev, op);
	else
		ret = realtek_spi_tx_mode(sdev, op);

	realtek_spi_chipselect_active(spi, false);

	return ret;
}

static const struct spi_controller_mem_ops realtek_spi_mem_ops = {
	.exec_op = realtek_spi_exec_mem_op,
};

/**
 * realtek_spi_interrupt - Interrupt service routine of the SPI controller
 * @irq:	IRQ number
 * @dev_id:	Pointer to the realtek_spi structure
 *
 * This function handles TX empty and RX full interrupts only.
 * On TX empty interrupt this function fills the TX FIFO if there is any data
 * remaining to be transferred.
 * On RX full interrupt this function indicates current transfer is completed,
 * and notify realtek_spi_rx_mode() to recive data from RX FIFO.
 *
 * Return:	IRQ_HANDLED when handled; IRQ_NONE otherwise.
 */
static irqreturn_t realtek_spi_interrupt(int irq, void *dev_id)
{
	struct realtek_spi *sdev = dev_id;
	struct platform_device *pdev = sdev->pdev;
	u32 isr, i, left, fifo_entry = sdev->fifo_size;
	u32 *data_u32;
	u8 *data_u8;

	if (!sdev)
		return IRQ_NONE;

	isr = spi_readl(sdev, ISR);
	dev_dbg(&pdev->dev, "irq: ISR %08x\n", isr);

	if (isr & ISR_TXEIS) {
		data_u8 = sdev->data + sdev->offset;
		left = sdev->nbytes - sdev->offset;

		if (left == 0) {
			/* All tx data transfer done */
			spi_writel(sdev, IMR, 0);
			complete(&sdev->xfer_done);
		} else if (left >= fifo_entry) {
			/* Transmit fifo size each time */
			data_u32 = (u32 *)data_u8;
			for (i = 0; i < fifo_entry / 4; i++)
				spi_writel(sdev, DR, data_u32[i]);
			sdev->offset += fifo_entry;
		} else {
			/* Transmit last fifo size unaligned data */
			for (i = 0; i < left; i++)
				spi_writeb(sdev, DR, data_u8[i]);
			sdev->offset += left;
		}
	} else if (isr | ISR_RXFIS) {
		spi_writel(sdev, IMR, 0);
		complete(&sdev->xfer_done);
	} else {
		dev_err(&pdev->dev, "unexpected irq: ISR %08x IMR %08x\n", isr,
			spi_readl(sdev, IMR));
		WARN_ON(1);
	}

	/* Clear interrupt */
	spi_writel(sdev, ICR, 0);

	return IRQ_HANDLED;
}

/**
 * realtek_spi_setup - Configure the SPI controller
 * @spi:	Pointer to the spi_device structure
 *
 * Sets the operational mode of SPI controller for the next SPI transfer, baud
 * rate and divisor value to setup the requested spi clock.
 *
 * Return:	0 on success and error value on failure
 */
static int realtek_spi_setup(struct spi_device *spi)
{
	struct realtek_spi *sdev = spi_master_get_devdata(spi->master);
	u32 reg, baudr;

	/* Set mode(SPI_CPHA | SPI_CPOL) */
	reg = spi_readl(sdev, CTRLR0);
	reg &= ~(CTL0_SCPH | CTL0_SCPOL);
	if (spi->mode & SPI_CPHA)
		reg |= CTL0_SCPH;
	if (spi->mode & SPI_CPOL)
		reg |= CTL0_SCPOL;

	/* Set max CK_MTIMES */
	spi_writel(sdev, CTRLR0, reg | CTL0_CK_MTIMES(0x1F));

	/* Set clock ratio: F(spi_sclk) = F(bus) / (2 * baudr) */
	baudr = DIV_ROUND_UP(sdev->clk_rate, spi->max_speed_hz * 2);
	spi_writel(sdev, BAUDR, baudr);
	spi_writel(sdev, FBAUDR, baudr);

	return 0;
}

/**
 * realtek_spi_init_hw - Initialize the hardware
 * @sdev:	Pointer to the realtek_spi structure
 */
static void realtek_spi_hw_init(struct realtek_spi *sdev)
{
	u32 reg, fifo_entry;

	/* User can't program some control register if SSIENR is enabled.
	 * So disable it before init registers
	 */
	spi_writel(sdev, SSIENR, 0);

	/* User mode */
	reg = spi_readl(sdev, CTRLR0);
	reg &= ~CTL0_UAR;
	reg |= CTL0_USER_MODE;
	spi_writel(sdev, CTRLR0, reg);

	/* Pin route & FIFO depth 2^fifo_entry Byte */
	WARN_ON(sdev->fifo_size == 0);
	fifo_entry = __ffs(sdev->fifo_size);
	reg = spi_readl(sdev, CTRLR2);
	reg &= ~(CTL2_WPN_DNUM | CTL2_TX_FIFO_MASK | CTL2_RX_FIFO_MASK);
	reg |= CTL2_TX_FIFO_ENTRY(fifo_entry) | CTL2_RX_FIFO_ENTRY(fifo_entry) |
	       CTL2_SO_DNUM;
	spi_writel(sdev, CTRLR2, reg);

	/* Disable all interrupt */
	spi_writel(sdev, ICR, 0);
	spi_writel(sdev, IMR, 0);
}

/**
 * realtek_spi_probe - Probe method for the SPI driver
 * @pdev:	Pointer to the platform_device structure
 *
 * This function initializes the driver data structures and the hardware.
 *
 * Return:	0 on success and error value on failure
 */
static int realtek_spi_probe(struct platform_device *pdev)
{
	int ret = 0, irq;
	struct device *dev = &pdev->dev;
	struct spi_master *master;
	struct realtek_spi *sdev;
	u32 val;

	/* We only support device-tree instantiation */
	if (!dev->of_node)
		return -ENODEV;

	master = spi_alloc_master(dev, sizeof(*sdev));
	if (!master) {
		dev_dbg(dev, "master allocation failed\n");
		return -ENOMEM;
	}

	master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_RX_DUAL | SPI_RX_QUAD;
	master->setup = realtek_spi_setup;
	master->mem_ops = &realtek_spi_mem_ops;
	master->dev.of_node = dev->of_node;
	master->bus_num = -1;
	master->num_chipselect = REALTEK_SPI_MAX_SLAVES;

	if (!of_property_read_u32(dev->of_node, "bus_num", &val))
		master->bus_num = val;

	if (!of_property_read_u32(dev->of_node, "num-cs", &val))
		master->num_chipselect = val;

	sdev = spi_master_get_devdata(master);
	platform_set_drvdata(pdev, sdev);

	init_completion(&sdev->xfer_done);
	sdev->timeout_ms = REALTEK_SPI_TIMEOUT;
	sdev->fifo_size = REALTEK_SPI_DEFAULT_FIFO_SIZE;
	sdev->pdev = pdev;
	sdev->master = master;
	sdev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(sdev->regs)) {
		ret = PTR_ERR(sdev->regs);
		goto err_put_master;
	}

	if (!of_property_read_u32(dev->of_node, "spi-fifo-size", &val))
		sdev->fifo_size = val;

	if (!of_property_read_u32(dev->of_node, "spi-max-frequency", &val))
		sdev->clk_rate = val;

	realtek_spi_hw_init(sdev);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		dev_warn(dev, "no IRQ resource found\n");
	else {
		ret = devm_request_irq(dev, irq, realtek_spi_interrupt,
				       IRQF_TRIGGER_NONE, pdev->name, sdev);
		if (ret) {
			dev_err(dev, "failed to register irq (%d)\n", ret);
			goto err_put_master;
		}
	}

	ret = devm_spi_register_controller(&pdev->dev, master);
	if (ret)
		dev_err(dev, "spi_register_master failed\n");

	return ret;

err_put_master:
	spi_master_put(master);

	return ret;
}

static int realtek_spi_remove(struct platform_device *pdev)
{
	struct realtek_spi *sdev;
	struct resource *mem;

	sdev = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);

	spi_writel(sdev, FLUSH_FIFO, FLUSH_FIFO_ALL);

	iounmap(sdev->regs);
	spi_unregister_master(sdev->master);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	release_mem_region(mem->start, resource_size(mem));

	return 0;
}

static const struct of_device_id realtek_spi_of_match[] = {
	{ .compatible = "realtek,spi-rxi312", },
	{ /* end of table */ }
};

MODULE_DEVICE_TABLE(of, realtek_spi_of_match);

static struct platform_driver realtek_spi_driver = {
	.probe = realtek_spi_probe,
	.remove = realtek_spi_remove,
	.driver = {
		.name = "realtek-spi",
		.of_match_table = realtek_spi_of_match,
	},
};

module_platform_driver(realtek_spi_driver);

MODULE_AUTHOR("CTC PSP Software");
MODULE_DESCRIPTION("Realtek RXI312 driver");
MODULE_LICENSE("GPL");
