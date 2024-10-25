// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __AMEBA_IPC_H__
#define __AMEBA_IPC_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/kernel.h>
#include <linux/platform_device.h>

/* internal head files */

/* -------------------------------- Defines --------------------------------- */
/* port number */
#define AIPC_PORT_NP (0) /* port for NP(KM4) */
#define AIPC_PORT_LP (1) /* port for LP(KM0) */

#define AIPC_CH_MAX_NUM (8)
#define AIPC_NOT_ASSIGNED_CH (AIPC_CH_MAX_NUM + 1)
#define AIPC_CH_IS_ASSIGNED (AIPC_NOT_ASSIGNED_CH + 1)
/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
struct aipc_ch;
/*
 * structure to describe the ipc message.
 * it's original defination in freertos is below:
 * typedef struct _IPC_MSG_STRUCT_ {
 * 	u32 msg_type;
 * 	u32 msg;
 * 	u32 msg_len;
 * 	u32 rsvd;
 *} IPC_MSG_STRUCT, *PIPC_MSG_STRUCT;
 * don't use typedef in linux advice.
 */
typedef struct ipc_msg_struct {
	u32 msg_type;
	u32 msg;
	u32 msg_len;
	u32 rsvd;
} ipc_msg_struct_t;

/*
 * This structure describes all the operations that can be done on the IPC
 * channel.
 * @channel_recv: The interrupt function for relactive registed channel. The
 *	pmsg is the ipc message from peer.
 * @channel_empty_indicate: The IPC empty interrupt function for relactive
 *	registed channel, If the channel is configured to AIPC_CONFIG_HANDSAKKE.
 *	It means that the interrupt ISR has been clean in other peer.
 */
struct aipc_ch_ops {
	u32 (*channel_recv)(struct aipc_ch *ch, struct ipc_msg_struct *pmsg);
	void (*channel_empty_indicate)(struct aipc_ch *ch);
};

/*
 * This enumeration describes the configuration for IPC channel.
 * @AIPC_CONFIG_NOTHING no configure for this channel, use default
 * 	configutation.
 * @AIPC_CONFIG_HANDSAKKE configure the handshake for send message. If to
 * 	configure this option, need to use ameba_ipc_channel_send_wait. otherwise
 * 	to use ameba_ipc_channel_send.
 */
typedef enum aipc_ch_config {
	AIPC_CONFIG_NOTHING = (unsigned int)0x00000000,
	AIPC_CONFIG_HANDSAKKE = (unsigned int)0x00000001
} aipc_ch_config_t;

/*
 * structure to describe device reigisting channel, and it will be associated
 *	to a struct aipc_c.
 */
typedef struct aipc_ch {
	struct device *pdev; /* to get the device for ipc device */
	u32 port_id; /* port id for NP or LP */
	u32 ch_id; /* channel number */
	enum aipc_ch_config ch_config; /* configuration for channel */
	struct aipc_ch_ops *ops; /* operations for channel */
	void *priv_data; /* generic platform data pointer */
} aipc_ch_t;

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */
/**
 * @brief  to tegister the ipc channel to the ipc port.
 * @param  pch[inout]: the pointer to ipc channel.
 * @return if is OK, return 0, failed return negative number.
 * @note: if to want to get dynamic channel id, set the channel id to
 * AIPC_NOT_ASSIGNED_CH.
 */
int ameba_ipc_channel_register(struct aipc_ch *ch);
/**
 * @brief  to untegister the ipc channel to the ipc port.
 * @param  pch[inout]: the pointer to ipc channel.
 * @return if is OK, return 0, failed return negative number.
 */
int ameba_ipc_channel_unregister(struct aipc_ch *ch);
/**
 * @brief  to send the message by the channel without handshake.
 * @param  pch[in]: the pointer to ipc channel.
 * @param  pmsg[in]: message to send.
 * @return if is OK, return 0, failed return negative number.
 */
int ameba_ipc_channel_send(struct aipc_ch *ch, \
			   struct ipc_msg_struct *pmsg);
/**
 * @brief  to accllocate the ipc channel.
 * @param  size_of_priv[in]: the length of private_data in aipc_ch_node.
 * @return return the allocated aipc_ch_node. If failed, return NULL.
 */
struct aipc_ch *ameba_ipc_alloc_ch(int size_of_priv);

#endif /* __AMEBA_IPC_H__ */
