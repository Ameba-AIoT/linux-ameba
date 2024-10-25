// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek DMAC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include "realtek_dmac.h"

/**************************************************************************/

/***************** Directly Read and Write RTK_GDMA Hardware **************/

/**************************************************************************/

static void rsdma_writel(
	void __iomem *ptr, u32 reg, u32 value)
{
	writel(value, (void __iomem *)((u32)ptr + reg));
}

static u32 rsdma_readl(void __iomem *ptr, u32 reg)
{
	return readl((void __iomem *)((u32)ptr + reg));
}

/**************************************************************************/

/************ Indirect Operation to control RTK_GDMA Hardware *************/

/**************************************************************************/
static void rtk_dma_reg_check_all(struct rtk_dma *rsdma, u32 channel)
{
#if RTK_DMAC_DEBUG_DETAILS
	dev_dbg(rsdma->dma.dev, "\n\nrtk_dma_reg_check_all\n\n");
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_SAR_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_SAR_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_DAR_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_DAR_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_LLP_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_LLP_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_CTL_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_CTL_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_CTL_U = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_CTL_U));

	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_SSTAT_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_SSTAT_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_DSTAT_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_DSTAT_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_SSTATAR_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_SSTATAR_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_DSTATAR_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_DSTATAR_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_CFG_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_CFG_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_CFG_U = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_CFG_U));

	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_SGR_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_SGR_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_CHAN_DSR_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHAN_BASE(channel) + RTK_DMA_CHAN_DSR_L));

	dev_dbg(rsdma->dma.dev, "\n");
	dev_dbg(rsdma->dma.dev, "RTK_DMA_INT_MASK_BLOCK_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_INT_MASK_BLOCK_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_INT_MASK_DST_TRAN_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_INT_MASK_DST_TRAN_L));
	dev_dbg(rsdma->dma.dev, "RTK_DMA_INT_MASK_ERR_L = 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_INT_MASK_ERR_L));

	dev_dbg(rsdma->dma.dev, "\n\n");
#endif // RTK_DMAC_DEBUG_DETAILS
}

static void rtk_dma_set_interrupt_mask(
	struct rtk_dma *rsdma,
	u32 channel_num, u8 status)
{
	u32 int_mask = 0;

	if (status == ENABLE) {
		int_mask = BIT(channel_num) | BIT(channel_num + 8);
		rsdma_writel(rsdma->base, RTK_DMA_INT_MASK_BLOCK_L, int_mask);
		rsdma_writel(rsdma->base, RTK_DMA_INT_MASK_DST_TRAN_L, int_mask);
		rsdma_writel(rsdma->base, RTK_DMA_INT_MASK_ERR_L, int_mask);
	} else {
		int_mask = BIT(channel_num + 8);
		rsdma_writel(rsdma->base, RTK_DMA_INT_MASK_BLOCK_L, int_mask);
		rsdma_writel(rsdma->base, RTK_DMA_INT_MASK_DST_TRAN_L, int_mask);
		rsdma_writel(rsdma->base, RTK_DMA_INT_MASK_ERR_L, int_mask);
	}
}

void rtk_dma_clear_channel_interrupts(struct rtk_dma *rsdma, u32 channel_num)
{
	u32 clear_int = 0xff & BIT(channel_num);

	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_BLOCK_L, clear_int);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_SRC_TRAN_L, clear_int);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_DST_TRAN_L, clear_int);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_ERR_L, clear_int);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_TFR_L, clear_int);
}

static void rtk_dma_clear_all_interrupts(struct rtk_dma *rsdma)
{
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_BLOCK_L, 0xff);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_SRC_TRAN_L, 0xff);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_DST_TRAN_L, 0xff);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_ERR_L, 0xff);
	rsdma_writel(rsdma->base, RTK_DMA_INT_CLEAR_TFR_L, 0xff);
}

static void rtk_dma_channel_enable(struct rtk_dma *rsdma, u32 channel_num, u8 enable)
{
	u32 channel_order = 0;
	/* ChEnWE is used to tell HW just set the needed ChEn bit */
	channel_order |= BIT(channel_num + 8);

	if (enable) {
		channel_order |= BIT(channel_num);
		dev_dbg(rsdma->dma.dev, "Channel %d is %s\n", channel_num, enable ? "enable" : "disable");
	} else {
		channel_order &= ~ BIT(channel_num);
	}

	rsdma_writel(rsdma->base, RTK_DMA_CHANNEL_EN_L, channel_order);
}

static void rtk_dma_prepare_cmd(
	struct rtk_dma *rsdma,
	struct rtk_dma_hw_cfg *hw_cfg,
	bool interrupt_en)
{
	/* 1. Set this DMA non-secure. */
	rsdma_writel(rsdma->base, RTK_DMA_CHAN_CFG_U, BIT_DMA_CFG_PROTCTL_U(0));

	/* 2. Enable RTK_DMA. */
	rsdma_writel(rsdma->base, RTK_DMA_EN_L, 1);

	/* 3. Disable the preparing channel. */
	rtk_dma_channel_enable(rsdma, hw_cfg->channel_index, DISABLE);

	/* 4. Set dma interrupt mask for this transmit. */
	if (interrupt_en) {
		rtk_dma_set_interrupt_mask(rsdma, hw_cfg->channel_index, ENABLE);
	} else {
		rtk_dma_set_interrupt_mask(rsdma, hw_cfg->channel_index, DISABLE);
	}

	/* 5. Set src addr and dst addr*/
	rsdma_writel(rsdma->base, RTK_DMA_CHAN_BASE(hw_cfg->channel_index) + RTK_DMA_CHAN_SAR_L, hw_cfg->srcx);
	rsdma_writel(rsdma->base, RTK_DMA_CHAN_BASE(hw_cfg->channel_index) + RTK_DMA_CHAN_DAR_L, hw_cfg->dstx);

	/* 6. Set ctl and cfg regs. */
	rsdma_writel(rsdma->base, RTK_DMA_CHAN_BASE(hw_cfg->channel_index) + RTK_DMA_CHAN_CTL_L, hw_cfg->ctlx_low);
	rsdma_writel(rsdma->base, RTK_DMA_CHAN_BASE(hw_cfg->channel_index) + RTK_DMA_CHAN_CTL_U, hw_cfg->ctlx_up);

	rsdma_writel(rsdma->base, RTK_DMA_CHAN_BASE(hw_cfg->channel_index) + RTK_DMA_CHAN_CFG_L, hw_cfg->cfgx_low);
	rsdma_writel(rsdma->base, RTK_DMA_CHAN_BASE(hw_cfg->channel_index) + RTK_DMA_CHAN_CFG_U, hw_cfg->cfgx_up);
}

/**************************************************************************/

/************************* Software Layer Operations **********************/

/**************************************************************************/

static inline struct rtk_dma *to_rtk_dma(struct dma_device *dd)
{
	return container_of(dd, struct rtk_dma, dma);
}

static inline struct rtk_dma_vchan *to_rtk_vchan(struct dma_chan *chan)
{
	return container_of(chan, struct rtk_dma_vchan, vc.chan);
}

struct rtk_dma_txd *rtk_dma_find_unused_txd(struct rtk_dma_vchan *vchan)
{
	struct rtk_dma_txd *txd = NULL;
	int cnt_txd;

	for (cnt_txd = 0; cnt_txd < RTK_MAX_TXD_PER_VCHAN; cnt_txd++) {
		if (vchan->txd[cnt_txd].allocated == false) {
			txd = &vchan->txd[cnt_txd];
			memset(txd, 0, sizeof(struct rtk_dma_txd));
			txd->allocated = true;
			txd->is_active = true;
			txd->vchan = vchan;
			break;
		}
	}

	return txd;
}

struct rtk_dma_txd *rtk_dma_find_vchan_txd(struct rtk_dma_vchan *vchan)
{
	struct rtk_dma_txd *txd = NULL;
	int cnt_txd;

	for (cnt_txd = 0; cnt_txd < RTK_MAX_TXD_PER_VCHAN; cnt_txd++) {
		if ((vchan->txd[cnt_txd].allocated == true) && (vchan->txd[cnt_txd].vchan == vchan) && (vchan->txd[cnt_txd].is_active == true)) {
			txd = &vchan->txd[cnt_txd];
			break;
		}
	}

	return txd;
}

struct rtk_dma_txd *rtk_dma_find_vd_txd(struct virt_dma_desc *vd)
{
	struct rtk_dma_vchan *vchan = NULL;
	struct rtk_dma *rsdma = NULL;
	struct rtk_dma_txd *txd = NULL;
	int cnt_vchans, cnt_txd;

	rsdma = to_rtk_dma(vd->tx.chan->device);

	for (cnt_vchans = 0; cnt_vchans < rsdma->nr_vchans; cnt_vchans++) {
		vchan = &rsdma->vchans[cnt_vchans];
		for (cnt_txd = 0; cnt_txd < RTK_MAX_TXD_PER_VCHAN; cnt_txd++) {
			if (&vchan->txd[cnt_txd].vd == vd) {
				txd = &vchan->txd[cnt_txd];
				break;
			}
		}
	}

	return txd;
}

static void rtk_dma_free_lli(
	struct rtk_dma *rsdma,
	struct rtk_dma_lli *lli)
{
	list_del(&lli->node);
	dma_pool_free(rsdma->lli_pool, lli, lli->phys);
}

static struct rtk_dma_lli *rtk_dma_alloc_lli(struct rtk_dma *rsdma)
{
	struct rtk_dma_lli *lli;
	dma_addr_t phys;

	lli = dma_pool_alloc(rsdma->lli_pool, GFP_NOWAIT, &phys);
	if (!lli) {
		return NULL;
	}

	INIT_LIST_HEAD(&lli->node);
	lli->phys = phys;

	return lli;
}

static struct rtk_dma_lli *rtk_dma_add_lli(struct rtk_dma_txd *txd,
		struct rtk_dma_lli *prev,
		struct rtk_dma_lli *next,
		bool is_cyclic)
{
	if (!is_cyclic) {
		list_add_tail(&next->node, &txd->lli_list);
	}

	if (prev) {
		prev->hw.next_lli = next->phys;

	}

	return next;
}

static u32 to_rtk_width(struct rtk_dma *rsdma, u32 width)
{
	switch (width) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
		return TrWidthOneByte;
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
		return TrWidthTwoBytes;
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
		return TrWidthFourBytes;
	default:
		dev_err(rsdma->dma.dev, "DMA width error\n");
		return TrWidthOneByte;
	}
}

static u32 to_rtk_msize(struct rtk_dma *rsdma, u32 msize)
{
	switch (msize) {
	case 1:
		return MsizeOne;
	case 4:
		return MsizeFour;
	case 8:
		return MsizeEight;
	case 16:
		return MsizeSixteen;
	default:
		dev_err(rsdma->dma.dev, "DMA msize error\n");
		return MsizeFour;
	}
}

static inline int rtk_dma_hw_cfg_lli(
	struct rtk_dma_vchan *vchan,
	struct rtk_dma_lli *lli,
	dma_addr_t src, dma_addr_t dst,
	u32 len, enum dma_transfer_direction dir,
	struct dma_slave_config *sconfig,
	struct rtk_dma_txd *txd,
	bool interrupt_en)
{
	struct rtk_dma *rsdma = to_rtk_dma(vchan->vc.chan.device);
	struct rtk_dma_hw_cfg *hw_cfg = &lli->hw;
	bool is_cyclic = txd->cyclic;

	u8 secure = 0;
	u8 src_handshake_interface = 0; //dev has.
	u8 dst_handshake_interface = 0;
	u32 dst_width = sconfig->dst_addr_width;
	u32 src_width = sconfig->src_addr_width;
	u32 dst_msize = sconfig->dst_maxburst;
	u32 src_msize = sconfig->src_maxburst;
	u8 src_handshake_add1 = 0;
	u8 src_handshake_add2 = 0;
	u8 dst_handshake_add1 = 0;
	u8 dst_handshake_add2 = 0;

	switch (dir) {
	case DMA_DEV_TO_MEM:
		if (is_cyclic) {
			hw_cfg->src_inc_type = NoChange;
			hw_cfg->dst_inc_type = IncType;
		}
		if (sconfig->slave_id < 16) {
			src_handshake_interface = sconfig->slave_id;
		} else if (sconfig->slave_id >= 16 && sconfig->slave_id < 32) {
			src_handshake_add1 = 1;
			src_handshake_interface = sconfig->slave_id - 16;
		} else if (sconfig->slave_id >= 32) {
			src_handshake_add2 = 1;
			if (sconfig->slave_id >= 48) {
				src_handshake_add1 = 1;
			}
			src_handshake_interface = sconfig->slave_id - 32;
		}
		break;
	case DMA_MEM_TO_DEV:
		if (is_cyclic) {
			hw_cfg->src_inc_type = IncType;
			hw_cfg->dst_inc_type = NoChange;
		}
		if (sconfig->slave_id < 16) {
			dst_handshake_interface = sconfig->slave_id;
		} else if (sconfig->slave_id >= 16 && sconfig->slave_id < 32) {
			dst_handshake_add1 = 1;
			dst_handshake_interface = sconfig->slave_id - 16;
		} else if (sconfig->slave_id >= 32) {
			dst_handshake_add2 = 1;
			if (sconfig->slave_id >= 48) {
				dst_handshake_add1 = 1;
			}
			dst_handshake_interface = sconfig->slave_id - 32;
		}
		break;
	case DMA_MEM_TO_MEM:
		/* When doing memory to memory dma, please keep copy size of dst and src the same. */
		hw_cfg->src_inc_type = IncType;
		hw_cfg->dst_inc_type = IncType;
		dst_handshake_interface = 0;
		break;
	default:
		break;
	}

	dst_width = to_rtk_width(rsdma, dst_width);
	src_width = to_rtk_width(rsdma, src_width);
	dst_msize = to_rtk_msize(rsdma, dst_msize);
	src_msize = to_rtk_msize(rsdma, src_msize);

	//hw_cfg->channel_index = vchan->vc.chan.chan_id;
	hw_cfg->srcx = src;
	hw_cfg->dstx = dst;

	/* block size controled by dma slave. */
	if (sconfig->dst_port_window_size && dir == DMA_DEV_TO_MEM) {
		hw_cfg->block_size = sconfig->dst_port_window_size / (sconfig->dst_addr_width * sconfig->dst_maxburst);
	} else if (sconfig->src_port_window_size && dir == DMA_MEM_TO_DEV) {
		hw_cfg->block_size = sconfig->src_port_window_size / (sconfig->src_addr_width * sconfig->src_maxburst);
	} else {
		/* BLOCK TS controled by single block. */
		/* single block tx size = block length / src width. */
		hw_cfg->block_size = len / sconfig->src_addr_width;
	}

	/* auto-reload mode. */
	if (!interrupt_en) {
		hw_cfg->src_reload = 1;
		hw_cfg->dst_reload = 1;
	} else {
		hw_cfg->src_reload = 0;
		hw_cfg->dst_reload = 0;
	}

	dev_dbg(rsdma->dma.dev, "Config block size = %d\n", hw_cfg->block_size);

	/* combine ctl and cfg regs */
	hw_cfg->ctlx_up = BIT_DMA_CTL_BLOCK_TS_U(hw_cfg->block_size);
	hw_cfg->ctlx_low = BIT_DMA_CTL_INT_EN_L(1) |
					   BIT_DMA_CTL_DST_TR_WIDTH_L(dst_width) |
					   BIT_DMA_CTL_SRC_TR_WIDTH_L(src_width) |
					   BIT_DMA_CTL_DINC_L(hw_cfg->dst_inc_type) |
					   BIT_DMA_CTL_SINC_L(hw_cfg->src_inc_type) |
					   BIT_DMA_CTL_DEST_MSIZE_L(dst_msize) |
					   BIT_DMA_CTL_SRC_MSIZE_L(src_msize) |
					   BIT_DMA_CTL_SRC_GATHER_EN_L(0) |
					   BIT_DMA_CTL_DST_SCATTER_EN_L(0) |
					   BIT_DMA_CTL_DST_WRNP_EN_L(0) |
					   BIT_DMA_CTL_TT_FC_L(dir) |
					   BIT_DMA_CTL_DMS_L(0) |
					   BIT_DMA_CTL_SMS_L(0) |
					   BIT_DMA_CTL_LLP_DST_EN_L(0) |
					   BIT_DMA_CTL_LLP_SRC_EN_L(0);

	hw_cfg->cfgx_up = BIT_DMA_CFG_FCMODE_U(sconfig->device_fc) |
					  BIT_DMA_CFG_FIFO_MODE_U(0) |
					  BIT_DMA_CFG_PROTCTL_U(secure) |
					  BIT_DMA_CFG_SRC_PER_U(src_handshake_interface) |
					  BIT_DMA_CFG_DEST_PER_U(dst_handshake_interface) |
					  BIT_DMA_CFG_SRC_PER1_EXTEND_U(src_handshake_add1) |
					  BIT_DMA_CFG_SRC_PER2_EXTEND_U(src_handshake_add2) |
					  BIT_DMA_CFG_DEST_PER1_EXTEND_U(dst_handshake_add1) |
					  BIT_DMA_CFG_DEST_PER1_EXTEND_U(dst_handshake_add2);
	hw_cfg->cfgx_low = BIT_DMA_CFG_RELOAD_SRC_L(hw_cfg->src_reload) |
					   BIT_DMA_CFG_RELOAD_DST_L(hw_cfg->dst_reload);

	return 0;
}

static int rtk_dma_pchan_busy(
	struct rtk_dma *rsdma, struct rtk_dma_pchan *pchan)
{
	unsigned int val;

	val = rsdma_readl(rsdma->base, RTK_DMA_CHANNEL_EN_L);

	return (val & (1 << pchan->id));
}

static void rtk_dma_terminate_pchan(
	struct rtk_dma *rsdma, struct rtk_dma_pchan *pchan)
{
	u32 interrupt_block_end = 0;
	u32 interrupt_error = 0;
	u32 temp;

	dev_dbg(rsdma->dma.dev, "DMA terminate pchan %d\n", pchan->id);

	/* suspend and disable the chan. */
	temp = rsdma_readl(pchan->base, RTK_DMA_CHAN_CFG_L);
	rsdma_writel(pchan->base, RTK_DMA_CHAN_CFG_L, temp | BIT_DMA_CFG_CH_SUSP(1));

	rtk_dma_channel_enable(rsdma, pchan->id, DISABLE);

	interrupt_block_end = rsdma_readl(rsdma->base, RTK_DMA_INT_RAW_BLOCK_L) & (BIT(1) << pchan->id);
	interrupt_error = rsdma_readl(rsdma->base, RTK_DMA_INT_RAW_ERR_L) & (BIT(1) << pchan->id);

	if (interrupt_block_end || interrupt_error) {
		dev_warn(rsdma->dma.dev,
				 "Terminate pchan %d with unresolved interrupt %s\n",
				 pchan->id, interrupt_block_end ? "success" : "error");
	}

	rtk_dma_clear_channel_interrupts(rsdma, pchan->id);
}

static int rtk_dma_start_next_txd(struct rtk_dma_vchan *vchan, u8 last_lli)
{
	struct rtk_dma *rsdma = to_rtk_dma(vchan->vc.chan.device);
	struct virt_dma_desc *vd = vchan_next_desc(&vchan->vc);
	struct rtk_dma_pchan *pchan = vchan->pchan;
	struct rtk_dma_txd *txd;
	struct rtk_dma_lli *lli;
	int pchan_wait_timeout = 0;

	txd = rtk_dma_find_vchan_txd(vchan);
	if (!txd) {
		dev_dbg(rsdma->dma.dev, "Unable to find TXD for channel %d\n", vchan->vc.chan.chan_id);
		return -1;
	}

	if (last_lli) {
		list_del(&vd->node);
	}

	vchan->pchan->dma_pause.status = DMA_ONGOING;

	/* Wait for channel inactive */
	while (rtk_dma_pchan_busy(rsdma, pchan)) {
		cpu_relax();
		pchan_wait_timeout++;
		if (pchan_wait_timeout % 10 == 0) {
			dev_dbg(rsdma->dma.dev, "Vchan %d is using pchan %d, please release this vchan first\n", vchan->vc.chan.chan_id, pchan->id);
		}
		if (pchan_wait_timeout == 100) {
			dev_warn(rsdma->dma.dev, "Pchan %d keeps busy\n", pchan->id);
			return -1;
		}
	}

	lli = list_first_entry(&txd->lli_list, struct rtk_dma_lli, node);

	/* Clear IRQ status for this pchan */
	rtk_dma_clear_channel_interrupts(rsdma, pchan->id);

	lli->hw.channel_index = pchan->id;
	dev_dbg(rsdma->dma.dev, "Link list lli->hw.channel_index = %d\n", lli->hw.channel_index);

	rtk_dma_prepare_cmd(rsdma, &lli->hw, vd->tx.flags & DMA_PREP_INTERRUPT);

	dev_dbg(rsdma->dma.dev, "Starting pchan %d\n", pchan->id);

	rtk_dma_reg_check_all(rsdma, pchan->id);

	/* Start DMA transfer for this pchan */
	rtk_dma_channel_enable(rsdma, pchan->id, ENABLE);
	return 0;
}

static void rtk_dma_free_txd(struct rtk_dma *rsdma, struct rtk_dma_txd *txd)
{
	struct rtk_dma_lli *lli = NULL;
	struct rtk_dma_lli *_lli = NULL;

	if (unlikely(!txd)) {
		return;
	}

	if (txd->allocated) {

		list_for_each_entry_safe(lli, _lli, &txd->lli_list, node) {
			rtk_dma_free_lli(rsdma, lli);
		}

		txd->allocated = false;
	}
}

static void rtk_dma_phy_free(struct rtk_dma *rsdma, struct rtk_dma_vchan *vchan)
{
	/* Ensure that the physical channel is stopped */
	/* Other interruptions. */

	dev_dbg(rsdma->dma.dev, "Channel usage see 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHANNEL_EN_L));

	if (!vchan->pchan) {
		/* This is expected coz pchan can be freed from both
		 * free_chan_resources & terminate_all API's */
		return;
	}

	rtk_dma_set_interrupt_mask(rsdma, vchan->pchan->id, DISABLE);

	if ((vchan->pchan->id < GDMA_MAX_CHANNEL) &&
		(rsdma_readl(rsdma->base, RTK_DMA_CHANNEL_EN_L) &
		 BIT(vchan->pchan->id))) {
		dev_dbg(rsdma->dma.dev, "Terminate pchan\n");
		rtk_dma_terminate_pchan(rsdma, vchan->pchan);
	}
}

static void rtk_dma_phy_terminate(struct rtk_dma *rsdma, struct rtk_dma_vchan *vchan)
{
	/* Ensure that the physical channel is stopped */
	/* Other interruptions. */

	dev_dbg(rsdma->dma.dev, "Channel usage see 0x%08X\n", rsdma_readl(rsdma->base, RTK_DMA_CHANNEL_EN_L));

	if (!vchan->pchan) {
		return;
	}

	rtk_dma_set_interrupt_mask(rsdma, vchan->pchan->id, DISABLE);

	if ((vchan->pchan->id < GDMA_MAX_CHANNEL) &&
		(rsdma_readl(rsdma->base, RTK_DMA_CHANNEL_EN_L) &
		 BIT(vchan->pchan->id))) {
		dev_dbg(rsdma->dma.dev, "Terminate pchan\n");
		rtk_dma_terminate_pchan(rsdma, vchan->pchan);
	}
}

static void rtk_dma_success(struct rtk_dma *rsdma, u32 channel_num)
{
	struct rtk_dma_vchan *vchan;
	struct rtk_dma_pchan *pchan;
	struct rtk_dma_txd *txd;
	struct rtk_dma_lli *lli = NULL;
	struct rtk_dma_lli *temp = NULL;
	u8 has_next = 0;
	u8 delete_prev = 0;

	pchan = &rsdma->pchans[channel_num];

	vchan = pchan->vchan;
	if (!vchan) {
		dev_warn(rsdma->dma.dev, "No vchan attached on pchan %d\n", pchan->id);
		rtk_dma_channel_enable(rsdma, channel_num, DISABLE);
		rtk_dma_clear_channel_interrupts(rsdma, channel_num);
		return;
	}

	spin_lock(&vchan->vc.lock);

	txd = rtk_dma_find_vchan_txd(vchan);
	if (!txd) {
		dev_warn(rsdma->dma.dev, "Unable to find TXD for channel %d\n", vchan->vc.chan.chan_id);
		goto END_SUCCESS;
	}

	list_for_each_entry_safe(lli, temp, &txd->lli_list, node) {
		if (delete_prev == 0) {
			rtk_dma_free_lli(rsdma, lli);
			delete_prev++;
		}
	}

	list_for_each_entry(lli, &txd->lli_list, node) {
		has_next++;
	}

	/*
	 * Start the next descriptor (if any),
	 * otherwise free this channel.
	 */
	if (has_next) {
		if (vchan->pchan->dma_pause.status == DMA_TERMINATE) {
			has_next = 0;
			goto END_SUCCESS;
		} else if (vchan->pchan->dma_pause.status == DMA_PAUSE_REQUSTED) {
			vchan->pchan->dma_pause.status = DMA_PAUSED_NOW;
			vchan->pchan->dma_pause.has_next = has_next;
			has_next = 0;
			goto END_SUCCESS;
		} else if (vchan->pchan->dma_pause.status == DMA_PAUSED_NOW) {
			dev_err(rsdma->dma.dev, "DMA has been paused, but still online\n");
			has_next = 0;
			goto END_SUCCESS;
		}
		if (has_next == 1) {
			/* Last txd. */
			rtk_dma_start_next_txd(vchan, 1);
		} else {
			rtk_dma_start_next_txd(vchan, 0);
		}
	} else {
		/* Clear IRQ status for this pchan */
		if (vchan->pchan->dma_pause.status == DMA_TERMINATE) {
			goto END_SUCCESS;
		} else if (vchan->pchan->dma_pause.status == DMA_PAUSE_REQUSTED) {
			vchan->pchan->dma_pause.status = DMA_PAUSED_NOW;
			vchan->pchan->dma_pause.has_next = 0;
		} else if (vchan->pchan->dma_pause.status == DMA_PAUSED_NOW) {
			dev_err(rsdma->dma.dev, "DMA has been paused, but still online\n");
			goto END_SUCCESS;
		}
		txd->vd.tx_result.result = DMA_TRANS_NOERROR;
		vchan->pchan->dma_pause.status = DMA_COMPLETED;
		/* for txd callback function. */
		txd->is_active = false;
		vchan_cookie_complete(&txd->vd);
	}

END_SUCCESS:
	if (!has_next) {
		rtk_dma_clear_channel_interrupts(rsdma, channel_num);
	}
	spin_unlock(&vchan->vc.lock);
}

static void rtk_dma_fail(struct rtk_dma *rsdma, u32 channel_num)
{
	struct rtk_dma_vchan *vchan;
	struct rtk_dma_pchan *pchan;
	struct rtk_dma_txd *txd;

	pchan = &rsdma->pchans[channel_num];

	vchan = pchan->vchan;
	if (!vchan) {
		dev_warn(rsdma->dma.dev, "No vchan attached on pchan %d\n", pchan->id);
		rtk_dma_channel_enable(rsdma, channel_num, DISABLE);
		rtk_dma_clear_channel_interrupts(rsdma, channel_num);
		return;
	}

	spin_lock(&vchan->vc.lock);
	rtk_dma_channel_enable(rsdma, channel_num, DISABLE);
	rtk_dma_clear_channel_interrupts(rsdma, channel_num);

	txd = rtk_dma_find_vchan_txd(vchan);
	if (!txd) {
		dev_warn(rsdma->dma.dev, "Unable to find TXD for channel %d\n", vchan->vc.chan.chan_id);
		spin_unlock(&vchan->vc.lock);
		return;
	}

	txd->vd.tx_result.result = DMA_TRANS_WRITE_FAILED;
	vchan_cookie_complete(&txd->vd);
	spin_unlock(&vchan->vc.lock);
}

#if RTK_DMAC_INTERRUPT_COMBINE
static u8 rtk_dma_interrupt_check(struct rtk_dma *rsdma, u32 *channel)
{
	u32 interrupt_block_end;
	u32 interrupt_error;
	u32 i;

	interrupt_block_end = rsdma_readl(rsdma->base, RTK_DMA_INT_RAW_BLOCK_L);
	interrupt_error = rsdma_readl(rsdma->base, RTK_DMA_INT_RAW_ERR_L);

	for (i = 0; i < 8; i++) {
		if (interrupt_block_end & BIT(i)) {
			*channel = i;
			return DMA_SUCCESS;
		}
		if (interrupt_error & BIT(i)) {
			*channel = i;
			return DMA_FAIL;
		}
	}
	return DMA_PROCESS_END;

}

static irqreturn_t rtk_dma_interrupt(int irq, void *dev_id)
{
	struct rtk_dma *rsdma = dev_id;
	int i;
	u8 pending_int_check;
	u32 channel;

	spin_lock(&rsdma->lock);

	/* max check times = GDMA_MAX_CHANNEL, usually < GDMA_MAX_CHANNEL */
	for (i = 0; i < GDMA_MAX_CHANNEL; i++) {

		pending_int_check = rtk_dma_interrupt_check(rsdma, &channel);

		if (pending_int_check == DMA_SUCCESS) {
			rtk_dma_success(rsdma, channel);
			break;
		} else if (pending_int_check == DMA_FAIL) {
			rtk_dma_fail(rsdma, channel);
			break;
		} else if (pending_int_check == DMA_PROCESS_END) {
			/* All concerned pending irq solved. */
			rtk_dma_clear_all_interrupts(rsdma);
			break;
		}
	}

	spin_unlock(&rsdma->lock);
	return IRQ_HANDLED;
}

#else // RTK_DMAC_INTERRUPT_COMBINE
static void rtk_dma_interrupt_check_ch(void *dev_id, u32 channel)
{
	struct rtk_dma *rsdma = dev_id;
	u32 interrupt_block_end;
	u32 interrupt_error;

	spin_lock(&rsdma->lock);

	interrupt_block_end = rsdma_readl(rsdma->base, RTK_DMA_INT_RAW_BLOCK_L);
	interrupt_error = rsdma_readl(rsdma->base, RTK_DMA_INT_RAW_ERR_L);

	if (interrupt_block_end & BIT(channel)) {
		rtk_dma_success(rsdma, channel);
	} else if (interrupt_error & BIT(channel)) {
		rtk_dma_fail(rsdma, channel);
	} else {
		rtk_dma_clear_channel_interrupts(rsdma, channel);
	}

	spin_unlock(&rsdma->lock);

}

static irqreturn_t rtk_dma_interrupt_ch0(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 0);
	return IRQ_HANDLED;
}

static irqreturn_t rtk_dma_interrupt_ch1(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 1);
	return IRQ_HANDLED;
}

static irqreturn_t rtk_dma_interrupt_ch2(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 2);
	return IRQ_HANDLED;
}

static irqreturn_t rtk_dma_interrupt_ch3(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 3);
	return IRQ_HANDLED;
}

static irqreturn_t rtk_dma_interrupt_ch4(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 4);
	return IRQ_HANDLED;
}

static irqreturn_t rtk_dma_interrupt_ch5(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 5);
	return IRQ_HANDLED;
}

static irqreturn_t rtk_dma_interrupt_ch6(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 6);
	return IRQ_HANDLED;
}

static irqreturn_t rtk_dma_interrupt_ch7(int irq, void *dev_id)
{
	rtk_dma_interrupt_check_ch(dev_id, 7);
	return IRQ_HANDLED;
}
#endif // RTK_DMAC_INTERRUPT_COMBINE

static void rtk_dma_desc_free(struct virt_dma_desc *vd)
{
	struct rtk_dma *rsdma = NULL;
	struct rtk_dma_txd *txd = NULL;

	if (!vd || !vd->tx.chan) {
		return;
	}

	rsdma = to_rtk_dma(vd->tx.chan->device);
	txd = rtk_dma_find_vd_txd(vd);
	if (txd) {
		rtk_dma_free_txd(rsdma, txd);
	} else {
		dev_warn(rsdma->dma.dev, "Input vd has no TXD\n");
	}
}

static int rtk_dma_terminate_all(struct dma_chan *chan)
{
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	unsigned long flags;
	int i;
	LIST_HEAD(head);

	spin_lock_irqsave(&vchan->vc.lock, flags);

	/* Set TERMINATE before release all txdesc. */
	vchan->pchan->dma_pause.status = DMA_TERMINATE;

	for (i = 0; i < RTK_MAX_TXD_PER_VCHAN; i++) {
		if (vchan->txd[i].allocated == true) {
			rtk_dma_free_txd(rsdma, &vchan->txd[i]);
		}
	}

	vchan_get_all_descriptors(&vchan->vc, &head);

	vchan_dma_desc_free_list(&vchan->vc, &head);

	/* restore to init value */
	rtk_dma_phy_terminate(rsdma, vchan);

	dev_dbg(rsdma->dma.dev, "Terminate channel %d\n",	vchan->pchan->id);

	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	return 0;
}

static int rtk_dma_config(struct dma_chan *chan, struct dma_slave_config *config)
{
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);

	/* Reject definitely invalid configurations */
	if (config->src_addr_width == DMA_SLAVE_BUSWIDTH_8_BYTES ||
		config->dst_addr_width == DMA_SLAVE_BUSWIDTH_8_BYTES) {
		return -EINVAL;
	}

	memcpy(&vchan->cfg, config, sizeof(struct dma_slave_config));

	return 0;
}

static int rtk_dma_pause(struct dma_chan *chan)
{
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);
	struct rtk_dma_txd *txd;
	unsigned long flags;
	u32 temp;

	spin_lock_irqsave(&vchan->vc.lock, flags);

	if (!vchan->pchan) {
		dev_dbg(rsdma->dma.dev, "Vchan %p has no pchan, do not need to pause\n",
				&vchan->vc);
		goto END_PAUSE_PROCESS;
	}

	txd = rtk_dma_find_vchan_txd(vchan);

	if ((txd) && (txd->vd.tx.flags & DMA_PREP_INTERRUPT)) {
		if (vchan->pchan->dma_pause.status != DMA_ONGOING) {
			dev_dbg(rsdma->dma.dev, "Vchan %p DMA status before pause-requst is 0x%08X\n",
					 &vchan->vc, vchan->pchan->dma_pause.status);
		}
		vchan->pchan->dma_pause.status = DMA_PAUSE_REQUSTED;
	} else {
		/* Suspend the auto-reload chan. */
		temp = rsdma_readl(vchan->pchan->base, RTK_DMA_CHAN_CFG_L);
		rsdma_writel(vchan->pchan->base, RTK_DMA_CHAN_CFG_L, temp | BIT_DMA_CFG_CH_SUSP(1));
		vchan->pchan->dma_pause.status = DMA_PAUSED_NOW;
		rtk_dma_reg_check_all(rsdma, vchan->pchan->id);
	}

END_PAUSE_PROCESS:
	spin_unlock_irqrestore(&vchan->vc.lock, flags);

	return 0;
}

static int rtk_dma_resume(struct dma_chan *chan)
{
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	struct rtk_dma_txd *txd;
	unsigned long flags;
	u32 temp;
	int ret = 0;
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);

	spin_lock_irqsave(&vchan->vc.lock, flags);

	if (!vchan->pchan) {
		dev_err(rsdma->dma.dev, "Vchan %p has no pchan, cannot pause\n",
				&vchan->vc);
		ret = -EFAULT;
		goto END_RESUME_PROCESS;
	}

	dev_dbg(rsdma->dma.dev, "Vchan %p resume\n", &vchan->vc);

	txd = rtk_dma_find_vchan_txd(vchan);
	if (!txd) {
		dev_err(rsdma->dma.dev, "Unable to find TXD for channel %d\n", vchan->vc.chan.chan_id);
		ret = -EINVAL;
		goto END_RESUME_PROCESS;
	}

	if ((txd) && (txd->vd.tx.flags & DMA_PREP_INTERRUPT)) {
		if (vchan->pchan->dma_pause.status == DMA_PAUSED_NOW) {
			vchan->pchan->dma_pause.status = DMA_RESUME_REQUSTED;
			if (vchan->pchan->dma_pause.has_next == 1) {
				rtk_dma_start_next_txd(vchan, 1);
			} else if (vchan->pchan->dma_pause.has_next > 1) {
				rtk_dma_start_next_txd(vchan, 0);
			} else {
				dev_dbg(rsdma->dma.dev, "Last TXD has been completed\n");
			}
		} else {
			vchan->pchan->dma_pause.status = DMA_RESUMED_NOW;
			dev_err(rsdma->dma.dev, "Vchan %p DMA has not been paused\n", &vchan->vc);
			ret = -EINVAL;
			goto END_RESUME_PROCESS;
		}
	} else {
		/* Resume the auto-reload chan. */
		temp = rsdma_readl(vchan->pchan->base, RTK_DMA_CHAN_CFG_L);
		rsdma_writel(vchan->pchan->base, RTK_DMA_CHAN_CFG_L, ~(~temp | BIT_DMA_CFG_CH_SUSP(1)));
		rtk_dma_channel_enable(rsdma, vchan->pchan->id, ENABLE);
		vchan->pchan->dma_pause.status = DMA_RESUMED_NOW;
		rtk_dma_reg_check_all(rsdma, vchan->pchan->id);
	}

END_RESUME_PROCESS:
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	return ret;
}

static void rtk_dma_synchronize(struct dma_chan *chan)
{
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);

	vchan_synchronize(&vchan->vc);
}

static enum dma_status rtk_dma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie,
		struct dma_tx_state *state)
{
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	int completed_count = 0;

	if (state) {
		completed_count = rsdma_readl(vchan->pchan->base, RTK_DMA_CHAN_CTL_U);
		state->residue = completed_count;
	}

	switch (vchan->pchan->dma_pause.status) {
	case DMA_COMPLETED:
		return DMA_COMPLETE;
	case DMA_PAUSED_NOW:
		return DMA_PAUSED;
	default:
		return DMA_IN_PROGRESS;
	}
}

static struct rtk_dma_pchan *rtk_dma_get_pchan(struct rtk_dma *rsdma,
		struct rtk_dma_vchan *vchan)
{
	struct rtk_dma_pchan *pchan = NULL;
	unsigned long flags;
	int i;

	if ((vchan->vc.chan.chan_id == RTK_DMA_HIGH_PERF_CH0) || (vchan->vc.chan.chan_id == RTK_DMA_HIGH_PERF_CH1)) {
		/*
		 * Physical channels 0 & 1 are high performance channels with bigger
		 * fifo size. Hence, we reserve these two channels for special
		 * allocation by peripherals requiring higher performance. In order to
		 * reserve them, we fix the mapping of these 2 physical channels with
		 * virtual channel numbers such that vchan=0 <--> pchan=0 and vchan=1
		 * <--> pchan=1. vchans from 2 onwards are allocated any other
		 * pchans from 2 to 7 dynamically whichever is free to use.
		 */
		pchan = &rsdma->pchans[vchan->vc.chan.chan_id];
		spin_lock_irqsave(&rsdma->lock, flags);
		if (!pchan->vchan) {
			pchan->vchan = vchan;
		}
		spin_unlock_irqrestore(&rsdma->lock, flags);
		goto pchan_done;
	}

	for (i = RTK_DMA_HIGH_PERF_CH1 + 1; i < rsdma->nr_pchans; i++) {
		pchan = &rsdma->pchans[i];
		spin_lock_irqsave(&rsdma->lock, flags);
		if (!pchan->vchan) {
			pchan->vchan = vchan;
			spin_unlock_irqrestore(&rsdma->lock, flags);
			goto pchan_done;
		}
		spin_unlock_irqrestore(&rsdma->lock, flags);
	}

pchan_done:
	return pchan;
}

static void rtk_dma_phy_alloc_and_start(struct rtk_dma_vchan *vchan)
{
	struct rtk_dma *rsdma = to_rtk_dma(vchan->vc.chan.device);
	struct rtk_dma_pchan *pchan;

	pchan = rtk_dma_get_pchan(rsdma, vchan);
	if (!pchan) {
		dev_err(rsdma->dma.dev, "No dma physical channel available for use for requested virtual channel %d, all are used up\n", vchan->vc.chan.chan_id);
		return;
	}

	dev_dbg(rsdma->dma.dev, "Allocated pchan %d\n", pchan->id);

	vchan->pchan = pchan;
	rtk_dma_start_next_txd(vchan, 0);
}

static void rtk_dma_issue_pending(struct dma_chan *chan)
{
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);
	unsigned long flags;

	spin_lock_irqsave(&vchan->vc.lock, flags);
	if (vchan_issue_pending(&vchan->vc)) {
		if (!vchan->pchan) {
			rtk_dma_phy_alloc_and_start(vchan);
		} else {
			dev_dbg(rsdma->dma.dev, "DMA has pchan, start TXD directly\n");
			rtk_dma_start_next_txd(vchan, 0);
		}
	}
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
}

static struct dma_async_tx_descriptor
*rtk_dma_prep_memcpy(struct dma_chan *chan,
					 dma_addr_t dst, dma_addr_t src,
					 size_t len, unsigned long flags)
{
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	struct rtk_dma_txd *txd;
	struct rtk_dma_lli *lli, *prev = NULL;
	size_t offset, bytes;
	int ret;

	if (!len) {
		return NULL;
	}

	txd = rtk_dma_find_unused_txd(vchan);
	if (!txd) {
		dev_err(rsdma->dma.dev, "No free TXD available for channel %d\n", vchan->vc.chan.chan_id);
		return NULL;
	}

	INIT_LIST_HEAD(&txd->lli_list);

	/* Process the transfer as frame by frame */
	for (offset = 0; offset < len; offset += bytes) {
		lli = rtk_dma_alloc_lli(rsdma);
		if (!lli) {
			dev_warn(rsdma->dma.dev, "Failed to allocate lli\n");
			goto err_txd_free;
		}

		bytes = min_t(size_t, (len - offset), RTK_DMA_FRAME_MAX_LENGTH);

		ret = rtk_dma_hw_cfg_lli(vchan, lli, src + offset, dst + offset,
								 bytes, DMA_MEM_TO_MEM,
								 &vchan->cfg, txd, 0);
		if (ret) {
			dev_warn(rsdma->dma.dev, "Failed to config lli\n");
			goto err_txd_free;
		}

		prev = rtk_dma_add_lli(txd, prev, lli, false);
	}

	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_txd_free:
	rtk_dma_free_txd(rsdma, txd);
	return NULL;
}

static struct dma_async_tx_descriptor
*rtk_dma_prep_slave_sg(
	struct dma_chan *chan,
	struct scatterlist *sgl,
	unsigned int sg_len,
	enum dma_transfer_direction dir,
	unsigned long flags, void *context)
{
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct rtk_dma_txd *txd;
	struct rtk_dma_lli *lli, *prev = NULL;
	struct scatterlist *sg = NULL;
	dma_addr_t addr, src = 0, dst = 0;
	size_t len;
	int ret, i = 0;

	txd = rtk_dma_find_unused_txd(vchan);
	if (!txd) {
		dev_err(rsdma->dma.dev, "No free TXD available for channel %d\n", vchan->vc.chan.chan_id);
		return NULL;
	}

	INIT_LIST_HEAD(&txd->lli_list);

	for_each_sg(sgl, sg, sg_len, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);

		if (len > RTK_DMA_FRAME_MAX_LENGTH) {
			dev_err(rsdma->dma.dev,
					"Frame length exceeds max supported length\n");
			goto err_txd_free;
		}

		lli = rtk_dma_alloc_lli(rsdma);
		if (!lli) {
			dev_err(rsdma->dma.dev, "Failed to allocate lli\n");
			goto err_txd_free;
		}

		if (dir == DMA_MEM_TO_DEV) {
			src = addr;
			dst = sconfig->dst_addr;
		} else {
			src = sconfig->src_addr;
			dst = addr;
		}

		ret = rtk_dma_hw_cfg_lli(vchan, lli, src, dst, len, dir, sconfig,
								 txd, 0);
		if (ret) {
			dev_warn(rsdma->dma.dev, "Failed to config lli\n");
			goto err_txd_free;
		}

		prev = rtk_dma_add_lli(txd, prev, lli, false);
	}

	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_txd_free:
	rtk_dma_free_txd(rsdma, txd);

	return NULL;
}

/* Important!! This category is for audio dma. */
static struct dma_async_tx_descriptor
*rtk_prep_dma_cyclic(
	struct dma_chan *chan,
	dma_addr_t buf_addr, size_t buf_len,
	size_t period_len,
	enum dma_transfer_direction dir,
	unsigned long flags)
{
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct rtk_dma_txd *txd;
	struct rtk_dma_lli *lli, *prev = NULL, *first = NULL;
	dma_addr_t src = 0, dst = 0;
	unsigned int periods = buf_len / period_len;
	int ret, i;
	bool interrupt_en;

	interrupt_en = flags & DMA_PREP_INTERRUPT;
	if (!interrupt_en) {
		/* Do not trigger callback after dma success, use auto-reload mode. */
		dev_dbg(rsdma->dma.dev, "Enter DMA auto-reload mode\n");
		if (period_len != buf_len) {
			dev_warn(rsdma->dma.dev, "Wrong period length for DMA auto-reload mode\n");
			return NULL;
		}
	}

	txd = rtk_dma_find_unused_txd(vchan);
	if (!txd) {
		dev_err(rsdma->dma.dev, "No free TXD available for channel %d\n", vchan->vc.chan.chan_id);
		return NULL;
	}

	INIT_LIST_HEAD(&txd->lli_list);
	txd->cyclic = true;

	for (i = 0; i < periods; i++) {
		lli = rtk_dma_alloc_lli(rsdma);
		if (!lli) {
			dev_warn(rsdma->dma.dev, "Failed to allocate lli\n");
			goto err_txd_free;
		}
		if (dir == DMA_MEM_TO_DEV) {
			src = buf_addr + (period_len * i);
			dst = sconfig->dst_addr;
		} else if (dir == DMA_DEV_TO_MEM) {
			src = sconfig->src_addr;
			dst = buf_addr + (period_len * i);
		} else {
			src = sconfig->src_addr + (period_len * i);
			dst = sconfig->dst_addr + (period_len * i);
		}

		ret = rtk_dma_hw_cfg_lli(vchan, lli, src, dst, period_len,
								 dir, sconfig, txd, interrupt_en);

		if (ret) {
			dev_warn(rsdma->dma.dev, "Failed to config lli\n");
			goto err_txd_free;
		}

		if (!first) {
			first = lli;
		}

		prev = rtk_dma_add_lli(txd, prev, lli, false);
	}

	/* close the cyclic list */
	rtk_dma_add_lli(txd, prev, first, true);
	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_txd_free:
	rtk_dma_free_txd(rsdma, txd);

	return NULL;
}

static void rtk_dma_free_chan_resources(struct dma_chan *chan)
{
	struct rtk_dma *rsdma = to_rtk_dma(chan->device);
	struct rtk_dma_vchan *vchan = to_rtk_vchan(chan);

	/* Ensure all queued descriptors are freed */
	vchan_free_chan_resources(&vchan->vc);
	rtk_dma_phy_free(rsdma, vchan);

	if (vchan->pchan) {
		vchan->pchan->vchan = NULL;
		vchan->pchan = NULL;
	}
}

static struct dma_chan *rtk_dma_of_xlate(
	struct of_phandle_args *dma_spec,
	struct of_dma *ofdma)
{
	struct rtk_dma *rsdma = ofdma->of_dma_data;
	struct rtk_dma_vchan *vchan;
	struct dma_chan *chan;
	u8 drq = dma_spec->args[0];

	if (drq > rsdma->nr_vchans) {
		dev_err(rsdma->dma.dev, "Requested virtual channel %d exceeds max virtual channels configured %d\n", drq, rsdma->nr_vchans);
		return NULL;
	}

	vchan = &rsdma->vchans[drq];
	if (vchan->pchan != NULL) {
		dev_err(rsdma->dma.dev, "Requested virtual channel %d is already in use by another device, pls check for channel conflict in DTS\n", drq);
		return NULL;
	}

	chan = dma_get_slave_channel(&vchan->vc.chan);
	if (!chan) {
		dev_err(rsdma->dma.dev, "Unable to get DMA channel for vchan %d\n", drq);
		return NULL;
	}

	vchan->drq = drq;
	vchan->pchan = NULL;

	return chan;
}

/**************************************************************************/

/************************* RTK_GDMA Driver Top Level **********************/

/**************************************************************************/

static const struct of_device_id rtk_dma_match[] = {
	{.compatible = "realtek,ameba-dmac",},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_dma_match);

static int rtk_dma_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct rtk_dma *rsdma;
	struct resource *res;
	int ret, i, nr_channels, nr_requests;
	const struct of_device_id *of_id = NULL;

	of_id = of_match_device(rtk_dma_match, &pdev->dev);
	if (!of_id || strcmp(of_id->compatible, rtk_dma_match->compatible)) {
		return 0;
	}

	rsdma = devm_kzalloc(&pdev->dev, sizeof(*rsdma), GFP_KERNEL);
	if (!rsdma) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		return -EINVAL;
	}

	rsdma->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rsdma->base)) {
		return PTR_ERR(rsdma->base);
	}

	ret = of_property_read_u32(np, "dma-channels", &nr_channels);
	if (ret) {
		dev_err(&pdev->dev, "can't get dma-channels\n");
		return ret;
	}

	ret = of_property_read_u32(np, "dma-requests", &nr_requests);
	if (ret) {
		dev_err(&pdev->dev, "Can't get dma-requests\n");
		return ret;
	}
	dev_info(&pdev->dev, "DMA channels %d, DMA requests %d\n",
			 nr_channels, nr_requests);

	rsdma->nr_pchans = nr_channels;
	rsdma->nr_vchans = nr_requests;

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	platform_set_drvdata(pdev, rsdma);
	spin_lock_init(&rsdma->lock);

	dma_cap_set(DMA_MEMCPY, rsdma->dma.cap_mask);
	dma_cap_set(DMA_SLAVE, rsdma->dma.cap_mask);
	dma_cap_set(DMA_CYCLIC, rsdma->dma.cap_mask);

	rsdma->dma.dev = &pdev->dev;
	rsdma->dma.device_free_chan_resources = rtk_dma_free_chan_resources;
	rsdma->dma.device_issue_pending = rtk_dma_issue_pending;
	rsdma->dma.device_prep_dma_memcpy = rtk_dma_prep_memcpy;
	rsdma->dma.device_prep_slave_sg = rtk_dma_prep_slave_sg;
	rsdma->dma.device_prep_dma_cyclic = rtk_prep_dma_cyclic;
	rsdma->dma.device_config = rtk_dma_config;
	rsdma->dma.device_terminate_all = rtk_dma_terminate_all;
	rsdma->dma.src_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_4_BYTES);
	rsdma->dma.dst_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_4_BYTES);
	rsdma->dma.directions = BIT(DMA_MEM_TO_MEM);
	rsdma->dma.residue_granularity = DMA_RESIDUE_GRANULARITY_BURST;
	rsdma->dma.device_tx_status = rtk_dma_tx_status;
	rsdma->dma.device_pause = rtk_dma_pause;
	rsdma->dma.device_resume = rtk_dma_resume;
	rsdma->dma.device_synchronize = rtk_dma_synchronize;

	INIT_LIST_HEAD(&rsdma->dma.channels);

	rsdma->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(rsdma->clk)) {
		dev_err(&pdev->dev, "Unable to get clock\n");
		return PTR_ERR(rsdma->clk);
	}

	/*
	 * Eventhough the DMA controller is capable of generating 4
	 * IRQ's for DMA priority feature, we only use 1 IRQ for
	 * simplification. request for irq-dma-0.
	 */
	for (i = 0; i < GDMA_MAX_CHANNEL; i++) {
		rsdma->irq[i] = platform_get_irq(pdev, i);
#if RTK_DMAC_INTERRUPT_COMBINE
		ret = devm_request_irq(&pdev->dev, rsdma->irq[i], rtk_dma_interrupt, 0, dev_name(&pdev->dev), rsdma);
#endif // RTK_DMAC_INTERRUPT_COMBINE
	}
#if !RTK_DMAC_INTERRUPT_COMBINE
	ret = devm_request_irq(&pdev->dev, rsdma->irq[0], rtk_dma_interrupt_ch0, 0, dev_name(&pdev->dev), rsdma);
	ret = devm_request_irq(&pdev->dev, rsdma->irq[1], rtk_dma_interrupt_ch1, 0, dev_name(&pdev->dev), rsdma);
	ret = devm_request_irq(&pdev->dev, rsdma->irq[2], rtk_dma_interrupt_ch2, 0, dev_name(&pdev->dev), rsdma);
	ret = devm_request_irq(&pdev->dev, rsdma->irq[3], rtk_dma_interrupt_ch3, 0, dev_name(&pdev->dev), rsdma);
	ret = devm_request_irq(&pdev->dev, rsdma->irq[4], rtk_dma_interrupt_ch4, 0, dev_name(&pdev->dev), rsdma);
	ret = devm_request_irq(&pdev->dev, rsdma->irq[5], rtk_dma_interrupt_ch5, 0, dev_name(&pdev->dev), rsdma);
	ret = devm_request_irq(&pdev->dev, rsdma->irq[6], rtk_dma_interrupt_ch6, 0, dev_name(&pdev->dev), rsdma);
	ret = devm_request_irq(&pdev->dev, rsdma->irq[7], rtk_dma_interrupt_ch7, 0, dev_name(&pdev->dev), rsdma);

	for (i = 0; i < GDMA_MAX_CHANNEL; i++) {
		irq_set_affinity(rsdma->irq[i], cpumask_of(RTK_DMAC_RUN_CPU));
	}

#endif // !RTK_DMAC_INTERRUPT_COMBINE

	if (ret) {
		dev_err(&pdev->dev, "Unable to request IRQ\n");
		return ret;
	}

	/* Init physical channel */
	rsdma->pchans = devm_kcalloc(&pdev->dev, rsdma->nr_pchans, sizeof(struct rtk_dma_pchan), GFP_KERNEL);
	if (!rsdma->pchans) {
		return -ENOMEM;
	}

	for (i = 0; i < rsdma->nr_pchans; i++) {
		struct rtk_dma_pchan *pchan = &rsdma->pchans[i];

		pchan->id = i;
		pchan->base = rsdma->base + RTK_DMA_CHAN_BASE(i);
		pchan->vchan = NULL;
	}

	/* Init virtual channel */
	rsdma->vchans = devm_kcalloc(&pdev->dev, rsdma->nr_vchans, sizeof(struct rtk_dma_vchan), GFP_KERNEL);
	if (!rsdma->vchans) {
		return -ENOMEM;
	}

	for (i = 0; i < rsdma->nr_vchans; i++) {
		struct rtk_dma_vchan *vchan = &rsdma->vchans[i];

		vchan->vc.desc_free = rtk_dma_desc_free;
		vchan_init(&vchan->vc, &rsdma->dma);
		memset(&vchan->txd[i], 0, RTK_MAX_TXD_PER_VCHAN);
		vchan->txd[i].allocated = false;
		vchan->txd[i].is_active = false;
		vchan->txd[i].vchan = NULL;
	}

	/* Create a pool of consistent memory blocks for hardware descriptors */
	rsdma->lli_pool = dma_pool_create(dev_name(rsdma->dma.dev), rsdma->dma.dev, sizeof(struct rtk_dma_lli), __alignof__(struct rtk_dma_lli), 0);
	if (!rsdma->lli_pool) {
		dev_err(&pdev->dev, "Unable to allocate DMA descriptor pool\n");
		return -ENOMEM;
	}

	clk_prepare_enable(rsdma->clk);

	ret = dma_async_device_register(&rsdma->dma);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register DMA engine device\n");
		goto err_pool_free;
	}

	/* Device-tree DMA controller registration */
	ret = of_dma_controller_register(pdev->dev.of_node, rtk_dma_of_xlate, rsdma);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register DMA controller\n");
		goto err_dma_unregister;
	}

	return 0;

err_dma_unregister:
	dma_async_device_unregister(&rsdma->dma);
err_pool_free:
	clk_disable_unprepare(rsdma->clk);
	dma_pool_destroy(rsdma->lli_pool);

	return ret;
}

static inline void rtk_dma_free(struct rtk_dma *rsdma)
{
	struct rtk_dma_vchan *vchan = NULL;
	struct rtk_dma_vchan *next = NULL;

	list_for_each_entry_safe(vchan, next, &rsdma->dma.channels, vc.chan.device_node) {
		list_del(&vchan->vc.chan.device_node);
		tasklet_kill(&vchan->vc.task);
	}
}

static int rtk_dma_remove(struct platform_device *pdev)
{
	int i;
	struct rtk_dma *rsdma = platform_get_drvdata(pdev);

	of_dma_controller_free(pdev->dev.of_node);
	dma_async_device_unregister(&rsdma->dma);

	/* Mask all interrupts for this execution environment */
	rtk_dma_clear_all_interrupts(rsdma);

	/* Make sure we won't have any further interrupts */
	for (i = 0; i < GDMA_MAX_CHANNEL; i++) {
		devm_free_irq(rsdma->dma.dev, rsdma->irq[i], rsdma);
	}

	rtk_dma_free(rsdma);

	clk_disable_unprepare(rsdma->clk);

	return 0;
}

static struct platform_driver rtk_dma_driver = {
	.probe	= rtk_dma_probe,
	.remove	= rtk_dma_remove,
	.driver = {
		.name = "realtek-ameba-dmac",
		.of_match_table = of_match_ptr(rtk_dma_match),
	},
};

builtin_platform_driver(rtk_dma_driver);

MODULE_DESCRIPTION("Realtek Ameba DMAC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
