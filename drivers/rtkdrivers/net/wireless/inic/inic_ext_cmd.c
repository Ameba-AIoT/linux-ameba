/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
 /******************************************************************************
  * history *
 ******************************************************************************/
#define __INIC_EXT_CMD_C__

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
#include "inic_ext_cmd.h"
#include "inic_cmd.h"

/* -------------------------------- Defines --------------------------------- */
#define INIC_EXT_CMD "inic ext cmd"

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* -------------------------- Function declaration -------------------------- */

/* ---------------------------- Global Variables ---------------------------- */
p_wlan_autoreconnect_hdl_t p_wlan_autoreconnect_hdl = NULL;

/* --------------------------- Private Variables ---------------------------- */
static inic_port_t *p_inic_ext_iport[INIC_MAX_NET_PORT_NUM] = {NULL};

/* --------------------------- Private Functions ---------------------------- */
static void inic_ext_ap_channel_switch_cb(unsigned char channel, rtw_channel_switch_res_t ret)
{
	inic_port_t *iport = p_inic_ext_iport[INIC_AP_PORT];
	char buf[sizeof(rtw_channel_switch_res_t) + 1] = {0};

	buf[0] = channel;
	memcpy(&buf[1], &ret, sizeof(rtw_channel_switch_res_t));
	inic_io_event_notify(iport, RINICIO_EVT_APCHLSWITCH, buf, sizeof(rtw_channel_switch_res_t) + 1);

	return;
}

static void inic_ext_autoreconnect_cb(wifi_autoreconn_param_t *recon_param)
{
	inic_port_t *iport = p_inic_ext_iport[INIC_AP_PORT];
	char *buf = NULL, *ssid_pos = NULL, *passwd_pos = NULL;
	int len = 0;

	len = sizeof(wifi_autoreconn_param_t) + recon_param->ssid_len + recon_param->password_len;

	buf = kzalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		printk(KERN_ERR "%s, %s: alloc buffer failed!\n",\
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}
	memcpy(buf, recon_param, sizeof(wifi_autoreconn_param_t));
	ssid_pos = buf + sizeof(wifi_autoreconn_param_t);
	memcpy(ssid_pos, recon_param->ssid, recon_param->ssid_len);
	passwd_pos = ssid_pos + recon_param->password_len;
	memcpy(passwd_pos, recon_param->password, recon_param->password_len);
	inic_io_event_notify(iport, RINICIO_EVT_AUTORECONNECT, buf, len);

func_exit:
	return;
}

static int inic_ext_set_psk_info(inic_port_t *iport, void *arg)
{
	int ret = 0;
	struct psk_info info = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&info, (uint8_t __user *)arg,\
			     sizeof(struct psk_info));
	if (ret) {
		printk("%s, %s: copy psk_info usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = wifi_psk_info_set(iport, &info);
	if (ret < 0) {
		printk("%s, %s: set psk info failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_ble_scan_duty(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u8 duty = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(duty, (u8 __user *)arg);

	ret = wifi_btcoex_set_ble_scan_duty(duty);
	if (ret < 0) {
		printk("%s, %s: set ble scan duty failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_powersave_mode(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u32 mode = 0;
	u8 ips_mode = 0, lps_mode = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(mode, (u32 __user *)arg);
	ips_mode = (u8)(mode & 0xff);
	lps_mode = (u8)((mode >> 8) & 0xff);

	ret = wifi_set_powersave_mode(ips_mode, lps_mode);
	if (ret < 0) {
		printk("%s, %s: set powersave mode failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_mfp_support(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u8 value = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(value, (u8 __user *)arg);

	ret = wifi_set_mfp_support(value);
	if (ret < 0) {
		printk("%s, %s: set MFP support failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_group_id(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u8 value = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(value, (u8 __user *)arg);

	ret = wifi_set_group_id(value);
	if (ret < 0) {
		printk("%s, %s: set group ID failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_pmk_cache_enable(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u8 value = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(value, (u8 __user *)arg);

	ret = wifi_set_pmk_cache_enable(value);
	if (ret < 0) {
		printk("%s, %s: set PMK cache enable failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_wps_phase(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u8 is_trigger_wps = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(is_trigger_wps, (u8 __user *)arg);

	ret = wifi_set_wps_phase(is_trigger_wps);
	if (ret < 0) {
		printk("%s, %s: set WPS phase failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_network_mode(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_network_mode_t mode = RTW_NETWORK_B;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(mode, (rtw_network_mode_t __user *)arg);

	ret = wifi_set_network_mode(mode);
	if (ret < 0) {
		printk("%s, %s: set wireless network mode failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_gen_ie(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_ext_buf_type_t ie_param = {0};
	char *buf = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&ie_param, (uint8_t __user *)arg,\
			     sizeof(inic_ext_buf_type_t));
	if (ret) {
		printk("%s, %s: copy ie_param usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (ie_param.buf && (ie_param.buf_len > 0)) {
		buf = kzalloc(ie_param.buf_len, GFP_KERNEL);
		if (buf == NULL) {
			printk(KERN_ERR "%s, %s: alloc ie buffer failed!\n",\
			       INIC_EXT_CMD, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}

		ret = copy_from_user((uint8_t *)buf, (uint8_t __user *)ie_param.buf,\
				     ie_param.buf_len);
		if (ret) {
			printk("%s, %s: copy ie buffer from usersapce failed (%d)!\n",
			       INIC_EXT_CMD, __func__, ret);
			ret = -EIO;
			goto free_buf;
		}
	}

	ret = wifi_set_gen_ie(iport, (u8)iport->wlan_idx, buf, ie_param.buf_len, ie_param.flags);
	if (ret < 0) {
		printk("%s, %s: set gen IE failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto free_buf;
	}

free_buf:
	kfree(buf);

func_exit:
	return ret;
}

static int inic_ext_set_eap_phase(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u8 is_trigger_eap = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(is_trigger_eap, (u8 __user *)arg);

	ret = wifi_set_eap_phase(is_trigger_eap);
	if (ret < 0) {
		printk("%s, %s: set EAP phase mode failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_eap_method(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u8 eap_method = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(eap_method, (u8 __user *)arg);

	ret = wifi_set_eap_method(eap_method);
	if (ret < 0) {
		printk("%s, %s: set EAP method mode failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_send_eapol(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_ext_buf_type_t eapol_buf = {0};
	char *buf = NULL;
	char if_wlan0[] = "wlan0", if_wlan1[] = "wlan1";
	char *ifname = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&eapol_buf, (uint8_t __user *)arg,\
			     sizeof(inic_ext_buf_type_t));
	if (ret) {
		printk("%s, %s: copy inic_ext_buf_type_t from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (eapol_buf.buf && (eapol_buf.buf_len > 0)) {
		buf = kzalloc(eapol_buf.buf_len, GFP_KERNEL);
		if (buf == NULL) {
			printk(KERN_ERR "%s, %s: alloc eapol buffer failed!\n",\
			       INIC_EXT_CMD, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}

		ret = copy_from_user((uint8_t *)buf, (uint8_t __user *)eapol_buf.buf,\
				     eapol_buf.buf_len);
		if (ret) {
			printk("%s, %s: copy buf from usersapce failed (%d)!\n",
			       INIC_EXT_CMD, __func__, ret);
			ret = -EIO;
			goto free_buf;
		}
	}

	if (iport->wlan_idx == 0) {
		ifname = if_wlan0;
	} else {
		ifname = if_wlan1;
	}

	ret = wifi_send_eapol(iport, ifname, buf, eapol_buf.buf_len, eapol_buf.flags);
	if (ret < 0) {
		printk("%s, %s: set to send EAPOL failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto free_buf;
	}

free_buf:
	kfree(buf);

func_exit:
	return ret;
}

static int inic_ext_set_custom_ie(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_ext_buf_type_t cus_ie_buf = {0};
	void *buf = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&cus_ie_buf, (uint8_t __user *)arg,\
			     sizeof(inic_ext_buf_type_t));
	if (ret) {
		printk("%s, %s: copy inic_ext_buf_type_t from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (cus_ie_buf.buf && (cus_ie_buf.buf_len > 0)) {
		buf = kzalloc(cus_ie_buf.buf_len, GFP_KERNEL);
		if (buf == NULL) {
			printk(KERN_ERR "%s, %s: alloc IE buffer failed!\n",\
			       INIC_EXT_CMD, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}

		ret = copy_from_user((uint8_t *)buf, (uint8_t __user *)cus_ie_buf.buf,\
				     cus_ie_buf.buf_len);
		if (ret) {
			printk("%s, %s: copy IE buffer from usersapce failed (%d)!\n",
			       INIC_EXT_CMD, __func__, ret);
			ret = -EIO;
			goto free_buf;
		}
	}

	ret = wifi_set_custom_ie(iport, cus_ie_buf.flags, buf, (int)cus_ie_buf.buf_len);
	if (ret < 0) {
		printk("%s, %s: set to add custom IE failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto free_buf;
	}

free_buf:
	kfree(buf);

func_exit:
	return ret;
}

static int inic_ext_set_indicate_mgnt(inic_port_t *iport, void *arg)
{
	int ret = 0;
	int enable = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(enable, (int __user *)arg);

	wifi_set_indicate_mgnt(enable);
func_exit:
	return ret;
}

static int inic_ext_set_send_raw_frame(inic_port_t *iport, void *arg)
{
	int ret = 0;
	raw_data_desc_t raw_data_desc = {0};
	char *buf = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&raw_data_desc, (uint8_t __user *)arg,\
			     sizeof(raw_data_desc_t));
	if (ret) {
		printk("%s, %s: copy raw_data_desc_t from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	if (raw_data_desc.buf && (raw_data_desc.buf_len > 0)) {
		buf = kzalloc(raw_data_desc.buf_len, GFP_KERNEL);
		if (buf == NULL) {
			printk(KERN_ERR "%s, %s: alloc buf failed!\n",\
			       INIC_EXT_CMD, __func__);
			ret = -ENOMEM;
			goto func_exit;
		}

		ret = copy_from_user((uint8_t *)buf, (uint8_t __user *)raw_data_desc.buf,\
				     raw_data_desc.buf_len);
		if (ret) {
			printk("%s, %s: copy buf from usersapce failed (%d)!\n",
			       INIC_EXT_CMD, __func__, ret);
			ret = -EIO;
			goto free_buf;
		}
		raw_data_desc.buf = buf;
	}

	ret = wifi_send_raw_frame(iport, &raw_data_desc);
	if (ret < 0) {
		printk("%s, %s: set to send RAW frame failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto free_buf;
	}

free_buf:
	kfree(buf);

func_exit:
	return ret;
}

static int inic_ext_set_tx_rate_by_tos(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u32 data = 0;
	u8 enable = 0, tos_precedence = 0, tx_rate = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(data, (u32 __user *)arg);
	enable = (u8)(data & 0xff);
	tos_precedence = (u8)((data >> 8) & 0xff);
	tx_rate = (u8)((data >> 16) & 0xff);

	ret = wifi_set_tx_rate_by_ToS(enable, tos_precedence, tx_rate);
	if (ret < 0) {
		printk("%s, %s: set tx rate by ToS failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_edca_param(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned int ac_param = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(ac_param, (unsigned int __user *)arg);

	ret = wifi_set_EDCA_param(ac_param);
	if (ret < 0) {
		printk("%s, %s: set EDCA parameters failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_tx_cca(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned int enable = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(enable, (unsigned int __user *)arg);

	ret = wifi_set_TX_CCA(enable);
	if (ret < 0) {
		printk("%s, %s: set TX CCA failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_cts2self_duration_and_send(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned short duration = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(duration, (unsigned short __user *)arg);

	ret = wifi_set_cts2self_duration_and_send(iport->wlan_idx, duration);
	if (ret < 0) {
		printk("%s, %s: set CTS to sel duration and send failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_add_mac_filter(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char hwaddr[ETH_ALEN] = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)hwaddr, (uint8_t __user *)arg,\
			     ETH_ALEN);
	if (ret) {
		printk("%s, %s: copy hardware address from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = wifi_add_mac_filter(iport, hwaddr);
	if (ret < 0) {
		printk("%s, %s: add MAC filter failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_del_mac_filter(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char hwaddr[ETH_ALEN] = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)hwaddr, (uint8_t __user *)arg,\
			     ETH_ALEN);
	if (ret) {
		printk("%s, %s: copy hardware address from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = wifi_del_mac_filter(iport, hwaddr);
	if (ret < 0) {
		printk("%s, %s: delete MAC filter failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_del_station(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char hwaddr[ETH_ALEN] = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)hwaddr, (uint8_t __user *)arg,\
			     ETH_ALEN);
	if (ret) {
		printk("%s, %s: copy hardware address from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	ret = wifi_del_station(iport, iport->wlan_idx, hwaddr);
	if (ret < 0) {
		printk("%s, %s: delete station failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_ap_switch_chl_and_inform(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u32 data = 0;
	u8 new_chl = 0, chl_switch_cnt = 0, callback_en = 0;
	ap_channel_switch_callback_t callback = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(data, (u32 __user *)arg);
	new_chl = (u8)(data & 0xff);
	chl_switch_cnt = (u8)((data >> 8) & 0xff);
	callback_en = (u8)((data >> 16) & 0xff);

	if (callback_en) {
		callback = inic_ext_ap_channel_switch_cb;
	}

	ret = wifi_ap_switch_chl_and_inform(new_chl, chl_switch_cnt, callback);
	if (ret < 0) {
		printk("%s %s, set AP to switch channel and inform failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_autoreconnect(inic_port_t *iport, void *arg)
{
	int ret = 0;
	u32 data = 0;
	u8 mode = 0, retry_times = 0;
	u16 timeout = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(data, (u32 __user *)arg);
	mode = (u8)(data & 0xff);
	retry_times = (u8)((data >> 8) & 0xff);
	timeout = (u16)((data >> 16) & 0xff);

	if (mode == RTW_AUTORECONNECT_DISABLE) {
		p_wlan_autoreconnect_hdl = NULL;
	} else {
		p_wlan_autoreconnect_hdl = inic_ext_autoreconnect_cb;
	}


	ret = wifi_config_autoreconnect(mode, retry_times, timeout);
	if (ret < 0) {
		printk("%s %s, set AP to switch channel and inform failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_set_set_no_beacon_timeout(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned short timeout_sec = 0;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	get_user(timeout_sec, (unsigned short __user *)arg);

	ret = wifi_set_no_beacon_timeout(timeout_sec);
	if (ret < 0) {
		printk("%s, %s: set no beacon timeout failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_get_psk_info(inic_port_t *iport, void *arg)
{
	int ret = 0;
	struct psk_info info = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_psk_info_get(iport, &info);
	if (ret < 0) {
		printk("%s, %s: get psk info failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

	ret = copy_to_user((uint8_t __user *)arg, (uint8_t *)&info,\
			     sizeof(struct psk_info));
	if (ret) {
		printk("%s, %s: copy psk_info to usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_get_mac_address(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_mac_t mac = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_get_mac_address(iport, &mac);
	if (ret < 0) {
		printk("%s, %s: get wifi setting failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

	ret = copy_to_user((uint8_t __user *)arg, (uint8_t *)&mac,\
			    sizeof(rtw_mac_t));
	if (ret) {
		printk("%s, %s: copy MAC address to usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_get_associated_client_list(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_ext_buf_type_t client_list = {0};
	char *buf = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&client_list, (uint8_t __user *)arg,\
			     sizeof(inic_ext_buf_type_t));
	if (ret) {
		printk("%s, %s: copy client_list buf from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	buf = kzalloc(client_list.buf_len, GFP_KERNEL);
	if (buf == NULL) {
		printk(KERN_ERR "%s, %s: alloc ie buffer failed!\n",\
		       INIC_EXT_CMD, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)buf, (uint8_t __user *)client_list.buf,\
			     client_list.buf_len);
	if (ret) {
		printk("%s, %s: copy buf from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto free_buf;
	}

	ret = wifi_get_associated_client_list(iport, buf, client_list.buf_len);
	if (ret < 0) {
		printk("%s, %s: get associated client list failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto free_buf;
	}

	ret = copy_to_user((uint8_t __user *)client_list.buf, (uint8_t *)buf,\
			     client_list.buf_len);
	if (ret) {
		printk("%s, %s: copy client_list buf to usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto free_buf;
	}

free_buf:
	kfree(buf);

func_exit:
	return ret;
}

static int inic_ext_get_setting(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_wifi_setting_t setting = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_get_setting(iport, &setting);
	if (ret < 0) {
		printk("%s, %s: get wifi setting failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

	ret = copy_to_user((uint8_t __user *)arg, (uint8_t *)&setting,\
			    sizeof(rtw_wifi_setting_t));
	if (ret) {
		printk("%s, %s: copy scan_buf to usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_get_sw_statistic(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_sw_statistics_t sw_stats = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_get_sw_statistic(iport, iport->wlan_idx, &sw_stats);
	if (ret < 0) {
		printk("%s, %s: get SW statistics failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

	ret = copy_to_user((uint8_t __user *)arg, (uint8_t *)&sw_stats,\
			    sizeof(rtw_sw_statistics_t));
	if (ret) {
		printk("%s, %s: copy SW statistics to usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_get_phy_statistic(inic_port_t *iport, void *arg)
{
	int ret = 0;
	rtw_phy_statistics_t phy_stats = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_fetch_phy_statistic(iport, &phy_stats);
	if (ret < 0) {
		printk("%s, %s: get PHY statistics failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

	ret = copy_to_user((uint8_t __user *)arg, (uint8_t *)&phy_stats,\
			    sizeof(rtw_phy_statistics_t));
	if (ret) {
		printk("%s, %s: copy PHY statistics to usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

func_exit:
	return ret;
}

static int inic_ext_get_antenna_info(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char ant_info = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_get_antenna_info(iport, &ant_info);
	if (ret < 0) {
		printk("%s, %s: get PHY statistics failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

	put_user(ant_info, (unsigned char __user *)arg);

func_exit:
	return ret;
}

static int inic_ext_get_auto_channel(inic_port_t *iport, void *arg)
{
	int ret = 0;
	inic_ext_buf_type_t ch_buf = {0};
	char *buf = NULL;

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = copy_from_user((uint8_t *)&ch_buf, (uint8_t __user *)arg,\
		     sizeof(inic_ext_buf_type_t));
	if (ret) {
		printk("%s, %s: copy ch_buf buf from usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto func_exit;
	}

	buf = kzalloc(ch_buf.buf_len, GFP_KERNEL);
	if (buf == NULL) {
		printk(KERN_ERR "%s, %s: alloc ie buffer failed!\n",\
		       INIC_EXT_CMD, __func__);
		ret = -ENOMEM;
		goto func_exit;
	}

	ret = wifi_get_auto_chl(iport, iport->wlan_idx, buf, ch_buf.buf_len);
	if (ret < 0) {
		printk("%s, %s: get associated client list failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto free_buf;
	}

	ret = copy_to_user((uint8_t __user *)ch_buf.buf, (uint8_t *)buf,\
			     ch_buf.buf_len);
	if (ret) {
		printk("%s, %s: copy ch_buf buf to usersapce failed (%d)!\n",
		       INIC_EXT_CMD, __func__, ret);
		ret = -EIO;
		goto free_buf;
	}

free_buf:
	kfree(buf);

func_exit:
	return ret;
}

static int inic_ext_get_autoreconnect(inic_port_t *iport, void *arg)
{
	int ret = 0;
	unsigned char mode = {0};

	if (!arg) {
		ret = -EINVAL;
		printk("%s, %s: input argument is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	ret = wifi_get_autoreconnect(iport, &mode);
	if (ret < 0) {
		printk("%s, %s: get PHY statistics failed %d!\n",
		       INIC_EXT_CMD, __func__, ret);
		goto func_exit;
	}

	put_user(mode, (unsigned char __user *)arg);

func_exit:
	return ret;
}

/* ---------------------------- Public Functions ---------------------------- */
int inic_init_ext_cmd(inic_port_t *iport)
{
	int ret = 0;

	if (iport == NULL) {
		ret = -EINVAL;
		printk("%s, %s: iport is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	/* assigned iport to local for callback */
	p_inic_ext_iport[iport->wlan_idx] = iport;

func_exit:
	return ret;
}

int inic_deinit_ext_cmd(inic_port_t *iport)
{
	int ret = 0;

	if (iport == NULL) {
		ret = -EINVAL;
		printk("%s, %s: iport is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}

	p_inic_ext_iport[iport->wlan_idx] = NULL;

func_exit:
	return ret;
}

int inic_ext_ioctl_hdl(inic_port_t *iport, inic_io_cmd_t *pcmd)
{
	int ret = 0;
	int wlan_idx = 0;
	void *arg = NULL;

	if (pcmd == NULL) {
		ret = -EINVAL;
		printk("%s, %s: pcmd is NULL!\n",
		       INIC_EXT_CMD, __func__);
		goto func_exit;
	}
	wlan_idx = pcmd->index;
	arg = pcmd->pointer;

	switch (pcmd->cmd) {
	case RINICIO_ES_PSKINFO:
		ret = inic_ext_set_psk_info(iport, arg);
		break;
	case RINICIO_ES_COEXBLESCANDUTY:
		ret = inic_ext_set_ble_scan_duty(iport, arg);
		break;
	case RINICIO_ES_POWERSAVEMODE:
		ret = inic_ext_set_powersave_mode(iport, arg);
		break;
	case RINICIO_ES_MFPSUPPORT:
		ret = inic_ext_set_mfp_support(iport, arg);
		break;
	case RINICIO_ES_GROUPID:
		ret = inic_ext_set_group_id(iport, arg);
		break;
	case RINICIO_ES_PMKCACHEENABLE:
		ret = inic_ext_set_pmk_cache_enable(iport, arg);
		break;
	case RINICIO_ES_NETWORKMODE:
		ret = inic_ext_set_network_mode(iport, arg);
		break;
	case RINICIO_ES_WPSPHASE:
		ret = inic_ext_set_wps_phase(iport, arg);
		break;
	case RINICIO_ES_GENIE:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_ext_set_gen_ie(iport, arg);
		break;
	case RINICIO_ES_EAPPHASE:
		ret = inic_ext_set_eap_phase(iport, arg);
		break;
	case RINICIO_ES_EAPMETHOD:
		ret = inic_ext_set_eap_method(iport, arg);
		break;
	case RINICIO_ES_SENDEAPOL:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_ext_set_send_eapol(iport, arg);
		break;
	case RINICIO_ES_CUSTOMIE:
		ret = inic_ext_set_custom_ie(iport, arg);
		break;
	case RINICIO_ES_INDICATEMGNT:
		ret = inic_ext_set_indicate_mgnt(iport, arg);
		break;
	case RINICIO_ES_SENDRAWFRAME:
		ret = inic_ext_set_send_raw_frame(iport, arg);
		break;
	case RINICIO_ES_TXRATEBYTOS:
		ret = inic_ext_set_tx_rate_by_tos(iport, arg);
		break;
	case RINICIO_ES_EDCAPARAM:
		ret = inic_ext_set_edca_param(iport, arg);
		break;
	case RINICIO_ES_TXCCA:
		ret = inic_ext_set_tx_cca(iport, arg);
		break;
	case RINICIO_ES_CTS2SELF:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_ext_set_cts2self_duration_and_send(iport, arg);
		break;
	case RINICIO_ES_INITMACFILTER:
		ret = wifi_init_mac_filter();
		break;
	case RINICIO_ES_ADDMACFILTER:
		ret = inic_ext_set_add_mac_filter(iport, arg);
		break;
	case RINICIO_ES_DELMACFILTER:
		ret = inic_ext_set_del_mac_filter(iport, arg);
		break;
	case RINICIO_ES_DELSTA:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_ext_set_del_station(iport, arg);
		break;
	case RINICIO_ES_APSWITCHCHLANDINFORM:
		ret = inic_ext_set_ap_switch_chl_and_inform(iport, arg);
		break;
	case RINICIO_ES_AUTORECONNECT:
		ret = inic_ext_set_autoreconnect(iport, arg);
		break;
	case RINICIO_ES_NOBEACONTIMEOUT:
		ret = inic_ext_set_set_no_beacon_timeout(iport, arg);
		break;
	case RINICIO_EG_PSKINFO:
		ret = inic_ext_get_psk_info(iport, arg);
		break;
	case RINICIO_EG_MACADDR:
		ret = inic_ext_get_mac_address(iport, arg);
		break;
	case RINICIO_EG_ISMP:
		ret = (int)wifi_driver_is_mp();
		break;
	case RINICIO_EG_ASSOCCLIENTLIST:
		ret = inic_ext_get_associated_client_list(iport, arg);
		break;
	case RINICIO_EG_WIFISETTING:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_ext_get_setting(iport, arg);
		break;
	case RINICIO_EG_SWSTATISTIC:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_ext_get_sw_statistic(iport, arg);
		break;
	case RINICIO_EG_PHYTATISTIC:
		ret = inic_ext_get_phy_statistic(iport, arg);
		break;
	case RINICIO_EG_EAPPHASE:
		ret = wifi_get_eap_phase();
		break;
	case RINICIO_EG_ANTANNAINFO:
		ret = inic_ext_get_antenna_info(iport, arg);
		break;
	case RINICIO_EG_BANDTYPE:
		ret = wifi_get_band_type();
		break;
	case RINICIO_EG_AUTOCHANNEL:
		iport = iport->idev->piport[wlan_idx];
		ret = inic_ext_get_auto_channel(iport, arg);
		break;
	case RINICIO_EG_AUTORECONNECT:
		ret = inic_ext_get_autoreconnect(iport, arg);
		break;
	default:
		printk("%s, %s: unknown inic extended cmd 0x%x.\n",
		       INIC_EXT_CMD, __func__, pcmd->cmd);
		break;
	}

func_exit:
	return ret;
}
