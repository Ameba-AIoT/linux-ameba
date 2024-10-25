// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek OTP IPC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <ameba_ipc/ameba_ipc.h>

#include "rtk-otp-ipc.h"

enum {
	IPC_USER_POINT = 0,
	IPC_USER_DATA = 1
};

struct rtk_otp {
    struct device		    *dev;
    struct aipc_ch          *potp_ipc_ch;
	otp_ipc_host_req_t      *preq_msg;              /* host api message to send to device */
	dma_addr_t              req_msg_phy_addr;       /* host api message to send to device */
	struct tasklet_struct   otp_tasklet;            /* api task to haddle api msg */
	ipc_msg_struct_t        otp_ipc_msg;            /* to store ipc msg for api */
    struct completion       otp_complete;           /* only one otp process can send ipc instruction */
};

typedef struct otp_ipc_rx_res {
	int ret;
	int complete_num;
} otp_ipc_rx_res_t;

static struct rtk_otp *piihp_priv = NULL;
otp_ipc_rx_res_t *p_recv_res = NULL;
int otp_done;
int otp_channel_busy;
/**
 * entity of struct aipc_ch_ops. It will associate the channel_recv to
 * otp_ipc_host_event_int_hdl.
 */
static u32 otp_ipc_host_otp_int_hdl(aipc_ch_t *ch, ipc_msg_struct_t *pmsg);
static void otp_ipc_channel_empty(struct aipc_ch *ch);
static struct aipc_ch_ops otp_ipc_otp_ops = {
	.channel_recv = otp_ipc_host_otp_int_hdl,
	.channel_empty_indicate = otp_ipc_channel_empty,
};

static void otp_ipc_channel_empty(struct aipc_ch *ch)
{
	otp_channel_busy = 0;
}

/**
 * @brief  to haddle the otp_d interrupt from ipc device.
 * @return if is OK, return 0, failed return negative number.
 */
static u32 otp_ipc_host_otp_int_hdl(aipc_ch_t *ch, ipc_msg_struct_t *pmsg)
{
	struct rtk_otp *otp_d = piihp_priv;
	u32 ret = 0;

	if (!otp_d) {
		pr_err("Host_otp_priv is NULL in interrupt!\n");
		goto func_exit;
	}

	/* copy ipc_msg from temp memory in ipc interrupt. */
	memcpy((u8*)&(otp_d->otp_ipc_msg), (u8*)pmsg, sizeof(ipc_msg_struct_t));
	tasklet_schedule(&(otp_d->otp_tasklet));

func_exit:
	return ret;
}

int otp_ipc_host_otp_send_msg(otp_ipc_host_req_t *preq_msg)
{
	struct rtk_otp *otp_d = piihp_priv;
	int ret, retry = 0;

	if (!otp_d) {
		pr_err("Host_otp_priv is NULL when to send msg!\n");
		ret = -1;
		goto func_exit;
	}

	memset((u8*)(otp_d->preq_msg), 0, sizeof(otp_ipc_host_req_t));
	memcpy(otp_d->preq_msg, preq_msg, sizeof(otp_ipc_host_req_t));

	memset((u8*)&(otp_d->otp_ipc_msg), 0, sizeof(ipc_msg_struct_t));
	otp_d->otp_ipc_msg.msg = (u32)otp_d->req_msg_phy_addr;
	otp_d->otp_ipc_msg.msg_type = IPC_USER_POINT;
	otp_d->otp_ipc_msg.msg_len = sizeof(otp_ipc_host_req_t);
	otp_channel_busy = 1;
	ret = ameba_ipc_channel_send(otp_d->potp_ipc_ch, &(otp_d->otp_ipc_msg));

	if (!otp_channel_busy) {
		pr_debug("End no wait.");
	} else {
		retry = 100;
		while (retry > 0 && otp_channel_busy) {
			udelay(50);
			retry--;
		}
		otp_channel_busy = 0;
		if (retry <= 0) {
			/* force recover. */
			otp_channel_busy = 0;
			dev_err(otp_d->dev, "ipc send has some problem now.");
		} else {
			pr_debug("Wait and end.");
		}
	}

func_exit:
	return ret;
}

static void otp_ipc_host_otp_task(unsigned long data) {
	struct rtk_otp *otp_d = (struct rtk_otp *)data;
	struct device *pdev = NULL;
	int msg_len = 0;

	if (!otp_d || !otp_d->potp_ipc_ch) {
		if (!otp_d) {
			pr_err("Host_otp_priv is NULL when to send msg!\n");
		} else {
			dev_err(otp_d->dev, "Potp_ipc_ch is NULL!\n");
		}
		goto func_exit;
	}

	pdev = otp_d->potp_ipc_ch->pdev;
	if (!pdev) {
		dev_err(otp_d->dev, "Device is NULL!\n");
		goto func_exit;
	}

	if (!otp_d->otp_ipc_msg.msg || !otp_d->otp_ipc_msg.msg_len) {
		dev_err(otp_d->dev, "Invalid device message!\n");
		goto func_exit;
	}
	msg_len = otp_d->otp_ipc_msg.msg_len;
	p_recv_res = phys_to_virt(otp_d->otp_ipc_msg.msg);

	otp_done = 1;

func_exit:
	return;
}

/* input: */
/* data: in type of otp_ipc_host_req_t. */
/* otp_ipc_host_req_t: otp_id is LINUX_IPC_OTP_PHY_READ8/LINUX_IPC_OTP_PHY_WRITE8/LINUX_IPC_OTP_LOGI_READ_MAP/LINUX_IPC_OTP_LOGI_WRITE_MAP/LINUX_IPC_EFUSE_REMAIN_LEN. */
/* otp_ipc_host_req_t: addr is the address to read/write. */
/* otp_ipc_host_req_t: len is the len to read/write. */
/* otp_ipc_host_req_t: write_lock is the lock to write. (if set to 0, the param_buf will be written into Efuse) */
/* otp_ipc_host_req_t: param_buf is the value to write, or the read value returned. */
/* output: */
/* result: for otp read, result shall be a malloc buffer. */
/*         for otp write, the result can be ignored. */
/*		   for EFUSE Remain read, result length is 4*(u8), combined as a (u32) remain value. result[0] is the largest edian. */
int rtk_otp_process(void* data, u8 *result)
{
	otp_ipc_host_req_t *preq_msg = data;
	int ret = 0;
	int retry = 0;
	struct rtk_otp *otp_d = NULL;

	if (otp_done) {
		otp_done = 0;
	} else {
		return -EBUSY;
	}

	if (preq_msg->len > OPT_REQ_MSG_PARAM_NUM) {
		pr_err("OTP parameters requested is too much. Maximum for %d bytes. \n", OPT_REQ_MSG_PARAM_NUM);
		goto err_ret;
	}

	ret = otp_ipc_host_otp_send_msg(preq_msg);
	if (ret < 0) {
		goto err_ret;
	}

	while (!otp_done) {
		retry++;
		udelay(50);
		if (retry > 1000) {
			goto err_ret;
		}
	}

	if (p_recv_res->ret != 1) {
			pr_warning("OTP failed but has already complete %d", p_recv_res->complete_num);
			goto err_ret;
	}

	otp_d = piihp_priv;
	ret = p_recv_res->ret;
	memcpy(result, otp_d->preq_msg->param_buf, preq_msg->len);

	return ret;

err_ret:
	otp_done = 1;
	return -EINVAL;
}

static int rtk_otp_probe(struct platform_device *pdev)
{
	struct rtk_otp *otp_d;
	int ret;

	otp_d = devm_kzalloc(&pdev->dev, sizeof(struct rtk_otp), GFP_KERNEL);
	if (!otp_d) {
		return -ENOMEM;
	}
	otp_d->dev = &pdev->dev;

	/* allocate the ipc channel */
	otp_d->potp_ipc_ch = ameba_ipc_alloc_ch(sizeof(struct rtk_otp));
	if (!otp_d->potp_ipc_ch) {
		ret = -ENOMEM;
		dev_err(otp_d->dev, "No memory for ipc channel.\n");
		goto free_otp;
	}

	/* initialize the ipc channel */
	otp_d->potp_ipc_ch->port_id = AIPC_PORT_NP;
	otp_d->potp_ipc_ch->ch_id = 6; /* configure channel 6 */
	otp_d->potp_ipc_ch->ch_config = AIPC_CONFIG_HANDSAKKE;
	otp_d->potp_ipc_ch->ops = &otp_ipc_otp_ops;
	otp_d->potp_ipc_ch->priv_data = otp_d;

	/* regist the otp_d ipc channel */
	ret = ameba_ipc_channel_register(otp_d->potp_ipc_ch);
	if (ret < 0) {
        dev_err(otp_d->dev, "Regist otp_d channel error.\n");
		goto free_ipc_ch;
	}

	if (!otp_d->potp_ipc_ch->pdev) {
        dev_err(otp_d->dev, "No device in registed IPC channel.\n");
		goto free_ipc_ch;
	}

	otp_d->preq_msg = dmam_alloc_coherent(otp_d->potp_ipc_ch->pdev, sizeof(otp_ipc_host_req_t), &otp_d->req_msg_phy_addr, GFP_KERNEL);
	if (!otp_d->preq_msg) {
        dev_err(otp_d->dev, "Allloc req_msg error.\n");
		goto unregist_ch;
	}

	/* initialize otp_d tasklet */
	tasklet_init(&(otp_d->otp_tasklet), otp_ipc_host_otp_task, (unsigned long)otp_d);
	piihp_priv = otp_d;
	otp_done = 1;
	otp_channel_busy = 0;
	goto func_exit;

unregist_ch:
	ameba_ipc_channel_unregister(otp_d->potp_ipc_ch);

free_ipc_ch:
	kfree(otp_d->potp_ipc_ch);

free_otp:
	kfree(otp_d);

func_exit:
	return ret;
}

static const struct of_device_id rtk_otp_of_match[] = {
	{ .compatible = "realtek,ameba-otp-ipc",	},
	{ /* end node */ },
};

static struct platform_driver rtk_otp_driver = {
	.probe	= rtk_otp_probe,
	.driver	= {
		.name = "rtk-otp",
		.of_match_table = rtk_otp_of_match,
	},
};

builtin_platform_driver(rtk_otp_driver);

MODULE_DESCRIPTION("Realtek Ameba OTP IPC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");