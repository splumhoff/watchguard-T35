/* Copyright 2008-2011 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bman_test.h"
#include "bman_private.h"

/*************/
/* constants */
/*************/

#define PORTAL_OPAQUE	((void *)0xf00dbeef)
#define POOL_OPAQUE	((void *)0xdeadabba)
#define NUM_BUFS	93
#define LOOPS		3
#define BMAN_TOKEN_MASK 0x00FFFFFFFFFFLLU

/***************/
/* global vars */
/***************/

static struct bman_pool *pool;
static int depleted;
static struct bm_buffer bufs_in[NUM_BUFS] ____cacheline_aligned;
static struct bm_buffer bufs_out[NUM_BUFS] ____cacheline_aligned;
static int bufs_received;

/* Predeclare the callback so we can instantiate pool parameters */
static void depletion_cb(struct bman_portal *, struct bman_pool *, void *, int);

/**********************/
/* internal functions */
/**********************/

static void bufs_init(void)
{
	int i;
	for (i = 0; i < NUM_BUFS; i++)
		bm_buffer_set64(&bufs_in[i], 0xfedc01234567LLU * i);
	bufs_received = 0;
}

static inline int bufs_cmp(const struct bm_buffer *a, const struct bm_buffer *b)
{
	if ((bman_ip_rev == BMAN_REV20) || (bman_ip_rev == BMAN_REV21)) {

		/* On SoCs with Bman revison 2.0, Bman only respects the 40
		 * LS-bits of buffer addresses, masking off the upper 8-bits on
		 * release commands. The API provides for 48-bit addresses
		 * because some SoCs support all 48-bits. When generating
		 * garbage addresses for testing, we either need to zero the
		 * upper 8-bits when releasing to Bman (otherwise we'll be
		 * disappointed when the buffers we acquire back from Bman
		 * don't match), or we need to mask the upper 8-bits off when
		 * comparing. We do the latter.
		 */
		if ((bm_buffer_get64(a) & BMAN_TOKEN_MASK)
				< (bm_buffer_get64(b) & BMAN_TOKEN_MASK))
			return -1;
		if ((bm_buffer_get64(a) & BMAN_TOKEN_MASK)
				> (bm_buffer_get64(b) & BMAN_TOKEN_MASK))
			return 1;
	} else {
		if (bm_buffer_get64(a) < bm_buffer_get64(b))
			return -1;
		if (bm_buffer_get64(a) > bm_buffer_get64(b))
			return 1;
	}

	return 0;
}

static void bufs_confirm(void)
{
	int i, j;
	for (i = 0; i < NUM_BUFS; i++) {
		int matches = 0;
		for (j = 0; j < NUM_BUFS; j++)
			if (!bufs_cmp(&bufs_in[i], &bufs_out[j]))
				matches++;
		BUG_ON(matches != 1);
	}
}

/********/
/* test */
/********/

static void depletion_cb(struct bman_portal *__portal, struct bman_pool *__pool,
			void *pool_ctx, int __depleted)
{
	BUG_ON(__pool != pool);
	BUG_ON(pool_ctx != POOL_OPAQUE);
	depleted = __depleted;
}

void bman_test_high(void)
{
	struct bman_pool_params pparams = {
		.flags = BMAN_POOL_FLAG_DEPLETION | BMAN_POOL_FLAG_DYNAMIC_BPID,
		.cb = depletion_cb,
		.cb_ctx = POOL_OPAQUE,
	};
	int i, loops = LOOPS;

	bufs_init();

	pr_info("BMAN:  --- starting high-level test ---\n");

	pool = bman_new_pool(&pparams);
	BUG_ON(!pool);

	/*******************/
	/* Release buffers */
	/*******************/
do_loop:
	i = 0;
	while (i < NUM_BUFS) {
		u32 flags = BMAN_RELEASE_FLAG_WAIT;
		int num = 8;
		if ((i + num) > NUM_BUFS)
			num = NUM_BUFS - i;
		if ((i + num) == NUM_BUFS)
			flags |= BMAN_RELEASE_FLAG_WAIT_SYNC;
		if (bman_release(pool, bufs_in + i, num, flags))
			panic("bman_release() failed\n");
		i += num;
	}

	/*******************/
	/* Acquire buffers */
	/*******************/
	while (i > 0) {
		int tmp, num = 8;
		if (num > i)
			num = i;
		tmp = bman_acquire(pool, bufs_out + i - num, num, 0);
		BUG_ON(tmp != num);
		i -= num;
	}
	i = bman_acquire(pool, NULL, 1, 0);
	BUG_ON(i > 0);

	bufs_confirm();

	if (--loops)
		goto do_loop;

	/************/
	/* Clean up */
	/************/
	bman_free_pool(pool);
	pr_info("BMAN:  --- finished high-level test ---\n");
}
