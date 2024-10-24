/******************************************************************************
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
 *****************************************************************************/


/******************************************************************************
 *
 * load_test.c
 *
 * Re-uses cpa_sample_code functionality to execute four distinct tests that are
 * replicated across available logical QuickAssist(QA) instances using the
 * sample code multi-threaded framework.
 * The tests are:
 *          AES128-CBC using 4k buffers.
 *          Authenticated SHA512 using 4k buffers.
 *          RSA 1024.
 *          Dynamic Deflate Compression using 8k buffers.
 * These tests are designed to target individual hardware engines via the QA
 * instance handles available and allow for TDP measurements to be taken.
 *
 * The targeting policy is enforced by the configuration files supplied
 * (dh89xxcc_qa_dev<N>.conf).
 *
 *
 *****************************************************************************/
#include "cpa_sample_code_crypto_utils.h"
#include "cpa_sample_code_dc_perf.h"
#include "cpa_sample_code_framework.h"
#include "qae_mem_utils.h"

#include "icp_sal_user.h"


#define NUM_SYMMETRIC_BUFFERS           (100)
#define NUM_ASYMMETRIC_BUFFERS          (100)
#define NUM_LOOPS                       (10000000)

#define NUM_CY_FUNCTIONS                (3)
#define NUM_DC_FUNCTIONS                (1)

#define SINGLE_CORE                     (1)
#define SINGLE_INSTANCE                 (1)

/******************************************************************************
 *
 * Declared types for load test application
 *
 *****************************************************************************/
typedef enum inst_type_s
{
    DC = 0,
    CY
} inst_type_t;

typedef struct load_test_inst_info_s
{
    Cpa16U instanceNumber;
    CpaInstanceInfo2 instanceInfo;
    Cpa32U coreAffinity;

}load_test_inst_info_t;

typedef CpaStatus (*ptr2SetupFunction)( load_test_inst_info_t );

typedef CpaStatus (*ptr2GetInstancesFn)(Cpa16U , CpaInstanceHandle* );

typedef CpaStatus (*ptr2GetInfo2Fn)(const CpaInstanceHandle ,
                                    CpaInstanceInfo2 *);

/******************************************************************************
 *
 * External functions from sample code framework
 *
 *****************************************************************************/
extern CpaStatus qaeMemInit(void);
extern void qaeMemDestroy(void);


/******************************************************************************
 *
 * Initialise SAL and User/Kernel memory mapping driver
 *
 *****************************************************************************/

static CpaStatus loadTestInit(void)
{

    if(CPA_STATUS_SUCCESS != qaeMemInit())
    {
        PRINT_ERR("Could not start qae mem for user space\n");
        PRINT("Has the qaeMemDrv.ko module been loaded?\n");
        return CPA_STATUS_FAIL;
    }

    PRINT("Initializing user space \"LoadTest\" instances...\n");
    if(CPA_STATUS_SUCCESS != icp_sal_userStart("LoadTest"))
    {
        PRINT_ERR("Could not start sal for user space\n");
        qaeMemDestroy();
        return CPA_STATUS_FAIL;
    }
    else
    {
        PRINT("\"LoadTest\" instances initialization completed\n");
    }
    return CPA_STATUS_SUCCESS;
}

/******************************************************************************
 *
 * Shutdown SAL and exit User/Kernel memory mapping driver
 *
 *****************************************************************************/
static void loadTestExit(void)
{
    icp_sal_userStop();
    qaeMemDestroy();
}

/******************************************************************************
 *
 * Get logical instance to core mapping
 *
 *****************************************************************************/
static CpaStatus getCoreAffinity(load_test_inst_info_t * const pInstanceInfo)
{
    Cpa32U i = 0;

    for(i = 0; i < CPA_MAX_CORES; i++)
    {
        if(CPA_BITMAP_BIT_TEST(pInstanceInfo->instanceInfo.coreAffinity,i))
        {
            pInstanceInfo->coreAffinity = i;
            return CPA_STATUS_SUCCESS;
        }
    }
    PRINT_ERR("Could not find core affinity\n");
    return CPA_STATUS_FAIL;
}

/******************************************************************************
 *
 * Get logical instance to core and physical device mapping
 *
 *****************************************************************************/
static CpaStatus getInstanceInfo(load_test_inst_info_t * const pInstanceInfo,
                                    const Cpa16U numInstances,
                                    const inst_type_t instType)
{
    CpaInstanceHandle *pInstanceHandles = NULL;
    Cpa16U i = 0;
    CpaStatus status = CPA_STATUS_FAIL;
    ptr2GetInfo2Fn getInstanceInfo2;
    ptr2GetInstancesFn getInstances;

    switch(instType)
    {
        case(DC):
            getInstanceInfo2 = cpaDcInstanceGetInfo2;
            getInstances = cpaDcGetInstances;
            break;
        case(CY):
            getInstanceInfo2 = cpaCyInstanceGetInfo2;
            getInstances = cpaCyGetInstances;
            break;
        default:
            PRINT_ERR("Unknown Instance Type\n");
            return CPA_STATUS_FAIL;
    }

    pInstanceHandles = qaeMemAlloc(sizeof(CpaInstanceHandle)*numInstances);
    if(NULL == pInstanceHandles)
    {
        PRINT_ERR("Cannot allocate memory for Instance Handles\n");
        return CPA_STATUS_FAIL;
    }

    if(CPA_STATUS_SUCCESS !=
            getInstances(numInstances, pInstanceHandles))
    {
        PRINT_ERR("Get Instances failed\n");
        qaeMemFree((void**)&pInstanceHandles);
        return CPA_STATUS_FAIL;
    }
    for(i = 0; i < numInstances; i++)
    {
        status = getInstanceInfo2(pInstanceHandles[i],
                                    &pInstanceInfo[i].instanceInfo);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("could not get instance info\n");
            qaeMemFree((void**)&pInstanceHandles);
            return CPA_STATUS_FAIL;
        }
        status = getCoreAffinity(&pInstanceInfo[i]);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("getCoreAffinity failed\n");
            qaeMemFree((void**)&pInstanceHandles);
            return CPA_STATUS_FAIL;
        }

        pInstanceInfo[i].instanceNumber = i;
    }
    qaeMemFree((void**)&pInstanceHandles);
    return CPA_STATUS_SUCCESS;
}


/******************************************************************************
 *
 * AES128-CBC test targeting Cipher Engines
 *
 *****************************************************************************/
static CpaStatus setupAESCipherTest(load_test_inst_info_t instanceInfo)
{
    CpaStatus status = CPA_STATUS_FAIL;
    status = setupCipherTest(
            CPA_CY_SYM_CIPHER_AES_CBC,
            KEY_SIZE_128_IN_BYTES,
            CPA_CY_PRIORITY_NORMAL,
            ASYNC,
            BUFFER_SIZE_2048,
            DEFAULT_CPA_FLAT_BUFFERS_PER_LIST,
            NUM_SYMMETRIC_BUFFERS,
            NUM_LOOPS);
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error setting up Cipher(AES_CBC) Test with status %d\n",
                    status);
        return status;
    }
    PRINT("Creating Cipher(AES_CBC) thread:      instance %3d, core %2d, "
            "device %2d, accelerator %2d, engine %2d\n",
            instanceInfo.instanceNumber,
            instanceInfo.coreAffinity,
            instanceInfo.instanceInfo.physInstId.packageId,
            instanceInfo.instanceInfo.physInstId.acceleratorId,
            instanceInfo.instanceInfo.physInstId.executionEngineId);

    status = createPerfomanceThreads( SINGLE_CORE, &instanceInfo.coreAffinity,
                                      SINGLE_INSTANCE,
                                      instanceInfo.instanceNumber );
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error creating Cipher Test threads with status %d\n",
                    status);
    }
    return status;
}


/******************************************************************************
 *
 * Authenticated Hash(SHA512) test targeting Auth Engines
 *
 *****************************************************************************/
static CpaStatus setupAuthTest(load_test_inst_info_t instanceInfo)
{
    CpaStatus status = CPA_STATUS_FAIL;
    status = setupHashTest(
            CPA_CY_SYM_HASH_SHA512,
            CPA_CY_SYM_HASH_MODE_AUTH,
            SHA512_DIGEST_LENGTH_IN_BYTES,
            CPA_CY_PRIORITY_HIGH,
            ASYNC,
            BUFFER_SIZE_2048,
            NUM_SYMMETRIC_BUFFERS,
            NUM_LOOPS);
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error setting up Hash(SHA512) Test with status %d\n",status);
        return status;
    }

    PRINT("Creating Hash(Auth SHA512) thread:    instance %3d, core %2d, "
            "device %2d, accelerator %2d, engine %2d\n",
            instanceInfo.instanceNumber,
            instanceInfo.coreAffinity,
            instanceInfo.instanceInfo.physInstId.packageId,
            instanceInfo.instanceInfo.physInstId.acceleratorId,
            instanceInfo.instanceInfo.physInstId.executionEngineId);

    status = createPerfomanceThreads( SINGLE_CORE, &instanceInfo.coreAffinity,
                                      SINGLE_INSTANCE,
                                      instanceInfo.instanceNumber );
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error creating Hash(SHA512) thread with status %d\n",status);
    }
    return status;
}


/******************************************************************************
 *
 * RSA mod 1024 Type 2 test targeting PKE Engines
 *
 *****************************************************************************/
static CpaStatus setupPkeTest(load_test_inst_info_t instanceInfo)
{
    CpaStatus status = CPA_STATUS_FAIL;
    status = setupRsaTest(MODULUS_1024_BIT,
            CPA_CY_RSA_PRIVATE_KEY_REP_TYPE_2,
            ASYNC,
            NUM_ASYMMETRIC_BUFFERS,
            NUM_LOOPS );
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error setting up RSA Test with status %d\n",status);
        return status;
    }

    PRINT("Creating RSA(mod 1024 Type 2) thread: instance %3d, core %2d, "
            "device %2d, accelerator %2d, engine %2d\n",
            instanceInfo.instanceNumber,
            instanceInfo.coreAffinity,
            instanceInfo.instanceInfo.physInstId.packageId,
            instanceInfo.instanceInfo.physInstId.acceleratorId,
            instanceInfo.instanceInfo.physInstId.executionEngineId);

    status = createPerfomanceThreads( SINGLE_CORE, &instanceInfo.coreAffinity,
                                      SINGLE_INSTANCE,
                                      instanceInfo.instanceNumber );
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error creating RSA(mod 1024) Test threads with status %d\n",
                status);
    }
    return status;
}

/******************************************************************************
 *
 * Dynamic Deflate 8k (Calgary Corpus) test targeting Compression and
 * Translator Engines
 *
 *****************************************************************************/
static CpaStatus setupCompressionTest(load_test_inst_info_t instanceInfo)
{
    CpaStatus status = CPA_STATUS_FAIL;
    status = setupDcTest( CPA_DC_DEFLATE,
            CPA_DC_DIR_COMPRESS,
            CPA_DC_L3,
            CPA_DC_HT_FULL_DYNAMIC,
            CPA_DC_FT_ASCII,
            CPA_DC_STATELESS,
            DEFAULT_COMPRESSION_WINDOW_SIZE,
            BUFFER_SIZE_8192,
            CALGARY_CORPUS,
            CPA_SAMPLE_ASYNCHRONOUS,
            NUM_LOOPS );
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error setting up Compression Test with status %d\n",
                    status);
        return status;
    }

    PRINT("Creating Compression thread:          instance %3d, core %2d, "
            "device %2d, accelerator %2d\n",
            instanceInfo.instanceNumber,
            instanceInfo.coreAffinity,
            instanceInfo.instanceInfo.physInstId.packageId,
            instanceInfo.instanceInfo.physInstId.acceleratorId);

    status = createPerfomanceThreads( SINGLE_CORE, &instanceInfo.coreAffinity,
                                    SINGLE_INSTANCE,
                                    instanceInfo.instanceNumber );
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Error creating Compression Test threads with status %d\n",
                    status);
    }
    return status;
}

/******************************************************************************
 *
 * Setup the compression test threads.
 *
 *****************************************************************************/
static CpaStatus setupCompressionThreads(Cpa16U * const numInstancesFound)
{
    CpaStatus status = CPA_STATUS_FAIL;
    load_test_inst_info_t *pDcInstanceInfo;
    Cpa16U numDcInstances = 0, i = 0;
    /* Declare DC function pointer array */
    ptr2SetupFunction dcFuncionArray[NUM_DC_FUNCTIONS] =
                                            { &setupCompressionTest };

    if(CPA_STATUS_SUCCESS != cpaDcGetNumInstances(&numDcInstances))
    {
        PRINT_ERR("cpaDcGetNumInstances failed\n");
        *numInstancesFound = 0;
        return CPA_STATUS_FAIL;
    }

    *numInstancesFound = numDcInstances;
    /* If no instances are available, print a warning as device under test may
     * not have Compression available and has been disabled.
     */

    if(0 == numDcInstances)
    {
        PRINT("Warning: No Data Compression Instances present\n");
        return CPA_STATUS_SUCCESS;
    }

    pDcInstanceInfo = qaeMemAlloc(sizeof(load_test_inst_info_t)*numDcInstances);
    if(NULL == pDcInstanceInfo)
    {
        PRINT_ERR("Error allocating Data Compression Instance info\n");
        return CPA_STATUS_FAIL;
    }

    if(CPA_STATUS_SUCCESS !=
            getInstanceInfo(pDcInstanceInfo,numDcInstances,DC))
    {
        PRINT_ERR("Cannot determine the mapping of Data Compression instances "
                "to cores and HW device information\n");
        qaeMemFree((void**)&pDcInstanceInfo);
        return CPA_STATUS_FAIL;
    }

    PRINT("Creating Compression Tests across %d logical instances\n",
            numDcInstances);

    for(i = 0; i < numDcInstances; i++)
    {
        status = dcFuncionArray[i % NUM_DC_FUNCTIONS](pDcInstanceInfo[i]);
        if( CPA_STATUS_SUCCESS != status )
        {
            PRINT_ERR("Error calling Data Compression setup function\n");
            qaeMemFree((void**)&pDcInstanceInfo);
            return CPA_STATUS_FAIL;
        }
    }
    qaeMemFree((void**)&pDcInstanceInfo);
    return CPA_STATUS_SUCCESS;
}


/******************************************************************************
 *
 * Setup the crypto test threads.
 *
 *****************************************************************************/
static CpaStatus setupCryptoThreads(Cpa16U * const numInstancesFound)
{
    /* Declare and initialise local and status variables */
    CpaStatus status = CPA_STATUS_FAIL;
    load_test_inst_info_t *pCyInstanceInfo;
    Cpa16U numCyInstances = 0, i = 0;

    /* Declare CY function pointer array */
    ptr2SetupFunction cyFuncionArray[NUM_CY_FUNCTIONS] = {
            &setupAESCipherTest,
            &setupAuthTest,
            &setupPkeTest
    };

    if(CPA_STATUS_SUCCESS != cpaCyGetNumInstances(&numCyInstances))
    {
        PRINT_ERR("cpaDcGetNumInstances failed\n");
        *numInstancesFound = 0;
        return CPA_STATUS_FAIL;
    }
    *numInstancesFound = numCyInstances;

    /* If no instances are available, print a warning as device under test may
     * have Crypto disabled.
     */
    if(0 == numCyInstances)
    {
        PRINT("Warning No Crypto Instances present\n");
        return CPA_STATUS_SUCCESS;
    }

    pCyInstanceInfo = qaeMemAlloc(sizeof(load_test_inst_info_t)*numCyInstances);
    if(NULL == pCyInstanceInfo)
    {
        PRINT_ERR("Error allocating Crypto Instance info\n");
        return CPA_STATUS_FAIL;
    }

    if(CPA_STATUS_SUCCESS !=
            getInstanceInfo(pCyInstanceInfo,numCyInstances,CY))
    {
        PRINT_ERR("Cannot determine the mapping of Crypto instances "
                "to cores\n");
        qaeMemFree((void**)&pCyInstanceInfo);
        return CPA_STATUS_FAIL;
    }

    PRINT("Creating Crypto Tests across %d logical instances\n",
            numCyInstances);

    for(i = 0; i < numCyInstances; i++)
    {
        status = cyFuncionArray[i % NUM_CY_FUNCTIONS](pCyInstanceInfo[i]);
        if( CPA_STATUS_SUCCESS != status )
        {
            PRINT_ERR("Error calling Crypto setup function\n");
            qaeMemFree((void**)&pCyInstanceInfo);
            return CPA_STATUS_FAIL;
        }
    }
    qaeMemFree((void**)&pCyInstanceInfo);
    return CPA_STATUS_SUCCESS;
}


/******************************************************************************
 *
 * Execute test threads
 *
 *****************************************************************************/
CpaStatus runTests(void)
{
    Cpa16U numDcInstances = 0, numCyInstances = 0;

    if(CPA_STATUS_SUCCESS != setupCryptoThreads(&numCyInstances))
    {
        PRINT_ERR("Error setting up Crypto tests\n");
        return CPA_STATUS_FAIL;
    }

    if(CPA_STATUS_SUCCESS != setupCompressionThreads(&numDcInstances))
    {
        PRINT_ERR("Error setting up Compression Tests\n");
        return CPA_STATUS_FAIL;
    }
    if(numDcInstances == 0 && numCyInstances == 0)
    {
        PRINT_ERR("No instances found for both Crypto and Compression "
                    "services\n");
        return CPA_STATUS_FAIL;
    }
    /* Call start threads. Return if fail */
    if( CPA_STATUS_SUCCESS != startThreads() )
    {
        PRINT_ERR("Error starting test threads\n");
        return CPA_STATUS_FAIL;
    }
    PRINT("Starting test execution, press Ctl-c to exit\n");

    return waitForThreadCompletion();
}

int main(int argc, char **argv)
{
    if(CPA_STATUS_SUCCESS != loadTestInit())
    {
        PRINT_ERR("Error initialising load test application\n");
        return CPA_STATUS_FAIL;
    }

    if(CPA_STATUS_SUCCESS != runTests())
    {
        PRINT_ERR("Error running load test application\n");
        loadTestExit();
        return CPA_STATUS_FAIL;
    }
    loadTestExit();
    return CPA_STATUS_SUCCESS;
}
