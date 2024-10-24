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
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include "adf_accel_devices.h"
#include "adf_fw_counters.h"
#include "adf_common_drv.h"
#include "icp_qat_fw_init_admin.h"

static DEFINE_MUTEX(qat_fw_counters_read_lock);

static void adf_fw_counters_section_del_all(struct list_head *head);
static void adf_fw_counters_del_all(struct adf_accel_dev *accel_dev);
static int adf_fw_counters_add_key_value_param(struct adf_accel_dev *accel_dev,
					       const char *section_name,
					       const char *key,
					       const void *val);
static int adf_fw_counters_section_add(struct adf_accel_dev *accel_dev,
				       const char *name);

static void *qat_fw_counters_data_start(struct seq_file *sfile, loff_t *pos)
{
	struct adf_fw_counters_data *fw_counters_data = sfile->private;

	seq_printf(sfile,
		   "+------------------------------------------------+\n");
	seq_printf(sfile,
		   "| FW Statistics for Qat Device                   |\n");
	seq_printf(sfile,
		   "+------------------------------------------------+\n");
	mutex_lock(&qat_fw_counters_read_lock);
	return seq_list_start(&fw_counters_data->ae_sec_list, *pos);
}

static int qat_fw_counters_data_show(struct seq_file *sfile, void *v)
{
	struct list_head *list;
	struct adf_fw_counters_section *sec =
			list_entry(v, struct adf_fw_counters_section, list);

	list_for_each(list, &sec->param_head) {
		struct adf_fw_counters_val *ptr =
			list_entry(list, struct adf_fw_counters_val, list);
		seq_printf(sfile, "| %s[%s]:%20s |\n",
			   ptr->key, sec->name, ptr->val);
		if (!strcmp(ptr->key, "Firmware Responses"))
			seq_printf(sfile,
				   "+------------------------------------------------+\n");
	}
	return 0;
}

static void *qat_fw_counters_data_next(struct seq_file *sfile,
				       void *v, loff_t *pos)
{
	struct adf_fw_counters_data *fw_counters_data = sfile->private;

	return seq_list_next(v, &fw_counters_data->ae_sec_list, pos);
}

static void qat_fw_counters_data_stop(struct seq_file *sfile, void *v)
{
	mutex_unlock(&qat_fw_counters_read_lock);
}

static const struct seq_operations qat_fw_counters_data_sops = {
	.start = qat_fw_counters_data_start,
	.next = qat_fw_counters_data_next,
	.stop = qat_fw_counters_data_stop,
	.show = qat_fw_counters_data_show
};

static int qat_fw_counters_data_open(struct inode *inode, struct file *file)
{
	struct adf_accel_dev *accel_dev;
	struct adf_hw_device_data *hw_device;
	struct icp_qat_fw_init_admin_req req;
	struct icp_qat_fw_init_admin_resp resp;
	struct seq_file *seq_f;
	char *aeidstr = NULL;
	unsigned int i = 0;
	int ret = seq_open(file, &qat_fw_counters_data_sops);

	if (ret)
		return ret;

	seq_f = file->private_data;
	accel_dev = inode->i_private;
	if (!accel_dev) {
		ret = -EFAULT;
		goto out_err;
	}

	if (!adf_dev_started(accel_dev)) {
		dev_err(&GET_DEV(accel_dev),
			"Qat Device not started\n");
		ret = -EFAULT;
		goto out_err;
	}

	hw_device = accel_dev->hw_device;
	if (!hw_device) {
		ret = -EFAULT;
		goto out_err;
	}

	adf_fw_counters_del_all(accel_dev);
	memset(&req, 0, sizeof(struct icp_qat_fw_init_admin_req));
	req.init_admin_cmd_id = ICP_QAT_FW_COUNTERS_GET;
	for (i = 0; i < hw_device->get_num_aes(hw_device); i++) {
		memset(&resp, 0, sizeof(struct icp_qat_fw_init_admin_resp));
		if (adf_put_admin_msg_sync(accel_dev, i, &req, &resp) ||
		    resp.init_resp_hdr.status) {
			ret = -EFAULT;
			goto out_err;
		}

		aeidstr = kasprintf(GFP_ATOMIC, "AE %2d", i);
		if (!aeidstr) {
			ret = -ENOMEM;
			goto out_err;
		}

		if (adf_fw_counters_section_add(accel_dev, aeidstr)) {
			ret = -ENOMEM;
			goto fail_clean;
		}

		if (adf_fw_counters_add_key_value_param(
		    accel_dev,
		    aeidstr,
		    "Firmware Requests ",
		    (void *)&resp.init_resp_pars.u.s2.req_rec_count)) {
			adf_fw_counters_del_all(accel_dev);
			ret = -ENOMEM;
			goto fail_clean;
		}

		if (adf_fw_counters_add_key_value_param(
		    accel_dev,
		    aeidstr,
		    "Firmware Responses",
		    (void *)&resp.init_resp_pars.u.s2.resp_sent_count)) {
			adf_fw_counters_del_all(accel_dev);
			ret = -ENOMEM;
			goto fail_clean;
		}
	}
	seq_f->private = accel_dev->fw_counters_data;
	return ret;

fail_clean:
	kfree(aeidstr);
out_err:
	seq_release(inode, file);

	return ret;
}

static const struct file_operations qat_fw_counters_data_fops = {
	.open = qat_fw_counters_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/**
 * adf_fw_counters_add() - Create an acceleration device FW counters table.
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function creates a FW counters statistics table for the given
 * acceleration device.
 * The table stores device specific values of FW Requests sent to the FW and
 * FW Responses received from the FW.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_fw_counters_add(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_counters_data *fw_counters_data;

	fw_counters_data = kzalloc(sizeof(*fw_counters_data), GFP_KERNEL);
	if (!fw_counters_data)
		return -ENOMEM;

	INIT_LIST_HEAD(&fw_counters_data->ae_sec_list);

	init_rwsem(&fw_counters_data->lock);
	accel_dev->fw_counters_data = fw_counters_data;

	/* accel_dev->debugfs_dir should always be non-NULL here */
	fw_counters_data->debug = debugfs_create_file("fw_counters", S_IRUSR,
						  accel_dev->debugfs_dir,
						  accel_dev,
						  &qat_fw_counters_data_fops);
	if (!fw_counters_data->debug) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to create qat fw_counters debugfs entry.\n");
		kfree(fw_counters_data);
		accel_dev->fw_counters_data = NULL;
		return -EFAULT;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(adf_fw_counters_add);

static void adf_fw_counters_del_all(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_counters_data *fw_counters_data =
					accel_dev->fw_counters_data;

	down_write(&fw_counters_data->lock);
	adf_fw_counters_section_del_all(&fw_counters_data->ae_sec_list);
	up_write(&fw_counters_data->lock);
}

static void adf_fw_counters_keyval_add(struct adf_fw_counters_val *new,
				       struct adf_fw_counters_section *sec)
{
	list_add_tail(&new->list, &sec->param_head);
}

static void adf_fw_counters_keyval_del_all(struct list_head *head)
{
	struct list_head *list_ptr, *tmp;

	list_for_each_prev_safe(list_ptr, tmp, head) {
		struct adf_fw_counters_val *ptr =
			list_entry(list_ptr,
				      struct adf_fw_counters_val, list);
		list_del(list_ptr);
		kfree(ptr);
	}
}

static void adf_fw_counters_section_del_all(struct list_head *head)
{
	struct adf_fw_counters_section *ptr;
	struct list_head *list, *tmp;

	list_for_each_prev_safe(list, tmp, head) {
		ptr = list_entry(list, struct adf_fw_counters_section, list);
		adf_fw_counters_keyval_del_all(&ptr->param_head);
		list_del(list);
		kfree(ptr);
	}
}

static struct adf_fw_counters_section *adf_fw_counters_sec_find(
					struct adf_accel_dev *accel_dev,
					const char *sec_name)
{
	struct adf_fw_counters_data *fw_counters_data =
					accel_dev->fw_counters_data;
	struct list_head *list;

	list_for_each(list, &fw_counters_data->ae_sec_list) {
		struct adf_fw_counters_section *ptr =
			list_entry(list,
				      struct adf_fw_counters_section, list);
		if (!strcmp(ptr->name, sec_name))
			return ptr;
	}
	return NULL;
}

static int adf_fw_counters_add_key_value_param(struct adf_accel_dev *accel_dev,
					       const char *section_name,
					       const char *key, const void *val)
{
	struct adf_fw_counters_data *fw_counters_data =
						accel_dev->fw_counters_data;
	struct adf_fw_counters_val *key_val;
	struct adf_fw_counters_section *section = adf_fw_counters_sec_find(
						accel_dev, section_name);
	if (!section)
		return -EFAULT;

	key_val = kzalloc(sizeof(*key_val), GFP_KERNEL);
	if (!key_val)
		return -ENOMEM;

	INIT_LIST_HEAD(&key_val->list);

	strlcpy(key_val->key, key, sizeof(key_val->key));
	snprintf(key_val->val, FW_COUNTERS_MAX_VAL_LEN_IN_BYTES,
		 "%ld", (*((long *)val)));
	down_write(&fw_counters_data->lock);
	adf_fw_counters_keyval_add(key_val, section);
	up_write(&fw_counters_data->lock);
	return 0;
}

/**
 * adf_fw_counters_section_add() - Add AE section entry to FW counters table.
 * @accel_dev:  Pointer to acceleration device.
 * @name: Name of the section
 *
 * Function adds a section for each AE where FW Requests/Responses and their
 * values will be stored.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
static int adf_fw_counters_section_add(struct adf_accel_dev *accel_dev,
				       const char *name)
{
	struct adf_fw_counters_data *fw_counters_data =
					accel_dev->fw_counters_data;
	struct adf_fw_counters_section *sec = adf_fw_counters_sec_find(
							accel_dev, name);

	if (sec)
		return 0;

	sec = kzalloc(sizeof(*sec), GFP_KERNEL);
	if (!sec)
		return -ENOMEM;

	strlcpy(sec->name, name, sizeof(sec->name));
	INIT_LIST_HEAD(&sec->param_head);

	down_write(&fw_counters_data->lock);

	list_add_tail(&sec->list, &fw_counters_data->ae_sec_list);
	up_write(&fw_counters_data->lock);
	return 0;
}

/**
 * adf_fw_counters_remove() - Clears acceleration device FW counters table.
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function removes FW counters table from the given acceleration device
 * and frees all allocated memory.
 * To be used by QAT device specific drivers.
 *
 * Return: void
 */
void adf_fw_counters_remove(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_counters_data *fw_counters_data =
					accel_dev->fw_counters_data;

	if (!fw_counters_data)
		return;

	down_write(&fw_counters_data->lock);
	adf_fw_counters_section_del_all(&fw_counters_data->ae_sec_list);
	up_write(&fw_counters_data->lock);
	debugfs_remove(fw_counters_data->debug);
	kfree(fw_counters_data);
	accel_dev->fw_counters_data = NULL;
}
EXPORT_SYMBOL_GPL(adf_fw_counters_remove);

