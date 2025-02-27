/*****************************************************************************
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

/*****************************************************************************
 * @file adf_acceldev_drv.c
 *
 * @description
 *      This file contains the module init/release functions, performs PCI
 *      device driver registration, setup of system resource table and calls
 *      the main acceleration subsystem init and shutdown functions.
 *****************************************************************************/
#include <linux/kernel.h>
#include <linux/version.h>	// WG:JB Get Linux version
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <asm/io.h>

#ifdef	CONFIG_WG_PLATFORM

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,12,0)

#undef	hlist_for_each_entry
#define	hlist_for_each_entry(a,b,c,d)	hlist_for_each_entry_safe(a,b,c,d)

#if LINUX_VERSION_CODE <  KERNEL_VERSION(4,1,0)
static inline struct pci_cap_saved_state *pci_find_saved_cap(struct pci_dev *pci_dev, char cap)
{
	struct pci_cap_saved_state *tmp;
	struct hlist_node *pos;

	hlist_for_each_entry_safe(tmp, pos, &pci_dev->saved_cap_space, next) {
		if (tmp->cap.cap_nr == cap)
			return tmp;
	}
	return NULL;
}
#endif

#endif

#endif	// CONFIG_WG_PLATFORM

#include "cpa.h"
#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "adf_transport_ctrl.h"
#include "adf_ETring_mgr.h"
#include "adf_drv.h"
#include "adf_isr.h"
#include "adf_cfg.h"
#include "adf_init.h"
#include "adf_ae_fw.h"
#include "icp_platform.h"
#include "adf_ctl_drv.h"
#include "adf_devmgr.h"
#include "adf_user_proxy.h"
#include "adf_dev_csr.h"
#include "adf_dev_ring.h"
#include "adf_config_ctl.h"
#include "adf_platform.h"
#include "adf_bh.h"

static int num_device = 0;
char adf_driver_name[] = "adf";

extern CpaStatus adf_esramSetConfigInfo(icp_accel_dev_t *pAccelDev,
                                    Cpa64U physAddr,
                                    UARCH_INT virtAddr,
                                    Cpa32U size);

#define ADF_SYSTEM_DEVICE(device_id) \
        { PCI_DEVICE(PCI_VENDOR_ID_INTEL, device_id) }

/* Device BAR Offset */
#define ADF_BAR_OFFSET      2
#define PCI_DEV_ID_OFFSET   2
#define PCI_FUNCTION_OFFSET 8

/* Max number of PCI buses in system */
#define MAX_PCI_BUS 256

/* Reset vectors */
#define RESET_VECTOR      0x3BC0FF
#define RESET_GIGE_RESET_VECTOR 0x4000000
#define RESET_GIGE_VECTOR 0x7C000000
#define RESET_OFFSET 0xA0C
#define PPDSTAT_OFFSET 0x7E
#define MSLEEP_TIME 100
#define MSLEEP_TRIP_TIME 20
#define WAIT_FOR_TRIP_LOOPS 3
#define GIGES_NUM           4

/*IOV ressources*/
#define PCI_IOV_RESOURCES 7
#define PCI_IOV_RESOURCE_END 11
#define PCI_SRIOV_BAR 0x24

/*Sriov capabilities*/
struct adf_pci_sriov {
    int pos;
    int nres;
    uint32_t cap;
    uint16_t ctrl;
    uint16_t total;
    uint16_t initial;
    uint16_t nr_virtfn;
    uint16_t offset;
    uint16_t stride;
    uint32_t pgsz;
    uint8_t link;
    struct pci_dev *dev;
    struct pci_dev *self;
    struct mutex lock;
    struct work_struct mtask;
    uint8_t __iomem *mstate;
};

/*Capability structure*/
struct adf_pci_capabilities {
    int8_t cap_nr;
    uint16_t size;
    uint32_t data[0];
};

/*Capability list element*/
struct adf_pci_cap_saved_state {
    struct hlist_node next;
    struct adf_pci_capabilities cap;
};

/*Pci backup structure*/
struct adf_pci_saved_state {
    uint32_t config_space[16];
    struct adf_pci_capabilities cap[0];
};

/*
 * PCI Infrastructure prototypes/definitions
 */
static struct pci_device_id adf_pci_tbl[] = {
        ADF_SYSTEM_DEVICE( ICP_DH89xxCC_PCI_DEVICE_ID ),
        ADF_SYSTEM_DEVICE( ICP_SXXXX_PCI_DEVICE_ID ),
        ADF_SYSTEM_DEVICE( ICP_DH89xxCC_PCI_IOV_DEVICE_ID ),
        ADF_SYSTEM_DEVICE( ICP_SXXXX_PCI_IOV_DEVICE_ID ),
        {0,}
};
MODULE_DEVICE_TABLE(pci, adf_pci_tbl);

/*
 * Increase module usage counter function
 */
extern int adf_get_module(void);
/*
 * Decrease module usage counter function
 */
extern void adf_put_module(void);

static int adf_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void adf_remove(struct pci_dev *pdev);

static struct pci_driver adf_driver = {
        .id_table = adf_pci_tbl,
        .name = adf_driver_name,
        .probe = adf_probe,
        .remove =  adf_remove
};

/*
 * Module init/exit prototypes
 */
static int __devinit adfdrv_init(void);
static void adfdrv_release(void);
static void adf_free_coherent_buf(icp_accel_dev_t *accel_dev);

/*
 * adf_cleanup_vf
 * Cleans up any system resources used by an vf
 */
static void adf_cleanup_vf(icp_accel_pci_vf_info_t *vf)
{
        if (vf) {
                adf_cleanup_vf_bh(vf);
                ICP_FREE(vf->vf_ring_info_tbl);
                ICP_FREE(vf);
        }
}

/*
 * adf_cleanup_accel
 * Cleans up any system resources used by an accelerator structure
 */
static int adf_cleanup_accel(icp_accel_dev_t * accel_dev)
{
        int status = 0, i = 0;
        icp_accel_pci_info_t *pci_info = NULL;

        if (accel_dev == NULL) {
                ADF_ERROR("NULL accelerator structure\n");
                return -EFAULT;
        }

        if (accel_dev->pciAccelDev.pDev) {
                pci_release_regions(accel_dev->pciAccelDev.pDev);
                pci_disable_device(accel_dev->pciAccelDev.pDev);
        }

        if (NULL != accel_dev->pci_vfs) {
                ICP_FREE_NUMA(accel_dev->pci_vfs);
        }

        status = adf_cfgDeviceRemove(accel_dev->accelId);
        if (status) {
                ADF_ERROR("adf_cfgDeviceRemove failed, id =%d\n",
                              accel_dev->accelId);
        }

        /* unmap the pcidev bars */
        pci_info = &accel_dev->pciAccelDev;
        for (i = 0; i < ICP_PCI_MAX_BARS; i++) {
                if (pci_info->pciBars[i].virtAddr) {
                        pci_iounmap(pci_info->pDev, (void __iomem *)
                                   (UARCH_INT) pci_info->pciBars[i].virtAddr);
                        pci_info->pciBars[i].virtAddr = (UARCH_INT) NULL;
                }
        }

        /* Remove accelerator device from accelerator table */
        status = adf_devmgrRemoveAccelDev(accel_dev);

        /* Remove DMA buffers */
        adf_free_coherent_buf(accel_dev);

        if (NULL != accel_dev->pHwDeviceData) {
            /* clean dev config structure if needed                         */
            if (NOT_NULL_FUNCTION(accel_dev->pHwDeviceData->cleanup)) {
                accel_dev->pHwDeviceData->cleanup(accel_dev->pHwDeviceData);
            }
            ICP_FREE_NUMA(accel_dev->pHwDeviceData);
            accel_dev->pHwDeviceData = NULL;
        }

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,0,0)
        ICP_FREE(accel_dev->pci_state);
#else
        if(accel_dev->pci_state)
                kfree(accel_dev->pci_state);
#endif
        ICP_FREE(accel_dev);
        return status;
}

/*
 * adf_get_dev_node_id
 *
 * Function determines to what physical die the pci dev is connected to.
 */
static int adf_get_dev_node_id(struct pci_dev *pdev)
{
#ifdef CONFIG_X86
        unsigned int bus_per_cpu = 0;
        struct cpuinfo_x86 *c = &cpu_data(num_online_cpus()-1);

        ADF_DEBUG("CPU INFO: \n"
                  "processors\t: %u\n"
                  "vendor_id\t: %s\n"
                  "cpu family\t: %d\n"
                  "model\t\t: %u\n"
                  "model name\t: %s\n",
                   num_online_cpus(),
                   c->x86_vendor_id,
                   c->x86,
                   c->x86_model,
                   c->x86_model_id);

        /* if there is only one physical processor don't need
         * to do any further calculations */
        if (c->phys_proc_id == 0)
                return 0;
        bus_per_cpu = MAX_PCI_BUS / (c->phys_proc_id + 1);
        if (0 != bus_per_cpu) {
                return pdev->bus->number / bus_per_cpu;
        }
#endif
        return 0;
}

/*
 * adf_vf_probe
 * ICP ADF Device Driver Probe function for vf
 */
static int adf_vf_probe(struct pci_dev *pdev)
{
        struct pci_dev *pdev_pf = pdev->physfn;
        icp_accel_pci_vf_info_t *vf = NULL;
        int dev_nr = 0;
        Cpa32U numRingsPerBank = 0;
        icp_accel_dev_t *accel_dev = adf_devmgrGetAccelDev(pdev_pf);

        if (!accel_dev) {
                ADF_ERROR("Can not find PF device %p\n", pdev_pf);
                return -ENODEV;
        }

        /* Calculate vf device number */
        dev_nr = ((PCI_SLOT(pdev->devfn) - 1) * PCI_FUNCTION_OFFSET)
                                              + PCI_FUNC(pdev->devfn);

        if (accel_dev->pci_vfs[dev_nr]) {
                vf = accel_dev->pci_vfs[dev_nr];
                vf->attached = 1;
                if(!BIT_IS_SET(accel_dev->adfSubsystemStatus,
                                                ADF_STATUS_SYSTEM_STARTED))
                {
                        /* Dont enable msix here as we are starting */
                        return SUCCESS;
                }
                /* This means that a guest went down
                 * need to sent ring info updates for all rings
                 * allocated by guest on this vf */
                adf_vf_clean_ring_info(vf);
                /* Decrease module ref counter */
                adf_put_module();
                osalAtomicDec(&accel_dev->usageCounter);
                return adf_enable_bundle_msix(accel_dev, dev_nr);
        }

        vf = ICP_ZALLOC_GEN(sizeof(icp_accel_pci_vf_info_t));
        if (!vf) {
                ADF_ERROR("failed to allocate vf structure\n");
                return -ENOMEM;
        }

        numRingsPerBank = GET_NUM_RINGS_PER_BANK(accel_dev);
        vf->vf_ring_info_tbl = ICP_ZALLOC_GEN(numRingsPerBank * sizeof(void*));
        if (!vf->vf_ring_info_tbl) {
                ICP_FREE(vf);
                ADF_ERROR("failed to allocate memory for vf ring info tbl\n");
                return -ENOMEM;
        }
        vf->deviceId = dev_nr;
        vf->attached = 1;
        accel_dev->pci_vfs[dev_nr] = (void*)vf;
        vf->accel_dev = accel_dev;
        vf->pDev = pdev;

        if (adf_setup_vf_bh(vf)) {
                ADF_ERROR("Failed init bottom half for VF\n");
                adf_cleanup_vf(vf);
        }
        return SUCCESS;
}

/*
 * adf_free_coherent_buf
 * Frees DMA buffers
 */
static void adf_free_coherent_buf(icp_accel_dev_t *accel_dev)
{
        dma_addr_t dma_handle;
        int i = 0;

        if (accel_dev->pQatCpuMem && accel_dev->pQatDmaMem) {
                for (i = 0; i < GET_MAX_ACCEL(accel_dev); i++) {
                       if (accel_dev->pQatCpuMem[i]) {
                                dma_handle =
                                    (dma_addr_t)accel_dev->pQatDmaMem[i];
                                dma_free_coherent(&(GET_DEV(accel_dev)),
                                                  PAGE_SIZE,
                                                  accel_dev->pQatCpuMem[i],
                                                  dma_handle);
                                accel_dev->pQatCpuMem[i] = NULL;
                        }
                }
                ICP_FREE_NUMA(accel_dev->pQatCpuMem);
                ICP_FREE_NUMA(accel_dev->pQatDmaMem);
        }
}

/*
 * adf_alloc_coherent_buf
 * Allocates DMA buffers for QAT
 */
static int __devinit adf_alloc_coherent_buf(icp_accel_dev_t *accel_dev)
{
        dma_addr_t dma_handle;
        int i = 0;
        Cpa32U maxNumAccel = GET_MAX_ACCEL(accel_dev);

        accel_dev->pQatCpuMem = (void *)
                ICP_MALLOC_GEN_NUMA(sizeof(void *) * maxNumAccel,
                accel_dev->pkg_id);
        if (!accel_dev->pQatCpuMem)
                return -ENOMEM;

        accel_dev->pQatDmaMem = (Cpa64U *)
                ICP_MALLOC_GEN_NUMA(sizeof(Cpa64U) * maxNumAccel,
                accel_dev->pkg_id);
        if (!accel_dev->pQatDmaMem) {
                ICP_FREE_NUMA(accel_dev->pQatCpuMem);
                return -ENOMEM;
        }

        for (i = 0; i < maxNumAccel; i++) {
                accel_dev->pQatCpuMem[i] = dma_alloc_coherent(
                                                   &(GET_DEV(accel_dev)),
                                                   PAGE_SIZE,
                                                   &dma_handle,
                                                   GFP_KERNEL);
                if (!accel_dev->pQatCpuMem[i]) {
                        adf_free_coherent_buf(accel_dev);
                        return -ENOMEM;
                }
                accel_dev->pQatDmaMem[i] = (Cpa64U)dma_handle;
        }
        return SUCCESS;
}

#if (!(LINUX_VERSION_CODE > KERNEL_VERSION(3,0,0)))
/*
* adf_restore_pci_state
* Restore PCI state after a reset
*/
static void adf_restore_pci_state(struct pci_dev *pdev, void *bk_state)
{

        struct adf_pci_saved_state *state;
        struct adf_pci_capabilities *cap;
        struct adf_pci_cap_saved_state *tmp;
        struct adf_pci_sriov *iov;
        uint16_t *ptr;
        int8_t i;
        int32_t base;

        if (!bk_state || !pdev->state_saved) {
                ADF_ERROR("No Backup found for this device\n");
                return;
        }

        state = (struct adf_pci_saved_state *)bk_state;

        /*restore dev config space*/
        memcpy(pdev->saved_config_space, state->config_space,
                        sizeof(state->config_space));
        /*restore capabilities*/
        cap = state->cap;
        while (cap->size) {
                struct hlist_node *pos;
                tmp = NULL;
                hlist_for_each_entry(tmp, pos, &pdev->saved_cap_space, next) {
                        if (tmp->cap.cap_nr == cap->cap_nr) {
                                break;
                        }
                }

                if (!tmp || tmp->cap.size != cap->size)
                {
                        ADF_ERROR("Capabilities restoration failure\n");
                        return;
                }

                memcpy(tmp->cap.data, cap->data, tmp->cap.size);
                cap = (struct adf_pci_capabilities *)((uint8_t *)cap
                                + sizeof(struct adf_pci_cap_saved_state) +
                                cap->size);
        }
        /*restore pci-e capabilities*/
        tmp = (struct adf_pci_cap_saved_state *) pci_find_saved_cap(pdev,
                PCI_CAP_ID_EXP);
        base = pci_find_capability(pdev, PCI_CAP_ID_EXP);
        if (!tmp || base <= 0) {
                ADF_ERROR("Capacities buffer not found in %s\n", __func__);
                return;
        }
        ptr = (uint16_t *)&tmp->cap.data[0];
        i = 0;
        pci_write_config_word(pdev, base + PCI_EXP_DEVCTL, ptr[i++]);
        pci_write_config_word(pdev, base + PCI_EXP_LNKCTL, ptr[i++]);
        pci_write_config_word(pdev, base + PCI_EXP_SLTCTL, ptr[i++]);
        pci_write_config_word(pdev, base + PCI_EXP_RTCTL, ptr[i++]);
        pci_write_config_word(pdev, base + PCI_EXP_DEVCTL2, ptr[i++]);
        pci_write_config_word(pdev, base + PCI_EXP_LNKCTL2, ptr[i++]);
        pci_write_config_word(pdev, base + PCI_EXP_SLTCTL2, ptr[i++]);
        /*reconfigure config space*/
        for (i = 15; i >= 0; i--)
        {
                uint32_t val;
                int loop = 0;
                do
                {
                        pci_write_config_dword(pdev,
                                        i * 4, pdev->saved_config_space[i]);
                        pci_read_config_dword(pdev, i * 4, &val);
                        msleep(MSLEEP_TIME);
                } while(val != pdev->saved_config_space[i] || (loop >= 5));
        }
        /*restore MSI state*/
        pci_restore_msi_state(pdev);
        /*restore iov state*/
        iov = (struct adf_pci_sriov *)pdev->sriov;
        for (i = PCI_IOV_RESOURCES; i <= PCI_IOV_RESOURCE_END; i++) {
                struct pci_bus_region region;
                uint32_t new, check, mask;
                int reg;
                struct resource *res = pdev->resource + i;
                reg = iov->pos + PCI_SRIOV_BAR +
                        4 * (i - PCI_IOV_RESOURCES);
                pcibios_resource_to_bus(pdev, &region, res);
                new = region.start | (res->flags & PCI_REGION_FLAG_MASK);
                mask = (uint32_t)PCI_BASE_ADDRESS_MEM_MASK;

                pci_write_config_dword(pdev, reg, new);
                pci_read_config_dword(pdev, reg, &check);
                if ((new ^ check) & mask) {
                        ADF_ERROR("BAR %d: error updating (%#08x != %#08x)\n",
                                        i, new, check);
                        continue;
                }
                res->flags &= ~IORESOURCE_UNSET;
        }
        pci_write_config_dword(pdev, iov->pos + PCI_SRIOV_SYS_PGSIZE,
                iov->pgsz);
        pci_write_config_word(pdev, iov->pos + PCI_SRIOV_NUM_VF,
                iov->nr_virtfn);
        pci_write_config_word(pdev, iov->pos + PCI_SRIOV_CTRL,
                iov->ctrl);
}

/*
 * Store PCI state at probing time
 */
static void* adf_store_pci_state(struct pci_dev *pdev,
        uint32_t node)
{
        uint16_t *ptr, i;
        int32_t base;
        struct adf_pci_saved_state *state;
        struct adf_pci_cap_saved_state *tmp;
        struct adf_pci_capabilities *cap;
        struct hlist_node *pos;
        size_t size;

        /*save config space*/
        for (i = 0; i < 16 ; i++)
        {
                pci_read_config_dword(pdev,
                                i * 4, &pdev->saved_config_space[i]);
        }

        /*back up pci-e capabilities*/
        tmp = (struct adf_pci_cap_saved_state *) pci_find_saved_cap(pdev,
                PCI_CAP_ID_EXP);
        if (!tmp) {
                ADF_ERROR("Capabities buffer not found in %s\n", __func__);
                return NULL;
        }
        base = pdev->pcie_cap;
        ptr = (uint16_t *)&tmp->cap.data[0];
        i = 0;
        pci_read_config_word(pdev, base + PCI_EXP_DEVCTL, &ptr[i++]);
        pci_read_config_word(pdev, base + PCI_EXP_LNKCTL, &ptr[i++]);
        pci_read_config_word(pdev, base + PCI_EXP_SLTCTL, &ptr[i++]);
        pci_read_config_word(pdev, base + PCI_EXP_RTCTL, &ptr[i++]);
        pci_read_config_word(pdev, base + PCI_EXP_DEVCTL2, &ptr[i++]);
        pci_read_config_word(pdev, base + PCI_EXP_LNKCTL2, &ptr[i++]);
        pci_read_config_word(pdev, base + PCI_EXP_SLTCTL2, &ptr[i++]);
        /*compute backup size*/
        size = sizeof(*state) + sizeof(struct adf_pci_cap_saved_state);
        hlist_for_each_entry(tmp, pos, &pdev->saved_cap_space, next)
                size += sizeof(struct adf_pci_cap_saved_state) + tmp->cap.size;
        /*alloc the backup structure*/
        state = ICP_MALLOC_GEN_NUMA(size, node);
        if (!state) {
                ADF_ERROR("failed to alloc %d bytes on node %d"
                        "to backup the pci state\n",
                                size, node);
                return NULL;
        }
        /* copy config space into the backup struct */
        memcpy(state->config_space, pdev->saved_config_space,
                        sizeof(state->config_space));
        /*copy the capabilities into the back-up*/
        cap = state->cap;
        hlist_for_each_entry(tmp, pos, &pdev->saved_cap_space, next) {
                size_t len = sizeof(struct adf_pci_capabilities) +
                    tmp->cap.size;
                memcpy(cap, &tmp->cap, len);
                cap = (struct adf_pci_capabilities *)((uint8_t *)cap + len);
        }
        /*backup complete and return the allocated structure*/
        pdev->state_saved = true;
        return (void *)state;
}
#endif

/*
 * adf_probe
 * ICP ADF Device Driver Probe function
 */
static int __devinit adf_probe(struct pci_dev *pdev,
                               const struct pci_device_id *ent)
{
        int i = 0, status = 0, bar_index = 0;
        uint8_t rev_id = 0;
        uint32_t *pmisc_bar_addr = NULL;
        icp_accel_dev_t *accel_dev = NULL;
        icp_accel_pci_info_t *pci_info = NULL;
        adf_hw_device_data_t *hw_data = NULL;
        uint32_t sramBarId = 0;
        Cpa32U pmisc_bar_id = 0;
        Cpa32U smia_offset = 0, smia_mask = 0;
        Cpa32U fusectl_offset = 0, clk = 0;
        Cpa32U accfunc_offset = 0, accfunc = 0;

        /* Ensure have valid device */
        switch (ent->device) {
            case ICP_DH89xxCC_PCI_DEVICE_ID:
            case ICP_SXXXX_PCI_DEVICE_ID:
                    break;
            case ICP_DH89xxCC_PCI_IOV_DEVICE_ID:
            case ICP_SXXXX_PCI_IOV_DEVICE_ID:
                    return adf_vf_probe(pdev);
                    break;
            default:
                    ADF_ERROR("invalid device 0x%x found.\n", ent->device);
        }

        if (ADF_MAX_DEVICES == num_device) {
                ADF_ERROR("Only support upto %d devices\n", ADF_MAX_DEVICES);
                return -EIO;
        }

        /* Allocate memory for the accel device. */
        accel_dev = ICP_ZALLOC_GEN(sizeof(icp_accel_dev_t));
        if (NULL == accel_dev) {
                ADF_ERROR("failed to allocate accel structure\n");
                return -ENOMEM;
        }
        /* Set accel id */
        accel_dev->accelId = num_device;

        /* Create device configuration table */
        if (SUCCESS != adf_cfgDeviceAdd(num_device)) {
                adf_cleanup_accel(accel_dev);
                return -ENOMEM;
        }
        pci_info = &accel_dev->pciAccelDev;

        /* Get device node */
        accel_dev->pkg_id = adf_get_dev_node_id(pdev);

        /* Allocate memory for device configuration structure */
        hw_data = ICP_MALLOC_GEN_NUMA(sizeof(adf_hw_device_data_t),
                                                accel_dev->pkg_id);
        if (NULL == hw_data) {
                ADF_ERROR("Failed to allocate memory for device config\n");
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }

        ICP_MEMSET(hw_data, '\0', sizeof(adf_hw_device_data_t));
        accel_dev->pHwDeviceData = hw_data;
        switch (ent->device) {
              case ICP_DH89xxCC_PCI_DEVICE_ID:
                    adf_set_hw_data_dh89xxcc((void *)accel_dev->pHwDeviceData);
                    break;
              case ICP_SXXXX_PCI_DEVICE_ID:
                    adf_sxxxx_set_hw_data((void *)accel_dev->pHwDeviceData);
                    break;
              default:
                    ADF_ERROR("Unsupported device id\n");
                    adf_cleanup_accel(accel_dev);
                    return -EIO;
        }

        /* Initialize device config */
        if (NOT_NULL_FUNCTION(hw_data->init)) {
                status = hw_data->init(hw_data, accel_dev->pkg_id);
                if (CPA_STATUS_FAIL == status) {
                        ADF_ERROR("Failed to initialize hw_data init\n");
                        adf_cleanup_accel(accel_dev);
                        return -EIO;
                }
        }

        /* Retrieve the revision id and add it to the PCI info structure */
        pci_read_config_byte(pdev, ICP_PRID_OFFSET, &rev_id);
        pci_info->revisionId = rev_id;

        /* Read the fusectl register */
        if (NOT_NULL_FUNCTION(hw_data->getDevFuseOffset)) {
                fusectl_offset = hw_data->getDevFuseOffset();
                pci_read_config_dword(pdev, fusectl_offset,
                                      &pci_info->devFuses);
        }

        /* Get Accelerators and Accelerators Engines masks */
        accel_dev->accelMask =
            hw_data->getAcceleratorsMask(pci_info->devFuses);
        accel_dev->aeMask =
            hw_data->getAccelEnginesMask(pci_info->devFuses);

        /* Read the clkctl register */
        if (NOT_NULL_FUNCTION(hw_data->getDevClkOffset)) {
                Cpa32U ckl_offset = hw_data->getDevClkOffset();
                pci_read_config_dword(pdev, ckl_offset, &clk);
        }
        /* Get the device SKU */
        if (NOT_NULL_FUNCTION(hw_data->getDevSKU)) {
            accel_dev->sku =
                hw_data->getDevSKU(hw_data->getNumAccelerators(hw_data,
                                   accel_dev->accelMask),
                                   hw_data->getNumAccelEngines(hw_data,
                                   accel_dev->aeMask), clk,
                                   pci_info->devFuses);
        }

        /* Setup PCI domain information */
        pci_info->pciDomain.domain = pci_domain_nr(pdev->bus);
        pci_info->pciDomain.bus = pdev->bus->number;
        pci_info->pciDomain.slot = PCI_SLOT(pdev->devfn);
        pci_info->pciDomain.function = PCI_FUNC(pdev->devfn);

        /*
         * If the device has no acceleration engines the
         * device needs to be freed and ignored.
         */
        if (!accel_dev->aeMask || !accel_dev->accelMask ||
                            ((~accel_dev->aeMask) & 0x01)) {
                ADF_ERROR("%04x:%02x:%02x.%d: "
                          "There is no acceleration associated "
                          "with this device.\n",
                          pci_info->pciDomain.domain,
                          pci_info->pciDomain.bus,
                          pci_info->pciDomain.slot,
                          pci_info->pciDomain.function);
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }

        num_device++;
        /* enable PCI device */
        if (pci_enable_device(pdev)) {
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }

        /* Setup PCI Info structure for upper layers */
        pci_info->pDev = pdev;
        pci_info->deviceId = ent->device;
        pci_info->irq = pdev->irq;

        /* set dma identifier */
        if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
                if ((pci_set_dma_mask(pdev, DMA_BIT_MASK(32)))) {

                        ADF_ERROR("No usable DMA configuration,"
                                      "aborting\n");
                        adf_cleanup_accel(accel_dev);
                        return -EIO;
                }
                else {
                        pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
                }
        }
        else {
                pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
        }

        status = adf_alloc_coherent_buf(accel_dev);
        if (SUCCESS != status) {
                ADF_ERROR("Can't allocate DMA buffers");
                adf_cleanup_accel(accel_dev);
                return status;
        }

        if ((status = pci_request_regions(pdev, adf_driver_name))) {
                adf_cleanup_accel(accel_dev);
                return status;
        }

        /* Get accelerator functions */
        accfunc = 0;
        if (NOT_NULL_FUNCTION(hw_data->getAccFuncOffset)) {
                accfunc_offset = hw_data->getAccFuncOffset();
                pci_read_config_dword(pdev, accfunc_offset, &accfunc);
        }
        accel_dev->accelCapabilitiesMask = hw_data->getAccFuncMask(accfunc);

        for (i = 0; i < GET_MAX_BARS(accel_dev); i++) {
                bar_index = i * ADF_BAR_OFFSET;

                /* check if bar available and reset pciBars entry if not    */
                if (CPA_FALSE == hw_data->isBarAvailable(hw_data, i)) {
                        pci_info->pciBars[i].baseAddr = 0;
                        pci_info->pciBars[i].virtAddr = 0;
                        pci_info->pciBars[i].size = 0;
                        pci_info->numBars++;
                        continue;
                }

                /* Find all the device's BARS */
                pci_info->pciBars[i].baseAddr = pci_resource_start(pdev,
                                                                   bar_index);
                if (!pci_info->pciBars[i].baseAddr) {
                        break;
                }
                pci_info->pciBars[i].size = pci_resource_len(pdev, bar_index);
                pci_info->numBars++;

                ADF_DEBUG("Found BAR %d size 0x%X\n", i, pci_info->pciBars[i].size);

                /* Map the IO Memory */
                pci_info->pciBars[i].virtAddr =
                             (UARCH_INT) pci_iomap(pci_info->pDev, bar_index, 0);
                if (!pci_info->pciBars[i].virtAddr) {
                        ADF_ERROR("Failed to map BAR %d region\n",i);
                        status = FAIL;
                }
                ADF_DEBUG("PF v_addr 0x%llX ph_addr 0x%llX \n",
                            (Cpa64U)pci_info->pciBars[i].virtAddr,
                            (Cpa64U)pci_info->pciBars[i].baseAddr);
        }

        if (SUCCESS != status) {
                adf_cleanup_accel(accel_dev);
                return status;
        }

        pci_set_master(pdev);
        /*
         * Enable advanced error reporting
         */
        status = adf_enable_aer(accel_dev, (void *)&adf_driver);
        if (status) {
                ADF_ERROR("Failed to enable advanced error reporting\n");
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }

        /*
         * Enable interrupts to the IA by setting the appropriate bits
         * in the SMIA register
         */
        pmisc_bar_id = hw_data->getMiscBarId(hw_data);
        pmisc_bar_addr = (void*)pci_info->pciBars[pmisc_bar_id].virtAddr;
        hw_data->getSmiapfOffsetMask(hw_data, &smia_offset, &smia_mask);
        ICP_ADF_CSR_WR(pmisc_bar_addr, smia_offset, smia_mask);

        /* Check if SRAM ar exists */
        if (NOT_NULL_FUNCTION(hw_data->getSramBarId)) {
                adf_esram_data_t esram_info = {0};
                /* Set eSRAM configuration
                 * Note: used predefined actual base physical address in CPP
                 * space from the EAS instead of the one returned by OS */
                sramBarId = hw_data->getSramBarId(hw_data);

                /* get ESRAM information from device config */
                hw_data->getEsramInfo(hw_data, &esram_info);
                status = adf_esramSetConfigInfo(accel_dev,
                                    esram_info.sramAeAddr,
                                    pci_info->pciBars[sramBarId].virtAddr,
                                    esram_info.sramAeSize);
        }
        else {
                status = adf_esramSetConfigInfo(accel_dev, 0, 0, 0);
        }

        if (status) {
                ADF_ERROR("Failed to set eSRAM info\n");
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }

        /* Allocate buffers for virtual functions */
        if (hw_data->maxNumVf != 0) {
                Cpa32U pci_vfs_size = 0;
                void *pci_vfs = NULL;

                pci_vfs_size = (sizeof(void*) * hw_data->maxNumVf);
                pci_vfs = ICP_MALLOC_GEN_NUMA(pci_vfs_size, accel_dev->pkg_id);
                if (NULL == pci_vfs) {
                        ADF_ERROR("Failed to allocate VF buffer\n");
                        adf_cleanup_accel(accel_dev);
                        return -EIO;
                }
                ICP_MEMSET(pci_vfs, 0, pci_vfs_size);
                accel_dev->pci_vfs = pci_vfs;
        }

        /* Add accel device to table */
        status = adf_devmgrAddAccelDev(accel_dev);
        if (status) {
                ADF_ERROR("Failed to add new accelerator device.\n");
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }

        status = adf_csr_chrdev_create(accel_dev->accelId);
        if (SUCCESS != status) {
                ADF_ERROR("Failed to add csr device %d\n",
                                      accel_dev->accelId);
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }

        status = adf_ring_chrdev_create(accel_dev->accelId);
        if (SUCCESS != status) {
                ADF_ERROR("Failed to add ring device %d\n",
                                       accel_dev->accelId);
                adf_cleanup_accel(accel_dev);
                return -EIO;
        }
        /* Set usage counter to zero */
        osalAtomicSet(0, &accel_dev->usageCounter);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,0,0)
        pci_save_state(pdev);
        accel_dev->pci_state = pci_store_saved_state(pdev);
#else
        accel_dev->pci_state = adf_store_pci_state(pdev, accel_dev->pkg_id);
#endif
        if (!accel_dev->pci_state) {
                ADF_ERROR("Failed to store pci state for device %d\n",
                                                  accel_dev->accelId);
                adf_cleanup_accel(accel_dev);
                return -ENOMEM;
        }
        pci_info->state = ICP_ACCEL_STATE_INITIALIZED;
        return status;
}

/*
 * adf_restore_dev
 * Brings device to state just after probe - ready to be started.
 */
int adf_restore_dev(icp_accel_dev_t *accel_dev)
{
        int gige_ret = 0, ret = 0, pending = 0, ctr = 0;
        struct pci_dev *pdev = accel_dev->pciAccelDev.pDev;
        struct pci_dev *parent = pdev->bus->self;
        struct pci_dev *gige = NULL;
        uint32_t pmisc_bar_id = 0;
        uint32_t *pmisc_bar_addr = NULL;
        uint16_t ppdstat = 0, bridge_ctl = 0, i = 0;
        Cpa32U smia_offset = 0, smia_mask = 0;
        icp_accel_pci_info_t *pci_info = &accel_dev->pciAccelDev;
        adf_hw_device_data_t *hw_data = accel_dev->pHwDeviceData;

        pmisc_bar_id = hw_data->getMiscBarId(hw_data);
        pmisc_bar_addr = (void*)pci_info->pciBars[pmisc_bar_id].virtAddr;
        hw_data->getSmiapfOffsetMask(hw_data, &smia_offset, &smia_mask);

        ADF_PRINT("Reseting device icp_dev%d\n", accel_dev->accelId);
        /* Reset the device */
        if (!parent) {
                ADF_ERROR("Can not issue secondary bus reset\n");
                ADF_ERROR("Trying FLR\n");
                ret = __pci_reset_function(pdev);
                if (ret) {
                        ADF_ERROR("Could not reset device\n");
                        return ret;
                }
                /* Reset everything but endpoint and GigEs */
                ICP_ADF_CSR_WR(pmisc_bar_addr, RESET_OFFSET, RESET_VECTOR);
                msleep(MSLEEP_TIME);
                ICP_ADF_CSR_WR(pmisc_bar_addr, RESET_OFFSET, 0);
                msleep(MSLEEP_TIME);
        }
        else {
                if (DEV_DH89XXCC == hw_data->dev_class->type) {
                        /* Notify gige watchdog if it is running
                         * and device is DH89xxCC. */
                        gige_ret = adf_gige_notify_restarting_dev(accel_dev);
                }

                /* Reset endpoint using secondary bus reset.
                 * Wait for all transactions first. */
                do {
                        pci_read_config_word(pdev, PPDSTAT_OFFSET, &ppdstat);
                        pending = ppdstat & PCI_EXP_DEVSTA_TRPND;
                        if (pending)
                                msleep(MSLEEP_TRIP_TIME);
                } while(pending && ctr++ < WAIT_FOR_TRIP_LOOPS);
                if (pending) {
                        ADF_ERROR("Transaction still in progress. " \
                                  "Proceeding with reset\n");
                }
                if (DEV_DH89XXCC == hw_data->dev_class->type) {
                        /* Reset GigEs if device type is DH89xxCC. */
                        for (i = 1; i < GIGES_NUM + 1 &&
                                gige_ret != GIGE_WATCHDOG_NOT_RUNNING; i++) {
                                gige = pci_get_slot(pdev->bus,
                                               PCI_DEVFN(PCI_SLOT(pdev->devfn),
                                               PCI_FUNC(pdev->devfn) + i));
                                if (gige) {
                                        pci_save_state(gige);
                                }
                        }
                }

                /* Reset endpoint using secondary bus reset. */
                pci_read_config_word(parent, PCI_BRIDGE_CONTROL, &bridge_ctl);
                bridge_ctl |= PCI_BRIDGE_CTL_BUS_RESET;
                pci_write_config_word(parent, PCI_BRIDGE_CONTROL, bridge_ctl);
                msleep(MSLEEP_TIME);
                bridge_ctl &= ~PCI_BRIDGE_CTL_BUS_RESET;
                pci_write_config_word(parent, PCI_BRIDGE_CONTROL, bridge_ctl);
                msleep(MSLEEP_TIME);

                if (DEV_DH89XXCC == hw_data->dev_class->type) {
                        if (gige_ret != GIGE_WATCHDOG_NOT_RUNNING) {
                                ICP_ADF_CSR_WR(pmisc_bar_addr, RESET_OFFSET,
                                                   RESET_GIGE_RESET_VECTOR);
                                msleep(MSLEEP_TIME);
                                ICP_ADF_CSR_WR(pmisc_bar_addr, RESET_OFFSET,
                                                       RESET_GIGE_VECTOR);
                                msleep(MSLEEP_TIME);
                        }
                        for (i = 1; i < GIGES_NUM + 1 &&
                                gige_ret != GIGE_WATCHDOG_NOT_RUNNING; i++) {
                                gige = pci_get_slot(pdev->bus,
                                               PCI_DEVFN(PCI_SLOT(pdev->devfn),
                                               PCI_FUNC(pdev->devfn) + i));
                                if (gige) {
                                        pci_restore_state(gige);
                                }
                        }
                }
        }

        /* Restore dev state */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,0,0)
        pci_load_saved_state(pdev, accel_dev->pci_state);
        pci_restore_state(pdev);
#else
        adf_restore_pci_state(pdev, accel_dev->pci_state);
#endif
        /* Enable interrupts to the IA */
        ICP_ADF_CSR_WR(pmisc_bar_addr, smia_offset, smia_mask);
        return 0;
}

/*
 * adf_remove
 */
static void adf_remove(struct pci_dev *pdev)
{
        int status = 0, i = 0;
        icp_accel_dev_t *accel_dev = NULL;
        icp_accel_pci_info_t *pci_info = NULL;
        icp_accel_pci_vf_info_t *vf = NULL;
        adf_hw_device_data_t  *hw_data = NULL;

        if (NULL == pdev) {
                ADF_ERROR("%s(): pdev is NULL\n", __FUNCTION__);
                return;
        }

        if(ICP_DH89xxCC_PCI_IOV_DEVICE_ID == pdev->device
                || ICP_SXXXX_PCI_IOV_DEVICE_ID == pdev->device)
        {
                /* The VFs will be cleaned when PF will be removed.
                 * just need to disable MSIX IRQ for the bundle.
                 * It will be handled as MSI by VF */
                struct pci_dev *pdev_pf = pdev->physfn;
                int dev_nr = 0;
                uint32_t *pmisc_bar_addr = NULL;
                Cpa32U pmisc_bar_id = 0;

                accel_dev = adf_devmgrGetAccelDev(pdev_pf);
                if(!accel_dev)
                {
                        ADF_ERROR("Can not find PF device for given VF\n");
                        return;
                }
                if(!BIT_IS_SET(accel_dev->adfSubsystemStatus,
                                                ADF_STATUS_SYSTEM_STARTED))
                {
                        /* Dont disable msix as we are going down */
                        return;
                }

                dev_nr = ((PCI_SLOT(pdev->devfn) - 1) * PCI_FUNCTION_OFFSET)
                                                       + PCI_FUNC(pdev->devfn);
                /* Disable MSIX for the bundle.
                 * It will be handled by the VF now */
                adf_disable_bundle_msix(accel_dev, dev_nr);
                vf = accel_dev->pci_vfs[dev_nr];
                vf->attached = 0;
                pci_info = &accel_dev->pciAccelDev;
                hw_data = accel_dev->pHwDeviceData;
                pmisc_bar_id = hw_data->getMiscBarId(hw_data);
                pmisc_bar_addr = (void*)
                       pci_info->pciBars[pmisc_bar_id].virtAddr;

                /* Increase module ref counter */
                adf_get_module();
                osalAtomicInc(&accel_dev->usageCounter);
                return;
        }

        /* Lookup the accel_dev assoc with this pdev */
        accel_dev = adf_devmgrGetAccelDev(pdev);
        if (NULL == accel_dev) {
                return;
        }

        if (pdev == accel_dev->pciAccelDev.pDev) {
                accel_dev->pciAccelDev.state = ICP_ACCEL_STATE_UNINITIALIZED;
        }

        /* Clean all VF now */
        hw_data = accel_dev->pHwDeviceData;
        for(i = 0; i < hw_data->maxNumVf; i++)
        {
                vf = accel_dev->pci_vfs[i];
                if (!vf) {
                        continue;
                }
                adf_cleanup_vf(vf);
                accel_dev->pci_vfs[i] = NULL;
        }
        pci_info = &accel_dev->pciAccelDev;
        pci_info->irq = 0;

        /* unmap the pcidev bars */
        for (i = 0; i < GET_MAX_BARS(accel_dev); i++) {
                if (pci_info->pciBars[i].virtAddr) {
                        pci_iounmap(pci_info->pDev, (void __iomem *)
                                   (UARCH_INT) pci_info->pciBars[i].virtAddr);
                        pci_info->pciBars[i].virtAddr = (UARCH_INT) NULL;
                }
        }

        /* Disable advanced error reporting */
        adf_disable_aer(accel_dev);

        /* Cleanup the transport Data */
        adf_cleanup_ETR_Data(accel_dev);

        /* Free acces device */
        status = adf_cleanup_accel(accel_dev);
        if (status) {
                ADF_ERROR("adf_cleanup_accel failure\n");
        }
        num_device--;
}

/*
 * adfdrv_init
 * ICP ADF Module Init function
 */
static int __devinit adfdrv_init()
{
        int status = 0;

        /* Allocate Resource Config table */
        status = adf_cfgInit();

        status = adf_csr_chrdev_register();
        if (status) {
                ADF_ERROR("failed to register adf csr device driver\n");
                return status;
        }

        status = adf_ring_chrdev_register();
        if (status) {
                ADF_ERROR("failed to register adf ring device driver\n");
                adf_csr_chrdev_unregister();
                return status;
        }

        /*
         * Register the adf ctl character device driver
         * for dynamic configuration of the system resource
         * variables via the adf_ctl user space program
         */
        status = register_adf_ctl_device_driver();
        if (status) {
                ADF_ERROR("failed to register adf ctrl device driver\n");
                adf_csr_chrdev_unregister();
                adf_ring_chrdev_unregister();
                return status;
        }

        /*
         * Register the adf processes character device driver
         */
        status = register_adf_processes_device_driver();
        if (status) {
                ADF_ERROR("failed to register adf processes device driver\n");
                unregister_adf_ctl_device_driver();
                adf_csr_chrdev_unregister();
                adf_ring_chrdev_unregister();
                return status;
        }

        status = register_adf_gige_wd_device_driver();
        if (status) {
                ADF_ERROR("failed to register adf gige wd device driver\n");
                unregister_adf_ctl_device_driver();
                unregister_adf_processes_device_driver();
                adf_csr_chrdev_unregister();
                adf_ring_chrdev_unregister();
                return status;
        }

        status = pci_register_driver(&adf_driver);
        if (status) {
                ADF_ERROR("failed call to pci_register_driver\n");
                unregister_adf_ctl_device_driver();
                unregister_adf_processes_device_driver();
                unregister_adf_gige_wd_device_driver();
                adf_csr_chrdev_unregister();
                adf_ring_chrdev_unregister();
                return status;
        }
        return SUCCESS;
}

/*
 * adfdrv_release
 * ADF Module release function
 */
static void adfdrv_release()
{
        adf_ring_chrdev_unregister();
        adf_csr_chrdev_unregister();
        unregister_adf_ctl_device_driver();
        unregister_adf_processes_device_driver();
        unregister_adf_gige_wd_device_driver();
        pci_unregister_driver(&adf_driver);
        adf_cfgUninit();
}

int __init driver_module_init()
{
        return adfdrv_init();
}

void driver_module_exit()
{
        adfdrv_release();
}
