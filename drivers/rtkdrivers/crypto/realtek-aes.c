// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek AES support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/clk.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <crypto/aes.h>
#include <crypto/internal/aead.h>
#include <crypto/scatterwalk.h>

#include "realtek-crypto.h"
#include "realtek-aes.h"


__aligned(32) static u8 ipsec_padding[64]  = { 0x0 };
static const u8 gcm_iv_tail[] = {0x00, 0x00, 0x00, 0x01};

static struct realtek_crypto_drv realtek_aes = {
	.dev_list = LIST_HEAD_INIT(realtek_aes.dev_list),
};

static int realtek_aes_wait_ok(struct realtek_aes_dev *adev)
{
	u32 status;
	u32 ips_err;
	int ret;

	ret = readl_relaxed_poll_timeout(adev->io_base + RTK_RSTEACONFISR, status, (status & IPSEC_BIT_CMD_OK), 10, 1000000);
	if (ret) {
		return ret;
	}

	ips_err = readl(adev->io_base + RTK_ERRSR);
	if (ips_err) {
		dev_err(adev->dev, "RTK_ERRSR = 0x%08X\n", ips_err);
		ret = -ETIMEDOUT;
	}

	return ret;
}

void realtek_aes_mem_dump(struct device *dev, const u8 *start, u32 size, char *strHeader)
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
		dev_dbg(dev, "Dump buf[%d] = 0x%02X\n", index, buf[index]);
	}
}

/**
  * @brief  Set source descriptor.
  * @param  adev: aes driver data
  * @param  sd1: Source descriptor first word.
  * @param  sdpr: Source descriptor second word.
  * @retval None
  */
static void realtek_aes_set_src_desc(struct realtek_aes_dev *adev, u32 sd1, dma_addr_t sdpr)
{
	u32 timeout;
	u32 tmp_value;

	timeout = FIFOCNT_TIMEOUT;
	while (1) {
		tmp_value = readl(adev->io_base + RTK_SDSR);
		if ((tmp_value & IPSEC_MASK_FIFO_EMPTY_CNT) > 0) {
			dev_dbg(adev->dev, "Set sd1 = 0x%08X, sdpr = 0x%08X\n", sd1, sdpr);
			writel(sd1, adev->io_base + RTK_SDFWR);
			writel(sdpr, adev->io_base + RTK_SDSWR);
			break;
		}
		timeout--;
		if (timeout == 0) {
			dev_err(adev->dev, "Timeout, src fifo is full\n");
			break;
		}
	}
}

/**
  * @brief  Set destination descriptor.
  * @param  adev: aes driver data
  * @param  dd1: Destination descriptor first word.
  * @param  ddpr: Destination descriptor second word.
  * @retval None
  */
static void realtek_aes_set_dst_desc(struct realtek_aes_dev *adev, u32 dd1, dma_addr_t ddpr)
{
	u32 tmp_value;

	tmp_value = readl(adev->io_base + RTK_DDSR);
	if ((tmp_value & IPSEC_MASK_FIFO_EMPTY_CNT) > 0) {
		dev_dbg(adev->dev, "Set dd1 = 0x%08X, ddpr = 0x%08X\n", dd1, ddpr);
		writel(dd1, adev->io_base + RTK_DDFWR);
		writel(ddpr, adev->io_base + RTK_DDSWR);
	} else {
		dev_err(adev->dev, "Destination fifo_cnt %d is not correct\n", IPSEC_GET_FIFO_EMPTY_CNT(tmp_value));
	}
}

/**
  * @brief  Clear command ok and error interrupts.
  * @param  adev: aes driver data
  * @retval None
  */
static void realtek_aes_clear_all(struct realtek_aes_dev *adev)
{
	u32 ok_intr_cnt = 0;
	u32 tmp_value;

	writel(0xFFFF, adev->io_base + RTK_ERRSR);
	tmp_value = readl(adev->io_base + RTK_RSTEACONFISR);
	ok_intr_cnt = IPSEC_GET_OK_INTR_CNT(tmp_value);
	tmp_value |= (IPSEC_CLEAR_OK_INT_NUM(ok_intr_cnt) | IPSEC_BIT_CMD_OK);
	writel(tmp_value, adev->io_base + RTK_RSTEACONFISR);
}

/**
  * @brief  Set source descriptor command buffer.
  * @param  ctx: Point to aes context.
  * @retval None
  */
static void realtek_aes_set_cmd_buf(struct realtek_aes_ctx *ctx)
{
	rtl_crypto_cl_t *pCL;
	u32 a2eo;
	u32 enl;

	a2eo = ctx->a2eo;
	enl = ctx->enl;

	pCL = (rtl_crypto_cl_t *)ctx->cl_buffer;

	memset(pCL, 0, sizeof(ctx->cl_buffer));

	//cipher mode: 0x0 : ECB, 0x1: CBC, 0x2: CFB , 0x3 : OFB , 0x4 : CTR, 0x5 : GCTR, 0x6: GMAC, 0x7: GHASH, 0x8: GCM
	pCL->cipher_mode = ctx->cipher_type;
	//AES key length
	pCL->cipher_eng_sel = 0;
	switch (ctx->keylen) {
	case 128/8 :
		pCL->aes_key_sel = 0;
		break;
	case 192/8 :
		pCL->aes_key_sel = 1;
		break;
	case 256/8 :
		pCL->aes_key_sel = 2;
		break;
	default:
		break;
	}

	pCL->enl = (enl + 15) / 16;
	pCL->enc_last_data_size = ctx->enc_last_data_size;

	if (ctx->cipher_type ==  AES_TYPE_GCM) {
		pCL->header_total_len = (a2eo + 15) / 16;
		pCL->aad_last_data_size = ctx->aad_last_data_size;
	}

	if (ctx->isDecrypt == 0) {
		pCL->cipher_encrypt = 1;
	}

	pCL->icv_total_length = 0x40; // for mix mode, but need to set a value 0x40
}

/**
  * @brief  Set source descriptor command, key, iv array.
  * @param  ctx: Point to aes context.
  * @retval None
  */
static int realtek_aes_cmd_key_iv(struct realtek_aes_ctx *ctx)
{
	rtl_crypto_srcdesc_t src_desc;

	src_desc.w = 0;

	// FS=1, CL=3
	src_desc.b.rs = 1;
	src_desc.b.fs = 1;
	src_desc.b.cl = 3;
	src_desc.b.key_len = ctx->keylen / 4;

	//Set command buffer array
	realtek_aes_set_cmd_buf(ctx);
	memcpy(ctx->cl_key_iv, ctx->cl_buffer, 12);
	realtek_aes_mem_dump(ctx->adev->dev, (const u8 *)(&(ctx->cl_key_iv)[0]), 12, "Command Setting: ");
	//Set Key array
	memcpy((void *)&ctx->cl_key_iv[12], ctx->key, ctx->keylen);
	realtek_aes_mem_dump(ctx->adev->dev, (const u8 *)(&(ctx->cl_key_iv)[12]), ctx->keylen, "Key: ");
	// Set IV array
	if (ctx->ivlen > 0) {
		src_desc.b.iv_len = ctx->ivlen / 4;
		memcpy((void *)&ctx->cl_key_iv[12 + ctx->keylen], ctx->iv, ctx->ivlen);
		realtek_aes_mem_dump(ctx->adev->dev, (const u8 *)(&(ctx->cl_key_iv)[12 + ctx->keylen]), ctx->keylen, "IV: ");
	}

	ctx->dma_handle_cl = dma_map_single(ctx->adev->dev, &(ctx->cl_key_iv[0]), sizeof(ctx->cl_key_iv), DMA_TO_DEVICE);
	if (dma_mapping_error(ctx->adev->dev, ctx->dma_handle_cl)) {
		return -ENOMEM;
	}
	realtek_aes_set_src_desc(ctx->adev, src_desc.w, ctx->dma_handle_cl);

	return 0;
}

/**
  * @brief  Copy sg content to buf.
  * @param  buf: Point to aes ablkcipher request.
  * @param  areq: Point to aes aead request.
  * @retval Status.
  */
static int realtek_aes_prepare_buf(struct ablkcipher_request *req, struct aead_request *areq)
{
	struct scatter_walk walk;
	struct realtek_aes_ctx *ctx;
	struct scatterlist *in_sg;
	unsigned int tag_len;

	if (!req && !areq) {
		return -EINVAL;
	}

	if (req) {
		ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
		ctx->buf_in_len = req->nbytes;
		ctx->buf_out_len = req->nbytes;
		in_sg = req->src;
	} else {
		ctx = crypto_aead_ctx(crypto_aead_reqtfm(areq));
		tag_len = crypto_aead_authsize((crypto_aead_reqtfm(areq)));
		ctx->buf_in_len = areq->assoclen + areq->cryptlen;
		ctx->buf_out_len = ctx->isDecrypt ? (areq->assoclen + areq->cryptlen - tag_len) : (areq->assoclen + areq->cryptlen + tag_len);
		in_sg = areq->src;
	}

	if (!ctx->buf_in_len || !ctx->buf_out_len) {
		return -EBUSY;
	}

	ctx->buf_in = (u8 *)dma_alloc_coherent(ctx->adev->dev, ctx->buf_in_len, &ctx->dma_handle_msg, GFP_KERNEL);
	if (!ctx->buf_in) {
		dev_err(ctx->adev->dev, "Can't allocate in buffer\n");
		return -EFAULT;
	}

	ctx->buf_out = (u8 *)dma_alloc_coherent(ctx->adev->dev, ctx->buf_out_len, &ctx->dma_handle_dst, GFP_KERNEL);
	if (!ctx->buf_out) {
		dev_err(ctx->adev->dev, "Can't allocate out buffer\n");
		dma_free_coherent(ctx->adev->dev, ctx->buf_in_len, ctx->buf_in, ctx->dma_handle_msg);
		return -EFAULT;
	}

	scatterwalk_start(&walk, in_sg);
	scatterwalk_advance(&walk, 0);
	scatterwalk_copychunks(ctx->buf_in, &walk, ctx->buf_in_len, 0);
	scatterwalk_done(&walk, 0, 0);

	return 0;
}

/**
  * @brief  Copy result to dst sg content.
  * @param  buf: Point to aes ablkcipher request.
  * @param  areq: Point to aes aead request.
  * @retval Status.
  */
static int realtek_aes_final_result(struct ablkcipher_request *req, struct aead_request *areq)
{
	struct scatter_walk walk;
	struct realtek_aes_ctx *ctx;
	struct scatterlist *out_sg;

	if (!req && !areq) {
		return -EINVAL;
	}

	if (req) {
		ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
		out_sg = req->dst;
	} else {
		ctx = crypto_aead_ctx(crypto_aead_reqtfm(areq));
		out_sg = areq->dst;
	}

	scatterwalk_start(&walk, out_sg);
	scatterwalk_advance(&walk, 0);
	scatterwalk_copychunks(ctx->buf_out, &walk, ctx->buf_out_len, 1);
	scatterwalk_done(&walk, 1, 0);

	return 0;
}

/**
  * @brief  Crypto engine aes process.
  * @param  req: Point to aes ablkcipher request.
  * @param  areq: Point to aes aead request.
  * @retval Result of process.
  */
static int realtek_aes_process(struct ablkcipher_request *req, struct aead_request *areq)
{
	rtl_crypto_srcdesc_t srcdesc;
	rtl_crypto_dstdesc_t dst_cipher_desc;
	rtl_crypto_dstdesc_t  dst_auth_desc;
	u32 enl;
	u32 a2eo;
	unsigned int tag_len;
	int ret = 0;
	struct realtek_aes_ctx *ctx;
	dma_addr_t dma_handle_msg, dma_handle_dst;
	u8 *buf_in, *buf_out;

	if (!req && !areq) {
		return -EINVAL;
	}

	if (req) {
		ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
		tag_len = 0;
		a2eo = 0;

		enl = req->nbytes;
	} else {
		ctx = crypto_aead_ctx(crypto_aead_reqtfm(areq));
		tag_len = crypto_aead_authsize((crypto_aead_reqtfm(areq)));
		a2eo = areq->assoclen;
		if (a2eo > 1008) {
			dev_err(ctx->adev->dev, "AES AAD length error, aad_len <= (63 * 16) bytes\n");
			return -EINVAL;
		}
		enl = areq->cryptlen;
	}

	if (!enl) {
		dev_err(ctx->adev->dev, "AES message length error, msg_len is 0\n");
		return -EINVAL;
	}

	ret = realtek_aes_prepare_buf(req, areq);
	if (ret) {
		if (ret == -EBUSY) {
			ret = 0;
		}
		return ret;
	}
	dma_handle_msg = ctx->dma_handle_msg;
	dma_handle_dst = ctx->dma_handle_dst;
	buf_in = ctx->buf_in;
	buf_out = ctx->buf_out;

	realtek_aes_clear_all(ctx->adev);

	//   Set relative length data
	/* ** Calculate message auto padding length ** */
	ctx->enl = enl;
	ctx->enc_last_data_size = enl % 16;
	ctx->apl = (16 - ctx->enc_last_data_size) % 16;

	ctx->a2eo = a2eo;
	ctx->aad_last_data_size = a2eo % 16;
	ctx->apl_aad = (16 - ctx->aad_last_data_size) % 16;

	/********************************************
	 * step 1: Setup desitination descriptor
	 * Auth descriptor (1):
	 	-----------------
		|digest			|
		-----------------
	 ********************************************/
	dma_handle_dst += ctx->a2eo;
	if (ctx->enl != 0) {
		dst_cipher_desc.w = 0;
		dst_cipher_desc.cipher.ws = 1;
		dst_cipher_desc.cipher.fs = 1;
		dst_cipher_desc.cipher.enc = 1;
		dst_cipher_desc.cipher.enl = ctx->enl;
		if (tag_len == 0) {
			dst_cipher_desc.cipher.ls = 1;
		}
		realtek_aes_set_dst_desc(ctx->adev, dst_cipher_desc.w, dma_handle_dst);// Data
	}

	if (tag_len != 0) {	// Tag
		dst_auth_desc.w = 0;
		dst_auth_desc.auth.ws = 1;
		dst_auth_desc.auth.enc = 0;
		dst_auth_desc.auth.adl = tag_len;
		dst_auth_desc.auth.ls = 1;
		if (ctx->enl == 0) {
			dst_auth_desc.auth.fs = 1;
		} else {
			dma_handle_dst += ctx->enl;
		}
		realtek_aes_set_dst_desc(ctx->adev, dst_auth_desc.w, dma_handle_dst);
	}

	/********************************************
	 * step 2: Setup source descriptor
			 ----------
			|Cmd buffer |
			----------
			|Key array  |
			----------
			|IV array  |
			----------
			|AAD array  |
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
	  * step 2-1: prepare Cmd & Key & IV array:
	  ********************************************/
	if (realtek_aes_cmd_key_iv(ctx)) {
		ret = -ENOMEM;
		goto fail;
	}

	/********************************************
	 * step 2-2: prepare Data1 ~ DataN
	 ********************************************/
	srcdesc.w = 0;
	srcdesc.d.rs = 1;

	if ((ctx->apl_aad > 0) || (ctx->apl > 0)) {
		ctx->dma_handle_pad = dma_map_single(ctx->adev->dev, ipsec_padding, sizeof(ipsec_padding), DMA_TO_DEVICE);
		if (dma_mapping_error(ctx->adev->dev, ctx->dma_handle_pad)) {
			ret = -ENOMEM;
			goto fail;
		}
	}

	if (a2eo > 0) {
		while (a2eo > 16) {	//AAD, 16-byte per src desc
			srcdesc.d.a2eo = 16;

			realtek_aes_mem_dump(ctx->adev->dev, buf_in, 16, "AAD: ");
			realtek_aes_set_src_desc(ctx->adev, srcdesc.w, dma_handle_msg);

			buf_in += 16;
			dma_handle_msg += 16;
			a2eo -= 16;
			srcdesc.w = 0;
			srcdesc.d.rs = 1;
		}

		if (a2eo > 0) {	//AAD, last block
			srcdesc.d.a2eo = a2eo;
			realtek_aes_mem_dump(ctx->adev->dev, buf_in, a2eo, "AAD: ");
			realtek_aes_set_src_desc(ctx->adev, srcdesc.w, dma_handle_msg);
			srcdesc.w = 0;
			srcdesc.d.rs = 1;
			buf_in += a2eo;
			dma_handle_msg += a2eo;
		}

		if (ctx->apl_aad > 0) {	//AAD padding, last block
			srcdesc.d.a2eo = ctx->apl_aad;
			realtek_aes_set_src_desc(ctx->adev, srcdesc.w, ctx->dma_handle_pad);
			srcdesc.w = 0;
			srcdesc.d.rs = 1;
		}
	}


	while (enl > 16368) {	// Data body 16368-byte per src desc
		srcdesc.d.enl = 16368;
		realtek_aes_set_src_desc(ctx->adev, srcdesc.w, dma_handle_msg);

		buf_in += 16368;
		dma_handle_msg += 16368;
		enl -= 16368;
		srcdesc.w = 0;
		srcdesc.d.rs = 1;
	}

	// Data body msglen < 16368
	if (ctx->apl == 0) {
		srcdesc.d.ls = 1;
	}
	srcdesc.d.enl = enl;
	realtek_aes_mem_dump(ctx->adev->dev, (const u8 *)buf_in, srcdesc.d.enl, "message: ");
	realtek_aes_set_src_desc(ctx->adev, srcdesc.w, dma_handle_msg);

	// data padding, regard as enl
	if (ctx->apl > 0) {
		srcdesc.w = 0;
		srcdesc.d.rs = 1;
		srcdesc.d.enl = ctx->apl;
		srcdesc.d.ls = 1;
		realtek_aes_set_src_desc(ctx->adev, srcdesc.w, ctx->dma_handle_pad);
	}

	/********************************************
	 * step 3: Wait until ipsec engine stop
	 *Polling mode, intr_mode = 0
	 ********************************************/
	if (realtek_aes_wait_ok(ctx->adev)) {
		dev_err(ctx->adev->dev, "AES process timeout\n");
		ret = -ETIMEDOUT;
	}

fail:
	if (ctx->dma_handle_cl) {
		dma_unmap_single(ctx->adev->dev, ctx->dma_handle_cl, sizeof(ctx->cl_key_iv), DMA_TO_DEVICE);
	}

	if (ctx->dma_handle_pad) {
		dma_unmap_single(ctx->adev->dev, ctx->dma_handle_pad, sizeof(ipsec_padding), DMA_TO_DEVICE);
	}

	if (!ret) {
		realtek_aes_mem_dump(ctx->adev->dev, (const u8 *)(buf_out + ctx->a2eo), ctx->enl, "result");
		realtek_aes_mem_dump(ctx->adev->dev, (const u8 *)(buf_out + ctx->a2eo + ctx->enl), tag_len, "tag");
		realtek_aes_final_result(req, areq);
	}

	if (ctx->buf_in) {
		dma_free_coherent(ctx->adev->dev, ctx->buf_in_len, ctx->buf_in, ctx->dma_handle_msg);
	}

	if (ctx->buf_out) {
		dma_free_coherent(ctx->adev->dev, ctx->buf_out_len, ctx->buf_out, ctx->dma_handle_dst);
	}

	return ret;
}

static struct realtek_aes_dev *realtek_aes_find_dev(struct realtek_aes_ctx *ctx)
{
	struct realtek_aes_dev *adev = NULL, *tmp = NULL;

	list_for_each_entry(tmp, &realtek_aes.dev_list, list) {
		if (tmp->ctx == ctx) {
			adev = tmp;
			ctx->adev = adev;
			break;
		}
	}

	return adev;
}

static int realtek_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_ECB;
	ctx->isDecrypt = 0;
	ctx->ivlen = 0;
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_ECB;
	ctx->isDecrypt = 1;
	ctx->ivlen = 0;
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_CBC;
	ctx->isDecrypt = 0;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_CBC;
	ctx->isDecrypt = 1;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_CTR;
	ctx->isDecrypt = 0;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_CTR;
	ctx->isDecrypt = 1;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_cfb_encrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_CFB;
	ctx->isDecrypt = 0;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_cfb_decrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_CFB;
	ctx->isDecrypt = 1;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_ofb_encrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_OFB;
	ctx->isDecrypt = 0;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_ofb_decrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_OFB;
	ctx->isDecrypt = 1;
	ctx->ivlen = crypto_ablkcipher_ivsize(tfm);
	memcpy(ctx->iv, req->info, ctx->ivlen);
	ret = realtek_aes_process(req, NULL);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key, unsigned int keylen)
{
	struct realtek_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}


	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 && keylen != AES_KEYSIZE_256) {
		mutex_unlock(&adev->adev_mutex);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	mutex_unlock(&adev->adev_mutex);
	return 0;
}

static int realtek_aes_cra_init(struct crypto_tfm *tfm)
{
	struct realtek_aes_ctx *ctx = crypto_tfm_ctx(tfm);
	struct realtek_aes_dev *adev;

	/* request to allocate a new aes device instance from crypto subsystem */
	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {

		/* Allocate a new aes device for new tfm */
		adev = devm_kzalloc(realtek_aes.dev, sizeof(*adev), GFP_KERNEL);
		if (!adev) {
			mutex_unlock(&realtek_aes.drv_mutex);
			return -ENOMEM;
		}
		mutex_init(&adev->adev_mutex);

		/* add this adev in the global driver list */
		list_add_tail(&adev->list, &realtek_aes.dev_list);
	}

	memset(ctx, 0, sizeof(struct realtek_aes_ctx));

	adev->tfm = tfm;
	adev->ctx = ctx;
	adev->dev = realtek_aes.dev;
	adev->io_base = realtek_aes.io_base;
	mutex_unlock(&realtek_aes.drv_mutex);

	return 0;
}

static int realtek_aes_gcm_setkey(struct crypto_aead *tfm, const u8 *key, unsigned int keylen)
{
	struct realtek_aes_ctx *ctx = crypto_aead_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}
	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 && keylen != AES_KEYSIZE_256) {
		mutex_unlock(&adev->adev_mutex);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;
	mutex_unlock(&adev->adev_mutex);

	return 0;
}

static int realtek_aes_gcm_setauthsize(struct crypto_aead *tfm, unsigned int authsize)
{
	return authsize == AES_BLOCK_SIZE ? 0 : -EINVAL;
}

static int realtek_aes_gcm_encrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_aead_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_GCM;
	ctx->isDecrypt = 0;
	ctx->ivlen = crypto_aead_ivsize(tfm);
	memcpy(ctx->iv, req->iv, ctx->ivlen);
	memcpy((void *)(&ctx->iv[ctx->ivlen]), (const void *)gcm_iv_tail, 4);
	ctx->ivlen += 4;
	ret = realtek_aes_process(NULL, req);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_gcm_decrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct realtek_aes_ctx *ctx = crypto_aead_ctx(tfm);
	struct realtek_aes_dev *adev = NULL;
	int ret;


	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {
		mutex_unlock(&realtek_aes.drv_mutex);
		dev_err(realtek_aes.dev, "Failed to find adev for requested ctx %p\n", ctx);
		return -ENODEV;
	}
	mutex_unlock(&realtek_aes.drv_mutex);

	if (mutex_lock_interruptible(&adev->adev_mutex)) {
		return -ERESTARTSYS;
	}

	ctx->cipher_type = AES_TYPE_GCM;
	ctx->isDecrypt = 1;
	ctx->ivlen = crypto_aead_ivsize(tfm);
	memcpy(ctx->iv, req->iv, ctx->ivlen);
	memcpy((void *)(&ctx->iv[ctx->ivlen]), (const void *)gcm_iv_tail, 4);
	ctx->ivlen += 4;
	ret = realtek_aes_process(NULL, req);

	mutex_unlock(&adev->adev_mutex);
	return ret;
}

static int realtek_aes_gcm_init(struct crypto_aead *tfm)
{
	struct realtek_aes_ctx *ctx = crypto_aead_ctx(tfm);
	struct realtek_aes_dev *adev;

	/* request to allocate a new aes device instance from crypto subsystem */
	if (mutex_lock_interruptible(&realtek_aes.drv_mutex)) {
		return -ERESTARTSYS;
	}

	adev = realtek_aes_find_dev(ctx);
	if (!adev) {

		/* Allocate a new aes device for new tfm */
		adev = devm_kzalloc(realtek_aes.dev, sizeof(*adev), GFP_KERNEL);
		if (!adev) {
			mutex_unlock(&realtek_aes.drv_mutex);
			return -ENOMEM;
		}
		mutex_init(&adev->adev_mutex);

		/* add this adev in the global driver list */
		list_add_tail(&adev->list, &realtek_aes.dev_list);
	}

	memset(ctx, 0, sizeof(struct realtek_aes_ctx));

	adev->ctx = ctx;
	adev->dev = realtek_aes.dev;
	adev->io_base = realtek_aes.io_base;
	mutex_unlock(&realtek_aes.drv_mutex);

	return 0;
}

#define AES_ALG(name, block_size, iv_size)	\
{  \
	.cra_name		= #name"(aes)",  \
	.cra_driver_name	= "realtek-aes-"#name,  \
	.cra_priority		= 200,  \
	.cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,  \
	.cra_blocksize		= block_size,  \
	.cra_ctxsize		= sizeof(struct realtek_aes_ctx),  \
	.cra_alignmask		= 0xf,  \
	.cra_type		= &crypto_ablkcipher_type,  \
	.cra_module		= THIS_MODULE,  \
	.cra_init		= realtek_aes_cra_init,  \
	.cra_ablkcipher = {  \
		.min_keysize	= AES_MIN_KEY_SIZE,  \
		.max_keysize	= AES_MAX_KEY_SIZE,  \
		.ivsize		= iv_size,  \
		.setkey		= realtek_aes_setkey,  \
		.encrypt	= realtek_aes_##name##_encrypt,  \
		.decrypt	= realtek_aes_##name##_decrypt,  \
	}  \
}

static struct crypto_alg realtek_aes_algs[] = {
	AES_ALG(ecb, AES_BLOCK_SIZE, 0),
	AES_ALG(cbc, AES_BLOCK_SIZE, AES_BLOCK_SIZE),
	AES_ALG(ctr, 1, AES_BLOCK_SIZE),
	AES_ALG(ofb, 1, AES_BLOCK_SIZE),
	AES_ALG(cfb, 1, AES_BLOCK_SIZE),
};

static struct aead_alg realtek_gcm_alg = {
	.setkey		= realtek_aes_gcm_setkey,
	.setauthsize	= realtek_aes_gcm_setauthsize,
	.encrypt	= realtek_aes_gcm_encrypt,
	.decrypt	= realtek_aes_gcm_decrypt,
	.init		= realtek_aes_gcm_init,
	.ivsize		= 12,
	.maxauthsize	= AES_BLOCK_SIZE,

	.base = {
		.cra_name		= "gcm(aes)",
		.cra_driver_name	= "realtek-aes-gcm",
		.cra_priority		= 200,
		.cra_flags		= CRYPTO_ALG_ASYNC,
		.cra_blocksize		= 1,
		.cra_ctxsize		= sizeof(struct realtek_aes_ctx),
		.cra_alignmask		= 0xf,
		.cra_module		= THIS_MODULE,
	},
};

/**
  * @brief  SHA engine reset.
  * @param  None
  * @retval None
  */
static void realtek_aes_reset(void)
{
	u32 reg_value;

	// Crypto engine : Software Reset
	writel(IPSEC_BIT_SOFT_RST, realtek_aes.io_base + RTK_RSTEACONFISR);
	// Crypto Engine : Set swap
	reg_value = IPSEC_DMA_BURST_LENGTH(16);
	reg_value |= (IPSEC_BIT_KEY_IV_SWAP | IPSEC_BIT_KEY_PAD_SWAP);
	reg_value |= (IPSEC_BIT_DATA_IN_LITTLE_ENDIAN | IPSEC_BIT_DATA_OUT_LITTLE_ENDIAN | IPSEC_BIT_MAC_OUT_LITTLE_ENDIAN);
	writel(reg_value, realtek_aes.io_base + RTK_SWAPABURSTR);
	// Crypto Engine : DMA arbiter(detect fifo wasted level) , clock enable
	writel((IPSEC_BIT_ARBITER_MODE | IPSEC_BIT_ENGINE_CLK_EN), realtek_aes.io_base + RTK_DBGR);

	// OTP key disable
	writel(0, realtek_aes.io_base + RTK_IPSEKCR);
}

static int realtek_aes_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	mutex_init(&realtek_aes.drv_mutex);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	realtek_aes.io_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(realtek_aes.io_base)) {
		return PTR_ERR(realtek_aes.io_base);
	}

	realtek_aes.dev = dev;
	realtek_aes_reset();

	/* Register algos */
	ret = crypto_register_algs(realtek_aes_algs, ARRAY_SIZE(realtek_aes_algs));
	if (ret) {
		dev_err(dev, "Could not register aes algs\n");
		return ret;
	}

	ret = crypto_register_aead(&realtek_gcm_alg);
	if (ret) {
		dev_err(dev, "Could not register aead(gcm) algs\n");
		goto err_aead_algs;
	}

	return 0;

err_aead_algs:
	crypto_unregister_algs(realtek_aes_algs, ARRAY_SIZE(realtek_aes_algs));
	return ret;
}

static int realtek_aes_remove(struct platform_device *pdev)
{
	struct realtek_aes_dev *adev = NULL;

	crypto_unregister_aead(&realtek_gcm_alg);
	crypto_unregister_algs(realtek_aes_algs, ARRAY_SIZE(realtek_aes_algs));

	list_for_each_entry(adev, &realtek_aes.dev_list, list)
	list_del(&adev->list);

	return 0;
}

static const struct of_device_id realtek_aes_of_match[] = {
	{.compatible = "realtek,ameba-aes",},
	{ }
};

MODULE_DEVICE_TABLE(of, realtek_aes_of_match);

static struct platform_driver realtek_aes_driver = {
	.probe		= realtek_aes_probe,
	.remove		= realtek_aes_remove,
	.driver		= {
		.name	= "realtek-ameba-aes",
		.of_match_table	= realtek_aes_of_match,
	}
};

builtin_platform_driver(realtek_aes_driver);

MODULE_DESCRIPTION("Realtek Ameba AES driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");