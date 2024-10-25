// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek USB PHY support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __PHY_RTK_VIEWPORT_H
#define __PHY_RTK_VIEWPORT_H

#include "core.h"

/* LSYS BG registers */
#define REG_LSYS_AIP_CTRL1                              0UL // 0x025C
#define LSYS_BIT_BG_PWR                                 ((u32)0x00000001 << 8)          /* 1: power on ddrphy bandgap 0: shutdown bg */
#define LSYS_BIT_BG_ON_USB2                             ((u32)0x00000001 << 5)          /* 1: Bandgap USB2 current enable */
#define LSYS_MASK_BG_ALL                                ((u32)0x00000003 << 0)          /* 0x3  Bandgap enable mode */
#define LSYS_BG_ALL(x)                                  ((u32)(((x) & 0x00000003) << 0))
#define LSYS_GET_BG_ALL(x)                              ((u32)(((x >> 0) & 0x00000003)))

/* USB OTG addon registers */
#define USB_OTG_ADDON_REG_CTRL							0x004 // 0x30004UL
#define USB_OTG_ADDON_REG_VND_STS_OUT					0x01C // 0x3001CUL

#define USB_OTG_ADDON_REG_CTRL_BIT_UPLL_CKRDY			BIT(5)  /* 1: USB PHY clock ready */
#define USB_OTG_ADDON_REG_CTRL_BIT_USB_OTG_RST			BIT(8)  /* 1: Enable USB OTG */
#define USB_OTG_ADDON_REG_CTRL_BIT_USB_DPHY_FEN			BIT(9)  /* 1: Enable USB DPHY */
#define USB_OTG_ADDON_REG_CTRL_BIT_USB_APHY_EN			BIT(14) /* 1: Enable USB APHY */
#define USB_OTG_ADDON_REG_CTRL_BIT_LS_HST_UTMI_EN		BIT(22) /* 1: Enable the support of low-speed host mode when using utmi 16bit */
#define USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_POS	20U		/* MAC high-speed host inter-packet delay */
#define USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_MASK	(0x3U << USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_POS)
#define USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT		USB_OTG_ADDON_REG_CTRL_BIT_HS_IP_GAP_OPT_MASK

/* USB control registers */
#define REG_HSYS_USB_CTRL								0UL // 0x0060

#define HSYS_BIT_USB2_DIGOTGPADEN						BIT(28)
#define HSYS_BIT_USB_OTGMODE            				BIT(27)
#define HSYS_BIT_USB2_DIGPADEN           				BIT(26)
#define HSYS_BIT_ISO_USBA_EN             				BIT(25) /* Default 1, 1: enable signal from USBPHY analog */
#define HSYS_BIT_ISO_USBD_EN             				BIT(24)
#define HSYS_BIT_USBA_LDO_EN             				BIT(23)
#define HSYS_BIT_PDN_UPHY_EN             				BIT(20) /* Default 1, 0: enable USBPHY shutdown */
#define HSYS_BIT_PWC_UABG_EN             				BIT(19) /* Default 0, 1: enable USBPHY BG power cut */
#define HSYS_BIT_PWC_UAHV_EN             				BIT(18) /* Default 0, 1: enable USBPHY HV power cut */
#define HSYS_BIT_PWC_UALV_EN             				BIT(17) /* Default 0, 1: enable USBPHY LV power cut */
#define HSYS_BIT_OTG_ANA_EN              				BIT(16) /* Default 0, 1: enable USBPHY lv2hv, hv2lv check for OTG */

/* USB PHY vendor control register */
#define USB_OTG_GPVNDCTL								0x0034

#define USB_OTG_GPVNDCTL_REGDATA_Pos					(0U)
#define USB_OTG_GPVNDCTL_REGDATA_Msk					(0xFFUL << USB_OTG_GPVNDCTL_REGDATA_Pos) /*!< 0x000000FF */
#define USB_OTG_GPVNDCTL_REGDATA						USB_OTG_GPVNDCTL_REGDATA_Msk /*!< Register Data */
#define USB_OTG_GPVNDCTL_VCTRL_Pos						(8U)
#define USB_OTG_GPVNDCTL_VCTRL_Msk						(0xFFUL << USB_OTG_GPVNDCTL_VCTRL_Pos) /*!< 0x0000FF00 */
#define USB_OTG_GPVNDCTL_VCTRL							USB_OTG_GPVNDCTL_VCTRL_Msk /*!< UTMI+ Vendor Control Register Address */
#define USB_OTG_GPVNDCTL_VSTSDONE_Pos					(27U)
#define USB_OTG_GPVNDCTL_VSTSDONE_Msk					(0x1UL << USB_OTG_GPVNDCTL_VSTSDONE_Pos) /*!< 0x02000000 */
#define USB_OTG_GPVNDCTL_VSTSDONE						USB_OTG_GPVNDCTL_VSTSDONE_Msk /*!< VStatus Done */

/* USB PHY registers */
#define USB_OTG_PHY_REG_E0								0xE0U
#define USB_OTG_PHY_REG_E1								0xE1U
#define USB_OTG_PHY_REG_E2								0xE2U
#define USB_OTG_PHY_REG_E3								0xE3U
#define USB_OTG_PHY_REG_E4								0xE4U
#define USB_OTG_PHY_REG_E5								0xE5U
#define USB_OTG_PHY_REG_E6								0xE6U
#define USB_OTG_PHY_REG_E7								0xE7U
#define USB_OTG_PHY_REG_F0								0xF0U
#define USB_OTG_PHY_REG_F1								0xF1U
#define USB_OTG_PHY_REG_F2								0xF2U
#define USB_OTG_PHY_REG_F3								0xF3U
#define USB_OTG_PHY_REG_F4								0xF4U
#define USB_OTG_PHY_REG_F5								0xF5U
#define USB_OTG_PHY_REG_F6								0xF6U
#define USB_OTG_PHY_REG_F7								0xF7U


/* USB_OTG_PHY_REG_E0 PAGE0 bits */
#define USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_AUTO_K_POS		6U
#define USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_AUTO_K_MASK		(0x1U << USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_AUTO_K_POS)
#define USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_AUTO_K			USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_AUTO_K_MASK
#define USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_ADJR_POS		2U
#define USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_ADJR_MASK		(0xFU << USB_OTG_PHY_REG_E0_PAGE0_BIT_Z0_ADJR_POS)

/* USB_OTG_PHY_REG_E1 PAGE1 bits */
#define USB_OTG_PHY_REG_E1_PAGE1_BIT_EN_OTG_POS			7U
#define USB_OTG_PHY_REG_E1_PAGE1_BIT_EN_OTG_MASK		(0x1U << USB_OTG_PHY_REG_E1_PAGE1_BIT_EN_OTG_POS)
#define USB_OTG_PHY_REG_E1_PAGE1_BIT_EN_OTG				USB_OTG_PHY_REG_E1_PAGE1_BIT_EN_OTG_MASK
#define USB_OTG_PHY_REG_E1_PAGE1_BIT_REG_SRC_POS		4U
#define USB_OTG_PHY_REG_E1_PAGE1_BIT_REG_SRC_MASK		(0x7U << USB_OTG_PHY_REG_E1_PAGE1_BIT_REG_SRC_POS)
#define USB_OTG_PHY_REG_E1_PAGE1_BIT_REG_SRC			USB_OTG_PHY_REG_E1_PAGE1_BIT_REG_SRC_MASK

/* USB_OTG_PHY_REG_E2 PAGE1 bits */
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRVH_POS		0U
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRVH_MASK		(0x7U << USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRVH_POS)
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRVH			USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRVH_MASK
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRM_POS		3U
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRM_MASK		(0x7U << USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRM_POS)
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRM			USB_OTG_PHY_REG_E2_PAGE1_BIT_REG_SRM_MASK
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_SREN_POS			6U
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_SREN_MASK			(0x1U << USB_OTG_PHY_REG_E2_PAGE1_BIT_SREN_POS)
#define USB_OTG_PHY_REG_E2_PAGE1_BIT_SREN				USB_OTG_PHY_REG_E2_PAGE1_BIT_SREN_MASK

/* USB_OTG_PHY_REG_E4 PAGE0 bits */
#define USB_OTG_PHY_REG_E4_PAGE0_BIT_REG_SITX_POS		0U
#define USB_OTG_PHY_REG_E4_PAGE0_BIT_REG_SITX_MASK		(0xFU << USB_OTG_PHY_REG_E4_PAGE0_BIT_REG_SITX_POS)

/* USB_OTG_PHY_REG_E6 PAGE0 bits */
#define USB_OTG_PHY_REG_E6_PAGE0_BIT_RX_BOOST_POS		0U
#define USB_OTG_PHY_REG_E6_PAGE0_BIT_RX_BOOST_MASK		(0x3U << USB_OTG_PHY_REG_E6_PAGE0_BIT_RX_BOOST_POS)

/* USB_OTG_PHY_REG_E7 PAGE2 bits */
#define USB_OTG_PHY_REG_E7_PAGE2_BIT_SEND_OBJ_POS		0U
#define USB_OTG_PHY_REG_E7_PAGE2_BIT_SEND_OBJ_MASK		(0xFU << USB_OTG_PHY_REG_E7_PAGE2_BIT_SEND_OBJ_POS)
#define USB_OTG_PHY_REG_E7_PAGE2_BIT_SENH_OBJ_POS		4U
#define USB_OTG_PHY_REG_E7_PAGE2_BIT_SENH_OBJ_MASK		(0xFU << USB_OTG_PHY_REG_E7_PAGE2_BIT_SENH_OBJ_POS)

/* USB_OTG_PHY_REG_F1 PAGE0 bits */
#define USB_OTG_PHY_REG_F1_PAGE0_BIT_UTMI_POS_OUT_POS	7U
#define USB_OTG_PHY_REG_F1_PAGE0_BIT_UTMI_POS_OUT_MASK	(0x1U << USB_OTG_PHY_REG_F1_PAGE0_BIT_UTMI_POS_OUT_POS)
#define USB_OTG_PHY_REG_F1_PAGE0_BIT_UTMI_POS_OUT		USB_OTG_PHY_REG_F1_PAGE0_BIT_UTMI_POS_OUT_MASK

/* USB_OTG_PHY_REG_F4 bits */
#define USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_POS				5U
#define USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_MASK			(0x3U << USB_OTG_PHY_REG_F4_BIT_PAGE_SEL_POS)
#define USB_OTG_PHY_REG_F4_BIT_PAGE0					0U
#define USB_OTG_PHY_REG_F4_BIT_PAGE1					1U
#define USB_OTG_PHY_REG_F4_BIT_PAGE2					2U
#define USB_OTG_PHY_REG_F4_BIT_PAGE3					3U

/* USB_OTG_PHY_REG_F6 PAGE1 bits */
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_LATE_DLLEN_POS				0U
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_LATE_DLLEN_MASK			(0x3U << USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_LATE_DLLEN_POS)
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_REG_NSQ_VLD_SEL_POS		2U
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_REG_NSQ_VLD_SEL_MASK		(0x1U << USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_REG_NSQ_VLD_SEL_POS)
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_NSQ_VLD_SEL				USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_REG_NSQ_VLD_SEL_MASK
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_DISABLE_EB_WAIT4IDLE_POS	3U
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_DISABLE_EB_WAIT4IDLE_MASK	(0x1U << USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_DISABLE_EB_WAIT4IDLE_POS)
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_DISABLE_EB_WAIT4IDLE		USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_DISABLE_EB_WAIT4IDLE_MASK
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_WATER_LEVEL_CLEAN_SEL_POS	4U
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_WATER_LEVEL_CLEAN_SEL_MASK	(0x1U << USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_WATER_LEVEL_CLEAN_SEL_POS)
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_WATER_LEVEL_CLEAN_SEL		USB_OTG_PHY_REG_F6_PAGE1_BIT_REG_WATER_LEVEL_CLEAN_SEL_MASK
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_NSQ_DEL_SEL_POS				5U
#define USB_OTG_PHY_REG_F6_PAGE1_BIT_NSQ_DEL_SEL_MASK				(0x3U << USB_OTG_PHY_REG_F6_PAGE1_BIT_NSQ_DEL_SEL_POS)


/*==========SEC Register Address Definition==========*/
#define SEC_OTP_SYSCFG2                              0x08
#define SEC_OTP_SYSCFG3                              0x0C

/** @defgroup SEC_OTP_SYSCFG2
 * @brief
 * @{
 **/
#define SEC_BIT_USB_PHY_UTMI_POS_OUT                ((u32)0x00000001 << 31)          /*!<R/W 0  All output signals on UTMI interface are triggered at positive edge of UTMI clock */
#define SEC_MASK_USB_PHY_REG_SITX                   ((u32)0x0000000F << 27)          /*!<R/W 0  Control magnitude of HSTX output swing IBX_TX = 3.125×sitx+125 (uA) Default : 3.125×8+125 = 150 (uA) IBXDP/IBXDM = 120×IBX_TX Default : 120×0.15 (mA) = 18 (mA) HSDP/HSDM = 18(mA)×45 (Ω) /2 = 405 mV sitx (4-bit): REG_SITX[3:0] IBX_TX(uA): current for HSTX output swing control IBXDP/IBXDM : HSTX DP (or DM) output current HSDP/HSDM : HSTX DP (or DM) output voltage */
#define SEC_USB_PHY_REG_SITX(x)                     ((u32)(((x) & 0x0000000F) << 27))
#define SEC_GET_USB_PHY_REG_SITX(x)                 ((u32)(((x >> 27) & 0x0000000F)))
#define SEC_MASK_USB_PHY_REG_SRC                    ((u32)0x00000007 << 24)          /*!<R/W 0  USB PHY slew-rate (rise time ,tr /fall time ,tf) control of HSDP,HSDM, tr: 000: 560ps , 001: 530ps 010: 500ps , 011: 470ps 100: 440ps , 101: 420ps 110: 390ps , 111: 370ps */
#define SEC_USB_PHY_REG_SRC(x)                      ((u32)(((x) & 0x00000007) << 24))
#define SEC_GET_USB_PHY_REG_SRC(x)                  ((u32)(((x >> 24) & 0x00000007)))
#define SEC_MASK_USB_PHY_SENH_OBJ                   ((u32)0x0000000F << 20)          /*!<R/W 0  USB PHY host squelch level calibration target */
#define SEC_USB_PHY_SENH_OBJ(x)                     ((u32)(((x) & 0x0000000F) << 20))
#define SEC_GET_USB_PHY_SENH_OBJ(x)                 ((u32)(((x >> 20) & 0x0000000F)))
#define SEC_MASK_USB_PHY_SEND_OBJ                   ((u32)0x0000000F << 16)          /*!<R/W 0  USB PHY device squelch level calibration target */
#define SEC_USB_PHY_SEND_OBJ(x)                     ((u32)(((x) & 0x0000000F) << 16))
#define SEC_GET_USB_PHY_SEND_OBJ(x)                 ((u32)(((x >> 16) & 0x0000000F)))
#define SEC_BIT_USB_SW_RSVD                         ((u32)0x00000001 << 15)          /*!<R/W 0   */
#define SEC_BIT_USB_PHY_SREN                        ((u32)0x00000001 << 14)          /*!<R/W 0  Enable USB PHY new slew rate control for HSTX 0: disable 1: enable */
#define SEC_MASK_USB_PHY_REG_SRM                    ((u32)0x00000007 << 11)          /*!<R/W 0  USB PHY new slew-rate (rise time ,tr /fall time ,tf) control of HSTX */
#define SEC_USB_PHY_REG_SRM(x)                      ((u32)(((x) & 0x00000007) << 11))
#define SEC_GET_USB_PHY_REG_SRM(x)                  ((u32)(((x >> 11) & 0x00000007)))
#define SEC_MASK_USB_PHY_REG_SRVH                   ((u32)0x00000007 << 8)          /*!<R/W 0  USB PHY high voltage level of control signal of new slew rate control for HSTX */
#define SEC_USB_PHY_REG_SRVH(x)                     ((u32)(((x) & 0x00000007) << 8))
#define SEC_GET_USB_PHY_REG_SRVH(x)                 ((u32)(((x >> 8) & 0x00000007)))
#define SEC_BIT_USB_PHY_Z0_AUTO_K                   ((u32)0x00000001 << 7)          /*!<R/W 0  USB PHY Zo auto calibration mode 0: Manual mode, Zo value is from REG_Z0_ADJR 1: Auto mode, Zo value is from Zo_Tune */
#define SEC_MASK_USB_PHY_Z0_ADJR                    ((u32)0x0000000F << 3)          /*!<R/W 0  USB PHY Zo value of manual mode */
#define SEC_USB_PHY_Z0_ADJR(x)                      ((u32)(((x) & 0x0000000F) << 3))
#define SEC_GET_USB_PHY_Z0_ADJR(x)                  ((u32)(((x >> 3) & 0x0000000F)))
#define SEC_MASK_USB_PHY_RX_BOOST                   ((u32)0x00000003 << 1)          /*!<R/W 0  USB PHY AC boost gain of HSRX amp 00: 0dB 01: 3.3dB 10: 6.9dB 11: 9.3dB */
#define SEC_USB_PHY_RX_BOOST(x)                     ((u32)(((x) & 0x00000003) << 1))
#define SEC_GET_USB_PHY_RX_BOOST(x)                 ((u32)(((x >> 1) & 0x00000003)))
#define SEC_BIT_USB_PHY_CAL_EN                      ((u32)0x00000001 << 0)          /*!<R/W 0  USB PHY calibration enable */
/** @} */

/** @defgroup SEC_OTP_SYSCFG3
 * @brief
 * @{
 **/
#define SEC_MASK_USB_RSVD7                          ((u32)0x000000FF << 24)          /*!<R/W 0  HW para for USB usage */
#define SEC_USB_RSVD7(x)                            ((u32)(((x) & 0x000000FF) << 24))
#define SEC_GET_USB_RSVD7(x)                        ((u32)(((x >> 24) & 0x000000FF)))
#define SEC_MASK_USB_RSVD6                          ((u32)0x000000FF << 16)          /*!<R/W 0  HW para for USB usage */
#define SEC_USB_RSVD6(x)                            ((u32)(((x) & 0x000000FF) << 16))
#define SEC_GET_USB_RSVD6(x)                        ((u32)(((x >> 16) & 0x000000FF)))
#define SEC_MASK_USB_RSVD5                          ((u32)0x000000FF << 8)          /*!<R/W 0  HW para for USB usage */
#define SEC_USB_RSVD5(x)                            ((u32)(((x) & 0x000000FF) << 8))
#define SEC_GET_USB_RSVD5(x)                        ((u32)(((x >> 8) & 0x000000FF)))
#define SEC_BIT_USB_RSVD4                           ((u32)0x00000001 << 7)          /*!<R/W 0  HW para for USB usage */
#define SEC_MASK_USB_PHY_NSQ_DEL_SEL                ((u32)0x00000003 << 5)          /*!<R/W 0  Modify to RX packet min packet delay 32 bit times. Need delay more than 8 stages in 32 bit times condition. 00：delay 6 stages 01：delay 9 stages 10：delay 11 stages 11：delay 13 stages */
#define SEC_USB_PHY_NSQ_DEL_SEL(x)                  ((u32)(((x) & 0x00000003) << 5))
#define SEC_GET_USB_PHY_NSQ_DEL_SEL(x)              ((u32)(((x >> 5) & 0x00000003)))
#define SEC_BIT_USB_PHY_REG_WATER_LEVEL_CLEAN_SEL   ((u32)0x00000001 << 4)          /*!<R/W 0  Modify to RX packet min packet delay 32 bit times. Need set 0 in 32 bit times condition. water_level_clean condition： 1’b1： [(start_receive)| (|curr_case)] (old_mode) 1’b0： [(|curr_case)] (new_mode) */
#define SEC_BIT_USB_PHY_REG_DISABLE_EB_WAIT4IDLE    ((u32)0x00000001 << 3)          /*!<R/W 0  Modify to RX packet min packet delay 32 bit times. If first RX packet is too long and water level is too big, need disable EB wait4idle. 1’b0：enable EB wait4idle 1’b1：disable EB wait4idle */
#define SEC_BIT_USB_PHY_REG_NSQ_VLD_SEL             ((u32)0x00000001 << 2)          /*!<R/W 0  Modify to RX packet min packet delay 32 bit times. 1’b1：(NSQ_d[9:0])|NSQ_s (old mode) 1’b0：(NSQ_d[5:0])|NSQ_s (new mode) */
#define SEC_MASK_USB_PHY_REG_LATE_DLLEN             ((u32)0x00000003 << 0)          /*!<R/W 0  Modify to TX2RX packet min packet delay 8 bit times. Need less than 7T in 8 bit times condition. HSDLLEN will be asserted to start RX 100ns after TX is done if LATE_DLLEN is set. Else if LATE_DLLEN is cleared, only 66ns duration will be set. This is used to avoid the reception of ECHO following the TX issued by PHY itself. 2’B00：3T 2’B01：5T 2’B10：7T 2’B11：11T */
#define SEC_USB_PHY_REG_LATE_DLLEN(x)               ((u32)(((x) & 0x00000003) << 0))
#define SEC_GET_USB_PHY_REG_LATE_DLLEN(x)           ((u32)(((x >> 0) & 0x00000003)))
/** @} */

int rtk_phy_calibrate(struct dwc2_hsotg *hsotg);

#endif /* __PHY_RTK_VIEWPORT_H */
