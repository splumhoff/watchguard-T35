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
#ifndef ADF_ACCEL_DEVICES_H_
#define ADF_ACCEL_DEVICES_H_
#ifndef USER_SPACE
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/ratelimit.h>
#include "adf_cfg_common.h"
#endif /* USER_SPACE */

#define ADF_DH895XCC_DEVICE_NAME "dh895xcc"
#define ADF_DH895XCCVF_DEVICE_NAME "dh895xccvf"
#define ADF_C62X_DEVICE_NAME "c6xx"
#define ADF_C62XVF_DEVICE_NAME "c6xxvf"
#define ADF_C3XXX_DEVICE_NAME "c3xxx"
#define ADF_C3XXXVF_DEVICE_NAME "c3xxxvf"
#define ADF_D15XX_DEVICE_NAME "d15xx"
#define ADF_D15XXVF_DEVICE_NAME "d15xxvf"
#define ADF_DH895XCC_PCI_DEVICE_ID 0x435
#define ADF_DH895XCCIOV_PCI_DEVICE_ID 0x443
#define ADF_C62X_PCI_DEVICE_ID 0x37c8
#define ADF_C62XIOV_PCI_DEVICE_ID 0x37c9
#define ADF_C3XXX_PCI_DEVICE_ID 0x19e2
#define ADF_C3XXXIOV_PCI_DEVICE_ID 0x19e3
#define ADF_D15XX_PCI_DEVICE_ID 0x6f54
#define ADF_D15XXIOV_PCI_DEVICE_ID 0x6f55
#ifndef DEFER_UPSTREAM
#define ADF_ERRSOU3 (0x3A000 + 0x0C)
#define ADF_ERRSOU5 (0x3A000 + 0xD8)
#else

/* Error source registers. */
#define ADF_ERRSOU0		(0x3A000 + 0x00)
#define ADF_ERRSOU1		(0x3A000 + 0x04)
#define ADF_ERRSOU2		(0x3A000 + 0x08)
#define ADF_ERRSOU3		(0x3A000 + 0x0C)
#define ADF_ERRSOU4		(0x3A000 + 0xD0)
#define ADF_ERRSOU5		(0x3A000 + 0xD8)

/* Error source mask registers. */
#define ADF_ERRMSK0		(0x3A000 + 0x10)
#define ADF_ERRMSK1		(0x3A000 + 0x14)
#define ADF_ERRMSK2		(0x3A000 + 0x18)
#define ADF_ERRMSK3		(0x3A000 + 0x1C)
#define ADF_ERRMSK4		(0x3A000 + 0xD4)
#define ADF_ERRMSK5		(0x3A000 + 0xDC)

#define ADF_EMSK3_CPM0_MASK BIT(2)
#define ADF_EMSK3_CPM1_MASK BIT(3)
#define ADF_EMSK5_CPM2_MASK BIT(16)
#define ADF_EMSK5_CPM3_MASK BIT(17)
#define ADF_EMSK5_CPM4_MASK BIT(18)

/* RI CPP interface status register. */
#define ADF_RICPPINTSTS		(0x3A000 + 0x114)
/* RI push ID of uncorrectable error transaction register. */
#define ADF_RIERRPUSHID		(0x3A000 + 0x118)
/* RI pull ID of uncorrectable error transaction register. */
#define ADF_RIERRPULLID		(0x3A000 + 0x11C)

/* Error status register. */
#define ADF_CPP_CFC_ERR_STATUS	(0x30000 + 0xC04)
/* Error PPID register. */
#define ADF_CPP_CFC_ERR_PPID	(0x30000 + 0xC08)

/* TI CPP interface status register. */
#define ADF_TICPPINTSTS		(0x3A400 + 0x13C)
/* TI push ID of uncorrectable error transaction register. */
#define ADF_TIERRPUSHID		(0x3A400 + 0x140)
/* TI pull ID of uncorrectable error transaction register. */
#define ADF_TIERRPULLID		(0x3A400 + 0x144)

/* Secure RAM uncorrectable error register. */
#define ADF_SECRAMUERR		(0x3AC00 + 0x04)
/* Secure RAM uncorrectable error address register. */
#define ADF_SECRAMUERRAD	(0x3AC00 + 0x0C)

/* Miscellaneous memory target errors register. */
#define ADF_CPPMEMTGTERR	(0x3AC00 + 0x10)
/* Push or pull ID of uncorrectable error transaction register. */
#define ADF_ERRPPID		(0x3AC00 + 0x14)

/* Common registers for error logging. */
#define ADF_INTSTATSSM(i)	((i) * 0x4000 + 0x04)
#define ADF_INTSTATSSM_SHANGERR BIT(13)
#define ADF_PPERR(i)		((i) * 0x4000 + 0x08)
#define ADF_PPERRID(i)		((i) * 0x4000 + 0x0C)
#define ADF_CERRSSMSH(i)	((i) * 0x4000 + 0x10)
#define ADF_UERRSSMSH(i)	((i) * 0x4000 + 0x18)
#define ADF_UERRSSMSHAD(i)	((i) * 0x4000 + 0x1C)
#define ADF_SLICEHANGSTATUS(i)  ((i) * 0x4000 + 0x4C)
#define ADF_SLICE_HANG_AUTH0_MASK BIT(0)
#define ADF_SLICE_HANG_AUTH1_MASK BIT(1)
#define ADF_SLICE_HANG_CPHR0_MASK BIT(4)
#define ADF_SLICE_HANG_CPHR1_MASK BIT(5)
#define ADF_SLICE_HANG_CMP0_MASK  BIT(8)
#define ADF_SLICE_HANG_CMP1_MASK  BIT(9)
#define ADF_SLICE_HANG_XLT0_MASK  BIT(12)
#define ADF_SLICE_HANG_XLT1_MASK  BIT(13)
#define ADF_SLICE_HANG_MMP0_MASK  BIT(16)
#define ADF_SLICE_HANG_MMP1_MASK  BIT(17)
#define ADF_SLICE_HANG_MMP2_MASK  BIT(18)
#define ADF_SLICE_HANG_MMP3_MASK  BIT(19)
#define ADF_SLICE_HANG_MMP4_MASK  BIT(20)
#define ADF_SSMWDT(i)            ((i) * 0x4000 + 0x54)
#define ADF_SSMWDTPKE(i)         ((i) * 0x4000 + 0x58)
#define ADF_SHINTMASKSSM(i)      ((i) * 0x4000 + 0x1018)
#define ADF_ENABLE_SLICE_HANG  0x000000

/* Error registers for MMP0-MMP4. */
#define ADF_MAX_MMP		(5)
#define ADF_MMP_BASE(i)		((i) * 0x1000 % 0x3800)
#define ADF_CERRSSMMMP(i, n)	((i) * 0x4000 + ADF_MMP_BASE(n) + 0x380)
#define ADF_UERRSSMMMP(i, n)	((i) * 0x4000 + ADF_MMP_BASE(n) + 0x388)
#define ADF_UERRSSMMMPAD(i, n)	((i) * 0x4000 + ADF_MMP_BASE(n) + 0x38C)

#endif
#define ADF_DEVICE_FUSECTL_OFFSET 0x40
#define ADF_DEVICE_LEGFUSE_OFFSET 0x4C
#define ADF_DEVICE_FUSECTL_MASK 0x80000000
#define ADF_PCI_MAX_BARS 3
#define ADF_DEVICE_NAME_LENGTH 32
#define ADF_ETR_MAX_RINGS_PER_BANK 16
#define ADF_MAX_MSIX_VECTOR_NAME 16
#define ADF_DEVICE_NAME_PREFIX "qat_"

enum adf_accel_capabilities {
	ADF_ACCEL_CAPABILITIES_NULL = 0,
	ADF_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC = 1,
	ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC = 2,
	ADF_ACCEL_CAPABILITIES_CIPHER = 4,
	ADF_ACCEL_CAPABILITIES_AUTHENTICATION = 8,
	ADF_ACCEL_CAPABILITIES_COMPRESSION = 32,
	ADF_ACCEL_CAPABILITIES_LZS_COMPRESSION = 64,
	ADF_ACCEL_CAPABILITIES_RANDOM_NUMBER = 128
};

#ifndef USER_SPACE
struct adf_bar {
	resource_size_t base_addr;
	void __iomem *virt_addr;
	resource_size_t size;
} __packed;

struct adf_accel_msix {
	struct msix_entry *entries;
	char **names;
	u32 num_entries;
} __packed;

struct adf_accel_pci {
	struct pci_dev *pci_dev;
	struct adf_accel_msix msix_entries;
	struct adf_bar pci_bars[ADF_PCI_MAX_BARS];
	uint8_t revid;
	uint8_t sku;
} __packed;
#endif /* USER_SPACE */

enum dev_state {
	DEV_DOWN = 0,
	DEV_UP
};

enum dev_sku_info {
	DEV_SKU_1 = 0,
	DEV_SKU_2,
	DEV_SKU_3,
	DEV_SKU_4,
	DEV_SKU_VF,
	DEV_SKU_UNKNOWN,
};

static inline const char *get_sku_info(enum dev_sku_info info)
{
	switch (info) {
	case DEV_SKU_1:
		return "SKU1";
	case DEV_SKU_2:
		return "SKU2";
	case DEV_SKU_3:
		return "SKU3";
	case DEV_SKU_4:
		return "SKU4";
	case DEV_SKU_VF:
		return "SKUVF";
	case DEV_SKU_UNKNOWN:
	default:
		break;
	}
	return "Unknown SKU";
}

#ifndef USER_SPACE
struct adf_hw_device_class {
	const char *name;
	const enum adf_device_type type;
	uint32_t instances;
} __packed;

struct adf_cfg_device_data;
struct adf_accel_dev;
struct adf_etr_data;
struct adf_etr_ring_data;

struct adf_hw_device_data {
	struct adf_hw_device_class *dev_class;
#ifndef DEFER_UPSTREAM
	uint32_t (*get_accel_mask)(uint32_t fuse);
	uint32_t (*get_ae_mask)(uint32_t fuse);
#else
	uint32_t (*get_accel_mask)(struct adf_accel_dev *accel_dev);
	uint32_t (*get_ae_mask)(struct adf_accel_dev *accel_dev);
#endif
	uint32_t (*get_sram_bar_id)(struct adf_hw_device_data *self);
	uint32_t (*get_misc_bar_id)(struct adf_hw_device_data *self);
	uint32_t (*get_etr_bar_id)(struct adf_hw_device_data *self);
	uint32_t (*get_num_aes)(struct adf_hw_device_data *self);
	uint32_t (*get_num_accels)(struct adf_hw_device_data *self);
	uint32_t (*get_pf2vf_offset)(uint32_t i);
	uint32_t (*get_vintmsk_offset)(uint32_t i);
#ifdef DEFER_UPSTREAM
	uint32_t (*get_clock_speed)(struct adf_hw_device_data *self);
#endif
	enum dev_sku_info (*get_sku)(struct adf_hw_device_data *self);
	int (*alloc_irq)(struct adf_accel_dev *accel_dev);
	void (*free_irq)(struct adf_accel_dev *accel_dev);
	void (*enable_error_correction)(struct adf_accel_dev *accel_dev);
#ifdef DEFER_UPSTREAM
	int (*check_uncorrectable_error)(struct adf_accel_dev *accel_dev);
	void (*disable_error_interrupts)(struct adf_accel_dev *accel_dev);
#endif
	int (*init_admin_comms)(struct adf_accel_dev *accel_dev);
	void (*exit_admin_comms)(struct adf_accel_dev *accel_dev);
	int (*send_admin_init)(struct adf_accel_dev *accel_dev);
#ifdef QAT_UIO
#ifdef KPT
	int (*get_kpt_enabled)(struct adf_accel_dev *accel_dev,
			       uint32_t *kpt_enable);
	int (*init_kpt_params)(struct adf_accel_dev *accel_dev);
	void (*init_mailbox1)(struct adf_accel_dev *accel_dev);
	void (*update_kpt_hw_capability)(struct adf_accel_dev *accel_dev);
#endif
#endif
	int (*init_arb)(struct adf_accel_dev *accel_dev);
	void (*exit_arb)(struct adf_accel_dev *accel_dev);
	void (*get_arb_mapping)(struct adf_accel_dev *accel_dev,
				const uint32_t **cfg);
	void (*disable_iov)(struct adf_accel_dev *accel_dev);
	void (*enable_ints)(struct adf_accel_dev *accel_dev);
#ifdef DEFER_UPSTREAM
	bool (*check_slice_hang)(struct adf_accel_dev *accel_dev);
	int (*set_ssm_wdtimer)(struct adf_accel_dev *accel_dev);
#endif
	int (*enable_vf2pf_comms)(struct adf_accel_dev *accel_dev);
	void (*reset_device)(struct adf_accel_dev *accel_dev);
	const char *fw_name;
	const char *fw_mmp_name;
	uint32_t fuses;
	uint32_t accel_capabilities_mask;
	uint32_t instance_id;
	uint16_t accel_mask;
	uint16_t ae_mask;
	uint16_t tx_rings_mask;
	uint8_t tx_rx_gap;
	uint8_t num_banks;
	uint8_t num_accel;
	uint8_t num_logical_accel;
	uint8_t num_engines;
	uint8_t min_iov_compat_ver;
#ifdef QAT_UIO
	uint32_t extended_dc_capabilities;
#ifdef KPT
	uint32_t kpt_hw_capabilities;
	uint32_t kpt_achandle;
#endif
#endif
} __packed;

/* CSR write macro */
#define ADF_CSR_WR(csr_base, csr_offset, val) \
	__raw_writel(val, csr_base + csr_offset)

/* CSR read macro */
#define ADF_CSR_RD(csr_base, csr_offset) __raw_readl(csr_base + csr_offset)

#define GET_DEV(accel_dev) ((accel_dev)->accel_pci_dev.pci_dev->dev)
#define GET_BARS(accel_dev) ((accel_dev)->accel_pci_dev.pci_bars)
#define GET_HW_DATA(accel_dev) (accel_dev->hw_device)
#define GET_MAX_BANKS(accel_dev) (GET_HW_DATA(accel_dev)->num_banks)
#define GET_MAX_ACCELENGINES(accel_dev) (GET_HW_DATA(accel_dev)->num_engines)
#define accel_to_pci_dev(accel_ptr) accel_ptr->accel_pci_dev.pci_dev
#ifdef QAT_UIO
#define GET_MAX_PROCESSES(accel_dev) (GET_MAX_BANKS(accel_dev) * ADF_ETR_MAX_RINGS_PER_BANK/2)
#endif

#ifdef DEFER_UPSTREAM
static inline void adf_csr_fetch_and_and(void __iomem *csr,
					 size_t offs, unsigned long mask)
{
	unsigned int val = ADF_CSR_RD(csr, offs);

	val &= mask;
	ADF_CSR_WR(csr, offs, val);
}

static inline void adf_csr_fetch_and_or(void __iomem *csr,
					size_t offs, unsigned long mask)
{
	unsigned int val = ADF_CSR_RD(csr, offs);

	val |= mask;
	ADF_CSR_WR(csr, offs, val);
}
#endif
struct adf_admin_comms;
struct icp_qat_fw_loader_handle;
struct adf_fw_loader_data {
	struct icp_qat_fw_loader_handle *fw_loader;
	const struct firmware *uof_fw;
	const struct firmware *mmp_fw;
};

struct adf_accel_vf_info {
	struct adf_accel_dev *accel_dev;
	struct tasklet_struct vf2pf_bh_tasklet;
	struct mutex pf2vf_lock; /* protect CSR access for PF2VF messages */
	struct ratelimit_state vf2pf_ratelimit;
	u32 vf_nr;
	bool init;
};

#ifdef QAT_UIO
struct adf_uio_control_accel;
#endif
struct adf_accel_dev {
	struct adf_etr_data *transport;
	struct adf_hw_device_data *hw_device;
	struct adf_cfg_device_data *cfg;
	struct adf_fw_loader_data *fw_loader;
	struct adf_admin_comms *admin;
#ifdef QAT_UIO
	struct adf_uio_control_accel *accel;
	unsigned int num_ker_bundles;
#endif
#ifdef DEFER_UPSTREAM
	struct adf_fw_counters_data *fw_counters_data;
	unsigned int autoreset_on_error;
	struct tasklet_struct fatal_error_tasklet;
#endif
	struct list_head crypto_list;
	unsigned long status;
	atomic_t ref_count;
	struct dentry *debugfs_dir;
	struct list_head list;
	struct module *owner;
	struct adf_accel_pci accel_pci_dev;
	union {
		struct {
			/* vf_info is non-zero when SR-IOV is init'ed */
			struct adf_accel_vf_info *vf_info;
		} pf;
		struct {
			char *irq_name;
			struct tasklet_struct pf2vf_bh_tasklet;
			struct mutex vf2pf_lock; /* protect CSR access */
			struct completion iov_msg_completion;
			uint8_t compatible;
			uint8_t pf_version;
		} vf;
	};
	bool is_vf;
	u32 accel_id;
#ifdef QAT_UIO
#ifdef KPT
	u32 detect_kpt;
#endif
#endif
} __packed;
#endif
#endif
