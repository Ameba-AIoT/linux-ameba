// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek I2C support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/signal.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/completion.h>

#ifndef RTK_I2C_DEFINE
#define RTK_I2C_DEFINE

#ifdef CONFIG_PM
/* SET_RUNTIME_PM_OPS only provided when CONFIG_PM enabled. */
/* RTK I2C Runtime PM is not suggested to enable. */
#define RTK_I2C_PM_RUNTIME			0
#endif
/*-----------------------------------------------------------------*/
/*-------------------------- Hardware Layer -----------------------*/
/*-----------------------------------------------------------------*/

/* I2C base address read only here!!! Refer to DTS for details!!!! */
/*KM0 I2C0 */
#define I2C0_REG_BASE			0x4200F000
/* ID:HSLV8-14, Inter. Type:APB4, Top Address:0x400EFFFF, Size(KB):4K, Clk Domain:HS_AHB_CLK */
#define I2C1_REG_BASE			0x400EF000
/* ID:HSLV8-15, Inter. Type:APB4, Top Address:0x400F0FFF, Size(KB):4K, Clk Domain:HS_AHB_CLK */
#define I2C2_REG_BASE			0x400F0000

/**************************************************************************//**
 * @defgroup AMEBA_I2C
 * @{
 * @brief AMEBA_I2C Register Declaration
 *****************************************************************************/
#define IC_CON				0x000
#define IC_TAR				0x004
#define IC_SAR				0x008
#define IC_HS_MAR			0x00C
#define IC_DATA_CMD			0x010
#define IC_SS_SCL_HCNT			0x014
#define IC_SS_SCL_LCNT			0x018
#define IC_FS_SCL_HCNT			0x01C
#define IC_FS_SCL_LCNT			0x020
#define IC_HS_SCL_HCNT			0x024
#define IC_HS_SCL_LCNT			0x028
#define IC_INTR_STAT			0x02C
#define IC_INTR_MASK			0x030
#define IC_RAW_INTR_STAT		0x034
#define IC_RX_TL			0x038
#define IC_TX_TL			0x03C
#define IC_CLR_INTR			0x040
#define IC_CLR_RX_UNDER			0x044
#define IC_CLR_RX_OVER			0x048
#define IC_CLR_TX_OVER			0x04C
#define IC_CLR_RD_REQ			0x050
#define IC_CLR_TX_ABRT			0x054
#define IC_CLR_RX_DONE			0x058
#define IC_CLR_ACTIVITY			0x05C
#define IC_CLR_STOP_DET			0x060
#define IC_CLR_START_DET		0x064
#define IC_CLR_GEN_CALL			0x068
#define IC_ENABLE			0x06C
#define IC_STATUS			0x070
#define IC_TXFLR			0x074
#define IC_RXFLR			0x078
#define IC_SDA_HOLD			0x07C
#define IC_TX_ABRT_SOURCE		0x080
#define IC_SLV_DATA_NACK_ONLY		0x084
#define IC_DMA_CR			0x088
#define IC_DMA_TDLR			0x08C
#define IC_DMA_RDLR			0x090
#define IC_SDA_SETUP			0x094
#define IC_ACK_GENERAL_CALL		0x098
#define IC_ENABLE_STATUS		0x09C
#define IC_DMA_CMD			0x0A0
#define IC_DMA_DATA_LEN			0x0A4
#define IC_DMA_MODE			0x0A8
#define IC_SLEEP			0x0AC
#define IC_DEBUG_SEL			0x0B0
#define IC_OUT_SMP_DLY			0x0B4
#define RSVD0				0	/* 0xB8-0xE0 */
#define IC_CLR_ADDR_MATCH		0x0E4
#define IC_CLR_DMA_DONE			0x0E8
#define IC_FILTER			0x0EC
#define RSVD1				0x0F0
#define IC_SAR2				0x0F4
#define RSVD2				0x0F8
#define IC_COMP_VERSION			0x0FC

/**************************************************************************//**
 * @defgroup IC_CON
 * @brief I2C Control Register
 * @{
 *****************************************************************************/
#define I2C_BIT_IC_SLAVE_DISABLE_1	((u32)0x00000001 << 7)          /*!<R/W 0x0  This bit controls whether I2C has its slave2 (7-bit address) disabled. * 0: slave2 is enabled * 1: slave2 is disabled */
#define I2C_BIT_IC_SLAVE_DISABLE_0	((u32)0x00000001 << 6)          /*!<R/W 0x0  This bit controls whether I2C has its slave1 (7-bit or 10-bit address) disabled. * 0: slave1 is enabled * 1: slave1 is disabled */
#define I2C_BIT_IC_RESTATRT_EN		((u32)0x00000001 << 5)          /*!<R/W 0x0  Determine whether RESTART conditions may be sent when acting as a master. * 0: disable * 1: enable */
#define I2C_BIT_IC_10BITADDR_SLAVE	((u32)0x00000001 << 3)          /*!<R/W 0x0  When acting as a slave, this bit controls whether the I2C responds to 7- or 10-bit addresses. * 0: 7-bit addressing * 1: 10-bit addressing */
#define I2C_MASK_SPEED			((u32)0x00000003 << 1)          /*!<R/W 0x0  These bits control at which speed the I2C operates; its setting is relevant only if one is operating the I2C in master mode. * 1: standard mode (0 to 100kbit/s) * 2: fast mode (<=400kbit/s) * 3: high speed mode (<=3.4Mbit/s) */
#define I2C_SPEED(x)			((u32)(((x) & 0x00000003) << 1))
#define I2C_GET_SPEED(x)		((u32)(((x >> 1) & 0x00000003)))
#define I2C_BIT_MASTER_MODE		((u32)0x00000001 << 0);          /*!<R/W 0x0  This bit controls whether the I2C master is enabled. * 0: master disabled * 1: master enabled */
/** @} */

/**************************************************************************//**
 * @defgroup IC_TAR
 * @brief I2C Target Address Register
 * @{
 *****************************************************************************/
#define I2C_BIT_IC_10BITADDR_MASTER	((u32)0x00000001 << 12)          /*!<R/W 0x0  Control whether I2C starts its transfers in 7- or 10-bit addressing mode when acting as a master. * 0: 7-bit addressing * 1: 10-bit addressing */
#define I2C_BIT_SPECIAL			((u32)0x00000001 << 11)          /*!<R/W 0x0  This bit indicates whether software performs a General Call or START BYTE command. * 0: ignore bit 10 GC_OR_START and use IC_TAR normally * 1: perform special I2C command as specified in GC_OR_START bit */
#define I2C_BIT_GC_OR_START		((u32)0x00000001 << 10)          /*!<R/W 0x0  If SPECIAL is set to 1, then this bit indicates whether a General Call or START BYTE command is to be performed by I2C. (ic_clk domain) * 0: General Call - after issuing a General Call, only write may be performed * 1: START BYTE */
#define I2C_MASK_IC_TAR			((u32)0x000003FF << 0)          /*!<R/W 0x10  This is the target address for any master transaction. When transmitting a General Call, these bits are ignored. To generate a START BYTE, the CPU needs to write only once into these bits. */
#define I2C_IC_TAR(x)			((u32)(((x) & 0x000003FF) << 0))
#define I2C_GET_IC_TAR(x)		((u32)(((x >> 0) & 0x000003FF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_SAR
 * @brief I2C Slave Address Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_SAR			((u32)0x000003FF << 0)          /*!<R/W 0x11  The IC_SAR holds the slave address when the I2C is operating as slave1. For 7-bit addressing, only IC_SAR[6:0] is used. The register can be written only when the I2C interface is disabled, which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. */
#define I2C_IC_SAR(x)			((u32)(((x) & 0x000003FF) << 0))
#define I2C_GET_IC_SAR(x)		((u32)(((x >> 0) & 0x000003FF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_HS_MAR
 * @brief I2C High Speed Master Mode Code Address Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_HS_MAR		((u32)0x00000007 << 0)          /*!<R/W 0x0  This bit field holds the value of the I2C HS mode master mode. HS-mode master codes are reserved 8-bit codes (00001xxx) that are not used for slave addressing or other purposes. This register can be written only when the I2C interface is disabled, which corresponds to the IC_ENABLE register being set to 0. Write at other times have no effect. */
#define I2C_IC_HS_MAR(x)		((u32)(((x) & 0x00000007) << 0))
#define I2C_GET_IC_HS_MAR(x)		((u32)(((x >> 0) & 0x00000007)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_DATA_CMD
 * @brief I2C Rx/Tx Data Buffer and Command Register
 * @{
 *****************************************************************************/
#define I2C_BIT_NULL_DATA		((u32)0x00000001 << 11)          /*!<W 0x0  *1 : null data transfer */
#define I2C_BIT_CMD_RESTART		((u32)0x00000001 << 10)          /*!<W 0x0  This bit controls whether a RESTART is issued after the byte is sent or received. * 1: a RESTART is issued after the data is sent/received (according to the value of CMD_RW), regardless of whether or not the transfer direction is changing from the previous command. */
#define I2C_BIT_CMD_STOP		((u32)0x00000001 << 9)          /*!<W 0x0  This bit controls whether a STOP is issued after the byte is sent or received. * 1: STOP is issued after this byte, regardless of whether or not the Tx FIFO is empty. If the Tx FIFO is not empty, the master immediately tries to start a new transfer by issuing a START and arbitrating for the bus. * 0: STOP is not issued after this byte. */
#define I2C_BIT_CMD_RW			((u32)0x00000001 << 8)          /*!<W 0x0  This bit controls whether a read or write is performed. This bit does not control the direction when the I2C acts as a slave. It controls only the direction when it acts as a master. * 1: Read * 0: Write */
#define I2C_MASK_IC_DATA		((u32)0x000000FF << 0)          /*!<R/W 0x0  This register contains the data to be transmitted or received on the I2C bus. If you are writing to this register and want to perform a read, these bits are ignored by the I2C. However, when you read this register, these bits return the value of data received on the I2C interface. */
#define I2C_IC_DATA(x)			((u32)(((x) & 0x000000FF) << 0))
#define I2C_GET_IC_DATA(x)		((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_SS_SCL_HCNT
 * @brief Standard Speed I2C Clock SCL High Count Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_SS_SCL_HCNT		((u32)0x0000FFFF << 0)          /*!<R/W 0x0190  This register sets the SCL clock high-period count for standard speed. This register must be set before any I2C bus transaction can take place to ensure proper I/O timing. This register can be written only when the I2C interface is disabled which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. */
#define I2C_IC_SS_SCL_HCNT(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define I2C_GET_IC_SS_SCL_HCNT(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_SS_SCL_LCNT
 * @brief Standard Speed I2C Clock SCL Low Count Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_SS_SCL_LCNT		((u32)0x0000FFFF << 0)          /*!<R/W 0x01D6  This register sets the SCL clock low-period count for standard speed. This register must be set before any I2C bus transaction can take place to ensure proper I/O timing. This register can be written only when the I2C interface is disabled which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. */
#define I2C_IC_SS_SCL_LCNT(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define I2C_GET_IC_SS_SCL_LCNT(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_FS_SCL_HCNT
 * @brief Fast Speed I2C Clock SCL High Count Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_FS_SCL_HCNT		((u32)0x0000FFFF << 0)          /*!<R/W 0x003C  This register sets the SCL clock high-period count for fast speed. This register must be set before any I2C bus transaction can take place to ensure proper I/O timing. This register can be written only when the I2C interface is disabled which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. */
#define I2C_IC_FS_SCL_HCNT(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define I2C_GET_IC_FS_SCL_HCNT(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_FS_SCL_LCNT
 * @brief Fast Speed I2C Clock SCL Low Count Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_FS_SCL_LCNT		((u32)0x0000FFFF << 0)          /*!<R/W 0x0082  This register sets the SCL clock low-period count for fast speed. This register must be set before any I2C bus transaction can take place to ensure proper I/O timing. This register can be written only when the I2C interface is disabled which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. */
#define I2C_IC_FS_SCL_LCNT(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define I2C_GET_IC_FS_SCL_LCNT(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_HS_SCL_HCNT
 * @brief High Speed I2C Clock SCL High Count Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_HS_SCL_HCNT		((u32)0x0000FFFF << 0)          /*!<R/W 0x0006  This register sets the SCL clock high-period count for high speed. This register must be set before any I2C bus transaction can take place to ensure proper I/O timing. This register can be written only when the I2C interface is disabled which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. The SCL High time depends on the loading of the bus. For 100pF loading, the SCL High time is 60ns; for 400pF loading, the SCL High time is 120ns. */
#define I2C_IC_HS_SCL_HCNT(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define I2C_GET_IC_HS_SCL_HCNT(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_HS_SCL_LCNT
 * @brief High Speed I2C Clock SCL Low Count Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_HS_SCL_LCNT		((u32)0x0000FFFF << 0)          /*!<R/W 0x0010  This register sets the SCL clock low-period count for high speed. This register must be set before any I2C bus transaction can take place to ensure proper I/O timing. This register can be written only when the I2C interface is disabled which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. The SCL Low time depends on the loading of the bus. For 100pF loading, the SCL High time is 160ns; for 400pF loading, the SCL High time is 320ns. */
#define I2C_IC_HS_SCL_LCNT(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define I2C_GET_IC_HS_SCL_LCNT(x)	((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_INTR_STAT
 * @brief I2C Interrupt Status Register
 * @{
 *****************************************************************************/
#define I2C_BIT_R_LP_WAKE_2		((u32)0x00000001 << 13)          /*!<R 0x0   */
#define I2C_BIT_R_LP_WAKE_1		((u32)0x00000001 << 12)          /*!<R 0x0   */
#define I2C_BIT_R_GEN_CALL		((u32)0x00000001 << 11)          /*!<R 0x0   */
#define I2C_BIT_R_START_DET		((u32)0x00000001 << 10)          /*!<R 0x0   */
#define I2C_BIT_R_STOP_DET		((u32)0x00000001 << 9)          /*!<R 0x0   */
#define I2C_BIT_R_ACTIVITY		((u32)0x00000001 << 8)          /*!<R 0x0   */
#define I2C_BIT_R_RX_DONE		((u32)0x00000001 << 7)          /*!<R 0x0   */
#define I2C_BIT_R_TX_ABRT		((u32)0x00000001 << 6)          /*!<R 0x0   */
#define I2C_BIT_R_RD_REQ		((u32)0x00000001 << 5)          /*!<R 0x0   */
#define I2C_BIT_R_TX_EMPTY		((u32)0x00000001 << 4)          /*!<R 0x0   */
#define I2C_BIT_R_TX_OVER		((u32)0x00000001 << 3)          /*!<R 0x0   */
#define I2C_BIT_R_RX_FULL		((u32)0x00000001 << 2)          /*!<R 0x0   */
#define I2C_BIT_R_RX_OVER		((u32)0x00000001 << 1)          /*!<R 0x0   */
#define I2C_BIT_R_RX_UNDER		((u32)0x00000001 << 0)          /*!<R 0x0   */
/** @} */

/**************************************************************************//**
 * @defgroup IC_INTR_MASK
 * @brief I2C Interrupt Mask Register
 * @{
 *****************************************************************************/
#define I2C_BIT_M_LP_WAKE_2		((u32)0x00000001 << 13)          /*!<R/W 0x0   */
#define I2C_BIT_M_LP_WAKE_1		((u32)0x00000001 << 12)          /*!<R/W 0x0   */
#define I2C_BIT_M_GEN_CALL		((u32)0x00000001 << 11)          /*!<R/W 0x0   */
#define I2C_BIT_M_START_DET		((u32)0x00000001 << 10)          /*!<R/W 0x0   */
#define I2C_BIT_M_STOP_DET		((u32)0x00000001 << 9)          /*!<R/W 0x0   */
#define I2C_BIT_M_ACTIVITY		((u32)0x00000001 << 8)          /*!<R/W 0x0   */
#define I2C_BIT_M_RX_DONE		((u32)0x00000001 << 7)          /*!<R/W 0x0   */
#define I2C_BIT_M_TX_ABRT		((u32)0x00000001 << 6)          /*!<R/W 0x0   */
#define I2C_BIT_M_RD_REQ		((u32)0x00000001 << 5)          /*!<R/W 0x0   */
#define I2C_BIT_M_TX_EMPTY		((u32)0x00000001 << 4)          /*!<R/W 0x0   */
#define I2C_BIT_M_TX_OVER		((u32)0x00000001 << 3)          /*!<R/W 0x0   */
#define I2C_BIT_M_RX_FULL		((u32)0x00000001 << 2)          /*!<R/W 0x0   */
#define I2C_BIT_M_RX_OVER		((u32)0x00000001 << 1)          /*!<R/W 0x0   */
#define I2C_BIT_M_RX_UNDER		((u32)0x00000001 << 0)          /*!<R/W 0x0   */
/** @} */

/**************************************************************************//**
 * @defgroup IC_RAW_INTR_STAT
 * @brief I2C Raw Interrupt Status Register
 * @{
 *****************************************************************************/
#define I2C_BIT_LP_WAKE_2		((u32)0x00000001 << 13)          /*!<R 0x0  Set when slave2's address is matched . */
#define I2C_BIT_LP_WAKE_1		((u32)0x00000001 << 12)          /*!<R 0x0  Set when slave1's address is matched . */
#define I2C_BIT_GEN_CALL		((u32)0x00000001 << 11)          /*!<R 0x0  Set only when a General Call address is received and it is acknowledged. */
#define I2C_BIT_START_DET		((u32)0x00000001 << 10)          /*!<R 0x0  Indicates whether a START or RESTART condition has occurred on the I2C interface regardless of whether I2C is operating in slave or master mode. */
#define I2C_BIT_STOP_DET		((u32)0x00000001 << 9)          /*!<R 0x0  Indicates whether a STOP condition has occurred on the I2C interface regardless of whether I2C is operating in slave or master mode. */
#define I2C_BIT_ACTIVITY		((u32)0x00000001 << 8)          /*!<R 0x0  This bit captures I2C activity. */
#define I2C_BIT_RX_DONE			((u32)0x00000001 << 7)          /*!<R 0x0  When the I2C is acting as a slave-transmitter, this bit is set to 1 if the master does not acknowledge a transmitted byte. This occurs on the last byte of the transmission, indicating that the transmission is done. */
#define I2C_BIT_TX_ABRT			((u32)0x00000001 << 6)          /*!<R 0x0  This bit indicates if I2C as a transmitter, is unable to complete the intended actions on the contents of the transmit FIFO. This situation can occur both as an I2C master or an I2C slave, and is referred to as a 'transmit abort'. * 1: the IC_TX_ABRT_SOURCE register indicates the reason why the transmit abort take places. */
#define I2C_BIT_RD_REQ			((u32)0x00000001 << 5)          /*!<R 0x0  This bit is set to 1 when I2C is acting as a slave and another I2C master is attempting to read data from I2C. The I2C holds the I2C bus in a wait state(SCL=0) until this interrupt is serviced, which means that the slave has been addressed by a remote master that is asking for data to be transferred. The processor must respond to this interrupt and then write the requested data to the IC_DATA_CMD register. */
#define I2C_BIT_TX_EMPTY		((u32)0x00000001 << 4)          /*!<R 0x0  This bit is set to 1 when the transmit buffer is at or below the threshold value set in the IC_TX_TL register. It is automatically cleared by hardware when the buffer level goes above the threshold. */
#define I2C_BIT_TX_OVER			((u32)0x00000001 << 3)          /*!<R 0x0  Set during transmit if the transmit buffer is filled to IC_TX_BUFFER_DEPTH and the processor attempts to issue another I2C command by writing to the IC_DATA_CMD register. */
#define I2C_BIT_RX_FULL			((u32)0x00000001 << 2)          /*!<R 0x0  Set when the receive buffer reaches or goes above the RX_TL threshold in the IC_RX_TL register. It is automatically cleared by hardware when buffer level goes below the threshold. */
#define I2C_BIT_RX_OVER			((u32)0x00000001 << 1)          /*!<R 0x0  Set if the receive buffer is completely filled to IC_RX_BUFFER_DEPTH and an additional byte is received from an external I2C device. */
#define I2C_BIT_RX_UNDER		((u32)0x00000001 << 0)          /*!<R 0x0  Set if the processor attempts to read the receive buffer when it is empty by reading from the IC_DATA_CMD register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_RX_TL
 * @brief I2C Receive FIFO Threshold Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_RX_0_TL		((u32)0x000000FF << 0)          /*!<R/W 0x0  Receive FIFO Threshold Level. Control the level of entries (or above) that triggers the RX_FULL interrupt (bit 2 in IC_RAW_INTR_STAT register). */
#define I2C_IC_RX_0_TL(x)		((u32)(((x) & 0x000000FF) << 0))
#define I2C_GET_IC_RX_0_TL(x)		((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_TX_TL
 * @brief I2C Transmit FIFO Threshold Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_TX_0_TL		((u32)0x000000FF << 0)          /*!<R/W 0x0  Transmit FIFO Threshold Level. Control the level of entries (or below) that triggers the TX_EMPTY interrupt (bit 4 in IC_RAW_INTR_STAT register). */
#define I2C_IC_TX_0_TL(x)		((u32)(((x) & 0x000000FF) << 0))
#define I2C_GET_IC_TX_0_TL(x)		((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_INTR
 * @brief Clear Combined and Individual Interrupt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_INRT		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the combined interrupt, all individual interrupts, and the IC_TX_ABRT_SOURCE register. This bit does not clear hardware clearable interrupts but software clearable interrupts. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_RX_UNDER
 * @brief Clear RX_UNDER Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_RX_UNDER		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the RX_UNDER interrupt (bit 0) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_RX_OVER
 * @brief Clear RX_OVER Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_RX_OVER		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the RX_OVER interrupt (bit 1) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_TX_OVER
 * @brief Clear TX_OVER Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_TX_OVER		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the TX_OVER interrupt (bit 3) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_RD_REQ
 * @brief Clear RD_REQ Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_RD_REQ		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the RD_REQ interrupt (bit 5) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_TX_ABRT
 * @brief Clear TX_ABRT Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_TX_ABRT		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the TX_ABRT interrupt (bit 6) of IC_RAW_INTR_STAT register, and the IC_TX_ABRT_SOURCE register. This is also releases the TX FIFO from the flushed/reset state, allowing more writes to the TX FIFO. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_RX_DONE
 * @brief Clear RX_DONE Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_RX_DONE		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the RX_DONE interrupt (bit 7) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_ACTIVITY
 * @brief Clear ACTIVITY Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_ACTIVITY		((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the ACTIVITY interrupt if the I2C is not active anymore. If the I2C module is still active on the bus, the ACTIVITY interrupt bit continues to be set. It is automatically cleared by hardware if the module is disabled and if there is no further activity on the bus. The value read from this register to get status of the ACTIVITY interrupt (bit 8) of the IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_STOP_DET
 * @brief Clear STOP_DET Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_STOP_DET		((u32)0x00000001 << 0)          /*!<R 0x0  ead this register to clear the STOP_DET interrupt (bit 9) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_START_DET
 * @brief Clear START_DET Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_START_DET		((u32)0x00000001 << 0)          /*!<R 0x0  ead this register to clear the START_DET interrupt (bit 10) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_GEN_CALL
 * @brief Clear GEN_ALL Interrpt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_CLR_GEN_CALL		((u32)0x00000001 << 0)          /*!<R 0x0  ead this register to clear the GEN_CALL interrupt (bit 11) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_ENABLE
 * @brief I2C Enable Register
 * @{
 *****************************************************************************/
#define I2C_BIT_FORCE			((u32)0x00000001 << 1)          /*!<R/W 0x0  Force stop of master I2C when ENABLE&MATER_MODE=0. * 1: force master FSM to IDLE, data transition maybe inttrupt. * 0: master FSM while finish current transition before go to IDLE. */
#define I2C_BIT_ENABLE			((u32)0x00000001 << 0)          /*!<R/W 0x0  Control whether the I2C is enabled. * 0: Disable I2C (TX and RX FIFOs are held in an erased state) * 1: Enable I2C */
/** @} */

/**************************************************************************//**
 * @defgroup IC_STATUS
 * @brief I2C Status Register
 * @{
 *****************************************************************************/
#define I2C_BIT_SLV_ACTIVITY		((u32)0x00000001 << 6)          /*!<R 0x0  Slave FSM Activity Status. When the Slave FSM is not in the IDLE state, this bit is set. * 0: Slave FSM is in the IDLE state so the slave part of I2C is not active * 1: Slave FSM is not he IDLE state so the slave part of I2C is active */
#define I2C_BIT_MST_ACTIVITY		((u32)0x00000001 << 5)          /*!<R 0x0  Master FSM Activity Status. When the Master FSM is not in the IDLE state, this bit is set. * 0: Master FSM is in the IDLE state so the master part of I2C is not active * 1: Master FSM is not he IDLE state so the master part of I2C is active */
#define I2C_BIT_RFF			((u32)0x00000001 << 4)          /*!<R 0x0  Receive FIFO Completely Full. When the receive FIFO is completely full, this bit is set. When the receive FIFO contains one or more empty location, this bit is cleared. * 0: Receive FIFO is not full * 1: Receive FIFO is full */
#define I2C_BIT_RFNE			((u32)0x00000001 << 3)          /*!<R 0x0  Receive FIFO Not Empty. This bit is set when the receive FIFO contains one or more entries; it is cleared when the receive FIFO is empty. * 0: Receive FIFO is empty * 1: Receive FIFO is not empty */
#define I2C_BIT_TFE			((u32)0x00000001 << 2)          /*!<R 0x1  Transmit FIFO Completely Empty. When the transmit FIFO is completely empty, this bit is set. When it contains one or more valid entries, this bit is cleared. This bit field does not request an interrupt. * 0: Transmit FIFO is not empty * 1: Transmit FIFO is empty */
#define I2C_BIT_TFNF			((u32)0x00000001 << 1)          /*!<R 0x1  Transmit FIFO Not Full. Set when the transmit FIFO contains one or more empty locations, and is cleared when the FIFO is full. * 0: Transmit FIFO is full * 1: Transmit FIFO is not full */
#define I2C_BIT_IC_ACTIVITY		((u32)0x00000001 << 0)          /*!<R 0x0  I2C Activity Status. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_TXFLR
 * @brief I2C Transmit FIFO Level Register
 * @{
 *****************************************************************************/
#define I2C_MASK_TXFLR			((u32)0x0000003F << 0)          /*!<R 0x0  Transmit FIFO Level. Contains the number of valid data entries in the transmit FIFO. */
#define I2C_TXFLR(x)			((u32)(((x) & 0x0000003F) << 0))
#define I2C_GET_TXFLR(x)		((u32)(((x >> 0) & 0x0000003F)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_RXFLR
 * @brief I2C Receive FIFO Level Register
 * @{
 *****************************************************************************/
#define I2C_MASK_RXFLR			((u32)0x0000003F << 0)          /*!<R 0x0  Receive FIFO Level. Contains the number of valid data entries in the receive FIFO. */
#define I2C_RXFLR(x)			((u32)(((x) & 0x0000003F) << 0))
#define I2C_GET_RXFLR(x)		((u32)(((x >> 0) & 0x0000003F)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_SDA_HOLD
 * @brief I2C SDA Hold Time Length Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_SDA_HOLD		((u32)0x0000FFFF << 0)          /*!<R/W 0x1  Set the required SDA hold time in units of ic_clk period. */
#define I2C_IC_SDA_HOLD(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define I2C_GET_IC_SDA_HOLD(x)		((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_TX_ABRT_SOURCE
 * @brief I2C Transmit Abort Source Register
 * @{
 *****************************************************************************/
#define I2C_BIT_ABRT_SLV_ARBLOST	((u32)0x00000001 << 14)          /*!<R 0x0  * 1: Slave lost the bus while transmitting data to a remote master. IC_TX_ABRT_SOURCE [12] is set at the same time. */
#define I2C_BIT_ABRT_SLVFLUSH_TXFIFO	((u32)0x00000001 << 13)          /*!<R 0x0  * 1: Slave has received a read command and some data exists in the TXFIFO so the slave issues a TX_ABRT interrupt to flush old data in TXFIFO. */
#define I2C_BIT_ARBT_LOST		((u32)0x00000001 << 12)          /*!<R 0x0  * 1: Master has lost arbitration, or if IC_TX_ABRT_SOURCE [* 14] is also set, then the slave transmitter has lost arbitration. */
#define I2C_BIT_ABRT_MASTER_DIS		((u32)0x00000001 << 11)          /*!<R 0x0  * 1: User tries to initiate a Master operation with the Master mode disabled. */
#define I2C_BIT_ABRT_10B_RD_NORSTRT	((u32)0x00000001 << 10)          /*!<R 0x0  * 1: The restart is disabled and the master sends a read command in * 10-bit addressing mode. */
#define I2C_BIT_ABRT_SBYTE_NORSTRT	((u32)0x00000001 << 9)          /*!<R 0x0  * 1: The restart is disabled and the user is trying to send a START Byte. */
#define I2C_BIT_ABRT_HS_NORSTRT		((u32)0x00000001 << 8)          /*!<R 0x0  * 1: The restart is disabled and the user is trying to use the master to transfer data in High Speed mode. */
#define I2C_BIT_ABRT_SBYTE_ACKDET	((u32)0x00000001 << 7)          /*!<R 0x0  * 1: Master has sent a START Byte and the START Byte was acknowledged (wrong behavior). */
#define I2C_BIT_ABRT_HS_ACKDET		((u32)0x00000001 << 6)          /*!<R 0x0  * 1: Master is in High Speed mode and the High Speed Master code was acknowledged (wrong behavior). */
#define I2C_BIT_ABRT_GCALL_READ		((u32)0x00000001 << 5)          /*!<R 0x0  * 1: I2C in master mode sent a General Call but the user programmed the byte following the General Call to be a read from the bus. */
#define I2C_BIT_ABRT_GCALL_NOACK	((u32)0x00000001 << 4)          /*!<R 0x0  * 1: I2C in master mode sent a General Call and no slave on the bus acknowledged the General Call. */
#define I2C_BIT_ABRT_TXDATA_NOACK	((u32)0x00000001 << 3)          /*!<R 0x0  * 1: This is a master-mode only bit. Master has received an acknowledgement for the address, but when it sent data byte(s) following the address, it did not receive an acknowledge from the remote slave(s). */
#define I2C_BIT_ABRT_10ADDR2_NOACK	((u32)0x00000001 << 2)          /*!<R 0x0  * 1: Master is in * 10-bit address mode and the second byte of the * 10-bit address was not acknowledged by any slave. */
#define I2C_BIT_ABRT_10ADDR1_NOACK	((u32)0x00000001 << 1)          /*!<R 0x0  * 1: Master is in * 10-bit address mode and the first * 10-bit address byte was not acknowledged by any slave. */
#define I2C_BIT_ABRT_7B_ADDR_NOACK	((u32)0x00000001 << 0)          /*!<R 0x0  * 1: Master is in 7-bit addressing mode and the address sent was not acknowledged by any slave */
/** @} */

/**************************************************************************//**
 * @defgroup IC_SLV_DATA_NACK_ONLY
 * @brief Generate Slave Data NACK Register
 * @{
 *****************************************************************************/
#define I2C_BIT_NACK			((u32)0x00000001 << 0)          /*!<R/W 0x0  Generate NACK. This NACK generation only occurs when I2C is a slave-receiver. If this register is set to a value of 1, it can only generate a NACK after a data byte is received; hence, the data transfer is aborted and the data received is not pushed to the receive buffer. When the register is set to a value of 0, it generates NACK/ACK, depending on normal criteria. * 1: generate NACK after data byte received * 0: generate NACK/ACK normally */
/** @} */

/**************************************************************************//**
 * @defgroup IC_SDA_SETUP
 * @brief I2C SDA Setup Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_SDA_SETUP		((u32)0x000000FF << 0)          /*!<R/W 0x5  Set the required SDA setup time in units of ic_clk period. */
#define I2C_IC_SDA_SETUP(x)		((u32)(((x) & 0x000000FF) << 0))
#define I2C_GET_IC_SDA_SETUP(x)		((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_ACK_GENERAL_CALL
 * @brief I2C ACK General Call Register
 * @{
 *****************************************************************************/
#define I2C_BIT_ACK_GEN_CALL		((u32)0x00000001 << 0)          /*!<R/W 0x1  * 1: I2C responds with a ACK when it receives a General Call. * 0: I2C does not generate General Call interrupts. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_ENABLE_STATUS
 * @brief I2C Enable Status Register
 * @{
 *****************************************************************************/
#define I2C_BIT_SLV_DISABLE_WHILE_BUSY	((u32)0x00000001 << 2)          /*!<R 0x0  Slave Disabled While Busy (Transmit, Receive). This bit indicates if a potential or active Slave operation has been aborted due to the setting of the IC_ENABLE register from 1 to 0. */
#define I2C_BIT_IC_EN			((u32)0x00000001 << 0)          /*!<R 0x0  ic_en Status. This bit always reflects the value driven on the output port ic_en. * 1: I2C is deemed to be in an enabled state. * 0: I2C is deemed completely inactive. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_DEBUG_SEL
 * @brief I2C Debug SEL Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_DEBUG_SEL		((u32)0x0000000F << 0)          /*!<R/W 0x0  * 0: Debug clock * 1: Debug APB * 2: RSVD * 3: Debug FIFO * 4: Debug Timing * 5: Debug Slave Mode Address Match * 6: Debug Interrupt */
#define I2C_IC_DEBUG_SEL(x)		((u32)(((x) & 0x0000000F) << 0))
#define I2C_GET_IC_DEBUG_SEL(x)		((u32)(((x >> 0) & 0x0000000F)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_OUT_SMP_DLY
 * @brief
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_OUT_SMP_DLY		((u32)0x00000007 << 0)          /*!<R/W 0x2  Delay cycles for sample of master drived SCL value. The sample of master drived SCL is used for slave stretch. Slave stretch is valid when master drive SCL High, but slave drive SCL low. Valid value of IC_OUT_SMP_DLY is 0-7. Inicate a delay of 1-8 cycles. */
#define I2C_IC_OUT_SMP_DLY(x)		((u32)(((x) & 0x00000007) << 0))
#define I2C_GET_IC_OUT_SMP_DLY(x)	((u32)(((x >> 0) & 0x00000007)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_CLR_ADDR_MATCH
 * @brief Clear Slave Mode Address Match Interrupt Register
 * @{
 *****************************************************************************/
#define I2C_BIT_IC_CLR_ADDR_MATCH	((u32)0x00000001 << 0)          /*!<R 0x0  Read this register to clear the slave mode address match interrupt (bit 12&13) of IC_RAW_INTR_STAT register. */
/** @} */

/**************************************************************************//**
 * @defgroup IC_FILTER
 * @brief I2C Filter Register
 * @{
 *****************************************************************************/
#define I2C_BIT_IC_DIG_FLTR_SEL		((u32)0x00000001 << 8)          /*!<R/W 0x0  1: Enable filter */
#define I2C_MASK_IC_DIG_FLTR_DEG	((u32)0x0000000F << 0)          /*!<R/W 0x0  DIG_FLTR_DEG is to define frequency range of filter. A greater value of DIG_FLTR_DEG results in a slower transfer speed and hardware would be able to filter a lower frequency. */
#define I2C_IC_DIG_FLTR_DEG(x)		((u32)(((x) & 0x0000000F) << 0))
#define I2C_GET_IC_DIG_FLTR_DEG(x)	((u32)(((x >> 0) & 0x0000000F)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_SAR2
 * @brief I2C Slave2 Address Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_SAR2		((u32)0x0000007F << 0)          /*!<R/W 0x12  The IC_SAR2 holds the slave address when the I2C is operating as slave2. Slave2 only supports 7 bit address. The register can be written only when the I2C interface is disabled, which corresponds to the IC_ENABLE register being set to 0. Writes at other times have no effect. */
#define I2C_IC_SAR2(x)			((u32)(((x) & 0x0000007F) << 0))
#define I2C_GET_IC_SAR2(x)		((u32)(((x >> 0) & 0x0000007F)))
/** @} */

/**************************************************************************//**
 * @defgroup IC_COMP_VERSION
 * @brief I2C Component Version Register
 * @{
 *****************************************************************************/
#define I2C_MASK_IC_COMP_VERSION	((u32)0xFFFFFFFF << 0)          /*!<R 0x20200911  I2C version number */
#define I2C_IC_COMP_VERSION(x)		((u32)(((x) & 0xFFFFFFFF) << 0))
#define I2C_GET_IC_COMP_VERSION(x)	((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/** @defgroup I2C_Addr_Mode_definitions
  * @{
  */
#define I2C_ADDR_7BIT			((u32)0x00000000)
#define I2C_ADDR_10BIT			((u32)0x00000001)
#define IS_I2C_ADDR_MODE(MODE)		(((MODE) == I2C_ADDR_7BIT) || \
					((MODE) == I2C_ADDR_10BIT))

/** @defgroup I2C_Speed_Mode_definitions
  * @{
  */
#define I2C_SS_MODE			((u32)0x00000001)
#define I2C_FS_MODE			((u32)0x00000002)
#define I2C_HS_MODE			((u32)0x00000003)
#define IS_I2C_SPEED_MODE(MODE)		(((MODE) == I2C_SS_MODE) || \
					((MODE) == I2C_FS_MODE) || \
					((MODE) == I2C_HS_MODE))

/** @defgroup I2C_Role_definitions
  * @{
  */
#define I2C_SLAVE_MODE			((u32)0x00000000)
#define I2C_MASTER_MODE			((u32)0x00000001)

/** @defgroup I2C_DMA_Mode_definitions
  * @{
  */
#define I2C_DMA_LEGACY			((u32)0x00000000)
#define I2C_DMA_REGISTER		((u32)0x00000001)
#define I2C_DMA_DESCRIPTOR		((u32)0x00000002)
#define IS_I2C_DMA_MODE(MODE)		(((MODE) == I2C_DMA_LEGACY) || \
					((MODE) == I2C_DMA_REGISTER) || \
					((MODE) == I2C_DMA_DESCRIPTOR))

/** @defgroup I2C_DMA_DATA_LENGTH
  * @{
  */
#define IS_I2C_DMA_DATA_LEN(LENGTH)   ((LENGTH) <= 0xFFFF)

/* Other Definitions --------------------------------------------------------*/
//I2C Timing Parameters
#define I2C_SS_MIN_SCL_HTIME		4000	//the unit is ns.
#define I2C_SS_MIN_SCL_LTIME		4700	//the unit is ns.

#define I2C_FS_MIN_SCL_HTIME		600	//the unit is ns.
#define I2C_FS_MIN_SCL_LTIME		1300	//the unit is ns.

#define I2C_HS_MIN_SCL_HTIME_100	60	//the unit is ns, with bus loading = 100pf
#define I2C_HS_MIN_SCL_LTIME_100	120	//the unit is ns., with bus loading = 100pf

#define I2C_HS_MIN_SCL_HTIME_400	160	//the unit is ns, with bus loading = 400pf
#define I2C_HS_MIN_SCL_LTIME_400	320	//the unit is ns., with bus loading = 400pf

#define CONFIG_SUPPORT_I2C_SLAVE	1
#define CONFIG_SUPPORT_POLL_MODE	1
#define I2C_DMA_DATA_TX_LENGTH		255
#define I2C_DMA_DATA_RX_LENGTH		4095

#define BIT0				0x0001
#define BIT1				0x0002
#define BIT2				0x0004
#define BIT3				0x0008
#define BIT4				0x0010
#define BIT5				0x0020
#define BIT6				0x0040
#define BIT7				0x0080
#define BIT8				0x0100
#define BIT9				0x0200
#define BIT10				0x0400
#define BIT11				0x0800
#define BIT12				0x1000
#define BIT13				0x2000
#define BIT14				0x4000
#define BIT15				0x8000
#define BIT16				0x00010000
#define BIT17				0x00020000
#define BIT18				0x00040000
#define BIT19				0x00080000
#define BIT20				0x00100000
#define BIT21				0x00200000
#define BIT22				0x00400000
#define BIT23				0x00800000
#define BIT24				0x01000000
#define BIT25				0x02000000
#define BIT26				0x04000000
#define BIT27				0x08000000
#define BIT28				0x10000000
#define BIT29				0x20000000
#define BIT30				0x40000000
#define BIT31				0x80000000

/**************************************************************************//**
 * @defgroup SR
 * @brief Status Register
 * @{
 *****************************************************************************/
#define BIT_ATWR_RDSR_N			((u32)0x00000001 << 8)	/*!<R/W 0x0  The previous auto write cmd didn't check the status register (RDSR). User should check the status register of Flash before next user mode transaction. ATWR_RDSR_N will only be set by SPIC when SEQ_WR_EN = 1. */
#define BIT_BOOT_FIN			((u32)0x00000001 << 7)	/*!<R 0x0  (Not Yet Ready) Boot Finish. Set if count waiting cycles (Boot Delay Count) for SPI Flash becomes a stable state after power on (or system reset). 1: Boot Finish Note: Auto_mode would be blocked until boot finish. User_mode is allowed with SSIENR inactive before boot finish. */
#define BIT_DCOL			((u32)0x00000001 << 6)	/*!<R 0x0  Data Collision, or in Transmitting Status. 1: Status shows that SPIC is transmitting spi_flash_cmd/spi_flash_addr/spi_flash_data to SPI Flash. Suggest not reading DR during this transmitting state. (Check this status can avoid reading wrong data and cause SPIC error.) */
#define BIT_TXE				((u32)0x00000001 << 5)	/*!<R 0x0  Transmission error. Set if FIFO is empty and starting to transmit data to SPI Flash. This bit is cleared when read. */
#define BIT_RFF				((u32)0x00000001 << 4)	/*!<R 0x0  Receive FIFO full. Set if FIFO is full in receiving mode. This bit is cleared when read. */
#define BIT_RFNE			((u32)0x00000001 << 3)	/*!<R 0x0  Receive FIFO is not empty. Set if FIFO is not empty in receiving mode. This bit is cleared when read. */
#define BIT_TFE				((u32)0x00000001 << 2)	/*!<R 0x1  Transmit FIFO is empty. Set if FIFO is empty in transmit mode, else it is cleared when it has data in FIFO. */
#define BIT_TFNF			((u32)0x00000001 << 1)	/*!<R 0x1  Transmit FIFO is not full. Set if FIFO is not full in transmit mode, else it is cleared when FIFO is full. */
#define BIT_BUSY			((u32)0x00000001 << 0)	/*!<R 0x0  SPIC busy flag. Set if SPIC is still transmitting to or receiving data from SPI Flash, or TX_FIFO/RX_FIFO are not empty. */
/** @} */

//======================================================
// I2C SAL related enumerations
// I2C Extend Features
typedef enum _I2C_EXD_SUPPORT_ {
	I2C_EXD_RESTART			= BIT0,		// I2C RESTART supported, 0: NOT supported, 1: supported
	I2C_EXD_GeneralCall		= BIT1,		// I2C General Call supported, 0: NOT supported, 1: supported
	I2C_EXD_START			= BIT2,		// I2C START Byte supported, 0: NOT supported, 1: supported
	I2C_EXD_Slave_NONACK		= BIT3,		// I2C Slave-No-Ack supported, 0: NOT supported, 1: supported
	I2C_EXD_Bus_Loading		= BIT4,		// I2C bus loading supported, 0: 100pf, 1: 400pf
	I2C_EXD_Slave_Ack_GC		= BIT5,		// I2C slave ack to General Call supported, 0: NOT supported, 1: supported
	I2C_EXD_USER_REG		= BIT6,		// Using User Register Address
	I2C_EXD_USER_REG_2		= BIT6,		// Using User Register Address
	I2C_EXD_MTR_ADDR_RTY		= BIT8,		// Master retries to send start condition and Slave address when the slave doesn't ack the address.
	I2C_EXD_MTR_ADDR_UPD		= BIT9,		// Master dynamically updates slave address
	I2C_EXD_MTR_HOLD_BUS		= BIT10,	// Master doesn't generate STOP when the FIFO is empty. This would make Master hold
} I2C_EXD_SUPPORT, *PI2C_EXD_SUPPORT;

// I2C operation type
typedef enum _I2C_OP_TYPE_ {
	I2C_POLL_TYPE			= 0x0,
	I2C_DMA_TYPE			= 0x1,
	I2C_INTR_TYPE			= 0x2,
} I2C_OP_TYPE, *PI2C_OP_TYPE;

//I2C DMA module number
typedef enum _I2C_DMA_MODULE_SEL_ {
	I2C_DMA_MODULE_0		= 0x0,
	I2C_DMA_MODULE_1		= 0x1
} I2C_DMA_MODULE_SEL, *PI2C_DMA_MODULE_SEL;

// I2C0 DMA peripheral number
typedef enum _I2C0_DMA_PERI_NUM_ {
	I2C0_DMA_TX_NUM			= 0x8,
	I2C0_DMA_RX_NUM			= 0x9,
} I2C0_DMA_PERI_NUM, *PI2C0_DMA_PERI_NUM;

// I2C1 DMA peripheral number
typedef enum _I2C1_DMA_PERI_NUM_ {
	I2C1_DMA_TX_NUM			= 0xA,
	I2C1_DMA_RX_NUM			= 0xB,
} I2C1_DMA_PERI_NUM, *PI2C1_DMA_PERI_NUM;

// I2C0 DMA module used
typedef enum _I2C0_DMA_MODULE_ {
	I2C0_DMA0			= 0x0,
	I2C0_DMA1			= 0x1,
} I2C0_DMA_MODULE, *PI2C0_DMA_MODULE;

// I2C command type
typedef enum _I2C_COMMAND_TYPE_ {
	I2C_WRITE_CMD			= 0x0,
	I2C_READ_CMD			= 0x1,
} I2C_COMMAND_TYPE, *PI2C_COMMAND_TYPE;

// I2C STOP BIT
typedef enum _I2C_STOP_TYPE_ {
	I2C_STOP_DIS			= 0x0,
	I2C_STOP_EN			= 0x1,
} I2C_STOP_TYPE, *PI2C_STOP_TYPE;

// I2C error type
typedef enum _I2C_ERR_TYPE_ {
	I2C_ERR_RX_UNDER		= 0x01,           //I2C RX FIFO Underflow
	I2C_ERR_RX_OVER			= 0x02,           //I2C RX FIFO Overflow
	I2C_ERR_TX_OVER			= 0x04,           //I2C TX FIFO Overflow
	I2C_ERR_TX_ABRT			= 0x08,           //I2C TX terminated
	I2C_ERR_SLV_TX_NACK		= 0x10,           //I2C slave transmission terminated by master NACK,
	//but there are data in slave TX FIFO
	I2C_ERR_USER_REG_TO		= 0x20,
	I2C_ERR_RX_CMD_TO		= 0x21,
	I2C_ERR_RX_FF_TO		= 0x22,
	I2C_ERR_TX_CMD_TO		= 0x23,
	I2C_ERR_TX_FF_TO		= 0x24,
	I2C_ERR_TX_ADD_TO		= 0x25,
	I2C_ERR_RX_ADD_TO		= 0x26,
	I2C_ERR_TX_TO_NOT_SET		= 0x27,
	I2C_ERR_RX_TO_NOT_SET		= 0x28,
} I2C_ERR_TYPE, *PI2C_ERR_TYPE;

// I2C Time Out type
typedef enum _I2C_TIMEOUT_TYPE_ {
	I2C_TRANS_TIMEOUT		= 0,
	I2C_TRANS_NOT_TIMEOUT		= 1,
	I2C_TIMEOOUT_DISABLE		= 2,
	I2C_TIMEOOUT_ENDLESS		= 0xFFFFFFFF,
} I2C_TIMEOUT_TYPE, *PI2C_TIMEOUT_TYPE;

typedef enum  _hal_status {
	HAL_OK				= 0x00,
	HAL_BUSY			= 0x01,
	HAL_TIMEOUT			= 0x02,
	HAL_ERR_PARA			= 0x03,     // error with invaild parameters
	HAL_ERR_MEM			= 0x04,     // error with memory allocation failed
	HAL_ERR_HW			= 0x05,     // error with hardware error
	HAL_ERR_UNKNOWN			= 0xee      // unknown error

} hal_status;

// I2C device status
typedef enum _I2C_Device_STATUS_ {
	I2C_STS_UNINITIAL		= 0x00,
	I2C_STS_INITIALIZED		= 0x01,
	I2C_STS_IDLE			= 0x02,
	I2C_STS_TX_READY		= 0x03,
	I2C_STS_TX_ING			= 0x04,
	I2C_STS_RX_READY		= 0x05,
	I2C_STS_RX_ING			= 0x06,
	I2C_STS_ERROR			= 0x10,
	I2C_STS_TIMEOUT			= 0x11,
	I2C_SYS_SLAVE_READY		= 0x12,
} I2C_Device_STATUS, *PI2C_Device_STATUS;

#define RTK_I2C_SLAVE_EXTEND_FLAGS	0x3
enum rtk_i2c_slave_extend_flags {
	I2C_SLAVE_READ_ONLY,
	I2C_SLAVE_WRITE_ONLY,
	I2C_SLAVE_READ_AND_WRITE,
	I2C_SLAVE_WRITE_AND_READ,
};

#define RTK_I2C_T_RX_FIFO_MAX		16

/*-----------------------------------------------------------------*/
/*-------------------------- Software Layer -----------------------*/
/*-----------------------------------------------------------------*/

/**
  * @brief  I2C Init structure definition
  */
struct rtk_i2c_hw_params {
	struct rtk_i2c_dev	*i2c_dev;

	u32	i2c_index;		/*!< Specifies the I2C Device Index.
					This parameter should be 0 */

	u32	i2c_master;		/*!< Specifies the I2C Role.
					This parameter can be a value of @ref I2C_Role_definitions */

	u32	i2c_addr_mode;		/*!< Specifies the I2C Addressing Mode.
					This parameter can be a value of @ref I2C_Addr_Mode_definitions */

	u32	i2c_speed_mode;		/*!< Specifies the I2C Speed Mode. Now the circuit don't support High Speed Mode.
					This parameter can be a value of @ref I2C_Speed_Mode_definitions */

	u32	i2c_rx_thres;		/*!< Specifies the I2C RX FIFO Threshold. It controls the level of
					entries(or above) that triggers the RX_FULL interrupt.
					This parameter must be set to a value in the 0-255 range. A value of 0 sets
					the threshold for 1 entry, and a value of 255 sets the threshold for 256 entry*/

	u32	i2c_tx_thres;		/*!< Specifies the I2C TX FIFO Threshold.It controls the level of
					entries(or below) that triggers the TX_EMPTY interrupt.
					This parameter must be set to a value in the 0-255 range. A value of 0 sets
					the threshold for 0 entry, and a value of 255 sets the threshold for 255 entry*/
	u32	i2c_mst_restart_en;	/*!< Specifies the I2C Restart Support of Master. */

	u32	i2c_mst_gen_call;	/*!< Specifies the I2C General Call Support of Master. */

	u32	i2c_mst_startb;		/*!< Specifies the I2C Start Byte Support of Master. */

	u32	i2c_slv_nack;		/*!< Specifies the I2C Slave No Ack Support. */

	u32	i2c_slv_ack_gen_call;	/*!< Specifies the I2C Slave Acks to General Call. */

	u32	i2c_ack_addr;		/*!< Specifies the I2C Target Address in I2C Master Mode or
					Ack Address in I2C Slave0 Mode.
					This parameter must be set to a value in the 0-127 range if the I2C_ADDR_7BIT
					is selected or 0-1023 range if the I2C_ADDR_10BIT is selected. */

	u32	i2c_slv_setup;		/*!< Specifies the I2C SDA Setup Time. It controls the amount of time delay
					introduced in the rising edge of SCL—relative to SDA changing—by holding SCL low
					when I2C Device operating as a slave transmitter, in units of ic_clk period.
					This parameter must be set to a value in the 0-255 range. It must be set larger than I2CSdaHd */

	u32	i2c_sda_hold;		/*!< Specifies the I2C SDA Hold Time. It controls the amount of
					hold time on the SDA signal after a negative edge of SCL in both master
					and slave mode, in units of ic_clk period.
					This parameter must be set to a value in the 0-0xFFFF range. */

	u32	i2c_clk;		/*!< Specifies the I2C Bus Clock (in kHz). It is closely related to I2CSpdMod */

	u32	i2c_ip_clk;		/*!< Specifies the I2C IP Clock (in Hz). */

	u32	i2c_filter;		/*!< Specifies the I2C SCL/SDA Spike Filter. */

	u32	i2c_tx_dma_empty_level;	/*!< Specifies the I2C TX DMA Empty Level. dma_tx_req signal is generated when
					the number of valid data entries in the transmit FIFO is equal to or below the DMA
					Transmit Data Level Register. The value of DMA Transmit Data Level Register is equal
					to this value. This parameter must be set to a value in the 0-31 range. */

	u32	i2c_rx_dma_full_level;	/*!< Specifies the I2C RX DMA Full Level. dma_rx_req signal is generated when
					the number of valid data entries in the transmit FIFO is equal to or above the DMA
					Receive Data Level Register. The value of DMA Receive Data Level Register is equal to
					this value+1. This parameter must be set to a value in the 0-31 range. */

	u32	i2c_dma_mode;		/*!< Specifies the I2C DMA Mode.
					This parameter can be a value of @ref I2C_DMA_Mode_definitions */

	u32	i2c_ack_addr1;		/*!< Specifies the I2C Ack Address in I2C Slave1 Mode. I2C Slave1 only
					support I2C_ADDR_7BIT mode. This parameter must be set to a value in the 0-127 range. */
};
/**
  * @}
  */

struct i2c_trans_buf {
	u16			data_len;		//I2C Transmfer Length
	u16			target_addr;		//I2C Target Address. It's only valid in Master Mode.
	u32			reg_addr;		//I2C Register Address. It's only valid in Master Mode.
	u8			*p_data_buf;		//I2C Transfer Buffer Pointer
};

//======================================================
//I2C SAL management adapter
struct i2c_management_adapter {
	volatile u32		master_rd_cmd_cnt;	// Used for Master Read command count
	struct i2c_trans_buf	tx_info;		// Pointer to I2C TX buffer
	struct i2c_trans_buf	rx_info;		// Pointer to I2C RX buffer
	u8			timer_index;		// timer_index allocated by rtk mfd driver
	u32			timeout;
	u32			user_set_timeout;	// I2C IO Timeout count, in ms;
	u8			operation_type;		// I2C operation type selection
	volatile u8		dev_status;		// I2C device status
	u32			i2c_extend;		// I2C extended options: _I2C_EXD_SUPPORT_
};

struct rtk_i2c_slave {
	struct i2c_client		*slave;
	int				slave_id;
};

struct rtk_i2c_slave_dev {
	struct rtk_i2c_slave		*reg_slaves;
	struct rtk_i2c_slave		*current_slave;
	bool				broadcast;
};

struct rtk_i2c_dev {
	struct i2c_adapter		adap;
	struct device			*dev;
	void __iomem			*base;
	struct clk			*clock;
	int				irq;
	struct i2c_management_adapter	i2c_manage;
	struct rtk_i2c_hw_params	i2c_param;

	int				nr_slaves;
	struct rtk_i2c_slave_dev	*slave_dev;
	struct completion	xfer_completion;
};

#ifndef ENABLE
#define ENABLE						1
#endif
#ifndef DISABLE
#define DISABLE						0
#endif
#define FORCE_DISABLE					2

#define SOWFTWARE_MAX_RETRYTIMES			10000
#endif // RTK_I2C_DEFINE

/* core */
#ifndef RTK_I2C_CORE_FUNCTIONS
#define RTK_I2C_CORE_FUNCTIONS
void rtk_i2c_writel(void __iomem *ptr, u32 reg, u32 value);
u32 rtk_i2c_readl(void __iomem *ptr, u32 reg);
void rtk_i2c_reg_update(void __iomem *ptr, u32 reg, u32 mask, u32 value);
void rtk_i2c_enable_cmd(struct rtk_i2c_hw_params *i2c_param, u8 new_state);
u8 rtk_i2c_check_flag_state(struct rtk_i2c_hw_params *i2c_param, u32 i2c_flag);
void rtk_i2c_interrupt_config(struct rtk_i2c_hw_params *i2c_param, u32 i2c_interrupt, u32 new_state);
void rtk_i2c_clear_interrupt(struct rtk_i2c_hw_params *i2c_param, u32 interrupt_bit);
void rtk_i2c_clear_all_interrupts(struct rtk_i2c_hw_params *i2c_param);
u32 rtk_i2c_get_raw_interrupt(struct rtk_i2c_hw_params *i2c_param);
u32 rtk_i2c_get_interrupt(struct rtk_i2c_hw_params *i2c_param);
void rtk_i2c_init_hw(struct rtk_i2c_hw_params *i2c_param);
u8 rtk_i2c_receive_data(struct rtk_i2c_hw_params *i2c_param);
hal_status rtk_i2c_flow_init(struct rtk_i2c_dev *i2c_dev);
hal_status rtk_i2c_flow_deinit(struct rtk_i2c_dev *i2c_dev);
#endif // RTK_I2C_CORE_FUNCTIONS

/* master */
#ifndef RTK_I2C_MASTER_FUNCTIONS
#define RTK_I2C_MASTER_FUNCTIONS
int rtk_i2c_master_probe(struct platform_device *pdev, struct rtk_i2c_dev *i2c_dev, struct i2c_adapter *adap);
bool rtk_i2c_master_irq_wait_timeout(struct rtk_i2c_dev *i2c_dev);
void rtk_i2c_isr_master_handle_rx_full(struct rtk_i2c_dev *i2c_dev);
void rtk_i2c_isr_master_handle_tx_empty(struct rtk_i2c_dev *i2c_dev);
hal_status rtk_i2c_receive_master(struct rtk_i2c_dev *i2c_dev);
#endif // RTK_I2C_MASTER_FUNCTIONS

/* slave */
#ifndef RTK_I2C_SLAVE_FUNCTIONS
#define RTK_I2C_SLAVE_FUNCTIONS
#if IS_ENABLED(CONFIG_I2C_SLAVE_RTK_AMEBA)
int rtk_i2c_slave_probe(struct platform_device *pdev, struct rtk_i2c_dev *i2c_dev, struct i2c_adapter *adap);
void rkt_i2c_isr_slave_handle_res_done(struct rtk_i2c_dev *i2c_dev);
void rtk_i2c_isr_slave_handle_rx_full(struct rtk_i2c_dev *i2c_dev);
void rtk_i2c_isr_slave_handle_rd_req(struct rtk_i2c_dev *i2c_dev);
void rtk_i2c_isr_slave_handle_sar1_wake(struct rtk_i2c_dev *i2c_dev);
void rtk_i2c_isr_slave_handle_sar2_wake(struct rtk_i2c_dev *i2c_dev);
void rtk_i2c_isr_slave_handle_generall_call(struct rtk_i2c_dev *i2c_dev);
#endif // IS_ENABLED(CONFIG_I2C_SLAVE_RTK_AMEBA)
#endif // RTK_I2C_SLAVE_FUNCTIONS
