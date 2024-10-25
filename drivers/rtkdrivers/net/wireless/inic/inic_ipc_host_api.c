/* SPDX-License-Identifier:  GPL-2.0-or-later */
/*
 * Derived from many drivers using ameba IPC inic device.
 *
 * Copyright (C) 2020 Realsil <andrew_su@realsil.com.cn>
 *
 * RTK wlan driver for Ameba IPC inic.
 *
 */
#include "inic_linux_base_type.h"
#include "inic_ipc_api.h"
#include "inic_ipc.h"
#include "inic_net_device.h"
#include "wifi_ind.h"
#include "wifi_conf.h"
#include "wifi_promisc.h"

#define __INIC_IPC_HOST_API_C__
/* -------------------------------- Defines --------------------------------- */
#define INIC_API_INFO "inic host api"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* -------------------------- Function declaration -------------------------- */
static u32 inic_ipc_host_api_int_hdl(aipc_ch_t *ch, ipc_msg_struct_t *pmsg);

/* --------------------------- Private Variables ---------------------------- */

/* ---------------------------- Global Variables ---------------------------- */
extern ap_channel_switch_callback_t p_ap_channel_switch_callback;
extern p_wlan_autoreconnect_hdl_t p_wlan_autoreconnect_hdl;
extern ipc_promisc_callback_t ipc_promisc_callback;

/* ----------------------------- Local Variables ---------------------------- */
static host_api_priv_t *piihp_priv = NULL;
/**
 * entity of struct aipc_ch_ops. It will associate the channel_recv to
 * inic_ipc_host_event_int_hdl.
 */
static struct aipc_ch_ops inic_ipc_api_ops = {
	.channel_recv = inic_ipc_host_api_int_hdl,
};

/* ---------------------------- Private Functions --------------------------- */
/**
 * @brief  return the scanning result from inic device, it will call the
 * 	callback defined by user.
 * @param  p_ipc_msg[in]: the ipc message from inic device.
 */
static void iiha_scan_user_cb_hdl(host_api_priv_t *api,\
				  inic_ipc_dev_req_t *p_ipc_msg)
{
	unsigned int ap_num = p_ipc_msg->param_buf[0];
	void *user_data = (void *)p_ipc_msg->param_buf[1];

	if (scan_user_callback_ptr) {
		scan_user_callback_ptr(ap_num, user_data);
	}
}

/**
 * @brief  to report the scan result one by one when to scan each ap.
 * @param  p_ipc_msg[in]: the ipc message from inic device.
 */
static void iiha_scan_each_report_cb_hdl(host_api_priv_t *api,\
					 inic_ipc_dev_req_t *p_ipc_msg)
{
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	rtw_scan_result_t *scanned_ap_info = phys_to_virt(p_ipc_msg->param_buf[0]);
	void *user_data = (void *)p_ipc_msg->param_buf[1];

	if (!api || !api->papi_ipc_ch) {
		printk(KERN_ERR "%s,%s: host_api_priv is NULL in!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s,%s: device is NULL in scan!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, scanned_ap_info, sizeof(rtw_scan_result_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s,%s: mapping dma error!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	if (scan_each_report_user_callback_ptr) {
		scan_each_report_user_callback_ptr(scanned_ap_info, user_data);
	}

	dma_unmap_single(pdev, dma_addr, sizeof(rtw_scan_result_t), DMA_FROM_DEVICE);

func_exit:
	return;
}

static void iiha_autoreconnect_hdl(host_api_priv_t *api,\
				   inic_ipc_dev_req_t *p_ipc_msg)
{
	struct device *pdev = NULL;
	rtw_security_t security_type = (rtw_security_t)p_ipc_msg->param_buf[0];
	char *ssid = (char *)phys_to_virt(p_ipc_msg->param_buf[1]);
	int ssid_len = (int)p_ipc_msg->param_buf[2];
	char *password = (char *)phys_to_virt(p_ipc_msg->param_buf[3]);
	int password_len = (int)p_ipc_msg->param_buf[4];
	int key_id = (int)p_ipc_msg->param_buf[5];
	dma_addr_t dma_addr_ssid = 0, dma_addr_passwd = 0;
	wifi_autoreconn_param_t recon_param = {0};

	if (!api || !api->papi_ipc_ch) {
		printk(KERN_ERR "%s,%s: host_api_priv is NULL in!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s,%s: device is NULL in scan!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	dma_addr_ssid = dma_map_single(pdev, ssid, ssid_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_ssid)) {
		printk(KERN_ERR "%s,%s: mapping ssid dma error!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	dma_addr_passwd = dma_map_single(pdev, password, password_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_passwd)) {
		printk(KERN_ERR "%s,%s: mapping ssid dma error!\n",\
		      INIC_API_INFO, __func__);
		dma_unmap_single(pdev, dma_addr_ssid, ssid_len, DMA_FROM_DEVICE);
		goto func_exit;
	}

	recon_param.security_type = security_type;
	recon_param.ssid = ssid;
	recon_param.ssid_len = ssid_len;
	recon_param.password = password;
	recon_param.password_len = password_len;
	recon_param.key_id = key_id;

	if (p_wlan_autoreconnect_hdl) {
		p_wlan_autoreconnect_hdl(&recon_param);
	}

	dma_unmap_single(pdev, dma_addr_ssid, ssid_len, DMA_FROM_DEVICE);
	dma_unmap_single(pdev, dma_addr_passwd, password_len, DMA_FROM_DEVICE);

func_exit:
	return;
}

static void iiha_wifi_promisc_hdl(host_api_priv_t *api,\
				  inic_ipc_dev_req_t *p_ipc_msg)
{
	struct device *pdev = NULL;
	unsigned char *buf = (unsigned char *)phys_to_virt(p_ipc_msg->param_buf[0]);
	unsigned int buf_len = p_ipc_msg->param_buf[1];
	void *user_data = (void *)phys_to_virt(p_ipc_msg->param_buf[2]);
	dma_addr_t dma_addr_buf = 0, dma_addr_data = 0;

	if (!api || !api->papi_ipc_ch) {
		printk(KERN_ERR "%s,%s: host_api_priv is NULL in!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s,%s: device is NULL in scan!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	dma_addr_buf = dma_map_single(pdev, buf, buf_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_buf)) {
		printk(KERN_ERR "%s,%s: mapping buf dma error!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	dma_addr_data = dma_map_single(pdev, user_data, sizeof(ieee80211_frame_info_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_data)) {
		printk(KERN_ERR "%s,%s: mapping user_data dma error!\n",\
		      INIC_API_INFO, __func__);
		dma_unmap_single(pdev, dma_addr_buf, buf_len, DMA_FROM_DEVICE);
		goto func_exit;
	}

	if (ipc_promisc_callback) {
		ipc_promisc_callback(buf, buf_len, user_data);
	}
	dma_unmap_single(pdev, dma_addr_buf, buf_len, DMA_FROM_DEVICE);
	dma_unmap_single(pdev, dma_addr_data, sizeof(ieee80211_frame_info_t), DMA_FROM_DEVICE);

func_exit:
	return;
}

static void iiha_ap_ch_switch_hdl(host_api_priv_t *api,\
				  inic_ipc_dev_req_t *p_ipc_msg)
{
	unsigned char channel = (unsigned char)p_ipc_msg->param_buf[0];
	rtw_channel_switch_res_t res = (rtw_channel_switch_res_t)p_ipc_msg->param_buf[1];

	if (p_ap_channel_switch_callback) {
		p_ap_channel_switch_callback(channel, res);
	}
}

/**
 * @brief  to set the MAC address for wlan port by wlan index based on the ipc
 * 	message from inic device.
 * @param  p_ipc_msg[in]: the ipc message from inic device.
 */
static void iiha_set_netif_info_hdl(host_api_priv_t *api,\
				    inic_ipc_dev_req_t *p_ipc_msg)
{
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	int idx = (u32)p_ipc_msg->param_buf[0];
	unsigned char *dev_addr = phys_to_virt(p_ipc_msg->param_buf[1]);

	if (!api || !api->papi_ipc_ch) {
		printk(KERN_ERR "%s,%s: host_api_priv is NULL in!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s,%s: device is NULL in scan!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, dev_addr, ETH_ALEN, DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s,%s: mapping dma error!\n",\
		      INIC_API_INFO, __func__);
		goto func_exit;
	}
	/* call the interface function from inic_net_device */
	inic_net_set_mac_addr_by_idx(idx, dev_addr);
	dma_unmap_single(pdev, dma_addr, ETH_ALEN, DMA_FROM_DEVICE);

func_exit:
	return;
}

/**
 * @brief  to haddle the wifi event from inic device.
 * @param[in]  api: to get device.
 * @param  p_ipc_msg[in]: the ipc message from inic device.
 */
static void iiha_wifi_event_hdl(host_api_priv_t *api,\
				inic_ipc_dev_req_t *p_ipc_msg)
{
	rtw_event_indicate_t event = (rtw_event_indicate_t)p_ipc_msg->param_buf[0];
	char *buf = phys_to_virt(p_ipc_msg->param_buf[1]);
	int buf_len = (int)p_ipc_msg->param_buf[2];
	int flags = (int)p_ipc_msg->param_buf[3];

	wifi_indication(api, event, buf, buf_len, flags);

	return;
}

/**
 * @brief  to get the nerwork information (Ex. IP or GW) from device.
 * @param[in]  api: to get device.
 * @param  p_ipc_msg[in]: the ipc message from inic device.
 */
static void iiha_network_info_hdl(host_api_priv_t *api,\
				  inic_ipc_dev_req_t *p_ipc_msg)
{
	struct device *pdev = NULL;
	uint32_t type = (uint32_t)p_ipc_msg->param_buf[0];
	/* input is used for IPC_WLAN_IS_VALID_IP, not used now. */
	/* uint8_t *input = (uint8_t *)phys_to_virt(p_ipc_msg->param_buf[1]); */
	int idx = p_ipc_msg->param_buf[2];
	dma_addr_t dma_addr = 0;
	uint32_t *rsp_ptr = NULL;
	uint32_t rsp_len = 0;

	if (!api || !api->papi_ipc_ch) {
		printk(KERN_ERR "%s: host_api_priv is NULL!\n", INIC_API_INFO);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s: device is NULL!\n", INIC_API_INFO);
		goto func_exit;
	}

	switch (type) {
	case IPC_WLAN_GET_IP:
		rsp_ptr = inic_netif_get_ip_address(idx);
		rsp_len = 4;
		break;
	case IPC_WLAN_GET_GW:
		rsp_ptr = inic_netif_get_ip_gateway(idx);
		rsp_len = 4;
		break;
	case IPC_WLAN_GET_GWMSK:
		rsp_ptr = inic_netif_get_ip_mask(idx);
		rsp_len = 4;
		break;
	case IPC_WLAN_GET_HW_ADDR:
		rsp_ptr = (uint32_t *)inic_net_get_mac_addr_by_idx(idx);
		rsp_len = ETH_ALEN;
		break;
	case IPC_WLAN_IS_VALID_IP:
		/* todo in future */
		return;
	}

	dma_addr = dma_map_single(pdev, rsp_ptr, rsp_len, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s: dma mapping failed!\n", INIC_API_INFO);
		goto func_exit;
	}
	p_ipc_msg->ret = (u32)dma_addr;
	dma_sync_single_for_device(pdev, dma_addr, rsp_len, DMA_TO_DEVICE);
	dma_unmap_single(pdev, dma_addr, rsp_len, DMA_TO_DEVICE);

func_exit:
	return;
}

/**
 * @brief
 * @param  p_recv_msg[in]: the ipc message from inic device.
 */
static void inic_ipc_host_api_task(unsigned long data) {
	dma_addr_t dma_addr = 0;
	host_api_priv_t *api = (host_api_priv_t *)data;
	struct device *pdev = NULL;
	inic_ipc_dev_req_t *p_recv_msg = NULL;
	int msg_len = 0;

	if (!api || !api->papi_ipc_ch) {
		printk(KERN_ERR "%s: host_api_priv is NULL!\n", INIC_API_INFO);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s: device is NULL!\n", INIC_API_INFO);
		goto func_exit;
	}

	msg_len = api->api_ipc_msg.msg_len;
	if (!api->api_ipc_msg.msg || !msg_len) {
		printk(KERN_ERR "%s: Invalid device message!\n", INIC_API_INFO);
		goto func_exit;
	}
	p_recv_msg = phys_to_virt(api->api_ipc_msg.msg);
	dma_addr = dma_map_single(pdev, p_recv_msg, sizeof(inic_ipc_dev_req_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s: device is NULL!\n", INIC_API_INFO);
		goto func_exit;
	}

	switch (p_recv_msg->enevt_id) {
	/* receive callback indication */
	case IPC_WIFI_EVT_SCAN_USER_CALLBACK:
		iiha_scan_user_cb_hdl(api, p_recv_msg);
		break;
	case IPC_WIFI_EVT_SCAN_EACH_REPORT_USER_CALLBACK:
		iiha_scan_each_report_cb_hdl(api, p_recv_msg);
		break;
	case IPC_WIFI_EVT_AUTO_RECONNECT:
		iiha_autoreconnect_hdl(api, p_recv_msg);
		break;
	case IPC_WIFI_EVT_AP_CH_SWITCH:
		iiha_ap_ch_switch_hdl(api, p_recv_msg);
		break;
	case IPC_WIFI_EVT_HDL:
		iiha_wifi_event_hdl(api, p_recv_msg);
		break;
	case IPC_WIFI_EVT_PROMISC_CALLBACK:
		iiha_wifi_promisc_hdl(api, p_recv_msg);
		break;
	case IPC_WIFI_EVT_GET_LWIP_INFO:
		iiha_network_info_hdl(api, p_recv_msg);
		break;
	case IPC_WIFI_EVT_SET_NETIF_INFO:
		iiha_set_netif_info_hdl(api, p_recv_msg);
		break;
	default:
		printk(KERN_ERR "%s: Unknown Device event(%d)!\n\r", \
		       INIC_API_INFO, p_recv_msg->enevt_id);
		break;
	}

	/*set enevt_id to 0 to notify NP that event is finished*/
	p_recv_msg->enevt_id = 0;
	dma_sync_single_for_device(pdev, dma_addr, sizeof(inic_ipc_dev_req_t), DMA_TO_DEVICE);
	dma_unmap_single(pdev, dma_addr, sizeof(inic_ipc_dev_req_t), DMA_TO_DEVICE);

func_exit:
	return;
}

/**
 * @brief  to haddle the api interrupt from ipc device.
 * @return if is OK, return 0, failed return negative number.
 */
static u32 inic_ipc_host_api_int_hdl(aipc_ch_t *ch, \
				     ipc_msg_struct_t *pmsg)
{
	host_api_priv_t *api = piihp_priv;
	u32 ret = 0;

	if (!api) {
		printk(KERN_ERR "%s: host_api_priv is NULL in interrupt!\n",\
		       INIC_API_INFO);
		goto func_exit;
	}

	/* copy ipc_msg from temp memory in ipc interrupt. */
	memcpy((u8*)&(api->api_ipc_msg), (u8*)pmsg, sizeof(ipc_msg_struct_t));
	tasklet_schedule(&(api->api_tasklet));

func_exit:
	return ret;
}

/* ---------------------------- Public Functions ---------------------------- */
/**
 * @brief  to send api message to device.
 * @param  id[in]: h2c message id defined in IPC_WIFI_H2C_EVENT_TYPE.
 * @param  param_buf[in]: parameters' buffer for message.
 * @param  buf_len[in]: the length of parameters' buffer.
 * @return message return from device.
 */
int inic_ipc_host_api_send_msg(u32 id, u32 *param_buf, u32 buf_len)
{
	host_api_priv_t *api = piihp_priv;
	int ret = 0;

	if (!api) {
		printk(KERN_ERR "%s: host_api_priv is NULL when to send msg!\n", \
		       INIC_API_INFO);
		ret = -1;
		goto func_exit;
	}

	mutex_lock(&(api->iiha_send_mutex));

	memset((u8*)(api->preq_msg), 0, sizeof(inic_ipc_host_req_t));
	api->preq_msg->api_id = id;
	if (param_buf) {
		memcpy(api->preq_msg->param_buf, param_buf, buf_len * sizeof(u32));
	}

	memset((u8*)&(api->api_ipc_msg), 0, sizeof(ipc_msg_struct_t));
	api->api_ipc_msg.msg = (u32)api->req_msg_phy_addr;
	api->api_ipc_msg.msg_type = IPC_USER_POINT;
	api->api_ipc_msg.msg_len = sizeof(inic_ipc_host_req_t);

	ameba_ipc_channel_send(api->papi_ipc_ch, &(api->api_ipc_msg));

	while (api->preq_msg->api_id != IPC_WIFI_API_PROCESS_DONE) {
		udelay(10);
	}

	mutex_unlock(&(api->iiha_send_mutex));

	ret = api->preq_msg->ret;

func_exit:
	return ret;
}

/**
 * @brief  to initialize api priv.
 * @return if is OK, return 0, failed return negative number.
 */
int inic_ipc_host_api_init(inic_dev_t *idev)
{
	struct device *pdev = NULL;
	host_api_priv_t *api = NULL;
	int ret = 0;

	api = kmalloc(sizeof(host_api_priv_t), GFP_KERNEL);
	if (!api) {
		printk(KERN_ERR "%s: allloc host_api_priv error.\n", \
		       INIC_API_INFO);
		goto func_exit;
	}

	/* initialize the mutex to send api message. */
	mutex_init(&(api->iiha_send_mutex));

	/* allocate the ipc channel */
	api->papi_ipc_ch = ameba_ipc_alloc_ch(sizeof(host_api_priv_t *));
	if (!api->papi_ipc_ch) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s: no memory for ipc channel(%d).\n", \
		       INIC_API_INFO, ret);
		goto free_api;
	}

	/* initialize the ipc channel */
	api->papi_ipc_ch->port_id = AIPC_PORT_NP;
	api->papi_ipc_ch->ch_id = 1; /* configure channel 1 */
	api->papi_ipc_ch->ch_config = AIPC_CONFIG_NOTHING;
	api->papi_ipc_ch->ops = &inic_ipc_api_ops;
	api->papi_ipc_ch->priv_data = api;

	/* regist the api ipc channel */
	ret = ameba_ipc_channel_register(api->papi_ipc_ch);
	if (ret < 0) {
		printk(KERN_ERR "%s: regist api channel error(%d).\n", \
		       INIC_API_INFO, ret);
		goto free_ipc_ch;
	}

	if (idev) {
		idev->api_ch = api->papi_ipc_ch;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s: no device in registed IPC channel\n", \
		       INIC_API_INFO);
		goto free_ipc_ch;
	}

	api->preq_msg = dmam_alloc_coherent(pdev, sizeof(inic_ipc_host_req_t),\
					    &api->req_msg_phy_addr, GFP_KERNEL);
	if (!api->preq_msg) {
		printk(KERN_ERR "%s: allloc req_msg error.\n", \
		       INIC_API_INFO);
		goto unregist_ch;
	}

	/* initialize api tasklet */
	tasklet_init(&(api->api_tasklet), inic_ipc_host_api_task, (unsigned long)api);
	piihp_priv = api;

	goto func_exit;

unregist_ch:
	ameba_ipc_channel_unregister(api->papi_ipc_ch);

free_ipc_ch:
	kfree(api->papi_ipc_ch);

free_api:
	kfree(api);

func_exit:
	return ret;
}

/**
 * @brief  to deinitialize api priv.
 * @return NULL.
 */
void inic_ipc_host_api_deinit(void)
{
	host_api_priv_t *api = piihp_priv;
	struct device *pdev = NULL;

	if (!api) {
		printk(KERN_ERR "%s: host_api_priv is NULL, cannot deinit!\n", \
		       INIC_API_INFO);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	/* free sema to wakeup the message queue task */
	tasklet_kill(&(api->api_tasklet));

	dma_free_coherent(pdev, sizeof(inic_ipc_host_req_t), api->preq_msg, api->req_msg_phy_addr);

	/* unregist the channel */
	ameba_ipc_channel_unregister(api->papi_ipc_ch);

	/* deinitialize the mutex to send api message. */
	mutex_destroy(&(api->iiha_send_mutex));

	kfree(api->papi_ipc_ch);

	kfree(api);

func_exit:
	return;
}
