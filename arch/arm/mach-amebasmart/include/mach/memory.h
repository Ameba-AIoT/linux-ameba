// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek Memory support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H __FILE__

#define PLAT_PHYS_OFFSET		UL(0x00000000)

/* Maximum of 256MiB in one bank */
#define MAX_PHYSMEM_BITS	32
#define SECTION_SIZE_BITS	28

#define PAGE_OFFSET1	(PAGE_OFFSET + 0x10000000)

#define __phys_to_virt(phys)						\
	((phys) >= 0x30000000 ?	(phys) - 0x30000000 + PAGE_OFFSET1 :	\
	 (phys) + PAGE_OFFSET)

#define __virt_to_phys(virt)						\
	 ((virt) >= PAGE_OFFSET1 ? (virt) - PAGE_OFFSET1 + 0x30000000 :	\
	  (virt) - PAGE_OFFSET)

#endif /* __ASM_ARCH_MEMORY_H */
