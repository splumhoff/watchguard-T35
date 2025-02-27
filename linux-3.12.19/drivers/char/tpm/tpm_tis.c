/*
 * Copyright (C) 2005, 2006 IBM Corporation
 *
 * Authors:
 * Leendert van Doorn <leendert@watson.ibm.com>
 * Kylene Hall <kjhall@us.ibm.com>
 *
 * Maintained by: <tpmdd-devel@lists.sourceforge.net>
 *
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org
 *
 * This device driver implements the TPM interface as defined in
 * the TCG TPM Interface Spec version 1.2, revision 1.0.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pnp.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/acpi.h>
#include <linux/freezer.h>
#include "tpm.h"

enum tis_access {
	TPM_ACCESS_VALID = 0x80,
	TPM_ACCESS_ACTIVE_LOCALITY = 0x20,
	TPM_ACCESS_REQUEST_PENDING = 0x04,
	TPM_ACCESS_REQUEST_USE = 0x02,
};

enum tis_status {
	TPM_STS_VALID = 0x80,
	TPM_STS_COMMAND_READY = 0x40,
	TPM_STS_GO = 0x20,
	TPM_STS_DATA_AVAIL = 0x10,
	TPM_STS_DATA_EXPECT = 0x08,
};

enum tis_int_flags {
	TPM_GLOBAL_INT_ENABLE = 0x80000000,
	TPM_INTF_BURST_COUNT_STATIC = 0x100,
	TPM_INTF_CMD_READY_INT = 0x080,
	TPM_INTF_INT_EDGE_FALLING = 0x040,
	TPM_INTF_INT_EDGE_RISING = 0x020,
	TPM_INTF_INT_LEVEL_LOW = 0x010,
	TPM_INTF_INT_LEVEL_HIGH = 0x008,
	TPM_INTF_LOCALITY_CHANGE_INT = 0x004,
	TPM_INTF_STS_VALID_INT = 0x002,
	TPM_INTF_DATA_AVAIL_INT = 0x001,
};

enum tis_defaults {
	TIS_MEM_BASE = 0xFED40000,
	TIS_MEM_LEN = 0x5000,
#ifdef CONFIG_WG_PLATFORM_PATCHES
	TIS_SHORT_TIMEOUT = 2000,	/* 2.0 sec */
	TIS_LONG_TIMEOUT = 2800,	/* 2.8 sec */
#else
	TIS_SHORT_TIMEOUT = 750,	/* ms */
	TIS_LONG_TIMEOUT = 2000,	/* 2 sec */
#endif
};

#define	TPM_ACCESS(l)			(0x0000 | ((l) << 12))
#define	TPM_INT_ENABLE(l)		(0x0008 | ((l) << 12))
#define	TPM_INT_VECTOR(l)		(0x000C | ((l) << 12))
#define	TPM_INT_STATUS(l)		(0x0010 | ((l) << 12))
#define	TPM_INTF_CAPS(l)		(0x0014 | ((l) << 12))
#define	TPM_STS(l)			(0x0018 | ((l) << 12))
#define	TPM_DATA_FIFO(l)		(0x0024 | ((l) << 12))

#define	TPM_DID_VID(l)			(0x0F00 | ((l) << 12))
#define	TPM_RID(l)			(0x0F04 | ((l) << 12))

static LIST_HEAD(tis_chips);
static DEFINE_MUTEX(tis_lock);

#if defined(CONFIG_PNP) && defined(CONFIG_ACPI)
static int is_itpm(struct pnp_dev *dev)
{
	struct acpi_device *acpi = pnp_acpi_device(dev);
	struct acpi_hardware_id *id;

	if (!acpi)
		return 0;

	list_for_each_entry(id, &acpi->pnp.ids, list) {
		if (!strcmp("INTC0102", id->id))
			return 1;
	}

	return 0;
}
#else
static inline int is_itpm(struct pnp_dev *dev)
{
	return 0;
}
#endif

/* Before we attempt to access the TPM we must see that the valid bit is set.
 * The specification says that this bit is 0 at reset and remains 0 until the
 * 'TPM has gone through its self test and initialization and has established
 * correct values in the other bits.' */
static int wait_startup(struct tpm_chip *chip, int l)
{
	unsigned long stop = jiffies + chip->vendor.timeout_a;
	do {
		if (ioread8(chip->vendor.iobase + TPM_ACCESS(l)) &
		    TPM_ACCESS_VALID)
			return 0;
		msleep(TPM_TIMEOUT);
	} while (time_before(jiffies, stop));
	return -1;
}

static int check_locality(struct tpm_chip *chip, int l)
{
	if ((ioread8(chip->vendor.iobase + TPM_ACCESS(l)) &
	     (TPM_ACCESS_ACTIVE_LOCALITY | TPM_ACCESS_VALID)) ==
	    (TPM_ACCESS_ACTIVE_LOCALITY | TPM_ACCESS_VALID))
		return chip->vendor.locality = l;

	return -1;
}

static void release_locality(struct tpm_chip *chip, int l, int force)
{
	if (force || (ioread8(chip->vendor.iobase + TPM_ACCESS(l)) &
		      (TPM_ACCESS_REQUEST_PENDING | TPM_ACCESS_VALID)) ==
	    (TPM_ACCESS_REQUEST_PENDING | TPM_ACCESS_VALID))
		iowrite8(TPM_ACCESS_ACTIVE_LOCALITY,
			 chip->vendor.iobase + TPM_ACCESS(l));
}

static int request_locality(struct tpm_chip *chip, int l)
{
	unsigned long stop, timeout;
	long rc;

	if (check_locality(chip, l) >= 0)
		return l;

	iowrite8(TPM_ACCESS_REQUEST_USE,
		 chip->vendor.iobase + TPM_ACCESS(l));

	stop = jiffies + chip->vendor.timeout_a;

	if (chip->vendor.irq) {
again:
		timeout = stop - jiffies;
		if ((long)timeout <= 0)
			return -1;
		rc = wait_event_interruptible_timeout(chip->vendor.int_queue,
						      (check_locality
						       (chip, l) >= 0),
						      timeout);
		if (rc > 0)
			return l;
		if (rc == -ERESTARTSYS && freezing(current)) {
			clear_thread_flag(TIF_SIGPENDING);
			goto again;
		}
	} else {
		/* wait for burstcount */
		do {
			if (check_locality(chip, l) >= 0)
				return l;
			msleep(TPM_TIMEOUT);
		}
		while (time_before(jiffies, stop));
	}
	return -1;
}

static u8 tpm_tis_status(struct tpm_chip *chip)
{
	return ioread8(chip->vendor.iobase +
		       TPM_STS(chip->vendor.locality));
}

static void tpm_tis_ready(struct tpm_chip *chip)
{
	/* this causes the current command to be aborted */
	iowrite8(TPM_STS_COMMAND_READY,
		 chip->vendor.iobase + TPM_STS(chip->vendor.locality));
}

static int get_burstcount(struct tpm_chip *chip)
{
	unsigned long stop;
	int burstcnt;

	/* wait for burstcount */
	/* which timeout value, spec has 2 answers (c & d) */
	stop = jiffies + chip->vendor.timeout_d;
	do {
		burstcnt = ioread8(chip->vendor.iobase +
				   TPM_STS(chip->vendor.locality) + 1);
		burstcnt += ioread8(chip->vendor.iobase +
				    TPM_STS(chip->vendor.locality) +
				    2) << 8;
		if (burstcnt)
			return burstcnt;
		msleep(TPM_TIMEOUT);
	} while (time_before(jiffies, stop));
	return -EBUSY;
}

static int recv_data(struct tpm_chip *chip, u8 *buf, size_t count)
{
	int size = 0, burstcnt;
	while (size < count &&
	       wait_for_tpm_stat(chip,
				 TPM_STS_DATA_AVAIL | TPM_STS_VALID,
				 chip->vendor.timeout_c,
				 &chip->vendor.read_queue, true)
	       == 0) {
		burstcnt = get_burstcount(chip);
		for (; burstcnt > 0 && size < count; burstcnt--)
			buf[size++] = ioread8(chip->vendor.iobase +
					      TPM_DATA_FIFO(chip->vendor.
							    locality));
	}
	return size;
}

static int tpm_tis_recv(struct tpm_chip *chip, u8 *buf, size_t count)
{
	int size = 0;
	int expected, status;

	if (count < TPM_HEADER_SIZE) {
		size = -EIO;
		goto out;
	}

	/* read first 10 bytes, including tag, paramsize, and result */
	if ((size =
	     recv_data(chip, buf, TPM_HEADER_SIZE)) < TPM_HEADER_SIZE) {
		dev_err(chip->dev, "Unable to read header\n");
		goto out;
	}

	expected = be32_to_cpu(*(__be32 *) (buf + 2));
	if (expected > count) {
		size = -EIO;
		goto out;
	}

	if ((size +=
	     recv_data(chip, &buf[TPM_HEADER_SIZE],
		       expected - TPM_HEADER_SIZE)) < expected) {
		dev_err(chip->dev, "Unable to read remainder of result\n");
		size = -ETIME;
		goto out;
	}

	wait_for_tpm_stat(chip, TPM_STS_VALID, chip->vendor.timeout_c,
			  &chip->vendor.int_queue, false);
	status = tpm_tis_status(chip);
	if (status & TPM_STS_DATA_AVAIL) {	/* retry? */
		dev_err(chip->dev, "Error left over data\n");
		size = -EIO;
		goto out;
	}

out:
	tpm_tis_ready(chip);
	release_locality(chip, chip->vendor.locality, 0);
	return size;
}

static bool itpm;
module_param(itpm, bool, 0444);
MODULE_PARM_DESC(itpm, "Force iTPM workarounds (found on some Lenovo laptops)");

/*
 * If interrupts are used (signaled by an irq set in the vendor structure)
 * tpm.c can skip polling for the data to be available as the interrupt is
 * waited for here
 */
static int tpm_tis_send_data(struct tpm_chip *chip, u8 *buf, size_t len)
{
	int rc, status, burstcnt;
#ifdef CONFIG_WG_PLATFORM_PATCHES
	int rc1;
#endif
	size_t count = 0;

	if (request_locality(chip, 0) < 0)
		return -EBUSY;

	status = tpm_tis_status(chip);
	if ((status & TPM_STS_COMMAND_READY) == 0) {
		tpm_tis_ready(chip);
#ifdef CONFIG_WG_PLATFORM_PATCHES
		rc1 = wait_for_tpm_stat
		    (chip, TPM_STS_COMMAND_READY, chip->vendor.timeout_b,
		     &chip->vendor.int_queue, false);
		if (rc1 < 0) {
			dev_err(chip->dev,
			 "wait_for_tpm_stat failed: rc1 = %d \n", rc1);
#else
		if(wait_for_tpm_stat
		    (chip, TPM_STS_COMMAND_READY, chip->vendor.timeout_b,
		     &chip->vendor.int_queue, false) < 0) {
#endif
			rc = -ETIME;
			goto out_err;
		}
	}

	while (count < len - 1) {
		burstcnt = get_burstcount(chip);
		for (; burstcnt > 0 && count < len - 1; burstcnt--) {
			iowrite8(buf[count], chip->vendor.iobase +
				 TPM_DATA_FIFO(chip->vendor.locality));
			count++;
		}

		wait_for_tpm_stat(chip, TPM_STS_VALID, chip->vendor.timeout_c,
				  &chip->vendor.int_queue, false);
		status = tpm_tis_status(chip);
		if (!itpm && (status & TPM_STS_DATA_EXPECT) == 0) {
			rc = -EIO;
			goto out_err;
		}
	}

	/* write last byte */
	iowrite8(buf[count],
		 chip->vendor.iobase + TPM_DATA_FIFO(chip->vendor.locality));
	wait_for_tpm_stat(chip, TPM_STS_VALID, chip->vendor.timeout_c,
			  &chip->vendor.int_queue, false);
	status = tpm_tis_status(chip);
	if ((status & TPM_STS_DATA_EXPECT) != 0) {
		rc = -EIO;
		goto out_err;
	}

	return 0;

out_err:
	tpm_tis_ready(chip);
	release_locality(chip, chip->vendor.locality, 0);
	return rc;
}

/*
 * If interrupts are used (signaled by an irq set in the vendor structure)
 * tpm.c can skip polling for the data to be available as the interrupt is
 * waited for here
 */
static int tpm_tis_send(struct tpm_chip *chip, u8 *buf, size_t len)
{
	int rc;
	u32 ordinal;

	rc = tpm_tis_send_data(chip, buf, len);
#ifdef CONFIG_WG_PLATFORM_PATCHES
	if (rc < 0) {
		dev_err(chip->dev,
		 "tpm_tis_send: tpm_tis_send_data failed: rc = %d \n", rc);
		return rc;
	}
#else
	if (rc < 0)
		return rc;
#endif

	/* go and do it */
	iowrite8(TPM_STS_GO,
		 chip->vendor.iobase + TPM_STS(chip->vendor.locality));

	if (chip->vendor.irq) {
		ordinal = be32_to_cpu(*((__be32 *) (buf + 6)));
		if (wait_for_tpm_stat
		    (chip, TPM_STS_DATA_AVAIL | TPM_STS_VALID,
		     tpm_calc_ordinal_duration(chip, ordinal),
		     &chip->vendor.read_queue, false) < 0) {
#ifdef CONFIG_WG_PLATFORM_PATCHES
		dev_err(chip->dev,
		 "tpm_tis_send: (TPM_STS_DATA_AVAIL | TPM_STS_VALID) check failed\n");
#endif
			rc = -ETIME;
			goto out_err;
		}
	}
	return len;
out_err:
#ifdef CONFIG_WG_PLATFORM_PATCHES
	dev_err(chip->dev,
	 "tpm_tis_send: at goto out_err, rc = %d\n", rc);
#endif
	tpm_tis_ready(chip);
	release_locality(chip, chip->vendor.locality, 0);
	return rc;
}

/*
 * Early probing for iTPM with STS_DATA_EXPECT flaw.
 * Try sending command without itpm flag set and if that
 * fails, repeat with itpm flag set.
 */
static int probe_itpm(struct tpm_chip *chip)
{
	int rc = 0;
	u8 cmd_getticks[] = {
		0x00, 0xc1, 0x00, 0x00, 0x00, 0x0a,
		0x00, 0x00, 0x00, 0xf1
	};
	size_t len = sizeof(cmd_getticks);
	bool rem_itpm = itpm;
	u16 vendor = ioread16(chip->vendor.iobase + TPM_DID_VID(0));

	/* probe only iTPMS */
	if (vendor != TPM_VID_INTEL)
		return 0;

	itpm = false;

	rc = tpm_tis_send_data(chip, cmd_getticks, len);
	if (rc == 0)
		goto out;

	tpm_tis_ready(chip);
	release_locality(chip, chip->vendor.locality, 0);

	itpm = true;

	rc = tpm_tis_send_data(chip, cmd_getticks, len);
	if (rc == 0) {
		dev_info(chip->dev, "Detected an iTPM.\n");
		rc = 1;
	} else
		rc = -EFAULT;

out:
	itpm = rem_itpm;
	tpm_tis_ready(chip);
	release_locality(chip, chip->vendor.locality, 0);

	return rc;
}

static bool tpm_tis_req_canceled(struct tpm_chip *chip, u8 status)
{
	switch (chip->vendor.manufacturer_id) {
	case TPM_VID_WINBOND:
		return ((status == TPM_STS_VALID) ||
			(status == (TPM_STS_VALID | TPM_STS_COMMAND_READY)));
	case TPM_VID_STM:
		return (status == (TPM_STS_VALID | TPM_STS_COMMAND_READY));
	default:
		return (status == TPM_STS_COMMAND_READY);
	}
}

static const struct file_operations tis_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = tpm_open,
	.read = tpm_read,
	.write = tpm_write,
	.release = tpm_release,
};

static DEVICE_ATTR(pubek, S_IRUGO, tpm_show_pubek, NULL);
static DEVICE_ATTR(pcrs, S_IRUGO, tpm_show_pcrs, NULL);
static DEVICE_ATTR(enabled, S_IRUGO, tpm_show_enabled, NULL);
static DEVICE_ATTR(active, S_IRUGO, tpm_show_active, NULL);
static DEVICE_ATTR(owned, S_IRUGO, tpm_show_owned, NULL);
static DEVICE_ATTR(temp_deactivated, S_IRUGO, tpm_show_temp_deactivated,
		   NULL);
static DEVICE_ATTR(caps, S_IRUGO, tpm_show_caps_1_2, NULL);
static DEVICE_ATTR(cancel, S_IWUSR | S_IWGRP, NULL, tpm_store_cancel);
static DEVICE_ATTR(durations, S_IRUGO, tpm_show_durations, NULL);
static DEVICE_ATTR(timeouts, S_IRUGO, tpm_show_timeouts, NULL);

static struct attribute *tis_attrs[] = {
	&dev_attr_pubek.attr,
	&dev_attr_pcrs.attr,
	&dev_attr_enabled.attr,
	&dev_attr_active.attr,
	&dev_attr_owned.attr,
	&dev_attr_temp_deactivated.attr,
	&dev_attr_caps.attr,
	&dev_attr_cancel.attr,
	&dev_attr_durations.attr,
	&dev_attr_timeouts.attr, NULL,
};

static struct attribute_group tis_attr_grp = {
	.attrs = tis_attrs
};

static struct tpm_vendor_specific tpm_tis = {
	.status = tpm_tis_status,
	.recv = tpm_tis_recv,
	.send = tpm_tis_send,
	.cancel = tpm_tis_ready,
	.req_complete_mask = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_complete_val = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_canceled = tpm_tis_req_canceled,
	.attr_group = &tis_attr_grp,
	.miscdev = {
		    .fops = &tis_ops,},
};

static irqreturn_t tis_int_probe(int irq, void *dev_id)
{
	struct tpm_chip *chip = dev_id;
	u32 interrupt;

	interrupt = ioread32(chip->vendor.iobase +
			     TPM_INT_STATUS(chip->vendor.locality));

	if (interrupt == 0)
		return IRQ_NONE;

	chip->vendor.probed_irq = irq;

	/* Clear interrupts handled with TPM_EOI */
	iowrite32(interrupt,
		  chip->vendor.iobase +
		  TPM_INT_STATUS(chip->vendor.locality));
	return IRQ_HANDLED;
}

static irqreturn_t tis_int_handler(int dummy, void *dev_id)
{
	struct tpm_chip *chip = dev_id;
	u32 interrupt;
	int i;

	interrupt = ioread32(chip->vendor.iobase +
			     TPM_INT_STATUS(chip->vendor.locality));

	if (interrupt == 0)
		return IRQ_NONE;

	if (interrupt & TPM_INTF_DATA_AVAIL_INT)
		wake_up_interruptible(&chip->vendor.read_queue);
	if (interrupt & TPM_INTF_LOCALITY_CHANGE_INT)
		for (i = 0; i < 5; i++)
			if (check_locality(chip, i) >= 0)
				break;
	if (interrupt &
	    (TPM_INTF_LOCALITY_CHANGE_INT | TPM_INTF_STS_VALID_INT |
	     TPM_INTF_CMD_READY_INT))
		wake_up_interruptible(&chip->vendor.int_queue);

	/* Clear interrupts handled with TPM_EOI */
	iowrite32(interrupt,
		  chip->vendor.iobase +
		  TPM_INT_STATUS(chip->vendor.locality));
	ioread32(chip->vendor.iobase + TPM_INT_STATUS(chip->vendor.locality));
	return IRQ_HANDLED;
}


#ifdef CONFIG_WG_PLATFORM_PATCHES 
// WG:MJ Bump the timeout if case of ATMEL

struct tis_vendor_timeout_override {
	u32 did_vid;
	unsigned long timeout_us[4];
};

static const struct tis_vendor_timeout_override vendor_timeout_overrides[] = {
	/* Atmel 3204 */
	{ 0x32041114, { (TIS_SHORT_TIMEOUT*1000), (TIS_LONG_TIMEOUT*1000),
			(TIS_SHORT_TIMEOUT*1000), (TIS_SHORT_TIMEOUT*1000) } },
};

static bool tpm_tis_update_timeouts_quirks(struct tpm_chip *chip,
				    unsigned long *timeout_cap)
{
	int i;
	u32 did_vid;

	did_vid = ioread32(chip->vendor.iobase + TPM_DID_VID(0));

	for (i = 0; i != ARRAY_SIZE(vendor_timeout_overrides); i++) {
		if (vendor_timeout_overrides[i].did_vid != did_vid)
			continue;
		memcpy(timeout_cap, vendor_timeout_overrides[i].timeout_us,
		       sizeof(vendor_timeout_overrides[i].timeout_us));
		return true;
	}

	return false;
}
#endif // CONFIG_WG_PLATFORM_PATCHES 


static bool interrupts = true;
module_param(interrupts, bool, 0444);
MODULE_PARM_DESC(interrupts, "Enable interrupts");

static int tpm_tis_init(struct device *dev, resource_size_t start,
			resource_size_t len, unsigned int irq)
{
	u32 vendor, intfcaps, intmask;
	int rc, i, irq_s, irq_e, probe;
	struct tpm_chip *chip;
#ifdef CONFIG_WG_PLATFORM_PATCHES 
	unsigned long new_timeout[4];
#endif

	if (!(chip = tpm_register_hardware(dev, &tpm_tis)))
		return -ENODEV;

	chip->vendor.iobase = ioremap(start, len);
	if (!chip->vendor.iobase) {
		rc = -EIO;
		goto out_err;
	}

	/* Default timeouts */
	chip->vendor.timeout_a = msecs_to_jiffies(TIS_SHORT_TIMEOUT);
	chip->vendor.timeout_b = msecs_to_jiffies(TIS_LONG_TIMEOUT);
	chip->vendor.timeout_c = msecs_to_jiffies(TIS_SHORT_TIMEOUT);
	chip->vendor.timeout_d = msecs_to_jiffies(TIS_SHORT_TIMEOUT);

	if (wait_startup(chip, 0) != 0) {
		rc = -ENODEV;
		goto out_err;
	}

	if (request_locality(chip, 0) != 0) {
		rc = -ENODEV;
		goto out_err;
	}

	vendor = ioread32(chip->vendor.iobase + TPM_DID_VID(0));
	chip->vendor.manufacturer_id = vendor;

	dev_info(dev,
		 "1.2 TPM (device-id 0x%X, rev-id %d)\n",
		 vendor >> 16, ioread8(chip->vendor.iobase + TPM_RID(0)));

	if (!itpm) {
		probe = probe_itpm(chip);
		if (probe < 0) {
			rc = -ENODEV;
			goto out_err;
		}
		itpm = !!probe;
	}

	if (itpm)
		dev_info(dev, "Intel iTPM workaround enabled\n");


	/* Figure out the capabilities */
	intfcaps =
	    ioread32(chip->vendor.iobase +
		     TPM_INTF_CAPS(chip->vendor.locality));
	dev_dbg(dev, "TPM interface capabilities (0x%x):\n",
		intfcaps);
	if (intfcaps & TPM_INTF_BURST_COUNT_STATIC)
		dev_dbg(dev, "\tBurst Count Static\n");
	if (intfcaps & TPM_INTF_CMD_READY_INT)
		dev_dbg(dev, "\tCommand Ready Int Support\n");
	if (intfcaps & TPM_INTF_INT_EDGE_FALLING)
		dev_dbg(dev, "\tInterrupt Edge Falling\n");
	if (intfcaps & TPM_INTF_INT_EDGE_RISING)
		dev_dbg(dev, "\tInterrupt Edge Rising\n");
	if (intfcaps & TPM_INTF_INT_LEVEL_LOW)
		dev_dbg(dev, "\tInterrupt Level Low\n");
	if (intfcaps & TPM_INTF_INT_LEVEL_HIGH)
		dev_dbg(dev, "\tInterrupt Level High\n");
	if (intfcaps & TPM_INTF_LOCALITY_CHANGE_INT)
		dev_dbg(dev, "\tLocality Change Int Support\n");
	if (intfcaps & TPM_INTF_STS_VALID_INT)
		dev_dbg(dev, "\tSts Valid Int Support\n");
	if (intfcaps & TPM_INTF_DATA_AVAIL_INT)
		dev_dbg(dev, "\tData Avail Int Support\n");

	/* get the timeouts before testing for irqs */
	if (tpm_get_timeouts(chip)) {
		dev_err(dev, "Could not get TPM timeouts and durations\n");
		rc = -ENODEV;
		goto out_err;
	}

#ifdef CONFIG_WG_PLATFORM_PATCHES 
	/* Erranta: Winthrop units failing on timeout
	 *   tpm_tis 00:0e: tpm_transmit: tpm_send: error -62
	 *   tpm_tis 00:0e: tpm_transmit: tpm_send: error -62
	 * 
	 * tweek the timeout so that tpm chip send op does not FAIL
	 */
	chip->vendor.timeout_adjusted =
		tpm_tis_update_timeouts_quirks(chip, new_timeout);

	/* Report adjusted timeouts and update the chip.vendor */
	if (chip->vendor.timeout_adjusted) {
		dev_info(chip->dev, "Adjusting reported timeouts: A %luus, B %luus, C %luus, D %luus\n",
			 new_timeout[0],
			 new_timeout[1],
			 new_timeout[2],
			 new_timeout[3]);

		chip->vendor.timeout_a = usecs_to_jiffies(new_timeout[0]);
		chip->vendor.timeout_b = usecs_to_jiffies(new_timeout[1]);
		chip->vendor.timeout_c = usecs_to_jiffies(new_timeout[2]);
		chip->vendor.timeout_d = usecs_to_jiffies(new_timeout[3]);
	}
#endif

	if (tpm_do_selftest(chip)) {
		dev_err(dev, "TPM self test failed\n");
		rc = -ENODEV;
		goto out_err;
	}

	/* INTERRUPT Setup */
	init_waitqueue_head(&chip->vendor.read_queue);
	init_waitqueue_head(&chip->vendor.int_queue);

	intmask =
	    ioread32(chip->vendor.iobase +
		     TPM_INT_ENABLE(chip->vendor.locality));

	intmask |= TPM_INTF_CMD_READY_INT
	    | TPM_INTF_LOCALITY_CHANGE_INT | TPM_INTF_DATA_AVAIL_INT
	    | TPM_INTF_STS_VALID_INT;

	iowrite32(intmask,
		  chip->vendor.iobase +
		  TPM_INT_ENABLE(chip->vendor.locality));
	if (interrupts)
		chip->vendor.irq = irq;
	if (interrupts && !chip->vendor.irq) {
		irq_s =
		    ioread8(chip->vendor.iobase +
			    TPM_INT_VECTOR(chip->vendor.locality));
		if (irq_s) {
			irq_e = irq_s;
		} else {
			irq_s = 3;
			irq_e = 15;
		}

		for (i = irq_s; i <= irq_e && chip->vendor.irq == 0; i++) {
			iowrite8(i, chip->vendor.iobase +
				 TPM_INT_VECTOR(chip->vendor.locality));
			if (request_irq
			    (i, tis_int_probe, IRQF_SHARED,
			     chip->vendor.miscdev.name, chip) != 0) {
				dev_info(chip->dev,
					 "Unable to request irq: %d for probe\n",
					 i);
				continue;
			}

			/* Clear all existing */
			iowrite32(ioread32
				  (chip->vendor.iobase +
				   TPM_INT_STATUS(chip->vendor.locality)),
				  chip->vendor.iobase +
				  TPM_INT_STATUS(chip->vendor.locality));

			/* Turn on */
			iowrite32(intmask | TPM_GLOBAL_INT_ENABLE,
				  chip->vendor.iobase +
				  TPM_INT_ENABLE(chip->vendor.locality));

			chip->vendor.probed_irq = 0;

			/* Generate Interrupts */
			tpm_gen_interrupt(chip);

			chip->vendor.irq = chip->vendor.probed_irq;

			/* free_irq will call into tis_int_probe;
			   clear all irqs we haven't seen while doing
			   tpm_gen_interrupt */
			iowrite32(ioread32
				  (chip->vendor.iobase +
				   TPM_INT_STATUS(chip->vendor.locality)),
				  chip->vendor.iobase +
				  TPM_INT_STATUS(chip->vendor.locality));

			/* Turn off */
			iowrite32(intmask,
				  chip->vendor.iobase +
				  TPM_INT_ENABLE(chip->vendor.locality));
			free_irq(i, chip);
		}
	}
	if (chip->vendor.irq) {
		iowrite8(chip->vendor.irq,
			 chip->vendor.iobase +
			 TPM_INT_VECTOR(chip->vendor.locality));
		if (request_irq
		    (chip->vendor.irq, tis_int_handler, IRQF_SHARED,
		     chip->vendor.miscdev.name, chip) != 0) {
			dev_info(chip->dev,
				 "Unable to request irq: %d for use\n",
				 chip->vendor.irq);
			chip->vendor.irq = 0;
		} else {
			/* Clear all existing */
			iowrite32(ioread32
				  (chip->vendor.iobase +
				   TPM_INT_STATUS(chip->vendor.locality)),
				  chip->vendor.iobase +
				  TPM_INT_STATUS(chip->vendor.locality));

			/* Turn on */
			iowrite32(intmask | TPM_GLOBAL_INT_ENABLE,
				  chip->vendor.iobase +
				  TPM_INT_ENABLE(chip->vendor.locality));
		}
	}

	INIT_LIST_HEAD(&chip->vendor.list);
	mutex_lock(&tis_lock);
	list_add(&chip->vendor.list, &tis_chips);
	mutex_unlock(&tis_lock);


	return 0;
out_err:
	if (chip->vendor.iobase)
		iounmap(chip->vendor.iobase);
	tpm_remove_hardware(chip->dev);
	return rc;
}

#if defined(CONFIG_PNP) || defined(CONFIG_PM_SLEEP)
static void tpm_tis_reenable_interrupts(struct tpm_chip *chip)
{
	u32 intmask;

	/* reenable interrupts that device may have lost or
	   BIOS/firmware may have disabled */
	iowrite8(chip->vendor.irq, chip->vendor.iobase +
		 TPM_INT_VECTOR(chip->vendor.locality));

	intmask =
	    ioread32(chip->vendor.iobase +
		     TPM_INT_ENABLE(chip->vendor.locality));

	intmask |= TPM_INTF_CMD_READY_INT
	    | TPM_INTF_LOCALITY_CHANGE_INT | TPM_INTF_DATA_AVAIL_INT
	    | TPM_INTF_STS_VALID_INT | TPM_GLOBAL_INT_ENABLE;

	iowrite32(intmask,
		  chip->vendor.iobase + TPM_INT_ENABLE(chip->vendor.locality));
}
#endif

#ifdef CONFIG_PM_SLEEP
static int tpm_tis_resume(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);
	int ret;

	if (chip->vendor.irq)
		tpm_tis_reenable_interrupts(chip);

	ret = tpm_pm_resume(dev);
	if (!ret)
		tpm_do_selftest(chip);

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(tpm_tis_pm, tpm_pm_suspend, tpm_tis_resume);

#ifdef CONFIG_PNP
static int tpm_tis_pnp_init(struct pnp_dev *pnp_dev,
				      const struct pnp_device_id *pnp_id)
{
	resource_size_t start, len;
	unsigned int irq = 0;

	start = pnp_mem_start(pnp_dev, 0);
	len = pnp_mem_len(pnp_dev, 0);

	if (pnp_irq_valid(pnp_dev, 0))
		irq = pnp_irq(pnp_dev, 0);
	else
		interrupts = false;

	if (is_itpm(pnp_dev))
		itpm = true;

	return tpm_tis_init(&pnp_dev->dev, start, len, irq);
}

static struct pnp_device_id tpm_pnp_tbl[] = {
	{"PNP0C31", 0},		/* TPM */
	{"ATM1200", 0},		/* Atmel */
	{"IFX0102", 0},		/* Infineon */
	{"BCM0101", 0},		/* Broadcom */
	{"BCM0102", 0},		/* Broadcom */
	{"NSC1200", 0},		/* National */
	{"ICO0102", 0},		/* Intel */
	/* Add new here */
	{"", 0},		/* User Specified */
	{"", 0}			/* Terminator */
};
MODULE_DEVICE_TABLE(pnp, tpm_pnp_tbl);

static void tpm_tis_pnp_remove(struct pnp_dev *dev)
{
	struct tpm_chip *chip = pnp_get_drvdata(dev);

	tpm_dev_vendor_release(chip);

	kfree(chip);
}


static struct pnp_driver tis_pnp_driver = {
	.name = "tpm_tis",
	.id_table = tpm_pnp_tbl,
	.probe = tpm_tis_pnp_init,
	.remove = tpm_tis_pnp_remove,
#ifdef CONFIG_PM_SLEEP
	.driver	= {
		.pm = &tpm_tis_pm,
	},
#endif
};

#define TIS_HID_USR_IDX sizeof(tpm_pnp_tbl)/sizeof(struct pnp_device_id) -2
module_param_string(hid, tpm_pnp_tbl[TIS_HID_USR_IDX].id,
		    sizeof(tpm_pnp_tbl[TIS_HID_USR_IDX].id), 0444);
MODULE_PARM_DESC(hid, "Set additional specific HID for this driver to probe");
#endif

static struct platform_driver tis_drv = {
	.driver = {
		.name = "tpm_tis",
		.owner		= THIS_MODULE,
		.pm		= &tpm_tis_pm,
	},
};

static struct platform_device *pdev;

static bool force;
module_param(force, bool, 0444);
MODULE_PARM_DESC(force, "Force device probe rather than using ACPI entry");
static int __init init_tis(void)
{
	int rc;
#ifdef CONFIG_PNP
	if (!force)
		return pnp_register_driver(&tis_pnp_driver);
#endif

	rc = platform_driver_register(&tis_drv);
	if (rc < 0)
		return rc;
	pdev = platform_device_register_simple("tpm_tis", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		rc = PTR_ERR(pdev);
		goto err_dev;
	}
	rc = tpm_tis_init(&pdev->dev, TIS_MEM_BASE, TIS_MEM_LEN, 0);
	if (rc)
		goto err_init;
	return 0;
err_init:
	platform_device_unregister(pdev);
err_dev:
	platform_driver_unregister(&tis_drv);
	return rc;
}

static void __exit cleanup_tis(void)
{
	struct tpm_vendor_specific *i, *j;
	struct tpm_chip *chip;
	mutex_lock(&tis_lock);
	list_for_each_entry_safe(i, j, &tis_chips, list) {
		chip = to_tpm_chip(i);
		tpm_remove_hardware(chip->dev);
		iowrite32(~TPM_GLOBAL_INT_ENABLE &
			  ioread32(chip->vendor.iobase +
				   TPM_INT_ENABLE(chip->vendor.
						  locality)),
			  chip->vendor.iobase +
			  TPM_INT_ENABLE(chip->vendor.locality));
		release_locality(chip, chip->vendor.locality, 1);
		if (chip->vendor.irq)
			free_irq(chip->vendor.irq, chip);
		iounmap(i->iobase);
		list_del(&i->list);
	}
	mutex_unlock(&tis_lock);
#ifdef CONFIG_PNP
	if (!force) {
		pnp_unregister_driver(&tis_pnp_driver);
		return;
	}
#endif
	platform_device_unregister(pdev);
	platform_driver_unregister(&tis_drv);
}

module_init(init_tis);
module_exit(cleanup_tis);
MODULE_AUTHOR("Leendert van Doorn (leendert@watson.ibm.com)");
MODULE_DESCRIPTION("TPM Driver");
MODULE_VERSION("2.0");
MODULE_LICENSE("GPL");
