// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Hash support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __REALTEK_HASH_H
#define __REALTEK_HASH_H

/* Definitions for hash flags */
#define HASH_FLAGS_MD5				BIT(0)
#define HASH_FLAGS_SHA1				BIT(1)
#define HASH_FLAGS_SHA224			BIT(2)
#define HASH_FLAGS_SHA256			BIT(3)
#define HASH_FLAGS_SHA384			BIT(4)
#define HASH_FLAGS_SHA512			BIT(5)
#define HASH_FLAGS_HMAC				BIT(6)
/* Buffer length */
#define HASH_HMAC_MAX_KLEN			(255 * 4)

#define IOPAD_LEN				(128 * 2 + 4)

/**
 * struct realtek_hash_pdata - hash data
 * @algs_list: hash algorithms list
 * @algs_list_size: algorithms list size
 */
struct realtek_hash_pdata {
	struct ahash_alg	*algs_list;
	size_t			algs_list_size;
};

/**
 * struct realtek_hash_ctx - hash context
 * @flags: algorithms flags
 * @ipad: hmac ipad
 * @opad: hmac opad
 * @g_IOPAD: hmac ipad and opad result
 * @dma_handle_io: io pad dma address
 */
struct realtek_hash_ctx {
	unsigned int		flags;
	u8 *ipad;						/*HMAC ipad */
	u8 *opad;						/*HMAC opad */
	u8 g_IOPAD[IOPAD_LEN] __aligned(sizeof(u32));
	dma_addr_t dma_handle_io;
};

/**
 * struct realtek_hash_request_ctx - hash request context
 * @hdev: realtek hash driver data
 * @flags: algorithms flags
 * @enl: hash message length
 * @apl: message padding length
 * @enc_last_data_size: message last data size
 * @hash_digest_result: point to digest
 * @digcnt: digest size
 * @dma_handle_dig: digest dma address
 * @state: hash digest state
 * @st_len: hash digest state length
 * @dma_handle_st: state dma address
 * @initial: seq hash init state
 * @hmac_seq_hash_first: seq hash the first message payload block
 * @hmac_seq_hash_last: seq hash the last message payload block
 * @hmac_seq_hash_total_len: seq hash message total length
 * @hmac_seq_last_message: previous message payload
 * @hmac_seq_last_msglen: previous message payload length
 * @hmac_seq_buf_is_used_bytes: seq buf used bytes(total 64-bytes)
 * @lasthash: last seq hash
 * @hmac_seq_buf: seq hash buffer
 * @cl_buffer: command buffer
 * @dma_handle_cl: command buffer dma address
 * @dma_handle_pad: data pad dma address
 */
struct realtek_hash_request_ctx {
	struct realtek_hash_dev	*hdev;
	unsigned int		flags;
	u32 enl;
	u32 apl;
	u32 enc_last_data_size;
	u8 *hash_digest_result;
	unsigned int		digcnt;
	dma_addr_t dma_handle_dig;
	u32 state[16] __aligned(8);            /*!< intermediate digest state  */;
	unsigned int st_len;
	dma_addr_t dma_handle_st;

	// sequential hash
	u8 initial;
	u8 hmac_seq_hash_first;
	u8 hmac_seq_hash_last;
	u32 hmac_seq_hash_total_len;
	u8 *hmac_seq_last_message;
	u32 hmac_seq_last_msglen;
	u8 hmac_seq_buf_is_used_bytes;
	u32 lasthash;
	u8  hmac_seq_buf[128] __aligned(sizeof(u32));
	u8  cl_buffer[32] __aligned(sizeof(u32));
	dma_addr_t dma_handle_cl;
	dma_addr_t dma_handle_pad;
};

/**
 * struct realtek_hash_hw_context - hash export/import context
 * @hdev: realtek hash driver data
 * @flags: algorithms flags
 * @hash_digest_result: point to digest
 * @dma_handle_dig: digest dma address
 * @digestlen: digest size
 * @initial: seq hash init state
 * @hmac_seq_hash_first: seq hash the first message payload block
 * @hmac_seq_hash_last: seq hash the last message payload block
 * @hmac_seq_hash_total_len: seq hash message total length
 * @hmac_seq_buf_is_used_bytes: seq buf used bytes(total 64-bytes)
 * @buffer: seq hash buffer
 * @lasthash: last seq hash
 * @state: hash digest state
 */
struct realtek_hash_hw_context {
	struct realtek_hash_dev	*hdev;
	unsigned int		flags;
	u8 *hash_digest_result;
	dma_addr_t dma_handle_dig;
	u32 digestlen;
	u8 initial;
	u8 hmac_seq_hash_first;
	u8 hmac_seq_hash_last;
	u32 hmac_seq_hash_total_len;
	u8 hmac_seq_buf_is_used_bytes;
	unsigned char buffer[128];   	/*!< data block being processed */
	u32 lasthash;
	u32 state[16];
};

/**
 * struct realtek_hash_dev - realtek hash device data, one per tfm
 * @dev: platform driver device
 * @io_base: control registers base cpu addr
 * @list: hash device list
 * @hdev_mutex: mutex to protect multiple hash devices
 * @tfm: tfm associated with this hash device
 * @ctx: ctx for this tfm
 * @rctx: rctx for this tfm
 */
struct realtek_hash_dev {
	struct device	*dev;
	void __iomem	*io_base;
	struct list_head	list;
	struct mutex	hdev_mutex;
	struct crypto_tfm	*tfm;
	struct realtek_hash_ctx	*ctx;
	struct realtek_hash_request_ctx	*rctx;
};

#endif
