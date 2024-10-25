/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __INIC_NET_DEVICE_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */
#include "inic_net_device.h"
#include "inic_ipc_host_trx.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_NET_INFO "inic net"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* -------------------------- Function declaration -------------------------- */
static int inic_netdev_open(struct net_device *pnetdev);
static int inic_netdev_close(struct net_device *pnetdev);
static netdev_tx_t inic_xmit_entry(struct sk_buff *skb, struct net_device *dev);
static int inic_net_set_mac_address(struct net_device *dev, void *addr);
static struct net_device_stats* inic_net_get_stats(struct net_device *dev);
static void inic_net_get_stats64(struct net_device *dev,
				 struct rtnl_link_stats64 *storage);
static int inic_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

/* ---------------------------- Global Variables ---------------------------- */

/* --------------------------- Private Variables ---------------------------- */
/**
 * to link the net_device_ops' function. This is the interface for net device,
 * the driver must follow the standard interface.
 */
static const struct net_device_ops inic_netdev_ops = {
	.ndo_open = inic_netdev_open,
	.ndo_stop = inic_netdev_close,
	.ndo_start_xmit = inic_xmit_entry,
	.ndo_set_mac_address = inic_net_set_mac_address,
	.ndo_get_stats = inic_net_get_stats,
	.ndo_get_stats64 = inic_net_get_stats64,
	.ndo_do_ioctl = inic_ioctl,
};

/**
 * because of index to distigush the wlan port in inic, local_idev is used to
 * assocciate the inic_port and index.
 */
static inic_dev_t *local_idev = NULL;

static uint32_t inic_ip_addr[INIC_MAX_NET_PORT_NUM] = {0};
static uint32_t inic_ip_mask[INIC_MAX_NET_PORT_NUM] = {0};
static uint32_t inic_ip_gw[INIC_MAX_NET_PORT_NUM] = {0};

/* --------------------------- Private Functions ---------------------------- */
/**
 * @brief  the function will be call when the network is up. It will intilizate
 *	some component in the driver.
 * @param  dev[in]: the pointer to the net_device.
 * @return if is OK, return 0, failed return -1.
 */
static int inic_netdev_open(struct net_device *dev)
{
	inic_port_t *iport = netdev_priv(dev);
	int ret = 0;

	if (iport == NULL) {
		printk("%s: inic_port is NULL!\n", __func__);
		ret = -1;
		goto func_exit;
	}

	if (iport->state == INIC_STATE_DOWN) {
		if (iport->wlan_idx == 0) {
			/* eth0 open to sta mode in default */
			ret = wifi_on(iport, RTW_MODE_STA);
				if (ret == RTW_SUCCESS) {
				printk("%s: inic dev %d is up!\n", __func__, iport->wlan_idx);
				iport->state = INIC_STATE_UP;
			}
		} else {
			iport->state = INIC_STATE_AP_UP;
		}
	}

func_exit:
	return ret;
}

/**
 * @brief  the function will be call when the network is down. It will
 * 	deintilizate some component in the driver.
 * @param  dev[in]: the pointer to the net_device.
 * @return if is OK, return 0, failed return -1.
 */
static int inic_netdev_close(struct net_device *dev)
{
	inic_port_t *iport = netdev_priv(dev);
	char defualt_mac[ETH_ALEN] = {0};
	int ret = 0;

	if (iport == NULL) {
		printk("%s: inic_port is NULL!\n\r", __func__);
		ret = -1;
		goto func_exit;
	}

	if (iport->state != INIC_STATE_DOWN) {
		if (iport->wlan_idx == 0) {
			ret = wifi_off(iport);
			if (ret == RTW_SUCCESS) {
				printk("%s: inic is down!\n", __func__);
				inic_net_set_mac_addr_by_idx(iport->wlan_idx, &defualt_mac);
				iport->state = INIC_STATE_DOWN;
			}
		} else {
			iport->state = INIC_STATE_DOWN;
		}
	}

func_exit:
	return ret;
}

/**
 * @brief  to implement the xmit function for net device.
 * @param  skb[in]: skb buffer to send.
 * @param  dev[in]: the pointer to the net_device.
 * @return if is OK, return 0, failed return -1.
 */
static netdev_tx_t inic_xmit_entry(struct sk_buff *skb, struct net_device *dev)
{
	inic_port_t *iport = netdev_priv(dev);
	int idx = 0;
	int ret = 0;

	if (iport == NULL) {
		printk("%s: invalid inic_port!\n\r", __func__);
		ret = -1;
		goto func_exit;
	}

	if (iport->state < INIC_STATE_CONNECTED) {
		printk("%s: Wifi is not connected!\n\r", __func__);
		ret = -1;
		goto func_exit;
	}

	idx = iport->wlan_idx;
	if (inic_ipc_host_send(idx, skb) == 0) {
		ret = NETDEV_TX_OK;
	} else
		ret = NETDEV_TX_BUSY;

func_exit:
	return ret;
}

/**
 * @brief  set the MAC address for wlan port
 * @param  dev[in]: the pointer to the net_device.
 * @param  addr[in]: the MAC address.
 * @return if is OK, return 0, failed return -1.
 */
static int inic_net_set_mac_address(struct net_device *dev, void *addr)
{
	return 0;
}

/**
 * @brief  to update the status of wlan
 * @param  dev[in]: the pointer to the net_device.
 * @return inet_device_stats: the stats stucture.
 */
static struct net_device_stats* inic_net_get_stats(struct net_device *dev)
{
	inic_port_t *iport = netdev_priv(dev);
	struct net_device_stats *psts = &(iport->stats);

	return psts;
}

/**
 * @brief  to update the status with 64 bits format of wlan. It is same to
 * 	inic_net_get_stats, and the noly difference is the bit length of member
 * 	in the structure. So to fill zore before assigning the data.
 * @param  dev[in]: the pointer to the net_device.
 * @param  storage[out]: the pointer to update rtnl_link_stats64.
 */
static void inic_net_get_stats64(struct net_device *dev,
				 struct rtnl_link_stats64 *storage)
{
	inic_port_t *iport = netdev_priv(dev);

	storage->tx_packets = iport->stats.tx_packets;
	storage->tx_bytes = iport->stats.tx_bytes;
	storage->rx_packets = iport->stats.rx_packets;
	storage->rx_bytes = iport->stats.rx_bytes;
	storage->rx_errors = iport->stats.rx_errors;
	storage->tx_errors = iport->stats.tx_errors;
	storage->rx_dropped = iport->stats.rx_dropped;
	storage->tx_dropped = iport->stats.tx_dropped;
	storage->multicast = iport->stats.multicast;
	storage->collisions = iport->stats.collisions;
}

/**
 * @brief  Called when a user requests an ioctl which can't be handled by
 * 	the generic interface code. If not defined ioctls return not supported
 * 	error code. It is used to divivery the AT command from userspace.
 * @param  dev[in]: the pointer to the net_device.
 * @param  ifr[in]: Interface request structure used for socket ioctl ioctl's.
 * @param  cmd[in]: command used for socket ioctl.
 * @return if is OK, return 0, failed return -1.
 */
static int inic_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	inic_port_t *iport = netdev_priv(dev);
	struct iwreq *wrq = (struct iwreq *)ifr;
	struct iw_point *data = (void *)&wrq->u.data;
	inic_io_cmd_t inic_cmd = {0};
	int ret = 0;

	ret = copy_from_user((uint8_t *)&inic_cmd, (uint8_t __user *)data->pointer,\
			     sizeof(inic_io_cmd_t));
	if (ret) {
		printk("%s: copy inic_cmd from usersapce failed (%d)!\n",
		       __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	switch (cmd) {
	case RINICIO_CMD:
		ret = inic_ioctl_hdl(iport, &inic_cmd);
		break;
	case RINICIO_EXT_CMD:
		ret = inic_ext_ioctl_hdl(iport, &inic_cmd);
		break;
	case RINICIO_PRMC_CMD:
		ret = inic_prmc_ioctl_hdl(iport, &inic_cmd);
		break;
	default:
		printk("%s: unknown ioctl cmd 0x%x.\n",
		       __func__, cmd);
		break;
	}

func_exit:
	return ret;
}

/**
 * @brief  to allocate the net_device and inic_port, the initialize the
 * 	netdev_ops and the inic_port.
 * @param  idev[in]: the inic device for the entire inic driver.
 * @param  wlan_idx[in]: index for this wlan port.
 * @return return the allocated net_device.
 */
static struct net_device *inic_alloc_netdevice(inic_dev_t *idev, \
					       int wlan_idx)
{
	struct net_device *ndev = NULL;
	inic_port_t *iport = NULL;

	if (!idev) {
		printk("%s: no initialize the inic device!\n", __FUNCTION__);
		goto func_exit;
	}

	ndev = alloc_netdev_mqs(sizeof(inic_port_t), "wlan%d", NET_NAME_UNKNOWN,
				ether_setup, 1, 1);
	if (!ndev) {
		printk("%s: Allocate etherdev error!\n", __FUNCTION__);
		goto func_exit;
	}
	/* get the inic port */
	iport = netdev_priv(ndev);

	if (!iport) {
		printk("%s: Allocate inic port error!\n", __FUNCTION__);
		goto free_netdev;
	}
	iport->ndev = ndev;

	/* add port in the port list */
	spin_lock(&idev->lock);
	list_add(&iport->list, &idev->port_list);
	spin_unlock(&idev->lock);
	idev->piport[wlan_idx] = iport;
	iport->idev = idev;
	iport->wlan_idx = wlan_idx;
	iport->state = INIC_STATE_DOWN;
	iport->nl_sk = NULL;

	/* associate the iport and commad operate */
	inic_init_cmd(iport);
	inic_init_ext_cmd(iport);
	inic_init_prmc_cmd(iport);

	/* assigne the opos functions */
	ndev->netdev_ops = &inic_netdev_ops;

	printk("%s: Allocate netdevice %d succussfully!\n", __FUNCTION__, wlan_idx);

	goto func_exit;

free_netdev:
	free_netdev(ndev);

func_exit:
	return ndev;
}

/**
 * @brief  to free the resource of netdevice include the inic port.
 * @param  ndev[inout]: the net devide to free.
 * @return none.
 */
static void inic_free_netdevice(struct net_device *ndev)
{
	inic_port_t *iport = NULL;
	inic_dev_t *idev = NULL;

	iport = netdev_priv(ndev);
	if (iport && iport->idev) {
		/* remove inic port from the port list */
		inic_deinit_prmc_cmd(iport);
		inic_deinit_ext_cmd(iport);
		inic_deinit_cmd(iport);
		idev = iport->idev;
		spin_lock(&iport->idev->lock);
		list_del(&iport->list);
		spin_unlock(&iport->idev->lock);
		idev->piport[iport->wlan_idx] = NULL;
		idev->pndev[iport->wlan_idx] = NULL;
	}
	free_netdev(ndev);

	return;
}

/**
 * @brief  to find the netdevice from inic devide by index.
 * @param  idev[in]: the inic device for the entire inic driver.
 * @param  wlan_idx[in]: index for this wlan port.
 * @return return the find net_device.
 */
static struct net_device *inic_find_ndev_by_idx(inic_dev_t *idev, \
						int wlan_idx)
{
	inic_port_t *iport = NULL;
	struct net_device *ndev = NULL;

	if (idev && (wlan_idx < MAX_NUM_WLAN_PORT) \
	    && !list_empty(&idev->port_list)) {
		list_for_each_entry(iport, &idev->port_list, list) {
			if (iport->wlan_idx == wlan_idx) {
				ndev = iport->ndev;
				goto func_exit;
			}
		}
	} else {
		printk("%s: invalid inic_device or wlan_idx!\n", __FUNCTION__);
	}

func_exit:
	return ndev;
}

/* ---------------------------- Public Functions ---------------------------- */
/**
 * @brief  set the MAC address for wlan port by wlan index
 * @param  idx[in]: the index for wlan port.
 * @param  addr[in]: the MAC address.
 * @return if is OK, return 0, failed return -1.
 */
int inic_net_set_mac_addr_by_idx(int idx, void *addr)
{
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;
	int ret = 0;

	if (addr == NULL) {
		ret = -EINVAL;
		printk("%s: mac address is NULL!\n", __func__);
		goto func_exit;
	}

	ndev = inic_find_ndev_by_idx(idev, idx);
	if (ndev == NULL) {
		ret = -ENODEV;
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}
	memcpy(ndev->dev_addr, addr, ETH_ALEN);

func_exit:
	return ret;
}

uint8_t *inic_net_get_mac_addr_by_idx(int idx)
{
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;
	uint8_t *ret = NULL;

	ndev = inic_find_ndev_by_idx(idev, idx);
	if (ndev == NULL) {
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}

	ret = ndev->dev_addr;

func_exit:
	return ret;
}

/**
 * @brief  indicate the net device is connected
 * @param  idx[in]: the index for wlan port.
 * @return if is OK, return 0, failed return -1.
 */
int inic_net_indicate_connect(int idx)
{
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;
	struct in_ifaddr *ifa;
	int ret = 0;

	if (idx >= MAX_NUM_WLAN_PORT) {
		ret = -EINVAL;
		printk("%s: wlan indix is wrong!\n", __func__);
		goto func_exit;
	}

	ndev = inic_find_ndev_by_idx(idev, idx);
	if (ndev == NULL) {
		ret = -ENODEV;
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}

	if ((idev->mode != RTW_MODE_AP) && (idx == 0)) {
		idev->piport[idx]->state = INIC_STATE_CONNECTED;
	}
	netif_carrier_on(ndev);
	if (!(netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 0)) \
	    && netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 1)) \
	    && netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 2)) \
	    && netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 3)))) {
		netif_tx_start_all_queues(ndev);
	} else {
		netif_tx_wake_all_queues(ndev);
	}

	rcu_read_lock();
	in_dev_for_each_ifa_rcu(ifa, ndev->ip_ptr)
		 printk("address: %pI4, mask: %pI4\n", &ifa->ifa_address, &ifa->ifa_mask);
	rcu_read_unlock();

	ret = 0;

func_exit:
	return ret;
}

/**
 * @brief  indicate the net device is disconected.
 * @param  idx[in]: the index for wlan port.
 * @return if is OK, return 0, failed return -1.
 */
int inic_net_indicate_disconnect(int idx)
{
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;
	int ret = 0;

	if (idx >= MAX_NUM_WLAN_PORT) {
		ret = -EINVAL;
		printk("%s: wlan indix is wrong!\n", __func__);
		goto func_exit;
	}

	ndev = idev->pndev[idx];
	if (ndev == NULL) {
		ret = -ENODEV;
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}

	if (!(netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 0)) \
	    && netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 1)) \
	    && netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 2)) \
	    && netif_tx_queue_stopped(netdev_get_tx_queue(ndev, 3)))) {
		netif_tx_stop_all_queues(ndev);
	}

	netif_carrier_off(ndev);
	if ((idev->mode != RTW_MODE_AP) && (idx == 0)) {
		idev->piport[idx]->state = INIC_STATE_DISCONNECTED;
	}
	ret = 0;

func_exit:
	return ret;
}

/**
 * @brief  put the skb to the network by index
 * @param  idx[in]: the index for wlan port.
 * @param  skb[in]: Rx packet.
 * @return if is OK, return 0, failed return -1.
 */
int inic_netif_rx_by_idx(int idx, struct sk_buff *skb)
{
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;
	int ret = NET_RX_DROP;

	ndev = idev->pndev[idx];
	if (ndev == NULL) {
		ret = -ENODEV;
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}

	skb->dev = ndev;
	skb->protocol = eth_type_trans(skb, ndev);
	skb->ip_summed = CHECKSUM_NONE;
	ret = netif_rx(skb);

func_exit:
	return ret;
}

uint32_t *inic_netif_get_ip_gateway(int idx)
{
	int ret = 0;
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;

	ndev = idev->pndev[idx];
	if (ndev == NULL) {
		ret = -ENODEV;
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}

func_exit:
	return &inic_ip_gw[idx];
}

uint32_t *inic_netif_get_ip_mask(int idx)
{
	int ret = 0;
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;
	struct in_ifaddr *ifa = NULL;

	ndev = idev->pndev[idx];
	if (ndev == NULL) {
		ret = -ENODEV;
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}

	rcu_read_lock();
	in_dev_for_each_ifa_rcu(ifa, ndev->ip_ptr)
		memcpy(&inic_ip_mask[idx], &ifa->ifa_mask, 4);
	rcu_read_unlock();

func_exit:
	return &inic_ip_mask[idx];
}

uint32_t *inic_netif_get_ip_address(int idx)
{
	int ret = 0;
	inic_dev_t *idev = local_idev;
	struct net_device *ndev = NULL;
	struct in_ifaddr *ifa = NULL;

	ndev = idev->pndev[idx];
	if (ndev == NULL) {
		ret = -ENODEV;
		printk("%s: Error priv is NULL!\n", __func__);
		goto func_exit;
	}

	rcu_read_lock();
	in_dev_for_each_ifa_rcu(ifa, ndev->ip_ptr)
		memcpy(&inic_ip_addr[idx], &ifa->ifa_address, 4);
	rcu_read_unlock();

func_exit:
	return &inic_ip_addr[idx];
}

/**
 * @brief  to allocate inic_device and initialize all net device and inic port.
 * @return return the allocated and initialized inic_device.
 */
inic_dev_t *inic_init_device(struct device *pdev, int port_num)
{
	inic_dev_t *idev = NULL;
	struct net_device *ndev = NULL;
	inic_port_t *iport = NULL;
	int i = 0, ret = 0;

	if (port_num > INIC_MAX_NET_PORT_NUM) {
		printk("%s: net device number is exceeded. need(%d), provide(%d).\n",\
			__FUNCTION__, port_num, INIC_MAX_NET_PORT_NUM);
		goto func_exit;
	}

	if (!pdev) {
		ret = -ENODEV;
		printk(KERN_ERR "%s: ipc device is null (%d).\n", \
		       __func__, ret);
		goto func_exit;
	}


	idev = kzalloc(sizeof(inic_dev_t), GFP_KERNEL);
	if (!idev) {
		printk("%s: Allocate inic_port error!\n", __FUNCTION__);
		goto func_exit;
	}

	/* initialize the lock */
	spin_lock_init(&idev->lock);
	/* initialize the port list in inic device */
	INIT_LIST_HEAD(&idev->port_list);
	/* assigne idev to private inic device poitner */
	local_idev = idev;

	/* allocated net device and regist it */
	for (i = 0; i < port_num; i++) {
		ndev = inic_alloc_netdevice(idev, i);
		if (!ndev) {
			ret = -ENOMEM;
			printk("%s: Allocate netdevice error!\n", __FUNCTION__);
			break;
		}

		netif_carrier_off(ndev);
		ret = register_netdev(ndev);
		if (ret != 0) {
			printk(KERN_ERR "%s: regist ipc channel err(%d).\n", \
			       __FUNCTION__, ret);
			break;
		}
		idev->pndev[i] = ndev;
		SET_NETDEV_DEV(ndev, pdev);
	}
	idev->port_num = port_num;
	idev->mode = RTW_MODE_NONE;

	if (ret == 0)
		goto func_exit;

	/* allocate netdevice failed, free resource. */
	if (idev && !list_empty(&idev->port_list)) {
		list_for_each_entry(iport, &idev->port_list, list)
			inic_free_netdevice(iport->ndev);
		kfree(idev);
		idev = NULL;
	}

func_exit:
	return idev;
}

/**
 * @brief  to free inic_device and deinitialize all net device and inic port.
 * @return return the allocated and initialized inic_device.
 */
void inic_deinit_device(inic_dev_t *idev)
{
	inic_port_t *iport = NULL;

	local_idev = NULL;
	if (idev && !list_empty(&idev->port_list)) {
		list_for_each_entry(iport, &idev->port_list, list)
			inic_free_netdevice(iport->ndev);
		kfree(idev);
	}
}
