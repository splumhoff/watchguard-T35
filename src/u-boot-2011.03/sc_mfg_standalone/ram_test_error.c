/* error.c - change from MemTest-86 Version 3.4
 *
 * Released under version 2 of the Gnu Public License.
 * By Tony Tang
 */

/*===============================*/

#include "ram_test.h"

extern int bail;
extern struct tseq_t tseq[];
extern void poll_errors(void);

static void update_err_counts(void);
static void print_err_counts(void);
static void common_err(ulong *adr, ulong good, ulong bad, ulong xor, int type);
static int syn, chan, len=1;

/*
 * Display data error message. Don't display duplicate errors.
 */
void ram_error(ulong *adr, ulong good, ulong bad)
{
	ulong xor;
 	diag_printf("%s %d called: address %08lx, good %08lx, bad %08lx \n",
			__func__, __LINE__, (ulong)adr, good, bad);

	xor = good ^ bad;

	common_err(adr, good, bad, xor, 0);
}

/*
 * Display address error message.
 * Since this is strictly an address test, trying to create BadRAM
 * patterns does not make sense. Just report the error.
 */
void ad_err1(ulong *adr1, ulong *mask, ulong bad, ulong good)
{
	common_err(adr1, good, bad, (ulong)mask, 1);
}

/*
 * Display address error message.
 * Since this type of address error can also report data errors go
 * ahead and generate BadRAM patterns.
 */
void ad_err2(ulong *adr, ulong bad)
{
	common_err(adr, (ulong)adr, bad, ((ulong)adr) ^ bad, 0);
}

static void update_err_counts(void)
{
	if (v->pass && v->ecount == 0)
	{
		diag_printf("TEST PASS and ERROR COUNT = 0\n");
	}
	++(v->ecount);
	tseq[v->test].errors++;
		
}

static void print_err_counts(void)
{
}

unsigned int err_address = 0;
int err_test_type = -1;
char errbitmask = 0;

void ramtest_find_err_pos(ulong good, ulong bad)
{
	ulong xor;
	ulong i= 0;
	diag_printf("%s %d called: good %08lx, bad %08lx \n",
			__func__, __LINE__, good, bad);
	xor = good ^ bad;
	while(i<4)
	{
		if( (xor>>(i*8)) && 0xFF)
 			errbitmask |= (1<<i);
		i++;
 	}
	diag_printf("%s %d called: errbitmask %x \n",
			__func__, __LINE__, errbitmask);
}

/*
 * Print an individual error
 */
void common_err(ulong *adr, ulong good, ulong bad, ulong xor, int type)
{
	unsigned int i, n, flag=0;

	update_err_counts();

	diag_printf("%s %d called: address %08lx, good %08lx, bad %08lx \n",
			__func__, __LINE__, (ulong)adr, good, bad);
	err_test_type = type;
	err_address = (ulong)adr;
	ramtest_find_err_pos(good, bad);

	{	/* Don't do anything for a parity error. */
		if (type == 3)
		{
			diag_printf("Parity Error - do nothing then return!\n");
			return;
		}
		/* Address error */
		if (type == 1)
		{
			diag_printf("Address ERROR.\n");
			xor = good ^ bad;
		}
		/* Ecc correctable errors */
		if (type == 2)
		{
			/* the bad value is the corrected flag */
			if (bad)
			{
				v->erri.cor_err++;
			}
		}
		else /* 0 */
		{
			/* nothing */
		}
		bail ++;
			
		/* Calc upper and lower error addresses */
			/* removed now */

		/* Calc bits in error */
		for (i=0, n=0; i<32; i++)
		{
			if (xor>>i & 1)
			{
				n++;
			}
		}
		v->erri.tbits += n;
		if (n > v->erri.max_bits)
		{
			v->erri.max_bits = n;
			flag++;
		}
		if (n < v->erri.min_bits)
		{
			v->erri.min_bits = n;
			flag++;
		}
		if (v->erri.ebits ^ xor)
		{
			flag++;
		}
		v->erri.ebits |= xor;

	 	/* Calc max contig errors */
		len = 1;
		if ((ulong)adr == (ulong)v->erri.eadr+4 || (ulong)adr == (ulong)v->erri.eadr-4 )
		{
			len++;
		}
		if (len > v->erri.maxl)
		{
			v->erri.maxl = len;
			flag++;
		}
		v->erri.eadr = (ulong)adr;
		
		{
			diag_printf("ERROR Information: \n");
			diag_printf("Current Detect Error On Address: %08lx. bad data %08lx\n", v->erri.eadr, bad);	
		}

		if (flag)
		{
			/* Calc bits in error */
			for (i=0, n=0; i<32; i++)
		 	{
				if (v->erri.ebits>>i & 1)
				{
					n++;
				}
			}
			
			for (i=0; tseq[i].msg != NULL; i++)
			{
				diag_printf("Each kind of test errors : %d - %08x.\n", i, tseq[i].errors);
				diag_printf("Current test num %d : %s\n", v->test, tseq[v->test].msg);
			}
		}
		if (v->erri.cor_err)
		{
			/* ECC enable - this dose make sense */
		}
	}
}

/*
 * Print an ecc error
 */
void print_ecc_err(unsigned long page, unsigned long offset,
	int corrected, unsigned short syndrome, int channel)
{
	++(v->ecc_ecount);
	syn = syndrome;
	chan = channel;
	common_err((ulong *)page, offset, corrected, 0, 2);
}

#ifdef PARITY_MEM
/*
 * Print a parity error message
 */
void parity_err( unsigned long edi, unsigned long esi)
{
	unsigned long addr;

	if (v->test == 5) {
		addr = esi;
	} else {
		addr = edi;
	}
	common_err((ulong *)addr, addr & 0xFFF, 0, 0, 3);
}
#endif

	
/*
 * Show progress by displaying elapsed time and update bar graphs
 */
void do_tick(void)
{
	/*
	int i, n, pct;
	ulong h, l, t;
	*/

	/* FIXME only print serial error messages from the tick handler */
	if (v->ecount)
	{
		print_err_counts();
	}

	/* Poll for ECC errors */
	poll_errors();
}

