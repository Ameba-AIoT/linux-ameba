/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
#ifndef __INIC_IPC_HOST_TRX_H__
#define __INIC_IPC_HOST_TRX_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <asm/string.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

/* internal head files */
#include "inic_ipc.h"
#include "inic_linux_base_type.h"
#include "inic_net_device.h"
#include "inic_cmd.h"

/* -------------------------------- Defines --------------------------------- */

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
struct  dev_sk_buff_head {
	struct list_head	*next, *prev;
	unsigned int 		qlen;
};

#ifdef CONFIG_TRACE_SKB
#define	TRACE_SKB_DEPTH			8
#endif

struct dev_sk_buff {
	/* These two members must be first. */
	struct dev_sk_buff		*next;		/* Next buffer in list */
	struct dev_sk_buff		*prev;		/* Previous buffer in list */

	struct dev_sk_buff_head	*list;		/* List we are on */
	unsigned char		*head;		/* Head of buffer */
	unsigned char		*data;		/* Data head pointer */
	unsigned char		*tail;		/* Tail pointer	*/
	unsigned char		*end;		/* End pointer */
	void	*dev;		/* Device we arrived on/are leaving by */
	unsigned int 		len;		/* Length of actual data */
#ifdef CONFIG_TRACE_SKB
	unsigned int		liston[TRACE_SKB_DEPTH];	/* Trace the Lists we went through */
	const char		*funcname[TRACE_SKB_DEPTH];
	unsigned int		list_idx;	/* Trace the List we are on */
#endif

	int 			dyalloc_flag;

};

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */
void inic_ipc_host_init_priv(inic_dev_t *idev);
void inic_ipc_host_deinit_priv(void);
void inic_ipc_host_tx_done(void);
void inic_ipc_host_rx_handler(int idx_wlan, struct dev_sk_buff *skb);
void inic_ipc_host_tx_alloc_skb_rsp(inic_ipc_ex_msg_t *p_ipc_msg);
int inic_ipc_host_send(int idx, struct sk_buff *pskb);
#endif /* __INIC_IPC_HOST_TRX_H__ */
