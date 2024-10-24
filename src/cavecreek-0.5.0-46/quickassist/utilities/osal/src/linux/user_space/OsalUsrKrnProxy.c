/**
 * @file OsalUsrKrnProxy.c (linux user space)
 *
 * @brief Implementation for NUMA.
 *
 *
 * @par
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
 *  version: SXXXX.L.0.5.0-46
 */

#include "Osal.h"
#include "OsalOsTypes.h"
#include "OsalDevDrv.h"
#include "OsalDevDrvCommon.h"

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

#define ICP_DEV_MEM "/dev/icp_dev_mem"
#define ICP_DEV_MEM_PAGE "/dev/icp_dev_mem_page"
static int fd = 0;
static int fdp = 0;


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_page = PTHREAD_MUTEX_INITIALIZER;

static dev_mem_info_t *pUserMemList = NULL;
static dev_mem_info_t *pUserMemListHead = NULL;

static dev_mem_info_t *pUserMemListPage = NULL;
static dev_mem_info_t *pUserMemListHeadPage = NULL;

OSAL_STATUS userMemListAdd(dev_mem_info_t *pMemInfo)
{
    pthread_mutex_lock(&mutex);
    ADD_ELEMENT_TO_END_OF_LIST(pMemInfo, pUserMemList, pUserMemListHead);
    pthread_mutex_unlock(&mutex);
    return OSAL_SUCCESS;
}

OSAL_STATUS userMemListAddPage(dev_mem_info_t *pMemInfo)
{
    pthread_mutex_lock(&mutex_page);
    ADD_ELEMENT_TO_END_OF_LIST(pMemInfo,
                               pUserMemListPage,
                               pUserMemListHeadPage);
    pthread_mutex_unlock(&mutex_page);

    return OSAL_SUCCESS;
}

void userMemListFree(dev_mem_info_t *pMemInfo)
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

void userMemListFreePage(dev_mem_info_t *pMemInfo)
{
    dev_mem_info_t *pCurr = NULL;

    for (pCurr = pUserMemListHeadPage; pCurr != NULL; pCurr = pCurr->pNext)
    {
        if (pCurr == pMemInfo)
        {
             REMOVE_ELEMENT_FROM_LIST(pCurr,
                                      pUserMemListPage,
                                      pUserMemListHeadPage);
             break;
        }
    }
}

dev_mem_info_t* userMemLookupBySize(UINT32 size)
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

dev_mem_info_t* userMemLookupByVirtAddr(void* virt_addr)
{
    dev_mem_info_t *pCurr = NULL;

    for (pCurr = pUserMemListHead; pCurr != NULL; pCurr = pCurr->pNext)
    {
        if ((UARCH_INT)pCurr->virt_addr <= (UARCH_INT)virt_addr &&
        ((UARCH_INT)pCurr->virt_addr + pCurr->size) > (UARCH_INT)virt_addr)
        {
            return pCurr;
         }
    }
    return NULL;
}

dev_mem_info_t* userMemLookupByVirtAddrPage(void* virt_addr)
{
    dev_mem_info_t *pCurr = NULL;

    for (pCurr = pUserMemListHeadPage; pCurr != NULL; pCurr = pCurr->pNext)
    {
         if((UARCH_INT)pCurr->virt_addr == (UARCH_INT)virt_addr)
         {
            return pCurr;
         }
    }
    return NULL;
}

OSAL_PUBLIC OSAL_STATUS
osalMemInit()
{
    fd = open(ICP_DEV_MEM, O_RDWR);
    if (fd < 0) {
       osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "unable to open /dev/icp_dev_mem %d\n",
                fd, 0, 0, 0, 0, 0);
            return OSAL_FAIL;
    }
    fdp = open(ICP_DEV_MEM_PAGE, O_RDWR);
    if (fdp < 0) {
       osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "unable to open /dev/icp_dev_mem_page %d\n",
                fd, 0, 0, 0, 0, 0);
            close(fd);
            return OSAL_FAIL;
    }
    return OSAL_SUCCESS;
}

OSAL_PUBLIC void
osalMemDestroy()
{
    close(fd);
    close(fdp);
}

OSAL_PUBLIC void*
osalMemAllocContiguousNUMA(UINT32 size, UINT32 node, UINT32 alignment)
{
    int ret = 0;
    dev_mem_info_t *pMemInfo = NULL;
    void *pVirtAddress = NULL;
    void* pOriginalAddress = NULL;
    UARCH_INT padding = 0;
    UARCH_INT aligned_address = 0;

    if (size == 0 || alignment == 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "Invalid size or alignment parameter\n",
                0, 0, 0, 0, 0, 0);
        return NULL;
    }
    if (fd < 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "Memory file handle is not ready\n",
                fd, 0, 0, 0, 0, 0);
        return NULL;
    }
    
    pthread_mutex_lock(&mutex);

    if ( (pMemInfo = userMemLookupBySize(size + alignment)) != NULL)
    {
        pOriginalAddress = (void*) ((UARCH_INT) pMemInfo->virt_addr +
           (UARCH_INT)(pMemInfo->size - pMemInfo->available_size));
        padding = (UARCH_INT) pOriginalAddress % alignment;
        aligned_address = ((UARCH_INT) pOriginalAddress) - padding 
                           + alignment;
        pMemInfo->available_size -= (size + (aligned_address - 
                (UARCH_INT) pOriginalAddress));
        pMemInfo->allocations += 1;
        pthread_mutex_unlock(&mutex);
        return (void*) aligned_address;
    }
    pthread_mutex_unlock(&mutex);


    pMemInfo = malloc(sizeof(dev_mem_info_t));
    if (NULL == pMemInfo)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "unable to allocate pMemInfo buffer\n",
                fd, 0, 0, 0, 0, 0);
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
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "ioctl call failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
        free(pMemInfo);
        return NULL;
    }

    pMemInfo->virt_addr = mmap((caddr_t) 0, pMemInfo->size,
                                PROT_READ|PROT_WRITE, MAP_SHARED, fd,
                                (pMemInfo->id * getpagesize()));

    if (pMemInfo->virt_addr == (caddr_t) MAP_FAILED)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "mmap failed\n",
                0, 0, 0, 0, 0, 0);
        ret = ioctl(fd, DEV_MEM_IOC_MEMFREE, pMemInfo);
        if (ret != 0)
        {
            osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "ioctl call failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
         }
         free(pMemInfo);
         return NULL;
    }

    pMemInfo->available_size = pMemInfo->size - size - USER_MEM_128BYTE_OFFSET;
    pMemInfo->allocations = 1;
    memcpy(pMemInfo->virt_addr, pMemInfo, sizeof(dev_mem_info_t));
    pVirtAddress = (void *)((UARCH_INT)pMemInfo->virt_addr 
          + USER_MEM_128BYTE_OFFSET);
    userMemListAdd(pMemInfo);
    return pVirtAddress;
}


OSAL_PUBLIC void*
osalMemAllocPage(UINT32 node, UINT64 *physAddr)
{
    int ret = 0;
    dev_mem_info_t *pMemInfo = NULL;

    if (fd < 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "Memory file handle is not ready\n",
                fd, 0, 0, 0, 0, 0);
        return NULL;
    }

    pMemInfo = malloc(sizeof(dev_mem_info_t));
    if (NULL == pMemInfo)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "unable to allocate pMemInfo buffer\n",
                fd, 0, 0, 0, 0, 0);
        return NULL;
    }

    pMemInfo->nodeId = node;
    pMemInfo->size = getpagesize();

    ret = ioctl(fdp, DEV_MEM_IOC_MEMALLOCPAGE, pMemInfo);
    if (ret != 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "ioctl call failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
        free(pMemInfo);
        return NULL;
    }

    pMemInfo->virt_addr = mmap(NULL, pMemInfo->size, PROT_READ|PROT_WRITE,
                               MAP_PRIVATE, fdp, pMemInfo->id * getpagesize());
    if (pMemInfo->virt_addr == MAP_FAILED)
    {
        osalStdLog("Errno: %d\n", errno);
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "mmap failed\n",
                0, 0, 0, 0, 0, 0);
        ret = ioctl(fd, DEV_MEM_IOC_MEMFREEPAGE, pMemInfo);
        if (ret != 0)
        {
            osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "ioctl call failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
         }
         free(pMemInfo);
         return NULL;
    }

    userMemListAddPage(pMemInfo);
    *physAddr = pMemInfo->phy_addr;
    return pMemInfo->virt_addr;
}



OSAL_PUBLIC void
osalMemFreeNUMA(void* pVirtAddress)
{
    int ret = 0;
    dev_mem_info_t *pMemInfo = NULL;

    if (NULL == pVirtAddress)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "Invalid virtual address\n",
                0, 0, 0, 0, 0, 0);
        return;
    }

    pthread_mutex_lock(&mutex);
    if ((pMemInfo = userMemLookupByVirtAddr(pVirtAddress)) != NULL)
    {
        pMemInfo->allocations -= 1;
        if (pMemInfo->allocations != 0)
        {
            pVirtAddress = NULL;
            pthread_mutex_unlock(&mutex);
            return;
        }
    }
    else
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "userMemLookupByVirtAddr failed\n",
                0, 0, 0, 0, 0, 0);
        pthread_mutex_unlock(&mutex);
        return;
    }

    ret = munmap(pMemInfo->virt_addr, pMemInfo->size);
    if (ret != 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "munmap failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
    }

    ret = ioctl(fd, DEV_MEM_IOC_MEMFREE, pMemInfo);
    if (ret != 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "ioctl call failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
    }

    userMemListFree(pMemInfo);
    free(pMemInfo);
    pthread_mutex_unlock(&mutex);
    return;
}

OSAL_PUBLIC void
osalMemFreePage(void* pVirtAddress)
{
    int ret = 0;
    dev_mem_info_t *pMemInfo = NULL;

    if (NULL == pVirtAddress)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "Invalid virtual address\n",
                0, 0, 0, 0, 0, 0);
        return;
    }

    pthread_mutex_lock(&mutex_page);
    pMemInfo = userMemLookupByVirtAddrPage(pVirtAddress);

    if(pMemInfo == NULL)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
               "userMemLookupByVirtAddrPage failed \n", 0, 0, 0, 0,
                0, 0);
         pthread_mutex_unlock(&mutex_page);
         return;
    }

    ret = munmap(pMemInfo->virt_addr, getpagesize());
    if (ret != 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "munmap failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
    }

    ret = ioctl(fdp, DEV_MEM_IOC_MEMFREEPAGE, pMemInfo);
    if (ret != 0)
    {
        osalLog (OSAL_LOG_LVL_ERROR, OSAL_LOG_DEV_STDOUT,
                "ioctl call failed, ret = %d\n",
                ret, 0, 0, 0, 0, 0);
    }
    userMemListFreePage(pMemInfo);
    free(pMemInfo);
    pthread_mutex_unlock(&mutex_page);
    return;
}

UINT64
osalVirtToPhysNUMA(void* pVirtAddress)
{
    dev_mem_info_t *pMemInfo = NULL;
    void *pVirtPageAddress = NULL;
    UARCH_INT offset = 0;

    OSAL_LOCAL_ENSURE(pVirtAddress != NULL,
                "osalVirtToPhysNUMA():   Null virtual address pointer",
                0);

    pVirtPageAddress = ((int *)((((UARCH_INT)pVirtAddress)) & (PAGE_MASK)));

    offset = (UARCH_INT)pVirtAddress - (UARCH_INT)pVirtPageAddress;

    do
    {
        pMemInfo = (dev_mem_info_t *)pVirtPageAddress;
        if (pMemInfo->virt_addr == pVirtPageAddress)
        {
              break;
        }
        pVirtPageAddress = (void*)((UARCH_INT)pVirtPageAddress - PAGE_SIZE);

        offset += PAGE_SIZE;
    }
    while (pMemInfo->virt_addr != pVirtPageAddress);
    return (UINT64)(pMemInfo->phy_addr + offset);
}

