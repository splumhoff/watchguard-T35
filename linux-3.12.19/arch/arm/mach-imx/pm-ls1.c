/*
 * Support deep sleep feature for LS1
 *
 * Copyright 2014 Freescale Semiconductor Inc.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/cpu_pm.h>
#include <asm/suspend.h>
#include <asm/delay.h>
#include <asm/cp15.h>
#include <asm/cacheflush.h>
#include <asm/idmap.h>

#include "common.h"
#include "sleep-ls1.h"

#define FSL_SLEEP		0x1
#define FSL_DEEP_SLEEP		0x2

#define DCSR_EPU_EPSMCR15	0x278
#define DCSR_EPU_EPECR0		0x300
#define DCSR_RCPM_CG1CR0	0x31c
#define DCSR_RCPM_CSTTACR0	0xb00

#define CCSR_SCFG_DPSLPCR	0
#define CCSR_SCFG_DPSLPCR_VAL	0x1
#define CCSR_SCFG_PMCINTECR	0x160
#define CCSR_SCFG_PMCINTLECR	0x164
#define CCSR_SCFG_PMCINTSR	0x168
#define CCSR_SCFG_SPARECR2	0x504
#define CCSR_SCFG_SPARECR3	0x508

#define CCSR_DCFG_CRSTSR	0x400
#define CCSR_DCFG_CRSTSR_VAL	0x00000008

#define CCSR_RCPM_POWMGTCSR		0x130
#define CCSR_RCPM_POWMGTCSR_LPM20_REQ	0x00100000
#define CCSR_RCPM_POWMGTCSR_LPM20_ST	0x00000200
#define CCSR_RCPM_POWMGTCSR_P_LPM20_ST	0x00000100
#define CCSR_RCPM_CLPCL10SETR		0x1c4
#define CCSR_RCPM_CLPCL10SETR_C0	0x1
#define CCSR_RCPM_IPPDEXPCR0		0x140
#define CCSR_RCPM_IPPDEXPCR1		0x144

#define QIXIS_CTL_SYS			0x5
#define QIXIS_CTL_SYS_EVTSW_MASK	0x0c
#define QIXIS_CTL_SYS_EVTSW_IRQ		0x04

#define QIXIS_PWR_CTL2		0x21
#define QIXIS_PWR_CTL2_PCTL	0x2

#define OCRAM_BASE	0x10000000
#define OCRAM_SIZE	0x10000		/* 64K */
/* use the last page of SRAM */
#define SRAM_CODE_BASE_PHY	(OCRAM_BASE + OCRAM_SIZE - PAGE_SIZE)

struct ls1_pm_baseaddr {
	void __iomem *epu;
	void __iomem *dcsr_rcpm1;
	void __iomem *dcsr_rcpm2;
	void __iomem *rcpm;
	void __iomem *scfg;
	void __iomem *dcfg;
	void __iomem *fpga;
	void __iomem *sram;
};

/* 128 bytes buffer for restoring data broke by DDR training initialization */
#define DDR_BUF_SIZE	128
static u8 ddr_buff[DDR_BUF_SIZE] __aligned(64);
static struct ls1_pm_baseaddr ls1_pm_base;
/* supported sleep modes by the present platform */
static unsigned int sleep_modes;
static suspend_state_t ls1_pm_state;

static void ls1_pm_iomap(void)
{
	struct device_node *np;
	void *base;

	np = of_find_compatible_node(NULL, NULL, "fsl,ls1021a-dcsr-epu");
	base = of_iomap(np, 0);
	BUG_ON(!base);
	ls1_pm_base.epu = base;

	np = of_find_compatible_node(NULL, NULL, "fsl,ls1021a-dcsr-rcpm");
	base = of_iomap(np, 0);
	BUG_ON(!base);
	ls1_pm_base.dcsr_rcpm1 = base;
	base = of_iomap(np, 1);
	BUG_ON(!base);
	ls1_pm_base.dcsr_rcpm2 = base;

	np = of_find_compatible_node(NULL, NULL, "fsl,ls1021a-scfg");
	base = of_iomap(np, 0);
	BUG_ON(!base);
	ls1_pm_base.scfg = base;

	np = of_find_compatible_node(NULL, NULL, "fsl,ls1021a-dcfg");
	base = of_iomap(np, 0);
	BUG_ON(!base);
	ls1_pm_base.dcfg = base;

	np = of_find_compatible_node(NULL, NULL, "fsl,ls1021aqds-fpga");
	base = of_iomap(np, 0);
	BUG_ON(!base);
	ls1_pm_base.fpga = base;

	base = ioremap(SRAM_CODE_BASE_PHY, PAGE_SIZE);
	BUG_ON(!base);
	ls1_pm_base.sram = base;
}

static void ls1_pm_uniomap(void)
{
	iounmap(ls1_pm_base.epu);
	iounmap(ls1_pm_base.dcsr_rcpm1);
	iounmap(ls1_pm_base.dcsr_rcpm2);
	iounmap(ls1_pm_base.scfg);
	iounmap(ls1_pm_base.dcfg);
	iounmap(ls1_pm_base.fpga);
	iounmap(ls1_pm_base.sram);
}


static void ls1_setup_wakeup_source(void)
{
	/* enable wakeup interrupt during deep sleep */
	iowrite32be(0xfcfc0000, ls1_pm_base.scfg + CCSR_SCFG_PMCINTECR);
	iowrite32be(0, ls1_pm_base.scfg + CCSR_SCFG_PMCINTLECR);
	/* clear PMC interrupt status */
	iowrite32be(0xffffffff, ls1_pm_base.scfg + CCSR_SCFG_PMCINTSR);
}

static void ls1_clear_wakeup_source(void)
{
	/* disable wakeup interrupt during deep sleep */
	iowrite32be(0, ls1_pm_base.scfg + CCSR_SCFG_PMCINTECR);
	/* clear PMC interrupt status */
	iowrite32be(0xffffffff, ls1_pm_base.scfg + CCSR_SCFG_PMCINTSR);
}

/* set IP powerdown exception, make them work during sleep/deep sleep */
static void ls1_set_powerdown(void)
{
	iowrite32be(0x8000c000, ls1_pm_base.rcpm + CCSR_RCPM_IPPDEXPCR0);
	iowrite32be(0xf0000000, ls1_pm_base.rcpm + CCSR_RCPM_IPPDEXPCR1);
}

static void ls1_save_ddr(void *base)
{
	u32 ddr_buff_addr;

	/*
	 * DDR training initialization will break 128 bytes at the beginning
	 * of DDR, therefore, save them so that the bootloader will restore
	 * them. Assume that DDR is mapped to the address space started with
	 * CONFIG_PAGE_OFFSET.
	 */
	memcpy(ddr_buff, (void *)CONFIG_PAGE_OFFSET, DDR_BUF_SIZE);

	ddr_buff_addr = (u32)__pa(ddr_buff);

	/*
	 * the bootloader will restore the first 128 bytes of DDR from
	 * the location indicated by the register SPARECR3
	 */
	iowrite32(ddr_buff_addr, base + CCSR_SCFG_SPARECR3);
}

static void ls1_set_resume_entry(void *base)
{
	u32 resume_addr;

	/* the bootloader will finally jump to this address to resume kernel */
	resume_addr = (u32)(__pa(ls1_deepsleep_resume));

	/* use the register SPARECR2 to save the return entry */
	iowrite32(resume_addr, base + CCSR_SCFG_SPARECR2);
}

static void ls1_copy_sram_code(void)
{
	memcpy(ls1_pm_base.sram, ls1_start_fsm, ls1_sram_code_size);
}

static int ls1_start_deepsleep(unsigned long addr)
{
	typedef void (*ls1_deepsleep_t)(unsigned long);
	ls1_deepsleep_t ls1_do_deepsleep_phy;

	/* Switch to the identity mapping */
	setup_mm_for_reboot();
	v7_exit_coherency_flush(all);

	ls1_do_deepsleep_phy =
		(ls1_deepsleep_t)(unsigned long)virt_to_phys(ls1_do_deepsleep);
	ls1_do_deepsleep_phy(addr);

	/* never get here  */
	BUG();

	return 0;
}

void ls1_fsm_setup(void)
{
	iowrite32be(0x00001001, ls1_pm_base.dcsr_rcpm1 + DCSR_RCPM_CSTTACR0);
	iowrite32be(0x00000001, ls1_pm_base.dcsr_rcpm1 + DCSR_RCPM_CG1CR0);

	fsl_epu_setup_default(ls1_pm_base.epu);

	/*
	 * pull the MCKE signal(EVT4_B pin) low before enabling
	 * deep sleep signals by FPGA
	 */
	iowrite32be(0x5, ls1_pm_base.epu + DCSR_EPU_EPECR0);

	iowrite32be(0x76300000, ls1_pm_base.epu + DCSR_EPU_EPSMCR15);
}

static inline void ls1_clrsetbits_be32(void __iomem *addr, u32 clear, u32 set)
{
	u32 tmp;

	tmp = ioread32be(addr);
	tmp = (tmp & ~clear) | set;
	iowrite32be(tmp, addr);
}

static void ls1_enter_deepsleep(void)
{
	u32 tmp;

	/* save DDR data */
	ls1_save_ddr(ls1_pm_base.scfg);

	/* save kernel resume entry */
	ls1_set_resume_entry(ls1_pm_base.scfg);

	/* Request to put cluster 0 in PCL10 state */
	ls1_clrsetbits_be32(ls1_pm_base.rcpm + CCSR_RCPM_CLPCL10SETR,
			    CCSR_RCPM_CLPCL10SETR_C0,
			    CCSR_RCPM_CLPCL10SETR_C0);

	/* setup the registers of the EPU FSM for deep sleep */
	ls1_fsm_setup();

	/* connect the EVENT button to IRQ in FPGA */
	tmp = ioread8(ls1_pm_base.fpga + QIXIS_CTL_SYS);
	tmp &= ~QIXIS_CTL_SYS_EVTSW_MASK;
	tmp |= QIXIS_CTL_SYS_EVTSW_IRQ;
	iowrite8(tmp, ls1_pm_base.fpga + QIXIS_CTL_SYS);

	/* enable deep sleep signals in FPGA */
	tmp = ioread8(ls1_pm_base.fpga + QIXIS_PWR_CTL2);
	iowrite8(tmp | QIXIS_PWR_CTL2_PCTL, ls1_pm_base.fpga + QIXIS_PWR_CTL2);

	/* enable Warm Device Reset */
	ls1_clrsetbits_be32(ls1_pm_base.scfg + CCSR_SCFG_DPSLPCR,
			    CCSR_SCFG_DPSLPCR_VAL, CCSR_SCFG_DPSLPCR_VAL);

	ls1_clrsetbits_be32(ls1_pm_base.dcfg + CCSR_DCFG_CRSTSR,
			    CCSR_DCFG_CRSTSR_VAL, CCSR_DCFG_CRSTSR_VAL);

	/* copy the last stage code to sram */
	ls1_copy_sram_code();

	ls1_setup_wakeup_source();

	cpu_suspend(SRAM_CODE_BASE_PHY, ls1_start_deepsleep);

	ls1_clear_wakeup_source();

	/* disable Warm Device Reset */
	ls1_clrsetbits_be32(ls1_pm_base.scfg + CCSR_SCFG_DPSLPCR,
			    CCSR_SCFG_DPSLPCR_VAL, 0);

	/* disable deep sleep signals in FPGA */
	tmp = ioread8(ls1_pm_base.fpga + QIXIS_PWR_CTL2);
	iowrite8(tmp & ~QIXIS_PWR_CTL2_PCTL, ls1_pm_base.fpga + QIXIS_PWR_CTL2);
}

static int ls1_suspend_enter(suspend_state_t state)
{
	int ret = 0;

	ls1_set_powerdown();

	switch (state) {
	case PM_SUSPEND_STANDBY:
		flush_cache_louis();
		ls1_clrsetbits_be32(ls1_pm_base.rcpm + CCSR_RCPM_POWMGTCSR,
				    CCSR_RCPM_POWMGTCSR_LPM20_REQ,
				    CCSR_RCPM_POWMGTCSR_LPM20_REQ);

		cpu_do_idle();
		break;

	case PM_SUSPEND_MEM:
		ls1_enter_deepsleep();
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int ls1_suspend_valid(suspend_state_t state)
{
	if (state == PM_SUSPEND_STANDBY && (sleep_modes & FSL_SLEEP))
		return 1;

	if (state == PM_SUSPEND_MEM && (sleep_modes & FSL_DEEP_SLEEP))
		return 1;

	return 0;
}

static int ls1_suspend_begin(suspend_state_t state)
{
	ls1_pm_state = state;

	if (ls1_pm_state == PM_SUSPEND_MEM)
		ls1_pm_iomap();

	return 0;
}

static void ls1_suspend_end(void)
{
	if (ls1_pm_state == PM_SUSPEND_MEM)
		ls1_pm_uniomap();
}

static const struct platform_suspend_ops ls1_suspend_ops = {
	.valid = ls1_suspend_valid,
	.enter = ls1_suspend_enter,
	.begin = ls1_suspend_begin,
	.end = ls1_suspend_end,
};


static const struct of_device_id rcpm_matches[] = {
	{
		.compatible = "fsl,ls1021a-rcpm",
		.data = (void *)(FSL_SLEEP | FSL_DEEP_SLEEP),
	},
	{}
};

static int __init ls1_pm_init(void)
{
	const struct of_device_id *match;
	struct device_node *np;
	void *base;

	np = of_find_matching_node_and_match(NULL, rcpm_matches, &match);
	if (!np) {
		pr_err("%s: can't find the rcpm node.\n", __func__);
		return -EINVAL;
	}

	base = of_iomap(np, 0);
	of_node_put(np);
	if (!base)
		return -ENOMEM;

	sleep_modes = (unsigned int)match->data;
	ls1_pm_base.rcpm = base;
	suspend_set_ops(&ls1_suspend_ops);
	return 0;
}
arch_initcall(ls1_pm_init);
