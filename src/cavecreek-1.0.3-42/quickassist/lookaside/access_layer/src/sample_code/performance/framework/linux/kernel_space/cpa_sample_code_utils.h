/******************************************************************************
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
 *****************************************************************************/

/**
*****************************************************************************
 * @file cpa_sample_code_utils.h
 *
 * This file provides linux kernel os abstraction functions
 *
 *****************************************************************************/
#ifndef _KERNEL_SPACE_SAMPLECODEUTILS_H__
#define _KERNEL_SPACE_SAMPLECODEUTILS_H__

#include "cpa.h"
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/module.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,2,0))
#include <linux/vmalloc.h>
#endif

/*these are defined to allow an application compile in user space or kernel
 * space, in kernel space these funtions are not not needed*/
#define sample_code_thread_mutex_init(mutex_ptr)
#define sample_code_thread_mutex_destroy(mutex_ptr)
#define sample_code_thread_cond_init(condPtr)
#define sample_code_thread_cond_broadcast(condPtr)
#define sample_code_thread_cond_destroy(cPtr)
typedef Cpa32U sample_code_thread_barrier_t;
/*there is no such thing as threadExit in kernel space,
 * we provide this macro so that our code
 * builds in kernel space and user space*/
#define sampleCodeThreadExit() return




#define sample_code_thread_mutex_t Cpa32U
#define sample_code_thread_cond_t Cpa32U
typedef struct semaphore* sample_code_semaphore_t;
typedef struct task_struct *sample_code_thread_t;




#define PRINT(args...)              \
{                                   \
    printk(args);                   \
}

#endif
