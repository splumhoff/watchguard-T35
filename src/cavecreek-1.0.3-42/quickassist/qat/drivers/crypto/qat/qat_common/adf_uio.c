/*
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY
  Copyright(c) 2015 Intel Corporation.
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
  Copyright(c) 2015 Intel Corporation.
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
#include <linux/uio_driver.h>
#include <linux/sched.h>
#include <linux/kobject.h>
#include <linux/semaphore.h>

#include "adf_common_drv.h"
#include "adf_uio_control.h"
#include "adf_transport_access_macros.h"
#include "adf_uio_cleanup.h"
#include "adf_uio.h"
#include "adf_cfg.h"
#include "adf_cfg_user.h"
#include "qdm.h"
#include "adf_transport_internal.h"

#define ADF_UIO_NAME "UIO_%s_%02d_BUNDLE_%02d"
#define ADF_UIO_DEV_NAME "/dev/uio%i"
#define ADF_UIO_MAP_NAME "ADF_%s_ETR_BUNDLE_%02d"

#define ADF_UIO_GET_NAME(accel_dev) (GET_HW_DATA(accel_dev)->dev_class->name)
#define ADF_UIO_GET_TYPE(accel_dev) (GET_HW_DATA(accel_dev)->dev_class->type)
#define ADF_UIO_GET_BAR(accel_dev)  (GET_HW_DATA(accel_dev)->get_etr_bar_id(\
				     GET_HW_DATA(accel_dev)))

static struct service_hndl adf_uio_hndl;

static inline int adf_uio_get_minor(struct uio_info *info)
{
	struct uio_device *uio_dev = info->uio_dev;

	return uio_dev->minor;
}

/*
 * Structure defining the QAT UIO device private information
 */
struct qat_uio_pci_dev {
	uint8_t nb_bundles;
	struct uio_info *info;
	struct adf_uio_control *control;
};

static inline
void adf_uio_init_bundle_ctrl(struct adf_uio_control_bundle *bundle,
			      struct uio_info *info)
{
	int minor = adf_uio_get_minor(info);
	struct qat_uio_bundle_dev *priv = info->priv;

	snprintf(bundle->name, sizeof(bundle->name), ADF_UIO_DEV_NAME,
		 minor);
	bundle->hardware_bundle_number = priv->hardware_bundle_number;
	bundle->device_minor = minor;
	INIT_LIST_HEAD(&bundle->list);
	priv->bundle = bundle;
	mutex_init(&bundle->lock);
	bundle->csr_addr = info->mem[0].internal_addr;
}

static inline void adf_uio_init_accel_ctrl(struct adf_uio_control_accel *accel,
					   struct adf_accel_dev *accel_dev,
					   struct uio_info *info,
					   unsigned int nb_bundles)
{
	int i;

	accel->nb_bundles = nb_bundles;

	for (i = 0; i < nb_bundles; i++)
		adf_uio_init_bundle_ctrl(accel->bundle[i], &info[i]);

	accel->first_minor = accel->bundle[0]->device_minor;
	accel->last_minor = accel->bundle[nb_bundles - 1]->device_minor;
}

static struct adf_uio_control_bundle *adf_ctl_ioctl_bundle(
		struct adf_user_reserve_ring reserve)
{
        struct adf_accel_dev *accel_dev;
        struct adf_uio_control_accel *accel;
	struct adf_uio_control_bundle *bundle;

        accel_dev = adf_devmgr_get_dev_by_id(reserve.accel_id);
        if (!accel_dev) {
                pr_err("QAT: Failed to get accel_dev\n");
                return NULL;
        }

	accel = accel_dev->accel;
	if (!accel) {
		pr_err("QAT: Failed to get accel\n");
		return NULL;
	}

	if (reserve.bank_nr >= GET_MAX_BANKS(accel_dev)) {
		pr_err("QAT: Invalid bank bunber %d\n", reserve.bank_nr);
		return NULL;
	}
	if (reserve.ring_mask & ~((1 << ADF_ETR_MAX_RINGS_PER_BANK) - 1)) {
		pr_err("QAT: Invalid ring mask %0X\n", reserve.ring_mask);
		return NULL;
	}

	bundle = accel->bundle[reserve.bank_nr - accel->num_ker_bundles];

	return bundle;
}

int adf_ctl_ioctl_reserve_ring(struct file *fp, unsigned int cmd,
			       unsigned long arg)
{
	struct adf_user_reserve_ring reserve;
	struct adf_uio_control_bundle *bundle;
	struct adf_uio_instance_rings *instance_rings;
	int pid_entry_found;

	if (copy_from_user(&reserve, (void __user *)arg,
			   sizeof(struct adf_user_reserve_ring))) {
		pr_err("QAT: failed to copy from user.\n");
		return -EFAULT;
	}

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	if (bundle->rings_used & (reserve.ring_mask)) {
		pr_err("QAT: Bundle %d, rings 0x%04X already reserved\n",
				reserve.bank_nr, reserve.ring_mask);
		return -EINVAL;
	}

	/* Find the list entry for this process */
	pid_entry_found = 0;
	list_for_each_entry(instance_rings, &bundle->list, list) {
		if (instance_rings->user_pid == current->tgid) {
			pid_entry_found = 1;
			break;
		}
	}
	if (!pid_entry_found) {
		instance_rings = kzalloc(sizeof(*instance_rings), GFP_KERNEL);
		instance_rings->user_pid = current->tgid;
		instance_rings->ring_mask = 0;
		list_add_tail(&instance_rings->list, &bundle->list);
	}

	instance_rings->ring_mask |= reserve.ring_mask;
	mutex_lock(&bundle->lock);
	bundle->rings_used |= reserve.ring_mask;
	mutex_unlock(&bundle->lock);

	return 0;
}

int adf_ctl_ioctl_release_ring(struct file *fp, unsigned int cmd,
			       unsigned long arg)
{
	struct adf_user_reserve_ring reserve;
	struct adf_uio_control_bundle *bundle;
	struct adf_uio_instance_rings *instance_rings;
	int pid_entry_found;

	if (copy_from_user(&reserve, (void __user *)arg,
			   sizeof(struct adf_user_reserve_ring))) {
		pr_err("QAT: failed to copy from user.\n");
		return -EFAULT;
	}

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	/* Find the list entry for this process */
	pid_entry_found = 0;
	list_for_each_entry(instance_rings, &bundle->list, list) {
		if (instance_rings->user_pid == current->tgid) {
			pid_entry_found = 1;
			break;
		}
	}
	if (!pid_entry_found) {
		pr_err("QAT: No ring reservation found for PID %d\n",
			current->tgid);
		return -EINVAL;
	}

	if ((instance_rings->ring_mask & reserve.ring_mask) !=
			reserve.ring_mask) {
		pr_err("QAT: Attempt to release rings not reserved by this process\n");
		return -EINVAL;
	}

	instance_rings->ring_mask &= ~reserve.ring_mask;
	mutex_lock(&bundle->lock);
	bundle->rings_used &= ~reserve.ring_mask;
	mutex_unlock(&bundle->lock);
	if (!instance_rings->ring_mask) {
		list_del(&instance_rings->list);
		kfree(instance_rings);
	}

	return 0;
}


int adf_ctl_ioctl_enable_ring(struct file *fp, unsigned int cmd,
			      unsigned long arg)
{
	struct adf_user_reserve_ring reserve;
	struct adf_uio_control_bundle *bundle;

	if (copy_from_user(&reserve, (void __user *)arg,
			   sizeof(struct adf_user_reserve_ring))) {
		pr_err("QAT: failed to copy from user.\n");
		return -EFAULT;
	}

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	mutex_lock(&bundle->lock);
	adf_enable_ring_arb(bundle->csr_addr,
			    reserve.ring_mask);
	mutex_unlock(&bundle->lock);

	return 0;
}
int adf_ctl_ioctl_disable_ring(struct file *fp, unsigned int cmd,
			      unsigned long arg)
{
	struct adf_user_reserve_ring reserve;
	struct adf_uio_control_bundle *bundle;

	if (copy_from_user(&reserve, (void __user *)arg,
			   sizeof(struct adf_user_reserve_ring))) {
		pr_err("QAT: failed to copy from user.\n");
		return -EFAULT;
	}

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	mutex_lock(&bundle->lock);
	adf_disable_ring_arb(bundle->csr_addr,
			    reserve.ring_mask);
	mutex_unlock(&bundle->lock);

	return 0;
}

static int adf_uio_open(struct uio_info *info, struct inode *inode)
{
	struct qat_uio_bundle_dev *priv = info->priv;

	adf_dev_get(priv->accel->accel_dev);
	return 0;
}

static int adf_uio_release(struct uio_info *info, struct inode *inode)
{
	return 0;
}

static int adf_uio_remap_bar(struct adf_accel_dev *accel_dev,
			     struct uio_info *info,
			     uint8_t bundle, uint8_t bank_offset)
{
	struct adf_bar bar =
		accel_dev->accel_pci_dev.pci_bars[ADF_UIO_GET_BAR(accel_dev)];
	char bar_name[ADF_DEVICE_NAME_LENGTH];
	unsigned int offset = bank_offset * ADF_RING_BUNDLE_SIZE;

	snprintf(bar_name, sizeof(bar_name), ADF_UIO_MAP_NAME,
		 ADF_UIO_GET_NAME(accel_dev), bundle);
	info->mem[0].name = kstrndup(bar_name, sizeof(bar_name), GFP_KERNEL);
	info->mem[0].addr = bar.base_addr + offset;
	info->mem[0].internal_addr = bar.virt_addr + offset;
	info->mem[0].size = ADF_RING_BUNDLE_SIZE;
	info->mem[0].memtype = UIO_MEM_PHYS;

	return 0;
}

/*   adf memory map operatoin   */
/*   in the close operation, we do the ring clean up if needed  */
static void adf_uio_mmap_close(struct vm_area_struct *vma)
{
	struct uio_info *info = vma->vm_private_data;
	struct qat_uio_bundle_dev *priv;

	if (!info)
		return;

	priv = info->priv;
	adf_uio_do_cleanup_orphan(info, priv->accel);
	adf_dev_put(priv->accel->accel_dev);
}

static struct vm_operations_struct adf_uio_mmap_operation = {
	.close = adf_uio_mmap_close,
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys,
#endif
};

static int find_mem_index(struct vm_area_struct *vma)
{
	struct uio_info *info = vma->vm_private_data;

	if (!info)
		return -1;

	if (vma->vm_pgoff < MAX_UIO_MAPS) {
		if (!info->mem[vma->vm_pgoff].size)
			return -1;
		return (int)vma->vm_pgoff;
	}

	return -EINVAL;
}

static int adf_uio_mmap(struct uio_info *info, struct vm_area_struct *vma)
{
	int mi;
	struct uio_mem *mem;

	vma->vm_private_data = info;
	mi = find_mem_index(vma);
	if (mi < 0)
		return -EINVAL;

	/*  only support PHYS type here  */
	if (info->mem[mi].memtype != UIO_MEM_PHYS)
		return -EINVAL;

	mem = info->mem + mi;
	vma->vm_ops = &adf_uio_mmap_operation;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return remap_pfn_range(vma,
			       vma->vm_start,
			       mem->addr >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
}

static irqreturn_t adf_uio_isr_bundle(int irq, struct uio_info *info)
{
	struct qat_uio_bundle_dev *bundle = info->priv;
	struct adf_accel_dev *accel_dev = bundle->accel->accel_dev;
	struct adf_etr_data *etr_data = accel_dev->transport;
	struct adf_etr_bank_data *bank =
		&etr_data->banks[bundle->hardware_bundle_number];

	WRITE_CSR_INT_FLAG_AND_COL(bank->csr_addr, bank->bank_number, 0);

	return IRQ_HANDLED;
}

static int adf_uio_create_bundle_dev(struct adf_accel_dev *accel_dev,
				     struct qat_uio_pci_dev *uiodev,
				     uint8_t bundle, uint8_t nb_bundles)
{
	char name[ADF_DEVICE_NAME_LENGTH];
	struct qat_uio_bundle_dev *priv;
	struct adf_accel_pci *pci_dev_info = &accel_dev->accel_pci_dev;
	unsigned hw_bundle_number = bundle +
			(GET_MAX_BANKS(accel_dev) - nb_bundles);
	struct adf_etr_data *etr_data = accel_dev->transport;
	struct adf_etr_bank_data *bank = &etr_data->banks[hw_bundle_number];
	unsigned int irq_flags = 0;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->hardware_bundle_number = GET_MAX_BANKS(accel_dev)
				       - nb_bundles + bundle;
	priv->accel = accel_dev->accel;

	if (adf_uio_remap_bar(accel_dev, &uiodev->info[bundle], bundle,
			      priv->hardware_bundle_number))
		return -ENOMEM;

	snprintf(name, sizeof(name), ADF_UIO_NAME,
		 ADF_UIO_GET_NAME(accel_dev), accel_dev->accel_id, bundle);
	uiodev->info[bundle].name = kstrndup(name, sizeof(name), GFP_KERNEL);
	uiodev->info[bundle].version = ADF_DRV_VERSION;
	uiodev->info[bundle].priv = priv;
	uiodev->info[bundle].open = adf_uio_open;
	uiodev->info[bundle].release = adf_uio_release;
	uiodev->info[bundle].handler = adf_uio_isr_bundle;
	uiodev->info[bundle].mmap = adf_uio_mmap;

	/* Use MSIX vector for PF and the proper IRQ for VF */
	if (!accel_dev->is_vf) {
		struct msix_entry *msixe = pci_dev_info->msix_entries.entries;
		uiodev->info[bundle].irq = msixe[hw_bundle_number].vector;
	} else {
		struct pci_dev *pdev = accel_to_pci_dev(accel_dev);
		uiodev->info[bundle].irq = pdev->irq;

		/* In VF we are sharing the interrupt */
		irq_flags = IRQF_SHARED;
	}

	uiodev->info[bundle].irq_flags = irq_flags;

	/* There is no need to set a hint for IRQs affinity cause the CPU
	 * affinity will be set from user space in adf_ctl
	 */

	/* Disable interrupts for this bundle but set the coalescence timer so
	 * that interrupts can be enabled on demand when creating a trans handle
	 */
	WRITE_CSR_INT_COL_EN(bank->csr_addr, hw_bundle_number, 0);
	WRITE_CSR_INT_COL_CTL(bank->csr_addr, hw_bundle_number,
			      bank->irq_coalesc_timer);

	if (uio_register_device(&accel_to_pci_dev(accel_dev)->dev,
				&uiodev->info[bundle]))
		return -ENODEV;
	return 0;
}

static void adf_uio_del_bundle_dev(struct qat_uio_pci_dev *uiodev,
				   struct adf_accel_dev *accel_dev,
				   uint8_t nb_bundles)
{
	unsigned i;

	if (nb_bundles)
		qdm_detach_device(&GET_DEV(accel_dev));

	for (i = 0; i < nb_bundles; i++) {
		irq_set_affinity_hint(uiodev->info[i].irq, NULL);
		adf_uio_sysfs_bundle_delete(accel_dev, i);
		uio_unregister_device(&uiodev->info[i]);
		kfree(uiodev->info[i].priv);
		kfree(uiodev->info[i].name);
		kfree(uiodev->info[i].mem[0].name);
	}
}

static void adf_uio_clean(struct adf_accel_dev *accel_dev,
			  struct qat_uio_pci_dev *uiodev)
{
	adf_uio_del_bundle_dev(uiodev, accel_dev,
			       uiodev->nb_bundles);
	kfree(uiodev->info);
	kfree(uiodev);
	pci_set_drvdata(accel_to_pci_dev(accel_dev), NULL);
}

int adf_uio_register(struct adf_accel_dev *accel_dev)
{
	struct qat_uio_pci_dev *uiodev;
	uint8_t i;
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
	unsigned long num_ker_bundles = 0;

	uiodev = kzalloc(sizeof(*uiodev), GFP_KERNEL);
	if (!uiodev)
		return -ENOMEM;

	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC,
				    ADF_FIRST_USER_BUNDLE, val)) {
		uiodev->nb_bundles = 0;
	}
	else {
		if (kstrtoul(val, 10, &num_ker_bundles))
			goto fail_clean;
		uiodev->nb_bundles = GET_MAX_BANKS(accel_dev) - num_ker_bundles;
	}

	uiodev->info =  kcalloc(uiodev->nb_bundles,
				sizeof(struct uio_info),
				GFP_KERNEL);
	if (!uiodev->info)
		goto fail_clean;

	accel_dev->accel->num_ker_bundles = num_ker_bundles;
	for (i = 0; i < uiodev->nb_bundles; i++) {
		if (adf_uio_create_bundle_dev(accel_dev,
					      uiodev, i, uiodev->nb_bundles))
			goto fail_clean;
		adf_uio_sysfs_bundle_create(accel_to_pci_dev(accel_dev), i,
					    accel_dev->accel);
	}
	if (uiodev->nb_bundles) {
		adf_uio_init_accel_ctrl(accel_dev->accel,
					accel_dev, uiodev->info,
					uiodev->nb_bundles);

		pci_set_drvdata(accel_to_pci_dev(accel_dev), uiodev);

		if (qdm_attach_device(&GET_DEV(accel_dev)))
			goto fail_clean;
	}

	return 0;
fail_clean:
	adf_uio_clean(accel_dev, uiodev);
	dev_err(&accel_to_pci_dev(accel_dev)->dev,
		"Failed to register UIO devices\n");
	return -ENODEV;
}

void adf_uio_remove(struct adf_accel_dev *accel_dev)
{
	struct qat_uio_pci_dev *uiodev =
		pci_get_drvdata(accel_to_pci_dev(accel_dev));

	if (uiodev)
		adf_uio_clean(accel_dev, uiodev);
}

static int adf_uio_event_handler(struct adf_accel_dev *accel_dev,
				 enum adf_event event)
{
	int ret = 0;
	struct device *dev = &GET_DEV(accel_dev);
	char *eventStr = NULL;
	char *deviceIdStr = NULL;
	char *envp[3];

	switch (event) {
	case ADF_EVENT_INIT:
		eventStr = "qat_event=init";
		break;
	case ADF_EVENT_SHUTDOWN:
		eventStr = "qat_event=shutdown";
		break;
	case ADF_EVENT_RESTARTING:
		eventStr = "qat_event=restarting";
		break;
	case ADF_EVENT_RESTARTED:
		eventStr = "qat_event=restarted";
		break;
	case ADF_EVENT_START:
		eventStr = "qat_event=start";
		break;
	case ADF_EVENT_STOP:
		eventStr = "qat_event=stop";
		break;
#ifdef DEFER_UPSTREAM
	case ADF_EVENT_ERROR:
		eventStr = "qat_event=error";
		break;
#endif
	default:
		return -EINVAL;
	}

	deviceIdStr = kasprintf(GFP_ATOMIC, "accelid=%d", accel_dev->accel_id);
	if (!deviceIdStr) {
		dev_err(&GET_DEV(accel_dev), "Failed to allocate memory\n");
		return -ENOMEM;
	}

	envp[0] = eventStr;
	envp[1] = deviceIdStr;
	envp[2] = NULL;
	ret = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
	if (ret)
		dev_err(&GET_DEV(accel_dev), "Failed to send event %s\n",
			eventStr);
	kfree(deviceIdStr);

	return ret;
}

int adf_uio_service_register(void)
{
	memset(&adf_uio_hndl, 0, sizeof(adf_uio_hndl));
	adf_uio_hndl.event_hld = adf_uio_event_handler;
	adf_uio_hndl.name = "adf_event_handler";
	return adf_service_register(&adf_uio_hndl);
}

int adf_uio_service_unregister(void)
{
	return adf_service_unregister(&adf_uio_hndl);
}
