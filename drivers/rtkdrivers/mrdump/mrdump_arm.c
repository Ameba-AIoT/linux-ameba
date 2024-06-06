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
#include <asm/stacktrace.h>
#include <asm/bug.h>

#include "mrdump.h"

void mrdump_setup_regs(struct pt_regs *newregs, struct pt_regs *oldregs)
{
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
	} else {
		__asm__ __volatile__("stmia	%[regs_base], {r0-r12}\n\t"
							 "mov	%[_ARM_sp], sp\n\t"
							 "str	lr, %[_ARM_lr]\n\t"
							 "adr	%[_ARM_pc], 1f\n\t"
							 "mrs	%[_ARM_cpsr], cpsr\n\t"
							 "1:"
							 : [_ARM_pc] "=r"(newregs->ARM_pc),
							 [_ARM_cpsr] "=r"(newregs->ARM_cpsr),
							 [_ARM_sp] "=r"(newregs->ARM_sp),
							 [_ARM_lr] "=o"(newregs->ARM_lr)
							 : [regs_base] "r"(&newregs->ARM_r0)
							 : "memory");
	}
}

void mrdump_setup_cpu_context(struct pt_regs *regs, struct task_struct *tsk)
{
	struct cpu_context_save *cpu_context;

	cpu_context = &task_thread_info(tsk)->cpu_context;

	regs->ARM_r4 = cpu_context->r4;
	regs->ARM_r5 = cpu_context->r5;
	regs->ARM_r6 = cpu_context->r6;
	regs->ARM_r7 = cpu_context->r7;
	regs->ARM_r8 = cpu_context->r8;
	regs->ARM_r9 = cpu_context->r9;
	regs->ARM_r10 = cpu_context->sl;
	regs->ARM_fp = cpu_context->fp;
	regs->ARM_sp = cpu_context->sp;
	regs->ARM_pc = cpu_context->pc;
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

	pr_info("K32+U32 pc/lr/sp 0x%08lx/0x%08lx/0x%08lx\n", regs->ARM_pc,
			regs->ARM_lr, regs->ARM_sp);
	userstack_start = (unsigned long)regs->ARM_sp;
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
	pr_info("copy_from_user ret(0x%08x), len: 0x%lx\n", ret, length);
	if (ret) {
		return ret;
	}

	pr_info("dump user stack range (0x%08lx to 0x%08lx)\n", userstack_start,
			userstack_end);
	bottom = (unsigned long)user_thread->stack;
	top = bottom + length;
	for (first = bottom & ~31, addr = userstack_start & ~31; first < top;
		 first += 32) {
		unsigned long p;
		char str[sizeof(" 12345678") * 8 + 1];

		memset(str, ' ', sizeof(str));
		str[sizeof(str) - 1] = '\0';

		for (p = first, i = 0; i < 8 && p < top;
			 i++, p += 4, addr += 4) {
			if (p >= bottom && p < top && addr >= userstack_start &&
				addr < userstack_end) {
				sprintf(str + i * 9, " %08x", *(u32 *)p);
			}
		}

		pr_info("%04lx:%s\n", (addr - 32) & 0xffff, str);
	}

	return 0;
}

static unsigned long PAR(unsigned long addr, int user)
{
	unsigned long ret;

	__asm__ __volatile__(
		"cmp  %2, #0 \n\t"
		"MCReq p15, 0, %1, c7, c8, 0  @; ATS1CPR operation \n\t"
		"MCRne p15, 0, %1, c7, c8, 2  @; ATS1CUR operation \n\t"
		"isb \n\t"
		"dsb \n\t"
		"MRC p15, 0, %0, c7, c4, 0    @PAR \n\t"
		"dsb \n\t"
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

	for (first = bottom & ~31; first < top; first += 32) {
		unsigned long p;

		char str[sizeof(" 12345678") * 8 + 1];

		memset(str, ' ', sizeof(str));
		str[sizeof(str) - 1] = '\0';

		for (p = first, i = 0; i < 8 && p < top; i++, p += 4) {
			if (p >= bottom && p < top) {
				unsigned long val;
				if (__get_user(val, (unsigned long *)p) == 0) {
					sprintf(str + i * 9, " %08lx", val);
				} else {
					sprintf(str + i * 9, " ????????");
				}
			}
		}
		pr_info("%04lx:%s\n", first & 0xffff, str);
	}

	if (!user) {
		set_fs(fs);
	}
}

#define DUMP_REG_REFS(reg, user, range)                                        \
	dump_mem(#reg ": ", regs->ARM_##reg - (range),                         \
		 regs->ARM_##reg + (range), regs->ARM_##reg, user)

void mrdump_dump_regs(struct pt_regs *regs)
{
	int user = (user_mode(regs) != 0) ? 1 : 0;

	pr_info("arm regs: ");

	DUMP_REG_REFS(r0, user, 256);
	DUMP_REG_REFS(r1, user, 256);
	DUMP_REG_REFS(r2, user, 256);
	DUMP_REG_REFS(r3, user, 256);
	DUMP_REG_REFS(r4, user, 256);
	DUMP_REG_REFS(r5, user, 256);
	DUMP_REG_REFS(r6, user, 256);
	DUMP_REG_REFS(r7, user, 256);
	DUMP_REG_REFS(r8, user, 256);
	DUMP_REG_REFS(r9, user, 256);
	DUMP_REG_REFS(r10, user, 256);
	DUMP_REG_REFS(fp, user, 256);
	DUMP_REG_REFS(ip, user, 256);
	DUMP_REG_REFS(sp, user, 512);
	DUMP_REG_REFS(lr, user, 256);
	DUMP_REG_REFS(pc, user, 256);
}