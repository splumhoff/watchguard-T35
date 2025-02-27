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
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/bitops.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/crypto.h>

#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "adf_cfg_common.h"
#include "adf_cfg_user.h"
#ifdef QAT_UIO
#include "adf_uio.h"
#include "qdm.h"
#endif

#define DEVICE_NAME "qat_adf_ctl"

static DEFINE_MUTEX(adf_ctl_lock);
static long adf_ctl_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);

static const struct file_operations adf_ctl_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adf_ctl_ioctl,
	.compat_ioctl = adf_ctl_ioctl,
};

struct adf_ctl_drv_info {
	unsigned int major;
	struct cdev drv_cdev;
	struct class *drv_class;
};

static struct adf_ctl_drv_info adf_ctl_drv;

static void adf_chr_drv_destroy(void)
{
	device_destroy(adf_ctl_drv.drv_class, MKDEV(adf_ctl_drv.major, 0));
	cdev_del(&adf_ctl_drv.drv_cdev);
	class_destroy(adf_ctl_drv.drv_class);
	unregister_chrdev_region(MKDEV(adf_ctl_drv.major, 0), 1);
}

static int adf_chr_drv_create(void)
{
	dev_t dev_id;
	struct device *drv_device;

	if (alloc_chrdev_region(&dev_id, 0, 1, DEVICE_NAME)) {
		pr_err("QAT: unable to allocate chrdev region\n");
		return -EFAULT;
	}

	adf_ctl_drv.drv_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(adf_ctl_drv.drv_class)) {
		pr_err("QAT: class_create failed for adf_ctl\n");
		goto err_chrdev_unreg;
	}
	adf_ctl_drv.major = MAJOR(dev_id);
	cdev_init(&adf_ctl_drv.drv_cdev, &adf_ctl_ops);
	if (cdev_add(&adf_ctl_drv.drv_cdev, dev_id, 1)) {
		pr_err("QAT: cdev add failed\n");
		goto err_class_destr;
	}

	drv_device = device_create(adf_ctl_drv.drv_class, NULL,
				   MKDEV(adf_ctl_drv.major, 0),
				   NULL, DEVICE_NAME);
	if (IS_ERR(drv_device)) {
		pr_err("QAT: failed to create device\n");
		goto err_cdev_del;
	}
	return 0;
err_cdev_del:
	cdev_del(&adf_ctl_drv.drv_cdev);
err_class_destr:
	class_destroy(adf_ctl_drv.drv_class);
err_chrdev_unreg:
	unregister_chrdev_region(dev_id, 1);
	return -EFAULT;
}

static int adf_ctl_alloc_resources(struct adf_user_cfg_ctl_data **ctl_data,
				   unsigned long arg)
{
	struct adf_user_cfg_ctl_data *cfg_data;

	cfg_data = kzalloc(sizeof(*cfg_data), GFP_KERNEL);
	if (!cfg_data)
		return -ENOMEM;

	/* Initialize device id to NO DEVICE as 0 is a valid device id */
	cfg_data->device_id = ADF_CFG_NO_DEVICE;

	if (copy_from_user(cfg_data, (void __user *)arg, sizeof(*cfg_data))) {
		pr_err("QAT: failed to copy from user cfg_data.\n");
		kfree(cfg_data);
		return -EIO;
	}

	*ctl_data = cfg_data;
	return 0;
}

static int adf_add_key_value_data(struct adf_accel_dev *accel_dev,
				  const char *section,
				  const struct adf_user_cfg_key_val *key_val)
{
	if (key_val->type == ADF_HEX) {
		long *ptr = (long *)key_val->val;
		long val = *ptr;

		if (adf_cfg_add_key_value_param(accel_dev, section,
						key_val->key, (void *)val,
						key_val->type)) {
			dev_err(&GET_DEV(accel_dev),
				"failed to add hex keyvalue.\n");
			return -EFAULT;
		}
	} else {
		if (adf_cfg_add_key_value_param(accel_dev, section,
						key_val->key, key_val->val,
						key_val->type)) {
			dev_err(&GET_DEV(accel_dev),
				"failed to add keyvalue.\n");
			return -EFAULT;
		}
	}
	return 0;
}

static int adf_copy_key_value_data(struct adf_accel_dev *accel_dev,
				   struct adf_user_cfg_ctl_data *ctl_data)
{
	struct adf_user_cfg_key_val key_val;
	struct adf_user_cfg_key_val *params_head;
	struct adf_user_cfg_section section, *section_head;

	section_head = ctl_data->config_section;

	while (section_head) {
		if (copy_from_user(&section, (void __user *)section_head,
				   sizeof(*section_head))) {
			dev_err(&GET_DEV(accel_dev),
				"failed to copy section info\n");
			goto out_err;
		}

		if (adf_cfg_section_add(accel_dev, section.name)) {
			dev_err(&GET_DEV(accel_dev),
				"failed to add section.\n");
			goto out_err;
		}

		params_head = section.params;

		while (params_head) {
			if (copy_from_user(&key_val, (void __user *)params_head,
					   sizeof(key_val))) {
				dev_err(&GET_DEV(accel_dev),
					"Failed to copy keyvalue.\n");
				goto out_err;
			}
			if (adf_add_key_value_data(accel_dev, section.name,
						   &key_val)) {
				goto out_err;
			}
			params_head = key_val.next;
		}
		section_head = section.next;
	}
	return 0;
out_err:
	adf_cfg_del_all(accel_dev);
	return -EFAULT;
}
#ifdef QAT_UIO

static int adf_copy_keyval_to_user(struct adf_accel_dev *accel_dev,
				   struct adf_user_cfg_ctl_data *ctl_data)
{
	struct adf_user_cfg_key_val key_val;
	struct adf_user_cfg_section section;
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
	char *user_ptr;

	if (copy_from_user(&section, (void __user *)ctl_data->config_section,
			   sizeof(struct adf_user_cfg_section))) {
		dev_err(&GET_DEV(accel_dev), "failed to copy section info\n");
		return -EFAULT;
	}

	if (copy_from_user(&key_val, (void __user *)section.params,
			   sizeof(struct adf_user_cfg_key_val))) {
		dev_err(&GET_DEV(accel_dev), "failed to copy key val\n");
		return -EFAULT;
	}

	user_ptr = ((char *)section.params) + ADF_CFG_MAX_KEY_LEN_IN_BYTES;

	if (adf_cfg_get_param_value(accel_dev, section.name,
				    key_val.key, val)) {
		dev_err(&GET_DEV(accel_dev),
			"failed to get %s value from config!\n", key_val.key);
		return -EFAULT;
	}

	if (copy_to_user((void __user *)user_ptr, val,
			 ADF_CFG_MAX_VAL_LEN_IN_BYTES)) {
		dev_err(&GET_DEV(accel_dev),
			"failed to copy keyvalue to user!\n");
		return -EFAULT;
	}

	return 0;
}
#endif

static int adf_ctl_ioctl_dev_config(struct file *fp, unsigned int cmd,
				    unsigned long arg)
{
	int ret;
	struct adf_user_cfg_ctl_data *ctl_data;
	struct adf_accel_dev *accel_dev;

	ret = adf_ctl_alloc_resources(&ctl_data, arg);
	if (ret)
		return ret;

	accel_dev = adf_devmgr_get_dev_by_id(ctl_data->device_id);
	if (!accel_dev) {
		ret = -EFAULT;
		goto out;
	}

	if (adf_dev_started(accel_dev)) {
		ret = -EFAULT;
		goto out;
	}

	if (adf_copy_key_value_data(accel_dev, ctl_data)) {
		ret = -EFAULT;
		goto out;
	}
	set_bit(ADF_STATUS_CONFIGURED, &accel_dev->status);
out:
	kfree(ctl_data);
	return ret;
}

static int adf_ctl_is_device_in_use(int id)
{
	struct adf_accel_dev *dev;

	list_for_each_entry(dev, adf_devmgr_get_head(), list) {
		if (id == dev->accel_id || id == ADF_CFG_ALL_DEVICES) {
			if (adf_devmgr_in_reset(dev) || adf_dev_in_use(dev)) {
				dev_info(&GET_DEV(dev),
					 "device qat_dev%d is busy\n",
					 dev->accel_id);
				return -EBUSY;
			}
		}
	}
	return 0;
}

static void adf_ctl_stop_devices(uint32_t id)
{
	struct adf_accel_dev *accel_dev;

	list_for_each_entry(accel_dev, adf_devmgr_get_head(), list) {
		if (id == accel_dev->accel_id || id == ADF_CFG_ALL_DEVICES) {
			if (!adf_dev_started(accel_dev))
				continue;

			/* First stop all VFs */
			if (!accel_dev->is_vf)
				continue;

			adf_dev_stop(accel_dev);
			adf_dev_shutdown(accel_dev);
		}
	}

	list_for_each_entry(accel_dev, adf_devmgr_get_head(), list) {
		if (id == accel_dev->accel_id || id == ADF_CFG_ALL_DEVICES) {
			if (!adf_dev_started(accel_dev))
				continue;

			adf_dev_stop(accel_dev);
			adf_dev_shutdown(accel_dev);
		}
	}
}
#ifdef QAT_UIO

static int adf_ctl_reset_devices(uint32_t id)
{
	struct adf_accel_dev *accel_dev;
	int ret = 0;

	list_for_each_entry(accel_dev, adf_devmgr_get_head(), list) {
		if (id == accel_dev->accel_id ||
		    (id == ADF_CFG_ALL_DEVICES && !accel_dev->is_vf)) {
			if (adf_dev_reset(accel_dev, ADF_DEV_RESET_ASYNC)) {
				dev_info(&GET_DEV(accel_dev),
					 "Failed to reset qat_dev%d\n",
					 accel_dev->accel_id);
				ret = -EFAULT;
			}
		}
	}
	return ret;
}
#endif

static int adf_ctl_ioctl_dev_stop(struct file *fp, unsigned int cmd,
				  unsigned long arg)
{
	int ret;
	struct adf_user_cfg_ctl_data *ctl_data;

	ret = adf_ctl_alloc_resources(&ctl_data, arg);
	if (ret)
		return ret;

	if (adf_devmgr_verify_id(ctl_data->device_id)) {
		pr_err("QAT: Device %d not found\n", ctl_data->device_id);
		ret = -ENODEV;
		goto out;
	}

	ret = adf_ctl_is_device_in_use(ctl_data->device_id);
	if (ret)
		goto out;

	if (ctl_data->device_id == ADF_CFG_ALL_DEVICES)
		pr_info("QAT: Stopping all acceleration devices.\n");
	else
		pr_info("QAT: Stopping acceleration device qat_dev%d.\n",
			ctl_data->device_id);

	adf_ctl_stop_devices(ctl_data->device_id);

out:
	kfree(ctl_data);
	return ret;
}

static int adf_ctl_ioctl_dev_start(struct file *fp, unsigned int cmd,
				   unsigned long arg)
{
	int ret;
	struct adf_user_cfg_ctl_data *ctl_data;
	struct adf_accel_dev *accel_dev;

	ret = adf_ctl_alloc_resources(&ctl_data, arg);
	if (ret)
		return ret;

	ret = -ENODEV;
	accel_dev = adf_devmgr_get_dev_by_id(ctl_data->device_id);
	if (!accel_dev)
		goto out;

	if (!adf_dev_started(accel_dev)) {
		dev_info(&GET_DEV(accel_dev),
			 "Starting acceleration device qat_dev%d.\n",
			 ctl_data->device_id);
		ret = adf_dev_init(accel_dev);
		if (!ret)
			ret = adf_dev_start(accel_dev);
	} else {
		dev_info(&GET_DEV(accel_dev),
			 "Acceleration device qat_dev%d already started.\n",
			 ctl_data->device_id);
	}
	if (ret) {
		dev_err(&GET_DEV(accel_dev), "Failed to start qat_dev%d\n",
			ctl_data->device_id);
		adf_dev_stop(accel_dev);
		adf_dev_shutdown(accel_dev);
	}
out:
	kfree(ctl_data);
	return ret;
}

static int adf_ctl_ioctl_get_num_devices(struct file *fp, unsigned int cmd,
					 unsigned long arg)
{
	uint32_t num_devices = 0;

	adf_devmgr_get_num_dev(&num_devices);
	if (copy_to_user((void __user *)arg, &num_devices, sizeof(num_devices)))
		return -EFAULT;

	return 0;
}
#ifdef QAT_UIO

/*
 * adf_ctl_ioctl_dev_reset
 *
 * Function to reset acceleration device
 */
static int adf_ctl_ioctl_dev_reset(unsigned int cmd,
				   unsigned long arg)
{
	int ret;
	struct adf_user_cfg_ctl_data *ctl_data = NULL;

	ret = adf_ctl_alloc_resources(&ctl_data, arg);
	if (ret)
		return ret;

	/* Verify the device id */
	if (adf_devmgr_verify_id(ctl_data->device_id)) {
		pr_err("QAT: Device %d not found\n", ctl_data->device_id);
		ret = -ENODEV;
		goto out;
	}

	if (ADF_CFG_ALL_DEVICES == ctl_data->device_id)
		pr_info("Scheduling reset of all acceleration devices.\n");
	else
		pr_info("Scheduling reset of device icp_dev%d.\n",
			(uint32_t)ctl_data->device_id);

	ret = adf_ctl_reset_devices(ctl_data->device_id);
	if (ret) {
		pr_err("failed to reset device %d.\n",
		       (uint32_t)ctl_data->device_id);
		ret = -ENODEV;
		goto out;
	}

out:
	kfree(ctl_data);
	return ret;
}
#endif

static int adf_ctl_ioctl_get_status(struct file *fp, unsigned int cmd,
				    unsigned long arg)
{
	struct adf_hw_device_data *hw_data;
	struct adf_dev_status_info dev_info;
	struct adf_accel_dev *accel_dev;

	if (copy_from_user(&dev_info, (void __user *)arg,
			   sizeof(struct adf_dev_status_info))) {
		pr_err("QAT: failed to copy from user.\n");
		return -EFAULT;
	}

	accel_dev = adf_devmgr_get_dev_by_id(dev_info.accel_id);
	if (!accel_dev)
		return -ENODEV;

	hw_data = accel_dev->hw_device;
	dev_info.state = adf_dev_started(accel_dev) ? DEV_UP : DEV_DOWN;
	dev_info.num_ae = hw_data->get_num_aes(hw_data);
	dev_info.num_accel = hw_data->get_num_accels(hw_data);
	dev_info.num_logical_accel = hw_data->num_logical_accel;
	dev_info.banks_per_accel = hw_data->num_banks
					/ hw_data->num_logical_accel;
	strlcpy(dev_info.name, hw_data->dev_class->name, sizeof(dev_info.name));
	dev_info.instance_id = hw_data->instance_id;
#ifdef QAT_UIO
#ifdef KPT
	dev_info.kpt_achandle = hw_data->kpt_achandle;
#endif
#endif
	dev_info.type = hw_data->dev_class->type;
	dev_info.bus = accel_to_pci_dev(accel_dev)->bus->number;
	dev_info.dev = PCI_SLOT(accel_to_pci_dev(accel_dev)->devfn);
	dev_info.fun = PCI_FUNC(accel_to_pci_dev(accel_dev)->devfn);

	if (copy_to_user((void __user *)arg, &dev_info,
			 sizeof(struct adf_dev_status_info))) {
		dev_err(&GET_DEV(accel_dev), "failed to copy status.\n");
		return -EFAULT;
	}
	return 0;
}
#ifdef QAT_UIO

static int adf_ctl_ioctl_dev_get_value(struct file *fp, unsigned int cmd,
				       unsigned long arg)
{
	int ret = 0;
	struct adf_user_cfg_ctl_data *ctl_data;
	struct adf_accel_dev *accel_dev;

	ret = adf_ctl_alloc_resources(&ctl_data, arg);
	if (ret)
		return ret;

	accel_dev = adf_devmgr_get_dev_by_id(ctl_data->device_id);
	if (!accel_dev) {
		pr_err("QAT: Device %d not found\n", ctl_data->device_id);
		ret = -ENODEV;
		goto out;
	}

	if (!adf_dev_started(accel_dev)) {
		dev_err(&GET_DEV(accel_dev), "QAT: Device %d not started\n",
			ctl_data->device_id);
		ret = -EFAULT;
		goto out;
	}

	ret = adf_copy_keyval_to_user(accel_dev, ctl_data);
	if (ret) {
		ret = -ENODEV;
		goto out;
	}
out:
	kfree(ctl_data);
	return ret;
}
#endif

static long adf_ctl_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret;

	if (mutex_lock_interruptible(&adf_ctl_lock))
		return -EFAULT;

	switch (cmd) {
	case IOCTL_CONFIG_SYS_RESOURCE_PARAMETERS:
		ret = adf_ctl_ioctl_dev_config(fp, cmd, arg);
		break;

	case IOCTL_STOP_ACCEL_DEV:
		ret = adf_ctl_ioctl_dev_stop(fp, cmd, arg);
		break;

	case IOCTL_START_ACCEL_DEV:
		ret = adf_ctl_ioctl_dev_start(fp, cmd, arg);
		break;

	case IOCTL_GET_NUM_DEVICES:
		ret = adf_ctl_ioctl_get_num_devices(fp, cmd, arg);
		break;

	case IOCTL_STATUS_ACCEL_DEV:
		ret = adf_ctl_ioctl_get_status(fp, cmd, arg);
		break;
#ifdef QAT_UIO

	case IOCTL_RESET_ACCEL_DEV:
		ret = adf_ctl_ioctl_dev_reset(cmd, arg);
		break;
	case IOCTL_GET_CFG_VAL:
		ret = adf_ctl_ioctl_dev_get_value(fp, cmd, arg);
		break;
	case IOCTL_RESERVE_RING:
		ret = adf_ctl_ioctl_reserve_ring(fp, cmd, arg);
		break;
	case IOCTL_RELEASE_RING:
		ret = adf_ctl_ioctl_release_ring(fp, cmd, arg);
		break;
	case IOCTL_ENABLE_RING:
		ret = adf_ctl_ioctl_enable_ring(fp, cmd, arg);
		break;
	case IOCTL_DISABLE_RING:
		ret = adf_ctl_ioctl_disable_ring(fp, cmd, arg);
		break;
#endif
	default:
		pr_err("QAT: Invalid ioctl\n");
		ret = -EFAULT;
		break;
	}
	mutex_unlock(&adf_ctl_lock);
	return ret;
}

static int __init adf_register_ctl_device_driver(void)
{
	mutex_init(&adf_ctl_lock);

	if (adf_chr_drv_create())
		goto err_chr_dev;

	if (adf_init_aer())
		goto err_aer;

	if (adf_init_pf_wq())
		goto err_pf_wq;

	if (adf_init_vf_wq())
		goto err_vf_wq;

#ifdef DEFER_UPSTREAM
	if (adf_init_event_wq())
		goto err_event_wq;

#endif
	if (qat_crypto_register())
		goto err_crypto_register;

#ifdef QAT_UIO
	if (adf_processes_dev_register())
		goto err_processes_dev_register;
	if (qdm_init())
		goto err_qdm_init;
	if (adf_uio_service_register())
		goto err_adf_service_register;

	return 0;

err_adf_service_register:
err_qdm_init:
err_processes_dev_register:
	qat_crypto_unregister();
#else
	return 0;

#endif
err_crypto_register:
#ifdef DEFER_UPSTREAM
	adf_exit_event_wq();
err_event_wq:
#endif
	adf_exit_vf_wq();
err_vf_wq:
	adf_exit_pf_wq();
err_pf_wq:
	adf_exit_aer();
err_aer:
	adf_chr_drv_destroy();
err_chr_dev:
	mutex_destroy(&adf_ctl_lock);
	return -EFAULT;
}

static void __exit adf_unregister_ctl_device_driver(void)
{
#ifdef QAT_UIO
	adf_uio_service_unregister();
	qdm_exit();
	adf_processes_dev_unregister();
#endif
	adf_chr_drv_destroy();
	adf_exit_aer();
	adf_exit_vf_wq();
	adf_exit_pf_wq();
#ifdef DEFER_UPSTREAM
	adf_exit_event_wq();
#endif
	qat_crypto_unregister();
	adf_clean_vf_map(false);
	mutex_destroy(&adf_ctl_lock);
}

module_init(adf_register_ctl_device_driver);
module_exit(adf_unregister_ctl_device_driver);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel");
MODULE_DESCRIPTION("Intel(R) QuickAssist Technology");
MODULE_ALIAS_CRYPTO("intel_qat");
MODULE_VERSION(ADF_DRV_VERSION);
