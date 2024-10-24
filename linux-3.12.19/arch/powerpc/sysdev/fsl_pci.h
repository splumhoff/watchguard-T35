/*
 * MPC85xx/86xx PCI Express structure define
 *
 * Copyright 2007,2011 Freescale Semiconductor, Inc
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifdef __KERNEL__
#ifndef __POWERPC_FSL_PCI_H
#define __POWERPC_FSL_PCI_H

struct platform_device;


/* FSL PCI controller BRR1 register */
#define PCI_FSL_BRR1      0xbf8
#define PCI_FSL_BRR1_VER 0xffff

#define PCIE_LTSSM	0x0404		/* PCIE Link Training and Status */
#define PCIE_LTSSM_L0	0x16		/* L0 state */
#define PCIE_IP_REV_2_2		0x02080202 /* PCIE IP block version Rev2.2 */
#define PCIE_IP_REV_3_0		0x02080300 /* PCIE IP block version Rev3.0 */
#define PIWAR_EN		0x80000000	/* Enable */
#define PIWAR_PF		0x20000000	/* prefetch */
#define PIWAR_TGI_LOCAL		0x00f00000	/* target - local memory */
#define PIWAR_READ_SNOOP	0x00050000
#define PIWAR_WRITE_SNOOP	0x00005000
#define PIWAR_SZ_MASK          0x0000003f

#define PEX_PMCR_PTOMR		0x1
#define PEX_PMCR_EXL2S		0x2

#define PME_DISR_EN_PTOD	0x00008000
#define PME_DISR_EN_ENL23D	0x00002000
#define PME_DISR_EN_EXL23D	0x00001000

#define ENL23_DETECT_BIT	0x00002000
#define EXL23_DETECT_BIT	0x00001000

#define PAOR_LIODON_MODE 1	/* To access LIODN permission table */
#define PAOR_MSIX_VECTOR_MODE 2	/* To access MSI-X table structure */
#define PAOR_MSIX_PBA_MODE 3	/* To access MSI-X PBA structure */
#define PAOR_MSIX_PF_TYPE 0	/* Access physical function MSIX */
#define PAOR_MSIX_VF_TYPE 1	/* Access virtual function MSIX */
#define PAOR_MSIX_TYPE_SHIFT 31
#define PAOR_MSIX_PF_SHIFT 26
#define PAOR_MSIX_VF_SHIFT 16
#define PAOR_MSIX_ENTRY_IDX_SHIFT 8
#define PAOR_MSIX_ENTRY_EIDX_SHIFT 4
#define PAOR_MSIX_CONTROL_IDX 3
#define PAOR_MSIX_MSG_DATA_IDX 2
#define PAOR_MSIX_MSG_UADDR_IDX 1
#define PAOR_MSIX_MSG_LADDR_IDX 0
#define PCIE_MSIX_VECTOR_MAX_NUM 8

/* PCI/PCI Express outbound window reg */
struct pci_outbound_window_regs {
	__be32	potar;	/* 0x.0 - Outbound translation address register */
	__be32	potear;	/* 0x.4 - Outbound translation extended address register */
	__be32	powbar;	/* 0x.8 - Outbound window base address register */
	u8	res1[4];
	__be32	powar;	/* 0x.10 - Outbound window attributes register */
	u8	res2[12];
};

/* PCI/PCI Express inbound window reg */
struct pci_inbound_window_regs {
	__be32	pitar;	/* 0x.0 - Inbound translation address register */
	u8	res1[4];
	__be32	piwbar;	/* 0x.8 - Inbound window base address register */
	__be32	piwbear;	/* 0x.c - Inbound window base extended address register */
	__be32	piwar;	/* 0x.10 - Inbound window attributes register */
	u8	res2[12];
};

/* PCI Error Management Registers */
struct pci_err_regs {
	/*   0x.e00 - PCI Error Detect Register */
	__be32	pedr;
	/*   0x.e04 - PCI Error Capture Disable Register */
	__be32	pecdr;
	/*   0x.e08 - PCI Error Interrupt Enable Register */
	__be32	peer;
	/*   0x.e0c - PCI Error Attributes Capture Register */
	__be32	peattrcr;
	/*   0x.e10 - PCI Error Address Capture Register */
	__be32	peaddrcr;
	/*   0x.e14 - PCI Error Extended Address Capture Register */
	__be32	peextaddrcr;
	/*   0x.e18 - PCI Error Data Low Capture Register */
	__be32	pedlcr;
	/*   0x.e1c - PCI Error Data High Capture Register */
	__be32	pedhcr;
	/*   0x.e20 - PCI Gasket Timer Register */
	__be32	gas_timr;
	u8	res21[4];
};

/* PCI Express Error Management Registers */
struct pcie_err_regs {
	/*  0x.e00 - PCI/PCIE error detect register */
	__be32	pex_err_dr;
	u8	res21[4];
	/*  0x.e08 - PCI/PCIE error interrupt enable register */
	__be32	pex_err_en;
	u8	res22[4];
	/*  0x.e10 - PCI/PCIE error disable register */
	__be32	pex_err_disr;
	u8	res23[12];
	/*  0x.e20 - PCI/PCIE error capture status register */
	__be32	pex_err_cap_stat;
	u8	res24[4];
};

/* PCI Express Utility Registers */
struct pcie_utility_regs {
	__be32	pex_aor;	/* 0x050 - PCIE address offset register */
	u8	res_0x054[4];
	__be32	pex_udr;	/* 0x058 - PCIE upper data register */
	__be32	pex_ldr;	/* 0x05c - PCIE lower data register */
};

/* PCI/PCI Express IO block registers for 85xx/86xx */
struct ccsr_pci {
	__be32	config_addr;		/* 0x.000 - PCI/PCIE Configuration Address Register */
	__be32	config_data;		/* 0x.004 - PCI/PCIE Configuration Data Register */
	__be32	int_ack;		/* 0x.008 - PCI Interrupt Acknowledge Register */
	__be32	pex_otb_cpl_tor;	/* 0x.00c - PCIE Outbound completion timeout register */
	__be32	pex_conf_tor;		/* 0x.010 - PCIE configuration timeout register */
	__be32	pex_config;		/* 0x.014 - PCIE CONFIG Register */
	__be32	pex_int_status;		/* 0x.018 - PCIE interrupt status */
	u8	res2[4];
	__be32	pex_pme_mes_dr;		/* 0x.020 - PCIE PME and message detect register */
	__be32	pex_pme_mes_disr;	/* 0x.024 - PCIE PME and message disable register */
	__be32	pex_pme_mes_ier;	/* 0x.028 - PCIE PME and message interrupt enable register */
	__be32	pex_pmcr;		/* 0x.02c - PCIE power management command register */
	u8	res_030[32];
	struct pcie_utility_regs putil;
	u8	res_060[2968];
	__be32	block_rev1;	/* 0x.bf8 - PCIE Block Revision register 1 */
	__be32	block_rev2;	/* 0x.bfc - PCIE Block Revision register 2 */

/* PCI/PCI Express outbound window 0-4
 * Window 0 is the default window and is the only window enabled upon reset.
 * The default outbound register set is used when a transaction misses
 * in all of the other outbound windows.
 */
	struct pci_outbound_window_regs pow[5];
	u8	res_ca0[32];
	/* 0xcc0 - 0xcdc MSI-X Trap Outbound Window Address Registers */
	struct pci_outbound_window_regs msixow;
	u8	res_ce0[32];
	struct pci_inbound_window_regs	pmit;	/* 0xd00 - 0xd9c Inbound MSI */
	u8	res6[96];
/* PCI/PCI Express inbound window 3-0
 * inbound window 1 supports only a 32-bit base address and does not
 * define an inbound window base extended address register.
 */
	struct pci_inbound_window_regs piw[4];
/* PCI/PCI Express Error Management Registers */
	union {
		struct pci_err_regs pcier;
		struct pcie_err_regs pexer;
	};
	__be32	pex_err_cap_r0;		/* 0x.e28 - PCIE error capture register 0 */
	__be32	pex_err_cap_r1;		/* 0x.e2c - PCIE error capture register 0 */
	__be32	pex_err_cap_r2;		/* 0x.e30 - PCIE error capture register 0 */
	__be32	pex_err_cap_r3;		/* 0x.e34 - PCIE error capture register 0 */
	u8	res_e38[200];
	__be32	pdb_stat;		/* 0x.f00 - PCIE Debug Status */
	u8	res_f04[16];
	__be32	pex_csr0;		/* 0x.f14 - PEX Control/Status register 0*/
#define PEX_CSR0_LTSSM_MASK	0xFC
#define PEX_CSR0_LTSSM_SHIFT	2
#define PEX_CSR0_LTSSM_L0	0x11
	__be32	pex_csr1;		/* 0x.f18 - PEX Control/Status register 1*/
	u8	res_f1c[228];

};

#define VF_ATMU_OFFSET 0x1000
#define VF_OW_NUM 4
#define VF_IW_NUM 4

/* PCIE VF outbound window translation address regs */
struct vf_owta_regs {
	__be32 tar;
	__be32 tear;
};

/* PCIE VF ATMU regs */
struct vf_atmu_regs {
	struct pci_outbound_window_regs vfow[VF_OW_NUM];/* 0x1000 - 0x107c */
	struct pci_inbound_window_regs vfiw[VF_IW_NUM]; /* 0x1080 - 0x10fc */
	u8 res_1100[1792];				/* 0x1100 - 0x17fc */
	struct vf_owta_regs vfowta[VF_OW_NUM][64];	/* 0x1800 - 0x1ffc */
};

extern int fsl_add_bridge(struct platform_device *pdev, int is_primary);
extern void fsl_pcibios_fixup_bus(struct pci_bus *bus);
void fsl_pcibios_fixup_phb(struct pci_controller *phb);
extern int mpc83xx_add_bridge(struct device_node *dev);
u64 fsl_pci_immrbar_base(struct pci_controller *hose);

extern struct device_node *fsl_pci_primary;

extern unsigned int qemu_e500_pci;

#ifdef CONFIG_PCI
void fsl_pci_assign_primary(void);
#else
static inline void fsl_pci_assign_primary(void) {}
#endif

#ifdef CONFIG_P1022_DS
void p1022ds_reset_pcie_slot(void);
#endif

#ifdef CONFIG_EDAC_MPC85XX
int mpc85xx_pci_err_probe(struct platform_device *op);
#else
static inline int mpc85xx_pci_err_probe(struct platform_device *op)
{
	return -ENOTSUPP;
}
#endif

#ifdef CONFIG_FSL_PCI
extern int fsl_pci_mcheck_exception(struct pt_regs *);
#else
static inline int fsl_pci_mcheck_exception(struct pt_regs *regs) {return 0; }
#endif

#endif /* __POWERPC_FSL_PCI_H */
#endif /* __KERNEL__ */
