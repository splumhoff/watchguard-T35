/*
 * Copyright (c) 2011 The Chromium OS Authors.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * The code in this file is based on the article "Writing a TPM Device Driver"
 * published on http://ptgmedia.pearsoncmg.com.
 *
 * One principal difference is that in the simplest config the other than 0
 * TPM localities do not get mapped by some devices (for instance, by Infineon
 * slb9635), so this driver provides access to locality 0 only.
 */

#include <common.h>
#include <asm/io.h>
#include <tpm.h>

#define PREFIX "lpc_tpm: "

struct tpm_locality {
	u32 access;
	u8 padding0[4];
	u32 int_enable;
	u8 vector;
	u8 padding1[3];
	u32 int_status;
	u32 int_capability;
	u32 tpm_status;
	u8 padding2[8];
	u8 data;
	u8 padding3[3803];
	u32 did_vid;
	u8 rid;
	u8 padding4[251];
};

/*
 * This pointer refers to the TPM chip, 5 of its localities are mapped as an
 * array.
 */
#define TPM_TOTAL_LOCALITIES	5
/*
static struct tpm_locality *lpc_tpm_dev =
	(struct tpm_locality *)CONFIG_TPM_TIS_BASE_ADDRESS;
*/
static struct fsl_i2c *sc_i2c_dev = (struct fsl_i2c *) (CONFIG_SYS_IMMR + CONFIG_SYS_I2C1_OFFSET);

/* Some registers' bit field definitions */
#define TIS_STS_VALID                  (1 << 7) /* 0x80 */
#define TIS_STS_COMMAND_READY          (1 << 6) /* 0x40 */
#define TIS_STS_TPM_GO                 (1 << 5) /* 0x20 */
#define TIS_STS_DATA_AVAILABLE         (1 << 4) /* 0x10 */
#define TIS_STS_EXPECT                 (1 << 3) /* 0x08 */
#define TIS_STS_RESPONSE_RETRY         (1 << 1) /* 0x02 */

#define TIS_ACCESS_TPM_REG_VALID_STS   (1 << 7) /* 0x80 */
#define TIS_ACCESS_ACTIVE_LOCALITY     (1 << 5) /* 0x20 */
#define TIS_ACCESS_BEEN_SEIZED         (1 << 4) /* 0x10 */
#define TIS_ACCESS_SEIZE               (1 << 3) /* 0x08 */
#define TIS_ACCESS_PENDING_REQUEST     (1 << 2) /* 0x04 */
#define TIS_ACCESS_REQUEST_USE         (1 << 1) /* 0x02 */
#define TIS_ACCESS_TPM_ESTABLISHMENT   (1 << 0) /* 0x01 */

#define TIS_STS_BURST_COUNT_MASK       (0xffff)
#define TIS_STS_BURST_COUNT_SHIFT      (8)

/*
 * Error value returned if a tpm register does not enter the expected state
 * after continuous polling. No actual TPM register reading ever returns -1,
 * so this value is a safe error indication to be mixed with possible status
 * register values.
 */
#define TPM_TIMEOUT_ERR			(-1)

/* Error value returned on various TPM driver errors. */
#define TPM_DRIVER_ERR		(1)

 /* 1 second is plenty for anything TPM does. */
#define MAX_DELAY_US	(1000 * 1000)

/* Retrieve burst count value out of the status register contents. */
static u16 burst_count(u32 status)
{
	return (status >> TIS_STS_BURST_COUNT_SHIFT) & TIS_STS_BURST_COUNT_MASK;
}

/*
 * Structures defined below allow creating descriptions of TPM vendor/device
 * ID information for run time discovery. The only device the system knows
 * about at this time is Infineon slb9635.
 */
struct device_name {
	u16 dev_id;
	const char * const dev_name;
};

struct vendor_name {
	u16 vendor_id;
	const char *vendor_name;
	const struct device_name *dev_names;
};

static const struct device_name infineon_devices[] = {
	{0xb, "SLB9635 TT 1.2"},
	{0}
};

static const struct vendor_name vendor_names[] = {
	{0x15d1, "Infineon", infineon_devices},
};

/*
 * Cached vendor/device ID pair to indicate that the device has been already
 * discovered.
 */
static u32 vendor_dev_id;

/* TPM access wrappers to support tracing */
static u8 tpm_read_byte(const u8 *ptr)
{
	u8  ret = readb(ptr);
	debug(PREFIX "Read reg 0x%4.4x returns 0x%2.2x\n",
	      (u32)ptr - (u32)lpc_tpm_dev, ret);
	return ret;
}

static u32 tpm_read_word(const u32 *ptr)
{
	u32  ret = readl(ptr);
	debug(PREFIX "Read reg 0x%4.4x returns 0x%8.8x\n",
	      (u32)ptr - (u32)lpc_tpm_dev, ret);
	return ret;
}

static void tpm_write_byte(u8 value, u8 *ptr)
{
	debug(PREFIX "Write reg 0x%4.4x with 0x%2.2x\n",
	      (u32)ptr - (u32)lpc_tpm_dev, value);
	writeb(value, ptr);
}

static void tpm_write_word(u32 value, u32 *ptr)
{
	debug(PREFIX "Write reg 0x%4.4x with 0x%8.8x\n",
	      (u32)ptr - (u32)lpc_tpm_dev, value);
	writel(value, ptr);
}

/*
 * tis_wait_reg()
 *
 * Wait for at least a second for a register to change its state to match the
 * expected state. Normally the transition happens within microseconds.
 *
 * @reg - pointer to the TPM register
 * @mask - bitmask for the bitfield(s) to watch
 * @expected - value the field(s) are supposed to be set to
 *
 * Returns the register contents in case the expected value was found in the
 * appropriate register bits, or TPM_TIMEOUT_ERR on timeout.
 */
static u32 tis_wait_reg(u32 *reg, u8 mask, u8 expected)
{
	u32 time_us = MAX_DELAY_US;

	while (time_us > 0) {
		u32 value = tpm_read_word(reg);
		if ((value & mask) == expected)
			return value;
		udelay(1); /* 1 us */
		time_us--;
	}
	return TPM_TIMEOUT_ERR;
}

/*
 * Probe the TPM device and try determining its manufacturer/device name.
 *
 * Returns 0 on success (the device is found or was found during an earlier
 * invocation) or TPM_DRIVER_ERR if the device is not found.
 */
int tis_init(void)
{
	u32 didvid = tpm_read_word(&lpc_tpm_dev[0].did_vid);
	int i;
	const char *device_name = "unknown";
	const char *vendor_name = device_name;
	u16 vid, did;

	if (vendor_dev_id)
		return 0;  /* Already probed. */

	if (!didvid || (didvid == 0xffffffff)) {
		printf("%s: No TPM device found\n", __func__);
		return TPM_DRIVER_ERR;
	}

	vendor_dev_id = didvid;

	vid = didvid & 0xffff;
	did = (didvid >> 16) & 0xffff;
	for (i = 0; i < ARRAY_SIZE(vendor_names); i++) {
		int j = 0;
		u16 known_did;

		if (vid == vendor_names[i].vendor_id)
			vendor_name = vendor_names[i].vendor_name;

		while ((known_did = vendor_names[i].dev_names[j].dev_id) != 0) {
			if (known_did == did) {
				device_name =
					vendor_names[i].dev_names[j].dev_name;
				break;
			}
			j++;
		}
		break;
	}

	printf("Found TPM %s by %s\n", device_name, vendor_name);
	return 0;
}

/*
 * tis_senddata()
 *
 * send the passed in data to the TPM device.
 *
 * @data - address of the data to send, byte by byte
 * @len - length of the data to send
 *
 * Returns 0 on success, TPM_DRIVER_ERR on error (in case the device does
 * not accept the entire command).
 */
static u32 tis_senddata(const u8 * const data, u32 len)
{
	u32 offset = 0;
	u16 burst = 0;
	u32 max_cycles = 0;
	u8 locality = 0;
	u32 value;

	value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
			     TIS_STS_COMMAND_READY, TIS_STS_COMMAND_READY);
	if (value == TPM_TIMEOUT_ERR) {
		printf("%s:%d - failed to get 'command_ready' status\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}
	burst = burst_count(value);

	while (1) {
		unsigned count;

		/* Wait till the device is ready to accept more data. */
		while (!burst) {
			if (max_cycles++ == MAX_DELAY_US) {
				printf("%s:%d failed to feed %d bytes of %d\n",
				       __FILE__, __LINE__, len - offset, len);
				return TPM_DRIVER_ERR;
			}
			udelay(1);
			burst = burst_count(tpm_read_word(&lpc_tpm_dev
						     [locality].tpm_status));
		}

		max_cycles = 0;

		/*
		 * Calculate number of bytes the TPM is ready to accept in one
		 * shot.
		 *
		 * We want to send the last byte outside of the loop (hence
		 * the -1 below) to make sure that the 'expected' status bit
		 * changes to zero exactly after the last byte is fed into the
		 * FIFO.
		 */
		count = min(burst, len - offset - 1);
		while (count--)
			tpm_write_byte(data[offset++],
				  &lpc_tpm_dev[locality].data);

		value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
				     TIS_STS_VALID, TIS_STS_VALID);

		if ((value == TPM_TIMEOUT_ERR) || !(value & TIS_STS_EXPECT)) {
			printf("%s:%d TPM command feed overflow\n",
			       __FILE__, __LINE__);
			return TPM_DRIVER_ERR;
		}

		burst = burst_count(value);
		if ((offset == (len - 1)) && burst) {
			/*
			 * We need to be able to send the last byte to the
			 * device, so burst size must be nonzero before we
			 * break out.
			 */
			break;
		}
	}

	/* Send the last byte. */
	tpm_write_byte(data[offset++], &lpc_tpm_dev[locality].data);
	/*
	 * Verify that TPM does not expect any more data as part of this
	 * command.
	 */
	value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
			     TIS_STS_VALID, TIS_STS_VALID);
	if ((value == TPM_TIMEOUT_ERR) || (value & TIS_STS_EXPECT)) {
		printf("%s:%d unexpected TPM status 0x%x\n",
		       __FILE__, __LINE__, value);
		return TPM_DRIVER_ERR;
	}

	/* OK, sitting pretty, let's start the command execution. */
	tpm_write_word(TIS_STS_TPM_GO, &lpc_tpm_dev[locality].tpm_status);
	return 0;
}

/*
 * tis_readresponse()
 *
 * read the TPM device response after a command was issued.
 *
 * @buffer - address where to read the response, byte by byte.
 * @len - pointer to the size of buffer
 *
 * On success stores the number of received bytes to len and returns 0. On
 * errors (misformatted TPM data or synchronization problems) returns
 * TPM_DRIVER_ERR.
 */
static u32 tis_readresponse(u8 *buffer, u32 *len)
{
	u16 burst;
	u32 value;
	u32 offset = 0;
	u8 locality = 0;
	const u32 has_data = TIS_STS_DATA_AVAILABLE | TIS_STS_VALID;
	u32 expected_count = *len;
	int max_cycles = 0;

	/* Wait for the TPM to process the command. */
	value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
			      has_data, has_data);
	if (value == TPM_TIMEOUT_ERR) {
		printf("%s:%d failed processing command\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}

	do {
		while ((burst = burst_count(value)) == 0) {
			if (max_cycles++ == MAX_DELAY_US) {
				printf("%s:%d TPM stuck on read\n",
				       __FILE__, __LINE__);
				return TPM_DRIVER_ERR;
			}
			udelay(1);
			value = tpm_read_word(&lpc_tpm_dev
					      [locality].tpm_status);
		}

		max_cycles = 0;

		while (burst-- && (offset < expected_count)) {
			buffer[offset++] = tpm_read_byte(&lpc_tpm_dev
							 [locality].data);

			if (offset == 6) {
				/*
				 * We got the first six bytes of the reply,
				 * let's figure out how many bytes to expect
				 * total - it is stored as a 4 byte number in
				 * network order, starting with offset 2 into
				 * the body of the reply.
				 */
				u32 real_length;
				memcpy(&real_length,
				       buffer + 2,
				       sizeof(real_length));
				expected_count = be32_to_cpu(real_length);

				if ((expected_count < offset) ||
				    (expected_count > *len)) {
					printf("%s:%d bad response size %d\n",
					       __FILE__, __LINE__,
					       expected_count);
					return TPM_DRIVER_ERR;
				}
			}
		}

		/* Wait for the next portion. */
		value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
				     TIS_STS_VALID, TIS_STS_VALID);
		if (value == TPM_TIMEOUT_ERR) {
			printf("%s:%d failed to read response\n",
			       __FILE__, __LINE__);
			return TPM_DRIVER_ERR;
		}

		if (offset == expected_count)
			break;	/* We got all we needed. */

	} while ((value & has_data) == has_data);

	/*
	 * Make sure we indeed read all there was. The TIS_STS_VALID bit is
	 * known to be set.
	 */
	if (value & TIS_STS_DATA_AVAILABLE) {
		printf("%s:%d wrong receive status %x\n",
		       __FILE__, __LINE__, value);
		return TPM_DRIVER_ERR;
	}

	/* Tell the TPM that we are done. */
	tpm_write_word(TIS_STS_COMMAND_READY, &lpc_tpm_dev
		  [locality].tpm_status);
	*len = offset;
	return 0;
}


/***********************************************************************/

int sc_i2c_set_bus_speed(unsigned int speed)
{
    unsigned int i2c_clk = gd->i2c2_clk;

    writeb(0, &sc_i2c_dev->cr);		/* stop controller */
    sc_i2c_bus_speed =
        sc_set_i2c_bus_speed(sc_i2c_dev, i2c_clk, speed);
    writeb(I2C_CR_MEN, &sc_i2c_dev->cr);	/* start controller */

    return 0;
}


unsigned int sc_i2c_get_bus_speed(void)
{
    return sc_i2c_bus_speed;
}
/**
 * Set the I2C bus speed for a given I2C device
 *
 * @param dev: the I2C device
 * @i2c_clk: I2C bus clock frequency
 * @speed: the desired speed of the bus
 *
 * The I2C device must be stopped before calling this function.
 *
 * The return value is the actual bus speed that is set.
 */
static unsigned int sc_set_i2c_bus_speed(struct fsl_i2c *dev,
        unsigned int i2c_clk, unsigned int speed)
{
    unsigned short divider = min(i2c_clk / speed, (unsigned short) -1);

    /*
     * We want to choose an FDR/DFSR that generates an I2C bus speed that
     * is equal to or lower than the requested speed.  That means that we
     * want the first divider that is equal to or greater than the
     * calculated divider.
     */
#if defined (__PPC__) || defined (__powerpc__)
    u8 dfsr, fdr = 0x31; /* Default if no FDR found */
    /* a, b and dfsr matches identifiers A,B and C respectively in AN2919 */
    unsigned short a, b, ga, gb;
    unsigned long c_div, est_div;

    /* Condition 1: dfsr <= 50/T */
    dfsr = (5 * (i2c_clk / 1000)) / 100000;

    debug("Requested speed:%d, i2c_clk:%d\n", speed, i2c_clk);
    if (!dfsr)
        dfsr = 1;

    est_div = ~0;
    for (ga = 0x4, a = 10; a <= 30; ga++, a += 2) {
        for (gb = 0; gb < 8; gb++) {
            b = 16 << gb;
            c_div = b * (a + ((3*dfsr)/b)*2);
            if ((c_div > divider) && (c_div < est_div)) {
                unsigned short bin_gb, bin_ga;

                est_div = c_div;
                bin_gb = gb << 2;
                bin_ga = (ga & 0x3) | ((ga & 0x4) << 3);
                fdr = bin_gb | bin_ga;
                speed = i2c_clk / est_div;
                debug("FDR:0x%.2x, div:%ld, ga:0x%x, gb:0x%x, "
                      "a:%d, b:%d, speed:%d\n",
                      fdr, est_div, ga, gb, a, b, speed);
                /* Condition 2 not accounted for */
                debug("Tr <= %d ns\n",
                      (b - 3 * dfsr) * 1000000 /
                      (i2c_clk / 1000));
            }
        }
        if (a == 20)
            a += 2;
        if (a == 24)
            a += 4;
    }
    debug("divider:%d, est_div:%ld, DFSR:%d\n", divider, est_div, dfsr);
    debug("FDR:0x%.2x, speed:%d\n", fdr, speed);

    writeb(dfsr, &dev->dfsrr);	/* set default filter */
    writeb(fdr, &dev->fdr);		/* set bus speed */
#endif
    return speed;
}

void
sc_i2c_init(int speed, int slaveadd)
{
    /*
     * init i2c bus first:
     * because XTM3x has no i2c slave, so put i2c bus init in RTC init
     */	
    
	struct fsl_i2c *dev;
    unsigned int temp;

    dev = (struct fsl_i2c *) (CONFIG_SYS_IMMR + CONFIG_SYS_I2C_OFFSET);

    writeb(0, &dev->cr);			/* stop I2C controller */
    udelay(5);				/* let it shutdown in peace */
    temp = sc_set_i2c_bus_speed(dev, gd->i2c1_clk, speed);
    if (gd->flags & GD_FLG_RELOC)
        sc_i2c_bus_speed = temp;
    writeb(slaveadd << 1, &dev->adr);	/* write slave address */
    writeb(0x0, &dev->sr);			/* clear status register */
    writeb(I2C_CR_MEN , &dev->cr);		/* start I2C controller */   
}

static __inline__ int
sc_i2c_wait(int write)
{
    u32 csr;
    unsigned long long timeval = get_ticks();
    const unsigned long long timeout = usec2ticks(SC_CONFIG_I2C_TIMEOUT);

    do {
        csr = readb(&sc_i2c_dev->sr);
        if (!(csr & I2C_SR_MIF))
            continue;
        /* Read again to allow register to stabilise */
        csr = readb(&sc_i2c_dev->sr);

        writeb(0x0, &sc_i2c_dev->sr);

        if (csr & I2C_SR_MAL) {
            debug("i2c_wait: MAL\n");
            return -1;
        }

        if (!(csr & I2C_SR_MCF))	{
            debug("i2c_wait: unfinished\n");
            return -1;
        }

        if (write == SC_I2C_WRITE_BIT && (csr & I2C_SR_RXAK)) {
            debug("i2c_wait: No RXACK\n");
            return -1;
        }

        return 0;
    } while ((get_ticks() - timeval) < timeout);

    debug("i2c_wait: timed out\n");
    return -1;
}

/***********************************************************************/
static inline void writeccr(struct fsl_i2c * dev, u32 x)
{
    writeb(x, &dev->cr);	
}

static void tpm_i2c_start(struct fsl_i2c * dev)
{
	/* Clear arbitration */
	writeb(0, &dev->sr);
	/* Start with MEN */
	writeccr(dev, I2C_CR_MEN);
}

static void tpm_i2c_stop(struct fsl_i2c * dev)
{
	writeccr(dev, I2C_CR_MEN);
}
/* Sometimes 9th clock pulse isn't generated, and slave doesn't release
 * the bus, because it wants to send ACK.
 * Following sequence of enabling/disabling and sending start/stop generates
 * the pulse, so it's all OK.
 */
static void tpm_i2c_fixup(struct fsl_i2c *i2c)
{
	writeccr(i2c, 0);
	udelay(30);
	writeccr(i2c, I2C_CR_MEN);
	udelay(30);
	writeccr(i2c, I2C_CR_MSTA | I2C_CR_MTX);
	udelay(30);
	writeccr(i2c, I2C_CR_MSTA | I2C_CR_MTX | I2C_CR_MEN);
	udelay(30);
	writeccr(i2c, I2C_CR_MEN);
	udelay(30);
}

static int tpm_write(struct fsl_i2c *i2c, int target,
		     const u8 *data, int length, int restart)
{
	int i, result;
	u32 flags = restart ? I2C_CR_RSTA : 0;

	/* Start as master */
	writeccr(i2c, I2C_CR_MEN | I2C_CR_MSTA | I2C_CR_MTX | flags);
	/* Write target byte */
	writeb((target << 1), &i2c->dr);

	result = sc_i2c_wait(SC_I2C_WRITE_BIT);
	if (result < 0)
		return result;

	for (i = 0; i < length; i++) {
		/* Write data byte */
		writeb(data[i], &i2c->dr);

		result = sc_i2c_wait(SC_I2C_WRITE_BIT);
		if (result < 0)
			return result;
	}

	return 0;
}

static int tpm_read(struct fsl_i2c *i2c, int target,
		    u8 *data, int length, int restart)
{
	int i, result;
	u32 flags = restart ? I2C_CR_RSTA : 0;

	/* Switch to read - restart */
	writeccr(i2c, I2C_CR_MEN | I2C_CR_MSTA | I2C_CR_MTX | flags);
	/* Write target address byte - this time with the read flag set */
	writeb((target << 1) | 1, &i2c->dr);

	result = sc_i2c_wait(SC_I2C_WRITE_BIT);
	if (result < 0)
		return result;

	if (length) {
		if (length == 1)
			writeccr(i2c, I2C_CR_MEN | I2C_CR_MSTA | I2C_CR_TXAK);
		else
			writeccr(i2c, I2C_CR_MEN | I2C_CR_MSTA);
		/* Dummy read */
		readb(&i2c->dr);
	}

	for (i = 0; i < length; i++) {
		result = sc_i2c_wait(SC_I2C_READ_BIT);
		if (result < 0)
			return result;

		/* Generate txack on next to last byte */
		if (i == length - 2)
			writeccr(i2c, I2C_CR_MEN | I2C_CR_MSTA | I2C_CR_TXAK);
		/* Do not generate stop on last byte */
		if (i == length - 1)
			writeccr(i2c, I2C_CR_MEN | I2C_CR_MSTA | I2C_CR_MTX);
		data[i] = readb(&i2c->dr);
	}

	return length;
}

static int tpm_xfer(struct fsl_i2c *i2c, struct i2c_msg *msgs, int num)
{
	struct i2c_msg *pmsg;
	int i;
	int ret = 0;
	
	unsigned long long timeval = get_ticks();
	const unsigned long long timeout = usec2ticks(SC_CONFIG_I2C_MBB_TIMEOUT);
	
	tpm_i2c_start(i2c);

	/* Allow bus up to 1s to become not busy */
	while (readb(&i2c->sr) & I2C_SR_MBB) 
	{
		if ((get_ticks() - timeval) > timeout)
		{
			debug("%s %d : i2c bus always busy!\n", __func__, __LINE__);
			tpm_i2c_fixup(i2c);
			return -1;
		}
	}


	for (i = 0; ret >= 0 && i < num; i++) {
		pmsg = &msgs[i];
		debug("Doing %s %d bytes to 0x%02x - %d of %d messages\n",
			pmsg->flags & I2C_M_RD ? "read" : "write",
			pmsg->len, pmsg->addr, i + 1, num);
		if (pmsg->flags & I2C_M_RD)
			ret =
			    tpm_read(i2c, pmsg->addr, pmsg->buf, pmsg->len, i);
		else
			ret =
			    tpm_write(i2c, pmsg->addr, pmsg->buf, pmsg->len, i);
	}
	tpm_i2c_stop(i2c);
	return (ret < 0) ? ret : num;
}

int tpm_get_reg(struct fsl_i2c *i2c, int reg, char *buf, int len)
{
	struct i2c_msg msg = {
		(RTC_DEV_S35390 << RTC_DEV_ADDR_SHIFT) | (reg << RTC_CMD_ADDR_SHIFT), 
		SC_I2C_READ_BIT, len, (unsigned char *)buf
	};

	if (tpm_xfer(i2c, &msg, 1) != 1)
		return -1;

	return 0;
}

int tpm_set_reg(struct fsl_i2c *i2c, int reg, char *buf, int len)
{

	struct i2c_msg msg = {
		(RTC_DEV_S35390 << RTC_DEV_ADDR_SHIFT) | (reg << RTC_CMD_ADDR_SHIFT), 
		SC_I2C_WRITE_BIT, len, (unsigned char *)buf
	};

	if (tpm_xfer(i2c, &msg, 1) != 1)
		return -1;

	return 0;
}

int tpm_reset(struct fsl_i2c *i2c)
{
	char buf[1];

	if (tpm_get_reg(i2c, CMD_STATUS_1ST, buf, sizeof(buf)) < 0)
		return -1;

	if (!(buf[0] & (S35390A_FLAG_POC | S35390A_FLAG_BLD)))
		return 0;

	buf[0] |= (S35390A_FLAG_RESET | S35390A_FLAG_24H);
	buf[0] &= 0xf0;
	return tpm_set_reg(i2c, CMD_STATUS_1ST, buf, sizeof(buf));
}

int sc_tpm_init(void)
{

}

int tis_open(void)
{
	u8 locality = 0; /* we use locality zero for everything. */
	sc_i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	sc_tpm_init();

	if (tis_close())
		return TPM_DRIVER_ERR;

	/* now request access to locality. */
	tpm_write_word(TIS_ACCESS_REQUEST_USE, &lpc_tpm_dev[locality].access);

	/* did we get a lock? */
	if (tis_wait_reg(&lpc_tpm_dev[locality].access,
			 TIS_ACCESS_ACTIVE_LOCALITY,
			 TIS_ACCESS_ACTIVE_LOCALITY) == TPM_TIMEOUT_ERR) {
		printf("%s:%d - failed to lock locality %d\n",
		       __FILE__, __LINE__, locality);
		return TPM_DRIVER_ERR;
	}

	tpm_write_word(TIS_STS_COMMAND_READY,
		       &lpc_tpm_dev[locality].tpm_status);
	return 0;
}

int tis_close(void)
{
	u8 locality = 0;

	if (tpm_read_word(&lpc_tpm_dev[locality].access) &
	    TIS_ACCESS_ACTIVE_LOCALITY) {
		tpm_write_word(TIS_ACCESS_ACTIVE_LOCALITY,
			       &lpc_tpm_dev[locality].access);

		if (tis_wait_reg(&lpc_tpm_dev[locality].access,
				 TIS_ACCESS_ACTIVE_LOCALITY, 0) ==
		    TPM_TIMEOUT_ERR) {
			printf("%s:%d - failed to release locality %d\n",
			       __FILE__, __LINE__, locality);
			return TPM_DRIVER_ERR;
		}
	}
	return 0;
}

int tis_sendrecv(const u8 *sendbuf, size_t send_size,
		 u8 *recvbuf, size_t *recv_len)
{
	if (tis_senddata(sendbuf, send_size)) {
		printf("%s:%d failed sending data to TPM\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}

	return tis_readresponse(recvbuf, recv_len);
}
