/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
#ifndef __WIFI_CONF_H__
#define __WIFI_CONF_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */
#include "inic_linux_base_type.h"

/* -------------------------------- Defines --------------------------------- */

/* -------------------------------- Macros ---------------------------------- */
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARG(x)	((u8*)(x))[0],((u8*)(x))[1],\
			((u8*)(x))[2],((u8*)(x))[3],\
			((u8*)(x))[4],((u8*)(x))[5]
#define CMP_MAC(a, b)	(((a[0])==(b[0]))&& \
			((a[1])==(b[1]))&& \
			((a[2])==(b[2]))&& \
			((a[3])==(b[3]))&& \
			((a[4])==(b[4]))&& \
			((a[5])==(b[5])))

/* ------------------------------- Data Types ------------------------------- */
/* ie format
 * +-----------+--------+-----------------------+
 * |element ID | length | content in length byte|
 * +-----------+--------+-----------------------+
 *
 * type: refer to CUSTOM_IE_TYPE
 */
#ifndef _CUS_IE_
#define _CUS_IE_
/**
 * @brief  The structure is used to set WIFI custom ie list,
 * 	and type match CUSTOM_IE_TYPE.
 * 	The ie will be transmitted according to the type.
 */
typedef struct _cus_ie {
	__u8 *ie;
	__u8 type;
} rtw_custom_ie_t, *p_rtw_custom_ie_t;
#endif /* _CUS_IE_ */

typedef enum _WL_BAND_TYPE {
	WL_BAND_2_4G = 0,
	WL_BAND_5G,
	WL_BAND_2_4G_5G_BOTH,
	WL_BANDMAX
} WL_BAND_TYPE, *PWL_BAND_TYPE;
/* ---------------------------- Global Variables ---------------------------- */
extern rtw_joinstatus_callback_t p_wifi_join_status_internal_callback;
extern rtw_result_t (*scan_user_callback_ptr)(unsigned int, void *);
extern rtw_result_t (*scan_each_report_user_callback_ptr)(rtw_scan_result_t *, void *);

/* -------------------------- Function declaration -------------------------- */
int wifi_is_connected_to_ap(void);
int wifi_is_running(int index);
int wifi_connect(inic_port_t *iport, rtw_network_info_t *connect_param,\
		 unsigned char block);
int wifi_disconnect(void);
int wifi_get_disconn_reason_code(inic_port_t *iport, unsigned short *reason_code);
int wifi_set_channel(int channel);
int wifi_get_channel(inic_port_t *iport, int *channel);
int wifi_set_mode(inic_port_t *iport, rtw_mode_t mode);
int wifi_start_ap(inic_port_t *iport, rtw_softap_info_t *softAP_config);
int wifi_get_scan_records(inic_port_t *iport, unsigned int *ap_num, char *scan_buf);
int wifi_get_setting(inic_port_t *iport, rtw_wifi_setting_t *psetting);
int wifi_scan_networks(inic_port_t *iport, rtw_scan_param_t *scan_param,\
		       int ssid_length, unsigned char block);
int wifi_scan_abort(void);
rtw_join_status_t wifi_get_join_status(void);
int wifi_on(inic_port_t *iport, rtw_mode_t mode);
int wifi_off(inic_port_t *iport);

/* ---------------------------- extended wifi conf --------------------------- */
int wifi_psk_info_set(inic_port_t *iport, struct psk_info *psk_data);
int wifi_psk_info_get(inic_port_t *iport, struct psk_info *psk_data);
int wifi_get_mac_address(inic_port_t *iport, rtw_mac_t *mac);
int wifi_btcoex_set_ble_scan_duty(u8 duty);
u8 wifi_driver_is_mp(void);
int wifi_get_associated_client_list(inic_port_t *iport, void *client_list_buffer, unsigned short buffer_length);
int wifi_get_setting(inic_port_t *iport, rtw_wifi_setting_t *psetting);
int wifi_set_powersave_mode(u8 ips_mode, u8 lps_mode);
int wifi_set_mfp_support(unsigned char value);
int wifi_set_group_id(unsigned char value);
int wifi_set_pmk_cache_enable(unsigned char value);
int wifi_get_sw_statistic(inic_port_t *iport, unsigned char idx, rtw_sw_statistics_t *statistic);
int wifi_fetch_phy_statistic(inic_port_t *iport, rtw_phy_statistics_t *phy_statistic);
int wifi_set_network_mode(rtw_network_mode_t mode);
int wifi_set_wps_phase(unsigned char is_trigger_wps);
int wifi_set_gen_ie(inic_port_t *iport, unsigned char wlan_idx, char *buf, __u16 buf_len, __u16 flags);
int wifi_set_eap_phase(unsigned char is_trigger_eap);
unsigned char wifi_get_eap_phase(void);
int wifi_set_eap_method(unsigned char eap_method);
int wifi_send_eapol(inic_port_t *iport, char *ifname, char *buf, __u16 buf_len, __u16 flags);
int wifi_set_custom_ie(inic_port_t *iport, int type, void *cus_ie, int ie_num);
void wifi_set_indicate_mgnt(int enable);
int wifi_send_raw_frame(inic_port_t *iport, raw_data_desc_t *raw_data_desc);
int wifi_set_tx_rate_by_ToS(unsigned char enable, unsigned char ToS_precedence, unsigned char tx_rate);
int wifi_set_EDCA_param(unsigned int AC_param);
int wifi_set_TX_CCA(unsigned char enable);
int wifi_set_cts2self_duration_and_send(unsigned char wlan_idx, unsigned short duration);
int wifi_init_mac_filter(void);
int wifi_add_mac_filter(inic_port_t *iport, unsigned char *hwaddr);
int wifi_del_mac_filter(inic_port_t *iport, unsigned char *hwaddr);
int wifi_get_antenna_info(inic_port_t *iport, unsigned char *antenna);
WL_BAND_TYPE wifi_get_band_type(void);
int wifi_get_auto_chl(inic_port_t *iport, unsigned char wlan_idx, unsigned char *channel_set, unsigned char channel_num);
int wifi_del_station(inic_port_t *iport, unsigned char wlan_idx, unsigned char *hwaddr);
int wifi_ap_switch_chl_and_inform(unsigned char new_chl, unsigned char chl_switch_cnt, ap_channel_switch_callback_t callback);
int wifi_config_autoreconnect(__u8 mode, __u8 retry_times, __u16 timeout);
int wifi_get_autoreconnect(inic_port_t *iport, __u8 *mode);
int wifi_set_no_beacon_timeout(unsigned char timeout_sec);

#endif /* __WIFI_CONF_H__ */
