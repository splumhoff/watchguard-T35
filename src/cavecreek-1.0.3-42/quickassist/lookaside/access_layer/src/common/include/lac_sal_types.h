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
 * @file lac_sal_types.h
 *
 * @ingroup SalCtrl
 *
 * Generic instance type definitions of SAL controller
 *
 ***************************************************************************/

#ifndef LAC_SAL_TYPES_H
#define LAC_SAL_TYPES_H

#include "lac_sync.h"
#include "lac_list.h"
#include "icp_accel_devices.h"
#include "sal_statistics.h"
#include "icp_adf_debug.h"

#define SAL_CFG_BASE_DEC 10
#define SAL_CFG_BASE_HEX 16

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      Instance States
 *
 * @description
 *    An enumeration containing the possible states for an instance.
 *
 *****************************************************************************/
typedef enum sal_service_state_s
{
        SAL_SERVICE_STATE_UNINITIALIZED = 0,
        SAL_SERVICE_STATE_INITIALIZING,
        SAL_SERVICE_STATE_INITIALIZED,
        SAL_SERVICE_STATE_RUNNING,
        SAL_SERVICE_STATE_SHUTTING_DOWN,
        SAL_SERVICE_STATE_SHUTDOWN,
        SAL_SERVICE_STATE_RESTARTING,
        SAL_SERVICE_STATE_END
} sal_service_state_t;

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      Service Instance Types
 *
 * @description
 *      An enumeration containing the possible types for a service.
 *
 *****************************************************************************/
typedef enum
{
        SAL_SERVICE_TYPE_UNKNOWN = 0,
        SAL_SERVICE_TYPE_CRYPTO = 1,
        SAL_SERVICE_TYPE_COMPRESSION = 2,
        SAL_SERVICE_TYPE_QAT = 16
} sal_service_type_t;

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      Generic Instance Container
 *
 * @description
 *      Contains all the common information across the different instances.
 *
 *****************************************************************************/
typedef struct sal_service_s
{
        sal_service_type_t type;
        /**< Service type (e.g. SAL_SERVICE_TYPE_CRYPTO)*/

        Cpa8U state;
        /**< Status of the service instance
           (e.g. SAL_SERVICE_STATE_INITIALIZED) */

        Cpa32U instance;
        /**< Instance number */

        CpaVirtualToPhysical virt2PhysClient;
        /**< Function pointer to client supplied virt_to_phys */


        CpaStatus (*init)(icp_accel_dev_t* device,
                          struct sal_service_s* service);
        /**< Function pointer for instance INIT function */
        CpaStatus (*start)(icp_accel_dev_t* device,
                           struct sal_service_s* service);
        /**< Function pointer for instance START function */
        CpaStatus (*stop)(icp_accel_dev_t* device,
                          struct sal_service_s* service);
        /**< Function pointer for instance STOP function */
        CpaStatus (*shutdown)(icp_accel_dev_t* device,
                              struct sal_service_s* service);
        /**< Function pointer for instance SHUTDOWN function */

        CpaCyInstanceNotificationCbFunc notification_cb;
        /**< Function pointer for instance restarting handler */

        void *cb_tag;
        /**< Restarting handler priv data */

        sal_statistics_collection_t *stats;
        /**< Pointer to device statistics configuration */

        void *debug_parent_dir;
        /**< Pointer to parent proc dir entry */

        CpaBoolean is_dyn;

        Cpa32U capabilitiesMask;
        /**< Capabilities mask of the device */

        Cpa32U dcExtendedFeatures;
        /**< Bit field of features. I.e. Compress And Verify */

        CpaBoolean isInstanceStarted;
        /**< True if user called StartInstance on this instance */
} sal_service_t;

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      SAL structure
 *
 * @description
 *      Contains lists to crypto and compression instances.
 *
 *****************************************************************************/
typedef struct sal_s
{
        sal_list_t* crypto_services;
        /**< Container of sal_crypto_service_t */
        sal_list_t* compression_services;
        /**< Container of sal_compression_service_t */
        debug_dir_info_t *cy_dir;
        /**< Container for crypto proc debug */
        debug_dir_info_t *dc_dir;
        /**< Container for compression proc debug */
        debug_file_info_t *ver_file;
        /**< Container for version debug file */
} sal_t;

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      SAL debug structure
 *
 * @description
 *      Service debug handler
 *
 *****************************************************************************/
typedef struct sal_service_debug_s {
    icp_accel_dev_t *accel_dev;
    debug_file_info_t debug_file;
} sal_service_debug_t;

/**
 *******************************************************************************
 * @ingroup SalCtrl
 *      This macro verifies that the right service type has been passed in.
 *
 * @param[in] pService         pointer to service instance
 * @param[in] service_type     service type to check againstx.
 *
 * @return CPA_STATUS_FAIL      Paramater is incorrect type
  *
 ******************************************************************************/
#define SAL_CHECK_INSTANCE_TYPE(pService, service_type)                     \
do {                                                                        \
    sal_service_t* pGenericService = NULL;                                  \
    pGenericService = (sal_service_t*) pService;                            \
    if(!(service_type & pGenericService->type))                             \
    {                                                                       \
        LAC_LOG_ERROR("The instance handle is the wrong type");             \
        return CPA_STATUS_FAIL;                                             \
    }                                                                       \
} while(0)

#endif
