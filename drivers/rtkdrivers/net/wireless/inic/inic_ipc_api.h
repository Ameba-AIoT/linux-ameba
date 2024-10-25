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
  * xxxx.xx.xx | name | summary.
 ******************************************************************************/

#ifndef __INIC_IPC_API_H__
#define __INIC_IPC_API_H__

/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */

/* -------------------------------- Defines --------------------------------- */
#define HOST_MSG_PARAM_NUM (9)
#define DEV_MSG_PARAM_NUM (6)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
enum IPC_WIFI_C2H_EVENT_TYPE {
	IPC_WIFI_API_PROCESS_DONE = 0,
	IPC_WIFI_EVT_SCAN_USER_CALLBACK,
	IPC_WIFI_EVT_SCAN_EACH_REPORT_USER_CALLBACK,
	IPC_WIFI_EVT_AUTO_RECONNECT,
	IPC_WIFI_EVT_EAP_AUTO_RECONNECT,
	IPC_WIFI_EVT_AP_CH_SWITCH,
	IPC_WIFI_EVT_HDL,
	IPC_WIFI_EVT_PROMISC_CALLBACK,
	IPC_WIFI_EVT_GET_LWIP_INFO,
	IPC_WIFI_EVT_SET_NETIF_INFO
};

enum IPC_WIFI_H2C_EVENT_TYPE {
	IPC_WIFI_EVT_PROCESS_DONE = 0,
	//basic
	IPC_API_WIFI_CONNECT,
	IPC_API_WIFI_DISCONNECT,
	IPC_API_WIFI_IS_CONNECTED_TO_AP,
	IPC_API_WIFI_IS_RUNNING,
	IPC_API_WIFI_SET_CHANNEL,
	IPC_API_WIFI_GET_CHANNEL,
	IPC_API_WIFI_GET_DISCONN_REASCON,
	IPC_API_WIFI_ON,
	IPC_API_WIFI_OFF,
	IPC_API_WIFI_SET_MODE,
	IPC_API_WIFI_START_AP,
	IPC_API_WIFI_SCAN_NETWROKS,
	IPC_API_WIFI_GET_SCANNED_AP_INFO,
	IPC_API_WIFI_SCAN_ABORT,
	//ext
	IPC_API_WIFI_PSK_INFO_SET,
	IPC_API_WIFI_PSK_INFO_GET,
	IPC_API_WIFI_GET_MAC_ADDR,
	IPC_API_WIFI_COEX_BLE_SET_SCAN_DUTY,
	IPC_API_WIFI_DRIVE_IS_MP,
	IPC_API_WIFI_GET_ASSOCIATED_CLIENT_LIST,
	IPC_API_WIFI_GET_SETTING,
	IPC_API_WIFI_SET_POWERSAVE_MODE,
	IPC_API_WIFI_SET_MFP_SUPPORT,
	IPC_API_WIFI_SET_GROUP_ID,
	IPC_API_WIFI_SET_PMK_CACHE_EN,
	IPC_API_WIFI_GET_SW_STATISTIC,
	IPC_API_WIFI_GET_PHY_STATISTIC,
	IPC_API_WIFI_SET_NETWORK_MODE,
	IPC_API_WIFI_SET_WPS_PHASE,
	IPC_API_WIFI_SET_GEN_IE,
	IPC_API_WIFI_SET_EAP_PHASE,
	IPC_API_WIFI_GET_EAP_PHASE,
	IPC_API_WIFI_SET_EAP_METHOD,
	IPC_API_WIFI_SEND_EAPOL,
	IPC_API_WIFI_CONFIG_AUTORECONNECT,
	IPC_API_WIFI_GET_AUTORECONNECT,
	IPC_API_WIFI_CUS_IE,
	IPC_API_WIFI_SET_IND_MGNT,
	IPC_API_WIFI_SEND_MGNT,
	IPC_API_WIFI_SET_TXRATE_BY_TOS,
	IPC_API_WIFI_SET_EDCA_PARAM,
	IPC_API_WIFI_SET_TX_CCA,
	IPC_API_WIFI_SET_CTS2SEFL_DUR_AND_SEND,
	IPC_API_WIFI_MAC_FILTER,
	IPC_API_WIFI_GET_ANTENNA_INFO,
	IPC_API_WIFI_GET_BAND_TYPE,
	IPC_API_WIFI_GET_AUTO_CHANNEL,
	IPC_API_WIFI_DEL_STA,
	IPC_API_WIFI_AP_CH_SWITCH,
	IPC_API_WIFI_SET_NO_BEACON_TIMEOUT,
	//promisc
	IPC_API_PROMISC_FILTER_RETRANSMIT_PKT,
	IPC_API_PROMISC_FILTER_WITH_LEN,
	IPC_API_PROMISC_SET,
	IPC_API_PROMISC_IS_ENABLED,
	IPC_API_PROMISC_GET_FIXED_CHANNEL,
	IPC_API_PROMISC_FILTER_BY_AP_AND_PHONE_MAC,
	IPC_API_PROMISC_SET_MGNTFRAME,
	IPC_API_PROMISC_GET_CHANNEL_BY_BSSID,
	IPC_API_PROMISC_UPDATE_CANDI_AP_RSSI_AVG,
	IPC_API_PROMISC_TX_BEACON_CONTROL,
	IPC_API_PROMISC_RESUME_TX_BEACON,
	IPC_API_PROMISC_ISSUE_PROBERSP,
	IPC_API_PROMISC_INIT_PACKET_FILTER,
	IPC_API_PROMISC_ADD_PACKET_FILTER,
	IPC_API_PROMISC_PACKET_FILTER_CONTROL
};

enum IPC_LWIP_INFO_TYPE {
	IPC_WLAN_GET_IP,
	IPC_WLAN_GET_GW,
	IPC_WLAN_GET_GWMSK,
	IPC_WLAN_GET_HW_ADDR,
	IPC_WLAN_IS_VALID_IP
};

typedef struct inic_ipc_host_req_msg {
	u32 api_id;
	u32 param_buf[HOST_MSG_PARAM_NUM];
	int ret;
	u8 dummy[20]; /* add for 64B size alignment */
} inic_ipc_host_req_t;

typedef struct inic_ipc_dev_req_msg {
	u32 enevt_id;
	u32 param_buf[DEV_MSG_PARAM_NUM];
	int ret;
	u8 dummy[32]; /* add for 64B size alignment */
} inic_ipc_dev_req_t;

/* host api structure */
typedef struct host_api_priv {
	struct device *pdev; /* lower layer device */
	struct tasklet_struct api_tasklet; /* api task to haddle api msg */
	ipc_msg_struct_t api_ipc_msg; /* to store ipc msg for api */
	aipc_ch_t *papi_ipc_ch; /* ipc channel to register */
	struct mutex iiha_send_mutex; /* mutex to protect send host api message */
	inic_ipc_host_req_t *preq_msg;/* host api message to send to device */
	dma_addr_t req_msg_phy_addr;/* host api message to send to device */
} host_api_priv_t;

/* -------------------------- Function declaration -------------------------- */
/**
 * @brief  to initialize api priv.
 * @return if is OK, return 0, failed return negative number.
 */
int inic_ipc_host_api_init(inic_dev_t *idev);

/**
 * @brief  to deinitialize api priv.
 * @return NULL.
 */
void inic_ipc_host_api_deinit(void);

/**
 * @brief  to send api message to device.
 * @param  id[in]: h2c message id defined in IPC_WIFI_H2C_EVENT_TYPE.
 * @param  param_buf[in]: parameters' buffer for message.
 * @param  buf_len[in]: the length of parameters' buffer.
 * @return message return from device.
 */
int inic_ipc_host_api_send_msg(u32 id, u32 *param_buf, u32 buf_len);

#endif /* __INIC_IPC_API_H__ */
