/****************************************************************
 *	NewCastle Uboot Test Software Version 1.0		*
 ****************************************************************
 *	Module 	: RTC Test					*
 *	Chip	: Seiko 35390					*
 ****************************************************************/

/*
 * Header File
 */
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include "sc_mfg.h"
#include "../drivers/rtc/s35390.h"

/*
 * RTC Test example function:
 * Set time to 2008-08-08 08:08:08 and delay 5 second
 * and then check the time
 */
int rtc_set_get_test(void)
{
	SC_RTC_TIME time;
	SC_RTC_TIME time_get;

	time.year	= 8; //year 2008
	time.month	= 8; //month
	time.day	= 8; //day
	time.week	= 5; //day of week data,2008 -08 -08 is Fri
	time.hour	= 8; //hour
	time.min	= 8; //minute
	time.sec	= 8; //second

	printf("***Will set time to 2008-08-08 08:08:08***\n");

	if (sc_rtc_set_time(&time) == RTC_OP_FAIL) {
		printf("***Set time to 2008-08-08 08:08:08 error***\n");
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("***Set time to 2008-08-08 08:08:08 success, will delay %d second...***\n", RTC_TEST_DELAY);
	udelay(1000000 * RTC_TEST_DELAY);

	if (sc_rtc_get_time(&time_get) == RTC_OP_FAIL) {
		printf("***After set time,read back error***");
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("Read back time:%d-%d-%d %d(day of week) %d:%d:%d\n",
	time_get.year + 2000,
	time_get.month,
	time_get.day,
	time_get.week,
	time_get.hour,
	time_get.min,
	time_get.sec);

	if ((time_get.year != time.year ) || (time_get.month != time.month) ||
	    (time_get.day != time.day ) || (time_get.week != time.week) ||
	    (time_get.hour != time.hour) || (time_get.min != time.min )) {
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("Set second:%d, Read second:%d \n", time.sec, time_get.sec);
	if ( (time_get.sec - time.sec) == RTC_TEST_DELAY ) {
		return SC_MFG_TEST_OP_PASS;
	}
	else {
		return SC_MFG_TEST_OP_FAIL;
	}
}

/*
* RTC Test main entry
*/
int RTC_Test(void)
{
	int RTC_result;

	/* I2C bus Init */
	sc_i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	/* RTC Chip Init */
	sc_rtc_init();

	RTC_result = rtc_set_get_test();

	if ( RTC_result != SC_MFG_TEST_OP_PASS ) {
		memcpy(test_result, TEST_RTC_FAIL_STR, sizeof(TEST_RTC_FAIL_STR));
		test_result[TEST_RESULT_OFF] = RTC_TEST_FAIL;
		set_result_state = Set_Test_Result( test_result, RTC_TEST_ITEM );

		if (set_result_state) {
			printf("RTC Test\n");
			printf("RTC Test Fail\n");
			printf("Fail\n");
			printf("write the RTC Test (Fail) is fail \n");
			return SC_MFG_TEST_OP_FAIL;
		}
		else {
			printf("RTC Test\n");
			printf("RTC Test Fail\n");
			printf("Fail\n");
			printf("write the RTC Test (Fail) is ok \n");
			return SC_MFG_TEST_OP_FAIL;
		}
	}
	else {
		memcpy(test_result, TEST_RTC_PASS_STR, sizeof(TEST_RTC_PASS_STR));
		test_result[TEST_RESULT_OFF] = RTC_TEST_PASS;
		set_result_state = Set_Test_Result( test_result, RTC_TEST_ITEM );
		if (set_result_state) {
			printf("RTC Test\n");
			printf("RTC Test Pass\n");
			printf("Pass\n");
			printf("write the RTC Test (OK) is fail \n");
			return SC_MFG_TEST_OP_FAIL;
		}
		else {
			printf("RTC Test\n");
			printf("RTC Test Pass\n");
			printf("Pass\n");
			printf("write the RTC Test (OK) is OK \n");
			return SC_MFG_TEST_OP_PASS;
		}
	}
}

