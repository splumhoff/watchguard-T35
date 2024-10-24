/*
 * (C) Copyright 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * RTC, Date & Time support: get and set date & time
 */
#include <common.h>
#include <command.h>
#include <rtc.h>
#include <i2c.h>

DECLARE_GLOBAL_DATA_PTR;

static const char * const weekdays[] = {
	"Sun", "Mon", "Tues", "Wednes", "Thurs", "Fri", "Satur",
};

#ifdef CONFIG_NEEDS_MANUAL_RELOC
#define RELOC(a)	((typeof(a))((unsigned long)(a) + gd->reloc_off))
#else
#define RELOC(a)	a
#endif

int mk_date (const char *, struct rtc_time *);

static int do_date(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct rtc_time tm;
	int rcode = 0;
	int old_bus =0;

	/* switch to correct I2C bus */
#ifdef CONFIG_SYS_I2C
	old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_RTC_BUS_NUM);
#else
	old_bus = I2C_GET_BUS();
	I2C_SET_BUS(CONFIG_SYS_RTC_BUS_NUM);
#endif

	switch (argc) {
	case 2:			/* set date & time */
		if (strcmp(argv[1],"reset") == 0) {
			puts ("Reset RTC...\n");
			rtc_reset ();
		} else {
			/* initialize tm with current time */
			rcode = rtc_get (&tm);

			if(!rcode) {
				/* insert new date & time */
				if (mk_date (argv[1], &tm) != 0) {
					puts ("## Bad date format\n");
					break;
				}
				/* and write to RTC */
				rcode = rtc_set (&tm);
				if(rcode)
					puts("## Set date failed\n");
			} else {
				puts("## Get date failed\n");
			}
		}
		/* FALL TROUGH */
	case 1:			/* get date & time */
		rcode = rtc_get (&tm);

		if (rcode) {
			puts("## Get date failed\n");
			break;
		}

		printf ("Date: %4d-%02d-%02d (%sday)    Time: %2d:%02d:%02d\n",
			tm.tm_year, tm.tm_mon, tm.tm_mday,
			(tm.tm_wday<0 || tm.tm_wday>6) ?
				"unknown " : RELOC(weekdays[tm.tm_wday]),
			tm.tm_hour, tm.tm_min, tm.tm_sec);

		break;
	default:
		rcode = CMD_RET_USAGE;
	}

	/* switch back to original I2C bus */
#ifdef CONFIG_SYS_I2C
	i2c_set_bus_num(old_bus);
#else
	I2C_SET_BUS(old_bus);
#endif

	return rcode;
}

#ifdef CONFIG_LANNER_BURN
extern int testlog (int offset, unsigned char data);
int do_rtc_test (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct rtc_time tm1;
	struct rtc_time tm2;
	int rcode = 0;
	int test_time = 1 ;
	int current = 0;
	test_time  = simple_strtoul(argv[1], NULL, 10);
	printf("RTC Test:\n");
	testlog(2,0);
	if(test_time < 0  && test_time >= 60)
	{
		testlog(2,1);
		printf("Test time error\n");
		return 1;
	}
	/* switch to correct I2C bus */
	I2C_SET_BUS(CONFIG_SYS_RTC_BUS_NUM);
	rcode = rtc_get (&tm1);
	if (rcode) {
		puts("## Get date failed\n");
		return 1;
	}
	printf ("Date: %4d-%02d-%02d (%sday)    Time: %2d:%02d:%02d\n",
		tm1.tm_year, tm1.tm_mon, tm1.tm_mday,
		(tm1.tm_wday<0 || tm1.tm_wday>6) ?
		"unknown " : RELOC(weekdays[tm1.tm_wday]),
		tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
	//system wait time
	udelay(1000000*test_time);
	rcode = rtc_get (&tm2);
	if (rcode) {
		puts("## Get date failed\n");
		return 1;
	}
	printf ("Date: %4d-%02d-%02d (%sday)    Time: %2d:%02d:%02d\n",
		tm2.tm_year, tm2.tm_mon, tm2.tm_mday,
		(tm2.tm_wday<0 || tm2.tm_wday>6) ?
		"unknown " : RELOC(weekdays[tm2.tm_wday]),
		tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
	if((tm1.tm_sec + test_time) < 60 )
	{
		if((tm1.tm_sec + test_time) == tm2.tm_sec)
			current = 1;
	}
	else
	{
		if((tm1.tm_min + 1) == 60 )
		{
			if(tm2.tm_min == 0 )
			{
				if(((tm1.tm_sec + test_time)-60 ) == tm2.tm_sec)
					current = 1;
			}
		}
		else if((tm1.tm_min + 1) == tm2.tm_min)
		{
			if(((tm1.tm_sec + test_time)-60 ) == tm2.tm_sec)
				current = 1;
		}
	}
	if(current  == 1 )
	{
		printf("PASS\n");
		testlog(2,99);
	}
	else
	{
		printf("FAIL\n");
		testlog(2,2);
	}
	return 0;
}
#endif /* CONFIG_LANNER_BURN */

/*
 * simple conversion of two-digit string with error checking
 */
static int cnvrt2 (const char *str, int *valp)
{
	int val;

	if ((*str < '0') || (*str > '9'))
		return (-1);

	val = *str - '0';

	++str;

	if ((*str < '0') || (*str > '9'))
		return (-1);

	*valp = 10 * val + (*str - '0');

	return (0);
}

/*
 * Convert date string: MMDDhhmm[[CC]YY][.ss]
 *
 * Some basic checking for valid values is done, but this will not catch
 * all possible error conditions.
 */
int mk_date (const char *datestr, struct rtc_time *tmp)
{
	int len, val;
	char *ptr;

	ptr = strchr (datestr,'.');
	len = strlen (datestr);

	/* Set seconds */
	if (ptr) {
		int sec;

		*ptr++ = '\0';
		if ((len - (ptr - datestr)) != 2)
			return (-1);

		len = strlen (datestr);

		if (cnvrt2 (ptr, &sec))
			return (-1);

		tmp->tm_sec = sec;
	} else {
		tmp->tm_sec = 0;
	}

	if (len == 12) {		/* MMDDhhmmCCYY	*/
		int year, century;

		if (cnvrt2 (datestr+ 8, &century) ||
		    cnvrt2 (datestr+10, &year) ) {
			return (-1);
		}
		tmp->tm_year = 100 * century + year;
	} else if (len == 10) {		/* MMDDhhmmYY	*/
		int year, century;

		century = tmp->tm_year / 100;
		if (cnvrt2 (datestr+ 8, &year))
			return (-1);
		tmp->tm_year = 100 * century + year;
	}

	switch (len) {
	case 8:			/* MMDDhhmm	*/
		/* fall thru */
	case 10:		/* MMDDhhmmYY	*/
		/* fall thru */
	case 12:		/* MMDDhhmmCCYY	*/
		if (cnvrt2 (datestr+0, &val) ||
		    val > 12) {
			break;
		}
		tmp->tm_mon  = val;
		if (cnvrt2 (datestr+2, &val) ||
		    val > ((tmp->tm_mon==2) ? 29 : 31)) {
			break;
		}
		tmp->tm_mday = val;

		if (cnvrt2 (datestr+4, &val) ||
		    val > 23) {
			break;
		}
		tmp->tm_hour = val;

		if (cnvrt2 (datestr+6, &val) ||
		    val > 59) {
			break;
		}
		tmp->tm_min  = val;

		/* calculate day of week */
		GregorianDay (tmp);

		return (0);
	default:
		break;
	}

	return (-1);
}

/***************************************************/

U_BOOT_CMD(
	date,	2,	1,	do_date,
	"get/set/reset date & time",
	"[MMDDhhmm[[CC]YY][.ss]]\ndate reset\n"
	"  - without arguments: print date & time\n"
	"  - with numeric argument: set the system date & time\n"
	"  - with 'reset' argument: reset the RTC"
);

#ifdef CONFIG_LANNER_BURN
U_BOOT_CMD(
	rtc_test,       2,      1,      do_rtc_test,
	"simple rtc read test for second",
	"waittime\n"
);
#endif /* CONFIG_LANNER_BURN */

