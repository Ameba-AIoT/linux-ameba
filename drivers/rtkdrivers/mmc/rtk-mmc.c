// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek MMC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/sizes.h>
#include <linux/of.h>

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/workqueue.h>

#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sd.h>

#include "rtk-mmc.h"

#define MMC_ENABLE 1
#define MMC_DISABLE 0

struct rtk_mmc_host {
	spinlock_t		lock;
	struct mutex		mutex;
	struct platform_device	*pdev;  /*platform device*/

	void __iomem		*ioaddr;   /*host base addr*/
	struct clk *mmc_clk;

	int			bus_width;  /*current bus width*/
	int			clock;		/* Current clock speed */
	unsigned int		max_clk;	/* Max possible freq */
	struct delayed_work	timeout_work;	/* Timer for timeouts */
	int			irq;		/* Device IRQ */

	bool use_bounce;      /*use bounce buffer?*/
	u8 *bounce_buffer;    /*dma transfer buffer*/
	u32 bounce_addr;         /*bounce buffer address */
	u32 bounce_buffer_size;        /*dma transfer length*/

	u8 dma_buf[64]__attribute__((aligned(32)));  /*dma_buf for cmd rather than data*/
	u32 dma_buf_addr;    /*dma buffer phy addr*/

	u8 sdioh_idle_level;
};



static void rtk_mmc_sdioh_dma_reset(struct mmc_host *mmc);
static u32 rtk_mmc_sdioh_check_resp(struct mmc_host *mmc);
static u8 rtk_mmc_sdioh_get_response(struct mmc_host *mmc, u8 byte_index);


/**
  * @brief  Check SDIOH is busy or not.
  */
static u32 rtk_mmc_sdioh_busy(struct mmc_host *mmc)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	struct device *dev = &host->pdev->dev;
	u8 idle_level = host->sdioh_idle_level;
	u32 tmp;
	u32 tmp_u8;

	/* Check the SD bus status. note that use register number other than macro because
	not all sdioh register is 4 bytes size but they must access 4 byte aligned on testchip
	because of some design defect*/
	tmp = readl(base + 0x584);
	tmp_u8 = SDIOH_REG_GET_8(tmp); // 0x585
	if ((tmp_u8  & idle_level) == idle_level) {
		/* Check the CMD & DATA state machine */
		if ((readb(base + SD_CMD_STATE) & SDIOH_CMD_FSM_IDLE) && (readb(base + SD_DATA_STATE) & SDIOH_DATA_FSM_IDLE)) {
			/* Check the SD card module state machine */
			if ((readb(base + SD_TRANSFER) & SDIOH_SD_MODULE_FSM_IDLE)) {
				return 0;
			} else {
				dev_err(dev, "SD card module state machine isn't in the idle state\n");
				return -EBUSY;
			}
		} else {
			dev_err(dev, "CMD or data state machine isn't in the idle state\n");
			return -EBUSY;
		}
	} else {
		dev_err(dev, "CMD or DAT[3:0] pin isn't in the idle state\n");
		return -EBUSY;
	}

}

/**
  * @brief  Check if some error occurs when SDIOH transfer.
  */
static u32 rtk_mmc_sdioh_check_tx_error(struct mmc_host *mmc, u16 *status)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 tmp;
	tmp = readl(base + 0x590);

	if (tmp & BIT(28)) {
		if (status != NULL) {
			*status = (u16)(SDIOH_REG_GET_24(readl(base + 0x580)) |
							(SDIOH_REG_GET_8(readl(base + 0x584)) << SDIOH_REG_SHIFT_8));
		}
		return -EIO;
	} else {
		return 0;
	}
}

/**
  * @brief  Wait some time for SDIOH tx done.
  */
static u32 rtk_mmc_sdioh_wait_tx_done(struct mmc_host *mmc, u32 timeout_us)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 tmp;
	u8 tmp_u8;

	do {
		tmp = readl(base + 0x590);
		tmp_u8 = SDIOH_REG_GET_24(tmp);
		tmp_u8 &= (SDIOH_TRANSFER_END | SDIOH_SD_MODULE_FSM_IDLE);
		if (tmp_u8 == (SDIOH_TRANSFER_END | SDIOH_SD_MODULE_FSM_IDLE)) {

			return 0;
		}
		udelay(1000);
	} while (timeout_us-- != 0);

	return -ETIMEDOUT;
}

/**
  * @brief  Wait some time for SDIOH DMA done.
  */
static u32 rtk_mmc_sdioh_wait_dma_done(struct mmc_host *mmc, u32 timeout_us)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	struct device *dev = &host->pdev->dev;
	u32 tmp;
	u8 tmp_u8;

	do {
		tmp = readl(base + 0x590);
		tmp_u8 = SDIOH_REG_GET_24(tmp);
		if ((tmp_u8 & SDIOH_TRANSFER_END) && (!(readl(base + DMA_CRL3) & SDIOH_DMA_XFER))) {
			rtk_mmc_sdioh_dma_reset(mmc);

			return 0;
		}

		udelay(1);
	} while (timeout_us-- != 0);

	dev_err(dev, "DMA timeout\n");
	return -ETIMEDOUT;
}

/**
  * @brief  Get SDIOH interrupt status.
  */
static u32 rtk_mmc_sdioh_get_isr(struct mmc_host *mmc)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;

	return readl(base + SD_ISR);
}

/**
  * @brief  SDIOH interrupt configure.
  */
static void rtk_mmc_sdioh_int_config(struct mmc_host *mmc, u8 SDIO_IT, u32 newState)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;

	u32 reg = readl(base + SD_ISREN);

	if (newState == MMC_ENABLE) {
		reg |= SDIO_IT;
	} else {
		reg &= ~SDIO_IT;
	}

	writel(reg, base + SD_ISREN);
}

/**
  * @brief  Clear SDIOH pending interrupt status.
  */
static void rtk_mmc_sdioh_int_clear_pending_bit(struct mmc_host *mmc, u8 SDIO_IT)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;

	writel(SDIO_IT, base + SD_ISR);
}


/**
  * @brief  Set SDIOH bus width.
  */
static void rtk_mmc_sdioh_set_bus_Width(struct mmc_host *mmc, u8 width)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 tmp;

	tmp = readl(base + 0x580);
	tmp &= ~SDIOH_MASK_BUS_WIDTH;
	tmp |= (width & SDIOH_MASK_BUS_WIDTH);

	writel(tmp, base + 0x580);
}

/**
  * @brief  Set SDIOH DMA configuration.
  */
static void rtk_mmc_sdioh_dma_config(struct mmc_host *mmc, SDIOH_DmaCtl *dma_ctl)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 tmp = 0;
	u32 temp;

	/* Set block length and count */
	if ((dma_ctl->type) == SDIOH_DMA_NORMAL) {
		/* 512 bytes (one block size) */
		temp = readl(base + 0x58C);
		temp &= ~(SDIOH_REG_MASK_24);
		writel(temp, base + 0x58C);

		temp = readl(base + 0x590);
		temp &= ~(SDIOH_REG_MASK_0);
		temp |= 0x2;
		writel(temp, base + 0x590);

	} else {
		/* 64 bytes( CMD6, R2 response...) */
		temp = readl(base + 0x58C);
		temp &= ~(SDIOH_REG_MASK_24);
		temp |= (SDIOH_C6R2_BUF_LEN << SDIOH_REG_SHIFT_24);
		writel(temp, base + 0x58C);

		temp = readl(base + 0x590);
		temp &= ~(SDIOH_REG_MASK_0);
		writel(temp, base + 0x590);
	}

	temp = readl(base + 0x590);
	temp &= (SDIOH_REG_MASK_0 | SDIOH_REG_MASK_24);
	temp |= ((((dma_ctl->blk_cnt) & SDIOH_MASK_BLOCL_CNT_L) << 8) |
			 ((((dma_ctl->blk_cnt) >> 8) & SDIOH_MASK_BLOCL_CNT_H) << 16));
	writel(temp, base + 0x590);

	/* Set the DMA control register */
	writel((dma_ctl->start_addr) & SDIOH_MASK_DRAM_SA, base + DMA_CRL1); /* DMA start address (unit: 8 Bytes) */
	writel((dma_ctl->blk_cnt) & SDIOH_MASK_DMA_LEN, base + DMA_CRL2); /* DMA transfer length (unit: 512 Bytes) */

	if ((dma_ctl->type) == SDIOH_DMA_64B) {
		tmp = SDIOH_DAT64_SEL;
	} else if ((dma_ctl->type) == SDIOH_DMA_R2) {
		tmp = SDIOH_RSP17_SEL;
	}

	tmp |= (dma_ctl->op << SDIOH_SHIFT_DDR_WR);
	writel(tmp, base + DMA_CRL3);

	/* Clear pending interrupt */
	rtk_mmc_sdioh_int_clear_pending_bit(mmc, SDIOH_DMA_TRANSFER_DONE | SDIOH_CARD_ERROR | SDIOH_CARD_END);

	/* Enable DMA transfer */
	tmp = readl(base + DMA_CRL3);
	tmp |= SDIOH_DMA_XFER;
	writel(tmp, base + DMA_CRL3);

}

/**
  * @brief  Reset SDIOH DMA configuration.
  */
static void rtk_mmc_sdioh_dma_reset(struct mmc_host *mmc)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 tmp;

	tmp = readl(base + 0x58C);
	tmp &= ~(SDIOH_REG_MASK_24);
	writel(tmp, base + 0x58C);

	tmp = readl(base + 0x590);
	tmp &= SDIOH_REG_MASK_24;
	writel(tmp, base + 0x590);

	writel(0, base + DMA_CRL1);
	writel(0, base + DMA_CRL2);
	writel(0, base + DMA_CRL3);
}

/**
  * @brief  SDIOH send command to card.
  */
static u32 rtk_mmc_sdioh_send_command(struct mmc_host *mmc, SDIOH_CmdTypeDef *cmd_attrib, u32 timeout_us)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u8 cmd = cmd_attrib->idx, cmd_code;
	u8 val0 = 0;
	int ret;
	u32 tmp;

	ret = rtk_mmc_sdioh_busy(mmc);
	if (ret != 0) {
		return ret;
	}

	/* Set SD_CONFIGURE2 (0x581) */
	val0 = SDIOH_CRC7_CAL_EN | (cmd_attrib->rsp_type);

	if (cmd_attrib->rsp_crc_chk) {
		val0 |= SDIOH_CRC7_CHK_EN;
	} else {
		val0 |= SDIOH_CRC7_CHK_DIS;
	}

	if ((cmd == MMC_READ_SINGLE_BLOCK) || (cmd == MMC_READ_MULTIPLE_BLOCK) || (cmd == MMC_SEND_TUNING_BLOCK)) {
		val0 |= SDIOH_CRC16_CHK_EN;
	} else {
		val0 |= SDIOH_CRC16_CHK_DIS;
	}

	if ((cmd == MMC_WRITE_BLOCK) || (cmd == MMC_WRITE_MULTIPLE_BLOCK)) {
		val0 |= SDIOH_WAIT_WR_CRCSTA_TO_EN | SDIOH_IGNORE_WR_CRC_ERR_EN;
	} else {
		val0 |= SDIOH_WAIT_WR_CRCSTA_TO_DIS | SDIOH_IGNORE_WR_CRC_ERR_DIS;
	}

	if (cmd == MMC_READ_DAT_UNTIL_STOP) {
		val0 |= SDIOH_WAIT_BUSY_END_DIS;
	} else {
		val0 |= SDIOH_WAIT_BUSY_END_EN;
	}

	tmp = readl(base + 0x580);
	tmp &= ~(SDIOH_REG_MASK_8);
	tmp |= (val0 << SDIOH_REG_SHIFT_8);
	writel(tmp, base + 0x580);

	/* Set SD_CONFIGURE3 (0x582) */
	val0 = SDIOH_STOP_STA_WAIT_BUSY_EN | SDIOH_SD30_CLK_STOP_DIS | SDIOH_SD20_CLK_STOP_DIS | \
		   SDIOH_SD_CMD_RESP_CHK_DIS | SDIOH_ADDR_MODE_SECTOR;

	if ((cmd == MMC_READ_MULTIPLE_BLOCK) || (cmd == MMC_WRITE_MULTIPLE_BLOCK) || (cmd == MMC_STOP_TRANSMISSION)) {
		val0 |= SDIOH_CMD_STA_WAIT_BUSY_DIS;
	} else {
		val0 |= SDIOH_CMD_STA_WAIT_BUSY_EN;
	}

	if ((cmd == MMC_READ_DAT_UNTIL_STOP) || (cmd == MMC_STOP_TRANSMISSION)) {
		val0 |= SDIOH_DATA_PHA_WAIT_BUSY_DIS;
	} else {
		val0 |= SDIOH_DATA_PHA_WAIT_BUSY_EN;
	}

	if ((cmd_attrib->rsp_type) == SDIOH_NO_RESP) {
		val0 |= SDIOH_CMD_RESP_TO_DIS;
	} else {
		val0 |= SDIOH_CMD_RESP_TO_EN;
	}


	tmp = readl(base + 0x580);
	tmp &= ~(SDIOH_REG_MASK_16);
	tmp |= (val0 << SDIOH_REG_SHIFT_16);
	writel(tmp, base + 0x580);

	tmp = readl(base + 0x588);
	tmp &= SDIOH_REG_MASK_0;
	tmp |= ((SDIOH_REG_8(HOST_COMMAND | (cmd & 0x3F))) | \
			(SDIOH_REG_16((cmd_attrib->arg) >> SDIOH_REG_SHIFT_24)) | \
			(SDIOH_REG_24((cmd_attrib->arg) >> SDIOH_REG_SHIFT_16)));
	writel(tmp, base + 0x588);

	tmp = readl(base + 0x58C);
	tmp &= SDIOH_REG_MASK_24;
	tmp |= ((((cmd_attrib->arg) >> 8) & 0xFF) | (((cmd_attrib->arg) & 0xFF) << 8) | (0x0 << 16));
	writel(tmp, base + 0x58C);

	/* Set the command code */
	switch (cmd) {
	case MMC_SWITCH:
	case MMC_SEND_STATUS:
		cmd_code = ((cmd_attrib->data_present) == SDIOH_NO_DATA) ? \
				   SDIOH_SEND_CMD_GET_RSP : SDIOH_NORMAL_READ;
		break;
	case MMC_BUS_TEST_W:
		cmd_code = SDIOH_TUNING;
		break;
	case MMC_READ_SINGLE_BLOCK:
		//case MMC_SEND_EXT_CSD:
		cmd_code = SDIOH_AUTO_READ2;
		break;
	case MMC_READ_MULTIPLE_BLOCK:
		cmd_code = SDIOH_AUTO_READ1;
		break;
	case MMC_WRITE_BLOCK:
		cmd_code = SDIOH_AUTO_WRITE2;
		break;
	case MMC_WRITE_MULTIPLE_BLOCK:
		cmd_code = SDIOH_AUTO_WRITE1;
		break;
	case SD_APP_SEND_SCR:
		cmd_code = SDIOH_NORMAL_READ;
		break;
	default:
		cmd_code = SDIOH_SEND_CMD_GET_RSP;
	}

	tmp = readl(base + 0x590);
	tmp &= 0xF0FFFFFF;
	tmp |= (cmd_code << SDIOH_REG_SHIFT_24);
	writel(tmp, base + 0x590);

	/* Start to transfer */
	tmp = readl(base + 0x590);
	tmp |= (SDIOH_START_TRANSFER << SDIOH_REG_SHIFT_24);
	writel(tmp, base + 0x590);

	if (timeout_us != 0) {
		ret = rtk_mmc_sdioh_wait_tx_done(mmc, timeout_us);
	}

	return ret;
}

/**
  * @brief  SDIOH get command response.
  */
static u8 rtk_mmc_sdioh_get_response(struct mmc_host *mmc, u8 byte_index)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 tmp;
	u8 tmp_u8 = 0;

	if ((byte_index == SDIO_RESP0) || (byte_index == SDIO_RESP1) || (byte_index == SDIO_RESP2)) {
		tmp = readl(base + 0x588);
	} else  {
		tmp = readl(base + 0x58C);
	}

	if (byte_index == SDIO_RESP0) {
		tmp_u8 = SDIOH_REG_GET_8(tmp); //0x589
	} else if (byte_index == SDIO_RESP1) {
		tmp_u8 = SDIOH_REG_GET_16(tmp); //0x58A
	} else if (byte_index == SDIO_RESP2) {
		tmp_u8 = SDIOH_REG_GET_24(tmp); //0x58B
	} else if (byte_index == SDIO_RESP3) {
		tmp_u8 = SDIOH_REG_GET_0(tmp); //0x58C
	} else if (byte_index == SDIO_RESP4) {
		tmp_u8 = SDIOH_REG_GET_8(tmp); //0x58D
	} else if (byte_index == SDIO_RESP5) {
		tmp_u8 = SDIOH_REG_GET_16(tmp); //0x58E
	}

	return tmp_u8;
}

/**
  * @brief  Switch speed of SDIOH.
  */
static void rtk_mmc_sdioh_switch_speed(struct mmc_host *mmc, u8 clk_div, u8 mode)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 value32;
	u32 tmp;

	value32 = readl(base + CKGEN_CTL);
	value32 &= ~SDIOH_MASK_CLKDIV;
	value32 |= clk_div;
	writel(value32, base + CKGEN_CTL);

	tmp = readl(base + 0x580);
	tmp &= ~SDIOH_MASK_MODE_SEL;
	tmp |= (mode << SDIOH_SHIFT_MODE_SEL);
	writel(tmp, base + 0x580);
}

/**
  * @brief  Configure SDIOH to initialization mode or not.
  */
static u32 rtk_mmc_sdioh_initial_mode_cmd(struct mmc_host *mmc, u8 NewState, u8 Level)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 value32;
	u32 ret = 0;
	u32 tmp;

	ret = rtk_mmc_sdioh_busy(mmc);
	if (ret != 0) {
		return ret;
	}

	if (NewState) {
		value32	= SDIOH_SD30_SAMP_CLK_VP1 | SDIOH_SD30_PUSH_CLK_VP0 | \
				  SDIOH_CRC_CLK_SSC | SDIOH_CLK_DIV4;
		writel(value32, base + CKGEN_CTL);

		/* SDCLK (Card Identification mode) : 390.625 KHz */
		tmp = readl(base + 0x580);
		tmp &= ~(SDIOH_REG_MASK_0);
		tmp |= ((SDIOH_SD20_MODE << SDIOH_SHIFT_MODE_SEL) | \
				SDIOH_CLK_DIV_BY_128 | SDIOH_INITIAL_MODE | SDIOH_SD30_ASYNC_FIFO_RST_N | \
				(SDIOH_BUS_WIDTH_1BIT << SDIOH_SHIFT_BUS_WIDTH));
		writel(tmp, base + 0x580);

	} else {
		/* Switch to DS mode (3.3V) or SDR12 (1.8V). SDCLK (Data Transfer mode) : 25 MHz */
		if (Level == SDIOH_SIG_VOL_18) {
			value32 = SDIOH_SD30_SAMP_CLK_VP1 | SDIOH_SD30_PUSH_CLK_VP0 | \
					  SDIOH_CRC_CLK_SSC | SDIOH_CLK_DIV4;
			writel(value32, base + CKGEN_CTL);

			tmp = readl(base + 0x580);
			tmp &= ~(SDIOH_REG_MASK_0);
			tmp |= ((SDIOH_SD30_MODE << SDIOH_SHIFT_MODE_SEL) | \
					SDIOH_CLK_DIV_BY_128 | SDIOH_SD30_ASYNC_FIFO_RST_N | \
					(SDIOH_BUS_WIDTH_1BIT << SDIOH_SHIFT_BUS_WIDTH));
			writel(tmp, base + 0x580);
		} else {
			value32 = SDIOH_SD30_SAMP_CLK_VP1 | SDIOH_SD30_PUSH_CLK_VP0 | \
					  SDIOH_CRC_CLK_SSC | (SDIOH_CLK_DIV4);
			writel(value32, base + CKGEN_CTL);

			tmp = readl(base + 0x580);
			tmp &= ~(SDIOH_REG_MASK_0);
			tmp |= ((SDIOH_SD20_MODE << SDIOH_SHIFT_MODE_SEL) | SDIOH_CLK_DIV_BY_128 | (SDIOH_BUS_WIDTH_1BIT << SDIOH_SHIFT_BUS_WIDTH));
			writel(tmp, base + 0x580);
		}
	}

	return ret;
}

/**
  * @brief  Get card plug status.
  */
static int rtk_mmc_sdioh_get_card_detect(struct mmc_host *mmc)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	struct device *dev = &host->pdev->dev;
	u8 reg = readb(base + CARD_EXIST);

	if (reg & SDIOH_SD_EXIST) {
		if (reg & SDIOH_SD_WP) {
			dev_dbg(dev, "Card detect and protect\n");
			return CARD_DETECT_PROTECT;
		} else {
			dev_dbg(dev, "Card detect\n");
			return CARD_DETECT;
		}
	} else {
		dev_dbg(dev, "Card remove\n");
		return CARD_REMOVE;
	}
}


/**
  * @brief  Initialize SDIOH peripheral.
  */
static u32 rtk_mmc_sdioh_init(struct mmc_host *mmc, u8 BusBitMode)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;
	u32 ret = 0;
	u32 tmp = 0;

	/* Set pin idle level */
	if (BusBitMode == SDIOH_BUS_WIDTH_4BIT) {
		host->sdioh_idle_level = 0x1F;
	} else {
		host->sdioh_idle_level = 0x03;
	}

	tmp = readl(base + 0x400);
	tmp &= ~(SDIOH_REG_MASK_0);
	tmp |= (SDIOH_MAP_SEL_DEC | SDIOH_DIRECT_ACCESS_SEL);
	writel(tmp, base + 0x400);

	tmp = readl(base + 0x520);
	tmp &= 0xFFFF0000;
	tmp |= SDIOH_SDMMC_INT_EN;
	writel(tmp, base + 0x520);

	tmp = readl(base + 0x50c);
	tmp &= 0xFF00FFFF;
	tmp |= (SDIOH_CARD_SEL_SD_MODULE << SDIOH_REG_SHIFT_16);
	writel(tmp, base + 0x50c);

	tmp = readl(base + 0x500);
	tmp &= 0x00FFFFFF;
	tmp |= (SDIOH_TARGET_MODULE_SD << SDIOH_REG_SHIFT_24);
	writel(tmp, base + 0x500);

	/* Initial mode */
	ret = rtk_mmc_sdioh_initial_mode_cmd(mmc, MMC_ENABLE, 0);
	if (ret != 0) {
		return ret;
	}

	tmp = readl(base + 0x528);
	tmp &= ~(SDIOH_REG_MASK_8);
	tmp |= (SDIOH_SD_CARD_MOUDLE_EN << SDIOH_REG_SHIFT_8);
	writel(tmp, base + 0x528);

	return ret;
}

/**
  * @brief  De-initialize SDIOH peripheral.
  */
static void rtk_mmc_sdioh_deinit(struct mmc_host *mmc)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	void __iomem *base = host->ioaddr;

	/* Disable interrupt & clear all pending interrupts */
	writeb(SDIOH_SDMMC_INT_PEND, base + CARD_INT_PEND);
	writeb(SDIOH_SDMMC_INT_EN, base + CARD_INT_EN);
	rtk_mmc_sdioh_int_clear_pending_bit(mmc, SDIOH_SD_ISR_ALL);
	rtk_mmc_sdioh_int_config(mmc, SDIOH_SD_ISR_ALL, MMC_DISABLE);
}


static u32 rtk_mmc_sdioh_check_resp(struct mmc_host *mmc)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	u32 ret = 0;
	u16 err_status;

	ret = rtk_mmc_sdioh_check_tx_error(mmc, &err_status);
	if (ret != 0) {
		if (err_status & SDIOH_SD_CMD_RSP_TO_ERR) {
			dev_err(dev, "Check TX error: to error\n");
			return -ETIMEDOUT;
		} else if (err_status & SDIOH_SD_TUNNING_PAT_COMP_ERR) {
			dev_err(dev, "Check TX error: comp error\n");
			return -EIO;
		} else {
			return ret;
		}
	}

	return 0;
}


static void rtk_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int ret;
	struct rtk_mmc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	SDIOH_CmdTypeDef cmd_attr;
	SDIOH_DmaCtl dma_cfg;
	u32 timeout;
	u32 data_length = 0;
	u8 resp_byte1, resp_byte2, resp_byte3, resp_byte4;

	if (!mrq) return;

	mutex_lock(&host->mutex);

	dev_dbg(dev, "MMC request code: %d\n", mrq->cmd->opcode);

	if (mrq->sbc) {
		mrq->sbc->error = 0;
	}
	if (mrq->cmd) {
		mrq->cmd->error = 0;
	}
	if (mrq->data) {
		mrq->data->error = 0;
	}
	if (mrq->stop) {
		mrq->stop->error = 0;
	}

	if (mrq->cmd && mrq->cmd->opcode == MMC_SEND_CSD) {
		/* Initial mode disable here */
		ret = rtk_mmc_sdioh_initial_mode_cmd(mmc, MMC_DISABLE, SDIOH_SIG_VOL_33);
		if (ret) {
			mrq->cmd->error = 1;
			dev_err(dev, "Initial mode disable error\n");
			goto exit;
		}
	}

	/*===================Send CMD23 if support(not support now)========================*/
	if (mrq->sbc) {
		cmd_attr.arg = mrq->sbc->arg;
		cmd_attr.idx = mrq->sbc->opcode;
		cmd_attr.rsp_type = SDIOH_RSP_6B;
		cmd_attr.rsp_crc_chk = MMC_ENABLE;
		cmd_attr.data_present = SDIOH_NO_DATA;

		ret = rtk_mmc_sdioh_send_command(mmc, &cmd_attr, mrq->sbc->busy_timeout);
		if (ret != 0) {
			mrq->sbc->error = 1;
			dev_err(dev, "Failed to send CMD23 : %d\n", ret);
			goto exit;
		}

		ret = rtk_mmc_sdioh_check_resp(mmc);
		if (ret != 0) {
			mrq->sbc->error = 1;
			dev_err(dev, "CMD23 response error: %d\n", ret);
			goto exit;
		}
	}

	/*========================DMA setting============================*/

	/* If response type is MMC_RSP_136, dedicated DMA is required, prepare for DMA */
	if (mrq->cmd->flags & MMC_RSP_136) {
		dma_cfg.op = SDIOH_DMA_READ;
		dma_cfg.blk_cnt = 1;
		dma_cfg.start_addr = ((u32)host->dma_buf_addr) / 8;
		dma_cfg.type = SDIOH_DMA_R2;

		rtk_mmc_sdioh_dma_config(mmc, &dma_cfg);
		timeout = (mrq->cmd->busy_timeout != 0) ? (mrq->cmd->busy_timeout * 1000) : 80000;

		/* If there is data transfer, dedicated DMA is required, prepare for DMA */
	} else if (mrq->data) {
		data_length = mrq->data->blksz * mrq->data->blocks;
		if (mrq->data->flags & MMC_DATA_READ) {
			dma_cfg.op = SDIOH_DMA_READ;
		} else {
			dma_cfg.op = SDIOH_DMA_WRITE;
			if (host->use_bounce == 1) {
				/* Copy scatterlist to bounce buffer */
				sg_copy_to_buffer(mrq->data->sg,
								  mrq->data->sg_len,
								  host->bounce_buffer,
								  data_length);
				dma_cfg.start_addr = host->bounce_addr / 8;

				dma_sync_single_for_device(&host->pdev->dev, host->bounce_addr,
										   host->bounce_buffer_size, DMA_BIDIRECTIONAL);
			}
		}

		/* acmd6 acmd13.. means sg_length==64 */
		if (mrq->data->sg[0].length == 64) {
			dma_cfg.type = SDIOH_DMA_64B;
		} else {
			dma_cfg.type = SDIOH_DMA_NORMAL;
		}

		dma_cfg.blk_cnt = mrq->data->blocks;

		/* If not use bounce buffer, seg number = 1 */
		if (host->use_bounce == 1) {
			dma_cfg.start_addr = host->bounce_addr / 8;
		} else {
			dma_cfg.start_addr = sg_dma_address(mrq->data->sg);
		}

		rtk_mmc_sdioh_dma_config(mmc, &dma_cfg);
	}

	/*========================Send normal cmd============================*/

	cmd_attr.arg = mrq->cmd->arg;
	cmd_attr.idx = mrq->cmd->opcode;

	if (!(mrq->cmd->flags & MMC_RSP_PRESENT)) {
		cmd_attr.rsp_type = SDIOH_NO_RESP;
	} else if (mrq->cmd->flags & MMC_RSP_136) {
		cmd_attr.rsp_type = SDIOH_RSP_17B;
	} else {
		cmd_attr.rsp_type = SDIOH_RSP_6B;
	}

	if (mrq->cmd->flags & MMC_RSP_CRC) {
		cmd_attr.rsp_crc_chk = MMC_ENABLE;
	} else {
		cmd_attr.rsp_crc_chk = MMC_DISABLE;
	}

	/* SDIOH_DATA_EXIST means there is data to receive, include acmd6, acmd13, acmd51, cmd17... */
	if (mrq->data && (mrq->data->flags & MMC_DATA_READ)) {
		cmd_attr.data_present = SDIOH_DATA_EXIST;
	} else {
		cmd_attr.data_present = SDIOH_NO_DATA;
	}

	/* Send command */
	ret = rtk_mmc_sdioh_send_command(mmc, &cmd_attr, SDIOH_CMD_CPLT_TIMEOUT);
	if (ret != 0) {
		mrq->cmd->error = 1;
		dev_err(dev, "Failed to send CMD %d: %d\n", cmd_attr.idx, ret);
		goto exit;
	}

	/*========================Request done process============================*/

	/* Use dedicated DMA to receive response info, so copy is required */
	if (mrq->cmd->flags & MMC_RSP_136) {
		ret = rtk_mmc_sdioh_wait_dma_done(mmc, timeout);
		if (ret != 0) {
			dev_err(dev, "Wait DMA done fail(MMC_RSP_136): %d\n", ret);
			mrq->cmd->error = 1;
			goto exit;
		}
		dma_sync_single_for_device(&host->pdev->dev, host->dma_buf_addr, 64, DMA_FROM_DEVICE);

		//Resp data is reverse, real data is start at dma_buf[1]
		mrq->cmd->resp[3] = host->dma_buf[13] << 24 | host->dma_buf[14] << 16 | host->dma_buf[15] << 8 | host->dma_buf[16];
		mrq->cmd->resp[2] = host->dma_buf[9] << 24 | host->dma_buf[10] << 16 | host->dma_buf[11] << 8 | host->dma_buf[12];
		mrq->cmd->resp[1] = host->dma_buf[5] << 24 | host->dma_buf[6] << 16 | host->dma_buf[7] << 8 | host->dma_buf[8];
		mrq->cmd->resp[0] = host->dma_buf[1] << 24 | host->dma_buf[2] << 16 | host->dma_buf[3] << 8 | host->dma_buf[4];
	}

	/* Use dedicated DMA to transfer data, so copy is required */
	if (mrq->data) {
		timeout = (mrq->data->timeout_ns / 1000) ? (mrq->data->timeout_ns / 1000) : 80000;
		ret = rtk_mmc_sdioh_wait_dma_done(mmc, timeout);
		if (ret != 0) {
			mrq->data->error = 1;
			dev_err(dev, "Wait DMA done fail(data): %d\n", ret);
			goto exit;
		}

		dma_sync_single_for_cpu(&host->pdev->dev, host->bounce_addr, host->bounce_buffer_size, DMA_BIDIRECTIONAL);

		if (mrq->data->flags & MMC_DATA_READ) {
			sg_copy_from_buffer(mrq->data->sg, mrq->data->sg_len,
								host->bounce_buffer,
								data_length);
		}

		mrq->data->bytes_xfered = data_length;
	}

	/*========================Check response============================*/

	/* Check response error */
	ret = rtk_mmc_sdioh_check_resp(mmc);
	if (ret != 0) {
		dev_err(dev, "Normal CMD %d response error: %d\n", mrq->cmd->opcode, ret);
		mrq->cmd->error = 1;
		goto exit;
	}

	/* Deliver response value to resp[] */
	if (!(mrq->cmd->flags & MMC_RSP_136)) {
		resp_byte1 = rtk_mmc_sdioh_get_response(mmc, SDIO_RESP1);
		resp_byte2 = rtk_mmc_sdioh_get_response(mmc, SDIO_RESP2);
		resp_byte3 = rtk_mmc_sdioh_get_response(mmc, SDIO_RESP3);
		resp_byte4 = rtk_mmc_sdioh_get_response(mmc, SDIO_RESP4);

		/* Return response information, such as cid, ocr ,cid... */
		mrq->cmd->resp[0] = resp_byte1 << 24 | resp_byte2 << 16 | resp_byte3 << 8 | resp_byte4;
	}

exit:
	mmc_request_done(mmc, mrq);
	mutex_unlock(&host->mutex);
}


static void rtk_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct rtk_mmc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	u32 clk_div, ini_clk_div, width, tmp;

	mutex_lock(&host->mutex);

	/* Set bus speed */
	if ((ios->clock != 0) && (ios->clock != host->clock)) {
		host->clock = ios->clock;

		/* clk<=400k means it is initial mode now */
		if (ios->clock <= 400000) {
			switch (ios->clock) {
			case 400000:
				clk_div = SDIOH_CLK_DIV2;
				ini_clk_div = SDIOH_CLK_DIV_BY_128;
				break;
			case 200000:
				clk_div = SDIOH_CLK_DIV2;
				ini_clk_div = SDIOH_CLK_DIV_BY_256;
				break;
			case 100000:
				clk_div = SDIOH_CLK_DIV4;
				ini_clk_div = SDIOH_CLK_DIV_BY_256;
				break;
			default:
				clk_div = SDIOH_CLK_DIV2;
				ini_clk_div = SDIOH_CLK_DIV_BY_128;
				host->clock = 400000;
				break;
			}

			tmp = readl(host->ioaddr + 0x478);
			tmp &= ~SDIOH_MASK_CLKDIV;
			tmp |= clk_div;
			writel(tmp, host->ioaddr + 0x478);

			tmp = readl(host->ioaddr + 0x580);
			tmp &= ~BIT(6);
			tmp |= ini_clk_div;
			writel(tmp, host->ioaddr + 0x580);

			/* Data transfer mode */
		} else {

			/* Calculate divider to select correct SDIOH_CLK_DIV */
			clk_div = DIV_ROUND_CLOSEST(host->max_clk, ios->clock);
			switch (clk_div) {
			case 0:
			case 1:
				clk_div = SDIOH_CLK_DIV1;
				break;
			case 2:
				clk_div = SDIOH_CLK_DIV2;
				break;
			case 3:
			case 4:
				clk_div = SDIOH_CLK_DIV4;
				break;
			default:
				clk_div = SDIOH_CLK_DIV8;
				break;
			}

			rtk_mmc_sdioh_switch_speed(mmc, clk_div, SDIOH_SD20_MODE);
		}

	}

	/* Set bus width */
	if (ios->bus_width != host->bus_width) {

		if (ios->bus_width == MMC_BUS_WIDTH_1) {
			width = SDIOH_BUS_WIDTH_1BIT;
		} else if (ios->bus_width == MMC_BUS_WIDTH_4) {
			width = SDIOH_BUS_WIDTH_4BIT;
		} else {
			dev_err(dev, "Bus width %d not support\n", ios->bus_width);
			return;
		}

		/* Set bus width */
		rtk_mmc_sdioh_set_bus_Width(mmc, width);
		host->bus_width = ios->bus_width;
	}

	mutex_unlock(&host->mutex);
}

static int rtk_mmc_get_cd(struct mmc_host *mmc)
{
	int ret;

	ret = rtk_mmc_sdioh_get_card_detect(mmc);

	if (ret == CARD_REMOVE) {
		return 0;
	} else {
		return 1;
	}
}

static int rtk_mmc_get_ro(struct mmc_host *mmc)
{
	int ret;
	ret = rtk_mmc_sdioh_get_card_detect(mmc);

	if (ret == CARD_DETECT_PROTECT) {
		return 1;
	} else {
		return 0;
	}
}


static const struct mmc_host_ops rtk_mmc_ops = {
	.request = rtk_mmc_request,
	.set_ios = rtk_mmc_set_ios,
	.get_cd = rtk_mmc_get_cd,
	.get_ro = rtk_mmc_get_ro,
};



static irqreturn_t rtk_mmc_irq(int irq, void *dev_id)
{
	struct rtk_mmc_host *host = dev_id;
	u32 tmp1, tmp2, card_status;
	void __iomem *base = host->ioaddr;
	struct mmc_host *mmc = mmc_from_priv(host);
	struct device *dev = &host->pdev->dev;

	spin_lock(&host->lock);

	tmp1 = rtk_mmc_sdioh_get_isr(mmc);
	if (tmp1) {
		rtk_mmc_sdioh_int_clear_pending_bit(mmc, tmp1);
	}

	tmp2 = readl(base + 0x520);
	if (SDIOH_REG_GET_8(tmp2) & SDIOH_SDMMC_INT_PEND) {
		tmp2 = SDIOH_REG_GET_24(readl(base + 0x51C));

		if (tmp2 & SDIOH_SD_EXIST) {
			if (tmp2 & SDIOH_SD_WP) {
				card_status = CARD_DETECT_PROTECT;
			} else {
				card_status = CARD_DETECT;
			}

			dev_dbg(dev, "Card detect\n");

		} else {
			card_status = CARD_REMOVE;

			dev_dbg(dev, "Card remove\n");
		}

		tmp2 = readl(base + 0x520);
		tmp2 |= SDIOH_SDMMC_INT_PEND;
		writel(tmp2, base + 0x520);

		mmc_detect_change(mmc, msecs_to_jiffies(200));
	}

	spin_unlock(&host->lock);

	return IRQ_HANDLED;
}


static int rtk_mmc_allocate_bounce_buffer(struct rtk_mmc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	struct device *dev = &host->pdev->dev;
	unsigned int max_blocks;
	unsigned int bounce_size;
	int ret = 0;
	bounce_size = SZ_64K;

	host->bounce_buffer = devm_kmalloc(mmc->parent, bounce_size, GFP_KERNEL);
	if (!host->bounce_buffer) {
		dev_dbg(dev, "Failed to allocate %u bytes for bounce buffer, falling back to single segments\n", bounce_size);
		mmc->max_segs = 1;
		host->use_bounce = 0;
		return -ENOMEM;
	}

	/* Get physical addr of bounce_buffer */
	host->bounce_addr = dma_map_single(mmc->parent,
									   host->bounce_buffer,
									   bounce_size,
									   DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(mmc->parent, host->bounce_addr);
	if (ret) {
		return ret;
	}

	host->bounce_buffer_size = bounce_size;
	host->use_bounce = 1;

	mmc->max_req_size = host->bounce_buffer_size;
	mmc->max_seg_size = mmc->max_req_size;

	dev_dbg(dev, "Bounce buffer addr: 0x%08X, dma addr: 0x%08X, buffer size:%d\n",
			(u32)host->bounce_buffer, (u32)host->bounce_addr, host->bounce_buffer_size);
	dev_dbg(dev, "Bounce up to %u segments into one, max segment size %u bytes\n",
			max_blocks, bounce_size);

	return ret;
}


static int rtk_mmc_add_host(struct rtk_mmc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	struct device *dev = &host->pdev->dev;
	int ret;

	mmc->f_max = host->max_clk;
	mmc->f_min = 200;

	mmc->max_busy_timeout = ~0 / (mmc->f_max / 1000);

	dev_dbg(dev, "Host params f_max = %d, f_min = %d, max_busy_timeout = %d\n",
			mmc->f_max, mmc->f_min, mmc->max_busy_timeout);

	/* Host controller capabilities */
	mmc->caps |= MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED |
				 MMC_CAP_ERASE | MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25;
	//MMC_CAP_4_BIT_DATA;// | MMC_CAP_NEEDS_POLL | MMC_CAP_CMD23;

	mmc->caps2 |= MMC_CAP2_NO_WRITE_PROTECT | MMC_CAP2_NO_SDIO | MMC_CAP2_NO_MMC;

	mmc->max_blk_size = 512;             //block size
	mmc->max_blk_count = 65535;          //0xffff of DMA_CRL2

	mmc->max_segs = 128;
	/*64k could be a good value*/
	mmc->max_req_size = SZ_64K;
	mmc->max_seg_size = mmc->max_req_size;

	/* Report supported voltage ranges */
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	ret = rtk_mmc_allocate_bounce_buffer(host);
	if (ret) {
		dev_err(dev, "Failed to alloc bounce buffer\n");
		return ret;
	}

	host->dma_buf_addr = dma_map_single(&host->pdev->dev, host->dma_buf, 64, DMA_FROM_DEVICE);

	ret = mmc_add_host(mmc);
	if (ret) {
		dev_err(dev, "Failed to add mmc host: %d\n", ret);
		return ret;
	}

	return 0;
}


static int rtk_mmc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *iomem;
	struct rtk_mmc_host *host;
	struct mmc_host *mmc;
	int ret;

	mmc = mmc_alloc_host(sizeof(*host), dev);
	if (!mmc) {
		return -ENOMEM;
	}

	mmc->ops = &rtk_mmc_ops;
	host = mmc_priv(mmc);
	host->pdev = pdev;
	spin_lock_init(&host->lock);
	mutex_init(&host->mutex);

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->ioaddr = devm_ioremap_resource(dev, iomem);
	if (IS_ERR(host->ioaddr)) {
		ret = PTR_ERR(host->ioaddr);
		goto err;
	}

	host->mmc_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(host->mmc_clk)) {
		ret =  PTR_ERR(host->mmc_clk);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(host->mmc_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable clock: %d\n", ret);
		goto err;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq <= 0) {
		ret = -EINVAL;
		dev_err(dev, "Failed to get IRQ\n");
		goto err;
	}

	ret = devm_request_irq(dev, host->irq, rtk_mmc_irq,
						   0, mmc_hostname(mmc), host);
	if (ret) {
		dev_err(dev, "Failed to request IRQ %d: %d\n", host->irq, ret);
		return ret;
	}

	ret = rtk_mmc_sdioh_init(mmc, SDIOH_BUS_WIDTH_1BIT);
	if (ret) {
		dev_err(dev, "Failed to init SDIOH: %d\n", ret);
		rtk_mmc_sdioh_deinit(mmc);
		goto err;
	}

	host->max_clk = 100000000;   //IP clk is 100M from PLL
	host->clock = 200000;        //Set SDIO CLK to 200kHz in the first
	host->bus_width = MMC_BUS_WIDTH_1;

	ret = rtk_mmc_add_host(host);
	if (ret) {
		dev_err(dev, "Failed to add host: %d\n", ret);
		goto err;
	}
	dev_info(dev, "MMC init done\n");

	platform_set_drvdata(pdev, host);

	return 0;

err:
	mmc_free_host(mmc);

	return ret;
}


static const struct of_device_id rtk_mmc_match[] = {
	{ .compatible = "realtek,ameba-sdiohost" },
	{ }
};

MODULE_DEVICE_TABLE(of, rtk_mmc_match);

static struct platform_driver rtk_mmc_driver = {
	.probe      = rtk_mmc_probe,
	.driver     = {
		.name		= "realtek-ameba-sdiohost",
		.of_match_table	= rtk_mmc_match,
	},
};

builtin_platform_driver(rtk_mmc_driver);

MODULE_DESCRIPTION("Realtek Ameba MMC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
