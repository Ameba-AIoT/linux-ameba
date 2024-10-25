/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __WIFI_PROMISC_INIC_IPC_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/semaphore.h>

/* internal head files */
#include "wifi_promisc.h"
#include "inic_net_device.h"
#include "inic_ipc_api.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_PROMISC_INFO "wifi promisc"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* -------------------------- Function declaration -------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* --------------------------- Private Variables ---------------------------- */

/* --------------------------- Private Functions ---------------------------- */

/* ---------------------------- Public Functions ---------------------------- */
int promisc_filter_retransmit_pkt(u8 enable, u8 filter_interval_ms)
{
	int ret = 0;
	u32 param_buf[2];

	param_buf[0] = enable;
	param_buf[1] = filter_interval_ms;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_FILTER_RETRANSMIT_PKT, param_buf, 2);

	return ret;
}

int promisc_filter_with_len(u16 len)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = len;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_FILTER_WITH_LEN, param_buf, 1);

	return ret;
}

int promisc_set(rtw_rcr_level_t enabled, ipc_promisc_callback_t cb, unsigned char len_used)
{
	int ret = 0;
	u32 param_buf[3];

	param_buf[0] = enabled;
	if (cb != NULL) {
		param_buf[1] = 0xFFFFFFFF;
	} else {
		param_buf[1] = (u32)cb;
	}
	param_buf[2] = len_used;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_SET, param_buf, 3);

	return ret;
}

unsigned char is_promisc_enabled(void)
{
	int ret = 0;

	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_IS_ENABLED, NULL, 0);

	return ret;
}

int promisc_get_fixed_channel(inic_port_t *iport, void *fixed_bssid, u8 *ssid,\
			      int *ssid_length)
{
	int ret = 0;
	u32 param_buf[3];
	struct device *pdev = NULL;
	dma_addr_t dma_addr_ssid = 0, dma_addr_fssid = 0, dma_addr_len = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	if (fixed_bssid) {
		dma_addr_fssid = dma_map_single(pdev, fixed_bssid, ETH_ALEN, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_fssid)) {
			printk(KERN_ERR "%s, %s: mapping fixed ssid dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}
	}

	if (ssid && ssid_length) {
		dma_addr_len = dma_map_single(pdev, ssid_length, sizeof(int), DMA_FROM_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_len)) {
			printk(KERN_ERR "%s, %s: mapping ssid length dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto unmap_fixed_bssid;
		}

		dma_addr_ssid = dma_map_single(pdev, ssid, INIC_MAX_SSID_LENGTH, DMA_FROM_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_ssid)) {
			printk(KERN_ERR "%s, %s: mapping ssid dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			dma_unmap_single(pdev, dma_addr_len, sizeof(int), DMA_FROM_DEVICE);
			goto unmap_fixed_bssid;
		}
	}
	param_buf[0] = (u32)dma_addr_fssid;
	param_buf[1] = (u32)dma_addr_ssid;
	param_buf[2] = (u32)dma_addr_len;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_GET_FIXED_CHANNEL, param_buf, 3);

	if (ssid && ssid_length) {
		dma_unmap_single(pdev, dma_addr_len, sizeof(int), DMA_FROM_DEVICE);
		dma_unmap_single(pdev, dma_addr_ssid, INIC_MAX_SSID_LENGTH, DMA_FROM_DEVICE);
	}

unmap_fixed_bssid:
	if (fixed_bssid) {
		dma_unmap_single(pdev, dma_addr_fssid, ETH_ALEN, DMA_TO_DEVICE);
	}

func_exit:
	return ret;
}

int promisc_filter_by_ap_and_phone_mac(inic_port_t *iport, u8 enable,\
				       void *ap_mac, void *phone_mac)
{
	int ret = 0;
	u32 param_buf[3];
	struct device *pdev = NULL;
	dma_addr_t dma_addr_ap = 0, dma_addr_phone = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	if (ap_mac) {
		dma_addr_ap = dma_map_single(pdev, ap_mac, ETH_ALEN, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_ap)) {
			printk(KERN_ERR "%s, %s: mapping ap mac dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}
	}

	if (phone_mac) {
		dma_addr_phone = dma_map_single(pdev, phone_mac, ETH_ALEN, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr_phone)) {
			printk(KERN_ERR "%s, %s: mapping phone mac dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			if (ap_mac) {
				dma_unmap_single(pdev, dma_addr_ap, ETH_ALEN, DMA_TO_DEVICE);
			}
			goto func_exit;
		}
	}
	param_buf[0] = enable;
	param_buf[1] = (u32)dma_addr_ap;
	param_buf[2] = (u32)dma_addr_phone;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_FILTER_BY_AP_AND_PHONE_MAC, param_buf, 3);

	if (phone_mac) {
		dma_unmap_single(pdev, dma_addr_phone, ETH_ALEN, DMA_TO_DEVICE);
	}

	if (ap_mac) {
		dma_unmap_single(pdev, dma_addr_ap, ETH_ALEN, DMA_TO_DEVICE);
	}

func_exit:
	return ret;
}

int promisc_set_mgntframe(u8 enable)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = enable;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_SET_MGNTFRAME, param_buf, 1);

	return ret;
}

int promisc_get_chnl_by_bssid(inic_port_t *iport, u8 *bssid)
{
	int ret = 0;
	u32 param_buf[1];
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}


	if (bssid) {
		dma_addr = dma_map_single(pdev, bssid, ETH_ALEN, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr)) {
			printk(KERN_ERR "%s, %s: mapping bssid dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}
	}
	param_buf[0] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_GET_CHANNEL_BY_BSSID, param_buf, 1);

	if (bssid) {
		dma_unmap_single(pdev, dma_addr, ETH_ALEN, DMA_TO_DEVICE);
	}

func_exit:
	return ret;
}

int promisc_update_candi_ap_rssi_avg(s8 rssi, u8 cnt)
{
	int ret = 0;
	u32 param_buf[2];

	param_buf[0] = rssi;
	param_buf[1] = cnt;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_UPDATE_CANDI_AP_RSSI_AVG, param_buf, 2);

	return ret;
}

int promisc_stop_tx_beacn(void)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = 1; //for stop
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_TX_BEACON_CONTROL, param_buf, 1);

	return ret;
}

int promisc_resume_tx_beacn(void)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = 2; //for resume
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_TX_BEACON_CONTROL, param_buf, 1);

	return ret;
}

int promisc_issue_probersp(inic_port_t *iport, u8 *da)
{
	int ret = 0;
	u32 param_buf[1];
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}


	if (da) {
		dma_addr = dma_map_single(pdev, da, ETH_ALEN, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_addr)) {
			printk(KERN_ERR "%s, %s: mapping da dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}
	}
	param_buf[0] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_RESUME_TX_BEACON, param_buf, 1);

	if (da) {
		dma_unmap_single(pdev, dma_addr, ETH_ALEN, DMA_TO_DEVICE);
	}

func_exit:
	return ret;
}

int promisc_init_packet_filter(void)
{
	int ret = 0;

	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_INIT_PACKET_FILTER, NULL, 0);

	return ret;
}

int promisc_add_packet_filter(inic_port_t *iport, u8 filter_id,\
			       rtw_packet_filter_pattern_t *patt,\
			       rtw_packet_filter_rule_t rule)
{
	int ret = 0;
	u32 param_buf[3];
	struct device *pdev = NULL;
	dma_addr_t dma_patt = 0, dma_mask = 0, dma_pattern = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_PROMISC_INFO, __func__);
		ret = RTW_ERROR;
		goto func_exit;
	}

	if (patt) {
		dma_patt = dma_map_single(pdev, patt, sizeof(rtw_packet_filter_pattern_t), DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_patt)) {
			printk(KERN_ERR "%s, %s: mapping patt dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}
	}

	if (patt->mask) {
		dma_mask = dma_map_single(pdev, patt->mask, patt->mask_size, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_mask)) {
			printk(KERN_ERR "%s, %s: mapping patt mask dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto free_patt;
		}
		patt->mask = (unsigned char *)dma_mask;
	}

	if (patt->pattern) {
		dma_pattern = dma_map_single(pdev, patt->pattern, patt->mask_size, DMA_TO_DEVICE);
		if (dma_mapping_error(pdev, dma_pattern)) {
			printk(KERN_ERR "%s, %s: mapping patt pattern dma error!\n",\
			       INIC_PROMISC_INFO, __func__);
			ret = -ENOMEM;
			goto free_mask;
		}
		patt->pattern = (unsigned char *)dma_pattern;
	}

	param_buf[0] = filter_id;
	param_buf[1] = dma_patt;
	param_buf[2] = rule;
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_ADD_PACKET_FILTER, param_buf, 3);

	if (patt->pattern) {
		dma_unmap_single(pdev, dma_pattern, patt->mask_size, DMA_TO_DEVICE);
	}

free_mask:
	if (patt->mask) {
		dma_unmap_single(pdev, dma_mask, patt->mask_size, DMA_TO_DEVICE);
	}

free_patt:
	if (patt) {
		dma_unmap_single(pdev, dma_patt, sizeof(rtw_packet_filter_pattern_t), DMA_TO_DEVICE);
	}

func_exit:
	return ret;
}

int promisc_enable_packet_filter(u8 filter_id)
{
	int ret = 0;
	u32 param_buf[2];

	param_buf[0] = filter_id;
	param_buf[1] = 1; //for enable
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_PACKET_FILTER_CONTROL, param_buf, 2);

	return ret;
}

int promisc_disable_packet_filter(u8 filter_id)
{
	int ret = 0;
	u32 param_buf[2];

	param_buf[0] = filter_id;
	param_buf[1] = 2; //for disable
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_PACKET_FILTER_CONTROL, param_buf, 2);

	return ret;
}

int promisc_remove_packet_filter(u8 filter_id)
{
	int ret = 0;
	u32 param_buf[2];

	param_buf[0] = filter_id;
	param_buf[1] = 3; //for remove
	ret = inic_ipc_host_api_send_msg(IPC_API_PROMISC_PACKET_FILTER_CONTROL, param_buf, 2);

	return ret;
}