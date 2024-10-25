// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#define __AMEBA_IPC_C__

/* -------------------------------- Includes -------------------------------- */
/* External head files */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/kern_levels.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/pm_wakeirq.h>

/* Internal head files */
#include <ameba_ipc/ameba_ipc.h>
#include "ameba_ipc_reg.h"

/* -------------------------------- Defines --------------------------------- */
#define MAX_NUM_AIPC_PORT (2)

/**
 * The port and channel memory mapping is below:
 * |<--8-->|<--8-->|<--8-->|<--8-->|<--8-->|<--8-->|
 * | AP2NP | AP2LP | NP2AP | NP2LP | LP2AP | LP2NP |
 * The unit is not byte, and is the size of ipc_msg_struct_t.
 */
#define TOT_MEM_SZIE (3 * sizeof(ipc_msg_struct_t) * AIPC_CH_MAX_NUM \
		      * MAX_NUM_AIPC_PORT)

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
struct aipc_ch_node;

/*
 * Structure to describe the ipc device of ameba.
 */
struct aipc_device {
	spinlock_t lock; /* list lock */
	struct device *dev; /* parent device */
	struct aipc_port *pnp_port; /* pointer to NP port */
	struct aipc_port *plp_port; /* pointer to LP port */
	ipc_msg_struct_t *dev_mem; /* LP shared SRAM for IPC */
	u32 *preg_tx_data; /* mapping address of AIPC_REG_TX_DATA */
	u32 *preg_rx_data; /* mapping address of AIPC_REG_RX_DATA */
	u32 *preg_isr; /* mapping address of AIPC_REG_ISR */
	u32 *preg_imr; /* mapping address of AIPC_REG_IMR */
	u32 *preg_icr; /* mapping address of AIPC_REG_ICR */
	u32 irq; /* irq number */
	unsigned long irqflags; /* irq flags */
};

/*
 * Structure to describe the ipc port of ameba. There may be tow ports in
 * ambebad2: from NP and from LP. But in mode case, there is only from NP.
 */
struct aipc_port {
	struct list_head ch_list; /* channel list */
	struct aipc_device *dev; /* parent device */
	u32 port_id; /* port id for NP or LP */
	const char *name; /* port name */
	spinlock_t lock; /* port lock */
	u32 free_chnl_num; /* free channel number in port */
};

/*
 * Structure to describe the ipc channel. There are 8 channels in one port.
 */
struct aipc_ch_node {
	struct list_head list; /* list to add the channel list */
	struct aipc_port *port; /* pointer to ipc port */
	ipc_msg_struct_t *ch_rmem; /* channel read shared SRAM for IPC */
	ipc_msg_struct_t *ch_wmem; /* channel written shared SRAM for IPC */
	struct aipc_ch *ch; /* customer regisiting channel */
};

/* -------------------------- Function declaration -------------------------- */
static int ameba_ipc_probe(struct platform_device *pdev);
static int ameba_ipc_remove(struct platform_device *pdev);


/* ---------------------------- Global Variables ---------------------------- */
/* Define the name of ipc port */
const char NAME_OF_NP_PORT[] = "ipc np port";
const char NAME_OF_LP_PORT[] = "ipc lp port";

/* --------------------------- Private Variables ---------------------------- */
/* Set the compatible for driver to match the dtb */
static const struct of_device_id of_aipc_device_match[] = {
	{.compatible = "realtek,aipc"},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, of_aipc_device_match);

/* Set the platform_driver to associate the driver probe and remove */
static struct platform_driver dameba_ipc_driver = {
	.probe  = ameba_ipc_probe,
	.remove = ameba_ipc_remove,
	.driver = {
		.name = "realtek-aipc",
		.owner = THIS_MODULE,
		.of_match_table = of_aipc_device_match,
	},
};

/* Local global pointer to describe the ipc port list */
static struct aipc_device *pipc_dev = NULL;

/* --------------------------- Private Functions ---------------------------- */
/**
 * @brief  send work queue haddle function to send message.
 * @param  data[in]: work queue data.
 * @return NULL.
 */
static int ameba_ipc_send_work(struct aipc_ch_node *chn, ipc_msg_struct_t *msg)
{
	struct aipc_ch *ch = chn->ch;
	struct aipc_device *pdev = chn->port->dev;
	u32 reg_tx_data = 0;
	int ret = 0;

	reg_tx_data = readl(pdev->preg_tx_data);
	/**
	 * read TX_DATA register to judge whether tx is busy.
	 * structure of ISP (unit bit):
	 * |<------8------>|<------8------>|
	 * |-----NP_TX-----|-----LP_TX-----|
	 * |7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|
	 * If bit of relactive channel is 1, It means the last tx is not
	 * completed. It associated to the RX data of peer. When the bit of
	 * peer RX is clean, this bit will be clean. If to tx, need to set the
	 * bit to 1.
	 */
	if (ch->port_id == AIPC_PORT_LP) {
		/* Check the TX data */
		if (AIPC_GET_LP_CH_NR(ch->ch_id, reg_tx_data)) {
			dev_err(pdev->dev, "LP TX busy\n");
			ret = -EBUSY;
		} else {
			/* Copy data to the lp shared memory */
			memcpy_toio((u8 *)chn->ch_wmem, (u8 *)msg, sizeof(ipc_msg_struct_t));
			reg_tx_data = AIPC_SET_LP_CH_NR(ch->ch_id);
		}
	} else if (ch->port_id == AIPC_PORT_NP) {
		/* Check the TX data */
		if (AIPC_GET_NP_CH_NR(ch->ch_id, reg_tx_data)) {
			dev_err(pdev->dev, "NP TX busy\n");
			ret = -EBUSY;
		} else {
			/* Copy data to the lp shared memory */
			memcpy_toio((u8 *)chn->ch_wmem, (u8 *)msg, sizeof(ipc_msg_struct_t));
			reg_tx_data = AIPC_SET_NP_CH_NR(ch->ch_id);
		}
	} else {
		dev_err(pdev->dev, "Inavalib port id %d\n", ch->port_id);
		ret = -EINVAL;
	}

	writel(reg_tx_data, pdev->preg_tx_data);

	return ret;
}

/**
 * @brief  to search the channel list in this port and find the valid channel
 * 	id(not used).
 * @param  pport[inout]: the pointer to ipc port.
 * @return return the valid channel id, if id >= AIPC_CH_MAX_NUM, no valid
 * 	channel.
 */
static u32 ameba_ipc_find_channel_id(struct aipc_port *pport)
{
	u32 id = AIPC_NOT_ASSIGNED_CH;
	struct aipc_ch_node *chn = NULL;

	/* Assume is is 0 before polling the channel list */
	id = 0;
	list_for_each_entry(chn, &pport->ch_list, list) {
		if (id == chn->ch->ch_id) {
			id++;    /* Find same id, id++ and compare next channel */
		} else {
			goto func_exit;
		}; /* This id is not used, break. */

		if (id >= AIPC_CH_MAX_NUM) {
			id = AIPC_NOT_ASSIGNED_CH;
			dev_err(pport->dev->dev, "No valid channel\n");
			goto func_exit;
		}
	}

func_exit:
	return id;
}

/**
 * @brief  to check the assgined channel is valid channel
 * 	id(not used).
 * @param  pport[inout]: the pointer to ipc port.
 * @param  pid[inout]: the pointer to assgined channel. If value is
 *	   >= AIPC_CH_MAX_NUM, this channel id is used..
 * @return NULL.
 */
static void ameba_ipc_check_channel_id(struct aipc_port *pport, u32 *pid)
{
	struct aipc_ch_node *chn = NULL;

	list_for_each_entry(chn, &pport->ch_list, list) {
		if (*pid == chn->ch->ch_id) {
			*pid = AIPC_CH_IS_ASSIGNED;
			return; /* This id is used, break. */
		}
	}
}

/**
 * @brief  to find the channel to haddle the interrupt.
 * @param  pport[inout]: the pointer to ipc port.
 * @return return the interrupt channel.
 * @detail read ISR register to distingish distingish which channel the
 *	interrupt happen on.
 *	structure of ISP (unit bit):
 * 	|<------8------>|<------8------>|<------8------>|<------8------>|
 * 	|----NP_FULL----|----LP_FULL----|----NP_TMPT----|----LP_EMPT----|
 * 	|7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|
 * 	when the bit is 1, int means the interrupt happens on the relactive
 * 	channel.
 */
static inline \
struct aipc_ch_node *ameba_ipc_find_int_ch(struct aipc_port *pport, \
		u32 reg_isr)
{
	struct aipc_ch_node *chn = NULL;
	u32 reg_isr_empt = (reg_isr & ISR_EMPT_MASK) >> EMPT_OFFSET;
	u32 reg_isr_full = (reg_isr & ISR_FULL_MASK) >> FULL_OFFSET;

	if (list_empty(&pport->ch_list)) {
		goto func_exit;
	}

	if (pport->port_id == AIPC_PORT_LP) {
		reg_isr_empt = reg_isr_empt >> LP_OFFSET;
		reg_isr_full = reg_isr_full >> LP_OFFSET;
	} else {
		reg_isr_empt = reg_isr_empt & 0xFF;
		reg_isr_full = reg_isr_full & 0xFF;
	}

	list_for_each_entry(chn, &pport->ch_list, list) {
		if (reg_isr_empt && (chn->ch) \
			&& (reg_isr_empt & (0x01 << chn->ch->ch_id))) {
			break;
		} else if (reg_isr_full && (chn->ch) \
				   && (reg_isr_full & (0x01 << chn->ch->ch_id))) {
			break;
		}
	}

func_exit:
	return chn;
}

/**
 * @brief  the real haddle function to process the interrupt.
 * @param  pport[inout]: the pointer to ipc port.
 * @return return the interrupt channel.
 */
static inline void ameba_ipc_real_int_hdl(struct aipc_device *pipc, \
		struct aipc_port *pport, u32 isr)
{
	struct aipc_ch_node *chn = NULL;
	struct aipc_ch *ch = NULL;
	ipc_msg_struct_t msg = {0};
	u32 reg_isr = isr;

	while (reg_isr) {
		chn = ameba_ipc_find_int_ch(pport, reg_isr);
		if (!chn || !(chn->ch)) {
			break;
		}
		ch = chn->ch;

		if ((reg_isr & ISR_EMPT_MASK) && (ch->ch_config & AIPC_CONFIG_HANDSAKKE)) {
			if ((ch->ops) && (ch->ops->channel_empty_indicate)) {
				ch->ops->channel_empty_indicate(ch);
			}
			/* Empty interrupt  */
			/**
			 * reg_isr & 0x0000ffff means to get the empty bits in ISR. If
			 * is not 0, this is a empty interrupt.
			 */
			if (ch->port_id == AIPC_PORT_LP) {
				reg_isr = AIPC_CLR_LP_CH_IR_EMPT(ch->ch_id, reg_isr);
			} else {
				reg_isr = AIPC_CLR_NP_CH_IR_EMPT(ch->ch_id, reg_isr);
			}
		}

		if (reg_isr & ISR_FULL_MASK) { /* Full interrupt */
			if ((ch->ops) && (ch->ops->channel_recv)) {
				memcpy_fromio((u8 *)&msg, (u8 *)(chn->ch_rmem), sizeof(ipc_msg_struct_t));
				ch->ops->channel_recv(ch, &msg);
			}

			if (ch->port_id == AIPC_PORT_LP) {
				reg_isr = AIPC_CLR_LP_CH_IR_FULL(ch->ch_id, reg_isr);
			} else {
				reg_isr = AIPC_CLR_NP_CH_IR_FULL(ch->ch_id, reg_isr);
			}
		}
	}

	return;
}

/**
 * @brief  to process the ipc interrupr, it will poll all channels of this port,
 * 	and call the register function of relactive channel.
 * @param  pipc[inout]: the pointer to ipc port.
 * @return if is OK, return 0, failed return negative number.
 */
static irqreturn_t ameba_ipc_int_hdl(int irq, void *dev)
{
	struct aipc_device *pipc = (struct aipc_device *)dev;
	struct aipc_port *pport = NULL;
	u32 reg_isr = 0, reg_imr = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&pipc->lock, flags);
	/**
	 * Read ISR register to distingish the type of interrupt.
	 * structure of ISP (unit bit):
	 * |<------8------>|<------8------>|<------8------>|<------8------>|
	 * |----LP_FULL----|----NP_FULL----|----LP_TMPT----|----NP_EMPT----|
	 * |7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|7-6-5-4-3-2-1-0|
	 * LP_EMPT means IPC channel empty interrupt from LP processer. When
	 * the bit of channel is not 0, it will be clean and wakeup the waiting
	 * of last TX with handshake. If no handshake, don't care these bits.
	 * same to NP_TMPT.
	 * LP_FULL means IPC channel full interrupt from LP processer. When the
	 * bit of channel is not 0, it needs to process the RX interrupt.
	 * of course, this ISR also is used to distingish which channel the
	 * interrupt happen on.
	 * Notice:
	 * To write bit 1 means to clean relactive channel interrupt.
	 */

	reg_isr = readl(pipc->preg_isr);
	/* There is a bug in test chip. The isr cannot be clean cometimes. So to clear
	 * it three times.
	 */
	writel(reg_isr, pipc->preg_isr);
	reg_imr = readl(pipc->preg_imr);

	reg_isr &= reg_imr;
	if (reg_isr & ISR_FROM_NP_MASK) {
		pport = pipc->pnp_port;
		ameba_ipc_real_int_hdl(pipc, pport, reg_isr);
	}

	if (reg_isr & ISR_FROM_LP_MASK) {
		pport = pipc->plp_port;
		ameba_ipc_real_int_hdl(pipc, pport, reg_isr);
	}

	if (pport == NULL) {
		dev_err(pipc->dev, "Invalid port, ISR: 0x%08X, IMR: 0x%08X\n", reg_isr, reg_imr);
	}

	spin_unlock_irqrestore(&pipc->lock, flags);

interrupt_exit:
	return IRQ_HANDLED;
}


/**
 * @brief  to initialize the hardware of ipc port, include the interrupt.
 * @param  pipc[inout]: the pointer to ipc port.
 * @return if is OK, return 0, failed return negative number.
 */
static int ameba_ipc_init_hw(struct aipc_device *pipc)
{
	int ret = 0;

	/* Request interrupt for IPC port. */
	spin_lock(&pipc->lock);
	if (request_irq(pipc->irq, ameba_ipc_int_hdl, 0, \
					dev_name(pipc->dev), pipc)) {
		dev_err(pipc->dev, "Failed to request IRQ\n");
		ret = -EIO;
		goto func_exit;
	}
	disable_irq(pipc->irq);
	spin_unlock(&pipc->lock);

func_exit:
	return ret;
}

/**
 * @brief  this function will be called when the kenel matches compatible in
 * 	DTB. It will do some intialization of the IPC(Inter Process
 * 	Communication).
 * @param  pdev[inout]: the pointer to platform_device.
 * @return if is OK, return 0, failed return negative number.
 */
static inline struct aipc_ch_node *ameba_ipc_find_ch(struct aipc_device *pipc, \
		struct aipc_ch *ch)
{
	struct aipc_ch_node *chn = NULL;
	struct aipc_port *pport = NULL;

	if (!pipc || !ch) {
		goto func_exit;
	}

	if (ch->port_id == AIPC_PORT_NP) {
		pport = pipc->pnp_port;
	} else if (ch->port_id == AIPC_PORT_LP) {
		pport = pipc->plp_port;
	}

	if (!pport) {
		dev_err(pipc->dev, "Invalid channel's pport\n");
		goto func_exit;
	}

	list_for_each_entry(chn, &pport->ch_list, list) {
		if ((chn->ch) && (chn->ch == ch)) {
			break;
		}
	}

func_exit:
	return chn;
}

/**
 * @brief  this function will be called when the kenel matches compatible in
 * 	DTB. It will do some intialization of the IPC(Inter Process
 * 	Communication).
 * @param  pdev[inout]: the pointer to platform_device.
 * @return if is OK, return 0, failed return negative number.
 */
static int ameba_ipc_probe(struct platform_device *pdev)
{
	int ret = 0, irq = -1;
	struct aipc_port *pport = NULL;
	struct resource *res_mem = NULL;
	struct resource *res_reg = NULL;

	pipc_dev = kzalloc(sizeof(struct aipc_device), GFP_KERNEL);
	if (!pipc_dev) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to alloc IPC device\n");
		goto func_exit;
	}
	/* Initialize the port list */
	spin_lock_init(&pipc_dev->lock);

	pipc_dev->dev = &pdev->dev;
	platform_set_drvdata(pdev, pipc_dev);

	/* Get the IRQ nember from device tree */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		ret = -ENXIO;
		goto free_pipc;
	}
	pipc_dev->irq = irq;
	pipc_dev->irqflags = 0;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(&pdev->dev, "No LP shared SRAM for IPC\n");
		ret = -EINVAL;
		goto free_pipc;
	}

	if ((res_mem->end - res_mem->start + 1) < TOT_MEM_SZIE) {
		dev_err(&pdev->dev, "LP shared SRAM for IPC is not enough\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	/* Mapping the LP shared SRAM for IPC */
	pipc_dev->dev_mem = (ipc_msg_struct_t *)devm_ioremap_resource(&pdev->dev, res_mem);

	/* Mapping the address of registers */
	res_reg = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res_reg) {
		dev_err(&pdev->dev, "No TX REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_tx_data = (u32 *)devm_ioremap_resource(&pdev->dev, res_reg);

	res_reg = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res_reg) {
		dev_err(&pdev->dev, "No RX REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_rx_data = (u32 *)devm_ioremap_resource(&pdev->dev, res_reg);

	res_reg = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res_reg) {
		dev_err(&pdev->dev, "No INTERRUPT STATUS REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_isr = (u32 *)devm_ioremap_resource(&pdev->dev, res_reg);

	res_reg = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	if (!res_reg) {
		dev_err(&pdev->dev, "No INTERRUPT MASK REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_imr = (u32 *)devm_ioremap_resource(&pdev->dev, res_reg);

	res_reg = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	if (!res_reg) {
		dev_err(&pdev->dev, "No CLEAR TX REGISTER in the resources\n");
		ret = -EINVAL;
		goto free_pipc;
	}
	pipc_dev->preg_icr = (u32 *)devm_ioremap_resource(&pdev->dev, res_reg);

	/* Initialize the hardware of this ipc port. */
	ameba_ipc_init_hw(pipc_dev);

	/* Initialize NP port start */
	pport = kzalloc(sizeof(struct aipc_port), GFP_KERNEL);
	if (!pport) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to alloc IPC NP port\n");
		goto free_pipc;
	}
	pipc_dev->pnp_port = pport;

	/* Initialize the channel list */
	spin_lock_init(&pport->lock);
	INIT_LIST_HEAD(&pport->ch_list);
	pport->free_chnl_num = AIPC_CH_MAX_NUM;

	/* Associate the ipc device */
	pport->dev = pipc_dev;

	pport->port_id = AIPC_PORT_NP;
	pport->name = NAME_OF_NP_PORT;
	/* Initialize NP port end */
	dev_info(&pdev->dev, "IPC init NP port done, port id is %d\n", pport->port_id);

	/* Initialize LP port start */
	pport = NULL;
	pport = kzalloc(sizeof(struct aipc_port), GFP_KERNEL);
	if (!pport) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to alloc IPC LP port\n");
		goto free_np_port;
	}
	pipc_dev->plp_port = pport;

	/* Initialize the channel list */
	spin_lock_init(&pport->lock);
	INIT_LIST_HEAD(&pport->ch_list);
	pport->free_chnl_num = AIPC_CH_MAX_NUM;

	/* Associate the ipc device */
	pport->dev = pipc_dev;

	pport->port_id = AIPC_PORT_LP;
	pport->name = NAME_OF_LP_PORT;
	dev_info(&pdev->dev, "IPC init LP port done, port id is %d\n", pport->port_id);

	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source")) {
		device_init_wakeup(&pdev->dev, true);
		dev_pm_set_wake_irq(&pdev->dev, pipc_dev->irq);
	}

	goto func_exit;

free_np_port:
	kfree(pipc_dev->pnp_port);

free_pipc:
	kfree(pipc_dev);

func_exit:
	return ret;
}

/**
 * @brief  this function will be called when the kenel remove the device of IPC.
 * @param  pdev[inout]: the pointer to platform_device.
 * @return if is OK, return 0, failed return negative number.
 */
static int ameba_ipc_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct aipc_device *pipc = NULL;
	struct aipc_ch_node *chn = NULL, *tmp = NULL;

	pipc = platform_get_drvdata(pdev);
	if (pipc) {
		spin_lock(&pipc->lock);
		/* Disable interrupt */
		disable_irq(pipc->irq);
		free_irq(pipc->irq, pipc);

		/* Free the ports */
		/* Free the resource of NP port */
		if (pipc->pnp_port) {
			if (!list_empty(&pipc->pnp_port->ch_list))
				list_for_each_entry_safe(chn, tmp, \
										 &pipc->pnp_port->ch_list, \
										 list) {
				spin_lock(&pipc->pnp_port->lock);
				chn->ch = NULL;
				list_del(&chn->list);
				spin_unlock(&pipc->pnp_port->lock);
				kfree(chn);
			}
			kfree(pipc->pnp_port);
		}

		/* Free the resource of LP port */
		if (pipc->plp_port) {
			if (!list_empty(&pipc->plp_port->ch_list))
				list_for_each_entry_safe(chn, tmp, \
										 &pipc->plp_port->ch_list, \
										 list) {
				spin_lock(&pipc->plp_port->lock);
				chn->ch = NULL;
				list_del(&chn->list);
				spin_unlock(&pipc->plp_port->lock);
				kfree(chn);
			}
			kfree(pipc->plp_port);
		}
		ioport_unmap(pipc->dev_mem);
		spin_unlock(&pipc->lock);

		kfree(pipc);
	}

	return ret;
}

/**
 * @brief  to initilaze the ameba IPC driver and register its platform_driver.
 * 	It will be called when the driver is insmod or kernal init.
 * @return if is OK, return 0, failed return negative number.
 */
static int __init ameba_ipc_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dameba_ipc_driver);
	if (ret) {
		pr_err("Register IPC platform driver error: %d\n", ret);
	}

	return ret;
}

/**
 * @brief  to deinitilaze the ameba IPC driver and unregister its
 * 	platform_driver. It will be called when the driver is remove or kernal
 * 	is free.
 */
static void __exit ameba_ipc_exit(void)
{
	platform_driver_unregister(&dameba_ipc_driver);
}

/* ---------------------------- Public Functions ---------------------------- */
/**
 * @brief  to tegister the ipc channel to the ipc port.
 * @param  pch[inout]: the pointer to ipc channel.
 * @return if is OK, return 0, failed return negative number.
 * @note: if to want to get dynamic channel id, set the channel id to
 * AIPC_NOT_ASSIGNED_CH.
 */
int ameba_ipc_channel_register(struct aipc_ch *ch)
{
	struct aipc_port *pport = NULL;
	struct aipc_ch_node *chn = NULL;
	u32 reg_imr = 0;
	int ret = 0;

	if (!ch || !(ch->ops) || (!(ch->ops->channel_recv))) {
		pr_err("IPC register error: Input parameter error\n");
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pipc_dev) {
		pr_err("IPC register error: Invalid IPC device\n");
		ret = -ENODEV;
		goto func_exit;
	}

	/* Find the port by port id */
	if (ch->port_id == AIPC_PORT_NP) {
		pport = pipc_dev->pnp_port;
	} else if (ch->port_id == AIPC_PORT_LP) {
		pport = pipc_dev->plp_port;
	} else {
		dev_err(pipc_dev->dev, "No avalib port id\n");
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pport) {
		dev_err(pipc_dev->dev, "This port id is not initialized\n");
		ret = -EINVAL;
		goto func_exit;
	}

	/*
	 * If channel list is empty, the interrupt is not enabled. So enable
	 * the interrupt.
	 */
	if (list_empty(&pipc_dev->plp_port->ch_list) \
		&& list_empty(&pipc_dev->pnp_port->ch_list)) {
		enable_irq(pipc_dev->irq);
	}

	if (pport->free_chnl_num == 0) {
		dev_err(pipc_dev->dev, "No channel to register\n");
		ret = -EBUSY;
		goto func_exit;
	}

	chn = kzalloc(sizeof(struct aipc_ch_node), GFP_KERNEL);
	if (!chn) {
		dev_err(pipc_dev->dev, "Failed to alloc IPC channel enity\n");
		ret = -EBUSY;
		goto func_exit;
	}

	/* Find valid channel and register the channel to the port */
	if (ch->ch_id >= AIPC_CH_MAX_NUM) {
		ch->ch_id = ameba_ipc_find_channel_id(pport);
	} else {
		ameba_ipc_check_channel_id(pport, &(ch->ch_id));
	}

	if (ch->ch_id >= AIPC_CH_MAX_NUM) {
		dev_err(pipc_dev->dev, "Get channel id is wrong: %s\n", (ch->ch_id == AIPC_CH_IS_ASSIGNED) ? \
				"channel is assgined, to choose another one" : \
				"no valid channel");
		ret = -EBUSY;
		goto free_chn;
	}
	chn->port = pport;
	chn->ch = ch;
	ch->pdev = pipc_dev->dev;

	spin_lock(&pport->lock);
	list_add(&chn->list, &pport->ch_list);
	spin_unlock(&pport->lock);
	pport->free_chnl_num--;

	reg_imr = readl(pipc_dev->preg_imr);
	/* Set the hard ware interrupt */
	if (ch->port_id == AIPC_PORT_NP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_NP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2NP_IDX(ch->ch_id);
		dev_dbg(pipc_dev->dev, "NP channel rmem = 0x%08X, wmem = 0x%08X\n", (u32)chn->ch_rmem, (u32)chn->ch_wmem);
		reg_imr = AIPC_SET_NP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_SET_NP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	} else if (ch->port_id == AIPC_PORT_LP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_LP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2LP_IDX(ch->ch_id);
		dev_dbg(pipc_dev->dev, "LP channel rmem = 0x%08X, wmem = 0x%08X\n", (u32)chn->ch_rmem, (u32)chn->ch_wmem);
		reg_imr = AIPC_SET_LP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_SET_LP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	}
	writel(reg_imr, pipc_dev->preg_imr);

	dev_dbg(pipc_dev->dev, "Register channel %d successfully\n", ch->ch_id);

	goto func_exit;
free_chn:
	kfree(chn);

func_exit:
	return ret;
}
EXPORT_SYMBOL(ameba_ipc_channel_register);

/**
 * @brief  to untegister the ipc channel to the ipc port.
 * @param  pch[inout]: the pointer to ipc channel.
 * @return if is OK, return 0, failed return negative number.
 */
int ameba_ipc_channel_unregister(struct aipc_ch *ch)
{
	struct aipc_port *pport = NULL;
	struct aipc_ch_node *chn = NULL;
	u32 reg_imr = 0;
	int ret = 0;

	if (!ch) {
		pr_err("IPC unregister error: Invalid parameter\n");
		ret = -EINVAL;
		goto func_exit;
	}
	pr_info("IPC unregister channel %d\n", ch->ch_id);

	if (!pipc_dev) {
		pr_err("IPC unregister error: Invalid IPC device\n");
		ret = -ENODEV;
		goto func_exit;
	}

	chn = ameba_ipc_find_ch(pipc_dev, ch);
	if (!chn) {
		dev_err(pipc_dev->dev, "Channel %d not registered\n", ch->ch_id);
		ret = -EINVAL;
		goto func_exit;
	}
	pport = chn->port;

	reg_imr = readl(pipc_dev->preg_imr);
	/* Set the HW interrupt */
	if (ch->port_id == AIPC_PORT_NP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_NP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2NP_IDX(ch->ch_id);
		reg_imr = AIPC_CLR_NP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_CLR_NP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	} else if (ch->port_id == AIPC_PORT_LP) {
		chn->ch_rmem = pipc_dev->dev_mem + BUF_LP2AP_IDX(ch->ch_id);
		chn->ch_wmem = pipc_dev->dev_mem + BUF_AP2LP_IDX(ch->ch_id);
		reg_imr = AIPC_CLR_LP_CH_IR_FULL(ch->ch_id, reg_imr);
		/* Configure the empty interrupt */
		if (ch->ch_config & AIPC_CONFIG_HANDSAKKE) {
			reg_imr = AIPC_CLR_LP_CH_IR_EMPT(ch->ch_id, reg_imr);
		}
	}
	writel(reg_imr, pipc_dev->preg_imr);

	/* Unregister the channel from the port and disable the interrupt */
	spin_lock(&pport->lock);
	chn->ch = NULL;
	list_del(&chn->list);
	spin_unlock(&pport->lock);
	pport->free_chnl_num++;
	kfree(chn);

	/*
	 * No channel in the channel list. disable the interrupt.
	 */
	if (list_empty(&pipc_dev->plp_port->ch_list) \
		&& list_empty(&pipc_dev->pnp_port->ch_list)) {
		disable_irq(pipc_dev->irq);
	}

func_exit:
	return ret;
}
EXPORT_SYMBOL(ameba_ipc_channel_unregister);

/**
 * @brief  to send the message by the channel without handshake.
 * @param  pch[in]: the pointer to ipc channel.
 * @param  pmsg[in]: message to send.
 * @return if is OK, return 0, failed return negative number.
 */
int ameba_ipc_channel_send(struct aipc_ch *ch, \
						   ipc_msg_struct_t *pmsg)
{
	int ret = 0;
	struct aipc_ch_node *chn = NULL;

	if (!ch) {
		pr_err("IPC send error: Invalid channel\n");
		ret = -EINVAL;
		goto func_exit;
	}

	if (!pipc_dev) {
		pr_err("IPC send error: Invalid IPC device\n");
		ret = -ENODEV;
		goto func_exit;
	}

	chn = ameba_ipc_find_ch(pipc_dev, ch);
	if (!chn) {
		dev_err(pipc_dev->dev, "IPC channel unregistered\n");
		ret = -EINVAL;
		goto func_exit;
	}

	ret = ameba_ipc_send_work(chn, pmsg);

func_exit:
	return ret;
}
EXPORT_SYMBOL(ameba_ipc_channel_send);

/**
 * @brief  to accllocate the ipc channel.
 * @param  size_of_priv[in]: the length of private_data in aipc_ch_node.
 * @return return the allocated aipc_ch_node. If failed, return NULL.
 */
struct aipc_ch *ameba_ipc_alloc_ch(int size_of_priv)
{
	struct aipc_ch *ch = NULL;
	int alloc_size = sizeof(struct aipc_ch);

	alloc_size += size_of_priv - sizeof(void *);

	ch = kzalloc(alloc_size, GFP_KERNEL);
	if (!ch) {
		pr_err("Failed to alloc IPC channel\n");
		goto func_exit;
	}
	ch->ch_id = AIPC_NOT_ASSIGNED_CH;

func_exit:
	return ch;
}
EXPORT_SYMBOL(ameba_ipc_alloc_ch);

subsys_initcall(ameba_ipc_init);
module_exit(ameba_ipc_exit);

MODULE_DESCRIPTION("Realtek Ameba IPC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
