/*
 * Copyright (C) 2021 Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __MRDUMP_H__
#define __MRDUMP_H__

#include <asm/ptrace.h>

#define MRDUMP_MINI_NR_SECTION		40
#define MRDUMP_MINI_SECTION_SIZE	(4 * 1024)

#define MRDUMP_FILE_PATH_LEN		60
#define MRDUMP_DEFAULT_FILE_PATH	"/minidump"
#define MRDUMP_VERSION			"1.0"

/* mrdump arch macros */
#ifdef __aarch64__
#define reg_pc pc
#define reg_lr regs[30]
#define reg_sp sp
#define reg_fp regs[29]
#else
#define reg_pc ARM_pc
#define reg_lr ARM_lr
#define reg_sp ARM_sp
#define reg_ip ARM_ip
#define reg_fp ARM_fp
#endif

#define MaxStackSize 8100

/* mrdump description */
struct user_thread_info {
	struct task_struct *tsk;
	struct pt_regs regs;
	int stack_length;
	unsigned char *stack;
	int maps_length;
	unsigned char *maps;
};

struct mrdump_desc {
	char *version;
	union {
		struct file *file;
		unsigned long offset;
	};

	uint32_t reboot_mode;
	char msg[32];
	char backtrace[512];
	struct pt_regs cpu_regs[CONFIG_NR_CPUS];
	struct user_thread_info user_thread;

	struct mrdump_platform_ops *plat_ops;
};

/* mrdump arch operations */
void mrdump_setup_regs(struct pt_regs *newregs, struct pt_regs *oldregs);
void mrdump_setup_cpu_context(struct pt_regs *regs, struct task_struct *tsk);
int mrdump_dump_user_stack(struct user_thread_info *user_thread);
void mrdump_dump_regs(struct pt_regs *regs);

/* mrdump platform operations */
struct mrdump_platform_ops {
	void (*enable)(bool enabled);
	void (*reboot)(void);
};

int mrdump_platform_init(struct mrdump_desc *mrdump_desc);

/* mrdump data operations */
struct mrdump_data_ops {
	int (*open)(const char *path, int flags, int rights);
	int (*close)(void);
	int (*read)(loff_t off, size_t len, size_t *buf);
	int (*write)(loff_t off, size_t len, const size_t *buf);
	int (*erase)(void);
	int (*sync)(void);
};

int mrdump_data_open(const char *path, int flags, int rights);
int mrdump_data_close(void);
int mrdump_data_read(loff_t off, size_t len, void *buf, int encrypt);
int mrdump_data_write(loff_t off, size_t len, void *buf, int encrypt);
int mrdump_data_erase(void);
int mrdump_data_sync(void);

void register_mrdump_data_ops(struct mrdump_data_ops *ops);

#ifdef CONFIG_MTD
void mrdump_mtd_init(void);
#endif

/* mrdump mini */
struct reserved_mem;
typedef int (*mrdump_write)(loff_t off, size_t len, void *buf, int encrypt);

int mrdump_mini_create_dump(mrdump_write write, loff_t offset);
void mrdump_mini_per_cpu(struct pt_regs *regs, int cpu);
void mrdump_mini_main_cpu(struct pt_regs *regs);
void mrdump_mini_add_tasks(void);
void mrdump_mini_add_data(void);
void mrdump_mini_add_loads(void);
void mrdump_mini_done(void);
bool mrdump_mini_check_done(void);
int mrdump_mini_init(void);
void mrdump_mini_exit(void);
int mrdump_mini_reserve_memory(struct reserved_mem *rmem);

#endif