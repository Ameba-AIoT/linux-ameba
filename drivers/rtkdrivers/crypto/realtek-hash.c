// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Hash support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/clk.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/scatterwalk.h>
#include <crypto/sha.h>
#include <crypto/internal/hash.h>

#include "realtek-crypto.h"
#include "realtek-hash.h"


__aligned(32) static u8 ipsec_padding[64]  = { 0x0 };

static struct realtek_crypto_drv realtek_hash = {
	.dev_list = LIST_HEAD_INIT(realtek_hash.dev_list),
};

static int realtek_hash_wait_ok(struct realtek_hash_dev *hdev)
{
	u32 status;
	u32 ips_err;
	int ret;

	ret = readl_relaxed_poll_timeout(hdev->io_base + RTK_RSTEACONFISR, status, (status & IPSEC_BIT_CMD_OK), 10, 1000000);
	if (ret) {
		return ret;
	}

	ips_err = readl(hdev->io_base + RTK_ERRSR);
	if (ips_err) {
		dev_err(hdev->dev, "RTK_ERRSR = 0x%08X\n", ips_err);
		ret = -ETIMEDOUT;
	}
	return ret;
}

static void realtek_hash_mem_dump(struct device *dev, const u8 *start, u32 size, char *strHeader)
{
	int index;
	u8 *buf;

	if (!start || (size == 0)) {
		return;
	}

	buf = (u8 *)start;

	if (strHeader) {
		dev_dbg(dev, "Dump %s addr = 0x%08X, size = %d\n", strHeader, (u32)start, size);
	}

	for (index = 0; index < size; index++) {
		dev_dbg(dev, "Dump mem[%d] = 0x%02X\n", index, buf[index]);
	}
}

/**
  * @brief  Set source descriptor.
  * @param  hdev: hash driver data
  * @param  sd1: Source descriptor first word.
  * @param  sdpr: Source descriptor second word.
  * @retval None
  */
static void realtek_hash_set_src_desc(struct realtek_hash_dev *hdev, u32 sd1, dma_addr_t sdpr)
{
	u32 timeout;
	u32 tmp_value;

	timeout = FIFOCNT_TIMEOUT;
	while (1) {
		tmp_value = readl(hdev->io_base + RTK_SDSR);
		if ((tmp_value & IPSEC_MASK_FIFO_EMPTY_CNT) > 0) {
			dev_dbg(hdev->dev, "Set sd1 = 0x%08X , sdpr = 0x%08X\n", sd1, sdpr);
			writel(sd1, hdev->io_base + RTK_SDFWR);
			writel(sdpr, hdev->io_base + RTK_SDSWR);
			break;
		}
		timeout--;
		if (timeout == 0) {
			dev_err(hdev->dev, "Timeout, src fifo is full\n");
			break;
		}
	}
}

/**
  * @brief  Set destination descriptor.
  * @param  hdev: hash driver data
  * @param  dd1: Destination descriptor first word.
  * @param  ddpr: Destination descriptor second word.
  * @retval None
  */
static void realtek_hash_set_dst_desc(struct realtek_hash_dev *hdev, u32 dd1, dma_addr_t ddpr)
{
	u32 tmp_value;

	tmp_value = readl(hdev->io_base + RTK_DDSR);
	if ((tmp_value & IPSEC_MASK_FIFO_EMPTY_CNT) > 0) {
		dev_dbg(hdev->dev, "Set dd1 = 0x%08X, ddpr = 0x%08X\n", dd1, ddpr);
		writel(dd1, hdev->io_base + RTK_DDFWR);
		writel(ddpr, hdev->io_base + RTK_DDSWR);
	} else {
		dev_err(hdev->dev, "Destination fifo_cnt %d is not correct\n", IPSEC_GET_FIFO_EMPTY_CNT(tmp_value));
	}
}

/**
  * @brief  Clear command ok and error interrupts.
  * @param  hdev: hash driver data
  * @retval None
  */
static void realtek_hash_clear_all(struct realtek_hash_dev *hdev)
{
	u32 ok_intr_cnt = 0;
	u32 tmp_value;

	writel(0xFFFF, hdev->io_base + RTK_ERRSR);
	tmp_value = readl(hdev->io_base + RTK_RSTEACONFISR);
	ok_intr_cnt = IPSEC_GET_OK_INTR_CNT(tmp_value);
	tmp_value |= (IPSEC_CLEAR_OK_INT_NUM(ok_intr_cnt) | IPSEC_BIT_CMD_OK);
	writel(tmp_value, hdev->io_base + RTK_RSTEACONFISR);
}

/**
  * @brief  Set source descriptor command buffer.
  * @param  req: Point to hash request.
  * @retval None
  */
static void realtek_hash_set_cmd_buf(struct ahash_request *req)
{
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	rtl_crypto_cl_t *pCL;
	u32 enl;
	u32 blocksize = 64;

	enl = rctx->enl;

	pCL = (rtl_crypto_cl_t *)rctx->cl_buffer;

	memset(pCL, 0, sizeof(rctx->cl_buffer));

	pCL->engine_mode = 1; //hash only
	// Sequential hash setting
	pCL->hmac_seq_hash = 1;
	pCL->hmac_seq_hash_first = 0;
	if (rctx->hmac_seq_hash_first == 1) {
		pCL->hmac_seq_hash_first = 1;
		rctx->hmac_seq_hash_first = 0;
	}

	//Swap settting
	if (rctx->flags & HASH_FLAGS_MD5) {
		pCL->habs = 1;
		pCL->hibs = 1;
		pCL->hobs = 1;
		pCL->hkbs = 1;
	} else if (rctx->flags & HASH_FLAGS_SHA1) {
		pCL->hmac_mode = 1;
	} else if (rctx->flags & HASH_FLAGS_SHA224) {
		pCL->hmac_mode = 0x2;
	} else if (rctx->flags & HASH_FLAGS_SHA256) {
		pCL->hmac_mode = 0x3;
	} else if (rctx->flags & HASH_FLAGS_SHA384) {
		pCL->hmac_mode = 0x4;
		blocksize = 128;
	} else if (rctx->flags & HASH_FLAGS_SHA512) {
		pCL->hmac_mode = 0x5;
		blocksize = 128;
	}

	if (rctx->flags & HASH_FLAGS_HMAC) {
		pCL->hmac_en = 1;
	}
	// Use auto padding and sequential hash II
	if (blocksize == 64) {
		// Use auto padding and sequential hash II
		if (rctx->hmac_seq_hash_last == 1) {	//The last block autopadding: 16-byte
			// always using auto padding
			pCL->enl = (enl + 15) / 16 ;
			pCL->enc_last_data_size = rctx->enc_last_data_size;
			pCL->ap0 = rctx->hmac_seq_hash_total_len * 8;
			if (rctx->flags & HASH_FLAGS_HMAC) {
				pCL->ap0 += blocksize * 8;
			}
		} else {	//MD5/SHA1/SHA224/SHA256: 64-byte
			pCL->enl = enl / 64;
		}
	} else if (blocksize == 128) {
		// Use auto padding and sequential hash II
		if (rctx->hmac_seq_hash_last == 1) {	//The last block autopadding: 16-byte
			// always using auto padding
			pCL->enl = (enl + 15) / 16 ;
			pCL->enc_last_data_size = rctx->enc_last_data_size;
			pCL->ap0 = rctx->hmac_seq_hash_total_len * 8;
			if (rctx->flags & HASH_FLAGS_HMAC) {
				pCL->ap0 += blocksize * 8;
			}
		} else {
			pCL->enl = enl / 128;
		}
	}

	pCL->hmac_seq_hash_last = 0;
	pCL->hmac_seq_hash_no_wb = 0;
	if (rctx->hmac_seq_hash_last == 1) {
		pCL->hmac_seq_hash_last = 1;
	}

	pCL->icv_total_length = 0x40; // for mix mode, but need to set a value 0x40
}

/**
  * @brief  Set source descriptor command, pad array.
  * @param  req: Point to hash request.
  * @retval None
  */
static int realtek_hash_cmd_pad(struct ahash_request *req)
{
	rtl_crypto_srcdesc_t src_desc;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	unsigned int i;

	src_desc.w = 0;

	// FS=1, CL=3
	src_desc.b.rs = 1;
	src_desc.b.fs = 1;
	src_desc.b.cl = 3;

	// auto padding
	if (rctx->hmac_seq_hash_last) {
		src_desc.b.ap = 0x01;
	}

	//Set command buffer array
	realtek_hash_set_cmd_buf(req);
	realtek_hash_mem_dump(rctx->hdev->dev, (const u8 *)(&(rctx->cl_buffer)[0]), sizeof(rctx->cl_buffer), "Command Setting: ");
	rctx->dma_handle_cl = dma_map_single(rctx->hdev->dev, &(rctx->cl_buffer[0]), sizeof(rctx->cl_buffer), DMA_TO_DEVICE);
	if (dma_mapping_error(rctx->hdev->dev, rctx->dma_handle_cl)) {
		return -ENOMEM;
	}
	realtek_hash_set_src_desc(rctx->hdev, src_desc.w, rctx->dma_handle_cl);

	// Set Pad array
	if (rctx->flags & HASH_FLAGS_HMAC) {
		src_desc.w = 0;
		src_desc.b.rs = 1;
		src_desc.b.fs = 1;
		src_desc.b.keypad_len = 64;
		ctx->dma_handle_io = dma_map_single(rctx->hdev->dev, &(ctx->g_IOPAD[0]), sizeof(ctx->g_IOPAD), DMA_TO_DEVICE);
		if (dma_mapping_error(rctx->hdev->dev, ctx->dma_handle_io)) {
			return -ENOMEM;
		}

		realtek_hash_mem_dump(rctx->hdev->dev, (const u8 *)(&(ctx->g_IOPAD)[0]), sizeof(ctx->g_IOPAD), "IO PAD: ");
		realtek_hash_set_src_desc(rctx->hdev, src_desc.w, ctx->dma_handle_io);
	}

	//set initial value
	rctx->st_len = rctx->digcnt;
	if (rctx->st_len == 28) {
		rctx->st_len = 32;
	} else if (rctx->st_len == 48) {
		rctx->st_len = 64;
	}
	if (rctx->initial == 1) {
		rctx->initial = 0;
	} else {
		memcpy(rctx->state, rctx->hash_digest_result, rctx->st_len);
		if (!(rctx->flags & HASH_FLAGS_MD5)) {
			for (i = 0; i < (rctx->st_len / 4); i++) {
				rctx->state[i] = ((u32)(
									  (((u32)(rctx->state[i]) & (u32)0x000000ffUL) << 24) |
									  (((u32)(rctx->state[i]) & (u32)0x0000ff00UL) <<  8) |
									  (((u32)(rctx->state[i]) & (u32)0x00ff0000UL) >>  8) |
									  (((u32)(rctx->state[i]) & (u32)0xff000000UL) >> 24)));
			}
		}
	}

	rctx->dma_handle_st = dma_map_single(rctx->hdev->dev, &(rctx->state[0]), rctx->st_len, DMA_TO_DEVICE);
	if (dma_mapping_error(rctx->hdev->dev, rctx->dma_handle_st)) {
		return -ENOMEM;
	}
	realtek_hash_mem_dump(rctx->hdev->dev, (const u8 *)(&rctx->state[0]), rctx->st_len, "Initial Value: ");

	src_desc.w = 0;
	src_desc.b.rs = 1;
	src_desc.b.fs = 1;
	src_desc.b.hash_iv_len = rctx->st_len / 4;
	realtek_hash_set_src_desc(rctx->hdev, src_desc.w, rctx->dma_handle_st);

	return 0;
}

/**
  * @brief  Crypto engine hash process.
  * @param  req: Point to hash request.
  * @retval Result of process.
  */
static int realtek_hash_process(struct ahash_request *req, u8 *message, u32 msglen, u8 *pDigest)
{
	rtl_crypto_srcdesc_t srcdesc;
	rtl_crypto_dstdesc_t dst_desc;
	u32 enl;
	u32 digestlen;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	int ret = 0;
	dma_addr_t dma_handle_msg = NULL;

	// Use only one scatter
	enl = msglen;

	realtek_hash_clear_all(rctx->hdev);

	//   Set relative length data
	/* ** Calculate message auto padding length ** */
	rctx->enl = enl;
	rctx->enc_last_data_size = enl % 16;
	rctx->apl = (16 - rctx->enc_last_data_size) % 16;

	digestlen = rctx->digcnt;

	rctx->hmac_seq_hash_total_len += msglen;

	if (rctx->lasthash) {
		rctx->hmac_seq_hash_last = 1;
	}

	/********************************************
	 * step 1: Setup desitination descriptor
	 * Auth descriptor (1):
	 	-----------------
		|digest			|
		-----------------
	 ********************************************/
	if (rctx->lasthash != 1) {
		if (digestlen == 28) {
			digestlen = 32;
		} else if (digestlen == 48) {
			digestlen = 64;
		}
	}
	dst_desc.w = 0;
	dst_desc.auth.ws = 1;
	dst_desc.auth.fs = 1;
	dst_desc.auth.ls = 1;
	dst_desc.auth.adl = digestlen;
	realtek_hash_set_dst_desc(rctx->hdev, dst_desc.w, rctx->dma_handle_dig);

	/********************************************
	 * step 2: Setup source descriptor
			 ----------
			|Cmd buffer |
			----------
			|PAD array  |
			----------
			|Data 1       |
			|   .             |
			|   .             |
			|   .             |
			|Data N       |
			----------
			|HMAC APL  |
			----------
	 ********************************************/
	/********************************************
	  * step 2-1: prepare Cmd & Pad array:
	  ********************************************/

	if (realtek_hash_cmd_pad(req)) {
		ret = -ENOMEM;
		goto fail;
	}

	/********************************************
	 * step 2-2: prepare Data1 ~ DataN
	 ********************************************/
	srcdesc.w = 0;
	srcdesc.d.rs = 1;

	dma_handle_msg = dma_map_single(rctx->hdev->dev, message, msglen, DMA_TO_DEVICE);
	if (dma_mapping_error(rctx->hdev->dev, dma_handle_msg)) {
		ret = -ENOMEM;
		goto fail;
	}

	while (enl > 16368) {	// Data body 16368-byte per src desc
		srcdesc.d.enl = 16368;
		realtek_hash_set_src_desc(rctx->hdev, srcdesc.w, dma_handle_msg);

		message += 16368;
		dma_handle_msg += 16368;
		enl -= 16368;
		srcdesc.w = 0;
		srcdesc.d.rs = 1;
	}

	// Data body msglen < 16368
	if (rctx->apl == 0) {
		srcdesc.d.ls = 1;
	}
	srcdesc.d.enl = enl;
	realtek_hash_mem_dump(rctx->hdev->dev, (const u8 *)message, srcdesc.d.enl, "message: ");
	realtek_hash_set_src_desc(rctx->hdev, srcdesc.w, dma_handle_msg);

	// data padding, regard as enl
	if (rctx->apl != 0) {
		srcdesc.w = 0;
		srcdesc.d.rs = 1;
		srcdesc.d.enl = rctx->apl;
		srcdesc.d.ls = 1;
		rctx->dma_handle_pad = dma_map_single(rctx->hdev->dev, ipsec_padding, sizeof(ipsec_padding), DMA_TO_DEVICE);
		if (dma_mapping_error(rctx->hdev->dev, rctx->dma_handle_pad)) {
			ret = -ENOMEM;
			goto fail;
		}
		realtek_hash_set_src_desc(rctx->hdev, srcdesc.w, rctx->dma_handle_pad);
	}

	/********************************************
	 * step 3: Wait until ipsec engine stop
	 *Polling mode, intr_mode = 0
	 ********************************************/
	if (realtek_hash_wait_ok(rctx->hdev)) {
		dev_err(rctx->hdev->dev, "Hash process timeout\n");
		ret = -ETIMEDOUT;
	}

fail:
	if (rctx->dma_handle_cl) {
		dma_unmap_single(rctx->hdev->dev, rctx->dma_handle_cl, sizeof(rctx->cl_buffer), DMA_TO_DEVICE);
	}

	if (ctx->dma_handle_io) {
		dma_unmap_single(rctx->hdev->dev, ctx->dma_handle_io, sizeof(ctx->g_IOPAD), DMA_TO_DEVICE);
	}

	if (rctx->dma_handle_st) {
		dma_unmap_single(rctx->hdev->dev, rctx->dma_handle_st, rctx->st_len, DMA_TO_DEVICE);
	}

	if (dma_handle_msg) {
		dma_unmap_single(rctx->hdev->dev, dma_handle_msg, msglen, DMA_TO_DEVICE);
	}

	if (rctx->dma_handle_pad) {
		dma_unmap_single(rctx->hdev->dev, rctx->dma_handle_pad, sizeof(ipsec_padding), DMA_TO_DEVICE);
	}

	realtek_hash_mem_dump(rctx->hdev->dev, (const u8 *)pDigest, digestlen, "digest");

	return ret;
}

/**
  * @brief  Sequential hash process.
  * @param  req: Point to hash request.
  * @note  There exist two buffers : last_message and seq_buf;
  *             last_message : store the previous message pointer.
  *             seq_buf : store the data less than blocksize.
  * @retval Process status.
  */
static int realtek_hash_send_seqbuf(struct ahash_request *req, u8 *pDigest)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	unsigned int blocksize = crypto_ahash_blocksize(tfm);

	int ret = 0;
	int rest_bytes = 0;
	int bodylen;
	int restlen;
	int total_len = rctx->hmac_seq_last_msglen;
	int buf_pos = rctx->hmac_seq_buf_is_used_bytes;

	rest_bytes = blocksize - buf_pos;

	if (total_len < rest_bytes) {	//store into seq buf
		memcpy((void *)(&rctx->hmac_seq_buf[buf_pos]), (const void *)(rctx->hmac_seq_last_message), total_len);
		rctx->hmac_seq_buf_is_used_bytes += total_len;
	} else {
		// send out a seq buf if the seq buf is the last and length is block-byte
		memcpy((void *)(&rctx->hmac_seq_buf[buf_pos]), (const void *)(rctx->hmac_seq_last_message), rest_bytes);
		ret =  realtek_hash_process(req, (u8 *)(rctx->hmac_seq_buf), blocksize, pDigest);
		if (ret != 0) {
			return ret;
		}

		rctx->hmac_seq_buf_is_used_bytes = 0;
		buf_pos = 0;

		total_len -= rest_bytes;
		rctx->hmac_seq_last_msglen = total_len;
		rctx->hmac_seq_last_message += rest_bytes;

		//send out all blocksize-byte align message
		restlen = total_len & (blocksize - 1);
		bodylen = total_len - restlen;
		if (bodylen > 0) {   // there are 64x messages
			ret =  realtek_hash_process(req, rctx->hmac_seq_last_message, bodylen, pDigest);
			if (ret != 0) {
				return ret;
			}
			rctx->hmac_seq_last_message += bodylen;
		}

		// backup the rest
		if (restlen > 0) {
			memcpy((void *)(&rctx->hmac_seq_buf[0]), (const void *)(rctx->hmac_seq_last_message), restlen);
		}
		rctx->hmac_seq_buf_is_used_bytes = restlen;
	}

	return ret;
}

static struct realtek_hash_dev *realtek_hash_find_dev(struct realtek_hash_ctx *ctx)
{
	struct realtek_hash_dev *hdev = NULL, *tmp = NULL;

	list_for_each_entry(tmp, &realtek_hash.dev_list, list) {
		if (tmp->ctx == ctx) {
			hdev = tmp;
			break;
		}
	}

	return hdev;
}

static void realtek_hash_prepare_msg(struct ahash_request *req, u8 *message)
{
	struct scatter_walk walk;

	scatterwalk_start(&walk, req->src);
	scatterwalk_advance(&walk, 0);
	scatterwalk_copychunks(message, &walk, req->nbytes, 0);
	scatterwalk_done(&walk, 0, 0);
}

static int realtek_hash_init(struct ahash_request *req)
{
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct realtek_hash_dev *hdev = NULL;

	memset(rctx, 0, sizeof(struct realtek_hash_request_ctx));

	if (mutex_lock_interruptible(&realtek_hash.drv_mutex)) {
		return -ERESTARTSYS;
	}

	hdev = realtek_hash_find_dev(ctx);
	if (!hdev) {
		mutex_unlock(&realtek_hash.drv_mutex);
		dev_err(realtek_hash.dev, "Failed to find hash dev for requested tfm %p\n", tfm);
		return -ENODEV;
	}
	mutex_unlock(&realtek_hash.drv_mutex);

	if (mutex_lock_interruptible(&hdev->hdev_mutex)) {
		return -ERESTARTSYS;
	}

	rctx->hdev = hdev;
	hdev->rctx = rctx;
	rctx->digcnt = crypto_ahash_digestsize(tfm);

	switch (rctx->digcnt) {
	case MD5_DIGEST_SIZE:
		rctx->flags |= HASH_FLAGS_MD5;
		rctx->state[0] = 0x67452301;
		rctx->state[1] = 0xEFCDAB89;
		rctx->state[2] = 0x98BADCFE;
		rctx->state[3] = 0x10325476;
		break;
	case SHA1_DIGEST_SIZE:
		rctx->flags |= HASH_FLAGS_SHA1;
		rctx->state[0] = 0x67452301;
		rctx->state[1] = 0xEFCDAB89;
		rctx->state[2] = 0x98BADCFE;
		rctx->state[3] = 0x10325476;
		rctx->state[4] = 0xC3D2E1F0;
		break;
	case SHA224_DIGEST_SIZE:
		rctx->flags |= HASH_FLAGS_SHA224;
		rctx->state[0] = 0xC1059ED8;
		rctx->state[1] = 0x367CD507;
		rctx->state[2] = 0x3070DD17;
		rctx->state[3] = 0xF70E5939;
		rctx->state[4] = 0xFFC00B31;
		rctx->state[5] = 0x68581511;
		rctx->state[6] = 0x64F98FA7;
		rctx->state[7] = 0xBEFA4FA4;
		break;
	case SHA256_DIGEST_SIZE:
		rctx->flags |= HASH_FLAGS_SHA256;
		rctx->state[0] = 0x6A09E667;
		rctx->state[1] = 0xBB67AE85;
		rctx->state[2] = 0x3C6EF372;
		rctx->state[3] = 0xA54FF53A;
		rctx->state[4] = 0x510E527F;
		rctx->state[5] = 0x9B05688C;
		rctx->state[6] = 0x1F83D9AB;
		rctx->state[7] = 0x5BE0CD19;
		break;
	case SHA384_DIGEST_SIZE:
		rctx->flags |= HASH_FLAGS_SHA384;
		rctx->state[1] = 0xC1059ED8;
		rctx->state[0] = 0xCBBB9D5D;
		rctx->state[3] = 0x367CD507;
		rctx->state[2] = 0x629A292A;
		rctx->state[5] = 0x3070DD17;
		rctx->state[4] = 0x9159015A;
		rctx->state[7] = 0xF70E5939;
		rctx->state[6] = 0x152FECD8;
		rctx->state[9] = 0xFFC00B31;
		rctx->state[8] = 0x67332667;
		rctx->state[11] = 0x68581511;
		rctx->state[10] = 0x8EB44A87;
		rctx->state[13] = 0x64F98FA7;
		rctx->state[12] = 0xDB0C2E0D;
		rctx->state[15] = 0xBEFA4FA4;
		rctx->state[14] = 0x47B5481D;
		break;
	case SHA512_DIGEST_SIZE:
		rctx->flags |= HASH_FLAGS_SHA512;
		rctx->state[1] = 0xF3BCC908;
		rctx->state[0] = 0x6A09E667;
		rctx->state[3] = 0x84CAA73B;
		rctx->state[2] = 0xBB67AE85;
		rctx->state[5] = 0xFE94F82B;
		rctx->state[4] = 0x3C6EF372;
		rctx->state[7] = 0x5F1D36F1;
		rctx->state[6] = 0xA54FF53A;
		rctx->state[9] = 0xADE682D1;
		rctx->state[8] = 0x510E527F;
		rctx->state[11] = 0x2B3E6C1F;
		rctx->state[10] = 0x9B05688C;
		rctx->state[13] = 0xFB41BD6B;
		rctx->state[12] = 0x1F83D9AB;
		rctx->state[15] = 0x137E2179;
		rctx->state[14] = 0x5BE0CD19;
		break;
	default:
		mutex_unlock(&hdev->hdev_mutex);
		dev_err(hdev->dev, "Unsupported algo\n");
		return -EINVAL;
	}

	if (ctx->flags & HASH_FLAGS_HMAC) {
		rctx->flags |= HASH_FLAGS_HMAC;
	}

	rctx->initial = 1;
	rctx->hmac_seq_hash_first = 1;
	rctx->hmac_seq_hash_last = 0;
	rctx->hmac_seq_hash_total_len = 0;

	dev_dbg(hdev->dev, "Hash init flags = 0x%08X\n", rctx->flags);

	rctx->hash_digest_result = (u8 *)dma_alloc_coherent(hdev->dev, rctx->digcnt, &rctx->dma_handle_dig, GFP_KERNEL);
	if (!rctx->hash_digest_result) {
		dev_err(hdev->dev, "Can't allocate digest buffer\n");
		mutex_unlock(&hdev->hdev_mutex);
		return -EFAULT;
	}

	mutex_unlock(&hdev->hdev_mutex);
	return 0;
}

static int realtek_hash_update(struct ahash_request *req)
{
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct realtek_hash_dev *hdev = NULL;
	u8 *message;
	int ret = 0;

	if (mutex_lock_interruptible(&realtek_hash.drv_mutex)) {
		return -ERESTARTSYS;
	}

	hdev = realtek_hash_find_dev(ctx);
	if (!hdev) {
		mutex_unlock(&realtek_hash.drv_mutex);
		dev_err(realtek_hash.dev, "Failed to find hash dev for requested tfm %p\n", tfm);
		return -ENODEV;
	}
	mutex_unlock(&realtek_hash.drv_mutex);

	if (mutex_lock_interruptible(&hdev->hdev_mutex)) {
		return -ERESTARTSYS;
	}

	if (!req->nbytes) {
		mutex_unlock(&hdev->hdev_mutex);
		dev_dbg(hdev->dev, "Update called with 0 data\n");
		return 0;
	}

	message = devm_kzalloc(hdev->dev, req->nbytes, GFP_KERNEL);
	if (!message) {
		dev_err(hdev->dev, "Can't allocate message buffer\n");
		ret = -EFAULT;
		goto fail;
	}

	realtek_hash_prepare_msg(req, message);

	rctx->hmac_seq_last_message = message;
	rctx->hmac_seq_last_msglen = req->nbytes;
	ret = realtek_hash_send_seqbuf(req, rctx->hash_digest_result);
	if (ret) {
		goto fail;
	}

	mutex_unlock(&hdev->hdev_mutex);
	return ret;

fail:
	if (rctx->hash_digest_result) {
		dma_free_coherent(hdev->dev, rctx->digcnt, rctx->hash_digest_result, rctx->dma_handle_dig);
	}
	mutex_unlock(&hdev->hdev_mutex);
	return ret;
}

static int realtek_hash_final(struct ahash_request *req)
{
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct realtek_hash_dev *hdev = NULL;
	int ret = 0;

	if (mutex_lock_interruptible(&realtek_hash.drv_mutex)) {
		return -ERESTARTSYS;
	}

	hdev = realtek_hash_find_dev(ctx);
	if (!hdev) {
		mutex_unlock(&realtek_hash.drv_mutex);
		dev_err(realtek_hash.dev, "Failed to find hash dev for requested tfm %p\n", tfm);
		return -ENODEV;
	}
	mutex_unlock(&realtek_hash.drv_mutex);

	if (mutex_lock_interruptible(&hdev->hdev_mutex)) {
		return -ERESTARTSYS;
	}

	rctx->lasthash = 1;
	ret = realtek_hash_process(req, (u8 *)(&rctx->hmac_seq_buf[0]), rctx->hmac_seq_buf_is_used_bytes, rctx->hash_digest_result);
	memcpy(req->result, rctx->hash_digest_result, rctx->digcnt);
	realtek_hash_mem_dump(hdev->dev, req->result, rctx->digcnt, "Final digest: ");

	if (rctx->hash_digest_result) {
		dma_free_coherent(hdev->dev, rctx->digcnt, rctx->hash_digest_result, rctx->dma_handle_dig);
	}
	mutex_unlock(&hdev->hdev_mutex);

	return ret;
}

static int realtek_hash_finup(struct ahash_request *req)
{
	int err1, err2;

	err1 = realtek_hash_update(req);

	if (err1) {
		return err1;
	}

	/*
	 * final() has to be always called to cleanup resources
	 * even if update() failed, except EINPROGRESS
	 */
	err2 = realtek_hash_final(req);

	return err2;
}

static int realtek_hash_digest(struct ahash_request *req)
{
	return realtek_hash_init(req) ? : realtek_hash_finup(req);
}

static int realtek_hash_export(struct ahash_request *req, void *out)
{
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct realtek_hash_hw_context *octx = out;
	struct realtek_hash_dev *hdev = NULL;

	if (mutex_lock_interruptible(&realtek_hash.drv_mutex)) {
		return -ERESTARTSYS;
	}

	hdev = realtek_hash_find_dev(ctx);
	if (!hdev) {
		mutex_unlock(&realtek_hash.drv_mutex);
		dev_err(realtek_hash.dev, "Failed to find hash dev for requested tfm %p\n", tfm);
		return -ENODEV;
	}
	mutex_unlock(&realtek_hash.drv_mutex);

	if (mutex_lock_interruptible(&hdev->hdev_mutex)) {
		return -ERESTARTSYS;
	}

	octx->hdev = rctx->hdev;
	octx->flags = rctx->flags;
	octx->digestlen = rctx->digcnt;
	octx->hash_digest_result = rctx->hash_digest_result;
	octx->dma_handle_dig = rctx->dma_handle_dig;
	octx->initial = rctx->initial;
	octx->hmac_seq_hash_first = rctx->hmac_seq_hash_first;
	octx->hmac_seq_hash_last = rctx->hmac_seq_hash_last;
	octx->hmac_seq_hash_total_len = rctx->hmac_seq_hash_total_len;
	octx->hmac_seq_buf_is_used_bytes = rctx->hmac_seq_buf_is_used_bytes;
	octx->lasthash = rctx->lasthash;

	memcpy(octx->buffer, rctx->hmac_seq_buf, 128);
	memcpy(octx->state, rctx->state, 64);

	mutex_unlock(&hdev->hdev_mutex);
	return 0;
}

static int realtek_hash_import(struct ahash_request *req, const void *in)
{
	struct realtek_hash_request_ctx *rctx = ahash_request_ctx(req);
	const struct realtek_hash_hw_context *ictx = in;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct realtek_hash_dev *hdev = NULL;

	if (mutex_lock_interruptible(&realtek_hash.drv_mutex)) {
		return -ERESTARTSYS;
	}

	hdev = realtek_hash_find_dev(ctx);
	if (!hdev) {
		mutex_unlock(&realtek_hash.drv_mutex);
		dev_err(realtek_hash.dev, "Failed to find hash dev for requested tfm %p\n", tfm);
		return -ENODEV;
	}
	mutex_unlock(&realtek_hash.drv_mutex);

	if (mutex_lock_interruptible(&hdev->hdev_mutex)) {
		return -ERESTARTSYS;
	}

	rctx->hdev = ictx->hdev;
	rctx->flags = ictx->flags;
	rctx->hash_digest_result = ictx->hash_digest_result;
	rctx->dma_handle_dig = ictx->dma_handle_dig;
	rctx->digcnt = ictx->digestlen;
	rctx->initial = ictx->initial;
	rctx->hmac_seq_hash_first = ictx->hmac_seq_hash_first;
	rctx->hmac_seq_hash_last = ictx->hmac_seq_hash_last;
	rctx->hmac_seq_hash_total_len = ictx->hmac_seq_hash_total_len;
	rctx->hmac_seq_buf_is_used_bytes = ictx->hmac_seq_buf_is_used_bytes;
	rctx->lasthash = ictx->lasthash;

	memcpy(rctx->hmac_seq_buf, ictx->buffer, 128);
	memcpy(rctx->state, ictx->state, 64);

	mutex_unlock(&hdev->hdev_mutex);
	return 0;
}

static int realtek_hash_key_process(struct realtek_hash_dev *hdev, struct crypto_ahash *tfm, u8 *message, u32 msglen, u8 *pDigest)
{
	rtl_crypto_srcdesc_t srcdesc;
	rtl_crypto_dstdesc_t dst_desc;
	u32 enl;
	u32 digestlen = crypto_ahash_digestsize(tfm);
	int ret = 0;
	rtl_crypto_cl_t CL;
	u32 enc_last_data_size;
	u32 apl;
	dma_addr_t dma_handle_dig = NULL;
	dma_addr_t dma_handle_msg = NULL;
	dma_addr_t dma_handle_cl = NULL, dma_handle_pad = NULL;

	// Use only one scatter
	enl = msglen;

	realtek_hash_clear_all(hdev);

	//   Set relative length data
	/* ** Calculate message auto padding length ** */
	enc_last_data_size = enl % 16;
	apl = (16 - enc_last_data_size) % 16;

	/********************************************
	 * step 1: Setup desitination descriptor
	 * Auth descriptor (1):
	 	-----------------
		|digest			|
		-----------------
	 ********************************************/
	dst_desc.w = 0;
	dst_desc.auth.ws = 1;
	dst_desc.auth.fs = 1;
	dst_desc.auth.ls = 1;
	dst_desc.auth.adl = digestlen;
	dma_handle_dig = dma_map_single(hdev->dev, pDigest, digestlen, DMA_FROM_DEVICE);
	if (dma_mapping_error(hdev->dev, dma_handle_dig)) {
		return -ENOMEM;
	}
	realtek_hash_set_dst_desc(hdev, dst_desc.w, dma_handle_dig);

	/********************************************
	 * step 2: Setup source descriptor
	  * step 2-1: prepare Cmd & Pad array:
	  ********************************************/
	srcdesc.w = 0;

	// FS=1, CL=3
	srcdesc.b.rs = 1;
	srcdesc.b.fs = 1;
	srcdesc.b.cl = 3;
	// auto padding
	srcdesc.b.ap = 0x01;

	//Set command buffer array
	memset(&CL, 0, sizeof(CL));

	CL.engine_mode = 1; //hash only
	// Sequential hash setting
	CL.hmac_seq_hash = 1;
	CL.hmac_seq_hash_first = 1;

	//Swap settting
	switch (digestlen) {
	case MD5_DIGEST_SIZE:
		CL.habs = 1;
		CL.hibs = 1;
		CL.hobs = 1;
		CL.hkbs = 1;
		break;
	case SHA1_DIGEST_SIZE:
		CL.hmac_mode = 1;
		break;
	case SHA224_DIGEST_SIZE:
		CL.hmac_mode = 0x2;
		break;
	case SHA256_DIGEST_SIZE:
		CL.hmac_mode = 0x3;
		break;
	case SHA384_DIGEST_SIZE:
		CL.hmac_mode = 0x4;
		break;
	case SHA512_DIGEST_SIZE:
		CL.hmac_mode = 0x5;
		break;
	default:
		ret = -EINVAL;
		goto fail;
	}

	// always using auto padding
	CL.enl = (enl + 15) / 16 ;
	CL.enc_last_data_size = enc_last_data_size;
	CL.ap0 = enl * 8;

	CL.hmac_seq_hash_no_wb = 0;
	CL.hmac_seq_hash_last = 1;
	CL.icv_total_length = 0x40; // for mix mode, but need to set a value 0x40
	realtek_hash_mem_dump(hdev->dev, (const u8 *)(&CL), sizeof(CL), "Command Setting: ");
	dma_handle_cl = dma_map_single(hdev->dev, &CL, sizeof(CL), DMA_TO_DEVICE);
	if (dma_mapping_error(hdev->dev, dma_handle_cl)) {
		ret = -ENOMEM;
		goto fail;
	}
	realtek_hash_set_src_desc(hdev, srcdesc.w, dma_handle_cl);

	/********************************************
	 * step 2-2: prepare Data
	 ********************************************/
	srcdesc.w = 0;
	srcdesc.d.rs = 1;

	dma_handle_msg = dma_map_single(hdev->dev, message, msglen, DMA_TO_DEVICE);
	if (dma_mapping_error(hdev->dev, dma_handle_msg)) {
		ret = -ENOMEM;
		goto fail;
	}

	// Data body
	if (apl == 0) {
		srcdesc.d.ls = 1;
	}
	srcdesc.d.enl = enl;
	realtek_hash_mem_dump(hdev->dev, (const u8 *)message, srcdesc.d.enl, "message: ");
	realtek_hash_set_src_desc(hdev, srcdesc.w, dma_handle_msg);

	// data padding, regard as enl
	if (apl != 0) {
		srcdesc.w = 0;
		srcdesc.d.rs = 1;
		srcdesc.d.enl = apl;
		srcdesc.d.ls = 1;
		dma_handle_pad = dma_map_single(hdev->dev, ipsec_padding, sizeof(ipsec_padding), DMA_TO_DEVICE);
		if (dma_mapping_error(hdev->dev, dma_handle_pad)) {
			ret = -ENOMEM;
			goto fail;
		}
		realtek_hash_set_src_desc(hdev, srcdesc.w, dma_handle_pad);
	}

	/********************************************
	 * step 3: Wait until ipsec engine stop
	 *Polling mode, intr_mode = 0
	 ********************************************/
	if (realtek_hash_wait_ok(hdev)) {
		dev_err(hdev->dev, "Hash process timeout\n");
		ret = -ETIMEDOUT;
	}

fail:
	if (dma_handle_cl) {
		dma_unmap_single(hdev->dev, dma_handle_cl, sizeof(CL), DMA_TO_DEVICE);
	}

	if (dma_handle_msg) {
		dma_unmap_single(hdev->dev, dma_handle_msg, msglen, DMA_TO_DEVICE);
	}

	if (dma_handle_pad) {
		dma_unmap_single(hdev->dev, dma_handle_pad, sizeof(ipsec_padding), DMA_TO_DEVICE);
	}

	if (dma_handle_dig) {
		dma_unmap_single(hdev->dev, dma_handle_dig, digestlen, DMA_FROM_DEVICE);
	}

	realtek_hash_mem_dump(hdev->dev, (const u8 *)pDigest, digestlen, "digest");

	return ret;
}

static int realtek_hash_setkey(struct crypto_ahash *tfm, const u8 *key, unsigned int keylen)
{
	struct realtek_hash_ctx *ctx = crypto_ahash_ctx(tfm);
	int i;
	int ret = 0;
	u32 digestlen = crypto_ahash_digestsize(tfm);
	unsigned int padsize = crypto_ahash_blocksize(tfm); //padding size = block size, sha384/512=128, others=64
	u8 hash_digest_result[SHA512_DIGEST_SIZE];
	struct realtek_hash_dev *hdev = NULL;

	if (mutex_lock_interruptible(&realtek_hash.drv_mutex)) {
		return -ERESTARTSYS;
	}

	hdev = realtek_hash_find_dev(ctx);
	if (!hdev) {
		mutex_unlock(&realtek_hash.drv_mutex);
		dev_err(realtek_hash.dev, "Failed to find hash dev for requested tfm %p\n", tfm);
		return -ENODEV;
	}
	mutex_unlock(&realtek_hash.drv_mutex);

	if (mutex_lock_interruptible(&hdev->hdev_mutex)) {
		return -ERESTARTSYS;
	}

	if (keylen <= HASH_HMAC_MAX_KLEN) {
		ctx->ipad = (u8 *)(&(ctx->g_IOPAD[0]));
		ctx->opad = (u8 *)(&(ctx->g_IOPAD[padsize]));
		memset(ctx->ipad, 0x36, padsize);
		memset(ctx->opad, 0x5c, padsize);
		if (keylen > padsize) {
			ret = realtek_hash_key_process(hdev, tfm, (u8 *)key, keylen, hash_digest_result);
			if (ret != 0) {
				mutex_unlock(&hdev->hdev_mutex);
				return ret;
			}
			keylen = digestlen;
			for (i = 0; i < keylen; i++) {
				ctx->ipad[i] ^= hash_digest_result[i];
				ctx->opad[i] ^= hash_digest_result[i];
			}
		} else {
			for (i = 0; i < keylen; i++) {
				if (i >= IOPAD_LEN) {
					dev_err(realtek_hash.dev, "Invalid padsize.");
					break;
				}
				ctx->ipad[i] ^= ((u8 *) key)[i];
				ctx->opad[i] ^= ((u8 *) key)[i];
			}
		}
	} else {
		mutex_unlock(&hdev->hdev_mutex);
		return -ENOMEM;
	}

	mutex_unlock(&hdev->hdev_mutex);
	return ret;
}

static int realtek_hash_cra_init_algs(struct crypto_tfm *tfm,
									  const char *algs_hmac_name)
{
	struct realtek_hash_ctx *ctx = crypto_tfm_ctx(tfm);
	struct realtek_hash_dev *hdev;

	/* request to allocate a new hash device instance from crypto subsystem */
	if (mutex_lock_interruptible(&realtek_hash.drv_mutex)) {
		return -ERESTARTSYS;
	}

	hdev = realtek_hash_find_dev(ctx);
	if (!hdev) {
		/* Allocate a new hash device for new tfm */
		hdev = devm_kzalloc(realtek_hash.dev, sizeof(*hdev), GFP_KERNEL);
		if (!hdev) {
			mutex_unlock(&realtek_hash.drv_mutex);
			return -ENOMEM;
		}
		mutex_init(&hdev->hdev_mutex);

		/* add this hdev in the global driver list */
		list_add_tail(&hdev->list, &realtek_hash.dev_list);
	}

	memset(ctx, 0, sizeof(struct realtek_hash_ctx));

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
							 sizeof(struct realtek_hash_request_ctx));

	if (algs_hmac_name) {
		ctx->flags |= HASH_FLAGS_HMAC;
	}

	hdev->tfm = tfm;
	hdev->ctx = ctx;
	hdev->dev = realtek_hash.dev;
	hdev->io_base = realtek_hash.io_base;
	mutex_unlock(&realtek_hash.drv_mutex);

	return 0;
}

static int realtek_hash_cra_init(struct crypto_tfm *tfm)
{
	return realtek_hash_cra_init_algs(tfm, NULL);
}

static int realtek_hash_cra_md5_init(struct crypto_tfm *tfm)
{
	return realtek_hash_cra_init_algs(tfm, "md5");
}

static int realtek_hash_cra_sha1_init(struct crypto_tfm *tfm)
{
	return realtek_hash_cra_init_algs(tfm, "sha1");
}

static int realtek_hash_cra_sha224_init(struct crypto_tfm *tfm)
{
	return realtek_hash_cra_init_algs(tfm, "sha224");
}

static int realtek_hash_cra_sha256_init(struct crypto_tfm *tfm)
{
	return realtek_hash_cra_init_algs(tfm, "sha256");
}

static int realtek_hash_cra_sha384_init(struct crypto_tfm *tfm)
{
	return realtek_hash_cra_init_algs(tfm, "sha384");
}

static int realtek_hash_cra_sha512_init(struct crypto_tfm *tfm)
{
	return realtek_hash_cra_init_algs(tfm, "sha512");
}

#define HASH_ALG(size, name, bsize)	\
{ \
	.init = realtek_hash_init, \
	.update = realtek_hash_update, \
	.final = realtek_hash_final, \
	.finup = realtek_hash_finup, \
	.digest = realtek_hash_digest, \
	.export = realtek_hash_export, \
	.import = realtek_hash_import, \
	.halg = { \
		.digestsize = size, \
		.statesize = sizeof(struct realtek_hash_request_ctx), \
		.base = { \
			.cra_name = #name, \
			.cra_driver_name = "realtek-"#name, \
			.cra_priority = 200, \
			.cra_flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_KERN_DRIVER_ONLY, \
			.cra_blocksize = bsize, \
			.cra_ctxsize = sizeof(struct realtek_hash_ctx), \
			.cra_alignmask = 3, \
			.cra_init = realtek_hash_cra_init, \
			.cra_module = THIS_MODULE, \
		} \
	} \
}

#define HASH_ALG_HMAC(size, name, bsize)	\
{ \
	.init = realtek_hash_init, \
	.update = realtek_hash_update, \
	.final = realtek_hash_final, \
	.finup = realtek_hash_finup, \
	.digest = realtek_hash_digest, \
	.export = realtek_hash_export, \
	.import = realtek_hash_import, \
	.setkey = realtek_hash_setkey, \
	.halg = { \
		.digestsize = size, \
		.statesize = sizeof(struct realtek_hash_request_ctx), \
		.base = { \
			.cra_name = "hmac("#name")", \
			.cra_driver_name = "realtek-hmac-"#name, \
			.cra_priority = 200, \
			.cra_flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_KERN_DRIVER_ONLY, \
			.cra_blocksize = bsize, \
			.cra_ctxsize = sizeof(struct realtek_hash_ctx), \
			.cra_alignmask = 3, \
			.cra_init = realtek_hash_cra_##name##_init, \
			.cra_module = THIS_MODULE, \
		} \
	} \
}

/**
  * @brief  SHA engine reset.
  * @param  None
  * @retval None
  */
static void realtek_hash_reset(void)
{
	u32 reg_value;

	// Crypto engine : Software Reset
	writel(IPSEC_BIT_SOFT_RST,  realtek_hash.io_base + RTK_RSTEACONFISR);
	// Crypto Engine : Set swap
	reg_value = IPSEC_DMA_BURST_LENGTH(16);
	reg_value |= (IPSEC_BIT_KEY_IV_SWAP | IPSEC_BIT_KEY_PAD_SWAP);
	reg_value |= (IPSEC_BIT_DATA_IN_LITTLE_ENDIAN | IPSEC_BIT_DATA_OUT_LITTLE_ENDIAN | IPSEC_BIT_MAC_OUT_LITTLE_ENDIAN);
	writel(reg_value, realtek_hash.io_base + RTK_SWAPABURSTR);
	// Crypto Engine : DMA arbiter(detect fifo wasted level) , clock enable
	writel((IPSEC_BIT_ARBITER_MODE | IPSEC_BIT_ENGINE_CLK_EN), realtek_hash.io_base + RTK_DBGR);

	// OTP key disable
	writel(0, realtek_hash.io_base + RTK_IPSEKCR);
}

static int realtek_hash_register_algs(void)
{
	unsigned int i;
	int err;

	for (i = 0; i < realtek_hash.pdata->algs_list_size; i++) {
		err = crypto_register_ahash(&realtek_hash.pdata->algs_list[i]);
		if (err) {
			goto err_algs;
		}
	}

	return 0;

err_algs:
	dev_err(realtek_hash.dev, "Algo %d failed\n", i);
	for (; i--;) {
		crypto_unregister_ahash(&realtek_hash.pdata->algs_list[i]);
	}

	return err;
}

static int realtek_hash_unregister_algs(void)
{
	unsigned int i;

	for (i = 0; i < realtek_hash.pdata->algs_list_size; i++) {
		crypto_unregister_ahash(&realtek_hash.pdata->algs_list[i]);
	}

	return 0;
}

static int realtek_hash_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;

	mutex_init(&realtek_hash.drv_mutex);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	realtek_hash.io_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(realtek_hash.io_base)) {
		return PTR_ERR(realtek_hash.io_base);
	}

	realtek_hash.pdata = of_device_get_match_data(dev);
	if (!realtek_hash.pdata) {
		dev_err(dev, "No compatible OF match\n");
		return -EINVAL;
	}

	realtek_hash.dev = dev;
	realtek_hash_reset();

	/* Register algos */
	if (realtek_hash_register_algs()) {
		return -EIO;
	}

	return 0;
}

static int realtek_hash_remove(struct platform_device *pdev)
{
	struct realtek_hash_dev *hdev = NULL;

	realtek_hash_unregister_algs();
	list_for_each_entry(hdev, &realtek_hash.dev_list, list)
	list_del(&hdev->list);

	return 0;
}

static struct ahash_alg realtek_hash_algs[] = {
	HASH_ALG(MD5_DIGEST_SIZE, md5, MD5_HMAC_BLOCK_SIZE),
	HASH_ALG_HMAC(MD5_DIGEST_SIZE, md5, MD5_HMAC_BLOCK_SIZE),
	HASH_ALG(SHA1_DIGEST_SIZE, sha1, SHA1_BLOCK_SIZE),
	HASH_ALG_HMAC(SHA1_DIGEST_SIZE, sha1, SHA1_BLOCK_SIZE),
	HASH_ALG(SHA224_DIGEST_SIZE, sha224, SHA224_BLOCK_SIZE),
	HASH_ALG_HMAC(SHA224_DIGEST_SIZE, sha224, SHA224_BLOCK_SIZE),
	HASH_ALG(SHA256_DIGEST_SIZE, sha256, SHA256_BLOCK_SIZE),
	HASH_ALG_HMAC(SHA256_DIGEST_SIZE, sha256, SHA256_BLOCK_SIZE),
	HASH_ALG(SHA384_DIGEST_SIZE, sha384, SHA384_BLOCK_SIZE),
	HASH_ALG_HMAC(SHA384_DIGEST_SIZE, sha384, SHA384_BLOCK_SIZE),
	HASH_ALG(SHA512_DIGEST_SIZE, sha512, SHA512_BLOCK_SIZE),
	HASH_ALG_HMAC(SHA512_DIGEST_SIZE, sha512, SHA512_BLOCK_SIZE),
};

static const struct realtek_hash_pdata realtek_hash_match_data = {
	.algs_list	 = realtek_hash_algs,
	.algs_list_size	= ARRAY_SIZE(realtek_hash_algs),
};

static const struct of_device_id realtek_hash_of_match[] = {
	{
		.compatible = "realtek,ameba-hash",
		.data = &realtek_hash_match_data,
	},
	{ }
};

MODULE_DEVICE_TABLE(of, realtek_hash_of_match);

static struct platform_driver realtek_hash_driver = {
	.probe		= realtek_hash_probe,
	.remove		= realtek_hash_remove,
	.driver		= {
		.name	= "realtek-ameba-hash",
		.of_match_table	= realtek_hash_of_match,
	}
};

builtin_platform_driver(realtek_hash_driver);

MODULE_DESCRIPTION("Realtek Ameba Hash driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
