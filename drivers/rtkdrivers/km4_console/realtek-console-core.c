// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Km4-console support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <misc/realtek-console-core.h>

static struct rtk_console *piihp_priv = NULL;
//console_ipc_rx_res_t *p_recv_res = NULL;
int console_done;
//struct mutex console_lock;
/**
 * entity of struct aipc_ch_ops. It will associate the channel_recv to
 * console_ipc_host_event_int_hdl.
 */
static u32 console_ipc_host_console_int_hdl(aipc_ch_t *ch, ipc_msg_struct_t *pmsg);
static void console_ipc_channel_empty(struct aipc_ch *ch);
static struct aipc_ch_ops console_ipc_console_ops = {
	.channel_recv = console_ipc_host_console_int_hdl,
	.channel_empty_indicate = console_ipc_channel_empty,
};

/**
 * @brief  to haddle the console_d interrupt from ipc device.
 * @return if is OK, return 0, failed return negative number.
 */
static u32 console_ipc_host_console_int_hdl(aipc_ch_t *ch, ipc_msg_struct_t *pmsg)
{
	struct rtk_console *console_d = piihp_priv;

	if (!console_d) {
		return -ENODEV;
	}

	/* Copy ipc_msg from temp memory in IPC interrupt. */
	memcpy((u8 *) & (console_d->console_ipc_msg), (u8 *)pmsg, sizeof(ipc_msg_struct_t));
	tasklet_schedule(&(console_d->console_tasklet));

	return 0;
}

static void console_ipc_channel_empty(struct aipc_ch *ch)
{
	console_done = 1;
}

int console_ipc_host_console_send_msg(char *preq_msg)
{
	struct rtk_console *console_d = piihp_priv;

	if (!console_d) {
		return -ENODEV;
	}

	memset((u8 *)(console_d->preq_msg), 0, sizeof(console_ipc_host_req_t));
	memcpy(console_d->preq_msg, preq_msg, sizeof(console_ipc_host_req_t));

	memset((u8 *) & (console_d->console_ipc_msg), 0, sizeof(ipc_msg_struct_t));
	console_d->console_ipc_msg.msg = (u32)console_d->req_msg_phy_addr;
	console_d->console_ipc_msg.msg_type = IPC_USER_POINT;
	console_d->console_ipc_msg.msg_len = sizeof(console_ipc_host_req_t);

	ameba_ipc_channel_send(console_d->pconsole_ipc_ch, &(console_d->console_ipc_msg));

	return 0;
}

static void console_ipc_host_console_task(unsigned long data)
{
	struct rtk_console *console_d = (struct rtk_console *)data;
	struct device *pdev = NULL;

	if (!console_d || !console_d->pconsole_ipc_ch) {
		return;
	}

	pdev = console_d->pconsole_ipc_ch->pdev;
	if (!pdev) {
		dev_err(console_d->dev, "Invalid device\n");
		return;
	}

	if (!console_d->console_ipc_msg.msg || !console_d->console_ipc_msg.msg_len) {
		dev_err(console_d->dev, "Invalid device message\n");
		return;
	}
}

int rtk_console_process(char *data, int len, u8 *result)
{
	struct rtk_console *console_d = piihp_priv;
	char *preq_msg = data;
	int ret = 0;
	int retry = 0;

	if (console_done) {
		console_done = 0;
	} else {
		return -EBUSY;
	}

	if (len > CONSOLE_MAX_CHAR) {
		dev_err(console_d->dev, "KM4 console parameters requested is too much. Maximum for %d bytes\n", CONSOLE_MAX_CHAR);
		goto err_ret;
	}

	ret = console_ipc_host_console_send_msg(preq_msg);

	while (!console_done) {
		retry++;
		msleep(2);
		if (retry > 1000) {
			dev_err(console_d->dev, "Wait for KM4 IPC channel empty handshake timeout\n");
			goto err_ret;
		}
	}

	return ret;

err_ret:
	console_done = 1;
	return -EINVAL;
}
EXPORT_SYMBOL(rtk_console_process);

static int rtk_console_probe(struct platform_device *pdev)
{
	struct rtk_console *console_d;
	int ret;

	console_d = devm_kzalloc(&pdev->dev, sizeof(struct rtk_console), GFP_KERNEL);
	if (!console_d) {
		return -ENOMEM;
	}
	console_d->dev = &pdev->dev;

	/* Allocate the IPC channel */
	console_d->pconsole_ipc_ch = ameba_ipc_alloc_ch(sizeof(struct rtk_console));
	if (!console_d->pconsole_ipc_ch) {
		ret = -ENOMEM;
		dev_err(console_d->dev, "No memory for IPC channel\n");
		goto free_console;
	}

	/* Initialize the IPC channel */
	console_d->pconsole_ipc_ch->port_id = AIPC_PORT_NP;
	/* Configure channel 7: IPC_A2N_LOGUART_RX_SWITCH */
	console_d->pconsole_ipc_ch->ch_id = 7;
	/* Configure enable IPC handshask IRQ. */
	console_d->pconsole_ipc_ch->ch_config = AIPC_CONFIG_HANDSAKKE;
	console_d->pconsole_ipc_ch->ops = &console_ipc_console_ops;
	console_d->pconsole_ipc_ch->priv_data = console_d;

	/* Regist the console_d IPC channel */
	ret = ameba_ipc_channel_register(console_d->pconsole_ipc_ch);
	if (ret < 0) {
		dev_err(console_d->dev, "Failed to regist console_d channel\n");
		goto free_ipc_ch;
	}

	if (!console_d->pconsole_ipc_ch->pdev) {
		dev_err(console_d->dev, "No device in registed IPC channel\n");
		goto free_ipc_ch;
	}

	console_d->preq_msg = dmam_alloc_coherent(console_d->pconsole_ipc_ch->pdev, sizeof(console_ipc_host_req_t), &console_d->req_msg_phy_addr, GFP_KERNEL);
	if (!console_d->preq_msg) {
		dev_err(console_d->dev, "Failed to allloc req_msg\n");
		goto unregist_ch;
	}

	/* Initialize console_d tasklet */
	tasklet_init(&(console_d->console_tasklet), console_ipc_host_console_task, (unsigned long)console_d);
	piihp_priv = console_d;
	console_done = 1;
	//mutex_init(&console_lock);
	goto func_exit;

unregist_ch:
	ameba_ipc_channel_unregister(console_d->pconsole_ipc_ch);

free_ipc_ch:
	kfree(console_d->pconsole_ipc_ch);

free_console:
	kfree(console_d);

func_exit:
	return ret;
}

static const struct of_device_id rtk_console_of_match[] = {
	{ .compatible = "realtek,ameba-km4-console",	},
	{ /* end node */ },
};

static struct platform_driver rtk_console_driver = {
	.probe	= rtk_console_probe,
	.driver	= {
		.name = "realtek-ameba-km4-console",
		.of_match_table = rtk_console_of_match,
	},
};

builtin_platform_driver(rtk_console_driver);

MODULE_DESCRIPTION("Realtek Ameba Km4 console driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
