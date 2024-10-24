/*
 * Copyright 2013 Freescale Semiconductor, Inc.
 *
 * Author: Minghuan Lian <Minghuan.Lian@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <sysdev/fsl_pci.h>
#include <linux/log2.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/eventfd.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/init.h>
#include <linux/module.h>

#include "fsl_pci_ep.h"

#define CONFIG_ACCESS_PF_SHIFT 8
#define CONFIG_ACCESS_VF_SHIFT 12
#define CONFIG_ACCESS_EXTREG_SHIFT 24
#define CONFIG_ACCESS_TYPE_SHIFT 29
#define CONFIG_ACCESS_TYPE_NO_SRIOV 0
#define CONFIG_ACCESS_TYPE_PF_SRIOV 1
#define CONFIG_ACCESS_TYPE_VF_SRIOV 3
#define CONFIG_ACCESS_ENABLE (1 << 31)

#define PCI_ATMU_MIN_SIZE (4 * 1024) /* The smallest window size is 4 Kbytes */
#define PCI_OB_WIN_MEM_ATTR 0x80044000 /* enable & mem R/W */

static DEFINE_SPINLOCK(pci_ep_spinlock);
LIST_HEAD(pci_ep_controllers);

static int global_phb_number;	/* Global phb counter */

u32 fsl_pci_msix_vector_read(struct pci_ep_dev *ep, int vector, int eidx)
{
	struct pcie_utility_regs *putil;
	int type, vf_idx, addr, data;

	putil = &ep->pf->regs->putil;

	if (ep->type == PCI_EP_TYPE_PF) {
		type = PAOR_MSIX_PF_TYPE;
		vf_idx = 0;
	} else {
		type = PAOR_MSIX_VF_TYPE;
		vf_idx = ep->idx - 1;
	}

	addr = type << PAOR_MSIX_TYPE_SHIFT |
	       ep->pf->idx << PAOR_MSIX_PF_SHIFT |
	       vf_idx << PAOR_MSIX_VF_SHIFT |
	       vector << PAOR_MSIX_ENTRY_IDX_SHIFT |
	       eidx << PAOR_MSIX_ENTRY_EIDX_SHIFT |
	       PAOR_MSIX_VECTOR_MODE;

	iowrite32be(addr, &putil->pex_aor);
	data = ioread32(&putil->pex_ldr);

	return data;
}

u32 fsl_pci_msix_pba_read(struct pci_ep_dev *ep, int vector)
{
	struct pcie_utility_regs *putil;
	int type, vf_idx, addr, data;

	putil = &ep->pf->regs->putil;

	if (ep->type == PCI_EP_TYPE_PF) {
		type = PAOR_MSIX_PF_TYPE;
		vf_idx = 0;
	} else {
		type = PAOR_MSIX_VF_TYPE;
		vf_idx = ep->idx - 1;
	}

	addr = type << PAOR_MSIX_TYPE_SHIFT |
	       ep->pf->idx << PAOR_MSIX_PF_SHIFT |
	       vf_idx << PAOR_MSIX_VF_SHIFT |
	       vector << PAOR_MSIX_ENTRY_IDX_SHIFT |
	       (vector / 32) << PAOR_MSIX_ENTRY_EIDX_SHIFT |
	       PAOR_MSIX_PBA_MODE;

	iowrite32be(addr, &putil->pex_aor);
	data = ioread32(&putil->pex_ldr);

	return data;
}

static inline size_t attr_to_size(u32 attr)
{
	int bits = (attr & 0x3f);

	if (bits == 0)
		return 0;

	return (size_t)2 << bits;
}

static int fsl_pci_find_bar_pos(struct pci_dev *dev, int base, int idx)
{
	int i, bar32, pos = base;

	if (idx >= FSL_PCI_EP_BAR_NUM)
		return -EINVAL;

	for (i = 0; i < idx; i++) {
		pci_read_config_dword(dev, pos, &bar32);
		if (bar32 & 0x4) /* 64-bit address space */
			pos += 8;
		else
			pos += 4;
	}

	return pos;
}

static u64 fsl_pci_read_bar(struct pci_dev *dev, int pos)
{
	u64 bar64;
	u32 bar32;

	pci_read_config_dword(dev, pos, &bar32);
	bar64 = bar32 & 0xfffffff0;
	if (bar32 & 0x4) {
		/* 64-bit address space */
		pci_read_config_dword(dev, pos + 4, &bar32);
		bar64 |= (u64)bar32 << 32;
	}

	return bar64;
}

static u64 fsl_pci_pf_read_vfbar(struct pci_pf_dev *pf, int idx)
{
	struct pci_dev *pdev = pf->pdev;
	int pos;

	pos = fsl_pci_find_bar_pos(pdev, pf->vf_pos + PCI_SRIOV_BAR, idx);
	return fsl_pci_read_bar(pdev, pos);
}

static u64 fsl_pci_pf_read_bar(struct pci_pf_dev *pf, int idx)
{
	struct pci_dev *pdev = pf->pdev;
	int pos;

	pos = fsl_pci_find_bar_pos(pdev, PCI_BASE_ADDRESS_0, idx);
	return fsl_pci_read_bar(pdev, pos);
}

static u64 fsl_pci_ep_read_bar(struct pci_ep_dev *ep, int idx)
{
	if (idx >= ep->iw_num)
		return 0;

	if (ep->type == PCI_EP_TYPE_PF)
		return fsl_pci_pf_read_bar(ep->pf, idx);
	else
		return fsl_pci_pf_read_vfbar(ep->pf, idx);
}

static int fsl_pci_ep_get_ibwin(struct pci_ep_dev *ep,
				struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_inbound_window_regs *iw_regs;

	if (win->idx >= ep->iw_num)
		return -EINVAL;

	if (ep->type == PCI_EP_TYPE_PF) {
		iw_regs = &pf->regs->piw[ep->iw_num - win->idx - 1];

		spin_lock(&pf->lock);
		win->cpu_addr =
			((u64)in_be32(&iw_regs->pitar)) << 12;
		win->attr = in_be32(&iw_regs->piwar);
		spin_unlock(&pf->lock);

		win->size = attr_to_size(win->attr);
		win->pci_addr = fsl_pci_ep_read_bar(ep, win->idx);
	} else {
		/* VF inbound windows */
		iw_regs = &pf->vf_regs->vfiw[ep->iw_num - win->idx - 1];

		spin_lock(&pf->lock);
		win->cpu_addr =
			((u64)in_be32(&iw_regs->pitar)) << 12;
		win->attr = in_be32(&iw_regs->piwar);
		spin_unlock(&pf->lock);

		win->size = attr_to_size(win->attr);
		win->pci_addr = fsl_pci_ep_read_bar(ep, win->idx);
		win->cpu_addr += win->size * (ep->idx - 1);
		win->pci_addr += win->size * (ep->idx - 1);
	}

	return 0;
}

static int fsl_pci_ep_get_obwin(struct pci_ep_dev *ep,
				struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_outbound_window_regs *ow_regs;

	if (win->idx >= ep->ow_num)
		return -EINVAL;

	if (ep->type == PCI_EP_TYPE_PF) {
		ow_regs = &pf->regs->pow[win->idx];

		spin_lock(&pf->lock);
		win->cpu_addr = ((u64)in_be32(&ow_regs->powbar)) << 12;
		win->pci_addr =
			((u64)in_be32(&ow_regs->potear)) << 44 |
			((u64)in_be32(&ow_regs->potar)) << 12;
		win->attr = in_be32(&ow_regs->powar);
		spin_unlock(&pf->lock);

		win->size = attr_to_size(win->attr);
	} else {
		/* VF outbound windows */
		struct vf_owta_regs *ta;

		ow_regs = &pf->vf_regs->vfow[win->idx];
		ta = &pf->vf_regs->vfowta[win->idx][ep->idx - 1];

		spin_lock(&pf->lock);
		win->cpu_addr = ((u64)in_be32(&ow_regs->powbar)) << 12;
		win->pci_addr =
			((u64)in_be32(&ta->tear)) << 44 |
			((u64)in_be32(&ta->tar)) << 12;
		win->attr = in_be32(&ow_regs->powar);
		spin_unlock(&pf->lock);

		win->size = attr_to_size(win->attr);
		win->cpu_addr += win->size * (ep->idx - 1);
	}

	return 0;
}

static int fsl_pci_ep_get_msixobwin(struct pci_ep_dev *ep,
				    struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_outbound_window_regs *ow_regs;


	if (ep->type != PCI_EP_TYPE_PF)
		return -EINVAL;

	ow_regs = &pf->regs->msixow;

	spin_lock(&pf->lock);
	win->cpu_addr = ((u64)in_be32(&ow_regs->powbar)) << 12;
	win->pci_addr = 0;
	win->attr = in_be32(&ow_regs->powar);
	spin_unlock(&pf->lock);

	win->size = attr_to_size(win->attr);

	return 0;
}

static int fsl_pci_ep_get_vfibwin(struct pci_ep_dev *ep,
				struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_inbound_window_regs *iw_regs;

	if (win->idx >= pf->vf_iw_num)
		return -EINVAL;

	if (ep->type != PCI_EP_TYPE_PF)
		return -EINVAL;

	/* VF inbound windows */
	iw_regs = &pf->vf_regs->vfiw[pf->vf_iw_num - win->idx - 1];

	spin_lock(&pf->lock);
	win->cpu_addr =
		((u64)in_be32(&iw_regs->pitar)) << 12;
	win->attr = in_be32(&iw_regs->piwar);
	spin_unlock(&pf->lock);

	win->size = attr_to_size(win->attr);
	win->pci_addr = fsl_pci_pf_read_vfbar(pf, win->idx);

	return 0;
}

static int fsl_pci_ep_get_vfobwin(struct pci_ep_dev *ep,
				  struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_outbound_window_regs *ow_regs;
	struct vf_owta_regs *ta;

	if (win->idx >= pf->vf_ow_num)
		return -EINVAL;

	if (ep->type != PCI_EP_TYPE_PF)
		return -EINVAL;

	ow_regs = &pf->vf_regs->vfow[win->idx];
	ta = &pf->vf_regs->vfowta[win->idx][0];

	spin_lock(&pf->lock);
	win->cpu_addr = ((u64)in_be32(&ow_regs->powbar)) << 12;
	win->pci_addr =
		((u64)in_be32(&ta->tear)) << 44 |
		((u64)in_be32(&ta->tar)) << 12;
	win->attr = in_be32(&ow_regs->powar);
	spin_unlock(&pf->lock);

	win->size = attr_to_size(win->attr);

	return 0;
}

static int fsl_pci_ep_get_reg_win(struct pci_ep_dev *ep,
				  struct pci_ep_win *win)
{
	win->cpu_addr = ep->pf->regs_rs.start;
	win->size = resource_size(&ep->pf->regs_rs);
	win->attr = 0x80000000;	/* Enabled */

	return 0;
}

static int fsl_pci_ep_get_mem_win(struct pci_ep_dev *ep,
				  struct pci_ep_win *win)
{
	if (ep->type == PCI_EP_TYPE_PF) {
		if (win->idx != 0)
			goto _err;
		win->cpu_addr = ep->pf->mem_resources[win->idx].start;
		win->size = resource_size(&ep->pf->mem_resources[win->idx]);
		win->attr = 0x80000000;	/* Enabled */
	}

	return 0;
_err:
	win->attr = 0;
	return -EINVAL;
}

int fsl_pci_ep_get_win(struct pci_ep_dev *ep, struct pci_ep_win *win)
{
	int ret;

	switch (win->type) {
	case PCI_EP_REGION_IBWIN:
		ret = fsl_pci_ep_get_ibwin(ep, win);
		break;
	case PCI_EP_REGION_OBWIN:
		ret = fsl_pci_ep_get_obwin(ep, win);
		break;
	case PCI_EP_REGION_VF_IBWIN:
		ret = fsl_pci_ep_get_vfibwin(ep, win);
		break;
	case PCI_EP_REGION_VF_OBWIN:
		ret = fsl_pci_ep_get_vfobwin(ep, win);
		break;
	case PCI_EP_REGION_REGS:
		ret = fsl_pci_ep_get_reg_win(ep, win);
		break;
	case PCI_EP_REGION_MEM:
		ret = fsl_pci_ep_get_mem_win(ep, win);
		break;
	case PCI_EP_REGION_MSIX_OBWIN:
		ret = fsl_pci_ep_get_msixobwin(ep, win);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int fsl_pci_ep_set_ibwin(struct pci_ep_dev *ep,
				struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_inbound_window_regs *iw_regs;

	if (win->idx >= ep->iw_num)
		return -EINVAL;

	if (ep->type == PCI_EP_TYPE_PF) {
		u32 attr;

		iw_regs = &pf->regs->piw[ep->iw_num - win->idx - 1];

		if (win->size < PCI_ATMU_MIN_SIZE)
			attr = 0;
		else
			attr = PIWAR_EN | PIWAR_PF | PIWAR_TGI_LOCAL |
			       PIWAR_READ_SNOOP | PIWAR_WRITE_SNOOP |
			       (ilog2(win->size) - 1);

		/* Setup inbound memory window */
		spin_lock(&pf->lock);
		out_be32(&iw_regs->pitar, win->cpu_addr >> 12);
		if (win->attr) /* use the specific attribute */
			out_be32(&iw_regs->piwar, win->attr);
		else /* use the default attribute */
			out_be32(&iw_regs->piwar, attr);
		spin_unlock(&pf->lock);
	} else {
		/*
		 * All VFs use the common VF inbound windows
		 * So an EP(VF) device can not change VF Inbound windows
		 * Only PF device can call fsl_pci_ep_set_vfibwin() to
		 * change VF's inbound windows
		 */
		return -EINVAL;
	}

	return 0;
}

static int fsl_pci_ep_set_obwin(struct pci_ep_dev *ep,
				struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_outbound_window_regs *ow_regs;

	if (win->idx >= ep->iw_num)
		return -EINVAL;

	ow_regs = &pf->regs->pow[win->idx];

	if (ep->type == PCI_EP_TYPE_PF) {
		int bits;
		u32 attr; /* enable & mem R/W */

		if (win->size < PCI_ATMU_MIN_SIZE)
			attr = 0;
		else {
			bits = min(ilog2(win->size),
				   __ffs(win->pci_addr | win->cpu_addr));
			attr = PCI_OB_WIN_MEM_ATTR | (bits - 1);
		}
		spin_lock(&pf->lock);
		out_be32(&ow_regs->potar, win->pci_addr >> 12);
		out_be32(&ow_regs->potear, win->pci_addr >> 44);
		out_be32(&ow_regs->powbar, win->cpu_addr >> 12);
		if (win->attr) /* use the specific attribute */
			out_be32(&ow_regs->powar, win->attr);
		else /* use the default attribute */
			out_be32(&ow_regs->powar, attr);
		spin_unlock(&pf->lock);
	} else {
		/*
		 * For an outbound window, each VF has a translation
		 * register separately and other registers are shared
		 * by all VFs. So an EP device can only change owtar
		 */
		struct vf_owta_regs *ta;

		ta = &pf->vf_regs->vfowta[win->idx][ep->idx - 1];
		out_be32(&ta->tar, win->pci_addr >> 12);
		out_be32(&ta->tear, win->pci_addr >> 44);
	}

	return 0;
}

static int fsl_pci_ep_set_msixobwin(struct pci_ep_dev *ep,
				    struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_outbound_window_regs *ow_regs;
	u32 flags = 0x8000400b; /* enable 4K MSIX outbound window */

	ow_regs = &pf->regs->msixow;

	if (ep->type != PCI_EP_TYPE_PF)
		return -EINVAL;

	if (win->size != FSL_PCI_EP_MSIX_OW_SIZE)
		return -EINVAL;

	spin_lock(&pf->lock);
	out_be32(&ow_regs->powbar, win->cpu_addr >> 12);
	if (win->attr) /* use the specific attribute */
		out_be32(&ow_regs->powar, win->attr);
	else /* use the default attribute */
		out_be32(&ow_regs->powar, flags);
	spin_unlock(&pf->lock);

	return 0;
}

static int fsl_pci_ep_set_vfibwin(struct pci_ep_dev *ep,
				struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_inbound_window_regs *iw_regs;
	u32 attr;

	if (win->idx >= ep->iw_num)
		return -EINVAL;

	/* Only PF can set VF ATMU */
	if (ep->type != PCI_EP_TYPE_PF)
		return -EINVAL;

	if (win->size < PCI_ATMU_MIN_SIZE)
		attr = 0;
	else
		attr = PIWAR_EN | PIWAR_PF | PIWAR_TGI_LOCAL |
		       PIWAR_READ_SNOOP | PIWAR_WRITE_SNOOP |
		       (ilog2(win->size) - 1);

	iw_regs = &pf->vf_regs->vfiw[ep->iw_num - win->idx - 1];

	/* Setup inbound memory window */
	spin_lock(&pf->lock);
	out_be32(&iw_regs->pitar, win->cpu_addr >> 12);
	if (win->attr) /* use the specified attribute */
		out_be32(&iw_regs->piwar, win->attr);
	else /* use the default attribute */
		out_be32(&iw_regs->piwar, attr);
	spin_unlock(&pf->lock);

	return 0;
}

static int fsl_pci_ep_set_vfobwin(struct pci_ep_dev *ep,
				  struct pci_ep_win *win)
{
	struct pci_pf_dev *pf = ep->pf;
	struct pci_outbound_window_regs *ow_regs;
	int bits;
	u32 attr;

	if (win->idx >= ep->iw_num)
		return -EINVAL;

	/* Only PF can set VF ATMU */
	if (ep->type != PCI_EP_TYPE_PF)
		return -EINVAL;

	ow_regs = &pf->vf_regs->vfow[win->idx];

	if (win->size < PCI_ATMU_MIN_SIZE)
		attr = 0;
	else {
		bits = min(ilog2(win->size),
			   __ffs(win->pci_addr | win->cpu_addr));
		attr = PCI_OB_WIN_MEM_ATTR | (bits - 1);
	}

	spin_lock(&pf->lock);
	out_be32(&ow_regs->powbar, win->cpu_addr >> 12);
	if (win->attr) /* use the specified attribute */
		out_be32(&ow_regs->powar, win->attr);
	else /* use the default attribute */
		out_be32(&ow_regs->powar, attr);
	spin_unlock(&pf->lock);

	return 0;
}

int fsl_pci_ep_set_win(struct pci_ep_dev *ep, struct pci_ep_win *win)
{
	int ret;

	switch (win->type) {
	case PCI_EP_REGION_IBWIN:
		ret = fsl_pci_ep_set_ibwin(ep, win);
		break;
	case PCI_EP_REGION_OBWIN:
		ret = fsl_pci_ep_set_obwin(ep, win);
		break;
	case PCI_EP_REGION_VF_IBWIN:
		ret = fsl_pci_ep_set_vfibwin(ep, win);
		break;
	case PCI_EP_REGION_VF_OBWIN:
		ret = fsl_pci_ep_set_vfobwin(ep, win);
		break;
	case PCI_EP_REGION_MSIX_OBWIN:
		ret = fsl_pci_ep_set_msixobwin(ep, win);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static struct pci_ep_dev *
fsl_pci_ep_find(struct pci_controller *controller, int devfn)
{
	struct pci_pf_dev *pf;
	struct pci_ep_dev *tmp, *ep = NULL;
	struct list_head *pf_list = controller->private_data;

	list_for_each_entry(pf, pf_list, node)
		list_for_each_entry(tmp, &pf->ep_list, node)
			if (tmp->devfn == devfn) {
				ep = tmp;
				break;
			}

	return ep;
}

static int fsl_pci_ep_read_config(struct pci_bus *bus, unsigned int devfn,
				  int offset, int len, u32 *val)
{
	struct pci_controller *hose = pci_bus_to_host(bus);
	struct pci_ep_dev *ep;
	volatile void __iomem *cfg_data;
	u32 type, reg;
	int real_devfn, pf_idx, vf_idx;

	real_devfn = bus->number << 8 | devfn;

	if (real_devfn < MULTI_FUNCTION_NUM) {
		/* physical function */
		pf_idx = real_devfn;
		vf_idx = 0;
		type = CONFIG_ACCESS_TYPE_NO_SRIOV;
	} else {
		/* virtual function */
		ep = fsl_pci_ep_find(hose, real_devfn);
		if (!ep || ep->type != PCI_EP_TYPE_VF)
			return PCIBIOS_DEVICE_NOT_FOUND;

		pf_idx = ep->pf->idx;
		vf_idx = ep->idx - 1;
		type = CONFIG_ACCESS_TYPE_VF_SRIOV;
	}

	reg = ((offset & 0xf00) << (CONFIG_ACCESS_EXTREG_SHIFT - 8)) |
	      (offset & 0xfc);

	out_be32(hose->cfg_addr, CONFIG_ACCESS_ENABLE | reg |
		 type << CONFIG_ACCESS_TYPE_SHIFT |
		 vf_idx << CONFIG_ACCESS_VF_SHIFT |
		 pf_idx << CONFIG_ACCESS_PF_SHIFT);

	/*
	 * Note: the caller has already checked that offset is
	 * suitably aligned and that len is 1, 2 or 4.
	 */
	cfg_data = hose->cfg_data + (offset & 3);
	switch (len) {
	case 1:
		*val = in_8(cfg_data);
		break;
	case 2:
		*val = in_le16(cfg_data);
		break;
	default:
		*val = in_le32(cfg_data);
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int fsl_pci_ep_write_config(struct pci_bus *bus, unsigned int devfn,
				   int offset, int len, u32 val)
{
	/* EP should not configure any register on config field */
	return PCIBIOS_SET_FAILED;
}

static struct pci_ops fsl_pci_ep_ops = {
	.read = fsl_pci_ep_read_config,
	.write = fsl_pci_ep_write_config,
};

static void fsl_pci_ep_access_create(struct pci_controller *host,
				     resource_size_t cfg_addr,
				     resource_size_t cfg_data, u32 flags)
{
	resource_size_t base = cfg_addr & PAGE_MASK;
	void __iomem *mbase;

	mbase = ioremap(base, PAGE_SIZE);
	host->cfg_addr = mbase + (cfg_addr & ~PAGE_MASK);
	if ((cfg_data & PAGE_MASK) != base)
		mbase = ioremap(cfg_data & PAGE_MASK, PAGE_SIZE);
	host->cfg_data = mbase + (cfg_data & ~PAGE_MASK);
	host->ops = &fsl_pci_ep_ops;
	host->indirect_type = flags;
}

static void fsl_pci_ep_access_free(struct pci_controller *host)
{
	/* unmap cfg_data & cfg_addr separately if not on same page */
	if (((unsigned long)host->cfg_data & PAGE_MASK) !=
	    ((unsigned long)host->cfg_addr & PAGE_MASK))
		iounmap(host->cfg_data);
	iounmap(host->cfg_addr);
}

static ssize_t
inbound_windows_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct pci_ep_dev *ep = container_of(dev, struct pci_ep_dev, dev);
	struct pci_ep_win win;
	char *str = buf;
	int i;

	for (i = 0; i < ep->iw_num; i++) {
		win.type = PCI_EP_REGION_IBWIN;
		win.idx = i;
		fsl_pci_ep_get_ibwin(ep, &win);
		str += sprintf(str, "Inbound Window%d:\n"
				"\tcpu_addr:0x%016llx pci_addr:0x%016llx\n"
				"\twin_size:0x%016llx attribute:0x%08x\n",
				i,
				win.cpu_addr,
				win.pci_addr,
				win.size,
				win.attr);
	}

	if (ep->type == PCI_EP_TYPE_PF) {
		struct pci_pf_dev *pf = ep->pf;

		if (pf->vf_regs) {
			for (i = 0; i < pf->vf_iw_num; i++) {
				win.idx = i;
				fsl_pci_ep_get_vfibwin(ep, &win);
				str += sprintf(str, "VF Inbound Window%d:\n"
					"\tcpu_addr:0x%016llx"
					" pci_addr:0x%016llx\n"
					"\twin_size:0x%016llx"
					" attribute:0x%08x\n",
					i,
					win.cpu_addr,
					win.pci_addr,
					win.size,
					win.attr);
			}
		}
	}

	return str - buf;
}

static ssize_t
outbound_windows_show(struct device *dev,
		      struct device_attribute *attr, char *buf)
{
	struct pci_ep_dev *ep = container_of(dev, struct pci_ep_dev, dev);
	struct pci_ep_win win;
	char *str = buf;
	int i;

	for (i = 0; i < ep->ow_num; i++) {
		win.type = PCI_EP_REGION_OBWIN;
		win.idx = i;
		fsl_pci_ep_get_obwin(ep, &win);
		str += sprintf(str, "Outbound Window%d:\n"
				"\tcpu_addr:0x%016llx pci_addr:0x%016llx\n"
				"\twin_size:0x%016llx attribute:0x%08x\n",
				i,
				win.cpu_addr,
				win.pci_addr,
				win.size,
				win.attr);
	}

	if (ep->type == PCI_EP_TYPE_PF) {
		struct pci_pf_dev *pf = ep->pf;
		if (pf->msix_enable) {
			fsl_pci_ep_get_msixobwin(ep, &win);
				str += sprintf(str, "MSIX Outbound Window:\n"
						"\tcpu_addr:0x%016llx\n"
						"\twin_size:0x%016llx"
						" attribute:0x%08x\n",
						win.cpu_addr,
						win.size,
						win.attr);
		}

		if (pf->vf_regs) {
			for (i = 0; i < pf->vf_ow_num; i++) {
				win.idx = i;
				fsl_pci_ep_get_vfobwin(ep, &win);
				str += sprintf(str, "VF Outbound Window%d:\n"
					"\tcpu_addr:0x%016llx"
					" pci_addr:0x%016llx\n"
					"\twin_size:0x%016llx"
					" attribute:0x%08x\n",
					i,
					win.cpu_addr,
					win.pci_addr,
					win.size,
					win.attr);
			}
		}
	}

	return str - buf;
}

static ssize_t
pf_idx_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pci_ep_dev *ep = container_of(dev, struct pci_ep_dev, dev);

	return sprintf(buf, "%d\n", ep->pf->idx);
}

static ssize_t
vf_idx_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pci_ep_dev *ep = container_of(dev, struct pci_ep_dev, dev);

	return sprintf(buf, "%d\n", ep->idx);
}

static ssize_t
ep_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pci_ep_dev *ep = container_of(dev, struct pci_ep_dev, dev);

	return sprintf(buf, "%s\n",
		       ep->type == PCI_EP_TYPE_PF ? "PF" : "VF");
}

static ssize_t
msix_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pci_ep_dev *ep = container_of(dev, struct pci_ep_dev, dev);
	char *str = buf;
	int i;

	if (!ep->pf->msix_enable) {
		str += sprintf(str, "Not support MSIX\n");
		return str - buf;
	}

	for (i = 0; i < PCIE_MSIX_VECTOR_MAX_NUM; i++) {
		str += sprintf(str,
				"MSIX venctor %d:\n"
				"\tcontrol:0x%x data:0x%08x addr:0x%016llx\n",
				i,
				fsl_pci_msix_vector_read(ep, i,
						PAOR_MSIX_CONTROL_IDX),
				fsl_pci_msix_vector_read(ep, i,
						PAOR_MSIX_MSG_DATA_IDX),
				(u64) fsl_pci_msix_vector_read(ep, i,
						PAOR_MSIX_MSG_UADDR_IDX) << 32 |
					fsl_pci_msix_vector_read(ep, i,
						PAOR_MSIX_MSG_LADDR_IDX));
	}

	return str - buf;
}

static DEVICE_ATTR_RO(ep_type);
static DEVICE_ATTR_RO(pf_idx);
static DEVICE_ATTR_RO(vf_idx);
static DEVICE_ATTR_RO(inbound_windows);
static DEVICE_ATTR_RO(outbound_windows);
static DEVICE_ATTR_RO(msix);

static struct attribute *pci_ep_attrs[] = {
	&dev_attr_ep_type.attr,
	&dev_attr_pf_idx.attr,
	&dev_attr_vf_idx.attr,
	&dev_attr_inbound_windows.attr,
	&dev_attr_outbound_windows.attr,
	&dev_attr_msix.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pci_ep);

static void pci_ep_dev_release(struct device *dev)
{
	/* nothing to do */
}

static void pci_ep_class_release(struct class *cls)
{
	/* nothing to do */
}

static struct class pci_ep_class = {
	.name = "pci_ep",
	.dev_groups = pci_ep_groups,
	.dev_release = pci_ep_dev_release,
	.class_release = pci_ep_class_release,
};

static int pci_ep_class_init(void)
{
	int ret;

	ret = class_register(&pci_ep_class);
	if (ret) {
		pr_err("class_register failed for pci ep\n");
		return -EINVAL;
	}

	return 0;
}

static void pci_ep_class_free(void)
{
	class_unregister(&pci_ep_class);
}

static struct pci_bus *
create_dummy_pci_bus(void *sysdata, struct pci_ops *ops, int busnum)
{
	struct pci_bus *b;

	b = kzalloc(sizeof(*b), GFP_KERNEL);
	if (!b)
		return NULL;

	INIT_LIST_HEAD(&b->node);
	INIT_LIST_HEAD(&b->children);
	INIT_LIST_HEAD(&b->devices);
	INIT_LIST_HEAD(&b->slots);
	INIT_LIST_HEAD(&b->resources);
	b->max_bus_speed = PCI_SPEED_UNKNOWN;
	b->cur_bus_speed = PCI_SPEED_UNKNOWN;
	b->sysdata = sysdata;
	b->ops = ops;
	b->number = busnum;
	dev_set_name(&b->dev, "%04x:%02x", pci_domain_nr(b), busnum);

	return b;
}

static inline u8 fsl_pci_ep_devfn(struct pci_ep_dev *ep)
{
	struct pci_pf_dev *pf = ep->pf;

	if (ep->type == PCI_EP_TYPE_PF)
		return pf->idx;
	else
		return (pf->idx + pf->vf_offset +
			pf->vf_stride * (ep->idx - 1)) & 0xff;
}

static inline u8 fsl_pci_ep_bus(struct pci_ep_dev *ep)
{
	struct pci_pf_dev *pf = ep->pf;

	if (ep->type == PCI_EP_TYPE_PF)
		return 0;
	else
		return (pf->idx + pf->vf_offset +
			pf->vf_stride * (ep->idx - 1)) >> 8;
}

static struct pci_bus *fsl_pci_ep_add_bus(struct pci_bus *bus, int busnr)
{
	struct pci_bus *child;

	if (bus->number == busnr)
		return bus;

	list_for_each_entry(child, &bus->children, children) {
		if (child->number == busnr)
			return child;
	}

	child = create_dummy_pci_bus(bus->sysdata, bus->ops, busnr);
	if (!child)
		return NULL;

	child->self = bus->self;

	list_add_tail(&child->children, &bus->children);

	return child;
}

static void fsl_pci_bus_free(struct pci_bus *bus)
{
	struct pci_bus *child, *tmp;

	list_for_each_entry_safe(child, tmp, &bus->children, children) {
		list_del(&child->children);
		kfree(child);
	}

	kfree(bus);
}

static struct pci_ep_dev *fsl_pci_ep_alloc(struct pci_pf_dev *pf, int idx)
{
	struct pci_ep_dev *ep;

	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return NULL;

	ep->idx = idx;
	ep->pf = pf;

	if (idx == 0) {
		ep->type = PCI_EP_TYPE_PF;
		ep->iw_num = pf->iw_num;
		ep->ow_num = pf->ow_num;
		ep->devfn = pf->idx;
	} else {
		ep->type = PCI_EP_TYPE_VF;
		ep->iw_num = pf->vf_iw_num;
		ep->ow_num = pf->vf_ow_num;
		ep->devfn = pf->idx + pf->vf_offset +
			    pf->vf_stride * (ep->idx - 1);
	}

	list_add_tail(&ep->node, &pf->ep_list);

	return ep;
}

static int fsl_pci_ep_setup(struct pci_ep_dev *ep)
{
	struct pci_dev *pdev;

	if (!ep || !ep->pf)
		return -EINVAL;

	if (!ep->pdev) {
		pdev = alloc_pci_dev();
		if (!pdev)
			return -ENOMEM;

		ep->pdev = pdev;
		pdev->bus = fsl_pci_ep_add_bus(ep->pf->pbus,
					       fsl_pci_ep_bus(ep));
		if (!pdev->bus)
			return -ENOMEM;

		pdev->devfn = fsl_pci_ep_devfn(ep);
		pdev->sysdata = ep;
		pdev->vendor = ep->pf->pdev->vendor;
		pdev->device = ep->pf->pdev->device;
		dev_set_name(&pdev->dev, "%04x:%02x:%02x.%d",
			     pci_domain_nr(pdev->bus),
			     pdev->bus->number, PCI_SLOT(pdev->devfn),
			     PCI_FUNC(pdev->devfn));
	}

	ep->dev.parent = ep->pf->parent;
	ep->dev.of_node = ep->dev.parent->of_node;
	ep->dev.iommu_group = iommu_group_get(ep->pf->parent);
	ep->dev.bus = NULL;
	ep->dev.class = &pci_ep_class;
	if (ep->type == PCI_EP_TYPE_PF)
		dev_set_name(&ep->dev, "pci%d-pf%d",
			     ep->pf->host->global_number,
			     ep->pf->idx);
	else
		dev_set_name(&ep->dev, "pci%d-pf%d-vf%d",
			     ep->pf->host->global_number,
			     ep->pf->idx,
			     ep->idx);

	if (device_register(&ep->dev))
		return -EINVAL;
	ep->registered = 1;

	fsl_pci_ep_vfio_init(ep);

	return 0;
}

static void fsl_pci_ep_free(struct pci_ep_dev *ep)
{
	if (!ep)
		return;

	fsl_pci_ep_vfio_remove(ep);

	kfree(ep->pdev);

	if (ep->registered)
		device_unregister(&ep->dev);

	list_del(&ep->node);

	kfree(ep);
}

static void fsl_pci_pf_cfg_ready(struct pci_pf_dev *pf)
{
	#define FSL_PCIE_CFG_RDY	0x4b0
	#define CFG_READY		1

	/* PCIe - set CFG_READY bit of Configuration Ready Register */
	if (!pf->regs)
		return;

	if (in_be32(&pf->regs->block_rev1) >= PCIE_IP_REV_3_0)
		out_be32(&pf->regs->pex_config,
			 in_be32(&pf->regs->pex_config) | CFG_READY);
	else
		pci_write_config_byte(pf->pdev, FSL_PCIE_CFG_RDY,
				      CFG_READY);
}

static int fsl_pci_pf_iov_init(struct pci_pf_dev *pf)
{
	int pos;
	u16 total, offset, stride;
	struct pci_dev *dev = pf->pdev;

	if (!pci_is_pcie(dev))
		return -ENODEV;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos)
		return 0;

	pci_read_config_word(dev, pos + PCI_SRIOV_TOTAL_VF, &total);
	if (!total)
		return 0;

	pci_read_config_word(dev, pos + PCI_SRIOV_VF_OFFSET, &offset);
	pci_read_config_word(dev, pos + PCI_SRIOV_VF_STRIDE, &stride);
	if (!offset || (total > 1 && !stride))
		return -EIO;

	pf->vf_total = total;
	pf->vf_offset = offset;
	pf->vf_stride = stride;
	pf->vf_num = pf->vf_total;
	pf->vf_iw_num = VF_IW_NUM;
	pf->vf_ow_num = VF_OW_NUM;
	pf->vf_pos = pos;
	pf->vf_regs = (void *)pf->regs + VF_ATMU_OFFSET;

	return pf->vf_num;
}

static int fsl_pci_pf_atmu_init(struct pci_pf_dev *pf)
{
	struct pci_ep_dev *ep = pf->pdev->sysdata;
	struct pci_ep_win win;
	int i, bits;
	int win_idx = 3, start_idx = 1, end_idx = 4;
	u64 sz, reserve_sz = 0, free_sz;

	if (in_be32(&pf->regs->block_rev1) >= PCIE_IP_REV_2_2) {
		win_idx = 2;
		start_idx = 0;
		end_idx = 3;
	}

	sz = resource_size(&pf->mem_resources[0]) / (pf->vf_total + 1);
	bits = ilog2(sz);
	sz = 1ull << bits;

	if (pf->vf_regs) {
		/* Disable all VF windows */
		for (i = 0; i < pf->vf_ow_num; i++)
			out_be32(&pf->vf_regs->vfow[i].powar, 0);
		for (i = 0; i < pf->vf_iw_num - 1; i++)
			out_be32(&pf->vf_regs->vfiw[i].piwar, 0);

		/* Setup VF outbound windows*/
		win.cpu_addr = pf->mem_resources[0].start;
		win.pci_addr = win.cpu_addr - pf->mem_offset[0];
		win.size = sz;
		win.attr = 0;
		win.idx = 0;
		fsl_pci_ep_set_vfobwin(ep, &win);

		/* Setup VF MSIX inbound windows*/
		if (pf->msix_enable) {
			win.cpu_addr = 0;
			win.pci_addr = 0;
			win.size = FSL_PCI_EP_MSIX_IW_SIZE;
			win.attr = FSL_PCI_MSIX_IW_ATTR |
				   (ilog2(win.size) - 1);
			win.idx = PCI_EP_MSI_WIN_INDEX;
			fsl_pci_ep_set_vfibwin(ep, &win);
		}
	}

	/* Disable all PF windows (except powar0 since it's ignored) */
	for (i = 1; i < pf->ow_num; i++)
		out_be32(&pf->regs->pow[i].powar, 0);
	for (i = start_idx; i < end_idx; i++)
		out_be32(&pf->regs->piw[i].piwar, 0);

	/* Setup PF MSIX outbound windows */
	if (pf->msix_enable) {
		win.cpu_addr = pf->mem_resources[0].end -
			       FSL_PCI_EP_MSIX_OW_SIZE + 1;
		win.size = FSL_PCI_EP_MSIX_OW_SIZE;
		win.attr = 0;
		win.idx = 0;
		fsl_pci_ep_set_msixobwin(ep, &win);
		reserve_sz = FSL_PCI_EP_MSIX_OW_SIZE;
	}

	/* Setup PF outbound windows */
	win.cpu_addr = pf->mem_resources[0].start + pf->vf_total * sz;
	win.pci_addr = win.cpu_addr - pf->mem_offset[0];
	free_sz = pf->mem_resources[0].end - win.cpu_addr - reserve_sz + 1;
	if (free_sz < sz) {
		bits = ilog2(free_sz);
		sz = 1ull << bits;
	}
	win.size = sz;
	win.attr = 0;
	win.idx = 1;
	fsl_pci_ep_set_obwin(ep, &win);

	/* Setup VF MSIX inbound windows*/
	if (pf->msix_enable) {
		win.cpu_addr = 0;
		win.pci_addr = 0;
		win.size = FSL_PCI_EP_MSIX_IW_SIZE;
		win.attr = FSL_PCI_MSIX_IW_ATTR |
			   (ilog2(win.size) - 1);
		win.idx = PCI_EP_MSI_WIN_INDEX;
		fsl_pci_ep_set_ibwin(ep, &win);
	}

	return 0;
}

struct pci_pf_dev *fsl_pci_pf_alloc(struct pci_dev *pdev,
				    struct list_head *pf_list)
{
	struct pci_pf_dev *pf;

	if (!pdev->is_physfn)
		return NULL;

	pf = kzalloc(sizeof(*pf), GFP_KERNEL);
	if (!pf)
		return NULL;

	pf->pbus = pdev->bus;
	pf->pdev = pdev;
	pf->host = pci_bus_to_host(pdev->bus);
	pf->idx = pdev->devfn;
	pf->ow_num = FSL_PCI_EP_OW_NUM;
	pf->iw_num = FSL_PCI_EP_BAR_NUM;

	spin_lock_init(&pf->lock);
	INIT_LIST_HEAD(&pf->ep_list);

	spin_lock(&pci_ep_spinlock);
	list_add_tail(&pf->node, pf_list);
	spin_unlock(&pci_ep_spinlock);

	return pf;
}

void fsl_pci_pf_free(struct pci_pf_dev *pf)
{
	struct pci_ep_dev *ep, *tmp;

	if (!pf)
		return;
	list_for_each_entry_safe(ep, tmp, &pf->ep_list, node)
		fsl_pci_ep_free(ep);

	if (pf->regs)
		iounmap(pf->regs);

	spin_lock(&pci_ep_spinlock);
	list_del(&pf->node);
	spin_unlock(&pci_ep_spinlock);

	kfree(pf);
}

static void fsl_pci_pfs_free(struct list_head *pf_list)
{
	struct pci_pf_dev *pf, *tmp;

	list_for_each_entry_safe(pf, tmp, pf_list, node)
			fsl_pci_pf_free(pf);
}

int fsl_pci_pf_setup(struct pci_bus *bus, int pf_num)
{
	struct pci_controller *host;
	struct pci_pf_dev *pf;
	struct pci_ep_dev *ep;
	struct pci_dev *pdev;
	struct resource rsrc;
	struct list_head *pf_list;
	size_t regs_size, mem_size;
	int pf_idx, pos, i;

	host = pci_bus_to_host(bus);
	pf_list = host->private_data;

	/* Fetch host bridge registers address */
	if (of_address_to_resource(host->dn, 0, &rsrc)) {
		pr_warn("Can't get pci register base!");
		return -ENOMEM;
	}

	if (pf_num > 1)
		regs_size = PCI_EP_PF_OFFSET;
	else
		regs_size = resource_size(&rsrc);

	if (resource_size(&rsrc) < (regs_size * pf_num))
		return -EINVAL;

	for (pf_idx = 0; pf_idx < pf_num; pf_idx++) {
		pdev = alloc_pci_dev();
		if (!pdev)
			goto _err;
		pdev->bus = bus;
		pdev->devfn = pf_idx;
		pdev->is_physfn = 1;
		pdev->hdr_type = PCI_HEADER_TYPE_NORMAL;
		pos = pci_find_capability(pdev, PCI_CAP_ID_EXP);
		if (pos) {
			pdev->pcie_cap = pos;
			pdev->cfg_size = 0x1000;
		} else
			pdev->cfg_size = 0x100;

		dev_set_name(&pdev->dev, "%04x:%02x:%02x.%d",
			     pci_domain_nr(pdev->bus),
			     pdev->bus->number, PCI_SLOT(pdev->devfn),
			     PCI_FUNC(pdev->devfn));

		if (!bus->self)
			bus->self = pdev;

		pf = fsl_pci_pf_alloc(pdev, pf_list);
		if (!pf)
			goto _err;

		pf->parent = host->parent;

		ep = fsl_pci_ep_alloc(pf, 0);

		ep->pdev = pdev;
		pdev->sysdata = ep;

		pf->regs_rs.start = rsrc.start + pf->idx * regs_size;
		pf->regs_rs.end = pf->regs_rs.start + regs_size - 1;

		pf->regs = ioremap(pf->regs_rs.start, regs_size);
		if (!pf->regs) {
			pr_err("Unable to map PF%d ccsr regs\n", pf->idx);
			goto _err;
		}

		if (pci_find_capability(pf->pdev, PCI_CAP_ID_MSIX))
			pf->msix_enable = true;

		for (i = 0; i < 3; i++) {
			mem_size = resource_size(&host->mem_resources[i]);
			if (!mem_size)
				continue;
			mem_size = mem_size / pf_num;
			pf->mem_resources[i].start =
				host->mem_resources[i].start +
				mem_size * pf->idx;
			pf->mem_resources[i].end =
				pf->mem_resources[i].start + mem_size - 1;
			pf->mem_offset[i] = host->mem_offset[i];
		}

		fsl_pci_pf_cfg_ready(pf);
		fsl_pci_pf_iov_init(pf);
		fsl_pci_pf_atmu_init(pf);

		for (i = 1; i <= pf->vf_num; i++)
			fsl_pci_ep_alloc(pf, i);

		list_for_each_entry(ep, &pf->ep_list, node)
			fsl_pci_ep_setup(ep);
	}

	return 0;
_err:
	fsl_pci_pfs_free(pf_list);
	return -EINVAL;
}

static void pci_process_of_ranges(struct pci_controller *hose,
				  struct device_node *dev)
{
	const u32 *ranges;
	int rlen;
	int pna = of_n_addr_cells(dev);
	int np = pna + 5;
	int memno = 0;
	u32 pci_space;
	unsigned long long pci_addr, cpu_addr, pci_next, cpu_next, size;
	struct resource *res;

	/* Get ranges property */
	ranges = of_get_property(dev, "ranges", &rlen);
	if (ranges == NULL)
		return;

	/* Parse it */
	while ((rlen -= np * 4) >= 0) {
		/* Read next ranges element */
		pci_space = ranges[0];
		pci_addr = of_read_number(ranges + 1, 2);
		cpu_addr = of_translate_address(dev, ranges + 3);
		size = of_read_number(ranges + pna + 3, 2);
		ranges += np;

		if (cpu_addr == OF_BAD_ADDR || size == 0)
			continue;

		/* Now consume following elements while they are contiguous */
		for (; rlen >= np * sizeof(u32);
		     ranges += np, rlen -= np * 4) {
			if (ranges[0] != pci_space)
				break;
			pci_next = of_read_number(ranges + 1, 2);
			cpu_next = of_translate_address(dev, ranges + 3);
			if (pci_next != pci_addr + size ||
			    cpu_next != cpu_addr + size)
				break;
			size += of_read_number(ranges + pna + 3, 2);
		}

		/* Act based on address space type */
		res = NULL;
		switch ((pci_space >> 24) & 0x3) {
		case 1:		/* PCI IO space */
			break;
		case 2:		/* PCI Memory space */
		case 3:		/* PCI 64 bits Memory space */
			/* We support only 3 memory ranges */
			if (memno >= 3) {
				pr_info(" --> Skipped (too many) !\n");
				continue;
			}

			hose->mem_offset[memno] = cpu_addr - pci_addr;

			/* Build resource */
			res = &hose->mem_resources[memno++];
			res->flags = IORESOURCE_MEM;
			if (pci_space & 0x40000000)
				res->flags |= IORESOURCE_PREFETCH;
			res->start = cpu_addr;
			break;
		}
		if (res != NULL) {
			res->name = dev->full_name;
			res->end = res->start + size - 1;
			res->parent = NULL;
			res->sibling = NULL;
			res->child = NULL;
		}
	}
}

static struct pci_controller *
fsl_pci_ep_controller_alloc(struct device_node *dev)
{
	struct pci_controller *host;
	struct list_head *pf_list;

	host = kzalloc(sizeof(*host), GFP_KERNEL);
	if (!host)
		return NULL;

	pf_list = kzalloc(sizeof(*pf_list), GFP_KERNEL);
	if (!pf_list) {
		kfree(host);
		return NULL;
	}
	INIT_LIST_HEAD(pf_list);
	host->private_data = pf_list;

	spin_lock(&pci_ep_spinlock);
	host->global_number = global_phb_number++;
	list_add_tail(&host->list_node, &pci_ep_controllers);
	spin_unlock(&pci_ep_spinlock);
	host->dn = dev;

	return host;
}

static void fsl_pci_ep_controller_free(struct pci_controller *host)
{
	if (host->private_data) {
		fsl_pci_pfs_free(host->private_data);
		kfree(host->private_data);
	}

	if (host->bus)
		fsl_pci_bus_free(host->bus);

	if (host->ops)
		fsl_pci_ep_access_free(host);

	spin_lock(&pci_ep_spinlock);
	list_del(&host->list_node);
	spin_unlock(&pci_ep_spinlock);

	kfree(host);
}

static int fsl_pci_ep_controller_probe(struct platform_device *device)
{
	struct device_node *dn;
	struct pci_controller *host;
	struct resource rsrc;
	u8 hdr_type;
	int pf_num;
	int err;

	dn = device->dev.of_node;
	if (!of_device_is_available(dn)) {
		pr_warn("%s: disabled\n", dn->full_name);
		return -ENODEV;
	}

	/* Fetch host bridge registers address */
	if (of_address_to_resource(dn, 0, &rsrc))
		return -ENOMEM;

	host = fsl_pci_ep_controller_alloc(dn);
	if (!host)
		return -ENOMEM;

	host->parent = &device->dev;

	fsl_pci_ep_access_create(host, rsrc.start, rsrc.start + 0x4, 0);

	host->first_busno = host->last_busno = 0;

	host->bus = create_dummy_pci_bus(host, host->ops, 0);
	if (!host->bus) {
		err = -ENOMEM;
		goto _err;
	}

	pci_bus_read_config_byte(host->bus, 0,  PCI_HEADER_TYPE, &hdr_type);
	if ((hdr_type & 0x7f) != PCI_HEADER_TYPE_NORMAL) {
		err = -ENODEV;
		goto _err;
	}

	if (hdr_type & 0x80)
		pf_num = MULTI_FUNCTION_NUM;
	else
		pf_num = 1;

	pci_process_of_ranges(host, host->dn);

	fsl_pci_pf_setup(host->bus, pf_num);

	return 0;
_err:
	fsl_pci_ep_controller_free(host);
	return err;
}

static int fsl_pci_ep_controller_remove(struct platform_device *pdev)
{
	struct pci_controller *host, *tmp;

	list_for_each_entry_safe(host, tmp, &pci_ep_controllers, list_node) {
		/*
		 * Because EADC driver has registered private data to the
		 * device drvdata, so we can not register private host
		 * to the device again, and only identify controller
		 * corresponding to platform device via comparing the
		 * device node pointer.
		 */
		if (host->dn == pdev->dev.of_node)
			fsl_pci_ep_controller_free(host);
	}

	return 0;
}

static const struct of_device_id pci_ep_ids[] = {
	{ .compatible = "fsl,qoriq-pcie-v2.1", },
	{ .compatible = "fsl,qoriq-pcie-v2.2", },
	{ .compatible = "fsl,qoriq-pcie-v2.3", },
	{ .compatible = "fsl,qoriq-pcie-v2.4", },
	{ .compatible = "fsl,qoriq-pcie-v3.0", },
	{},
};

static struct platform_driver fsl_pci_ep_driver = {
	.driver = {
		.name = "fsl-pci-ep",
		.of_match_table = pci_ep_ids,
	},
	.probe = fsl_pci_ep_controller_probe,
	.remove = fsl_pci_ep_controller_remove,
};

static int __init fsl_pci_ep_controller_init(void)
{
	int err;

	err = pci_ep_class_init();
	if (err)
		return err;

	err = platform_driver_register(&fsl_pci_ep_driver);
	if (err) {
		pr_err("Unable to register platform driver\n");
		pci_ep_class_free();
		return err;
	}

	return 0;
}

static void __exit fsl_pci_ep_controller_exit(void)
{
	platform_driver_unregister(&fsl_pci_ep_driver);
	pci_ep_class_free();
}

module_init(fsl_pci_ep_controller_init);
module_exit(fsl_pci_ep_controller_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Minghuan Lian <Minghuan.Lian@freescale.com>");
MODULE_DESCRIPTION("Freescale PCI EP driver");
