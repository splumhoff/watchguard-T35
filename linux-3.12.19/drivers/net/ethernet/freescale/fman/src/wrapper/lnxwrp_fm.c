/*
 * Copyright 2008-2012 Freescale Semiconductor Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 @File          lnxwrp_fm.c
 @Author        Shlomi Gridish
 @Description   FM Linux wrapper functions.
*/

#include <linux/version.h>
#include <linux/slab.h>
#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif
#ifdef MODVERSIONS
#include <config/modversions.h>
#endif /* MODVERSIONS */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <linux/fsl/qe.h>        /* For struct qe_firmware */
#include <sysdev/fsl_soc.h>
#include <asm/fsl_pm.h>
#include <linux/stat.h>	   /* For file access mask */
#include <linux/skbuff.h>
#include <linux/proc_fs.h>

/* NetCommSw Headers --------------- */
#include "std_ext.h"
#include "error_ext.h"
#include "sprint_ext.h"
#include "debug_ext.h"
#include "sys_io_ext.h"

#include "fm_ioctls.h"

#include "lnxwrp_fm.h"
#include "lnxwrp_resources.h"
#include "lnxwrp_sysfs_fm.h"
#include "lnxwrp_sysfs_fm_port.h"
#include "lnxwrp_exp_sym.h"
#include "fm_common.h"
#include "../../fman/Peripherals/FM/fm.h"
#define __ERR_MODULE__  MODULE_FM

extern struct device_node *GetFmPortAdvArgsDevTreeNode (struct device_node *fm_node,
                                                         e_FmPortType       portType,
                                                         uint8_t            portId);

#define PROC_PRINT(args...) offset += sprintf(buf+offset,args)

#define ADD_ADV_CONFIG_NO_RET(_func, _param)    \
    do {                                        \
        if (i<max){                             \
            p_Entry = &p_Entrys[i];             \
            p_Entry->p_Function = _func;        \
            _param                              \
            i++;                                \
        }                                       \
        else                                    \
            REPORT_ERROR(MAJOR, E_INVALID_VALUE,\
                         ("Number of advanced-configuration entries exceeded"));\
    } while (0)

/* Bootarg used to override the Kconfig FSL_FM_MAX_FRAME_SIZE value */
#define FSL_FM_MAX_FRM_BOOTARG     "fsl_fm_max_frm"

/* Bootarg used to override FSL_FM_RX_EXTRA_HEADROOM Kconfig value */
#define FSL_FM_RX_EXTRA_HEADROOM_BOOTARG  "fsl_fm_rx_extra_headroom"

/* Minimum and maximum value for the fsl_fm_rx_extra_headroom bootarg */
#define FSL_FM_RX_EXTRA_HEADROOM_MIN 16
#define FSL_FM_RX_EXTRA_HEADROOM_MAX 384

#define FSL_FM_PAUSE_TIME_ENABLE 0xf000
#define FSL_FM_PAUSE_TIME_DISABLE 0
#define FSL_FM_PAUSE_THRESH_DEFAULT 0

/*
 * Max frame size, across all interfaces.
 * Configurable from Kconfig or bootargs, to avoid allocating
 * oversized (socket) buffers when not using jumbo frames.
 * Must be large enough to accommodate the network MTU, but small enough
 * to avoid wasting skb memory.
 *
 * Could be overridden once, at boot-time, via the
 * fm_set_max_frm() callback.
 */
int fsl_fm_max_frm = CONFIG_FSL_FM_MAX_FRAME_SIZE;

/*
 * Extra headroom for Rx buffers.
 * FMan is instructed to allocate, on the Rx path, this amount of
 * space at the beginning of a data buffer, beside the DPA private
 * data area and the IC fields.
 * Does not impact Tx buffer layout.
 *
 * Configurable from Kconfig or bootargs. Zero by default, it's needed
 * on particular forwarding scenarios that add extra headers to the
 * forwarded frame.
 */
int fsl_fm_rx_extra_headroom = CONFIG_FSL_FM_RX_EXTRA_HEADROOM;

#ifdef CONFIG_FMAN_PFC
static int fsl_fm_pfc_quanta[] = {
		CONFIG_FMAN_PFC_QUANTA_0,
		CONFIG_FMAN_PFC_QUANTA_1,
		CONFIG_FMAN_PFC_QUANTA_2,
		CONFIG_FMAN_PFC_QUANTA_3
};
#endif

static t_LnxWrpFm   lnxWrpFm;

int fm_get_max_frm()
{
	return fsl_fm_max_frm;
}

int fm_get_rx_extra_headroom()
{
	return ALIGN(fsl_fm_rx_extra_headroom, 16);
}

static int __init fm_set_max_frm(char *str)
{
	int ret = 0;

	ret = get_option(&str, &fsl_fm_max_frm);
	if (ret != 1) {
		/*
		 * This will only work if CONFIG_EARLY_PRINTK is compiled in,
		 * and something like "earlyprintk=serial,uart0,115200" is
		 * specified in the bootargs
		 */
		printk(KERN_WARNING "No suitable %s=<int> prop in bootargs; "
			"will use the default FSL_FM_MAX_FRAME_SIZE (%d) "
			"from Kconfig.\n", FSL_FM_MAX_FRM_BOOTARG,
			CONFIG_FSL_FM_MAX_FRAME_SIZE);

		fsl_fm_max_frm = CONFIG_FSL_FM_MAX_FRAME_SIZE;
		return 1;
	}

	/* Don't allow invalid bootargs; fallback to the Kconfig value */
	if (fsl_fm_max_frm < 64 || fsl_fm_max_frm > 9600) {
		printk(KERN_WARNING "Invalid %s=%d in bootargs, valid range is "
			"64-9600. Falling back to the FSL_FM_MAX_FRAME_SIZE (%d) "
			"from Kconfig.\n",
			FSL_FM_MAX_FRM_BOOTARG, fsl_fm_max_frm,
			CONFIG_FSL_FM_MAX_FRAME_SIZE);

		fsl_fm_max_frm = CONFIG_FSL_FM_MAX_FRAME_SIZE;
		return 1;
	}

	printk(KERN_INFO "Using fsl_fm_max_frm=%d from bootargs\n",
		fsl_fm_max_frm);
	return 0;
}
early_param(FSL_FM_MAX_FRM_BOOTARG, fm_set_max_frm);

static int __init fm_set_rx_extra_headroom(char *str)
{
	int ret;

	ret = get_option(&str, &fsl_fm_rx_extra_headroom);

	if (ret != 1) {
		printk(KERN_WARNING "No suitable %s=<int> prop in bootargs; "
			"will use the default FSL_FM_RX_EXTRA_HEADROOM (%d) "
			"from Kconfig.\n", FSL_FM_RX_EXTRA_HEADROOM_BOOTARG,
			CONFIG_FSL_FM_RX_EXTRA_HEADROOM);
		fsl_fm_rx_extra_headroom = CONFIG_FSL_FM_RX_EXTRA_HEADROOM;

		return 1;
	}

	if (fsl_fm_rx_extra_headroom < FSL_FM_RX_EXTRA_HEADROOM_MIN ||
		fsl_fm_rx_extra_headroom > FSL_FM_RX_EXTRA_HEADROOM_MAX) {
		printk(KERN_WARNING "Invalid value for %s=%d prop in "
			"bootargs; will use the default "
			"FSL_FM_RX_EXTRA_HEADROOM (%d) from Kconfig.\n",
			FSL_FM_RX_EXTRA_HEADROOM_BOOTARG,
			fsl_fm_rx_extra_headroom,
			CONFIG_FSL_FM_RX_EXTRA_HEADROOM);
		fsl_fm_rx_extra_headroom = CONFIG_FSL_FM_RX_EXTRA_HEADROOM;
	}

	printk(KERN_INFO "Using fsl_fm_rx_extra_headroom=%d from bootargs\n",
		fsl_fm_rx_extra_headroom);

	return 0;
}
early_param(FSL_FM_RX_EXTRA_HEADROOM_BOOTARG, fm_set_rx_extra_headroom);

static irqreturn_t fm_irq(int irq, void *_dev)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev *)_dev;
#ifdef CONFIG_PM_SLEEP
    t_Fm               *p_Fm = (t_Fm*)p_LnxWrpFmDev->h_Dev;
#endif
    if (!p_LnxWrpFmDev || !p_LnxWrpFmDev->h_Dev)
        return IRQ_NONE;

#ifdef CONFIG_PM_SLEEP
    if (fman_get_normal_pending(p_Fm->p_FmFpmRegs) & INTR_EN_WAKEUP)
    {
        pm_wakeup_event(p_LnxWrpFmDev->dev, 200);        
    }
#endif
    FM_EventIsr(p_LnxWrpFmDev->h_Dev);
    return IRQ_HANDLED;
}

static irqreturn_t fm_err_irq(int irq, void *_dev)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev *)_dev;

    if (!p_LnxWrpFmDev || !p_LnxWrpFmDev->h_Dev)
        return IRQ_NONE;

    if (FM_ErrorIsr(p_LnxWrpFmDev->h_Dev) == E_OK)
        return IRQ_HANDLED;

    return IRQ_NONE;
}

/* used to protect FMD/LLD from concurrent calls in functions fm_mutex_lock / fm_mutex_unlock */
static struct mutex   lnxwrp_mutex;

static t_LnxWrpFmDev * CreateFmDev(uint8_t  id)
{
    t_LnxWrpFmDev   *p_LnxWrpFmDev;
    int             j;

    p_LnxWrpFmDev = (t_LnxWrpFmDev *)XX_Malloc(sizeof(t_LnxWrpFmDev));
    if (!p_LnxWrpFmDev)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, NO_MSG);
        return NULL;
    }

    memset(p_LnxWrpFmDev, 0, sizeof(t_LnxWrpFmDev));
    p_LnxWrpFmDev->fmDevSettings.advConfig = (t_SysObjectAdvConfigEntry*)XX_Malloc(FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry));
    memset(p_LnxWrpFmDev->fmDevSettings.advConfig, 0, (FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry)));
    p_LnxWrpFmDev->fmPcdDevSettings.advConfig = (t_SysObjectAdvConfigEntry*)XX_Malloc(FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry));
    memset(p_LnxWrpFmDev->fmPcdDevSettings.advConfig, 0, (FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry)));
    p_LnxWrpFmDev->hcPort.settings.advConfig = (t_SysObjectAdvConfigEntry*)XX_Malloc(FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry));
    memset(p_LnxWrpFmDev->hcPort.settings.advConfig, 0, (FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry)));
    for (j=0; j<FM_MAX_NUM_OF_RX_PORTS; j++)
    {
        p_LnxWrpFmDev->rxPorts[j].settings.advConfig = (t_SysObjectAdvConfigEntry*)XX_Malloc(FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry));
        memset(p_LnxWrpFmDev->rxPorts[j].settings.advConfig, 0, (FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry)));
    }
    for (j=0; j<FM_MAX_NUM_OF_TX_PORTS; j++)
    {
        p_LnxWrpFmDev->txPorts[j].settings.advConfig = (t_SysObjectAdvConfigEntry*)XX_Malloc(FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry));
        memset(p_LnxWrpFmDev->txPorts[j].settings.advConfig, 0, (FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry)));
    }
    for (j=0; j<FM_MAX_NUM_OF_OH_PORTS-1; j++)
    {
        p_LnxWrpFmDev->opPorts[j].settings.advConfig = (t_SysObjectAdvConfigEntry*)XX_Malloc(FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry));
        memset(p_LnxWrpFmDev->opPorts[j].settings.advConfig, 0, (FM_MAX_NUM_OF_ADV_SETTINGS*sizeof(t_SysObjectAdvConfigEntry)));
    }

    return p_LnxWrpFmDev;
}

static void DestroyFmDev(t_LnxWrpFmDev *p_LnxWrpFmDev)
{
    int             j;

    for (j=0; j<FM_MAX_NUM_OF_OH_PORTS-1; j++)
        if (p_LnxWrpFmDev->opPorts[j].settings.advConfig)
            XX_Free(p_LnxWrpFmDev->opPorts[j].settings.advConfig);
    for (j=0; j<FM_MAX_NUM_OF_TX_PORTS; j++)
        if (p_LnxWrpFmDev->txPorts[j].settings.advConfig)
            XX_Free(p_LnxWrpFmDev->txPorts[j].settings.advConfig);
    for (j=0; j<FM_MAX_NUM_OF_RX_PORTS; j++)
        if (p_LnxWrpFmDev->rxPorts[j].settings.advConfig)
            XX_Free(p_LnxWrpFmDev->rxPorts[j].settings.advConfig);
    if (p_LnxWrpFmDev->hcPort.settings.advConfig)
        XX_Free(p_LnxWrpFmDev->hcPort.settings.advConfig);
    if (p_LnxWrpFmDev->fmPcdDevSettings.advConfig)
        XX_Free(p_LnxWrpFmDev->fmPcdDevSettings.advConfig);
    if (p_LnxWrpFmDev->fmDevSettings.advConfig)
        XX_Free(p_LnxWrpFmDev->fmDevSettings.advConfig);

    XX_Free(p_LnxWrpFmDev);
}

static t_Error FillRestFmInfo(t_LnxWrpFmDev *p_LnxWrpFmDev)
{
#define FM_BMI_PPIDS_OFFSET                 0x00080304
#define FM_DMA_PLR_OFFSET                   0x000c2060
#define FM_FPM_IP_REV_1_OFFSET              0x000c30c4
#define DMA_HIGH_LIODN_MASK                 0x0FFF0000
#define DMA_LOW_LIODN_MASK                  0x00000FFF
#define DMA_LIODN_SHIFT                     16

typedef _Packed struct {
    uint32_t    plr[32];
} _PackedType t_Plr;

typedef _Packed struct {
   volatile uint32_t   fmbm_ppid[63];
} _PackedType t_Ppids;

    t_Plr       *p_Plr;
    t_Ppids     *p_Ppids;
    int         i,j;
    uint32_t    fmRev;

    static const uint8_t     phys1GRxPortId[] = {0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf};
    static const uint8_t     phys10GRxPortId[] = {0x10,0x11};
#if (DPAA_VERSION >= 11)
    static const uint8_t     physOhPortId[] = {/* 0x1, */0x2,0x3,0x4,0x5,0x6,0x7};
#else
    static const uint8_t     physOhPortId[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7};
#endif
    static const uint8_t     phys1GTxPortId[] = {0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f};
    static const uint8_t     phys10GTxPortId[] = {0x30,0x31};

    fmRev = (uint32_t)(*((volatile uint32_t *)UINT_TO_PTR(p_LnxWrpFmDev->fmBaseAddr+FM_FPM_IP_REV_1_OFFSET)));
    fmRev &= 0xffff;

    p_Plr = (t_Plr *)UINT_TO_PTR(p_LnxWrpFmDev->fmBaseAddr+FM_DMA_PLR_OFFSET);
#ifdef MODULE
    for (i=0;i<FM_MAX_NUM_OF_PARTITIONS/2;i++)
        p_Plr->plr[i] = 0;
#endif /* MODULE */

    for (i=0; i<FM_MAX_NUM_OF_PARTITIONS; i++)
    {
        uint16_t liodnBase = (uint16_t)((i%2) ?
                       (p_Plr->plr[i/2] & DMA_LOW_LIODN_MASK) :
                       ((p_Plr->plr[i/2] & DMA_HIGH_LIODN_MASK) >> DMA_LIODN_SHIFT));
#ifdef FM_PARTITION_ARRAY
        /* TODO: this was .liodnPerPartition[i] = liodnBase; is the index meaning the same? */
        p_LnxWrpFmDev->fmDevSettings.param.liodnBasePerPort[i] = liodnBase;
#endif /* FM_PARTITION_ARRAY */

        if ((i >= phys1GRxPortId[0]) &&
             (i <= phys1GRxPortId[FM_MAX_NUM_OF_1G_RX_PORTS-1]))
        {
            for (j=0; j<ARRAY_SIZE(phys1GRxPortId); j++)
                if (phys1GRxPortId[j] == i)
                    break;
            ASSERT_COND(j<ARRAY_SIZE(phys1GRxPortId));
            p_LnxWrpFmDev->rxPorts[j].settings.param.liodnBase = liodnBase;
        }
        else if (FM_MAX_NUM_OF_10G_RX_PORTS &&
                 (i >= phys10GRxPortId[0]) &&
                 (i <= phys10GRxPortId[FM_MAX_NUM_OF_10G_RX_PORTS-1]))
        {
            for (j=0; j<ARRAY_SIZE(phys10GRxPortId); j++)
                if (phys10GRxPortId[j] == i)
                    break;
            ASSERT_COND(j<ARRAY_SIZE(phys10GRxPortId));
            p_LnxWrpFmDev->rxPorts[FM_MAX_NUM_OF_1G_RX_PORTS+j].settings.param.liodnBase = liodnBase;
        }
        else if ((i >= physOhPortId[0]) &&
                 (i <= physOhPortId[FM_MAX_NUM_OF_OH_PORTS-1]))
        {
            for (j=0; j<ARRAY_SIZE(physOhPortId); j++)
                if (physOhPortId[j] == i)
                    break;
            ASSERT_COND(j<ARRAY_SIZE(physOhPortId));
            if (j == 0)
                p_LnxWrpFmDev->hcPort.settings.param.liodnBase = liodnBase;
            else
                p_LnxWrpFmDev->opPorts[j - 1].settings.param.liodnBase = liodnBase;
        }
        else if ((i >= phys1GTxPortId[0]) &&
                  (i <= phys1GTxPortId[FM_MAX_NUM_OF_1G_TX_PORTS-1]))
        {
            for (j=0; j<ARRAY_SIZE(phys1GTxPortId); j++)
                if (phys1GTxPortId[j] == i)
                    break;
            ASSERT_COND(j<ARRAY_SIZE(phys1GTxPortId));
            p_LnxWrpFmDev->txPorts[j].settings.param.liodnBase = liodnBase;
        }
        else if (FM_MAX_NUM_OF_10G_TX_PORTS &&
                 (i >= phys10GTxPortId[0]) &&
                 (i <= phys10GTxPortId[FM_MAX_NUM_OF_10G_TX_PORTS-1]))
        {
            for (j=0; j<ARRAY_SIZE(phys10GTxPortId); j++)
                if (phys10GTxPortId[j] == i)
                    break;
            ASSERT_COND(j<ARRAY_SIZE(phys10GTxPortId));
            p_LnxWrpFmDev->txPorts[FM_MAX_NUM_OF_1G_TX_PORTS+j].settings.param.liodnBase = liodnBase;
        }
    }

    p_Ppids = (t_Ppids *)UINT_TO_PTR(p_LnxWrpFmDev->fmBaseAddr+FM_BMI_PPIDS_OFFSET);

    for (i=0; i<FM_MAX_NUM_OF_1G_RX_PORTS; i++)
        p_LnxWrpFmDev->rxPorts[i].settings.param.specificParams.rxParams.liodnOffset =
                p_Ppids->fmbm_ppid[phys1GRxPortId[i]-1];

    for (i=0; i<FM_MAX_NUM_OF_10G_RX_PORTS; i++)
            p_LnxWrpFmDev->rxPorts[FM_MAX_NUM_OF_1G_RX_PORTS+i].settings.param.specificParams.rxParams.liodnOffset =
                p_Ppids->fmbm_ppid[phys10GRxPortId[i]-1];

    return E_OK;
}

/**
 * FindFmanMicrocode - find the Fman microcode
 *
 * This function returns a pointer to the QE Firmware blob that holds
 * the Fman microcode.  We use the QE Firmware structure because Fman microcode
 * is similar to QE microcode, so there's no point in defining a new layout.
 *
 * Current versions of U-Boot embed the Fman firmware into the device tree,
 * so we check for that first.  Each Fman node in the device tree contains a
 * node or a pointer to node that holds the firmware.  Technically, we should
 * be fetching the firmware node for the current Fman, but we don't have that
 * information any more, so we assume that there is only one firmware node in
 * the device tree, and that all Fmen use the same firmware.
 */
static const struct qe_firmware *FindFmanMicrocode(void)
{
    static const struct qe_firmware *P4080_UCPatch;
    struct device_node *np;

    if (P4080_UCPatch)
	    return P4080_UCPatch;

    /* The firmware should be inside the device tree. */
    np = of_find_compatible_node(NULL, NULL, "fsl,fman-firmware");
    if (np) {
	    P4080_UCPatch = of_get_property(np, "fsl,firmware", NULL);
            of_node_put(np);
	    if (P4080_UCPatch)
		    return P4080_UCPatch;
	    else
		    REPORT_ERROR(WARNING, E_NOT_FOUND, ("firmware node is incomplete"));
    }

    /* Returning NULL here forces the reuse of the IRAM content */
    return NULL;
}
#define SVR_SECURITY_MASK    0x00080000
#define SVR_PERSONALITY_MASK 0x0000FF00
#define SVR_VER_IGNORE_MASK (SVR_SECURITY_MASK | SVR_PERSONALITY_MASK)
#define SVR_B4860_REV1_VALUE 0x86800010
#define SVR_B4860_REV2_VALUE 0x86800020


static t_LnxWrpFmDev * ReadFmDevTreeNode (struct platform_device *of_dev)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev;
    struct device_node  *fm_node, *dev_node;
    struct of_device_id name;
    struct resource     res;
    const uint32_t      *uint32_prop;
    int                 _errno=0, lenp;

    fm_node = of_node_get(of_dev->dev.of_node);

    uint32_prop = (uint32_t *)of_get_property(fm_node, "cell-index", &lenp);
    if (unlikely(uint32_prop == NULL)) {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_get_property(%s, cell-index) failed", fm_node->full_name));
        return NULL;
    }
    if (WARN_ON(lenp != sizeof(uint32_t)))
        return NULL;
    if (*uint32_prop > INTG_MAX_NUM_OF_FM) {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("fm id!"));
        return NULL;
    }
    p_LnxWrpFmDev = CreateFmDev(*uint32_prop);
    if (!p_LnxWrpFmDev) {
        REPORT_ERROR(MAJOR, E_NULL_POINTER, NO_MSG);
        return NULL;
    }
    p_LnxWrpFmDev->dev = &of_dev->dev;
    p_LnxWrpFmDev->id = *uint32_prop;

    /* Get the FM interrupt */
    p_LnxWrpFmDev->irq = of_irq_to_resource(fm_node, 0, NULL);
    if (unlikely(p_LnxWrpFmDev->irq == /*NO_IRQ*/0)) {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_irq_to_resource() = %d", NO_IRQ));
        return NULL;
    }

    /* Get the FM error interrupt */
    p_LnxWrpFmDev->err_irq = of_irq_to_resource(fm_node, 1, NULL);
    /* TODO - un-comment it once there will be err_irq in the DTS */
#if 0
    if (unlikely(p_LnxWrpFmDev->err_irq == /*NO_IRQ*/0)) {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_irq_to_resource() = %d", NO_IRQ));
        return NULL;
    }
#endif /* 0 */

    /* Get the FM address */
    _errno = of_address_to_resource(fm_node, 0, &res);
    if (unlikely(_errno < 0)) {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_address_to_resource() = %d", _errno));
        return NULL;
    }


    p_LnxWrpFmDev->fmBaseAddr = 0;
    p_LnxWrpFmDev->fmPhysBaseAddr = res.start;
    p_LnxWrpFmDev->fmMemSize = res.end + 1 - res.start;

    uint32_prop = (uint32_t *)of_get_property(fm_node, "clock-frequency", &lenp);
    if (unlikely(uint32_prop == NULL)) {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_get_property(%s, clock-frequency) failed", fm_node->full_name));
        return NULL;
    }
    if (WARN_ON(lenp != sizeof(uint32_t)))
        return NULL;
    p_LnxWrpFmDev->fmDevSettings.param.fmClkFreq = (*uint32_prop + 500000)/1000000; /* In MHz, rounded */

    /* Get the MURAM base address and size */
    memset(&name, 0, sizeof(struct of_device_id));
    if (WARN_ON(strlen("muram") >= sizeof(name.name)))
        return NULL;
    strcpy(name.name, "muram");
    if (WARN_ON(strlen("fsl,fman-muram") >= sizeof(name.compatible)))
        return NULL;
    strcpy(name.compatible, "fsl,fman-muram");
    for_each_child_of_node(fm_node, dev_node) {
        if (likely(of_match_node(&name, dev_node) != NULL)) {
            _errno = of_address_to_resource(dev_node, 0, &res);
            if (unlikely(_errno < 0)) {
                REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_address_to_resource() = %d", _errno));
                return NULL;
            }

            p_LnxWrpFmDev->fmMuramBaseAddr = 0;
            p_LnxWrpFmDev->fmMuramPhysBaseAddr = res.start;
            p_LnxWrpFmDev->fmMuramMemSize = res.end + 1 - res.start;
            {
               uint32_t svr;
                svr = mfspr(SPRN_SVR);

                if ((svr & ~SVR_VER_IGNORE_MASK) >= SVR_B4860_REV2_VALUE)
                    p_LnxWrpFmDev->fmMuramMemSize = 0x80000;
            }
        }
    }

    /* Get the RTC base address and size */
    memset(&name, 0, sizeof(struct of_device_id));
    if (WARN_ON(strlen("rtc") >= sizeof(name.name)))
        return NULL;
    strcpy(name.name, "rtc");
    if (WARN_ON(strlen("fsl,fman-rtc") >= sizeof(name.compatible)))
        return NULL;
    strcpy(name.compatible, "fsl,fman-rtc");
    for_each_child_of_node(fm_node, dev_node) {
        if (likely(of_match_node(&name, dev_node) != NULL)) {
            _errno = of_address_to_resource(dev_node, 0, &res);
            if (unlikely(_errno < 0)) {
                REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_address_to_resource() = %d", _errno));
                return NULL;
            }

            p_LnxWrpFmDev->fmRtcBaseAddr = 0;
            p_LnxWrpFmDev->fmRtcPhysBaseAddr = res.start;
            p_LnxWrpFmDev->fmRtcMemSize = res.end + 1 - res.start;
        }
    }

#if (DPAA_VERSION >= 11)
    /* Get the VSP base address */
    for_each_child_of_node(fm_node, dev_node) {
        if (of_device_is_compatible(dev_node, "fsl,fman-vsps")) {
            _errno = of_address_to_resource(dev_node, 0, &res);
            if (unlikely(_errno < 0)) {
                REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("of_address_to_resource() = %d", _errno));
                return NULL;
            }
            p_LnxWrpFmDev->fmVspBaseAddr = 0;
            p_LnxWrpFmDev->fmVspPhysBaseAddr = res.start;
            p_LnxWrpFmDev->fmVspMemSize = res.end + 1 - res.start;
        }
    }
#endif

    /* Get all PCD nodes */
    memset(&name, 0, sizeof(struct of_device_id));
    if (WARN_ON(strlen("parser") >= sizeof(name.name)))
        return NULL;
    strcpy(name.name, "parser");
    if (WARN_ON(strlen("fsl,fman-parser") >= sizeof(name.compatible)))
        return NULL;
    strcpy(name.compatible, "fsl,fman-parser");
    for_each_child_of_node(fm_node, dev_node)
        if (likely(of_match_node(&name, dev_node) != NULL))
            p_LnxWrpFmDev->prsActive = TRUE;

    memset(&name, 0, sizeof(struct of_device_id));
    if (WARN_ON(strlen("keygen") >= sizeof(name.name)))
        return NULL;
    strcpy(name.name, "keygen");
    if (WARN_ON(strlen("fsl,fman-keygen") >= sizeof(name.compatible)))
        return NULL;
    strcpy(name.compatible, "fsl,fman-keygen");
    for_each_child_of_node(fm_node, dev_node)
        if (likely(of_match_node(&name, dev_node) != NULL))
            p_LnxWrpFmDev->kgActive = TRUE;

    memset(&name, 0, sizeof(struct of_device_id));
    if (WARN_ON(strlen("cc") >= sizeof(name.name)))
        return NULL;
    strcpy(name.name, "cc");
    if (WARN_ON(strlen("fsl,fman-cc") >= sizeof(name.compatible)))
        return NULL;
    strcpy(name.compatible, "fsl,fman-cc");
    for_each_child_of_node(fm_node, dev_node)
        if (likely(of_match_node(&name, dev_node) != NULL))
            p_LnxWrpFmDev->ccActive = TRUE;

    memset(&name, 0, sizeof(struct of_device_id));
    if (WARN_ON(strlen("policer") >= sizeof(name.name)))
        return NULL;
    strcpy(name.name, "policer");
    if (WARN_ON(strlen("fsl,fman-policer") >= sizeof(name.compatible)))
        return NULL;
    strcpy(name.compatible, "fsl,fman-policer");
    for_each_child_of_node(fm_node, dev_node)
        if (likely(of_match_node(&name, dev_node) != NULL))
            p_LnxWrpFmDev->plcrActive = TRUE;

    if (p_LnxWrpFmDev->prsActive || p_LnxWrpFmDev->kgActive ||
        p_LnxWrpFmDev->ccActive || p_LnxWrpFmDev->plcrActive)
        p_LnxWrpFmDev->pcdActive = TRUE;

    if (p_LnxWrpFmDev->pcdActive)
    {
        const char *str_prop = (char *)of_get_property(fm_node, "fsl,default-pcd", &lenp);
        if (str_prop) {
            if (strncmp(str_prop, "3-tuple", strlen("3-tuple")) == 0)
                p_LnxWrpFmDev->defPcd = e_FM_PCD_3_TUPLE;
        }
        else
            p_LnxWrpFmDev->defPcd = e_NO_PCD;
    }

    of_node_put(fm_node);

    p_LnxWrpFmDev->hcCh =
        qman_affine_channel(cpumask_first(qman_affine_cpus()));

    p_LnxWrpFmDev->active = TRUE;

    return p_LnxWrpFmDev;
}

struct device_node *GetFmAdvArgsDevTreeNode (uint8_t fmIndx)
{
    struct device_node  *dev_node;
    const uint32_t      *uint32_prop;
    int                 lenp;

    for_each_compatible_node(dev_node, NULL, "fsl,fman-extended-args") {
        uint32_prop = (uint32_t *)of_get_property(dev_node, "cell-index", &lenp);
        if (unlikely(uint32_prop == NULL)) {
            REPORT_ERROR(MAJOR, E_INVALID_VALUE,
                         ("of_get_property(%s, cell-index) failed",
                          dev_node->full_name));
            return NULL;
        }
        if (WARN_ON(lenp != sizeof(uint32_t)))
            return NULL;
        if (*uint32_prop > INTG_MAX_NUM_OF_FM) {
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("fm id!"));
            return NULL;
        }
        if (fmIndx == *uint32_prop)
            return dev_node;
    }

    return NULL;
}

static t_Error CheckNConfigFmAdvArgs (t_LnxWrpFmDev *p_LnxWrpFmDev)
{
    struct device_node  *dev_node;
    t_Error             err = E_INVALID_VALUE;
    const uint32_t      *uint32_prop;
    const char          *str_prop;
    int                 lenp;

    dev_node = GetFmAdvArgsDevTreeNode(p_LnxWrpFmDev->id);
    if (!dev_node) /* no advance parameters for FMan */
        return E_OK;

    str_prop = (char *)of_get_property(dev_node, "dma-aid-mode", &lenp);
    if (str_prop) {
        if (strcmp(str_prop, "port") == 0)
            err = FM_ConfigDmaAidMode(p_LnxWrpFmDev->h_Dev, e_FM_DMA_AID_OUT_PORT_ID);
        else if (strcmp(str_prop, "tnum") == 0)
            err = FM_ConfigDmaAidMode(p_LnxWrpFmDev->h_Dev, e_FM_DMA_AID_OUT_TNUM);

        if (err != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
    }

	uint32_prop = (uint32_t *)of_get_property(dev_node,
						"total-fifo-size", &lenp);
	if (uint32_prop) {
		if (WARN_ON(lenp != sizeof(uint32_t)))
			RETURN_ERROR(MINOR, E_INVALID_VALUE, NO_MSG);

		if (FM_ConfigTotalFifoSize(p_LnxWrpFmDev->h_Dev,
				*uint32_prop) != E_OK)
			RETURN_ERROR(MINOR, E_INVALID_VALUE, NO_MSG);
	}

    uint32_prop = (uint32_t *)of_get_property(dev_node, "tnum-aging-period",
	&lenp);
    if (uint32_prop) {
    	if (WARN_ON(lenp != sizeof(uint32_t)))
            RETURN_ERROR(MINOR, E_INVALID_VALUE, NO_MSG);
 
        err = FM_ConfigTnumAgingPeriod(p_LnxWrpFmDev->h_Dev,
            (uint16_t)uint32_prop[0]/*tnumAgingPeriod*/);

        if (err != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
    }

    of_node_put(dev_node);

    return E_OK;
}

static void LnxwrpFmDevExceptionsCb(t_Handle h_App, e_FmExceptions exception)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev *)h_App;

    ASSERT_COND(p_LnxWrpFmDev);

    DBG(INFO, ("got fm exception %d", exception));

    /* do nothing */
    UNUSED(exception);
}

static void LnxwrpFmDevBusErrorCb(t_Handle        h_App,
                                  e_FmPortType    portType,
                                  uint8_t         portId,
                                  uint64_t        addr,
                                  uint8_t         tnum,
                                  uint16_t        liodn)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev *)h_App;

    ASSERT_COND(p_LnxWrpFmDev);

    /* do nothing */
    UNUSED(portType);UNUSED(portId);UNUSED(addr);UNUSED(tnum);UNUSED(liodn);
}

static t_Error ConfigureFmDev(t_LnxWrpFmDev  *p_LnxWrpFmDev)
{
    struct resource     *dev_res;
    int                 _errno;

    if (!p_LnxWrpFmDev->active)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM not configured!!!"));

#ifndef MODULE
    _errno = can_request_irq(p_LnxWrpFmDev->irq, 0);
    if (unlikely(_errno < 0))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("can_request_irq() = %d", _errno));
#endif
    _errno = devm_request_irq(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->irq, fm_irq, IRQF_NO_SUSPEND, "fman", p_LnxWrpFmDev);
    if (unlikely(_errno < 0))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("request_irq(%d) = %d", p_LnxWrpFmDev->irq, _errno));

    if (p_LnxWrpFmDev->err_irq != 0) {
#ifndef MODULE
        _errno = can_request_irq(p_LnxWrpFmDev->err_irq, 0);
        if (unlikely(_errno < 0))
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("can_request_irq() = %d", _errno));
#endif
        _errno = devm_request_irq(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->err_irq, fm_err_irq, IRQF_SHARED | IRQF_NO_SUSPEND, "fman-err", p_LnxWrpFmDev);
        if (unlikely(_errno < 0))
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("request_irq(%d) = %d", p_LnxWrpFmDev->err_irq, _errno));
    }

    p_LnxWrpFmDev->res = devm_request_mem_region(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->fmPhysBaseAddr, p_LnxWrpFmDev->fmMemSize, "fman");
    if (unlikely(p_LnxWrpFmDev->res == NULL))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("request_mem_region() failed"));

    p_LnxWrpFmDev->fmBaseAddr = PTR_TO_UINT(devm_ioremap(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->fmPhysBaseAddr, p_LnxWrpFmDev->fmMemSize));
    if (unlikely(p_LnxWrpFmDev->fmBaseAddr == 0))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("devm_ioremap() failed"));

    if (SYS_RegisterIoMap((uint64_t)p_LnxWrpFmDev->fmBaseAddr, (uint64_t)p_LnxWrpFmDev->fmPhysBaseAddr, p_LnxWrpFmDev->fmMemSize) != E_OK)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM memory map"));

    dev_res = __devm_request_region(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->res, p_LnxWrpFmDev->fmMuramPhysBaseAddr, p_LnxWrpFmDev->fmMuramMemSize, "fman-muram");
    if (unlikely(dev_res == NULL))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("__devm_request_region() failed"));

    p_LnxWrpFmDev->fmMuramBaseAddr = PTR_TO_UINT(devm_ioremap(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->fmMuramPhysBaseAddr, p_LnxWrpFmDev->fmMuramMemSize));
    if (unlikely(p_LnxWrpFmDev->fmMuramBaseAddr == 0))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("devm_ioremap() failed"));

    if (SYS_RegisterIoMap((uint64_t)p_LnxWrpFmDev->fmMuramBaseAddr, (uint64_t)p_LnxWrpFmDev->fmMuramPhysBaseAddr, p_LnxWrpFmDev->fmMuramMemSize) != E_OK)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM MURAM memory map"));

    if (p_LnxWrpFmDev->fmRtcPhysBaseAddr)
    {
        dev_res = __devm_request_region(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->res, p_LnxWrpFmDev->fmRtcPhysBaseAddr, p_LnxWrpFmDev->fmRtcMemSize, "fman-rtc");
        if (unlikely(dev_res == NULL))
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("__devm_request_region() failed"));

        p_LnxWrpFmDev->fmRtcBaseAddr = PTR_TO_UINT(devm_ioremap(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->fmRtcPhysBaseAddr, p_LnxWrpFmDev->fmRtcMemSize));
        if (unlikely(p_LnxWrpFmDev->fmRtcBaseAddr == 0))
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("devm_ioremap() failed"));

        if (SYS_RegisterIoMap((uint64_t)p_LnxWrpFmDev->fmRtcBaseAddr, (uint64_t)p_LnxWrpFmDev->fmRtcPhysBaseAddr, p_LnxWrpFmDev->fmRtcMemSize) != E_OK)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM-RTC memory map"));
    }

#if (DPAA_VERSION >= 11)
    if (p_LnxWrpFmDev->fmVspPhysBaseAddr) {
        dev_res = __devm_request_region(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->res, p_LnxWrpFmDev->fmVspPhysBaseAddr, p_LnxWrpFmDev->fmVspMemSize, "fman-vsp");
        if (unlikely(dev_res == NULL))
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("__devm_request_region() failed"));

        p_LnxWrpFmDev->fmVspBaseAddr = PTR_TO_UINT(devm_ioremap(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->fmVspPhysBaseAddr, p_LnxWrpFmDev->fmVspMemSize));
        if (unlikely(p_LnxWrpFmDev->fmVspBaseAddr == 0))
	    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("devm_ioremap() failed"));
    }
#endif

    p_LnxWrpFmDev->fmDevSettings.param.baseAddr     = p_LnxWrpFmDev->fmBaseAddr;
    p_LnxWrpFmDev->fmDevSettings.param.fmId         = p_LnxWrpFmDev->id;
    p_LnxWrpFmDev->fmDevSettings.param.irq          = NO_IRQ;
    p_LnxWrpFmDev->fmDevSettings.param.errIrq       = NO_IRQ;
    p_LnxWrpFmDev->fmDevSettings.param.f_Exception  = LnxwrpFmDevExceptionsCb;
    p_LnxWrpFmDev->fmDevSettings.param.f_BusError   = LnxwrpFmDevBusErrorCb;
    p_LnxWrpFmDev->fmDevSettings.param.h_App        = p_LnxWrpFmDev;

    return FillRestFmInfo(p_LnxWrpFmDev);
}

static t_Error InitFmDev(t_LnxWrpFmDev  *p_LnxWrpFmDev)
{
    const struct qe_firmware *fw;

    if (!p_LnxWrpFmDev->active)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM not configured!!!"));

    if ((p_LnxWrpFmDev->h_MuramDev = FM_MURAM_ConfigAndInit(p_LnxWrpFmDev->fmMuramBaseAddr, p_LnxWrpFmDev->fmMuramMemSize)) == NULL)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("FM-MURAM!"));

    /* Loading the fman-controller code */
    fw = FindFmanMicrocode();

    if (!fw) {
        /* this forces the reuse of the current IRAM content */
        p_LnxWrpFmDev->fmDevSettings.param.firmware.size = 0;
        p_LnxWrpFmDev->fmDevSettings.param.firmware.p_Code = NULL;
    } else {
        p_LnxWrpFmDev->fmDevSettings.param.firmware.p_Code =
            (void *) fw + fw->microcode[0].code_offset;
        p_LnxWrpFmDev->fmDevSettings.param.firmware.size =
            sizeof(u32) * fw->microcode[0].count;
        DBG(INFO, ("Loading fman-controller code version %d.%d.%d",
                   fw->microcode[0].major,
                   fw->microcode[0].minor,
                   fw->microcode[0].revision));
    }

    p_LnxWrpFmDev->fmDevSettings.param.h_FmMuram = p_LnxWrpFmDev->h_MuramDev;

#if (DPAA_VERSION >= 11)
    if (p_LnxWrpFmDev->fmVspBaseAddr) {
        p_LnxWrpFmDev->fmDevSettings.param.vspBaseAddr = p_LnxWrpFmDev->fmVspBaseAddr;
        p_LnxWrpFmDev->fmDevSettings.param.partVSPBase = 0;
        p_LnxWrpFmDev->fmDevSettings.param.partNumOfVSPs = FM_VSP_MAX_NUM_OF_ENTRIES;
    }
#endif

    if ((p_LnxWrpFmDev->h_Dev = FM_Config(&p_LnxWrpFmDev->fmDevSettings.param)) == NULL)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("FM"));


    if (FM_ConfigResetOnInit(p_LnxWrpFmDev->h_Dev, TRUE) != E_OK)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM"));

#ifdef CONFIG_FMAN_P1023
    if (FM_ConfigDmaAidOverride(p_LnxWrpFmDev->h_Dev, TRUE) != E_OK)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM"));
#endif


    CheckNConfigFmAdvArgs(p_LnxWrpFmDev);

    if (FM_Init(p_LnxWrpFmDev->h_Dev) != E_OK)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM"));

    /* TODO: Why we mask these interrupts? */
    if (p_LnxWrpFmDev->err_irq == 0) {
        FM_SetException(p_LnxWrpFmDev->h_Dev, e_FM_EX_DMA_BUS_ERROR,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_DMA_READ_ECC,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_DMA_SYSTEM_WRITE_ECC,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_DMA_FM_WRITE_ECC,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_DMA_SINGLE_PORT_ECC, FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_FPM_STALL_ON_TASKS , FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_FPM_SINGLE_ECC, FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_FPM_DOUBLE_ECC,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_QMI_SINGLE_ECC, FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_QMI_DOUBLE_ECC,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_BMI_LIST_RAM_ECC,FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_BMI_STORAGE_PROFILE_ECC, FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_BMI_STATISTICS_RAM_ECC, FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_BMI_DISPATCH_RAM_ECC, FALSE);
        FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_IRAM_ECC,FALSE);
        /* TODO: FmDisableRamsEcc assert for ramsEccOwners.
         * FM_SetException(p_LnxWrpFmDev->h_Dev,e_FM_EX_MURAM_ECC,FALSE);*/
    }

    if (p_LnxWrpFmDev->fmRtcBaseAddr)
    {
        t_FmRtcParams   fmRtcParam;

        memset(&fmRtcParam, 0, sizeof(fmRtcParam));
        fmRtcParam.h_App = p_LnxWrpFmDev;
        fmRtcParam.h_Fm = p_LnxWrpFmDev->h_Dev;
        fmRtcParam.baseAddress = p_LnxWrpFmDev->fmRtcBaseAddr;

        if(!(p_LnxWrpFmDev->h_RtcDev = FM_RTC_Config(&fmRtcParam)))
            RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("FM-RTC"));

	if (FM_RTC_ConfigPeriod(p_LnxWrpFmDev->h_RtcDev, DPA_PTP_NOMINAL_FREQ_PERIOD_NS) != E_OK)
	    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM-RTC"));

        if (FM_RTC_Init(p_LnxWrpFmDev->h_RtcDev) != E_OK)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM-RTC"));
    }

    return E_OK;
}

/* TODO: to be moved back here */
extern void FreeFmPcdDev(t_LnxWrpFmDev  *p_LnxWrpFmDev);

static void FreeFmDev(t_LnxWrpFmDev  *p_LnxWrpFmDev)
{
    if (!p_LnxWrpFmDev->active)
        return;

    FreeFmPcdDev(p_LnxWrpFmDev);

    if (p_LnxWrpFmDev->h_RtcDev)
	FM_RTC_Free(p_LnxWrpFmDev->h_RtcDev);

    if (p_LnxWrpFmDev->h_Dev)
        FM_Free(p_LnxWrpFmDev->h_Dev);

    if (p_LnxWrpFmDev->h_MuramDev)
        FM_MURAM_Free(p_LnxWrpFmDev->h_MuramDev);

    if (p_LnxWrpFmDev->fmRtcBaseAddr)
    {
        SYS_UnregisterIoMap(p_LnxWrpFmDev->fmRtcBaseAddr);
        devm_iounmap(p_LnxWrpFmDev->dev, UINT_TO_PTR(p_LnxWrpFmDev->fmRtcBaseAddr));
        __devm_release_region(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->res, p_LnxWrpFmDev->fmRtcPhysBaseAddr, p_LnxWrpFmDev->fmRtcMemSize);
    }
    SYS_UnregisterIoMap(p_LnxWrpFmDev->fmMuramBaseAddr);
    devm_iounmap(p_LnxWrpFmDev->dev, UINT_TO_PTR(p_LnxWrpFmDev->fmMuramBaseAddr));
    __devm_release_region(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->res, p_LnxWrpFmDev->fmMuramPhysBaseAddr, p_LnxWrpFmDev->fmMuramMemSize);
    SYS_UnregisterIoMap(p_LnxWrpFmDev->fmBaseAddr);
    devm_iounmap(p_LnxWrpFmDev->dev, UINT_TO_PTR(p_LnxWrpFmDev->fmBaseAddr));
    devm_release_mem_region(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->fmPhysBaseAddr, p_LnxWrpFmDev->fmMemSize);
    if (p_LnxWrpFmDev->err_irq != 0) {
        devm_free_irq(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->err_irq, p_LnxWrpFmDev);
    }

    devm_free_irq(p_LnxWrpFmDev->dev, p_LnxWrpFmDev->irq, p_LnxWrpFmDev);
}

/* FMan character device file operations */
extern struct file_operations fm_fops;

static int /*__devinit*/ fm_probe(struct platform_device *of_dev)
{
    t_LnxWrpFmDev   *p_LnxWrpFmDev;

    if ((p_LnxWrpFmDev = ReadFmDevTreeNode(of_dev)) == NULL)
        return -EIO;
    if (ConfigureFmDev(p_LnxWrpFmDev) != E_OK)
        return -EIO;
    if (InitFmDev(p_LnxWrpFmDev) != E_OK)
        return -EIO;

    /* IOCTL ABI checking */
    LnxWrpPCDIOCTLEnumChecking();
    LnxWrpPCDIOCTLTypeChecking();

    Sprint (p_LnxWrpFmDev->name, "%s%d", DEV_FM_NAME, p_LnxWrpFmDev->id);

    /* Register to the /dev for IOCTL API */
    /* Register dynamically a new major number for the character device: */
    if ((p_LnxWrpFmDev->major = register_chrdev(0, p_LnxWrpFmDev->name, &fm_fops)) <= 0) {
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Failed to allocate a major number for device \"%s\"", p_LnxWrpFmDev->name));
        return -EIO;
    }

    /* Creating classes for FM */
    DBG(TRACE ,("class_create fm_class"));
    p_LnxWrpFmDev->fm_class = class_create(THIS_MODULE, p_LnxWrpFmDev->name);
    if (IS_ERR(p_LnxWrpFmDev->fm_class)) {
        unregister_chrdev(p_LnxWrpFmDev->major, p_LnxWrpFmDev->name);
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("class_create error fm_class"));
        return -EIO;
    }

    device_create(p_LnxWrpFmDev->fm_class, NULL, MKDEV(p_LnxWrpFmDev->major, DEV_FM_MINOR_BASE), NULL,
                  "fm%d", p_LnxWrpFmDev->id);
    device_create(p_LnxWrpFmDev->fm_class, NULL, MKDEV(p_LnxWrpFmDev->major, DEV_FM_PCD_MINOR_BASE), NULL,
                  "fm%d-pcd", p_LnxWrpFmDev->id);
    dev_set_drvdata(p_LnxWrpFmDev->dev, p_LnxWrpFmDev);

   /* create sysfs entries for stats and regs */
    if ( fm_sysfs_create(p_LnxWrpFmDev->dev) !=0 )
    {
        FreeFmDev(p_LnxWrpFmDev);
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Unable to create sysfs entry - fm!!!"));
        return -EIO;
    }

#ifdef CONFIG_PM
    device_set_wakeup_capable(p_LnxWrpFmDev->dev, true);
#endif

    DBG(TRACE, ("FM%d probed", p_LnxWrpFmDev->id));

    return 0;
}

static int fm_remove(struct platform_device *of_dev)
{
    t_LnxWrpFmDev   *p_LnxWrpFmDev;
    struct device   *dev;

    dev = &of_dev->dev;
    p_LnxWrpFmDev = dev_get_drvdata(dev);

    fm_sysfs_destroy(dev);

    DBG(TRACE, ("destroy fm_class"));
    device_destroy(p_LnxWrpFmDev->fm_class, MKDEV(p_LnxWrpFmDev->major, DEV_FM_MINOR_BASE));
    device_destroy(p_LnxWrpFmDev->fm_class, MKDEV(p_LnxWrpFmDev->major, DEV_FM_PCD_MINOR_BASE));
    class_destroy(p_LnxWrpFmDev->fm_class);

    /* Destroy chardev */
    unregister_chrdev(p_LnxWrpFmDev->major, p_LnxWrpFmDev->name);

    FreeFmDev(p_LnxWrpFmDev);

    DestroyFmDev(p_LnxWrpFmDev);

    dev_set_drvdata(dev, NULL);

    return 0;
}

static const struct of_device_id fm_match[] = {
    {
        .compatible    = "fsl,fman"
    },
    {}
};
#ifndef MODULE
MODULE_DEVICE_TABLE(of, fm_match);
#endif /* !MODULE */

#ifdef CONFIG_PM

#define SCFG_FMCLKDPSLPCR_ADDR 0xFFE0FC00C
#define SCFG_FMCLKDPSLPCR_DS_VAL 0x48402000
#define SCFG_FMCLKDPSLPCR_NORMAL_VAL 0x00402000

struct device *g_fm_dev;

static int fm_soc_suspend(struct device *dev)
{
	int err = 0;
	uint32_t *fmclk;
	t_LnxWrpFmDev *p_LnxWrpFmDev = dev_get_drvdata(get_device(dev));
	g_fm_dev = dev;
	fmclk = ioremap(SCFG_FMCLKDPSLPCR_ADDR, 4);
	WRITE_UINT32(*fmclk, SCFG_FMCLKDPSLPCR_DS_VAL);
	if (p_LnxWrpFmDev->h_DsarRxPort)
	{
#ifdef CONFIG_FSL_QORIQ_PM
		device_set_wakeup_enable(p_LnxWrpFmDev->dev, 1);
		fsl_set_power_except(dev,1);
#endif
		err = FM_PORT_EnterDsarFinal(p_LnxWrpFmDev->h_DsarRxPort,
			p_LnxWrpFmDev->h_DsarTxPort);
	}
	return err;
}

static int fm_soc_resume(struct device *dev)
{
	t_LnxWrpFmDev *p_LnxWrpFmDev = dev_get_drvdata(get_device(dev));
	uint32_t *fmclk;
	fmclk = ioremap(SCFG_FMCLKDPSLPCR_ADDR, 4);
	WRITE_UINT32(*fmclk, SCFG_FMCLKDPSLPCR_NORMAL_VAL);
	if (p_LnxWrpFmDev->h_DsarRxPort)
	{
#ifdef CONFIG_FSL_QORIQ_PM
		fsl_set_power_except(dev,0);
		device_set_wakeup_enable(p_LnxWrpFmDev->dev, 0);
#endif
		FM_PORT_ExitDsar(p_LnxWrpFmDev->h_DsarRxPort,
			p_LnxWrpFmDev->h_DsarTxPort);
		p_LnxWrpFmDev->h_DsarRxPort = 0;
		p_LnxWrpFmDev->h_DsarTxPort = 0;
	}
	return 0;
}

static const struct dev_pm_ops fm_pm_ops = {
	.suspend = fm_soc_suspend,
	.resume = fm_soc_resume,
};

#define FM_PM_OPS (&fm_pm_ops)

#else /* CONFIG_PM */

#define FM_PM_OPS NULL

#endif /* CONFIG_PM */

static struct platform_driver fm_driver = {
    .driver = {
        .name           = "fsl-fman",
        .of_match_table    = fm_match,
        .owner          = THIS_MODULE,
	.pm		= FM_PM_OPS,
    },
    .probe          = fm_probe,
    .remove         = fm_remove
};

t_Handle LNXWRP_FM_Init(void)
{
    memset(&lnxWrpFm, 0, sizeof(lnxWrpFm));
    mutex_init(&lnxwrp_mutex);

    /* Register to the DTB for basic FM API */
    platform_driver_register(&fm_driver);

    return &lnxWrpFm;
}

t_Error LNXWRP_FM_Free(t_Handle h_LnxWrpFm)
{
    platform_driver_unregister(&fm_driver);
    mutex_destroy(&lnxwrp_mutex);

    return E_OK;
}


struct fm * fm_bind(struct device *fm_dev)
{
    return (struct fm *)(dev_get_drvdata(get_device(fm_dev)));
}
EXPORT_SYMBOL(fm_bind);

void fm_unbind(struct fm *fm)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev*)fm;

    put_device(p_LnxWrpFmDev->dev);
}
EXPORT_SYMBOL(fm_unbind);

struct resource * fm_get_mem_region(struct fm *fm)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev*)fm;

    return p_LnxWrpFmDev->res;
}
EXPORT_SYMBOL(fm_get_mem_region);

void * fm_get_handle(struct fm *fm)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev*)fm;

    return (void *)p_LnxWrpFmDev->h_Dev;
}
EXPORT_SYMBOL(fm_get_handle);

void * fm_get_rtc_handle(struct fm *fm)
{
    t_LnxWrpFmDev       *p_LnxWrpFmDev = (t_LnxWrpFmDev*)fm;

    return (void *)p_LnxWrpFmDev->h_RtcDev;
}
EXPORT_SYMBOL(fm_get_rtc_handle);

struct fm_port * fm_port_bind (struct device *fm_port_dev)
{
    return (struct fm_port *)(dev_get_drvdata(get_device(fm_port_dev)));
}
EXPORT_SYMBOL(fm_port_bind);

void fm_port_unbind(struct fm_port *port)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev*)port;

    put_device(p_LnxWrpFmPortDev->dev);
}
EXPORT_SYMBOL(fm_port_unbind);

void *fm_port_get_handle(const struct fm_port *port)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev*)port;

    return (void *)p_LnxWrpFmPortDev->h_Dev;
}
EXPORT_SYMBOL(fm_port_get_handle);

u64 *fm_port_get_buffer_time_stamp(const struct fm_port *port,
		const void *data)
{
	return FM_PORT_GetBufferTimeStamp(fm_port_get_handle(port),
					  (void *)data);
}
EXPORT_SYMBOL(fm_port_get_buffer_time_stamp);

void fm_port_get_base_addr(const struct fm_port *port, uint64_t *base_addr)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;

    *base_addr = p_LnxWrpFmPortDev->settings.param.baseAddr;
}
EXPORT_SYMBOL(fm_port_get_base_addr);

void fm_port_pcd_bind (struct fm_port *port, struct fm_port_pcd_param *params)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev*)port;

    p_LnxWrpFmPortDev->pcd_owner_params.cba = params->cba;
    p_LnxWrpFmPortDev->pcd_owner_params.cbf = params->cbf;
    p_LnxWrpFmPortDev->pcd_owner_params.dev = params->dev;
}
EXPORT_SYMBOL(fm_port_pcd_bind);

void fm_port_get_buff_layout_ext_params(struct fm_port *port, struct fm_port_params *params)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
    struct device_node  *fm_node, *port_node;
    const uint32_t       *uint32_prop;
    int                  lenp;

    params->data_align = 0;
    params->manip_extra_space = 0;

    fm_node = GetFmAdvArgsDevTreeNode(((t_LnxWrpFmDev *) p_LnxWrpFmPortDev->h_LnxWrpFmDev)->id);
    if (!fm_node) /* no advance parameters for FMan */
        return;

    port_node = GetFmPortAdvArgsDevTreeNode(fm_node,
                                            p_LnxWrpFmPortDev->settings.param.portType,
                                            p_LnxWrpFmPortDev->settings.param.portId);
    if (!port_node) /* no advance parameters for FMan-Port */
        return;

    uint32_prop = (uint32_t *)of_get_property(port_node, "buffer-layout", &lenp);
    if (uint32_prop) {
       if (WARN_ON(lenp != sizeof(uint32_t)*2))
            return;

        params->manip_extra_space = (uint8_t)uint32_prop[0];
        params->data_align        = (uint16_t)uint32_prop[1];
    }

    of_node_put(port_node);
    of_node_put(fm_node);
}
EXPORT_SYMBOL(fm_port_get_buff_layout_ext_params);

uint16_t fm_get_tx_port_channel(struct fm_port *port)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev*)port;

    return p_LnxWrpFmPortDev->txCh;
}
EXPORT_SYMBOL(fm_get_tx_port_channel);

int fm_port_enable (struct fm_port *port)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev*)port;
    t_Error err = FM_PORT_Enable(p_LnxWrpFmPortDev->h_Dev);

    return GET_ERROR_TYPE(err);
}
EXPORT_SYMBOL(fm_port_enable);

int fm_port_disable(struct fm_port *port)
{
    t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev*)port;
    t_Error err = FM_PORT_Disable(p_LnxWrpFmPortDev->h_Dev);

    return GET_ERROR_TYPE(err);
}
EXPORT_SYMBOL(fm_port_disable);

int fm_port_set_rate_limit(struct fm_port *port,
			uint16_t	max_burst_size,
			uint32_t	rate_limit)
{
	t_FmPortRateLimit param;
	t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
	int err = 0;

	param.maxBurstSize = max_burst_size;
	param.rateLimit = rate_limit;
	param.rateLimitDivider = 0;

	err = FM_PORT_SetRateLimit(p_LnxWrpFmPortDev->h_Dev, &param);
	return err;
}
EXPORT_SYMBOL(fm_port_set_rate_limit);

int fm_port_del_rate_limit(struct fm_port *port)
{
	t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;

	FM_PORT_DeleteRateLimit(p_LnxWrpFmPortDev->h_Dev);
	return 0;
}
EXPORT_SYMBOL(fm_port_del_rate_limit);

void FM_PORT_Dsar_DumpRegs(void);
int ar_showmem(struct file *file, const char __user *buffer,
		unsigned long count, void *data)
{
	FM_PORT_Dsar_DumpRegs();
	return 2;
}

struct auto_res_tables_sizes *fm_port_get_autores_maxsize(
	struct fm_port *port)
{
	t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
	return &p_LnxWrpFmPortDev->dsar_table_sizes;
}
EXPORT_SYMBOL(fm_port_get_autores_maxsize);

int fm_port_enter_autores_for_deepsleep(struct fm_port *port,
	struct auto_res_port_params *params)
{
	t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
	t_LnxWrpFmDev* p_LnxWrpFmDev = (t_LnxWrpFmDev*)p_LnxWrpFmPortDev->h_LnxWrpFmDev;
	p_LnxWrpFmDev->h_DsarRxPort = p_LnxWrpFmPortDev->h_Dev;
	p_LnxWrpFmDev->h_DsarTxPort = params->h_FmPortTx;

		/*Register other under /proc/autoresponse */
    	if (WARN_ON(sizeof(t_FmPortDsarParams) != sizeof(struct auto_res_port_params)))
            return -EFAULT;
	
	FM_PORT_EnterDsar(p_LnxWrpFmPortDev->h_Dev, (t_FmPortDsarParams*)params);
	return 0;
}
EXPORT_SYMBOL(fm_port_enter_autores_for_deepsleep);

void fm_port_exit_auto_res_for_deep_sleep(struct fm_port *port_rx,
	struct fm_port *port_tx)
{
}
EXPORT_SYMBOL(fm_port_exit_auto_res_for_deep_sleep);

int fm_port_get_autores_stats(struct fm_port *port,
	struct auto_res_port_stats *stats)
{
	t_LnxWrpFmPortDev *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
    	if (WARN_ON(sizeof(t_FmPortDsarStats) != sizeof(struct auto_res_port_stats)))
            return -EFAULT;
	return FM_PORT_GetDsarStats(p_LnxWrpFmPortDev->h_Dev, (t_FmPortDsarStats*)stats);
}
EXPORT_SYMBOL(fm_port_get_autores_stats);

int fm_port_suspend(struct fm_port *port)
{
	t_LnxWrpFmPortDev *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
	if (!FM_PORT_IsInDsar(p_LnxWrpFmPortDev->h_Dev))
		return FM_PORT_Disable(p_LnxWrpFmPortDev->h_Dev);
	else
		return 0;
}
EXPORT_SYMBOL(fm_port_suspend);

int fm_port_resume(struct fm_port *port)
{
	t_LnxWrpFmPortDev *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
	if (!FM_PORT_IsInDsar(p_LnxWrpFmPortDev->h_Dev))
		return FM_PORT_Enable(p_LnxWrpFmPortDev->h_Dev);
	else
		return 0;
}
EXPORT_SYMBOL(fm_port_resume);

bool fm_port_is_in_auto_res_mode(struct fm_port *port)
{
	return FM_PORT_IsInDsar(port);
}
EXPORT_SYMBOL(fm_port_is_in_auto_res_mode);

#ifdef CONFIG_FMAN_PFC
int fm_port_set_pfc_priorities_mapping_to_qman_wq(struct fm_port *port,
		uint8_t prio, uint8_t wq)
{
	t_LnxWrpFmPortDev   *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;
	int err;
	int _errno;

	err = FM_PORT_SetPfcPrioritiesMappingToQmanWQ(p_LnxWrpFmPortDev->h_Dev,
			prio, wq);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_PORT_SetPfcPrioritiesMappingToQmanWQ() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_port_set_pfc_priorities_mapping_to_qman_wq);
#endif

int fm_mac_set_exception(struct fm_mac_dev *fm_mac_dev,
		e_FmMacExceptions exception, bool enable)
{
	int err;
	int _errno;

	err = FM_MAC_SetException(fm_mac_dev, exception, enable);

	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_SetException() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_set_exception);

int fm_mac_free(struct fm_mac_dev *fm_mac_dev)
{
	int err;
	int _error;

	err = FM_MAC_Free(fm_mac_dev);
	_error = -GET_ERROR_TYPE(err);

	if (unlikely(_error < 0))
		pr_err("FM_MAC_Free() = 0x%08x\n", err);

	return _error;
}
EXPORT_SYMBOL(fm_mac_free);

struct fm_mac_dev *fm_mac_config(t_FmMacParams *params)
{
	struct fm_mac_dev *fm_mac_dev;

	fm_mac_dev = FM_MAC_Config(params);
	if (unlikely(fm_mac_dev == NULL))
		pr_err("FM_MAC_Config() failed\n");

	return fm_mac_dev;
}
EXPORT_SYMBOL(fm_mac_config);

int fm_mac_config_max_frame_length(struct fm_mac_dev *fm_mac_dev,
		int len)
{
	int err;
	int _errno;

	err = FM_MAC_ConfigMaxFrameLength(fm_mac_dev, len);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_ConfigMaxFrameLength() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_config_max_frame_length);

int fm_mac_config_pad_and_crc(struct fm_mac_dev *fm_mac_dev, bool enable)
{
	int err;
	int _errno;

	err = FM_MAC_ConfigPadAndCrc(fm_mac_dev, enable);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_ConfigPadAndCrc() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_config_pad_and_crc);

int fm_mac_config_half_duplex(struct fm_mac_dev *fm_mac_dev, bool enable)
{
	int err;
	int _errno;

	err = FM_MAC_ConfigHalfDuplex(fm_mac_dev, enable);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_ConfigHalfDuplex() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_config_half_duplex);

int fm_mac_config_reset_on_init(struct fm_mac_dev *fm_mac_dev, bool enable)
{
	int err;
	int _errno;

	err = FM_MAC_ConfigResetOnInit(fm_mac_dev, enable);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_ConfigResetOnInit() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_config_reset_on_init);

int fm_mac_init(struct fm_mac_dev *fm_mac_dev)
{
	int err;
	int _errno;

	err = FM_MAC_Init(fm_mac_dev);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_Init() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_init);

int fm_mac_get_version(struct fm_mac_dev *fm_mac_dev, uint32_t *version)
{
	int err;
	int _errno;

	err = FM_MAC_GetVesrion(fm_mac_dev, version);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_GetVesrion() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_get_version);

int fm_mac_enable(struct fm_mac_dev *fm_mac_dev)
{
	int	 _errno;
	t_Error	 err;

	err = FM_MAC_Enable(fm_mac_dev, e_COMM_MODE_RX_AND_TX);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_Enable() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_enable);

int fm_mac_disable(struct fm_mac_dev *fm_mac_dev)
{
	int	 _errno;
	t_Error	 err;

	err = FM_MAC_Disable(fm_mac_dev, e_COMM_MODE_RX_AND_TX);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_Disable() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_disable);

int fm_mac_set_promiscuous(struct fm_mac_dev *fm_mac_dev,
		bool enable)
{
	int	_errno;
	t_Error	err;

	err = FM_MAC_SetPromiscuous(fm_mac_dev, enable);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_SetPromiscuous() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_set_promiscuous);

int fm_mac_remove_hash_mac_addr(struct fm_mac_dev *fm_mac_dev,
		t_EnetAddr *mac_addr)
{
	int	_errno;
	t_Error	err;

	err = FM_MAC_RemoveHashMacAddr(fm_mac_dev, mac_addr);
	_errno = -GET_ERROR_TYPE(err);
	if (_errno < 0) {
		pr_err("FM_MAC_RemoveHashMacAddr() = 0x%08x\n", err);
		return _errno;
	}

	return 0;
}
EXPORT_SYMBOL(fm_mac_remove_hash_mac_addr);

int fm_mac_add_hash_mac_addr(struct fm_mac_dev *fm_mac_dev,
		t_EnetAddr *mac_addr)
{
	int	_errno;
	t_Error	err;

	err = FM_MAC_AddHashMacAddr(fm_mac_dev, mac_addr);
	_errno = -GET_ERROR_TYPE(err);
	if (_errno < 0) {
		pr_err("FM_MAC_AddHashMacAddr() = 0x%08x\n", err);
		return _errno;
	}

	return 0;
}
EXPORT_SYMBOL(fm_mac_add_hash_mac_addr);

int fm_mac_modify_mac_addr(struct fm_mac_dev *fm_mac_dev,
					 uint8_t *addr)
{
	int	_errno;
	t_Error err;

	err = FM_MAC_ModifyMacAddr(fm_mac_dev, (t_EnetAddr *)addr);
	_errno = -GET_ERROR_TYPE(err);
	if (_errno < 0)
		pr_err("FM_MAC_ModifyMacAddr() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_modify_mac_addr);

int fm_mac_adjust_link(struct fm_mac_dev *fm_mac_dev,
		bool link, int speed, bool duplex)
{
	int	 _errno;
	t_Error	 err;

	if (!link) {
#if (DPAA_VERSION < 11)
		FM_MAC_RestartAutoneg(fm_mac_dev);
#endif
		return 0;
	}

	err = FM_MAC_AdjustLink(fm_mac_dev, speed, duplex);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_AdjustLink() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_adjust_link);

int fm_mac_enable_1588_time_stamp(struct fm_mac_dev *fm_mac_dev)
{
	int			 _errno;
	t_Error			 err;

	err = FM_MAC_Enable1588TimeStamp(fm_mac_dev);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_Enable1588TimeStamp() = 0x%08x\n", err);
	return _errno;
}
EXPORT_SYMBOL(fm_mac_enable_1588_time_stamp);

int fm_mac_disable_1588_time_stamp(struct fm_mac_dev *fm_mac_dev)
{
	int			 _errno;
	t_Error			 err;

	err = FM_MAC_Disable1588TimeStamp(fm_mac_dev);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_MAC_Disable1588TimeStamp() = 0x%08x\n", err);
	return _errno;
}
EXPORT_SYMBOL(fm_mac_disable_1588_time_stamp);

int fm_mac_set_rx_pause_frames(
		struct fm_mac_dev *fm_mac_dev, bool en)
{
	int	_errno;
	t_Error err;

	/* if rx pause is enabled, do NOT ignore pause frames */
	err = FM_MAC_SetRxIgnorePauseFrames(fm_mac_dev, !en);

	_errno = -GET_ERROR_TYPE(err);
	if (_errno < 0)
		pr_err("FM_MAC_SetRxIgnorePauseFrames() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_set_rx_pause_frames);

#ifdef CONFIG_FMAN_PFC
int fm_mac_set_tx_pause_frames(struct fm_mac_dev *fm_mac_dev,
					     bool en)
{
	int	_errno, i;
	t_Error err;

	if (en)
		for (i = 0; i < CONFIG_FMAN_PFC_COS_COUNT; i++) {
			err = FM_MAC_SetTxPauseFrames(fm_mac_dev,
					i, fsl_fm_pfc_quanta[i],
					FSL_FM_PAUSE_THRESH_DEFAULT);
			_errno = -GET_ERROR_TYPE(err);
			if (_errno < 0) {
				pr_err("FM_MAC_SetTxPauseFrames() = 0x%08x\n", err);
				return _errno;
			}
		}
	else
		for (i = 0; i < CONFIG_FMAN_PFC_COS_COUNT; i++) {
			err = FM_MAC_SetTxPauseFrames(fm_mac_dev,
					i, FSL_FM_PAUSE_TIME_DISABLE,
					FSL_FM_PAUSE_THRESH_DEFAULT);
			_errno = -GET_ERROR_TYPE(err);
			if (_errno < 0) {
				pr_err("FM_MAC_SetTxPauseFrames() = 0x%08x\n", err);
				return _errno;
			}
		}

	return _errno;
}
#else
int fm_mac_set_tx_pause_frames(struct fm_mac_dev *fm_mac_dev,
					     bool en)
{
	int	_errno;
	t_Error err;

	if (en)
		err = FM_MAC_SetTxAutoPauseFrames(fm_mac_dev,
				FSL_FM_PAUSE_TIME_ENABLE);
	else
		err = FM_MAC_SetTxAutoPauseFrames(fm_mac_dev,
				FSL_FM_PAUSE_TIME_DISABLE);

	_errno = -GET_ERROR_TYPE(err);
	if (_errno < 0)
		pr_err("FM_MAC_SetTxAutoPauseFrames() = 0x%08x\n", err);

	return _errno;
}
#endif
EXPORT_SYMBOL(fm_mac_set_tx_pause_frames);

int fm_rtc_enable(struct fm *fm_dev)
{
	int			 _errno;
	t_Error			 err;

	err = FM_RTC_Enable(fm_get_rtc_handle(fm_dev), 0);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_Enable = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_enable);

int fm_rtc_disable(struct fm *fm_dev)
{
	int			 _errno;
	t_Error			 err;

	err = FM_RTC_Disable(fm_get_rtc_handle(fm_dev));
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_Disable = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_disable);

int fm_rtc_get_cnt(struct fm *fm_dev, uint64_t *ts)
{
	int _errno;
	t_Error	err;

	err = FM_RTC_GetCurrentTime(fm_get_rtc_handle(fm_dev), ts);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_GetCurrentTime = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_get_cnt);

int fm_rtc_set_cnt(struct fm *fm_dev, uint64_t ts)
{
	int _errno;
	t_Error	err;

	err = FM_RTC_SetCurrentTime(fm_get_rtc_handle(fm_dev), ts);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_SetCurrentTime = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_set_cnt);

int fm_rtc_get_drift(struct fm *fm_dev, uint32_t *drift)
{
	int _errno;
	t_Error	err;

	err = FM_RTC_GetFreqCompensation(fm_get_rtc_handle(fm_dev),
			drift);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_GetFreqCompensation = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_get_drift);

int fm_rtc_set_drift(struct fm *fm_dev, uint32_t drift)
{
	int _errno;
	t_Error	err;

	err = FM_RTC_SetFreqCompensation(fm_get_rtc_handle(fm_dev),
			drift);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_SetFreqCompensation = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_set_drift);

int fm_rtc_set_alarm(struct fm *fm_dev, uint32_t id,
		uint64_t time)
{
	t_FmRtcAlarmParams alarm;
	int _errno;
	t_Error	err;

	alarm.alarmId = id;
	alarm.alarmTime = time;
	alarm.f_AlarmCallback = NULL;
	err = FM_RTC_SetAlarm(fm_get_rtc_handle(fm_dev),
			&alarm);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_SetAlarm = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_set_alarm);

int fm_rtc_set_fiper(struct fm *fm_dev, uint32_t id,
		uint64_t fiper)
{
	t_FmRtcPeriodicPulseParams pp;
	int _errno;
	t_Error	err;

	pp.periodicPulseId = id;
	pp.periodicPulsePeriod = fiper;
	pp.f_PeriodicPulseCallback = NULL;
	err = FM_RTC_SetPeriodicPulse(fm_get_rtc_handle(fm_dev), &pp);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_SetPeriodicPulse = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_set_fiper);

#ifdef CONFIG_PTP_1588_CLOCK_DPAA
int fm_rtc_enable_interrupt(struct fm *fm_dev, uint32_t events)
{
	int _errno;
	t_Error	err;

	err = FM_RTC_EnableInterrupt(fm_get_rtc_handle(fm_dev),
			events);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_EnableInterrupt = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_enable_interrupt);

int fm_rtc_disable_interrupt(struct fm *fm_dev, uint32_t events)
{
	int _errno;
	t_Error	err;

	err = FM_RTC_DisableInterrupt(fm_get_rtc_handle(fm_dev),
			events);
	_errno = -GET_ERROR_TYPE(err);
	if (unlikely(_errno < 0))
		pr_err("FM_RTC_DisableInterrupt = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_rtc_disable_interrupt);
#endif

int fm_mac_set_wol(struct fm_port *port, struct fm_mac_dev *fm_mac_dev, bool en)
{
	int _errno;
	t_Error err;
	t_LnxWrpFmPortDev *p_LnxWrpFmPortDev = (t_LnxWrpFmPortDev *)port;

	/* Do not set WoL on AR ports */
	if (FM_PORT_IsInDsar(p_LnxWrpFmPortDev->h_Dev)) {
		printk(KERN_WARNING "Port is AutoResponse enabled! WoL will not be set on this port!\n");
		return 0;
	}

	err = FM_MAC_SetWakeOnLan(fm_mac_dev, en);

	_errno = -GET_ERROR_TYPE(err);
	if (_errno < 0)
		pr_err("FM_MAC_SetWakeOnLan() = 0x%08x\n", err);

	return _errno;
}
EXPORT_SYMBOL(fm_mac_set_wol);

void fm_mutex_lock(void)
{
    mutex_lock(&lnxwrp_mutex);
}
EXPORT_SYMBOL(fm_mutex_lock);

void fm_mutex_unlock(void)
{
    mutex_unlock(&lnxwrp_mutex);
}
EXPORT_SYMBOL(fm_mutex_unlock);

static t_Handle h_FmLnxWrp;

static int __init __cold fm_load (void)
{
    if ((h_FmLnxWrp = LNXWRP_FM_Init()) == NULL)
    {
        printk("Failed to init FM wrapper!\n");
        return -ENODEV;
    }

	printk(KERN_CRIT "Freescale FM module ("__DATE__ ":"__TIME__")," \
		" FMD API version %d.%d.%d\n",
		FMD_API_VERSION_MAJOR,
		FMD_API_VERSION_MINOR,
		FMD_API_VERSION_RESPIN);
    return 0;
}

static void __exit __cold fm_unload (void)
{
    if (h_FmLnxWrp)
        LNXWRP_FM_Free(h_FmLnxWrp);
}

module_init (fm_load);
module_exit (fm_unload);
