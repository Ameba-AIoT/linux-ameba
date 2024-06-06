// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DMAC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/slab.h>
#include "../../dma/virt-dma.h"

#define DEBUG
#define RTK_DMAC_DEBUG_DETAILS		0


/* RTK_DMAC_INTERRUPT_COMBINE set 0 means use 8 interrupt callback function. */
/*RTK_DMAC_INTERRUPT_COMBINE set 1 means use 1 interrupt callback function. */
#define RTK_DMAC_INTERRUPT_COMBINE	0

#define RTK_DMAC_RUN_CPU		1
#define RTK_MAX_TXD_PER_VCHAN	8
#define RTK_DMA_HIGH_PERF_CH0	0
#define RTK_DMA_HIGH_PERF_CH1	1


/**************************************************************************/

/************************ RTK_GDMA Hardware Defines ***********************/

/**************************************************************************/

/* GDMA REG BASE not used, write in linux device tree, read only here. */
#define GDMA_REG_BASE			0x400E0000

/* Channel Register Base */
#define RTK_DMA_CHAN_BASE(chan)		(u32)((chan) * 0x58)

#define GDMA_MAX_CHANNEL		8
#define IS_GDMA_ChannelNum(NUM)		((NUM) <= GDMA_MAX_CHANNEL)
//#define IS_GDMA_Index(NUM)		((NUM) <= GDMA_MAX_CHANNEL)

/* Channel Registers Offset
 * (REG Definitions all from RTK_DMAC spec)
 * LOW bytes: [31:0], UP bytes: [63:32].
*/
#define RTK_DMA_CHAN_SAR_L		0x0
#define RTK_DMA_CHAN_DAR_L		0x8
#define RTK_DMA_CHAN_LLP_L		0x10
#define RTK_DMA_CHAN_CTL_L		0x18
#define RTK_DMA_CHAN_SSTAT_L		0x20
#define RTK_DMA_CHAN_DSTAT_L		0x28
#define RTK_DMA_CHAN_SSTATAR_L		0x30
#define RTK_DMA_CHAN_DSTATAR_L		0x38
#define RTK_DMA_CHAN_CFG_L		0x40
#define RTK_DMA_CHAN_SGR_L		0x48
#define RTK_DMA_CHAN_DSR_L		0x50

#define RTK_DMA_CHAN_SAR_U		0x4
#define RTK_DMA_CHAN_DAR_U		0xC
#define RTK_DMA_CHAN_LLP_U		0x14
#define RTK_DMA_CHAN_CTL_U		0x1C	// Read and Write represents different meaning.
#define RTK_DMA_CHAN_SSTAT_U		0x24
#define RTK_DMA_CHAN_DSTAT_U		0x2C
#define RTK_DMA_CHAN_SSTATAR_U		0x34
#define RTK_DMA_CHAN_DSTATAR_U		0x3C
#define RTK_DMA_CHAN_CFG_U		0x44
#define RTK_DMA_CHAN_SGR_U		0x4C
#define RTK_DMA_CHAN_DSR_U		0x54

/* Channel Register Parmeter to Fill in
 * (REG Definitions all from RTK_DMAC spec)
 * x means the parameters for DMA REG above
 */

/* 1. RTK_DMA_CHAN_SAR */
#define BIT_DMA_SAR_L(x)		((u32)(((x) & 0xFFFFFFFF) << 0))

/* 2. RTK_DMA_CHAN_DAR */
#define BIT_DMA_DAR_L(x)		((u32)(((x) & 0xFFFFFFFF) << 0))

/* 3. RTK_DMA_CHAN_LLP */
#define BIT_DMA_LLP_LOC_L(x)		((u32)(((x) & 0x03FFFFFF) << 2))
#define BIT_DMA_LLP_LMS_L(x)		((u32)(((x) & 0x00000003) << 0))

/* 4. RTK_DMA_CHAN_CTL */
#define BIT_DMA_CTL_INT_EN_L(x)		((u32)(((x) & 0x00000001) << 0))
#define BIT_DMA_CTL_DST_TR_WIDTH_L(x)	((u32)(((x) & 0x00000007) << 1))
#define BIT_DMA_CTL_SRC_TR_WIDTH_L(x)	((u32)(((x) & 0x00000007) << 4))
#define BIT_DMA_CTL_DINC_L(x)		((u32)(((x) & 0x00000003) << 7))
#define BIT_DMA_CTL_SINC_L(x)		((u32)(((x) & 0x00000003) << 9))
#define BIT_DMA_CTL_DEST_MSIZE_L(x)	((u32)(((x) & 0x00000007) << 11))
#define BIT_DMA_CTL_SRC_MSIZE_L(x)	((u32)(((x) & 0x00000007) << 14))
#define BIT_DMA_CTL_SRC_GATHER_EN_L(x)	((u32)(((x) & 0x00000001) << 17))
#define BIT_DMA_CTL_DST_SCATTER_EN_L(x)	((u32)(((x) & 0x00000001) << 18))
#define BIT_DMA_CTL_DST_WRNP_EN_L(x)	((u32)(((x) & 0x00000001) << 19))
#define BIT_DMA_CTL_TT_FC_L(x)		((u32)(((x) & 0x00000007) << 20))
#define BIT_DMA_CTL_DMS_L(x)		((u32)(((x) & 0x00000003) << 23))
#define BIT_DMA_CTL_SMS_L(x)		((u32)(((x) & 0x00000003) << 25))
#define BIT_DMA_CTL_LLP_DST_EN_L(x)	((u32)(((x) & 0x00000001) << 27))
#define BIT_DMA_CTL_LLP_SRC_EN_L(x)	((u32)(((x) & 0x00000001) << 28))
#define BIT_DMA_CTL_TRANS_DATA_CNT	0xFFFFFFFF // read only

#define BIT_DMA_CTL_BLOCK_TS_U(x)	((u32)(((x) & 0x0000FFFF) << 0))

/* 5. RTK_DMA_CHAN_SSTAT */
/* 6. RTK_DMA_CHAN_DSTAT */
/* 7. RTK_DMA_CHAN_SSTATAR */
/* 8. RTK_DMA_CHAN_DSTATAR */

/* 9. RTK_DMA_CHAN_CFG */
#define BIT_DMA_CFG_CH_SUSP(x)		((u32)(((x) & 0x00000001) << 8))
#define BIT_DMA_CFG_RELOAD_SRC_L(x)	((u32)(((x) & 0x00000001) << 30))
#define BIT_DMA_CFG_RELOAD_DST_L(x)	((u32)(((x) & 0x00000001) << 31))

#define BIT_DMA_CFG_FCMODE_U(x)		((u32)(((x) & 0x00000001) << 0))
#define BIT_DMA_CFG_FIFO_MODE_U(x)	((u32)(((x) & 0x00000001) << 1))
#define BIT_DMA_CFG_PROTCTL_U(x)	((u32)(((x) & 0x00000001) << 3))
#define BIT_DMA_CFG_SRC_PER_U(x)	((u32)(((x) & 0x0000000F) << 7))
#define BIT_DMA_CFG_DEST_PER_U(x)	((u32)(((x) & 0x0000000F) << 11))

#define BIT_DMA_CFG_SRC_PER1_EXTEND_U(x)	((u32)(((x) & 0x0000000F) << 15))
#define BIT_DMA_CFG_DEST_PER1_EXTEND_U(x)	((u32)(((x) & 0x0000000F) << 16))
#define BIT_DMA_CFG_SRC_PER2_EXTEND_U(x)	((u32)(((x) & 0x0000000F) << 17))
#define BIT_DMA_CFG_DEST_PER2_EXTEND_U(x)	((u32)(((x) & 0x0000000F) << 18))

/* 10. RTK_DMA_CHAN_SGR */
#define BIT_DMA_SGR_SGI_L(x)		((u32)(((x) & 0x000FFFFF) << 0))
#define BIT_DMA_SGR_SGC_L(x)		((u32)(((x) & 0x00000FFF) << 20))

#define BIT_DMA_SGR_SGSN_U(x)		((u32)(((x) & 0x0000FFFF) << 0))
#define BIT_DMA_SGR_BLOCK_TS_U(x)	((u32)(((x) & 0x0000FFFF) << 16))

/* 11. RTK_DMA_CHAN_DGR */
#define BIT_DMA_DGR_DSI_L(x)		((u32)(((x) & 0x000FFFFF) << 0))
#define BIT_DMA_DGR_DSC_L(x)		((u32)(((x) & 0x00000FFF) << 20))

#define BIT_DMA_DGR_DSSN_U(x)		((u32)(((x) & 0x0000FFFF) << 0))

/* Interrupt Registers Offset
 * (REG Definitions all from RTK_DMAC spec)
 LOW bytes: [31:0], UP bytes: [63:32].
 */
#define RTK_DMA_INT_RAW_TFR_L		0x2C0
#define RTK_DMA_INT_RAW_BLOCK_L		0x2C8
#define RTK_DMA_INT_RAW_SRC_TRAN_L	0x2D0
#define RTK_DMA_INT_RAW_DST_TRAN_L	0x2D8
#define RTK_DMA_INT_RAW_ERR_L		0x2E0

#define RTK_DMA_INT_STATUS_TFR_L	0x2E0
#define RTK_DMA_INT_STATUS_BLOCK_L	0x2E8
#define RTK_DMA_INT_STATUS_SRC_TRAN_L	0x2F0
#define RTK_DMA_INT_STATUS_DST_TRAN_L	0x300
#define RTK_DMA_INT_STATUS_ERR_L	0x308

#define RTK_DMA_INT_MASK_TFR_L		0x310
#define RTK_DMA_INT_MASK_BLOCK_L	0x318
#define RTK_DMA_INT_MASK_SRC_TRAN_L	0x320
#define RTK_DMA_INT_MASK_DST_TRAN_L	0x328
#define RTK_DMA_INT_MASK_ERR_L		0x330

#define RTK_DMA_INT_CLEAR_TFR_L		0x338
#define RTK_DMA_INT_CLEAR_BLOCK_L	0x340
#define RTK_DMA_INT_CLEAR_SRC_TRAN_L	0x348
#define RTK_DMA_INT_CLEAR_DST_TRAN_L	0x350
#define RTK_DMA_INT_CLEAR_ERR_L		0x358

/* Other Registers Offset
 * (REG Definitions all from RTK_DMAC spec)
 LOW bytes: [31:0], UP bytes: [63:32].
 */
#define RTK_DMA_COMBIED_INT_STATUS_L	0x360

#define RTK_DMA_EN_L			0x398
#define RTK_DMA_CHANNEL_EN_L		0x3A0
#define RTK_DMA_ID_REGISTER_L		0x3A8
#define RTK_DMA_COMPONENT_ID_REGISTER_L	0x3F8
#define RTK_DMA_OUTSTANDING_NUM_L	0x3B8

/* param 2~6, channel specific read only paramters.
 * (REG Definitions all from RTK_DMAC spec)
 */
#define RTK_DMA_COMP_PARAMS_1_L		0x3F0
#define RTK_DMA_COMP_PARAMS_2_L		0x3E8
#define RTK_DMA_COMP_PARAMS_3_L		0x3E0
#define RTK_DMA_COMP_PARAMS_4_L		0x3D8
#define RTK_DMA_COMP_PARAMS_5_L		0x3D0
#define RTK_DMA_COMP_PARAMS_6_L		0x3C8

#define RTK_DMA_REG_LOW_TO_UP(offset)	(u32)(offset + 0x4)

/*
  @defgroup GDMA_data_transfer_direction GDMA Data Transfer Direction
  */
#define TTFCMemToMem			((u32)0x00000000)
#define TTFCMemToPeri			((u32)0x00000001)
#define TTFCPeriToMem			((u32)0x00000002)
#define TTFCPeriToPeri			((u32)0x00000003)
#define TTFCPeriToMem_PerCtrl		((u32)0x00000004)

#define IS_GDMA_DIR(DIR)		(((DIR) == TTFCMemToMem) || \
					((DIR) == TTFCMemToPeri) || \
					((DIR) == TTFCPeriToMem) ||\
					((DIR) == TTFCPeriToPeri) ||\
					((DIR) == TTFCPeriToMem_PerCtrl))

/**
 @defgroup GDMA_source_data_size GDMA Source Data Size
  */
#define TrWidthOneByte			((u32)0x00000000)
#define TrWidthTwoBytes			((u32)0x00000001)
#define TrWidthFourBytes		((u32)0x00000002)

#define IS_GDMA_DATA_SIZE(SIZE)		(((SIZE) == TrWidthOneByte) || \
					((SIZE) == TrWidthTwoBytes) || \
					((SIZE) == TrWidthFourBytes))

/**
 @defgroup GDMA_Msize GDMA Msize
  */
#define MsizeOne			((u32)0x00000000)
#define MsizeFour			((u32)0x00000001)
#define MsizeEight			((u32)0x00000002)
#define MsizeSixteen			((u32)0x00000003)

#define IS_GDMA_MSIZE(SIZE)		(((SIZE) == MsizeOne) || \
					((SIZE) == MsizeFour) || \
					((SIZE) == MsizeEight)||\
					((SIZE) == MsizeSixteen))

/** @defgroup GDMA_incremented_mode GDMA Source Incremented Mode
  * @{
  */
#define IncType				((u32)0x00000000)
#define DecType				((u32)0x00000001)
#define NoChange			((u32)0x00000002)

#define IS_GDMA_IncMode(STATE) (((STATE) == IncType) || \
					((STATE) == DecType) || \
					((STATE) == NoChange))

/** @defgroup GDMA_interrupts_definition GDMA Interrupts Definition
  * @{
  */
#define TransferType			((u32)0x00000001)
#define BlockType			((u32)0x00000002)
#define SrcTransferType			((u32)0x00000004)
#define DstTransferType			((u32)0x00000008)
#define ErrType				((u32)0x000000010)

#define IS_GDMA_CONFIG_IT(IT) ((((IT) & 0xFFFFFFE0) == 0x00) && ((IT) != 0x00))

/** @defgroup GDMA0_HS_Interface_definition GDMA HandShake Interface Definition
  * @{
  */
#define GDMA_HANDSHAKE_INTERFACE_UART0_TX	(0)
#define GDMA_HANDSHAKE_INTERFACE_UART0_RX	(1)
#define GDMA_HANDSHAKE_INTERFACE_UART1_TX	(2)
#define GDMA_HANDSHAKE_INTERFACE_UART1_RX	(3)
#define GDMA_HANDSHAKE_INTERFACE_UART3_TX	(6)
#define GDMA_HANDSHAKE_INTERFACE_UART3_RX	(7)
#define GDMA_HANDSHAKE_INTERFACE_SPI0_TX	(4)
#define GDMA_HANDSHAKE_INTERFACE_SPI0_RX	(5)
#define GDMA_HANDSHAKE_INTERFACE_SPI1_TX	(6)
#define GDMA_HANDSHAKE_INTERFACE_SPI1_RX	(7)
#define GDMA_HANDSHAKE_INTERFACE_I2C0_TX	(2)
#define GDMA_HANDSHAKE_INTERFACE_I2C0_RX	(3)
#define GDMA_HANDSHAKE_INTERFACE_ADC_RX		(5)
#define GDMA_HANDSHAKE_INTERFACE_AUDIO_TX	(10)
#define GDMA_HANDSHAKE_INTERFACE_AUDIO_RX	(11)
#define GDMA_HANDSHAKE_INTERFACE_USI0_TX	(8)
#define GDMA_HANDSHAKE_INTERFACE_USI0_RX	(9)
#define GDMA_HANDSHAKE_INTERFACE_SGPIO_TX	(4)


/**************************************************************************/

/************************ RTK_GDMA Software Defines ***********************/

/**************************************************************************/

#define RTK_DMA_FRAME_MAX_LENGTH		0xfffff

/**
 @Sarx: Specifies the GDMA channel x Source Address Register (SARx)
	value field of a block descriptor in block chaining.
	This parameter stores the source address of the current block transfer.
 @Darx: Specifies the GDMA channel x Destination Address Register(DARx)
	value field of a block descriptor in block chaining.
	This parameter stores the destination address of the current block transfer.
 @Llpx: Specifies the GDMA channel x Linked List Pointer Register(LLPx)
	value field of a block descriptor in block chaining.
	This parameter is a address, which points to the next block descriptor.
 @CtlxLow: Specifies the GDMA channel x Control Register(CTRx)
	Low 32 bit value field of a block descriptor in block chaining.
	This parameter stores the DMA control parameters of the current block transfer.
 @CtlxUp: Specifies the GDMA channel x Control Register(CTRx)
	High 32 bit value field of a block descriptor in block chaining.
	This parameter stores the DMA control parameters of the current block transfer.
 @Temp Specifies the reserved GDMA channel x register value field of
	a block descriptor in block chaining
 */
/**
 @LliEle: Specifies the GDMA Linked List Item Element structure field
 	of Linked List Item in block chaining. This structure variable
 	stores the necessary parameters of a block descriptor.
 @BlockSize: Specifies the GDMA block size of one block in block chaining.
 	This parameter indicates the block size of the current block transfer.
 @*pNextLli: Specifies the GDMA Linked List Item pointer.
	This parameter stores the address pointing to the next Linked
	List Item in block chaining.
  */
/*
struct rtk_dma_lli_hw {
	u8			channel_index;
	u32			srcx;
	u32			dstx;
	u32			llpx;
	u32			ctlx_low;
	u32			ctlx_up;
	u32			cfgx_low;
	u32			cfgx_up;
	u32			block_size;
	u32			frame_len:20;
	u32			frame_cnt:12;
	u32			src_reload;
	u32			dst_reload;
	u32			next_lli;
};
*/

struct rtk_dma_hw_cfg {
	u32			channel_index;
	u32			srcx;
	u32			dstx;
	u32			llpx;
	u32			ctlx_low;
	u32			ctlx_up;
	u32			cfgx_low;
	u32			cfgx_up;
	u32			block_size;
	u32			src_reload;
	u32			dst_reload;
	u32			next_lli;
	u32			src_inc_type;
	u32			dst_inc_type;
};


/**
 * struct rtk_dma_lli - Link list for dma transfer
 * @hw: hardware link list
 * @phys: physical address of hardware link list
 * @node: node for txd's lli_list
 */
struct rtk_dma_lli {
	struct rtk_dma_hw_cfg	hw;
	dma_addr_t		phys;
	struct list_head	node;
};

/**
 * struct rtk_dma_txd - Wrapper for struct dma_async_tx_descriptor
 * @vd: virtual DMA descriptor
 * @lli_list: link list of lli nodes
 * @cyclic: flag to indicate cyclic transfers
 */
struct rtk_dma_txd {
	struct virt_dma_desc	vd;
	struct list_head	lli_list;
	bool			cyclic;
	bool	allocated;
	bool	is_active;
	struct rtk_dma_vchan	*vchan;
};

enum dma_status_record {
	DMA_PAUSE_REQUSTED,
	DMA_PAUSED_NOW,
	DMA_RESUME_REQUSTED,
	DMA_RESUMED_NOW,
	DMA_ONGOING,
	DMA_COMPLETED,
	DMA_TERMINATE,
};

struct dma_pause_record {
	enum dma_status_record	status;
	int			has_next;
};

/**
 * struct rtk_dma_pchan - Holder for the physical channels
 * @id: physical index to this channel
 * @base: virtual memory base for the dma channel
 * @vchan: the virtual channel currently being served by this physical channel
 */
struct rtk_dma_pchan {
	u32			id;
	void __iomem		*base;
	struct rtk_dma_vchan	*vchan;
	struct dma_pause_record	dma_pause;
};

/*
struct rtk_dma_pbase {
	u32			id;
	void __iomem		*base;
};

struct rtk_dma_vbase {
	u32			id;
};
*/

/**
 * struct rtk_dma_pchan - Wrapper for DMA ENGINE channel
 * @vc: wrappped virtual channel
 * @pchan: the physical channel utilized by this channel
 * @txd: active transaction on this channel
 * @cfg: slave configuration for this channel
 * @drq: physical DMA request ID for this channel
 */
struct rtk_dma_vchan {
	struct virt_dma_chan	vc;
	struct rtk_dma_pchan	*pchan;
	struct rtk_dma_txd	txd[RTK_MAX_TXD_PER_VCHAN];
	struct dma_slave_config cfg;
	u8			drq;
};

/**
 * struct rtk_dma - Holder for the rtk DMA controller
 * @dma: dma engine for this instance
 * @base: virtual memory base for the DMA controller
 * @clk: clock for the DMA controller
 * @lock: a lock to use when change DMA controller global register
 * @lli_pool: a pool for the LLI descriptors
 * @irq: interrupt ID for the DMA controller
 * @nr_pchans: the number of physical channels
 * @pchans: array of data for the physical channels
 * @nr_vchans: the number of physical channels
 * @vchans: array of data for the physical channels
 */
struct rtk_dma {
	struct dma_device		dma;
	void __iomem			*base;
	struct clk			*clk;
	spinlock_t			lock;
	struct dma_pool			*lli_pool;
	int				irq[8];

	unsigned int			nr_pchans;
	struct rtk_dma_pchan		*pchans;

	unsigned int			nr_vchans;
	struct rtk_dma_vchan		*vchans;
};

#ifndef ENABLE
#define ENABLE				1
#endif
#ifndef DISABLE
#define DISABLE				0
#endif

#define DMA_FAIL			0
#define DMA_SUCCESS			1
#define DMA_PROCESS_END			2


