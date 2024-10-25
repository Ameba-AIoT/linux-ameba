/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __WIFI_CONF_INIC_IPC_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/semaphore.h>

/* internal head files */
#include "wifi_conf.h"
#include "inic_net_device.h"
#include "inic_ipc_api.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_CONF_INFO "wifi conf"
#define RTW_JOIN_TIMEOUT (20000)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
typedef struct internal_join_block_param {
	struct completion join_done; /* completion to block join operations */
	unsigned int join_timeout; /* timeout for joining */
	unsigned char block; /* block parameters */
} internal_join_block_param_t;

/* -------------------------- Function declaration -------------------------- */

/* ---------------------------- Global Variables ---------------------------- */
rtw_joinstatus_callback_t p_wifi_join_status_internal_callback = NULL;
rtw_result_t (*scan_user_callback_ptr)(unsigned int, void *) = NULL;
rtw_result_t (*scan_each_report_user_callback_ptr)(rtw_scan_result_t *, void *) = NULL;

/* --------------------------- Private Variables ---------------------------- */
/* to store the join status of wifi */
static rtw_join_status_t rtw_join_status;
/* to store the join block parameters internally */
static internal_join_block_param_t *join_block_param = NULL;
static rtw_joinstatus_callback_t p_wifi_joinstatus_user_callback = NULL;
static inic_port_t *scan_iport = NULL;

/* --------------------------- Private Functions ---------------------------- */
/**
 * @brief  to change the join status of wifi.
 * @param  join_status[in]: join status.
 * @return NULL.
 */
static void _wifi_join_status_indicate(rtw_join_status_t join_status)
{
	/* step 1: internal process for wifi_connect*/
	if (join_status == RTW_JOINSTATUS_SUCCESS) {
		/* only sta to use this status.
		 * whatever concurrent mode and sta only, the index is 0.
		 */
		inic_net_indicate_connect(0);
		/* if Synchronized connect, up sema when connect success*/
		if (join_block_param && join_block_param->block) {
			complete(&join_block_param->join_done);
		}
	}

	if (join_status == RTW_JOINSTATUS_FAIL) {
		/* if blocking connection, up sema when connect fail*/
		if (join_block_param && join_block_param->block) {
			complete(&join_block_param->join_done);
		}
	}

	if (join_status == RTW_JOINSTATUS_DISCONNECT) {
		inic_net_indicate_disconnect(0);
	}

	rtw_join_status = join_status;

	/* step 2: execute user callback to process join_status*/
	if (p_wifi_joinstatus_user_callback) {
		p_wifi_joinstatus_user_callback(join_status);
	}

	return;
}

/* ---------------------------- Public Functions ---------------------------- */
/**
 * @brief  Join a Wi-Fi network.
 * 	Scan for, associate and authenticate with a Wi-Fi network.
 * @param[in]  iport: inic port for device, to get the device.
 * @param[in]  connect_param: the pointer of a struct which store the connection
 * 	info, including ssid, bssid, password, etc, for details, please refer to struct
 * 	rtw_network_info_t in wifi_structures.h
 * @param[in]  block: if block is set to 1, it means synchronized wifi connect, and this
* 	API will return until connect is finished; if block is set to 0, it means asynchronized
* 	wifi connect, and this API will return immediately.
 * @return  RTW_SUCCESS: when the system is joined for synchronized wifi connect, when connect
* 	cmd is set successfully for asynchronized wifi connect.
 * @return  RTW_ERROR: if an error occurred.
 * @note  Please make sure the Wi-Fi is enabled before invoking this function.
 * 	(@ref wifi_on())
 * @note  if bssid in connect_param is set, then bssid will be used for connect, otherwise ssid
 * 	is used for connect.
 */
int wifi_connect(inic_port_t *iport, rtw_network_info_t *connect_param,\
		 unsigned char block)
{
	rtw_result_t ret = RTW_SUCCESS;
	dma_addr_t dma_addr = 0, dma_addr_pswd = 0;
	internal_join_block_param_t *block_param = NULL;
	unsigned long residue_time = 0;
	struct device *pdev = NULL;
	u32 param_buf[1] = {0};

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	if (connect_param == NULL) {
		printk(KERN_ERR "%s, %s: wifi connect param not set!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	/* step1: check if there's ongoing connect*/
	if ((rtw_join_status > RTW_JOINSTATUS_UNKNOWN)\
	     && (rtw_join_status < RTW_JOINSTATUS_SUCCESS)) {
		printk(KERN_ERR "%s, %s: there is ongoing wifi connect!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_BUSY;
		goto func_exit;
	}

	p_wifi_joinstatus_user_callback = connect_param->joinstatus_user_callback;
	p_wifi_join_status_internal_callback = _wifi_join_status_indicate;

	/*clear for last connect status */
	rtw_join_status = RTW_JOINSTATUS_STARTING;
	_wifi_join_status_indicate(RTW_JOINSTATUS_STARTING);

	/* step2: malloc and set synchronous connection related variables*/
	if (block) {
		block_param = (internal_join_block_param_t *)kzalloc(sizeof(internal_join_block_param_t), GFP_KERNEL);
		if (!block_param) {
			ret = (rtw_result_t)RTW_NOMEM;
			rtw_join_status = RTW_JOINSTATUS_FAIL;
			goto error;
		}
		block_param->block = block;
		init_completion(&block_param->join_done);
	}

	/* step3: set connect cmd to driver*/
	if (connect_param->password_len) {
		dma_addr_pswd = dma_map_single(pdev, connect_param->password,\
					  connect_param->password_len, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr)) {
			printk(KERN_ERR "%s, %s: mapping passworderror!\n",\
			       INIC_CONF_INFO, __func__);
			ret = RTW_ERROR;
			goto error;
		}
		connect_param->password = (unsigned char *)dma_addr_pswd;
	}

	dma_addr = dma_map_single(pdev, connect_param, sizeof(rtw_network_info_t), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto error;
	}

	param_buf[0] = (u32 )dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_CONNECT, param_buf, 1);
	if (connect_param->password_len) {
		dma_unmap_single(pdev, dma_addr_pswd,\
				 connect_param->password_len, DMA_TO_DEVICE);
	}
	dma_unmap_single(pdev, dma_addr, sizeof(rtw_network_info_t), DMA_TO_DEVICE);

	if (ret != RTW_SUCCESS) {
		rtw_join_status = RTW_JOINSTATUS_FAIL;
		goto error;
	}

	/* step4: wait connect finished for synchronous connection*/
	if (block) {
		join_block_param = block_param;

#ifdef CONFIG_ENABLE_EAP
		// for eap connection, timeout should be longer (default value in wpa_supplicant: 60s)
		if (wifi_get_eap_phase()) {
			block_param->join_timeout = 60000;
		} else
#endif
			block_param->join_timeout = RTW_JOIN_TIMEOUT;

		residue_time = wait_for_completion_timeout(&block_param->join_done,\
							   msecs_to_jiffies(block_param->join_timeout));
		if (residue_time == 0) {
			printk(KERN_ERR "%s: Join bss timeout!\n", INIC_CONF_INFO);
			rtw_join_status = RTW_JOINSTATUS_FAIL;
			ret = RTW_TIMEOUT;
			goto error;
		} else {
			ret = wifi_is_connected_to_ap();
			if (ret != RTW_SUCCESS) {
				ret = RTW_ERROR;
				rtw_join_status = RTW_JOINSTATUS_FAIL;
				goto error;
			}
		}
	}

error:
	if (block_param) {
		complete_release(&block_param->join_done);
		kfree((u8 *)block_param);
		join_block_param = NULL;
	}

	if (rtw_join_status == RTW_JOINSTATUS_FAIL) {
		_wifi_join_status_indicate(RTW_JOINSTATUS_FAIL);
	}

func_exit:
	return ret;
}

/**
 * @brief  Check if Wi-Fi has connected to AP before dhcp.
 * @param  None
 * @return  RTW_SUCCESS: If conneced.
 * @return  RTW_ERROR: If not connect.
 */
int wifi_disconnect(void)
{
	int ret = 0;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_DISCONNECT, NULL, 0);
	return ret;
}

/**
 * @brief  Check if Wi-Fi has connected to AP before dhcp.
 * @param  None
 * @return  RTW_SUCCESS: If conneced.
 * @return  RTW_ERROR: If not connect.
 */
int wifi_is_connected_to_ap(void)
{
	int ret = 0;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_IS_CONNECTED_TO_AP, NULL, 0);
	return ret;
}

/**
 * @brief  Check if the specified wlan_idx is running.
 * @param[in]  index: to get wlan_idx(WLAN0_IDX or WLAN1_IDX).
 * @return  If the function succeeds, the return value is 1.
 * 	Otherwise, return 0.
 * @note  For STA mode, only use WLAN0_IDX
 * 	For AP mode, only use WLAN0_IDX
 * 	For CONCURRENT mode, use WLAN0_IDX for sta and WLAN1_IDX for ap
 */
int wifi_is_running(int index)
{
	int ret;
	u32 param_buf[1];
	param_buf[0] = index;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_IS_RUNNING, param_buf, 1);
	return ret;
}

/**
 * @brief  Set the listening channel for promiscuous mode.
 * 	Promiscuous mode will receive all the packets in
 * 	this channel.
 * @param[in]  channel: The desired channel.
 * @return  RTW_SUCCESS: If the channel is successfully set.
 * @return  RTW_ERROR: If the channel is not successfully set.
 * @note  DO NOT call this function for STA mode wifi driver,
 * 	since driver will determine the channel from its
 * 	received beacon.
 */
int wifi_set_channel(int channel)
{
	int ret = 0;
	u32 param_buf[1];
	param_buf[0] = channel;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_CHANNEL, param_buf, 1);
	return ret;
}

/**
 * @brief  Get the current channel on STA interface(WLAN0_NAME).
 * @param[in]  iport: inic port for device, to get the device.
 * @param[out]  channel: A pointer to the variable where the
 * 	channel value will be written.
 * @return  RTW_SUCCESS: If the channel is successfully read.
 * @return  RTW_ERROR: If the channel is not successfully read.
 */
int wifi_get_channel(inic_port_t *iport, int *channel)
{
	int ret = RTW_SUCCESS;
	u32 param_buf[1];
	int *channel_temp = NULL;
	dma_addr_t dma_addr = 0;
	struct device *pdev = NULL;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	channel_temp = kzalloc(sizeof(int), GFP_KERNEL);
	if (channel_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc channel_temp failed!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, channel_temp, sizeof(int), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto free_buf;
	}
	param_buf[0] = dma_addr;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_CHANNEL, param_buf, 1);

	dma_sync_single_for_cpu(pdev, dma_addr, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);
	*channel = *channel_temp;
	dma_unmap_single(pdev, dma_addr, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);

free_buf:
	kfree((u8 *)channel_temp);

func_exit:
	return ret;
}

int wifi_get_disconn_reason_code(inic_port_t *iport, unsigned short *reason_code)
{
	int ret = 0;
	u32 param_buf[1];
	dma_addr_t dma_addr = 0;
	struct device *pdev = NULL;
	unsigned short *reason_code_temp = NULL;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	reason_code_temp = (unsigned short *)kzalloc(sizeof(unsigned short), GFP_KERNEL);
	if (reason_code_temp == NULL) {
		return -1;
	}

	dma_addr = dma_map_single(pdev, reason_code_temp, sizeof(unsigned short), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}
	param_buf[0] = dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_DISCONN_REASCON, param_buf, 1);

	dma_sync_single_for_cpu(pdev, dma_addr, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);
	*reason_code = *reason_code_temp;
	dma_unmap_single(pdev, dma_addr, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);
	kfree((u8 *)reason_code_temp);

func_exit:
	return ret;
}

/**
 * @brief  get join status during wifi connectection
 * @param  None
 * @return join status, refer to macros in wifi_conf.c
 */
rtw_join_status_t wifi_get_join_status(void)
{
	return rtw_join_status;
}

int wifi_on(inic_port_t *iport, rtw_mode_t mode)
{
	int ret = 1;
	u32 param_buf[1];

	if (!iport || !iport->idev) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	param_buf[0] = mode;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_ON, param_buf, 1);

	if (ret == RTW_SUCCESS) { //wifi on success
		iport->idev->mode = mode;
		if (mode == RTW_MODE_AP) {
			inic_net_indicate_connect(0);
		} else	 if (mode == RTW_MODE_STA_AP) {
			inic_net_indicate_connect(1);
		}
	} else {
		printk(KERN_ERR "%s, %s: WIFI on failed!\n",\
		       INIC_CONF_INFO, __func__);
	}

func_exit:
	return ret;
}

int wifi_off(inic_port_t *iport)
{
	int ret = 0;

	if (!iport || !iport->idev) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if ((wifi_is_running(WLAN0_IDX) == 0) &&
		(wifi_is_running(WLAN1_IDX) == 0)) {
		printk(KERN_ERR "%s, %s: WIFI is not running!\n",\
		       INIC_CONF_INFO, __func__);
		goto func_exit;
	}

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_OFF, NULL, 0);

	if (ret == RTW_SUCCESS) {
		iport->idev->mode = RTW_MODE_NONE;
	} else {
		printk(KERN_ERR "%s, %s: WIFI off failed!\n",\
		       INIC_CONF_INFO, __func__);
	}

func_exit:
	return ret;
}

int wifi_set_mode(inic_port_t *iport, rtw_mode_t mode)
{
	int ret = 0;
	u32 param_buf[1];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	spin_lock(&iport->idev->lock);

	if ((wifi_is_running(WLAN0_IDX) == 0) &&
		(wifi_is_running(WLAN1_IDX) == 0)) {
		printk(KERN_ERR "%s, %s: WIFI is not running\n",\
		       INIC_CONF_INFO, __func__);
		spin_unlock(&iport->idev->lock);
		ret = -1;
		goto func_exit;
	}

	param_buf[0] = mode;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_MODE, param_buf, 1);

	if (ret == RTW_SUCCESS) {
		iport->idev->mode = mode;
	}

	spin_unlock(&iport->idev->lock);
func_exit:
	return ret;
}

int wifi_start_ap(inic_port_t *iport, rtw_softap_info_t *softAP_config)
{
	int ret = 0;
	u32 param_buf[1];
	dma_addr_t dma_addr_conf = 0, dma_addr_pswd = 0;
	struct device *pdev = NULL;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	if (softAP_config->password_len > 0) {
		dma_addr_pswd = dma_map_single(pdev, softAP_config->password, softAP_config->password_len, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_pswd)) {
			printk(KERN_ERR "%s, %s: mapping dma error!\n",\
			       INIC_CONF_INFO, __func__);
			ret = RTW_ERROR;
			goto func_exit;
		}
		softAP_config->password = (u8 *)dma_addr_pswd;
	}

	dma_addr_conf = dma_map_single(pdev, softAP_config, sizeof(rtw_softap_info_t), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_conf)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_INFO, __func__);
		if (softAP_config->password_len > 0) {
			dma_unmap_single(pdev, dma_addr_pswd, softAP_config->password_len, DMA_TO_DEVICE);
		}
		ret = RTW_ERROR;
		goto func_exit;
	}
	param_buf[0] = (u32)dma_addr_conf;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_START_AP, param_buf, 1);

	if (softAP_config->password_len > 0) {
		dma_unmap_single(pdev, dma_addr_pswd, softAP_config->password_len, DMA_TO_DEVICE);
	}
	dma_unmap_single(pdev, dma_addr_conf, sizeof(rtw_softap_info_t), DMA_TO_DEVICE);

func_exit:
	return ret;
}

int wifi_scan_networks(inic_port_t *iport, rtw_scan_param_t *scan_param,\
		       int ssid_length, unsigned char block)
{
	int ret = 0;
	u32 param_buf[3];
	struct device *pdev = NULL;
	dma_addr_t dma_addr_scan_param = 0, dma_addr_ssid = 0, dma_addr_ch_list = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	scan_iport = iport;

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	/* lock 2s to forbid suspend under scan todo*/
	scan_user_callback_ptr = scan_param->scan_user_callback;
	scan_each_report_user_callback_ptr = scan_param->scan_report_each_mode_user_callback;;

	if (scan_param->ssid && (ssid_length > 0)) {
		dma_addr_ssid = dma_map_single(pdev, scan_param->ssid, ssid_length, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_ssid)) {
			printk(KERN_ERR "%s, %s: mapping dma error!\n",\
			       INIC_CONF_INFO, __func__);
			ret = RTW_ERROR;
			goto func_exit;
		}
		scan_param->ssid = (char *)dma_addr_ssid;
	}
	if (scan_param->channel_list) {
		dma_addr_ch_list = dma_map_single(pdev, scan_param->channel_list, scan_param->channel_list_num, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_ch_list)) {
			printk(KERN_ERR "%s, %s: mapping dma error!\n",\
			       INIC_CONF_INFO, __func__);
			ret = RTW_ERROR;
			dma_unmap_single(pdev, dma_addr_ssid, ssid_length, DMA_TO_DEVICE);
			goto func_exit;
		}
		scan_param->channel_list = (char *)dma_addr_ch_list;
	}

	dma_addr_scan_param = dma_map_single(pdev, scan_param, sizeof(rtw_scan_param_t), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_scan_param)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}
	param_buf[0] = (u32)dma_addr_scan_param;
	param_buf[1] = block;
	param_buf[2] = ssid_length;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SCAN_NETWROKS, param_buf, 3);
	dma_unmap_single(pdev, dma_addr_scan_param, sizeof(rtw_scan_param_t), DMA_TO_DEVICE);

	if (scan_param->channel_list) {
		dma_unmap_single(pdev, dma_addr_ch_list, sizeof(rtw_network_info_t), DMA_TO_DEVICE);
	}

	if (scan_param->ssid && (ssid_length > 0)) {
		dma_unmap_single(pdev, dma_addr_ssid, ssid_length, DMA_TO_DEVICE);
	}
func_exit:
	return ret;
}

int wifi_get_scan_records(inic_port_t *iport, unsigned int *ap_num, char *scan_buf)
{
	int ret = 0;
	u32 param_buf[2];
	struct device *pdev = NULL;
	unsigned int *ap_num_temp = NULL;
	dma_addr_t dma_addr_ap_num = 0, dma_addr_scan_buf = 0;
	char *scan_buf_temp = NULL;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	ap_num_temp = (unsigned int *)kzalloc(sizeof(unsigned int), GFP_KERNEL);
	if (ap_num_temp == NULL) {
		return -1;
	}
	*ap_num_temp = *ap_num;

	scan_buf_temp = (char *)kzalloc((*ap_num) * sizeof(rtw_scan_result_t), GFP_KERNEL);
	if (scan_buf_temp == NULL) {
		kfree(ap_num_temp);
		return -1;
	}

	dma_addr_ap_num = dma_map_single(pdev, ap_num_temp, sizeof(unsigned int), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_ap_num)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		goto free_buf;
	}

	dma_addr_scan_buf = dma_map_single(pdev, scan_buf_temp, (*ap_num) * sizeof(rtw_scan_result_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_scan_buf)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_INFO, __func__);
		ret = RTW_ERROR;
		dma_unmap_single(pdev, dma_addr_ap_num, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);
		goto free_buf;
	}

	param_buf[0] = (u32)dma_addr_ap_num;
	param_buf[1] = (u32)dma_addr_scan_buf;

	dma_sync_single_for_cpu(pdev, dma_addr_ap_num, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);
	dma_sync_single_for_cpu(pdev, dma_addr_scan_buf, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_SCANNED_AP_INFO, param_buf, 2);

	*ap_num = *ap_num_temp;
	memcpy(scan_buf, scan_buf_temp, ((*ap_num)*sizeof(rtw_scan_result_t)));

	dma_unmap_single(pdev, dma_addr_ap_num, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);
	dma_unmap_single(pdev, dma_addr_scan_buf, sizeof(rtw_network_info_t), DMA_FROM_DEVICE);

free_buf:
	kfree((u8 *)ap_num_temp);
	kfree((u8 *)scan_buf_temp);

func_exit:
	return ret;
}

int wifi_scan_abort(void)
{
	int ret = 0;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SCAN_ABORT, NULL, 0);

	return ret;
}
