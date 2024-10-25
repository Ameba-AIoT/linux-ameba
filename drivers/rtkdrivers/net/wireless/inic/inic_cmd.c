/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __INIC_CMD_C__

/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <uapi/linux/time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/module.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <asm/types.h>

/* internal head files */
#include "inic_cmd.h"
#include "wifi_ind.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_CMD "inic ioctl"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
typedef struct inic_io_block_scan_param {
	struct completion scan_done; /* completion to block join operations */
	unsigned char block; /* block parameters */
} inic_io_block_scan_param_t;

/* -------------------------- Function declaration -------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* --------------------------- Private Variables ---------------------------- */
static int inic_io_scanned_ap_num = 0;
static inic_port_t *p_inic_io_iport[INIC_MAX_NET_PORT_NUM] = {NULL};
static unsigned char cmd_ssid[INIC_MAX_SSID_LENGTH] = {0};
static unsigned char cmd_passwd[INIC_MAX_PASSWORD_LENGTH] = {0};
static unsigned char cmd_channel_list[INIC_MAX_CHANNEL_NUM] = {0};

 /* to block ioctl operation for uplayer */
static struct completion operation_done[INIC_MAX_NET_PORT_NUM];

static inic_io_block_scan_param_t block_scan_param = {0};

/* --------------------------- Private Functions ---------------------------- */
int inic_io_event_notify(inic_port_t *iport, inic_io_event_t evt, void *data, int len)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	int ret = 0;

	if (iport == NULL) {
		ret = -EINVAL;
		printk("%s, %s: iport is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	skb = nlmsg_new(len, 0);
	if (!skb) {
		ret = -ENOMEM;
		printk("%s, %s: Failed to allocate new skb!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	nlh = nlmsg_put(skb, 0, 1, evt, len, 0);
	memcpy(nlmsg_data(nlh), (uint8_t *)data, len);

	ret = nlmsg_multicast(iport->nl_sk, skb, 0, RINICIO_NL_NOTIFY_GPID, 0);
	if (ret < 0) {
		printk("%s, %s: Error while sending to user, err id %d.\n",
		       INIC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static void inic_io_join_status_cb(rtw_join_status_t join_status)
{
	inic_port_t *iport = p_inic_io_iport[INIC_STA_PORT];
	inic_wifi_state_t status = 0;

	status = join_status_to_wifi_status(join_status);
	iport->state = status;
	inic_io_event_notify(iport, RINICIO_EVT_WIFISTATUS, &join_status, sizeof(rtw_join_status_t));

	return;
}

static rtw_result_t inic_io_scan_done_cb(unsigned int scanned_ap_num, \
					   void *user_data)
{
	inic_port_t *iport = p_inic_io_iport[INIC_STA_PORT];
	int ret = RTW_SUCCESS;
	/* To avoid gcc warnings */
	(void) user_data;

	if (scanned_ap_num == 0) {/* scanned no AP*/
		ret = RTW_ERROR;
		goto func_exit;
	}
	inic_io_scanned_ap_num = scanned_ap_num;
	inic_io_event_notify(iport, RINICIO_EVT_SCANDONE, &scanned_ap_num, sizeof(int));

func_exit:
	return ret;
}

static rtw_result_t inic_io_block_scan_done_cb(unsigned int scanned_ap_num, \
					   void *user_data)
{
	int ret = RTW_SUCCESS;
	/* To avoid gcc warnings */
	(void) user_data;

	if (scanned_ap_num == 0) {/* scanned no AP*/
		ret = RTW_ERROR;
		goto func_exit;
	}
	inic_io_scanned_ap_num = scanned_ap_num;
	complete(&block_scan_param.scan_done);

func_exit:
	return ret;
}

static rtw_result_t inic_io_scan_each_report_cb(rtw_scan_result_t * scanned_ap_info, \
						void *user_data)
{
	inic_port_t *iport = p_inic_io_iport[INIC_STA_PORT];
	int ret = RTW_SUCCESS;
	/* To avoid gcc warnings */
	(void) user_data;

	inic_io_event_notify(iport, RINICIO_EVT_SACNEACHREPORT, scanned_ap_info,\
			     sizeof(rtw_scan_result_t));

	return ret;
}

static int inic_io_set_reg_event(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_io_reg_event_param_t event_param = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&event_param, (uint8_t __user *)arg,\
			     sizeof(inic_io_reg_event_param_t));
	if (ret) {
		printk("%s, %s: copy event_param from usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	wifi_reg_event_handler(event_param.event_cmd, event_param.row_id);

func_exit:
	return ret;
}

static int inic_io_set_unreg_event(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_io_reg_event_param_t event_param = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&event_param, (uint8_t __user *)arg,\
			     sizeof(inic_io_reg_event_param_t));
	if (ret) {
		printk("%s, %s: copy event_param from usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	wifi_unreg_event_handler(event_param.event_cmd, event_param.row_id);

func_exit:
	return ret;
}

static int inic_io_set_indicate_event(inic_port_t *iport, void *arg)
{
	int ret = 0;
	host_api_priv_t *api = (host_api_priv_t *)(iport->idev->api_ch->priv_data);
	inic_io_ind_event_param_t ind_param = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&ind_param, (uint8_t __user *)arg,\
			     sizeof(inic_io_ind_event_param_t));
	if (ret) {
		printk("%s, %s: copy ind_param from usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	wifi_indication(api, ind_param.event_cmd, ind_param.buf,\
			ind_param.buf_len, ind_param.flags);

func_exit:
	return ret;
}

static int inic_io_set_scan(inic_port_t *iport, void *arg, int block)
{
	int ret = 0;
	inic_io_scan_arg_t scan_arg = {0};
	rtw_scan_param_t scan_param = {0};

	scan_param.ssid = NULL;
	scan_param.channel_list = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&scan_arg, (uint8_t __user *)arg,\
			     sizeof(inic_io_scan_arg_t));
	if (ret) {
		printk("%s, %s: copy user_scan_arg usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&scan_param, (uint8_t __user *)scan_arg.param_ptr,\
			     sizeof(rtw_scan_param_t));
	if (ret) {
		printk("%s, %s: copy rtw_scan_param_t usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (scan_param.ssid && (scan_arg.ssid_len > 0)\
	    &&(scan_arg.ssid_len < INIC_MAX_SSID_LENGTH)) {
		memset(cmd_ssid, 0, INIC_MAX_SSID_LENGTH);
		ret = copy_from_user((uint8_t *)cmd_ssid,\
				     (uint8_t __user *)scan_param.ssid,\
				     scan_arg.ssid_len);
		if (ret) {
			printk("%s, %s: copy ssid from usersapce failed (%d)!\n",
			       INIC_CMD, __func__, ret);
			ret = -EIO;
			goto func_exit;
		}
		scan_param.ssid = cmd_ssid;
	} else if (scan_arg.ssid_len >= INIC_MAX_SSID_LENGTH) {
		printk(KERN_ERR "%s, %s: the length of ssid is exceeded!\n",\
		       INIC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (scan_param.channel_list && (scan_param.channel_list_num >0 )\
	    && (scan_param.channel_list_num <= INIC_MAX_CHANNEL_NUM)) {
		ret = copy_from_user((uint8_t *)scan_param.channel_list,\
				    (uint8_t __user *)scan_param.channel_list,\
				    scan_param.channel_list_num);
		if (ret) {
			printk("%s, %s: copy channel_listfrom usersapce failed (%d)!\n",
			       INIC_CMD, __func__, ret);
			ret = -EIO;
			goto func_exit;
		}
		scan_param.channel_list = cmd_channel_list;
	} else if (scan_param.channel_list_num > INIC_MAX_CHANNEL_NUM) {
		printk(KERN_ERR "%s, %s: the num of channel is exceeded!\n",\
		       INIC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (scan_param.scan_user_callback) {
		scan_param.scan_user_callback = inic_io_scan_done_cb;
	}

	if (scan_param.scan_report_each_mode_user_callback) {
		scan_param.scan_report_each_mode_user_callback = inic_io_scan_each_report_cb;
	}

	if (block && scan_param.scan_user_callback) {
		printk(KERN_ERR "%s, %s: not support user callback for block scan!!\n",\
		       INIC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (block) {
		block_scan_param.block = block;
		init_completion(&block_scan_param.scan_done);
		scan_param.scan_user_callback = &inic_io_block_scan_done_cb;
	}

	ret = wifi_scan_networks(iport, &scan_param, scan_arg.ssid_len, 0);

	if (block) {
		unsigned long residue_time = 0;

		residue_time = wait_for_completion_timeout(&block_scan_param.scan_done,\
							   msecs_to_jiffies(10 * 1000));
		if (residue_time == 0) {
			printk(KERN_ERR "%s, %s: scan timeout!!\n",\
			       INIC_CMD, __func__);
			ret = -EINVAL;
			goto func_exit;
		} else {
			ret = inic_io_scanned_ap_num;
		}
	}

func_exit:

	if (block_scan_param.block) {
		complete_release(&block_scan_param.scan_done);
		block_scan_param.block = 0;
	}

	return ret;
}

static int inic_io_set_connect(inic_port_t *iport, void *arg,\
			       int block)
{
	int ret = 0;
	rtw_network_info_t wifi = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}
	wifi.password = NULL;
	if (!iport || !iport->idev) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}


	ret = copy_from_user((uint8_t *)&wifi, (uint8_t __user *)arg,\
			     sizeof(rtw_network_info_t));
	if (ret) {
		printk("%s, %s: copy user_scan_arg usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (wifi.password && (wifi.password_len > 0)
	    && (wifi.password_len <= INIC_MAX_PASSWORD_LENGTH)) {
		memset(cmd_passwd, 0, INIC_MAX_PASSWORD_LENGTH);

		ret = copy_from_user((uint8_t *)cmd_passwd, (uint8_t __user *)wifi.password,\
				     wifi.password_len);
		if (ret) {
			printk("%s, %s: copy user_scan_arg usersapce failed (%d)!\n",
			       INIC_CMD, __func__, ret);
			ret = -EIO;
			goto func_exit;
		}
		wifi.password = cmd_passwd;
	} else if (wifi.password_len > INIC_MAX_PASSWORD_LENGTH) {
		printk(KERN_ERR "%s, %s: the length of password is exceeded!\n",\
		       INIC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (wifi.joinstatus_user_callback) {
		wifi.joinstatus_user_callback = &inic_io_join_status_cb;
	}

	ret = wifi_connect(iport, &wifi, block);

func_exit:
	return ret;
}

static int inic_io_get_join_status(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_join_status_t join_status = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	join_status = wifi_get_join_status();
	put_user(join_status, (unsigned int __user *)arg);

func_exit:
	return ret;
}

static int inic_io_get_scanned_ap_num(inic_port_t *iport, void *arg)
{
	int ret = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	put_user(inic_io_scanned_ap_num, (int __user *)arg);

func_exit:
	return ret;
}

static int inic_io_get_scan_result(inic_port_t *iport, void *arg)
{
	int ret = 0;
	uint8_t *scan_buf = NULL;
	int scanned_ap_num = inic_io_scanned_ap_num;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	scan_buf = (char *)kzalloc(scanned_ap_num * sizeof(rtw_scan_result_t), GFP_KERNEL);
	if (scan_buf == NULL) {
		ret = -ENOMEM;
		printk("%s, %s: alloc scan_buf failed!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	if (wifi_get_scan_records(iport, &scanned_ap_num, (uint8_t *)scan_buf) < 0) {
		ret = -EIO;
		printk("%s, %s: get scanned result failed!\n",
		       INIC_CMD, __func__);
		goto free_scan_buf;
	}

	if (scanned_ap_num > inic_io_scanned_ap_num) {
		scanned_ap_num = inic_io_scanned_ap_num;
	} else {
		inic_io_scanned_ap_num = scanned_ap_num;
	}

	ret = copy_to_user((uint8_t __user *)arg, (uint8_t *)scan_buf,\
			    scanned_ap_num * sizeof(rtw_scan_result_t));
	if (ret) {
		printk("%s, %s: copy scan_buf to usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto free_scan_buf;
	}

free_scan_buf:
	kfree(scan_buf);

func_exit:
	return ret;
}

static int inic_io_get_is_connected_to_ap(inic_port_t *iport, void *arg)
{
	int ret = 0, is_connedted = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	is_connedted = wifi_is_connected_to_ap();

	put_user(is_connedted, (int __user *)arg);

func_exit:
	return ret;
}

static int inic_io_set_disconnect(inic_port_t *iport)
{
	int ret = 0;

	ret = wifi_disconnect();

	return ret;
}

static int inic_io_set_wifi_on(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_mode_t mode = RTW_MODE_NONE;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	get_user(mode, (rtw_mode_t __user *)arg);

	ret = wifi_on(iport, mode);

func_exit:
	return ret;
}

static int inic_io_set_wifi_off(inic_port_t *iport)
{
	int ret = 0;

	ret = wifi_off(iport);

	return ret;
}

static int inic_io_get_mode(inic_port_t *iport, void *arg)
{
	int ret = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	put_user(iport->idev->mode, (rtw_mode_t __user *)arg);

func_exit:
	return ret;
}

static int inic_io_set_start_ap(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_softap_info_t ap_config = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	if (!iport || !iport->idev) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&ap_config, (uint8_t __user *)arg,\
			     sizeof(rtw_softap_info_t));
	if (ret) {
		printk("%s, %s: copy ap_config usersapce failed (%d)!\n",
		       INIC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (ap_config.password && (ap_config.password_len > 0)\
	    && (ap_config.password_len <= INIC_MAX_PASSWORD_LENGTH)) {
		memset(cmd_passwd, 0, INIC_MAX_PASSWORD_LENGTH);

		ret = copy_from_user((uint8_t *)cmd_passwd, (uint8_t __user *)ap_config.password,\
				     ap_config.password_len);
		if (ret) {
			printk("%s, %s: copy password usersapce failed (%d)!\n",
			       INIC_CMD, __func__, ret);
			ret = -EIO;
			goto func_exit;
		}
		ap_config.password = cmd_passwd;
	} else if (ap_config.password_len > INIC_MAX_PASSWORD_LENGTH) {
		printk(KERN_ERR "%s, %s: the length of password is exceeded!\n",\
		       INIC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (iport->idev->mode == RTW_MODE_STA_AP) {
		if (!iport->idev->piport[1]->ndev) {
			printk(KERN_ERR "%s, %s: no eth1!\n",\
			       INIC_CMD, __func__);
			ret = -EINVAL;
			goto func_exit;
		}

		if (iport->idev->piport[1]->state == INIC_STATE_DOWN) {
			dev_open(iport->idev->piport[1]->ndev, NULL);
		}
	}

	ret = wifi_start_ap(iport, &ap_config);

	if (ret == RTW_SUCCESS) {
		if (iport->idev->mode == RTW_MODE_STA_AP) {
			iport->idev->piport[1]->state = INIC_STATE_AP_UP;
			inic_net_indicate_connect(1);
		} else {
			iport->idev->piport[0]->state = INIC_STATE_AP_UP;
			inic_net_indicate_connect(0);
		}
	} else {
		if (iport->idev->mode == RTW_MODE_STA_AP) {
			if (!iport->idev->piport[1]->ndev) {
				printk(KERN_ERR "%s, %s: no eth1!\n",\
				       INIC_CMD, __func__);
				ret = -EINVAL;
				goto func_exit;
			}

			if (iport->idev->piport[1]->state != INIC_STATE_DOWN) {
				dev_close(iport->idev->piport[1]->ndev);
			}
		}
	}

func_exit:
	return ret;
}

static int inic_io_set_scan_abort(inic_port_t *iport)
{
	int ret = 0;

	ret = wifi_scan_abort();

	return ret;
}

static int inic_io_set_channel(inic_port_t *iport, void *arg)
{
	int ret = 0;
	int channel = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	get_user(channel, (int __user *)arg);

	ret = wifi_set_channel(channel);
	if (ret < 0) {
		printk("%s, %s: set wifi channel failed %d!\n",
		       INIC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_io_set_mode(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_mode_t mode = RTW_MODE_NONE;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	get_user(mode, (rtw_mode_t __user *)arg);

	ret = wifi_set_mode(iport, mode);
	if (ret < 0) {
		printk("%s, %s: set wifi mode failed %d!\n",
		       INIC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_io_get_wifi_is_running(inic_port_t *iport)
{
	int ret = 0;

	ret = wifi_is_running(iport->wlan_idx);

	return ret;
}

static int inic_io_get_channel(inic_port_t *iport, void *arg)
{
	int ret = 0;
	int channel = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_get_channel(iport, &channel);
	if (ret < 0) {
		printk("%s, %s: get wifi channel failed %d!\n",
		       INIC_CMD, __func__, ret);
		goto func_exit;
	}


	put_user(channel, (int __user *)arg);

func_exit:
	return ret;
}

static int inic_io_get_disconnected_reason_code(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned short reason_code = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_get_disconn_reason_code(iport, &reason_code);
	if (ret < 0) {
		printk("%s, %s: get wifi disconnected reason code failed %d!\n",
		       INIC_CMD, __func__, ret);
		goto func_exit;
	}


	put_user(reason_code, (unsigned short __user *)arg);

func_exit:
	return ret;
}

/* ---------------------------- Public Functions ---------------------------- */
void inic_io_wifi_ind_cb(rtw_event_indicate_t event_cmd,\
			 char *buf, int buf_len, int flags)
{
	inic_port_t *iport = p_inic_io_iport[INIC_STA_PORT];
	char *tem_buf = NULL, *buf_pos = NULL;
	int tem_len = 0;

	tem_len = buf_len + 2 * sizeof(int) + sizeof(rtw_event_indicate_t);

	tem_buf = kzalloc(buf_len, GFP_KERNEL);
	if (tem_buf == NULL) {
		printk(KERN_ERR "%s, %s: alloc buffer failed!\n",\
		       INIC_CMD, __func__);
		goto func_exit;
	}
	buf_pos = tem_buf;

	memcpy(buf_pos, &event_cmd, sizeof(rtw_event_indicate_t));
	buf_pos = buf_pos + sizeof(rtw_event_indicate_t);
	memcpy(buf_pos, &buf_len, sizeof(int));
	buf_pos = buf_pos + sizeof(int);
	memcpy(buf_pos, buf, buf_len);
	buf_pos = buf_pos + buf_len;
	memcpy(buf_pos, &flags, sizeof(int));

	inic_io_event_notify(iport, RINICIO_EVT_WIFIIND, tem_buf, tem_len);

	kfree(tem_buf);

func_exit:
	return;
}

/*
 * to initilize the inic command.
 * @iport[in]: the pointer to the inic_port.
 * @return if is OK, return 0, failed return negative.
 */
int inic_init_cmd(inic_port_t *iport)
{
	int ret = 0;
	/* init the multicast id for inic netlink */
	struct netlink_kernel_cfg nl_cfg = {
		.groups = RINICIO_NL_NOTIFY_GPID,
	};

	if (iport == NULL) {
		ret = -EINVAL;
		printk("%s, %s: iport is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}
	/* assigned iport to local for callback */
	p_inic_io_iport[iport->wlan_idx] = iport;

	/* init completion for block function */
	init_completion(&operation_done[iport->wlan_idx]);

	/* netlink */
	/* init inic netlink for event notify */
	iport->nl_sk = netlink_kernel_create(&init_net, RINICIO_NL_NOTIFY_EVENT, &nl_cfg);
	if (iport->nl_sk) {
		ret = -ENOMEM;
		printk("%s, %s: create nl event %d, failed.\n",
		       INIC_CMD, __func__, RINICIO_NL_NOTIFY_EVENT);
		goto func_exit;
	}

func_exit:
	return ret;
}

/*
 * to deinitilize the inic command.
 * @iport[in]: the pointer to the inic_port.
 * @return if is OK, return 0, failed return negative.
 */
int inic_deinit_cmd(inic_port_t *iport)
{
	int ret = 0;

	if (iport == NULL) {
		ret = -EINVAL;
		printk("%s, %s: iport is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}

	netlink_kernel_release(iport->nl_sk);
	iport->nl_sk = NULL;
	complete_release(&operation_done[iport->wlan_idx]);
	p_inic_io_iport[iport->wlan_idx] = NULL;

func_exit:
	return ret;
}

/*
 * @brief  to haddle the AT commmand from userspace.
 * @param  pcmd[in]: the parameter of command from userspace.
 * @return if is OK, return 0, failed return negative.
 */
int inic_ioctl_hdl(inic_port_t *iport, inic_io_cmd_t *pcmd)
{
	int ret = 0;
	int block = 0;
	int wlan_idx = 0;
	void *arg = NULL;

	if (pcmd == NULL) {
		ret = -EINVAL;
		printk("%s, %s: pcmd is NULL!\n",
		       INIC_CMD, __func__);
		goto func_exit;
	}
	wlan_idx = pcmd->index;
	arg = pcmd->pointer;

	switch (pcmd->cmd) {
	case RINICIO_S_SACN:
		block = (int)(pcmd->flags & RINICIO_FLAGS_BLOCK);
		ret = inic_io_set_scan(iport, arg, block);
		break;
	case RINICIO_S_CONNECT:
		block = (int)(pcmd->flags & RINICIO_FLAGS_BLOCK);
		ret = inic_io_set_connect(iport, arg, block);
		break;
	case RINICIO_S_DISCONNECT:
		ret = inic_io_set_disconnect(iport);
		break;
	case RINICIO_S_WIFION:
		ret = inic_io_set_wifi_on(iport, arg);
		break;
	case RINICIO_S_WIFIOFF:
		ret = inic_io_set_wifi_off(iport);
		break;
	case RINICIO_S_STARTAP:
		ret = inic_io_set_start_ap(iport, arg);
		break;
	case RINICIO_S_SCANABORT:
		ret = inic_io_set_scan_abort(iport);
		break;
	case RINICIO_S_CHANNEL:
		ret = inic_io_set_channel(iport, arg);
		break;
	case RINICIO_S_MODE:
		ret = inic_io_set_mode(iport, arg);
		break;
	case RINICIO_S_REGEVENT:
		ret = inic_io_set_reg_event(iport, arg);
		break;
	case RINICIO_S_UNREGEVENT:
		ret = inic_io_set_unreg_event(iport, arg);
		break;
	case RINICIO_S_INDEVENT:
		ret = inic_io_set_indicate_event(iport, arg);
		break;
	case RINICIO_G_JOINSTAUTS:
		ret = inic_io_get_join_status(iport, arg);
		break;
	case RINICIO_G_SCANNNEDAPNUM:
		ret = inic_io_get_scanned_ap_num(iport, arg);
		break;
	case RINICIO_G_SCANRESULT:
		ret = inic_io_get_scan_result(iport, arg);
		break;
	case RINICIO_G_ISCONNECTTOAP:
		ret = inic_io_get_is_connected_to_ap(iport, arg);
		break;
	case RINICIO_G_MODE:
		ret = inic_io_get_mode(iport, arg);
		break;
	case RINICIO_G_WIFIISRUNNING:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_io_get_wifi_is_running(iport);
		break;
	case RINICIO_G_CHANNEL:
		ret = inic_io_get_channel(iport, arg);
		break;
	case RINICIO_G_DISREASONCODE:
		ret = inic_io_get_disconnected_reason_code(iport, arg);
		break;
	default:
		printk("%s, %s: unknown inic cmd 0x%x.\n",
		       INIC_CMD, __func__, pcmd->cmd);
		break;
	}

func_exit:
	return ret;
}
