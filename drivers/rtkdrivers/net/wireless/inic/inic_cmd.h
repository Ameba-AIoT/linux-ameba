/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
#ifndef __INIC_CMD_H__
#define __INIC_CMD_H__
/* -------------------------------- Includes -------------------------------- */
/* external head files */
#include <uapi/inic/inic_wireless.h>

/* internal head files */
#include "inic_ipc.h"
#include "inic_linux_base_type.h"
#include "inic_ipc_msg_queue.h"
#include "inic_net_device.h"
#include "wifi_conf.h"

/* -------------------------------- Defines --------------------------------- */

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */
void inic_io_wifi_ind_cb(rtw_event_indicate_t event_cmd,\
			 char *buf, int buf_len, int flags);
int inic_init_cmd(inic_port_t *iport);
int inic_deinit_cmd(inic_port_t *iport);
int inic_ioctl_hdl(inic_port_t *iport, inic_io_cmd_t *pcmd);
int inic_io_event_notify(inic_port_t *iport, inic_io_event_t evt, void *data, int len);

#endif /* __INIC_CMD_H__ */
