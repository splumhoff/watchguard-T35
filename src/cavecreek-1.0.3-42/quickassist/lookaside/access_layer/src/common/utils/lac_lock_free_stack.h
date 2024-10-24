#ifndef LAC_LOCK_FREE_STACK_H_1
#define LAC_LOCK_FREE_STACK_H_1

/*******************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *  version: QAT1.7.Upstream.L.1.0.3-42
 *
 * @lac_lock_free_stack.h
 *
 * This file provides a lock-free stack implementation.
 * There is an assumption that effective virtual address size is 48-bit only,
 * which is true for Linux user space applications in 32/64-bit modes.
 *
 ******************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lac_mem_pools.h"

typedef union {
    struct {
        uint64_t    ctr:16;
        uint64_t    ptr:48;
    };
    uint64_t    atomic;
} pointer_t;

typedef struct {
    volatile pointer_t top;
} lock_free_stack_t;

#define PTR(x) ((void *) (uintptr_t) (x))

static inline lac_mem_blk_t * pop(lock_free_stack_t * stack)
{
    pointer_t old_top;
    pointer_t new_top;
    lac_mem_blk_t * next;

    do {
        old_top.atomic = stack->top.atomic;
        next = PTR(old_top.ptr);
        if (NULL == next)
            return next;

        new_top.ptr = (uintptr_t) next->pNext;
        new_top.ctr = old_top.ctr + 1;
    } while (!__sync_bool_compare_and_swap(&stack->top.atomic, old_top.atomic, new_top.atomic));

    return next;
}

static inline void push(lock_free_stack_t * stack, lac_mem_blk_t * val)
{
    pointer_t new_top;
    pointer_t old_top;

    do {
        old_top.atomic = stack->top.atomic;
        val->pNext = PTR(old_top.ptr);
        new_top.ptr = (uintptr_t) val;
        new_top.ctr = old_top.ctr + 1;
    } while (!__sync_bool_compare_and_swap(&stack->top.atomic, old_top.atomic, new_top.atomic));
}

static inline lock_free_stack_t init_stack(void)
{
    lock_free_stack_t stack = {{{0}}};
    return stack;
}

static inline lac_mem_blk_t * top(lock_free_stack_t * stack)
{
    pointer_t old_top = stack->top;
    lac_mem_blk_t * next = PTR(old_top.ptr);
    return next;
}

#endif
