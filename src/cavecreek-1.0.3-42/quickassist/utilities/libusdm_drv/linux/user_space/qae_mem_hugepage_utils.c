/***************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or 
 *   redistributing this file, you may do so under either license.
 * 
 *   GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
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
 * 
 *  version: QAT1.7.Upstream.L.1.0.3-42
 *
 ***************************************************************************/
/**
 ****************************************************************************
 * @file qae_mem_hugepage_utils.c
 *
 * This file provides for utilities for Linux user space memory
 * allocation with huge page enabled.
 *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include "qae_page_table.h"
#include "qae_mem_hugepage_utils.h"
#include "qae_mem_user_utils.h"


#define HUGEPAGE_FILE_DIR "/dev/hugepages/usdm.XXXXXX"
#define HUGEPAGE_FILE_LEN (sizeof(HUGEPAGE_FILE_DIR))

static size_t g_num_hugepages = 0;

/*
 * Get physical address of any mapped virtual address in the current process.
 */
API_LOCAL
uint64_t hugepage_virt2phy(const void *virtaddr)
{
    int fd;
    uint64_t page, physaddr;
    unsigned long virt_pfn;
    int page_size;
    off_t offset;

    /* standard page size */
    page_size = getpagesize();

    fd = qae_open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    virt_pfn = (unsigned long)virtaddr / page_size;
    offset = sizeof(uint64_t) * virt_pfn;
    if (qae_lseek(fd, offset, SEEK_SET) == (off_t) -1) {
        close(fd);
        return 0;
    }
    if (qae_read(fd, &page, sizeof(uint64_t)) < 0) {
        close(fd);
        return 0;
    }

    /*
     *  the pfn (page frame number) are bits 0-54 (see
     *  pagemap.txt in linux Documentation)
     */
    physaddr = ((page & 0x7fffffffffffffULL) * page_size)
                + ((unsigned long)virtaddr % page_size);
    close(fd);
    return physaddr;
}

API_LOCAL
void * hugepage_mmap_phy_addr(const size_t len)
{
    void *addr = NULL;
    int ret = 0;
    int hpg_fd;
    char hpg_fname[HUGEPAGE_FILE_LEN];

    /*
     * for every mapped huge page there will be a separate file descriptor
     * created from a temporary file, we should NOT close fd explicitly, it
     * will be reclaimed by the OS when the process gets terminated, and
     * meanwhile the huge page binding to the fd will be released, this could
     * guarantee the memory cleanup order between user buffers and ETR.
     */
    snprintf(hpg_fname, sizeof(HUGEPAGE_FILE_DIR), "%s", HUGEPAGE_FILE_DIR);
    hpg_fd = qae_mkstemp(hpg_fname);
    unlink(hpg_fname);

    if (hpg_fd < 0)
    {
        CMD_ERROR("%s:%d mkstemp(%s) for hpg_fd failed with errno: %d\n",
                __func__,__LINE__, hpg_fname, errno);
        return NULL;
    }

    addr = qae_mmap(
            NULL,
            len,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB,
            hpg_fd,
            0);

    if (MAP_FAILED == addr)
        return NULL;

    ret = qae_madvise(addr, len, MADV_DONTFORK);
    if (0 != ret)
    {
        munmap(addr, len);
        return NULL;
    }

    ((dev_mem_info_t*)addr)->hpg_fd = hpg_fd;
    return addr;
}

API_LOCAL
dev_mem_info_t* hugepage_alloc_slab(const size_t size, const int node,
        const int type)
{
    dev_mem_info_t *slab = NULL;

    slab = hugepage_mmap_phy_addr(size);
    if (!slab)
    {
        CMD_ERROR("%s:%d mmap on huge page memory allocation failed\n",
                __func__,__LINE__);
        return NULL;
    }
    slab->nodeId = node;
    slab->size = size;
    slab->type = type;
    slab->virt_addr = slab;
    slab->phy_addr = hugepage_virt2phy(slab);
    if (!slab->phy_addr)
    {
        CMD_ERROR("%s:%d virt2phy on huge page memory allocation failed\n",
                __func__,__LINE__);
        munmap(slab, size);
        return NULL;
    }
    return slab;
}

API_LOCAL
int init_hugepages(const int fd)
{
    int ret = 0;
#if (QAE_NUM_PAGES_PER_ALLOC == 512)
    ret = mem_ioctl(fd, DEV_MEM_IOC_GET_NUM_HPT, &g_num_hugepages);
    if (ret)
    {
        CMD_ERROR("%s:%d ioctl call for checking number of huge page failed, ret = %d\n",
                __func__,__LINE__,ret);
        g_num_hugepages = 0;
        ret = -EIO;
    }
    if (g_num_hugepages > 0)
    {
        set_free_page_table_fptr(free_page_table_hpg);
        set_loadaddr_fptr(load_addr_hpg);
        set_loadkey_fptr(load_key_hpg);
    }
#else
    if (fd < 0)
        return -EIO;
#endif
    return ret;
}

API_LOCAL
int hugepage_enabled()
{
    return g_num_hugepages > 0;
}
