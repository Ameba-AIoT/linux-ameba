// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek RCC support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef _RCC_REALTEK_H
#define _RCC_REALTEK_H

/* Realtek clock data definitions */
#define RTK_NO_FLAGS						0
#define RTK_NO_GATE_FLAGS					0
#define RTK_NO_MUX_FLAGS					0
#define RTK_NO_DIV_FLAGS					0
#define RTK_NO_DIV_TABLE					NULL

enum {
	RTK_CLK_GATE 			= 0x01,
	RTK_CLK_MUX 			= 0x02,
	RTK_CLK_DIV 			= 0x03,
	RTK_CLK_FIXED_FACTOR	= 0x04,
	RTK_CLK_APLL			= 0x05,

	RTK_CLK_MASK			= 0xFF,
};

struct rtk_fen {
	void __iomem *reg;
	u32	offset;
	u8	bit;
};

struct rtk_apll {
	void __iomem *reg;
	struct clk_hw hw;
};


struct rtk_clk {
	u32 offset;
	int flags;

	struct clk_gate gate;
	struct rtk_fen fen;
	struct clk_mux mux;
	struct clk_divider div;
	struct clk_fixed_factor ff;
	struct rtk_apll apll;
};

struct rtk_reset_map {
	u32 offset;
	u32	bit;
};

struct rtk_reset {
	const struct rtk_reset_map *map;
	struct reset_controller_dev	rdev;
};

struct rtk_rcc_data {
	const struct rtk_clk **clocks;
	int clock_num;
	const struct rtk_reset_map *resets;
	int reset_num;
};

#define RTK_GATE_CLK(_struct, _name, _parent, _gate_offset, _gate_bit, _gate_flags, _fen_offset, _fen_bit, _flags)	\
	static struct rtk_clk _struct = { 					\
		.offset			= _gate_offset,					\
		.flags			= RTK_CLK_GATE,					\
		.gate = {										\
			.bit_idx	= _gate_bit,					\
			.flags		= _gate_flags,					\
			.hw.init	= CLK_HW_INIT(_name,			\
							  _parent,					\
							  &rtk_gate_clk_ops,		\
							  _flags),					\
		},												\
		.fen = {										\
			.offset		= _fen_offset, 					\
			.bit		= _fen_bit, 					\
		},												\
	}

#define RTK_MUX_CLK(_struct, _name, _parents, _mux_offset, _mux_shift, _mux_mask, _mux_flags, _flags)	\
	static struct rtk_clk _struct = { 					\
		.offset			= _mux_offset,					\
		.flags			= RTK_CLK_MUX,					\
		.mux = {										\
			.shift		= _mux_shift,					\
			.mask		= _mux_mask,					\
			.flags		= _mux_flags,					\
			.hw.init	= CLK_HW_INIT_PARENTS(_name,	\
							  _parents,					\
							  &clk_mux_ops,				\
							  _flags),					\
		},												\
	}

#define RTK_DIV_CLK(_struct, _name, _parent, _div_offset, _div_shift, _div_width, _div_table, _div_flags, _flags)	\
	static struct rtk_clk _struct = { 					\
		.offset			= _div_offset,					\
		.flags			= RTK_CLK_DIV,					\
		.div = {										\
			.shift		= _div_shift,					\
			.width		= _div_width,					\
			.table		= _div_table,					\
			.flags		= _div_flags,					\
			.hw.init	= CLK_HW_INIT(_name,			\
							  _parent,					\
							  &clk_divider_ops,			\
							  _flags),					\
		},												\
	}

#define RTK_FIXED_FACTOR_CLK(_struct, _name, _parent, _mult, _div, _flags)	\
	static struct rtk_clk _struct = { 					\
		.flags			= RTK_CLK_FIXED_FACTOR,			\
		.ff = {											\
			.mult		= _mult,						\
			.div		= _div,							\
			.hw.init	= CLK_HW_INIT(_name,			\
							  _parent,					\
							  &clk_fixed_factor_ops,	\
							  _flags),					\
		},												\
	}

#define RTK_APLL_CLK(_struct, _name, _apll_offset, _flags)	\
	static struct rtk_clk _struct = {					\
		.offset			= _apll_offset,					\
		.flags			= RTK_CLK_APLL, 				\
		.apll = { 										\
			.hw.init	= CLK_HW_INIT_NO_PARENT(_name,	\
							  &rtk_apll_clk_ops,		\
							  _flags),					\
		},												\
	}


extern const struct clk_ops rtk_gate_clk_ops;
extern const struct clk_ops rtk_apll_clk_ops;

inline struct rtk_clk *rtk_gate_hw_to_clk(struct clk_hw *hw)
{
	struct clk_gate *gate = to_clk_gate(hw);

	return container_of(gate, struct rtk_clk, gate);
}

inline struct rtk_clk *rtk_mux_hw_to_clk(struct clk_hw *hw)
{
	struct clk_mux *mux = to_clk_mux(hw);

	return container_of(mux, struct rtk_clk, mux);
}

inline struct rtk_clk *rtk_div_hw_to_clk(struct clk_hw *hw)
{
	struct clk_divider *div = to_clk_divider(hw);

	return container_of(div, struct rtk_clk, div);
}

inline struct rtk_clk *rtk_ff_hw_to_clk(struct clk_hw *hw)
{
	struct clk_fixed_factor *ff = to_clk_fixed_factor(hw);

	return container_of(ff, struct rtk_clk, ff);
}

inline struct rtk_reset *rdev_to_rtk_reset(struct reset_controller_dev *rdev)
{
	return container_of(rdev, struct rtk_reset, rdev);
}

#endif /* _RCC_REALTEK_H */
