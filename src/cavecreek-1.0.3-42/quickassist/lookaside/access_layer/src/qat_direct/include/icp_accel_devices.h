/*****************************************************************************
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
/*****************************************************************************
 * @file icp_accel_devices.h
 *
 * @defgroup Acceleration Driver Framework
 *
 * @ingroup icp_Adf
 *
 * @description
 *      This is the top level header file that contains the layout of the ADF
 *      icp_accel_dev_t structure and related macros/definitions.
 *      It can be used to dereference the icp_accel_dev_t *passed into upper
 *      layers.
 *
 *****************************************************************************/

#ifndef ICP_ACCEL_DEVICES_H_
#define ICP_ACCEL_DEVICES_H_
#include "adf_accel_devices.h"

#include "cpa.h"
#include "Osal.h"

#define ADF_CFG_NO_INSTANCE 0xFFFFFFFF

#define ICP_DC_TX_RING_0       6
#define ICP_DC_TX_RING_1       7
#define ICP_RX_RINGS_OFFSET    8
#define ICP_RINGS_PER_BANK     16

#define MAX_ACCEL_NAME_LEN              16
#define ADF_DEVICE_NAME_LENGTH          32
#define ADF_DEVICE_TYPE_LENGTH          8

#define ADF_CTL_DEVICE_NAME "/dev/qat_adf_ctl"

#define ADF_C3XXX_REV_ID_A0     0x00
#define ADF_C62X_REV_ID_A0      0x00

/**
 *****************************************************************************
 * @ingroup icp_AdfAccelHandle
 *
 * @description
 *      Accelerator capabilities
 *
 *****************************************************************************/
typedef enum
{
    ICP_ACCEL_CAPABILITIES_NULL             = 0,
    ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC = 1,
    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC = 2,
    ICP_ACCEL_CAPABILITIES_CIPHER = 4,
    ICP_ACCEL_CAPABILITIES_AUTHENTICATION = 8,
    ICP_ACCEL_CAPABILITIES_REGEX = 16,
    ICP_ACCEL_CAPABILITIES_COMPRESSION = 32,
    ICP_ACCEL_CAPABILITIES_LZS_COMPRESSION = 64,
    ICP_ACCEL_CAPABILITIES_RANDOM_NUMBER = 128,
    ICP_ACCEL_CAPABILITIES_CRYPTO_ZUC = 256,
    ICP_ACCEL_CAPABILITIES_CRYPTO_SHA3 = 512,
    ICP_ACCEL_CAPABILITIES_KPT = 1024
} icp_accel_capabilities_t;

/**
 *****************************************************************************
 * @ingroup icp_AdfAccelHandle
 *
 * @description
 *      Device Configuration Data Structure
 *
 *****************************************************************************/
typedef enum device_type_e
{
    DEVICE_UNKNOWN = 0,
    DEVICE_DH895XCC,
    DEVICE_DH895XCCVF,
    DEVICE_C62X,
    DEVICE_C62XVF,
    DEVICE_C3XXX,
    DEVICE_C3XXXVF,
    DEVICE_D15XX,
    DEVICE_D15XXVF
} device_type_t;

/*
 * Enumeration on Service Type
 */
typedef enum adf_service_type_s
{
    ADF_SERVICE_CRYPTO,
    ADF_SERVICE_COMPRESS,
    ADF_SERVICE_MAX   /* this is always the last one */
} adf_service_type_t;

typedef struct accel_dev_s
{
    /* Some generic information */
    Cpa32U          accelId;
    Cpa32U          aeMask;                 /* Acceleration Engine mask */
    device_type_t   deviceType;             /* Device Type              */
    /* Device name for SAL */
    char            deviceName[ADF_DEVICE_NAME_LENGTH + 1];
    Cpa32U          accelCapabilitiesMask;  /* Accelerator's capabilities
                                               mask */
    Cpa32U          dcExtendedFeatures;       /* bit field of features
                                               bit 0: CNV
                                               bit 1#31: unused */
    OsalAtomic      usageCounter;           /* Usage counter. Prevents
                                               shutting down the dev if not 0*/
    /* Component specific fields - cast to relevent layer */
    void            *pRingInflight;     /* For offload optimization */
    void            *pSalHandle;        /* For SAL*/
    void            *pQatStats;         /* For QATAL/SAL stats */
    void            *ringInfoCallBack;  /* Callback for user space
                                           ring enabling */

    /* Status of ADF and registered subsystems */
    Cpa32U          adfSubsystemStatus;
    /* Physical processor to which the dev is connected */
    Cpa32S          numa_node;
    enum dev_sku_info  sku;
    Cpa8U           devFileName[ADF_DEVICE_NAME_LENGTH];
    Cpa32S          csrFileHdl;
    Cpa32S          ringFileHdl;
    void            *accel;
    /* the revision id of the pcie device */
    Cpa32U          revisionId;

    Cpa32U          maxNumBanks;
    Cpa32U          maxNumRingsPerBank;

    /* pointer to dynamic instance resource manager */
    void            *pInstMgr;
    void *banks;                 /* banks information */ 
} icp_accel_dev_t;

#endif /* ICP_ACCEL_HANDLE_H */
