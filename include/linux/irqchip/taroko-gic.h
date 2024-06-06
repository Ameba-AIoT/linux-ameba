/*
 * Realtek Semiconductor Corp.
 *
 * GIC Register Definitions
 *
 * Copyright (C) 2015  Tony Wu (tonywu@realtek.com)
 */
#ifndef _ASM_TAROKO_GIC_H_
#define _ASM_TAROKO_GIC_H_

#define GIC_MAX_INTRS			256
#define GIC_NUM_INTRS			(24 + NR_CPUS * 2)

/* GIC Accessors */
#define _GMSK(n)		((1 << (n)) - 1)
#define _GR32(addr,offs)	(*(volatile unsigned int *) ((addr) + (offs)))

#define GIC_SHF(reg,bit)	(GIC_##reg##_##bit##_SHF)
#define GIC_MSK(reg,bit)	(_GMSK(GIC_##reg##_##bit##_BIT) << GIC_SHF(reg,bit))

#define GIC_REG32(reg)		_GR32(MMCR_BASE,GIC_##reg##_OFS)
#define GIC_REG32p(reg,p)	_GR32(MMCR_BASE,GIC_##reg##_OFS + (p))

#define GIC_VAL32(reg,bit)	((GIC_REG32(reg) & GIC_MSK(reg,bit)) >> GIC_SHF(reg,bit))
#define GIC_VAL32p(reg,p,bit)	((GIC_REG32p(reg,p) & GIC_MSK(reg,bit)) >> GIC_SHF(reg,bit))

/* Global control register */
#define GIC_GLOBAL_CTRL_OFS		0x1000
#define GIC_GLOBAL_DEBUG_OFS		0x1008
#define GIC_IPI_OFS			0x100c

/*
 * Set Mask (WO) - Enables Interrupt
 * 1-bit per interrupt
 */
#define GIC_SMASK_OFS			0x1020
#define GIC_SMASK0_OFS			0x1020
#define GIC_SMASK1_OFS			0x1024
#define GIC_SMASK2_OFS			0x1028
#define GIC_SMASK3_OFS			0x102c
#define GIC_SMASK4_OFS			0x1030
#define GIC_SMASK5_OFS			0x1034
#define GIC_SMASK6_OFS			0x1038
#define GIC_SMASK7_OFS			0x103c

/*
 * Reset Mask - Disables Interrupt
 * 1-bit per interrupt
 */
#define GIC_RMASK_OFS			0x1040
#define GIC_RMASK0_OFS			0x1040
#define GIC_RMASK1_OFS			0x1044
#define GIC_RMASK2_OFS			0x1048
#define GIC_RMASK3_OFS			0x104c
#define GIC_RMASK4_OFS			0x1050
#define GIC_RMASK5_OFS			0x1054
#define GIC_RMASK6_OFS			0x1058
#define GIC_RMASK7_OFS			0x105c

/*
 * Global Interrupt Mask Register (RO) - Bit Set == Interrupt enabled
 * 1-bit per interrupt
 */
#define GIC_MASK_OFS			0x1060
#define GIC_MASK0_OFS			0x1060
#define GIC_MASK1_OFS			0x1064
#define GIC_MASK2_OFS			0x1068
#define GIC_MASK3_OFS			0x106c
#define GIC_MASK4_OFS			0x1070
#define GIC_MASK5_OFS			0x1074
#define GIC_MASK6_OFS			0x1078
#define GIC_MASK7_OFS			0x107c

/*
 * Pending Global Interrupts (RO)
 * 1-bit per interrupt
 */

#define GIC_PEND_OFS			0x1080
#define GIC_PEND0_OFS			0x1080
#define GIC_PEND1_OFS			0x1084
#define GIC_PEND2_OFS			0x1088
#define GIC_PEND3_OFS			0x108c
#define GIC_PEND4_OFS			0x1090
#define GIC_PEND5_OFS			0x1094
#define GIC_PEND6_OFS			0x1098
#define GIC_PEND7_OFS			0x109c

/*
 * R2P and R2C
 * one word per interrupt
 */
#define GIC_R2P_OFS			0x1100
#define GIC_R2C_OFS			0x1500

#define GIC_LOCAL_CONTROL_OFS		0x1900
#define GIC_DBG_GROUP_OFS		0x1908
#define  GIC_DBG_GROUP_INVITE_CORE_SHF	0
#define  GIC_DBG_GROUP_INVITE_CORE_BIT	4
#define  GIC_DBG_GROUP_JOIN_GDINT_SHF	8
#define  GIC_DBG_GROUP_JOIN_GDINT_BIT	1
#define GIC_RPT_INTNUM_OFS		0x190c
#define GIC_SITIMER_R2P_OFS		0x1910
#define  GIC_SITIMER_R2P_SITIMER_SHF	0
#define  GIC_SITIMER_R2P_SITIMER_BIT	8

/* Convert an interrupt number to a byte offset/bit for multi-word registers */
#define GIC_INTR_OFS(intr)		(((intr)/32)*4)
#define GIC_INTR_BIT(intr)		(((intr)%32))

/* Maps Interrupt X to a pin */
#define GIC_ROUTE_TO_PIN(intr,pin)	(GIC_REG32p(R2P, (intr)*4) = (pin))

/*
 * Maps Interrupt X to cores:
 *   the input is one-hot bitmap
 *   cpu = (1 << core)
 *   1 << 0 => core 0
 *   1 << 1 => core 1
 *   1 << n => core n
 *
 * So far, we assume one interrupt per cpu
 */
#define GIC_ROUTE_TO_CORE(intr,cpu)	(GIC_REG32p(R2C, (intr)*4) = (1 << cpu))

/* Mask manipulation */
#define GIC_SET_INTR_MASK(intr) \
	(GIC_REG32p(SMASK,GIC_INTR_OFS(intr)) = 1 << GIC_INTR_BIT(intr))

#define GIC_SET_IPI_MASK(intr) \
	(GIC_REG32(IPI) = 0x80000000 | intr)

#define GIC_CLR_INTR_MASK(intr) \
	(GIC_REG32p(RMASK,GIC_INTR_OFS(intr)) = 1 << GIC_INTR_BIT(intr))

#define GIC_CLR_IPI_MASK(intr) \
	(GIC_REG32(IPI) = intr)

/* Add 2 to convert GIC CPU pin to core interrupt */
#define GIC_CPU_PIN_OFFSET	2

/* Convert between local/shared IRQ number and GIC HW IRQ number. */
#define GIC_LOCAL_HWIRQ_BASE	0
#define GIC_LOCAL_TO_HWIRQ(x)	(GIC_LOCAL_HWIRQ_BASE + (x))
#define GIC_HWIRQ_TO_LOCAL(x)	((x) - GIC_LOCAL_HWIRQ_BASE)
#define GIC_SHARED_HWIRQ_BASE	GIC_NUM_LOCAL_INTRS
#define GIC_SHARED_TO_HWIRQ(x)	(GIC_SHARED_HWIRQ_BASE + (x))
#define GIC_HWIRQ_TO_SHARED(x)	((x) - GIC_SHARED_HWIRQ_BASE)

extern void gic_init(unsigned int cpu_vec, unsigned int irqbase);

#endif /* _ASM_TAROKO_GIC_H_ */
