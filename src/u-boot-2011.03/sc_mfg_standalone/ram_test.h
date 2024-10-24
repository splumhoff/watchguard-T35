#ifndef _RAM_TEST_H_
#define _RAM_TEST_H_
/* ram_test.h
 * Athour	: Tony Tang
 * Date 	: 2008-04-15
 * Usage	: Common Ram Test Code header file, which includes data structures.
 */

/*
 * Platform Definitions - here for u-boot.
 */
#include <common.h>
#include <command.h>
#include <linux/types.h>
#define diag_printf printf

/*------------------------------------------*/
extern struct vars * v;
extern struct mem_info_t mem_info;

#define BAILOUT		if (bail) goto skip_test;
#define BAILR		if (bail) return;
#define MOD_SZ		20
#define TEST_OK		(99)
#define SPINSZ		0x100000 /* 1MB */

struct err_info
{
	unsigned long ebits;
	long tbits;
	short min_bits;
	short max_bits;
	unsigned long maxl;
	unsigned long eadr;
	unsigned long exor;
	unsigned long cor_err;
};

struct mem_info_t {
	ulong start;
	ulong end;
};

struct tseq_t {
	short cache;
	short pat;
	short iter;
	short ticks;
	short errors;
	char *msg;
};

struct vars
{
	int test;		/* test number of test sequence */
	int pass;		/* what this for ??? - if set 1, equals to the test is passed and memory test is OK */
	int msg_line;		/* ??? */
	int ecount;		/* Error Counter ? */
	int ecc_ecount;		/* ECC Error Counter ? */
	int testsel;		/* test number in progress */
	struct err_info erri;
	struct mem_info_t map;	/* The memory map for test, including start & end address */
	//struct mem_info_t *test_region;
	//ulong clks_msec;	/* CPU clock in khz */
};

void disableECC(void);
void sleep(int n);
void set_cache(int val) ;
void test_complete_enable_caches(void);
void get_test_region(ulong start, ulong end);
void get_mem_map(void);
void get_cpu_freq(void);
void poll_errors(void);

void addr_tst2(void);
void addr_tst1(void);
void bit_fade(void);
void block_move(int iter);
void modtst(int offset, int iter, ulong p1, ulong p2);
void movinv1(int iter, ulong p1, ulong p2);
void movinv32(int iter, ulong p1, ulong lb, ulong hb, int sval, int off);
void movinvr(void);
unsigned int rand(void);
void rand_seed( unsigned int seed1, unsigned int seed2 );

void ad_err1(ulong *adr1, ulong *mask, ulong bad, ulong good);
void ad_err2(ulong *adr, ulong bad);
void ram_error(ulong *adr, ulong good, ulong bad);

#endif /* _RAM_TEST_H_ */

