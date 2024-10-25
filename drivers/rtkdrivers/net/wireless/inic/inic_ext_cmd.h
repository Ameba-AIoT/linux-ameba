/******************************************************************************
 *
 * Copyright(c) 2020 - 2021 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
#ifndef __INIC_EXT_CMD_H__
#define __INIC_EXT_CMD_H__
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
int inic_init_ext_cmd(inic_port_t *iport);
int inic_deinit_ext_cmd(inic_port_t *iport);
int inic_ext_ioctl_hdl(inic_port_t *iport, inic_io_cmd_t *pcmd);

#endif /* __INIC_EXT_CMD_H__ */
