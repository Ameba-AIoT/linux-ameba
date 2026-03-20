// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek MMC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#define SDIOH_SHIFT_LX_BURST_SIZE									6
#define SDIOH_LX_BURST_SIZE_64B										(0 << SDIOH_SHIFT_LX_BURST_SIZE)
#define SDIOH_SHIFT_MAP_SEL											5
#define SDIOH_MAP_SEL_DEC											(1 << SDIOH_SHIFT_MAP_SEL)
#define SDIOH_SHIFT_DIRECT_ACCESS_SEL										4
#define SDIOH_DIRECT_ACCESS_SEL										(1 << SDIOH_SHIFT_DIRECT_ACCESS_SEL)

#define SDIOH_MASK_DRAM_SA											0x0FFFFFFF
#define SDIOH_MASK_DMA_LEN											0x0000FFFF


#define SDIOH_DAT64_SEL												BIT(5)
#define SDIOH_RSP17_SEL												BIT(4)
#define SDIOH_SHIFT_DDR_WR											1
#define SDIOH_DMA_XFER												BIT(0)

#define SDIOH_DMA_TRANSFER_DONE										BIT(4)
#define SDIOH_CARD_ERROR											BIT(2)
#define SDIOH_CARD_END												BIT(1)
#define SDIOH_SD_ISR_ALL											(0x16)

#define SDIOH_DMA_CTL_INT_EN										BIT(4)
#define SDIOH_CARD_ERR_INT_EN										BIT(2)
#define SDIOH_CARD_END_INT_EN										BIT(1)
#define SDIOH_WRITE_DATA											BIT(0)

#define SDIOH_SHIFT_SD30_SAMP_CLK_SRC								12
#define SDIOH_SHIFT_SD30_PUSH_CLK_SRC								8
#define SDIOH_SHIFT_CRC_CLK_SRC										4
#define SDIOH_SD30_SAMP_CLK_VP1										(2 << SDIOH_SHIFT_SD30_SAMP_CLK_SRC)
#define SDIOH_SD30_PUSH_CLK_VP0										(1 << SDIOH_SHIFT_SD30_PUSH_CLK_SRC)
#define SDIOH_CRC_CLK_SSC											(0 << SDIOH_SHIFT_CRC_CLK_SRC)
#define SDIOH_MASK_CLKDIV											(0x7)
#define SDIOH_CLK_DIV1												0
#define SDIOH_CLK_DIV2												1
#define SDIOH_CLK_DIV4												2
#define SDIOH_CLK_DIV8												3

#define SDIOH_TARGET_MODULE_SD										BIT(2)
#define SDIOH_CARD_SEL_SD_MODULE									0x2

#define SDIOH_SD_WP													BIT(5)
#define SDIOH_SD_EXIST												BIT(2)

#define SDIOH_SDMMC_INT_EN											BIT(2)

#define SDIOH_SDMMC_INT_PEND										BIT(2)

#define SDIOH_SD_CARD_MOUDLE_EN										BIT(2)

#define SDIOH_INITIAL_MODE											BIT(7)
#define SDIOH_CLK_DIV_BY_128										0
#define SDIOH_CLK_DIV_BY_256										BIT(6)
#define SDIOH_SD30_ASYNC_FIFO_RST_N									BIT(4)
#define SDIOH_SHIFT_MODE_SEL										2
#define SDIOH_MASK_MODE_SEL											0xc
#define SDIOH_SHIFT_BUS_WIDTH										0
#define SDIOH_MASK_BUS_WIDTH										0x3

#define SDIOH_CRC7_CAL_EN											0
#define SDIOH_CRC7_CAL_DIS											BIT(7)
#define SDIOH_CRC16_CHK_EN											0
#define SDIOH_CRC16_CHK_DIS											BIT(6)
#define SDIOH_WAIT_WR_CRCSTA_TO_EN									0
#define SDIOH_WAIT_WR_CRCSTA_TO_DIS									BIT(5)
#define SDIOH_IGNORE_WR_CRC_ERR_EN									0
#define SDIOH_IGNORE_WR_CRC_ERR_DIS									BIT(4)
#define SDIOH_WAIT_BUSY_END_DIS										0
#define SDIOH_WAIT_BUSY_END_EN										BIT(3)
#define SDIOH_CRC7_CHK_EN											0
#define SDIOH_CRC7_CHK_DIS											BIT(2)

#define SDIOH_STOP_STA_WAIT_BUSY_EN									0
#define SDIOH_STOP_STA_WAIT_BUSY_DIS								BIT(7)
#define SDIOH_CMD_STA_WAIT_BUSY_EN									0
#define SDIOH_CMD_STA_WAIT_BUSY_DIS									BIT(6)
#define SDIOH_DATA_PHA_WAIT_BUSY_EN									BIT(5)
#define SDIOH_DATA_PHA_WAIT_BUSY_DIS								0
#define SDIOH_SD30_CLK_STOP_EN										BIT(4)
#define SDIOH_SD30_CLK_STOP_DIS										0
#define SDIOH_SD20_CLK_STOP_EN										BIT(3)
#define SDIOH_SD20_CLK_STOP_DIS										0
#define SDIOH_SD_CMD_RESP_CHK_EN									BIT(2)
#define SDIOH_SD_CMD_RESP_CHK_DIS									0
#define SDIOH_ADDR_MODE_SECTOR										0
#define SDIOH_ADDR_MODE_BYTE										BIT(1)
#define SDIOH_CMD_RESP_TO_EN										BIT(0)
#define SDIOH_CMD_RESP_TO_DIS										0

#define SDIOH_SD_TUNNING_PAT_COMP_ERR								BIT(0)
#define SDIOH_GET_WRCRC_STA_TO_ERR									BIT(1)
#define SDIOH_MASK_WR_CRC_STA										0x1C
#define SDIOH_WR_CRC_ERR											BIT(5)
#define SDIOH_CRC16_ERR												BIT(6)
#define SDIOH_CRC7_ERR												BIT(7)
#define SDIOH_SD_CMD_RSP_TO_ERR										BIT(8)
#define SDIOH_SD_CMD_RSP_INVALID									BIT(9)

#define SDIOH_MASK_BLOCL_CNT_L										0xFF
#define SDIOH_MASK_BLOCL_CNT_H										0x7F
#define SDIOH_START_TRANSFER										BIT(7)
#define SDIOH_TRANSFER_END											BIT(6)
#define SDIOH_SD_MODULE_FSM_IDLE									BIT(5)
#define SDIOH_ERR_OCCUR												BIT(4)
#define SDIOH_MASK_COM_CODE											0xF

#define SDIOH_CMD_FSM_IDLE											BIT(7)

#define SDIOH_DATA_FSM_IDLE											BIT(7)


#define   SRAM_CRL			0x400	/*!< SRAM Control,	 offset: 0x00000400,  Type: u32*/
#define   DMA_CRL1			0x404	/*!< DMA Control 1,  offset: 0x00000404,  Type: u32*/
#define   DMA_CRL2			0x408	/*!< DMA Control 2,  offset: 0x00000408,  Type: u32*/
#define   DMA_CRL3			0x40C	/*!< DMA Control 3,  offset: 0x0000040C,  Type: u32*/
#define   SYS_LOW_PWR		0X410	/*!< Low Power Control,  offset: 0x00000410,  Type: u32*/
#define   SD_ISR 			0x424	/*!< SD Interrupt Status,  offset: 0x00000424,  Type: u32*/
#define   SD_ISREN			0x428	/*!< SD Interrupt Enable,  offset: 0x00000428,  Type: u32*/
#define   PAD_CTL			0x474	/*!< Pad Control,  offset: 0x00000474,  Type: u32*/
#define   CKGEN_CTL			0x478	/*!< Clock Generation Control,	 offset: 0x00000478,  Type: u32*/
#define   CARD_DRIVER_SEL	0x502	/*!< Card Driving Selection,  offset: 0x00000502,  Type: u8*/
#define   CARD_STOP			0x503	/*!< Stop Transfer,  offset: 0x00000503,  Type: u8*/
#define   CARD_SELECT 		0x50E	/*!< Card Type Select,	 offset: 0x0000050E,  Type: u8*/
#define   DUMMY1			0x50F	/*!< Dummy 1,  offset: 0x0000050F,  Type: u8*/
#define   CARD_EXIST		0x51F	/*!< Card Detection,  offset: 0x0000051F,  Type: u8*/
#define   CARD_INT_EN		0x520	/*!< Card Interrupt Enable,  offset: 0x00000520,  Type: u8*/
#define   CARD_INT_PEND		0x521	/*!< Card Interrupt Status,  offset: 0x00000521,  Type: u8*/
#define   CARD_CLK_EN_CTL	0x529	/*!< Card Clock Enable Control,  offset: 0x00000529,  Type: u8*/
#define   CLK_PAD_DRIVE		0x530	/*!< Clock Pad Driving,  offset: 0x00000530,  Type: u8*/
#define   CMD_PAD_DRIVE		0x531	/*!< Command Pad Driving,  offset: 0x00000531,  Type: u8*/
#define   DAT_INT_PEND		0x532	/*!< Data Pad Driving,	 offset: 0x00000532,  Type: u8*/
#define   SD_CONFIG1		0x580	/*!< SD Configuration 1,  offset: 0x00000580,  Type: u8*/
#define   SD_CONFIG2		0x581	/*!< SD Configuration 2,  offset: 0x00000581,  Type: u8*/
#define   SD_CONFIG3		0x582	/*!< SD Configuration 3,  offset: 0x00000582,  Type: u8*/
#define   SD_STATUS1		0x583	/*!< SD Status 1,  offset: 0x00000583,  Type: u8*/
#define   SD_STATUS2		0x584	/*!< SD Status 2,  offset: 0x00000584,  Type: u8*/
#define   SD_BUS_STATUS		0x585	/*!< SD Bus Status,  offset: 0x00000585,  Type: u8*/
#define   SD_CMD0			0x589	/*!< SD Command 0-5,  offset: 0x00000589-0x0000058E,  Type: u8*/
#define   SD_CMD1			0x58A	/*!< SD Command 0-5,  offset: 0x0000058A,  Type: u8*/
#define   SD_CMD2			0x58B	/*!< SD Command 0-5,  offset: 0x0000058B,  Type: u8*/
#define   SD_CMD3			0x58C	/*!< SD Command 0-5,  offset: 0x0000058C,  Type: u8*/
#define   SD_CMD4			0x58D	/*!< SD Command 0-5,  offset: 0x0000058D,  Type: u8*/
#define   SD_CMD5			0x58E	/*!< SD Command 0-5,  offset: 0x0000058E,  Type: u8*/
#define   SD_BYTE_CNT_L		0x58F	/*!< Byte Count (Low Byte),  offset: 0x0000058F,  Type: u8*/
#define   SD_BYTE_CNT_H		0x590	/*!< Byte Count (High Byte),  offset: 0x00000590,  Type: u8*/
#define   SD_BLOCK_CNT_L	0x591	/*!< Block Count (Low Byte),  offset: 0x00000591,  Type: u8*/
#define   SD_BLOCK_CNT_H	0x592	/*!< Block Count (High Byte),   offset: 0x00000592,  Type: u8*/
#define   SD_TRANSFER		0x593	/*!< SD Transfer Control,  offset: 0x00000593,  Type: u8*/
#define   SD_CMD_STATE		0x595	/*!< SD Command State,  offset: 0x00000595,  Type: u8*/
#define   SD_DATA_STATE		0x596	/*!< SD Data State,  offset: 0x00000596,  Type: u8*/

typedef struct {
	u32 start_addr;		/*!< Specify the DMA start address. Unit: 8 Bytes. */
	u16 blk_cnt;		/*!< Specify the DMA transfer length.  Unit: 512 Bytes). */
	u8 op;				/*!< Specify the data move direction. Should be a value of @ref SDIOH_DMA_Operation. */
	u8 type;			/*!< Specify the transfer type. Shold be a value of @ref SDIOH_DMA_Transfer_Type. */
} SDIOH_DmaCtl;


typedef struct {
	u32 arg;			/*!< Specify the argument to be transfered with command. */
	u8 idx;				/*!< Specify the command to be transfered. */
	u8 rsp_type;		/*!< Specify the response type. Should be a value of @ref SDIOH_Card_Response_Classfication. */
	u8 rsp_crc_chk;		/*!< Specify CRC7 check enable or not. Should be ENABLE or DISABLE. */
	u8 data_present;	/*!< Specify which thers is data need to read after get response from card. Should be a value of
    						@ref SDIOH_Data_Present */
} SDIOH_CmdTypeDef;


#define SDIOH_NORMAL_WRITE				0
#define SDIOH_AUTO_WRITE3				1
#define SDIOH_AUTO_WRITE4				2
#define SDIOH_AUTO_READ3				5
#define SDIOH_AUTO_READ4				6
#define SDIOH_SEND_CMD_GET_RSP			8
#define SDIOH_AUTO_WRITE1				9
#define SDIOH_AUTO_WRITE2				10
#define SDIOH_NORMAL_READ				12
#define SDIOH_AUTO_READ1				13
#define SDIOH_AUTO_READ2				14
#define SDIOH_TUNING					15

#define SDIOH_NO_RESP					0
#define SDIOH_RESP_R1					1
#define SDIOH_RESP_R2					2
#define SDIOH_RESP_R3					3
#define SDIOH_RESP_R6					4
#define SDIOH_RESP_R7					5

#define SDIOH_NO_RESP					0
#define SDIOH_RSP_6B					1
#define SDIOH_RSP_17B					2

#define SDIOH_NO_DATA					0
#define SDIOH_DATA_EXIST				1

#define SDIOH_DMA_NORMAL				0
#define SDIOH_DMA_64B					1
#define SDIOH_DMA_R2					2

#define SDIOH_SD20_MODE					0
#define SDIOH_DDR_MODE					1
#define SDIOH_SD30_MODE					2

#define SDIOH_BUS_WIDTH_1BIT			0
#define SDIOH_BUS_WIDTH_4BIT			1


#define SDIOH_DMA_WRITE					0
#define SDIOH_DMA_READ					1


#undef SDIOH_SUPPORT_SD30

#define SDIOH_CMD_CPLT_TIMEOUT		  	5000		/* Max. timeout value when checking the flag of command complete, unit: us */
#define SDIOH_XFER_CPLT_TIMEOUT			1000000	/* Max. timeout value when checking the flag of transfer complete, unit: us */

#define SDIOH_READ_TIMEOUT				100000
#define SDIOH_WRITE_TIMEOUT				250000
#define SDIOH_ERASE_TIMEOUT				2000000//250000

#define HOST_COMMAND					BIT(6)	/* Transmission bit of register "SD_CMD0", indicating the direction of transmission (host = 1)*/
#define SDIOH_CMD_IDX_MASK				0x3F		/* Command index mask of register "SD_CMD0" */
#define SDIOH_CMD8_VHS					0x1		/* Value of "VHS" field in CMD8, 2.7-3.6V */
#define SDIOH_CMD8_CHK_PATN				0xAA		/* Value of "Check pattern" field in CMD8 */
#define SDIOH_OCR_VDD_WIN				0xFF8000	/* Value of "OCR" field in ACMD41, OCR bit[23:0] */

#define SDIOH_C6R2_BUF_LEN				64		/* Buffer for CMD6, R2, etc.*/
#define SDIOH_CSD_LEN					16

/* SDIOH_Card_Response_Byte_Index */
#define SDIO_RESP0						0
#define SDIO_RESP1						1
#define SDIO_RESP2						2
#define SDIO_RESP3						3
#define SDIO_RESP4						4
#define SDIO_RESP5						5

/* SDIOH_Signal_Level */
#define SDIOH_SIG_VOL_33				0
#define SDIOH_SIG_VOL_18	 			1



#define SD_BLOCK_SIZE					512    //Bytes

/* SDIO_RESP4 */
#define SD_APP_CMD						BIT(5)

/* SDIO_RESP0 */
#define SD_ADDRESS_ERROR				BIT(6)
#define SD_BLOCK_LEN_ERROR				BIT(5)
#define SD_WP_VIOLATION					BIT(2)

/* SDXC_Power_Control SDXC_Power_Control used in ACMD41*/
#define SD_POWER_SAVING					0
#define SD_MAX_PERFORM					1

/* SD_Switch_1.8v_Request used in ACMD41 */
#define SD_USE_CUR_VOL					0
#define SD_SWITCH_18V					1

/* SD_operation_mode used in CMD6 */
#define SD_CMD6_CHECK_MODE				0
#define SD_CMD6_SWITCH_MODE				1

/* SD_Capacity_Support in ACMD41 */
#define SD_SUPPORT_SDSC_ONLY			0
#define SD_SUPPORT_SDHC_SDXC			1


#define SD_CARD_READY					0x00000001
#define SD_CARD_IDENTIFICATION			0x00000002
#define SD_CARD_STANDBY					0x00000003
#define SD_CARD_TRANSFER				0x00000004
#define SD_CARD_SENDING					0x00000005
#define SD_CARD_RECEIVING				0x00000006
#define SD_CARD_PROGRAMMING				0x00000007
#define SD_CARD_DISCONNECTED			0x00000008
#define SD_CARD_ERROR					0x000000FF


#define SD_SPEC_V101					0
#define SD_SPEC_V110					1
#define SD_SPEC_V200 					2
#define SD_SPEC_V300					3


#define SD_SPEED_DS						0 // 3.3V Function 0
#define SD_SPEED_HS						1 // 3.3V Function 1
#define SD_SPEED_SDR12					2 // 1.8V Function 0
#define SD_SPEED_SDR25					3 // 1.8V Function 1
#define SD_SPEED_SDR50					4 // 1.8V Function 2
#define SD_SPEED_SDR104					5 // 1.8V Function 3
#define SD_SPEED_DDR50					6 // 1.8V Function 4
#define SD_KEEP_CUR_SPEED				15

/* SDIOH Testchip Workaround*/
#define SDIOH_REG_MASK_0                     ((u32)0x000000FF << 0)
#define SDIOH_REG_GET_0(x)                     ((u8)((x & 0x000000FF)))
#define SDIOH_REG_0(x)                     ((u32)(((x) & 0x000000FF) << 0))
#define SDIOH_REG_SHIFT_8				8
#define SDIOH_REG_MASK_8                     ((u32)0x000000FF << 8)
#define SDIOH_REG_GET_8(x)                     ((u8)(((x >> 8) & 0x000000FF)))
#define SDIOH_REG_8(x)                     ((u32)(((x) & 0x000000FF) << 8))
#define SDIOH_REG_SHIFT_16				16
#define SDIOH_REG_MASK_16                     ((u32)0x000000FF << 16)
#define SDIOH_REG_GET_16(x)                     ((u8)(((x >> 16) & 0x000000FF)))
#define SDIOH_REG_16(x)                     ((u32)(((x) & 0x000000FF) << 16))
#define SDIOH_REG_SHIFT_24				24
#define SDIOH_REG_MASK_24                     ((u32)0x000000FF << 24)
#define SDIOH_REG_GET_24(x)                     ((u8)(((x >> 24) & 0x000000FF)))
#define SDIOH_REG_24(x)                     ((u32)(((x) & 0x000000FF) << 24))

#define CARD_REMOVE				0
#define CARD_DETECT				1
#define CARD_DETECT_PROTECT		2


