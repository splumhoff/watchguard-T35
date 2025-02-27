/*
 * CoreNet platform DIU operation
 *
 * Copyright 2014 Freescale Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <sysdev/fsl_soc.h>

/*DIU Pixel ClockCR offset in scfg*/
#define CCSR_SCFG_PIXCLKCR      0x28

/* DIU Pixel Clock bits of the PIXCLKCR */
#define PIXCLKCR_PXCKEN		0x80000000
#define PIXCLKCR_PXCKINV	0x40000000
#define PIXCLKCR_PXCKDLY	0x0000FF00
#define PIXCLKCR_PXCLK_MASK	0x00FF0000

/* Some CPLD register definitions */
#define CPLD_DIUCSR		0x16
#define CPLD_DIUCSR_DVIEN	0x80
#define CPLD_DIUCSR_BACKLIGHT	0x0f

/**
 * t1042rdb_set_monitor_port: switch the output to a different monitor port
 */
static void t1042rdb_set_monitor_port(enum fsl_diu_monitor_port port)
{
	struct device_node *cpld_node;
	static void __iomem *cpld_base;

	cpld_node = of_find_compatible_node(NULL, NULL, "fsl,t104xrdb-cpld");
	if (!cpld_node) {
		pr_err("%s: Missing CPLD node\n", __func__);
		return;
	}

	cpld_base = of_iomap(cpld_node, 0);
	if (!cpld_base) {
		pr_err("%s: Could not map cpld registers\n", __func__);
		goto exit;
	}

	switch (port) {
	case FSL_DIU_PORT_DVI:
		/* Enable the DVI(HDMI) port, disable the DFP and
		 * the backlight
		 */
		clrbits8(cpld_base + CPLD_DIUCSR, CPLD_DIUCSR_DVIEN);
		break;
	case FSL_DIU_PORT_LVDS:
		/*
		 * LVDS also needs backlight enabled, otherwise the display
		 * will be blank.
		 */
		/* Enable the DFP port, disable the DVI*/
		setbits8(cpld_base + CPLD_DIUCSR, 0x01 << 8);
		setbits8(cpld_base + CPLD_DIUCSR, 0x01 << 4);
		setbits8(cpld_base + CPLD_DIUCSR, CPLD_DIUCSR_BACKLIGHT);
		break;
	default:
		pr_err("%s: Unsupported monitor port %i\n", __func__, port);
	}

	iounmap(cpld_base);
exit:
	of_node_put(cpld_node);
}

/**
 * t102xqds_set_monitor_port: switch the output to a different monitor port
 */

#define BRDCFG5				0x55
#define BRDCFG15			0x5F
/* BRDCFG5[0:1] controls routing and use of I2C3 & I2C4 ports*/
#define BRDCFG5_IMX_DIU_HIGH		0x80
#define BRDCFG5_IMX_DIU_LOW		0x40

/* BRDCFG15[3] controls LCD Panel Powerdown*/
#define BRDCFG15_LCD_PD			0x10
#define BRDCFG15_LCD_ENABLED		0x80

/* BRDCFG15[6:7] controls DIU MUX selction*/
#define BRDCFG15_DIUSEL_HDMI		0x03
#define BRDCFG15_DIUSEL_LVDS		0x01

static void t102xqds_set_monitor_port(enum fsl_diu_monitor_port port)
{
	struct device_node *pixis_node;
	void __iomem *pixis;

	pixis_node = of_find_compatible_node(NULL, NULL, "fsl,tetra-fpga");
	if (!pixis_node) {
		pr_err("t102xqds: missing pixis node\n");
		goto exit;
	}

	pixis = of_iomap(pixis_node, 0);
	of_node_put(pixis_node);
	if (!pixis) {
		pr_err("t102xqds: could not map pixis registers\n");
		goto exit;
	}

	/* Route I2C4 to DIU system as HSYNC/VSYNC */
	clrbits8(pixis + BRDCFG5, BRDCFG5_IMX_DIU_LOW);
	setbits8(pixis + BRDCFG5, BRDCFG5_IMX_DIU_HIGH);

	switch (port) {
	case FSL_DIU_PORT_DVI:
		/* Enable the DVI(HDMI) port, disable the DFP and
		 * the backlight
		 */
		clrbits8(pixis + BRDCFG15, BRDCFG15_LCD_ENABLED);
		setbits8(pixis + BRDCFG15, BRDCFG15_LCD_PD);

		clrbits8(pixis + BRDCFG15, BRDCFG15_DIUSEL_HDMI);
		break;
	case FSL_DIU_PORT_LVDS:
		/*
		 * LVDS also needs backlight enabled, otherwise the display
		 * will be blank.
		 */
		/* Enable the DFP port, disable the DVI*/
		setbits8(pixis + BRDCFG15, BRDCFG15_DIUSEL_LVDS);

		clrbits8(pixis + BRDCFG15, BRDCFG15_LCD_PD);
		setbits8(pixis + BRDCFG15, BRDCFG15_LCD_ENABLED);
		break;
	default:
		pr_err("%s: Unsupported monitor port %i\n", __func__, port);
	}

	iounmap(pixis);

exit:
	return;
}

static const struct of_device_id scfg_matches[] = {
	{
		.compatible = "fsl,t1040-scfg",
	},
	{
		.compatible = "fsl,t1024-scfg",
	},
	{},
};


/**
 * t1042rdb_set_pixel_clock: program the DIU's clock
 *
 * @pixclock: the wavelength, in picoseconds, of the clock
 */
static void corenet_set_pixel_clock(unsigned int pixclock)
{
	struct device_node *scfg_np = NULL;
	void __iomem *scfg;
	unsigned long freq;
	u64 temp;
	u32 pxclk;

	/* Map the global utilities registers. */
	scfg_np = of_find_matching_node_and_match(NULL, scfg_matches, NULL);
	if (!scfg_np) {
		freq = temp;
		pr_err("%s: Missing scfg node. Can not display video.\n",
		       __func__);
		return;
	}

	scfg = of_iomap(scfg_np, 0);
	of_node_put(scfg_np);
	if (!scfg) {
		pr_err("%s: Could not map device. Can not display video.\n",
		       __func__);
		return;
	}

	/* Convert pixclock from a wavelength to a frequency */
	temp = 1000000000000ULL;
	do_div(temp, pixclock);
	freq = temp;

	/*
	 * 'pxclk' is the ratio of the platform clock to the pixel clock.
	 * This number is programmed into the PIXCLKCR register, and the valid
	 * range of values is 2-255.
	 */
	pxclk = DIV_ROUND_CLOSEST(fsl_get_sys_freq(), freq);
	pxclk = clamp_t(u32, pxclk, 2, 255);

	/* Disable the pixel clock, and set it to non-inverted and no delay */
	clrbits32(scfg + CCSR_SCFG_PIXCLKCR,
		  PIXCLKCR_PXCKEN | PIXCLKCR_PXCKDLY | PIXCLKCR_PXCLK_MASK);

	/* Enable the clock and set the pxclk */
	setbits32(scfg + CCSR_SCFG_PIXCLKCR, PIXCLKCR_PXCKEN | (pxclk << 16));

	iounmap(scfg);
}

/**
 * corenet_valid_monitor_port: set the monitor port for sysfs
 */
static enum fsl_diu_monitor_port
corenet_valid_monitor_port(enum fsl_diu_monitor_port port)
{
	switch (port) {
	case FSL_DIU_PORT_DVI:
	case FSL_DIU_PORT_LVDS:
		return port;
	default:
		return FSL_DIU_PORT_DVI; /* Dual-link LVDS is not supported */
	}
}

static void t1042rdb_diu_init(void)
{
	diu_ops.set_monitor_port	= t1042rdb_set_monitor_port;
	diu_ops.set_pixel_clock		= corenet_set_pixel_clock;
	diu_ops.valid_monitor_port	= corenet_valid_monitor_port;
}

static void t1024qds_diu_init(void)
{
	diu_ops.set_monitor_port        = t102xqds_set_monitor_port;
	diu_ops.set_pixel_clock         = corenet_set_pixel_clock;
	diu_ops.valid_monitor_port      = corenet_valid_monitor_port;
}

static int __init corenet_diu_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,diu");
	if (!np)
		return 0;

	/* T1042RDB_PI board */
	if (of_find_compatible_node(NULL, NULL, "fsl,T1042RDB_PI"))
		t1042rdb_diu_init();

	/* T1024QDS board */
	if (of_find_compatible_node(NULL, NULL, "fsl,T1024QDS"))
		t1024qds_diu_init();

	return 0;
}
early_initcall(corenet_diu_init);
