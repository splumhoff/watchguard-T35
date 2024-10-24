/***************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2007,2008,2009,2010,2011 Intel Corporation. All rights reserved.
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
 *   Copyright(c) 2007,2008,2009, 2010,2011 Intel Corporation. All rights reserved.
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
 *
 ***************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#ifdef	CONFIG_WG_KERNEL_4_14
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/cdev.h>

#include "qat_mem.h"

#define PAGE_ORDER 5
#define MAX_MEM_ALLOC (PAGE_SIZE * (2 << PAGE_ORDER) - sizeof(qat_mem_config))

static int major;
static unsigned long bytesToPageOrder(long int memSize);

module_param(major, int, S_IRUGO);

/******************************************************************************
* function:
*         qat_mem_read(struct file *filp, char __user *buffer, size_t length,
*         	       loff_t *offset)
*
* @param filp   [IN] - unused
* @param buffer [IN] - unused
* @param length [IN] - unused
* @param offset [IN] - unused
*
* description:
*   Callback for read operations on the device node. We don't support them.
*
******************************************************************************/
static ssize_t qat_mem_read(struct file *filp, char __user * buffer,
                            size_t length, loff_t * offset)
{
    return -EIO;
}

/******************************************************************************
* function:
*         qat_mem_write(struct file *filp, char __user *buffer, size_t length,
*         	       loff_t *offset)
*
* @param filp [IN] - unused
* @param buff [IN] - unused
* @param leng [IN] - unused
* @param off  [IN] - unused
*
* description:
*   Callback for write operations on the device node. We don't support them.
*
******************************************************************************/
static ssize_t qat_mem_write(struct file *filp, const char __user * buff,
                             size_t len, loff_t * off)
{
    return -EIO;
}

/******************************************************************************
* function:
*         do_ioctl(qat_mem_config *mem, unsigned int cmd, unsigned long arg)
*
* @param mem [IN] - pointer to mem structure
* @param cmd [IN] - ioctl number requested 
* @param arg [IN] - any arg needed by ioctl implementaion 
*
* description:
*   Callback for ioctl operations on the device node. This is our control path.
*   We support two ioctls, QAT_MEM_MALLOC and QAT_MEM_FREE.
*
******************************************************************************/
static int do_ioctl(qat_mem_config * mem, unsigned int cmd, unsigned long arg)
{

    switch (cmd)
    {
        case QAT_MEM_MALLOC:
            if (mem->length <= 0)
            {
                printk
                    ("%s: invalid inputs in qat_mem_config structure!\n",
                     __func__);
                return -EINVAL;
            }

            if (mem->length > MAX_MEM_ALLOC)
            {
                printk
                    ("%s: memory requested (%d) greater than max allocation (%ld)\n",
                     __func__, mem->length, MAX_MEM_ALLOC);
                return -EINVAL;
            }
            mem->virtualAddress =
                (uintptr_t) __get_free_pages(GFP_KERNEL,
                                             bytesToPageOrder(mem->length));
            if (mem->virtualAddress == (uintptr_t) 0)
            {
                printk("%s: __get_free_pages() failed\n", __func__);
                return -EINVAL;
            }

            mem->physicalAddress =
                (uintptr_t) virt_to_phys((void *)(mem->virtualAddress));
            mem->signature = 0xDEADBEEF;
            memcpy((unsigned char *)mem->virtualAddress, mem, sizeof(*mem));

            if (copy_to_user((void *)arg, mem, sizeof(*mem)))
            {
                printk("%s: copy_to_user failed\n", __func__);
                return -EFAULT;
            }
            break;

        case QAT_MEM_FREE:
            if ((void *)mem->virtualAddress == NULL)
            {
                printk
                    ("%s: invalid inputs in qat_mem_config structure !\n",
                     __func__);
                return -EINVAL;
            }

            free_pages((unsigned long)mem->virtualAddress,
                       bytesToPageOrder(mem->length));
            break;

        default:
            printk("%s: unknown request\n", __func__);
            return -ENOTTY;
    }

    return 0;

}

/******************************************************************************
* function:
*         qat_mem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
*
* @param file [IN] - unused
* @param cmd  [IN] - ioctl number requested
* @param arg  [IN] - any arg needed by the ioctl implementation
*
* description:
*   Parameter-check the ioctl call before calling do_ioctl() to do the actual
*   work.
*
* @see do_ioctl()
*
******************************************************************************/
static long
qat_mem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    qat_mem_config mem;

    if (_IOC_SIZE(cmd) != sizeof(mem))
    {
        printk("%s: invalid parameter length\n", __func__);
        return -EINVAL;
    }
    if (copy_from_user(&mem, (unsigned char *)arg, sizeof(mem)))
    {
        printk("%s: copy_from_user failed\n", __func__);
        return -EFAULT;
    }

    return do_ioctl(&mem, cmd, arg);
}

/******************************************************************************
* function:
*         qat_mem_mmap(struct file *filp, struct vm_area_struct *vma)
*
* @param filp [IN]    - unused
* @param vma  [INOUT] - struct containing details of the requested mmap, and
*                       also the resulting offset
*
* description:
*   Callback for mmap operations on the device node. This is identical to the
*   /dev/kmem device on some Linux distros, but others have removed this for
*   security reasons so we have to re-implement it.
*
******************************************************************************/
static int qat_mem_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    int ret;

    /* Convert the vm_pgoff page frame number to an address, then a physical
       address, then convert it back to a page frame number. The final result
       of this is to ensure that pfn is a _physical_ page frame number */
    pfn = __pa((u64) vma->vm_pgoff << PAGE_SHIFT) >> PAGE_SHIFT;
    if (!pfn_valid(pfn))
    {
        printk("%s: invalid pfn\n", __func__);
        return -EIO;
    }
    vma->vm_pgoff = pfn;

    if ((ret = remap_pfn_range(vma, vma->vm_start,
                               vma->vm_pgoff,
                               vma->vm_end - vma->vm_start,
                               vma->vm_page_prot)) < 0)
    {
        printk("%s: remap_pfn_range failed, returned %d\n", __func__, ret);
        return ret;
    }

    return 0;
}

static struct file_operations fops = {
    .read = qat_mem_read,
    .write = qat_mem_write,
    .unlocked_ioctl = qat_mem_ioctl,
    .mmap = qat_mem_mmap,
};

/******************************************************************************
* function:
*         qat_mem_module_init(void)
*
* description:
*   Initialise the QAT memory allocator kernel module.
*
******************************************************************************/
static int __init qat_mem_module_init(void)
{
    int results;
    int count = 10;
    int cdev_minor = 0;
    struct cdev *qat_mem_cdev;
    dev_t dev;

    dev = MKDEV(major, cdev_minor);

    results = alloc_chrdev_region(&dev, cdev_minor, count, "qat_mem");
    if (results < 0)
    {
        printk(KERN_WARNING "%s: can't get dynamic major number.\n", __func__);
        goto next;
    }

    major = MAJOR(dev);
    printk(KERN_ERR "%s: get dynamic major %d\n", __func__, major);

    qat_mem_cdev = cdev_alloc();
    kobject_set_name(&qat_mem_cdev->kobj, "qat_mem %d", major);
    cdev_init(qat_mem_cdev, &fops);

    results = cdev_add(qat_mem_cdev, dev, major);

    if (results)
        printk(KERN_WARNING "%s: qat_mem cdev add failed : %d. \n", __func__,
               results);

    return results;

next:
    results = register_chrdev(0, "qat_mem", &fops);
    if (results < 0)
    {
        printk("Can't get major: %d\n", results);
        return results;
    }
    printk("Assign major: %d\n", results);
    return 0;
}

/******************************************************************************
* function:
*         qat_mem_module_exit(void)
*
* description:
*   Cleanup before unloading the kernel module.
*
******************************************************************************/
static void __exit qat_mem_module_exit(void)
{
    unregister_chrdev(major, "qat_mem");
}

/******************************************************************************
* function:
*         bytesToPageOrder(long int memSize)
*
* @param memSize [IN] - number of bytes requested
*
* description:
*   Return the ln2 of the number of pages needed to store memSize bytes.
*
******************************************************************************/
static unsigned long bytesToPageOrder(long int memSize)
{
    if (memSize <= PAGE_SIZE)
        return 0;
    else if (memSize <= PAGE_SIZE * 1 << 1)
        return 1;
    else if (memSize <= PAGE_SIZE * 1 << 2)
        return 2;
    else if (memSize <= PAGE_SIZE * 1 << 3)
        return 3;
    else if (memSize <= PAGE_SIZE * 1 << 4)
        return 4;
    else if (memSize <= PAGE_SIZE * 1 << 5)
        return 5;
    else
        return -1;
}

module_init(qat_mem_module_init);
module_exit(qat_mem_module_exit);
MODULE_LICENSE("GPL");
