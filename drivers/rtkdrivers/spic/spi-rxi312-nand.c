// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek SPIC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/spi-nor.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/of_device.h>

/* TP test enable */
#define SPIC_TP_TEST_EN					0

/* Register definitions */
#define BIT_SCPOL						((u32)0x00000001 << 7)          /* R/W SCPOL_DEF  Indicate serial clock polarity. It is used to select the polarity of the inactive serial clock. 0: inactive state of serial clock is low 1: inactive state of serial clock is high */
#define BIT_SCPH						((u32)0x00000001 << 6)          /* R/W SCPH_DEF  Indicate serial clock phase. The serial clock phase selects the relationship the serial clock with the slave select signal. 0: serial clock toggles in middle of first data bit 1: serial clock toggles at start of first data bit */

#define BIT_ATWR_RDSR_N					((u32)0x00000001 << 8)          /* R/W 0x0  The previous auto write cmd didn't check the status register (RDSR). User should check the status register of Flash before next user mode transaction. ATWR_RDSR_N will only be set by SPIC when SEQ_WR_EN = 1. */
#define BIT_BOOT_FIN					((u32)0x00000001 << 7)          /* R 0x0  (Not Yet Ready) Boot Finish. Set if count waiting cycles (Boot Delay Count) for SPI Flash becomes a stable state after power on (or system reset). 1: Boot Finish Note: Auto_mode would be blocked until boot finish. User_mode is allowed with SSIENR inactive before boot finish. */
#define BIT_DCOL						((u32)0x00000001 << 6)          /* R 0x0  Data Collision, or in Transmitting Status. 1: Status shows that SPIC is transmitting spi_flash_cmd/spi_flash_addr/spi_flash_data to SPI Flash. Suggest not reading DR during this transmitting state. (Check this status can avoid reading wrong data and cause SPIC error.) */
#define BIT_TXE							((u32)0x00000001 << 5)          /* R 0x0  Transmission error. Set if FIFO is empty and starting to transmit data to SPI Flash. This bit is cleared when read. */
#define BIT_RFF							((u32)0x00000001 << 4)          /* R 0x0  Receive FIFO full. Set if FIFO is full in receiving mode. This bit is cleared when read. */
#define BIT_RFNE						((u32)0x00000001 << 3)          /* R 0x0  Receive FIFO is not empty. Set if FIFO is not empty in receiving mode. This bit is cleared when read. */
#define BIT_TFE							((u32)0x00000001 << 2)          /* R 0x1  Transmit FIFO is empty. Set if FIFO is empty in transmit mode, else it is cleared when it has data in FIFO. */
#define BIT_TFNF						((u32)0x00000001 << 1)          /* R 0x1  Transmit FIFO is not full. Set if FIFO is not full in transmit mode, else it is cleared when FIFO is full. */
#define BIT_BUSY						((u32)0x00000001 << 0)

#define BIT_RD_QUAD_IO					((u32)0x00000001 << 4)          /* R/W VALID_CMD_DEF[4]  Indicate quad address/data read is a valid command to execute. (known as (1-4-4)) */
#define BIT_RD_QUAD_O					((u32)0x00000001 << 3)          /* R/W VALID_CMD_DEF[3]  Indicate quad data read is a valid command to execute. (known as (1-1-4)) */
#define BIT_RD_DUAL_IO					((u32)0x00000001 << 2)          /* R/W VALID_CMD_DEF[2]  Indicate dual address/data read is a valid command to execute. (known as (1-2-2)) */
#define BIT_RD_DUAL_I					((u32)0x00000001 << 1)          /* R/W VALID_CMD_DEF[1]  Indicate dual data read is a valid command to execute. (known as (1-1-2)) */
#define BIT_FRD_SINGLE					((u32)0x00000001 << 0)          /* R/W VALID_CMD_DEF[0]  Indicate fast read command is a valid command to execute. (known as (1-1-1)) */

#define BIT_USER_MODE					((u32)0x00000001 << 31)
#define USER_RD_DUMMY_LENGTH(x)			((u32)(((x) & 0x00000FFF) << 0))
#define MASK_USER_RD_DUMMY_LENGTH		((u32)0x00000FFF << 0)
#define MASK_USER_ADDR_LENGTH			((u32)0x0000000F << 16)          /* R/W 0x3  Indicate number of bytes in address phase (between command phase and write/read phase) in user mode. If it is set to 4, it will transmit 4-byte Address to support 4-byte address mode in SPI Flash. */
#define USER_ADDR_LENGTH(x)				((u32)(((x) & 0x0000000F) << 16))
#define CMD_CH(x)						((u32)(((x) & 0x00000003) << 20))
#define ADDR_CH(x)						((u32)(((x) & 0x00000003) << 16))
#define DATA_CH(x)						((u32)(((x) & 0x00000003) << 18))
#define SPIC_VALID_CMD_MASK				(0x7fff)
#define TMOD(x)							((u32)(((x) & 0x00000003) << 8))
#define TX_NDF(x)						((u32)(((x) & 0x00FFFFFF) << 0))
#define BIT_TFNF						((u32)0x00000001 << 1)
#define BIT_SPIC_EN						((u32)0x00000001 << 0)
#define MASK_RX_NDF                		((u32)0x00FFFFFF << 0)			/* R/W 0x0  Indicate a number of data frames (unit: Byte). When executing receives operation in user mode, SPIC receives data continuously until data frames are equal to RX_NDF. */
#define RX_NDF(x)                  		((u32)(((x) & 0x00FFFFFF) << 0))
#define GET_RX_NDF(x)              		((u32)(((x >> 0) & 0x00FFFFFF)))
#define MASK_SCKDV                 		((u32)0x00000FFF << 0)          /* R/W Check SPIC frequency in configure form * *: (spic frequency) / 32 ex: spic freq=120 MHz, SCKDV= ⌊120/32⌋ = ⌊3.75⌋ =3  Define spi_sclk divider value. The frequency of spi_sclk = The frequency of bus_clk / (2*SCKDV). */
#define SCKDV(x)                   		((u32)(((x) & 0x00000FFF) << 0))
#define GET_SCKDV(x)               		((u32)(((x >> 0) & 0x00000FFF)))

/* NOR Flash opcodes */
#define FLASH_CMD_WREN					0x06        /* Write enable */
#define FLASH_CMD_WRDI					0x04        /* Write disable */
#define FLASH_CMD_WRSR					0x01        /* Write status register */
#define FLASH_CMD_RDID					0x9F        /* Read idenfication */
#define FLASH_CMD_RDSR					0x05        /* Read status register */
#define FLASH_CMD_RDSR2					0x35        /* Read status register-2 */
#define FLASH_CMD_READ					0x03        /* Read data */
#define FLASH_CMD_FREAD					0x0B        /* Fast read data */
#define FLASH_CMD_RDSFDP				0x5A        /* Read SFDP */
#define FLASH_CMD_RES					0xAB        /* Read Electronic ID */
#define FLASH_CMD_REMS					0x90        /* Read Electronic Manufacturer & Device ID */
#define FLASH_CMD_DREAD					0x3B        /*/ Double Output Mode command */
#define FLASH_CMD_SE					0x20        /* Sector Erase */
#define FLASH_CMD_BE					0xD8        /* 0x52 //64K Block Erase */
#define FLASH_CMD_CE					0x60        /* Chip Erase(or 0xC7) */
#define FLASH_CMD_PP					0x02        /* Page Program */
#define FLASH_CMD_DP					0xB9        /* Deep Power Down */
#define FLASH_CMD_RDP					0xAB        /* Release from Deep Power-Down */
#define FLASH_CMD_2READ					0xBB        /* 2 x I/O read  command */
#define FLASH_CMD_4READ					0xEB        /* 4 x I/O read  command */
#define FLASH_CMD_QREAD					0x6B        /* 1I / 4O read command */
#define FLASH_CMD_4PP					0x32        /* Quad page program, not for MXIC */
#define FLASH_CMD_FF					0xFF        /* Release Read Enhanced */
#define FLASH_CMD_REMS2					0x92        /* Read ID for 2x I/O mode, not for MXIC */
#define FLASH_CMD_REMS4					0x94        /* Read ID for 4x I/O mode, not for MXIC */
#define FLASH_CMD_RDSCUR				0x48        /* Read security register, not for MXIC */
#define FLASH_CMD_WRSCUR				0x42        /* Write security register, not for MXIC */
#define FLASH_CM_ERSCUR					0x44		/* Erase security register, not for MXIC */
#define FLASH_CM_RDUID					0x4B		/* Read unique ID, not for MXIC */

/* NAND Flash opcodes */
#define NAND_CMD_WREN					0x06        /* Write enable */
#define NAND_CMD_WRDI					0x04        /* Write disable */
#define NAND_CMD_RDID					0x9F        /* Read idenfication */
#define NAND_CMD_WRSR					0x1F        /* Set feature, write status register */
#define NAND_CMD_RDSR					0x0F        /* Get feature, read status register */

#define NAND_CMD_READ					0x03        /* Read data */
#define NAND_CMD_FREAD					0x0B        /* Fast read data */
#define NAND_CMD_DREAD					0x3B        /* Double Output Mode command	1-1-2 */
#define NAND_CMD_2READ					0xBB        /* 2 x I/O read  command	1-2-2, not for MXIC */
#define NAND_CMD_QREAD					0x6B        /* 1I / 4O read command		1-1-4 */
#define NAND_CMD_4READ					0xEB        /* 4 x I/O read  command	1-4-4, not for MXIC */

#define NAND_CMD_RESET					0xFF        /* Device Reset */
#define NAND_CMD_PAGERD					0x13        /* Page Read */
#define NAND_CMD_PROMEXEC				0x10        /* Program Execute */
#define NAND_CMD_BE						0xD8        /* Block Erase */

#define NAND_CMD_PP						0x02        /* Page load */
#define NAND_CMD_PP_RANDOM				0x84        /* Page load random (do not reset buffer) */
#define NAND_CMD_QPP					0x32        /* Quad page load 1-1-4 */
#define NAND_CMD_QPP_RANDOM				0x34        /* Quad page load random 1-1-4 (do not reset buffer) */

/* Transfer mode */
#define RXI312_SPI_TMODE_TX				0
#define RXI312_SPI_TMODE_RX				3

/* FIFO length */
#define RXI312_FIFO_LENGTH				32

/* Wait status */
#define WAIT_SPIC_BUSY					0
#define WAIT_FLASH_BUSY					1
#define WAIT_WRITE_DONE					2
#define WAIT_WRITE_EN					3
#define WAIT_TRANS_COMPLETE				4
#define WAIT_WRITE_DISABLE				3

/* Get unaligned buffer size */
#define UNALIGNED32(buf)	((4 - ((u32)(buf) & 0x3)) & 0x3)

/* RXI312 SPI data structure */
struct rxi312_spi {
	struct spi_master *master;
	void __iomem *user_base;
	struct mutex lock;
};

/* SPI register map */
struct rxi312_spi_map {
	volatile u32 ctrlr0;    	/* Control Reg 0				(0x000) */
	volatile u32 rx_ndf;
	volatile u32 ssienr;    	/* SPIC enable Reg1				(0x008) */
	volatile u32 rsvd0;
	volatile u32 ser;       	/* Slave enable Reg				(0x010) */
	volatile u32 baudr;
	volatile u32 txftlr;    	/* TX_FIFO threshold level		(0x018) */
	volatile u32 rxftlr;
	volatile u32 txflr;     	/* TX_FIFO threshold level		(0x020) */
	volatile u32 rxflr;
	volatile u32 sr;        	/* Destination Status Reg		(0x028) */
	volatile u32 imr;
	volatile u32 isr;       	/* Interrupt Stauts Reg			(0x030) */
	volatile u32 risr;
	volatile u32 txoicr;    	/* TX_FIFO overflow_INT clear	(0x038) */
	volatile u32 rxoicr;
	volatile u32 rxuicr;    	/* RX_FIFO underflow_INT clear	(0x040) */
	volatile u32 msticr;
	volatile u32 icr;       	/* Interrupt clear Reg			(0x048) */
	volatile u32 dmacr;
	volatile u32 dmatdlr;   	/* DMA TX_data level			(0x050) */
	volatile u32 dmardlr;
	volatile u32 idr;       	/* Identiation Scatter Reg		(0x058) */
	volatile u32 spi_flash_version;
	union {
		volatile u8  byte;
		volatile u16 half;
		volatile u32 word;
	} dr[32];
	volatile u32 rd_fast_single;
	volatile u32 rd_dual_o; 	/* Read dual data cmd Reg		(0x0e4) */
	volatile u32 rd_dual_io;
	volatile u32 rd_quad_o; 	/* Read quad data cnd Reg		(0x0ec) */
	volatile u32 rd_quad_io;
	volatile u32 wr_single; 	/* write single cmd Reg			(0x0f4) */
	volatile u32 wr_dual_i;
	volatile u32 wr_dual_ii;	/* write dual addr/data cmd		(0x0fc) */
	volatile u32 wr_quad_i;
	volatile u32 wr_quad_ii;	/* write quad addr/data cnd		(0x104) */
	volatile u32 wr_enable;
	volatile u32 rd_status; 	/* read status cmd Reg			(0x10c) */
	volatile u32 ctrlr2;
	volatile u32 fbaudr;    	/* fast baud rate Reg			(0x114) */
	volatile u32 user_length;
	volatile u32 auto_length; 	/* Auto addr length Reg			(0x11c) */
	volatile u32 valid_cmd;
	volatile u32 flash_size; 	/* Flash size Reg				(0x124) */
	volatile u32 flush_fifo;
	volatile u32 dum_byte;		/* Dummy byte value Reg			(0x12C) */
	volatile u32 tx_ndf;		/* TX_NDF Register				(0x130) */
	volatile u32 device_info;	/* Device info Register			(0x134) */
	volatile u32 tpr0;			/* Timing parameter Reg			(0x138) */
	volatile u32 auto_length2;	/* Auto addr length 2 Reg		(0x13C) */
	volatile u32 rsvd1[16];		/* Reserved						(0x140-0x17C) */
	union {
		volatile u8  byte;
		volatile u16 half;
		volatile u32 word;
	} st_dr[16];
	volatile  u32 stflr;		/* Status FIFO level Reg		(0x1C0) */
	volatile u32 rsvd2[3];		/* Reserved						(0x1C4-0x1CC) */
	volatile u32 page_read;
};

/* Get CMD/ADDR/DATA channel from bitwidth */
static u8 rxi312_spi_get_ch(u8 bitwidth)
{
	u8 ch = 0;
	switch (bitwidth) {
	case 2:
		ch = 1;
		break;
	case 4:
		ch = 2;
		break;
	case 8:
		ch = 3;
		break;
	default:
		break;
	}
	return ch;
}

/* Wait for SPI status */
static void rxi312_spi_waitbusy(struct rxi312_spi_map *map, u32 wait_type)
{
	u32 busy_check = 0;

	do {
		if (wait_type == WAIT_SPIC_BUSY) {
			busy_check = (map->sr & BIT_BUSY);

		} else if (wait_type == WAIT_TRANS_COMPLETE) {
			/* When transfer completes, SSIENR.SPIC_EN are cleared by HW automatically. */
			busy_check = (map->ssienr & BIT_SPIC_EN);
		}

		if (!busy_check) {
			break;
		}
	} while (1);
}

/* Enter/Exit user mode */
static void rxi312_spi_usermode_en(struct rxi312_spi_map *map, u8 enable)
{
	/* Wait SPIC busy done before switch mode */
	rxi312_spi_waitbusy(map, WAIT_SPIC_BUSY);

	if (enable) {
		map->ctrlr0 |= BIT_USER_MODE;
	} else {
		map->ctrlr0 &= ~BIT_USER_MODE;
	}
}

/* Callback for spi_controller_mem_ops->exec_op */
static int rxi312_spi_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	struct rxi312_spi *spi = spi_controller_get_devdata(mem->spi->master);
	struct rxi312_spi_map *map = spi->user_base;
	int i;
	int j;
	int ret = 0;
	u8 tmod = RXI312_SPI_TMODE_TX;
	u8 *buf;
	u32 *aligned32_buf;
	u32 ctrl0;
	u32 value;
	u32 nbytes;
	u32 unaligned32_bytes;
	u32 ctrl0_bak;
	u32 user_length_bak;
	u32 fifo_level;
	unsigned long flags;

#if SPIC_TP_TEST_EN
	u64 total_ns;
	u64 umode_en_ns;
	u64 read_ns;
	u64 write_ns;
	u64 read_wait_ns;
	u64 write_wait_ns;
	u64 umode_dis_ns;

	total_ns = ktime_get_raw_ns();
#endif

	mutex_lock(&spi->lock);
	local_irq_save(flags);

	nbytes = op->data.nbytes;
	ctrl0_bak = map->ctrlr0;
	user_length_bak = map->user_length;

	if ((nbytes) && (op->data.dir == SPI_MEM_DATA_IN)) {
		tmod = RXI312_SPI_TMODE_RX;
	}

#if SPIC_TP_TEST_EN
	umode_en_ns = ktime_get_raw_ns();
#endif
	rxi312_spi_usermode_en(map, 1);
#if SPIC_TP_TEST_EN
	umode_en_ns = ktime_get_raw_ns() - umode_en_ns;
#endif

	ctrl0 = map->ctrlr0;
	ctrl0 &= ~(TMOD(3) | CMD_CH(3) | ADDR_CH(3) | DATA_CH(3));
	ctrl0 |= TMOD(tmod);;
	if (op->cmd.buswidth > 0) {
		ctrl0 |= CMD_CH(rxi312_spi_get_ch(op->cmd.buswidth));
	}
	if (op->addr.buswidth > 0) {
		ctrl0 |= ADDR_CH(rxi312_spi_get_ch(op->addr.buswidth));
	}
	if (op->data.buswidth > 0) {
		ctrl0 |= DATA_CH(rxi312_spi_get_ch(op->data.buswidth));
	}
	map->ctrlr0 = ctrl0;

	value = map->user_length & ~MASK_USER_ADDR_LENGTH;
	value |= USER_ADDR_LENGTH(op->addr.nbytes);
	value &= ~MASK_USER_RD_DUMMY_LENGTH;
	if (op->dummy.nbytes > 0) {
		value |= USER_RD_DUMMY_LENGTH((op->dummy.nbytes * 8 / op->dummy.buswidth) * (map->baudr & MASK_SCKDV) * 2);
	}

	map->user_length = value;

	map->dr[0].byte = op->cmd.opcode;

	for (i = 0; i < op->addr.nbytes; i++) {
		map->dr[0].byte = (op->addr.val >> (8 * (op->addr.nbytes - i - 1))) & 0xFF;
	}

	if (tmod == RXI312_SPI_TMODE_RX) {
		map->rx_ndf = RX_NDF(nbytes);
		map->tx_ndf = TX_NDF(0);

		map->ssienr = BIT_SPIC_EN;

#if SPIC_TP_TEST_EN
		read_ns = ktime_get_raw_ns();
#endif

		i = 0;
		buf = (u8 *)op->data.buf.in;

		unaligned32_bytes = UNALIGNED32(buf);
		while ((i < unaligned32_bytes) && (i < nbytes)) {
			if (map->rxflr >= 1) {
				buf[i++] = map->dr[0].byte;
			}
		}

		aligned32_buf = (u32 *)&buf[i];

		while (i + 4 <= nbytes) {
			fifo_level = map->rxflr >> 2;
			/* A safe way to avoid HW error: (j < fifo_level) && (i + 4 <= nbytes) */
			for (j = 0; j < fifo_level; j++) {
				aligned32_buf[j] = map->dr[0].word;
			}
			i += fifo_level << 2;
			aligned32_buf += fifo_level;
		}

		while (i < nbytes) {
			if (map->rxflr >= 1) {
				buf[i++] = map->dr[0].byte;
			}
		}

#if SPIC_TP_TEST_EN
		read_ns = ktime_get_raw_ns() - read_ns;
		read_wait_ns = ktime_get_raw_ns();
#endif
		rxi312_spi_waitbusy(map, WAIT_TRANS_COMPLETE);
#if SPIC_TP_TEST_EN
		read_wait_ns = ktime_get_raw_ns() - read_wait_ns;
#endif
	} else {
		map->tx_ndf = TX_NDF(nbytes);
		map->rx_ndf = RX_NDF(0);
		map->ssienr = BIT_SPIC_EN;

#if SPIC_TP_TEST_EN
		write_ns = ktime_get_raw_ns();
#endif

		/* Write the remaining data into FIFO */
		if (nbytes) {
			i = 0;
			buf = (u8 *)op->data.buf.out;

			unaligned32_bytes = UNALIGNED32(buf);
			while ((i < unaligned32_bytes) && (i < nbytes)) {
				if (map->txflr <= RXI312_FIFO_LENGTH - 1) {
					map->dr[0].byte = buf[i++];
				}
			}

			aligned32_buf = (u32 *)&buf[i];

			while (i + 4 <= nbytes) {
				fifo_level = (RXI312_FIFO_LENGTH - map->txflr) >> 2;
				for (j = 0; (j < fifo_level) && (i + 4 <= nbytes); j++) {
					map->dr[0].word = aligned32_buf[j];
					i += 4;
				}
				aligned32_buf += fifo_level;
			}

			while (i < nbytes) {
				if (map->txflr <= RXI312_FIFO_LENGTH - 1) {
					map->dr[0].byte = buf[i++];
				}
			}
		}

#if SPIC_TP_TEST_EN
		write_ns = ktime_get_raw_ns() - write_ns;
		write_wait_ns = ktime_get_raw_ns();
#endif
		rxi312_spi_waitbusy(map, WAIT_TRANS_COMPLETE);
#if SPIC_TP_TEST_EN
		write_wait_ns = ktime_get_raw_ns() - write_wait_ns;
#endif
	}

	map->user_length = user_length_bak;
	map->ctrlr0 = ctrl0_bak;

#if SPIC_TP_TEST_EN
	umode_dis_ns = ktime_get_raw_ns();
#endif
	rxi312_spi_usermode_en(map, 0);
#if SPIC_TP_TEST_EN
	umode_dis_ns = ktime_get_raw_ns() - umode_dis_ns;
#endif

	local_irq_restore(flags);
	mutex_unlock(&spi->lock);

#if SPIC_TP_TEST_EN
	total_ns = ktime_get_raw_ns() - total_ns;

	if (nbytes >= 2048) {
		if (tmod == RXI312_SPI_TMODE_RX) {
			pr_info("SPIC: RX byte=%u total=%llu umode_en=%llu read=%llu wait=%llu umode_dis=%llu addrbw=%d databw=%d\n",
					nbytes,
					total_ns,
					umode_en_ns,
					read_ns,
					read_wait_ns,
					umode_dis_ns,
					op->addr.buswidth,
					op->data.buswidth);
		} else {
			pr_info("SPIC: TX byte=%u total=%llu umode_en=%llu write=%llu wait=%llu umode_dis=%llu addrbw=%d databw=%d\n",
					nbytes,
					total_ns,
					umode_en_ns,
					write_ns,
					write_wait_ns,
					umode_dis_ns,
					op->addr.buswidth,
					op->data.buswidth);
		}
	}
#endif

	return ret;
}

/* Callback for spi_mem subsystem, use default spi_mem_default_supports_op to check supported mode */
static const struct spi_controller_mem_ops rxi312_spi_mem_ops = {
	.exec_op = rxi312_spi_exec_op,
};

/* Setup, initialize SPI mode as per bootloader settings */
static int rxi312_spi_setup(struct spi_device *sdev)
{
	struct rxi312_spi *spi = spi_master_get_devdata(sdev->master);
	struct rxi312_spi_map *map = spi->user_base;
	u32 ctrl0;
	u32 valid_cmd;

	mutex_lock(&spi->lock);

	ctrl0 = map->ctrlr0;
	valid_cmd = map->valid_cmd;

	sdev->mode = 0;

	if (ctrl0 & BIT_SCPOL) {
		sdev->mode |= SPI_CPOL;
	}

	if (ctrl0 & BIT_SCPH) {
		sdev->mode |= SPI_CPHA;
	}

	if (((valid_cmd & BIT_RD_QUAD_IO) != 0) || ((valid_cmd & BIT_RD_QUAD_O) != 0)) {
		sdev->mode |= SPI_TX_QUAD | SPI_RX_QUAD;
	} else if (((valid_cmd & BIT_RD_DUAL_IO) != 0) || ((valid_cmd & BIT_RD_DUAL_I) != 0)) {
		sdev->mode |= SPI_TX_DUAL | SPI_RX_DUAL;
	}

	dev_info(&sdev->dev, "SPI mode = 0x%08X\n", sdev->mode);

	mutex_unlock(&spi->lock);

	return 0;
}

static const struct of_device_id rxi312_spi_of_match_table[] = {
	{
		.compatible = "realtek,rxi312-nand",
	},
	{ },
};

MODULE_DEVICE_TABLE(of, rxi312_spi_of_match_table);

/* Probe */
static int rxi312_spi_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct spi_master *master;
	struct rxi312_spi *spi;
	struct resource *mem;
	int status;
	u32 bus_num;

	of_id = of_match_device(rxi312_spi_of_match_table, &pdev->dev);
	if (!of_id) {
		return -ENODEV;
	}

	master = spi_alloc_master(&pdev->dev, sizeof(*spi));
	if (!master) {
		dev_err(&pdev->dev, "Failed to allocate SPI master\n");
		return -ENOMEM;
	}

	spi = spi_master_get_devdata(master);
	spi->master = master;

	master->bus_num = 0;
	if (pdev->dev.of_node) {
		if (!of_property_read_u32(pdev->dev.of_node, "bus_num", &bus_num)) {
			master->bus_num = bus_num;
		}
	}

	master->num_chipselect = 1;
	master->setup = rxi312_spi_setup;
	master->mem_ops = &rxi312_spi_mem_ops;
	master->dev.of_node = pdev->dev.of_node;

	spi_master_set_devdata(master, spi);
	platform_set_drvdata(pdev, spi);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		status = -ENODEV;
		goto fail_get_resource;
	}

	if (!devm_request_mem_region(&pdev->dev, mem->start, resource_size(mem), pdev->name)) {
		status = -EBUSY;
		goto fail_get_resource;
	}

	spi->user_base = devm_ioremap_nocache(&pdev->dev, mem->start, resource_size(mem));
	if (!spi->user_base) {
		status = -ENXIO;
		goto fail_ioremap_nocache;
	}

	status = devm_spi_register_master(&pdev->dev, master);
	if (status) {
		dev_err(&pdev->dev, "Failed to register SPI master: %d\n", status);
		goto fail_register_master;
	}

	return status;

fail_register_master:
	iounmap(spi->user_base);
fail_ioremap_nocache:
	release_mem_region(mem->start, resource_size(mem));
fail_get_resource:
	platform_set_drvdata(pdev, NULL);
	spi_master_put(master);

	return status;
}

static int rxi312_spi_remove(struct platform_device *pdev)
{
	struct rxi312_spi *spi;
	struct resource *mem;

	spi = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);

	iounmap(spi->user_base);
	spi_unregister_master(spi->master);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));

	return 0;
}

static struct platform_driver rxi312_spi_driver = {
	.driver = {
		.name = "realetek-rxi312-nand",
		.of_match_table =
		of_match_ptr(rxi312_spi_of_match_table),
	},
	.probe = rxi312_spi_probe,
	.remove = rxi312_spi_remove,
};

builtin_platform_driver(rxi312_spi_driver);

MODULE_DESCRIPTION("Realtek Ameba RXI312 SPIC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
