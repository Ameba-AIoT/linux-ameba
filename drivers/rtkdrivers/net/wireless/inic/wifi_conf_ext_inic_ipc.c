/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __WIFI_CONF_EXT_INIC_IPC_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/semaphore.h>

/* internal head files */
#include "wifi_conf.h"
#include "inic_ipc_api.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_CONF_EXT_INFO "wifi ext conf"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* -------------------------- Function declaration -------------------------- */

/* ---------------------------- Global Variables ---------------------------- */
ap_channel_switch_callback_t p_ap_channel_switch_callback = NULL;

/* --------------------------- Private Variables ---------------------------- */

/* --------------------------- Private Functions ---------------------------- */

/* ---------------------------- Public Functions ---------------------------- */
/* -------------------------------------------------------------------------- */
int wifi_psk_info_set(inic_port_t *iport, struct psk_info *psk_data)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[1];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, psk_data, sizeof(struct psk_info), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	param_buf[0] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_PSK_INFO_SET, param_buf, 1);
	dma_unmap_single(pdev, dma_addr, sizeof(struct psk_info), DMA_TO_DEVICE);

func_exit:
	return ret;
}

int wifi_psk_info_get(inic_port_t *iport, struct psk_info *psk_data)
{
	int ret = 0;
	struct device *pdev = NULL;
	struct psk_info *psk_info_temp = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[1];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	psk_info_temp = (struct psk_info *)kzalloc(sizeof(struct psk_info), GFP_KERNEL);
	if (psk_info_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc psk_info failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, psk_info_temp, sizeof(struct psk_info), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	param_buf[0] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_PSK_INFO_GET, param_buf, 1);
	dma_unmap_single(pdev, dma_addr, sizeof(struct psk_info), DMA_FROM_DEVICE);
	memcpy(psk_data, psk_info_temp, sizeof(struct psk_info));

free_buf:
	kfree(psk_info_temp);

func_exit:
	return ret;
}

int wifi_get_mac_address(inic_port_t *iport, rtw_mac_t *mac)
{
	int ret = 0;
	struct device *pdev = NULL;
	rtw_mac_t *mac_temp = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[1];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	mac_temp = (rtw_mac_t *)kzalloc(sizeof(rtw_mac_t), GFP_KERNEL);
	if (mac_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc mac_temp failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, mac_temp, sizeof(rtw_mac_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	param_buf[0] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_MAC_ADDR, param_buf, 1);
	dma_unmap_single(pdev, dma_addr, sizeof(rtw_mac_t), DMA_FROM_DEVICE);
	memcpy(mac, mac_temp, sizeof(rtw_mac_t));

free_buf:
	kfree(mac_temp);

func_exit:
	return ret;
}
/* -------------------------------------------------------------------------- */
int wifi_btcoex_set_ble_scan_duty(u8 duty)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = (u32)duty;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_COEX_BLE_SET_SCAN_DUTY, param_buf, 1);
	return ret;
}

u8 wifi_driver_is_mp(void)
{
	int ret = 0;

	ret = (u8)inic_ipc_host_api_send_msg(IPC_API_WIFI_DRIVE_IS_MP, NULL, 0);
	return ret;
}
/* -------------------------------------------------------------------------- */
int wifi_get_associated_client_list(inic_port_t *iport, void *client_list_buffer, unsigned short buffer_length)
{
	int ret = 0;
	struct device *pdev = NULL;
	void *client_list_buffer_temp = NULL;
	u32 param_buf[2];
	dma_addr_t dma_addr = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	client_list_buffer_temp = (rtw_mac_t *)kzalloc(buffer_length, GFP_KERNEL);
	if (client_list_buffer_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc client_list_buffer_temp failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}
	memcpy(client_list_buffer_temp, client_list_buffer, sizeof(int));

	dma_addr = dma_map_single(pdev, client_list_buffer_temp, buffer_length, DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	param_buf[0] = (u32)dma_addr;
	param_buf[1] = (u32)buffer_length;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_ASSOCIATED_CLIENT_LIST, param_buf, 2);
	dma_unmap_single(pdev, dma_addr, buffer_length, DMA_FROM_DEVICE);
	memcpy(client_list_buffer, client_list_buffer_temp, buffer_length);

free_buf:
	kfree(client_list_buffer_temp);

func_exit:
	return ret;
}
/* -------------------------------------------------------------------------- */

int wifi_get_setting(inic_port_t *iport, rtw_wifi_setting_t *psetting)
{
	int ret = 0;
	rtw_wifi_setting_t *setting_temp = NULL;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[2];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	setting_temp = (rtw_wifi_setting_t *)kzalloc(sizeof(rtw_wifi_setting_t), GFP_KERNEL);
	if (setting_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc rtw_wifi_setting_t failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, setting_temp, sizeof(rtw_wifi_setting_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	param_buf[0] = (u32)iport->wlan_idx;
	param_buf[1] = (u32)dma_addr;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_SETTING, param_buf, 2);
	dma_unmap_single(pdev, dma_addr, sizeof(rtw_wifi_setting_t), DMA_FROM_DEVICE);
	memcpy(psetting, setting_temp, sizeof(rtw_wifi_setting_t));

free_buf:
	kfree(setting_temp);

func_exit:
	return ret;
}

int wifi_set_powersave_mode(u8 ips_mode, u8 lps_mode)
{
	int ret = 0;
	u32 param_buf[2];

	param_buf[0] = (u32)ips_mode;
	param_buf[1] = (u32)lps_mode;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_POWERSAVE_MODE, param_buf, 1);
	return ret;
}
/* -------------------------------------------------------------------------- */
int wifi_set_mfp_support(unsigned char value)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = (u32)value;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_MFP_SUPPORT, param_buf, 1);
	return ret;
}

int wifi_set_group_id(unsigned char value)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = (u32)value;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_GROUP_ID, param_buf, 1);
	return ret;
}

int wifi_set_pmk_cache_enable(unsigned char value)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = (u32)value;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_PMK_CACHE_EN, param_buf, 1);
	return ret;
}
/* -------------------------------------------------------------------------- */
int wifi_get_sw_statistic(inic_port_t *iport, unsigned char idx, rtw_sw_statistics_t *statistic)
{
	int ret = 0;
	rtw_sw_statistics_t *statistic_temp = NULL;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[2];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	statistic_temp = (rtw_sw_statistics_t *)kzalloc(sizeof(rtw_sw_statistics_t), GFP_KERNEL);
	if (statistic_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc rtw_sw_statistics_t failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, statistic_temp, sizeof(rtw_sw_statistics_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	param_buf[0] = (u32)idx;
	param_buf[1] = (u32)dma_addr;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_SW_STATISTIC, param_buf, 2);

	dma_unmap_single(pdev, dma_addr, sizeof(rtw_sw_statistics_t), DMA_FROM_DEVICE);
	memcpy(statistic, statistic_temp, sizeof(rtw_sw_statistics_t));

free_buf:
	kfree(statistic_temp);

func_exit:
	return ret;
}

int wifi_fetch_phy_statistic(inic_port_t *iport, rtw_phy_statistics_t *phy_statistic)
{
	int ret = 0;
	rtw_phy_statistics_t *phy_statistic_temp = NULL;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[1];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	phy_statistic_temp = (rtw_phy_statistics_t *)kzalloc(sizeof(rtw_phy_statistics_t), GFP_KERNEL);
	if (phy_statistic_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc rtw_phy_statistics_t failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, phy_statistic_temp, sizeof(rtw_phy_statistics_t), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	param_buf[0] = (u32)dma_addr;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_PHY_STATISTIC, param_buf, 1);

	dma_unmap_single(pdev, dma_addr, sizeof(rtw_phy_statistics_t), DMA_FROM_DEVICE);
	memcpy(phy_statistic, phy_statistic_temp, sizeof(rtw_phy_statistics_t));

free_buf:
	kfree(phy_statistic_temp);

func_exit:
	return ret;
}


int wifi_set_network_mode(rtw_network_mode_t mode)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = (u32)mode;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_NETWORK_MODE, param_buf, 1);
	return ret;
}

int wifi_set_wps_phase(unsigned char is_trigger_wps)
{
	int ret = 0;
	u32 param_buf[1];
	param_buf[0] = is_trigger_wps;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_WPS_PHASE, param_buf, 1);
	return ret;
}

int wifi_set_gen_ie(inic_port_t *iport, unsigned char wlan_idx, char *buf, __u16 buf_len, __u16 flags)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[4];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, buf, buf_len, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	param_buf[0] = (u32)wlan_idx;
	param_buf[1] = (u32)dma_addr;
	param_buf[2] = (u32)buf_len;
	param_buf[3] = (u32)flags;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_GEN_IE, param_buf, 4);
	dma_unmap_single(pdev, dma_addr, buf_len, DMA_TO_DEVICE);

func_exit:
	return ret;
}

int wifi_set_eap_phase(unsigned char is_trigger_eap)
{
	int ret = 0;
	u32 param_buf[1];
	param_buf[0] = is_trigger_eap;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_EAP_PHASE, param_buf, 1);
	return ret;
}

unsigned char wifi_get_eap_phase(void)
{
	unsigned char eap_phase = 0;

	eap_phase = (u8)inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_EAP_PHASE, NULL, 0);
	return eap_phase;
}

int wifi_set_eap_method(unsigned char eap_method)
{
	int ret = 0;
	u32 param_buf[1];
	param_buf[0] = eap_method;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_EAP_METHOD, param_buf, 1);
	return ret;
}

int wifi_send_eapol(inic_port_t *iport, char *ifname, char *buf, __u16 buf_len, __u16 flags)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr_ifname = 0, dma_addr_buf = 0;
	u32 param_buf[4];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr_ifname = dma_map_single(pdev, ifname, strlen(ifname), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_ifname)) {
		printk(KERN_ERR "%s, %s: mapping ifname dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr_buf = dma_map_single(pdev, buf, buf_len, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_buf)) {
		printk(KERN_ERR "%s, %s: mapping buf dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		dma_unmap_single(pdev, dma_addr_ifname, strlen(ifname), DMA_TO_DEVICE);
		goto func_exit;
	}

	param_buf[0] = (u32)dma_addr_ifname;
	param_buf[1] = (u32)dma_addr_buf;
	param_buf[2] = buf_len;
	param_buf[3] = flags;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SEND_EAPOL, param_buf, 4);
	dma_unmap_single(pdev, dma_addr_buf, buf_len, DMA_TO_DEVICE);
	dma_unmap_single(pdev, dma_addr_ifname, strlen(ifname), DMA_TO_DEVICE);

func_exit:
	return ret;
}

/* -------------------------------------------------------------------------- */
int wifi_set_custom_ie(inic_port_t *iport, int type, void *cus_ie, int ie_num)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0, *pdma_addr_ie = NULL;
	u32 param_buf[3];
	u8 ie_len = 0, *pie_len = NULL;
	rtw_custom_ie_t *pcus_ie = NULL;
	rtw_custom_ie_t ie_t = {0};
	int cnt = 0;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdma_addr_ie = (dma_addr_t *)kzalloc(sizeof(dma_addr_t) * ie_num, GFP_KERNEL);
	if (pdma_addr_ie == NULL) {
		printk(KERN_ERR "%s, %s: alloc pdma_addr_ie failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}
	pie_len = kzalloc(ie_num, GFP_KERNEL);
	if (pie_len == NULL) {
		printk(KERN_ERR "%s, %s: alloc pie_len failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	dma_addr = dma_map_single(pdev, cus_ie, ie_num * sizeof(rtw_custom_ie_t), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping buf dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_len;
	}

	pcus_ie = cus_ie;
	for (cnt = 0; cnt < ie_num; cnt++) {
		ie_t = *(pcus_ie + cnt);
		ie_len = ie_t.ie[1];
		pdma_addr_ie[cnt] = dma_map_single(pdev, ie_t.ie, ie_len + 2, DMA_TO_DEVICE);
		ie_t.ie = (u8 *)pdma_addr_ie[cnt];
		pie_len[cnt] = ie_len;
	}

	param_buf[0] = type;//type 0 means add, type 1 means update
	param_buf[1] = (u32)dma_addr;
	param_buf[2] = ie_num;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_CUS_IE, param_buf, 3);

	for (cnt = 0; cnt < ie_num; cnt++) {
		dma_unmap_single(pdev, pdma_addr_ie[cnt], pie_len[cnt] + 2, DMA_TO_DEVICE);
	}
	dma_unmap_single(pdev, dma_addr, ie_num * sizeof(rtw_custom_ie_t), DMA_TO_DEVICE);

free_len:
	kfree(pie_len);

free_buf:
	kfree(pdma_addr_ie);

func_exit:
	return ret;
}

void wifi_set_indicate_mgnt(int enable)
{
	u32 param_buf[1];
	param_buf[0] = (u32)enable;
	inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_IND_MGNT, param_buf, 1);
}

int wifi_send_raw_frame(inic_port_t *iport, raw_data_desc_t *raw_data_desc)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr_data_desc = 0, dma_addr_buf = 0;
	u32 param_buf[1];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr_data_desc = dma_map_single(pdev, raw_data_desc, sizeof(raw_data_desc_t), DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_data_desc)) {
		printk(KERN_ERR "%s, %s: mapping raw_data_desc dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr_buf = dma_map_single(pdev, raw_data_desc->buf, raw_data_desc->buf_len, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr_buf)) {
		printk(KERN_ERR "%s, %s: mapping buf dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		dma_unmap_single(pdev, dma_addr_data_desc, sizeof(raw_data_desc_t), DMA_TO_DEVICE);
		goto func_exit;
	}

	raw_data_desc->buf = (u8 *)dma_addr_buf;
	param_buf[0] = (u32)dma_addr_data_desc;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SEND_MGNT, param_buf, 1);
	dma_unmap_single(pdev, dma_addr_buf, raw_data_desc->buf_len, DMA_TO_DEVICE);
	dma_unmap_single(pdev, dma_addr_data_desc, sizeof(raw_data_desc_t), DMA_TO_DEVICE);

func_exit:
	return ret;
}

int wifi_set_tx_rate_by_ToS(unsigned char enable, unsigned char ToS_precedence, unsigned char tx_rate)
{
	int ret = 0;
	u32 param_buf[3];

	param_buf[0] = (u32)enable;
	param_buf[1] = (u32)ToS_precedence;
	param_buf[2] = (u32)tx_rate;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_TXRATE_BY_TOS, param_buf, 3);
	return ret;
}

int wifi_set_EDCA_param(unsigned int AC_param)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = AC_param;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_EDCA_PARAM, param_buf, 1);
	return ret;
}

int wifi_set_TX_CCA(unsigned char enable)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = enable;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_TX_CCA, param_buf, 1);
	return ret;
}

int wifi_set_cts2self_duration_and_send(unsigned char wlan_idx, unsigned short duration)
{
	int ret = 0;
	u32 param_buf[2];

	param_buf[0] = (u32)wlan_idx;
	param_buf[1] = (u32)duration;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_CTS2SEFL_DUR_AND_SEND, param_buf, 2);
	return ret;

}

int wifi_init_mac_filter(void)
{
	int ret = 0;
	u32 param_buf[1];

	param_buf[0] = 0;//type 0 means init
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_MAC_FILTER, param_buf, 1);
	return ret;
}

int wifi_add_mac_filter(inic_port_t *iport, unsigned char *hwaddr)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[2];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, hwaddr, ETH_ALEN, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping hwaddr dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	param_buf[0] = 1;//type 1 means add
	param_buf[1] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_MAC_FILTER, param_buf, 2);
	dma_unmap_single(pdev, dma_addr, ETH_ALEN, DMA_TO_DEVICE);

func_exit:
	return ret;
}

int wifi_del_mac_filter(inic_port_t *iport, unsigned char *hwaddr)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[2];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, hwaddr, ETH_ALEN, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping hwaddr dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	param_buf[0] = 2;//type 2 means delete
	param_buf[1] = (u32)hwaddr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_MAC_FILTER, param_buf, 2);
	dma_unmap_single(pdev, dma_addr, ETH_ALEN, DMA_TO_DEVICE);

func_exit:
	return ret;
}

int wifi_get_antenna_info(inic_port_t *iport, unsigned char *antenna)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[1];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, antenna, sizeof(unsigned char), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping antenna dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	param_buf[0] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_ANTENNA_INFO, param_buf, 1);
	dma_unmap_single(pdev, dma_addr, sizeof(unsigned char), DMA_FROM_DEVICE);

func_exit:
	return ret;
}

WL_BAND_TYPE wifi_get_band_type(void)
{
	u8 ret;

	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_BAND_TYPE, NULL, 0);

	if (ret == 0) {
		return WL_BAND_2_4G;
	} else if (ret == 1) {
		return WL_BAND_5G;
	} else {
		return WL_BAND_2_4G_5G_BOTH;
	}
}

int wifi_get_auto_chl(inic_port_t *iport, unsigned char wlan_idx, unsigned char *channel_set, unsigned char channel_num)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	unsigned char *channel_set_temp = NULL;
	u32 param_buf[3];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	channel_set_temp = kzalloc(channel_num, GFP_KERNEL);
	if (channel_set_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc channel_set failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, channel_set_temp, channel_num, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}
	memcpy(channel_set_temp, channel_set, channel_num);

	param_buf[0] = (u32)wlan_idx;
	param_buf[1] = (u32)dma_addr;
	param_buf[2] = (u32)channel_num;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_AUTO_CHANNEL, param_buf, 3);
	dma_unmap_single(pdev, dma_addr, channel_num, DMA_TO_DEVICE);

free_buf:
	kfree(channel_set_temp);

func_exit:
	return ret;
}

int wifi_del_station(inic_port_t *iport, unsigned char wlan_idx, unsigned char *hwaddr)
{
	int ret = 0;
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	u32 param_buf[2];

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, hwaddr, ETH_ALEN, DMA_TO_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	param_buf[0] = (u32)wlan_idx;
	param_buf[1] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_DEL_STA, param_buf, 2);
	dma_unmap_single(pdev, dma_addr, ETH_ALEN, DMA_TO_DEVICE);

func_exit:
	return ret;
}

int wifi_ap_switch_chl_and_inform(unsigned char new_chl, unsigned char chl_switch_cnt, ap_channel_switch_callback_t callback)
{
	int ret = 0;
	u32 param_buf[3];
	p_ap_channel_switch_callback = callback;

	param_buf[0] = (u32)new_chl;
	param_buf[1] = (u32)chl_switch_cnt;
	param_buf[2] = (u32)callback;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_AP_CH_SWITCH, param_buf, 3);
	return ret;
}

int wifi_config_autoreconnect(__u8 mode, __u8 retry_times, __u16 timeout)
{
	int ret = 0;
	u32 param_buf[3];

	param_buf[0] = mode;
	param_buf[1] = retry_times;
	param_buf[2] = timeout;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_CONFIG_AUTORECONNECT, param_buf, 3);

	return ret;
}

int wifi_get_autoreconnect(inic_port_t *iport, __u8 *mode)
{
	int ret = 0;
	u32 param_buf[1] = {0};
	struct device *pdev = NULL;
	dma_addr_t dma_addr = 0;
	__u8 *mode_temp = NULL;

	if (!iport || !iport->idev || !iport->idev->api_ch) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	pdev = iport->idev->api_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s, %s: device is NULL!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	mode_temp = kzalloc(sizeof(__u8), GFP_KERNEL);
	if (mode_temp == NULL) {
		printk(KERN_ERR "%s, %s: alloc mode_temp failed!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, mode_temp, sizeof(__u8), DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s, %s: mapping dma error!\n",\
		       INIC_CONF_EXT_INFO, __func__);
		ret = -ENOMEM;
		goto free_buf;
	}

	param_buf[0] = (u32)dma_addr;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_GET_AUTORECONNECT, param_buf, 1);
	dma_unmap_single(pdev, dma_addr, sizeof(__u8), DMA_FROM_DEVICE);
	*mode = *mode_temp;

free_buf:
	kfree(mode_temp);

func_exit:
	return ret;
}

int wifi_set_no_beacon_timeout(unsigned char timeout_sec)
{
	u32 param_buf[1];
	int ret = 0;

	param_buf[0] = (u32)timeout_sec;
	ret = inic_ipc_host_api_send_msg(IPC_API_WIFI_SET_NO_BEACON_TIMEOUT, param_buf, 1);

	return ret;
}