/***************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or 
 *   redistributing this file, you may do so under either license.
 * 
 *   GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
 * 
 *   This program is free software; you can redistribute it and/or modify 
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 * 
 *   This program is distributed in the hope that it will be useful, but 
 *   WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *   General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program; if not, write to the Free Software 
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution 
 *   in the file called LICENSE.GPL.
 * 
 *   Contact Information:
 *   Intel Corporation
 * 
 *   BSD LICENSE 
 * 
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
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
 * 
 *  version: SXXXX.L.0.5.0-46
 *
 ***************************************************************************/
/**
 *****************************************************************************
 * @file qae_mem_utils.c
 *
 * This file provides provide for Linux user space memory allocation. It uses
 * a driver that allocates the memory in kernel memory space (to ensure
 * physically contiguous memory) and maps it to
 * user space for use by the  quick assist sample code
 *
 *****************************************************************************/

#include "qae_mem_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>



#define QAE_MEM "/dev/qae_mem"
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE-1))
#define USER_MEM_128BYTE_OFFSET         (128)

static pthread_mutex_t mutex_g = PTHREAD_MUTEX_INITIALIZER;
static dev_mem_info_t *pUserMemList = NULL;
static dev_mem_info_t *pUserMemListHead = NULL;

static int fd = -1;


/**************************************
 * Memory functions
 *************************************/
CpaStatus qaeMemInit(void)
{
    fd = open(QAE_MEM, O_RDWR);
    if (fd < 0)
    {
        printf("unable to open %s %d\n",QAE_MEM,fd);
        return CPA_STATUS_FAIL;
    }
    return CPA_STATUS_SUCCESS;
}

void qaeMemDestroy(void)
{
    close(fd);
}


void * qaeMemAlloc (Cpa32U memsize)
{
    QAE_UINT *memPtr = NULL;
    memPtr = malloc(memsize);
    return memPtr;
}

static CpaStatus userMemListAdd(dev_mem_info_t *pMemInfo)
{
    pthread_mutex_lock(&mutex_g);
    ADD_ELEMENT_TO_END_OF_LIST(pMemInfo, pUserMemList, pUserMemListHead);
    pthread_mutex_unlock(&mutex_g);
    return CPA_STATUS_SUCCESS;
}

static void userMemListFree(dev_mem_info_t *pMemInfo)
{
    dev_mem_info_t *pCurr = NULL;

    for (pCurr = pUserMemListHead; pCurr != NULL; pCurr = pCurr->pNext)
    {
        if (pCurr == pMemInfo)
        {
            REMOVE_ELEMENT_FROM_LIST(pCurr, pUserMemList, pUserMemListHead);
            break;
        }
    }
}


static dev_mem_info_t* userMemLookupBySize(Cpa32U size)
{
    dev_mem_info_t *pCurr = NULL;

    for (pCurr = pUserMemListHead; pCurr != NULL; pCurr = pCurr->pNext)
    {
        if (pCurr->available_size >= size)
        {
            return pCurr;
        }
    }
    return NULL;
}

static dev_mem_info_t* userMemLookupByVirtAddr(void* virt_addr)
{
    dev_mem_info_t *pCurr = NULL;

    for (pCurr = pUserMemListHead; pCurr != NULL; pCurr = pCurr->pNext)
    {
        if ((QAE_UINT)pCurr->virt_addr <= (QAE_UINT)virt_addr &&
                ((QAE_UINT)pCurr->virt_addr + pCurr->size) > (QAE_UINT)virt_addr)
        {
            return pCurr;
        }
    }
    return NULL;
}


void * qaeMemAllocNUMA (Cpa32U size, Cpa32U node, Cpa32U alignment)
{
    int ret = 0;
    dev_mem_info_t *pMemInfo = NULL;
    void* pVirtAddress = NULL;
    void* pOriginalAddress = NULL;
    QAE_UINT padding = 0;
    QAE_UINT aligned_address = 0;

    if (size == 0 || alignment == 0)
    {
        printf("Invalid size or alignment parameter\n");
        return NULL;
    }
    if (fd < 0)
    {
        printf("Memory file handle is not ready\n");
        return NULL;
    }

    pthread_mutex_lock(&mutex_g);

    if ( (pMemInfo = userMemLookupBySize(size + alignment)) != NULL)
    {
        pOriginalAddress = (void*) ((QAE_UINT) pMemInfo->virt_addr +
                (QAE_UINT)(pMemInfo->size - pMemInfo->available_size));
        padding = (QAE_UINT) pOriginalAddress % alignment;
        aligned_address = ((QAE_UINT) pOriginalAddress) - padding
        + alignment;
        pMemInfo->available_size -= (size + (aligned_address -
                (QAE_UINT) pOriginalAddress));
        pMemInfo->allocations += 1;
        pthread_mutex_unlock(&mutex_g);
        return (void*) aligned_address;
    }
    pthread_mutex_unlock(&mutex_g);


    pMemInfo = malloc(sizeof(dev_mem_info_t));
    if (NULL == pMemInfo)
    {
        printf("unable to allocate pMemInfo buffer\n");
        return NULL;
    }

    pMemInfo->allocations = 0;

    pMemInfo->size = USER_MEM_128BYTE_OFFSET + size;
    pMemInfo->size = pMemInfo->size%PAGE_SIZE?
            ((pMemInfo->size/PAGE_SIZE)+1)*PAGE_SIZE:
    pMemInfo->size;
    pMemInfo->nodeId = node;
    ret = ioctl(fd, DEV_MEM_IOC_MEMALLOC, pMemInfo);
    if (ret != 0)
    {
        printf("ioctl call failed, ret = %d\n",ret);
        free(pMemInfo);
        return NULL;
    }

    pMemInfo->virt_addr = mmap((caddr_t) 0, pMemInfo->size,
            PROT_READ|PROT_WRITE, MAP_SHARED, fd,
            (pMemInfo->id * getpagesize()));

    if (pMemInfo->virt_addr == (caddr_t) MAP_FAILED)
    {
        printf("mmap failed\n");
        ret = ioctl(fd, DEV_MEM_IOC_MEMFREE, pMemInfo);
        if (ret != 0)
        {
            printf("ioctl call failed, ret = %d\n",ret);
        }
        free(pMemInfo);
        return NULL;
    }

    pMemInfo->available_size = pMemInfo->size - size - USER_MEM_128BYTE_OFFSET;
    pMemInfo->allocations = 1;
    memcpy(pMemInfo->virt_addr, pMemInfo, sizeof(dev_mem_info_t));
    pVirtAddress = (void *)((QAE_UINT)pMemInfo->virt_addr
            + USER_MEM_128BYTE_OFFSET);
    userMemListAdd(pMemInfo);
    return pVirtAddress;
}

void qaeMemFreeNUMA (void** ptr)
{
    int ret = 0;
    dev_mem_info_t *pMemInfo = NULL;

    if (NULL == ptr || NULL == *ptr)
    {
        printf("Invalid virtual address\n");
        return;
    }

    pthread_mutex_lock(&mutex_g);
    if ((pMemInfo = userMemLookupByVirtAddr(*ptr)) != NULL)
    {
        pMemInfo->allocations -= 1;
        if (pMemInfo->allocations != 0)
        {
            *ptr = NULL;
            pthread_mutex_unlock(&mutex_g);
            return;
        }
    }
    else
    {
        printf("userMemLookupByVirtAddr failed\n");
        pthread_mutex_unlock(&mutex_g);
        return;
    }

    ret = munmap(pMemInfo->virt_addr, pMemInfo->size);
    if (ret != 0)
    {
        printf("munmap failed, ret = %d\n",ret);
    }

    ret = ioctl(fd, DEV_MEM_IOC_MEMFREE, pMemInfo);
    if (ret != 0)
    {
        printf("ioctl call failed, ret = %d\n",ret);
    }
    userMemListFree(pMemInfo);
    free(pMemInfo);
    *ptr = NULL;
    pthread_mutex_unlock(&mutex_g);
    return;
}

void qaeMemFree (void **ptr)
{
    if (NULL == ptr || NULL == *ptr)
    {
        printf("ERROR, Trying to Free NULL Pointer\n");
        return;
    }
    free(*ptr);
    *ptr = NULL;
}

QAE_PHYS_ADDR qaeVirtToPhysNUMA(void* pVirtAddress)
{
    dev_mem_info_t *pMemInfo = NULL;
    void *pVirtPageAddress = NULL;
    QAE_UINT offset = 0;
    if(pVirtAddress==NULL)
    {
        printf("qaeVirtToPhysNUMA():   Null virtual address pointer\n");
        return (QAE_PHYS_ADDR) 0;
    }

    pVirtPageAddress = ((int *)((((QAE_UINT)pVirtAddress)) & (PAGE_MASK)));

    offset = (QAE_UINT)pVirtAddress - (QAE_UINT)pVirtPageAddress;

    do
    {
        pMemInfo = (dev_mem_info_t *)pVirtPageAddress;
        if (pMemInfo->virt_addr == pVirtPageAddress)
        {
            break;
        }
        pVirtPageAddress = (void*)((QAE_UINT)pVirtPageAddress - PAGE_SIZE);

        offset += PAGE_SIZE;
    }
    while (pMemInfo->virt_addr != pVirtPageAddress);
    return (QAE_PHYS_ADDR)(pMemInfo->phy_addr + offset);
}





