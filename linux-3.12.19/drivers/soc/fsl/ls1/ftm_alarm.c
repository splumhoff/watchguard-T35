/*
 * Freescale FlexTimer Module (FTM) Alarm driver.
 *
 * Copyright 2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#define FTM_SC			0x00
#define FTM_SC_CLK_SHIFT	3
#define FTM_SC_CLK_MASK		(0x3 << FTM_SC_CLK_SHIFT)
#define FTM_SC_CLK(c)		((c) << FTM_SC_CLK_SHIFT)
#define FTM_SC_PS_MASK		0x7
#define FTM_SC_TOIE		BIT(6)
#define FTM_SC_TOF		BIT(7)

#define FTM_SC_CLKS_FIXED_FREQ	0x02

#define FTM_CNT			0x04
#define FTM_MOD			0x08
#define FTM_CNTIN		0x4C

#define FIXED_FREQ_CLK		32000
#define MAX_FREQ_DIV		(1 << FTM_SC_PS_MASK)
#define MAX_COUNT_VAL		0xffff

static void __iomem *ftm1_base;
static u32 alarm_freq;
static bool big_endian;

static inline u32 ftm_readl(void __iomem *addr)
{
	if (big_endian)
		return ioread32be(addr);
	else
		return ioread32(addr);
}

static inline void ftm_writel(u32 val, void __iomem *addr)
{
	if (big_endian)
		iowrite32be(val, addr);
	else
		iowrite32(val, addr);
}

static inline void ftm_counter_enable(void __iomem *base)
{
	u32 val;

	/* select and enable counter clock source */
	val = ftm_readl(base + FTM_SC);
	val &= ~(FTM_SC_PS_MASK | FTM_SC_CLK_MASK);
	val |= (FTM_SC_PS_MASK | FTM_SC_CLK(FTM_SC_CLKS_FIXED_FREQ));
	ftm_writel(val, base + FTM_SC);
}

static inline void ftm_counter_disable(void __iomem *base)
{
	u32 val;

	/* disable counter clock source */
	val = ftm_readl(base + FTM_SC);
	val &= ~(FTM_SC_PS_MASK | FTM_SC_CLK_MASK);
	ftm_writel(val, base + FTM_SC);
}

static inline void ftm_irq_acknowledge(void __iomem *base)
{
	u32 val;

	val = ftm_readl(base + FTM_SC);
	val &= ~FTM_SC_TOF;
	ftm_writel(val, base + FTM_SC);
}

static inline void ftm_irq_enable(void __iomem *base)
{
	u32 val;

	val = ftm_readl(base + FTM_SC);
	val |= FTM_SC_TOIE;
	ftm_writel(val, base + FTM_SC);
}

static inline void ftm_irq_disable(void __iomem *base)
{
	u32 val;

	val = ftm_readl(base + FTM_SC);
	val &= ~FTM_SC_TOIE;
	ftm_writel(val, base + FTM_SC);
}

static inline void ftm_reset_counter(void __iomem *base)
{
	/*
	 * The CNT register contains the FTM counter value.
	 * Reset clears the CNT register. Writing any value to COUNT
	 * updates the counter with its initial value, CNTIN.
	 */
	ftm_writel(0x00, base + FTM_CNT);
}

static u32 time_to_cycle(unsigned long time)
{
	u32 cycle;

	cycle = time * alarm_freq;
	if (cycle > MAX_COUNT_VAL) {
		pr_err("Out of alarm range.\n");
		cycle = 0;
	}

	return cycle;
}

static u32 cycle_to_time(u32 cycle)
{
	return cycle / alarm_freq + 1;
}

static void ftm_clean_alarm(void)
{
	ftm_counter_disable(ftm1_base);

	ftm_writel(0x00, ftm1_base + FTM_CNTIN);
	ftm_writel(~0UL, ftm1_base + FTM_MOD);

	ftm_reset_counter(ftm1_base);
}

static int ftm_set_alarm(u64 cycle)
{
	ftm_irq_disable(ftm1_base);

	/*
	 * The counter increments until the value of MOD is reached,
	 * at which point the counter is reloaded with the value of CNTIN.
	 * The TOF (the overflow flag) bit is set when the FTM counter
	 * changes from MOD to CNTIN. So we should using the cycle - 1.
	 */
	ftm_writel(cycle - 1, ftm1_base + FTM_MOD);

	ftm_counter_enable(ftm1_base);

	ftm_irq_enable(ftm1_base);

	return 0;
}

static irqreturn_t ftm_alarm_interrupt(int irq, void *dev_id)
{
	ftm_irq_acknowledge(ftm1_base);
	ftm_irq_disable(ftm1_base);
	ftm_clean_alarm();

	return IRQ_HANDLED;
}

static ssize_t ftm_alarm_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	u32 count, val;
	count = ftm_readl(ftm1_base + FTM_MOD);
	val = ftm_readl(ftm1_base + FTM_CNT);
	val = (count & MAX_COUNT_VAL) - val;
	val = cycle_to_time(val);

	return sprintf(buf, "%u\n", val);
}

static ssize_t ftm_alarm_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	u32 cycle;
	unsigned long time;

	if (kstrtoul(buf, 0, &time))
		return -EINVAL;

	ftm_clean_alarm();

	cycle = time_to_cycle(time);
	if (!cycle)
		return -EINVAL;

	ftm_set_alarm(cycle);

	return count;
}

static struct device_attribute ftm_alarm_attributes = __ATTR(ftm_alarm, 0644,
			ftm_alarm_show, ftm_alarm_store);

static int ftm_alarm_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *r;
	int irq;
	int ret;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -ENODEV;

	ftm1_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(ftm1_base))
		return PTR_ERR(ftm1_base);

	irq = irq_of_parse_and_map(np, 0);
	if (irq <= 0) {
		pr_err("ftm: unable to get IRQ from DT, %d\n", irq);
		return -EINVAL;
	}

	big_endian = of_property_read_bool(np, "big-endian");

	ret = devm_request_irq(&pdev->dev, irq, ftm_alarm_interrupt,
			       IRQF_NO_SUSPEND, dev_name(&pdev->dev), NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	ret = device_create_file(&pdev->dev, &ftm_alarm_attributes);
	if (ret) {
		dev_err(&pdev->dev, "create sysfs fail.\n");
		return ret;
	}

	alarm_freq = (u32)FIXED_FREQ_CLK / (u32)MAX_FREQ_DIV;

	ftm_clean_alarm();

	return ret;
}

static const struct of_device_id ftm_alarm_match[] = {
	{ .compatible = "fsl,ftm-alarm", },
	{ },
};

static struct platform_driver ftm_alarm_driver = {
	.probe		= ftm_alarm_probe,
	.driver		= {
		.name	= "ftm-alarm",
		.owner	= THIS_MODULE,
		.of_match_table = ftm_alarm_match,
	},
};

static int __init ftm_alarm_init(void)
{
	return platform_driver_register(&ftm_alarm_driver);
}
device_initcall(ftm_alarm_init);
