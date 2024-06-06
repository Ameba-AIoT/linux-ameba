// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek UART support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/clk.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sysrq.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/pm_wakeirq.h>
#include <linux/suspend.h>
#include <linux/clk-provider.h>

#define DLL					0x000				  /*UART DIVISOR LENGTH REGISTER*/
#define IER					0x004				  /*UART INTERRUPT ENABLE REGISTER*/
#define IIR					0x008				  /*UART INTERRUPT IDENTIFICATION REGISTER*/
#define LCR					0x00C				  /*UART LINE CONTROL REGISTER*/
#define MCR					0x010				  /*UART MODEM CONTROL REGISTER*/
#define LSR					0x014				  /*UART LINE STATUS REGISTER*/
#define MSR					0x018				  /*UART MODEM STATUS REGISTER*/
#define SCR					0x01C				  /*UART SCRATCH PAD REGISTER*/
#define STS					0x020				  /*UART STS REGISTER*/
#define RBR_OR_UART_THR		0x024				  /*UART RECEIVER BUFFER REGISTER/UART TRANSMITTER HOLDING REGISTER*/
#define MISCR				0x028				  /*UART MISC CONTROL REGISTER*/
#define SIR_TX_PWC0			0x02C				  /*UART IRDA SIR TX PULSE WIDTH CONTROL 0 REGISTER*/
#define SIR_RX_PFC			0x030				  /*UART IRDA SIR RX PULSE FILTER CONTROL REGISTER*/
#define BAUD_MON 			0x034				  /*UART BAUD MONITOR REGISTER*/
#define DBGR 				0x03C				  /*UART DEBUG REGISTER*/
#define RX_PATH_CTRL 		0x040				  /*UART RX PATH CONTROL REGISTER*/
#define MON_BAUD_CTRL		0x044				  /*UART MONITOR BAUD RATE CONTROL REGISTER*/
#define MON_BAUD_STS 		0x048				  /*UART MONITOR BAUD RATE STATUS REGISTER*/
#define MON_CYC_NUM			0x04C				  /*UART MONITORED CYCLE NUMBER REGISTER*/
#define RX_BYTE_CNT			0x050				  /*UART RX DATA BYTE COUNT REGISTER*/
#define FCR					0x054				  /*UART FIFO CONTROL REGISTER*/
#define ICR					0x058				  /*UART INTERRUPT CLEAR REGISTER*/
#define RXDBCR				0x05C				  /*UART RX DEBOUNCE CONTROL REGISTER*/

/**************************************************************************//**
 *  DLL
 *****************************************************************************/
#define RUART_MASK_BAUD                   ((u32)0x000000FF << 0)
#define RUART_BAUD(x)                     ((u32)(((x) & 0x000000FF) << 0))
#define RUART_GET_BAUD(x)                 ((u32)(((x >> 0) & 0x000000FF)))

/**************************************************************************//**
 *  IER
 *****************************************************************************/
#define RUART_BIT_ERXNDI                  ((u32)0x00000001 << 6)
#define RUART_BIT_ETOI                    ((u32)0x00000001 << 5)
#define RUART_BIT_EMDI                    ((u32)0x00000001 << 4)
#define RUART_BIT_EDSSI                   ((u32)0x00000001 << 3)
#define RUART_BIT_ELSI                    ((u32)0x00000001 << 2)
#define RUART_BIT_ETBEI                   ((u32)0x00000001 << 1)
#define RUART_BIT_ERBI                    ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  IIR
 *****************************************************************************/
#define RUART_BIT_INT_PEND                ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  LCR
 *****************************************************************************/
#define RUART_BIT_DLAB                    ((u32)0x00000001 << 7)
#define RUART_BIT_BRCTL                   ((u32)0x00000001 << 6)
#define RUART_BIT_STKP                    ((u32)0x00000001 << 5)
#define RUART_BIT_EPS                     ((u32)0x00000001 << 4)
#define RUART_BIT_PEN                     ((u32)0x00000001 << 3)
#define RUART_BIT_STB                     ((u32)0x00000001 << 2)
#define RUART_BIT_WLS0                    ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  MCR
 *****************************************************************************/
#define RUART_BIT_AFE                     ((u32)0x00000001 << 5)
#define RUART_BIT_LOOP_EN                 ((u32)0x00000001 << 4)
#define RUART_BIT_OUT2                    ((u32)0x00000001 << 3)
#define RUART_BIT_OUT1                    ((u32)0x00000001 << 2)
#define RUART_BIT_RTS                     ((u32)0x00000001 << 1)
#define RUART_BIT_DTR                     ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  LSR
 *****************************************************************************/
#define RUART_BIT_RXND_INT                ((u32)0x00000001 << 12)
#define RUART_BIT_MODEM_INT               ((u32)0x00000001 << 11)
#define RUART_BIT_MONITOR_DONE_INT        ((u32)0x00000001 << 10)
#define RUART_BIT_TIMEOUT_INT             ((u32)0x00000001 << 9)
#define RUART_BIT_RXFIFO_INT              ((u32)0x00000001 << 8)
#define RUART_BIT_RXFIFO_ERR              ((u32)0x00000001 << 7)
#define RUART_BIT_TX_NOT_FULL             ((u32)0x00000001 << 6)
#define RUART_BIT_TX_EMPTY                ((u32)0x00000001 << 5)
#define RUART_BIT_BREAK_INT               ((u32)0x00000001 << 4)
#define RUART_BIT_FRM_ERR                 ((u32)0x00000001 << 3)
#define RUART_BIT_PAR_ERR                 ((u32)0x00000001 << 2)
#define RUART_BIT_OVR_ERR                 ((u32)0x00000001 << 1)
#define RUART_BIT_DRDY                    ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  MSR
 *****************************************************************************/
#define RUART_BIT_R_DCD                   ((u32)0x00000001 << 7)
#define RUART_BIT_R_RI                    ((u32)0x00000001 << 6)
#define RUART_BIT_R_DSR                   ((u32)0x00000001 << 5)
#define RUART_BIT_R_CTS                   ((u32)0x00000001 << 4)
#define RUART_BIT_D_DCD                   ((u32)0x00000001 << 3)
#define RUART_BIT_TERI                    ((u32)0x00000001 << 2)
#define RUART_BIT_D_DSR                   ((u32)0x00000001 << 1)
#define RUART_BIT_D_CTS                   ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  SCR
 *****************************************************************************/
#define RUART_MASK_XFACTOR_ADJ            ((u32)0x000007FF << 16)
#define RUART_XFACTOR_ADJ(x)              ((u32)(((x) & 0x000007FF) << 16))
#define RUART_GET_XFACTOR_ADJ(x)          ((u32)(((x >> 16) & 0x000007FF)))
#define RUART_MASK_DBG_SEL                ((u32)0x0000001F << 8)
#define RUART_DBG_SEL(x)                  ((u32)(((x) & 0x0000001F) << 8))
#define RUART_GET_DBG_SEL(x)              ((u32)(((x >> 8) & 0x0000001F)))
#define RUART_BIT_SCRATCH_7               ((u32)0x00000001 << 7)
#define RUART_MASK_SCRATCH_6_0            ((u32)0x0000007F << 0)
#define RUART_SCRATCH_6_0(x)              ((u32)(((x) & 0x0000007F) << 0))
#define RUART_GET_SCRATCH_6_0(x)          ((u32)(((x >> 0) & 0x0000007F)))


/**************************************************************************//**
 *  STS
 *****************************************************************************/
#define RUART_MASK_XFACTOR                ((u32)0x000FFFFF << 4)
#define RUART_XFACTOR(x)                  ((u32)(((x) & 0x000FFFFF) << 4))
#define RUART_GET_XFACTOR(x)              ((u32)(((x >> 4) & 0x000FFFFF)))


/**************************************************************************//**
 *  RBR_OR_UART_THR
 *****************************************************************************/
#define RUART_MASK_DATABIT                ((u32)0x000000FF << 0)
#define RUART_DATABIT(x)                  ((u32)(((x) & 0x000000FF) << 0))
#define RUART_GET_DATABIT(x)              ((u32)(((x >> 0) & 0x000000FF)))


/**************************************************************************//**
 *  MISCR
 *****************************************************************************/
#define RUART_BIT_CLR_DUMMY_FLAG          ((u32)0x00000001 << 30)
#define RUART_MASK_DUMMY_DATA             ((u32)0x000000FF << 22)
#define RUART_DUMMY_DATA(x)               ((u32)(((x) & 0x000000FF) << 22))
#define RUART_GET_DUMMY_DATA(x)           ((u32)(((x >> 22) & 0x000000FF)))
#define RUART_BIT_RXDMA_OWNER             ((u32)0x00000001 << 21)
#define RUART_BIT_IRDA_RX_INV             ((u32)0x00000001 << 20)
#define RUART_BIT_IRDA_TX_INV             ((u32)0x00000001 << 19)
#define RUART_MASK_RXDMA_BURSTSIZE        ((u32)0x000000FF << 11)
#define RUART_RXDMA_BURSTSIZE(x)          ((u32)(((x) & 0x000000FF) << 11))
#define RUART_GET_RXDMA_BURSTSIZE(x)      ((u32)(((x >> 11) & 0x000000FF)))
#define RUART_MASK_TXDMA_BURSTSIZE        ((u32)0x000000FF << 3)
#define RUART_TXDMA_BURSTSIZE(x)          ((u32)(((x) & 0x000000FF) << 3))
#define RUART_GET_TXDMA_BURSTSIZE(x)      ((u32)(((x >> 3) & 0x000000FF)))
#define RUART_BIT_RXDMA_EN                ((u32)0x00000001 << 2)
#define RUART_BIT_TXDMA_EN                ((u32)0x00000001 << 1)
#define RUART_BIT_IRDA_ENABLE             ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  SIR_TX_PWC0
 *****************************************************************************/
#define RUART_BIT_UPPERBOUND_SHIFTRIGHT   ((u32)0x00000001 << 31)
#define RUART_MASK_UPPERBOUND_SHIFTVAL    ((u32)0x00007FFF << 16)
#define RUART_UPPERBOUND_SHIFTVAL(x)      ((u32)(((x) & 0x00007FFF) << 16))
#define RUART_GET_UPPERBOUND_SHIFTVAL(x)  ((u32)(((x >> 16) & 0x00007FFF)))
#define RUART_BIT_LOWBOUND_SHIFTRIGHT     ((u32)0x00000001 << 15)
#define RUART_MASK_LOWBOUND_SHIFTVAL      ((u32)0x00007FFF << 0)
#define RUART_LOWBOUND_SHIFTVAL(x)        ((u32)(((x) & 0x00007FFF) << 0))
#define RUART_GET_LOWBOUND_SHIFTVAL(x)    ((u32)(((x >> 0) & 0x00007FFF)))


/**************************************************************************//**
 *  SIR_RX_PFC
 *****************************************************************************/
#define RUART_MASK_R_SIR_RX_FILTER_THRS   ((u32)0x00007FFF << 1)
#define RUART_R_SIR_RX_FILTER_THRS(x)     ((u32)(((x) & 0x00007FFF) << 1))
#define RUART_GET_R_SIR_RX_FILTER_THRS(x) ((u32)(((x >> 1) & 0x00007FFF)))
#define RUART_BIT_R_SIR_RX_FILTER_EN      ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  BAUD_MON
 *****************************************************************************/
#define RUART_BIT_TOGGLE_MON_EN           ((u32)0x00000001 << 31)
#define RUART_BIT_MON_DATA_VLD            ((u32)0x00000001 << 30)
#define RUART_MASK_FALLING_THRESH         ((u32)0x0000003F << 24)
#define RUART_FALLING_THRESH(x)           ((u32)(((x) & 0x0000003F) << 24))
#define RUART_GET_FALLING_THRESH(x)       ((u32)(((x >> 24) & 0x0000003F)))
#define RUART_MASK_MIN_LOW_PERIOD         ((u32)0x00000FFF << 12)
#define RUART_MIN_LOW_PERIOD(x)           ((u32)(((x) & 0x00000FFF) << 12))
#define RUART_GET_MIN_LOW_PERIOD(x)       ((u32)(((x >> 12) & 0x00000FFF)))
#define RUART_MASK_MIN_FALL_SPACE         ((u32)0x00000FFF << 0)
#define RUART_MIN_FALL_SPACE(x)           ((u32)(((x) & 0x00000FFF) << 0))
#define RUART_GET_MIN_FALL_SPACE(x)       ((u32)(((x >> 0) & 0x00000FFF)))


/**************************************************************************//**
 *  DBGR
 *****************************************************************************/
#define RUART_MASK_DBG_UART               ((u32)0xFFFFFFFF << 0)
#define RUART_DBG_UART(x)                 ((u32)(((x) & 0xFFFFFFFF) << 0))
#define RUART_GET_DBG_UART(x)             ((u32)(((x >> 0) & 0xFFFFFFFF)))


/**************************************************************************//**
 *  RX_PATH_CTRL
 *****************************************************************************/
#define RUART_MASK_R_RXTO_THRS            ((u32)0x0000FFFF << 16)
#define RUART_R_RXTO_THRS(x)              ((u32)(((x) & 0x0000FFFF) << 16))
#define RUART_GET_R_RXTO_THRS(x)          ((u32)(((x >> 16) & 0x0000FFFF)))
#define RUART_MASK_RXBAUD_ADJ_10_0        ((u32)0x000007FF << 3)
#define RUART_RXBAUD_ADJ_10_0(x)          ((u32)(((x) & 0x000007FF) << 3))
#define RUART_GET_RXBAUD_ADJ_10_0(x)      ((u32)(((x >> 3) & 0x000007FF)))
#define RUART_BIT_R_RST_NEWRX_N           ((u32)0x00000001 << 2)


/**************************************************************************//**
 *  MON_BAUD_CTRL
 *****************************************************************************/
#define RUART_BIT_R_UPD_OSC_IN_XTAL       ((u32)0x00000001 << 29)
#define RUART_MASK_R_CYCNUM_PERBIT_OSC    ((u32)0x000FFFFF << 9)
#define RUART_R_CYCNUM_PERBIT_OSC(x)      ((u32)(((x) & 0x000FFFFF) << 9))
#define RUART_GET_R_CYCNUM_PERBIT_OSC(x)  ((u32)(((x >> 9) & 0x000FFFFF)))
#define RUART_MASK_R_BIT_NUM_THRES        ((u32)0x000000FF << 1)
#define RUART_R_BIT_NUM_THRES(x)          ((u32)(((x) & 0x000000FF) << 1))
#define RUART_GET_R_BIT_NUM_THRES(x)      ((u32)(((x >> 1) & 0x000000FF)))
#define RUART_BIT_R_MON_BAUD_EN           ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  MON_BAUD_STS
 *****************************************************************************/
#define RUART_MASK_RO_MON_TOTAL_BIT       ((u32)0x000000FF << 21)
#define RUART_RO_MON_TOTAL_BIT(x)         ((u32)(((x) & 0x000000FF) << 21))
#define RUART_GET_RO_MON_TOTAL_BIT(x)     ((u32)(((x >> 21) & 0x000000FF)))
#define RUART_BIT_RO_MON_RDY              ((u32)0x00000001 << 20)
#define RUART_MASK_R_CYCNUM_PERBIT_XTAL   ((u32)0x000FFFFF << 0)
#define RUART_R_CYCNUM_PERBIT_XTAL(x)     ((u32)(((x) & 0x000FFFFF) << 0))
#define RUART_GET_R_CYCNUM_PERBIT_XTAL(x) ((u32)(((x >> 0) & 0x000FFFFF)))


/**************************************************************************//**
 *  MON_CYC_NUM
 *****************************************************************************/
#define RUART_MASK_RO_MON_TOTAL_CYCLE     ((u32)0x0FFFFFFF << 0)
#define RUART_RO_MON_TOTAL_CYCLE(x)       ((u32)(((x) & 0x0FFFFFFF) << 0))
#define RUART_GET_RO_MON_TOTAL_CYCLE(x)   ((u32)(((x >> 0) & 0x0FFFFFFF)))


/**************************************************************************//**
 *  RX_BYTE_CNT
 *****************************************************************************/
#define RUART_BIT_CLR_RX_BYTE_CNT         ((u32)0x00000001 << 16)
#define RUART_MASK_RO_RX_BYTE_CNT         ((u32)0x0000FFFF << 0)
#define RUART_RO_RX_BYTE_CNT(x)           ((u32)(((x) & 0x0000FFFF) << 0))
#define RUART_GET_RO_RX_BYTE_CNT(x)       ((u32)(((x >> 0) & 0x0000FFFF)))


/**************************************************************************//**
 *  FCR
 *****************************************************************************/
#define RUART_MASK_RECVTRG                ((u32)0x00000003 << 6)
#define RUART_RECVTRG(x)                  ((u32)(((x) & 0x00000003) << 6))
#define RUART_GET_RECVTRG(x)              ((u32)(((x >> 6) & 0x00000003)))
#define RUART_BIT_DMA_MODE                ((u32)0x00000001 << 3)
#define RUART_BIT_XMIT_CLR                ((u32)0x00000001 << 2)
#define RUART_BIT_RECV_CLR                ((u32)0x00000001 << 1)
#define RUART_BIT_FIFO_EN                 ((u32)0x00000001 << 0)


#define UART_RX_FIFOTRIG_LEVEL_1BYTES        ((u32)0x00000000)
#define UART_RX_FIFOTRIG_LEVEL_16BYTES        ((u32)0x00000040)
#define UART_RX_FIFOTRIG_LEVEL_32BYTES        ((u32)0x00000080)
#define UART_RX_FIFOTRIG_LEVEL_62BYTES      ((u32)0x000000c0)



/**************************************************************************//**
 *  ICR
 *****************************************************************************/
#define RUART_BIT_RXNDICF                 ((u32)0x00000001 << 4)
#define RUART_BIT_MDICF                   ((u32)0x00000001 << 3)
#define RUART_BIT_MICF                    ((u32)0x00000001 << 2)
#define RUART_BIT_TOICF                   ((u32)0x00000001 << 1)
#define RUART_BIT_RLSICF                  ((u32)0x00000001 << 0)


/**************************************************************************//**
 *  RXDBCR
 *****************************************************************************/
#define RUART_MASK_DBNC_CYC               ((u32)0x00007FFF << 1)
#define RUART_DBNC_CYC(x)                 ((u32)(((x) & 0x00007FFF) << 1))
#define RUART_GET_DBNC_CYC(x)             ((u32)(((x >> 1) & 0x00007FFF)))
#define RUART_BIT_DBNC_FEN                ((u32)0x00000001 << 0)

#define XTAL_40M 40000000
#define OSC_2M 2000000

#define RTK_NR_UARTS	4
struct realtek_serial {
	struct uart_port port;
	struct clk *clk;
	struct clk *clk_sl;
	struct clk *parent_xtal_40m;
	struct clk *parent_osc_2m;
};

static struct realtek_serial serial_ports[RTK_NR_UARTS];
unsigned int baud_pm;

static inline unsigned int rtk_uart_readl(struct uart_port *port,
		unsigned int offset)
{
	return readl(port->membase + offset);
}

static inline void rtk_uart_writel(struct uart_port *port,
								   unsigned int value, unsigned int offset)
{
	writel(value, port->membase + offset);
}


/*
 * Serial core request to check if UART TX FIFO is empty
 */
static unsigned int rtk_uart_tx_empty(struct uart_port *port)
{
	unsigned int val;

	val = rtk_uart_readl(port, LSR);
	return (val & RUART_BIT_TX_EMPTY) ?  TIOCSER_TEMT : 0;
}

/*
 * Serial core request to set RTS and DTR pin state and loopback mode
 */
static void rtk_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	unsigned int val;

	val = rtk_uart_readl(port, MCR);
	val &= ~(RUART_BIT_DTR | RUART_BIT_RTS | RUART_BIT_AFE);

	if ((mctrl & TIOCM_RTS) && (port->status & UPSTAT_AUTORTS)) {
		val |= (RUART_BIT_AFE | RUART_BIT_RTS);
	}

	if (mctrl & TIOCM_LOOP) {
		val |= RUART_BIT_LOOP_EN;
	} else {
		val &= ~RUART_BIT_LOOP_EN;
	}

	rtk_uart_writel(port, val, MCR);

}

/*
 * Serial core request to return RI, CTS, DCD and DSR pin state
 */
static unsigned int rtk_uart_get_mctrl(struct uart_port *port)
{
	unsigned int val, mctrl;

	mctrl = 0;
	val = rtk_uart_readl(port, MSR);
	if (val & RUART_BIT_TERI) {
		mctrl |= TIOCM_RI;
	}
	if (val & RUART_BIT_D_CTS) {
		mctrl |= TIOCM_CTS;
	}
	if (val & RUART_BIT_D_DCD) {
		mctrl |= TIOCM_CD;
	}
	if (val & RUART_BIT_D_DSR) {
		mctrl |= TIOCM_DSR;
	}

	return mctrl;
}

/*
 * Serial core request to disable TX ASAP (used for flow control)
 */
static void rtk_uart_stop_tx(struct uart_port *port)
{
	unsigned int val;

	val = rtk_uart_readl(port, IER);
	val &= ~RUART_BIT_ETBEI;
	rtk_uart_writel(port, val, IER);

}

/*
 * Serial core request to (re)enable TX
 */
static void rtk_uart_start_tx(struct uart_port *port)
{
	unsigned int val;

	/* Enable TX FIFO empty interrupt*/
	val = rtk_uart_readl(port, IER);
	val |= RUART_BIT_ETBEI;
	rtk_uart_writel(port, val, IER);
}

/*
 * Serial core request to stop RX, called before port shutdown
 */
static void rtk_uart_stop_rx(struct uart_port *port)
{
	unsigned int val;

	val = rtk_uart_readl(port, IER);
	val &= ~(RUART_BIT_ELSI | RUART_BIT_ERBI);
	rtk_uart_writel(port, val, IER);
}

/*
 * Serial core request to enable modem status interrupt reporting
 */
static void rtk_uart_enable_ms(struct uart_port *port)
{
	unsigned int val;

	val = rtk_uart_readl(port, IER);
	val |= RUART_BIT_EDSSI;
	rtk_uart_writel(port, val, IER);
}

/*
 * Enable or disalbe ctl
 */
static void rtk_uart_break_ctl(struct uart_port *port, int ctl)
{
	unsigned long flags;
	unsigned int val;
	spin_lock_irqsave(&port->lock, flags);

	val = rtk_uart_readl(port, LCR);
	if (ctl) {
		val |= RUART_BIT_BRCTL;
	} else {
		val &= ~RUART_BIT_BRCTL;
	}
	rtk_uart_writel(port, val, LCR);

	spin_unlock_irqrestore(&port->lock, flags);
}

/*
 * Return port type in string format
 */
static const char *rtk_uart_type(struct uart_port *port)
{
	return (port->type == PORT_RTK) ? "rtk_uart" : NULL;
}

/*
 * Some interrupt need to clear
 */
static void rtk_uart_clear_flag(struct uart_port *port, u32 flag)
{
	u32 val;

	val = rtk_uart_readl(port, ICR);
	val |= flag;
	rtk_uart_writel(port, val, ICR);
}

/*
 * Read all chars in RX FIFO and send them to core
 * do not process RX error, should add later
 */
static void rtk_uart_do_rx(struct uart_port *port)
{
	u8 c = 0;
	u32 lsr = 0;
	u32 flag = TTY_NORMAL;
	int max_count = 32;

	do {
		lsr = rtk_uart_readl(port, LSR);

		if (unlikely(lsr & RUART_BIT_OVR_ERR)) {
			rtk_uart_clear_flag(port, RUART_BIT_RLSICF);

			port->icount.overrun++;
			tty_insert_flip_char(&port->state->port, 0, TTY_OVERRUN);
		}

		if (unlikely((lsr & RUART_BIT_BREAK_INT))) {
			port->icount.brk++;
			rtk_uart_clear_flag(port, RUART_BIT_RLSICF);
			if (uart_handle_break(port)) {
				continue;
			}
		}

		if (unlikely(lsr & RUART_BIT_PAR_ERR)) {
			port->icount.parity++;
			rtk_uart_clear_flag(port, RUART_BIT_RLSICF);
		}

		if (unlikely(lsr & RUART_BIT_FRM_ERR)) {
			port->icount.frame++;
			rtk_uart_clear_flag(port, RUART_BIT_RLSICF);
		}

		/* Update flag wrt read_status_mask */
		lsr &= port->read_status_mask;

		if (lsr & RUART_BIT_BREAK_INT) {
			flag = TTY_BREAK;
		}
		if (lsr & RUART_BIT_FRM_ERR) {
			flag = TTY_FRAME;
		}
		if (lsr & RUART_BIT_PAR_ERR) {
			flag = TTY_PARITY;
		}

		if (!(lsr & RUART_BIT_DRDY)) {
			break;
		}

		c = rtk_uart_readl(port, RBR_OR_UART_THR) & 0xff;
		port->icount.rx++;

		/*This is not console, so no need to handle sysrq.*/
		//if (uart_handle_sysrq_char(port, c))
		//	continue;

		if ((lsr & port->ignore_status_mask) == 0) {
			tty_insert_flip_char(&port->state->port, c, flag);
		}

	} while (--max_count);

	spin_unlock(&port->lock);
	tty_flip_buffer_push(&port->state->port);
	spin_lock(&port->lock);
}

/*
 * Fill TX FIFO with chars to send, stop when FIFO is about to be full
 * or when all chars have been sent.
 */
static void rtk_uart_do_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int val;

	if (uart_tx_stopped(port)) {
		rtk_uart_stop_tx(port);
		return;
	}

	if (port->x_char) {
		rtk_uart_writel(port, port->x_char, RBR_OR_UART_THR);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	while (rtk_uart_readl(port, LSR) & RUART_BIT_TX_NOT_FULL) {
		if (uart_circ_empty(xmit)) {
			break;
		}

		val = xmit->buf[xmit->tail];
		rtk_uart_writel(port, val, RBR_OR_UART_THR);
		xmit->tail = (xmit->tail + 1) & (SERIAL_XMIT_SIZE - 1);
		port->icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}

	if (uart_circ_empty(xmit)) {
		rtk_uart_stop_tx(port);
	}

}

/*
 * Process UART interrupt
 * only support RX full and TX empty interrupt
 * RX:FIFO drops below trigger level, do not need to clean the interrupt
 * TX: Writing to the TX FIFO of path, do not need to clean the interrupt
 */
static irqreturn_t rtk_uart_interrupt(int irq, void *dev_id)
{
	struct uart_port *port;
	unsigned int irqstat, ier, val;

	port = dev_id;
	spin_lock(&port->lock);

	irqstat = rtk_uart_readl(port, LSR);
	ier = rtk_uart_readl(port, IER);

	if (port == &serial_ports[3].port) {		//Amebasmart BT UART
		if ((irqstat & RUART_BIT_RXFIFO_INT) && (ier & RUART_BIT_ERBI)) {
			rtk_uart_do_rx(port);
		}
	} else {							//Other UART
		if ((irqstat & RUART_BIT_DRDY) && (ier & RUART_BIT_ERBI)) {
			rtk_uart_do_rx(port);
		}
	}

	if ((irqstat & RUART_BIT_TIMEOUT_INT) && (ier & RUART_BIT_ETOI)) {
		val = rtk_uart_readl(port, ICR);
		val |= RUART_BIT_TOICF;
		rtk_uart_writel(port, val, ICR);
		rtk_uart_do_rx(port);
	}

	if (((irqstat & RUART_BIT_TX_EMPTY) && (ier & RUART_BIT_ETBEI)) || \
		(ier & RUART_BIT_ELSI)) {
		rtk_uart_do_tx(port);
	}

	if ((irqstat & RUART_BIT_MODEM_INT) && (ier & RUART_BIT_EDSSI)) {
		/* Clear interrupt*/
		rtk_uart_clear_flag(port, RUART_BIT_MICF);

		/* Only care CTS because DCD, RI, DSR are not supported */
		val = !!(rtk_uart_readl(port, MSR) & RUART_BIT_R_CTS);
		uart_handle_cts_change(port, val);
	}

	spin_unlock(&port->lock);
	return IRQ_HANDLED;
}

/*
 * Enable RX & TX operation on UART
 */
static void rtk_uart_enable(struct uart_port *port)
{
	unsigned int val;

	val = rtk_uart_readl(port, FCR);
	val &= ~RUART_MASK_RECVTRG;
	if (port == &serial_ports[3].port) {		//Amebasmart BT UART
		val |= UART_RX_FIFOTRIG_LEVEL_16BYTES;
	} else {							//Other UART
		val |= UART_RX_FIFOTRIG_LEVEL_1BYTES;
	}
	rtk_uart_writel(port, val, FCR);

	val = rtk_uart_readl(port, IER);
	val |=  RUART_BIT_ELSI | RUART_BIT_ERBI | RUART_BIT_ETOI;
	rtk_uart_writel(port, val, IER);

	val = rtk_uart_readl(port, RX_PATH_CTRL);
	val &= ~RUART_MASK_R_RXTO_THRS;
	val |= RUART_R_RXTO_THRS(64);
	val |= RUART_BIT_R_RST_NEWRX_N;
	rtk_uart_writel(port, val, RX_PATH_CTRL);
}

/*
 * Disable RX & TX operation on UART
 */
static void rtk_uart_disable(struct uart_port *port)
{
	unsigned int val;
	//change later
	val = rtk_uart_readl(port, IER);
	val &= ~(RUART_BIT_ETBEI | RUART_BIT_ERBI);
	rtk_uart_writel(port, val, IER);


	val = rtk_uart_readl(port, RX_PATH_CTRL);
	val &= ~RUART_BIT_R_RST_NEWRX_N;
	rtk_uart_writel(port, val, RX_PATH_CTRL);
}


/*
 * Serial core request to initialize uart and start RX operation
 */
static int rtk_uart_startup(struct uart_port *port)
{
	int ret;

	/* Mask all IRQ and flush port */
	rtk_uart_disable(port);

	/* Register IRQ and enable RX interrupts */
	ret = request_irq(port->irq, rtk_uart_interrupt, 0,
					  dev_name(port->dev), port);
	if (ret) {
		return ret;
	}
	rtk_uart_enable(port);

	return 0;
}

/*
 * Serial core request to flush & disable UART
 */
static void rtk_uart_shutdown(struct uart_port *port)
{
	rtk_uart_disable(port);
	free_irq(port->irq, port);
}


/**
  * Get ovsr & ovsr_adj parameters according to the given baudrate and UART IP clock.
  */
static void rtk_uart_baudparagetfull(unsigned int clk, unsigned int baudrate,
									 unsigned int *ovsr, unsigned int *ovsr_adj)
{
	unsigned int i;
	unsigned int remainder;
	unsigned int temp_adj = 0;
	unsigned int temp_multly;

	/* Obtain the ovsr parameter */
	*ovsr = clk / baudrate;

	/* Get the remainder related to the ovsr_adj parameter */
	remainder = clk % baudrate;

	/* Calculate the ovsr_adj parameter */
	for (i = 0; i < 11; i++) {
		temp_adj = temp_adj << 1;
		temp_multly = (remainder * (12 - i));
		temp_adj |= ((temp_multly / baudrate - (temp_multly - remainder) / baudrate) ? 1 : 0);
	}

	/* Obtain the ovsr_adj parameter */
	*ovsr_adj = temp_adj;

}

/**
  * @brief    set uart baud rate of low power RX path
  * @param  UARTx: where x can be 0/1/2.
  * @param  BaudRate: the desired baud rate
  * @param  RxIPClockHz: the uart RX clock. unit: [Hz]
  * @note    according to the baud rate calculation formlula in low power RX path, method
  *              implemented is as follows:
  *          - cyc_perbit = round( fpclock/BaudRate)
  * @retval  None
  */
static void rtk_uart_lp_rx_setbaud(struct uart_port *port, u32 bandrate)
{
	u32 cyc_perbit = 0;
	u32 val = 0;

	/*Calculate the r_cycnum_perbit field of REG_MON_BAUD_STS,
	   according to clock and the desired baud rate*/
	if ((OSC_2M % bandrate) >= (bandrate + 1) / 2) {
		cyc_perbit = OSC_2M / bandrate + 1;
	} else {
		cyc_perbit = OSC_2M / bandrate;
	}

	/* Average clock cycle number of one bit. MON_BAUD_STS[19:0] */
	val = rtk_uart_readl(port, MON_BAUD_STS);
	val &= (~RUART_MASK_R_CYCNUM_PERBIT_XTAL);
	val |= (cyc_perbit & RUART_MASK_R_CYCNUM_PERBIT_XTAL);
	/* Set cyc_perbit */
	rtk_uart_writel(port, val, MON_BAUD_STS);


	/* Average clock cycle number of one bit OSC. REG_MON_BAUD_CTRL[28:9] */
	val = rtk_uart_readl(port, MON_BAUD_CTRL);
	val &= (~ RUART_MASK_R_CYCNUM_PERBIT_OSC);
	val |= ((cyc_perbit << 9) & RUART_MASK_R_CYCNUM_PERBIT_OSC);
	/* Set the OSC cyc_perbit*/
	rtk_uart_writel(port, val, MON_BAUD_CTRL);

	val = rtk_uart_readl(port, RX_PATH_CTRL);
	val &= (~ RUART_MASK_RXBAUD_ADJ_10_0);
	rtk_uart_writel(port, val, RX_PATH_CTRL);
}

/**
  * @brief    Set Uart Baud Rate use baudrate val.
  * @param  UARTx: where x can be 0/1/2.
  * @param  BaudRate: Baud Rate Val, like 115200 (unit is HZ).
  * @retval  None
  */
static void rtk_uart_setbaud(struct uart_port *port, unsigned int bandrate)
{
	unsigned int ovsr;
	unsigned int ovsr_adj;
	unsigned int val;

	/* Get baud rate parameter based on baudrate */
	rtk_uart_baudparagetfull(port->uartclk, bandrate, &ovsr, &ovsr_adj);

	/* Set DLAB bit to 1 to access DLL/DLM */
	val = rtk_uart_readl(port, LCR);
	val |= RUART_BIT_DLAB;
	rtk_uart_writel(port, val, LCR);

	/*Clean RX break signal interrupt status at initial stage*/
	val = rtk_uart_readl(port, SCR);
	val |= RUART_BIT_SCRATCH_7;
	rtk_uart_writel(port, val, SCR);

	/* Set ovsr(xfactor) */
	val = rtk_uart_readl(port, STS);
	val &= ~(RUART_MASK_XFACTOR);
	val |= ((ovsr << 4) & RUART_MASK_XFACTOR);
	rtk_uart_writel(port, val, STS);

	/* Set OVSR_ADJ[10:0] (xfactor_adj[26:16]) */
	val = rtk_uart_readl(port, SCR);
	val &= ~(RUART_MASK_XFACTOR_ADJ);
	val |= ((ovsr_adj << 16) & RUART_MASK_XFACTOR_ADJ);
	rtk_uart_writel(port, val, SCR);

	/* Clear DLAB bit */
	val = rtk_uart_readl(port, LCR);
	val &= ~(RUART_BIT_DLAB);
	rtk_uart_writel(port, val, LCR);


	/* RX baud rate configureation*/
	val = rtk_uart_readl(port, MON_BAUD_STS);
	val &= (~RUART_MASK_R_CYCNUM_PERBIT_XTAL);
	val |= ovsr;
	rtk_uart_writel(port, val, MON_BAUD_STS);

	val = rtk_uart_readl(port, MON_BAUD_CTRL);
	val &= (~RUART_MASK_R_CYCNUM_PERBIT_OSC);
	val |= (ovsr << 9);
	rtk_uart_writel(port, val, MON_BAUD_CTRL);

	val = rtk_uart_readl(port, RX_PATH_CTRL);
	val &= (~RUART_MASK_RXBAUD_ADJ_10_0);
	val |= (ovsr_adj << 3);
	rtk_uart_writel(port, val, RX_PATH_CTRL);

}

/*
 * Serial core request to change current UART setting
 */
static void rtk_uart_set_termios(struct uart_port *port,
								 struct ktermios *new,
								 struct ktermios *old)
{
	unsigned int ctl, baud, val;
	unsigned long flags;
	int tries;

	spin_lock_irqsave(&port->lock, flags);

	/* Drain the hot tub fully before we power it off for the winter. */
	for (tries = 3; !rtk_uart_tx_empty(port) && tries; tries--) {
		mdelay(10);
	}

	/* Disable UART while changing speed */
	rtk_uart_disable(port);
	//rtk_uart_flush(port);

	/* Update control register */
	ctl = rtk_uart_readl(port, LCR);

	switch (new->c_cflag & CSIZE) {
	/* Only support 7 and 8 bits*/
	case CS7:
		ctl &= ~RUART_BIT_WLS0;
		break;
	default:
		ctl |= RUART_BIT_WLS0;
		break;
	}

	if (new->c_cflag & CSTOPB) {
		ctl |= RUART_BIT_STB;
	} else {
		ctl &= ~RUART_BIT_STB;
	}

	if (new->c_cflag & PARENB) {
		ctl |= RUART_BIT_PEN;
		if (new->c_cflag & PARODD) {
			ctl &= ~RUART_BIT_EPS;
		} else {
			ctl |= RUART_BIT_EPS;
		}
	} else {
		ctl &= ~RUART_BIT_PEN;
	}
	rtk_uart_writel(port, ctl, LCR);

	/* Update baudword register. support max baudrate 40M/5 = 8MHz, min 110Hz */
	baud = uart_get_baud_rate(port, new, old, 110, port->uartclk / 5);
	baud_pm = baud;

	rtk_uart_setbaud(port, baud);

	/* Update interrupt register */
	val = rtk_uart_readl(port, IER);

	val &= ~RUART_BIT_EDSSI;
	if (UART_ENABLE_MS(port, new->c_cflag)) {
		val |= RUART_BIT_EDSSI;
	}

	rtk_uart_writel(port, val, IER);

	port->status &= ~(UPSTAT_AUTOCTS | UPSTAT_AUTORTS);
	if (new->c_cflag & CRTSCTS) {
		val = rtk_uart_readl(port, MCR);
		port->status |= UPSTAT_AUTOCTS | UPSTAT_AUTORTS;
		val |= (RUART_BIT_AFE | RUART_BIT_RTS);
		rtk_uart_writel(port, val, MCR);

	}

	/* Update read/ignore mask */
	port->read_status_mask = RUART_BIT_DRDY;
	if (new->c_iflag & INPCK) {
		port->read_status_mask |= RUART_BIT_FRM_ERR;
		port->read_status_mask |= RUART_BIT_PAR_ERR;
	}
	if (new->c_iflag & (IGNBRK | BRKINT)) {
		port->read_status_mask |= RUART_BIT_BREAK_INT;
	}

	port->ignore_status_mask = 0;
	if (new->c_iflag & IGNPAR) {
		port->ignore_status_mask |= RUART_BIT_PAR_ERR;
	}
	if (new->c_iflag & IGNBRK) {
		port->ignore_status_mask |= RUART_BIT_BREAK_INT;
	}

	/* Ignore all characters if CREAD is set.*/
	if (!(new->c_cflag & CREAD)) {
		port->ignore_status_mask |= RUART_BIT_DRDY;
	}

	uart_update_timeout(port, new->c_cflag, baud);
	rtk_uart_enable(port);
	spin_unlock_irqrestore(&port->lock, flags);

}

/*
 * Serial core request to claim UART iomem
 */
static int rtk_uart_request_port(struct uart_port *port)
{
	/* UARTs always present */
	return 0;
}

/*
 * Serial core request to release UART iomem
 */
static void rtk_uart_release_port(struct uart_port *port)
{
	/* Nothing to release ... */

}

/*
 * Serial core request to do any port required autoconfiguration
 */
static void rtk_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		if (rtk_uart_request_port(port)) {
			return;
		}

		port->type = PORT_RTK;
	}
}

/*
 * Serial core request to check that port information in serinfo are
 * suitable
 */
static int rtk_uart_verify_port(struct uart_port *port,
								struct serial_struct *serinfo)
{

	if (port->type != PORT_RTK) {
		return -EINVAL;
	}
	if (port->irq != serinfo->irq) {
		return -EINVAL;
	}
	if (port->iotype != serinfo->io_type) {
		return -EINVAL;
	}
	if (port->mapbase != (unsigned long)serinfo->iomem_base) {
		return -EINVAL;
	}

	return 0;
}


/* Serial core callbacks */
static const struct uart_ops rtk_uart_ops = {
	.tx_empty	= rtk_uart_tx_empty,
	.get_mctrl	= rtk_uart_get_mctrl,
	.set_mctrl	= rtk_uart_set_mctrl,
	.start_tx	= rtk_uart_start_tx,
	.stop_tx	= rtk_uart_stop_tx,
	.stop_rx	= rtk_uart_stop_rx,
	.enable_ms	= rtk_uart_enable_ms,
	.break_ctl	= rtk_uart_break_ctl,
	.startup	= rtk_uart_startup,
	.shutdown	= rtk_uart_shutdown,
	.set_termios	= rtk_uart_set_termios, //Change later
	.type		= rtk_uart_type,
	.release_port	= rtk_uart_release_port,
	.request_port	= rtk_uart_request_port,
	.config_port	= rtk_uart_config_port,
	.verify_port	= rtk_uart_verify_port,
};


static struct uart_driver rtk_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "rtk_uart",
	.dev_name	= "ttyRTK",
	.major		= 123,
	.minor		= 124,
	.nr		= RTK_NR_UARTS,
	.cons		= NULL,
};

/*
 * Platform driver probe/remove callback
 */
static int rtk_uart_probe(struct platform_device *pdev)
{
	struct resource *res_mem;
	struct uart_port *port;
	struct realtek_serial *serial;
	const struct clk_hw *hwclk;
	int ret, irq;

	if (pdev->dev.of_node) {
		pdev->id = of_alias_get_id(pdev->dev.of_node, "serial");
	} else {
		dev_err(&pdev->dev, "Invalid node\n");
		return -ENODEV;
	}

	if (pdev->id < 0 || pdev->id >= RTK_NR_UARTS) {
		dev_err(&pdev->dev, "Invalid device id\n");
		return -EINVAL;
	}

	serial = &serial_ports[pdev->id];
	port = &serial->port;

	if (!port) {
		dev_err(&pdev->dev, "Failed to get UART port\n");
		return -EBUSY;
	}

	memset(port, 0, sizeof(*port));

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(&pdev->dev, "Failed to get resource\n");
		return -ENODEV;
	}

	port->mapbase = res_mem->start;
	port->membase = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(port->membase)) {
		dev_err(&pdev->dev, "Failed to ioremap resource\n");
		return PTR_ERR(port->membase);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return irq;
	}

	serial->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(serial->clk)) {
		ret =  PTR_ERR(serial->clk);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(serial->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable clock: %d\n", ret);
		return ret;
	}

	port->iotype = UPIO_MEM;
	port->irq = irq;
	port->ops = &rtk_uart_ops;
	port->flags = UPF_BOOT_AUTOCONF;
	port->dev = &pdev->dev;
	port->fifosize = 16;
	port->line = pdev->id;
	port->type = PORT_RTK;
	port->uartclk = XTAL_40M;

	spin_lock_init(&port->lock);

	ret = uart_add_one_port(&rtk_uart_driver, port);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add port: %d\n", ret);
		port->membase = NULL;
		return ret;
	}

	platform_set_drvdata(pdev, serial);

	if (pdev->id < RTK_NR_UARTS - 1) { /*UARTx: where x can be 0/1/2*/
		if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
			device_init_wakeup(&pdev->dev, true);
			dev_pm_set_wake_irq(&pdev->dev, irq);
		}

		serial->clk_sl = clk_get_parent(serial->clk);
		hwclk = __clk_get_hw(serial->clk_sl);
		serial->parent_xtal_40m = clk_hw_get_parent_by_index(hwclk, 0)->clk;
		serial->parent_osc_2m = clk_hw_get_parent_by_index(hwclk, 1)->clk;
	}

	return 0;
}

#ifdef CONFIG_PM
static int realtek_uart_suspend(struct device *dev)
{
	struct realtek_serial *serial = dev_get_drvdata(dev);

	if (serial->port.line < RTK_NR_UARTS - 1) {
		if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
			clk_set_parent(serial->clk_sl, serial->parent_osc_2m);
			rtk_uart_lp_rx_setbaud(&serial->port, baud_pm);
		}
	}
	return 0;
}

static int realtek_uart_resume(struct device *dev)
{
	struct realtek_serial *serial = dev_get_drvdata(dev);

	if (serial->port.line < RTK_NR_UARTS - 1) {
		if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
			clk_set_parent(serial->clk_sl, serial->parent_xtal_40m);
			rtk_uart_setbaud(&serial->port, baud_pm);
		}
	}
	return 0;
}

static const struct dev_pm_ops realtek_ameba_uart_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(realtek_uart_suspend, realtek_uart_resume)
};
#endif

static const struct of_device_id rtk_uart_of_match[] = {
	{ .compatible = "realtek,ameba-uart" },
	{ /* end node */ }
};

static struct platform_driver rtk_uart_platform_driver = {
	.probe	= rtk_uart_probe,
	.driver	= {
		.name  = "realtek-ameba-uart",
		.of_match_table = rtk_uart_of_match,
#ifdef CONFIG_PM
		.pm = &realtek_ameba_uart_pm_ops,
#endif
	},
};

static int __init rtk_uart_init(void)
{
	int ret;

	ret = uart_register_driver(&rtk_uart_driver);
	if (ret) {
		return ret;
	}

	ret = platform_driver_register(&rtk_uart_platform_driver);
	if (ret) {
		uart_unregister_driver(&rtk_uart_driver);
	}

	return ret;
}

static void __exit rtk_uart_exit(void)
{
	platform_driver_unregister(&rtk_uart_platform_driver);
	uart_unregister_driver(&rtk_uart_driver);
}

module_init(rtk_uart_init);
module_exit(rtk_uart_exit);

MODULE_DESCRIPTION("Realtek Ameba UART driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
