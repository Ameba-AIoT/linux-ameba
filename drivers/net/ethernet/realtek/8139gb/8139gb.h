/*
 * Realtek Semiconductor Corp.
 *
 * 8139gb.h:
 *   A header file for 8139gb linux ethernet driver
 *
 * Copyright (C) 2006-2012 Jethro Hsu (jethro@realtek.com)
 */

#include <linux/ioport.h>

/* VLAN tagging feature enable/disable */
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define CP_VLAN_TAG_USED 1
#define CP_VLAN_TX_TAG(tx_desc,vlan_tag_value) \
	do { (tx_desc)->opts2 = (vlan_tag_value); } while (0)
#else
#define CP_VLAN_TAG_USED 0
#define CP_VLAN_TX_TAG(tx_desc,vlan_tag_value) \
	do { (tx_desc)->opts2 = 0; } while (0)
#endif

#ifndef TRUE
#define FALSE 0
#define TRUE (!FALSE)
#endif

#define CP_DEF_MSG_ENABLE   (NETIF_MSG_DRV   | \
                             NETIF_MSG_PROBE | \
                             NETIF_MSG_LINK)
#define CP_NUM_STATS        16          /* struct cp_dma_stats, plus one */
#define CP_STATS_SIZE       64          /* size in bytes of DMA stats block */
#define CP_REGS_SIZE        (0xff + 1)
#define CP_REGS_VER         1           /* version 1 */
#define CP_RX_RING_SIZE     128
#define CP_TX_RING_SIZE     32
#define DESC_ALIGN          0x100


#define CP_RXRING_BYTES ( (sizeof(struct cp_desc) * (CP_RX_RING_SIZE+1)) + DESC_ALIGN)
#define CP_TXRING_BYTES ( (sizeof(struct cp_desc) * (CP_TX_RING_SIZE+1)) + DESC_ALIGN)

#define NEXT_TX(N)      (((N) + 1) & (CP_TX_RING_SIZE - 1))
#define NEXT_RX(N)      (((N) + 1) & (CP_RX_RING_SIZE - 1))

#define TX_HQBUFFS_AVAIL(CP)     (((CP)->tx_hqtail <= (CP)->tx_hqhead) ?         \
                                 (CP)->tx_hqtail + (CP_TX_RING_SIZE - 1) - (CP)->tx_hqhead :   \
                                 (CP)->tx_hqtail - (CP)->tx_hqhead - 1)

#define PKT_BUF_SZ          1536    /* Size of each temporary Rx buffer.*/
#define CP_INTERNAL_PHY     1

/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT          (6*HZ)

/* hardware minimum and maximum for a single frame's data payload */
#define CP_MIN_MTU          60  /* TODO: allow lower, but pad */
#define CP_MAX_MTU          4096

#define CP_OFF          0
#define CP_ON           1
#define CP_UNDER_INIT   2
volatile static int cp_status = CP_OFF;

/* Regs Mapping Definition */

enum PHY_REGS
{
        TXFCE           = 1<<6,
        RXFCE           = 1<<5,
        SP1000          = 1<<4,
        LINK            = 1<<2,
        TXPF            = 1<<1,
        RXPF            = 1<<0,
        FORCE_TX        = 1<<7
};

enum
{
	/* Ethernet Module Registers */
	IDR0                    = 0,           /* Ethernet ID */
	IDR1                    = 0x1,         /* Ethernet ID */
	IDR2                    = 0x2,         /* Ethernet ID */
	IDR3                    = 0x3,         /* Ethernet ID */
	IDR4                    = 0x4,         /* Ethernet ID */
	IDR5                    = 0x5,         /* Ethernet ID */
	MAR0                    = 0x8,         /* Multicast register */
	MAR1                    = 0x9,
	MAR2                    = 0xa,
	MAR3                    = 0xb,
	MAR4                    = 0xc,
	MAR5                    = 0xd,
	MAR6                    = 0xe,
	MAR7                    = 0xf,
	TXOKCNT                 = 0x10,
	RXOKCNT                 = 0x12,
	TXERR                   = 0x14,
	RXERRR                  = 0x16,
	MISSPKT                 = 0x18,
	FAE                     = 0x1A,
	TX1COL                  = 0x1c,
	TXMCOL                  = 0x1e,
	RXOKPHY                 = 0x20,
	RXOKBRD                 = 0x22,
	RXOKMUL                 = 0x24,
	TXABT                   = 0x26,
	TXUNDRN                 = 0x28,
	TRSR                    = 0x34,
	CMD                     = 0x3b,
	IMR                     = 0x3C,
	ISR                     = 0x3E,
	TCR                     = 0x40,
	RCR                     = 0x44,
	OCPCMD			= 0x48,
	MSR                     = 0x58,
	MSR1                    = 0x59,
	MIIAR                   = 0x5C,
	/* CPU Interface Registers */
	TxFDP1                  = 0x0100,
	TxCDO1                  = 0x0104,
	TXCPO1                  = 0x0108,
	TXPSA1                  = 0x010A,
	TXCPA1                  = 0x010C,
	LastTxCDO1              = 0x0110,
	TXPAGECNT1              = 0x0112,
	Tx1ScratchDes           = 0x0150,
	TxFDP2                  = 0x0110,
	TxCDO2                  = 0x0114,
	TXCPO2                  = 0x0188,
	TXPSA2                  = 0x018A,
	TXCPA2                  = 0x018C,
	LASTTXCDO2              = 0x0190,
	TXPAGECNT2              = 0x0192,
	Tx2ScratchDes           = 0x01A0,
	RxFDP                   = 0x01F0,
	RxCDO                   = 0x01F4,
	RxRingSize              = 0x01F6,
	RxCPO                   = 0x01F8,
	RxPSA                   = 0x01FA,
	RxCPA                   = 0x1FC,
	RXPLEN                  = 0x200,
	RXPFDP                  = 0x0204,
	RXPAGECNT               = 0x0208,
	RXSCRATCHDES            = 0x0210,
	EthrntRxCPU_Des_Num     = 0x0230,
	EthrntRxCPU_Des_Wrap    = 0x0231,
	Rx_Pse_Des_Thres        = 0x0232,
	IO_CMD                  = 0x0234,
	Rx_Pse_Des_Thres_h      = 0x022c,
	Pse_Des_Thres_on        = 0x0231,
	Rx_Pse_Des_Thres_off    = 0x0232,
	EthrntRxCPU_Des_Num_h   = 0x0233,
	Ethrnt_Ctrl             = 0x023c,
	Ethrnt_Ctrl1            = 0x024c,
	Ethrnt_Phase_Sel        = 0x0258,
	TX_OWN                  = (1<<5),
	RX_OWN                  = (1<<4),
	MII_RE                  = (1<<3),
	MII_TE                  = (1<<2),
	TX_FNL                  = (1<1),
	TX_FNH                  = (1),
	RXMII                   = MII_RE,
	TXMII                   = MII_TE,
};

/* Tx and Rx status descriptors */
#define	DescOwn          (1 << 31)	/* Descriptor is owned by NIC */
#define	RingEnd          (1 << 30)	/* End of descriptor ring */
#define	FirstFrag        (1 << 29)	/* First segment of a packet */
#define	LastFrag         (1 << 28)	/* Final segment of a packet */

/* Tx DescOwn == 1 */
#define IPCS             (1 << 27)     /* Tx: Calculate IP checksum */
#define UDPCS            (1 << 26)     /* Tx: Calculate UDP/IP checksum */
#define TCPCS            (1 << 25)     /* Tx: Calculate TCP/IP checksum */
#define TxCRC            (1 << 23)     /* Tx: Append CRC */

/* Rx DescOwn == 0 */
#define RxErrFrame       (1 << 27)     /* Rx: Frame alignment error */
#define MulAddrRx        (1 << 26)     /* RX: Multicast Address */
#define PhyAddrRx        (1 << 25)     /* RX: Physical Address Matched */
#define BrdAddrRx        (1 << 24)     /* RX: Broadcast Address */
#define E8023            (1 << 22)     /* Rx: Ethernet 802.3 packet  */
#define RWT              (1 << 21)     /* Rx: Watchdog Timer expired */
#define	RxError          (1 << 20)     /* Rx: Error summary */
#define RxErrRunt        (1 << 19)     /* Rx: Error, packet < 64 bytes */
#define RxErrCRC         (1 << 18)     /* Rx: CRC error */
#define PID1             (1 << 17)     /* Rx: 2 protocol id bits:  0==non-IP, */
#define PID0             (1 << 16)     /* Rx: 1==UDP/IP, 2==TCP/IP, 3==IP */
#define IPFail           (1 << 15)     /* Rx: IP checksum failed */
#define UDPFail          (1 << 14)     /* Rx: UDP/IP checksum failed */
#define TCPFail          (1 << 13)     /* Rx: TCP/IP checksum failed */
#define IPSegment	 (1 << 12)     /* Rx: IP packet have more fragment */
#define IPV6		 (1 << 11)     /* Rx: Indicate it's an IPv6 packet */
#define RxVlanTagged     (1 << 16)     /* Rx: Opts2: VLAN tag available */

/* Misc. */
#define	TxVlanTag        (1 << 17)     /* Add VLAN tag */
#define	NormalTxPoll     (1 << 6) 	/* One or more normal Tx packets to send */
#define	RxProtoTCP       1
#define	RxProtoUDP       2
#define	RxProtoIP        3
#define	RxVlanOn         (1 << 2) 	/* Rx VLAN de-tagging enable */
#define	RxChkSum         (1 << 1) 	/* Rx checksum offload enable */

enum CP_THRESHOLD_REGS
{
	THVAL           = 16,
	THOFFVAL        = 32,
	RINGSIZE        = 2,         /* 0: 16 desc , 1: 32 desc , 2: 64 desc . */
	RINGSIZE_2      = 0x7f00,
	LOOPBACK        = (0x3 << 8),
	AcceptFlctl     = 0x40,
	AcceptErr       = 0x20,      /* Accept packets with CRC errors */
	AcceptRunt      = 0x10,      /* Accept runt (<64 bytes) packets */
	AcceptBroadcast = 0x08,      /* Accept broadcast packets */
	AcceptMulticast = 0x04,      /* Accept multicast packets */
	AcceptMyPhys    = 0x02,      /* Accept pkts with our MAC as dest */
	AcceptAllPhys   = 0x01,      /* Accept all pkts w/ physical dest */
	AcceptAll          = AcceptBroadcast | AcceptMulticast | AcceptMyPhys  | AcceptAllPhys | AcceptErr | AcceptRunt,
	AcceptNoBroad      = AcceptMulticast | AcceptMyPhys    | AcceptAllPhys | AcceptErr | AcceptRunt,
	AcceptNoMulti      = AcceptMyPhys    | AcceptAllPhys   | AcceptErr     | AcceptRunt,
	NoErrAccept        = AcceptBroadcast | AcceptMulticast | AcceptMyPhys,
	NoErrPromiscAccept = AcceptBroadcast | AcceptMulticast | AcceptMyPhys |  AcceptAllPhys,
};

enum CP_ISR_REGS
{
        SW_INT      = (1 <<10),
        TX_EMPTY    = (1 << 9),
        LINK_CHG    = (1 << 8),
        TX_ERR      = (1 << 7),
        TX_OK       = (1 << 6),
        RX_EMPTY    = (1 << 5),
        RX_FIFOOVR  = (1 << 4),
        RX_ERR      = (1 << 2),
        RESP_ERR    = (1 << 1),
        RX_OK       = (1 << 0),
};

static  const u32 cp_norx_intr_mask = TX_OK | TX_ERR | TX_EMPTY ;
static  const u32 cp_rx_intr_mask   = RX_OK | RX_ERR | RX_EMPTY | RX_FIFOOVR | LINK_CHG;
static  const u32 cp_intr_mask      = LINK_CHG | TX_OK | TX_ERR | TX_EMPTY | RX_OK | RX_ERR | RX_EMPTY | RX_FIFOOVR ;

enum CP_IOCMD_REG
{
        TX_FIFO     = 1,
        TX_POLL     = 1 << 0,
        ST_DES_FT   = 1,
	CMD_RX_EN = 0x20 | TX_FIFO<<19 | ST_DES_FT <<30,
        CMD_CONFIG = 0x30 | TX_FIFO<<19 | ST_DES_FT <<30,
};

/* Data Struct Definition */
struct cp_desc {
        u32     opts1;
        u32     addr;
        u32     opts2;
        u32     opts3;
};

struct ring_info {
        struct sk_buff*     skb;
        dma_addr_t          mapping;
        unsigned            frag;
};

struct cp_dma_stats {
        u64         tx_ok;
        u64         rx_ok;
        u64         tx_err;
        u32         rx_err;
        u16         rx_fifo;
        u16         frame_align;
        u32         tx_ok_1col;
        u32         tx_ok_mcol;
        u64         rx_ok_phys;
        u64         rx_ok_bcast;
        u32         rx_ok_mcast;
        u16         tx_abort;
        u16         tx_underrun;
} __attribute__((packed));

struct cp_extra_stats {
        unsigned long           rx_frags;
        unsigned long           tx_timeouts;
        unsigned long           tx_cnt;
};

struct cp_private {
        unsigned                tx_hqhead   ____cacheline_aligned;
        unsigned                tx_hqtail;
        unsigned                tx_lqhead   ____cacheline_aligned;
        unsigned                tx_lqtail;

        void 			__iomem *regs;
        struct net_device	*dev;
        spinlock_t              lock;
        u32                     msg_enable;

        struct napi_struct      napi;

	struct platform_device  *pdev;
        u32                     rx_config;
        u16                     cpcmd;

        struct cp_extra_stats   cp_stats;

        unsigned                rx_head         ____cacheline_aligned;
        unsigned                rx_tail;
        struct cp_desc		*rx_ring;
        struct ring_info        rx_skb[CP_RX_RING_SIZE];

        unsigned                rx_buf_sz;

        struct cp_desc		*tx_hqring;
        struct cp_desc		*tx_lqring;
        struct ring_info        tx_skb[CP_TX_RING_SIZE];
        dma_addr_t              ring_dma;

        struct sk_buff*         frag_skb;
        unsigned                dropping_frag : 1;
        char			*rxdesc_buf;
        char			*txdesc_buf;
#if CP_VLAN_TAG_USED
        struct vlan_group*      vlgrp;
#endif
        unsigned int            wol_enabled : 1; /* Is Wake-on-LAN enabled? */
        struct mii_if_info      mii_if;
        pid_t                   thr_pid;
        wait_queue_head_t       thr_wait;
        struct completion       thr_exited;
        int                     time_to_die;
};

static struct {
        const char str[ETH_GSTRING_LEN];
} ethtool_stats_keys[] =
{
        { "tx_ok" },
        { "rx_ok" },
        { "tx_err" },
        { "rx_err" },
        { "rx_fifo" },
        { "frame_align" },
        { "tx_ok_1col" },
        { "tx_ok_mcol" },
        { "rx_ok_phys" },
        { "rx_ok_bcast" },
        { "rx_ok_mcast" },
        { "tx_abort" },
        { "tx_underrun" },
        { "rx_frags" },
};

#define cpr8(reg)       readb((cp->regs) + (reg))
#define cpr16(reg)      readw((cp->regs) + (reg))
#define cpr32(reg)      readl((cp->regs) + (reg))
#define cpw8(reg,val)   writeb((val), (cp->regs) + (reg))
#define cpw16(reg,val)  writew((val), (cp->regs) + (reg))
#define cpw32(reg,val)  writel((val), (cp->regs) + (reg))

#define cpw8_f(reg,val) do {            \
        writeb((val), (cp->regs) + (reg));\
        readb(cp->regs + (reg));        \
        } while (0)

#define cpw16_f(reg,val) do {           \
        writew((val), (cp->regs) + (reg));\
        readw(cp->regs + (reg));        \
        } while (0)

#define cpw32_f(reg,val) do {           \
        writel((val), (cp->regs) + (reg));\
        readl(cp->regs + (reg));        \
        } while (0)

#define __cp_set_interrupt_mask(mask)   cpw16_f(IMR, mask)
#define __cp_clear_interrupts(mask)     cpw16_f(ISR, mask)
#define __cp_get_interrupts()           cpr16(ISR)
#define __cp_io_command(cmd)            cpw32(IO_CMD, cmd)
#define __cp_get_msr()                  cpr8(MSR)
#define __cp_set_msr(msr)               cpw8(MSR,msr)
