/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __INIC_PRMC_CMD_C__

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
#include "inic_prmc_cmd.h"
#include "inic_cmd.h"
#include "wifi_conf.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_PRMC_CMD "inic promisc cmd"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* -------------------------- Function declaration -------------------------- */

/* ---------------------------- Global Variables ---------------------------- */
ipc_promisc_callback_t ipc_promisc_callback = NULL;

/* --------------------------- Private Variables ---------------------------- */
static inic_port_t *p_inic_prmc_iport[INIC_MAX_NET_PORT_NUM] = {NULL};

/* --------------------------- Private Functions ---------------------------- */
static void inic_prmc_cb(unsigned char *in_buf, unsigned int in_buf_len, void *user_data)
{
	inic_port_t *iport = p_inic_prmc_iport[INIC_STA_PORT];
	char *buf = NULL;
	int len = 0;

	len = in_buf_len + sizeof(ieee80211_frame_info_t) + sizeof(unsigned int);

	buf = kzalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		printk(KERN_ERR "%s, %s: alloc ie buffer failed!\n",\
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	memcpy(buf, &in_buf_len, sizeof(unsigned int));
	memcpy(buf + sizeof(unsigned int), in_buf, in_buf_len);
	memcpy(buf + in_buf_len + sizeof(unsigned int), user_data, sizeof(ieee80211_frame_info_t));
	inic_io_event_notify(iport, RINICIO_EVT_PROMISC, buf, len);

	kfree(buf);
func_exit:
	return;
}

static int inic_prmc_set_promisc(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_prmc_setting_t prmc_setting = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&prmc_setting, (uint8_t __user *)arg,\
			     sizeof(inic_prmc_setting_t));
	if (ret) {
		printk("%s, %s: copy prmc_setting from usersapce failed (%d)!\n",
		       INIC_PRMC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (prmc_setting.cb_en) {
		ipc_promisc_callback = &inic_prmc_cb;
	}

	ret = promisc_set(prmc_setting.enabled, ipc_promisc_callback, prmc_setting.len_used);
	if (ret < 0) {
		printk("%s %s, set promisc failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_enter_promisc_mode(inic_port_t *iport)
{
	int ret = 0;
	rtw_wifi_setting_t setting = {0};

	if (!iport || !iport->idev) {
		printk(KERN_ERR "%s, %s: inic_port is NULL!\n",\
		       INIC_PRMC_CMD, __func__);
		ret = -EINVAL;
		goto func_exit;
	}

	if (iport->idev->mode == RTW_MODE_STA_AP) {
		ret = wifi_off(iport);
		if (ret < 0) {
			printk(KERN_ERR "%s, %s: wifi off failed!\n",\
			       INIC_PRMC_CMD, __func__);
			ret = -EIO;
			goto func_exit;
		}
		msleep(20);
		ret = wifi_on(iport, RTW_MODE_PROMISC);
		if (ret < 0) {
			printk(KERN_ERR "%s, %s: wifi on to promisc failed!\n",\
			       INIC_PRMC_CMD, __func__);
			ret = -EIO;
			goto func_exit;
		}
	} else if (iport->idev->mode == RTW_MODE_AP) {
		ret = wifi_set_mode(iport, RTW_MODE_PROMISC);
		if (ret < 0) {
			printk(KERN_ERR "%s, %s: set wifi to promisc failed!\n",\
			       INIC_PRMC_CMD, __func__);
			ret = -EIO;
			goto func_exit;
		}
	} else if (iport->idev->mode == RTW_MODE_STA) {
		if (strlen((const char *)setting.ssid) > 0) {
			ret = wifi_disconnect();
		}
	}

func_exit:
	return ret;
}

static int inic_prmc_init_packet_filter(void)
{
	int ret = 0;

	ret = promisc_init_packet_filter();

	return ret;
}

static int inic_prmc_add_packet_filter(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_prmc_pkt_filter_t pkt_filter = {0};
	rtw_packet_filter_pattern_t *patt = NULL;
	__u8 *mask = NULL, *pattern = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&pkt_filter, (uint8_t __user *)arg,\
			     sizeof(inic_prmc_pkt_filter_t));
	if (ret) {
		printk("%s, %s: copy pkt_filter from usersapce failed (%d)!\n",
		       INIC_PRMC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}
	patt = pkt_filter.patt;

	if (patt->mask) {
		mask = kzalloc(patt->mask_size, GFP_KERNEL);
		if (mask == NULL) {
			printk(KERN_ERR "%s, %s: alloc mask buffer failed!\n",\
			       INIC_PRMC_CMD, __func__);
			goto func_exit;
		}
		ret = copy_from_user((uint8_t *)&mask, (uint8_t __user *)patt->mask,\
				     patt->mask_size);
		if (ret) {
			printk("%s, %s: copy mask from usersapce failed (%d)!\n",
			       INIC_PRMC_CMD, __func__, ret);
			ret = -EIO;
			goto free_mask;
		}
		patt->mask = mask;
	}

	if (patt->pattern) {
		pattern = kzalloc(patt->mask_size, GFP_KERNEL);
		if (pattern == NULL) {
			printk(KERN_ERR "%s, %s: alloc pattern buffer failed!\n",\
			       INIC_PRMC_CMD, __func__);
			goto free_mask;
		}
		ret = copy_from_user((uint8_t *)&pattern, (uint8_t __user *)patt->mask,\
				     patt->mask_size);
		if (ret) {
			printk("%s, %s: copy pattern from usersapce failed (%d)!\n",
			       INIC_PRMC_CMD, __func__, ret);
			ret = -EIO;
			goto free_pattern;
		}
		patt->pattern = pattern;
	}

	ret = promisc_add_packet_filter(iport, pkt_filter.filter_id,\
					patt, pkt_filter.rule);
free_pattern:
	kfree(pattern);

free_mask:
	kfree(mask);

func_exit:
	return ret;
}

static int inic_prmc_en_pkt_filter(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char filter_id;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	get_user(filter_id, (unsigned char __user *)arg);

	ret = promisc_enable_packet_filter(filter_id);
	if (ret < 0) {
		printk("%s, %s: enable packet filter failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_dis_pkt_filter(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char filter_id;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	get_user(filter_id, (unsigned char __user *)arg);

	ret = promisc_disable_packet_filter(filter_id);
	if (ret < 0) {
		printk("%s, %s: diaable packet filter failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_rm_pkt_filter(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char filter_id;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	get_user(filter_id, (unsigned char __user *)arg);

	ret = promisc_remove_packet_filter(filter_id);
	if (ret < 0) {
		printk("%s, %s: remove packet filter failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_filter_retransmit_pkt(inic_port_t *iport, void *arg)
{
	int ret = 0;
	__u32 data = 0;
	__u8 enable = 0, filter_interval_ms = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	get_user(data, (__u32 __user *)arg);
	enable = (__u8)(data & 0xff);
	filter_interval_ms = (__u8)((data >> 8) & 0xff);

	ret = promisc_filter_retransmit_pkt(enable, filter_interval_ms);
	if (ret < 0) {
		printk("%s, %s: retransmit packet filter failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_filter_by_ap_and_sta_mac(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_prmc_mac_filter_t mac_filter = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&mac_filter, (uint8_t __user *)arg,\
			     sizeof(inic_prmc_mac_filter_t));
	if (ret) {
		printk("%s, %s: copy mac filter from usersapce failed (%d)!\n",
		       INIC_PRMC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = promisc_filter_by_ap_and_phone_mac(iport, mac_filter.enable,\
						 mac_filter.ap_mac,\
						 mac_filter.sta_mac);

func_exit:
	return ret;
}

static int inic_prmc_filter_with_len(inic_port_t *iport, void *arg)
{
	int ret = 0;
	__u16 len = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	get_user(len, (__u32 __user *)arg);

	ret = promisc_filter_with_len(len);
	if (ret < 0) {
		printk("%s, %s: set filter with length failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_is_promisc_enabled(void)
{
	int ret = 0;

	ret = is_promisc_enabled();

	return ret;
}

static int inic_prmc_get_fixed_channel(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_prmc_fixed_ch_param_t param = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	ret = promisc_get_fixed_channel(iport, param.fixed_bssid,\
					param.ssid, &param.ssid_len);
	if (ret < 0) {
		printk("%s, %s: get fixed channel failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

	ret = copy_to_user((uint8_t __user *)arg, (uint8_t *)&param,\
			     sizeof(inic_prmc_fixed_ch_param_t));
	if (ret) {
		printk("%s, %s: copy inic_prmc_fixed_ch_param_t to usersapce failed (%d)!\n",
		       INIC_PRMC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_set_mgntframe(inic_port_t *iport, void *arg)
{
	int ret = 0;
	__u8 enable = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	get_user(enable, (__u32 __user *)arg);

	ret = promisc_set_mgntframe(enable);
	if (ret < 0) {
		printk("%s, %s: set mgntframe failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_get_chnl_by_bssid(inic_port_t *iport, void *arg)
{
	int ret = 0;
	__u8 bssid[ETH_ALEN] = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&bssid, (uint8_t __user *)arg,\
			     sizeof(ETH_ALEN));
	if (ret) {
		printk("%s, %s: copy bssid from usersapce failed (%d)!\n",
		       INIC_PRMC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = promisc_get_chnl_by_bssid(iport, bssid);

func_exit:
	return ret;
}

static int inic_prmc_update_candi_ap_rssi_avg(inic_port_t *iport, void *arg)
{
	int ret = 0;
	__u32 data = 0;
	__s8 rssi = 0;
	__u8 cnt = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	get_user(data, (__u32 __user *)arg);
	rssi = (__s8)(data & 0xff);
	cnt = (__u8)((data >> 8) & 0xff);

	ret = promisc_update_candi_ap_rssi_avg(rssi, cnt);
	if (ret < 0) {
		printk("%s, %s: update candidate ap rssi avg failed %d!\n",
		       INIC_PRMC_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_prmc_issue_probersp(inic_port_t *iport, void *arg)
{
	int ret = 0;
	__u8 da[ETH_ALEN] = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&da, (uint8_t __user *)arg,\
			     sizeof(ETH_ALEN));
	if (ret) {
		printk("%s, %s: copy da from usersapce failed (%d)!\n",
		       INIC_PRMC_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = promisc_issue_probersp(iport, da);

func_exit:
	return ret;
}

/* ---------------------------- Public Functions ---------------------------- */
int inic_init_prmc_cmd(inic_port_t *iport)
{
	int ret = 0;

	if (iport == NULL) {
		ret = -EINVAL;
		printk("%s, %s: iport is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	/* assigned iport to local for callback */
	p_inic_prmc_iport[iport->wlan_idx] = iport;

func_exit:
	return ret;
}

int inic_deinit_prmc_cmd(inic_port_t *iport)
{
	int ret = 0;

	if (iport == NULL) {
		ret = -EINVAL;
		printk("%s, %s: iport is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}

	p_inic_prmc_iport[iport->wlan_idx] = NULL;

func_exit:
	return ret;
}

int inic_prmc_ioctl_hdl(inic_port_t *iport, inic_io_cmd_t *pcmd)
{
	int ret = 0;
	int wlan_idx = 0;
	void *arg = NULL;

	if (pcmd == NULL) {
		ret = -EINVAL;
		printk("%s, %s: pcmd is NULL!\n",
		       INIC_PRMC_CMD, __func__);
		goto func_exit;
	}
	wlan_idx = pcmd->index;
	arg = pcmd->pointer;

	switch (pcmd->cmd) {
	case RINICIO_P_SETPROMISC:
		ret = inic_prmc_set_promisc(iport, arg);
		break;
	case RINICIO_P_ENTERPROMISCMODE:
		ret = inic_prmc_enter_promisc_mode(iport);
		break;
	case RINICIO_P_INITPACKETFILTER:
		ret = inic_prmc_init_packet_filter();
		break;
	case RINICIO_P_ADDPACKETFILTER:
		ret = inic_prmc_add_packet_filter(iport, arg);
		break;
	case RINICIO_P_ENPACKETFILTER:
		ret = inic_prmc_en_pkt_filter(iport, arg);
		break;
	case RINICIO_P_DISPACKETFILTER:
		ret = inic_prmc_dis_pkt_filter(iport, arg);
		break;
	case RINICIO_P_RMPACKETFILTER:
		ret = inic_prmc_rm_pkt_filter(iport, arg);
		break;
	case RINICIO_P_RETXPACKETFILTER:
		ret = inic_prmc_filter_retransmit_pkt(iport, arg);
		break;
	case RINICIO_P_FILTERBYAPANDMAC:
		ret = inic_prmc_filter_by_ap_and_sta_mac(iport, arg);
		break;
	case RINICIO_P_FILTERWITHLEN:
		ret = inic_prmc_filter_with_len(iport, arg);
		break;
	case RINICIO_P_ISPROMISCENABLE:
		ret = inic_prmc_is_promisc_enabled();
		break;
	case RINICIO_P_GETFIXEDCHANNEL:
		ret = inic_prmc_get_fixed_channel(iport, arg);
		break;
	case RINICIO_P_SETMGNTFRAME:
		ret = inic_prmc_set_mgntframe(iport, arg);
		break;
	case RINICIO_P_GETCHNLBYBSSID:
		ret = inic_prmc_get_chnl_by_bssid(iport, arg);
		break;
	case RINICIO_P_UPDATECANDIAPRSSIAVG:
		ret = inic_prmc_update_candi_ap_rssi_avg(iport, arg);
		break;
	case RINICIO_P_STOPTXBEACON:
		ret = promisc_stop_tx_beacn();
		break;
	case RINICIO_P_RESUMETXBEACON:
		ret = promisc_resume_tx_beacn();
		break;
	case RINICIO_P_ISSUEPROBERSP:
		ret = inic_prmc_issue_probersp(iport, arg);
		break;
	default:
		printk("%s, %s: unknown inic promisc cmd 0x%x.\n",
		       INIC_PRMC_CMD, __func__, pcmd->cmd);
		break;
	}

func_exit:
	return ret;
}