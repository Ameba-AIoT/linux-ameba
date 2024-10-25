/* SPDX-License-Identifier:  GPL-2.0-or-later */
/*
 * Derived from many drivers using ameba IPC inic device.
 *
 * Copyright (C) 2020 Realsil <andrew_su@realsil.com.cn>
 *
 * RTK wlan driver for Ameba IPC inic.
 *
 */
#include "inic_ipc_host_trx.h"
#include "inic_ipc_msg_queue.h"
#include "inic_cmd.h"
#include "inic_linux_base_type.h"
#include "inic_net_device.h"
#include "inic_ipc.h"
#include "inic_ipc_api.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_DGB_INFO "inic host"

/* -------------------------------- Macros ---------------------------------- */

/* -------------------------- Function declaration -------------------------- */
static unsigned int inic_ipc_host_event_int_hdl(aipc_ch_t *ch, \
						ipc_msg_struct_t *pmsg);

/* --------------------------- Private Variables ---------------------------- */
/**
 * entity of struct aipc_ch_ops. It will associate the channel_recv to 
 * inic_ipc_host_event_int_hdl.
 */
static struct aipc_ch_ops inic_ipc_ops = {
	.channel_recv = inic_ipc_host_event_int_hdl,
};

/* ---------------------------- Global Variables ---------------------------- */
/**
 * global pointer for ipc channel.
 */
aipc_ch_t *p_inic_ipc_ch = NULL;

/* ---------------------------- Private Functions --------------------------- */
/**
 * @brief  haddle the message of IPC.
 * @param  none.
 * @return none.
 */
static void inic_ipc_host_task_hdl(inic_ipc_ex_msg_t *p_ipc_msg)
{
	if (p_ipc_msg == NULL) {
		printk("Device IPC message is NULL, invalid!\n\r");
		return;
	}

	switch (p_ipc_msg->event_num) {
	/* receive the data from device */
	case IPC_WIFI_EVT_RECV_PKTS:
		inic_ipc_host_rx_handler(p_ipc_msg->msg_len,
					 (struct dev_sk_buff *)(p_ipc_msg->msg_addr));
		break;
	/* other contrl operations */
	case IPC_WIFI_MSG_ALLOC_SKB:
		inic_ipc_host_tx_alloc_skb_rsp(p_ipc_msg);
		break;
	case IPC_WIFI_MSG_XMIT_DONE:
		inic_ipc_host_tx_done();
		break;
	default:
		printk("Host Unknown event(%d)!\n\r", \
			  p_ipc_msg->event_num);
		break;
	}
}

/**
 * @brief  to haddle the event interrupt from ipc device.
 * @return if is OK, return 0, failed return negative number.
 */
static unsigned int inic_ipc_host_event_int_hdl(aipc_ch_t *ch, \
						ipc_msg_struct_t *pmsg)
{
	inic_ipc_ex_msg_t *p_ipc_msg = (inic_ipc_ex_msg_t *)phys_to_virt(pmsg->msg);
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	int  ret = 0;

	pdev = ch->pdev;
	if (!pdev) {
		ret = -ENODEV;
		printk(KERN_ERR "%s:%s ipc device is null (%d).\n", \
		       INIC_DGB_INFO, __func__, ret);
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, p_ipc_msg, sizeof(inic_ipc_ex_msg_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s %s: mapping dma error!.\n", \
		       INIC_DGB_INFO, __func__);
		goto func_exit;
	}
	dma_sync_single_for_cpu(pdev, dma_addr, sizeof(inic_ipc_ex_msg_t), DMA_FROM_DEVICE);

	if (inic_ipc_msg_get_queue_status()) {
		/* put the ipc message to the queue */
		ret = inic_ipc_msg_enqueue(p_ipc_msg);
	} else {
		/* the message queue does't work, call haddle function
		 * directly */
		inic_ipc_host_task_hdl(p_ipc_msg);
		ret = 0;
	}

	dma_sync_single_for_cpu(pdev, dma_addr, sizeof(inic_ipc_ex_msg_t), DMA_TO_DEVICE);

	/* enqueuing message is seccussful, send acknowledgement to another
	 * port. */
	p_ipc_msg->event_num = IPC_WIFI_MSG_READ_DONE;
	if (ret == 0)
		p_ipc_msg->msg_len = 0;
	else
		p_ipc_msg->msg_len = IPC_WIFI_MSG_MEMORY_NOT_ENOUGH;
	dma_sync_single_for_device(pdev, dma_addr, sizeof(inic_ipc_ex_msg_t), DMA_TO_DEVICE);
	dma_unmap_single(pdev, dma_addr, sizeof(inic_ipc_ex_msg_t), DMA_TO_DEVICE);

func_exit:
	return ret;
}

/**
 * @brief  to initilaze the inic driver and register its ipc channel.
 * 	It will be called when the driver is insmod or kernal init.
 * @return if is OK, return 0, failed return negative number.
 */
static int __init inic_host_init(void)
{
	int ret = 0;
	struct device *pdev = NULL;
	inic_dev_t *idev = NULL;
	aipc_ch_t *ipc_ch = NULL;

	/* allocate the ipc channel */
	ipc_ch = ameba_ipc_alloc_ch(sizeof(inic_dev_t*));
	if (!ipc_ch) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s: no memory for ipc channel(%d).\n", \
		       INIC_DGB_INFO, ret);
		goto func_exit;
	}
	/* initialize the ipc channel */
	ipc_ch->port_id = AIPC_PORT_NP;
	ipc_ch->ch_id = 0; /* configure channel 0 */
	ipc_ch->ch_config = AIPC_CONFIG_NOTHING;
	ipc_ch->ops = &inic_ipc_ops;
	p_inic_ipc_ch = ipc_ch;

	/* regist the ipc channel */
	ret = ameba_ipc_channel_register(ipc_ch);
	if (ret < 0) {
		printk(KERN_ERR "%s: regist ipc channel error(%d).\n", \
		       INIC_DGB_INFO, ret);
		goto free_ipc_ch;
	}

	pdev = ipc_ch->pdev;
	if (!pdev) {
		ret = -ENODEV;
		printk(KERN_ERR "%s:%s ipc device is null (%d).\n", \
		       INIC_DGB_INFO, __func__, ret);
		goto unregist_ipc_ch;
	}

	/* initialize the message queue, and assign the task haddle function */
	inic_ipc_msg_q_init(pdev, inic_ipc_host_task_hdl);

	/* initialize one port now, and be going to do for tow in future. */
	idev = inic_init_device(pdev, INIC_MAX_NET_PORT_NUM);
	if (!idev){
		ret = -ENOMEM;
		printk(KERN_ERR "%s: regist ipc channel error(%d).\n", \
		       INIC_DGB_INFO, ret);
		goto unregist_ipc_ch;
	}
	idev->trx_ch = ipc_ch;
	ipc_ch->priv_data = idev;

	ret = inic_ipc_host_api_init(idev);
	if (ret < 0) {
		printk(KERN_ERR "%s: init ipc host api error(%d).\n", \
		       INIC_DGB_INFO, ret);
		goto free_ipc_ch;
	}

	/* Initialize the parameters of ipc host. */
	inic_ipc_host_init_priv(idev);

	goto func_exit;

unregist_ipc_ch:
	ameba_ipc_channel_unregister(ipc_ch);

free_ipc_ch:
	kfree(ipc_ch);

func_exit:
	return ret;
}

/**
 * @brief  to deinitilaze the inic driver and unregister its
 * 	ipc channel. It will be called when the driver is remove or kernal
 * 	is free.
 */
static void __exit inic_host_exit(void)
{
	inic_dev_t *idev = p_inic_ipc_ch->priv_data;

	inic_ipc_host_api_deinit();
	if (idev)
		inic_deinit_device(idev);

	inic_ipc_msg_q_deinit();

	/* unregist the channel */
	ameba_ipc_channel_unregister(p_inic_ipc_ch);
}

/* ---------------------------- Public Functions ---------------------------- */
/**
 * @brief  to send ipc message with ipc hardware channel.
 * @return if is OK, return 0, failed return negative number.
 */
int inic_ipc_channel_send(ipc_msg_struct_t *pmsg)
{
	return ameba_ipc_channel_send(p_inic_ipc_ch, pmsg);
}

device_initcall(inic_host_init);
module_exit(inic_host_exit);

MODULE_AUTHOR("Andrew Su <andrew_su@realsil.com.cn>");
MODULE_DESCRIPTION("Ameba inic driver");
MODULE_LICENSE("GPL");
