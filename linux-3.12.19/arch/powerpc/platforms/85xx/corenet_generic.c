/*
 * Corenet based SoC DS Setup
 *
 * Maintained by Kumar Gala (see MAINTAINERS for contact information)
 *
 * Copyright 2009-2011 Freescale Semiconductor Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <asm/time.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/ppc-pci.h>
#include <mm/mmu_decl.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <asm/mpic.h>
#include <asm/ehv_pic.h>
#include <linux/fsl/qe_ic.h>

#include <linux/of_platform.h>
#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_pci.h>
#include <asm/mpc85xx.h>
#include "smp.h"
#include "mpc85xx.h"

#include <linux/fsl_usdpaa.h>

void __init corenet_gen_pic_init(void)
{
	struct mpic *mpic;
	u32 svr = mfspr(SPRN_SVR);
	unsigned int flags = MPIC_BIG_ENDIAN | MPIC_SINGLE_DEST_CPU |
		MPIC_NO_RESET;

#ifdef CONFIG_QUICC_ENGINE
	struct device_node *np;
#endif
	/*
	 * The deep sleep does not wake consistently on T1040 and T1042 with
	 * the external proxy facility mode of MPIC (MPIC_ENABLE_COREINT),
	 * so use the mixed mode of MPIC. Set mpic_get_irq to ppc_md.get_irq
	 * to enable the mixed mode of MPIC.
	 */
	if ((SVR_SOC_VER(svr) == SVR_T1040) ||
	    (SVR_SOC_VER(svr) == SVR_T1042) ||
	    (SVR_SOC_VER(svr) == SVR_T1024)) {
		ppc_md.get_irq = mpic_get_irq;
	}

	if (ppc_md.get_irq == mpic_get_coreint_irq)
		flags |= MPIC_ENABLE_COREINT;

	mpic = mpic_alloc(NULL, 0, flags, 0, 512, " OpenPIC  ");
	BUG_ON(mpic == NULL);

	mpic_init(mpic);

#ifdef CONFIG_QUICC_ENGINE
	np = of_find_compatible_node(NULL, NULL, "fsl,qe-ic");
	if (np) {
		qe_ic_init(np, 0, qe_ic_cascade_low_mpic,
				qe_ic_cascade_high_mpic);
		of_node_put(np);
	}
#endif
}

/*
 * Setup the architecture
 */
void __init corenet_gen_setup_arch(void)
{
	mpc85xx_smp_init();

	swiotlb_detect_4g();

	pr_info("%s board from Freescale Semiconductor\n", ppc_md.name);

	mpc85xx_qe_init();
}

static const struct of_device_id of_device_ids[] = {
	{
		.compatible	= "simple-bus"
	},
	{
		.compatible	= "fsl,dpaa",
	},
	{
		.compatible	= "mdio-mux-gpio",
	},
	{
		.compatible	= "fsl,fpga-ngpixis",
	},
	{
		.compatible	= "fsl,fpga-qixis",
	},
	{
		.compatible	= "fsl,srio",
	},
	{
		.compatible	= "fsl,p4080-pcie",
	},
	{
		.compatible	= "fsl,qoriq-pcie-v2.2",
	},
	{
		.compatible	= "fsl,qoriq-pcie-v2.3",
	},
	{
		.compatible	= "fsl,qoriq-pcie-v2.4",
	},
	{
		.compatible	= "fsl,qoriq-pcie-v3.0",
	},
#ifdef CONFIG_QUICC_ENGINE
	{
		.compatible	= "fsl,qe",
	},
#endif
	/* The following two are for the Freescale hypervisor */
	{
		.name		= "hypervisor",
	},
	{
		.name		= "handles",
	},
	{}
};

int __init corenet_gen_publish_devices(void)
{
	return of_platform_bus_probe(NULL, of_device_ids, NULL);
}

static const char * const boards[] __initconst = {
	"fsl,P2041RDB",
	"fsl,P3041DS",
	"fsl,P4080DS",
	"fsl,P5020DS",
	"fsl,P5040DS",
	"fsl,T1023QDS",
	"fsl,T1024QDS",
	"fsl,T1024RDB",
	"fsl,T1040QDS",
	"fsl,T1042QDS",
	"fsl,T1040RDB",
	"fsl,T1042RDB",
	"fsl,T1042RDB_PI",
	"fsl,T2080QDS",
	"fsl,T2081QDS",
	"fsl,T2080RDB",
	"fsl,T4240QDS",
	"fsl,T4240RDB",
	"fsl,B4860QDS",
	"fsl,B4420QDS",
	"fsl,B4220QDS",
	NULL
};

static const char * const hv_boards[] __initconst = {
	"fsl,P2041RDB-hv",
	"fsl,P3041DS-hv",
	"fsl,P4080DS-hv",
	"fsl,P5020DS-hv",
	"fsl,P5040DS-hv",
	"fsl,T1023QDS-hv",
	"fsl,T1024QDS-hv",
	"fsl,T1024RDB-hv",
	"fsl,T1040QDS-hv",
	"fsl,T1042QDS-hv",
	"fsl,T1040RDB-hv",
	"fsl,T1042RDB-hv",
	"fsl,T1042RDB_PI-hv",
	"fsl,T2080QDS-hv",
	"fsl,T2081QDS-hv",
	"fsl,T2080RDB-hv",
	"fsl,T4240QDS-hv",
	"fsl,T4240RDB-hv",
	"fsl,B4860QDS-hv",
	"fsl,B4420QDS-hv",
	"fsl,B4220QDS-hv",
	NULL
};

/*
 * Called very early, device-tree isn't unflattened
 */
static int __init corenet_generic_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (of_flat_dt_match(root, boards))
		return 1;

	/* Check if we're running under the Freescale hypervisor */
	if (of_flat_dt_match(root, hv_boards)) {
		ppc_md.init_IRQ = ehv_pic_init;
		ppc_md.get_irq = ehv_pic_get_irq;
		ppc_md.restart = fsl_hv_restart;
		ppc_md.power_off = fsl_hv_halt;
		ppc_md.halt = fsl_hv_halt;
		return 1;
	}

	return 0;
}

/* Early setup is required for large chunks of contiguous (and coarsely-aligned)
 * memory. The following shoe-horns Q/Bman "init_early" calls into the
 * platform setup to let them parse their CCSR nodes early on. */
#ifdef CONFIG_FSL_QMAN_CONFIG
void __init qman_init_early(void);
#endif
#ifdef CONFIG_FSL_BMAN_CONFIG
void __init bman_init_early(void);
#endif
#ifdef CONFIG_FSL_PME2_CTRL
void __init pme2_init_early(void);
#endif

static __init void corenet_ds_init_early(void)
{
#ifdef CONFIG_FSL_QMAN_CONFIG
	qman_init_early();
#endif
#ifdef CONFIG_FSL_BMAN_CONFIG
	bman_init_early();
#endif
#ifdef CONFIG_FSL_PME2_CTRL
	pme2_init_early();
#endif
#ifdef CONFIG_FSL_USDPAA
	fsl_usdpaa_init_early();
#endif
}

define_machine(corenet_generic) {
	.name			= "CoreNet Generic",
	.probe			= corenet_generic_probe,
	.setup_arch		= corenet_gen_setup_arch,
	.init_IRQ		= corenet_gen_pic_init,
#ifdef CONFIG_PCI
	.pcibios_fixup_bus	= fsl_pcibios_fixup_bus,
	.pcibios_fixup_phb	= fsl_pcibios_fixup_phb,
#endif
/*
 * Core reset may cause issue if using the proxy mode of MPIC.
 * Use the mixed mode of MPIC if enabling CPU hotplug.
 */
#ifdef CONFIG_HOTPLUG_CPU
	.get_irq		= mpic_get_irq,
#else
	.get_irq		= mpic_get_coreint_irq,
#endif
	.restart		= fsl_rstcr_restart,
	.calibrate_decr		= generic_calibrate_decr,
	.progress		= udbg_progress,
#ifdef CONFIG_PPC64
	.power_save		= book3e_idle,
#else
	.power_save		= e500_idle,
#endif
	.init_early		= corenet_ds_init_early,
};

machine_arch_initcall(corenet_generic, corenet_gen_publish_devices);

#ifdef CONFIG_SWIOTLB
machine_arch_initcall(corenet_generic, swiotlb_setup_bus_notifier);
#endif
