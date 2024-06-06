/*
 * Copyright (C) 2021 Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/vmalloc.h>
#include <linux/ptrace.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

#include "mrdump.h"

void mrdump_setup_regs(struct pt_regs *newregs, struct pt_regs *oldregs)
{
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
	} else {
		u64 tmp1, tmp2;

		__asm__ __volatile__("stp	 x0,   x1, [%2, #16 *  0]\n"
							 "stp	 x2,   x3, [%2, #16 *  1]\n"
							 "stp	 x4,   x5, [%2, #16 *  2]\n"
							 "stp	 x6,   x7, [%2, #16 *  3]\n"
							 "stp	 x8,   x9, [%2, #16 *  4]\n"
							 "stp	x10,  x11, [%2, #16 *  5]\n"
							 "stp	x12,  x13, [%2, #16 *  6]\n"
							 "stp	x14,  x15, [%2, #16 *  7]\n"
							 "stp	x16,  x17, [%2, #16 *  8]\n"
							 "stp	x18,  x19, [%2, #16 *  9]\n"
							 "stp	x20,  x21, [%2, #16 * 10]\n"
							 "stp	x22,  x23, [%2, #16 * 11]\n"
							 "stp	x24,  x25, [%2, #16 * 12]\n"
							 "stp	x26,  x27, [%2, #16 * 13]\n"
							 "stp	x28,  x29, [%2, #16 * 14]\n"
							 "mov	 %0,  sp\n"
							 "stp	x30,  %0,  [%2, #16 * 15]\n"

							 "/* faked current PSTATE */\n"
							 "mrs	 %0, CurrentEL\n"
							 "mrs	 %1, SPSEL\n"
							 "orr	 %0, %0, %1\n"
							 "mrs	 %1, DAIF\n"
							 "orr	 %0, %0, %1\n"
							 "mrs	 %1, NZCV\n"
							 "orr	 %0, %0, %1\n"
							 /* pc */
							 "adr	 %1, 1f\n"
							 "1:\n"
							 "stp	 %1, %0,   [%2, #16 * 16]\n"
							 : "=&r"(tmp1), "=&r"(tmp2)
							 : "r"(newregs)
							 : "memory");
	}
}

void mrdump_setup_cpu_context(struct pt_regs *regs, struct task_struct *tsk)
{
	struct cpu_context *cpu_context;

	cpu_context = &tsk->thread.cpu_context;

	regs->regs[19] = cpu_context->x19;
	regs->regs[20] = cpu_context->x20;
	regs->regs[21] = cpu_context->x21;
	regs->regs[22] = cpu_context->x22;
	regs->regs[23] = cpu_context->x23;
	regs->regs[24] = cpu_context->x24;
	regs->regs[25] = cpu_context->x25;
	regs->regs[26] = cpu_context->x26;
	regs->regs[27] = cpu_context->x27;
	regs->regs[28] = cpu_context->x28;
	regs->regs[29] = cpu_context->fp;
	regs->sp = cpu_context->sp;
	regs->pc = cpu_context->pc;
}

int mrdump_dump_user_stack(struct user_thread_info *user_thread)
{
	struct vm_area_struct *vma;
	struct task_struct *tsk = user_thread->tsk;
	struct pt_regs *regs = &user_thread->regs;
	unsigned long userstack_start = 0, userstack_end = 0;
	unsigned long first, bottom, top, addr;
	unsigned long length = 0;
	int ret = 0, i;

	/* start of user stack */
	if (is_compat_task()) { /* K64_U32 */
		pr_info("K64+U32 pc/lr/sp 0x%016lx/0x%016lx/0x%016lx\n",
				(long)(regs->user_regs.pc),
				(long)(regs->user_regs.regs[14]),
				(long)(regs->user_regs.regs[13]));
		userstack_start = (unsigned long)regs->user_regs.regs[13];
	} else { /* K64_U64 */
		pr_info("K64+U64 pc/lr/sp 0x%016lx/0x%016lx/0x%016lx\n",
				(long)(regs->user_regs.pc),
				(long)(regs->user_regs.regs[30]),
				(long)(regs->user_regs.sp));
		userstack_start = (unsigned long)regs->user_regs.sp;
	}

	/* end of user stack */
	vma = tsk->mm->mmap;
	while (vma != NULL) {
		if (vma->vm_start <= userstack_start &&
			vma->vm_end >= userstack_start) {
			userstack_end = vma->vm_end;
			break;
		}
		vma = vma->vm_next;
		if (vma == tsk->mm->mmap) {
			break;
		}
	}

	if (userstack_end == 0) {
		pr_err("dump native stack failed:\n");
		return -EFAULT;
	}

	/* copy user stack */
	length = ((userstack_end - userstack_start) < (MaxStackSize - 1)) ?
			 (userstack_end - userstack_start) :
			 (MaxStackSize - 1);

	user_thread->stack_length = length;
	user_thread->stack = vzalloc(length);
	if (user_thread->stack == NULL) {
		return -ENOMEM;
	}

	ret = copy_from_user((void *)(user_thread->stack),
						 (const void __user *)(userstack_start), length);
	pr_info("copy_from_user ret(0x%016x), len: 0x%lx\n", ret, length);
	if (ret) {
		return ret;
	}

	/* dump user stack */
	if (is_compat_task()) { /* K64_U32 */
		pr_info("dump user stack range (0x%08lx to 0x%08lx)\n",
				(unsigned long)userstack_start,
				(unsigned long)userstack_end);

		bottom = (unsigned long)user_thread->stack;
		top = bottom + length;
		for (first = bottom & ~31, addr = userstack_start & ~31;
			 first < top; first += 32) {
			unsigned long p;
			char str[sizeof(" 12345678") * 8 + 1];

			memset(str, ' ', sizeof(str));
			str[sizeof(str) - 1] = '\0';

			for (p = first, i = 0; i < 8 && p < top;
				 i++, p += 4, addr += 4) {
				if (p >= bottom && p < top &&
					addr >= userstack_start &&
					addr < userstack_end)
					sprintf(str + i * 9, " %08x",
							*(u32 *)p);
			}

			pr_info("%04lx:%s\n", (addr - 32) & 0xffff, str);
		}

	} else { /* K64_U64 */
		pr_info("dump user stack range (0x%016lx to 0x%016lx)\n",
				userstack_start, userstack_end);

		bottom = (unsigned long)user_thread->stack;
		top = bottom + length;
		for (first = bottom & ~31, addr = userstack_start & ~31;
			 first < top; first += 32) {
			unsigned long p;
			char str[sizeof(" 123456789abcdef") * 4 + 1];

			memset(str, ' ', sizeof(str));
			str[sizeof(str) - 1] = '\0';

			for (p = first, i = 0; i < 4 && p < top;
				 i++, p += 8, addr += 8) {
				if (p >= bottom && p < top &&
					addr >= userstack_start &&
					addr < userstack_end)
					sprintf(str + i * 17, " %016llx",
							*(u64 *)p);
			}

			pr_info("%04lx:%s\n", (addr - 32) & 0xffff, str);
		}
	}

	return 0;
}

static unsigned long PAR(unsigned long addr, int user)
{
	unsigned long ret;

	__asm__ __volatile__("cbz %2, 1f      \n\t"
						 "AT S1E0R, %1    \n\t"
						 "b 2f            \n\t"
						 "1:              \n\t"
						 "AT S1E1R, %1    \n\t"
						 "2:              \n\t"
						 "isb             \n\t"
						 "dsb sy          \n\t"
						 "MRS %0, PAR_EL1 \n\t"
						 "dsb sy          \n\t"
						 : "=r"(ret)
						 : "r"(addr), "r"(user));
	return ret;
}

static void dump_mem(const char *str, unsigned long bottom, unsigned long top,
					 unsigned long base, int user)
{
	unsigned long first, par;
	mm_segment_t fs;
	int i;

	/* shape the underflow and overflow */
	if ((base < PAGE_SIZE) && ((long)bottom < 0)) {
		bottom = 0;
	}
	if ((PAGE_ALIGN(base) == 0) && ((long)top >= 0)) {
		top = (unsigned long)(0 - 1);
	}
	if ((base < bottom) || (base > top) || (top - bottom) > PAGE_SIZE) {
		pr_info("%s 0x%08lx (NA) - par(0x%lx)\n", str, base,
				PAR(base, user));
		return;
	}

	par = PAR(base, user);
	if (par & 1) {
		pr_info("%s 0x%08lx (no mapping) - par(0x%lx)\n", str, base,
				par);
		return;
	}

	/* skip BUS */
	if ((par & 0xff000000) == 0x1F000000) {
		pr_info("%s 0x%08lx (BUS) - par(0x%lx)\n", str, base, par);
		return;
	}

	bottom = PAR(bottom, user) & 1 ? PAGE_ALIGN(bottom) : bottom;
	top = PAR(top, user) & 1 ? (PAGE_MASK & (top)) - 1 : top;

	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	if (!user) {
		fs = get_fs();
		set_fs(KERNEL_DS);
	}

	pr_info("%s 0x%08lx (0x%08lx to 0x%08lx)\n", str, base, bottom, top);

	if (is_compat_task()) {
		for (first = bottom & ~31; first < top; first += 32) {
			unsigned long p;

			char str[sizeof(" 12345678") * 8 + 1];

			memset(str, ' ', sizeof(str));
			str[sizeof(str) - 1] = '\0';

			for (p = first, i = 0; i < 8 && p < top; i++, p += 4) {
				if (p >= bottom && p < top) {
					unsigned long val;
					if (__get_user(val,
								   (unsigned long *)p) == 0)
						sprintf(str + i * 9, " %08lx",
								val);
					else
						sprintf(str + i * 9,
								" ????????");
				}
			}
			pr_info("%04lx:%s\n", first & 0xffff, str);
		}
	} else {
		for (first = bottom & ~31; first < top; first += 32) {
			unsigned long p;

			char str[sizeof(" 123456789abcdef") * 4 + 1];

			memset(str, ' ', sizeof(str));
			str[sizeof(str) - 1] = '\0';

			for (p = first, i = 0; i < 4 && p < top; i++, p += 8) {
				if (p >= bottom && p < top) {
					unsigned long val;
					if (__get_user(val,
								   (unsigned long *)p) == 0)
						sprintf(str + i * 17, " %016lx",
								val);
					else
						sprintf(str + i * 17,
								" ????????????????");
				}
			}
			pr_info("%04lx:%s\n", first & 0xffff, str);
		}
	}

	if (!user) {
		set_fs(fs);
	}
}

#define DUMP_REG_REFS(reg, user, range)                                        \
	dump_mem(#reg ": ", regs->reg - (range), regs->reg + (range),          \
		 regs->reg, user)

void mrdump_dump_regs(struct pt_regs *regs)
{
	int user = (user_mode(regs) != 0) ? 1 : 0;

	pr_info("arm64 regs: ");

	DUMP_REG_REFS(regs[0], user, 256);
	DUMP_REG_REFS(regs[1], user, 256);
	DUMP_REG_REFS(regs[2], user, 256);
	DUMP_REG_REFS(regs[3], user, 256);
	DUMP_REG_REFS(regs[4], user, 256);
	DUMP_REG_REFS(regs[5], user, 256);
	DUMP_REG_REFS(regs[6], user, 256);
	DUMP_REG_REFS(regs[7], user, 256);
	DUMP_REG_REFS(regs[8], user, 256);
	DUMP_REG_REFS(regs[9], user, 256);
	DUMP_REG_REFS(regs[10], user, 256);
	DUMP_REG_REFS(regs[11], user, 256);
	DUMP_REG_REFS(regs[12], user, 256);
	DUMP_REG_REFS(regs[13], user, 256);
	DUMP_REG_REFS(regs[14], user, 256);
	DUMP_REG_REFS(regs[15], user, 256);
	DUMP_REG_REFS(regs[16], user, 256);
	DUMP_REG_REFS(regs[17], user, 256);
	DUMP_REG_REFS(regs[18], user, 256);
	DUMP_REG_REFS(regs[19], user, 256);
	DUMP_REG_REFS(regs[20], user, 256);
	DUMP_REG_REFS(regs[21], user, 256);
	DUMP_REG_REFS(regs[22], user, 256);
	DUMP_REG_REFS(regs[23], user, 256);
	DUMP_REG_REFS(regs[24], user, 256);
	DUMP_REG_REFS(regs[25], user, 256);
	DUMP_REG_REFS(regs[26], user, 256);
	DUMP_REG_REFS(regs[27], user, 256);
	DUMP_REG_REFS(regs[28], user, 256);
	DUMP_REG_REFS(regs[29], user, 256);
	DUMP_REG_REFS(regs[30], user, 256);

	DUMP_REG_REFS(sp, user, 512);
	DUMP_REG_REFS(pc, user, 256);
}