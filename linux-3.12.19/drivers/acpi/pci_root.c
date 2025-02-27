/*
 *  pci_root.c - ACPI PCI Root Bridge Driver ($Revision$)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/pci.h>
#include <linux/pci-acpi.h>
#include <linux/pci-aspm.h>
#include <linux/acpi.h>
#include <linux/slab.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <acpi/apei.h>

#define PREFIX "ACPI: "

#define _COMPONENT		ACPI_PCI_COMPONENT
ACPI_MODULE_NAME("pci_root");
#define ACPI_PCI_ROOT_CLASS		"pci_bridge"
#define ACPI_PCI_ROOT_DEVICE_NAME	"PCI Root Bridge"
static int acpi_pci_root_add(struct acpi_device *device,
			     const struct acpi_device_id *not_used);
static void acpi_pci_root_remove(struct acpi_device *device);

#define ACPI_PCIE_REQ_SUPPORT (OSC_EXT_PCI_CONFIG_SUPPORT \
				| OSC_ACTIVE_STATE_PWR_SUPPORT \
				| OSC_CLOCK_PWR_CAPABILITY_SUPPORT \
				| OSC_MSI_SUPPORT)

static const struct acpi_device_id root_device_ids[] = {
	{"PNP0A03", 0},
	{"", 0},
};

static struct acpi_scan_handler pci_root_handler = {
	.ids = root_device_ids,
	.attach = acpi_pci_root_add,
	.detach = acpi_pci_root_remove,
	.hotplug = {
		.ignore = true,
	},
};

static DEFINE_MUTEX(osc_lock);

/**
 * acpi_is_root_bridge - determine whether an ACPI CA node is a PCI root bridge
 * @handle - the ACPI CA node in question.
 *
 * Note: we could make this API take a struct acpi_device * instead, but
 * for now, it's more convenient to operate on an acpi_handle.
 */
int acpi_is_root_bridge(acpi_handle handle)
{
	int ret;
	struct acpi_device *device;

	ret = acpi_bus_get_device(handle, &device);
	if (ret)
		return 0;

	ret = acpi_match_device_ids(device, root_device_ids);
	if (ret)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL_GPL(acpi_is_root_bridge);

static acpi_status
get_root_bridge_busnr_callback(struct acpi_resource *resource, void *data)
{
	struct resource *res = data;
	struct acpi_resource_address64 address;
	acpi_status status;

	status = acpi_resource_to_address64(resource, &address);
	if (ACPI_FAILURE(status))
		return AE_OK;

	if ((address.address_length > 0) &&
	    (address.resource_type == ACPI_BUS_NUMBER_RANGE)) {
		res->start = address.minimum;
		res->end = address.minimum + address.address_length - 1;
	}

	return AE_OK;
}

static acpi_status try_get_root_bridge_busnr(acpi_handle handle,
					     struct resource *res)
{
	acpi_status status;

	res->start = -1;
	status =
	    acpi_walk_resources(handle, METHOD_NAME__CRS,
				get_root_bridge_busnr_callback, res);
	if (ACPI_FAILURE(status))
		return status;
	if (res->start == -1)
		return AE_ERROR;
	return AE_OK;
}

static u8 pci_osc_uuid_str[] = "33DB4D5B-1FF7-401C-9657-7441C03DD766";

static acpi_status acpi_pci_run_osc(acpi_handle handle,
				    const u32 *capbuf, u32 *retval)
{
	struct acpi_osc_context context = {
		.uuid_str = pci_osc_uuid_str,
		.rev = 1,
		.cap.length = 12,
		.cap.pointer = (void *)capbuf,
	};
	acpi_status status;

	status = acpi_run_osc(handle, &context);
	if (ACPI_SUCCESS(status)) {
		*retval = *((u32 *)(context.ret.pointer + 8));
		kfree(context.ret.pointer);
	}
	return status;
}

static acpi_status acpi_pci_query_osc(struct acpi_pci_root *root,
					u32 support,
					u32 *control)
{
	acpi_status status;
	u32 result, capbuf[3];

	support &= OSC_PCI_SUPPORT_MASKS;
	support |= root->osc_support_set;

	capbuf[OSC_QUERY_TYPE] = OSC_QUERY_ENABLE;
	capbuf[OSC_SUPPORT_TYPE] = support;
	if (control) {
		*control &= OSC_PCI_CONTROL_MASKS;
		capbuf[OSC_CONTROL_TYPE] = *control | root->osc_control_set;
	} else {
		/* Run _OSC query only with existing controls. */
		capbuf[OSC_CONTROL_TYPE] = root->osc_control_set;
	}

	status = acpi_pci_run_osc(root->device->handle, capbuf, &result);
	if (ACPI_SUCCESS(status)) {
		root->osc_support_set = support;
		if (control)
			*control = result;
	}
	return status;
}

static acpi_status acpi_pci_osc_support(struct acpi_pci_root *root, u32 flags)
{
	acpi_status status;
	acpi_handle tmp;

	status = acpi_get_handle(root->device->handle, "_OSC", &tmp);
	if (ACPI_FAILURE(status))
		return status;
	mutex_lock(&osc_lock);
	status = acpi_pci_query_osc(root, flags, NULL);
	mutex_unlock(&osc_lock);
	return status;
}

struct acpi_pci_root *acpi_pci_find_root(acpi_handle handle)
{
	struct acpi_pci_root *root;
	struct acpi_device *device;

	if (acpi_bus_get_device(handle, &device) ||
	    acpi_match_device_ids(device, root_device_ids))
		return NULL;

	root = acpi_driver_data(device);

	return root;
}
EXPORT_SYMBOL_GPL(acpi_pci_find_root);

struct acpi_handle_node {
	struct list_head node;
	acpi_handle handle;
};

/**
 * acpi_get_pci_dev - convert ACPI CA handle to struct pci_dev
 * @handle: the handle in question
 *
 * Given an ACPI CA handle, the desired PCI device is located in the
 * list of PCI devices.
 *
 * If the device is found, its reference count is increased and this
 * function returns a pointer to its data structure.  The caller must
 * decrement the reference count by calling pci_dev_put().
 * If no device is found, %NULL is returned.
 */
struct pci_dev *acpi_get_pci_dev(acpi_handle handle)
{
	int dev, fn;
	unsigned long long adr;
	acpi_status status;
	acpi_handle phandle;
	struct pci_bus *pbus;
	struct pci_dev *pdev = NULL;
	struct acpi_handle_node *node, *tmp;
	struct acpi_pci_root *root;
	LIST_HEAD(device_list);

	/*
	 * Walk up the ACPI CA namespace until we reach a PCI root bridge.
	 */
	phandle = handle;
	while (!acpi_is_root_bridge(phandle)) {
		node = kzalloc(sizeof(struct acpi_handle_node), GFP_KERNEL);
		if (!node)
			goto out;

		INIT_LIST_HEAD(&node->node);
		node->handle = phandle;
		list_add(&node->node, &device_list);

		status = acpi_get_parent(phandle, &phandle);
		if (ACPI_FAILURE(status))
			goto out;
	}

	root = acpi_pci_find_root(phandle);
	if (!root)
		goto out;

	pbus = root->bus;

	/*
	 * Now, walk back down the PCI device tree until we return to our
	 * original handle. Assumes that everything between the PCI root
	 * bridge and the device we're looking for must be a P2P bridge.
	 */
	list_for_each_entry(node, &device_list, node) {
		acpi_handle hnd = node->handle;
		status = acpi_evaluate_integer(hnd, "_ADR", NULL, &adr);
		if (ACPI_FAILURE(status))
			goto out;
		dev = (adr >> 16) & 0xffff;
		fn  = adr & 0xffff;

		pdev = pci_get_slot(pbus, PCI_DEVFN(dev, fn));
		if (!pdev || hnd == handle)
			break;

		pbus = pdev->subordinate;
		pci_dev_put(pdev);

		/*
		 * This function may be called for a non-PCI device that has a
		 * PCI parent (eg. a disk under a PCI SATA controller).  In that
		 * case pdev->subordinate will be NULL for the parent.
		 */
		if (!pbus) {
			dev_dbg(&pdev->dev, "Not a PCI-to-PCI bridge\n");
			pdev = NULL;
			break;
		}
	}
out:
	list_for_each_entry_safe(node, tmp, &device_list, node)
		kfree(node);

	return pdev;
}
EXPORT_SYMBOL_GPL(acpi_get_pci_dev);

/**
 * acpi_pci_osc_control_set - Request control of PCI root _OSC features.
 * @handle: ACPI handle of a PCI root bridge (or PCIe Root Complex).
 * @mask: Mask of _OSC bits to request control of, place to store control mask.
 * @req: Mask of _OSC bits the control of is essential to the caller.
 *
 * Run _OSC query for @mask and if that is successful, compare the returned
 * mask of control bits with @req.  If all of the @req bits are set in the
 * returned mask, run _OSC request for it.
 *
 * The variable at the @mask address may be modified regardless of whether or
 * not the function returns success.  On success it will contain the mask of
 * _OSC bits the BIOS has granted control of, but its contents are meaningless
 * on failure.
 **/
acpi_status acpi_pci_osc_control_set(acpi_handle handle, u32 *mask, u32 req)
{
	struct acpi_pci_root *root;
	acpi_status status;
	u32 ctrl, capbuf[3];
	acpi_handle tmp;

	if (!mask)
		return AE_BAD_PARAMETER;

	ctrl = *mask & OSC_PCI_CONTROL_MASKS;
	if ((ctrl & req) != req)
		return AE_TYPE;

	root = acpi_pci_find_root(handle);
	if (!root)
		return AE_NOT_EXIST;

	status = acpi_get_handle(handle, "_OSC", &tmp);
	if (ACPI_FAILURE(status))
		return status;

	mutex_lock(&osc_lock);

	*mask = ctrl | root->osc_control_set;
	/* No need to evaluate _OSC if the control was already granted. */
	if ((root->osc_control_set & ctrl) == ctrl)
		goto out;

	/* Need to check the available controls bits before requesting them. */
	while (*mask) {
		status = acpi_pci_query_osc(root, root->osc_support_set, mask);
		if (ACPI_FAILURE(status))
			goto out;
		if (ctrl == *mask)
			break;
		ctrl = *mask;
	}

	if ((ctrl & req) != req) {
		status = AE_SUPPORT;
		goto out;
	}

	capbuf[OSC_QUERY_TYPE] = 0;
	capbuf[OSC_SUPPORT_TYPE] = root->osc_support_set;
	capbuf[OSC_CONTROL_TYPE] = ctrl;
	status = acpi_pci_run_osc(handle, capbuf, mask);
	if (ACPI_SUCCESS(status))
		root->osc_control_set = *mask;
out:
	mutex_unlock(&osc_lock);
	return status;
}
EXPORT_SYMBOL(acpi_pci_osc_control_set);

static int acpi_pci_root_add(struct acpi_device *device,
			     const struct acpi_device_id *not_used)
{
	unsigned long long segment, bus;
	acpi_status status;
	int result;
	struct acpi_pci_root *root;
	u32 flags, base_flags;
	acpi_handle handle = device->handle;
	bool no_aspm = false, clear_aspm = false;

	root = kzalloc(sizeof(struct acpi_pci_root), GFP_KERNEL);
	if (!root)
		return -ENOMEM;

	segment = 0;
	status = acpi_evaluate_integer(handle, METHOD_NAME__SEG, NULL,
				       &segment);
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		dev_err(&device->dev,  "can't evaluate _SEG\n");
		result = -ENODEV;
		goto end;
	}

	/* Check _CRS first, then _BBN.  If no _BBN, default to zero. */
	root->secondary.flags = IORESOURCE_BUS;
	status = try_get_root_bridge_busnr(handle, &root->secondary);
	if (ACPI_FAILURE(status)) {
		/*
		 * We need both the start and end of the downstream bus range
		 * to interpret _CBA (MMCONFIG base address), so it really is
		 * supposed to be in _CRS.  If we don't find it there, all we
		 * can do is assume [_BBN-0xFF] or [0-0xFF].
		 */
		root->secondary.end = 0xFF;
		dev_warn(&device->dev,
			 FW_BUG "no secondary bus range in _CRS\n");
		status = acpi_evaluate_integer(handle, METHOD_NAME__BBN,
					       NULL, &bus);
		if (ACPI_SUCCESS(status))
			root->secondary.start = bus;
		else if (status == AE_NOT_FOUND)
			root->secondary.start = 0;
		else {
			dev_err(&device->dev, "can't evaluate _BBN\n");
			result = -ENODEV;
			goto end;
		}
	}

	root->device = device;
	root->segment = segment & 0xFFFF;
	strcpy(acpi_device_name(device), ACPI_PCI_ROOT_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_PCI_ROOT_CLASS);
	device->driver_data = root;

	pr_info(PREFIX "%s [%s] (domain %04x %pR)\n",
	       acpi_device_name(device), acpi_device_bid(device),
	       root->segment, &root->secondary);

	root->mcfg_addr = acpi_pci_root_get_mcfg_addr(handle);

	/*
	 * All supported architectures that use ACPI have support for
	 * PCI domains, so we indicate this in _OSC support capabilities.
	 */
	flags = base_flags = OSC_PCI_SEGMENT_GROUPS_SUPPORT;
	acpi_pci_osc_support(root, flags);

	if (pci_ext_cfg_avail())
		flags |= OSC_EXT_PCI_CONFIG_SUPPORT;
	if (pcie_aspm_support_enabled()) {
		flags |= OSC_ACTIVE_STATE_PWR_SUPPORT |
		OSC_CLOCK_PWR_CAPABILITY_SUPPORT;
	}
	if (pci_msi_enabled())
		flags |= OSC_MSI_SUPPORT;
	if (flags != base_flags) {
		status = acpi_pci_osc_support(root, flags);
		if (ACPI_FAILURE(status)) {
			dev_info(&device->dev, "ACPI _OSC support "
				"notification failed, disabling PCIe ASPM\n");
			no_aspm = true;
			flags = base_flags;
		}
	}

	if (!pcie_ports_disabled
	    && (flags & ACPI_PCIE_REQ_SUPPORT) == ACPI_PCIE_REQ_SUPPORT) {
		flags = OSC_PCI_EXPRESS_CAP_STRUCTURE_CONTROL
			| OSC_PCI_EXPRESS_NATIVE_HP_CONTROL
			| OSC_PCI_EXPRESS_PME_CONTROL;

		if (pci_aer_available()) {
			if (aer_acpi_firmware_first())
				dev_dbg(&device->dev,
					"PCIe errors handled by BIOS.\n");
			else
				flags |= OSC_PCI_EXPRESS_AER_CONTROL;
		}

		dev_info(&device->dev,
			"Requesting ACPI _OSC control (0x%02x)\n", flags);

		status = acpi_pci_osc_control_set(handle, &flags,
				       OSC_PCI_EXPRESS_CAP_STRUCTURE_CONTROL);
		if (ACPI_SUCCESS(status)) {
			dev_info(&device->dev,
				"ACPI _OSC control (0x%02x) granted\n", flags);
			if (acpi_gbl_FADT.boot_flags & ACPI_FADT_NO_ASPM) {
				/*
				 * We have ASPM control, but the FADT indicates
				 * that it's unsupported. Clear it.
				 */
				clear_aspm = true;
			}
		} else {
			dev_info(&device->dev,
				"ACPI _OSC request failed (%s), "
				"returned control mask: 0x%02x\n",
				acpi_format_exception(status), flags);
			dev_info(&device->dev,
				 "ACPI _OSC control for PCIe not granted, disabling ASPM\n");
			/*
			 * We want to disable ASPM here, but aspm_disabled
			 * needs to remain in its state from boot so that we
			 * properly handle PCIe 1.1 devices.  So we set this
			 * flag here, to defer the action until after the ACPI
			 * root scan.
			 */
			no_aspm = true;
		}
	} else {
		dev_info(&device->dev,
			 "Unable to request _OSC control "
			 "(_OSC support mask: 0x%02x)\n", flags);
	}

	/*
	 * TBD: Need PCI interface for enumeration/configuration of roots.
	 */

	/*
	 * Scan the Root Bridge
	 * --------------------
	 * Must do this prior to any attempt to bind the root device, as the
	 * PCI namespace does not get created until this call is made (and
	 * thus the root bridge's pci_dev does not exist).
	 */
	root->bus = pci_acpi_scan_root(root);
	if (!root->bus) {
		dev_err(&device->dev,
			"Bus %04x:%02x not present in PCI namespace\n",
			root->segment, (unsigned int)root->secondary.start);
		result = -ENODEV;
		goto end;
	}

	if (clear_aspm) {
		dev_info(&device->dev, "Disabling ASPM (FADT indicates it is unsupported)\n");
		pcie_clear_aspm(root->bus);
	}
	if (no_aspm)
		pcie_no_aspm();

	pci_acpi_add_bus_pm_notifier(device, root->bus);
	if (device->wakeup.flags.run_wake)
		device_set_run_wake(root->bus->bridge, true);

	if (system_state != SYSTEM_BOOTING) {
		pcibios_resource_survey_bus(root->bus);
		pci_assign_unassigned_root_bus_resources(root->bus);
	}

	pci_bus_add_devices(root->bus);
	return 1;

end:
	kfree(root);
	return result;
}

static void acpi_pci_root_remove(struct acpi_device *device)
{
	struct acpi_pci_root *root = acpi_driver_data(device);

	pci_stop_root_bus(root->bus);

	device_set_run_wake(root->bus->bridge, false);
	pci_acpi_remove_bus_pm_notifier(device);

	pci_remove_root_bus(root->bus);

	kfree(root);
}

void __init acpi_pci_root_init(void)
{
	acpi_hest_init();

	if (!acpi_pci_disabled) {
		pci_acpi_crs_quirks();
		acpi_scan_add_handler(&pci_root_handler);
	}
}
/* Support root bridge hotplug */

static void handle_root_bridge_insertion(acpi_handle handle)
{
	struct acpi_device *device;

	if (!acpi_bus_get_device(handle, &device)) {
		dev_printk(KERN_DEBUG, &device->dev,
			   "acpi device already exists; ignoring notify\n");
		return;
	}

	if (acpi_bus_scan(handle))
		acpi_handle_err(handle, "cannot add bridge to acpi list\n");
}

static void handle_root_bridge_removal(struct acpi_device *device)
{
	acpi_status status;
	struct acpi_eject_event *ej_event;

	ej_event = kmalloc(sizeof(*ej_event), GFP_KERNEL);
	if (!ej_event) {
		/* Inform firmware the hot-remove operation has error */
		(void) acpi_evaluate_hotplug_ost(device->handle,
					ACPI_NOTIFY_EJECT_REQUEST,
					ACPI_OST_SC_NON_SPECIFIC_FAILURE,
					NULL);
		return;
	}

	ej_event->device = device;
	ej_event->event = ACPI_NOTIFY_EJECT_REQUEST;

	get_device(&device->dev);
	status = acpi_os_hotplug_execute(acpi_bus_hot_remove_device, ej_event);
	if (ACPI_FAILURE(status)) {
		put_device(&device->dev);
		kfree(ej_event);
	}
}

static void _handle_hotplug_event_root(struct work_struct *work)
{
	struct acpi_pci_root *root;
	struct acpi_hp_work *hp_work;
	acpi_handle handle;
	u32 type;

	hp_work = container_of(work, struct acpi_hp_work, work);
	handle = hp_work->handle;
	type = hp_work->type;

	acpi_scan_lock_acquire();

	root = acpi_pci_find_root(handle);

	switch (type) {
	case ACPI_NOTIFY_BUS_CHECK:
		/* bus enumerate */
		acpi_handle_printk(KERN_DEBUG, handle,
				   "Bus check notify on %s\n", __func__);
		if (root)
			acpiphp_check_host_bridge(handle);
		else
			handle_root_bridge_insertion(handle);

		break;

	case ACPI_NOTIFY_DEVICE_CHECK:
		/* device check */
		acpi_handle_printk(KERN_DEBUG, handle,
				   "Device check notify on %s\n", __func__);
		if (!root)
			handle_root_bridge_insertion(handle);
		break;

	case ACPI_NOTIFY_EJECT_REQUEST:
		/* request device eject */
		acpi_handle_printk(KERN_DEBUG, handle,
				   "Device eject notify on %s\n", __func__);
		if (root)
			handle_root_bridge_removal(root->device);
		break;
	default:
		acpi_handle_warn(handle,
				 "notify_handler: unknown event type 0x%x\n",
				 type);
		break;
	}

	acpi_scan_lock_release();
	kfree(hp_work); /* allocated in handle_hotplug_event_bridge */
}

static void handle_hotplug_event_root(acpi_handle handle, u32 type,
					void *context)
{
	alloc_acpi_hp_work(handle, type, context,
				_handle_hotplug_event_root);
}

static acpi_status __init
find_root_bridges(acpi_handle handle, u32 lvl, void *context, void **rv)
{
	acpi_status status;
	int *count = (int *)context;

	if (!acpi_is_root_bridge(handle))
		return AE_OK;

	(*count)++;

	status = acpi_install_notify_handler(handle, ACPI_SYSTEM_NOTIFY,
					handle_hotplug_event_root, NULL);
	if (ACPI_FAILURE(status))
		acpi_handle_printk(KERN_DEBUG, handle,
			"notify handler is not installed, exit status: %u\n",
			 (unsigned int)status);
	else
		acpi_handle_printk(KERN_DEBUG, handle,
				   "notify handler is installed\n");

	return AE_OK;
}

void __init acpi_pci_root_hp_init(void)
{
	int num = 0;

	acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
		ACPI_UINT32_MAX, find_root_bridges, NULL, &num, NULL);

	printk(KERN_DEBUG "Found %d acpi root devices\n", num);
}
