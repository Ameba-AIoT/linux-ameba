/*
 * Realtek Semiconductor Corp.
 *
 * 8139gb.c:
 *   A Linux Ethernet driver for the RealTek 8139C+ chips.
 *
 * Copyright (C) 2006-2012 Jethro Hsu (jethro@realtek.com)
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/cache.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include "8139gb.h"

#define DEBUG 0
#define DRV_NAME	"8139gb"
#define DRV_VERSION	"1.0"
#define DRV_RELDATE	"Nov 14, 2012"

/* These identify the driver base version and may not be removed. */
static char version[] =
KERN_INFO DRV_NAME ": Gigabit Ethernet driver v" DRV_VERSION " (" DRV_RELDATE ")\n";
static const char  drv_name [] = DRV_NAME;

MODULE_AUTHOR("Jethro Hsu<jethro@realtek.com");
MODULE_DESCRIPTION("RealTek 8139GB Gigabit Ethernet driver");
MODULE_LICENSE("GPL");

static int debug = -1;
module_param(debug, int, 0);
MODULE_PARM_DESC (debug, "8139gb: bitmapped message enable number");

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;
module_param(multicast_filter_limit, int, 0);
MODULE_PARM_DESC (multicast_filter_limit, "8139gb: maximum number of filtered multicast addresses");

static void __cp_set_rx_mode (struct net_device *dev);
static void cp_tx (struct cp_private *cp, unsigned int status);
static int cp_init_rings (struct cp_private *cp);
static void cp_clean_rings (struct cp_private *cp);
static void cp_stop_hw (struct cp_private *cp);
static inline void cp_start_hw (struct cp_private *cp);
static int cp_open (struct net_device *dev);
static int cp_close(struct net_device *dev);
static void cp_init_hw (struct cp_private *cp);
static void cp_tx_timeout (struct net_device *dev);
#if DEBUG
static void cp_show_desc (struct net_device *dev);
static void cp_show_regs_datum (struct net_device *dev);
#endif

#if CP_VLAN_TAG_USED
static void cp_vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&cp->lock, flags);
	cp->vlgrp = grp;
	cp->cpcmd |= RxVlanOn;
	cpw16(CMD, cp->cpcmd);
	spin_unlock_irqrestore(&cp->lock, flags);
}


static void cp_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&cp->lock, flags);
	cp->cpcmd &= ~RxVlanOn;
	cpw16(CMD, cp->cpcmd);
	if (cp->vlgrp)
		cp->vlgrp->vlan_devices[vid] = NULL;
	spin_unlock_irqrestore(&cp->lock, flags);
}
#endif /* CP_VLAN_TAG_USED */

static inline void cp_set_rxbufsize (struct cp_private *cp)
{
	unsigned int mtu = cp->dev->mtu;

	if (mtu > ETH_DATA_LEN)
		/* MTU + ethernet header + FCS + optional VLAN tag */
		cp->rx_buf_sz = mtu + ETH_HLEN + 8;
	else
		cp->rx_buf_sz = PKT_BUF_SZ;
}

static inline void cp_rx_skb (struct cp_private *cp, struct sk_buff *skb,
		  struct cp_desc *desc)
{
	skb->dev = cp->dev;
	skb->protocol = eth_type_trans(skb, cp->dev);

	cp->dev->stats.rx_packets++;
	cp->dev->stats.rx_bytes += skb->len;
	cp->dev->last_rx = jiffies;

#if CP_VLAN_TAG_USED
	if (cp->vlgrp && (desc->opts2 & RxVlanTagged)) {
		vlan_hwaccel_receive_skb(skb, cp->vlgrp,
					 be16_to_cpu(desc->opts2 & 0xffff));
	} else
#endif
	netif_receive_skb(skb);
}

static void cp_rx_err_acct (struct cp_private *cp, unsigned rx_tail,
		u32 status, u32 len)
{
	if (netif_msg_rx_err (cp))
		printk (KERN_DEBUG "%s: rx err, slot %d status 0x%x len %d\n",
			cp->dev->name, rx_tail, status, len);
	cp->dev->stats.rx_errors++;
	if (status & RxErrFrame)
		cp->dev->stats.rx_frame_errors++;
	if (status & RxErrCRC)
		cp->dev->stats.rx_crc_errors++;
	if (status & RxErrRunt)
		cp->dev->stats.rx_length_errors++;
}

static inline unsigned int cp_rx_csum_ok(u32 status)
{
	unsigned int protocol = (status >> 16) & 0x3;

	/* if no protocol , we dont sw check either */
	if (!protocol)
		return 1;

	if (((protocol == RxProtoTCP) && !(status & TCPFail)) ||
	    ((protocol == RxProtoUDP) && !(status & UDPFail)) ||
	    ((protocol == RxProtoIP) && !(status & IPFail)))
		return 1;
	else
		return 0;
}

static int cp_rx_poll (struct napi_struct *napi, int budget)
{
	struct cp_private *cp = container_of(napi, struct cp_private, napi);
	struct net_device *dev = cp->dev;
	unsigned rx_tail = cp->rx_tail;
	unsigned long intr_flags;
	unsigned rx;

#ifdef CONFIG_PM
	if (cp_status!=CP_ON)
	{
		printk("cp_status not on \n");
		__cp_clear_interrupts(0xffff);
		__napi_complete(napi);
		return 0;
	}
#endif
rx_status_loop:
	rx = 0;
	__cp_clear_interrupts(cp_rx_intr_mask);

	while (1) {
		u32 status;
		u32 len;
		dma_addr_t mapping;
		struct sk_buff* skb;
		struct sk_buff* new_skb;
		struct cp_desc* desc;
		unsigned buflen;

		skb = cp->rx_skb[rx_tail].skb;
		buflen = cp->rx_buf_sz;
		BUG_ON(!skb);

		desc   = &cp->rx_ring[rx_tail];
		status = le32_to_cpu(desc->opts1);

		if (status & DescOwn)
			break;

		len = ((status) & (0x7ff)) - 4;

		mapping = le64_to_cpu(desc->addr);

		if ((status & (FirstFrag | LastFrag)) != (FirstFrag | LastFrag)) {
			/* we don't support incoming fragmented frames.
			 * instead, we attempt to ensure that the
			 * pre-allocated RX skbs are properly sized such
			 * that RX fragments are never encountered
			 */
			cp_rx_err_acct(cp, rx_tail, status, len);
			cp->dev->stats.rx_dropped++;
			cp->cp_stats.rx_frags++;

			goto rx_next;
		}

		if ((status & (RxError)) && !(status & (RxErrCRC))) {
			cp_rx_err_acct(cp, rx_tail, status, len);
			goto rx_next;
		}

		dma_unmap_page(&cp->pdev->dev, mapping,
			       buflen, DMA_FROM_DEVICE);

		new_skb = netdev_alloc_skb_ip_align(dev, buflen);
		if (!new_skb) {
			cp->dev->stats.rx_dropped++;
			goto rx_next;
		}

		new_skb->dev = cp->dev;

		/* Handle checksum offloading for incoming packets. */
		if (cp_rx_csum_ok(status))
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);

		skb_put(skb, len);

		cp->rx_skb[rx_tail].mapping =
			dma_map_page(&cp->pdev->dev, virt_to_page(new_skb->data),
				     (size_t) new_skb->data & ~PAGE_MASK,
				     buflen, DMA_FROM_DEVICE);

		mapping = cp->rx_skb[rx_tail].mapping;
		cp->rx_skb[rx_tail].skb = new_skb;
		cp_rx_skb(cp, skb, desc);
		rx++;

rx_next:
		cp->rx_ring[rx_tail].opts2 = 0;
		cp->rx_ring[rx_tail].addr = cpu_to_le32(mapping);
		if (rx_tail == (CP_RX_RING_SIZE - 1))
			desc->opts1 = cpu_to_le32(DescOwn | RingEnd |
							cp->rx_buf_sz);
		else
			desc->opts1 = cpu_to_le32(DescOwn | cp->rx_buf_sz);

		cpw8(EthrntRxCPU_Des_Num, rx_tail);
		rx_tail = NEXT_RX(rx_tail);

		if (rx >= budget)
			break;
	}

	cp->rx_tail = rx_tail;

	/* if we did not reach work limit, then we're done with
	 * this round of polling
	 */
	if (rx < budget) {
		if (__cp_get_interrupts() & cp_rx_intr_mask)
			goto rx_status_loop;
		local_irq_save(intr_flags);
		__napi_complete(napi);
		__cp_set_interrupt_mask(cp_intr_mask);
		local_irq_restore(intr_flags);
	}

	return rx;
}



static irqreturn_t cp_interrupt(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct cp_private *cp;
	u32 status;

	if (unlikely(dev == NULL))
		return IRQ_NONE;

	cp = netdev_priv(dev);
	status = __cp_get_interrupts();

	if (!status || (status == 0xFFFF))
		return IRQ_NONE;

	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: intr, status %04x cmd %02x \n",
			dev->name, status, cpr8(CMD));

	__cp_clear_interrupts(status & ~cp_rx_intr_mask);

	spin_lock(&cp->lock);
	/* close possible race's with dev_close */
	if (unlikely(!netif_running(dev))) {
		__cp_set_interrupt_mask(0);
		__cp_clear_interrupts(0xFFFF);
		spin_unlock(&cp->lock);
		return IRQ_HANDLED;
	}

	if (status & cp_rx_intr_mask) {
		if (napi_schedule_prep(&cp->napi)) {
			__cp_set_interrupt_mask(cp_norx_intr_mask);
			__napi_schedule(&cp->napi);
		}
	}

	if (status & (TX_OK | TX_ERR | TX_EMPTY | SW_INT))
		cp_tx(cp, status);

	spin_unlock(&cp->lock);
	return IRQ_HANDLED;
}



#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling receive - used by netconsole and other diagnostic tools
 * to allow network i/o with interrupts disabled.
 */
static void cp_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	cp_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif



static void cp_tx(struct cp_private *cp, unsigned int status)
{
	unsigned tx_head = cp->tx_hqhead;
	unsigned tx_tail = cp->tx_hqtail;

	while (tx_tail != tx_head) {
		struct sk_buff *skb;
		u32 DescStatus;

		rmb();
		DescStatus = le32_to_cpu(cp->tx_hqring[tx_tail].opts1);
		if (DescStatus & DescOwn)
			break;

		skb = cp->tx_skb[tx_tail].skb;
		BUG_ON(!skb);

		dma_unmap_page(&cp->pdev->dev, le64_to_cpu(cp->tx_hqring[tx_tail].addr),
			       DescStatus & 0xffff, DMA_TO_DEVICE);

		if (DescStatus & LastFrag) {
			if (status & (TX_ERR)) {
				if (netif_msg_tx_err(cp))
				printk(KERN_DEBUG "%s: tx err, status 0x%x\n",
				cp->dev->name, status);

				cp->dev->stats.tx_errors++;
			} else {
				cp->dev->stats.tx_packets++;
				cp->dev->stats.tx_bytes += skb->len;
				if (netif_msg_tx_done(cp))
					printk(KERN_DEBUG "%s: tx done, slot %d\n",
						cp->dev->name, tx_tail);
			}
			dev_kfree_skb_irq(skb);
		}

		cp->tx_skb[tx_tail].skb = NULL;

		tx_tail = NEXT_TX(tx_tail);
	}

	cp->tx_hqtail = tx_tail;

	if (TX_HQBUFFS_AVAIL(cp) > (MAX_SKB_FRAGS + 1))
		netif_wake_queue(cp->dev);
}


static netdev_tx_t cp_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned entry;
	u32 eor, flags;
	unsigned long intr_flags;

#if CP_VLAN_TAG_USED
	u32 vlan_tag = 0;
#endif
	cp->cp_stats.tx_cnt++;
	spin_lock_irqsave(&cp->lock, intr_flags);

	/* This is a hard error, log it. */
	if (TX_HQBUFFS_AVAIL(cp) <= (skb_shinfo(skb)->nr_frags + 1)) {
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&cp->lock, intr_flags);
		printk(KERN_ERR DRV_NAME ":%s: BUG! Tx Ring full when queue awake!\n",
			dev->name);
		return NETDEV_TX_BUSY;
	}

#if CP_VLAN_TAG_USED
	if (cp->vlgrp && vlan_tx_tag_present(skb))
		vlan_tag = TxVlanTag | (vlan_tx_tag_get(skb));
#endif

	entry = cp->tx_hqhead;
	eor = (entry == (CP_TX_RING_SIZE - 1)) ? RingEnd : 0;

	if (skb_shinfo(skb)->nr_frags == 0) {
		struct cp_desc *txd = &cp->tx_hqring[entry];
		u32 len;
		dma_addr_t mapping;

		len = skb->len;
		mapping = dma_map_page(&cp->pdev->dev, virt_to_page(skb->data),
				       (size_t) skb->data & ~PAGE_MASK,
				       len, DMA_TO_DEVICE);

		CP_VLAN_TX_TAG(txd, vlan_tag);

		txd->addr = cpu_to_le64(mapping);

		wmb();

		flags = (eor | len | DescOwn | FirstFrag | LastFrag | TxCRC);
		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			const struct iphdr *ip = ip_hdr(skb);
			if (ip->protocol == IPPROTO_TCP)
				flags |= IPCS | TCPCS;
			else if (ip->protocol == IPPROTO_UDP)
				flags |= IPCS | UDPCS;
			else
				WARN_ON(1);	/* we need a WARN() */
		}

		txd->opts1 = cpu_to_le32(flags);
		wmb();

		cp->tx_skb[entry].skb = skb;
		cp->tx_skb[entry].mapping = mapping;
		cp->tx_skb[entry].frag = 0;
		entry = NEXT_TX(entry);
	}
	else
	{
		struct cp_desc *txd;
		u32 first_len, first_eor, first_ctrl;
		dma_addr_t first_mapping;
		int frag, first_entry = entry;
		const struct iphdr *ip = ip_hdr(skb);

		/* We must give this initial chunk to the device last.
		 * Otherwise we could race with the device.
		 */
		first_eor = eor;
		first_len = skb_headlen(skb);
		first_mapping =
			dma_map_page(&cp->pdev->dev, virt_to_page(skb->data),
				     (size_t) skb->data & ~PAGE_MASK,
				     first_len, DMA_TO_DEVICE);
		cp->tx_skb[entry].skb = skb;
		cp->tx_skb[entry].mapping = first_mapping;
		cp->tx_skb[entry].frag = 1;
		entry = NEXT_TX(entry);

		for (frag = 0; frag < skb_shinfo(skb)->nr_frags; frag++) {
			skb_frag_t *this_frag = &skb_shinfo(skb)->frags[frag];
			u32 len;
			u32 ctrl;
			dma_addr_t mapping;

			len = this_frag->size;
			mapping = skb_frag_dma_map(&cp->pdev->dev, this_frag, 0,
						   len, DMA_TO_DEVICE);

			eor = (entry == (CP_TX_RING_SIZE - 1)) ? RingEnd : 0;

			ctrl = (eor | len | DescOwn | TxCRC);

			if (skb->ip_summed == CHECKSUM_PARTIAL) {
				if (ip->protocol == IPPROTO_TCP)
					ctrl |= IPCS | TCPCS;
				else if (ip->protocol == IPPROTO_UDP)
					ctrl |= IPCS | UDPCS;
				else
					BUG();
			}

			if (frag == skb_shinfo(skb)->nr_frags - 1)
				ctrl |= LastFrag;

			txd = &cp->tx_hqring[entry];
			CP_VLAN_TX_TAG(txd, vlan_tag);
			txd->addr = cpu_to_le64(mapping);
			wmb();
			txd->opts1 = cpu_to_le32(ctrl);
			wmb();

			cp->tx_skb[entry].skb = skb;
			cp->tx_skb[entry].mapping = mapping;
			cp->tx_skb[entry].frag = frag + 2;
			entry = NEXT_TX(entry);
		}

		txd = &cp->tx_hqring[first_entry];
		CP_VLAN_TX_TAG(txd, vlan_tag);
		txd->addr = cpu_to_le64(first_mapping);
		wmb();
		first_ctrl = 0;
		first_ctrl = (first_eor | first_len | FirstFrag | DescOwn | TxCRC);

		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			if (ip->protocol == IPPROTO_TCP)
				first_ctrl |= IPCS | TCPCS;
			else if (ip->protocol == IPPROTO_UDP)
				first_ctrl |= IPCS | UDPCS;
			else
				BUG();
		}
		txd->opts1 = cpu_to_le32(first_ctrl);
		wmb();
	}

	cp->tx_hqhead = entry;

	if (netif_msg_tx_queued(cp))
		printk(KERN_DEBUG "%s: tx queued, slot %d, skblen %d\n",
			dev->name, entry, skb->len);

	if (TX_HQBUFFS_AVAIL(cp) <= (MAX_SKB_FRAGS + 1))
		netif_stop_queue(dev);

	spin_unlock_irqrestore(&cp->lock, intr_flags);

	__cp_io_command(CMD_CONFIG | TX_POLL);
	dev->trans_start = jiffies;

	return NETDEV_TX_OK;
}



/* Set or clear the multicast filter for this adaptor.
   This routine is not state sensitive and need not be SMP locked. */

static void __cp_set_rx_mode (struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	u32 mc_filter[2];   /* Multicast hash filter */
	int rx_mode;
	u32 tmp;

	/* Note: do not reorder, GCC is clever about common statements. */
	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
			  AcceptAllPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	}
	else if ((netdev_mc_count(dev) > multicast_filter_limit) ||
		(dev->flags & IFF_ALLMULTI)) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	}
	else {
		struct netdev_hw_addr *ha;
		rx_mode = AcceptBroadcast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0;
		netdev_for_each_mc_addr(ha, dev) {
			int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;

			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);

			rx_mode |= AcceptMulticast;
		}
	}

	/* We can safely update without stopping the chip. */
	tmp = rx_mode;
	if (cp->rx_config != tmp) {
		cpw32_f (RCR, tmp);
		cp->rx_config = tmp;
	}
	cpw32_f (MAR0 + 0, __cpu_to_be32(mc_filter[0]));
	cpw32_f (MAR0 + 4, __cpu_to_be32(mc_filter[1]));
}


static void cp_set_rx_mode (struct net_device *dev)
{
	unsigned long flags;
	struct cp_private *cp = netdev_priv(dev);

	spin_lock_irqsave (&cp->lock, flags);
	__cp_set_rx_mode(dev);
	spin_unlock_irqrestore (&cp->lock, flags);
}


static void __cp_get_stats(struct cp_private *cp)
{
	cp->dev->stats.rx_missed_errors += (cpr16 (MISSPKT) & 0xffff);
	cpw32 (MISSPKT, 0);
}


static struct net_device_stats *cp_get_stats(struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;

	/* The chip only need report frame silently dropped. */
	spin_lock_irqsave(&cp->lock, flags);
	if (netif_running(dev) && netif_device_present(dev))
		__cp_get_stats(cp);
	spin_unlock_irqrestore(&cp->lock, flags);

	return &dev->stats;
}


static void cp_reset_hw (struct cp_private *cp)
{
	unsigned work = 1000;

	/* reset */
	cpw8(CMD, 0x1);

	while (work--) {
		if (!(cpr8(CMD) & 0x1)) {
			cpw8(CMD, 0x2);
			return;
		}
		schedule_timeout_uninterruptible(10);
	}

	printk(KERN_ERR "%s: hardware reset timeout\n", cp->dev->name);
}



static void cp_stop_hw (struct cp_private *cp)
{
	__cp_clear_interrupts(0xffff);
	__cp_set_interrupt_mask(0);
	__cp_io_command(0);
	cp_reset_hw(cp);
	__cp_clear_interrupts(0xffff);
	cp->rx_config=0;
	cp->rx_tail = 0;
	cp->tx_hqhead = cp->tx_hqtail = 0;
}



static inline void cp_start_hw(struct cp_private *cp)
{
	__cp_io_command(CMD_RX_EN);
}


static void cp_init_hw(struct cp_private *cp)
{
	struct net_device *dev = cp->dev;
	dma_addr_t ring_dma;
	u32 status;
	cp_reset_hw(cp);

	__cp_clear_interrupts(0xffff);
	__cp_set_interrupt_mask(cp_intr_mask);

	ring_dma = cp->ring_dma;
	cpw32(RxFDP, ring_dma & 0xffffffff); /* Set Rx first descriptor pointer */

	cpw16(RxCDO, (ring_dma >> 16) >> 16);

	ring_dma += sizeof(struct cp_desc) * CP_RX_RING_SIZE;
	cpw32(TxFDP1,ring_dma & 0xffffffff); /* Set Tx first descriptor pointer */

	cpw16(TxCDO1, (ring_dma >> 16) >> 16);
	cpw32(TxFDP2,(u32)cp->tx_lqring); /* Set Tx first descriptor pointer Prio2*/
	cpw16(TxCDO2,0);

	cpw32(TCR,(u32)(0xC00)); /* Set inter fram gap */
	cpw8(EthrntRxCPU_Des_Num, CP_RX_RING_SIZE-1);
	cpw8(Pse_Des_Thres_on, THVAL);
	cpw8(Rx_Pse_Des_Thres_off, THOFFVAL);
	cpw8(Rx_Pse_Des_Thres_h, 0);
	cpw8(EthrntRxCPU_Des_Num_h, 0);
	cpw16(RxRingSize,RINGSIZE_2);
	status = __cp_get_msr();
	status = status |(TXFCE | RXFCE | FORCE_TX);
	__cp_set_msr(status);

	/* Restore our idea of the MAC address. */
	cpw32(IDR0 + 0, le32_to_cpu (*(__le32 *) (dev->dev_addr + 0)));
	cpw32(IDR0 + 4, le32_to_cpu (*(__le32 *) (dev->dev_addr + 4)));

#if defined(CONFIG_MIPS) || defined(CONFIG_RLX)
	/* Enable DMA coherent */
	if (plat_device_is_coherent(&cp->pdev->dev))
		cpw32(OCPCMD, 0x6);
#endif

	cp_start_hw(cp);

	__cp_set_rx_mode(dev);

	cpw32(Ethrnt_Ctrl1, 0x52080);
	cpw8(Ethrnt_Ctrl, 0x04);
	cpw32(Ethrnt_Phase_Sel, 0x6326);
}

static int cp_refill_rx(struct cp_private *cp)
{
	struct net_device *dev = cp->dev;
	unsigned i;

	for (i = 0; i < CP_RX_RING_SIZE; i++) {
		struct sk_buff *skb;

		skb = netdev_alloc_skb_ip_align(dev, cp->rx_buf_sz);
		if (!skb)
			goto err_out;

		skb->dev = cp->dev;
		cp->rx_skb[i].mapping =
			dma_map_page(&cp->pdev->dev, virt_to_page(skb->data),
				     (size_t) skb->data & ~PAGE_MASK,
				     cp->rx_buf_sz, DMA_FROM_DEVICE);
		cp->rx_skb[i].skb = skb;
		cp->rx_skb[i].frag = 0;

		cp->rx_ring[i].opts2 = 0;
		cp->rx_ring[i].addr = cpu_to_le32(cp->rx_skb[i].mapping);

		if (i == (CP_RX_RING_SIZE - 1))
			cp->rx_ring[i].opts1 = cpu_to_le32(DescOwn | RingEnd | cp->rx_buf_sz);
		else
			cp->rx_ring[i].opts1 = cpu_to_le32(DescOwn | cp->rx_buf_sz);
	}
	return 0;

err_out:
	cp_clean_rings(cp);
	return -ENOMEM;
}



static int cp_init_rings (struct cp_private *cp)
{
	memset(cp->tx_hqring, 0, sizeof(struct cp_desc) * CP_TX_RING_SIZE);
	cp->tx_hqring[CP_TX_RING_SIZE - 1].opts1 = cpu_to_le32(RingEnd);
	memset(cp->rx_ring, 0, sizeof(struct cp_desc) * CP_RX_RING_SIZE);
	cp->rx_tail = 0;
	cp->tx_hqhead = cp->tx_hqtail = 0;
	return cp_refill_rx(cp);
}


static int cp_alloc_rings (struct cp_private *cp)
{
	void* pBuf;

	pBuf = dma_alloc_coherent(&cp->pdev->dev, CP_RXRING_BYTES+CP_TXRING_BYTES,
				  &cp->ring_dma, GFP_KERNEL);
	if (!pBuf) {
		printk("cp_alloc_rings() fail !\n");
		BUG();
		goto ErrMem;
	}

	memset(pBuf, 0, CP_RXRING_BYTES+CP_TXRING_BYTES);

	/* 256 bytes alignment */
	pBuf = (void*)( (u32)(pBuf + DESC_ALIGN-1) &  ~(DESC_ALIGN -1) ) ;

	cp->rx_ring = pBuf;
	cp->tx_hqring = &cp->rx_ring[CP_RX_RING_SIZE];

	return cp_init_rings(cp);

ErrMem:
	return -ENOMEM;
}



static void cp_clean_rings (struct cp_private *cp)
{
	struct cp_desc *desc;
	unsigned i;


	for (i = 0; i < CP_RX_RING_SIZE; i++) {
		if (cp->rx_skb[i].skb) {
			desc = cp->rx_ring + i;
			dma_unmap_page(&cp->pdev->dev, le64_to_cpu(desc->addr),
				       cp->rx_buf_sz, DMA_FROM_DEVICE);
			dev_kfree_skb(cp->rx_skb[i].skb);
		}
	}

	for (i = 0; i < CP_TX_RING_SIZE; i++) {
		if (cp->tx_skb[i].skb) {
			struct sk_buff *skb = cp->tx_skb[i].skb;

			desc = cp->tx_hqring + i;
			dma_unmap_page(&cp->pdev->dev, le64_to_cpu(desc->addr),
				       le32_to_cpu(desc->opts1) & 0xffff,
				       DMA_TO_DEVICE);
			if (le32_to_cpu(desc->opts1) & LastFrag)
				dev_kfree_skb(skb);
			cp->dev->stats.tx_dropped++;
		}
	}

	memset(cp->rx_ring, 0, sizeof(struct cp_desc) * CP_RX_RING_SIZE);
	memset(cp->tx_hqring, 0, sizeof(struct cp_desc) * CP_TX_RING_SIZE);

	memset(&cp->rx_skb, 0, sizeof(struct ring_info) * CP_RX_RING_SIZE);
	memset(&cp->tx_skb, 0, sizeof(struct ring_info) * CP_TX_RING_SIZE);
}



static void cp_free_rings (struct cp_private *cp)
{
	cp_clean_rings(cp);
	dma_free_coherent(&cp->pdev->dev, CP_RXRING_BYTES+CP_TXRING_BYTES,
			cp->rx_ring, cp->ring_dma);
	cp->rx_ring = NULL;
	cp->tx_hqring = NULL;
}



static int cp_open (struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	int rc;
	cp_status = CP_UNDER_INIT;

	if (netif_msg_ifup(cp))
		printk(KERN_DEBUG "%s: enabling interface\n", dev->name);

	rc = cp_alloc_rings(cp);
	if (rc)
		return rc;

	napi_enable(&cp->napi);

	cp_init_hw(cp);

	rc = request_irq(dev->irq, cp_interrupt, IRQF_SHARED, dev->name, dev);
	if (rc)
		goto err_out_hw;

	netif_carrier_off(dev);
	mii_check_media(&cp->mii_if, netif_msg_link(cp), TRUE);
	netif_start_queue(dev);

	cp_status = CP_ON;

	return 0;

err_out_hw:
	cp_stop_hw(cp);
	napi_disable(&cp->napi);
	cp_free_rings(cp);
	return rc;
}



static int cp_close (struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;

	napi_disable(&cp->napi); /* napi_disable did napi_synchronize */

	if (netif_msg_ifdown(cp))
		printk(KERN_DEBUG "%s: disabling interface\n", dev->name);

	spin_lock_irqsave(&cp->lock, flags);

	netif_stop_queue(dev);
	netif_carrier_off(dev);

	cp_stop_hw(cp);

	spin_unlock_irqrestore(&cp->lock, flags);

	synchronize_irq(dev->irq);
	free_irq(dev->irq, dev);

	cp_free_rings(cp);

	cp_status = CP_OFF;
	return 0;
}

static void cp_tx_timeout(struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;
	int rc;

	netdev_warn(dev, "Transmit timeout, status %2x %4x %4x\n",
		    cpr8(CMD),
		    cpr16(ISR), cpr16(IMR));

	spin_lock_irqsave(&cp->lock, flags);

	cp_stop_hw(cp);
	cp_clean_rings(cp);
	rc = cp_init_rings(cp);
	cp_start_hw(cp);

	netif_wake_queue(dev);

	spin_unlock_irqrestore(&cp->lock, flags);
}

#ifdef BROKEN
static int cp_change_mtu(struct net_device *dev, int new_mtu)
{
	struct cp_private *cp = netdev_priv(dev);
	int rc;
	unsigned long flags;

	/* check for invalid MTU, according to hardware limits */
	if (new_mtu < CP_MIN_MTU || new_mtu > CP_MAX_MTU)
		return -EINVAL;

	/* if network interface not up, no need for complexity */
	if (!netif_running(dev)) {
		dev->mtu = new_mtu;
		cp_set_rxbufsize(cp); /* set new rx buf size */
		return 0;
	}

	spin_lock_irqsave(&cp->lock, flags);

	cp_stop_hw(cp);	/* stop h/w and free rings */
	cp_clean_rings(cp);

	dev->mtu = new_mtu;
	cp_set_rxbufsize(cp); /* set new rx buf size */

	rc = cp_init_rings(cp); /* realloc and restart h/w */
	cp_start_hw(cp);

	spin_unlock_irqrestore(&cp->lock, flags);

	return rc;
}
#endif /* BROKEN */


static char mii_2_8139_map[16] = {
	1, /* MII_BMCR */
	1, /* MII_BMSR */
	1, /* MII_PHYSID1 */
	1, /* MII_PHYSID2  */
	1, /* MII_ADVERTISE */
	1, /* MII_LPA */
	1, /* MII_EXPANSION */
	0, /* */
	0, /* */
	1, /* MII_CTRL1000 */
	1, /* MII_STAT1000 */
	0,
	0,
	0,
	0,
	1  /* MII_ESTATUS */
};


static DEFINE_MUTEX(cp_mdio_mutex);
static inline void mdio_mutex_lock(void)
{
	if (in_atomic() ||  irqs_disabled())
		return;

	mutex_lock(&cp_mdio_mutex);
}

static inline void mdio_mutex_unlock(void)
{
	if (in_atomic() ||  irqs_disabled())
		return;

	mutex_unlock(&cp_mdio_mutex );
}

static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	struct cp_private *cp = netdev_priv(dev);
	int retry = 1000000;

	mdio_mutex_lock();

	if (location < 16 && mii_2_8139_map[location]) {
		cpw32(MIIAR, 0x00000000 + (phy_id << 26)+ (location <<16));
		do {
			udelay(1);
			if ((retry--) <= 0) {
				printk(KERN_ERR "%s : timeout (MIIAR = 0x%x)\n",__func__,
				       cpr32(MIIAR));
				break;
			}
		} while (!(cpr32(MIIAR) & 0x80000000));

		mdio_mutex_unlock();
		return cpr32(MIIAR) & 0xffff;
	} else {
		mdio_mutex_unlock();
		return 0;
	}
}

static void mdio_write(struct net_device *dev, int phy_id, int location, int value)
{
	struct cp_private *cp = netdev_priv(dev);
	int retry = 1000000;

	mdio_mutex_lock();

	if (location < 16 && mii_2_8139_map[location]) {
		cpw32(MIIAR, 0x80000000 + (phy_id << 26) + (location <<16)+ value);
		do {
			udelay(1);
			if ((retry--) <= 0) {
				printk(KERN_ERR "%s : timeout (MIIAR = 0x%x)\n",__func__,
				       cpr32(MIIAR));
				break;
			}
		} while (cpr32(MIIAR) & 0x80000000);
	}

	mdio_mutex_unlock();

}

/* Set the ethtool Wake-on-LAN settings */
static int netdev_set_wol (struct cp_private *cp, const struct ethtool_wolinfo *wol)
{
	return 0;
}


/* Get the ethtool Wake-on-LAN settings */
static void netdev_get_wol (struct cp_private *cp, struct ethtool_wolinfo *wol)
{
}

static void cp_get_drvinfo (struct net_device *dev, struct ethtool_drvinfo *info)
{
	strcpy (info->driver, DRV_NAME);
	strcpy (info->version, DRV_VERSION);
}


static int cp_get_regs_len(struct net_device *dev)
{
	return CP_REGS_SIZE;
}


static int cp_get_sset_count (struct net_device *dev, int sset)
{
	switch (sset) {
		case ETH_SS_STATS:
			return CP_NUM_STATS;
		default:
			return -EOPNOTSUPP;
	}
}

static int cp_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct cp_private *cp = netdev_priv(dev);
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&cp->lock, flags);
	rc = mii_ethtool_gset(&cp->mii_if, cmd);
	spin_unlock_irqrestore(&cp->lock, flags);

	return rc;
}

static int cp_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct cp_private *cp = netdev_priv(dev);
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&cp->lock, flags);
	rc = mii_ethtool_sset(&cp->mii_if, cmd);
	spin_unlock_irqrestore(&cp->lock, flags);

	return rc;
}

static int cp_nway_reset(struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	return mii_nway_restart(&cp->mii_if);
}

static u32 cp_get_msglevel(struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	return cp->msg_enable;
}

static void cp_set_msglevel(struct net_device *dev, u32 value)
{
	struct cp_private *cp = netdev_priv(dev);
	cp->msg_enable = value;
}

static int cp_set_features(struct net_device *dev, netdev_features_t features)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;

	if (!((dev->features ^ features) & NETIF_F_RXCSUM))
		return 0;

	spin_lock_irqsave(&cp->lock, flags);

	if (features & NETIF_F_RXCSUM)
		cp->cpcmd |= RxChkSum;
	else
		cp->cpcmd &= ~RxChkSum;

	cpw8(CMD, cp->cpcmd);
	spin_unlock_irqrestore(&cp->lock, flags);

	return 0;
}

static void cp_get_regs(struct net_device *dev, struct ethtool_regs *regs, void *p)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;

	if (regs->len < CP_REGS_SIZE)
		return /* -EINVAL */;

	regs->version = CP_REGS_VER;

	spin_lock_irqsave(&cp->lock, flags);
	memcpy_fromio(p, cp->regs, CP_REGS_SIZE);
	spin_unlock_irqrestore(&cp->lock, flags);
}

static void cp_get_wol (struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave (&cp->lock, flags);
	netdev_get_wol (cp, wol);
	spin_unlock_irqrestore (&cp->lock, flags);
}

static int cp_set_wol (struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct cp_private *cp = netdev_priv(dev);
	unsigned long flags;
	int rc;

	spin_lock_irqsave (&cp->lock, flags);
	rc = netdev_set_wol (cp, wol);
	spin_unlock_irqrestore (&cp->lock, flags);

	return rc;
}

static void cp_get_strings (struct net_device *dev, u32 stringset, u8 *buf)
{
	switch (stringset) {
		case ETH_SS_STATS:
			memcpy(buf, &ethtool_stats_keys, sizeof(ethtool_stats_keys));
			break;
		default:
			BUG();
			break;
	}
}

#if DEBUG
static void cp_show_desc (struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);
	struct cp_desc* rx_desc = &cp->rx_ring[0];
	struct cp_desc* tx_desc = &cp->tx_hqring[0];
	int i;

	for(i=0; i<CP_RX_RING_SIZE; i++){
		rx_desc = &cp->rx_ring[i];
		printk("rx_desc[%d](0x%08x) opts1 = 0x%08x addr = 0x%08x\n", i,
			(u32)rx_desc, rx_desc->opts1, rx_desc->addr);
	}

	printk("\n\n");
	for(i=0; i<CP_TX_RING_SIZE; i++){
		tx_desc = &cp->tx_hqring[i];
		printk("tx_desc[%d](0x%08x) opts1 = 0x%08x addr = 0x%08x\n", i,
			(u32)tx_desc, tx_desc->opts1, tx_desc->addr);
	}

}

static void cp_show_regs_datum (struct net_device *dev)
{
	struct cp_private *cp = netdev_priv(dev);

	printk("0x00: IDR:	0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		cpr8(0x5), cpr8(0x4), cpr8(0x3), cpr8(0x2), cpr8(0x1), cpr8(0x0));
	printk("0x00: MAR:      0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		cpr8(0xf), cpr8(0xe), cpr8(0xd), cpr8(0xc), cpr8(0xb), cpr8(0xa), cpr8(0x9), cpr8(0x8));
	printk("0x10: TxOk: 	0x%08x\n", cpr16(0x10));
	printk("0x12: RxOk: 	0x%08x\n", cpr16(0x12));
	printk("0x14: TxErr: 	0x%08x\n", cpr16(0x14));
	printk("0x16: RxErr: 	0x%08x\n", cpr16(0x16));
	printk("0x18: MissPkt: 	0x%08x\n", cpr16(0x18));
	printk("0x1a: FAE: 	0x%08x\n", cpr16(0x1a));
	printk("0x1c: Tx1Col: 	0x%08x\n", cpr16(0x1c));
	printk("0x1e: TxMcol: 	0x%08x\n", cpr16(0x1e));
	printk("0x20: RxOkPhy: 	0x%08x\n", cpr16(0x20));
	printk("0x22: RxOkBrd: 	0x%08x\n", cpr16(0x22));
	printk("0x24: RxOkMul: 	0x%08x\n", cpr16(0x24));
	printk("0x26: TxAbt: 	0x%08x\n", cpr16(0x26));
	printk("0x28: TXUNDERC: 0x%08x\n", cpr16(0x28));
	printk("0x34: TXUNDER: 	0x%08x\n", cpr32(0x34));
	printk("0x38: CPUTag: 	0x%08x\n", cpr32(0x38));
	printk("0x3b: CmdReg: 	0x%08x\n", cpr8(0x3b));
	printk("0x3c: IsrMsk: 	0x%08x\n", cpr16(0x3c));
	printk("0x3e: ISR: 	0x%08x\n", cpr16(0x3e));
	printk("0x40: TxCfg: 	0x%08x\n", cpr32(0x40));
	printk("0x44: RxCfg: 	0x%08x\n", cpr32(0x44));
	printk("0x58: MSR: 	0x%08x\n", cpr8(0x58));
	printk("0x59: MSR1: 	0x%08x\n", cpr8(0x59));
	printk("0x5c: MIIAR: 	0x%08x\n", cpr8(0x5c));
	printk("\n\n\n");
	printk("0x100: TxFDP1: 	0x%08x\n", cpr32(0x100));
	printk("0x104: TxCDO1: 	0x%08x\n", cpr16(0x104));
	printk("0x110: TxFDP2: 	0x%08x\n", cpr32(0x110));
	printk("0x114: TxCDO2: 	0x%08x\n", cpr16(0x114));
	printk("0x120: TxFDP3: 	0x%08x\n", cpr32(0x120));
	printk("0x124: TxCDO3: 	0x%08x\n", cpr16(0x124));
	printk("0x130: TxFDP4: 	0x%08x\n", cpr32(0x130));
	printk("0x134: TxCDO4: 	0x%08x\n", cpr16(0x134));
	printk("0x1f0: RxFDP: 	0x%08x\n", cpr32(0x1f0));
	printk("0x1f4: RxCDO: 	0x%08x\n", cpr16(0x1f4));
	printk("0x1f6: RxRingS:	0x%08x\n", cpr16(0x1f6));
	printk("0x1fc: SMSA:	0x%08x\n", cpr32(0x1fc));
	printk("0x203: ProbSel:	0x%08x\n", cpr8(0x203));
	printk("0x22c: Rx_Pse_Des_Thres_h:	0x%08x\n", cpr8(0x22c));
	printk("0x230: EthrntRxCPU_Des_Num:	0x%08x\n", cpr8(0x230));
	printk("0x231: Rx_Pse_Des_Thres_on:	0x%08x\n", cpr8(0x231));
	printk("0x232: Rx_Pse_Des_Thres_off:	0x%08x\n", cpr8(0x232));
	printk("0x233: EthrntRxCPU_Des_Num_h:	0x%08x\n", cpr8(0x233));
	printk("0x234: Ethernet_IO_CMD:		0x%08x\n", cpr32(0x234));
	printk("0x23c: Ethernet_CTRL:		0x%08x\n", cpr32(0x23c));
	printk("0x240: Ethernet_DBG:		0x%08x\n", cpr32(0x240));
	printk("0x244: Ethernet_DESC_1:		0x%08x\n", cpr32(0x244));
	printk("0x248: Ethernet_DESC_2:		0x%08x\n", cpr32(0x248));
	printk("0x24c: Ethernet_CTRL_1:		0x%08x\n", cpr32(0x24c));
	printk("0x250: Ethernet_DESC_last2_1:	0x%08x\n", cpr32(0x250));
	printk("0x254: Ethernet_DESC_last2_2:	0x%08x\n", cpr32(0x254));
	printk("0x258: Ethernet_phase:		0x%08x\n", cpr32(0x258));
	printk("0x25c: FPGA:			0x%08x\n", cpr32(0x25c));
	printk("0x260: FPGA:			0x%08x\n", cpr32(0x260));
}
#endif

static void cp_get_ethtool_stats (struct net_device *dev,
		  struct ethtool_stats *estats, u64 *tmp_stats)
{
	struct cp_private *cp = netdev_priv(dev);
	int i;

	i = 0;

	tmp_stats[i++] = cpr16(TXOKCNT);
	tmp_stats[i++] = cpr16(RXOKCNT);
	tmp_stats[i++] = cpr16(TXERR);
	tmp_stats[i++] = cpr16(RXERRR);
	tmp_stats[i++] = cpr16(MISSPKT);
	tmp_stats[i++] = cpr16(FAE);
	tmp_stats[i++] = cpr16(TX1COL);
	tmp_stats[i++] = cpr16(TXMCOL);
	tmp_stats[i++] = cpr16(RXOKPHY);
	tmp_stats[i++] = cpr16(RXOKBRD);
	tmp_stats[i++] = cpr16(RXOKMUL);
	tmp_stats[i++] = cpr16(TXABT);
	tmp_stats[i++] = cpr16(TXUNDRN);
	tmp_stats[i++] = cp->cp_stats.rx_frags;
	tmp_stats[i++] = cpr8(MSR);
	tmp_stats[i++] = cpr8(MSR1);

	if (i != CP_NUM_STATS)
		BUG();
}

static const struct ethtool_ops cp_ethtool_ops = {
	.get_drvinfo		= cp_get_drvinfo,
	.get_regs_len		= cp_get_regs_len,
	.get_sset_count		= cp_get_sset_count,
	.get_settings		= cp_get_settings,
	.set_settings		= cp_set_settings,
	.nway_reset		= cp_nway_reset,
	.get_link		= ethtool_op_get_link,
	.get_msglevel		= cp_get_msglevel,
	.set_msglevel		= cp_set_msglevel,
	.get_regs		= cp_get_regs,
	.get_wol		= cp_get_wol,
	.set_wol		= cp_set_wol,
	.get_strings		= cp_get_strings,
	.get_ethtool_stats	= cp_get_ethtool_stats,
};

static int cp_ioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct cp_private *cp = netdev_priv(dev);
	int rc;
	unsigned long flags;

	if (!netif_running(dev))
		return -EINVAL;

	spin_lock_irqsave(&cp->lock, flags);
	rc = generic_mii_ioctl(&cp->mii_if, if_mii(rq), cmd, NULL);
	spin_unlock_irqrestore(&cp->lock, flags);
	return rc;
}

static int cp_set_mac_address(struct net_device *dev, void *p)
{
	struct cp_private *cp = netdev_priv(dev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	spin_lock_irq(&cp->lock);
	cpw32(IDR0 + 0, le32_to_cpu (*(__le32 *) (dev->dev_addr + 0)));
	cpw32(IDR0 + 4, le32_to_cpu (*(__le32 *) (dev->dev_addr + 4)));
	spin_unlock_irq(&cp->lock);

	return 0;
}

static const struct net_device_ops cp_netdev_ops = {
	.ndo_open		= cp_open,
	.ndo_stop		= cp_close,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= cp_set_mac_address,
	.ndo_set_rx_mode	= cp_set_rx_mode,
	.ndo_get_stats		= cp_get_stats,
	.ndo_do_ioctl		= cp_ioctl,
	.ndo_start_xmit		= cp_start_xmit,
	.ndo_tx_timeout		= cp_tx_timeout,
	.ndo_set_features	= cp_set_features,
#ifdef BROKEN
	.ndo_change_mtu		= cp_change_mtu,
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cp_poll_controller,
#endif
};

static int cp_probe (struct platform_device *pdev)
{
	struct net_device *dev;
	struct cp_private *cp;
	struct resource *res = NULL;
	u64 rsrc_start;
	u64 rsrc_len;
	const char *env_base;
	int rc;
	int ret;
#ifndef MODULE
	static int version_printed;
	if (version_printed++ == 0)
		printk("%s", version);
#endif
	dev = alloc_etherdev(sizeof(struct cp_private));
	if (!dev)
		return -ENOMEM;
	platform_set_drvdata(pdev, dev);
	SET_NETDEV_DEV(dev, &pdev->dev);

	cp = netdev_priv(dev);
	cp->pdev = pdev;
	cp->dev = dev;
	cp->msg_enable = (debug < 0 ? CP_DEF_MSG_ENABLE : debug);
	spin_lock_init (&cp->lock);

	init_waitqueue_head (&cp->thr_wait); /* for hotplug */
	init_completion (&cp->thr_exited);   /* for hotplug */

	/* set GNIC device base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		printk (KERN_ERR __FILE__ ": get mem resource failed!\n");
		return -ENOMEM;
	}

	rsrc_start = res->start;
	rsrc_len = res->end - res->start + 1;
	cp->regs = ioremap_nocache(rsrc_start, rsrc_len);
	dev->base_addr = (unsigned long) cp->regs;

	cp->mii_if.dev = dev;
	cp->mii_if.mdio_read = mdio_read;
	cp->mii_if.mdio_write = mdio_write;
	cp->mii_if.phy_id = CP_INTERNAL_PHY;
	cp->mii_if.phy_id_mask = 0x1f;
	cp->mii_if.reg_num_mask = 0x1f;
	cp->mii_if.supports_gmii = mii_check_gmii_support(&cp->mii_if);
	cp_set_rxbufsize(cp);
	cp_stop_hw(cp);

	/* default MAC address */
	((u16 *) (dev->dev_addr))[0] =0x0000;
	((u16 *) (dev->dev_addr))[1] =0x0000;
	((u16 *) (dev->dev_addr))[2] =0x0000;
	((u16 *) (dev->dev_addr))[3] =0x0000;

	/* read MAC address from FLASH */
	ret = of_property_read_string(pdev->dev.of_node, "mac-address", &env_base);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read mac address from DT\n");
		return -EINVAL;
	}

	dev->dev_addr[0] = (u8)simple_strtoul( env_base, NULL, 16 );
	dev->dev_addr[1] = (u8)simple_strtoul( env_base+3, NULL, 16 );
	dev->dev_addr[2] = (u8)simple_strtoul( env_base+6, NULL, 16 );
	dev->dev_addr[3] = (u8)simple_strtoul( env_base+9, NULL, 16 );
	dev->dev_addr[4] = (u8)simple_strtoul( env_base+12, NULL, 16 );
	dev->dev_addr[5] = (u8)simple_strtoul( env_base+15, NULL, 16 );
	dev->dev_addr[6] = (u8)(0x00);
	dev->dev_addr[7] = (u8)(0x00);

	dev->netdev_ops = &cp_netdev_ops;
	dev->hw_features |= NETIF_F_SG;
	dev->ethtool_ops = &cp_ethtool_ops;
	netif_napi_add(dev, &cp->napi, cp_rx_poll, 16);
	dev->watchdog_timeo = TX_TIMEOUT;

#if CP_VLAN_TAG_USED
	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
	dev->vlan_rx_register = cp_vlan_rx_register;
	dev->vlan_rx_kill_vid = cp_vlan_rx_kill_vid;
#endif

	dev->features |= NETIF_F_HIGHDMA;
	dev->features |= NETIF_F_SG | NETIF_F_RXCSUM;
	dev->features |= NETIF_F_IP_CSUM;

	/* Set up device irq number */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		printk (KERN_ERR __FILE__ ": get irq resource failed!\n");
		return -ENOMEM;
	}

	dev->irq = res->start;

	rc = register_netdev(dev);
	if (rc)
		goto err_out_iomap;

	printk (KERN_INFO "%s: GMAC8139 at 0x%lx, "
		"%02x:%02x:%02x:%02x:%02x:%02x, "
		"IRQ %d\n",
		dev->name,
		dev->base_addr,
		dev->dev_addr[0], dev->dev_addr[1],
		dev->dev_addr[2], dev->dev_addr[3],
		dev->dev_addr[4], dev->dev_addr[5],
		dev->irq);

	return 0;

err_out_iomap:

	iounmap(cp->regs);
	free_netdev(dev);
	return rc;
}

static int cp_remove (struct platform_device *pdev)
{
	struct net_device *dev;

	dev = platform_get_drvdata (pdev);
	unregister_netdev(dev);
	free_netdev(dev);
	return 0;
}

#ifdef CONFIG_PM
static int cp_pm_suspend (struct device *p_dev)
{
	struct net_device *dev;
	if (cp_status == CP_ON) {
		dev = dev_get_drvdata (p_dev);
		cp_close(dev);
	}
	return 0;
}


static int cp_pm_resume (struct device *p_dev)
{
	struct net_device *dev;

	if (cp_status ==CP_OFF) {
		dev = dev_get_drvdata (p_dev);
		if (!dev)
			return 0;
		cp_open(dev);
	}
	return 0;
}

static const struct dev_pm_ops cp_pm_ops = {
	.suspend    = cp_pm_suspend,
	.resume     = cp_pm_resume,
#ifdef CONFIG_HIBERNATION
	.freeze     = cp_pm_suspend,
	.thaw       = cp_pm_resume,
	.poweroff   = cp_pm_suspend,
	.restore    = cp_pm_resume,
#endif
};
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static const struct of_device_id cp_of_match[] = {
	{ .compatible = "realtek,8139gb" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, cp_of_match);
#endif

static struct platform_driver cp_driver = {
	.probe = cp_probe,
	.remove = cp_remove,
	.driver = {
		.name		= (char *)drv_name,
		.owner		= THIS_MODULE,
		.bus		= &platform_bus_type,
		.of_match_table = of_match_ptr(cp_of_match),
#ifdef CONFIG_PM
		.pm		= &cp_pm_ops,
#endif
	},
};

module_platform_driver(cp_driver);
