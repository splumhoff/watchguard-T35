/* This file inclues all functions which are changed on different platforms. */
/* This means : these APIs should be supported by externel of this test tool */
#include <asm/cache.h>
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <exports.h>

/*===============================*/

#include "ram_test.h"
/*
 * Sleep fucntion used by bit fade test
 */
void sleep(int n)
{
	int i = 0,j;
	for(;i<n;i++)
	{
		for(j=0;j<1000;j++)		
		{	//1 s
			udelay(1000); //1 ms
		}
		if(i==60)
			diag_printf("Elapsed time : about 1 minute \n");
	}
}

/*
 * Control to enable caches if need be ...
 */
int test_complete_en_caches = 0;
void test_complete_enable_caches(void)
{
	test_complete_en_caches = 1;
}

/*
 * Cache Operation
 */
void set_cache(int val) /* 1 - on , 0 - off */
{
#ifdef TEST_CODE
	ulong enable_caches = 0; /* release code */
	char *s;

	 /* release code */
	if ((s = getenv ("enable_caches")) != NULL) {
		enable_caches = simple_strtoul (s, NULL, 16);
	}

	if(val && enable_caches && test_complete_en_caches)
	{
#else
	if(val && test_complete_en_caches)
	{
#endif
		diag_printf("MJ:new cache code executing\n");

		flush_dcache();
		invalidate_icache();	
		
		icache_enable();
		dcache_enable();
	}
	else
	{		
		flush_dcache();
		invalidate_icache();	
		
		icache_disable();
		dcache_disable();	
	}	
}
/*
 * Find out how much memory there is.
 */
/*
void get_test_region(ulong start, ulong end)
{
	v->map.start = start;
	v->map.end = end;
}
*/
void get_mem_map(void)
{
#if defined(CONFIG_XTM2_1M)
	mem_info.start = 0x01300000;	//star: start of address space for DDR
	mem_info.end   = 0x1F900000;	//end : leave 7 Mbytes space for uboot itself and other.
#else
	mem_info.start = 0x01300000;	//star: start of address space for DDR
	mem_info.end   = 0x3F900000;	//end : leave 7 Mbytes space for uboot itself and other.
#endif

	diag_printf("Test Range : %lx - %lx.\n", mem_info.start, mem_info.end);
}
/*
extern ulong cpu_freq;
void get_cpu_freq(void)
{
	v->clks_msec = cpu_freq;
}
*/
void disable_DDR_ECC(void)
{
	/* set dummy now, we won't used ECC for NewCastle */
}
void enable_DDR_ECC(void)
{
	/* set dummy now, we won't used ECC for NewCastle */
}
void poll_errors(void)
{
}

