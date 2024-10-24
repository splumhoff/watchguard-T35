#include <common.h>
#include <command.h>
#include <i2c.h>		/* Functional interface */

#include <asm/io.h>
#include <asm/fsl_i2c.h>	/* HW definitions */
#include <rtc.h>

#include "s35390.h"

#undef DEBUG_RTC                                                                                                                                                                                                 
                                                                                                                                                                                                                 
#ifdef DEBUG_RTC                                                                                                                                                                                                 
#define DEBUGR(fmt,args...) printf(fmt ,##args)                                                                                                                                                                  
#else                                                                                                                                                                                                            
#define DEBUGR(fmt,args...)                                                                                                                                                                                      
#endif

/**************************************************************************
 * 	The I2C interface designed for S-35390, using P1020 I2C BUS controller
 **************************************************************************/
/* The maximum number of microseconds we will wait until another master has
 * released the bus.  If not defined in the board header file, then use a
 * generic value.
 */
#ifndef SC_CONFIG_I2C_MBB_TIMEOUT
#define SC_CONFIG_I2C_MBB_TIMEOUT	100000
#endif

/* The maximum number of microseconds we will wait for a read or write
 * operation to complete.  If not defined in the board header file, then use a
 * generic value.
 */
#ifndef SC_CONFIG_I2C_TIMEOUT
#define SC_CONFIG_I2C_TIMEOUT	10000
#endif

#define SC_I2C_READ_BIT  1
#define SC_I2C_WRITE_BIT 0

DECLARE_GLOBAL_DATA_PTR;

/* Initialize the bus pointer to whatever one the SPD EEPROM is on.
 * Default is bus 0.  This is necessary because the DDR initialization
 * runs from ROM, and we can't switch buses because we can't modify
 * the global variables.
 */

static unsigned int sc_i2c_bus_speed = CONFIG_SYS_I2C_SPEED;

static struct fsl_i2c *sc_i2c_dev = (struct fsl_i2c *) (CONFIG_SYS_IMMR + CONFIG_SYS_I2C_OFFSET);


/* Function Protypes */
static unsigned int sc_set_i2c_bus_speed(struct fsl_i2c *dev,
        unsigned int i2c_clk, unsigned int speed);
int sc_i2c_set_bus_speed(unsigned int speed);
unsigned int sc_i2c_get_bus_speed(void);
void sc_i2c_init(int speed, int slaveadd);
static __inline__ int sc_i2c_wait(int write);
// static int sc_i2c_wait4bus(void);


/***********************************************************************/
static inline void writeccr(struct fsl_i2c * dev, u32 x)
{
    writeb(x, &dev->cr);	
}

static void mpc_i2c_start(struct fsl_i2c * dev)
{
	/* Clear arbitration */
	writeb(0, &dev->sr);
	/* Start with MEN */
	writeccr(dev, I2C_CR_MEN);
}

static void mpc_i2c_stop(struct fsl_i2c * dev)
{
	writeccr(dev, I2C_CR_MEN);
}
/* Sometimes 9th clock pulse isn't generated, and slave doesn't release
 * the bus, because it wants to send ACK.
 * Following sequence of enabling/disabling and sending start/stop generates
 * the pulse, so it's all OK.
 */
static void mpc_i2c_fixup(struct fsl_i2c *i2c)
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

static int mpc_write(struct fsl_i2c *i2c, int target,
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

static int mpc_read(struct fsl_i2c *i2c, int target,
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

static int mpc_xfer(struct fsl_i2c *i2c, struct i2c_msg *msgs, int num)
{
	struct i2c_msg *pmsg;
	int i;
	int ret = 0;
	
	unsigned long long timeval = get_ticks();
	const unsigned long long timeout = usec2ticks(SC_CONFIG_I2C_MBB_TIMEOUT);
	
	mpc_i2c_start(i2c);

	/* Allow bus up to 1s to become not busy */
	while (readb(&i2c->sr) & I2C_SR_MBB) 
	{
		if ((get_ticks() - timeval) > timeout)
		{
			debug("%s %d : i2c bus always busy!\n", __func__, __LINE__);
			mpc_i2c_fixup(i2c);
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
			    mpc_read(i2c, pmsg->addr, pmsg->buf, pmsg->len, i);
		else
			ret =
			    mpc_write(i2c, pmsg->addr, pmsg->buf, pmsg->len, i);
	}
	mpc_i2c_stop(i2c);
	return (ret < 0) ? ret : num;
}

int s35390a_get_reg(struct fsl_i2c *i2c, int reg, char *buf, int len)
{
	struct i2c_msg msg = {
		(RTC_DEV_S35390 << RTC_DEV_ADDR_SHIFT) | (reg << RTC_CMD_ADDR_SHIFT), 
		SC_I2C_READ_BIT, len, (unsigned char *)buf
	};

	if (mpc_xfer(i2c, &msg, 1) != 1)
		return -1;

	return 0;
}

int s35390a_set_reg(struct fsl_i2c *i2c, int reg, char *buf, int len)
{
	struct i2c_msg msg = {
		(RTC_DEV_S35390 << RTC_DEV_ADDR_SHIFT) | (reg << RTC_CMD_ADDR_SHIFT), 
		SC_I2C_WRITE_BIT, len, (unsigned char *)buf
	};

	if (mpc_xfer(i2c, &msg, 1) != 1)
		return -1;

	return 0;
}

int s35390a_reset(struct fsl_i2c *i2c)
{
	char buf[1];

	if (s35390a_get_reg(i2c, CMD_STATUS_1ST, buf, sizeof(buf)) < 0)
		return -1;

	if (!(buf[0] & (S35390A_FLAG_POC | S35390A_FLAG_BLD)))
		return 0;

	buf[0] |= (S35390A_FLAG_RESET | S35390A_FLAG_24H);
	buf[0] &= 0xf0;
	return s35390a_set_reg(i2c, CMD_STATUS_1ST, buf, sizeof(buf));
}

static int s35390a_disable_test_mode(struct fsl_i2c *i2c)
{
	char buf[1];

	if (s35390a_get_reg(i2c, CMD_STATUS_2ND, buf, sizeof(buf)) < 0)
		return -1;

	if (!(buf[0] & S35390A_FLAG_TEST))
		return 0;

	buf[0] &= ~S35390A_FLAG_TEST;
	return s35390a_set_reg(i2c, CMD_STATUS_2ND, buf, sizeof(buf));
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

/********************************************************
 * RTC Functions
 ********************************************************/
static char DEC_To_BCD(unsigned char dec)
{
    unsigned char temp1, temp2, temp;
#if 0
    printf("in : %d \n", dec);
#endif
    temp1 = dec/10;
    temp2 = dec%10;

    temp1 <<= 4;
    temp = temp1|temp2;
#if 0
    printf("out : %d \n", temp);
#endif
    return temp;
}

/* BCD and DEC convert */
static char BCD_To_DEC (unsigned char bcd)
{
    unsigned char temp1, temp2, temp;

    temp1 = bcd&0xf0;
    temp2 = bcd&0x0f;

    temp1 >>= 4;
    temp = temp1*10 + temp2;

    return temp;
}

/*
 * RTC init function for Seiko S-35390
 */
void sc_rtc_init(void)
{
	int err;
	    
    udelay(1000);

    /* reset RTC */
	err = s35390a_reset(sc_i2c_dev);
	if (err < 0) {
		debug("error resetting chip\n");
		return;
	}	
	
	err = s35390a_disable_test_mode(sc_i2c_dev);
	if (err < 0) {
		debug("error disabling test mode\n");
		return;
	}	
}

/*
 * RTC get time 
 */
int sc_rtc_get_time(SC_RTC_TIME * time)
{
	SC_RTC_TIME time_in_bcd;
	
	if(s35390a_get_reg(sc_i2c_dev, CMD_TIME_YEAR, (char *)&(time_in_bcd), RTC_TIME_YEAR_LEN))
	{
		printf("Error : read rtc time failed!\n");
		return RTC_OP_FAIL;
	}
	
	/* bits convertion */
	time_in_bcd.year 	= swap_bits_in_char(time_in_bcd.year );
	time_in_bcd.month 	= swap_bits_in_char(time_in_bcd.month);
	time_in_bcd.day 	= swap_bits_in_char(time_in_bcd.day  );
	time_in_bcd.week 	= swap_bits_in_char(time_in_bcd.week );
	time_in_bcd.hour 	= swap_bits_in_char(time_in_bcd.hour );
	time_in_bcd.min 	= swap_bits_in_char(time_in_bcd.min  );
	time_in_bcd.sec 	= swap_bits_in_char(time_in_bcd.sec  );	
	
	/* bcd -> dec */
	time->year 	= BCD_To_DEC(time_in_bcd.year );
	time->month = BCD_To_DEC(time_in_bcd.month);
	time->day 	= BCD_To_DEC(time_in_bcd.day  );
	time->week	= BCD_To_DEC(time_in_bcd.week );
	time->hour 	= BCD_To_DEC(time_in_bcd.hour );
	time->min  	= BCD_To_DEC(time_in_bcd.min  );
	time->sec	= BCD_To_DEC(time_in_bcd.sec  );
	
	return RTC_OP_OK;
}

/* 
 * RTC set time
 */
int sc_rtc_set_time(SC_RTC_TIME * time)
{
	SC_RTC_TIME time_in_bcd;
	
	if(time == NULL)
	{
		printf("Error : NULL time to set into RTC.\n");
		return RTC_OP_FAIL;
	}
	
	/* dec -> bcd */
	time_in_bcd.year 	= DEC_To_BCD(time->year ); 			
	time_in_bcd.month 	= DEC_To_BCD(time->month); 	
	time_in_bcd.day 	= DEC_To_BCD(time->day 	);
	time_in_bcd.week 	= DEC_To_BCD(time->week	);
	time_in_bcd.hour 	= DEC_To_BCD(time->hour ); 	
	time_in_bcd.min 	= DEC_To_BCD(time->min 	); 	
	time_in_bcd.sec 	= DEC_To_BCD(time->sec	);	    

	/* bits convertion */
	time_in_bcd.year 	= swap_bits_in_char(time_in_bcd.year );
	time_in_bcd.month 	= swap_bits_in_char(time_in_bcd.month);
	time_in_bcd.day 	= swap_bits_in_char(time_in_bcd.day  );
	time_in_bcd.week 	= swap_bits_in_char(time_in_bcd.week );
	time_in_bcd.hour 	= swap_bits_in_char(time_in_bcd.hour );
	time_in_bcd.min 	= swap_bits_in_char(time_in_bcd.min  );
	time_in_bcd.sec 	= swap_bits_in_char(time_in_bcd.sec  );	
	
	/* set into RTC */
	if(s35390a_set_reg(sc_i2c_dev, CMD_TIME_YEAR, (char *)&(time_in_bcd), RTC_TIME_YEAR_LEN))
	{
		printf("Error : set RTC time failed.\n");
		return RTC_OP_FAIL;
	}
	else
		return RTC_OP_OK;
}


/*
 * Set the RTC
 */
int rtc_set (struct rtc_time *tmp)
{	
	SC_RTC_TIME sc_time;

	DEBUGR ("Set DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
		tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	sc_time.year  = tmp->tm_year - 2000;
	sc_time.month = tmp->tm_mon;
	sc_time.day   = tmp->tm_mday;
	sc_time.week  = tmp->tm_wday;
	sc_time.hour  = tmp->tm_hour;
	sc_time.min   = tmp->tm_min;
	sc_time.sec   = tmp->tm_sec;

	sc_rtc_set_time(&sc_time);
	return 0;
}

/*
 * Get the current time from the RTC
 */
int rtc_get (struct rtc_time *tmp)
{
	int rel = 0;
	
	SC_RTC_TIME sc_time;
	
	if(sc_rtc_get_time(&sc_time) != RTC_OP_OK)
	{
		printf("get RTC time failed!\n");
		return -1;
	}

	tmp->tm_sec  = sc_time.sec;
	tmp->tm_min  = sc_time.min;
	tmp->tm_hour = sc_time.hour;
	tmp->tm_mday = sc_time.day;
	tmp->tm_mon  = sc_time.month;
	tmp->tm_year = sc_time.year + 2000;
	tmp->tm_wday = sc_time.week;
	tmp->tm_yday = 0;
	tmp->tm_isdst= 0;

	DEBUGR ("Get DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
		tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	return rel;
}

void rtc_reset (void)
{
    /* reset RTC */
	s35390a_reset(sc_i2c_dev);	
}
	
