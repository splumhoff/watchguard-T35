/*
 * drivers/char/hw_random/wg_x86-rng.c
 *
 * RNG driver for Intel Cavecreek and Cavium Nitrox crypto devices
 *
 * Author: John Borchek <john@sli.com>
 *
 * Copyright 2012 (c) Watchguard, Inc.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/vmalloc.h>
#include <linux/kallsyms.h>
#include <linux/hw_random.h>

#ifndef	CONFIG_CRASH_DUMP  // Only build for production kernels
#ifdef	CONFIG_WG_ARCH_X86 // Only build for X86

#include <net/xfrm.h>

#define	Handle		void*

#define	ASYNC		0  // Make operation asynchronous
#define	DEBUG		0  // Some debug code

// Keep a page of entropy on tap, refill when half full

#define	ENTROPY_SIZE	(PAGE_SIZE)
#define	ENTROPY_HALF	(ENTROPY_SIZE / 2)
#define	ENTROPY_ITEMS	(ENTROPY_SIZE / sizeof(u32))

// Abort macro with useful error reporting

#define	ABORT(err)	wg_x86_rng_abort(err, __FUNCTION__, __LINE__)

static	Handle	handle = NULL;	// Handle to Cavecreek instance

static	u32*    values = NULL;	// Pointer to entropy buffer

static	u32	rd = 0;		// Read/write pointers into entropy
static	u32	wr = 0;

static	u32	IOLength[2];	// Length of request

static	struct	Buffer {	// Cavecreek buffer
  u32	size;
  u8*	data;
}	IOBuffer[2];

// Externs for Cavecreek functions we need

extern	int       icp_sal_nrbgHealthTest(Handle handle, u32* failures);
extern	void*	  icp_sal_drbgGetEntropyInputFuncRegister(void*);

static  void* (*p_icp_sal_drbgGetEntropyInputFuncRegister)(void*) = NULL;

extern	int	cpaCyGetNumInstances(u32* instances);
extern	int	cpaCyGetInstances(int n, Handle* handle);
extern	int	cpaCyStartInstance(Handle handle);
extern	int	cpaCyNrbgGetEntropy( Handle handle, void* callback, int tag,
                                     u32* Len,      struct Buffer* Out);

static	int (*p_cpaCyNrbgGetEntropy)(Handle handle, void* callback, int tag,
                                     u32* Len,      struct Buffer* Out) = NULL;

static	void*	p_OrgGetEntropyFunc = NULL;

#if ASYNC

// Asynchronous completion function

void wg_x86_rng_done(int  toggle, int status,
                     u32* len,    struct Buffer* buf)
{
  if (status == 0) wr += (ENTROPY_ITEMS / 2);

#if DEBUG
  if (console_loglevel >= 9)
    printk(KERN_DEBUG "%s: %c Buf %p Data %p Size %4d/%-4d Status %d\n",
           __FUNCTION__, toggle ? 'B'  : 'T',
           buf, buf->data, *len, buf->size, status);
#endif
}
#else

// Set to NULL for synchronous operation

#define	wg_x86_rng_done	NULL
#endif

// Top/Bottom half toggle on the entropy buffer

static u32 toggle = 0;

// Wrapper for rdrand instruction

static noinline unsigned long rdrand(void)
{
#ifdef	CONFIG_64BIT
  // asm volatile(".byte 0x48"); 64 bit prefix
#endif
  asm volatile(".byte 0x0f; .byte 0xc7; .byte 0xf0"); // rdrand %eax
  asm volatile("ret"); // return before we clear %eax
  return 0;
}

// Pointer to rdrand function

static unsigned long (*p_rdrand)(void) = NULL;

// Get random via rdrand numbers

static int rdrand_get(void)
{
  if (p_rdrand)
  while ((wr - rd) < ENTROPY_ITEMS) {
    u32 data = (*p_rdrand)();
    if (!data) break;
    values[wr & (ENTROPY_ITEMS - 1)] = data;
    wr++;
  }

  return 0;
}

// Wrapper for Cavecreek entropy function

static int nrbgGetEntropy(void*  pCb,
                          void*  pCallbackTag,
                          u32*   pOpData,
                          struct Buffer* pBuffer,
                          u32*   pLengthReturned)
{
  int err = 0;

  if (console_loglevel >= 9)
    printk(KERN_DEBUG "%s:\n", __FUNCTION__);

  IOLength[toggle] = ENTROPY_HALF;

  if     (p_cpaCyNrbgGetEntropy)
  err = (*p_cpaCyNrbgGetEntropy)(handle, NULL, toggle,
                                 &IOLength[toggle],
                                 &IOBuffer[toggle]);
  return err;
}

// Get more entropy, fills on the empty half

int wg_x86_rng_get(void)
{
  int err = 0;

#if DEBUG
  static u8  fill;
  memset(IOBuffer[toggle].data, ++fill, ENTROPY_HALF);
  
  if (console_loglevel >= 9)
    printk(KERN_DEBUG "%s:  %c Buf %p Data %p Fill %02x\n",
           __FUNCTION__, toggle ? 'B'  : 'T',
           &IOBuffer[toggle], IOBuffer[toggle].data, fill);
#endif

  // Call rdrand instruction if available
  if (p_rdrand) return rdrand_get();

  // Get half a buffer at a time
  IOLength[toggle] = ENTROPY_HALF;

  // Call Cavecreek function if it exists
  if       (p_cpaCyNrbgGetEntropy) {
    if (console_loglevel >= 9)
      printk(KERN_DEBUG "%s: cpaCyNrbgGetEntropy %p\n",
             __FUNCTION__, p_cpaCyNrbgGetEntropy);
    err = (*p_cpaCyNrbgGetEntropy)(handle, wg_x86_rng_done, toggle,
                                   &IOLength[toggle], &IOBuffer[toggle]);
  }

  if (err)
    // Report errors
    printk(KERN_ERR "%s: Get Random Error %d\n", __FUNCTION__, err);
  else
  if (!wg_x86_rng_done)
    // Update the write pointer
    wr += (ENTROPY_ITEMS / 2);

  toggle ^= 1;

  return err;
}

// Return if we have entrpy left

int wg_x86_rng_data_present(struct hwrng *rng, int wait)
{
  int size = ((int)(wr - rd));

  if (size < 0)
    printk(KERN_EMERG "%s: Bad rd %u wr %u\n", __FUNCTION__, rd, wr);

  return (size > 0);
}

// Read 32 bits of entropy

int wg_x86_rng_data_read(struct hwrng *rng, u32 *buffer)
{
  int j = (rd++) & (ENTROPY_ITEMS - 1);

  // Get more if needed
  if ((j == 0) || (j == (ENTROPY_ITEMS / 2)))
    wg_x86_rng_get();

  *buffer = values[j];

  return 4;
}

// Data block to register this HW random supplier

struct hwrng wg_x86_rng_ops = {
  .name		= "wg_x86",
  .data_present	= wg_x86_rng_data_present,
  .data_read	= wg_x86_rng_data_read,
};

// Clean up function for errors, free resources

int wg_x86_rng_abort(int code, const char* func, int line)
{
  if (code)
  printk(KERN_EMERG "%s:%d Error %d\n", func, line, code);

  if (values) kfree(values); values = NULL;

  if (p_icp_sal_drbgGetEntropyInputFuncRegister)
    symbol_put(icp_sal_drbgGetEntropyInputFuncRegister);

  if (p_cpaCyNrbgGetEntropy) symbol_put(cpaCyNrbgGetEntropy);

  return code;
}

// Module init

int __init wg_x86_rng_init(void)
{
  int err = -ENOENT;

  // Clear Cavecreek function pointers
  int (*p_icp_sal_nrbgHealthTest)(Handle handle, u32* failures) = NULL;
  int (*p_cpaCyGetNumInstances)(u32* instances)                 = NULL;
  int (*p_cpaCyGetInstances)(int n, Handle* handle)             = NULL;
  int (*p_cpaCyStartInstance)(Handle handle)                    = NULL;

  // Useful herald
  printk(KERN_INFO "%s: Built " __DATE__ " " __TIME__ "\n", __FUNCTION__);

  // Get the primary entropy function
  p_cpaCyNrbgGetEntropy = (void*)symbol_get(cpaCyNrbgGetEntropy);
  if (p_cpaCyNrbgGetEntropy) {

    // We have a Cavecreek
    u32 Instances = 0;
    u32 Failures  = -ENOSPC;

    // Get Cavecreek function pointers
    p_icp_sal_drbgGetEntropyInputFuncRegister =
      (void*)symbol_get(icp_sal_drbgGetEntropyInputFuncRegister);
    if (!p_icp_sal_drbgGetEntropyInputFuncRegister) goto Fail;
    p_icp_sal_nrbgHealthTest = (void*)symbol_get(icp_sal_nrbgHealthTest);
    if (!p_icp_sal_nrbgHealthTest)  goto Fail;

    p_cpaCyGetNumInstances   = (void*)symbol_get(cpaCyGetNumInstances);
    if (!p_cpaCyGetNumInstances)    goto Fail;
    p_cpaCyGetInstances      = (void*)symbol_get(cpaCyGetInstances);
    if (!p_cpaCyGetInstances)       goto Fail;
    p_cpaCyStartInstance     = (void*)symbol_get(cpaCyStartInstance);
    if (!p_cpaCyStartInstance)      goto Fail;

    // Make sure we have some crypto instances
    err = (*p_cpaCyGetNumInstances)(&Instances);
    if (err) goto Fail;

    if (console_loglevel >= 9)
      printk(KERN_DEBUG "%s: Instances %2d\n", __FUNCTION__, Instances);

    if (!Instances) {
      err = -ENODEV;
      goto Fail;
    }

    // Get first instance
    err = (*p_cpaCyGetInstances)(1, &handle);
    if (err) goto Fail;

    // Start first instance
    err = (*p_cpaCyStartInstance)(handle);
    if (err) goto Fail;

    // Check RNG health
    err = (*p_icp_sal_nrbgHealthTest)(handle, &Failures);
    if (err) {
      if (wg_seattle == 0) goto Fail;
      p_rdrand = &rdrand;
      printk(KERN_ERR "%s: Using rdrand instruction\n", __FUNCTION__);
      err = Failures = 0;
    }

    if (Failures) {
      err = -ENOSPC;
      printk(KERN_ERR "%s: Health Test %d\n", __FUNCTION__, Failures);
    }

  Fail:
    if (err) {
      // Exit on errors
      if (p_icp_sal_nrbgHealthTest) symbol_put(icp_sal_nrbgHealthTest);
      if (p_cpaCyGetNumInstances)   symbol_put(cpaCyGetNumInstances);
      if (p_cpaCyGetInstances)      symbol_put(cpaCyGetInstances);
      if (p_cpaCyStartInstance)     symbol_put(cpaCyStartInstance);
    
      return ABORT(err);
    }

    // Hook entropy  function
    p_OrgGetEntropyFunc =
      (*p_icp_sal_drbgGetEntropyInputFuncRegister)(nrbgGetEntropy);

    if (p_OrgGetEntropyFunc)
    if (console_loglevel >= 9)
      printk(KERN_DEBUG "%s: Original Get Entry Function %p\n",
             __FUNCTION__, p_OrgGetEntropyFunc);
  }
  else {
    // Didn't find any HW random source
    printk(KERN_EMERG "%s: No HW random devices\n", __FUNCTION__);
    return err;
  }

  // Get DMA memory for random numbers, Cavecreek NEEDS GFP_DMA
  values = kmalloc(ENTROPY_SIZE, GFP_KERNEL |  GFP_DMA);
  if (!values) return ABORT(-ENOMEM);

  // Set up top/bottom half pointers
  IOBuffer[0].size = ENTROPY_HALF;
  IOBuffer[0].data = ((u8*)values);

  IOBuffer[1].size = ENTROPY_HALF;;
  IOBuffer[1].data = ((u8*)values) + ENTROPY_HALF;;

  // Get some initial data
  err = wg_x86_rng_get();
  if (err) return ABORT(err);

  // Register this source of randomness
  err = hwrng_register(&wg_x86_rng_ops);
  if (err) return ABORT(err);

  // All good
  return 0;
}

void __exit wg_x86_rng_exit(void)
{
  // Unhook entropy function
  if (p_icp_sal_drbgGetEntropyInputFuncRegister)
    (*p_icp_sal_drbgGetEntropyInputFuncRegister)(p_OrgGetEntropyFunc);

  // Cleanup
  ABORT(0);

  // Unregister us as a source of randomness
  hwrng_unregister(&wg_x86_rng_ops);
}

module_init(wg_x86_rng_init);
module_exit(wg_x86_rng_exit);

MODULE_AUTHOR("John Borchek <john@sli.com>");
MODULE_DESCRIPTION("H/W Random Number Generator (RNG) driver for Cavecreek");
MODULE_LICENSE("GPL");

#endif	// CONFIG_WG_ARCH_X86
#endif	// CONFIG_CRASH_DUMP
