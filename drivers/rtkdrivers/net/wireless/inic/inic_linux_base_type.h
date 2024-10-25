/*
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */
 /******************************************************************************
  * history *
 ******************************************************************************/

#ifndef __INIC_LINUX_BASE_TYPE__
#define __INIC_LINUX_BASE_TYPE__

/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/if_link.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <uapi/linux/wireless.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <ameba_ipc/ameba_ipc.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysrq.h>
#include <linux/of.h>
#include <linux/kern_levels.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <asm-generic/io.h>
#include <linux/io.h>
#include <linux/inetdevice.h>
#include <uapi/inic/inic_wireless.h>

/* internal head files */

/* -------------------------------- Defines --------------------------------- */
#define INIC_MAX_NET_PORT_NUM (2)
#define INIC_STA_PORT (0)
#define INIC_AP_PORT (1)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
typedef struct inic_port inic_port_t;
/*
 * Implement queue mechanism with list and spinlock.
 */
typedef struct __queue {
	struct list_head queue; /* queue list */
	spinlock_t lock;
} queue_t;

typedef enum {
	INIC_STATE_DOWN,
	INIC_STATE_UP,
	INIC_STATE_AUTH,
	INIC_STATE_ASSOC,
	INIC_STATE_DISCONNECTED,
	INIC_STATE_CONNECTED,
	INIC_STATE_AP_UP,
	INIC_STATE_MAX
} inic_state_t;

/*
 * structure to define inic device. It will be as the privete data of
 * 	net device.
 * @port_list: head of inic_port list, it will store the head to include all
 * 	init port.
 * @lock: lock for this inic device.
 * @trx_ch: ipc channel to send and receive data or message.
 */
typedef struct inic_device {
	struct list_head port_list;
	spinlock_t lock;
	struct net_device *pndev[INIC_MAX_NET_PORT_NUM];
	inic_port_t *piport[INIC_MAX_NET_PORT_NUM];
	int port_num;
	aipc_ch_t *trx_ch;
	aipc_ch_t *api_ch;
	rtw_mode_t mode;
} inic_dev_t;

/*
 * structure to define inic wlan port.
 * @list: list to add the port list in inic device.
 * @idev：the parent inic device, this port is mounted in this device.
 * @dev: the pointer to net_device, to associate the function of net_device.
 * @stats: the status of net work.
 * @wlan_idx: the index of wlan, to choose which port to operate.
 * @state: the state of inic wifi.
 */
struct inic_port {
	struct list_head list;
	inic_dev_t *idev;
	struct task_struct *evt_task;
	struct net_device *ndev;
	struct net_device_stats stats;
	int wlan_idx;
	inic_state_t state;
	/* netlink */
	struct sock *nl_sk;
};

/* -------------------------- Function declaration -------------------------- */

#endif /* __INIC_LINUX_BASE_TYPE__ */
