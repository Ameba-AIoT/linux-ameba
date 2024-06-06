// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek UART support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#if defined(CONFIG_SERIAL_RTK_AMEBA_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

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
#include <linux/serial_ameba.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/pm_wakeirq.h>
#include <linux/suspend.h>
#include <linux/clk-provider.h>

#define RTK_LOGUART_BAUDRATE 1500000
#define RTK_LOGUART_OSC_2M 2000000

struct log_uart {
	struct uart_port port;
	struct clk *clk;
	struct clk *clk_sl;
	struct clk *parent_xtal_40m;
	struct clk *parent_osc_2m;
} ;

extern void ameba_console_putchar(struct uart_port *port, int ch);
static struct log_uart loguart;

/*
 * handy uart register accessor
 */
static inline unsigned int ameba_uart_readl(struct uart_port *port,
		unsigned int offset)
{
	return readl(port->membase + offset);
}

static inline void ameba_uart_writel(struct uart_port *port,
									 unsigned int value, unsigned int offset)
{
	writel(value, port->membase + offset);
}

/*
 * Serial core request to check if UART TX FIFO is empty
 */
static unsigned int ameba_uart_tx_empty(struct uart_port *port)
{
	unsigned int val;

	val = ameba_uart_readl(port, AMEBA_LOGUART_LSR);
	return (val & LOGUART_LINE_STATUS_REG_P4_THRE) ?  TIOCSER_TEMT : 0;
}

/*
 * Serial core request to set RTS and DTR pin state and loopback mode
 * loguart do not support, no output pins
 */
static void ameba_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

/*
 * Serial core request to return RI, CTS, DCD and DSR pin state
 * loguart do not support, no output pins
 */
static unsigned int ameba_uart_get_mctrl(struct uart_port *port)
{
	unsigned int val, mctrl;

	mctrl = 0;
	val = ameba_uart_readl(port, AMEBA_LOGUART_MDSR);
	if (val & LOGUART_MDSR_RI) {
		mctrl |= TIOCM_RI;
	}
	if (val & LOGUART_MDSR_CTS) {
		mctrl |= TIOCM_CTS;
	}
	if (val & LOGUART_MDSR_DCD) {
		mctrl |= TIOCM_CD;
	}
	if (val & LOGUART_MDSR_DSR) {
		mctrl |= TIOCM_DSR;
	}
	return mctrl;
}

/*
 * Serial core request to disable TX ASAP (used for flow control)
 * AMEBA_LOGUART_AGGC bit(30): 0 disable TX Path4, 1 enable TX Path 4
 * AMEBA_LOGUART_DLH_INTCR bit(8-10): 000 disable TX path FIFO empty interrupt
 */
static void ameba_uart_stop_tx(struct uart_port *port)
{
	unsigned int val;
	val = ameba_uart_readl(port, AMEBA_LOGUART_DLH_INTCR);
	val &= ~LOGUART_IER_ETPEFI;
	ameba_uart_writel(port, val, AMEBA_LOGUART_DLH_INTCR);
}

/*
 * Serial core request to (re)enable TX
 * AMEBA_LOGUART_AGGC bit(30): 0 disable TX Path4, 1 enable TX Path 4
 * AMEBA_LOGUART_DLH_INTCR bit(10-8): 100 enable TX path4 FIFO empty interrupt
 */
static void ameba_uart_start_tx(struct uart_port *port)
{
	unsigned int val;
	val = ameba_uart_readl(port, AMEBA_LOGUART_DLH_INTCR);
	val &= ~LOGUART_IER_ETPEFI;
	val |= LOGUART_TX_PATH4_INT;
	ameba_uart_writel(port, val, AMEBA_LOGUART_DLH_INTCR);
}

/*
 * Serial core request to stop RX, called before port shutdown
 * AMEBA_LOGUART_DLH_INTCR bit(0) & bit(1): enable RX interrupt
 */
static void ameba_uart_stop_rx(struct uart_port *port)
{
}

/*
 * Serial core request to config interrupts
 */
static void ameba_uart_intr_config(struct uart_port *port, unsigned int intr, int state)
{
	unsigned int val = ameba_uart_readl(port, AMEBA_LOGUART_DLH_INTCR);
	if (state != 0) {
		val |= intr;
	} else {
		val &= ~intr;
	}
	ameba_uart_writel(port, val, AMEBA_LOGUART_DLH_INTCR);
}

/*
 * Serial core request to enable modem status interrupt reporting
 * loguart do not support flow control, there is no pin for flow control
 */
static void ameba_uart_enable_ms(struct uart_port *port)
{
}

/*
 * enable or disalbe ctl
 */
static void ameba_uart_break_ctl(struct uart_port *port, int ctl)
{
}

/*
 * return port type in string format
 */
static const char *ameba_uart_type(struct uart_port *port)
{
	return (port->type == PORT_AMEBA) ? "ameba_uart" : NULL;
}

/*
 * read all chars in RX FIFO and send them to core
 * do not process RX error, should add later
 */
static void ameba_uart_do_rx(struct uart_port *port)
{
	u8 buf[256];
	u32 i = 0, size = 0, cnt, mlen = 0;
	u32 timeout = 0;

	do {
		cnt = (ameba_uart_readl(port, AMEBA_LOGUART_LSR) & LOGUART_LINE_STATUS_REG_RX_FIFO_PTR) >> 24;
		if (cnt == 0) {
			if (timeout++ < 0x1000) {
				continue;
			} else {
				break;
			}
		}

		/* Total cnt should not exceed 256 because buf length is 256 */
		mlen += cnt;
		if (unlikely(mlen > 256)) {
			break;
		}

		for (i = 0; i < cnt; i++) {
			buf[size++] = ameba_uart_readl(port, AMEBA_LOGUART_RBR) & 0xff;
		}
	} while (1);

	port->icount.rx += size;
	tty_insert_flip_string(&port->state->port, buf, size);

	spin_unlock(&port->lock);
	tty_flip_buffer_push(&port->state->port);
	spin_lock(&port->lock);

}

/*
 * fill TX FIFO with chars to send, stop when FIFO is about to be full
 * or when all chars have been sent.
 */
static void ameba_uart_do_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int val;

	if (uart_tx_stopped(port)) {
		ameba_uart_stop_tx(port);
		return;
	}

	if (port->x_char) {
		ameba_uart_writel(port, port->x_char, AMEBA_LOGUART_THR4);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	while (ameba_uart_readl(port, AMEBA_LOGUART_LSR) & LOGUART_LINE_STATUS_REG_P4_FIFONF) {
		if (uart_circ_empty(xmit)) {
			break;
		}

		val = xmit->buf[xmit->tail];
		ameba_uart_writel(port, val, AMEBA_LOGUART_THR4);
		xmit->tail = (xmit->tail + 1) & (SERIAL_XMIT_SIZE - 1);
		port->icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}

	if (uart_circ_empty(xmit)) {
		ameba_uart_stop_tx(port);
	}
}

/*
 * Process UART interrupt
 * only support RX full and TX empty interrupt
 * RX: FIFO drops below trigger level, do not need to clean the interrupt
 * TX:  Writing to the TX FIFO of path, do not need to clean the interrupt
 */
static irqreturn_t ameba_uart_interrupt(int irq, void *dev_id)
{
	struct uart_port *port;
	unsigned int irqstat;

	port = dev_id;
	spin_lock(&port->lock);

	irqstat = ameba_uart_readl(port, AMEBA_LOGUART_LSR);
	if (irqstat & LOGUART_LINE_STATUS_REG_DR) {
		ameba_uart_do_rx(port);
	}

	if (irqstat & LOGUART_LINE_STATUS_REG_P4_THRE) {
		ameba_uart_do_tx(port);
	}

	spin_unlock(&port->lock);
	return IRQ_HANDLED;
}

/*
 * Enable RX & TX operation on UART
 */
static void ameba_uart_enable(struct uart_port *port)
{
}

/*
 * Disable RX & TX operation on UART
 */
static void ameba_uart_disable(struct uart_port *port)
{
}

#if 0
/*
 * clear all unread data in RX FIFO and unsent data in TX FIFO
 */
static void ameba_uart_flush(struct uart_port *port)
{
	unsigned int val;

	/* empty RX and TX FIFO */
	val = ameba_uart_readl(port, AMEBA_LOGUART_FCR);
	val |= (LOGUART_FIFO_CTL_REG_CLEAR_RXFIFO | LOGUART_FIFO_CTL_REG_CLEAR_TXFIFO);
	ameba_uart_writel(port, val, AMEBA_LOGUART_FCR);

	/* read any pending char to make sure all irq status are
	 * cleared */
	(void)ameba_uart_readl(port, AMEBA_LOGUART_RBR);
}
#endif

/*
 * Serial core request to initialize UART and start RX operation
 */
static int ameba_uart_startup(struct uart_port *port)
{
	int ret;

	/* Mask all IRQ and flush port */
	ameba_uart_disable(port);

	/* Register IRQ and enable RX interrupts */
	ret = request_irq(port->irq, ameba_uart_interrupt, 0,
					  dev_name(port->dev), port);
	if (ret) {
		return ret;
	}

	ameba_uart_intr_config(port, LOGUART_IER_ERBI, 1);

	ameba_uart_enable(port);

	return 0;
}

/*
 * Serial core request to flush & disable UART
 */
static void ameba_uart_shutdown(struct uart_port *port)
{
	ameba_uart_intr_config(port, LOGUART_IER_ERBI, 0);
}

static void ameba_uart_lp_setbaud(struct uart_port *port, u32 bandrate)
{
	u32 cyc_perbit = 0;
	u32 val = 0;

	/*Calculate the r_cycnum_perbit field of REG_MON_BAUD_STS,
	   according to clock and the desired baud rate*/
	if ((RTK_LOGUART_OSC_2M % bandrate) >= (bandrate + 1) / 2) {
		cyc_perbit = RTK_LOGUART_OSC_2M / bandrate + 1;
	} else {
		cyc_perbit = RTK_LOGUART_OSC_2M / bandrate;
	}

	/* Set DLAB bit to 1 to access DLL/DLM */
	val = ameba_uart_readl(port, AMEBA_LOGUART_LCR);
	val |= LOGUART_RP_LINE_CTL_REG_DLAB_ENABLE;
	ameba_uart_writel(port, val, AMEBA_LOGUART_LCR);

	/* Clean Rx break signal interrupt status at initial stage */
	val = ameba_uart_readl(port, AMEBA_LOGUART_SPR);
	val |= RUART_SP_REG_RXBREAK_INT_STATUS;
	ameba_uart_writel(port, val, AMEBA_LOGUART_SPR);

	/* Set OVSR(xfactor) */
	val = ameba_uart_readl(port, AMEBA_LOGUART_STSR);
	val &= ~(RUART_STS_REG_XFACTOR);
	val |= ((cyc_perbit << 4) & RUART_STS_REG_XFACTOR);
	ameba_uart_writel(port, val, AMEBA_LOGUART_STSR);

	/* Clr OVSR_ADJ[10:0] (xfactor_adj[26:16]) */
	val = ameba_uart_readl(port, AMEBA_LOGUART_SPR);
	val &= ~(RUART_SP_REG_XFACTOR_ADJ);
	ameba_uart_writel(port, val, AMEBA_LOGUART_SPR);

	/* Clear DLAB bit */
	val = ameba_uart_readl(port, AMEBA_LOGUART_LCR);
	val &= ~(LOGUART_RP_LINE_CTL_REG_DLAB_ENABLE);
	ameba_uart_writel(port, val, AMEBA_LOGUART_LCR);

	/* Average clock cycle number of one bit. MON_BAUD_STS[19:0] */
	val = ameba_uart_readl(port, AMEBA_LOGUART_MON_BAUD_STS);
	val &= (~RUART_LP_RX_XTAL_CYCNUM_PERBIT);
	val |= cyc_perbit & RUART_LP_RX_XTAL_CYCNUM_PERBIT;
	/* set cyc_perbit */
	ameba_uart_writel(port, val, AMEBA_LOGUART_MON_BAUD_STS);

	/* Average clock cycle number of one bit OSC. REG_MON_BAUD_CTRL[28:9] */
	val = ameba_uart_readl(port, AMEBA_LOGUART_MON_BAUD_CTRL);
	val &= (~RUART_LP_RX_OSC_CYCNUM_PERBIT);
	val |= ((cyc_perbit << 9) & RUART_LP_RX_OSC_CYCNUM_PERBIT);
	/* Set the OSC cyc_perbit*/
	ameba_uart_writel(port, val, AMEBA_LOGUART_MON_BAUD_CTRL);

	val = ameba_uart_readl(port, AMEBA_LOGUART_RX_PATH);
	val &= (~RUART_REG_RX_XFACTOR_ADJ);
	ameba_uart_writel(port, val, AMEBA_LOGUART_RX_PATH);

}
/**
  * @brief    get ovsr & ovsr_adj parameters according to the given baudrate and UART IP clock.
  * @param  UARTx: where x can be 0/1/3.
  * @param  IPclk: the given UART IP clock, Unit: [ Hz ]
  * @param  baudrate: the desired baudrate, Unit: bps[ bit per second ]
  * @param  ovsr: parameter related to the desired baud rate( corresponding to STSR[23:4] )
  * @param  ovsr_adj: parameter related to the desired baud rate( corresponding to SCR[26:16] )
  * @retval  None
  */
static void ameba_uart_baudparagetfull(unsigned int IPclk, unsigned int baudrate,
									   unsigned int *ovsr, unsigned int *ovsr_adj)
{
	unsigned int i;
	unsigned int Remainder;
	unsigned int TempAdj = 0;
	unsigned int TempMultly;

	/* Obtain the ovsr parameter */
	*ovsr = IPclk / baudrate;

	/* Get the remainder related to the ovsr_adj parameter */
	Remainder = IPclk % baudrate;

	/* Calculate the ovsr_adj parameter */
	for (i = 0; i < 11; i++) {
		TempAdj = TempAdj << 1;
		TempMultly = (Remainder * (12 - i));
		TempAdj |= ((TempMultly / baudrate - (TempMultly - Remainder) / baudrate) ? 1 : 0);
	}

	/* Obtain the ovsr_adj parameter */
	*ovsr_adj = TempAdj;
}

/**
  * @brief    Set UART baud rate use baudrate val.
  * @param  UARTx: where x can be 0/1/3.
  * @param  BaudRate: Baud Rate Val, like 115200 (unit is HZ).
  * @retval  None
  */
static void ameba_uart_setbaud(struct uart_port *port, unsigned int bandrate)
{
	unsigned int Ovsr;
	unsigned int Ovsr_adj;
	unsigned int val;

	/* Get baud rate parameter based on baudrate */
	ameba_uart_baudparagetfull(port->uartclk, bandrate, &Ovsr, &Ovsr_adj);

	/* Set DLAB bit to 1 to access DLL/DLM */
	val = ameba_uart_readl(port, AMEBA_LOGUART_LCR);
	val |= LOGUART_RP_LINE_CTL_REG_DLAB_ENABLE;
	ameba_uart_writel(port, val, AMEBA_LOGUART_LCR);

	/* Clean RX break signal interrupt status at initial stage */
	val = ameba_uart_readl(port, AMEBA_LOGUART_SPR);
	val |= RUART_SP_REG_RXBREAK_INT_STATUS;
	ameba_uart_writel(port, val, AMEBA_LOGUART_SPR);

	/* Set OVSR(xfactor) */
	val = ameba_uart_readl(port, AMEBA_LOGUART_STSR);
	val &= ~(RUART_STS_REG_XFACTOR);
	val |= ((Ovsr << 4) & RUART_STS_REG_XFACTOR);
	ameba_uart_writel(port, val, AMEBA_LOGUART_STSR);

	/* Set OVSR_ADJ[10:0] (xfactor_adj[26:16]) */
	val = ameba_uart_readl(port, AMEBA_LOGUART_SPR);
	val &= ~(RUART_SP_REG_XFACTOR_ADJ);
	val |= ((Ovsr_adj << 16) & RUART_SP_REG_XFACTOR_ADJ);
	ameba_uart_writel(port, val, AMEBA_LOGUART_SPR);

	/* Clear DLAB bit */
	val = ameba_uart_readl(port, AMEBA_LOGUART_LCR);
	val &= ~(LOGUART_RP_LINE_CTL_REG_DLAB_ENABLE);
	ameba_uart_writel(port, val, AMEBA_LOGUART_LCR);


	/* RX baud rate configureation */
	val = ameba_uart_readl(port, AMEBA_LOGUART_MON_BAUD_STS);
	val &= (~RUART_LP_RX_XTAL_CYCNUM_PERBIT);
	val |= Ovsr;
	ameba_uart_writel(port, val, AMEBA_LOGUART_MON_BAUD_STS);

	val = ameba_uart_readl(port, AMEBA_LOGUART_MON_BAUD_CTRL);
	val &= (~RUART_LP_RX_OSC_CYCNUM_PERBIT);
	val |= (Ovsr << 9);
	ameba_uart_writel(port, val, AMEBA_LOGUART_MON_BAUD_CTRL);

	val = ameba_uart_readl(port, AMEBA_LOGUART_RX_PATH);
	val &= (~RUART_REG_RX_XFACTOR_ADJ);
	val |= (Ovsr_adj << 3);
	ameba_uart_writel(port, val, AMEBA_LOGUART_RX_PATH);
}

/*
 * Serial core request to change current UART setting
 */
static void ameba_uart_set_termios(struct uart_port *port,
								   struct ktermios *new,
								   struct ktermios *old)
{
	unsigned int ctl, ier;
	unsigned long flags;
	int tries;

	spin_lock_irqsave(&port->lock, flags);

	/* Drain the hot tub fully before we power it off for the winter. */
	for (tries = 3; !ameba_uart_tx_empty(port) && tries; tries--) {
		mdelay(10);
	}


	/* Disable uart while changing speed */
	//ameba_uart_disable(port);

	/* Update Control register */
	ctl = ameba_uart_readl(port, AMEBA_LOGUART_LCR);

	switch (new->c_cflag & CSIZE) {
	/* Only support 7 and 8 bits*/
	case CS7:
		ctl &= ~LOGUART_LINE_RP_WLS;
		break;
	default:
		ctl |= LOGUART_LINE_RP_WLS;
		break;
	}

	if (new->c_cflag & CSTOPB) {
		ctl |= LOGUART_LINE_RP_STB;
	} else {
		ctl &= ~LOGUART_LINE_RP_STB;
	}

	if (new->c_cflag & PARENB) {
		ctl |= LOGUART_LINE_RP_PEN;
		if (new->c_cflag & PARODD) {
			ctl &= ~LOGUART_LINE_RP_EPS;
		} else {
			ctl |= LOGUART_LINE_RP_EPS;
		}
	} else {
		ctl &= ~LOGUART_LINE_RP_PEN;
	}
	ameba_uart_writel(port, ctl, AMEBA_LOGUART_LCR);

	/* Never change loguart baudrate */
	//baud = uart_get_baud_rate(port, new, old, 0, port->uartclk / 16);
	//ameba_uart_setbaud(port, baud);

	/* Update Interrupt register */
	ier = ameba_uart_readl(port, AMEBA_LOGUART_DLH_INTCR);

	ier &= ~LOGUART_IER_EDSSI;
	if (UART_ENABLE_MS(port, new->c_cflag)) {
		ier |= LOGUART_IER_EDSSI;
	}

	ameba_uart_writel(port, ier, AMEBA_LOGUART_DLH_INTCR);

#if 0
	/* Update read/ignore mask */
	port->read_status_mask = UART_FIFO_VALID_MASK;
	if (new->c_iflag & INPCK) {
		port->read_status_mask |= UART_FIFO_FRAMEERR_MASK;
		port->read_status_mask |= UART_FIFO_PARERR_MASK;
	}
	if (new->c_iflag & (IGNBRK | BRKINT)) {
		port->read_status_mask |= UART_FIFO_BRKDET_MASK;
	}

	port->ignore_status_mask = 0;
	if (new->c_iflag & IGNPAR) {
		port->ignore_status_mask |= UART_FIFO_PARERR_MASK;
	}
	if (new->c_iflag & IGNBRK) {
		port->ignore_status_mask |= UART_FIFO_BRKDET_MASK;
	}
	if (!(new->c_cflag & CREAD)) {
		port->ignore_status_mask |= UART_FIFO_VALID_MASK;
	}
#endif
	uart_update_timeout(port, new->c_cflag, RTK_LOGUART_BAUDRATE);
	ameba_uart_enable(port);
	spin_unlock_irqrestore(&port->lock, flags);
}

/*
 * Serial core request to claim UART iomem
 */
static int ameba_uart_request_port(struct uart_port *port)
{
	/* UARTs always present */
	return 0;
}

/*
 * Serial core request to release UART iomem
 */
static void ameba_uart_release_port(struct uart_port *port)
{
	/* Nothing to release ... */
}

/*
 * Serial core request to do any port required autoconfiguration
 */
static void ameba_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_AMEBA;
	}
}

/*
 * Serial core request to check that port information in serinfo are
 * suitable
 */
static int ameba_uart_verify_port(struct uart_port *port,
								  struct serial_struct *serinfo)
{
	if (port->type != PORT_AMEBA) {
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
static const struct uart_ops ameba_uart_ops = {
	.tx_empty	= ameba_uart_tx_empty,
	.get_mctrl	= ameba_uart_get_mctrl,
	.set_mctrl	= ameba_uart_set_mctrl,
	.start_tx	= ameba_uart_start_tx,
	.stop_tx	= ameba_uart_stop_tx,
	.stop_rx	= ameba_uart_stop_rx,
	.enable_ms	= ameba_uart_enable_ms,
	.break_ctl	= ameba_uart_break_ctl,
	.startup	= ameba_uart_startup,
	.shutdown	= ameba_uart_shutdown,
	.set_termios	= ameba_uart_set_termios, // Change later
	.type		= ameba_uart_type,
	.release_port	= ameba_uart_release_port,
	.request_port	= ameba_uart_request_port,
	.config_port	= ameba_uart_config_port,
	.verify_port	= ameba_uart_verify_port,
};



#ifdef CONFIG_SERIAL_RTK_AMEBA_CONSOLE
static void wait_for_xmitr(struct uart_port *port)
{
	int timeout = 10000;
	/* Wait up to 10ms for the character(s) to be sent. */
	while (--timeout) {
		unsigned int val;

		val = ameba_uart_readl(port, AMEBA_LOGUART_LSR);
		if (val & LOGUART_LINE_STATUS_REG_P4_FIFONF) {
			break;
		}
		udelay(1);
		cpu_relax();
	}
}

/*
 * output given char
 */
void ameba_console_putchar(struct uart_port *port, int ch)
{
	wait_for_xmitr(port);
	ameba_uart_writel(port, ch, AMEBA_LOGUART_THR4);
}

/*
 * console core request to output given string
 */
static void ameba_console_write(struct console *co, const char *s,
								unsigned int count)
{
	struct uart_port *port;
	//unsigned long flags;
	int locked;

	port = &loguart.port;

	//local_irq_save(flags);
	if (port->sysrq) {
		/* bcm_uart_interrupt() already took the lock */
		locked = 0;
	} else if (oops_in_progress) {
		locked = spin_trylock(&port->lock);
	} else {
		spin_lock(&port->lock);
		locked = 1;
	}

	/* Call helper to deal with \r\n */
	uart_console_write(port, s, count, ameba_console_putchar);

	/* And wait for char to be transmitted */
	wait_for_xmitr(port);

	if (locked) {
		spin_unlock(&port->lock);
	}
	//local_irq_restore(flags);
}

/*
 * console core request to setup given console, find matching uart
 * port and setup it.
 * baud should change later
 */
static int ameba_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = RTK_LOGUART_BAUDRATE;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	port = &loguart.port;
	if (!port->membase) {
		return -ENODEV;
	}

	if (options) {
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	}

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver ameba_uart_driver;

static struct console ameba_console = {
	.name		= "ttyS",
	.write		= ameba_console_write,
	.device		= uart_console_device,
	.setup		= ameba_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &ameba_uart_driver,
};

static void ameba_early_write(struct console *con, const char *s, unsigned n)
{
	struct earlycon_device *dev = con->data;

	uart_console_write(&dev->port, s, n, ameba_console_putchar);
	//wait_for_xmitr(&dev->port);
}

static int __init ameba_early_console_setup(struct earlycon_device *device,
		const char *opt)
{
	if (!device->port.membase) {
		return -ENODEV;
	}

	device->con->write = ameba_early_write;
	return 0;
}

OF_EARLYCON_DECLARE(ameba_uart, "realtek,ameba-loguart", ameba_early_console_setup);

#define AMEBA_CONSOLE	(&ameba_console)
#else
#define AMEBA_CONSOLE	NULL
#endif /* CONFIG_SERIAL_RTK_AMEBA_CONSOLE */

static struct uart_driver ameba_uart_driver = {
	.owner			= THIS_MODULE,
	.driver_name	= "ameba_uart",
	.dev_name		= "ttyS",
	.major			= TTY_MAJOR,
	.minor			= 64,
	.nr				= 1,
	.cons			= AMEBA_CONSOLE,
};

/*
 * platform driver probe/remove callback
 */
static int ameba_uart_probe(struct platform_device *pdev)
{
	struct resource *res_mem;
	struct uart_port *port = &loguart.port;
	int ret, irq;
	u32 clk_rate;
	struct clk_hw *hwclk;

	pdev->id = 0;
	memset(port, 0, sizeof(struct uart_port));

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		return -ENODEV;
	}

	port->mapbase = res_mem->start;
	port->membase = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(port->membase)) {
		return PTR_ERR(port->membase);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		return irq;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &clk_rate);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to get clock rate %d\n", ret);
	}

	port->iotype = UPIO_MEM;
	port->irq = irq;
	port->ops = &ameba_uart_ops;
	port->flags = UPF_BOOT_AUTOCONF;
	port->dev = &pdev->dev;
	port->fifosize = 16;
	port->line = pdev->id;
	port->type = PORT_AMEBA;
	port->uartclk = clk_rate;

	spin_lock_init(&port->lock);

	ret = uart_add_one_port(&ameba_uart_driver, port);
	if (ret) {
		port->membase = NULL;
		return ret;
	}

	platform_set_drvdata(pdev, &loguart);

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		device_init_wakeup(&pdev->dev, true);
		dev_pm_set_wake_irq(&pdev->dev, port->irq);
	}

	loguart.clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(loguart.clk)) {
		ret =  PTR_ERR(loguart.clk);
		dev_err(&pdev->dev, "Fail to get rcc clk: %d\n", ret);
		return ret;
	}

	loguart.clk_sl = clk_get_parent(loguart.clk);
	hwclk = __clk_get_hw(loguart.clk_sl);
	loguart.parent_xtal_40m = clk_hw_get_parent_by_index(hwclk, 0)->clk;
	loguart.parent_osc_2m = clk_hw_get_parent_by_index(hwclk, 1)->clk;

	return 0;
}

static int ameba_uart_remove(struct platform_device *pdev)
{
	struct log_uart *log_port;
	struct uart_port *port;

	log_port = platform_get_drvdata(pdev);
	port = &log_port->port;

	uart_remove_one_port(&ameba_uart_driver, port);
	/* Mark port as free */
	port->membase = NULL;
	return 0;
}

#ifdef CONFIG_PM
static int realtek_loguart_suspend(struct device *dev)
{
	struct log_uart *log_port = dev_get_drvdata(dev);

	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		clk_set_parent(log_port->clk_sl, log_port->parent_osc_2m);
		ameba_uart_lp_setbaud(&log_port->port, RTK_LOGUART_BAUDRATE);
	}
	return 0;
}

static int realtek_loguart_resume(struct device *dev)
{
	struct log_uart *log_port = dev_get_drvdata(dev);

	if (pm_suspend_target_state == PM_SUSPEND_CG || pm_suspend_target_state == PM_SUSPEND_PG) {
		clk_set_parent(log_port->clk_sl, log_port->parent_xtal_40m);
		ameba_uart_setbaud(&log_port->port, RTK_LOGUART_BAUDRATE);
	}
	return 0;
}

static const struct dev_pm_ops realtek_ameba_loguart_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(realtek_loguart_suspend, realtek_loguart_resume)
};
#endif

static const struct of_device_id ameba_of_match[] = {
	{ .compatible = "realtek,ameba-loguart" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ameba_of_match);

/*
 * platform driver stuff
 */
static struct platform_driver ameba_uart_platform_driver = {
	.probe	= ameba_uart_probe,
	.remove	= ameba_uart_remove,
	.driver	= {
		.name  = "realtek-ameba-loguart",
		.of_match_table = ameba_of_match,
#ifdef CONFIG_PM
		//.pm = &realtek_ameba_loguart_pm_ops,
#endif
	},
};

static int __init ameba_uart_init(void)
{
	int ret;

	ret = uart_register_driver(&ameba_uart_driver);
	if (ret) {
		return ret;
	}

	ret = platform_driver_register(&ameba_uart_platform_driver);
	if (ret) {
		uart_unregister_driver(&ameba_uart_driver);
	}

	return ret;
}

static void __exit ameba_uart_exit(void)
{
	platform_driver_unregister(&ameba_uart_platform_driver);
	uart_unregister_driver(&ameba_uart_driver);
}

module_init(ameba_uart_init);
module_exit(ameba_uart_exit);

MODULE_DESCRIPTION("Realtek Ameba UART driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
