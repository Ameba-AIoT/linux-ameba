/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __INIC_IPC_MSG_QUEUE_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */
#include "inic_ipc_msg_queue.h"

/* -------------------------------- Defines --------------------------------- */
#define IPC_MSG_QUEUE_DEPTH (10)
#define IPC_MSG_QUEUE_WARNING_DEPTH (4)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
/* node to store the message to the queue. */
typedef struct ipc_msg_node {
	struct list_head list;
	inic_ipc_ex_msg_t ipc_msg; /* to store ipc message */
	bool is_used; /* sign whether to be used */
} ipc_msg_node_t;

/* message queue priv */
typedef struct ipc_msg_q_priv {
	struct list_head queue_head; /* msg queue */
	spinlock_t lock; /* queue lock */
	struct work_struct msg_work; /* message task in linux */
	struct mutex msg_work_lock; /* tx lock lock */
	void (*task_hdl)(inic_ipc_ex_msg_t *); /* the haddle function of task */
	bool b_queue_working; /* flag to notice the queue is working */
	ipc_msg_node_t ipc_msg_pool[IPC_MSG_QUEUE_DEPTH]; /* static pool for queue node */
	int queue_free; /* the free size of queue */
	struct device *pdev; /* lower layer device */
	inic_ipc_ex_msg_t *p_inic_ipc_msg;/* host inic ipc message to send to device */
	dma_addr_t ipc_msg_phy_addr;/* host inic message to send to device */
} ipc_msg_q_priv_t;

/* -------------------------- Function declaration -------------------------- */
int inic_ipc_channel_send(ipc_msg_struct_t *pmsg);

/* ---------------------------- Global Variables ---------------------------- */

/* --------------------------- Private Variables ---------------------------- */
static ipc_msg_q_priv_t g_ipc_msg_q_priv = {0};
static spinlock_t g_inic_ipc_msg_lock;

/* ---------------------------- Private Functions --------------------------- */
/******************************************************************************
 * @brief  put the ipc message to queue.
 * @param  p_node[in]: the pointer for the ipc message node that need to be
 * 	pushed into the queue.
 * @param  p_queue[in]: the queue used to store the p_node.
 * @return status, always true.
 ******************************************************************************/
static int enqueue_ipc_msg_node(ipc_msg_q_priv_t *priv, ipc_msg_node_t *p_node)
{
	spin_lock(&(priv->lock));
	list_add_tail(&(p_node->list), &(priv->queue_head));
	spin_unlock(&(priv->lock));

	return 0;
}

/******************************************************************************
 * @brief  get the ipc message from queue.
 * @param  p_ipc_msg[in]: the queue used to store the p_node.
 * @return the ipc_msg_node got from message queue.
 ******************************************************************************/
static ipc_msg_node_t *dequeue_ipc_msg_node(ipc_msg_q_priv_t *priv)
{
	ipc_msg_node_t *p_node;
	struct list_head *plist, *phead;

	/* stop ipc interrupt interrupting this process to cause dead lock. */
	spin_lock_irq(&(priv->lock));

	if (list_empty(&(priv->queue_head)) == true) {
		p_node = NULL;
	} else {
		phead = &(priv->queue_head);
		plist = phead->next;
		p_node = list_entry(plist, ipc_msg_node_t, list);
		list_del(&(p_node->list));
	}

	spin_unlock_irq(&(priv->lock));

	return p_node;
}

/******************************************************************************
 * @brief  task to operation the queue when the queue is not empty.
 * @param  none
 * @return none
 ******************************************************************************/
static void inic_ipc_msg_q_task(struct work_struct *data) {
	ipc_msg_q_priv_t *priv = container_of(data, ipc_msg_q_priv_t, msg_work);
	ipc_msg_node_t *p_node = NULL;

	mutex_lock(&(priv->msg_work_lock));
	if (priv->b_queue_working) {

		/* get the data from tx queue. */
		p_node = dequeue_ipc_msg_node(priv);
		while (p_node) {
			/* haddle the message */
			if (priv->task_hdl)
				priv->task_hdl(&(p_node->ipc_msg));

			/* release the memory for this ipc message. */
			p_node->is_used = 0;
			priv->queue_free++;

			p_node = dequeue_ipc_msg_node(priv);
		}
	}
	mutex_unlock(&(priv->msg_work_lock));
}

/* ---------------------------- Public Functions ---------------------------- */
/******************************************************************************
 * @brief  to initialize the message queue.
 * @param  task_func[in]: the pointer to the task function to operate this
 * 	queue.
 * @return none
 ******************************************************************************/
void inic_ipc_msg_q_init(struct device *pdev, void (*task_hdl)(inic_ipc_ex_msg_t *))
{
	int i = 0;
	ipc_msg_q_priv_t *imsg = &g_ipc_msg_q_priv;

	/* initialize queue. */
	spin_lock_init(&imsg->lock);
	spin_lock_init(&g_inic_ipc_msg_lock);
	INIT_LIST_HEAD(&imsg->queue_head);

	if (pdev) {
		imsg->pdev = pdev;
	} else {
		printk("%s, init msg queue fault, device is NULL!\n", __func__);
		return;
	}

	imsg->p_inic_ipc_msg = dmam_alloc_coherent(pdev, sizeof(inic_ipc_ex_msg_t),\
					    &imsg->ipc_msg_phy_addr, GFP_KERNEL);
	if (!imsg->p_inic_ipc_msg) {
		printk("%s: allloc p_inic_ipc_msg error.\n", \
		       __func__);
		return;
	}

	/* assign the haddle function for the task */
	imsg->task_hdl = task_hdl;

	for (i = 0; i < IPC_MSG_QUEUE_DEPTH; i++) {
		imsg->ipc_msg_pool[i].is_used = 0;
	}
	imsg->queue_free = IPC_MSG_QUEUE_DEPTH;

	mutex_init(&(imsg->msg_work_lock));
	/* initialize message tasklet */
	INIT_WORK(&(imsg->msg_work), inic_ipc_msg_q_task);

	/* sign the queue is working */
	imsg->b_queue_working = 1;
}

/******************************************************************************
 * @brief  put the ipc message to queue.
 * @param  p_node[in]: the pointer for the ipc message that need to be
 * 	pushed into the queue.
 * @return status, always true.
 ******************************************************************************/
int inic_ipc_msg_enqueue(inic_ipc_ex_msg_t *p_ipc_msg)
{
	ipc_msg_q_priv_t *priv = &g_ipc_msg_q_priv;
	ipc_msg_node_t *p_node = NULL;
	int ret = 0, i = 0;

	/* allocate memory for message node */
	for (i = 0; i < IPC_MSG_QUEUE_DEPTH; i++) {
		if (priv->ipc_msg_pool[i].is_used == 0) {
			p_node = &(priv->ipc_msg_pool[i]);
			/* a node is used, the free node will decrease */
			priv->queue_free--;
		}
	}

	if (p_node == NULL) {
		ret = -ENOMEM;
		printk("NO buffer for new nodes, waiting!\n\r");
		goto func_out;
	}


	/* To store the ipc message to queue's node. */
	p_node->ipc_msg.event_num = p_ipc_msg->event_num;
	p_node->ipc_msg.msg_addr = p_ipc_msg->msg_addr;
	p_node->ipc_msg.msg_len = p_ipc_msg->msg_len;
	p_node->is_used = 1;

	/* put the ipc message to the queue */
	ret = enqueue_ipc_msg_node(priv, p_node);

	/* the free number of nodes is smaller than the warning depth. */
	if (priv->queue_free <= IPC_MSG_QUEUE_WARNING_DEPTH) {
		/* ask peer to wait */
		ret = -EBUSY;
	}

func_out:
	/* wakeup task */
	schedule_work(&(priv->msg_work));

	return ret;
}

/******************************************************************************
 * @brief  to deinitialize the message queue.
 * @param  none.
 * @return none
 ******************************************************************************/
void inic_ipc_msg_q_deinit(void)
{
	ipc_msg_q_priv_t *imsg = &g_ipc_msg_q_priv;

	/* sign the queue is stop */
	imsg->b_queue_working = 0;

	/* assign the haddle function to NULL */
	imsg->task_hdl = NULL;

	/* free sema to wakeup the message queue task */

	dma_free_coherent(imsg->pdev, sizeof(inic_ipc_ex_msg_t),\
			  imsg->p_inic_ipc_msg, imsg->ipc_msg_phy_addr);

	/* de initialize queue, todo */
	mutex_destroy(&(imsg->msg_work_lock));
}

/******************************************************************************
 * @brief  to get the status of queue, working or stop.
 * @param  none.
 * @return the status of queue, 1 means working, 0 means stop.
 ******************************************************************************/
u8 inic_ipc_msg_get_queue_status(void)
{
	return g_ipc_msg_q_priv.b_queue_working;
}

/******************************************************************************
 * @brief  to send the ipc message. It will wait when the last message is not
 * 	read.
 * @param  p_ipc_msg[inout]: the message to send.
 * @return none.
 ******************************************************************************/
void inic_ipc_ipc_send_msg(inic_ipc_ex_msg_t *p_ipc_msg)
{
	ipc_msg_q_priv_t *imsg = &g_ipc_msg_q_priv;
	inic_ipc_ex_msg_t *pmsg = imsg->p_inic_ipc_msg;
	ipc_msg_struct_t ipc_msg = {0};
	int try_cnt = 100;

	if (!pmsg) {
		printk("%s: p_inic_ipc_msg in inic msg is NULL!\n", \
		       __func__);
		return;
	}
	/* Get the warning of queue's depth not enough in peer, delay send the
	 * the next message.
	 */
	if (p_ipc_msg->msg_len == IPC_WIFI_MSG_MEMORY_NOT_ENOUGH)
		mdelay(5);

	spin_lock_bh(&g_inic_ipc_msg_lock);

	/* Wait for another port ack acknowledgement last message sending */
	while (pmsg->event_num != IPC_WIFI_MSG_READ_DONE) {
		udelay(1);
		try_cnt--;
		if (try_cnt <= 0) {
			break;
		}
	}

	pmsg->event_num = p_ipc_msg->event_num;
	pmsg->msg_addr = p_ipc_msg->msg_addr;
	pmsg->msg_len = p_ipc_msg->msg_len;

	/* Send the new message after last one acknowledgement */
	ipc_msg.msg_type = IPC_USER_POINT;
	ipc_msg.msg = imsg->ipc_msg_phy_addr;
	ipc_msg.msg_len = sizeof(inic_ipc_ex_msg_t);

	inic_ipc_channel_send(&ipc_msg);

	spin_unlock_bh(&g_inic_ipc_msg_lock);
}
