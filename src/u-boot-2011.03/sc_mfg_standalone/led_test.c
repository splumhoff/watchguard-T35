/***********************************************************************
 * 			NewCastle Uboot Test Software Version 1.0      *
 ***********************************************************************
 * Module	: LED Test					       *
 ***********************************************************************/

/*
 * Header File
 */
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include "sc_mfg.h"

/*
 * Init gpio for led & button
 */
void sc_led_init(void)
{
	unsigned int reg_val;
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	reg_val = pgpio->gpdir;
	/* set gpio dir out for led */
	reg_val |= 1 << (31 - LED_ATTENTION);
	reg_val |= 1 << (31 - LED_SW_STATUS);
	reg_val |= 1 << (31 - LED_SW_MODE);
	reg_val |= 1 << (31 - LED_WAP_BG);
	reg_val |= 1 << (31 - LED_WAP_N);
	reg_val |= 1 << (31 - LED_FAILOVER);
	/* set gpio dir in for button */
	reg_val &= ~(1 << (31 - BTN_RESET));

	pgpio->gpdir = reg_val;
	/* set default state: all off */
	reg_val = pgpio->gpdat;

	reg_val |= 1 << (31 - LED_ATTENTION);
	reg_val |= 1 << (31 - LED_SW_STATUS);
	reg_val |= 1 << (31 - LED_SW_MODE);
	reg_val &= ~(1 << (31 - LED_WAP_BG));
	reg_val &= ~(1 << (31 - LED_WAP_N));
	reg_val |= 1 << (31 - LED_FAILOVER);

	pgpio->gpdat = reg_val;
}

/*
* Turn led on by given gpio number
*/
void sc_led_on(unsigned int led)
{
	unsigned int reg_val;
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	reg_val = pgpio->gpdat;

	reg_val &= ~(1 << (31 - led));

	pgpio->gpdat = reg_val;
}

/*
* Turn led off by given gpio number
*/
void sc_led_off(unsigned int led)
{
	unsigned int reg_val;
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	reg_val = pgpio->gpdat;

	reg_val |= 1 << (31 - led);

	pgpio->gpdat = reg_val;
}

/*
* Read button status by given gpio number
*/
int sc_read_btn_status(unsigned int btn)
{
	unsigned int reg_val;
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	reg_val = pgpio->gpdat;
	/* Low active */
	if (reg_val & (1 << (31 - btn)) )
	return 0;
	else
	return 1;
}


/*
* LED Test main entry
*/
int Led_Test()
{		
	unsigned long long Led_start_time, Led_timer;
	int i = 0;

	/* init gpio for led & btn first */
	sc_led_init();

	for (i=0; i<LED_TEST_CYCLE; i++)
	{
		sc_led_on(LED_FAILOVER);
		udelay(1000000);
		sc_led_off(LED_FAILOVER);

		udelay(1000000);

		sc_led_on(LED_WAP_N);
		sc_led_off(LED_WAP_BG);
		udelay(1000000);
		sc_led_off(LED_WAP_N);
		sc_led_on(LED_WAP_BG);

		udelay(1000000);

		sc_led_on(LED_WAP_BG);
		sc_led_off(LED_WAP_N);
		udelay(1000000);
		sc_led_off(LED_WAP_BG);
		sc_led_on(LED_WAP_N); 	

		udelay(1000000);

		sc_led_on(LED_SW_MODE);
		udelay(1000000);
		sc_led_off(LED_SW_MODE);

		udelay(1000000);

		sc_led_on(LED_SW_STATUS);
		udelay(1000000);
		sc_led_off(LED_SW_STATUS);

		udelay(1000000);

		sc_led_on(LED_ATTENTION);
		udelay(1000000);
		sc_led_off(LED_ATTENTION);

		udelay(1000000);
	}

	printf("Monitor Button Status..\n");

	/* wait for button press */
	Led_start_time = 0;
	Led_timer = (LED_TEST_TIMEOUT)*1000;

	while ( Led_start_time <= Led_timer)
		{
		if (sc_read_btn_status(BTN_RESET)) {
			memcpy(test_result, TEST_LED_PASS_STR, sizeof(TEST_LED_PASS_STR));
			test_result[TEST_RESULT_OFF] = LED_TEST_PASS;
			set_result_state=Set_Test_Result( test_result, LED_TEST_ITEM );
			if (set_result_state) {
				printf("write the LED_TEST (OK) is fail \n");
			}
			else {
				printf("write the LED_TEST (OK) is OK \n");
			}
			printf("LED Test\n");
			printf("LED and Reset Button Test Pass\n");
			return SC_MFG_TEST_OP_PASS;
		}
		udelay(1000);
		Led_start_time += 1;
	}

	printf("LED Test\n");
	printf("LED or Reset Button Test Fail\n");
	memcpy(test_result, TEST_LED_FAIL_SRT, sizeof(TEST_LED_FAIL_SRT));
	printf("memcpy done.\n");
	test_result[TEST_RESULT_OFF] = LED_TEST_FAIL;
	set_result_state=Set_Test_Result( test_result, LED_TEST_ITEM );
	if (set_result_state) {
		printf("write the LED_TEST (fail) is fail \n");
		return SC_MFG_TEST_OP_FAIL;
	}
	else {
		printf("write the LED_TEST (fail) is OK \n");
		return SC_MFG_TEST_OP_FAIL;
	}

}


