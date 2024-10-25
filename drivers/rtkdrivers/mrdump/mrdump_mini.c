/*
 * Copyright (C) 2021 Realtek Semiconductor Corp.	All rights reserved.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define pr_fmt(fmt)	"mrdump mini: " fmt

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/memblock.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/kdebug.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/stacktrace.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <asm/memory.h>
#include <asm/ptrace.h>
#include <asm/pgtable.h>
#include <asm-generic/percpu.h>
#include <asm-generic/sections.h>
#include <asm/page.h>
#include "../../../kernel/sched/sched.h"

#include "mrdump.h"

#define MRDUMP_MINI_HEADER_SIZE                                                \
	ALIGN(sizeof(struct mrdump_mini_elf_header), PAGE_SIZE)
#define MRDUMP_MINI_DATA_SIZE                                                  \
	(MRDUMP_MINI_NR_SECTION * MRDUMP_MINI_SECTION_SIZE)
#define MRDUMP_MINI_BUF_SIZE (MRDUMP_MINI_HEADER_SIZE + MRDUMP_MINI_DATA_SIZE)

#define NOTE_NAME_SHORT		12
#define NOTE_NAME_LONG		20

#define virt_addr_is_valid(kaddr)                                              \
	(virt_addr_valid(kaddr) && pfn_valid(__pa(kaddr) >> PAGE_SHIFT))

struct mrdump_mini_elf_psinfo {
	struct elf_note note;
	char name[NOTE_NAME_SHORT];
	struct elf_prpsinfo data;
};

struct mrdump_mini_elf_prstatus {
	struct elf_note note;
	char name[NOTE_NAME_SHORT];
	struct elf_prstatus data;
};

struct mrdump_mini_elf_header {
	struct elfhdr ehdr;
	struct elf_phdr phdrs[MRDUMP_MINI_NR_SECTION];
	struct mrdump_mini_elf_psinfo psinfo;
	struct mrdump_mini_elf_prstatus prstatus[CONFIG_MRDUMP_MINI_TASKS_MAX];
};

static struct mrdump_mini_elf_header *mrdump_mini_ehdr;

/* copy from fs/binfmt_elf.c */
static void fill_elf_header(struct elfhdr *elf, int segs)
{
	memset(elf, 0, sizeof(*elf));

	memcpy(elf->e_ident, ELFMAG, SELFMAG);
	elf->e_ident[EI_CLASS] = ELF_CLASS;
	elf->e_ident[EI_DATA] = ELF_DATA;
	elf->e_ident[EI_VERSION] = EV_CURRENT;
	elf->e_ident[EI_OSABI] = ELF_OSABI;

	elf->e_type = ET_CORE;
	elf->e_machine = ELF_ARCH;
	elf->e_version = EV_CURRENT;
	elf->e_phoff = sizeof(struct elfhdr);
#ifndef ELF_CORE_EFLAGS
#define ELF_CORE_EFLAGS	0
#endif
	elf->e_flags = ELF_CORE_EFLAGS;
	elf->e_ehsize = sizeof(struct elfhdr);
	elf->e_phentsize = sizeof(struct elf_phdr);
	elf->e_phnum = segs;
}

static void fill_elf_note_phdr(struct elf_phdr *phdr, int sz, loff_t offset)
{
	phdr->p_type = PT_NOTE;
	phdr->p_offset = offset;
	phdr->p_vaddr = 0;
	phdr->p_paddr = 0;
	phdr->p_filesz = sz;
	phdr->p_memsz = 0;
	phdr->p_flags = 0;
	phdr->p_align = 0;
}

static void fill_elf_load_phdr(struct elf_phdr *phdr, int sz,
							   unsigned long vaddr, unsigned long paddr)
{
	phdr->p_type = PT_LOAD;
	phdr->p_vaddr = vaddr;
	phdr->p_paddr = paddr;
	phdr->p_filesz = sz;
	phdr->p_memsz = sz;
	phdr->p_flags = 0;
	phdr->p_align = 0;
}

static noinline void fill_note(struct elf_note *note, const char *name,
							   int type, unsigned int sz, unsigned int namesz)
{
	char *n_name = (char *)note + sizeof(struct elf_note);

	note->n_namesz = namesz;
	note->n_type = type;
	note->n_descsz = sz;
	strncpy(n_name, name, note->n_namesz);
}

static void fill_note_S(struct elf_note *note, const char *name, int type,
						unsigned int sz)
{
	fill_note(note, name, type, sz, NOTE_NAME_SHORT);
}

/*
 * fill up all the fields in prstatus from the given task struct, except
 * registers which need to be filled up separately.
 */
static void fill_prstatus(struct mrdump_mini_elf_prstatus *mrdump_prstatus,
						  struct pt_regs *regs, struct task_struct *p)
{
	struct elf_prstatus *prstatus = &mrdump_prstatus->data;
	struct elf_note *note = &mrdump_prstatus->note;

	if (p) {
		elf_core_copy_regs(&prstatus->pr_reg, regs);

		/* give a -1 to swapper process, gdb may analyse wrong with 0 */
		prstatus->pr_pid = p->pid ? p->pid : -1;
		prstatus->pr_ppid = p->real_parent->pid;

		fill_note_S(note, p->comm, NT_PRSTATUS,
					sizeof(struct elf_prstatus));
	} else
		fill_note_S(note, "NA", NT_PRSTATUS,
					sizeof(struct elf_prstatus));
}

static int fill_psinfo(struct mrdump_mini_elf_psinfo *mrdump_psinfo)
{
	struct elf_prpsinfo *psinfo = &mrdump_psinfo->data;
	struct elf_note *note = &mrdump_psinfo->note;
	unsigned int i = 0;

	fill_note_S(note, "vmlinux", NT_PRPSINFO, sizeof(struct elf_prpsinfo));

	strncpy(psinfo->pr_psargs, saved_command_line, ELF_PRARGSZ - 1);
	for (i = 0; i < ELF_PRARGSZ - 1; i++)
		if (psinfo->pr_psargs[i] == 0) {
			psinfo->pr_psargs[i] = ' ';
		}
	psinfo->pr_psargs[ELF_PRARGSZ - 1] = 0;
	strncpy(psinfo->pr_fname, "vmlinux", sizeof(psinfo->pr_fname));

	return 0;
}

int kernel_addr_valid(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (addr < PAGE_OFFSET) {
		return 0;
	}

	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd)) {
		return 0;
	}
	pr_err("[%08lx] *pgd=%08llx", addr, (long long)pgd_val(*pgd));

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud)) {
		return 0;
	}
	pr_err("*pud=%08llx", (long long)pud_val(*pud));

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd)) {
		return 0;
	}
	pr_err("*pmd=%08llx", (long long)pmd_val(*pmd));

	pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte)) {
		return 0;
	}
	pr_err("*pte=%08llx", (long long)pte_val(*pte));

	return pfn_valid(pte_pfn(*pte));
}

void mrdump_mini_add_entry(unsigned long addr, unsigned long size)
{
	struct elf_phdr *phdr;
	struct vm_struct *vm;
	unsigned long laddr, haddr, lnew, hnew;
	unsigned long paddr = 0;
	int i = 0;

	if (addr < PAGE_OFFSET) {
		return;
	}

	hnew = ALIGN(addr + size, PAGE_SIZE);
	lnew = ALIGN_DOWN(addr, PAGE_SIZE);

	if (!virt_addr_is_valid(addr)) {
		vm = find_vm_area((void *)addr);
		if (!vm) {
			return;
		}

		lnew = max((unsigned long)vm->addr,
				   PAGE_ALIGN(addr) - PAGE_SIZE);
		hnew = PAGE_ALIGN(lnew + size);
		paddr = __pfn_to_phys(vmalloc_to_pfn((void *)lnew));
	} else {
		lnew = max(lnew, PAGE_OFFSET);
		hnew = min_t(unsigned long, hnew, (unsigned long)high_memory);
		paddr = __pa(lnew);
	}

	for (i = 0; i < MRDUMP_MINI_NR_SECTION; i++) {
		phdr = &mrdump_mini_ehdr->phdrs[i];
		if (phdr->p_type == PT_NULL) {
			break;
		}

		if (phdr->p_type != PT_LOAD) {
			continue;
		}

		laddr = phdr->p_vaddr;
		haddr = laddr + phdr->p_filesz;

		/* full overlap with exist */
		if (lnew >= laddr && hnew <= haddr) {
			return;
		}

		/* no overlap, new */
		if (lnew > haddr || hnew < laddr) {
			continue;
		}

		/* partial overlap with exist, joining */
		lnew = lnew < laddr ? lnew : laddr;
		hnew = hnew > haddr ? hnew : haddr;
		paddr = __pa(lnew);
		break;
	}

	if (i < MRDUMP_MINI_NR_SECTION) {
		fill_elf_load_phdr(phdr, hnew - lnew, lnew, paddr);
	}
}

static void mrdump_mini_add_tsk_ti(int cpu, struct pt_regs *regs, int main)
{
	struct task_struct *tsk = NULL;
	struct thread_info *ti = NULL;
	unsigned long *bottom = NULL;
	unsigned long *top = NULL;
	unsigned long *p = NULL;
	int i = 0;

	ti = current_thread_info();
	tsk = cpu_curr(cpu);

	if (!virt_addr_is_valid(ti) || !virt_addr_is_valid(tsk)) {
		return;
	}

	mrdump_mini_add_entry((unsigned long)tsk, sizeof(struct task_struct));
	mrdump_mini_add_entry((unsigned long)tsk->stack, THREAD_SIZE);

	pr_info("cpu[%d] tsk: %lx ti: %lx cpu_rq(%lx)\n", cpu, (uintptr_t)tsk,
			(uintptr_t)ti, (uintptr_t)cpu_rq(cpu));

	if (!main) {
		return;
	}

	for (i = 0; i < ELF_NGREG; i++)
		mrdump_mini_add_entry(((unsigned long *)regs)[i],
							  MRDUMP_MINI_SECTION_SIZE);

	top = (unsigned long *)((unsigned long)tsk->stack + THREAD_SIZE);
	bottom = (unsigned long *)regs->reg_sp;

	if (!virt_addr_is_valid(top) || !virt_addr_is_valid(bottom) ||
		bottom < end_of_stack(tsk) || bottom > top) {
		return;
	}

	for (p = (unsigned long *)ALIGN((unsigned long)bottom,
									sizeof(unsigned long));
		 p < top; p++) {
		if (!virt_addr_is_valid(*p)) {
			continue;
		}
		if (*p >= (unsigned long)end_of_stack(tsk) &&
			*p <= (unsigned long)top) {
			continue;
		}
		if (*p >= (unsigned long)_stext && *p <= (unsigned long)_etext) {
			continue;
		}

		mrdump_mini_add_entry(*p, MRDUMP_MINI_SECTION_SIZE);
	}
}

static int mrdump_mini_cpu_regs(struct pt_regs *regs, int cpu)
{
	if (mrdump_mini_ehdr == NULL) {
		mrdump_mini_init();
	}

	if (cpu >= NR_CPUS || mrdump_mini_ehdr == NULL) {
		return -EINVAL;
	}

	if (strncmp(mrdump_mini_ehdr->prstatus[cpu].name, "NA", 2)) {
		return -EBUSY;
	}

	fill_prstatus(&mrdump_mini_ehdr->prstatus[cpu], regs, current);

	return 0;
}

void mrdump_mini_per_cpu(struct pt_regs *regs, int cpu)
{
	mrdump_mini_cpu_regs(regs, cpu);
	mrdump_mini_add_tsk_ti(cpu, regs, 0);
}
EXPORT_SYMBOL(mrdump_mini_per_cpu);

void mrdump_mini_main_cpu(struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	if (!regs) {
		mrdump_setup_regs(regs, NULL);
	}

	mrdump_mini_cpu_regs(regs, cpu);
	mrdump_mini_add_tsk_ti(cpu, regs, 1);
}
EXPORT_SYMBOL(mrdump_mini_main_cpu);

void mrdump_mini_add_loads(void)
{
	unsigned int cpu = 0;

	if (mrdump_mini_ehdr == NULL) {
		mrdump_mini_init();
	}

#if defined(CONFIG_SMP) || defined(CONFIG_ARM64)
	mrdump_mini_add_entry((unsigned long)__per_cpu_offset,
						  MRDUMP_MINI_SECTION_SIZE);
#endif

	for (cpu = 0; cpu <= num_possible_cpus(); cpu++)
		mrdump_mini_add_entry((unsigned long)cpu_rq(cpu),
							  sizeof(struct rq));

	mrdump_mini_add_entry((unsigned long)&mem_map,
						  MRDUMP_MINI_SECTION_SIZE);
	mrdump_mini_add_entry((unsigned long)mem_map, MRDUMP_MINI_SECTION_SIZE);
}
EXPORT_SYMBOL(mrdump_mini_add_loads);

void mrdump_mini_add_tasks(void)
{
	struct task_struct *p = NULL;
	struct pt_regs regs = {};
	int idx = num_possible_cpus();

	for_each_process(p) {
		pr_info("id:%3d tsk: %lx state: %04lx comm: %s\n", p->pid,
				(uintptr_t)p, p->state, p->comm);
		if (idx >= CONFIG_MRDUMP_MINI_TASKS_MAX) {
			break;
		}
		if (p->state == TASK_RUNNING) {
			continue;
		}
		if (p->state & TASK_NOLOAD) {
			continue;
		}

		mrdump_setup_cpu_context(&regs, p);

		fill_prstatus(&mrdump_mini_ehdr->prstatus[idx++], &regs, p);

		mrdump_mini_add_entry((unsigned long)p,
							  sizeof(struct task_struct));
		mrdump_mini_add_entry((unsigned long)p->stack, THREAD_SIZE);
	}

	pr_info("total %d task are added\n", idx);
}
EXPORT_SYMBOL(mrdump_mini_add_tasks);

void mrdump_mini_add_data(void)
{
	unsigned long size = 0;

#ifdef CONFIG_SMP
	/* PERCPU_SECTION(L1_CACHE_BYTES) */
	size = __per_cpu_end - __per_cpu_load;
	mrdump_mini_add_entry((unsigned long)__per_cpu_load, size);
#endif

	/*
	 * _sdata = .;
	 * RW_DATA_SECTION(L1_CACHE_BYTES, PAGE_SIZE, THREAD_SIZE)
	 * _edata = .;
	 */
	size = _edata - _sdata;
	mrdump_mini_add_entry((unsigned long)_sdata, size);

	/* BSS_SECTION(0, 0, 0) */
	size = __bss_stop - __bss_start;
	mrdump_mini_add_entry((unsigned long)__bss_start, size);
}
EXPORT_SYMBOL(mrdump_mini_add_data);

static int mrdump_mini_write_loads(loff_t offset, mrdump_write write)
{
	struct elf_phdr *phdr;
	unsigned long start, size;
	int errno = 0;
	int i = 0;

	for (i = 0; i < MRDUMP_MINI_NR_SECTION; i++) {
		phdr = &mrdump_mini_ehdr->phdrs[i];
		if (phdr->p_type == PT_NULL) {
			break;
		}

		if (phdr->p_type == PT_LOAD) {
			/* mrdump_mini_dump_phdr(phdr, &pos); */
			start = phdr->p_vaddr;
			size = ALIGN(phdr->p_filesz, SZ_512);
			phdr->p_offset = offset;
			errno = write(offset, size, (void *)start, 0);
			offset += size;
			if (IS_ERR(ERR_PTR(errno))) {
				pr_err("write fail: %x\n", errno);
				return errno;
			}
		}
	}

	return i;
}

int mrdump_mini_create_dump(mrdump_write write, loff_t offset)
{
	loff_t size;
	int i = 0;
	int ret = 0;

	for (i = 0; i < CONFIG_MRDUMP_MINI_TASKS_MAX; i++) {
		if (!strncmp(mrdump_mini_ehdr->prstatus[i].name, "NA", 2)) {
			break;
		}
	}

	size = i * sizeof(struct mrdump_mini_elf_prstatus) +
		   sizeof(struct mrdump_mini_elf_psinfo);
	fill_elf_note_phdr(&mrdump_mini_ehdr->phdrs[0], size,
					   offsetof(struct mrdump_mini_elf_header, psinfo));

	size += sizeof(mrdump_mini_ehdr->ehdr);
	size += sizeof(mrdump_mini_ehdr->phdrs);
	size = ALIGN(size, SZ_512);

	ret = mrdump_mini_write_loads(size + offset, write);
	if (IS_ERR(ERR_PTR(ret))) {
		return ret;
	}

	fill_elf_header(&mrdump_mini_ehdr->ehdr, ret);
	write(offset, size, (void *)mrdump_mini_ehdr, 0);

	return MRDUMP_MINI_BUF_SIZE;
}
EXPORT_SYMBOL(mrdump_mini_create_dump);

void mrdump_mini_done(void)
{
	mrdump_mini_ehdr->ehdr.e_ident[0] = 0;
	pr_info("write done\n");
}

/* initialization functions */
static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i = 0;
	u64 *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__,
			   page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_err("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

#define TASK_INFO_SIZE PAGE_SIZE
#define PSTORE_SIZE 0x8000
static int mrdump_mini_elf_header_init(void)
{
	if (CONFIG_MRDUMP_MINI_BUF_PADDR)
		mrdump_mini_ehdr = remap_lowmem(
							   CONFIG_MRDUMP_MINI_BUF_PADDR,
							   MRDUMP_MINI_HEADER_SIZE + TASK_INFO_SIZE + PSTORE_SIZE);
	else {
		mrdump_mini_ehdr = kmalloc(MRDUMP_MINI_HEADER_SIZE, GFP_KERNEL);
	}

	if (mrdump_mini_ehdr == NULL) {
		pr_err("reserve buffer fail\n");
		return -EINVAL;
	}

	pr_info("reserved: (0x%x,+0x%lx) -> %p\n", CONFIG_MRDUMP_MINI_BUF_PADDR,
			(unsigned long)MRDUMP_MINI_HEADER_SIZE, mrdump_mini_ehdr);

	memset_io(mrdump_mini_ehdr, 0, MRDUMP_MINI_HEADER_SIZE);

	return 0;
}

int mrdump_mini_init(void)
{
	unsigned long size, offset;
	struct pt_regs regs;
	int i = 0;

	if (mrdump_mini_elf_header_init()) {
		return -EINVAL;
	}

	fill_psinfo(&mrdump_mini_ehdr->psinfo);

	memset_io(&regs, 0, sizeof(struct pt_regs));
	for (i = 0; i < CONFIG_MRDUMP_MINI_TASKS_MAX; i++) {
		fill_prstatus(&mrdump_mini_ehdr->prstatus[i], &regs, 0);
	}

	offset = offsetof(struct mrdump_mini_elf_header, psinfo);
	size = sizeof(mrdump_mini_ehdr->psinfo) +
		   sizeof(mrdump_mini_ehdr->prstatus);
	fill_elf_note_phdr(&mrdump_mini_ehdr->phdrs[0], size, offset);

	return 0;
}

EXPORT_SYMBOL(mrdump_mini_init);
void mrdump_mini_exit(void)
{
	if (CONFIG_MRDUMP_MINI_BUF_PADDR) {
		vunmap(mrdump_mini_ehdr);
	} else {
		kfree(mrdump_mini_ehdr);
	}
}
EXPORT_SYMBOL(mrdump_mini_exit);

int mrdump_mini_reserve_memory(struct reserved_mem *rmem)
{
	pr_alert("[memblock]%s: 0x%llx - 0x%llx (0x%llx)\n", "minidump",
			 (unsigned long long)rmem->base,
			 (unsigned long long)rmem->base +
			 (unsigned long long)rmem->size,
			 (unsigned long long)rmem->size);
	return 0;
}
RESERVEDMEM_OF_DECLARE(minidump, "realtek,minidump",
					   mrdump_mini_reserve_memory);