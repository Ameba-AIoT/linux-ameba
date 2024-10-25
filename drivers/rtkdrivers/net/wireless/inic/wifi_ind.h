/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */
 /******************************************************************************
  * history *
  * xxxx.xx.xx | name | summary.
 ******************************************************************************/

#ifndef __WIFI_IND_H__
#define __WIFI_IND_H__

/* -------------------------------- Includes -------------------------------- */
/* external head files */

/* internal head files */
#include "inic_linux_base_type.h"
#include "inic_ipc_api.h"

/* -------------------------------- Defines --------------------------------- */

/* -------------------------------- Macros ---------------------------------- */

/* ------------------------------- Data Types ------------------------------- */
typedef void (*rtw_event_handler_t)(char *buf, int buf_len, int flags, void *handler_user_data);

typedef struct {
//	rtw_event_indicate_t	event_cmd;
	rtw_event_handler_t	handler;
	void	*handler_user_data;
} event_list_elem_t;

/* ---------------------------- Global Variables ---------------------------- */

/* -------------------------- Function declaration -------------------------- */
void wifi_indication(struct host_api_priv *api, rtw_event_indicate_t event,\
		     char *buf, int buf_len, int flags);
void wifi_reg_event_handler(unsigned int event_cmds, int row_id);
void wifi_unreg_event_handler(unsigned int event_cmds, int row_id);

#endif /* __WIFI_IND_H__ */