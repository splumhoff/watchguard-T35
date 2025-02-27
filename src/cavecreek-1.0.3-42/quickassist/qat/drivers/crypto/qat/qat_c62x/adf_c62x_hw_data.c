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
#include <adf_accel_devices.h>
#include <adf_common_drv.h>
#include <adf_pf2vf_msg.h>
#include "adf_c62x_hw_data.h"
#ifdef QAT_UIO
#include "icp_qat_hw.h"
#include "adf_cfg.h"
#endif

#ifdef QAT_UIO
#ifdef KPT
#define ADF_C6X_MAILBOX1_BASE_OFFSET 0x20974
#define ADF_C6X_MAILBOX1_STRIDE	0x1000
#endif
#endif

#ifndef DEFER_UPSTREAM
/* Worker thread to service arbiter mappings based on dev SKUs */
static const u32 thrd_to_arb_map_8_me_sku[] = {
	0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA, 0x12222AAA,
	0x11222AAA, 0x12222AAA, 0x11222AAA, 0, 0
};

static const u32 thrd_to_arb_map_10_me_sku[] = {
#else
/* Worker thread to service arbiter mappings */
static u32 thrd_to_arb_map[] = {
#endif
	0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA, 0x12222AAA,
	0x11222AAA, 0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA
};

static struct adf_hw_device_class c62x_class = {
	.name = ADF_C62X_DEVICE_NAME,
	.type = DEV_C62X,
	.instances = 0
};

#ifndef DEFER_UPSTREAM
static u32 get_accel_mask(u32 fuse)
{
	return (~fuse) >> ADF_C62X_ACCELERATORS_REG_OFFSET &
			  ADF_C62X_ACCELERATORS_MASK;
}

static u32 get_ae_mask(u32 fuse)
{
	return (~fuse) & ADF_C62X_ACCELENGINES_MASK;
}
#else
static u32 get_accel_mask(struct adf_accel_dev *accel_dev)
{
	struct pci_dev *pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 fuse;
	u32 straps;

	pci_read_config_dword(pdev, ADF_DEVICE_FUSECTL_OFFSET,
			      &fuse);
	pci_read_config_dword(pdev, ADF_C62X_SOFTSTRAP_CSR_OFFSET,
			      &straps);

	return (~(fuse | straps)) >> ADF_C62X_ACCELERATORS_REG_OFFSET &
		ADF_C62X_ACCELERATORS_MASK;
}

static u32 get_ae_mask(struct adf_accel_dev *accel_dev)
{
	struct pci_dev *pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 fuse;
	u32 me_straps;
	u32 me_disable;
	u32 ssms_disabled;

	pci_read_config_dword(pdev, ADF_DEVICE_FUSECTL_OFFSET,
			      &fuse);
	pci_read_config_dword(pdev, ADF_C62X_SOFTSTRAP_CSR_OFFSET,
			      &me_straps);

	/* If SSMs are disabled, then disable the corresponding MEs */
	ssms_disabled = (~get_accel_mask(accel_dev)) &
		ADF_C62X_ACCELERATORS_MASK;
	me_disable = 0x3;
	while (ssms_disabled) {
		if (ssms_disabled & 1)
			me_straps |= me_disable;
		ssms_disabled >>= 1;
		me_disable <<= 2;
	}

	return (~(fuse | me_straps)) & ADF_C62X_ACCELENGINES_MASK;
}
#endif

static u32 get_num_accels(struct adf_hw_device_data *self)
{
	u32 i, ctr = 0;

	if (!self || !self->accel_mask)
		return 0;

	for (i = 0; i < ADF_C62X_MAX_ACCELERATORS; i++) {
		if (self->accel_mask & (1 << i))
			ctr++;
	}
	return ctr;
}

static u32 get_num_aes(struct adf_hw_device_data *self)
{
	u32 i, ctr = 0;

	if (!self || !self->ae_mask)
		return 0;

	for (i = 0; i < ADF_C62X_MAX_ACCELENGINES; i++) {
		if (self->ae_mask & (1 << i))
			ctr++;
	}
	return ctr;
}

static u32 get_misc_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C62X_PMISC_BAR;
}

static u32 get_etr_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C62X_ETR_BAR;
}

static u32 get_sram_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C62X_SRAM_BAR;
}

static enum dev_sku_info get_sku(struct adf_hw_device_data *self)
{
	int aes = get_num_aes(self);

	if (aes == 8)
		return DEV_SKU_2;
	else if (aes == 10)
		return DEV_SKU_4;

	return DEV_SKU_UNKNOWN;
}

static void adf_get_arbiter_mapping(struct adf_accel_dev *accel_dev,
				    u32 const **arb_map_config)
{
#ifdef DEFER_UPSTREAM
	int i;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;

	for (i = 1; i < ADF_C62X_MAX_ACCELENGINES; i++) {
		if (~hw_device->ae_mask & (1 << i))
			thrd_to_arb_map[i] = 0;
	}
	*arb_map_config = thrd_to_arb_map;
#else
	switch (accel_dev->accel_pci_dev.sku) {
	case DEV_SKU_2:
		*arb_map_config = thrd_to_arb_map_8_me_sku;
		break;
	case DEV_SKU_4:
		*arb_map_config = thrd_to_arb_map_10_me_sku;
		break;
	default:
		dev_err(&GET_DEV(accel_dev),
			"The configuration doesn't match any SKU");
		*arb_map_config = NULL;
	}
#endif
}

static u32 get_pf2vf_offset(u32 i)
{
	return ADF_C62X_PF2VF_OFFSET(i);
}

static u32 get_vintmsk_offset(u32 i)
{
	return ADF_C62X_VINTMSK_OFFSET(i);
}

#ifdef DEFER_UPSTREAM
static u32 get_clock_speed(struct adf_hw_device_data *self)
{
	return ADF_C6X_CLK_PER_SEC;
}

#ifdef ADF_SUPPORT_C62X_A0
static void adf_enable_error_interrupts(void __iomem *csr, int revid)
#else
static void adf_enable_error_interrupts(void __iomem *csr)
#endif
{
	ADF_CSR_WR(csr, ADF_ERRMSK0, ADF_C62X_ERRMSK0_CERR); /* ME0-ME3  */
	ADF_CSR_WR(csr, ADF_ERRMSK1, ADF_C62X_ERRMSK1_CERR); /* ME4-ME7  */
	ADF_CSR_WR(csr, ADF_ERRMSK4, ADF_C62X_ERRMSK4_CERR); /* ME8-ME9  */
	ADF_CSR_WR(csr, ADF_ERRMSK5, ADF_C62X_ERRMSK5_CERR); /* SSM2-SSM4 */

	/* Reset everything except VFtoPF1_16 */
	adf_csr_fetch_and_and(csr, ADF_ERRMSK3, ADF_C62X_VF2PF1_16);
	/* Disable Secure RAM correctable error interrupt */
	adf_csr_fetch_and_or(csr, ADF_ERRMSK3, ADF_C62X_ERRMSK3_CERR);

	/* RI CPP bus interface error detection and reporting. */
	ADF_CSR_WR(csr, ADF_C62X_RICPPINTCTL, ADF_C62X_RICPP_EN);

#ifdef ADF_SUPPORT_C62X_A0
	if (revid == 0x00)
		/* Disable TI Parity error interrupt on A0. */
		ADF_CSR_WR(csr, ADF_C62X_TICPPINTCTL, ADF_C62X_TICPP_EN_A0);
	else
		/* TI CPP bus interface error detection and reporting. */
		ADF_CSR_WR(csr, ADF_C62X_TICPPINTCTL, ADF_C62X_TICPP_EN);
#else
	/* TI CPP bus interface error detection and reporting. */
	ADF_CSR_WR(csr, ADF_C62X_TICPPINTCTL, ADF_C62X_TICPP_EN);
#endif

	/* Enable CFC Error interrupts and logging */
	ADF_CSR_WR(csr, ADF_C62X_CPP_CFC_ERR_CTRL, ADF_C62X_CPP_CFC_UE);

	/* Enable SecureRAM to fix and log Correctable errors */
	ADF_CSR_WR(csr, ADF_C62X_SECRAMCERR, ADF_C62X_SECRAM_CERR);

	/* Enable SecureRAM Uncorrectable error interrupts and logging */
	ADF_CSR_WR(csr, ADF_SECRAMUERR, ADF_C62X_SECRAM_UERR);

	/* Enable Push/Pull Misc Uncorrectable error interrupts and logging */
	ADF_CSR_WR(csr, ADF_CPPMEMTGTERR, ADF_C62X_TGT_UERR);
}

static void adf_disable_error_interrupts(struct adf_accel_dev *accel_dev)
{
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_C62X_PMISC_BAR];
	void __iomem *csr = misc_bar->virt_addr;

	/* ME0-ME3 */
	ADF_CSR_WR(csr, ADF_ERRMSK0, ADF_C62X_ERRMSK0_UERR |
		   ADF_C62X_ERRMSK0_CERR);
	/* ME4-ME7 */
	ADF_CSR_WR(csr, ADF_ERRMSK1, ADF_C62X_ERRMSK1_UERR |
		   ADF_C62X_ERRMSK1_CERR);
	/* Secure RAM, CPP Push Pull, RI, TI, SSM0-SSM1, CFC */
	ADF_CSR_WR(csr, ADF_ERRMSK3, ADF_C62X_ERRMSK3_UERR |
		   ADF_C62X_ERRMSK3_CERR);
	/* ME8-ME9 */
	ADF_CSR_WR(csr, ADF_ERRMSK4, ADF_C62X_ERRMSK4_UERR |
		   ADF_C62X_ERRMSK4_CERR);
	/* SSM2-SSM4 */
	ADF_CSR_WR(csr, ADF_ERRMSK5, ADF_C62X_ERRMSK5_UERR |
		   ADF_C62X_ERRMSK5_CERR);
}

static int adf_check_uncorrectable_error(struct adf_accel_dev *accel_dev)
{
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_C62X_PMISC_BAR];
	void __iomem *csr = misc_bar->virt_addr;

	u32 errsou0 = ADF_CSR_RD(csr, ADF_ERRSOU0) & ADF_C62X_ERRMSK0_UERR;
	u32 errsou1 = ADF_CSR_RD(csr, ADF_ERRSOU1) & ADF_C62X_ERRMSK1_UERR;
	u32 errsou3 = ADF_CSR_RD(csr, ADF_ERRSOU3) & ADF_C62X_ERRMSK3_UERR;
	u32 errsou4 = ADF_CSR_RD(csr, ADF_ERRSOU4) & ADF_C62X_ERRMSK4_UERR;
	u32 errsou5 = ADF_CSR_RD(csr, ADF_ERRSOU5) & ADF_C62X_ERRMSK5_UERR;

	return (errsou0 | errsou1 | errsou3 | errsou4 | errsou5);
}

static void adf_enable_mmp_error_correction(void __iomem *csr,
					    struct adf_hw_device_data *hw_data)
{
	unsigned int dev, mmp;
	unsigned int mask;

	/* Enable MMP Logging */
	dev = 0;
	mask = hw_data->accel_mask;
	do {
		if (!(mask & 1))
			continue;
		/* Set power-up */
		adf_csr_fetch_and_and(csr,
				      ADF_C62X_SLICEPWRDOWN(dev),
				      ~ADF_C62X_MMP_PWR_UP_MSK);

		if (hw_data->accel_capabilities_mask &
		    ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC) {
			for (mmp = 0; mmp < ADF_MAX_MMP; ++mmp) {
				/*
				 * The device supports PKE,
				 * so enable error reporting from MMP memory
				 */
				adf_csr_fetch_and_or(csr,
						     ADF_UERRSSMMMP(dev, mmp),
						     ADF_C62X_UERRSSMMMP_EN);
				/*
				 * The device supports PKE,
				 * so enable error correction from MMP memory
				 */
				adf_csr_fetch_and_or(csr,
						     ADF_CERRSSMMMP(dev, mmp),
						     ADF_C62X_CERRSSMMMP_EN);
			}
		} else {
			for (mmp = 0; mmp < ADF_MAX_MMP; ++mmp) {
				/*
				 * The device doesn't support PKE,
				 * so disable error reporting from MMP memory
				 */
				adf_csr_fetch_and_and(csr,
						      ADF_UERRSSMMMP(dev, mmp),
						      ~ADF_C62X_UERRSSMMMP_EN);
				/*
				 * The device doesn't support PKE,
				 * so disable error correction from MMP memory
				 */
				adf_csr_fetch_and_and(csr,
						      ADF_CERRSSMMMP(dev, mmp),
						      ~ADF_C62X_CERRSSMMMP_EN);
			}
		}

		/* Restore power-down value */
		adf_csr_fetch_and_or(csr,
				     ADF_C62X_SLICEPWRDOWN(dev),
				     ADF_C62X_MMP_PWR_UP_MSK);

		/* Disabling correctable error interrupts. */
		ADF_CSR_WR(csr,
			   ADF_C62X_INTMASKSSM(dev),
			   ADF_C62X_INTMASKSSM_UERR);
	} while (dev++, mask >>= 1);
}
#endif
static void adf_enable_error_correction(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_C62X_PMISC_BAR];
	void __iomem *csr = misc_bar->virt_addr;
#ifdef DEFER_UPSTREAM
	unsigned int i;
	unsigned int mask;

	/* Enable Accel Engine error detection & correction */
	i = 0;
	mask = hw_device->ae_mask;
	do {
		if (!(mask & 1))
			continue;
		adf_csr_fetch_and_or(csr, ADF_C62X_AE_CTX_ENABLES(i),
				     ADF_C62X_ENABLE_AE_ECC_ERR);
		adf_csr_fetch_and_or(csr, ADF_C62X_AE_MISC_CONTROL(i),
				     ADF_C62X_ENABLE_AE_ECC_PARITY_CORR);
	} while (i++, mask >>= 1);

	/* Enable shared memory error detection & correction */
	i = 0;
	mask = hw_device->accel_mask;
	do {
		if (!(mask & 1))
			continue;
		adf_csr_fetch_and_or(csr, ADF_UERRSSMSH(i),
				     ADF_C62X_ERRSSMSH_EN);
		adf_csr_fetch_and_or(csr, ADF_CERRSSMSH(i),
				     ADF_C62X_ERRSSMSH_EN);
		adf_csr_fetch_and_or(csr, ADF_PPERR(i),
				     ADF_C62X_PPERR_EN);
	} while (i++, mask >>= 1);

#ifdef ADF_SUPPORT_C62X_A0
	adf_enable_error_interrupts(csr, accel_dev->accel_pci_dev.revid);
#else
	adf_enable_error_interrupts(csr);
#endif
	adf_enable_mmp_error_correction(csr, hw_device);
#else
	unsigned int val, i;

	/* Enable Accel Engine error detection & correction */
	for (i = 0; i < hw_device->get_num_aes(hw_device); i++) {
		val = ADF_CSR_RD(csr, ADF_C62X_AE_CTX_ENABLES(i));
		val |= ADF_C62X_ENABLE_AE_ECC_ERR;
		ADF_CSR_WR(csr, ADF_C62X_AE_CTX_ENABLES(i), val);
		val = ADF_CSR_RD(csr, ADF_C62X_AE_MISC_CONTROL(i));
		val |= ADF_C62X_ENABLE_AE_ECC_PARITY_CORR;
		ADF_CSR_WR(csr, ADF_C62X_AE_MISC_CONTROL(i), val);
	}

	/* Enable shared memory error detection & correction */
	for (i = 0; i < hw_device->get_num_accels(hw_device); i++) {
		val = ADF_CSR_RD(csr, ADF_C62X_UERRSSMSH(i));
		val |= ADF_C62X_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_C62X_UERRSSMSH(i), val);
		val = ADF_CSR_RD(csr, ADF_C62X_CERRSSMSH(i));
		val |= ADF_C62X_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_C62X_CERRSSMSH(i), val);
	}
#endif
}

static void adf_enable_ints(struct adf_accel_dev *accel_dev)
{
	void __iomem *addr;
#ifdef ALLOW_SLICE_HANG_INTERRUPT
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	u32 i;
	unsigned int mask;
#endif

	addr = (&GET_BARS(accel_dev)[ADF_C62X_PMISC_BAR])->virt_addr;

	/* Enable bundle and misc interrupts */
	ADF_CSR_WR(addr, ADF_C62X_SMIAPF0_MASK_OFFSET,
		   ADF_C62X_SMIA0_MASK);
	ADF_CSR_WR(addr, ADF_C62X_SMIAPF1_MASK_OFFSET,
		   ADF_C62X_SMIA1_MASK);
#ifdef ALLOW_SLICE_HANG_INTERRUPT
	/* Enable slice hang interrupt */
	i = 0;
	mask = hw_device->accel_mask;
	do {
		if (!(mask & 1))
			continue;
		ADF_CSR_WR(addr, ADF_SHINTMASKSSM(i),
			   ADF_ENABLE_SLICE_HANG);
	} while (i++, mask >>= 1);
#endif
}

static int adf_pf_enable_vf2pf_comms(struct adf_accel_dev *accel_dev)
{
	return 0;
}
#ifdef ADF_SUPPORT_C62X_A0

static void adf_reset_device(struct adf_accel_dev *accel_dev)
{
	if (accel_dev->accel_pci_dev.revid == 0x00) {
		adf_reset_sbr(accel_dev);
		return;
	}
	adf_reset_flr(accel_dev);
}
#endif
#ifdef QAT_UIO

u32 c62x_get_hw_cap(struct adf_accel_dev *accel_dev)
{
	struct pci_dev *pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 legfuses;
	u32 capabilities;
#ifdef DEFER_UPSTREAM
	u32 straps;
#endif

	/* Read accelerator capabilities mask */
	pci_read_config_dword(pdev, ADF_DEVICE_LEGFUSE_OFFSET,
			      &legfuses);
	capabilities =
		ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC +
		ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC +
		ICP_ACCEL_CAPABILITIES_CIPHER +
		ICP_ACCEL_CAPABILITIES_AUTHENTICATION +
		ICP_ACCEL_CAPABILITIES_COMPRESSION +
		ICP_ACCEL_CAPABILITIES_ZUC +
		ICP_ACCEL_CAPABILITIES_SHA3;
	if (legfuses & ICP_ACCEL_MASK_CIPHER_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CIPHER;
	}
	if (legfuses & ICP_ACCEL_MASK_AUTH_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_AUTHENTICATION;
	if (legfuses & ICP_ACCEL_MASK_PKE_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
	if (legfuses & ICP_ACCEL_MASK_COMPRESS_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_COMPRESSION;
	if (legfuses & ICP_ACCEL_MASK_EIA3_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_ZUC;

#ifdef DEFER_UPSTREAM
	pci_read_config_dword(pdev, ADF_C62X_SOFTSTRAP_CSR_OFFSET,
			      &straps);
	if (straps & ICP_C62X_SS_POWERGATE_PKE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
	if (straps & ICP_C62X_SS_POWERGATE_CY)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_COMPRESSION;

#endif
	return capabilities;
}
#ifdef KPT
static int get_kpt_enabled(struct adf_accel_dev *accel_dev,
			   uint32_t *kpt_enable)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	snprintf(key, sizeof(key), ADF_DEV_KPT_ENABLE);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return -EFAULT;
	if (kstrtouint(val, ADF_CFG_BASE_DEC, kpt_enable))
		return -EFAULT;
	return 0;
}

static int init_kpt_params(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	unsigned long val;

	if (adf_cfg_section_add(accel_dev, ADF_GENERAL_SEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_KPT_ENABLE);
	val = 1;
	if (adf_cfg_add_key_value_param(accel_dev, ADF_GENERAL_SEC,
					key, (void *)&val, ADF_DEC))
		goto err;
	return 0;
err:
	dev_err(&GET_DEV(accel_dev), "Failed to add KPT value to accel_dev cfg\n");
	return -EINVAL;
}


static void init_mailbox1(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *pmisc =
		&GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	void __iomem *csr = pmisc->virt_addr;
	u32 mailbox1_offset = 0;
	int i = 0;

	for (i = 0; i < hw_data->get_num_accels(hw_data); i++) {
		mailbox1_offset = ADF_C6X_MAILBOX1_BASE_OFFSET +
				  i * ADF_C6X_MAILBOX1_STRIDE;
		ADF_CSR_WR(csr, mailbox1_offset, 0);
	}
}

static void update_kpt_hw_capability(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *pmisc =
		&GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	u32 ac_value;
	void __iomem *csr = pmisc->virt_addr;

	ac_value = ADF_CSR_RD(csr, ADF_C6X_MAILBOX1_BASE_OFFSET);
	if (ac_value) {
		hw_data->kpt_hw_capabilities = 1;
		hw_data->kpt_achandle = ac_value;
	}
}
#endif
#endif

void adf_init_hw_data_c62x(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class = &c62x_class;
	hw_data->instance_id = c62x_class.instances++;
	hw_data->num_banks = ADF_C62X_ETR_MAX_BANKS;
	hw_data->num_accel = ADF_C62X_MAX_ACCELERATORS;
	hw_data->num_logical_accel = 1;
	hw_data->num_engines = ADF_C62X_MAX_ACCELENGINES;
	hw_data->tx_rx_gap = ADF_C62X_RX_RINGS_OFFSET;
	hw_data->tx_rings_mask = ADF_C62X_TX_RINGS_MASK;
	hw_data->alloc_irq = adf_isr_resource_alloc;
	hw_data->free_irq = adf_isr_resource_free;
	hw_data->enable_error_correction = adf_enable_error_correction;
#ifdef DEFER_UPSTREAM
	hw_data->check_uncorrectable_error = adf_check_uncorrectable_error;
	hw_data->disable_error_interrupts = adf_disable_error_interrupts;
#endif
	hw_data->get_accel_mask = get_accel_mask;
	hw_data->get_ae_mask = get_ae_mask;
	hw_data->get_num_accels = get_num_accels;
	hw_data->get_num_aes = get_num_aes;
	hw_data->get_sram_bar_id = get_sram_bar_id;
	hw_data->get_etr_bar_id = get_etr_bar_id;
	hw_data->get_misc_bar_id = get_misc_bar_id;
	hw_data->get_pf2vf_offset = get_pf2vf_offset;
	hw_data->get_vintmsk_offset = get_vintmsk_offset;
#ifdef DEFER_UPSTREAM
	hw_data->get_clock_speed = get_clock_speed;
#endif
	hw_data->get_sku = get_sku;
	hw_data->fw_name = ADF_C62X_FW;
	hw_data->fw_mmp_name = ADF_C62X_MMP;
	hw_data->init_admin_comms = adf_init_admin_comms;
	hw_data->exit_admin_comms = adf_exit_admin_comms;
	hw_data->disable_iov = adf_disable_sriov;
	hw_data->send_admin_init = adf_send_admin_init;
	hw_data->init_arb = adf_init_arb;
	hw_data->exit_arb = adf_exit_arb;
	hw_data->get_arb_mapping = adf_get_arbiter_mapping;
	hw_data->enable_ints = adf_enable_ints;
#ifdef DEFER_UPSTREAM
	hw_data->set_ssm_wdtimer = adf_set_ssm_wdtimer;
	hw_data->check_slice_hang = adf_check_slice_hang;
#endif
	hw_data->enable_vf2pf_comms = adf_pf_enable_vf2pf_comms;
#ifdef ADF_SUPPORT_C62X_A0
	hw_data->reset_device = adf_reset_device;
#else
	hw_data->reset_device = adf_reset_flr;
#endif
	hw_data->min_iov_compat_ver = ADF_PFVF_COMPATIBILITY_VERSION;
#ifdef QAT_UIO
	hw_data->extended_dc_capabilities = 0;
#ifdef KPT
	hw_data->get_kpt_enabled = get_kpt_enabled;
	hw_data->init_kpt_params = init_kpt_params;
	hw_data->init_mailbox1 = init_mailbox1;
	hw_data->update_kpt_hw_capability = update_kpt_hw_capability;
#endif
#endif
}

void adf_clean_hw_data_c62x(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class->instances--;
}
