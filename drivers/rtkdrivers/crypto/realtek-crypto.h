// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Crypto support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __REALTEK_CRYPTO_H
#define __REALTEK_CRYPTO_H

/**************************************************************************//**
 * @defgroup SDSR
 * @brief Source Descriptor Status Register
 * @{
 *****************************************************************************/
#define IPSEC_BIT_SRC_RST                     ((u32)0x00000001 << 31)          /*!<R/W 0  Source descriptor reset. (only for pk_up = 1'b1) */
#define IPSEC_BIT_PK_UP                       ((u32)0x00000001 << 30)          /*!<R/W 1  Packet base update wptr to engine. If pk_up =1, the total number of souce descriptor in one packet can't be over 16, and the total number of destination descriptor in one packet can't be over 8. */
#define IPSEC_MASK_SRC_FAIL_STATUS            ((u32)0x00000003 << 25)          /*!<R 0  Source Descriptor failed status. 2'd1: Users write the first word twice consecutively 2'd2: Users write the second word directly without writing first word in the beginning 2'd3: Overflow (Detect users try to write source descriptor, but there isn't available FIFO node could use). */
#define IPSEC_SRC_FAIL_STATUS(x)              ((u32)(((x) & 0x00000003) << 25))
#define IPSEC_GET_SRC_FAIL_STATUS(x)          ((u32)(((x >> 25) & 0x00000003)))
#define IPSEC_BIT_SRC_FAIL                    ((u32)0x00000001 << 24)          /*!<R/W 0  Source Descriptor failed interrupt. Write 1 to clear this bit. */
#define IPSEC_MASK_SRPTR                      ((u32)0x000000FF << 16)          /*!<R 0  Source Descriptor FIFO read pointer. When engine read a descriptor and finished it, SRPTR = SRPTR+2 */
#define IPSEC_SRPTR(x)                        ((u32)(((x) & 0x000000FF) << 16))
#define IPSEC_GET_SRPTR(x)                    ((u32)(((x >> 16) & 0x000000FF)))
#define IPSEC_MASK_SWPTR                      ((u32)0x000000FF << 8)          /*!<R 0  Source Descriptor FIFO write pointer. When write descriptor to SDSW successfully, SWPTR = SWPTR+2 */
#define IPSEC_SWPTR(x)                        ((u32)(((x) & 0x000000FF) << 8))
#define IPSEC_GET_SWPTR(x)                    ((u32)(((x >> 8) & 0x000000FF)))
#define IPSEC_MASK_FIFO_EMPTY_CNT             ((u32)0x000000FF << 0)          /*!<R 0  Source Descriptor FIFO empty counter. Use this field to check how many available FIFO nodes in Source descriptor FIFO could use. */
#define IPSEC_FIFO_EMPTY_CNT(x)               ((u32)(((x) & 0x000000FF) << 0))
#define IPSEC_GET_FIFO_EMPTY_CNT(x)           ((u32)(((x >> 0) & 0x000000FF)))
/** @} */

/**************************************************************************//**
 * @defgroup SDFWR
 * @brief Source Descriptor First Word Register
 * @{
 *****************************************************************************/
#define IPSEC_MASK_SDFW                       ((u32)0xFFFFFFFF << 0)          /*!<R/W 0  Source descriptor first word */
#define IPSEC_SDFW(x)                         ((u32)(((x) & 0xFFFFFFFF) << 0))
#define IPSEC_GET_SDFW(x)                     ((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SDSWR
 * @brief Source Descriptor Second Word Register
 * @{
 *****************************************************************************/
#define IPSEC_MASK_SDFW                       ((u32)0xFFFFFFFF << 0)          /*!<R/W 0  Source descriptor second word */
#define IPSEC_SDFW(x)                         ((u32)(((x) & 0xFFFFFFFF) << 0))
#define IPSEC_GET_SDFW(x)                     ((u32)(((x >> 0) & 0xFFFFFFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup RSTEACONFISR
 * @brief Reset Engine & Configure ISR Register
 * @{
 *****************************************************************************/
#define IPSEC_BIT_RST                         ((u32)0x00000001 << 31)          /*!<R/W 0  IPsec engine reset. Write "1" to reset the crypto engine and DMA engine. (Use to clear fatal error) */
#define IPSEC_BIT_AHB_ERR_INT_MASK            ((u32)0x00000001 << 26)          /*!<R/W 1   */
#define IPSEC_BIT_CLR_AHB_ERR_INT             ((u32)0x00000001 << 25)          /*!<R/W 0   */
#define IPSEC_BIT_AHB_ERR_INT                 ((u32)0x00000001 << 24)          /*!<R 0   */
#define IPSEC_MASK_CLEAR_OK_INT_NUM           ((u32)0x000000FF << 16)          /*!<R/W 1  Clear ok interrupt number. In interrupt counter mode, if the programmer wants to clear ok_intr_cnt , need to write ok_intr_cnt to this field first, then write "1" to cmd_ok. */
#define IPSEC_CLEAR_OK_INT_NUM(x)             ((u32)(((x) & 0x000000FF) << 16))
#define IPSEC_GET_CLEAR_OK_INT_NUM(x)         ((u32)(((x >> 16) & 0x000000FF)))
#define IPSEC_MASK_OK_INTR_CNT                ((u32)0x000000FF << 8)          /*!<R 0  Ok interrupt counter. Read this field can know how many interrupts are coming to notify crypto engine has calculated cryptographic feature results. Use this field in interrupt counter mode. */
#define IPSEC_OK_INTR_CNT(x)                  ((u32)(((x) & 0x000000FF) << 8))
#define IPSEC_GET_OK_INTR_CNT(x)              ((u32)(((x >> 8) & 0x000000FF)))
#define IPSEC_BIT_INTR_MODE                   ((u32)0x00000001 << 7)          /*!<R/W 0  Select ok interrupt mode: 1'd0: Interrupt mode is general mode. 1'd1: Interrupt mode is counter mode. General mode means no matter how many interrupts are coming to notify crypto engine has calculated cryptographic feature results, there always is one interrupt signal shows in cmd_ok field, and nothing in ok_intr_cnt. So if the programmer want to clear this signal, just write "1" to cmd_ok. Counter mode means the programmer can know how many interrupts are coming to notify crypto engine has calculated cryptographic feature results from ok_intr_cnt. The programmer also can know there are at least one interrupt from reading cmd_ok. So if the programmer want to clear ok_intr_cnt signals, need to write ok_intr_cnt into clear_ok_int_num, then write "1" to cmd_ok. */
#define IPSEC_BIT_CMD_OK                      ((u32)0x00000001 << 4)          /*!<R/W 0  Command ok Interrupt. Read this field to detect whether crypto engine has calculated a cryptographic feature result. Even if interrupt mode is counter mode, still can use this field to detect whether an interrupt is coming. Write "1" to clear this interrupt signal. */
#define IPSEC_BIT_DMA_BUSY                    ((u32)0x00000001 << 3)          /*!<R 0  Detect whether IPsec DMA is busy: 1'd0: DMA is not busy 1'd1: DMA is busy Using for debuging. To avoid data coherence issue[when user get crypto finish calculated interrupt, user can make sure DMA engine has already move data to result buffer.] */
#define IPSEC_BIT_SOFT_RST                    ((u32)0x00000001 << 0)          /*!<W 0  Software Reset. Write "1" to reset the crypto engine */
/** @} */

/**************************************************************************//**
 * @defgroup DBGR
 * @brief Debug Register
 * @{
 *****************************************************************************/
#define IPSEC_BIT_DEBUG_WB                    ((u32)0x00000001 << 31)          /*!<R/W 0  Debug: write back mode. 1'd0: Disable DMA write back mode. 1'd1: Enable DMA write back mode. Enable this field, Crypto DMA will move data from source address to destination address. During this period, the data won't be processed in any calculation(Encryption, Decryption, Hash). Then the programmer can check whether the source data is the same as destination data. */
#define IPSEC_MASK_MST_BAD_SEL                ((u32)0x00000003 << 28)          /*!<R/W 0  <for debug port> select Lexra master data bus (option) 2'd0: mst_ipsec_bad[07:00] 2'd1: mst_ipsec_bad[15:08] 2'd2: mst_ipsec_bad[23:16] 2'd3: mst_ipsec_bad[31:24] */
#define IPSEC_MST_BAD_SEL(x)                  ((u32)(((x) & 0x00000003) << 28))
#define IPSEC_GET_MST_BAD_SEL(x)              ((u32)(((x >> 28) & 0x00000003)))
#define IPSEC_BIT_ENGINE_CLK_EN               ((u32)0x00000001 << 24)          /*!<R/W 0  IPsec Engine clock enable 1'd0: Disable IPsec engine clock 1'd1: Enable IPsec engine clock */
#define IPSEC_MASK_DEBUG_PORT_SEL             ((u32)0x0000000F << 20)          /*!<R/W 0  Debug port selection 4'd0: engine_debug 4'd1: dma_lexra_debug 4'd2: dma_rx_debug 4'd3: dma_tx_debug */
#define IPSEC_DEBUG_PORT_SEL(x)               ((u32)(((x) & 0x0000000F) << 20))
#define IPSEC_GET_DEBUG_PORT_SEL(x)           ((u32)(((x >> 20) & 0x0000000F)))
#define IPSEC_BIT_ARBITER_MODE                ((u32)0x00000001 << 16)          /*!<R/W 1  DMA arbiter mode 1'd0: round-robin 1'd1: detect fifo wasted level There are 2 fifo in DMA, that are read-fifo and write-fifo. Detect fifo wasted level means that DMA will process which fifo first depend on which wasted level is high. Because the wasted level is high means there are many fifo nodes waiting to be processed in this fifo, so DMA must process them in hurry. */
#define IPSEC_MASK_DMA_WAIT_CYCLE             ((u32)0x0000FFFF << 0)          /*!<R/W 0  Set DMA wait cycles to assert next DMA request. */
#define IPSEC_DMA_WAIT_CYCLE(x)               ((u32)(((x) & 0x0000FFFF) << 0))
#define IPSEC_GET_DMA_WAIT_CYCLE(x)           ((u32)(((x >> 0) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup SWAPABURSTR
 * @brief Swap and Burst Register
 * @{
 *****************************************************************************/
#define IPSEC_BIT_CHA_INPUT_DATA_BYTE_SWAP    ((u32)0x00000001 << 27)          /*!<R/W 0  byte swap for cha input data 1: Enable 0: Disable */
#define IPSEC_BIT_CHA_OUTPUT_DATA_BYTE_SWAP   ((u32)0x00000001 << 26)          /*!<R/W 0  byte swap for cha output data 1: Enable 0: Disable */
#define IPSEC_BIT_MD5_INPUT_DATA_BYTE_SWAP    ((u32)0x00000001 << 25)          /*!<R/W 0  byte swap for md5 input data 1: Enable 0: Disable */
#define IPSEC_BIT_MD5_OUTPUT_DATA_BYTE_SWAP   ((u32)0x00000001 << 24)          /*!<R/W 0  byte swap for md5 output data 1: Enable 0: Disable */
#define IPSEC_MASK_DMA_BURST_LENGTH           ((u32)0x0000003F << 16)          /*!<R/W 0x10  Set DMA burst length: The maximum setting length is 16 bursts The minimum setting length is 1 burst Note: If all bits are zero(6'd0), it means maximum burst size or undefined burst size. */
#define IPSEC_DMA_BURST_LENGTH(x)             ((u32)(((x) & 0x0000003F) << 16))
#define IPSEC_GET_DMA_BURST_LENGTH(x)         ((u32)(((x >> 16) & 0x0000003F)))
#define IPSEC_BIT_TX_WD_SWAP                  ((u32)0x00000001 << 12)          /*!<R/W 0  Word swap for dma_tx engine input data: 1'd0: Disable 1'd1: Enable */
#define IPSEC_BIT_RX_WD_SWAP                  ((u32)0x00000001 << 11)          /*!<R/W 0  Word swap for dma_rx engine output data: 1'd0: Disable 1'd1: Enable */
#define IPSEC_BIT_MAC_OUT_LITTLE_ENDIAN       ((u32)0x00000001 << 10)          /*!<R/W 0  Ouput mac is little endian: 1'd0: Big endian 1'd1: Little endian */
#define IPSEC_BIT_DATA_OUT_LITTLE_ENDIAN      ((u32)0x00000001 << 9)          /*!<R/W 0  Ouput data is little endian: 1'd0: Big endian 1'd1: Little endian */
#define IPSEC_BIT_TX_BYTE_SWAP                ((u32)0x00000001 << 8)          /*!<R/W 0  Byte swap for dma_tx engine input data: 1'd0: Disable 1'd1: Enable */
#define IPSEC_BIT_AUTO_PADDING_SWAP           ((u32)0x00000001 << 5)          /*!<R/W 0  byte swap for padding_len input data 1: Enable 0: Disable */
#define IPSEC_BIT_DATA_IN_LITTLE_ENDIAN       ((u32)0x00000001 << 4)          /*!<R/W 0  Input data is little endian: 1'd0: Big endian 1'd1: Little endian */
#define IPSEC_BIT_HASH_INITIAL_VALUE_SWAP     ((u32)0x00000001 << 3)          /*!<R/W 0  Byte swap for sequential hash initial value: 1'd0: Disable 1'd1: Enable */
#define IPSEC_BIT_KEY_PAD_SWAP                ((u32)0x00000001 << 2)          /*!<R/W   Byte swap for HMAC key pad: 1'd0: Disable 1'd1: Enable */
#define IPSEC_BIT_KEY_IV_SWAP                 ((u32)0x00000001 << 1)          /*!<R/W 0  Byte swap for Cipher key and Initial Vector: 1'd0: Disable 1'd1: Enable */
#define IPSEC_BIT_SET_SWAP                    ((u32)0x00000001 << 0)          /*!<R/W 0  Byte swap for command setting data: 1'd0: Disable 1'd1: Enable */
/** @} */

/* Registers for crypto */
#define RTK_SDSR                                         0x0000 /*!< SOURCE DESCRIPTOR STATUS REGISTER */
#define RTK_SDFWR                                        0x0004 /*!< SOURCE DESCRIPTOR FIRST WORD REGISTER */
#define RTK_SDSWR                                        0x0008 /*!< SOURCE DESCRIPTOR SECOND WORD REGISTER */
#define RTK_RSTEACONFISR                                 0x0010 /*!< RESET ENGINE & CONFIGURE ISR REGISTER */
#define RTK_INTM                                         0x0014 /*!< INTERRUPT MASK REGISTER */
#define RTK_DBGR                                         0x0018 /*!< DEBUG REGISTER */
#define RTK_ERRSR                                        0x001C /*!< ERROR STATUS REGISTER */
#define RTK_SAADLR                                       0x0020 /*!< SUM OF ADDITIONAL AUTHENTICATION DATA LENGTH REGISTER */
#define RTK_SENLR                                        0x0024 /*!< SUM OF ENCRYPTION DATA LENGTH REGISTER */
#define RTK_SAPLR                                        0x0028 /*!< SUM OF AUTHENTICATION PADDING DATA LENGTH REGISTER */
#define RTK_SEPLR                                        0x002C /*!< SUM OF ENCRYPTION PADDING DATA LENGTH REGISTER */
#define RTK_SWAPABURSTR                                  0x0030 /*!< SWAP AND BURST REGISTER */
#define RTK_IPSEKCR                                      0x0034 /*!< IPSEC EFUSE KEY CONTROL REGISTER */
#define RTK_DDSR                                         0x1000 /*!< DESTINATION DESCRIPTOR STATUS REGISTER */
#define RTK_DDFWR                                        0x1004 /*!< DESTINATION DESCRIPTOR FIRST WORD REGISTER */
#define RTK_DDSWR                                        0x1008 /*!< DESTINATION DESCRIPTOR SECOND WORD REGISTER */
#define RTK_DES_PKTCONF                                  0x100C /*!< IPSEC DESCRIPTOR PACKET CONFIGURE REGISTER */
#define RTK_DBGSDR                                       0x1010 /*!< DEBUG SOURCE DESCRIPTOR DATA REGISTER */
#define RTK_DBGDDR                                       0x1014 /*!< DEBUG DESTINATION DESCRIPTOR DATA REGISTER */

/* Definitions for hash flags */
#define FIFOCNT_TIMEOUT				0x100000

/**
  * @brief  CRYPTO source descriptor structure definition
  */
typedef union {
	struct {
		u32 key_len: 4;			/*Offset 0, Bit[3:0], key length				*/
		u32 iv_len: 4;			/*Offset 0, Bit[7:3], IV length				*/
		u32 keypad_len: 8;		/*Offset 0, Bit[15:8], pad length			*/
		u32 hash_iv_len: 6;		/*Offset 0, Bit[21:16], Hash initial value length	*/
		u32 ap: 2;				/*Offset 0, Bit[23:22], auto-padding		*/
		u32 cl: 2;				/*Offset 0, Bit[25:24], Command length		*/
		u32 priv_key: 1;		/*Offset 0, Bit[26], Secure Key				*/
		u32 otp_key: 1;			/*Offset 0, Bit[27], Secure Key				*/
		u32 ls: 1;				/*Offset 0, Bit[28], Last segment descriptor	*/
		u32 fs: 1;				/*Offset 0, Bit[29], First segment descriptor	*/
		u32 rs: 1;				/*Offset 0, Bit[30], Read data source 		*/
		u32 rsvd: 1;			/*Offset 0, Bit[31], Reserved				*/
	} b;

	struct {
		u32 apl: 8;				/*Offset 0, Bit[7:0], Auth padding length		*/
		u32 a2eo: 5;			/*Offset 0, Bit[12:8], Auth to encryption offset	*/
		u32 zero: 1;			/*Offset 0, Bit[13], 0/1					*/
		u32 enl: 14;			/*Offset 0, Bit[27:14], Encryption data length	*/
		u32 ls: 1;				/*Offset 0, Bit[28], Last segment descriptor	*/
		u32 fs: 1;				/*Offset 0, Bit[29], First segment descriptor	*/
		u32 rs: 1;				/*Offset 0, Bit[30], Read data source		*/
		u32 rsvd: 1;			/*Offset 0, Bit[31], Reserved				*/
	} d;

	u32 w;
} rtl_crypto_srcdesc_t;

/**
  * @brief  CRYPTO destination descriptor structure definition
  */
typedef union {
	struct {
		u32 adl: 8;				/*Offset 0, Bit[7:0], Auth data length		*/
		u32 rsvd1: 19;			/*Offset 0, Bit[26:8], Reserved				*/
		u32 enc: 1;				/*Offset 0, Bit[27], Cipher or auth			*/
		u32 ls: 1;				/*Offset 0, Bit[28], Last segment descriptor	*/
		u32 fs: 1;				/*Offset 0, Bit[29], First segment descriptor	*/
		u32 ws: 1;				/*Offset 0, Bit[30], Write data source		*/
		u32 rsvd2: 1;			/*Offset 0, Bit[31], Reserved				*/
	} auth;

	struct {
		u32 enl: 24;			/*Offset 0, Bit[23:0], Auth padding length		*/
		u32 rsvd1: 3;			/*Offset 0, Bit[26:24], Reserved			*/
		u32 enc: 1;				/*Offset 0, Bit[27], Cipher or auth			*/
		u32 ls: 1;				/*Offset 0, Bit[28], Last segment descriptor	*/
		u32 fs: 1;				/*Offset 0, Bit[29], First segment descriptor	*/
		u32 ws: 1;				/*Offset 0, Bit[30], Write data source		*/
		u32 rsvd2: 1;			/*Offset 0, Bit[31], Reserved				*/
	} cipher;

	u32 w;
} rtl_crypto_dstdesc_t;

/**
  * @brief  CRYPTO command buffer structure definition
  */
typedef struct rtl_crypto_cl_struct_s {
	// offset 0 :
	u32 cipher_mode: 4;				/*Offset 0, Bit[3:0], Cipher mode			*/
	u32 cipher_eng_sel: 2;			/*Offset 0, Bit[5:4], Cipher algorithm selected	*/
	u32 rsvd1: 1;					/*Offset 0, Bit[6], Reserved				*/
	u32 cipher_encrypt: 1;			/*Offset 0, Bit[7], Encryption or decryption	*/
	u32 aes_key_sel: 2;				/*Offset 0, Bit[9:8], AES key length			*/
	u32 des3_en: 1;					/*Offset 0, Bit[10], 3DES					*/
	u32 des3_type: 1;				/*Offset 0, Bit[11], 3DES type				*/
	u32 ckbs: 1;					/*Offset 0, Bit[12], Cipher key byte swap		*/
	u32 hmac_en: 1;					/*Offset 0, Bit[13], HMAC 				*/
	u32 hmac_mode: 3;				/*Offset 0, Bit[16:14], Hash algorithm		*/
	u32 hmac_seq_hash_last: 1;		/*Offset 0, Bit[17], the last payload(seq hash)	*/
	u32 engine_mode: 3;				/*Offset 0, Bit[20:18], engine mode			*/
	u32 hmac_seq_hash_first: 1;		/*Offset 0, Bit[21], the first payload(seq hash)	*/
	u32 hmac_seq_hash: 1;			/*Offset 0, Bit[22], Seqential hash			*/
	u32 hmac_seq_hash_no_wb: 1;		/*Offset 0, Bit[23], Result hash output		*/
	u32 icv_total_length: 8;		/*Offset 0, Bit[31:24], icv length			*/

	// offset 4
	u32 aad_last_data_size: 4;		/*Offset 4, Bit[3:0], AAD last block data size				*/
	u32 enc_last_data_size: 4;		/*Offset 4, Bit[7:4], Message last block data size			*/
	u32 pad_last_data_size: 3;		/*Offset 4, Bit[10:8], Hash padding last block data size		*/
	u32 ckws: 1;					/*Offset 4, Bit[11], Cipher key word swap					*/
	u32 enc_pad_last_data_size: 3;	/*Offset 4, Bit[14:12], Encryption padding last block data size	*/
	u32 hsibs: 1;					/*Offset 4, Bit[15], Hash sequential initial value byte swap		*/
	u32 caws: 1;					/*Offset 4, Bit[16], Cipher align word swap				*/
	u32 cabs: 1;					/*Offset 4, Bit[17], Cipher align byte swap				*/
	u32 ciws: 1;					/*Offset 4, Bit[18], Cipher input word swap				*/
	u32 cibs: 1;					/*Offset 4, Bit[19], Cipher input byte swap				*/
	u32 cows: 1;					/*Offset 4, Bit[20], Cipher output word swap				*/
	u32 cobs: 1;					/*Offset 4, Bit[21], Cipher output byte swap				*/
	u32 codws: 1;					/*Offset 4, Bit[22], Cipher output double word swap			*/
	u32 cidws: 1;					/*Offset 4, Bit[23], Cipher input double word swap			*/
	u32 haws: 1;					/*Offset 4, Bit[24], Hash align word swap					*/
	u32 habs: 1;					/*Offset 4, Bit[25], Hash align byte swap					*/
	u32 hiws: 1;					/*Offset 4, Bit[26], Hash input word swap					*/
	u32 hibs: 1;					/*Offset 4, Bit[27], Hash input byte swap					*/
	u32 hows: 1;					/*Offset 4, Bit[28], Hash output word swap				*/
	u32 hobs: 1;					/*Offset 4, Bit[29], Hash output byte swap				*/
	u32 hkws: 1;					/*Offset 4, Bit[30], Hash key word swap					*/
	u32 hkbs: 1;					/*Offset 4, Bit[31], Hash key byte swap					*/

	// offset 8
	u32 hash_pad_len: 8;			/*Offset 8, Bit[7:0], Hash padding total length		*/
	u32 header_total_len: 6;		/*Offset 8, Bit[13:8], A2EO total length			*/
	u32 apl: 2;						/*Offset 8, Bit[15:14], Encryption padding total length*/
	u32 enl: 16;					/*Offset 8, Bit[31:16], Message total length		*/

	// offset
	u32 ap0;						/*Offset 12, Bit[31:0], Message length[31:0]		*/
	u32 ap1;						/*Offset 16, Bit[31:0], Message length[63:32]		*/
	u32 ap2;						/*Offset 20, Bit[31:0], Message length[95:64]		*/
	u32 ap3;						/*Offset 24, Bit[31:0], Message length[127:96]	*/
} rtl_crypto_cl_t;

/**
 * struct realtek_crypto_drv - crypto device list
 * @dev: platform driver device
 * @io_base: ioremapped base IO addr of crypto HW
 * @dev_list: crypto device list
 * @drv_mutex: driver mutex to protect global data
 * @pdata: platform data
 */
struct realtek_crypto_drv {
	struct device	*dev;
	void __iomem	*io_base;
	struct list_head	dev_list;
	struct mutex	drv_mutex;
	const struct realtek_hash_pdata	*pdata;
};

#endif
