/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SERIAL_AMEBA_H
#define _LINUX_SERIAL_AMEBA_H

#define AMEBA_LOGUART_DLL			0x00
#define AMEBA_LOGUART_DLH_INTCR		0x04
#define AMEBA_LOGUART_INTID			0x08						
#define AMEBA_LOGUART_LCR			0x0C
#define AMEBA_LOGUART_MCR			0x10
#define AMEBA_LOGUART_LSR			0x14
#define AMEBA_LOGUART_MDSR			0x18
#define AMEBA_LOGUART_SPR			0x1C
#define AMEBA_LOGUART_STSR			0x20
#define AMEBA_LOGUART_RBR			0x24
#define AMEBA_LOGUART_MISCR			0x28
#define AMEBA_LOGUART_TXPLSR		0x2C
#define AMEBA_LOGUART_RXPLSR		0x30
#define AMEBA_LOGUART_BAUDMONR		0x34
#define AMEBA_LOGUART_RSVD2			0x38
#define AMEBA_LOGUART_DBG_UART		0x3C
#define AMEBA_LOGUART_RX_PATH		0x40
#define AMEBA_LOGUART_MON_BAUD_CTRL	0x44
#define AMEBA_LOGUART_MON_BAUD_STS	0x48
#define AMEBA_LOGUART_MON_CYC_NUM	0x4C
#define AMEBA_LOGUART_RX_BYTE_CNT	0x50									
#define AMEBA_LOGUART_FCR			0x54
#define AMEBA_LOGUART_AGGC			0x58
#define AMEBA_LOGUART_THR1			0x5C
#define AMEBA_LOGUART_THR2			0x60
#define AMEBA_LOGUART_THR3			0x64
#define AMEBA_LOGUART_THR4			0x68
#define AMEBA_LOGUART_RP_LCR		0x6C
#define AMEBA_LOGUART_RP_CTRL		0x70										
#define AMEBA_LOGUART_ICR			0x74

#define LOGUART_IER_ERBI						((u32)0x00000001)	     /*BIT[0], Enable received data available interrupt (rx trigger)*/
#define LOGUART_IER_ELSI						((u32)0x00000001 << 2)	 /*BIT[2], Enable receiver line status interrupt (receiver line status)*/
#define LOGUART_IER_EDSSI						((u32)0x00000001 << 3)	 /*BIT[0], Enable received data available interrupt (rx trigger)*/
#define LOGUART_IER_ETOI						((u32)0x00000001 << 5)	 /*BIT[5], Enable rx time out interrupt*/
#define LOGUART_IER_ETPEFI						((u32)0x00000007 << 8)	 /*BIT[10:8], Enable tx path FIFO empty interrupt (tx path fifo empty)*/
#define LOGUART_TX_PATH4_INT					((u32)0x00000004 << 8)	 /*BIT[10]-BIT[8] b'100*/

#define LOGUART_LINE_RP_WLS						((u32)0x00000001)	     /*BIT[0], Word length selection*/
#define LOGUART_LINE_RP_STB						((u32)0x00000001 << 2)	 /*BIT[2], Stop bits length selection*/
#define LOGUART_LINE_RP_PEN						((u32)0x00000001 << 3)	 /*BIT[3], Parity enable selection*/
#define LOGUART_LINE_RP_EPS						((u32)0x00000001 << 4)	 /*BIT[4], Even parity selection*/
#define LOGUART_LINE_RP_STKP					((u32)0x00000001 << 5)	 /*BIT[5], Sticky parity bit*/
#define LOGUART_RP_LCR_BREAK_CTRL				((u32)0x00000001 << 6)   /*BIT[6], Break control function enable bit*/
#define LOGUART_RP_LINE_CTL_REG_DLAB_ENABLE		((u32)0x00000001 << 7)	 /*BIT[7], 0x80*/

#define LOGUART_MCR_FLOW_ENABLE					((u32)0x00000001 << 5)
#define LOGUART_MCR_LOOPBACK_ENABLE				((u32)0x00000001 << 4)
#define LOGUART_MCR_RTS							((u32)0x00000001 << 1) 
#define LOGUART_MCR_DTR							((u32)0x00000001 << 0)

#define LOGUART_LINE_STATUS_REG_DR				((u32)0x00000001)	     /*BIT[0], Data ready indicator*/
#define LOGUART_LINE_STATUS_ERR_OVERRUN			((u32)0x00000001 << 1)	 /*BIT[1], Over run*/
#define LOGUART_LINE_STATUS_ERR_PARITY			((u32)0x00000001 << 2)	 /*BIT[2], Parity error*/
#define LOGUART_LINE_STATUS_ERR_FRAMING			((u32)0x00000001 << 3)   /*BIT[3], Framing error*/
#define LOGUART_LINE_STATUS_ERR_BREAK			((u32)0x00000001 << 4)	 /*BIT[4], Break interrupt error*/
#define LOGUART_LINE_STATUS_ERR_RXFIFO			((u32)0x00000001 << 7)	 /*BIT[7], RX FIFO error*/
#define LOGUART_LINE_STATUS_ERR					(LOGUART_LINE_STATUS_ERR_OVERRUN |LOGUART_LINE_STATUS_ERR_PARITY| \
													LOGUART_LINE_STATUS_ERR_FRAMING|LOGUART_LINE_STATUS_ERR_BREAK| \
													LOGUART_LINE_STATUS_ERR_RXFIFO)	    /*Line status error*/
#define LOGUART_LINE_STATUS_REG_RP_DR			((u32)0x00000001 << 8)	 /*BIT[8], Relay path data ready indicator*/
#define LOGUART_LINE_STATUS_REG_RP_FIFONF		((u32)0x00000001 << 9)	 /*BIT[9], Relay path FIFO not full indicator*/
#define LOGUART_LINE_STATUS_ERR_RP_OVERRUN		((u32)0x00000001 << 10)	 /*BIT[10], Relay path over run*/
#define LOGUART_LINE_STATUS_ERR_RP_PARITY		((u32)0x00000001 << 11)	 /*BIT[11], Relay path parity error*/
#define LOGUART_LINE_STATUS_ERR_RP_FRAMING		((u32)0x00000001 << 12)  /*BIT[12], Relay path framing error*/
#define LOGUART_LINE_STATUS_RP_ERR

#define LOGUART_LINE_STATUS_REG_P4_THRE			(0x00000001 << 19)	 /*BIT[19], Path4 FIFO empty indicator*/
#define LOGUART_LINE_STATUS_REG_P4_FIFONF		((u32)0x00000001 << 23)	 /*BIT[23], Path4 FIFO not full indicator*/
#define LOGUART_LINE_STATUS_REG_RX_FIFO_PTR		((u32)0x000000FF << 24)	 /*BIT[31:24], Number of data bytes received in rx FIFO*/

#define LOGUART_MDSR_CTS						((u32)0x00000001 << 0)
#define LOGUART_MDSR_DSR						((u32)0x00000001 << 1)
#define LOGUART_MDSR_RI							((u32)0x00000001 << 2)
#define LOGUART_MDSR_DCD						((u32)0x00000001 << 3)

#define RUART_SP_REG_RXBREAK_INT_STATUS			((u32)0x00000001 << 7)	  /*BIT[7], 0x80, Write 1 clear*/
#define RUART_SP_REG_DBG_SEL					((u32)0x0000000F << 8)	  /*BIT[11:8], Debug port selection*/
#define RUART_SP_REG_XFACTOR_ADJ				((u32)0x000007FF << 16)	  /*BIT[26:16], ovsr_adj parameter field*/

#define RUART_STS_REG_XFACTOR					((u32)0x000FFFFF << 4)    /*BIT[23:4]ovsr parameter field*/

#define RUART_REG_LP_RX_PATH_SELECT				((u32)0x00000001)     	  /*BIT[0], 0x01, Select uart low power rx path*/
#define RUART_REG_LP_RX_PATH_RESET				((u32)0x00000001 << 2)    /*BIT[2], 0x40, Reset uart low power rx path receiver*/
#define RUART_REG_RX_XFACTOR_ADJ				((u32)0x000007FF << 3)	  /*BIT[13:3], One factor of Baud rate calculation for rx path, similar with xfactor_adj */
#define RUART_REG_RX_TO_THRES					((u32)0x0000FFFF << 16)   /*BIT[31:16], rx timeout threshold, unit is one bit period*/

#define RUART_LP_RX_MON_ENABLE					((u32)0x00000001)     	  /*BIT[0], 0x01, Enable low power rx monitor function*/
#define RUART_LP_RX_BIT_NUM_THRES				((u32)0x000000FF << 1)    /*BIT[8:1], Bit Number threshold of one monitor period*/
#define RUART_LP_RX_OSC_CYCNUM_PERBIT			((u32)0x000FFFFF << 9)    /*BIT[28:9], Cycle number perbit for osc clock */
#define RUART_LP_RX_OSC_UPD_IN_XTAL				((u32)0x00000001 << 29)   /*BIT[29], Control bit for osc monitor parameter update */

#define RUART_LP_RX_XTAL_CYCNUM_PERBIT			((u32)0x000FFFFF)     	  /*BIT[19:0], Cycle number perbit for xtal clock */
#define RUART_LP_RX_MON_RDY						((u32)0x00000001 << 20)   /*BIT[20], Monitor ready status*/
#define RUART_LP_RX_MON_TOTAL_BITS				((u32)0x0000000F << 21)   /*BIT[28:21], Actualy monitor bit number */

#define LOGUART_FIFO_CTL_RX_ERR_RPT				((u32)0x00000001)	      /*BIT[0], 0x01, RX error report control bit*/
#define LOGUART_FIFO_CTL_REG_CLEAR_RXFIFO		((u32)0x00000001 << 1)	  /*BIT[1], 0x02, Write 1 clear*/
#define LOGUART_FIFO_CTL_REG_CLEAR_TXFIFO		((u32)0x00000001 << 2)	  /*BIT[2], 0x04, Write 1 clear*/
#define LOGUART_FIFO_CTL_REG_DMA_ENABLE			((u32)0x00000001 << 3)	  /*BIT[3], 0x08, Uart DMA control bit*/
#define LOGUART_FIFO_CTL_REG_RX_TRG_LEV			((u32)0x00000003 << 6)	  /*BIT[7:6], 0xc0, Uart rx trigger level field*/

#define LOGUART_TP4_EN							((u32)0x00000001 << 30)   /*BIT[30], Tx path4 AGG enable */
#define LOGUART_RP_EN							((u32)0x00000001 << 31)   /*BIT[31], Relay path AGG enable */

#define LOGUART_ICR_RLSICF						((u32)0x00000001)	      /*BIT[0], Timeout interrupt clear flag*/
#define LOGUART_ICR_TOICF						((u32)0x00000001 << 1)	  /*BIT[1], Receiver line statue interrupt clear flag*/

#endif /* _LINUX_SERIAL_AMEBA_H */
