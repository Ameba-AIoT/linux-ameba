// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek AES support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __REALTEK_AES_H
#define __REALTEK_AES_H

/* Definitions for aes type */
#define AES_TYPE_ECB 		0x0
#define AES_TYPE_CBC 		0x1
#define AES_TYPE_CFB 		0x2
#define AES_TYPE_OFB 		0x3
#define AES_TYPE_CTR 		0x4
#define AES_TYPE_GCTR 		0x5
#define AES_TYPE_GMAC 		0x6
#define AES_TYPE_GHASH 	0x7
#define AES_TYPE_GCM 		0x8

/**
 * struct realtek_aes_ctx - aes context
 * @keylen: aes key length
 * @key: aes key
 * @ivlen: aes key length
 * @iv: aes iv
 * @a2eo: authentication to encryption offset
 * @aad_last_data_size: AAD last data size
 * @apl_aad: AAD padding length
 * @enl: message length
 * @apl: message padding length
 * @enc_last_data_size: message last data size
 * @cipher_type: aes cipher type
 * @cl_buffer: command buffer
 * @cl_key_iv: command+key+iv buffer
 * @dma_handle_cl: command+key+iv buffer dma address
 * @isDecrypt: 0-encryption, 1-decryption
 * @adev: realtek aes driver data
 * @buf_in: point to in data
 * @buf_in_len: in buffer length
 * @buf_out: point to out data
 * @buf_out_len: out buffer length
 * @dma_handle_msg: message dma address
 * @dma_handle_pad: padding dma address
 * @dma_handle_dst: result dma address
 */
struct realtek_aes_ctx {
	int keylen;
	u8 key[AES_MAX_KEY_SIZE];
	int ivlen;
	u8 iv[AES_BLOCK_SIZE];
	u32 a2eo;
	u32 aad_last_data_size;
	u32 apl_aad;
	u32 enl;
	u32 apl;
	u32 enc_last_data_size;
	u32 cipher_type;
	u8  cl_buffer[32];
	u8  cl_key_iv[12 + AES_MAX_KEY_SIZE + AES_BLOCK_SIZE] __aligned(sizeof(u32));
	dma_addr_t dma_handle_cl;
	u8 isDecrypt;
	struct realtek_aes_dev *adev;
	u8 *buf_in;
	int buf_in_len;
	u8 *buf_out;
	int buf_out_len;
	dma_addr_t dma_handle_msg;
	dma_addr_t dma_handle_pad;
	dma_addr_t dma_handle_dst;
};

/**
 * struct realtek_aes_dev - realtek aes driver data
 * @dev: platform driver device
 * @io_base: control registers base cpu addr
 * @list: aes device list
 * @adev_mutex: mutex to protect multiple aes devices
 * @tfm: tfm associated with this hash device
 * @ctx: ctx for this tfm
 */
struct realtek_aes_dev {
	struct device	*dev;
	void __iomem	*io_base;
	struct list_head	list;
	struct mutex	adev_mutex;
	struct crypto_tfm	*tfm;
	struct realtek_aes_ctx	*ctx;
};

#endif
