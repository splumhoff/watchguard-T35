/* Integrated Flash Controller NAND Machine Driver
 *
 * Copyright (c) 2011 Freescale Semiconductor, Inc
 *
 * Authors: Dipen Dudhat <Dipen.Dudhat@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <common.h>
#include <malloc.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>

#include <asm/io.h>
#include <asm/errno.h>
#include <asm/fsl_ifc.h>

#define MAX_BANKS	4
#define ERR_BYTE	0xFF /* Value returned for read bytes
				when read failed */
#define IFC_TIMEOUT_MSECS 10 /* Maximum number of mSecs to wait for IFC
				NAND Machine */

struct fsl_ifc_ctrl;

/* mtd information per set */
struct fsl_ifc_mtd {
	struct mtd_info mtd;
	struct nand_chip chip;
	struct fsl_ifc_ctrl *ctrl;

	struct device *dev;
	int bank;               /* Chip select bank number                */
	u8 __iomem *vbase;      /* Chip select base virtual address       */
};

/* overview of the fsl ifc controller */
struct fsl_ifc_ctrl {
	struct nand_hw_control controller;
	struct fsl_ifc_mtd *chips[MAX_BANKS];

	/* device info */
	struct fsl_ifc *regs;
	uint8_t __iomem *addr;   /* Address of assigned IFC buffer        */
	unsigned int cs_nand;    /* On which chipsel NAND is connected	  */
	unsigned int page;       /* Last page written to / read from      */
	unsigned int read_bytes; /* Number of bytes read during command   */
	unsigned int column;     /* Saved column from SEQIN               */
	unsigned int index;      /* Pointer to next byte to 'read'        */
	unsigned int status;     /* status read from NEESR after last op  */
	unsigned int mdr;        /* IFC Data Register value               */
	unsigned int use_mdr;    /* Non zero if the MDR is to be set      */
	unsigned int oob;        /* Non zero if operating on OOB data     */
};

static struct fsl_ifc_ctrl *ifc_ctrl;

/*
 * Generic flash bbt descriptors
 */
static u8 bbt_pattern[] = {'B', 'b', 't', '0' };
static u8 mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	0,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	11,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};


/*
 * Set up the IFC hardware block and page address fields, and the ifc nand
 * structure addr field to point to the correct IFC buffer in memory
 */
static void set_addr(struct mtd_info *mtd, int column, int page_addr, int oob)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	struct fsl_ifc *ifc = ctrl->regs;
	int buf_num;

	ctrl->page = page_addr;

	/* Program ROW0/COL0 */
	out_be32(&ifc->ifc_nand.row0, page_addr);
	out_be32(&ifc->ifc_nand.col0, (oob ? IFC_NAND_COL_MS : 0) | column);

	if (mtd->writesize == 4096)
		buf_num = page_addr & 0x1;
	else if (mtd->writesize == 2048)
		buf_num = page_addr & 0x3;
	else
		buf_num = page_addr & 0xf;

	ctrl->addr = priv->vbase + buf_num * (mtd->writesize * 2);
	ctrl->index = column;

	/* for OOB data point to the second half of the buffer */
	if (oob)
		ctrl->index += mtd->writesize;
}

/*
 * execute IFC NAND command and wait for it to complete
 */
static int fsl_ifc_run_command(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	struct fsl_ifc *ifc = ctrl->regs;
	long long end_tick;
	u32 nand_evter_stat = 0, pgrdcmpl_evt_stat = 0;

	if (ctrl->use_mdr)
		out_be32(&ifc->ifc_nand.nand_mdr, ctrl->mdr);

	out_be32(&ifc->ifc_nand.ncfgr, 0x0);

	/* start read/write seq */
	out_be32(&ifc->ifc_nand.nandseq_strt, IFC_NAND_SEQ_STRT_FIR_STRT);

	/* wait for NAND Machine complete flag or timeout */
	end_tick = usec2ticks(IFC_TIMEOUT_MSECS * 1000) + get_ticks();

	while (end_tick > get_ticks()) {
		nand_evter_stat = in_be32(&ifc->ifc_nand.nand_evter_stat);
		pgrdcmpl_evt_stat = in_be32(&ifc->ifc_nand.pgrdcmpl_evt_stat);

		if (nand_evter_stat & IFC_NAND_EVTER_STAT_OPC) {
			out_be32(&ifc->ifc_nand.nand_evter_stat,
					nand_evter_stat);
			out_be32(&ifc->ifc_nand.pgrdcmpl_evt_stat,
					pgrdcmpl_evt_stat);

			/* check for errors */
			if (nand_evter_stat & IFC_NAND_EVTER_STAT_FTOER)
				printf("%s: Flash Time Out Error\n", __func__);
			else if (nand_evter_stat & IFC_NAND_EVTER_STAT_WPER)
				printf("%s: Write Protect Error\n", __func__);
			else if (nand_evter_stat & IFC_NAND_EVTER_STAT_ECCER)
				printf("%s: Flash Time Out Error\n", __func__);
			break;
		}
	}

	if (ctrl->use_mdr)
		ctrl->mdr = in_be32(&ifc->ifc_nand.nand_mdr);

	ctrl->use_mdr = 0;
	ctrl->status = nand_evter_stat;

	/* returns 0 on success otherwise non-zero) */
	return ctrl->status == IFC_NAND_EVTER_STAT_OPC ? 0 : -EIO;
}


static void fsl_ifc_do_read(struct nand_chip *chip,
			    int oob,
			    struct mtd_info *mtd)
{
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	struct fsl_ifc *ifc = ctrl->regs;

	/* Program FIR/IFC_NAND_FCR0 for Small/Large page */
	if (mtd->writesize > 512) {
		out_be32(&ifc->ifc_nand.nand_fir0,
			 (IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT) |
			 (IFC_FIR_OP_CA0 << IFC_NAND_FIR0_OP1_SHIFT) |
			 (IFC_FIR_OP_RA0 << IFC_NAND_FIR0_OP2_SHIFT) |
			 (IFC_FIR_OP_CMD1 << IFC_NAND_FIR0_OP3_SHIFT) |
			 (IFC_FIR_OP_RBCD << IFC_NAND_FIR0_OP4_SHIFT));
		out_be32(&ifc->ifc_nand.nand_fir1, 0x0);

		out_be32(&ifc->ifc_nand.nand_fcr0,
			(NAND_CMD_READ0 << IFC_NAND_FCR0_CMD0_SHIFT) |
			(NAND_CMD_READSTART << IFC_NAND_FCR0_CMD1_SHIFT));
	} else {
		out_be32(&ifc->ifc_nand.nand_fir0,
			 (IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT) |
			 (IFC_FIR_OP_CA0 << IFC_NAND_FIR0_OP1_SHIFT) |
			 (IFC_FIR_OP_RA0  << IFC_NAND_FIR0_OP2_SHIFT) |
			 (IFC_FIR_OP_RBCD << IFC_NAND_FIR0_OP3_SHIFT));
		out_be32(&ifc->ifc_nand.nand_fir1, 0x0);

		if (oob)
			out_be32(&ifc->ifc_nand.nand_fcr0,
				 NAND_CMD_READOOB << IFC_NAND_FCR0_CMD0_SHIFT);
		else
			out_be32(&ifc->ifc_nand.nand_fcr0,
				NAND_CMD_READ0 << IFC_NAND_FCR0_CMD0_SHIFT);
	}
}

/* cmdfunc send commands to the IFC NAND Machine */
static void fsl_ifc_cmdfunc(struct mtd_info *mtd, unsigned int command,
			     int column, int page_addr)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	struct fsl_ifc *ifc = ctrl->regs;

	/* set the chip select for NAND Transaction */
	out_be32(&ifc->ifc_nand.nand_csel, ifc_ctrl->cs_nand);

	ctrl->use_mdr = 0;

	/* clear the read buffer */
	ctrl->read_bytes = 0;
	if (command != NAND_CMD_PAGEPROG)
		ctrl->index = 0;

	switch (command) {
	/* READ0 read the entire buffer to use hardware ECC. */
	case NAND_CMD_READ0:
		out_be32(&ifc->ifc_nand.nand_fbcr, 0);
		set_addr(mtd, 0, page_addr, 0);

		ctrl->read_bytes = mtd->writesize + mtd->oobsize;
		ctrl->index += column;

		fsl_ifc_do_read(chip, 0, mtd);
		fsl_ifc_run_command(mtd);
		return;

	/* READOOB reads only the OOB because no ECC is performed. */
	case NAND_CMD_READOOB:
		out_be32(&ifc->ifc_nand.nand_fbcr, mtd->oobsize - column);
		set_addr(mtd, column, page_addr, 1);

		ctrl->read_bytes = mtd->writesize + mtd->oobsize;

		fsl_ifc_do_read(chip, 1, mtd);
		fsl_ifc_run_command(mtd);

		return;

	/* READID must read all 5 possible bytes while CEB is active */
	case NAND_CMD_READID:
		out_be32(&ifc->ifc_nand.nand_fir0,
				(IFC_FIR_OP_CMD0 << IFC_NAND_FIR0_OP0_SHIFT) |
				(IFC_FIR_OP_UA  << IFC_NAND_FIR0_OP1_SHIFT) |
				(IFC_FIR_OP_RB << IFC_NAND_FIR0_OP2_SHIFT));
		out_be32(&ifc->ifc_nand.nand_fcr0,
				NAND_CMD_READID << IFC_NAND_FCR0_CMD0_SHIFT);
		/* 5 bytes for manuf, device and exts */
		out_be32(&ifc->ifc_nand.nand_fbcr, 4);
		ctrl->read_bytes = 4;
		ctrl->use_mdr = 0;
		ctrl->mdr = 0;

		set_addr(mtd, 0, 0, 0);
		fsl_ifc_run_command(mtd);
		return;

	/* ERASE1 stores the block and page address */
	case NAND_CMD_ERASE1:
		set_addr(mtd, 0, page_addr, 0);
		return;

	/* ERASE2 uses the block and page address from ERASE1 */
	case NAND_CMD_ERASE2:
		out_be32(&ifc->ifc_nand.nand_fir0,
			 (IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT) |
			 (IFC_FIR_OP_RA0 << IFC_NAND_FIR0_OP1_SHIFT) |
			 (IFC_FIR_OP_CMD1 << IFC_NAND_FIR0_OP2_SHIFT));

		out_be32(&ifc->ifc_nand.nand_fcr0,
			 (NAND_CMD_ERASE1 << IFC_NAND_FCR0_CMD0_SHIFT) |
			 (NAND_CMD_ERASE2 << IFC_NAND_FCR0_CMD1_SHIFT));

		out_be32(&ifc->ifc_nand.nand_fbcr, 0);
		ctrl->read_bytes = 0;
		fsl_ifc_run_command(mtd);
		return;

	/* SEQIN sets up the addr buffer and all registers except the length */
	case NAND_CMD_SEQIN: {
		u32 nand_fcr0;
		ctrl->column = column;
		ctrl->oob = 0;

		if (mtd->writesize > 512) {
			nand_fcr0 =
				(NAND_CMD_SEQIN << IFC_NAND_FCR0_CMD0_SHIFT) |
				(NAND_CMD_PAGEPROG << IFC_NAND_FCR0_CMD1_SHIFT);

			out_be32(&ifc->ifc_nand.nand_fir0,
				 (IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT) |
				 (IFC_FIR_OP_CA0 << IFC_NAND_FIR0_OP1_SHIFT) |
				 (IFC_FIR_OP_RA0 << IFC_NAND_FIR0_OP2_SHIFT) |
				 (IFC_FIR_OP_WBCD  << IFC_NAND_FIR0_OP3_SHIFT) |
				 (IFC_FIR_OP_CW1 << IFC_NAND_FIR0_OP4_SHIFT));
		} else {
			nand_fcr0 = ((NAND_CMD_PAGEPROG <<
					IFC_NAND_FCR0_CMD1_SHIFT) |
				    (NAND_CMD_SEQIN <<
					IFC_NAND_FCR0_CMD2_SHIFT));

			out_be32(&ifc->ifc_nand.nand_fir0,
				 (IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT) |
				 (IFC_FIR_OP_CMD2 << IFC_NAND_FIR0_OP1_SHIFT) |
				 (IFC_FIR_OP_CA0 << IFC_NAND_FIR0_OP2_SHIFT) |
				 (IFC_FIR_OP_RA0 << IFC_NAND_FIR0_OP3_SHIFT) |
				 (IFC_FIR_OP_WBCD << IFC_NAND_FIR0_OP4_SHIFT));
			out_be32(&ifc->ifc_nand.nand_fir1,
				 (IFC_FIR_OP_CW1 << IFC_NAND_FIR1_OP5_SHIFT));

			if (column >= mtd->writesize) {
				/* OOB area --> READOOB */
				column -= mtd->writesize;
				nand_fcr0 |= NAND_CMD_READOOB <<
						IFC_NAND_FCR0_CMD0_SHIFT;
				ctrl->oob = 1;
			} else if (column < 256)
				/* First 256 bytes --> READ0 */
				nand_fcr0 |= NAND_CMD_READ0 << FCR_CMD0_SHIFT;
			else
				/* Second 256 bytes --> READ1 */
				nand_fcr0 |= NAND_CMD_READ1 << FCR_CMD0_SHIFT;
		}

		out_be32(&ifc->ifc_nand.nand_fcr0, nand_fcr0);
		set_addr(mtd, column, page_addr, ctrl->oob);
		return;
	}

	/* PAGEPROG reuses all of the setup from SEQIN and adds the length */
	case NAND_CMD_PAGEPROG: {
		int full_page;
		if (ctrl->oob) {
			out_be32(&ifc->ifc_nand.nand_fbcr, ctrl->index);
			full_page = 0;
		} else {
			out_be32(&ifc->ifc_nand.nand_fbcr, 0);
			full_page = 1;
		}

		fsl_ifc_run_command(mtd);
		return;
	}

	case NAND_CMD_STATUS:
		out_be32(&ifc->ifc_nand.nand_fir0,
				(IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT) |
				(IFC_FIR_OP_RB << IFC_NAND_FIR0_OP1_SHIFT));
		out_be32(&ifc->ifc_nand.nand_fcr0,
				NAND_CMD_STATUS << IFC_NAND_FCR0_CMD0_SHIFT);
		out_be32(&ifc->ifc_nand.nand_fbcr, 1);
		set_addr(mtd, 0, 0, 0);
		ctrl->read_bytes = 1;

		fsl_ifc_run_command(mtd);

		/* Chip sometimes reporting write protect even when it's not */
		out_8(ctrl->addr, in_8(ctrl->addr) | NAND_STATUS_WP);
		return;

	case NAND_CMD_RESET:
		out_be32(&ifc->ifc_nand.nand_fir0,
				IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT);
		out_be32(&ifc->ifc_nand.nand_fcr0,
				NAND_CMD_RESET << IFC_NAND_FCR0_CMD0_SHIFT);
		fsl_ifc_run_command(mtd);
		return;

	default:
		printf("%s: error, unsupported command 0x%x.\n",
			__func__, command);
	}
}

/*
 * Write buf to the IFC NAND Controller Data Buffer
 */
static void fsl_ifc_write_buf(struct mtd_info *mtd, const u8 *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	unsigned int bufsize = mtd->writesize + mtd->oobsize;

	if (len <= 0) {
		printf("%s of %d bytes", __func__, len);
		ctrl->status = 0;
		return;
	}

	if ((unsigned int)len > bufsize - ctrl->index) {
		printf("%s beyond end of buffer "
		       "(%d requested, %u available)\n",
			__func__, len, bufsize - ctrl->index);
		len = bufsize - ctrl->index;
	}

	memcpy_toio(&ctrl->addr[ctrl->index], buf, len);
	ctrl->index += len;
}

/*
 * read a byte from either the IFC hardware buffer if it has any data left
 * otherwise issue a command to read a single byte.
 */
static u8 fsl_ifc_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;

	/* If there are still bytes in the IFC buffer, then use the
	 * next byte. */
	if (ctrl->index < ctrl->read_bytes)
		return in_8(&ctrl->addr[ctrl->index++]);

	printf("%s beyond end of buffer\n", __func__);
	return ERR_BYTE;
}

/*
 * Read from the IFC Controller Data Buffer
 */
static void fsl_ifc_read_buf(struct mtd_info *mtd, u8 *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	int avail;

	if (len < 0)
		return;

	avail = min((unsigned int)len, ctrl->read_bytes - ctrl->index);
	memcpy_fromio(buf, &ctrl->addr[ctrl->index], avail);
	ctrl->index += avail;

	if (len > avail)
		printf("%s beyond end of buffer "
		       "(%d requested, %d available)\n",
		       __func__, len, avail);
}

/*
 * Verify buffer against the IFC Controller Data Buffer
 */
static int fsl_ifc_verify_buf(struct mtd_info *mtd,
			       const u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	int i;

	if (len < 0) {
		printf("%s of %d bytes", __func__, len);
		return -EINVAL;
	}

	if ((unsigned int)len > ctrl->read_bytes - ctrl->index) {
		printf("%s beyond end of buffer "
		       "(%d requested, %u available)\n",
		       __func__, len, ctrl->read_bytes - ctrl->index);

		ctrl->index = ctrl->read_bytes;
		return -EINVAL;
	}

	for (i = 0; i < len; i++)
		if (in_8(&ctrl->addr[ctrl->index + i]) != buf[i])
			break;

	ctrl->index += len;
	return i == len && ctrl->status == IFC_NAND_EVTER_STAT_OPC ? 0 : -EIO;
}

/* This function is called after Program and Erase Operations to
 * check for success or failure.
 */
static int fsl_ifc_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct fsl_ifc_mtd *priv = chip->priv;
	struct fsl_ifc_ctrl *ctrl = priv->ctrl;
	struct fsl_ifc *ifc = ctrl->regs;
	u32 nand_fsr;

	if (ctrl->status != IFC_NAND_EVTER_STAT_OPC)
		return NAND_STATUS_FAIL;

	/* Use READ_STATUS command, but wait for the device to be ready */
	ctrl->use_mdr = 0;
	out_be32(&ifc->ifc_nand.nand_fir0,
		 (IFC_FIR_OP_CW0 << IFC_NAND_FIR0_OP0_SHIFT) |
		 (IFC_FIR_OP_RDSTAT << IFC_NAND_FIR0_OP1_SHIFT));
	out_be32(&ifc->ifc_nand.nand_fcr0, NAND_CMD_STATUS <<
			IFC_NAND_FCR0_CMD0_SHIFT);
	out_be32(&ifc->ifc_nand.nand_fbcr, 1);
	set_addr(mtd, 0, 0, 0);
	ctrl->read_bytes = 1;

	fsl_ifc_run_command(mtd);

	if (ctrl->status != IFC_NAND_EVTER_STAT_OPC)
		return NAND_STATUS_FAIL;

	nand_fsr = in_be32(&ifc->ifc_nand.nand_fsr);

	/* Chip sometimes reporting write protect even when it's not */
	nand_fsr = nand_fsr | NAND_STATUS_WP;
	return nand_fsr;
}

static void fsl_ifc_ctrl_init(void)
{
	ifc_ctrl = kzalloc(sizeof(*ifc_ctrl), GFP_KERNEL);
	if (!ifc_ctrl)
		return;

	ifc_ctrl->regs = IFC_BASE_ADDR;

	/* clear event registers */
	out_be32(&ifc_ctrl->regs->ifc_nand.nand_evter_stat, 0);
	out_be32(&ifc_ctrl->regs->ifc_nand.pgrdcmpl_evt_stat, 0);

	/* Enable error and event for any detected errors */
	out_be32(&ifc_ctrl->regs->ifc_nand.nand_evter_en,
			IFC_NAND_EVTER_EN_OPC_EN |
			IFC_NAND_EVTER_EN_PGRDCMPL_EN |
			IFC_NAND_EVTER_EN_FTOER_EN |
			IFC_NAND_EVTER_EN_WPER_EN |
			IFC_NAND_EVTER_EN_ECCER_EN);

	ifc_ctrl->read_bytes = 0;
	ifc_ctrl->index = 0;
	ifc_ctrl->addr = NULL;
}

static void fsl_ifc_select_chip(struct mtd_info *mtd, int chip)
{
}

int board_nand_init(struct nand_chip *nand)
{
	struct fsl_ifc_mtd *priv;
	uint32_t cspr = 0, csor = 0;

	if (!ifc_ctrl) {
		fsl_ifc_ctrl_init();
		if (!ifc_ctrl)
			return -1;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->ctrl = ifc_ctrl;
	priv->vbase = nand->IO_ADDR_R;

	/* Find which chip select it is connected to.
	 */
	for (priv->bank = 0; priv->bank < MAX_BANKS; priv->bank++) {
		phys_addr_t base_addr = virt_to_phys(nand->IO_ADDR_R);

		cspr = in_be32(&ifc_ctrl->regs->cspr_cs[priv->bank].cspr);
		csor = in_be32(&ifc_ctrl->regs->csor_cs[priv->bank].csor);

		if ((cspr & CSPR_V) && (cspr & CSPR_MSEL) == CSPR_MSEL_NAND &&
		    (cspr & CSPR_BA) == CSPR_PHYS_ADDR(base_addr)) {
			ifc_ctrl->cs_nand = priv->bank << IFC_NAND_CSEL_SHIFT;
			break;
		}
	}

	if (priv->bank >= MAX_BANKS) {
		printf("%s: address did not match any "
		       "chip selects\n", __func__);
		return -ENODEV;
	}

	ifc_ctrl->chips[priv->bank] = priv;

	/* fill in nand_chip structure */
	/* set up function call table */
	nand->read_byte = fsl_ifc_read_byte;
	nand->write_buf = fsl_ifc_write_buf;
	nand->read_buf = fsl_ifc_read_buf;
	nand->verify_buf = fsl_ifc_verify_buf;
	nand->select_chip = fsl_ifc_select_chip;
	nand->cmdfunc = fsl_ifc_cmdfunc;
	nand->waitfunc = fsl_ifc_wait;

	/* set up nand options */
	nand->bbt_td = &bbt_main_descr;
	nand->bbt_md = &bbt_mirror_descr;

	/* set up nand options */
	nand->options = NAND_NO_READRDY | NAND_NO_AUTOINCR |
			NAND_USE_FLASH_BBT;

	nand->controller = &ifc_ctrl->controller;
	nand->priv = priv;

	return 0;
}
