/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.

********************************************************************************
Marvell Commercial License Option

If you received this File from Marvell and you have entered into a commercial
license agreement (a "Commercial License") with Marvell, the File is licensed
to you under the terms of the applicable Commercial License.

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
********************************************************************************
Marvell BSD License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.

    *   Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    *   Neither the name of Marvell nor the names of its contributors may be
        used to endorse or promote products derived from this software without
        specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
* pssBspApis.c - bsp APIs
*
* DESCRIPTION:
*       API's supported by BSP.
*
* DEPENDENCIES:
*       None.
*
* FILE REVISION NUMBER:
*       $Revision$
*
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include "pssBspApis.h"

static void mv_fix_lion_b2b(void);
static void mv_sysctl_register(void);
#ifdef CONFIG_WG_PLATFORM // WG:JB Fixups
static struct pci_dev *mv_dev  = NULL; // Save device we found
static int rootfs_is_initramfs = 1;    // Spoof the initramfs
#else
extern int rootfs_is_initramfs;
#endif

static struct ctl_table_header *mv_sysctl_header;

void mv_late_init(void)
{
  mv_fix_lion_b2b();
  mv_sysctl_register();
}

#define STUB_FAIL printk("stub function %s returning MV_NOT_SUPPORTED\n", \
                         __FUNCTION__);  return MV_NOT_SUPPORTED

#define STUB_FAIL_NULL printk("stub function %s returning MV_NOT_SUPPORTED\n", \
                         __FUNCTION__);  return NULL

#define STUB_OK   printk("stub function %s returning MV_OK\n", \
                         __FUNCTION__);  return MV_OK

#define STUB_TBD printk("stub function TBD %s returning MV_FAIL\n", __FUNCTION__); \
  return MV_FAIL

#define HSU_OFFSET (128<<20) /* 128 MB after start of high_memory is hsu area */

static inline struct pci_dev *find_bdf(u32 bus, u32 device, u32 func)
{
  return pci_get_bus_and_slot(bus, PCI_DEVFN(device,func));
}

static inline u16 mv_swab16(u16 w)
{
  return (w << 8) | ((w >> 8) & 0xff);
}

static inline u32 mv_swab32(u32 w)
{
  return ((w & 0xff000000) >> 24) |
         ((w & 0x00ff0000) >> 8) |
         ((w & 0x0000ff00) << 8)  |
         ((w & 0x000000ff) << 24);
}

/* interrupt routine pointer */
static MV_VOIDFUNCPTR   bspIsrRoutine = NULL;
static MV_U32           bspIsrParameter = 0;

/*** reset ***/
/*******************************************************************************
* bspResetInit
*
* DESCRIPTION:
*       This routine calls in init to do system init config for reset.
*
* INPUTS:
*       none.
*
* OUTPUTS:
*       none.
*
* RETURNS:
*       MV_OK      - on success.
*       MV_FAIL    - otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspResetInit
(
    MV_VOID
)
{
  return MV_OK;
}


/*******************************************************************************
* bspReset
*
* DESCRIPTION:
*       This routine calls to reset of CPU.
*
* INPUTS:
*       none.
*
* OUTPUTS:
*       none.
*
* RETURNS:
*       MV_OK      - on success.
*       MV_FAIL    - otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/

MV_STATUS bspReset
(
    MV_VOID
)
{
  kernel_restart(NULL);
  return  MV_OK;
}

/*** cache ***/
/*******************************************************************************
* bspCacheFlush
*
* DESCRIPTION:
*       Flush to RAM content of cache
*
* INPUTS:
*       type        - type of cache memory data/intraction
*       address_PTR - starting address of memory block to flush
*       size        - size of memory block
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspCacheFlush
(
    IN bspCacheType_ENT         cacheType, 
    IN void                     *address_PTR, 
    IN size_t                   size
)
{
  switch (cacheType)
  {
  case bspCacheType_InstructionCache_E:
    return MV_BAD_PARAM; /* only data cache supported */

  case bspCacheType_DataCache_E:
    break;

  default:
    return MV_BAD_PARAM;
  }

  //
  // our area doesn't need cache flush/invalidate
  //
  return  MV_OK;
}

/*******************************************************************************
* bspCacheInvalidate
*
* DESCRIPTION:
*       Invalidate current content of cache
*
* INPUTS:
*       type        - type of cache memory data/intraction
*       address_PTR - starting address of memory block to flush
*       size        - size of memory block
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspCacheInvalidate 
(
    IN bspCacheType_ENT         cacheType, 
    IN void                     *address_PTR, 
    IN size_t                   size
)
{
  switch (cacheType)
  {
  case bspCacheType_InstructionCache_E:
    return MV_BAD_PARAM; /* only data cache supported */
    
  case bspCacheType_DataCache_E:
    break;
    
  default:
    return MV_BAD_PARAM;
  }

  //
  // our area doesn't need cache flush/invalidate
  //
  return MV_OK;
}

/*** DMA ***/
/*******************************************************************************
* bspDmaWrite
*
* DESCRIPTION:
*       Write a given buffer to the given address using the Dma.
*
* INPUTS:
*       address     - The destination address to write to.
*       buffer      - The buffer to be written.
*       length      - Length of buffer in words.
*       burstLimit  - Number of words to be written on each burst.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*       1.  The given buffer is allways 4 bytes aligned, any further allignment
*           requirements should be handled internally by this function.
*       2.  The given buffer may be allocated from an uncached memory space, and
*           it's to the function to handle the cache flushing.
*       3.  The Prestera Driver assumes that the implementation of the DMA is
*           blocking, otherwise the Driver functionality might be damaged.
*
*******************************************************************************/
MV_STATUS bspDmaWrite
(
    IN  MV_U32  address,
    IN  MV_U32  *buffer,
    IN  MV_U32  length,
    IN  MV_U32  burstLimit
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspDmaRead
*
* DESCRIPTION:
*       Read a memory block from a given address.
*
* INPUTS:
*       address     - The address to read from.
*       length      - Length of the memory block to read (in words).
*       burstLimit  - Number of words to be read on each burst.
*
* OUTPUTS:
*       buffer  - The read data.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*       1.  The given buffer is allways 4 bytes aligned, any further allignment
*           requirements should be handled internally by this function.
*       2.  The given buffer may be allocated from an uncached memory space, and
*           it's to the function to handle the cache flushing.
*       3.  The Prestera Driver assumes that the implementation of the DMA is
*           blocking, otherwise the Driver functionality might be damaged.
*
*******************************************************************************/
MV_STATUS bspDmaRead
(
    IN  MV_U32  address,
    IN  MV_U32  length,
    IN  MV_U32  burstLimit,
    OUT MV_U32  *buffer
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspCacheDmaMalloc
*
* DESCRIPTION:
*       Allocate a cache free area for DMA devices.
*
* INPUTS:
*       size_t bytes - number of bytes to allocate
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       pointer to allocated data per success
*       NULL - per failure to allocate space
*
* COMMENTS:
*       None
*
*******************************************************************************/
static unsigned long dma_base = 0;
static void *dma_area_base = NULL;

void *bspCacheDmaMalloc(IN size_t bytes)
{
  unsigned long        dma_len = bytes;
  void *dma_area;

  if (!dma_base)
    dma_base = __pa(high_memory);

  request_mem_region(dma_base, dma_len, "prestera-dma");
  dma_area = (unsigned long *)ioremap_nocache(dma_base, dma_len);  

  if (!dma_area_base)
    dma_area_base = dma_area;

  return dma_area;
}

/*******************************************************************************
* bspCacheDmaFree
*
* DESCRIPTION:
*       free a cache free area back to pool.
*
* INPUTS:
*       size_t bytes - number of bytes to allocate
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*       None
*
*******************************************************************************/
MV_STATUS bspCacheDmaFree(void * pBuf)
{
  STUB_FAIL;
}

/*** PCI ***/
/*******************************************************************************
* bspPciConfigWriteReg
*
* DESCRIPTION:
*       This routine write register to the PCI configuration space.
*
* INPUTS:
*       busNo    - PCI bus number.
*       devSel   - the device devSel.
*       funcNo   - function number.
*       regAddr  - Register offset in the configuration space.
*       data     - data to write.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspPciConfigWriteReg
(
    IN  MV_U32  busNo,
    IN  MV_U32  devSel,
    IN  MV_U32  funcNo,
    IN  MV_U32  regAddr,
    IN  MV_U32  data
)
{
 struct pci_dev *dev;
 
 if ((dev = find_bdf(busNo, devSel, funcNo)))
 {
   pci_write_config_dword(dev, regAddr, data);
   pci_dev_put(dev);
   return MV_OK;
 }
 else
   return MV_FAIL;
}

/*******************************************************************************
* bspPciConfigReadReg
*
* DESCRIPTION:
*       This routine read register from the PCI configuration space.
*
* INPUTS:
*       busNo    - PCI bus number.
*       devSel   - the device devSel.
*       funcNo   - function number.
*       regAddr  - Register offset in the configuration space.
*
* OUTPUTS:
*       data     - the read data.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspPciConfigReadReg
(
    IN  MV_U32  busNo,
    IN  MV_U32  devSel,
    IN  MV_U32  funcNo,
    IN  MV_U32  regAddr,
    OUT MV_U32  *data
)
{
  struct pci_dev *dev;
  
  if ((dev = find_bdf(busNo, devSel, funcNo)))
	{
		pci_read_config_dword(dev, regAddr, data);
    pci_dev_put(dev);
		return MV_OK;
	}
	else
		return MV_FAIL;
}

/*******************************************************************************
* bspPciGetResourceStart
*
* DESCRIPTION:
*       This routine performs pci_resource_start.
*       In INTEL64 this function must be used instead of reading the bar
*       directly.
*
* INPUTS:
*       busNo    - PCI bus number.
*       devSel   - the device devSel.
*       funcNo   - function number.
*       barNo    - Bar Number.
*
* OUTPUTS:
*       ResourceStart - the address of the resource.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspPciGetResourceStart
(
    IN  MV_U32  busNo,
    IN  MV_U32  devSel,
    IN  MV_U32  funcNo,
    IN  MV_U32  barNo,
    OUT MV_U64  *resourceStart
)
{
  struct pci_dev *dev;

  if ((dev = find_bdf(busNo, devSel, funcNo)))
  {
    *resourceStart = pci_resource_start(dev, barNo);
    pci_dev_put(dev);
    return MV_OK;
  }  
  return MV_FAIL;
}

/*******************************************************************************
* bspPciGetResourceLen
*
* DESCRIPTION:
*       This routine performs pci_resource_len.
*       In INTEL64 this function must be used instead of reading the bar
*       directly.
*
* INPUTS:
*       busNo    - PCI bus number.
*       devSel   - the device devSel.
*       funcNo   - function number.
*       barNo    - Bar Number.
*
* OUTPUTS:
*       ResourceLen - the address of the resource.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspPciGetResourceLen
(
    IN  MV_U32  busNo,
    IN  MV_U32  devSel,
    IN  MV_U32  funcNo,
    IN  MV_U32  barNo,
    OUT MV_U64  *resourceLen
)
{
  struct pci_dev *dev;

  if ((dev = find_bdf(busNo, devSel, funcNo)))
  {
    *resourceLen = pci_resource_len(dev, barNo);
    pci_dev_put(dev);
    return MV_OK;
  }
  return MV_FAIL;
}

/*******************************************************************************
* bspPciFindDev
*
* DESCRIPTION:
*       This routine returns the next instance of the given device (defined by
*       vendorId & devId).
*
* INPUTS:
*       vendorId - The device vendor Id.
*       devId    - The device Id.
*       instance - The requested device instance.
*
* OUTPUTS:
*       busNo    - PCI bus number.
*       devSel   - the device devSel.
*       funcNo   - function number.
*
* RETURNS:
*       MV_OK   - on success,
*       MV_FAIL - othersise.
*
* COMMENTS:
*
*******************************************************************************/

MV_STATUS bspPciFindDev
(
    IN  MV_U16  vendorId,
    IN  MV_U16  devId,
    IN  MV_U32  instance,
    OUT MV_U32  *busNo,
    OUT MV_U32  *devSel,
    OUT MV_U32  *funcNo
)
{
	struct pci_dev *dev = NULL;
	int count = 0;

	*busNo = *devSel = *funcNo = 0;

  for_each_pci_dev(dev)
	{
    if ((vendorId == 0xffff || dev->vendor == vendorId) && 
        (devId == 0xffff || dev->device == devId) && 
        (count++ == instance))
		{
			*busNo = dev->bus->number;
			*devSel = PCI_SLOT(dev->devfn);
			*funcNo = PCI_FUNC(dev->devfn);
			return MV_OK;
		}
	}
	
	return MV_FAIL;
}


/*******************************************************************************
* bspPciGetIntVec
*
* DESCRIPTION:
*       This routine return the PCI interrupt vector.
*
* INPUTS:
*       pciInt - PCI interrupt number.
*
* OUTPUTS:
*       intVec - PCI interrupt vector.
*
* RETURNS:
*       MV_OK      - on success.
*       MV_FAIL    - otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspPciGetIntVec
(
    IN  bspPciInt_PCI_INT  pciInt,
    OUT void               **intVec
)
{
  int pp_core_number;
  int intrLine;
  static int lion_quad_config = -1;
  static int b2b_duo_config = -1;
  struct pci_dev *dev;
  MV_STATUS rc = MV_FAIL;
  int b, d, f;

  intrLine = pciInt;

  /*
    First determine if we are dealing with lion or b2b.
    
    if the first GETINTVEC request is for intrLine=4 then it's
    a lion quad device.
    
    if the first GETINTVEC request is for intrLine=2 then it's
    a b2b duo device.
    
    else it's for a single device.
    
  */
  
  if (lion_quad_config == -1 )
  {
    if (intrLine == 4)
      lion_quad_config = 1;
    else
      lion_quad_config = 0;
  }
  
  else if (b2b_duo_config == -1 )
  {
    if (intrLine == 2)
      b2b_duo_config = 1;
    else
      b2b_duo_config = 0;
  }

  if (lion_quad_config)
  {
    /*
      pss - bsp handshake for lion quad config:
      pciInt = intrLine = 4 = core number 0 = fisrt pp core scanned
      pciInt = intrLine = 3 = core number 1    ...
      pciInt = intrLine = 2 = core number 2    ...
      pciInt = intrLine = 1 = core number 3 = last pp core scanned
    */
    
    pp_core_number = 4 - intrLine;
  }
  
  else if (b2b_duo_config)
  {
    /* same logic as for lion above */
    pp_core_number = 2 - intrLine;
  }

  else
  {
    pp_core_number = 0;
  }

  // iterate to the instance of pp_core_number
  if (bspPciFindDev(MARVELL_VENDOR_ID,
                    0xffff, pp_core_number, &b, &d, &f) == MV_OK)
  {
    if ((dev = find_bdf(b, d, f)))
    {
      *intVec = (void *)(unsigned long)(dev->irq);
      pci_dev_put(dev);
      rc = MV_OK;
    }
  }
        
  return rc;
}

/*******************************************************************************
* bspPciGetIntMask
*
* DESCRIPTION:
*       This routine return the PCI interrupt vector.
*
* INPUTS:
*       pciInt - PCI interrupt number.
*
* OUTPUTS:
*       intMask - PCI interrupt mask.
*
* RETURNS:
*       MV_OK      - on success.
*       MV_FAIL    - otherwise.
*
* COMMENTS:
*       PCI interrupt mask should be used for interrupt disable/enable.
*
*******************************************************************************/
MV_STATUS bspPciGetIntMask
(
    IN  bspPciInt_PCI_INT  pciInt,
    OUT MV_U32             *intMask
)
{
  return MV_OK;
}

/*******************************************************************************
* bspPciEnableCombinedAccess
*
* DESCRIPTION:
*       This function enables / disables the Pci writes / reads combining
*       feature.
*       Some system controllers support combining memory writes / reads. When a
*       long burst write / read is required and combining is enabled, the master
*       combines consecutive write / read transactions, if possible, and
*       performs one burst on the Pci instead of two. (see comments)
*
* INPUTS:
*       enWrCombine - MV_TRUE enables write requests combining.
*       enRdCombine - MV_TRUE enables read requests combining.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK               - on sucess,
*       MV_NOT_SUPPORTED    - if the controller does not support this feature,
*       MV_FAIL             - otherwise.
*
* COMMENTS:
*       1.  Example for combined write scenario:
*           The controller is required to write a 32-bit data to address 0x8000,
*           while this transaction is still in progress, a request for a write
*           operation to address 0x8004 arrives, in this case the two writes are
*           combined into a single burst of 8-bytes.
*
*******************************************************************************/
MV_STATUS bspPciEnableCombinedAccess
(
    IN  MV_BOOL     enWrCombine,
    IN  MV_BOOL     enRdCombine
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspEthInit
*
* DESCRIPTION: Init the ethernet HW and HAL
*
* INPUTS:
*       port   - eth port number
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthInit
(
    MV_U8 port
)
{
  STUB_OK;
}

/*******************************************************************************
* bspSmiInitDriver
*
* DESCRIPTION:
*       Init the TWSI interface 
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       smiAccessMode - direct/indirect mode
*
* RETURNS:
*       MV_OK               - on success
*       MV_FAIL   - on hardware error
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspSmiInitDriver
(
   bspSmiAccessMode_ENT  *smiAccessMode
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspSmiReadReg
*
* DESCRIPTION:
*       Reads a register from SMI slave.
*
* INPUTS:
*       devSlvId - Slave Device ID
*      actSmiAddr - actual smi addr to use (relevant for SX PPs)
*       regAddr - Register address to read from.
*
* OUTPUTS:
*       valuePtr     - Data read from register.
*
* RETURNS:
*       MV_OK               - on success
*       MV_ERROR   - on hardware error
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspSmiReadReg
(
    IN  MV_U32  devSlvId,
    IN  MV_U32  actSmiAddr,
    IN  MV_U32  regAddr,
    OUT MV_U32 *value
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspSmiWriteReg
*
* DESCRIPTION:
*       Writes a register to an SMI slave.
*
* INPUTS:
*       devSlvId - Slave Device ID
*       actSmiAddr - actual smi addr to use (relevant for SX PPs)
*       regAddr - Register address to read from.
*       value   - data to be written.
*
* OUTPUTS:
*        None,
*
* RETURNS:
*       MV_OK               - on success
*       MV_ERROR   - on hardware error
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspSmiWriteReg
(
    IN MV_U32 devSlvId,
    IN MV_U32 actSmiAddr,
    IN MV_U32 regAddr,
    IN MV_U32 value
)
{
  STUB_FAIL;
}


/*** TWSI ***/
/*******************************************************************************
* bspTwsiInitDriver
*
* DESCRIPTION:
*       Init the TWSI interface 
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK               - on success
*       MV_ERROR   - on hardware error
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspTwsiInitDriver
(
    MV_VOID
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspTwsiWaitNotBusy
*
* DESCRIPTION:
*       Wait for TWSI interface not BUSY
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK               - on success
*       MV_ERROR   - on hardware error
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspTwsiWaitNotBusy
(
    MV_VOID
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspTwsiMasterReadTrans
*
* DESCRIPTION:
*       do TWSI interface Transaction 
*
* INPUTS:
*    devId - I2c slave ID                               
*    pData - Pointer to array of chars (address / data)
*    len   - pData array size (in chars).              
*    stop  - Indicates if stop bit is needed.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK               - on success
*       MV_ERROR   - on hardware error
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspTwsiMasterReadTrans
(
    IN MV_U8           devId,       /* I2c slave ID                              */ 
    IN MV_U8           *pData,      /* Pointer to array of chars (address / data)*/
    IN MV_U8           len,         /* pData array size (in chars).              */
    IN MV_BOOL         stop         /* Indicates if stop bit is needed in the end  */
)
{
  STUB_FAIL;
}

/*******************************************************************************
* bspTwsiMasterWriteTrans
*
* DESCRIPTION:
*       do TWSI interface Transaction 
*
* INPUTS:
*    devId - I2c slave ID                               
*    pData - Pointer to array of chars (address / data)
*    len   - pData array size (in chars).              
*    stop  - Indicates if stop bit is needed.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK               - on success
*       MV_ERROR   - on hardware error
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS bspTwsiMasterWriteTrans
(
    IN MV_U8           devId,       /* I2c slave ID                              */ 
    IN MV_U8           *pData,      /* Pointer to array of chars (address / data)*/
    IN MV_U8           len,         /* pData array size (in chars).              */
    IN MV_BOOL         stop         /* Indicates if stop bit is needed in the end  */
    )
{
  STUB_FAIL;
}

/*******************************************************************************
* bspIsr
*
* DESCRIPTION:
*       This is the ISR reponsible for PP.
*
* INPUTS:
*       irq     - the Interrupt ReQuest number
*       dev_id  - the client data used as argument to the handler
*       regs    - holds a snapshot of the CPU context before interrupt
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       IRQ_HANDLED allways
*
* COMMENTS:
*       None.
*
*******************************************************************************/
static irqreturn_t bspIsr
(
    int             irq,
    void            *dev_id,
    struct pt_regs  *regs
)
{
	if (bspIsrRoutine)
		bspIsrRoutine(dev_id);	

    return IRQ_HANDLED;
}

/*******************************************************************************
* bspIntConnect
*
* DESCRIPTION:
*       Connect a specified C routine to a specified interrupt vector.
*
* INPUTS:
*       vector    - interrupt vector number to attach to
*       routine   - routine to be called
*       parameter - parameter to be passed to routine
*
* OUTPUTS:
*       None
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*       None
*
*******************************************************************************/
MV_STATUS bspIntConnect
(
    IN  MV_U32           vector,
    IN  MV_VOIDFUNCPTR   routine,
    IN  MV_U32           parameter
)
{
   int rc;

   bspIsrParameter = parameter;
   bspIsrRoutine   = routine;

   rc = request_irq(vector, 
										(irq_handler_t)bspIsr, 
										IRQF_DISABLED, "PP_interrupt", (void *)&bspIsrParameter);

   return (0 == rc) ? MV_OK : MV_FAIL;

}

/*******************************************************************************
* extDrvIntEnable
*
* DESCRIPTION:
*       Enable corresponding interrupt bits
*
* INPUTS:
*       intMask - new interrupt bits
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*       None
*
*******************************************************************************/
MV_STATUS bspIntEnable
(
    IN MV_U32   intMask
)
{
  enable_irq(intMask);
  return  MV_OK;
}

/*******************************************************************************
* bspIntDisable
*
* DESCRIPTION:
*       Disable corresponding interrupt bits.
*
* INPUTS:
*       intMask - new interrupt bits
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*       None
*
*******************************************************************************/
MV_STATUS bspIntDisable
(
    IN MV_U32   intMask
)
{
  disable_irq(intMask);
  return  MV_OK;
}

/*** Ethernet access MII with the Packet Processor ***/
/*******************************************************************************
* bspEthPortRxInit
*
* DESCRIPTION: Init the ethernet port Rx interface
*
* INPUTS:
*       rxBufPoolSize   - buffer pool size
*       rxBufPool_PTR   - the address of the pool
*       rxBufSize       - the buffer requested size
*       numOfRxBufs_PTR - number of requested buffers, and actual buffers created
*       headerOffset    - packet header offset size
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthPortRxInit
(
    IN MV_U32           rxBufPoolSize,
    IN MV_U8*           rxBufPool_PTR,
    IN MV_U32           rxBufSize,
    INOUT MV_U32        *numOfRxBufs_PTR,
    IN MV_U32           headerOffset,
    IN MV_U32           rxQNum,
    IN MV_U32           rxQbufPercentage[]
)
{
	STUB_FAIL;
}


/*******************************************************************************
* bspEthPortTxInit
*
* DESCRIPTION: Init the ethernet port Tx interface
*
* INPUTS:
*       numOfTxBufs - number of requested buffers
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthPortTxInit
(
    IN MV_U32           numOfTxBufs
)
{
	STUB_FAIL;
}

/*******************************************************************************
* bspEthPortEnable
*
* DESCRIPTION: Enable the ethernet port interface
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthPortEnable
(
    MV_VOID
)
{
	STUB_FAIL;
}

/*******************************************************************************
* bspEthPortDisable
*
* DESCRIPTION: Disable the ethernet port interface
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthPortDisable
(
    MV_VOID
)
{
	STUB_FAIL;
}

/*******************************************************************************
* bspEthInputHookAdd
*
* DESCRIPTION:
*       This bind the user Rx callback
*
* INPUTS:
*       userRxFunc - the user Rx callback function
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthInputHookAdd
(
    IN BSP_RX_CALLBACK_FUNCPTR    userRxFunc
)
{
	STUB_FAIL;
}

/*******************************************************************************
* bspEthPortTx
*
* DESCRIPTION:
*       This function is called after a TxEnd event has been received, it passes
*       the needed information to the Tapi part.
*
* INPUTS:
*       segmentsList     - A list of pointers to the packets segments.
*       segmentsLen      - A list of segment length.
*       numOfSegments   - The number of segment in segment list.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthPortTx
(
    IN MV_U8*           segmentsList[],
    IN MV_U32           segmentsLen[],   
    IN MV_U32           numOfSegments
)
{
	STUB_FAIL;
}

/*******************************************************************************
* bspEthPortTxModeSet
*
* DESCRIPTION: Set the ethernet port tx mode
*
* INPUTS:
*       if txMode == bspEthTxMode_asynch_E -- don't wait for TX done - free packet when interrupt received
*       if txMode == bspEthTxMode_synch_E  -- wait to TX done and free packet immediately
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful
*       MV_NOT_SUPPORTED if input is wrong
*       MV_FAIL if bspTxModeSetOn is zero
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthPortTxModeSet
(
 void *stub
)
{
  STUB_OK;
}

/*******************************************************************************
* bspEthPortTxQueue
*
* DESCRIPTION:
*       This function is called after a TxEnd event has been received, it passes
*       the needed information to the Tapi part.
*
* INPUTS:
*       segmentList     - A list of pointers to the packets segments.
*       segmentLen      - A list of segment length.
*       numOfSegments   - The number of segment in segment list.
*       txQueue         - The TX queue.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK if successful, or
*       GT_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthPortTxQueue
(
    IN MV_U8*           segmentList[],
    IN MV_U32           segmentLen[],   
    IN MV_U32           numOfSegments,
    IN MV_U32           txQueue
)
{
  return bspEthPortTx(segmentList, segmentLen, numOfSegments);
}

/*******************************************************************************
* bspEthTxCompleteHookAdd
*
* DESCRIPTION:
*       This bind the user Tx complete callback
*
* INPUTS:
*       userTxFunc - the user Tx callback function
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthTxCompleteHookAdd
(
    IN BSP_TX_COMPLETE_CALLBACK_FUNCPTR userTxFunc
)
{
	STUB_FAIL;
}

/*******************************************************************************
* bspEthRxPacketFree
*
* DESCRIPTION:
*       This routine frees the received Rx buffer. 
*
* INPUTS:
*       segmentsList     - A list of pointers to the packets segments.
*       numOfSegments   - The number of segment in segment list.
*       queueNum        - Receive queue number
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthRxPacketFree
(
    IN MV_U8*           segmentsList[],
    IN MV_U32           numOfSegments,
    IN MV_U32           queueNum
)
{
	STUB_FAIL;
}

/*******************************************************************************
* bspEthCpuCodeToQueue
*
* DESCRIPTION:
*       Binds DSA CPU code to RX queue.
*
* INPUTS:
*       dsaCpuCode - DSA CPU code
*       rxQueue    -  rx queue
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK if successful, or
*       MV_FAIL otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/
MV_STATUS bspEthCpuCodeToQueue
(
    IN MV_U32 dsaCpuCode,
    IN MV_U8  rxQueue
)
{
  STUB_OK;
}

/*******************************************************************************
 * bspPciFindDevReset
 *
 * DESCRIPTION:
 *       Reset gPPDevId to make chance to find internal PP again
 *
 * INPUTS:
 *       None
 *
 * OUTPUTS:
 *       None
 *
 * RETURNS:
 *       MV_OK   - on success,
 *
 * COMMENTS:
 *
 *******************************************************************************/
MV_STATUS bspPciFindDevReset(void)
{
  STUB_OK;
}

/*** hsu ***/

/*******************************************************************************
* bspWarmRestart
*
* DESCRIPTION:
*       This routine performs warm restart.
*
* INPUTS:
*       none.
*
* OUTPUTS:
*       none.
*
* RETURNS:
*       MV_OK      - on success.
*       MV_FAIL    - otherwise.
*
* COMMENTS:
*       None.
*
*******************************************************************************/

MV_STATUS bspWarmRestart
(
    MV_VOID
)
{
  static char *envp[] = {"HOME=/", "TERM=linux",
                         "PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL };

  char *argv[] = {"/usr/bin/hsuReboot", NULL };  
  printk("Performing warm restart ... \n");
  call_usermodehelper("/usr/bin/hsuReboot",   argv, envp, 1);
  while(1); // no return
  return MV_OK; // make the compiler happy
}

/*******************************************************************************
* bspHsuMalloc
*
* DESCRIPTION:
*       Allocate a free area for HSU usage.
*
* INPUTS:
*       size_t bytes - number of bytes to allocate
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       pointer to allocated data per success
*       NULL - per failure to allocate space
*
* COMMENTS:
*       None
*
*******************************************************************************/

static unsigned long hsu_base = 0;
static void *hsu_area_base = NULL;

void *bspHsuMalloc(IN size_t bytes)
{
  unsigned long        hsu_len = bytes;
  void *hsu_area;

  if (!hsu_base)
    hsu_base = __pa(high_memory) + HSU_OFFSET;

  request_mem_region(hsu_base, hsu_len, "hsu-area");
  hsu_area = (unsigned long *)ioremap_nocache(hsu_base, hsu_len);  

  if (!hsu_area_base)
    hsu_area_base = hsu_area;

  return hsu_area;
}

/*******************************************************************************
* bspHsuFree
*
* DESCRIPTION:
*       free a hsu area back to pool.
*
* INPUTS:
*       size_t bytes - number of bytes to allocate
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*       None
*
*******************************************************************************/
MV_STATUS bspHsuFree(void * pBuf)
{
  STUB_FAIL;
}

MV_U32  bspVirt2Phys(MV_U32 vAddr)
{

  if (!dma_area_base)
    bspCacheDmaMalloc(1024); 

  if (!hsu_area_base)
    bspHsuMalloc(1024); 

  if (vAddr ==  (MV_U32)((MV_U64)dma_area_base)) // bspCacheDmaMalloc  is done once !
    return dma_base;
  
  else
    return hsu_base;
}

MV_U32 bspPhys2Virt(MV_U32 pAddr)
{
  STUB_FAIL;
}

int bspAdv64Malloc32bit = 0;

static struct mv_rootfs_sysctl_settings 
{
	char	info[256];
} mv_rootfs_sysctl_settings;

static int mv_rootfs_info_proc(ctl_table *ctl, int write,
                           void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int pos;
	char *info = mv_rootfs_sysctl_settings.info;
	
	if (!*lenp || (*ppos && !write)) {
		*lenp = 0;
		return 0;
	}

	pos = sprintf(info, "%s", 
                (rootfs_is_initramfs)? "internal initramfs" : "nfs");
	return proc_dostring(ctl, write, buffer, lenp, ppos);
}

static ctl_table mv_rootfs_info[] = 
  {
    {
      .procname	= "rootfs",
      .data		= &mv_rootfs_sysctl_settings.info, 
      .maxlen		= 128,
      .mode		= 0444,
      .proc_handler	= mv_rootfs_info_proc,
    },
    { }
  };

static ctl_table mv_root_table[] = 
  {
    {
      .procname	= "mv",
      .mode		= 0555,
      .child		= mv_rootfs_info,
    },
    { }
  };

static struct ctl_table_header *mv_sysctl_header;

static void mv_sysctl_register(void)
{
	static int initialized;

	if (initialized == 1)
		return;

	mv_sysctl_header = register_sysctl_table(mv_root_table);
	initialized = 1;
}

static void mv_fix_lion_b2b()
{
  struct pci_dev *dev;
  int ignore;
  u64 start0 = 0;
  u64 len0;
  u64 start2 = 0;
  u64 len2;
  void *v_addr;
  u32  intBitIdx, intBitMask;
 
  printk("\nPEX: Marvell device fixup\n");
  
  //fix xbar on xcat2

  dev = NULL;
  for_each_pci_dev(dev)
  {
    if ( (dev->vendor == MARVELL_VENDOR_ID) &&         
         (((dev->device & MV_PP_CHIP_TYPE_MASK) >> MV_PP_CHIP_TYPE_OFFSET) ==
          MV_PP_CHIP_TYPE_XCAT2)
         )
    {
      // xcat2 found
      
      // get bar0      
      start0 = pci_resource_start(dev, 0);
      len0   = pci_resource_len(dev, 0);
      
      // get bar2
      start2 = pci_resource_start(dev, 2);
      len2   = pci_resource_len(dev, 2);
      
      if (start0)
      {
        v_addr = ioremap_nocache(start0, len0);
        if (v_addr)
        {
          printk("Configuring cross bar for xCat2 device 0x%04x \n", dev->device);
          
          /*config bar2 via bar0 */
          writel(0x03ff00c1, v_addr + 0x41820);
          writel(start2,     v_addr + 0x41824);
          
          /*config interrupts to be routed to the PEX */
          intBitIdx = INT_LVL_XCAT2_SWITCH % 32;
          intBitMask = 1 << intBitIdx;
          writel(intBitMask, v_addr + 0x2021c);
          
          iounmap(v_addr);
          
          /* enable switch access to dram - write to switch registers */
          v_addr = ioremap_nocache(start2, len2);
          if (v_addr)
          {
            writel(0xe804,     v_addr + 0x30c);
            writel(0xffff0000, v_addr + 0x310);
            writel(0x3e,       v_addr + 0x34c);
            iounmap(v_addr);
          }
        }     
      }
    }
  }
    
  // enable devices to get acpi interrupt routing
  dev = NULL;
  for_each_pci_dev(dev)
  { 
    if (dev->vendor == MARVELL_VENDOR_ID)
    {
      ignore = pci_enable_device(dev);
      pci_set_master(dev);
      
      // rewrite PCI_INTERRUPT_LINE with newly assigned (acpi) irq
      pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);

#ifdef CONFIG_WG_PLATFORM // WG:JB Save device
      mv_dev = dev;
#endif
    }      
  }
  printk("\n");
}

static int early_serial_base = 0x3f8;  /* ttyS0 */
#define XMTRDY          0x20
#define DLAB		        0x80
#define TXR             0       /*  Transmit register (WRITE) */
#define RXR             0       /*  Receive register  (READ)  */
#define IER             1       /*  Interrupt Enable          */
#define IIR             2       /*  Interrupt ID              */
#define FCR             2       /*  FIFO control              */
#define LCR             3       /*  Line control              */
#define MCR             4       /*  Modem control             */
#define LSR             5       /*  Line Status               */
#define MSR             6       /*  Modem Status              */
#define DLL             0       /*  Divisor Latch Low         */
#define DLH             1       /*  Divisor latch High        */

void xprintk_putc(unsigned char ch)
{
	while ((inb(early_serial_base + LSR) & XMTRDY) == 0);
	outb(ch, early_serial_base + TXR);
}

void xprintk(char *s)
{
	while (*s) 
  {
		if (*s == '\n')
			xprintk_putc('\r');
		xprintk_putc(*s);
		s++;
	}
}

MODULE_AUTHOR("Giora Cohen");
MODULE_LICENSE("GPL");
EXPORT_SYMBOL(bspCacheDmaFree);
EXPORT_SYMBOL(bspCacheDmaMalloc);
EXPORT_SYMBOL(bspCacheFlush);
EXPORT_SYMBOL(bspCacheInvalidate);
EXPORT_SYMBOL(bspDmaRead);
EXPORT_SYMBOL(bspDmaWrite);
EXPORT_SYMBOL(bspEthInputHookAdd);
EXPORT_SYMBOL(bspEthPortDisable);
EXPORT_SYMBOL(bspEthPortEnable);
EXPORT_SYMBOL(bspEthPortRxInit);
EXPORT_SYMBOL(bspEthPortTx);
EXPORT_SYMBOL(bspEthPortTxInit);
EXPORT_SYMBOL(bspEthPortTxQueue);
EXPORT_SYMBOL(bspEthPortTxModeSet);
EXPORT_SYMBOL(bspEthRxPacketFree);
EXPORT_SYMBOL(bspEthTxCompleteHookAdd);
EXPORT_SYMBOL(bspIntConnect);
EXPORT_SYMBOL(bspIntDisable);
EXPORT_SYMBOL(bspIntEnable);
EXPORT_SYMBOL(bspIsr);
EXPORT_SYMBOL(bspPciConfigReadReg);
EXPORT_SYMBOL(bspPciConfigWriteReg);
EXPORT_SYMBOL(bspPciEnableCombinedAccess);
EXPORT_SYMBOL(bspPciGetResourceStart);
EXPORT_SYMBOL(bspPciGetResourceLen);
EXPORT_SYMBOL(bspPciFindDev);
EXPORT_SYMBOL(bspPciGetIntMask);
EXPORT_SYMBOL(bspPciGetIntVec);
EXPORT_SYMBOL(bspPhys2Virt);
EXPORT_SYMBOL(bspReset);
EXPORT_SYMBOL(bspResetInit);
EXPORT_SYMBOL(bspSmiInitDriver);
EXPORT_SYMBOL(bspSmiReadReg);
EXPORT_SYMBOL(bspSmiWriteReg);
EXPORT_SYMBOL(bspTwsiInitDriver);
EXPORT_SYMBOL(bspTwsiMasterReadTrans);
EXPORT_SYMBOL(bspTwsiMasterWriteTrans);
EXPORT_SYMBOL(bspTwsiWaitNotBusy);
EXPORT_SYMBOL(bspEthCpuCodeToQueue);
EXPORT_SYMBOL(bspPciFindDevReset);
EXPORT_SYMBOL(bspEthInit);
EXPORT_SYMBOL(bspVirt2Phys);
EXPORT_SYMBOL(bspAdv64Malloc32bit);
EXPORT_SYMBOL(bspWarmRestart);
EXPORT_SYMBOL(bspHsuMalloc);
EXPORT_SYMBOL(bspHsuFree);
EXPORT_SYMBOL(xprintk);
EXPORT_SYMBOL(xprintk_putc);

#ifdef CONFIG_WG_PLATFORM // WG:JB Init the module
static int pssBspApis_init(void)
{
  mv_fix_lion_b2b();

  if (!mv_dev) {
    printk(KERN_INFO "%s: No Marvell device found\n", __FUNCTION__);
    return -ENODEV;
  }

  mv_sysctl_register();
  return 0;
}
module_init(pssBspApis_init);
MODULE_LICENSE("GPL");
#endif
