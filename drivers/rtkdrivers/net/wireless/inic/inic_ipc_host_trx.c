/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __INIC_IPC_HOST_TRX_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */
#include "inic_ipc_host_trx.h"

/* -------------------------------- Defines --------------------------------- */
#define MAX_LENGTH_OF_TX_QUEUE (200)
#define MAX_TIMES_TO_TRY_TX (5)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
/* recv buffer to store the data from IPC to queue. */
typedef struct host_qbuf {
	struct list_head list;
	int idx_wlan; /* index for wlan */
	struct sk_buff *pskb; /* rx data for ethernet buffer*/
} host_qbuf_t;

typedef struct host_list {
	struct list_head list;
	spinlock_t lock; /* queue lock */
	int len; /* list length */
} host_list_t;

/* recv structure */
typedef struct host_priv {
	struct work_struct recv_work; /* Rx work */
	struct work_struct xmit_work; /* Tx work */
	struct completion xmit_done; /* sema to ensure tx done */
	struct completion alloc_skb_done; /* sema to wait allloc skb from device */
	struct mutex send_lock; /* tx task lock*/
	struct mutex recv_lock; /* rx task lock*/
	host_list_t recv_list; /* recv queue list */
	host_list_t xmit_list; /* recv queue list */
	struct dev_sk_buff *allocated_tx_skb; /* to store allocated tx skb */
	struct device *pdev; /* lower layer device */
	inic_dev_t *idev; /* lower layer device */
	bool rx_pending_flag; /* host rx pending flag */
} host_priv_t;

struct skb_data {
	/* starting address must be aligned by 32 bytes for km4 cache. */
	struct list_head list __attribute__((aligned(32)));
	unsigned char buf[1658];
	/* to protect ref when to invalid cache, its address must be
	 * aligned by 32 bytes. */
	atomic_t ref __attribute__((aligned(32)));
};

struct skb_buf {
	struct list_head list;
	struct dev_sk_buff skb;
	u8 rsvd[16];
};

/* -------------------------- Function declaration -------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* --------------------------- Private Variables ---------------------------- */
static host_priv_t g_inic_host_priv;

/* ---------------------------- Private Functions --------------------------- */

unsigned char *dev_skb_put(struct dev_sk_buff *skb, unsigned int len)
{
	unsigned char *tmp = skb->tail;
	skb->tail += len;
	skb->len += len;
	if (unlikely(skb->tail > skb->end))
		panic("%s: skb %p, len %d, data %p, end %p, %p.\n",\
		      __func__, skb, len, skb->data, skb->end,\
		      __builtin_return_address(0));

	return tmp;
}

/******************************************************************************
 *
 * Function Name: inic_enqueue_buf
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	enqueue the precvbuf into the queue.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *	host_qbuf_t *precvbuf - the recv buffer enqueued into the queue.
 *	_queue *queue - the recv queue.
 *
 *-----------------------------------------------------------------------------
 * Returns:
 *	sint - the result.
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
static int inic_enqueue_buf(host_qbuf_t *precvbuf, host_list_t *queue)
{
	spin_lock_bh(&(queue->lock));
	list_add_tail(&(precvbuf->list), &(queue->list));
	queue->len++;
	spin_unlock_bh(&(queue->lock));

	return 0;
}

/******************************************************************************
 *
 * Function Name: inic_dequeue_buf
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	get the recv buffer from the recv queue.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *	_queue *queue - the recv queue.
 *
 *-----------------------------------------------------------------------------
 * Returns:
 *	host_qbuf_t * - recv buffer. if is NULL, the queue is empty.
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
static host_qbuf_t *inic_dequeue_buf(host_list_t *queue)
{
	host_qbuf_t *buf;
	struct list_head *plist, *phead;

	spin_lock_bh(&(queue->lock));

	if (list_empty(&(queue->list))) {
		buf = NULL;
	} else {
		phead = &(queue->list);
		plist = phead->next;
		buf = list_entry(plist, host_qbuf_t, list);
		list_del(&(buf->list));
		queue->len--;
	}

	spin_unlock_bh(&(queue->lock));

	return buf;
}

/******************************************************************************
 *
 * Function Name: inic_ipc_host_recv_task
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	rx task to handle the rx data, get the data from the rx queue and send
 *	to upper layer.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *
 *-----------------------------------------------------------------------------
 * Returns:
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
static void inic_ipc_host_recv_task(struct work_struct *data)
{
	host_priv_t *priv = container_of(data, host_priv_t, recv_work);
	host_qbuf_t *precvbuf = NULL;
	struct sk_buff *pskb = NULL;
	inic_port_t *piport = NULL;
	int index = 0;
	inic_ipc_ex_msg_t ipc_msg = {0};
	int ret = 0;

	if (!priv->idev) {
		printk(KERN_ERR "%s: inic device is NULL!\n",\
		       __func__);
		return;
	}

	mutex_lock(&(priv->recv_lock));
	precvbuf = inic_dequeue_buf(&(priv->recv_list));
	while (precvbuf) {
		pskb = precvbuf->pskb;
		index = precvbuf->idx_wlan;

		piport = priv->idev->piport[index];
		if (!piport) {
			printk(KERN_ERR "%s: no iport!\n",\
			       __func__);
			kfree(precvbuf);
			return;
		}

		ret = inic_netif_rx_by_idx(index, pskb);
		if (ret == NET_RX_SUCCESS) {
			piport->stats.rx_packets++;
			piport->stats.rx_bytes += pskb->len;
		} else {
			piport->stats.rx_dropped++;
		}

		/* release the memory for this packet. */
		kfree(precvbuf);
		precvbuf = inic_dequeue_buf(&(priv->recv_list));
	}

	if (g_inic_host_priv.rx_pending_flag) {
		ipc_msg.event_num = IPC_WIFI_MSG_RECV_DONE;
		inic_ipc_ipc_send_msg(&ipc_msg);
	}
	mutex_unlock(&(priv->recv_lock));
}

/******************************************************************************
 *
 * Function Name: inic_ipc_host_alloc_skb
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	to apply for the skb buffer to send the data from device.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 * 	unsigned int total_len - the length to require for sending from device.
 *
 *-----------------------------------------------------------------------------
 * Returns:
 * 	struct sk_buff * - address of skb from device
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
static struct dev_sk_buff * inic_ipc_host_alloc_skb(unsigned int total_len, int idx)
{
	struct dev_sk_buff *skb = NULL;
	inic_ipc_ex_msg_t ipc_msg = {0};
	int try_tx = 0;

	while ((skb == NULL) && (try_tx < MAX_TIMES_TO_TRY_TX)) {
		ipc_msg.event_num = IPC_WIFI_MSG_ALLOC_SKB;
		ipc_msg.msg_addr = idx;
		ipc_msg.msg_len = total_len;
		inic_ipc_ipc_send_msg(&ipc_msg);
		if (wait_for_completion_timeout(&(g_inic_host_priv.alloc_skb_done), msecs_to_jiffies(50)) == 0) {
			g_inic_host_priv.allocated_tx_skb = NULL;
			skb = NULL;
			break;
		}
		skb = g_inic_host_priv.allocated_tx_skb;
		g_inic_host_priv.allocated_tx_skb = NULL;

		try_tx++;
	}

	return skb;
}

/******************************************************************************
 *
 * Function Name: inic_ipc_host_send_skb
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	to send skb to device for port idx.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 * 	int idx - which port of wifi to tx.
 * 	struct sk_buff *skb - skb to send.
 *
 *-----------------------------------------------------------------------------
 * Returns:
 * 	int - -1 failed, 0 seccessful.
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
static int inic_ipc_host_send_skb(int idx, struct dev_sk_buff *skb)
{
	inic_ipc_ex_msg_t ipc_msg = {0};

	if(idx == -1){
		printk("%s=>wlan index is wrong!\n\r", __func__);
		return -1;
	}

	ipc_msg.event_num = IPC_WIFI_CMD_XIMT_PKTS;
	ipc_msg.msg_addr = (u32)skb;
	ipc_msg.msg_len = idx;
	inic_ipc_ipc_send_msg(&ipc_msg);

	/* wait for device tx done */
	if (wait_for_completion_timeout(&g_inic_host_priv.xmit_done, msecs_to_jiffies(40))) {
		return 0;
	} else {
		/* wait timeout, drop this skb */
		printk("%s=> tx timeout\n", __func__);
		return -1;
	}
}

static int inic_ipc_host_send_hdl(int idx, struct sk_buff *pskb)
{
	struct dev_sk_buff *skb = NULL, *skb_phy = NULL;
	int ret = 0;
	u16 queue;
	struct device *pdev = g_inic_host_priv.pdev;
	struct net_device *pndev = g_inic_host_priv.idev->pndev[idx];
	inic_port_t *piport = NULL;
	u8 *data_addr = NULL;
	u8 b_dropped = 0;

	if (!pdev) {
		printk(KERN_ERR "%s: net device is NULL!\n",\
		       __func__);
		ret = -ENODEV;
		goto func_exit;
	}
	piport = netdev_priv(pndev);

	/* allocate the skb buffer to send from device */
	skb_phy = inic_ipc_host_alloc_skb(pskb->len, idx);
	if ((u32)skb_phy == FLAG_WLAN_IF_NOT_RUNNING) {
		printk("device wlan port is not running.\n");
		ret = -EIO;
		goto drop_packet;
	} else if (skb_phy == NULL) {
		ret = -EBUSY;
		goto drop_packet;
	}

	skb = memremap((resource_size_t)skb_phy, sizeof(struct dev_sk_buff), MEMREMAP_WT);
	if (!skb) {
		printk("%s: cannot remap skb memory %p @ 0x%x", \
		       __func__, skb_phy, sizeof(struct dev_sk_buff));
		ret = -ENOMEM;
		goto drop_packet;
	}

	data_addr = memremap((resource_size_t)skb->data, pskb->len, MEMREMAP_WT);
	if (!data_addr) {
		printk("%s: cannot remap skb data memory %p @ 0x%x", \
		       __func__, skb->data, pskb->len);
		ret = -ENOMEM;
		memunmap(skb);
		goto drop_packet;
	}

	memcpy(data_addr, pskb->data, pskb->len);
	dev_skb_put(skb, pskb->len);

	memunmap(data_addr);
	memunmap(skb);

	if (inic_ipc_host_send_skb(idx, skb_phy) == 0) {
		piport->stats.tx_packets++;
		piport->stats.tx_bytes += pskb->len;
		goto func_exit;
	} else {
		ret = -EIO;
	}

drop_packet:
	b_dropped = 1;
	piport->stats.tx_dropped++;

func_exit:
	skb_tx_timestamp(pskb);

	queue = skb_get_queue_mapping(pskb);
	if(__netif_subqueue_stopped(pndev, queue))
		netif_wake_subqueue(pndev, queue);
	if (b_dropped)
		dev_kfree_skb_any(pskb);
	else
		dev_kfree_skb(pskb);

	return ret;
}

static void inic_ipc_host_send_task(struct work_struct *data)
{
	host_priv_t *priv = container_of(data, host_priv_t, xmit_work);
	host_qbuf_t *pxmitbuf = NULL;
	struct sk_buff *pskb = NULL;
	int index = 0;

	mutex_lock(&(g_inic_host_priv.send_lock));
	pxmitbuf = inic_dequeue_buf(&(priv->xmit_list));
	while (pxmitbuf) {
		pskb = pxmitbuf->pskb;
		index = pxmitbuf->idx_wlan;
		inic_ipc_host_send_hdl(index, pskb);

		/* release the memory for this packet. */
		kfree(pxmitbuf);
		pxmitbuf = inic_dequeue_buf(&(priv->xmit_list));
	}
	mutex_unlock(&(g_inic_host_priv.send_lock));
}

/* ---------------------------- Public Functions ---------------------------- */

/******************************************************************************
 *
 * Function Name: inic_ipc_host_init_priv
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	initialize the parameters of recv.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *
 *-----------------------------------------------------------------------------
 * Returns:
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
void inic_ipc_host_init_priv(inic_dev_t *idev)
{
	int i = 0;

	if (idev) {
		g_inic_host_priv.idev = idev;
	} else {
		printk("%s, init ipc_host fault, inic device is NULL!\n", __func__);
		return;
	}

	if (idev->trx_ch && idev->trx_ch->pdev) {
		g_inic_host_priv.pdev = idev->trx_ch->pdev;
	} else {
		printk("%s, init ipc_host fault, device is NULL!\n", __func__);
		return;
	}

	/* initialize the sema of tx done. */
	init_completion(&g_inic_host_priv.xmit_done);

	/* initialize the sema for tx alloc skb. */
	init_completion(&g_inic_host_priv.alloc_skb_done);

	/* initialize the Rx queue. */
	spin_lock_init(&(g_inic_host_priv.recv_list.lock));
	INIT_LIST_HEAD(&(g_inic_host_priv.recv_list.list));
	g_inic_host_priv.recv_list.len = 0;
	mutex_init(&(g_inic_host_priv.recv_lock));

	spin_lock_init(&(g_inic_host_priv.xmit_list.lock));
	INIT_LIST_HEAD(&(g_inic_host_priv.xmit_list.list));
	g_inic_host_priv.xmit_list.len = 0;
	mutex_init(&(g_inic_host_priv.send_lock));
	g_inic_host_priv.allocated_tx_skb = NULL;
	for (i = 0; i < idev->port_num; i++) {
		idev->pndev[i]->tx_queue_len = MAX_LENGTH_OF_TX_QUEUE;
	}

	g_inic_host_priv.rx_pending_flag = 0;

	/* Initialize the RX work */
	INIT_WORK(&(g_inic_host_priv.recv_work), inic_ipc_host_recv_task);
	INIT_WORK(&(g_inic_host_priv.xmit_work), inic_ipc_host_send_task);
}

void inic_ipc_host_deinit_priv(void)
{
	/* deinitialize the sema of tx done. */
	mutex_destroy(&(g_inic_host_priv.send_lock));
	mutex_destroy(&(g_inic_host_priv.recv_lock));

	complete_release(&g_inic_host_priv.alloc_skb_done);
	complete_release(&g_inic_host_priv.xmit_done);
}

/******************************************************************************
 *
 * Function Name: inic_ipc_host_rx_handler
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	put the Rx data from rx buffer into Rx queue.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *	int idx_wlan - which port of wifi to rx.
 *	struct sk_buff *skb - data from the ipc bus, its structure is sk_buff.
 *
 *-----------------------------------------------------------------------------
 * Returns:
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
void inic_ipc_host_rx_handler(int idx_wlan, struct dev_sk_buff *skb_phy)
{
	host_priv_t *priv = &g_inic_host_priv;
	host_qbuf_t *precvbuf = NULL;
	struct dev_sk_buff *skb = NULL;
	struct sk_buff *pskb = NULL;
	inic_ipc_ex_msg_t ipc_msg = {0};
	struct device *pdev = priv->pdev;
	inic_dev_t *idev = priv->idev;
	u8 *data_addr = NULL;

	/* get the rx queue. */
	if (!pdev || !idev) {
		printk(KERN_ERR "%s: device or inic device is NULL!\n",\
		       __func__);
		goto func_exit;
	}

	if (!skb_phy) {
		printk("%s: skb_phy is NULL", __func__);
		goto func_exit;
	}

	skb = memremap((resource_size_t)skb_phy, sizeof(struct dev_sk_buff), MEMREMAP_WT);
	if (!skb) {
		printk("%s: cannot remap skb memory %p @ 0x%x", \
		       __func__, skb_phy, sizeof(struct dev_sk_buff));
		goto func_exit;
	}

	data_addr = memremap((resource_size_t)skb->data, skb->len, MEMREMAP_WT);
	if (!data_addr) {
		printk("%s: cannot remap data memory %p @ 0x%x", \
		       __func__, skb->data, skb->len);
		memunmap(skb);
		goto func_exit;
	}

	/* allocate pbuf to store ethernet data from IPC. */
	pskb = netdev_alloc_skb(idev->pndev[idx_wlan], skb->len);
	if (pskb == NULL) {
		printk("%s: Alloc skb rx buf Err, alloc_sz %d!!\n\r",
		       __func__, skb->len);
		priv->rx_pending_flag = 1;
		ipc_msg.event_num = IPC_WIFI_MSG_RECV_PENDING;
		inic_ipc_ipc_send_msg(&ipc_msg);
		/* wakeup recv work */
		memunmap(data_addr);
		memunmap(skb);
		schedule_work(&(priv->recv_work));
		goto func_exit;
	}

	/* cpoy data from skb(ipc data) to local skb */
	memcpy(pskb->data, data_addr, skb->len);
	skb_put(pskb, skb->len);
	memunmap(data_addr);
	memunmap(skb);

	/* allocate host_qbuf and associate to the p_buf */
	precvbuf = kmalloc(sizeof(host_qbuf_t), GFP_KERNEL);
	precvbuf->pskb = pskb;
	precvbuf->idx_wlan = idx_wlan;

	/* enqueue recv buffer  */
	inic_enqueue_buf(precvbuf, &(priv->recv_list));

	priv->rx_pending_flag = 0;
	ipc_msg.event_num = IPC_WIFI_MSG_RECV_DONE;
	inic_ipc_ipc_send_msg(&ipc_msg);
	/* wakeup recv work */
	if (!work_pending(&(priv->recv_work)))
		schedule_work(&(priv->recv_work));

func_exit:
	return;
}

/******************************************************************************
 *
 * Function Name: inic_ipc_host_tx_alloc_skb_rsp
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	assigne the skb from device and wake up the alloc_skb_done.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *	inic_ipc_ex_msg *p_ipc_msg - ipc message from ipc device response. The
 *		skb address is stored in msg_addr.
 *
 *-----------------------------------------------------------------------------
 * Returns:
 * 	None
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
void inic_ipc_host_tx_alloc_skb_rsp(inic_ipc_ex_msg_t *p_ipc_msg)
{
	g_inic_host_priv.allocated_tx_skb = (struct dev_sk_buff *)p_ipc_msg->msg_addr;
	complete(&(g_inic_host_priv.alloc_skb_done));
}

/******************************************************************************
 *
 * Function Name: inic_ipc_host_tx_done
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	Notice the tx is done in device port. It will wakeup
 *	inic_ipc_host_send_skb.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *	None
 *
 *-----------------------------------------------------------------------------
 * Returns:
 *	None
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
void inic_ipc_host_tx_done(void)
{
	complete(&g_inic_host_priv.xmit_done);
}

/******************************************************************************
 *
 * Function Name: inic_ipc_host_send
 *
 *-----------------------------------------------------------------------------
 * Summary:
 *	put the Rx data from rx buffer into Rx queue.
 *
 *-----------------------------------------------------------------------------
 * Parameters:
 *	int idx - which port of wifi to tx.
 *	struct sk_buff *skb - skb buffer to send
 *
 *-----------------------------------------------------------------------------
 * Returns:
 * 	result
 *
 *-----------------------------------------------------------------------------
 * Detail:
 *	None
 *
 ******************************************************************************/
int inic_ipc_host_send(int idx, struct sk_buff *skb)
{
	host_qbuf_t *pxmitbuf = NULL;
	struct net_device *pndev = g_inic_host_priv.idev->pndev[idx];
	u16 queue;
	int ret = 0;

	if (skb == NULL) {
		printk("%s: xmit buf is NULL\n", __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (pndev == NULL) {
		printk("%s: net device is NULL\n", __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (g_inic_host_priv.xmit_list.len >= MAX_LENGTH_OF_TX_QUEUE) {
		ret = -EBUSY;
		goto func_exit;
	}

	/* allocate the skb buffer to send from device */
	pxmitbuf = kmalloc(sizeof(host_qbuf_t), GFP_KERNEL);
	if (pxmitbuf == NULL) {
		printk("%s: no resource to allocate pxmitbuf.\n", __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	pxmitbuf->pskb = skb;
	pxmitbuf->idx_wlan = idx;

	queue = skb_get_queue_mapping(skb);
	netif_stop_subqueue(pndev, queue);
	/* enqueue recv buffer  */
	inic_enqueue_buf(pxmitbuf, &(g_inic_host_priv.xmit_list));

	if (!work_pending(&(g_inic_host_priv.xmit_work)))
		schedule_work(&(g_inic_host_priv.xmit_work));

func_exit:
	return ret;
}
