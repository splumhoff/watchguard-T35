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
********************************************************************************
* prestera.c
*
* DESCRIPTION:
*       functions in kernel mode special for prestera.
*
* DEPENDENCIES:
*
*       $Revision$
*******************************************************************************/
#if defined(CONFIG_MIPS) && defined(CONFIG_64BIT)
#define MIPS64_CPU
#endif

#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#include <linux/module.h>

#ifdef  PRESTERA_SYSCALLS
#  include <linux/syscalls.h>
#  include <asm/unistd.h>
#endif

#include "./include/presteraGlob.h"
#include "./include/presteraSmiGlob.h"
#include "./include/presteraIrq.h"
#include "./include/prestera.h"
#include <cpssCommon/cpssPresteraDefs.h>
#include <cpss/generic/cpssTypes.h>
#include "gtExtDrv/drivers/pssBspApis.h"

/* #define PRESTERA_DEBUG */

#include "./include/presteraDebug.h"

#ifdef GDA8548
#define consistent_sync(x...)
#endif

#if defined(INTEL64_CPU) || defined(V320PLUS)
#include <linux/signal.h>
extern int send_sig_info(int, struct siginfo *, struct task_struct *);
#endif

#if defined(MIPS64_CPU) || defined(INTEL64_CPU)
#define ADV64_CPU
#endif

#if defined(MIPS64_CPU) || defined(INTEL64_CPU)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define consistent_sync(x...)
#endif
#else
#if defined(XCAT) || defined(DISCODUO)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
#define consistent_sync(x...)
#endif
#endif 
#endif

#if defined(V320PLUS)
#define consistent_sync(x...)
#endif

#define PCI_RESOURCE_PSS_CONFIG     0
#define PCI_RESOURCE_PSS_REGS       1
#define PCI_RESOURCE_PSS_REGS_PEX   2

   /* local variables and variables */
static int                  prestera_opened      = 0;
static int                  prestera_initialized = 0;
static struct Prestera_Dev  *prestera_dev;
static unsigned long        pci_phys_addr[PRV_CPSS_MAX_PP_DEVICES_CNS];
#ifdef XCAT
static unsigned long        dragonite_phys_addr;
#endif
static struct PP_Dev        *ppdevs[PRV_CPSS_MAX_PP_DEVICES_CNS];
static int                  founddevs = 0;
static unsigned long        dev_size = 0;
static int                  prestera_major = PRESTERA_MAJOR;
static struct cdev          prestera_cdev;
static unsigned long        legalDevTypes[] = {CPSS_DEV_TYPES};
static unsigned long        dma_base;
static GT_U32               dma_len;
static void                *dma_area;

#if defined(GDA8548) || defined(XCAT) || defined(INTEL64)
static unsigned long        hsu_base;
static unsigned long        hsu_len;
static void                *hsu_area;
#endif

static void*                dma_tmp_virt = NULL;
static dma_addr_t           dma_tmp_phys = 0;

#ifdef XCAT
#define PROCFS_MAX_SIZE   2560 /* 2,5 Kb */
static unsigned char procfs_buffer[PROCFS_MAX_SIZE];
#endif

module_param(prestera_major,int, S_IRUGO);
MODULE_AUTHOR("Marvell Semi.");
MODULE_LICENSE("GPL");
#define printk_line(x...) printk("error in line %d ", __LINE__); printk(x)
int bspPciFindDevReset(void);
ssize_t presteraSmi_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t presteraSmi_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
int presteraSmi_ioctl(unsigned int, unsigned long);
int presteraSmi_init(void);
static int prestera_DmaRead(
                            unsigned long address,
                            unsigned long length,
                            unsigned long burstLimit,
                            unsigned long buffer);
static int prestera_DmaWrite(
                             unsigned long address,
                             unsigned long length,
                             unsigned long burstLimit,
                             unsigned long buffer);
extern int rx_DSR; /* rx DSR invocation counter */
extern int tx_DSR; /* tx DSR invocation counter */

#ifdef ADV64_CPU
extern int bspAdv64Malloc32bit; /* in bssBspApis.c */
#endif

/************************************************************************
 *
 * And the init and cleanup functions come last
 *
 ************************************************************************/

/*
 * static struct prvPciDeviceQuirks prvPciDeviceQuirks[]
 *
 * Quirks can be added to GT_PCI_DEV_VENDOR_ID structure
 */
PRV_PCI_DEVICE_QUIRKS_ARRAY_MAC

static inline u32 read_u32(volatile void *addr)
{
#ifdef ADV64_CPU
  return readl(addr);
#else
  return *(volatile unsigned long *)addr;
#endif
}

static inline void  write_u32(u32 data, volatile void *addr)
{
#ifdef ADV64_CPU
  writel(data, addr);
#else
  *((unsigned long *)addr) = data;
#endif
}

static struct prvPciDeviceQuirks* prestera_find_quirks(struct PP_Dev *dev)
{
  int k;
  GT_U32 pciId = (dev->devId << 16) | dev->vendorId;

  for (k = 0; prvPciDeviceQuirks[k].pciId != 0xffffffff; k++)
  {
    if (prvPciDeviceQuirks[k].pciId == pciId)
      break;
  }
  return &(prvPciDeviceQuirks[k]);
}


static loff_t prestera_lseek(struct file *filp, loff_t off, int whence)
{
  struct  Prestera_Dev * dev;
  loff_t newpos;
  dev = (struct Prestera_Dev *) filp->private_data;
  switch (whence)
  {
  case 0: /* SEEK_SET */
    newpos = off;
    break;
  case 1: /* SEEK_CUR */
    newpos = filp->f_pos + off;
    break;
  case 2: /* SEEK_END */
    newpos = dev->size + off;
    break;
  default : /* can't happend */
    return -EINVAL;
  }
  if (newpos < 0)
  {
    return -EINVAL;
  }
  if (newpos >= dev->size)
  {
    return -EINVAL;
  }
  filp->f_pos = newpos;
  return newpos;
}
/************************************************************************
 *
 * prestera_ioctl: The device ioctl() implementation
 */
#ifdef V320PLUS
static long prestera_ioctl(struct file *filp,
                          unsigned int cmd, unsigned long arg)

#else
static int prestera_ioctl(struct inode *inode, struct file *filp,
                          unsigned int cmd, unsigned long arg)
#endif
{
  struct PP_Dev          *dev;
  PP_PCI_REG              pciReg;
  PciConfigReg_STC        pciConfReg;
  GT_PCI_Dev_STC          gtDev;
  GT_Intr2Vec             int2vec;
  GT_VecotrCookie_STC     vector_cookie;
  GT_RANGE_STC            range;
  struct intData          *intData;
  int                     i, instance;
  GT_DmaReadWrite_STC     dmaRWparams;
  GT_TwsiReadWrite_STC    twsiRWparams;
  GT_STATUS               rc;
  GT_U32                  temp_len;

  if (_IOC_TYPE(cmd) == PRESTERA_SMI_IOC_MAGIC)
    return presteraSmi_ioctl(cmd, arg);

  /* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
  if (_IOC_TYPE(cmd) != PRESTERA_IOC_MAGIC)
  {
    printk_line("wrong ioctl magic key\n");
    return -ENOTTY;
  }

  /* GETTING DATA */
  switch(cmd)
  {
  case PRESTERA_IOC_READREG:
    /* read and parse user data structur */
    if (copy_from_user(&pciReg,(PP_PCI_REG*) arg, sizeof(PP_PCI_REG)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    /* USER READS */
    if (copy_to_user((PP_PCI_REG*)arg, &pciReg, sizeof(PP_PCI_REG)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_WRITEREG:
    /* read and parse user data structur */
    if (copy_from_user(&pciReg,(PP_PCI_REG*) arg, sizeof(PP_PCI_REG)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_DMAWRITE:
    if (copy_from_user(&dmaRWparams, (GT_DmaReadWrite_STC*)arg, sizeof(dmaRWparams)))
    {
      printk_line("IOCTL: FAUIT: bad param\n");
      return -EFAULT;
    }
    return prestera_DmaWrite(
                             dmaRWparams.address,
                             dmaRWparams.length,
                             dmaRWparams.burstLimit,
                             (unsigned long)dmaRWparams.buffer);
  case PRESTERA_IOC_DMAREAD:
    if (copy_from_user(&dmaRWparams, (GT_DmaReadWrite_STC*)arg, sizeof(dmaRWparams)))
    {
      printk_line("IOCTL: FAUIT: bad param\n");
      return -EFAULT;
    }
    return prestera_DmaRead(
                            dmaRWparams.address,
                            dmaRWparams.length,
                            dmaRWparams.burstLimit,
                            (unsigned long)dmaRWparams.buffer);
  case PRESTERA_IOC_HWRESET:
    break;

  case PRESTERA_IOC_INTCONNECT:
    /* read and parse user data structure */
    if (copy_from_user(&vector_cookie,(PP_PCI_REG*) arg, sizeof(GT_VecotrCookie_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    intConnect(vector_cookie.vector, 0, (struct intData **)&vector_cookie.cookie);
    /* USER READS */
    if (copy_to_user((PP_PCI_REG*)arg, &vector_cookie, sizeof(GT_VecotrCookie_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_INTENABLE:
    /* clear the mask reg on device 0x10*/
    if (arg > 64)
    {
      printk_line("DEBUG!!!\n");
      send_sig_info(SIGSTOP, (struct siginfo*)1, current);
    }
    enable_irq(arg);
    break;

  case PRESTERA_IOC_INTDISABLE:
    disable_irq(arg);
    break;

  case PRESTERA_IOC_WAIT:
    /* cookie */
    intData = (struct intData *) arg;

    /* enable the interrupt vector */
    enable_irq(intData->intVec);

    if (down_interruptible(&intData->sem))
    {
      return -ERESTARTSYS;
    }
    break;

  case PRESTERA_IOC_FIND_DEV:
    /* read and parse user data structurr */
    if (copy_from_user(&gtDev,(GT_PCI_Dev_STC*) arg, sizeof(GT_PCI_Dev_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    instance = 0;
            
    for (i = 0; i < founddevs; i++)
    {
      dev = ppdevs[i];
      if (gtDev.vendorId != dev->vendorId)
      {
        continue;
      }
      if (gtDev.devId != dev->devId)
      {
        continue;
      }
      if (gtDev.instance != dev->instance)
      {
        continue;
      }
      /* Found */
      gtDev.busNo  = dev->busNo;
      gtDev.devSel = dev->devSel; 
      gtDev.funcNo = dev->funcNo;
      break;
    }
    if (i == founddevs)
    {
      return -ENODEV;
    }
    /* READ */
    if (copy_to_user((GT_PCI_Dev_STC*)arg, &gtDev, sizeof(GT_PCI_Dev_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_PCICONFIGWRITEREG:
    /* read and parse user data structure */
    if (copy_from_user(&pciConfReg,(PciConfigReg_STC*) arg, sizeof(PciConfigReg_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }

    rc = _bspPciConfigWriteReg(pciConfReg.busNo,pciConfReg.devSel,pciConfReg.funcNo,pciConfReg.regAddr,pciConfReg.data);
    if(rc != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_PCICONFIGREADREG:
    /* read and parse user data structure */
    if (copy_from_user(&pciConfReg,(PciConfigReg_STC*) arg, sizeof(PciConfigReg_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    rc = _bspPciConfigReadReg(pciConfReg.busNo,pciConfReg.devSel,pciConfReg.funcNo,pciConfReg.regAddr,&(pciConfReg.data));
    if(rc != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    if (copy_to_user((PciConfigReg_STC*)arg, &pciConfReg, sizeof(PciConfigReg_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_GETINTVEC:
    if (copy_from_user(&int2vec,(GT_Intr2Vec*) arg, sizeof(GT_Intr2Vec)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }

    rc = _bspPciGetIntVec(int2vec.intrLine, (void*)&int2vec.vector);
    if(rc != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
            
    if (copy_to_user((GT_Intr2Vec*)arg, &int2vec, sizeof(GT_Intr2Vec)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_FLUSH:
    /* read and parse user data structure */
    if (copy_from_user(&range,(GT_RANGE_STC*) arg, sizeof(GT_RANGE_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    consistent_sync((void*)range.address, range.length, PCI_DMA_TODEVICE);
    break;

  case PRESTERA_IOC_INVALIDATE:
    /* read and parse user data structure */
    if (copy_from_user(&range,(GT_RANGE_STC*) arg, sizeof(GT_RANGE_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    consistent_sync((void*)range.address, range.length, PCI_DMA_FROMDEVICE);
    break;

  case PRESTERA_IOC_GETBASEADDR:
    if (copy_to_user((void*)arg, &dma_base, sizeof(long)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;
  case PRESTERA_IOC_GETDMASIZE:
    temp_len = dma_len;

#ifdef ADV64_CPU
    if (bspAdv64Malloc32bit == 1) /* extern in bssBspApis.c. Defaults to 1 */
      temp_len |= 0x80000000;
#endif

    if (copy_to_user((void *)arg, &temp_len, sizeof(GT_U32)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_TWSIINITDRV:
    if(_bspTwsiInitDriver() != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_TWSIWAITNOBUSY:
    if(_bspTwsiWaitNotBusy() != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    break;

  case PRESTERA_IOC_TWSIWRITE:
    /* read and parse user data structure */
    if (copy_from_user(&twsiRWparams,(GT_TwsiReadWrite_STC*) arg, sizeof(GT_TwsiReadWrite_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    if(_bspTwsiMasterWriteTrans(twsiRWparams.devId, twsiRWparams.pData, 
                               twsiRWparams.len, twsiRWparams.stop) != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }

    break;

  case PRESTERA_IOC_TWSIREAD:
    /* read and parse user data structure */
    if (copy_from_user(&twsiRWparams,(GT_TwsiReadWrite_STC*) arg, sizeof(GT_TwsiReadWrite_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    if(_bspTwsiMasterReadTrans(twsiRWparams.devId, twsiRWparams.pData, 
                              twsiRWparams.len, twsiRWparams.stop) != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }
    if (copy_to_user((GT_TwsiReadWrite_STC*)arg, &twsiRWparams, sizeof(GT_TwsiReadWrite_STC)))
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }

    break;

#ifdef XCAT
  case PRESTERA_IOC_DRAGONITESWDOWNLOAD:
    {
      GT_DragoniteSwDownload_STC params;
    
      /* read and parse user data structure */
      if (copy_from_user(&params, (GT_DragoniteSwDownload_STC*) arg, sizeof(GT_DragoniteSwDownload_STC)))
      {
        printk_line("IOCTL: copy_from_user FAULT\n");
        return -EFAULT;
      }
                
      if(_bspDragoniteSWDownload(params.buffer, params.length) != GT_OK)
      {
        printk_line("IOCTL: bspDragoniteSWDownload FAULT\n");
        return -EFAULT;
      }
    }
            
    break;

  case PRESTERA_IOC_DRAGONITEENABLE:
    if(_bspDragoniteEnableSet(arg) != GT_OK)
    {
      printk_line("IOCTL: bspDragoniteEnableSet FAULT\n");
      return -EFAULT;
    }

    break;

  case PRESTERA_IOC_DRAGONITEINIT:
    if(_bspDragoniteInit() != GT_OK)
    {
      printk_line("IOCTL: FAULT\n");
      return -EFAULT;
    }

    break;

  case PRESTERA_IOC_DRAGONITEGETINTVEC:
    {
      GT_U32 intVecNum;

      if(_bspDragoniteGetIntVec(&intVecNum) != GT_OK)
      {
        printk_line("IOCTL: bspDragoniteGetIntVec FAULT\n");
        return -EFAULT;
      }

      if (copy_to_user((long*)arg, &intVecNum, sizeof(long)))
      {
        printk_line("IOCTL: copy_to_user FAULT\n");
        return -EFAULT;
      }
    }

    break;

  case PRESTERA_IOC_DRAGONITESHAREDMEMREAD:
    {
      GT_DragoniteMemAccsess_STC params;                
                
      /* read and parse user data structure */
      if (copy_from_user(&params, (GT_DragoniteMemAccsess_STC*) arg, sizeof(GT_DragoniteMemAccsess_STC)))
      {
        printk_line("IOCTL: FAULT READ 0 \n");
        return -EFAULT;
      }

      if (params.length > PROCFS_MAX_SIZE)
      {
        printk_line("IOCTL: FAULT READ 1 \n");
        return -EFAULT;
      }
                
      if(_bspDragoniteSharedMemRead(params.offset, procfs_buffer, params.length) != GT_OK)
      {               
        printk_line("IOCTL: FAULT READ 2\n");
        return -EFAULT;
      }

      if (copy_to_user(procfs_buffer, params.buffer, params.length))
      {
        printk_line("IOCTL: FAULT READ 3\n");
        return -EFAULT;
      }
    }
            
    break;
        
  case PRESTERA_IOC_DRAGONITESHAREDMEMWRITE:
    {
      GT_DragoniteMemAccsess_STC params;
                
      /* read and parse user data structure */
      if (copy_from_user(&params, (GT_DragoniteMemAccsess_STC*) arg, sizeof(GT_DragoniteMemAccsess_STC)))
      {
        printk_line("IOCTL: WRITE 0\n");
        return -EFAULT;
      }
    
      if (params.length > PROCFS_MAX_SIZE)
      {
        printk_line("IOCTL: FAULT WRITE 1\n");
        return -EFAULT;
      }
                
      if (copy_from_user(procfs_buffer, params.buffer, params.length))
      {
        printk_line("IOCTL: FAULT WRITE 2\n");
        return -EFAULT;
      }
                            
      if(_bspDragoniteSharedMemWrite(params.offset, procfs_buffer, params.length) != GT_OK)
      {
        printk_line("IOCTL: FAULT WRITE 3\n");
        return -EFAULT;
      }
    }
            
    break; 

  case PRESTERA_IOC_DRAGONITEITCMCRCCHECK:
    if(_bspDragoniteFwCrcCheck() != GT_OK)
    {
      printk("IOCTL: FAULT\n");
      return -EFAULT;
    }
    
    break;
        
#endif /* XCAT */

    
#if defined(GDA8548) || defined(XCAT) || defined(INTEL64)
  case PRESTERA_IOC_HSUWARMRESTART:
    _bspWarmRestart();
    break;
#endif

  default:
    printk_line (KERN_WARNING "Unknown ioctl (%x).\n", cmd);
    break;
  }
  return 0;
}


/*
 * open and close: just keep track of how many times the device is
 * mapped, to avoid releasing it.
 */

void prestera_vma_open(struct vm_area_struct *vma)
{
  prestera_opened++;
}

void prestera_vma_close(struct vm_area_struct *vma)
{
  prestera_opened--;
}

struct vm_operations_struct prestera_vm_ops = {
  .open   = prestera_vma_open,
  .close  = prestera_vma_close,
};

/************************************************************************
 *
 * prestera_mappedVirt2Phys: convert userspace address to physical
 * Only for mmaped areas
 *
 */
static unsigned long prestera_mappedVirt2Phys(
                                              unsigned long address
                                              )
{
  int ppNum;

  switch ((address & 0xf0000000))
  {

#ifdef XCAT
  case PRV_CPSS_EXT_DRV_XCAT_DRAGONITE_BASE_ADDR_CNS:
    address &= 0x0fffffff;
    if (address >= _32K)
      return 0;
    address += dragonite_phys_addr;
    return address;
#endif

#if defined(GDA8548) || defined(XCAT) || defined(INTEL64)
  case PRV_CPSS_EXT_DRV_HSU_BASE_ADDR_CNS:
    address &= 0x0fffffff;
    if (address >= (8 * _1M))
      return 0;
    address += hsu_base;
    return address;
#endif
    
#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
  case 0x20000000:
#else
  case 0x50000000:
#endif
    address &= 0x0fffffff;
    if (address >= dma_len)
      return 0;
    address += dma_base;
    return address;

#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
  case 0x30000000:
#else
  case 0x60000000:
#endif
  case 0x70000000:
    /* PSS regs */
#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
    address -= 0x30000000;
#else
    address -= 0x60000000;
#endif
    ppNum = address >> 26;
    if (ppNum >= founddevs)
    {
      return 0;
    }
    address &= 0x03ffffff;
    address += pci_phys_addr[ppNum];
    return address;
  }

  /* default */
  return 0;
}


/************************************************************************
 *
 * prestera_DmaRead: bspDmaRead() wrapper
 */
static int prestera_DmaRead(
                            unsigned long address,
                            unsigned long length,
                            unsigned long burstLimit,
                            unsigned long buffer
                            )
{
  unsigned long bufferPhys;
  unsigned long tmpLength;

  /* first convert source address to physical */
  address = prestera_mappedVirt2Phys(address);
  if (!address)
    return -EFAULT;

  bufferPhys = prestera_mappedVirt2Phys(buffer);
  if (bufferPhys)
  {
    return _bspDmaRead(address, length, burstLimit, (GT_U32*)bufferPhys);
  }

  /* use dma_tmp buffer */
  while (length > 0)
  {
    tmpLength = (length > (PAGE_SIZE / 4)) ? PAGE_SIZE/4 : length;

    if (_bspDmaRead(address, tmpLength, burstLimit, (GT_U32*)dma_tmp_phys))
      return -EFAULT;

    length -= tmpLength;
    tmpLength *= 4;
    if (copy_to_user((void*)buffer, dma_tmp_virt, tmpLength))
      return -EFAULT;

    address += tmpLength;
    buffer += tmpLength;
  }
  return 0;
}

/************************************************************************
 *
 * prestera_DmaWrite: bspDmaRead() wrapper
 */
static int prestera_DmaWrite(
                             unsigned long address,
                             unsigned long length,
                             unsigned long burstLimit,
                             unsigned long buffer
                             )
{
  unsigned long bufferPhys;
  unsigned long tmpLength;

  /* first convert source address to physical */
  address = prestera_mappedVirt2Phys(address);
  if (!address)
    return -EFAULT;

  bufferPhys = prestera_mappedVirt2Phys(buffer);
  if (bufferPhys)
  {
    return _bspDmaWrite(address, (GT_U32*)bufferPhys, length, burstLimit);
  }
  /* use dma_tmp buffer */
  while (length > 0)
  {
    tmpLength = (length > (PAGE_SIZE / 4)) ? PAGE_SIZE/4 : length;

    if (copy_from_user(dma_tmp_virt, (void*)buffer, tmpLength * 4))
      return -EFAULT;

    if (_bspDmaWrite(address, (GT_U32*)dma_tmp_phys, tmpLength, burstLimit))
      return -EFAULT;

    length -= tmpLength;
    tmpLength *= 4;
    address += tmpLength;
    buffer += tmpLength;
  }
  return 0;
}

/************************************************************************
 *
 * prestera_mmap: The device mmap() implementation
 */
static int prestera_mmap(struct file * file, struct vm_area_struct *vma)
{
  unsigned long   phys;
  unsigned long   ppNum;
  unsigned long   offset;
  unsigned long   pageSize;
#ifdef ADV64_CPU  
  unsigned long   start;
  unsigned long   len;
#endif  
  struct PP_Dev  *ppdev;
  struct prvPciDeviceQuirks *quirks;
  GT_STATUS       rc;

  if (((vma->vm_pgoff)<<PAGE_SHIFT) & (PAGE_SIZE-1))
  {
    /* need aligned offsets */                
    printk_line("prestera_mmap offset not aligned\n");
    return -ENXIO;  
  }
    
  /* bind the prestera_vm_ops */
  vma->vm_ops = &prestera_vm_ops;

  /* VM_IO for I/O memory */
  vma->vm_flags |= VM_IO;

  /* disable caching on mapped memory */
  vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
  vma->vm_private_data = prestera_dev;
    
  switch (vma->vm_start & 0xf0000000)
  {

#ifdef XCAT
  case PRV_CPSS_EXT_DRV_XCAT_DRAGONITE_BASE_ADDR_CNS:
    /* Dragonite communication (irq+comm.prot.) shared memory */
    phys = dragonite_phys_addr;
    pageSize = _32K; /* dtcm size */
    break;
#endif

#if defined(GDA8548) || defined(XCAT) || defined(INTEL64)
  case PRV_CPSS_EXT_DRV_HSU_BASE_ADDR_CNS:
    phys = hsu_base;
    pageSize = 8 * _1M;
    break;
#endif

#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
  case 0x20000000:
#else
  case 0x50000000:
#endif

#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
	  if (vma->vm_start < 0x28000000)
#else
	  if (vma->vm_start < 0x58000000)
#endif
	  {
		  /* DMA memory */
		  phys = dma_base;
		  pageSize = dma_len;
		  break;
	  }
	  else
	  {
		  /* PSS config */    
#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
		  offset = (vma->vm_start - 0x28000000);
#else
		  offset = (vma->vm_start - 0x58000000);
#endif
		  offset -= (vma->vm_pgoff<<PAGE_SHIFT);
		  ppNum = offset / PAGE_SIZE;
		  /*  avoid use of unitilialized pointer  */
		  if (ppNum >= founddevs)
		    {
		    return 1;
		  }                
		  		
		  ppdev = ppdevs[ppNum];
		  quirks = prestera_find_quirks(ppdev);
		
#ifdef ADV64_CPU
		  rc = _bspPciGetResourceStart(ppdev->busNo, ppdev->devSel, ppdev->funcNo, 0,
		  							 &start);
		  if (rc != GT_OK)
		    {
		    printk("bspPciGetResourceStart error\n");
		    return -ENODEV;
		    }
		    
		  rc = _bspPciGetResourceLen(ppdev->busNo, ppdev->devSel, ppdev->funcNo, 0,
		  						   &len);
		  if (rc != GT_OK)
		    {
		    printk("bspPciGetResourceLen error\n");
		    return -ENODEV;
		    }
		    
		  ppdev->config.phys = start;
		  ppdev->config.size = len;
		    
#else
		  rc = _bspPciConfigReadReg(ppdev->busNo,ppdev->devSel,ppdev->funcNo,
		  						  0x10,&ppdev->config.phys);
		  if (rc != GT_OK)
		    {
		    printk("bspPciConfigReadReg error\n");
		    return -ENODEV;
		    }
		    
		  /* only 16 MSB are used */
		  ppdev->config.phys &= 0xFFFF0000;
		  ppdev->config.size   = _1M;
#endif
		    
		  phys = ppdev->config.phys;
		  pageSize = ppdev->config.size;
		    
		  /* skip quirks->config_offset (0x70000) in this region */
		    
#ifdef ADV64_CPU
		  quirks->configOffset = 0x70000;
#endif
		  if (pageSize > quirks->configOffset) /* be paranoid */
		    {
		    phys += quirks->configOffset;
		    pageSize -= quirks->configOffset;
		    }
		    
		  if (pageSize > vma->vm_end - vma->vm_start)
		    pageSize = vma->vm_end - vma->vm_start;
		
		  break;
	  }


#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
  case 0x30000000:
#else
  case 0x60000000:
#endif
  case 0x70000000:
    /* PSS regs */
    
#ifdef	CONFIG_WG_PLATFORM // WG:JB Change mapping locations
    offset = (vma->vm_start - 0x30000000) + ((vma->vm_pgoff)<<PAGE_SHIFT);
#else
    offset = (vma->vm_start - 0x60000000) + ((vma->vm_pgoff)<<PAGE_SHIFT);
#endif
    ppNum = offset / _64M;
    /*  avoid use of unitilialized pointer  */
    if (ppNum >= founddevs)
    {
      return 1;
    }

    ppdev = ppdevs[ppNum];
    quirks = prestera_find_quirks(ppdev);
    phys = ppdev->ppregs.phys;
    pageSize = ppdev->ppregs.size;
          
    break;

  default:
    /* ??? */
    printk_line("unknown range (0%0x)\n", (int)vma->vm_start);
    return 1;
  }
  printk("remap_pfn_range(0x%lx, 0x%lx, 0x%lx, 0x%lx)\n",
         (unsigned long)(vma->vm_start), (unsigned long)(phys >> PAGE_SHIFT),
         (unsigned long)pageSize,
#ifdef ADV64_CPU
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
         (unsigned long)((vma->vm_page_prot).pgprot));
#else
         (unsigned long)vma->vm_page_prot);
#endif
#else
         (unsigned long)vma->vm_page_prot);
#endif

  if (remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT, pageSize,
                      vma->vm_page_prot))
  {
    printk_line("remap_pfn_range failed\n");
    return 1;
  }

  prestera_vma_open(vma);
    
  return 0;
}


/************************************************************************
 *
 * prestera_open: The device open() implementation
 */
static int prestera_open(struct inode * inode, struct file * filp)
{
  if (down_interruptible(&prestera_dev->sem))
  {
    return -ERESTARTSYS;
  }

  if (!prestera_initialized)
  {
    up(&prestera_dev->sem);
    return -EIO;
  }

#ifndef SHARED_MEMORY
  /* Avoid single-usage restriction for shared memory:
   * device should be accessible for multiple clients. */
  if (prestera_opened)
  {
    up(&prestera_dev->sem);
    return -EBUSY;
  }
#endif

  filp->private_data = prestera_dev;

  prestera_opened++;
  up(&prestera_dev->sem);

  _printk("presteraDrv: prestera device opened successfuly\n");

  return 0;
}


/************************************************************************
 *
 * prestera_release: The device close() implementation
 */
static int prestera_release(struct inode * inode, struct file * file)
{
  printk("prestera_release\n");
  prestera_opened--;
  if (prestera_opened == 0)
  {
    cleanupInterrupts();
  }
  return 0;
}

/************************************************************************
 *
 * proc read data rooutine
 */
int prestera_read_proc_mem(char * page, char **start, off_t offset, int count, int *eof, void *data)
{
  int len;
  struct PP_Dev *ppdev;
  unsigned long address;
  unsigned int  value;
  volatile unsigned long *ptr;
  int i, j;
  extern int short_bh_count;
    
  len = 0;
  len += sprintf(page+len,"short_bh_count %d\n", short_bh_count);
  len += sprintf(page+len,"rx_DSR %d\n", rx_DSR);
  len += sprintf(page+len,"tx_DSR %d\n", tx_DSR);

  for (i = 0; i < founddevs; i++)
  {
    ppdev = ppdevs[i];
    len += sprintf(page+len,"Device %d\n", i);
    len += sprintf(page+len,"\tbus(0x%0lx) slot(0x%0lx) func(0x%0lx)\n",
                   (unsigned long)ppdev->busNo,
                   (unsigned long)ppdev->devSel, 
                   (unsigned long)ppdev->funcNo);
    len += sprintf(page+len,"\tvendor_id(0x%0x) device_id(0x%0x)\n",ppdev->vendorId, ppdev->devId);
    /*len += sprintf(page+len,"\tirq(0x%0x)\n",dev->irq);*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
    /*len += sprintf(page+len,"\tname(%s)\n",pci_pretty_name(dev));*/
#else
    /*len += sprintf(page+len,"\tname(%s)\n",pci_name(dev));*/
#endif
    /*for (j = 0; j < DEVICE_COUNT_COMPATIBLE; j++)
      {
      len += sprintf(page+len,"\t\t%d start(0x%0llx) end(0x%0llx)\n",j, (unsigned long long)dev->resource[j].start, (unsigned long long)dev->resource[j].end);
      }*/
    address = ppdev->config.base;
    len += sprintf(page+len,"\tconfig map to (0x%0lx) len = (0x%0lx)\n", address,ppdev->config.size);

#ifdef GDA8548
    address = ppdev->ppregs.base;
    len += sprintf(page+len,"\tregs map to (0x%0lx) len = (0x%0lx)\n" , address,ppdev->ppregs.size);

    continue; /* wa for incorrect pex offset */
#endif

    for (j = 0; j < 0x14; j += 4)
    {
      ptr = (volatile unsigned long *) address;
      value = read_u32((void *)ptr);

      GT_SYNC;
      len += sprintf(page+len,"\t\toff(0x%0x) add(0x%0x) val(0x%0x)\n", j, (int)ptr, le32_to_cpu(value));
      address += 4;
    }
    /* Mask */
    address = ppdev->config.base + 0x118;
    ptr = (volatile unsigned long *) address;
    value = read_u32((void *)ptr);
    GT_SYNC;
    len += sprintf(page+len,"\tMASK(0x%0x) add(0x%0x) val(0x%0x)\n", 0x118, (int)ptr, le32_to_cpu(value));

    /* Cause */
    address = ppdev->config.base + 0x114;
    ptr = (volatile unsigned long *) address;
    value = read_u32((void *)ptr);
    GT_SYNC;
    len += sprintf(page+len,"\tMASK(0x%0x) add(0x%0x) val(0x%0x)\n", 0x114, (int)ptr, le32_to_cpu(value));

    address = ppdev->ppregs.base;
    len += sprintf(page+len,"\tregs map to (0x%0lx) len = (0x%0lx)\n" , address,ppdev->ppregs.size);

    ptr = (unsigned long *) (address + 0x0);
    value = read_u32((void *)ptr);
    GT_SYNC;
    len += sprintf(page+len,"\t\toff(0x%0x) val(0x%0x)\n", 0x0, le32_to_cpu(value));

    ptr = (unsigned long *) (address + 0x50);
    value = read_u32((void *)ptr);
    GT_SYNC;
    len += sprintf(page+len,"\t\toff(0x%0x) val(0x%0x)\n", 0x50, le32_to_cpu(value));

    ptr = (unsigned long *) (address + 0x1000000);
    value = read_u32((void *)ptr);
    GT_SYNC;
    len += sprintf(page+len,"\t\toff(0x%0x) val(0x%0x)\n", 0x1000000, le32_to_cpu(value));
        
#ifndef ADV64_CPU
    ptr = (unsigned long *) (address + 0x2000000);
    value = read_u32((void *)ptr);
    GT_SYNC;
    len += sprintf(page+len,"\t\toff(0x%0x) val(0x%0x)\n", 0x2000000, le32_to_cpu(value));
        
    ptr = (unsigned long *) (address + 0x3000000);
    value = read_u32((void *)ptr);
    GT_SYNC;
    len += sprintf(page+len,"\t\toff(0x%0x) val(0x%0x)\n", 0x3000000, le32_to_cpu(value));
#endif
  } 

  *eof = 1;

  return len;
}

static int pp_find_all_devices(GT_BOOL look4cpuEnabled)
{
  struct PP_Dev   *ppdev;
  int             type, i;
  unsigned long   start;
  unsigned long   len;
  GT_U32          data;
  GT_U32          addrCompletion;
#ifndef ADV64_CPU
  GT_U32 regAddr;
#endif

  GT_STATUS rc;
  GT_U32 busNo,devSel,funcNo;
  GT_U32 instance = 0;
  GT_U16 devId,vendorId;
  /*founddevs   = 0;*/
  dev_size    = 0;
  type        = 0;

  while (legalDevTypes[type] != CPSS_DEV_END)
  {   

    devId = legalDevTypes[type] >> 16;
    vendorId = legalDevTypes[type] & 0xFFFF; 

    if ((look4cpuEnabled == GT_FALSE && ((devId & 0x2) == 0)) ||
        (look4cpuEnabled == GT_TRUE && ((devId & 0x2) == 0x2)))
    {
      type++;
      instance = 0;
      continue;
    }

    rc = _bspPciFindDev(vendorId,devId,instance,&busNo, &devSel,&funcNo);
    if (rc != GT_OK)
    {
      /* no more devices */
      instance =0;
      type++;
      continue;
    }


    /* Save PCI device */
    ppdev = (struct PP_Dev *) kmalloc(sizeof(struct PP_Dev), GFP_KERNEL);
    if (NULL == ppdev)
    {
      printk("kmalloc failed\n");
      return -ENOMEM;
    }

    ppdev->devId = devId;
    ppdev->vendorId = vendorId;
    ppdev->instance = instance;
    ppdev->busNo = busNo;
    ppdev->devSel = devSel;
    ppdev->funcNo = funcNo;
        
    ppdevs[founddevs] = ppdev;
    instance++;
    founddevs++;        
  }

  if (look4cpuEnabled == GT_FALSE)
  {
    return 0;
  }
    
  /* Map registers space */
  for (i = 0; i < founddevs; i++)
  {
    struct prvPciDeviceQuirks *quirks;

    ppdev = ppdevs[i];
    quirks = prestera_find_quirks(ppdev);

    /* For PEX registers BAR is in the 0x18 */

#ifdef ADV64_CPU
    rc = _bspPciGetResourceStart(ppdev->busNo, ppdev->devSel, ppdev->funcNo, 2,
                                 &start);
    if (rc != GT_OK)
    {
      printk("bspPciGetResourceStart error\n");
      return -ENODEV;
    }
    
    rc = _bspPciGetResourceLen(ppdev->busNo, ppdev->devSel, ppdev->funcNo, 2,
                                  &len);
    if (rc != GT_OK)
    {
      printk("bspPciGetResourceLen error\n");
      return -ENODEV;
    }
#else
    regAddr = (quirks->isPex) ? 0x18 : 0x14;        
    rc = _bspPciConfigReadReg(ppdev->busNo,ppdev->devSel,ppdev->funcNo,regAddr,&start);
    if (rc != GT_OK)
    {
      printk("bspPciConfigReadReg error\n");
      return -ENODEV;
    }

    /* only 16 MSB are used */
    start &= 0xFFFF0000;
    len   = _64M;
#endif

    /* Reserve BAR1/2 region for normal IO access */
    if (!request_mem_region(start, len, "prestera - registers")) 
    {
      printk("Cannot reserve MMI0 region (prestera_mem_reg) 0x%lx @ 0x%lx\n",
             start, len);
      return -ENODEV;
    }

    ppdev->ppregs.allocbase = start;
    ppdev->ppregs.allocsize = len;
    ppdev->ppregs.size = len;
    ppdev->ppregs.phys = start;
    /* Map rgisters space to kernel virtual address space */
    ppdev->ppregs.base = (unsigned long)ioremap_nocache(start, len);

    /* save phys address */
    pci_phys_addr[i] = start;

    dev_size += len;

    if (!quirks->isPex)
    {
      /* Default configuration for address completion */
      data = read_u32((void *)ppdev->ppregs.base);
      addrCompletion = cpu_to_le32(0x01000100);
      write_u32(addrCompletion, (volatile void *)ppdev->ppregs.base);
      GT_SYNC;
      write_u32(addrCompletion, (volatile void *)ppdev->ppregs.base);
      GT_SYNC;
      write_u32(addrCompletion, (volatile void *)ppdev->ppregs.base);
      GT_SYNC;
      write_u32(addrCompletion, (volatile void *)ppdev->ppregs.base);
      GT_SYNC;
    }  
    data = read_u32((void *)ppdev->ppregs.base);
  }

  /* Map config space */
  for (i = 0; i < founddevs; i++)
  {
    struct prvPciDeviceQuirks *quirks;

    ppdev = ppdevs[i];
    quirks = prestera_find_quirks(ppdev);
        
#ifdef ADV64_CPU
    rc = _bspPciGetResourceStart(ppdev->busNo, ppdev->devSel, ppdev->funcNo, 0,
                                 &start);
    if (rc != GT_OK)
    {
      printk("bspPciGetResourceStart error\n");
      return -ENODEV;
    }
    
    rc = _bspPciGetResourceLen(ppdev->busNo, ppdev->devSel, ppdev->funcNo, 0,
                                  &len);
    if (rc != GT_OK)
    {
      printk("bspPciGetResourceLen error\n");
      return -ENODEV;
    }

#else
    rc = _bspPciConfigReadReg(ppdev->busNo,ppdev->devSel,ppdev->funcNo,0x10,&start);
    if (rc != GT_OK)
    {
      printk("bspPciConfigReadReg error\n");
      return -ENODEV;
    }

    /* only 16 MSB are used */
    start &= 0xFFFF0000;
    len   = _1M;
#endif

    /* Reserve BAR0 region for extended PCI configuration */ 
    if (!request_mem_region(start, len, "prestera - config")) 
    {
      printk("Cannot reserve MMIO region (prestera_ext_pci) 0x%lx @ 0x%lx\n",
             start, len);
      return -ENODEV;
    }
    ppdev->config.allocbase = start;
    ppdev->config.allocsize = len;

    /* skip quirks->config_offset (0x70000) in this region */

#ifdef ADV64_CPU
    quirks->configOffset = 0x70000;
#endif
    
      if (len > quirks->configOffset) /* be paranoid */
      {
        start += quirks->configOffset;
        len -= quirks->configOffset;
      }
    
    /* Find config space size */
    ppdev->config.size = len;
    ppdev->config.phys = start;

    /* Map config space to kernel virtual address space */
    ppdev->config.base = (unsigned long)ioremap_nocache(start, len);
    dev_size += len;

    /* Note: 
       It seems  that pex offset of 0xf0000 in lion for bar0 is wrong. 
       It is 0x70000, and it leads to hangs on gda8548/ep8548.
       
       It also appears that nobody in cpss uses bar0 !.
       Therefore, instead of fixing the quirks macro in cpss we skip 
       reading bar0 in the case of gda8548 . Giora
    */

#ifndef GDA8548
    data = read_u32((void *)ppdev->config.base);
#endif
  }

#ifndef GDA8548
  for (i = 0; i < founddevs; i++)
  {
    ppdev = ppdevs[i];
    data = read_u32((void *)ppdev->config.base);
    data = read_u32((void *)ppdev->config.base);
  }
#endif

#ifdef XCAT
  if(_bspDragoniteSharedMemoryBaseAddrGet(&dragonite_phys_addr) != GT_OK)
  {
    printk("bspDragoniteSharedMemoryBaseAddrGet failed\n");
    return -ENODEV;
  }
#endif

  return 0;
}


static struct file_operations prestera_fops =
  {
    .llseek = prestera_lseek,
    .read   = presteraSmi_read,
    .write  = presteraSmi_write,
#ifdef V320PLUS
    .unlocked_ioctl  = prestera_ioctl,
#else
    .ioctl  = prestera_ioctl,
#endif
    .mmap   = prestera_mmap,
    .open   = prestera_open,
    .release= prestera_release /* A.K.A close */
  };

#ifdef PRESTERA_SYSCALLS
/************************************************************************
 *
 * syscall entries for fast calls
 *
 ************************************************************************/
/* fast call to prestera_ioctl() */
asmlinkage long sys_prestera_ctl(unsigned int cmd, unsigned long arg)
{
#ifdef V320PLUS
    return prestera_ioctl(NULL, cmd, arg);
#else
    return prestera_ioctl(NULL, NULL, cmd, arg);
#endif
}

extern long sys_call_table[];

#define OWN_SYSCALLS 1

#ifdef __NR_SYSCALL_BASE
#  define __SYSCALL_TABLE_INDEX(name) (__NR_##name-__NR_SYSCALL_BASE)
#else
#  define __SYSCALL_TABLE_INDEX(name) (__NR_##name)
#endif

#define __TBL_ENTRY(name) { __SYSCALL_TABLE_INDEX(name), (long)sys_##name, 0 }
static struct {
    int     entry_number;
    long    own_entry;
    long    saved_entry;
} override_syscalls[OWN_SYSCALLS] = {
    __TBL_ENTRY(prestera_ctl)
};
#undef  __TBL_ENTRY


/*******************************************************************************
* prestera_OverrideSyscalls
*
* DESCRIPTION:
*       None
*
* COMMENTS:
*       None
*
*******************************************************************************/
static int
prestera_OverrideSyscalls(void)
{
    int k;
    for (k = 0; k < OWN_SYSCALLS; k++)
    {
        override_syscalls[k].saved_entry =
            sys_call_table[override_syscalls[k].entry_number];
        sys_call_table[override_syscalls[k].entry_number] =
            override_syscalls[k].own_entry;
    }
    return 0;
}

/*******************************************************************************
* prestera_RestoreSyscalls
*
* DESCRIPTION:
*       Restore original syscall entries.
*
* INPUTS:
*       None
*
* OUTPUTS:
*       None
*
* RETURNS:
*       None
*
* COMMENTS:
*       None
*
*******************************************************************************/
static int
prestera_RestoreSyscalls(void)
{
    int k;
    for (k = 0; k < OWN_SYSCALLS; k++)
    {
        if (override_syscalls[k].saved_entry)
            sys_call_table[override_syscalls[k].entry_number] =
                override_syscalls[k].saved_entry;
    }
    return 0;
}
#endif /* PRESTERA_SYSCALLS */


/************************************************************************
 *
 * prestera_cleanup: 
 */
static void prestera_cleanup(void)
{
  int i;
  struct PP_Dev *ppdev;

  printk("Prestera Says: Bye world from kernel\n");

  prestera_initialized = 0;

  cleanupInterrupts();

  for (i = 0; i < founddevs; i++)
  {
    struct prvPciDeviceQuirks *quirks;

    ppdev = ppdevs[i];
    quirks = prestera_find_quirks(ppdev);
        
    /* Unmap the memory regions */
    iounmap((void *)ppdev->config.base);
    iounmap((void *)ppdev->ppregs.base);
        
    /* relaese BAR0 */
    release_mem_region(ppdev->config.allocbase,ppdev->config.allocsize);
    /* relaese BAR1 */
    release_mem_region(ppdev->ppregs.allocbase,ppdev->ppregs.allocsize);

    kfree(ppdev);
  }
  founddevs = 0;

  if (dma_tmp_virt)
  {
    dma_free_coherent(NULL, PAGE_SIZE, dma_tmp_virt, dma_tmp_phys);
    dma_tmp_virt = NULL;
  }

   _bspCacheDmaFree(dma_area);

#ifdef PRESTERA_SYSCALLS
  prestera_RestoreSyscalls();
#endif
  remove_proc_entry("mvPP", NULL);

  cdev_del(&prestera_cdev);

  unregister_chrdev_region(MKDEV(prestera_major, 0), 1);
}

/************************************************************************
 *
 * prestera_init: 
 */
static int prestera_init(void)
{
  int         result = 0;

  printk(KERN_DEBUG "prestera_init\n");

  /* first thing register the device at OS */
    
  /* Register your major. */
  result = register_chrdev_region(MKDEV(prestera_major, 0), 1, "mvPP");
  if (result < 0)
  {
    printk("prestera_init: register_chrdev_region err= %d\n", result);
    return result;
  }

  cdev_init(&prestera_cdev, &prestera_fops);

  prestera_cdev.owner = THIS_MODULE;

  result = cdev_add(&prestera_cdev, MKDEV(prestera_major, 0), 1);
  if (result)
  {
    unregister_chrdev_region(MKDEV(prestera_major, 0), 1);
    printk("prestera_init: cdev_add err= %d\n", result);
    return result;
  }

  printk(KERN_DEBUG "prestera_major = %d\n", prestera_major);

  prestera_dev = (struct Prestera_Dev *) kmalloc(sizeof(struct Prestera_Dev), GFP_KERNEL);
  if (!prestera_dev)
  {
    printk("\nPresteraDrv: Failed allocating memory for device\n");
    result = -ENOMEM;
    goto fail;
  }

#ifdef PRESTERA_SYSCALLS
  prestera_OverrideSyscalls();
#endif
  /* create proc entry */
  create_proc_read_entry("mvPP", 0, NULL, prestera_read_proc_mem, NULL);

  /* initialize the device main semaphore */
  sema_init(&prestera_dev->sem, 1);

  memset(pci_phys_addr, 0, sizeof(pci_phys_addr));

  _bspPciFindDevReset();
    
  /* first add devices with CPU disable*/
  /* This is done to be in sync with cpss */
  result = pp_find_all_devices(GT_FALSE);
  if (0 != result)
  {
    goto fail;
  }
  /* add devices with CPU enable*/
  result = pp_find_all_devices(GT_TRUE);
  result = presteraSmi_init();
  if (0 != result)
  {
    goto fail;
  }
  result = initInterrupts();
  if (0 != result)
  {
    goto fail;
  }
  dma_len  = 2 * _1M;
  dma_area = (void *)_bspCacheDmaMalloc(dma_len);
  dma_base = _bspVirt2Phys((GT_U32)dma_area);    
  printk("DMA - dma_area: 0x%lx(v) ,dma_base: 0x%lx(p), dma_len: 0x%lx\n",
          (unsigned long)dma_area,(unsigned long)dma_base, dma_len);

#if defined(GDA8548) || defined(XCAT) || defined(INTEL64)
  hsu_len = 8 * _1M;
  hsu_area = (void *)_bspHsuMalloc(hsu_len);
  hsu_base = _bspVirt2Phys((GT_U32)hsu_area);    
  printk("HSU - hsu_area: 0x%lx(v) ,hsu_base: 0x%lx(p), hsu_len: 0x%lx\n",
          (unsigned long)hsu_area,(unsigned long)hsu_base, hsu_len);

#endif
  
#ifndef ADV64_CPU
  /* allocate temp area for bspDma operations */
  dma_tmp_virt = dma_alloc_coherent(NULL, PAGE_SIZE, &dma_tmp_phys,
                                    GFP_DMA | GFP_KERNEL);
  if (!dma_tmp_virt)
  {
    printk("failed to allocate page for bspDma*() operations\n");
    return -ENOMEM;
  }
#endif

  prestera_initialized = 1;

  printk(KERN_DEBUG "prestera_init finished\n");

  return 0;

 fail:
  prestera_cleanup();

  printk("\nPresteraDrv: Init FAIL!\n");
  return result;
}

module_init(prestera_init);
module_exit(prestera_cleanup);
