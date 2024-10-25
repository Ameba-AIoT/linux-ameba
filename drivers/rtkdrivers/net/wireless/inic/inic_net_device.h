/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
#ifndef __INIC_NET_DEVICE_H__
#define __INIC_NET_DEVICE_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */
#include "inic_linux_base_type.h"
#include "inic_ipc.h"
#include "inic_cmd.h"
#include "inic_ext_cmd.h"
#include "inic_prmc_cmd.h"

/* -------------------------------- Defines --------------------------------- */

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */
/**
 * @brief  put the skb to the network by index
 * @param  idx[in]: the index for wlan port.
 * @param  skb[in]: Rx packet.
 * @return if is OK, return 0, failed return -1.
 */
int inic_netif_rx_by_idx(int idx, struct sk_buff *skb);
/**
 * @brief  set the MAC address for wlan port by wlan index
 * @param  idx[in]: the index for wlan port.
 * @param  addr[in]: the MAC address.
 * @return if is OK, return 0, failed return -1.
 */
int inic_net_set_mac_addr_by_idx(int idx, void *addr);
/**
 * @brief  get the MAC address for wlan port by wlan index
 * @param  idx[in]: the index for wlan port.
 * @return poiter to the MAC address.
 */
uint8_t *inic_net_get_mac_addr_by_idx(int idx);
/**
 * @brief  indicate the net device is connected
 * @param  idx[in]: the index for wlan port.
 * @return if is OK, return 0, failed return -1.
 */
int inic_net_indicate_connect(int idx);
/**
 * @brief  indicate the net device is disconected.
 * @param  idx[in]: the index for wlan port.
 * @return if is OK, return 0, failed return -1.
 */
int inic_net_indicate_disconnect(int idx);
/**
 * @brief  to get the IP gateway.
 * @param  idx[in]: the index for wlan port.
 * @return return pointer to store IP address's gateway.
 */
uint32_t *inic_netif_get_ip_gateway(int idx);
/**
 * @brief  to get the IP address's mask.
 * @param  idx[in]: the index for wlan port.
 * @return return pointer to store IP address's mask.
 */
uint32_t *inic_netif_get_ip_mask(int idx);
/**
 * @brief  to get the IP address.
 * @param  idx[in]: the index for wlan port.
 * @return return pointer to store ip address.
 */
uint32_t *inic_netif_get_ip_address(int idx);
/**
 * @brief  to allocate inic_device and initialize all net device and inic port.
 * @return return the allocated and initialized inic_device.
 */
inic_dev_t *inic_init_device(struct device *pdev, int port_num);
/**
 * @brief  to free inic_device and deinitialize all net device and inic port.
 * @return return the allocated and initialized inic_device.
 */
void inic_deinit_device(inic_dev_t *idev);

#endif /* __INIC_NET_DEVICE_H__ */
