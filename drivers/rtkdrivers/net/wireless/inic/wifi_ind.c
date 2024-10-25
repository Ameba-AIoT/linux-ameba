/* SPDX-License-Identifier:  GPL-2.0-or-later */
/*
 * Derived from many drivers using ameba IPC inic device.
 *
 * Copyright (C) 2020 Realsil <andrew_su@realsil.com.cn>
 *
 * RTK wlan driver for Ameba IPC inic.
 *
 */
#include "wifi_ind.h"
#include "wifi_conf.h"
#include "inic_cmd.h"

#define __WIFI_IND_C__
/* -------------------------------- Defines --------------------------------- */
#define WIFI_IND "wifi ind"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* -------------------------- Function declaration -------------------------- */

/* --------------------------- Private Variables ---------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* ----------------------------- Local Variables ---------------------------- */
static uint8_t event_callback_en[WIFI_EVENT_MAX][WIFI_EVENT_MAX_ROW] = {0};

/* ---------------------------- Private Functions --------------------------- */
/**
 * @brief  to handle the wifi event from inic device.
 * @param[in]  api: to get device.
 * @param[in]  event: event is in _WIFI_EVENT_INDICATE.
 * @param[in]  buf: buf from device for this event.
 * @param[in]  buf_len: length of buf.
 * @param[in]  flags: flags of this event.
 * @return  If the function succeeds, the return value is RTW_SUCCESS.
 * @note
 */
static rtw_result_t wifi_indicate_event_handle(struct host_api_priv *api, rtw_event_indicate_t event,\
					      char *buf, int buf_len, int flags)
{
	dma_addr_t dma_addr = 0;
	struct device *pdev = NULL;
	rtw_result_t ret = RTW_SUCCESS;
	int i;

	if (!api || !api->papi_ipc_ch) {
		printk(KERN_ERR "%s: host_api_priv is NULL!\n", WIFI_IND);
		goto func_exit;
	}

	pdev = api->papi_ipc_ch->pdev;
	if (!pdev) {
		printk(KERN_ERR "%s: device is NULL!\n", WIFI_IND);
		goto func_exit;
	}

	if (event >= WIFI_EVENT_MAX) {
		ret = RTW_BADARG;
		goto func_exit;
	}

	dma_addr = dma_map_single(pdev, buf, buf_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(pdev, dma_addr)) {
		printk(KERN_ERR "%s: device is NULL!\n", WIFI_IND);
		ret = RTW_NOMEM;
		goto func_exit;
	}

	for (i = 0; i < WIFI_EVENT_MAX_ROW; i++) {
		if (event_callback_en[event][i]) {
			/* only call haddler once */
			inic_io_wifi_ind_cb(event, buf, buf_len, flags);
			break;
		}
	}
	dma_unmap_single(pdev, dma_addr, buf_len, DMA_FROM_DEVICE);

	ret = RTW_SUCCESS;

func_exit:
	return ret;
}

__attribute__((unused)) static void init_event_callback_list(void)
{
	memset(event_callback_en, 0, sizeof(event_callback_en));
}

/* ---------------------------- Public Functions ---------------------------- */
/**
  * @brief  Wlan driver indicate event to upper layer through wifi_indication.
  * @param[in]  api: to get device.
  * @param[in]  event: An event reported from driver to upper layer application. Please refer to rtw_event_indicate_t enum.
  * @param[in]  buf: If it is not NUL, buf is a pointer to the buffer for message string.
  * @param[in]  buf_len: The length of the buffer.
  * @param[in]  flags: Indicate some extra information, sometimes it is 0.
  * @retval None
  * @note  If upper layer application triggers additional operations on receiving of wireless_send_event,
  *	please strictly check current stack size usage (by using uxTaskGetStackHighWaterMark() ),
  *	and tries not to share the same stack with wlan driver if remaining stack space is not available
  *	for the following operations.
  *	ex: using semaphore to notice another thread instead of handing event directly in wifi_indication().
  * Wifi event list
  *
  * WIFI_EVENT_CONNECT			: Indicate station connect to AP
  * WIFI_EVENT_DISCONNECT		: Indicate station disconnect with AP
  * WIFI_EVENT_RX_MGNT			: Indicate managerment frame receive
  * WIFI_EVENT_STA_ASSOC		: Indicate client associate in AP mode
  * WIFI_EVENT_STA_DISASSOC		: Indicate client disassociate in AP mode

  * WIFI_EVENT_GROUP_KEY_CHANGED	: Indicate Group key(GTK) updated
  * WIFI_EVENT_RECONNECTION_FAIL	: Indicate wifi reconnection failed
  * WIFI_EVENT_ICV_ERROR		: Indicate that the receiving packets has ICV error.
  * WIFI_EVENT_CHALLENGE_FAIL		: Indicate authentication failed because of challenge failure

  * WIFI_EVENT_P2P_SEND_ACTION_DONE	: Indicate the action frame status in p2p. Need to define CONFIG_P2P_NEW in wlan library, default is disable.

  * WIFI_EVENT_WPA_STA_WPS_START	: Indicate WPS process starting. This event is used in wps process.
  * WIFI_EVENT_WPA_WPS_FINISH		: Indicate WPS process finish. This event is used in wps process.
  * WIFI_EVENT_WPA_EAPOL_START		: Indicate receiving EAPOL_START packets in eap process. This event is used in eap process.
  * WIFI_EVENT_WPA_EAPOL_RECVD		: Indicate receiving EAPOL packets in wps process. This event is used in wps process.
  * WIFI_EVENT_MAX			: It stands for the end of wifi event.
  */
void wifi_indication(struct host_api_priv *api, rtw_event_indicate_t event,\
		     char *buf, int buf_len, int flags)
{
	if (event == WIFI_EVENT_JOIN_STATUS) {
		if (p_wifi_join_status_internal_callback) {
			p_wifi_join_status_internal_callback((rtw_join_status_t)flags);
		}
	} else {
		wifi_indicate_event_handle(api, event, buf, buf_len, flags);
	}
}

/**
 * @brief  Register the event listener.
 * @param[in] event_cmds : The event command number indicated.
 * @param[in] row_id : The row number for same event command.
 * @return  RTW_SUCCESS : if successfully registers the event.
 * @return  RTW_ERROR : if an error occurred.
 * @note  Set the same event_cmds with empty handler_func will
 *  	 unregister the event_cmds.
 */
void wifi_reg_event_handler(unsigned int event_cmds, int row_id)
{
	if ((event_cmds < WIFI_EVENT_MAX) && (row_id < WIFI_EVENT_MAX_ROW)) {
		if (event_callback_en[event_cmds][row_id] == 0) {
			event_callback_en[event_cmds][row_id] = 1;
		}
	}

	return;
}

/**
 * @brief  Un-register the event listener.
 * @param[in] event_cmds : The event command number indicated.
 * @param[in] row_id : The row number for same event command.
 *
 * @return  RTW_SUCCESS : if successfully un-registers the event .
 * @return  RTW_ERROR : if an error occurred.
 */
void wifi_unreg_event_handler(unsigned int event_cmds, int row_id)
{
	if ((event_cmds < WIFI_EVENT_MAX) && (row_id < WIFI_EVENT_MAX_ROW)) {
		if (event_callback_en[event_cmds][row_id] != 0) {
			event_callback_en[event_cmds][row_id] = 0;
		}
	}

	return;
}
