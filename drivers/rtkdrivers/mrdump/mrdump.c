/*
 * Copyright (C) 2021 Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define pr_fmt(fmt)	"mrdump: " fmt

#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/kdebug.h>
#include <linux/crash_core.h>

#include <asm/traps.h>
#include <asm/cacheflush.h>

#include "mrdump.h"

#define KEXEC_CORE_NOTE_NAME		"CORE"

enum output_type {
	MRDUMP_OUTPUT_NULL = 0,
	MRDUMP_OUTPUT_FILE,
	MRDUMP_OUTPUT_MTD,
	MRDUMP_OUTPUT_TYPE_MAX,
};

enum mrdump_reboot_mode {
	REBOOT_NORMAL = 0,
	REBOOT_OOPS,
	REBOOT_PANIC,
	REBOOT_NESTED,
	REBOOT_WDT,
	REBOOT_MANUAL,
	REBOOT_MODE_MAX,
};

enum dump_level {
	DUMP_LEVEL_0 = 0,
	DUMP_LEVEL_1,
	DUMP_LEVEL_2,
	DUMP_LEVEL_MAX,
};

struct mrdump_desc *mrdump_desc;

/* Definition variables of sys/modules/mrdump/parameter/xxx */
static char file_dump[MRDUMP_FILE_PATH_LEN] = CONFIG_MRDUMP_MINI_FILE_PATH;
static char *mrdump_file_path = file_dump;
static bool mrdump_enable = 1;
static int mrdump_output_type = MRDUMP_OUTPUT_FILE;
static int mrdump_irq_affinity = CONFIG_MRDUMP_IRQ_AFFINITY;
static int mrdump_dump_level = DUMP_LEVEL_0;

extern struct mrdump_data_ops mrdump_file_ops;
extern struct mrdump_data_ops mrdump_mtd_ops;

/* Per cpu memory for storing cpu states in case of system crash. */
note_buf_t __percpu *crash_notes;

Elf_Word *append_elf_note(Elf_Word *buf, char *name, unsigned int type,
						  void *data, size_t data_len)
{
	struct elf_note *note = (struct elf_note *)buf;

	note->n_namesz = strlen(name) + 1;
	note->n_descsz = data_len;
	note->n_type = type;
	buf += DIV_ROUND_UP(sizeof(*note), sizeof(Elf_Word));
	memcpy(buf, name, note->n_namesz);
	buf += DIV_ROUND_UP(note->n_namesz, sizeof(Elf_Word));
	memcpy(buf, data, data_len);
	buf += DIV_ROUND_UP(data_len, sizeof(Elf_Word));

	return buf;
}

void final_note(Elf_Word *buf)
{
	memset(buf, 0, sizeof(struct elf_note));
}

static void crash_save_cpu(struct pt_regs *regs, int cpu)
{
	struct elf_prstatus prstatus;
	u32 *buf;

	if ((cpu < 0) || (cpu >= nr_cpu_ids)) {
		return;
	}

	buf = (u32 *)per_cpu_ptr(crash_notes, cpu);
	if (!buf) {
		return;
	}
	memset(&prstatus, 0, sizeof(prstatus));
	prstatus.pr_pid = current->pid;
	elf_core_copy_kernel_regs((elf_gregset_t *)&prstatus.pr_reg, regs);
	buf = append_elf_note(buf, KEXEC_CORE_NOTE_NAME, NT_PRSTATUS, &prstatus,
						  sizeof(prstatus));
	final_note(buf);
}

static void crash_save_current_tsk(void)
{
	int i;
	struct stack_trace trace;
	unsigned long stack_entries[16];
	struct task_struct *tsk = current;

	/* Grab kernel task stack trace */
	trace.nr_entries = 0;
	trace.max_entries = ARRAY_SIZE(stack_entries);
	trace.entries = stack_entries;
	trace.skip = 1;
	save_stack_trace_tsk(tsk, &trace);

	for (i = 0; i < trace.nr_entries; i++) {
		int off = strlen(mrdump_desc->backtrace);
		int plen = sizeof(mrdump_desc->backtrace) - off;

		if (plen > 16) {
			snprintf(mrdump_desc->backtrace + off, plen,
					 "[<%p>] %pS\n", (void *)stack_entries[i],
					 (void *)stack_entries[i]);
		}
	}
}

static int mrdump_dump_thread_native_info(void)
{
	struct user_thread_info *user_thread;
	struct task_struct *tsk;
	struct pt_regs *regs;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	struct file *file;
	int flags, map_count = 0;

	tsk = current;
	regs = task_pt_regs(tsk);

	if (!user_mode(regs)) {
		return 0;
	}

	user_thread = &mrdump_desc->user_thread;
	user_thread->tsk = tsk;
	memcpy(&user_thread->regs, regs, sizeof(struct pt_regs));

	if (tsk->mm == NULL) {
		return -EINVAL;
	}

	pr_info("thread native\n");

	vma = tsk->mm->mmap;
	while (vma && (map_count < tsk->mm->map_count)) {
		file = vma->vm_file;
		flags = vma->vm_flags;

		if (file)
			printk(KERN_INFO "%08lx-%08lx %c%c%c%c    %s\n",
				   vma->vm_start, vma->vm_end,
				   flags & VM_READ ? 'r' : '-',
				   flags & VM_WRITE ? 'w' : '-',
				   flags & VM_EXEC ? 'x' : '-',
				   flags & VM_MAYSHARE ? 's' : 'p',
				   (unsigned char *)(file->f_path.dentry->d_iname));
		else {
			const char *name = arch_vma_name(vma);

			mm = vma->vm_mm;
			if (!name) {
				if (mm) {
					if (vma->vm_start <= mm->start_brk &&
						vma->vm_end >= mm->brk) {
						name = "[heap]";
					} else if (vma->vm_start <=
							   mm->start_stack &&
							   vma->vm_end >= mm->start_stack) {
						name = "[stack]";
					}
				} else {
					name = "[vdso]";
				}
			}
			printk(KERN_INFO "%08lx-%08lx %c%c%c%c    %s\n",
				   vma->vm_start, vma->vm_end,
				   flags & VM_READ ? 'r' : '-',
				   flags & VM_WRITE ? 'w' : '-',
				   flags & VM_EXEC ? 'x' : '-',
				   flags & VM_MAYSHARE ? 's' : 'p', name);
		}

		vma = vma->vm_next;
		map_count++;
	}

	return mrdump_dump_user_stack(&mrdump_desc->user_thread);
}

static void mrdump_dump_crash_record(void)
{
	pr_info("system memory record (online cpus: %x, fault cpu id: %x)\n"
			"High Memory: 0x%lx\nPage Table : 0x%lx\n"
			"Phys Offset: 0x%lx\nPage Offset: 0x%lx\n"
			"Vmalloc    : 0x%lx-0x%lx\n"
			"Modules    : 0x%lx-0x%lx\n",
			num_online_cpus(), smp_processor_id(), (uintptr_t)high_memory,
			(uintptr_t)&swapper_pg_dir, (uintptr_t)PHYS_OFFSET,
			(uintptr_t)PAGE_OFFSET, (uintptr_t)VMALLOC_START,
			(uintptr_t)VMALLOC_END, (uintptr_t)MODULES_VADDR,
			(uintptr_t)MODULES_END);
	pr_info("backtrace\n%s", mrdump_desc->backtrace);
}

#if defined(CONFIG_SMP)
/* Generic IPI support */
static atomic_t waiting_for_crash_ipi;

static void mrdump_stop_noncrash_cpu(void *unused)
{
	int cpu = smp_processor_id();

	set_cpu_online(cpu, false);
	atomic_dec(&waiting_for_crash_ipi);

	local_irq_disable();

	while (1) {
		cpu_relax();
		wfe();
	}
}

static void mrdump_smp_send_stop(void)
{
	unsigned long timeout;

	atomic_set(&waiting_for_crash_ipi, num_online_cpus() - 1);
	smp_call_function(mrdump_stop_noncrash_cpu, NULL, false);

	/* Wait at most a second for the other cpus to stop */
	timeout = USEC_PER_SEC;
	while ((atomic_read(&waiting_for_crash_ipi) > 0) && timeout--) {
		udelay(1);
	}

	if (atomic_read(&waiting_for_crash_ipi) > 0) {
		pr_warn("Non-crashing CPUs did not react to IPI\n");
	}
}

static void mrdump_smp_save_noncrash(void *unused)
{
	struct pt_regs *cpu_regs;
	int cpu = smp_processor_id();

	cpu_regs = &mrdump_desc->cpu_regs[cpu];

	mrdump_setup_regs(cpu_regs, get_irq_regs());
	mrdump_mini_per_cpu(cpu_regs, cpu);

	crash_save_cpu(cpu_regs, cpu);
}

static void mrdump_set_interrupt_affinity(void)
{
	unsigned int i = 0;
	struct irq_desc *desc = NULL;

	if (!mrdump_irq_affinity) {
		return;
	}

	for_each_irq_desc(i, desc) {
		struct irq_chip *chip;

		if (desc->irq_data.hwirq != mrdump_irq_affinity) {
			continue;
		}

		chip = irq_desc_get_chip(desc);
		if (!chip || !irqd_can_balance(&desc->irq_data)) {
			continue;
		}

		if (chip->irq_eoi && irqd_irq_inprogress(&desc->irq_data)) {
			chip->irq_eoi(&desc->irq_data);
		}

		if (chip->irq_set_affinity)
			chip->irq_set_affinity(&desc->irq_data, cpu_online_mask,
								   false);

		break;
	}

	local_irq_enable();
}

extern int tick_do_timer_cpu;
static void mrdump_tick_handover(void)
{
	unsigned long flags;

	local_irq_save(flags);
	tick_do_timer_cpu = smp_processor_id();
	local_irq_restore(flags);
}
#endif

static void mrdump_create_dump(void)
{
	int ret;

	if (!strcmp(mrdump_file_path, "")) {
		mrdump_file_path = MRDUMP_DEFAULT_FILE_PATH;
	}

	pr_info("start mini dump to %s\n", mrdump_file_path);
	ret = mrdump_data_open(mrdump_file_path, O_CREAT | O_RDWR | O_NONBLOCK,
						   0755);
	if (ret) {
		pr_err("open failed, err: %d\n", ret);
		return;
	}

	mrdump_data_erase();
	mrdump_mini_create_dump(mrdump_data_write, 0);
	mrdump_data_sync();
	mrdump_data_close();
	mrdump_mini_done();
}

static void mrdump_smp_save_main(struct pt_regs *regs)
{
	struct pt_regs *cpu_regs;
	int cpu = smp_processor_id();

	cpu_regs = &mrdump_desc->cpu_regs[cpu];
	mrdump_setup_regs(cpu_regs, regs);
	mrdump_mini_main_cpu(cpu_regs);

	switch (mrdump_dump_level) {
	case DUMP_LEVEL_2:
		mrdump_mini_add_data();
	case DUMP_LEVEL_1:
		mrdump_mini_add_tasks();
	case DUMP_LEVEL_0:
	default:
		mrdump_mini_add_loads();
		break;
	}

	crash_save_cpu(cpu_regs, cpu);
	crash_save_current_tsk();
}

static void mrdump_dump_info(void)
{
	int cpu = smp_processor_id();

	pr_info("caused by %s\n", mrdump_desc->msg);

	mrdump_dump_regs(&mrdump_desc->cpu_regs[cpu]);
	mrdump_dump_crash_record();
	mrdump_dump_thread_native_info();
}

static void mrdump_reboot(int reboot_mode, const char *msg, ...)
{
	va_list ap;

	if (!mrdump_enable) {
		pr_info("disabled\n");
		return;
	}

	va_start(ap, msg);
	vsnprintf(mrdump_desc->msg, ARRAY_SIZE(mrdump_desc->msg), msg, ap);
	va_end(ap);

	mrdump_desc->reboot_mode = reboot_mode;

	if (mrdump_desc->reboot_mode == REBOOT_MANUAL) {
		preempt_disable_notrace();
		mrdump_smp_save_main(NULL);
	}

#if defined(CONFIG_SMP)
	mrdump_smp_send_stop();
	mrdump_tick_handover();
	mrdump_set_interrupt_affinity();
#endif

	mrdump_dump_info();
	mrdump_create_dump();

	local_irq_disable();

	if (mrdump_desc->plat_ops != NULL) {
		mrdump_desc->plat_ops->reboot();
	}

	while (1) {
		cpu_relax();
		wfe();
	}
}

static int mrdump_panic(struct notifier_block *this, unsigned long event,
						void *ptr)
{
	if (!mrdump_enable) {
		pr_err("disabled\n");
		return NOTIFY_DONE;
	}

	if (mrdump_desc->reboot_mode) {
		pr_err("nested exception, reboot mode: %d\n",
			   mrdump_desc->reboot_mode);
		mrdump_desc->reboot_mode = REBOOT_NESTED;
		while (1) {
			cpu_relax();
			wfe();
		}
	}

	if (test_taint(TAINT_DIE)) {
		mrdump_reboot(REBOOT_OOPS, "kernel Oops");
	} else {
		mrdump_reboot(REBOOT_PANIC, "kernel panic");
	}

	return NOTIFY_DONE;
}

static int mrdump_die(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	struct die_args *args = ptr;

	if (mrdump_desc->reboot_mode) {
		pr_err("nested exception, reboot mode: %d\n",
			   mrdump_desc->reboot_mode);
		mrdump_desc->reboot_mode = REBOOT_NESTED;
		while (1) {
			cpu_relax();
			wfe();
		}
	}

#if defined(CONFIG_SMP)
	smp_call_function(mrdump_smp_save_noncrash, NULL, true);
#endif

	mrdump_smp_save_main(args->regs);

	return NOTIFY_DONE;
}

static struct notifier_block mrdump_panic_blk = {
	.notifier_call = mrdump_panic,
};

static struct notifier_block die_blk = {
	.notifier_call = mrdump_die,
};

#if defined(CONFIG_SYSFS)
static ssize_t mrdump_status_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *page)
{
	return 0;
}

static ssize_t mrdump_version_show(struct kobject *kobj,
								   struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "MRDUMP-%s\n", mrdump_desc->version);
}

static ssize_t manual_dump_show(struct kobject *kobj,
								struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "Trigger format \"manualdump:HelloWorld\"\n");
}

static ssize_t manual_dump_store(struct kobject *kobj,
								 struct kobj_attribute *attr, const char *buf,
								 size_t count)
{
	if (strncmp(buf, "manualdump:", 11) == 0) {
		mrdump_reboot(REBOOT_MANUAL, buf + 11);
	}
	return count;
}

static struct kobj_attribute mrdump_status_attribute =
	__ATTR(status, 0400, mrdump_status_show, NULL);

static struct kobj_attribute mrdump_version_attribute =
	__ATTR(version, 0600, mrdump_version_show, NULL);

static struct kobj_attribute manual_dump_attribute =
	__ATTR(manualdump, 0600, manual_dump_show, manual_dump_store);

static struct attribute *attrs[] = {
	&mrdump_status_attribute.attr,
	&mrdump_version_attribute.attr,
	&manual_dump_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};
#endif

int __init mrdump_init(void)
{
#if defined(CONFIG_SYSFS)
	struct kobject *kobj;
#endif
	int ret;

	ret = mrdump_mini_init();
	if (ret) {
		mrdump_enable = 0;
		pr_err("mrdump mini init failed");
		return ret;
	}

	ret = -ENOMEM;
	mrdump_desc = kzalloc(sizeof(struct mrdump_desc), GFP_KERNEL);
	if (mrdump_desc == NULL) {
		pr_err("memory allocation for mrdump_desc failed\n");
		goto out_mini;
	}

	/* Allocate memory for saving cpu registers. */
	crash_notes = alloc_percpu(note_buf_t);
	if (!crash_notes) {
		pr_warn("memory allocation for crash_notes failed\n");
		goto out_desc;
	}

	/* Register notifier */
	atomic_notifier_chain_register(&panic_notifier_list, &mrdump_panic_blk);
	register_die_notifier(&die_blk);

#if defined(CONFIG_SYSFS)
	kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (kobj) {
		if (sysfs_create_group(kobj, &attr_group)) {
			pr_err("sysfs create sysfs failed\n");
			goto out_notes;
		}
	} else {
		pr_err("cannot find module %s object\n", KBUILD_MODNAME);
		ret = -EINVAL;
		goto out_notes;
	}
#endif

#if defined(CONFIG_MTD)
	mrdump_mtd_init();
#endif

	/* Default dump to file */
	mrdump_output_type = MRDUMP_OUTPUT_FILE;
	register_mrdump_data_ops(&mrdump_file_ops);

	mrdump_desc->version = MRDUMP_VERSION;

	if (mrdump_platform_init(mrdump_desc)) {
		goto out_notes;
	}
	if (mrdump_desc->plat_ops != NULL) {
		mrdump_desc->plat_ops->enable(mrdump_enable);
	}

	pr_info("initialize done\n");
	return ret;

out_notes:
	free_percpu(crash_notes);
out_desc:
	kfree(mrdump_desc);
out_mini:
	mrdump_mini_exit();
	mrdump_enable = 0;
	return ret;
}

/* sys/modules/mrdump/parameter/xxx */
static int param_get_mrdump_file_path(char *buffer,
									  const struct kernel_param *kp)
{
	if (mrdump_output_type != MRDUMP_OUTPUT_FILE) {
		return 0;
	}

	scnprintf(buffer, MRDUMP_FILE_PATH_LEN + 1, "%s\n", mrdump_file_path);
	return strlen(mrdump_file_path);
}

static int param_set_mrdump_output_type(const char *val,
										const struct kernel_param *kp)
{
	char strval[16], *strp;

	strlcpy(strval, val, sizeof(strval));
	strp = strstrip(strval);

	if (strcmp(strp, "file") == 0) {
		mrdump_output_type = MRDUMP_OUTPUT_FILE;
		register_mrdump_data_ops(&mrdump_file_ops);
#if defined(CONFIG_MTD)
	} else if (strcmp(strp, "mtd") == 0) {
		mrdump_output_type = MRDUMP_OUTPUT_MTD;
		register_mrdump_data_ops(&mrdump_mtd_ops);
#endif
	} else {
		mrdump_output_type = MRDUMP_OUTPUT_NULL;
		register_mrdump_data_ops(NULL);
	}

	*(int *)kp->arg = mrdump_output_type;
	return 0;
}

static int param_get_mrdump_output_type(char *buffer,
										const struct kernel_param *kp)
{
	char *type;

	switch (mrdump_output_type) {
	case MRDUMP_OUTPUT_NULL:
		type = "null\n";
		break;
	case MRDUMP_OUTPUT_FILE:
		type = "file\n";
		break;
#if defined(CONFIG_MTD)
	case MRDUMP_OUTPUT_MTD:
		type = "mtd\n";
		break;
#endif
	default:
		type = "none(unknown)\n";
		break;
	}

	strcpy(buffer, type);
	return strlen(type);
}

static int param_set_mrdump_enable(const char *val,
								   const struct kernel_param *kp)
{
	int retval = 0;

	retval = param_set_bool(val, kp);

	if (retval == 0) {
		if (mrdump_desc->plat_ops != NULL) {
			mrdump_desc->plat_ops->enable(mrdump_enable);
		}
	}

	return retval;
}

#define MRDUMP_PARAM(_name, _type, _set, _get)                                 \
	struct kernel_param_ops param_ops_mrdump_##_name = {                   \
		.set = _set,                                                   \
		.get = _get,                                                   \
	};                                                                     \
	param_check_##_type(_name, &mrdump_##_name);                           \
	module_param_cb(_name, &param_ops_mrdump_##_name, &mrdump_##_name,     \
			S_IRUGO | S_IWUSR);

MRDUMP_PARAM(file_path, charp, param_set_charp, param_get_mrdump_file_path);
MRDUMP_PARAM(output_type, int, param_set_mrdump_output_type,
			 param_get_mrdump_output_type);
MRDUMP_PARAM(enable, bool, param_set_mrdump_enable, param_get_bool);
MRDUMP_PARAM(irq_affinity, int, param_set_int, param_get_int);
MRDUMP_PARAM(dump_level, int, param_set_int, param_get_int);

module_init(mrdump_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MRDUMP module");
MODULE_AUTHOR("Realtek CTC software.");