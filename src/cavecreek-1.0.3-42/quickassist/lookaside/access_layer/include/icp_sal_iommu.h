/***************************************************************************
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
 ***************************************************************************/

/**
 ***************************************************************************
 * @file icp_sal_iommu.h
 *
 * @ingroup SalUser
 *
 * Sal iommu wrapper functions.
 *
 ***************************************************************************/

#ifndef ICP_SAL_IOMMU_H
#define ICP_SAL_IOMMU_H

/*************************************************************************
  * @ingroup Sal
  * @description
  *   Function returns page_size rounded size for iommu remapping
  *
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @reentrant
  *      No
  * @threadSafe
  *      No
  *
  * @param[in] size           Minimum required size.
  *
  * @retval    page_size rounded size for iommu remapping.
  *
  *************************************************************************/
size_t  icp_sal_iommu_get_remap_size(size_t size);

/*************************************************************************
  * @ingroup Sal
  * @description
  *   Function adds an entry into iommu remapping table
  *
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @reentrant
  *      No
  * @threadSafe
  *      No
  *
  * @param[in] phaddr         Host physical address.
  * @param[in] iova           Guest physical address.
  * @param[in] size           Size of the remapped region.
  *
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  *************************************************************************/
CpaStatus icp_sal_iommu_map(Cpa64U phaddr, Cpa64U iova, size_t size);

/*************************************************************************
  * @ingroup Sal
  * @description
  *   Function removes an entry from iommu remapping table
  *
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @reentrant
  *      No
  * @threadSafe
  *      No
  *
  * @param[in] iova           Guest physical address to be removed.
  * @param[in] size           Size of the remapped region.
  *
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  *************************************************************************/
CpaStatus icp_sal_iommu_unmap(Cpa64U iova, size_t size);
#endif
