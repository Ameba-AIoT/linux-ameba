// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Reset support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _DT_BINDINGS_REALTEK_AMEBA_RESET_H
#define _DT_BINDINGS_REALTEK_AMEBA_RESET_H

/* AON resets */
#define RTK_POR_SYSON			0	/* POR_SYSON */
#define RTK_POR_AON_OTP			1	/* POR_AON_OTP */

/* LSYS resets */
#define RTK_POR_LP_PERI			2	/* POR_LP_PERI */
#define RTK_POR_LP_PLAT			3	/* POR_LP_PLAT */
#define RTK_POR_HP_SYSON_RAM	4	/* POR_HP_SYSON_RAM */
#define RTK_POR_HP_PLAT			5	/* POR_HP_PLAT */
#define RTK_POR_HP_PERI_ON		6	/* POR_HP_PERI_ON */
#define RTK_POR_HP_PERI_OFF		7	/* POR_HP_PERI_OFF */
#define RTK_POR_HP_NP			8	/* POR_HP_NP */
#define RTK_POR_HP_AP			9	/* POR_HP_AP */
#define RTK_POR_HP_CODECON		10	/* POR_HP_CODECON */
#define RTK_POR_LP_BTON			11	/* POR_LP_BTON */
#define RTK_POR_LP_WIFION		12	/* POR_LP_WIFION */
#define RTK_POR_HP_DDRC			13	/* POR_HP_DDRC */
#define RTK_POR_HP_DDRPHY		14	/* POR_HP_DDRPHY */

#define RTK_RST_MAX_NUM			15

#endif /* _DT_BINDINGS_REALTEK_AMEBA_RESET_H */
