/*
 * Copyright 2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/immap_ls102xa.h>
#include <ahci.h>
#include <scsi.h>

int ls1021a_sata_init(void)
{
	struct ccsr_ahci __iomem *ccsr_ahci = (void *)AHCI_BASE_ADDR;

#ifdef CONFIG_SYS_FSL_ERRATUM_A008407
	unsigned int __iomem *dcfg_ecc = (void *)0x20220520;
#endif

	out_le32(&ccsr_ahci->ppcfg, 0xa003fffe);
	out_le32(&ccsr_ahci->pp2c, 0x28183411);
	out_le32(&ccsr_ahci->pp3c, 0x0e081004);
	out_le32(&ccsr_ahci->pp4c, 0x00480811);
	out_le32(&ccsr_ahci->pp5c, 0x192c96a4);
	out_le32(&ccsr_ahci->ptc, 0x08000024);

#ifdef CONFIG_SYS_FSL_ERRATUM_A008407
	out_le32(dcfg_ecc, 0x00020000);
#endif

	return 0;
}

void ls1021a_sata_start(void)
{
	ahci_init(AHCI_BASE_ADDR);
	udelay(100000);
	scsi_scan(1);
}
