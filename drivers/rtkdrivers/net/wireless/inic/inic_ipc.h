/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */
 /******************************************************************************
  * history *
  * 2020.11.20 | Andrew Su | Modification after first review.
  * 1, Add IPC_WIFI_DEV_XMIT_DONE in IPC_WIFI_CTRL_TYPE to notice the tx is
  * done in device port.
 ******************************************************************************/

#ifndef __INIC_IPC_H__
#define __INIC_IPC_H__

/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */

/* -------------------------------- Defines --------------------------------- */
#define MAX_NUM_WLAN_PORT (2)

#define FLAG_WLAN_IF_NOT_RUNNING ((u32)0xFFFFFFFF)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
enum {
	IPC_USER_POINT = 0,
	IPC_USER_DATA = 1
};

/*
 * define the ipc message.
 */
typedef struct inic_ipc_ex_msg {
	u32 event_num;
	u32 msg_addr;
	u32 msg_len;
	u32 rsvd[13]; /* keep total size 64B aligned */
} inic_ipc_ex_msg_t;

/*
 * the ipc message event id.
 */
enum IPC_WIFI_CTRL_TYPE {
	IPC_WIFI_MSG_READ_DONE = 0,
	IPC_WIFI_MSG_MEMORY_NOT_ENOUGH,
	IPC_WIFI_MSG_ALLOC_SKB,
	IPC_WIFI_MSG_RECV_DONE,
	IPC_WIFI_MSG_RECV_PENDING,
	IPC_WIFI_MSG_XMIT_DONE,
	IPC_WIFI_CMD_XIMT_PKTS,
	IPC_WIFI_EVT_RECV_PKTS
};

/* -------------------------- Function declaration -------------------------- */

#endif /* __INIC_IPC_H__ */
