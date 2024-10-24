/*
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY
  Copyright(c) 2014 Intel Corporation.
  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:
  qat-linux@intel.com

  BSD LICENSE
  Copyright(c) 2014 Intel Corporation.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "adf_cfg_strings.h"
#include "adf_cfg_common.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#ifdef DEFER_UPSTREAM
#include <linux/workqueue.h>

static struct workqueue_struct *event_data_wq;

struct adf_event_data {
	struct adf_accel_dev *accel_dev;
	struct work_struct notify_work;
};
#endif

static int adf_enable_msix(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *pci_dev_info = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 msix_num_entries = 1;

	/* If SR-IOV is disabled, add entries for each bank */
	if (!accel_dev->pf.vf_info) {
		int i;

		msix_num_entries += hw_data->num_banks;
		for (i = 0; i < msix_num_entries; i++)
			pci_dev_info->msix_entries.entries[i].entry = i;
	} else {
		pci_dev_info->msix_entries.entries[0].entry =
			hw_data->num_banks;
	}

	if (pci_enable_msix_exact(pci_dev_info->pci_dev,
				  pci_dev_info->msix_entries.entries,
				  msix_num_entries)) {
		dev_err(&GET_DEV(accel_dev), "Failed to enable MSI-X IRQ(s)\n");
		return -EFAULT;
	}
	return 0;
}

static void adf_disable_msix(struct adf_accel_pci *pci_dev_info)
{
	pci_disable_msix(pci_dev_info->pci_dev);
}

static irqreturn_t adf_msix_isr_bundle(int irq, void *bank_ptr)
{
	struct adf_etr_bank_data *bank = bank_ptr;

	WRITE_CSR_INT_FLAG_AND_COL(bank->csr_addr, bank->bank_number, 0);
	tasklet_hi_schedule(&bank->resp_handler);
	return IRQ_HANDLED;
}

#ifdef DEFER_UPSTREAM
struct reg_info_t {
	size_t	offs;
	char	*name;
};

static struct reg_info_t adf_err_regs[] = {
	{ADF_ERRSOU0, "ERRSOU0"},
	{ADF_ERRSOU1, "ERRSOU1"},
	{ADF_ERRSOU3, "ERRSOU3"},
	{ADF_ERRSOU4, "ERRSOU4"},
	{ADF_ERRSOU5, "ERRSOU5"},
	{ADF_RICPPINTSTS, "RICPPINTSTS"},
	{ADF_RIERRPUSHID, "RIERRPUSHID"},
	{ADF_RIERRPULLID, "RIERRPULLID"},
	{ADF_CPP_CFC_ERR_STATUS, "CPP_CFC_ERR_STATUS"},
	{ADF_CPP_CFC_ERR_PPID, "CPP_CFC_ERR_PPID"},
	{ADF_TICPPINTSTS, "TICPPINTSTS"},
	{ADF_TIERRPUSHID, "TIERRPUSHID"},
	{ADF_TIERRPULLID, "TIERRPULLID"},
	{ADF_SECRAMUERR, "SECRAMUERR"},
	{ADF_SECRAMUERRAD, "SECRAMUERRAD"},
	{ADF_CPPMEMTGTERR, "CPPMEMTGTERR"},
	{ADF_ERRPPID, "ERRPPID"},
};

static u32 adf_get_intstatsssm(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_INTSTATSSM(dev));
}

static u32 adf_get_pperr(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_PPERR(dev));
}

static u32 adf_get_pperrid(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_PPERRID(dev));
}

static u32 adf_get_uerrssmsh(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMSH(dev));
}

static u32 adf_get_uerrssmshad(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMSHAD(dev));
}

static u32 adf_get_uerrssmmmp0(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 0));
}

static u32 adf_get_uerrssmmmp1(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 1));
}

static u32 adf_get_uerrssmmmp2(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 2));
}

static u32 adf_get_uerrssmmmp3(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 3));
}

static u32 adf_get_uerrssmmmp4(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 4));
}

static u32 adf_get_uerrssmmmpad0(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 0));
}

static u32 adf_get_uerrssmmmpad1(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 1));
}

static u32 adf_get_uerrssmmmpad2(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 2));
}

static u32 adf_get_uerrssmmmpad3(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 3));
}

static u32 adf_get_uerrssmmmpad4(void __iomem *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 4));
}

struct reg_array_info_t {
	u32	(*read)(void __iomem *, size_t);
	char	*name;
};

static struct reg_array_info_t adf_cpm_err_regs[] = {
	{adf_get_intstatsssm, "INTSTATSSM"},
	{adf_get_pperr, "PPERR"},
	{adf_get_pperrid, "PPERRID"},
	{adf_get_uerrssmsh, "UERRSSMSH"},
	{adf_get_uerrssmshad, "UERRSSMSHAD"},
	{adf_get_uerrssmmmp0, "UERRSSMMMP0"},
	{adf_get_uerrssmmmp1, "UERRSSMMMP1"},
	{adf_get_uerrssmmmp2, "UERRSSMMMP2"},
	{adf_get_uerrssmmmp3, "UERRSSMMMP3"},
	{adf_get_uerrssmmmp4, "UERRSSMMMP4"},
	{adf_get_uerrssmmmpad0, "UERRSSMMMPAD0"},
	{adf_get_uerrssmmmpad1, "UERRSSMMMPAD1"},
	{adf_get_uerrssmmmpad2, "UERRSSMMMPAD2"},
	{adf_get_uerrssmmmpad3, "UERRSSMMMPAD3"},
	{adf_get_uerrssmmmpad4, "UERRSSMMMPAD4"},
};

static char adf_printf_buf[128] = {0};
static size_t adf_printf_len;

static void adf_print_flush(struct adf_accel_dev *accel_dev)
{
	if (adf_printf_len > 0) {
		dev_err(&GET_DEV(accel_dev), "%.128s\n", adf_printf_buf);
		adf_printf_len = 0;
	}
}

static void adf_print_reg(struct adf_accel_dev *accel_dev,
			  const char *name, size_t idx, u32 val)
{
	adf_printf_len += snprintf(&adf_printf_buf[adf_printf_len],
			sizeof(adf_printf_buf) - adf_printf_len,
			"%s[%zu],%.8x,", name, idx, val);

	if (adf_printf_len >= 80)
		adf_print_flush(accel_dev);
}

static void adf_print_err_registers(void __iomem *pmisc_bar_addr,
				    struct adf_accel_dev *accel_dev,
				    struct adf_hw_device_data *hw_data)
{
	size_t i;
	unsigned int mask;
	u32 val;

	for (i = 0; i < ARRAY_SIZE(adf_err_regs); ++i) {
		val = ADF_CSR_RD(pmisc_bar_addr, adf_err_regs[i].offs);

		adf_print_reg(accel_dev, adf_err_regs[i].name, 0, val);
	}

	for (i = 0; i < ARRAY_SIZE(adf_cpm_err_regs); ++i) {
		size_t cpm;

		cpm = 0;
		mask = hw_data->accel_mask;
		do {
			if (!(mask & 1))
				continue;
			val = adf_cpm_err_regs[i].read(pmisc_bar_addr, cpm);

			adf_print_reg(accel_dev, adf_cpm_err_regs[i].name,
				      cpm, val);
		} while (cpm++, mask >>= 1);
	}

	adf_print_flush(accel_dev);
}

static void adf_log_slice_hang(struct adf_accel_dev *accel_dev,
			       u8 accel_num, char *unit_name, u8 unit_number)
{
	dev_err(&GET_DEV(accel_dev),
		"CPM #%x Slice Hang Detected unit: %s%d.\n",
		accel_num, unit_name, unit_number);
}

static bool adf_handle_slice_hang(struct adf_accel_dev *accel_dev,
				  u8 accel_num, void __iomem *csr)
{
	u32 slice_hang = ADF_CSR_RD(csr, ADF_SLICEHANGSTATUS(accel_num));

	if (!slice_hang)
		return false;

	if (slice_hang & ADF_SLICE_HANG_AUTH0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Auth", 0);
	if (slice_hang & ADF_SLICE_HANG_AUTH1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Auth", 1);
	if (slice_hang & ADF_SLICE_HANG_CPHR0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Cipher", 0);
	if (slice_hang & ADF_SLICE_HANG_CPHR1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Cipher", 1);
	if (slice_hang & ADF_SLICE_HANG_CMP0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Comp", 0);
	if (slice_hang & ADF_SLICE_HANG_CMP1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Comp", 1);
	if (slice_hang & ADF_SLICE_HANG_XLT0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Xlator", 0);
	if (slice_hang & ADF_SLICE_HANG_XLT1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Xlator", 1);
	if (slice_hang & ADF_SLICE_HANG_MMP0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 0);
	if (slice_hang & ADF_SLICE_HANG_MMP1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 1);
	if (slice_hang & ADF_SLICE_HANG_MMP2_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 2);
	if (slice_hang & ADF_SLICE_HANG_MMP3_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 3);
	if (slice_hang & ADF_SLICE_HANG_MMP4_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 4);

	/* Clear the associated interrupt - write 1 to clear */
	ADF_CSR_WR(csr, ADF_SLICEHANGSTATUS(accel_num), slice_hang);

	return true;
}

/**
 * adf_check_slice_hang() - Check slice hang status
 *
 * Return: true if a slice hange interrupt is serviced..
 */
bool adf_check_slice_hang(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *misc_bar =
		&GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	void __iomem *csr = misc_bar->virt_addr;
	u32 errsou3 = ADF_CSR_RD(csr, ADF_ERRSOU3);
	u32 errsou5 = ADF_CSR_RD(csr, ADF_ERRSOU5);
	u32 accel_num;
	bool handled = false;
	u32 errsou[] = {errsou3, errsou3, errsou5, errsou5, errsou5};
	u32 mask[] = {ADF_EMSK3_CPM0_MASK,
		      ADF_EMSK3_CPM1_MASK,
		      ADF_EMSK5_CPM2_MASK,
		      ADF_EMSK5_CPM3_MASK,
		      ADF_EMSK5_CPM4_MASK};
	unsigned int accel_mask;

	accel_num = 0;
	accel_mask = hw_data->accel_mask;
	do {
		if (!(accel_mask & 1))
			continue;
		if (accel_num >= ARRAY_SIZE(errsou)) {
			dev_err(&GET_DEV(accel_dev),
				"Invalid accel_num %d.\n", accel_num);
			break;
		}

		if (errsou[accel_num] & mask[accel_num]) {
			if (ADF_CSR_RD(csr, ADF_INTSTATSSM(accel_num)) &
				       ADF_INTSTATSSM_SHANGERR)
				handled |= adf_handle_slice_hang(accel_dev,
								 accel_num,
								 csr);
		}
	} while (accel_num++, accel_mask >>= 1);

	return handled;
}
EXPORT_SYMBOL_GPL(adf_check_slice_hang);

static void adf_notify_error(struct work_struct *work)
{
	struct adf_event_data *event_data =
		container_of(work, struct adf_event_data, notify_work);
	struct adf_accel_dev *accel_dev = event_data->accel_dev;

	adf_error_notifier((unsigned long) accel_dev);
	kfree(event_data);
}

void adf_error_event(struct adf_accel_dev *accel_dev)
{
	struct adf_event_data *event_data;

	event_data = kzalloc(sizeof(*event_data), GFP_ATOMIC);
	if (!event_data) {
		dev_err(&GET_DEV(accel_dev), "Failed to allocate memory\n");
		return;
	}
	event_data->accel_dev = accel_dev;

	INIT_WORK(&event_data->notify_work, adf_notify_error);
	queue_work(event_data_wq, &event_data->notify_work);
}

#endif
static irqreturn_t adf_msix_isr_ae(int irq, void *dev_ptr)
{
	struct adf_accel_dev *accel_dev = dev_ptr;
#ifdef DEFER_UPSTREAM
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *pmisc =
		&GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	void __iomem *pmisc_bar_addr = pmisc->virt_addr;
#endif

#ifdef CONFIG_PCI_IOV
	/* If SR-IOV is enabled (vf_info is non-NULL), check for VF->PF ints */
	if (accel_dev->pf.vf_info) {
#ifndef DEFER_UPSTREAM
		struct adf_hw_device_data *hw_data = accel_dev->hw_device;
		struct adf_bar *pmisc =
			&GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
		void __iomem *pmisc_bar_addr = pmisc->virt_addr;
#endif
		u32 vf_mask;

		/* Get the interrupt sources triggered by VFs */
		vf_mask = ((ADF_CSR_RD(pmisc_bar_addr, ADF_ERRSOU5) &
			    0x0000FFFF) << 16) |
			  ((ADF_CSR_RD(pmisc_bar_addr, ADF_ERRSOU3) &
			    0x01FFFE00) >> 9);

		if (vf_mask) {
			struct adf_accel_vf_info *vf_info;
			bool irq_handled = false;
			int i;

			/* Disable VF2PF interrupts for VFs with pending ints */
			adf_disable_vf2pf_interrupts(accel_dev, vf_mask);

			/*
			 * Schedule tasklets to handle VF2PF interrupt BHs
			 * unless the VF is malicious and is attempting to
			 * flood the host OS with VF2PF interrupts.
			 */
			for_each_set_bit(i, (const unsigned long *)&vf_mask,
					 (sizeof(vf_mask) * BITS_PER_BYTE)) {
				vf_info = accel_dev->pf.vf_info + i;

				if (!__ratelimit(&vf_info->vf2pf_ratelimit)) {
					dev_info(&GET_DEV(accel_dev),
						 "Too many ints from VF%d\n",
						  vf_info->vf_nr + 1);
					continue;
				}

				/* Tasklet will re-enable ints from this VF */
				tasklet_hi_schedule(&vf_info->vf2pf_bh_tasklet);
				irq_handled = true;
			}

			if (irq_handled)
				return IRQ_HANDLED;
		}
	}
#endif /* CONFIG_PCI_IOV */

#ifdef DEFER_UPSTREAM
	if (hw_data->check_slice_hang &&
			hw_data->check_slice_hang(accel_dev))
		return IRQ_HANDLED;

	if (hw_data->check_uncorrectable_error &&
			hw_data->check_uncorrectable_error(accel_dev)) {
		adf_print_err_registers(pmisc_bar_addr, accel_dev, hw_data);
		if (hw_data->disable_error_interrupts)
			hw_data->disable_error_interrupts(accel_dev);

		tasklet_hi_schedule(&accel_dev->fatal_error_tasklet);
		return IRQ_HANDLED;
	} else {
		dev_dbg(&GET_DEV(accel_dev), "qat_dev%d spurious AE interrupt\n",
			accel_dev->accel_id);
	}
#else
	dev_dbg(&GET_DEV(accel_dev), "qat_dev%d spurious AE interrupt\n",
		accel_dev->accel_id);
#endif

	return IRQ_NONE;
}

static int adf_request_irqs(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *pci_dev_info = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct msix_entry *msixe = pci_dev_info->msix_entries.entries;
	struct adf_etr_data *etr_data = accel_dev->transport;
	int ret, i = 0;
	char *name;

	/* Request msix irq for all banks unless SR-IOV enabled */
	if (!accel_dev->pf.vf_info) {
#ifdef QAT_UIO
		char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
		unsigned long num_kernel_inst = hw_data->num_banks;

		if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
					    ADF_FIRST_USER_BUNDLE, val) == 0) {
			if (kstrtoul(val, 10, &num_kernel_inst))
				return -1;
		}

		for (i = 0; i < num_kernel_inst; i++) {
#else
		for (i = 0; i < hw_data->num_banks; i++) {
#endif
			struct adf_etr_bank_data *bank = &etr_data->banks[i];
			unsigned int cpu, cpus = num_online_cpus();

			name = *(pci_dev_info->msix_entries.names + i);
			snprintf(name, ADF_MAX_MSIX_VECTOR_NAME,
				 "qat%d-bundle%d", accel_dev->accel_id, i);
			ret = request_irq(msixe[i].vector,
					  adf_msix_isr_bundle, 0, name, bank);
			if (ret) {
				dev_err(&GET_DEV(accel_dev),
					"failed to enable irq %d for %s\n",
					msixe[i].vector, name);
				return ret;
			}

			cpu = ((accel_dev->accel_id * hw_data->num_banks) +
			       i) % cpus;
			irq_set_affinity_hint(msixe[i].vector,
					      get_cpu_mask(cpu));
		}
#ifdef QAT_UIO
	i = hw_data->num_banks;
#endif
	}

	/* Request msix irq for AE */
	name = *(pci_dev_info->msix_entries.names + i);
	snprintf(name, ADF_MAX_MSIX_VECTOR_NAME,
		 "qat%d-ae-cluster", accel_dev->accel_id);
	ret = request_irq(msixe[i].vector, adf_msix_isr_ae, 0, name, accel_dev);
	if (ret) {
		dev_err(&GET_DEV(accel_dev),
			"failed to enable irq %d, for %s\n",
			msixe[i].vector, name);
		return ret;
	}
	return ret;
}

static void adf_free_irqs(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *pci_dev_info = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct msix_entry *msixe = pci_dev_info->msix_entries.entries;
	struct adf_etr_data *etr_data = accel_dev->transport;
	int i = 0;

	if (pci_dev_info->msix_entries.num_entries > 1) {
#ifdef QAT_UIO
		char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
		unsigned long num_kernel_inst = hw_data->num_banks;

		if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
					    ADF_FIRST_USER_BUNDLE, val) == 0) {
			if (kstrtoul(val, 10, &num_kernel_inst))
				return;
		}

		for (i = 0; i < num_kernel_inst; i++) {
#else
		for (i = 0; i < hw_data->num_banks; i++) {
#endif
			irq_set_affinity_hint(msixe[i].vector, NULL);
			free_irq(msixe[i].vector, &etr_data->banks[i]);
		}
#ifdef QAT_UIO
	i = hw_data->num_banks;
#endif
	}
	irq_set_affinity_hint(msixe[i].vector, NULL);
	free_irq(msixe[i].vector, accel_dev);
}

static int adf_isr_alloc_msix_entry_table(struct adf_accel_dev *accel_dev)
{
	int i;
	char **names;
	struct msix_entry *entries;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 msix_num_entries = 1;

	/* If SR-IOV is disabled (vf_info is NULL), add entries for each bank */
	if (!accel_dev->pf.vf_info)
		msix_num_entries += hw_data->num_banks;

	entries = kzalloc_node(msix_num_entries * sizeof(*entries),
			       GFP_KERNEL, dev_to_node(&GET_DEV(accel_dev)));
	if (!entries)
		return -ENOMEM;

	names = kcalloc(msix_num_entries, sizeof(char *), GFP_KERNEL);
	if (!names) {
		kfree(entries);
		return -ENOMEM;
	}
	for (i = 0; i < msix_num_entries; i++) {
		*(names + i) = kzalloc(ADF_MAX_MSIX_VECTOR_NAME, GFP_KERNEL);
		if (!(*(names + i)))
			goto err;
	}
	accel_dev->accel_pci_dev.msix_entries.num_entries = msix_num_entries;
	accel_dev->accel_pci_dev.msix_entries.entries = entries;
	accel_dev->accel_pci_dev.msix_entries.names = names;
	return 0;
err:
	for (i = 0; i < msix_num_entries; i++)
		kfree(*(names + i));
	kfree(entries);
	kfree(names);
	return -ENOMEM;
}

static void adf_isr_free_msix_entry_table(struct adf_accel_dev *accel_dev)
{
	char **names = accel_dev->accel_pci_dev.msix_entries.names;
	int i;

	kfree(accel_dev->accel_pci_dev.msix_entries.entries);
	for (i = 0; i < accel_dev->accel_pci_dev.msix_entries.num_entries; i++)
		kfree(*(names + i));
	kfree(names);
}
#ifdef DEFER_UPSTREAM

static void adf_notify_uncorrectable_error(unsigned long arg)
{
	struct adf_accel_dev *accel_dev = (struct adf_accel_dev *) arg;
	adf_error_event(accel_dev);
	if (accel_dev->pf.vf_info) {
		adf_pf2vf_notify_uncorrectable_error(accel_dev);
	}
	adf_dev_autoreset(accel_dev);
}
#endif

static int adf_setup_bh(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *priv_data = accel_dev->transport;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	int i;

#ifdef QAT_UIO
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
	unsigned long num_kernel_inst = hw_data->num_banks;

	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
				    ADF_FIRST_USER_BUNDLE, val) == 0) {
		if (kstrtoul(val, 10, &num_kernel_inst))
			return -1;
	}

	for (i = 0; i < num_kernel_inst; i++)
#else
	for (i = 0; i < hw_data->num_banks; i++)
#endif
		tasklet_init(&priv_data->banks[i].resp_handler,
			     adf_response_handler,
			     (unsigned long)&priv_data->banks[i]);
#ifdef DEFER_UPSTREAM

	tasklet_init(&accel_dev->fatal_error_tasklet,
		     adf_notify_uncorrectable_error,
		     (unsigned long) accel_dev);
#endif
	return 0;
}

static void adf_cleanup_bh(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *priv_data = accel_dev->transport;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	int i;

#ifdef DEFER_UPSTREAM
	tasklet_disable(&accel_dev->fatal_error_tasklet);
	tasklet_kill(&accel_dev->fatal_error_tasklet);

#endif
	for (i = 0; i < hw_data->num_banks; i++) {
		tasklet_disable(&priv_data->banks[i].resp_handler);
		tasklet_kill(&priv_data->banks[i].resp_handler);
	}
}

/**
 * adf_isr_resource_free() - Free IRQ for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function frees interrupts for acceleration device.
 */
void adf_isr_resource_free(struct adf_accel_dev *accel_dev)
{
	adf_free_irqs(accel_dev);
	adf_cleanup_bh(accel_dev);
	adf_disable_msix(&accel_dev->accel_pci_dev);
	adf_isr_free_msix_entry_table(accel_dev);
}
EXPORT_SYMBOL_GPL(adf_isr_resource_free);

/**
 * adf_isr_resource_alloc() - Allocate IRQ for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function allocates interrupts for acceleration device.
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_isr_resource_alloc(struct adf_accel_dev *accel_dev)
{
	int ret;

	ret = adf_isr_alloc_msix_entry_table(accel_dev);
	if (ret)
		return ret;
	if (adf_enable_msix(accel_dev))
		goto err_out;

	if (adf_setup_bh(accel_dev))
		goto err_out;

	if (adf_request_irqs(accel_dev))
		goto err_out;

	return 0;
err_out:
	adf_isr_resource_free(accel_dev);
	return -EFAULT;
}
EXPORT_SYMBOL_GPL(adf_isr_resource_alloc);
#ifdef DEFER_UPSTREAM

int __init adf_init_event_wq(void)
{
	event_data_wq = alloc_workqueue("qat_event_data_wq",
					WQ_MEM_RECLAIM, 0);
	return !event_data_wq ? -EFAULT : 0;
}

void adf_exit_event_wq(void)
{
	if (event_data_wq)
		destroy_workqueue(event_data_wq);
	event_data_wq = NULL;
}
#endif
