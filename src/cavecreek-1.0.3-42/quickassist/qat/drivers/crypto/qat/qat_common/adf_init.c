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
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include "adf_accel_devices.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#ifdef QAT_UIO
#include "linux/iommu.h"
#include "icp_qat_fw.h"
#include "adf_uio.h"
#include "adf_uio_control.h"
#ifdef KPT
#include "adf_mei_kpt.h"
#endif
#endif

static LIST_HEAD(service_table);
static DEFINE_MUTEX(service_lock);

static void adf_service_add(struct service_hndl *service)
{
	mutex_lock(&service_lock);
	list_add(&service->list, &service_table);
	mutex_unlock(&service_lock);
}

int adf_service_register(struct service_hndl *service)
{
	memset(service->init_status, 0, sizeof(service->init_status));
	memset(service->start_status, 0, sizeof(service->start_status));
	adf_service_add(service);
	return 0;
}

static void adf_service_remove(struct service_hndl *service)
{
	mutex_lock(&service_lock);
	list_del(&service->list);
	mutex_unlock(&service_lock);
}

int adf_service_unregister(struct service_hndl *service)
{
	int i;

	for (i = 0; i < ADF_DEV_STATUS_ARRAY_SIZE; i++) {
		if (service->init_status[i] || service->start_status[i]) {
			pr_err("QAT: Could not remove active service\n");
			return -EFAULT;
		}
	}
	adf_service_remove(service);
	return 0;
}

#ifdef QAT_UIO
static int adf_cfg_add_device_params(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	unsigned long val;

	if (adf_cfg_section_add(accel_dev, ADF_GENERAL_SEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_MAX_BANKS);
	val = GET_MAX_BANKS(accel_dev);
	if (adf_cfg_add_key_value_param(accel_dev, ADF_GENERAL_SEC,
					key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_CAPABILITIES_MASK);
	val = hw_data->accel_capabilities_mask;
	if (adf_cfg_add_key_value_param(accel_dev, ADF_GENERAL_SEC,
					key, (void *)val, ADF_HEX))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_PKG_ID);
	val = accel_dev->accel_id;
	if (adf_cfg_add_key_value_param(accel_dev, ADF_GENERAL_SEC,
					key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_NODE_ID);
	val = dev_to_node(&GET_DEV(accel_dev));
	if (adf_cfg_add_key_value_param(accel_dev, ADF_GENERAL_SEC,
					key, (void *)&val, ADF_DEC))
		goto err;

	return 0;
err:
	dev_err(&GET_DEV(accel_dev), "Failed to add internal values to accel_dev cfg\n");
	return -EINVAL;
}

static int adf_cfg_add_ext_params(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	unsigned long val;

	snprintf(key, sizeof(key), ADF_DC_EXTENDED_FEATURES);
	val = hw_data->extended_dc_capabilities;
	if (adf_cfg_add_key_value_param(accel_dev, ADF_GENERAL_SEC,
					key, (void *)val, ADF_HEX))
		return -EINVAL;

	return 0;
}

#endif
#ifdef DEFER_UPSTREAM

void adf_error_notifier(unsigned long arg)
{
	struct adf_accel_dev *accel_dev = (struct adf_accel_dev *) arg;
	struct service_hndl *service;
	struct list_head *list_itr;

	list_for_each(list_itr, &service_table) {
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_ERROR))
			dev_err(&GET_DEV(accel_dev),
				"Failed to send error event to %s.\n",
				service->name);
	}
}

/**
 * adf_set_ssm_wdtimer() - Initialize the slice hang watchdog timer.
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_set_ssm_wdtimer(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 timer_val = ADF_SSM_WDT_DEFAULT_VALUE;
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[hw_data->
						get_misc_bar_id(hw_data)];
	void __iomem *csr = misc_bar->virt_addr;
	u32 clk_per_sec = hw_data->get_clock_speed(hw_data);
	u32 i;
#ifdef DEFER_UPSTREAM
	unsigned int mask;
#endif
#ifdef QAT_UIO
	u32 timer_val_pke = timer_val;
	char timer_str[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};

	/* Get Watch Dog Timer for CySym+Comp from the configuration */
	if (!adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
				     ADF_DEV_SSM_WDT_BULK, (char *)timer_str)) {
		if (kstrtouint((char *)timer_str, ADF_CFG_BASE_DEC,
				 &timer_val))
			timer_val = ADF_SSM_WDT_DEFAULT_VALUE;
	}
	/* Get Watch Dog Timer for CyAsym from the configuration */
	if (!adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
				     ADF_DEV_SSM_WDT_PKE, (char *)timer_str)) {
		if (kstrtouint((char *)timer_str, ADF_CFG_BASE_DEC,
				 &timer_val_pke))
			timer_val_pke = ADF_SSM_WDT_DEFAULT_VALUE;
	}

	/* Convert msec to CPP clocks */
	timer_val = timer_val * (clk_per_sec / 1000);
	timer_val_pke = timer_val_pke * (clk_per_sec / 1000);

	i = 0;
	mask = hw_data->accel_mask;
	do {
		if (!(mask & 1))
			continue;
		/* Enable Watch Dog Timer for CySym + Comp */
		ADF_CSR_WR(csr, ADF_SSMWDT(i), timer_val);
		/* Enable Watch Dog Timer for CyAsym */
		ADF_CSR_WR(csr, ADF_SSMWDTPKE(i), timer_val_pke);
	} while (i++, mask >>= 1);
#else

	/* Convert msec to CPP clocks */
	timer_val = timer_val * (clk_per_sec / 1000);
#ifndef DEFER_UPSTREAM
	for (i = 0; i < hw_data->get_num_accels(hw_data); i++) {
#else
	i = 0;
	mask = hw_data->accel_mask;
	do {
		if (!(mask & 1))
			continue;
#endif
		ADF_CSR_WR(csr, ADF_SSMWDT(i), timer_val);
		ADF_CSR_WR(csr, ADF_SSMWDTPKE(i), timer_val);
#ifdef DEFER_UPSTREAM
	} while (i++, mask >>= 1);
#else
	}
#endif
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(adf_set_ssm_wdtimer);

#endif
/**
 * adf_dev_init() - Init data structures and services for the given accel device
 * @accel_dev: Pointer to acceleration device.
 *
 * Initialize the ring data structures and the admin comms and arbitration
 * services.
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_dev_init(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

#ifdef QAT_UIO
	char value[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	int ret = 0;
#endif
	if (!hw_data) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to init device - hw_data not set\n");
		return -EFAULT;
	}

	if (!test_bit(ADF_STATUS_CONFIGURED, &accel_dev->status)) {
		dev_err(&GET_DEV(accel_dev), "Device not configured\n");
		return -EFAULT;
	}

	if (adf_init_etr_data(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "Failed initialize etr\n");
		return -EFAULT;
	}

	if (hw_data->init_admin_comms && hw_data->init_admin_comms(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "Failed initialize admin comms\n");
		return -EFAULT;
	}

	if (hw_data->init_arb && hw_data->init_arb(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "Failed initialize hw arbiter\n");
		return -EFAULT;
	}
#ifdef DEFER_UPSTREAM
	accel_dev->autoreset_on_error = 0;
#endif
#ifdef QAT_UIO

	/* Read autoreset on error parameter */
	ret = adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
				      ADF_AUTO_RESET_ON_ERROR, value);
	if (!ret) {
		if (kstrtouint(value, 10, &accel_dev->autoreset_on_error)) {
			dev_err(&GET_DEV(accel_dev),
				"Failed converting %s to a decimal value",
				ADF_AUTO_RESET_ON_ERROR);
			return -EFAULT;
		}
	}
#endif

#ifndef DEFER_UPSTREAM
	hw_data->enable_ints(accel_dev);

#endif
	if (adf_ae_init(accel_dev)) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to initialise Acceleration Engine\n");
		return -EFAULT;
	}
	set_bit(ADF_STATUS_AE_INITIALISED, &accel_dev->status);
#ifdef QAT_UIO
#ifdef KPT
    if (adf_dev_enable_kpt(accel_dev)) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to enable KPT\n");
		return -EFAULT;
    }
#endif
#endif
	if (adf_ae_fw_load(accel_dev)) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to load acceleration FW\n");
		return -EFAULT;
	}
	set_bit(ADF_STATUS_AE_UCODE_LOADED, &accel_dev->status);

	if (hw_data->alloc_irq(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "Failed to allocate interrupts\n");
		return -EFAULT;
	}
	set_bit(ADF_STATUS_IRQ_ALLOCATED, &accel_dev->status);
#ifdef DEFER_UPSTREAM
	hw_data->enable_ints(accel_dev);
#endif

#ifdef QAT_UIO
	if (!test_bit(ADF_STATUS_RESTARTING, &accel_dev->status) &&
		adf_cfg_add_device_params(accel_dev))
		return -EFAULT;
#endif
	/*
	 * Subservice initialisation is divided into two stages: init and start.
	 * This is to facilitate any ordering dependencies between services
	 * prior to starting any of the accelerators.
	 */
	list_for_each(list_itr, &service_table) {
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_INIT)) {
			dev_err(&GET_DEV(accel_dev),
				"Failed to initialise service %s\n",
				service->name);
			return -EFAULT;
		}
		set_bit(accel_dev->accel_id, service->init_status);
	}

	hw_data->enable_error_correction(accel_dev);
#ifndef DEFER_UPSTREAM
	hw_data->enable_vf2pf_comms(accel_dev);
#else
	if (hw_data->enable_vf2pf_comms(accel_dev))
		return -EINVAL;
	/*
	 * Set ssm watch dog timer for slice hang detection
	 * Note! Not supported on devices older than C62x
	 */
	if (hw_data->set_ssm_wdtimer && hw_data->set_ssm_wdtimer(accel_dev)) {
		dev_err(&GET_DEV(accel_dev),
			"QAT: Failed to set ssm watch dog timer\n");
		return -EFAULT;
	}
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(adf_dev_init);

/**
 * adf_dev_start() - Start acceleration service for the given accel device
 * @accel_dev:    Pointer to acceleration device.
 *
 * Function notifies all the registered services that the acceleration device
 * is ready to be used.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_dev_start(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct service_hndl *service;
	struct list_head *list_itr;
#ifdef DEFER_UPSTREAM
	int status = 0;
#endif

	set_bit(ADF_STATUS_STARTING, &accel_dev->status);

	if (adf_ae_start(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "AE Start Failed\n");
		return -EFAULT;
	}
	set_bit(ADF_STATUS_AE_STARTED, &accel_dev->status);

	if (hw_data->send_admin_init(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "Failed to send init message\n");
		return -EFAULT;
	}

	list_for_each(list_itr, &service_table) {
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_START)) {
			dev_err(&GET_DEV(accel_dev),
				"Failed to start service %s\n",
				service->name);
			return -EFAULT;
		}
		set_bit(accel_dev->accel_id, service->start_status);
	}

#ifdef QAT_UIO
	if (!accel_dev->is_vf && !accel_dev->pf.vf_info &&
			iommu_present(&pci_bus_type))
		dev_err(&GET_DEV(accel_dev),
			"Cannot use PF with IOMMU enabled\n");

	if (accel_dev->is_vf || (!accel_dev->pf.vf_info &&
				!iommu_present(&pci_bus_type)))  {
		/* Create the UIO sysfs entries */
		if (adf_uio_sysfs_create(accel_dev)) {
			dev_err(&GET_DEV(accel_dev),
				"Failed to create the sysfs entry\n");
			set_bit(ADF_STATUS_STARTING, &accel_dev->status);
			clear_bit(ADF_STATUS_STARTED, &accel_dev->status);
			return -EFAULT;
		}

		/*Register UIO devices */
		if (adf_uio_register(accel_dev)) {
			adf_uio_remove(accel_dev);
			dev_err(&GET_DEV(accel_dev),
				"Failed to register UIO devices\n");
			set_bit(ADF_STATUS_STARTING, &accel_dev->status);
			clear_bit(ADF_STATUS_STARTED, &accel_dev->status);
			return -ENODEV;
		}
	}

	if (!test_bit(ADF_STATUS_RESTARTING, &accel_dev->status) &&
	    adf_cfg_add_ext_params(accel_dev))
		return -EFAULT;
#endif
	clear_bit(ADF_STATUS_STARTING, &accel_dev->status);
	set_bit(ADF_STATUS_STARTED, &accel_dev->status);

#ifdef DEFER_UPSTREAM
	if (!list_empty(&accel_dev->crypto_list)) {
		if (hw_data->accel_capabilities_mask &
				ADF_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC)
			status = qat_algs_register();

		if (hw_data->accel_capabilities_mask &
				ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC)
			status |= qat_asym_algs_register();

		if (status) {
			dev_err(&GET_DEV(accel_dev),
					"Failed to register crypto algs\n");
			set_bit(ADF_STATUS_STARTING, &accel_dev->status);
			clear_bit(ADF_STATUS_STARTED, &accel_dev->status);
			return -EFAULT;
		}
	}
#else
	if (!list_empty(&accel_dev->crypto_list) &&
	    (qat_algs_register() || qat_asym_algs_register())) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to register crypto algs\n");
		set_bit(ADF_STATUS_STARTING, &accel_dev->status);
		clear_bit(ADF_STATUS_STARTED, &accel_dev->status);
		return -EFAULT;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(adf_dev_start);

/**
 * adf_dev_stop() - Stop acceleration service for the given accel device
 * @accel_dev:    Pointer to acceleration device.
 *
 * Function notifies all the registered services that the acceleration device
 * is shuting down.
 * To be used by QAT device specific drivers.
 *
 * Return: void
 */
void adf_dev_stop(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;
	bool wait = false;
	int ret;

	if (!adf_dev_started(accel_dev) &&
	    !test_bit(ADF_STATUS_STARTING, &accel_dev->status))
		return;

	clear_bit(ADF_STATUS_STARTING, &accel_dev->status);
	clear_bit(ADF_STATUS_STARTED, &accel_dev->status);

	if (!list_empty(&accel_dev->crypto_list)) {
		qat_algs_unregister();
		qat_asym_algs_unregister();
	}

	list_for_each(list_itr, &service_table) {
		service = list_entry(list_itr, struct service_hndl, list);
		if (!test_bit(accel_dev->accel_id, service->start_status))
			continue;
		ret = service->event_hld(accel_dev, ADF_EVENT_STOP);
		if (!ret) {
			clear_bit(accel_dev->accel_id, service->start_status);
		} else if (ret == -EAGAIN) {
			wait = true;
			clear_bit(accel_dev->accel_id, service->start_status);
		}
	}

	if (wait)
		msleep(100);

	if (test_bit(ADF_STATUS_AE_STARTED, &accel_dev->status)) {
		if (adf_ae_stop(accel_dev))
			dev_err(&GET_DEV(accel_dev), "failed to stop AE\n");
		else
			clear_bit(ADF_STATUS_AE_STARTED, &accel_dev->status);
	}
#ifdef QAT_UIO
	if (accel_dev->is_vf || !accel_dev->pf.vf_info) {
		/* Remove UIO Devices */
		adf_uio_remove(accel_dev);
		adf_uio_sysfs_delete(accel_dev);
	}
#endif
}
EXPORT_SYMBOL_GPL(adf_dev_stop);

/**
 * adf_dev_shutdown() - shutdown acceleration services and data strucutures
 * @accel_dev: Pointer to acceleration device
 *
 * Cleanup the ring data structures and the admin comms and arbitration
 * services.
 */
void adf_dev_shutdown(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct service_hndl *service;
	struct list_head *list_itr;

	if (!hw_data) {
		dev_err(&GET_DEV(accel_dev),
			"QAT: Failed to shutdown device - hw_data not set\n");
		return;
	}

	if (test_bit(ADF_STATUS_AE_UCODE_LOADED, &accel_dev->status)) {
		adf_ae_fw_release(accel_dev);
		clear_bit(ADF_STATUS_AE_UCODE_LOADED, &accel_dev->status);
	}

	if (test_bit(ADF_STATUS_AE_INITIALISED, &accel_dev->status)) {
		if (adf_ae_shutdown(accel_dev))
			dev_err(&GET_DEV(accel_dev),
				"Failed to shutdown Accel Engine\n");
		else
			clear_bit(ADF_STATUS_AE_INITIALISED,
				  &accel_dev->status);
	}

	list_for_each(list_itr, &service_table) {
		service = list_entry(list_itr, struct service_hndl, list);
		if (!test_bit(accel_dev->accel_id, service->init_status))
			continue;
		if (service->event_hld(accel_dev, ADF_EVENT_SHUTDOWN))
			dev_err(&GET_DEV(accel_dev),
				"Failed to shutdown service %s\n",
				service->name);
		else
			clear_bit(accel_dev->accel_id, service->init_status);
	}

	hw_data->disable_iov(accel_dev);

	if (test_bit(ADF_STATUS_IRQ_ALLOCATED, &accel_dev->status)) {
		hw_data->free_irq(accel_dev);
		clear_bit(ADF_STATUS_IRQ_ALLOCATED, &accel_dev->status);
	}

	/* Delete configuration only if not restarting */
	if (!test_bit(ADF_STATUS_RESTARTING, &accel_dev->status))
		adf_cfg_del_all(accel_dev);

	if (hw_data->exit_arb)
		hw_data->exit_arb(accel_dev);

	if (hw_data->exit_admin_comms)
		hw_data->exit_admin_comms(accel_dev);

	adf_cleanup_etr_data(accel_dev);
	adf_dev_restore(accel_dev);
}
EXPORT_SYMBOL_GPL(adf_dev_shutdown);

#ifdef DEFER_UPSTREAM
/**
 * adf_dev_reset() - Reset acceleration service for the given accel device
 * @accel_dev:    Pointer to acceleration device.
 *
 * Function notifies all the registered services that the acceleration device
 * is resetting.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_dev_reset(struct adf_accel_dev *accel_dev, enum adf_dev_reset_mode mode)
{
	return adf_dev_aer_schedule_reset(accel_dev, mode);
}
EXPORT_SYMBOL_GPL(adf_dev_reset);

#endif
int adf_dev_restarting_notify(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;

	list_for_each(list_itr, &service_table) {
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_RESTARTING))
			dev_err(&GET_DEV(accel_dev),
				"Failed to restart service %s.\n",
				service->name);
	}
	return 0;
}

int adf_dev_restarted_notify(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;

	list_for_each(list_itr, &service_table) {
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_RESTARTED))
			dev_err(&GET_DEV(accel_dev),
				"Failed to restart service %s.\n",
				service->name);
	}
	return 0;
}

#ifdef QAT_UIO
#ifdef KPT
int adf_dev_enable_kpt(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 kpt_enabled = 0;
	int ret = 0;
	if (!accel_dev->detect_kpt) {
		/*Initialize the mailbox1 to 0*/
		if (hw_data->init_mailbox1)
			hw_data->init_mailbox1(accel_dev);
		/* send discovery KPT hardware capability command*/
		ret = adf_mei_send_discovery_kpt();
		if (ret)
			return -EFAULT;
		if (hw_data->update_kpt_hw_capability)
			hw_data->update_kpt_hw_capability(accel_dev);

		if (hw_data->kpt_hw_capabilities) {
			if (hw_data->init_kpt_params) {
				ret = hw_data->init_kpt_params(accel_dev);
				if (ret)
					return -EFAULT;
			} else {
				dev_err(&GET_DEV(accel_dev),
					"Failed to get init_kpt_params\n");
				return -EFAULT;
			}
		}
		accel_dev->detect_kpt = 1;
	}

	/*
	 * Enable KPT capability and disable compression capability
	 * if KPT is enabled.
	 */
	if (hw_data->get_kpt_enabled &&
	    !hw_data->get_kpt_enabled(accel_dev, &kpt_enabled)) {
		if (kpt_enabled) {
#if (RHEL_RELEASE_CODE && (RHEL_RELEASE_VERSION(7, 3) == RHEL_RELEASE_CODE) && \
	 (LINUX_VERSION_CODE == KERNEL_VERSION(3, 10, 0)))
			if (!hw_data->kpt_hw_capabilities) {
				dev_err(&GET_DEV(accel_dev),
					"Device has no KPT capabilitiy\n");
				return -EFAULT;
			}
#endif
			adf_update_kpt_wrk_arb(accel_dev);
			hw_data->accel_capabilities_mask |=
				ICP_ACCEL_CAPABILITIES_KPT;
			hw_data->accel_capabilities_mask &=
				~(ICP_ACCEL_CAPABILITIES_COMPRESSION |
				  ICP_ACCEL_CAPABILITIES_LZS_COMPRESSION);
		} else {
			hw_data->accel_capabilities_mask &=
				~ICP_ACCEL_CAPABILITIES_KPT;
			hw_data->accel_capabilities_mask |=
				(ICP_ACCEL_CAPABILITIES_COMPRESSION |
				ICP_ACCEL_CAPABILITIES_LZS_COMPRESSION);
		}
	}
    return 0;
}
#endif
#endif
