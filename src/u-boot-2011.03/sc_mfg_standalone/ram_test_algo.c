/* test_algo.c - change from MemTest-86
 * tony tang
 */
#include "ram_test.h"

extern int bail;
extern volatile ulong *p;
extern ulong p1, p2;
extern struct tseq_t tseq[];
extern void poll_errors(void);

int ecount = 0;

static inline ulong roundup_x(ulong value, ulong mask)
{
	return (value + mask) & ~mask;
}
/*
 * Memory address test, walking ones
 */
void addr_tst1()
{
	int i, j, k;
	volatile ulong *pt;
	volatile ulong *end;
	ulong bad, mask, bank;

	//diag_printf("v->map.start %08x\n", (ulong)(v->map.start));
	/* Test the global address bits */
	for (p1=0, j=0; j<2; j++)
	{
		/* Set pattern in our lowest multiple of 0x20000 */
		p = (ulong *)roundup_x((ulong)(v->map.start), 0x1ffff);
		*p = p1;
	
		/* Now write pattern compliment */
		p1 = ~p1;
		end = (ulong *)v->map.end;
		for (i=0; i<1000; i++)
		{
			mask = 4;
			do
			{
				pt = (ulong *)((ulong)p | mask);
				if (pt == p)
				{
					mask = mask << 1;
					continue;
				}
				if (pt >= end)
				{
					break;
				}
				*pt = p1;
				if ((bad = *p) != ~p1)
				{
					diag_printf("excepted data : %08lx.\n", ~p1);
					ad_err1((ulong *)p, (ulong *)mask, bad, ~p1);
					i = 1000;
				}
				mask = mask << 1;
			} while(mask);
		}
		//do_tick();
		BAILR
	}
#if 0 /* TONY debug for init pattern */
		{
			int iiiii = 0;
			volatile ulong *ptr = (ulong *)v->map.start;
			for(;iiiii<256; iiiii++)
			{
				diag_printf("%08x\t", ptr[iiiii]);

				if(!((iiiii+1)%7))
					diag_printf("\n");
			}
		}
#endif
	/* Now check the address bits in each bank */
	/* If we have more than 8mb of memory then the bank size must be */
	/* bigger than 256k. If so use 1mb for the bank size. */
#if 1
	if ((v->map.end - v->map.start) > (0x800000))
	{
		bank = 0x100000;
	}
	else
	{
		bank = 0x40000;
	}
#else /* set default bank size now */
	bank = 0x40000;
#endif
	for (p1 = 0, k = 0; k<2; k++)
	{
		{
			p = (ulong *)v->map.start;
			/* Force start address to be a multiple of 256k */
			p = (ulong *)roundup_x((ulong)p, bank - 1);
			end = (ulong *)v->map.end;
			while (p < end)
			{

				*p = p1;

				p1 = ~p1;
				for (i=0; i<200; i++)
				{
					mask = 4;
					do{
						pt = (ulong *)((ulong)p | mask);
						if (pt == p)
						{
							mask = mask << 1;
							continue;
						}
						if (pt >= end)
						{
							break;
						}
						*pt = p1;
						if ((bad = *p) != ~p1)
						{
							ad_err1((ulong *)p,(ulong *)mask,bad,~p1);
							i = 200;
						}
						mask = mask << 1;
					} while(mask);
				}
				if (p + bank > p)
				{
					p += bank;
				}
				else
				{
					p = end;
				}
				p1 = ~p1;
			}
		}		
		//do_tick();
		BAILR
		p1 = ~p1;
	}
}

/*
 * Memory address test, own address
 */
void addr_tst2()
{
	int done;
	volatile ulong *pe;
	volatile ulong *end, *start;
	ulong bad;

	/* Write each address with it's own address */
	{
		start = (ulong *)v->map.start;
		end = (ulong *)v->map.end;
		pe = (ulong *)start;
		p = start;
		done = 0;
		do
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe)
			{
				pe += SPINSZ;
			}
			else
			{
				pe = end;
			}
			if (pe >= end)
			{
				pe = end;
				done++;
			} 	
			if (p == pe )
			{
				break;
			}
			for (; p < pe; p++)
 			{
 				*p = (ulong)p;
 			}
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Each address should have its own address */
	{
		start = (ulong *)v->map.start;
		end = (ulong *)v->map.end;
		pe = (ulong *)start;
		p = start;
		done = 0;
		do {
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p++)
 			{
 				if((bad = *p) != (ulong)p)
 				{
					diag_printf("excepted data : %08lx.\n", *p);
 					ad_err2((ulong *)p, bad);
 				}
 			}
			//do_tick();
			BAILR
		} while (!done);
	}
}

/*
 * Test all of memory using a "half moving inversions" algorithm using random
 * numbers and their complment as the data pattern. Since we are not able to
 * produce random numbers in reverse order testing is only done in the forward
 * direction.
 */
static int random_run = 0;
void movinvr()
{
	int i, done, seed1, seed2;
	volatile ulong *pe;
	volatile ulong *start,*end;
	ulong num;
	ulong bad;

	random_run ++;
	/* Initialize memory with initial sequence of random numbers. */
	{
#if 0
		seed1 = 521288629 + v->pass;
		seed2 = 362436069 - v->pass;
#else
		seed1 = 521288629 + random_run;
		seed2 = 362436069 - random_run;
#endif
	}

	/* Display the current seed */
	diag_printf("Current Seed :%08x .\n", seed1);

	rand_seed(seed1, seed2);
	
	{
		start = (ulong *)v->map.start;
		end = (ulong *)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			for (; p < pe; p++)
			{
				*p = rand();
			}
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down. */
	for (i=0; i<2; i++)
	{
		rand_seed(seed1, seed2);
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			pe = start;
			p = start;
			done = 0;
			do
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
				for (; p < pe; p++)
				{
					num = rand();
					if (i)
					{
						num = ~num;
					}
					if ((bad=*p) != num)
					{
						diag_printf("excepted data : %08lx.\n", num);
						ram_error((ulong*)p, num, bad);
					}
					*p = ~num;
				}
				//do_tick();
				BAILR
			} while (!done);
		}
	}
}

/*
 * Test all of memory using a "moving inversions" algorithm using the
 * pattern in p1 and it's complement in p2. parameter iter is times to be executed
 */
void movinv1(int iter, ulong p1, ulong p2)
{
	int i, done;
	volatile ulong *pe;
	//volatile ulong len;
	volatile ulong *start,*end;
	ulong bad;

	/* Display the current pattern */
	diag_printf("Current Pattern %08lx .\n", p1);

	/* Initialize memory with the initial pattern. */
	{
		start = (ulong *)v->map.start;
		end = (ulong *)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do
		{
#if 0 /* tony */
			printf("pe %x, p %x \n", pe, p);
#endif			
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			//len = pe - p;
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p++)
 			{
#if 0 /* tony */
				printf("p %x\t", p);
#endif 				
 				*p = p1;
 			}
 			//do_tick();
			BAILR
		} while (!done);
	}
#if 0 /* tony */
			printf("pe %x, p %x \t", pe, p);
#endif	
	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down. */
	for (i=0; i<iter; i++)
	{
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			pe = start;
			p = start;
			done = 0;
			do
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				for (; p < pe; p++)
 				{
 					if ((bad=*p) != p1)
 					{
						diag_printf("excepted data p1: %08lx.\n", p1);
 						ram_error((ulong*)p, p1, bad);
 					}
 					*p = p2;
 				}
				//do_tick();
				BAILR
			} while (!done);
		}
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			pe = end -1;
			p = end - 1;
				
			done = 0;
			do
			{
#if 0 /* tony */
			printf("pe %x, p %x \t", pe, p);
#endif				
			/* Check for underflow */
				if (pe - SPINSZ < pe) {
					pe -= SPINSZ;
				} else {
					pe = start;
				}
				if (pe <= start) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				do
 				{
 					if ((bad=*p) != p2)
 					{
						diag_printf("excepted data p2: %08lx.\n", p2);
 						ram_error((ulong*)p, p2, bad);
 					}
 					*p = p1;
 				} while (p-- > pe);
				//do_tick();
				BAILR
			} while (!done);
		}
	}
}

void movinv32(int iter, ulong p1, ulong lb, ulong hb, int sval, int off)
{
	int i, k=0, done;
	volatile ulong *pe;
	volatile ulong *start, *end;
	ulong pat = 0;
	ulong bad;

	/* Display the current pattern */
	diag_printf("Current Pattern %08lx .\n", p1);

	/* Initialize memory with the initial pattern. */
	{
		start = (ulong *)v->map.start;
		end = (ulong *)v->map.end;
		pe = start;
		p = start;
		done = 0;
		k = off;
		pat = p1;
		do
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			while (p < pe)
 			{
 				*p = pat;
 				if (++k >= 32)
 				{
 					pat = lb;
 					k = 0;
 				}
 				else
 				{
 					pat = pat << 1;
 					pat |= sval;
 				}
 				p++;
 			}
			/* Do a SPINSZ section of memory */
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down. */
	for (i=0; i<iter; i++)
	{
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			pe = start;
			p = start;
			done = 0;
			k = off;
			pat = p1;
			do
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
				while (p < pe)
				{
 					if ((bad=*p) != pat)
 					{
						diag_printf("excepted data : %08lx.\n", pat);
						ram_error((ulong*)p, pat, bad);
					}
 					*p = ~pat;
 					if (++k >= 32)
 					{
 						pat = lb;
 						k = 0;
 					}
 					else
 					{
 						pat = pat << 1;
 						pat |= sval;
 					}
 					p++;
 				}
				//do_tick();
				BAILR
			} while (!done);
		}

		/* Since we already adjusted k and the pattern this
		 * code backs both up one step
		 */
		pat = lb;
 		if ( 0 != (k = (k-1) & 31) )
 		{
 			pat = (pat << k);
 			if ( sval )
 				pat |= ((sval << k) - 1);
 		}
 		k++;
#if 0 /* for tony debug use */
 		diag_printf("pat now %08x, k now %08x\n", pat, k);
/* CDH start */
		{
			int iiiii = 0;
			volatile ulong *ptr = ((ulong *)v->map.end) - 20);
			for(;iiiii<20; iiiii++)
			{
				diag_printf("%08x\n", ptr[iiiii]);
			}
		}
#endif
/* CDH end */
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			p = end -1;
			pe = end -1;
			done = 0;
			do
			{
				/* Check for underflow */
				if (pe - SPINSZ < pe) {
					pe -= SPINSZ;
				} else {
					pe = start;
				}
				if (pe <= start) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				do
 				{
 					if ((bad=*p) != ~pat)
 					{
						diag_printf("excepted data : %08lx.\n", ~pat);
 						ram_error((ulong*)p, ~pat, bad);
 					}
 					*p = pat;
 					if (--k <= 0)
 					{
 						pat = hb;
 						k = 32;
 					}
 					else
 					{
 						pat = pat >> 1;
 						pat |= (sval<<31);	// org : p3 -- ??????????? -- this is decided by TONY TANG
 					}
 				} while (p-- > pe);
				//do_tick();
				BAILR
			} while (!done);
		}
	}
}

/*
 * Test all of memory using modulo X access pattern.
 */
void modtst(int offset, int iter, ulong p1, ulong p2)
{
	int k, l, done;
	volatile ulong *pe;
	volatile ulong *start, *end;
	ulong bad;

	/* Display the current pattern */
	diag_printf("Modelo X test using pattern %08lx with offset %08x.\n", p1, offset);

	/* Write every nth location with pattern */
	{
		start = (ulong *)v->map.start;
		end = (ulong *)v->map.end;
		pe = (ulong *)start;
		p = start + offset;
		done = 0;
		do
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p += MOD_SZ)
 			{
 				*p = p1;
 			}
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Write the rest of memory "iter" times with the pattern complement */
	for (l=0; l<iter; l++)
	{
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			pe = (ulong *)start;
			p = start;
			done = 0;
			k = 0;
			do
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				for (; p < pe; p++)
 				{
 					if (k != offset)
 					{
 						*p = p2;
 					}
 					if (++k > MOD_SZ-1)
 					{
 						k = 0;
 					}
 				}
				//do_tick();
				BAILR
			} while (!done);
		}
	}

	/* Now check every nth location */
	{
		start = (ulong *)v->map.start;
		end = (ulong *)v->map.end;
		pe = (ulong *)start;
		p = start+offset;
		done = 0;
		do
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p += MOD_SZ)
 			{
 				if ((bad=*p) != p1)
 				{
					diag_printf("excepted data : %08lx.\n", p1);
 					ram_error((ulong*)p, p1, bad);
 				}
 			}
			//do_tick();
			BAILR
		} while (!done);
	}
	diag_printf("\n");
}

/*
 * Test memory using block moves
 * Adapted from Robert Redelmeier's burnBX test
 */
void block_move(int iter)
{
#if 1
	int i, done;
	ulong len;
	volatile ulong p, pe, pp;
	volatile ulong start, end;

	ulong p1 = 1;

	/* Initialize memory with the initial pattern. */
	{
		start = (ulong)v->map.start;
		end = (ulong)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do
		{
			/* Check for overflow */
			if (pe + SPINSZ*4 > pe)
			{
				pe += SPINSZ*4;
			}
			else
			{
				pe = end;
			}
			if (pe >= end)
			{
				pe = end;
				done++;
			}
			if (p == pe )
			{
				break;
			}
			len = ((ulong)pe - (ulong)p) / 64;
			
			do
			{
				int off = 0;
				ulong p2 = ~p1;
				volatile ulong *ptr = (ulong *)p;
								
				for(;off<16;off++)
				{
					if((off==4) || (off==5) ||(off==10)||(off==11))
						*(ptr+off) = p2;
					else
						*(ptr+off) = p1;
				}
				p += 64;
				/****/// 带进位的循环左移！！！
				p1 = p1 >> 1;
				if(!p1)
				{
					p1 |= 0x80000000;
				}
				/****/			
			}while(--len);
#if 0 /* TONY debug for init pattern */
		{
			int iiiii = 0;
			volatile ulong *ptr = (v->map.start);
			for(;iiiii<128; iiiii++)
			{
				if(!((iiiii+1)%7))
					diag_printf("\n");
				diag_printf("%08x\t", ptr[iiiii]);
			}
		}
#endif
			//do_tick();
			BAILR
		} while (!done);
	}
	//diag_printf("pattern filled in\n");
	/* Now move the data around
	 * First move the data up half of the segment size we are testing
	 * Then move the data to the original location + 32 bytes
	 */
	{
		start = (ulong)v->map.start;
		end = (ulong)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do {
			/* Check for overflow */
			if (pe + SPINSZ*4 > pe) {
				pe += SPINSZ*4;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			pp = p + ((pe - p) / 2);
			len = ((ulong)pe - (ulong)p) / 8;
			for(i=0; i<iter; i++)
			{
				ulong *src = (ulong *)p;
				ulong *dst = (ulong *)pp;
				ulong tmp = p;
				ulong len_tmp = len;
				do
				{					
					*dst = *src;
					src += 1;
					dst += 1;										
				}while(--len_tmp);
				
				tmp = tmp + 32;
				dst = (ulong *)tmp;
				src = (ulong *)pp;
				len_tmp = len - 8;
				
				do
				{
					*dst = *src;
					src += 1;
					dst += 1;
				}while(--len_tmp);
				
				dst = (ulong *)p;
				len_tmp = 8;
				do
				{
					*dst = *src;
					src += 1;
					dst += 1;
				}while(--len_tmp);
				//do_tick();
				BAILR
			}
			p = pe;
		} while (!done);
	}
	//diag_printf("moves finished\n");
	/* Now check the data
	 * The error checking is rather crude. We just check that the
	 * adjacent words are the same.
	 */
	{
		start = (ulong)v->map.start;

		end = (ulong)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do {
			/* Check for overflow */
			if (pe + SPINSZ*4 > pe) {
				pe += SPINSZ*4;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			
			do
			{
				ulong tmp;
				ulong *tmp1, *tmp2;
				tmp = p;
				tmp1 = (ulong *)tmp;
				tmp = tmp + 4;
				tmp2 = (ulong *)tmp;			
				
				if(*tmp1 != *tmp2)
				{
					diag_printf("Block Move has error!!!\n");
					ram_error((ulong *)tmp, *tmp1, *tmp2);
				}
				
				p = p + 8;				
			}while(p < pe);
			
			BAILR
		} while (!done);
	}
#else
	diag_printf("block_move - dummy now.\n");
#endif
}

/*
 * Test memory for bit fade.
 */
#define STIME 5400
void bit_fade()
{
	volatile ulong *pe;
	volatile ulong bad;
	volatile ulong *start,*end;


	/* Do -1 and 0 patterns */
	p1 = 0;
	while (1)
	{
		/* Display the current pattern */

		/* Initialize memory with the initial pattern. */
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			pe = start;
			p = start;
			for (p=start; p<end; p++)
			{
				*p = p1;
			}
			//do_tick();
			BAILR
		}
		/* Snooze for 90 minutes */
		sleep (STIME); ///?????

		/* Make sure that nothing changed while sleeping */
		{
			start = (ulong *)v->map.start;
			end = (ulong *)v->map.end;
			pe = start;
			p = start;
			for (p=start; p<end; p++)
			{
 				if ((bad=*p) != p1)
 				{
					diag_printf("excepted data : %08lx.\n", p1);
					ram_error((ulong*)p, p1, bad);
				}
			}
			//do_tick();
			BAILR
		}
		if (p1 == 0)
		{
			p1=-1;
		}
		else
		{
			break;
		}
	}
}

