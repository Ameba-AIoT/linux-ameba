// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek OTP support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/nvmem-provider.h>

#define READONLY					0
#define RETRY_COUNT					3

//OTPC_OTP_PARAM
#define RTK_OTPC_BIT_BUSY			BIT(8)
#define RTK_OTPC_OTP_AS				0x0008
#define RTK_OTPC_OTP_CTRL			0x0014
#define RTK_OTPC_OTP_PARAM			0x0040
#define RTK_REG_AON_PWC				0x0000

#define RTK_OTP_LMAP_LEN			0x400
#define RTK_OTP_ACCESS_PWD			0x69
#define RTK_OTP_POLL_TIMES			20000

#define RTK_OTP_LBASE_EFUSE			0x07UL

#define RTK_OTP_LPGPKT_SIZE 		16

/*!<R/WPD/ET 0  Write this bit will trig an indirect read or write. Write 1: trigger write write 0: trigger read After this operation done, this bit will toggle. */
#define RTK_OTPC_BIT_EF_RD_WR_NS	((u32)0x00000001 << 31)
#define RTK_OTPC_MASK_EF_PG_PWD		((u32)0x000000FF << 24)
#define RTK_OTPC_MASK_EF_DATA_NS	((u32)0x000000FF << 0)
#define RTK_AON_BIT_PWC_AON_OTP		((u32)0x00000001 << 0)
#define RTK_OTPC_EF_PG_PWD(x)		((u32)(((x) & 0x000000FF) << 24))
#define RTK_OTPC_EF_ADDR_NS(x)		((u32)(((x) & 0x000007FF) << 8))
#define RTK_OTPC_EF_MODE_SEL_NS(x)	((u32)(((x) & 0x00000007) << 19))

#define RTK_LOGICAL_MAP_SECTION_LEN	0x1FD

/** OTPC_LOGICAL_Format_definitions*/
#define RTK_OTP_LTYP0				0x00UL
#define RTK_OTP_LTYP1				0x01UL
#define RTK_OTP_LTYP2				0x02UL
#define RTK_OTP_LTYP3				0x03UL
#define RTK_OTP_GET_LTYP(x)			((u32)(((x >> 28) & 0x0000000F)))

#define RTK_OTP_GET_LTYP1_BASE(x)	((u32)(((x >> 12) & 0x0000000F)))
#define RTK_OTP_GET_LTYP1_DATA(x)	((u32)(((x >> 16) & 0x000000FF)))
#define RTK_OTP_GET_LTYP1_OFFSET(x)	((u32)(((x >> 0) & 0x00000FFF)))

#define RTK_OTP_GET_LTYP2_LEN(x)	((u32)((((x >> 24) & 0x0000000F) + 1) << 2))
#define RTK_OTP_SET_LTYP2_LEN(x)	((u32)( ((x & 0x03F) >> 2) - 1))
#define RTK_OTP_GET_LTYP2_BASE(x)	((u32)(((x >> 12) & 0x0000000F)))
#define RTK_OTP_GET_LTYP2_OFFSET(x)	((u32)(((x >> 0) & 0x00000FFF)))

#define RTK_OTP_GET_LTYP3_OFFSET(x)	((u32)(((x >> 0) & 0x0FFFFFFF)))

enum rtk_otp_opmode {
	RTK_OTP_USER_MODE = 0,
	RTK_OTP_PGR_MODE = 2,
};

enum rtk_otp_pgmode {
	RTK_OTP_PG_BYTE = 0,
	RTK_OTP_PG_WORD = 4,
};

struct rtk_otp {
	struct device *dev;
	void __iomem *sys_base;
	void __iomem *mem_base;
	size_t mem_len;
};

struct rtk_otp_params {
	const char *name;
	int (*read)(void *priv, unsigned int offset, void *val, size_t bytes);
	int (*write)(void *priv, unsigned int offset, void *val, size_t bytes);
};

static void rtk_otp_wait_for_busy(struct rtk_otp *rtk_otp, u32 flags)
{
	u32 val = 0;

	if (!rtk_otp) {
		return;
	}

	val = readl(rtk_otp->mem_base + RTK_OTPC_OTP_PARAM);

	if (flags) {
		while (val & RTK_OTPC_BIT_BUSY) {
			udelay(100);
		}

		val |= RTK_OTPC_BIT_BUSY;
	} else {
		val &= ~RTK_OTPC_BIT_BUSY;
	}

	writel(val, rtk_otp->mem_base + RTK_OTPC_OTP_PARAM);
}

static bool rtk_otp_get_power_state(struct rtk_otp *rtk_otp)
{
	u32 state = 0;

	state = readl((volatile void __iomem *)(rtk_otp->sys_base + RTK_REG_AON_PWC));
	if (state & RTK_AON_BIT_PWC_AON_OTP) {
		return true;
	}

	return false;
}

static void rtk_otp_set_power_cmd(struct rtk_otp *rtk_otp, bool enable)
{
	u32 state = readl((volatile void __iomem *)(rtk_otp->sys_base + RTK_REG_AON_PWC));

	if (enable) {
		state |= RTK_AON_BIT_PWC_AON_OTP;
	} else {
		state &= ~RTK_AON_BIT_PWC_AON_OTP;
	}

	writel(state, (volatile void __iomem *)(rtk_otp->sys_base + RTK_REG_AON_PWC));
}

static void rtk_otp_access_cmd(struct rtk_otp *rtk_otp, bool enable)
{
	u32 val = 0;

	val = readl(rtk_otp->mem_base + RTK_OTPC_OTP_CTRL);
	if (enable) {
		val |= RTK_OTPC_EF_PG_PWD(RTK_OTP_ACCESS_PWD);
	} else {
		val &= ~RTK_OTPC_MASK_EF_PG_PWD;
	}

	writel(val, rtk_otp->mem_base + RTK_OTPC_OTP_CTRL);
}

static void rtk_otp_power_switch(struct rtk_otp *rtk_otp, bool bwrite, bool pstate)
{
	if (pstate == true) {
		if (rtk_otp_get_power_state(rtk_otp) == false) {
			rtk_otp_set_power_cmd(rtk_otp, true);
		}
	}

	rtk_otp_access_cmd(rtk_otp, bwrite);
}

static int rtk_otp_readb(struct rtk_otp *rtk_otp, u32 addr, u8 *data, int mode)
{
	int ret = 0;
	u32 val = 0;
	u32 idx = 0;
	volatile void __iomem *otp_as_addr = NULL;

	if (!rtk_otp) {
		*data = 0xff;
		ret = -1;
		goto exit;
	}

	if (addr >= rtk_otp->mem_len) {
		dev_err(rtk_otp->dev, "Failed to read addr = 0x%08X, mem_len = 0x%08X\n", addr, rtk_otp->mem_len);
		*data = 0xff;
		ret = -1;
		goto exit;
	}

	otp_as_addr = (volatile void __iomem *)(rtk_otp->mem_base + RTK_OTPC_OTP_AS);

	rtk_otp_wait_for_busy(rtk_otp, 1); // Wait and set busy flag
	rtk_otp_power_switch(rtk_otp, false, true);

	val = RTK_OTPC_EF_ADDR_NS(addr);
	if (mode == RTK_OTP_PGR_MODE) { // For pogram margin read
		val |= RTK_OTPC_EF_MODE_SEL_NS(RTK_OTP_PGR_MODE);
	}

	writel(val, otp_as_addr);

	/* 10~20us is needed */
	val = readl(otp_as_addr);
	while (idx < RTK_OTP_POLL_TIMES && (!(val & RTK_OTPC_BIT_EF_RD_WR_NS))) {
		udelay(5);
		idx++;
		val = readl(otp_as_addr);
	}

	if (idx < RTK_OTP_POLL_TIMES) {
		*data = (u8)(val & RTK_OTPC_MASK_EF_DATA_NS);
		dev_dbg(rtk_otp->dev, "Read 0x%08X = 0x%02x\n", addr, *data);
	} else {
		*data = 0xff;
		ret = -1;
		dev_err(rtk_otp->dev, "Failed to read addr 0x%08X\n", addr);
	}

	rtk_otp_power_switch(rtk_otp, false, false);
	rtk_otp_wait_for_busy(rtk_otp, 0); //reset busy flag

exit:
	return ret;
}

static int rtk_otp_readl(struct rtk_otp *rtk_otp, u32 addr, u32 *data)
{
	int i = 0;
	u8 val;

	*data = 0;
	for (; i < 4; i++) {
		if (rtk_otp_readb(rtk_otp, addr++, &val, RTK_OTP_USER_MODE) < 0) {
			return -1;
		}

		*data |= (((u32)val) << (8 * i));
	}

	return 0;
}

static int rtk_otp_logical_read(void *context,
								unsigned int reg, void *buf, size_t bytes)
{
	struct rtk_otp *rtk_otp = context;
	int ret = 0;
	u32 addr = reg;
	u8 *pbuf = (u8 *)buf;
	u32 otp_addr = 0;
	u32 otp_data = 0;
	u32 offset;
	u8 data, plen, type;

	if (addr + bytes > RTK_OTP_LMAP_LEN) {
		dev_err(rtk_otp->dev, "Logical map read 0x%08X bytes from addr 0x%08X exceed limit 0x%04X\n", bytes, addr, RTK_OTP_LMAP_LEN);
		return -1;
	}

	/*  0xff will be OTP default value instead of 0x00. */
	memset(pbuf, 0xff, bytes);

	while (otp_addr < RTK_LOGICAL_MAP_SECTION_LEN) {
		rtk_otp_readl(rtk_otp, otp_addr, &otp_data);

		if (otp_data == 0xFFFFFFFF) {
			dev_err(rtk_otp->dev, "Logical read data end at address 0x%08X\n", otp_addr);
			break;
		}

		otp_addr += 4;

		switch (RTK_OTP_GET_LTYP(otp_data)) {
		case RTK_OTP_LTYP0:
			/* Empty entry shift to next entry */
			break;
		case RTK_OTP_LTYP1:
			if (RTK_OTP_GET_LTYP1_BASE(otp_data) == RTK_OTP_LBASE_EFUSE) {
				offset = RTK_OTP_GET_LTYP1_OFFSET(otp_data);
				data = RTK_OTP_GET_LTYP1_DATA(otp_data);

				if ((offset >= addr) && (offset < addr + bytes)) {
					pbuf[offset - addr] = data;
				}
			}
			break;
		case RTK_OTP_LTYP2:
			plen = RTK_OTP_GET_LTYP2_LEN(otp_data);
			offset = RTK_OTP_GET_LTYP2_OFFSET(otp_data);
			type = RTK_OTP_GET_LTYP2_BASE(otp_data);

			if (type == RTK_OTP_LBASE_EFUSE) {
				while (plen-- > 0) {
					rtk_otp_readb(rtk_otp, otp_addr++, &data, RTK_OTP_USER_MODE);

					if ((offset >= addr) && (offset < addr + bytes)) {
						pbuf[offset - addr] = data;
					}
					offset++;
				}
			} else {
				otp_addr += plen;
			}
			break;
		default:
			break;
		}

		if ((otp_addr & 0x03) != 0) {
			dev_err(rtk_otp->dev, "Alignment addr = 0x%08X\n", otp_addr);
		}
	}

	return ret;
}

static int rtk_otp_physical_read(void *context,
								 unsigned int reg, void *buf, size_t bytes)
{
	struct rtk_otp *rtk_otp = context;
	int ret = 0;
	u32 addr = reg;
	u8 *pbuf = (u8 *)buf;
	u8 val;
	u32 i = 0;
	for (; i < bytes; i++) {
		if (rtk_otp_readb(rtk_otp, addr++, &val, RTK_OTP_USER_MODE) < 0) {
			ret = -1;
			goto exit;
		}

		*pbuf++ = val;
	}

exit:
	return ret;
}

static int rtk_otp_writeb(struct rtk_otp *rtk_otp, u32 addr, u8 data)
{
	int ret = 0;
	u32 idx = 0;
	u32 val = 0;
	volatile void __iomem *otp_as_addr = NULL;

	if (data == 0xff) {
		return 0;
	}

	if (!rtk_otp) {
		ret = -1;
		goto exit;
	}

	if (addr >= rtk_otp->mem_len) {
		dev_err(rtk_otp->dev, "Failed to write addr = 0x%08X, mem_len = 0x%08X\n", addr, rtk_otp->mem_len);
		ret = -1;
		goto exit;
	}

	otp_as_addr = (volatile void __iomem *)(rtk_otp->mem_base + RTK_OTPC_OTP_AS);

	rtk_otp_wait_for_busy(rtk_otp, 1); // Wait and set busy flag
	rtk_otp_power_switch(rtk_otp, true, true);

	val = data | RTK_OTPC_EF_ADDR_NS(addr) | RTK_OTPC_BIT_EF_RD_WR_NS;

	writel(val, otp_as_addr);

	/* 10~20us is needed */
	val = readl(otp_as_addr);
	while (idx < RTK_OTP_POLL_TIMES && (val & RTK_OTPC_BIT_EF_RD_WR_NS)) {
		udelay(5);
		idx++;
		val = readl(otp_as_addr);
	}

	if (idx >= RTK_OTP_POLL_TIMES) {
		ret = -1;
		dev_err(rtk_otp->dev, "Failed to write addr 0x%08X\n", addr);
	}

	rtk_otp_power_switch(rtk_otp, false, false);
	rtk_otp_wait_for_busy(rtk_otp, 0); //Reset busy flag

exit:
	return ret;
}

static int rtk_otp_write(struct rtk_otp *rtk_otp, u32 addr, u8 data)
{
	int ret = 0;
	u8 val = 0;
	u8 retry = 0;
	u8 target = data;

	if (rtk_otp_readb(rtk_otp, addr, &val, RTK_OTP_PGR_MODE) < 0) {
		dev_err(rtk_otp->dev, "Write addr 0x%08X, PMR read error\n", addr);
		ret = -1;
		goto exit;
	}

retry:
	/* Do not need program bits include originally do not need program
	(bits equals 1 in data) and already prgoramed bits(bits euqals 0 in Temp) */
	data |= ~val;

	/* Program*/
	if (rtk_otp_writeb(rtk_otp, addr, data) < 0) {
		dev_err(rtk_otp->dev, "Write addr 0x%08X, OTP write error\n", addr);
		ret = -1;
		goto exit;
	}

	/* Read after program*/
	if (rtk_otp_readb(rtk_otp, addr, &val, RTK_OTP_PGR_MODE) < 0) {
		dev_err(rtk_otp->dev, "Write addr 0x%08X, PMR read after program error\n", addr);
		ret = -1;
		goto exit;
	}

	/* Program do not get desired value,the OTP can be programmed at most 3 times
		here only try once.*/
	if (val != target) {
		if (retry++ >= RETRY_COUNT) {
			dev_err(rtk_otp->dev, "Write addr 0x%08X, read check error\n", addr);
			ret = -1;
			goto exit;
		} else {
			goto retry;
		}
	}

exit:
	return ret;
}

static int rtk_otp_physical_write(void *context,
								  unsigned int reg, void *val, size_t bytes)
{
	struct rtk_otp *rtk_otp = context;
	u32 i = 0;
	u32 addr = reg;
	u8 *data = (u8 *)val;

	for (; i < bytes; i++) {
		if (rtk_otp_write(rtk_otp, addr++, *data++) < 0) {
			dev_err(rtk_otp->dev, "Physical write failed\n");
			return -1;
		}
	}

	return 0;
}

/**
  * @brief  PG one logical map OTP packet in Byte format
  * @param  offset: offsetlogical addr
  * @param  bytes: packet len, byte = 0 represent one byte
  * @param  data: packet data
  * @param  mode:
  *           RTK_OTP_PG_BYTE represent one byte,
  *           RTK_OTP_PG_WORD represent word,
  * @retval status value:
  *         0: write ok
  *         -1: write fail
  */
static int rtk_otp_pg_packet(struct rtk_otp *rtk_otp,
							 u16 offset, u8 bytes, u8 *data, int mode)
{
	int ret = 0;
	u32 idx = 0, idxtmp;
	u32 otpdata;

	if (mode == RTK_OTP_PG_WORD && ((bytes > RTK_OTP_LPGPKT_SIZE) || ((bytes & 0x03) != 0))) {
		dev_err(rtk_otp->dev, "Size error: offset = 0x%08X, len = 0x%02X\n", offset, bytes);
		ret = -1;
		goto exit;
	}

	if (offset > RTK_OTP_LMAP_LEN) {
		dev_err(rtk_otp->dev, "Addr 0x%08X exceed limit\n", offset);
		ret = -1;
		goto exit;
	}

	while (idx < RTK_LOGICAL_MAP_SECTION_LEN) {
		if (rtk_otp_readl(rtk_otp, idx, &otpdata) < 0) {
			ret = -1;
			goto exit;
		}

		if (otpdata == 0xFFFFFFFF) {
			break;
		}

		idx += 4;
		if (RTK_OTP_GET_LTYP(otpdata) == RTK_OTP_LTYP2) {
			idx += RTK_OTP_GET_LTYP2_LEN(otpdata);
		}

		if (RTK_OTP_GET_LTYP(otpdata) == RTK_OTP_LTYP3) {
			idx += 4;
		}
	}

	if (idx + bytes > RTK_LOGICAL_MAP_SECTION_LEN) {
		dev_err(rtk_otp->dev, "No enough space from 0x%08X\n", idx);
		ret = -1;
		goto exit;
	}

	rtk_otp_writeb(rtk_otp, idx++, offset & 0xFF); //header[7:0]
	rtk_otp_writeb(rtk_otp, idx++, (RTK_OTP_LBASE_EFUSE << 4) | ((offset >> 8) & 0x0F)); //header[15:8]
	if (mode == RTK_OTP_PG_BYTE) {
		rtk_otp_writeb(rtk_otp, idx++, data[0]); //header[23:16]
		rtk_otp_writeb(rtk_otp, idx++, ((RTK_OTP_LTYP1 << 4) | 0x0F)); //header[31:24]
	} else if (mode == RTK_OTP_PG_WORD) {
		idx++; //header[23:16] reserved
		rtk_otp_writeb(rtk_otp, idx++, ((RTK_OTP_LTYP2 << 4) | RTK_OTP_SET_LTYP2_LEN(bytes))); //header[31:24]
		for (idxtmp = 0; idxtmp < bytes && !ret; idxtmp++) {
			ret = rtk_otp_writeb(rtk_otp, idx++, data[idxtmp]);
		}
	}

exit:
	return ret;
}

static int rtk_otp_logical_write(void *context,
								 unsigned int reg, void *val, size_t bytes)
{
	struct rtk_otp *rtk_otp = context;
	u8 *data = (u8 *)val;
	u32 addr = reg;
	u32	base, offset;
	u32 bytemap = 0, wordmap = 0, bytechange = 0, wordchange = 0;
	int remain = bytes;
	int ret = 0;

	dev_dbg(rtk_otp->dev, "Logical map write addr = 0x%08X, bytes = 0x%08X\n", addr, bytes);

	if (addr + bytes > RTK_OTP_LMAP_LEN) {
		dev_err(rtk_otp->dev, "Logical map write 0x%08X bytes from addr 0x%08X exceed limit\n", addr, bytes);
		return -1;
	}

	/* 4bytes one section */
	base = addr & 0xFFFFFFF0;
	offset = addr & 0x0000000F;

	while (remain > 0) {
		u32 i, j;
		u8 newdata[RTK_OTP_LPGPKT_SIZE];
		u8 wordstart = 0, wordend = 0, wordoffset;
		u8 wcnt = (remain + offset > RTK_OTP_LPGPKT_SIZE) ? RTK_OTP_LPGPKT_SIZE : (remain + offset);
		ret = rtk_otp_logical_read(rtk_otp, base, newdata, bytes);

		if (ret < 0) {
			dev_err(rtk_otp->dev, "Logical map read error when write 0x%08X\n", base);
			ret = -1;
			goto exit;
		}

		/*compare and record changed data*/
		for (i = offset, j = 0; i < wcnt; i++, j++) {
			if (data[j] != newdata[i]) {
				newdata[i] = data[j];
				bytemap |= BIT(i);
				wordmap |= BIT(i >> 2);
				bytechange++;
				dev_dbg(rtk_otp->dev, "Dump newdata[0x%08X] = 0x%08X\n", i, newdata[i]);
			}
		}

		/*no changes in the PKT,go and try next*/
		if (bytechange == 0) {
			goto next;
		}

		/*find start word and end word*/
		wordstart = RTK_OTP_LPGPKT_SIZE;
		wordend = 0;
		for (i = offset / 4 ; i < RTK_OTP_LPGPKT_SIZE / 4 ; i++) {
			if ((wordmap >> i) & BIT(0)) {
				/*choose a small one as start word*/
				wordstart = (wordstart < i) ? wordstart : i;
				/*choose a big one as end word*/
				wordend = (wordend > i) ? wordend : i;
			}
		}

		/*calculate word change*/
		wordchange = wordend - wordstart + 1;
		wordoffset = wordstart * 4;

		/*choose use witch type to write the logical efuse*/
		if ((wordchange + 1) < bytechange) {
			ret = rtk_otp_pg_packet(rtk_otp, base + wordoffset,
									wordchange * 4, &newdata[wordoffset], RTK_OTP_PG_WORD);
		} else {
			for (i = 0; i < RTK_OTP_LPGPKT_SIZE; i++) {
				if (bytemap & BIT(i)) {
					ret = rtk_otp_pg_packet(rtk_otp, base + i,
											0, &newdata[i], RTK_OTP_PG_BYTE);
				}
			}
		}

next:
		remain -= (RTK_OTP_LPGPKT_SIZE - offset);
		data += (RTK_OTP_LPGPKT_SIZE - offset);
		base += RTK_OTP_LPGPKT_SIZE;
		offset = 0;

		bytemap = 0;
		wordmap = 0;
		bytechange = 0;

		dev_dbg(rtk_otp->dev, "Next write cycle base = 0x%08X, remain = 0x%08X\n", base, remain);
	}

exit:
	return ret;
}

static int rtk_otp_probe(struct platform_device *pdev)
{
	int ret = 0;
	size_t sys_base_size;
	struct resource *res;
	struct rtk_otp *rtk_otp;
	struct nvmem_device *nvmem;
	struct nvmem_config config = {};
	const struct rtk_otp_params *data;

	data = of_device_get_match_data(&pdev->dev);
	if (!data) {
		dev_err(&pdev->dev, "Failed to get match data\n");
		return -EINVAL;
	}

	rtk_otp = devm_kzalloc(&pdev->dev, sizeof(struct rtk_otp), GFP_KERNEL);
	if (!rtk_otp) {
		dev_err(&pdev->dev, "Failed to alloc memory\n");
		ret = -ENOMEM;
		goto exit;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get resource\n");
		ret = -ENOMEM;
		goto exit;
	}

	rtk_otp->mem_len = res->end - res->start + 1;
	rtk_otp->mem_base = ioremap(res->start, rtk_otp->mem_len);
	if (!rtk_otp->mem_base) {
		dev_err(&pdev->dev, "Failed to map resource\n");
		ret = -ENXIO;
		goto exit;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get sys resource\n");
		ret = -ENOMEM;
		goto exit;
	}

	sys_base_size = res->end - res->start + 1;
	rtk_otp->sys_base = ioremap(res->start, sys_base_size);
	if (!rtk_otp->sys_base) {
		dev_err(&pdev->dev, "Failed to map sys resource\n");
		ret = -ENXIO;
		goto exit;
	}

	rtk_otp->dev = &pdev->dev;

	config.name = data->name;
	config.stride = 1;
	config.word_size = 1;
	config.reg_read = data->read;
	config.size = rtk_otp->mem_len;
	config.priv = rtk_otp;
	config.dev = &pdev->dev;
#if READONLY
	config.read_only = true;
#else
	config.read_only = false;
	config.reg_write = data->write;
#endif

	nvmem = devm_nvmem_register(rtk_otp->dev, &config);

	ret = PTR_ERR_OR_ZERO(nvmem);
	if (ret) {
		goto exit;
	}

	dev_info(&pdev->dev, "Initialized successfully\n");
	return ret;
exit:
	if (rtk_otp && rtk_otp->mem_base) {
		iounmap(rtk_otp->mem_base);
	}

	if (rtk_otp && rtk_otp->sys_base) {
		iounmap(rtk_otp->sys_base);
	}

	return ret;
}

static const struct rtk_otp_params otp_physical_params = {
	.name = "otp_raw",
	.read = rtk_otp_physical_read,
	.write = rtk_otp_physical_write,
};

static const struct rtk_otp_params otp_logical_params = {
	.name = "otp_map",
	.read = rtk_otp_logical_read,
	.write = rtk_otp_logical_write,
};

static const struct of_device_id rtk_otp_of_match[] = {
	{
		.compatible = "realtek,ameba-otp-phy",
		.data = (void *) &otp_physical_params,
	},
	{
		.compatible = "realtek,ameba-otp-logical",
		.data = (void *) &otp_logical_params,
	},
	{ /* end node */ },
};

static struct platform_driver rtk_otp_driver = {
	.probe	= rtk_otp_probe,
	.driver	= {
		.name = "realtek-ameba-otp",
		.of_match_table = rtk_otp_of_match,
	},
};

builtin_platform_driver(rtk_otp_driver);

MODULE_DESCRIPTION("Realtek Ameba OTP driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Realtek Corporation");
