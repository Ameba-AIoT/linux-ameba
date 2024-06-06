/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/linux/random.h
 *
 * Include file for the random number generator.
 */
#ifndef _LINUX_RANDOM_H
#define _LINUX_RANDOM_H

#include <linux/list.h>
#include <linux/once.h>

#include <uapi/linux/random.h>

struct random_ready_callback {
	struct list_head list;
	void (*func)(struct random_ready_callback *rdy);
	struct module *owner;
};

extern void add_device_randomness(const void *, unsigned int);
extern void add_bootloader_randomness(const void *, unsigned int);

#if defined(LATENT_ENTROPY_PLUGIN) && !defined(__CHECKER__)
static inline void add_latent_entropy(void)
{
	add_device_randomness((const void *)&latent_entropy,
			      sizeof(latent_entropy));
}
#else
static inline void add_latent_entropy(void) {}
#endif

extern void add_input_randomness(unsigned int type, unsigned int code,
				 unsigned int value) __latent_entropy;
extern void add_interrupt_randomness(int irq, int irq_flags) __latent_entropy;

extern void get_random_bytes(void *buf, int nbytes);
extern int wait_for_random_bytes(void);
extern int __init rand_initialize(void);
extern bool rng_is_initialized(void);
extern int add_random_ready_callback(struct random_ready_callback *rdy);
extern void del_random_ready_callback(struct random_ready_callback *rdy);
extern int __must_check get_random_bytes_arch(void *buf, int nbytes);

#ifndef MODULE
extern const struct file_operations random_fops, urandom_fops;
#endif

u8 get_random_u8(void);
u16 get_random_u16(void);
u32 get_random_u32(void);
u64 get_random_u64(void);
static inline unsigned int get_random_int(void)
{
	return get_random_u32();
}
static inline unsigned long get_random_long(void)
{
#if BITS_PER_LONG == 64
	return get_random_u64();
#else
	return get_random_u32();
#endif
}

u32 __get_random_u32_below(u32 ceil);

/*
 * Returns a random integer in the interval [0, ceil), with uniform
 * distribution, suitable for all uses. Fastest when ceil is a constant, but
 * still fast for variable ceil as well.
 */
static inline u32 get_random_u32_below(u32 ceil)
{
	if (!__builtin_constant_p(ceil))
		return __get_random_u32_below(ceil);

	/*
	 * For the fast path, below, all operations on ceil are precomputed by
	 * the compiler, so this incurs no overhead for checking pow2, doing
	 * divisions, or branching based on integer size. The resultant
	 * algorithm does traditional reciprocal multiplication (typically
	 * optimized by the compiler into shifts and adds), rejecting samples
	 * whose lower half would indicate a range indivisible by ceil.
	 */
	BUILD_BUG_ON_MSG(!ceil, "get_random_u32_below() must take ceil > 0");
	if (ceil <= 1)
		return 0;
	for (;;) {
		if (ceil <= 1U << 8) {
			u32 mult = ceil * get_random_u8();
			if (likely(is_power_of_2(ceil) || (u8)mult >= (1U << 8) % ceil))
				return mult >> 8;
		} else if (ceil <= 1U << 16) {
			u32 mult = ceil * get_random_u16();
			if (likely(is_power_of_2(ceil) || (u16)mult >= (1U << 16) % ceil))
				return mult >> 16;
		} else {
			u64 mult = (u64)ceil * get_random_u32();
			if (likely(is_power_of_2(ceil) || (u32)mult >= -ceil % ceil))
				return mult >> 32;
		}
	}
}

/*
 * Returns a random integer in the interval (floor, U32_MAX], with uniform
 * distribution, suitable for all uses. Fastest when floor is a constant, but
 * still fast for variable floor as well.
 */
static inline u32 get_random_u32_above(u32 floor)
{
	BUILD_BUG_ON_MSG(__builtin_constant_p(floor) && floor == U32_MAX,
			 "get_random_u32_above() must take floor < U32_MAX");
	return floor + 1 + get_random_u32_below(U32_MAX - floor);
}

/*
 * Returns a random integer in the interval [floor, ceil], with uniform
 * distribution, suitable for all uses. Fastest when floor and ceil are
 * constant, but still fast for variable floor and ceil as well.
 */
static inline u32 get_random_u32_inclusive(u32 floor, u32 ceil)
{
	BUILD_BUG_ON_MSG(__builtin_constant_p(floor) && __builtin_constant_p(ceil) &&
			 (floor > ceil || ceil - floor == U32_MAX),
			 "get_random_u32_inclusive() must take floor <= ceil");
	return floor + get_random_u32_below(ceil - floor + 1);
}


/*
 * On 64-bit architectures, protect against non-terminated C string overflows
 * by zeroing out the first byte of the canary; this leaves 56 bits of entropy.
 */
#ifdef CONFIG_64BIT
# ifdef __LITTLE_ENDIAN
#  define CANARY_MASK 0xffffffffffffff00UL
# else /* big endian, 64 bits: */
#  define CANARY_MASK 0x00ffffffffffffffUL
# endif
#else /* 32 bits: */
# define CANARY_MASK 0xffffffffUL
#endif

static inline unsigned long get_random_canary(void)
{
	unsigned long val = get_random_long();

	return val & CANARY_MASK;
}

/* Calls wait_for_random_bytes() and then calls get_random_bytes(buf, nbytes).
 * Returns the result of the call to wait_for_random_bytes. */
static inline int get_random_bytes_wait(void *buf, int nbytes)
{
	int ret = wait_for_random_bytes();
	get_random_bytes(buf, nbytes);
	return ret;
}

#define declare_get_random_var_wait(var) \
	static inline int get_random_ ## var ## _wait(var *out) { \
		int ret = wait_for_random_bytes(); \
		if (unlikely(ret)) \
			return ret; \
		*out = get_random_ ## var(); \
		return 0; \
	}
declare_get_random_var_wait(u32)
declare_get_random_var_wait(u64)
declare_get_random_var_wait(int)
declare_get_random_var_wait(long)
#undef declare_get_random_var

unsigned long randomize_page(unsigned long start, unsigned long range);

/*
 * This is designed to be standalone for just prandom
 * users, but for now we include it from <linux/random.h>
 * for legacy reasons.
 */
#include <linux/prandom.h>

#ifdef CONFIG_ARCH_RANDOM
# include <asm/archrandom.h>
#else
static inline bool arch_get_random_long(unsigned long *v)
{
	return 0;
}
static inline bool arch_get_random_int(unsigned int *v)
{
	return 0;
}
static inline bool arch_has_random(void)
{
	return 0;
}
static inline bool arch_get_random_seed_long(unsigned long *v)
{
	return 0;
}
static inline bool arch_get_random_seed_int(unsigned int *v)
{
	return 0;
}
static inline bool arch_has_random_seed(void)
{
	return 0;
}
#endif

#endif /* _LINUX_RANDOM_H */
