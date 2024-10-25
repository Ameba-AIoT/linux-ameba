/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
#ifndef __WIFI_PROMISC_H__
#define __WIFI_PROMISC_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */
#include "inic_linux_base_type.h"

/* -------------------------------- Defines --------------------------------- */

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
typedef void (*ipc_promisc_callback_t)(unsigned char *, unsigned int, void *);

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */
int promisc_filter_retransmit_pkt(u8 enable, u8 filter_interval_ms);
int promisc_filter_with_len(u16 len);
int promisc_set(rtw_rcr_level_t enabled, ipc_promisc_callback_t cb, unsigned char len_used);
unsigned char is_promisc_enabled(void);
int promisc_get_fixed_channel(inic_port_t *iport, void *fixed_bssid, u8 *ssid,\
			      int *ssid_length);
int promisc_filter_by_ap_and_phone_mac(inic_port_t *iport, u8 enable,\
				       void *ap_mac, void *phone_mac);
int promisc_set_mgntframe(u8 enable);
int promisc_get_chnl_by_bssid(inic_port_t *iport, u8 *bssid);
int promisc_update_candi_ap_rssi_avg(s8 rssi, u8 cnt);
int promisc_stop_tx_beacn(void);
int promisc_resume_tx_beacn(void);
int promisc_issue_probersp(inic_port_t *iport, u8 *da);
int promisc_init_packet_filter(void);
int promisc_add_packet_filter(inic_port_t *iport, u8 filter_id,\
			       rtw_packet_filter_pattern_t *patt,\
			       rtw_packet_filter_rule_t rule);
int promisc_enable_packet_filter(u8 filter_id);
int promisc_disable_packet_filter(u8 filter_id);
int promisc_remove_packet_filter(u8 filter_id);

#endif /* __WIFI_PROMISC_H__ */
