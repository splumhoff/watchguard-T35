/*******************************************************************************
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
 * @file qae_page_table.h
 *
 * This file provides user-space page tables (similar to Intel x86/x64
 * page tables) for fast virtual to physical address translation.
 * Memory required:
 *  - 8 Mb to cover 4 Gb address space.
 * I.e. if only 1 Gb is used it will require additional 2 Mb.
 *
 ******************************************************************************/

#ifndef QAE_PAGE_TABLE_H_1
#define QAE_PAGE_TABLE_H_1

#include <stdint.h>
#include <stdlib.h>

#define PAGE_SIZE (0x1000)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define LEVEL_SIZE (PAGE_SIZE / sizeof(uint64_t))

#define HUGEPAGE_SIZE (0x200000)
#define HUGEPAGE_MASK (~(HUGEPAGE_SIZE - 1))

typedef struct {
    uint64_t    offset:12;
    uint64_t    idx3:9;
    uint64_t    idx2:9;
    uint64_t    idx1:9;
    uint64_t    idx0:9;
} page_entry_t;

typedef struct {
    uint64_t    offset:21;
    uint64_t    idx2:9;
    uint64_t    idx1:9;
    uint64_t    idx0:9;
} hugepage_entry_t;

typedef union {
    uint64_t	     addr;
    page_entry_t     pg_entry;
    hugepage_entry_t hpg_entry;
} page_index_t;

typedef struct page_table_t {
    union {
        uint64_t pa;
        struct page_table_t *pt;
    } next[LEVEL_SIZE];
} page_table_t;

typedef void (*free_page_table_fptr_t)(page_table_t * const table);
typedef void (*store_addr_fptr_t)(page_table_t*, uintptr_t, uint64_t);
typedef uint64_t (*load_addr_fptr_t)(page_table_t*, void*);
typedef uint64_t (*load_key_fptr_t)(page_table_t*, void *);

void set_free_page_table_fptr(free_page_table_fptr_t fp);
void set_loadaddr_fptr(load_addr_fptr_t fp);
void set_loadkey_fptr(load_key_fptr_t fp);

static inline void *next_level(page_table_t *volatile *ptr)
{
    page_table_t *old_ptr = *ptr;
    page_table_t *new_ptr;

    if (NULL != old_ptr)
        return old_ptr;

    new_ptr = malloc(sizeof(page_table_t));
    if (NULL == new_ptr)
        return NULL;

    memset(new_ptr, 0, sizeof(page_table_t));

    if (!__sync_bool_compare_and_swap(ptr, NULL, new_ptr))
        free(new_ptr);

    return *ptr;
}

static inline void free_page_level(page_table_t * const level,
    const size_t iter)
{
    size_t i = 0;

    if (0 == iter)
        return;

    for (i = 0; i < LEVEL_SIZE; ++i)
    {
        page_table_t *pt = level->next[i].pt;
        if (NULL != pt)
        {
            free_page_level(pt, iter - 1);
            free(pt);
        }
    }
}

static inline void free_page_table(page_table_t * const table)
{
    /* There is only 3 levels in 64-bit page table for 4KB pages. */
    free_page_level(table, 3);
}

static inline void free_page_table_hpg(page_table_t * const table)
{
    /* There is only 2 levels in 64-bit page table for 2MB pages. */
    free_page_level(table, 2);
}

static inline void store_addr(page_table_t *level,
        uintptr_t virt, uint64_t phys)
{
    page_index_t id;

    id.addr = virt;

    level = next_level(&level->next[id.pg_entry.idx0].pt);
    if (NULL == level)
        return;

    level = next_level(&level->next[id.pg_entry.idx1].pt);
    if (NULL == level)
        return;

    level = next_level(&level->next[id.pg_entry.idx2].pt);
    if (NULL == level)
        return;

    level->next[id.pg_entry.idx3].pa = phys;
}

static inline void store_addr_hpg(page_table_t *level,
        uintptr_t virt, uint64_t phys)
{
    page_index_t id;

    id.addr = virt;

    level = next_level(&level->next[id.hpg_entry.idx0].pt);
    if (NULL == level)
        return;

    level = next_level(&level->next[id.hpg_entry.idx1].pt);
    if (NULL == level)
        return;

    level->next[id.hpg_entry.idx2].pa = phys;
}

static inline uint64_t get_key(const uint64_t phys)
{
    /* For 4KB page: use bits 20-31 of a physical address as a hash key.
     * It provides a good distribution for 1Mb/2Mb slabs and a moderate
     * distribution for 128Kb/256Kb/512Kbslabs.
     */
    return (phys >> 20) & ~PAGE_MASK;
}

static inline void store_mmap_range(page_table_t *p_level,
        void *p_virt, uint64_t p_phys, size_t p_size, int hp_en)
{
    size_t offset;
    size_t page_size = PAGE_SIZE;
    size_t page_mask = PAGE_MASK;
    store_addr_fptr_t store_addr_ptr = store_addr;
    const uintptr_t virt = (uintptr_t) p_virt;

    if (hp_en)
    {
        page_size = HUGEPAGE_SIZE;
        page_mask = HUGEPAGE_MASK;
        store_addr_ptr = store_addr_hpg;
    }
    /* Store the key into the physical address itself,
     * for 4KB pages: 12 lower bits are always 0 (physical page addresses
     * are 4KB-aligned).
     * for 2MB pages: 21 lower bits are always 0 (physical page addresses
     * are 2MB-aligned)
     */
    p_phys = (p_phys & page_mask) | get_key(p_phys);
    for (offset = 0; offset < p_size; offset += page_size)
    {
        store_addr_ptr(p_level, virt + offset, p_phys + offset);
    }
}

static inline uint64_t load_addr(page_table_t *level, void *virt)
{
    page_index_t id;
    uint64_t phy_addr;

    id.addr = (uintptr_t) virt;

    level = level->next[id.pg_entry.idx0].pt;
    if (NULL == level)
        return 0;

    level = level->next[id.pg_entry.idx1].pt;
    if (NULL == level)
        return 0;

    level = level->next[id.pg_entry.idx2].pt;
    if (NULL == level)
        return 0;

    phy_addr = level->next[id.pg_entry.idx3].pa;
    return (phy_addr & PAGE_MASK) | id.pg_entry.offset;
}

static inline uint64_t load_addr_hpg(page_table_t *level, void *virt)
{
    page_index_t id;
    uint64_t phy_addr;

    id.addr = (uintptr_t) virt;

    level = level->next[id.hpg_entry.idx0].pt;
    if (NULL == level)
        return 0;

    level = level->next[id.hpg_entry.idx1].pt;
    if (NULL == level)
        return 0;

    phy_addr = level->next[id.hpg_entry.idx2].pa;
    return (phy_addr & HUGEPAGE_MASK) | id.hpg_entry.offset;
}

static inline uint64_t load_key(page_table_t *level, void *virt)
{
    page_index_t id;
    uint64_t phy_addr;

    id.addr = (uintptr_t) virt;

    level = level->next[id.pg_entry.idx0].pt;
    if (NULL == level)
        return 0;

    level = level->next[id.pg_entry.idx1].pt;
    if (NULL == level)
        return 0;

    level = level->next[id.pg_entry.idx2].pt;
    if (NULL == level)
        return 0;

    phy_addr = level->next[id.pg_entry.idx3].pa;
    return phy_addr & ~PAGE_MASK;
}

static inline uint64_t load_key_hpg(page_table_t *level, void *virt)
{
    page_index_t id;
    uint64_t phy_addr;

    id.addr = (uintptr_t) virt;

    level = level->next[id.hpg_entry.idx0].pt;
    if (NULL == level)
        return 0;

    level = level->next[id.hpg_entry.idx1].pt;
    if (NULL == level)
        return 0;

    phy_addr = level->next[id.hpg_entry.idx2].pa;
    /* the hash key is of 4KB long for both normal page and huge page */
    return phy_addr & ~PAGE_MASK;
}

#endif
