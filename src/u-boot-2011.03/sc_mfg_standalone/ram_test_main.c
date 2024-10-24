/***********************************************************************
 * 			NewCastle Uboot Test Software Version 1.0      *
 ***********************************************************************
 * Module	: RAM Test					       *
 ***********************************************************************/
/*
 * Header File
 */
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include "sc_mfg.h"
#include "ram_test.h"

#define DEFTESTS 9

/* TODO tests in designed sequence */
struct tseq_t tseq[] = {
/*	cache	pat	iter	ticks	errors	msg					*/
	{0, 	5,	1,	0,	0,	"[Address test, walking ones, no cache]"},
	{1, 	6,	1,	2,	0,	"[Address test, own address]"		},
	{1, 	12, 	1,	14,	0,	"[Moving inversions, ones & zeros]"	},
	{1, 	1,	1,	80,	0,	"[Moving inversions, 8 bit pattern]"	},
	{1,	10,	1,	300,	0,	"[Moving inversions, random pattern]"	},
	{1,	7,	1,	66,	0,	"[Block move, 64 moves]"		},
	{1,	2,	1,	320,	0,	"[Moving inversions, 32 bit pattern]"	},
	{1,	9,	1,	120,	0,	"[Random number sequence]"		},
	{1,	11,	1,	1920,	0,	"[Modulo 20, Random pattern]"		},
	{1,	3,	1,	1920,	0,	"[Module 20, ones & zeros]"		},
	{1,	4,	1,	1920,	0,	"[Module 20, 8bit pattern]"		},
	/*{1,	8,	1,	2,	0,	"[Bit fade test, 90 min, 2 patterns]"	},*/
	{0,	0,	0,	0,	0,	NULL					}
};


struct mem_info_t mem_info;

char firsttime = 0;
struct vars variables = {};
struct vars * v ;

volatile ulong *p = 0;
ulong p1 = 0, p2 = 0, p0 = 0;
int bail = 0;
/***********************************************************************/
/*
 * Initialize test, setup screen and find out how much memory there is.
 */
void init(void)
{
	int i;
	
	v = &variables;
	/* Turn on cache */
	/* default cache is open */
	//set_cache(1);

	/* Setup the display */
	//display_init();
	/****************************************************/
	/* Determine the memory map			    */
	/* TONY :					    */
	/* (a) the X86 original code detects which BIOS is. */
	/* 	remove it, for its useless on our platform  */
	/* (b) the memory size reads from BIOS(pc/linux) Or */
	/*	probe the memory size, I think this could be*/	
	/*	omitted on our paltform			    */
	/* (c) because there maybe several segments in x86  */
	/*	system, so use 'v' data structure to store  */
	/*	segments information such as every segments'*/
	/*	start, end, and records total page bumber   */
	/*     On our platform, there is no need to do such */
	/* 	   action				    */
	/****************************************************/
	get_mem_map();
	/****************************************************/
	/* TONY : the original code do PCI_INIT here, REMOVE*/
	/****************************************************/
	v->test		= 0;
	v->pass		= 0;
	v->msg_line	= 0;
	v->ecount	= 0;
	v->ecc_ecount	= 0;
	v->testsel	= -1;
	
	/* test range set !!! */
	v->map.start = mem_info.start;
	v->map.end = mem_info.end;


	//get_cpu_freq();	
	
	for (i=0; tseq[i].msg != NULL; i++)
	{
		tseq[i].errors = 0;
	}
	/*************************************************************/
	/* TONY :						     */
	/* What is DMI ?					     */
	/* Desktop Management Interface - hardcored on motherboard   */
	/* on X86 PC system, which is used to gather system hardware */
	/* information. --- REMOVE it on our platform		     */
	/*************************************************************/
	/* TONY CLOSE*/
	/*
	 * Need to find out CPU type before seting up pci. This is
	 * because AMD Opterons dont have a host bridge on dev 0.
	 */
	/************************************************/
	/* TONY :					*/
	/* This function would find out CPU type of x86 */
	/* system, and would get Cache L1 & L2 size, 	*/
	/* Then CPU speed, L1 & L2 speed, Memory speed 	*/
	/* All of the speed detection is based in RDTSC */
	/* !!! PAY ATTECTION - this paltform related!   */
	/************************************************/
#if 0 /* tony : close for temp */
	cpu_type();
#endif
	/* setup pci */
	/************************************************/
	/* TONY : I dont think PCI init is useful for us*/
	/************************************************/
	#if 0 /* tony */
	pci_init();
	#endif
	/* Find the memory controller */
	/************************************************/
	/* TONY : Original code using this to what is   */
	/* controlling memory , such as ECC (?), disable*/
	/* ECC by now. Maybe we should follow this ???	*/
	/************************************************/
	/* I think disable ECC is good for us */
#if 0
	find_controller(); /* to disable ECC ? */
#else
	disable_DDR_ECC();
#endif
	/* print VAR init info */
#if 0
	/* ticks deal */
	find_ticks();
#endif
}

/* main thread part */
int do_ram_test(void)
{
	int i = 0, j = 0;
	int seq = 0;
	
	int err_no;


	diag_printf("Going to do RAM test now ...\n");
	/* If first time, initialize test */
	if (firsttime == 0)
	{
		init();
		firsttime = 1;
	}
	/* tony : useful for counter */
	bail = 0;

	/* Update display of memory segments being tested */

	/* Now setup the test parameters based on the current test number */
	/* Figure out the next test to run */
	for(; tseq[seq].msg != NULL; seq++)
	{

		v->testsel = seq;
		if (v->testsel >= 0)
		{
			v->test = v->testsel;
		}
		diag_printf("**************************************************************\n");
		diag_printf("*  Test To be done : %s  *\n", tseq[v->test].msg);
		diag_printf("**************************************************************\n");

		set_cache(tseq[v->test].cache);
		/* Have a way to cancel memory tests ... */
		if (tstc () && getc () == 0x3) {
			printf ("CTRL + C detected, Aborting memtests ...\n");
			goto skip_test;
		}
		switch(tseq[v->test].pat)
		{
	
		/* Now do the testing according to the selected pattern */
		case 0:	/* Moving inversions, all ones and zeros */
			p1 = 0;
			p2 = ~p1;
			movinv1(tseq[v->test].iter,p1,p2);
			BAILOUT;
		
			/* Switch patterns */
			p2 = p1;
			p1 = ~p2;
			movinv1(tseq[v->test].iter,p1,p2);
			BAILOUT;
			break;
			
		case 1: /* Moving inversions, 8 bit wide walking ones and zeros. */
			p0 = 0x80;
			for (i=0; i<8; i++, p0=p0>>1) {
				p1 = p0 | (p0<<8) | (p0<<16) | (p0<<24);
				p2 = ~p1;
				movinv1(tseq[v->test].iter,p1,p2);
				BAILOUT;
		
				/* Switch patterns */
				p2 = p1;
				p1 = ~p2;
				movinv1(tseq[v->test].iter,p1,p2);
				BAILOUT
			}
			break;
	
		case 2: /* Moving inversions, 32 bit shifting pattern, very long */
			for (i=0, p1=1; p1; p1=p1<<1, i++) {
				movinv32(tseq[v->test].iter,p1, 1, 0x80000000, 0, i);
				BAILOUT
				movinv32(tseq[v->test].iter,~p1, 0xfffffffe,0x7fffffff, 1, i);
				BAILOUT
			}
			break;
	
		case 3: /* Modulo 20 check, all ones and zeros */
			p1=0;
			for (i=0; i<MOD_SZ; i++) {
				p2 = ~p1;
				modtst(i, tseq[v->test].iter, p1, p2);
				BAILOUT
	
				/* Switch patterns */
				p2 = p1;
				p1 = ~p2;
				modtst(i, tseq[v->test].iter, p1,p2);
				BAILOUT
			}
			break;
	
		case 4: /* Modulo 20 check, 8 bit pattern */
			p0 = 0x80;
			for (j=0; j<8; j++, p0=p0>>1) {
				p1 = p0 | (p0<<8) | (p0<<16) | (p0<<24);
				for (i=0; i<MOD_SZ; i++) {
					p2 = ~p1;
					modtst(i, tseq[v->test].iter, p1, p2);
					BAILOUT
	
					/* Switch patterns */
					p2 = p1;
					p1 = ~p2;
					modtst(i, tseq[v->test].iter, p1, p2);
					BAILOUT
				}
			}
			break;
		case 5: /* Address test, walking ones */
			addr_tst1();
			BAILOUT;
			break;
	
		case 6: /* Address test, own address */
			addr_tst2();
			BAILOUT;
			break;
	
		case 7: /* Block move test */
			block_move(tseq[v->test].iter);
			BAILOUT;
			break;
		case 8: /* Bit fade test */
			bit_fade();

			BAILOUT;
			break;
		case 9: /* Random Data Sequence */
			for (i=0; i < tseq[v->test].iter; i++) {
				movinvr();
				BAILOUT;
			}
			break;
		case 10: /* Random Data */
			for (i=0; i < tseq[v->test].iter; i++) {
				p1 = rand();
				p2 = ~p1;
				movinv1(2,p1,p2);
				BAILOUT;
			}
			break;
	
		case 11: /* Modulo 20 check, Random pattern */
			for (j=0; j<tseq[v->test].iter; j++) {
				p1 = rand();
				for (i=0; i<MOD_SZ; i++) {
					p2 = ~p1;
					modtst(i, tseq[v->test].iter, p1, p2);
					BAILOUT
	
					/* Switch patterns */
					p2 = p1;
					p1 = ~p2;
					modtst(i, tseq[v->test].iter, p1, p2);
					BAILOUT
				}
			}
			break;
		}
		diag_printf("*********************************************\n");
		diag_printf("*	   Test item %d OVER                 *\n", seq);
		diag_printf("*********************************************\n");
		diag_printf("\n");
		diag_printf("\n");
	}
skip_test:
	if (v->ecount == 0)
	{
		diag_printf("Pass %d complete, no errors\n", v->pass);
		v->pass++;
		test_complete_enable_caches();
		set_cache(1);

		return TEST_OK;
	}
	else
	{
		
		diag_printf("Test Have Errors.\n");
		test_complete_enable_caches();
		set_cache(1);
		
		err_no = tseq[v->test].pat;

		return err_no;
	}	
	/* Rever to the default mapping and enable the cache */
#if 0
	/* TONY : we won't use ECC feature */
	enable_DDR_ECC();
#endif
}

int find_ram_test_item(int pat)
{
	int i = 0;
	
	for(;i < sizeof(tseq)/sizeof(struct tseq_t); i++)
	{
		/* success to find index */
		if(pat == tseq[i].pat)
			return i;
	}
	
	/* fail to find */
	return -1;
}

extern unsigned int err_address;
extern int err_test_type;
extern char errbitmask;

/*
 * Main Test Entry for RAM Test
 */
int Ram_Test(void)
{
	int ret = -1;
	int idx = -1;

	ret = do_ram_test();

	if (ret == TEST_OK) {
		memcpy(test_result, TEST_DRAM_PASS_STR, sizeof(TEST_DRAM_PASS_STR));

		test_result[TEST_RESULT_OFF] = DRAM_TEST_OK;
		set_result_state = Set_Test_Result( test_result, DRAM_TEST_ITEM );

		printf("DRAM Test Pass \n");

		if (set_result_state) {
			printf("write the DRAM Test (OK) is fail \n");
			return SC_MFG_TEST_OP_FAIL;
		}
		else {
			printf("write the DRAM Test (OK) is OK \n");
			return SC_MFG_TEST_OP_PASS;
		}
	}
	else {
		printf("DRAM Test Fail \n");
		memcpy(test_result, TEST_DRAM_FAIL_STR, sizeof(TEST_DRAM_FAIL_STR));

		idx = find_ram_test_item(ret);

		if (idx != -1) {
			printf("%s", tseq[idx].msg);
			test_result[TEST_RESULT_OFF] = ret;

			/* record the reasons for DRAM test Failure */
			*(unsigned int *)&test_result[DRAM_TEST_ERR_ADDR] = err_address;
			*(int *)&test_result[DRAM_TEST_ERR_TYPE] = err_test_type;
			*(char *)&test_result[DRAM_TEST_ERR_CHIP] = errbitmask;
		}
		else { /* unknown fail return value */
			test_result[TEST_RESULT_OFF] = DRAM_TEST_FAIL_UNKNOWN;
		}

		set_result_state = Set_Test_Result( test_result, DRAM_TEST_ITEM );
		if (set_result_state) {
			printf("write the DRAM Test (fail) is fail \n");
		}
		else {
			printf("write the DRAM Test (fail) is OK \n");
		}
		return SC_MFG_TEST_OP_FAIL;
	}
}


