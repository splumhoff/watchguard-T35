
/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

/*****************************************************************************
 * @file qae_mem_utils.c
 *
 * This file provides linux kernel memory allocation for quick assist API
 *
 *****************************************************************************/

#include "qae_mem_utils.h"
#include "qat_mem/qat_mem.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>

#define DEBUG(...)
//#define DEBUG(...) printf(__VA_ARGS__)
#define ERROR(...) printf(__VA_ARGS__)

/* flag for mutex lock */
static int inited = 0;

/* qat_mem ioctl open file descriptor */
static int qat_memfd = -1;

/* Big Slab Allocator Lock */
static pthread_mutex_t bsal = PTHREAD_MUTEX_INITIALIZER;   

/*  We allocate memory in slabs consisting of a number of slots
 *  to avoid fragmentation and also to reduce cost of allocation
 *
 *  There are six predefined slot sizes, 128 byte, 4096 byte,
 *  128KB, 256KB, 4096KB and 8192KB  Slabs are 128KB in size.  
 *  This implies the most slots that a slab can hold is 128KB/256 = 512.  
 *  The first slot is used for meta info, so actual is 511.
 */
#define SLAB_SIZE 	(0x20000 - sizeof(qat_mem_config))

/* slot free signature */
#define SIG_FREE        0xF1F2F3F4

/* slot allocate signatture */
#define SIG_ALLOC       0xA1A2A3A4

/* maxmium slot size */
#define MAX_ALLOC	(SLAB_SIZE - sizeof (qae_slab))

static int slot_sizes_available[] =
{
    0x0100,
    0x0400,
    0x1000,
    0x2000,
    0x4000,
    0x8000
};

typedef struct _qae_slot
{
    struct _qae_slot *next;
    int sig;
    char *file;
    int line;
}
qae_slot;

typedef struct _qae_slab
{
    qat_mem_config memCfg;
    int slot_size;
    int sig;
    struct _qae_slab *next_slab;
    struct _qae_slot *next_slot;
}
qae_slab;

qae_slab *slab_list;

static void init(void);

/*****************************************************************************
 * function:
 *         find_slab_slot(void *ptr)
 *
 * @param[in] ptr, pointer to a slot in a slab
 *
 * @description
 * 	find the slab that contains the slot of memory ptr
 *	retval pointer to the slab
 *
 *****************************************************************************/
static qae_slab *find_slab_slot(void *ptr)
{
    qae_slab *slb;
    qae_slab *result = NULL;

    /* We don't need to lock around access to the list for read purposes
       because we know new entries are always added to the head of the list,
       and entries are only ever removed when shutting down */
    DEBUG("%s: pthread_mutex_lock\n", __func__);
    for (slb = slab_list; slb != NULL; slb = slb->next_slab)
    {
        unsigned char *end = (unsigned char *)slb + SLAB_SIZE;

        if (ptr >= (void *)slb && ptr < (void *)end)
        {
            result = slb;
            goto exit;
        }
    }

  exit:
    DEBUG("%s: pthread_mutex_unlock\n", __func__);
    return result;
}

/*****************************************************************************
 * function:
 *         create_slab(int size)
 *
 * @param[in] size, the size of the slots within the slab. Note that this is
 *                  not the size of the slab itself
 *
 * @description
 * 	create a new slab and add it to the global linked list
 * 	retval pointer to the new slab
 *
 *****************************************************************************/
static qae_slab *create_slab(int size)
{
    int i = 0;
    int nslot = 0;
    qat_mem_config qmcfg;
    qae_slab *result = NULL;
    qae_slab *slb = NULL;

    qmcfg.length = SLAB_SIZE;
    if (ioctl(qat_memfd, QAT_MEM_MALLOC, &qmcfg) == -1)
    {
        static char errmsg[LINE_MAX];

        snprintf(errmsg, LINE_MAX, "ioctl QAT_MEM_MALLOC(%d)", qmcfg.length);
        perror(errmsg);
        goto exit;
    }

    if ((slb =
         mmap(NULL, qmcfg.length, PROT_READ | PROT_WRITE,
              MAP_SHARED | MAP_LOCKED, qat_memfd,
              qmcfg.virtualAddress)) == MAP_FAILED)
    {
        perror("mmap");
        goto exit;
    }

    DEBUG("%s slot size %d\n", __func__, size);
    slb->slot_size = size;
    slb->next_slab = slab_list;
    slb->next_slot = NULL;
    slb->sig = SIG_ALLOC;
    /* Make sure update of the slab list is the last thing to be done.  This
       means it is not necessary to lock against anyone iterating the list from 
       the head */
    slab_list = slb;

    qae_slot *slt = NULL;

    for (i = sizeof(qae_slab); SLAB_SIZE - i >= size; i += size)
    {
        slt = (qae_slot *) ((unsigned char *)slb + i);
        slt->next = slb->next_slot;
        slt->sig = SIG_FREE;
        slt->file = NULL;
        slt->line = 0;
        slb->next_slot = slt;
        nslot++;
    }
    result = slb;
    DEBUG("%s slab %p last slot is %p, count is %d\n", __func__, slb, slt,
          nslot);
  exit:
    return result;
}

/*****************************************************************************
 * function:
 *         alloc_from_slab(int size, const char *file, int line)
 *
 * @param[in] size, the size of the memory block required
 * @param[in] file, the C source filename of the call site
 * @param[in] line, the line number withing the C source file of the call site
 *
 * @description
 *      allocate a slot of memory from some slab
 * 	retval pointer to the allocated block
 *
 *****************************************************************************/
static void *alloc_from_slab(int size, const char *file, int line)
{
    qae_slab *slb;
    qae_slot *slt;
    int slot_size;
    void *result = NULL;
    int rc;
    int i;

    if ((rc = pthread_mutex_lock(&bsal)) != 0)
    {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        return result;
    }

    if (!inited)
        init();

    DEBUG("%s: pthread_mutex_lock\n", __func__);
    size += sizeof(qae_slot);

    slot_size = -1;
    
    for (i = 0; i < sizeof (slot_sizes_available) / sizeof (int); i++)
    {
        if (size < slot_sizes_available[i])
        {
            slot_size = slot_sizes_available[i];
            break;
        }
    }

    if (slot_size == -1)
    {
        if (size <= MAX_ALLOC)
            slot_size = MAX_ALLOC;
        else
        {
            DEBUG("%s too big %d\n", __func__, size);
            goto exit;
        }
    }

    for (slb = slab_list; slb != NULL; slb = slb->next_slab)
    {
        if (slb->slot_size == slot_size && slb->next_slot != NULL)
            break;
    }

    if (!slb)
        slb = create_slab(slot_size);

    if (slb == NULL)
    {
        printf("%s error, create_slab failed\n", __func__);
        goto exit;
    }

    if (slb->next_slot == NULL)
    {
        DEBUG("%s error, no slots\n", __func__);
        goto exit;
    }

    slt = slb->next_slot;

    if (slt->sig != SIG_FREE)
    {
        ERROR("%s error alloc slot that isn't free %p\n", __func__, slt);
        exit(1);
    }

    slb->next_slot = slt->next;
    memset(slt, 0xAA, slot_size);
    slt->sig = SIG_ALLOC;
    slt->file = strdup(file);
    slt->line = line;
    result = (void *)((unsigned char *)slt + sizeof(qae_slot));

  exit:
    if ((rc = pthread_mutex_unlock(&bsal)) != 0)
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
    DEBUG("%s: pthread_mutex_unlock\n", __func__);
    return result;
}

/*****************************************************************************
 * function:
 *         free_to_slab(void *ptr)
 *
 * @param[in] ptr, pointer to the memory to be freed
 *
 * @description
 *      free a slot of memory back to its slab
 *
 *****************************************************************************/
static void free_to_slab(void *ptr)
{
    qae_slab *slb;
    qae_slot *slt = (void *)((unsigned char *)ptr - sizeof(qae_slot));
    int rc;

    if ((rc = pthread_mutex_lock(&bsal)) != 0)
    {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        return;
    }
    DEBUG("%s: pthread_mutex_lock\n", __func__);
    if (slt->sig != SIG_ALLOC)
    {
        ERROR("%s error free slot that isn't alloc %p\n", __func__, slt);
        exit(1);
    }

    slb = find_slab_slot(ptr);

    if (slb)
    {
        free(slt->file);
        memset(slt, 0xFF, slb->slot_size);
        slt->sig = SIG_FREE;
        slt->file = NULL;
        slt->line = 0;
        slt->next = slb->next_slot;
        slb->next_slot = slt;
        goto exit;
    }

    ERROR ("%s error can't free %p\n", __func__, ptr);

  exit:
    if ((rc = pthread_mutex_unlock(&bsal)) != 0)
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
    DEBUG("%s: pthread_mutex_unlock\n", __func__);
}

/*****************************************************************************
 * function:
 *         slot_get_size(void *ptr)
 *
 * @param[in] ptr, pointer to the slot memory 
 *
 * @description
 *      get the slot memory size in bytes
 *
 *****************************************************************************/
static int slot_get_size(void *ptr)
{
    int rc;
    int result = 0;

    if ((rc = pthread_mutex_lock(&bsal)) != 0)
    {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        return result;
    }
    DEBUG("%s: pthread_mutex_lock\n", __func__);
    qae_slab *slb = find_slab_slot(ptr);

    if (!slb)
    {
        ERROR("%s error can't find %p\n", __func__, ptr);
        goto exit;
    }
    result = slb->slot_size - sizeof(qae_slot);

  exit:
    if ((rc = pthread_mutex_unlock(&bsal)) != 0)
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
    DEBUG("%s: pthread_mutex_unlock\n", __func__);
    return result;
}

/*****************************************************************************
 * function:
 *         cleanup_slabs(void)
 *
 * @description
 *      Free all memory managed by the slab allocator. This function is
 *      intended to be registered as an atexit() handler.
 *
 *****************************************************************************/
static void cleanup_slabs(void)
{
    qae_slab *slb, *s_next_slab;
    qat_mem_config qmcfg;
    int rc;

    if ((rc = pthread_mutex_lock(&bsal)) != 0)
    {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        return;
    }
    DEBUG("%s: pthread_mutex_lock\n", __func__);
    for (slb = slab_list; slb != NULL; slb = s_next_slab)
    {
        qae_slot *slt = NULL;
        int i;

        for (i = sizeof(qae_slab); SLAB_SIZE - i >= slb->slot_size;
             i += slb->slot_size)
        {
            slt = (qae_slot *) ((unsigned char *)slb + i);

            if (slt->sig == SIG_ALLOC && slt->file != NULL && slt->line != 0)
                DEBUG("Leak : %p %s:%d\n", slt, slt->file, slt->line);
        }

        DEBUG("%s do munmap  of %p\n", __func__, slb);
        qmcfg = *((qat_mem_config *) slb);

        /* Have to save this off before unmapping. This is why we can't have
           slb = slb->next_slab in the for loop above. */
        s_next_slab = slb->next_slab;

        if (munmap(slb, SLAB_SIZE) == -1)
        {
            perror("munmap");
            exit(EXIT_FAILURE);
        }
        DEBUG("%s ioctl free of %p\n", __func__, slb);
        if (ioctl(qat_memfd, QAT_MEM_FREE, &qmcfg) == -1)
        {
            perror("ioctl QAT_MEM_FREE");
            exit(EXIT_FAILURE);
        }
    }
    DEBUG("%s done\n", __func__);

    if ((rc = pthread_mutex_unlock(&bsal)) != 0)
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
    DEBUG("%s: pthread_mutex_unlock\n", __func__);
}

/******************************************************************************
* function:
*         init(void)
*
* description:
*   Initialise the user-space part of the QAT memory allocator.
*
******************************************************************************/
static void init(void)
{
    if ((qat_memfd = open("/dev/qat_mem", O_RDWR)) == -1)
    {
        perror("open qat_mem");
        exit(EXIT_FAILURE);
    }
    atexit(cleanup_slabs);
    inited = 1;
}

/******************************************************************************
* function:
*         qaeMemV2P(void *v)
*
* @param[in] v, virtual memory address pointer 
*
* description:
* 	map vritual memory address to physic memory address
*
******************************************************************************/
CpaPhysicalAddr qaeMemV2P(void *v)
{
    qae_slab *slb;

    slb = find_slab_slot(v);

    if (!slb)
    {
        printf("%s error can't find slab for %p\n", __func__, v);
        return 0;
    }

    qat_mem_config *memCfg = (qat_mem_config *) slb;
    ptrdiff_t offset = ((unsigned char *)v - (unsigned char *)slb);

    /* Exit if the allocator signature isn't found. */
    if (memCfg->signature != 0xDEADBEEF)
    {
        DEBUG
            ("%s: virtualAddr %p pageStart %p signature %x physicalAddress %x\n",
             __func__, v, slb, memCfg->signature,
             (unsigned int)memCfg->physicalAddress);

        ERROR("%s: Exiting due to invalid signature\n", __func__);
        exit(EXIT_FAILURE);
    }

    if (offset >= memCfg->length)
        DEBUG("%s: offset %d memCfg->length %d\n", __func__, (int)offset,
              memCfg->length);

    if (offset > memCfg->length)
        exit(EXIT_FAILURE);

    return (memCfg->physicalAddress) + offset;
}

/**************************************
 * Memory functions
 *************************************/

/******************************************************************************
* function:
*         qaeMemAlloc(size_t memsize, , const char *file, int line)
*
* @param[in] memsize,  size of usable memory requested
* @param[in] file,     the C source filename of the call site
* @param[in] line,     the line number withing the C source file of the call site
*
* description:
*   Allocate a block of pinned memory.
*
******************************************************************************/
void *qaeMemAlloc(size_t memsize, const char *file, int line)
{
    return alloc_from_slab(memsize, file, line);
}

/******************************************************************************
* function:
*         qaeMemFree(void *ptr)
*
* @param[in] ptr, address of start of usable memory
*
* description:
*   Free a block of memory previously allocated by this allocator.
*
******************************************************************************/
void qaeMemFree(void *ptr)
{
    if(NULL != ptr) 
        free_to_slab(ptr);
}

/******************************************************************************
* function:
*         qaeMemRealloc(void *ptr, size_t memsize, const char *file, int line)
*
* @param[in] ptr,     address of start of usable memory for old allocation
* @param[in] memsize, size of new block required
* @param[in] file,    the C source filename of the call site
* @param[in] line,    the line number withing the C source file of the call site
*
* description:
*   Change the size of usable memory in an allocated block. This may allocate
*   a new block and copy the data to it.
*
******************************************************************************/
void *qaeMemRealloc(void *ptr, size_t memsize, const char *file, int line)
{
    int copy = slot_get_size(ptr);
    void *n = alloc_from_slab(memsize, file, line);

    if (memsize < copy)
        copy = memsize;
    memcpy(n, ptr, copy);
    free_to_slab(ptr);
    return n;
}
