/*
 * Support Power Management feature
 *
 * Copyright 2014 Freescale Semiconductor Inc.
 *
 * Author: Chenhui Zhao <chenhui.zhao@freescale.com>
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/of_platform.h>

#include <asm/fsl_pm.h>

#define FSL_SLEEP		0x1
#define FSL_DEEP_SLEEP		0x2

int (*fsl_enter_deepsleep)(void);

/* specify the sleep state of the present platform */
unsigned int sleep_pm_state;
/* supported sleep modes by the present platform */
static unsigned int sleep_modes;

/**
 * fsl_set_power_except - set which IP block is not powerdown when sleep,
 * such as MAC, USB, etc.
 *
 * @dev: a pointer to the struct device
 * @on: if 1, do not power down; if 0, power down.
 */
void fsl_set_power_except(struct device *dev, int on)
{
	u32 value[2];
	u32 pw_mask;
	const phandle *phandle_prop;
	struct device_node *mac_node;
	int ret;

	ret = of_property_read_u32_array(dev->of_node, "sleep", value, 2);
	if (ret) {
		/* search fman mac node */
		phandle_prop = of_get_property(dev->of_node, "fsl,fman-mac",
					       NULL);
		if (phandle_prop == NULL)
			goto err;

		mac_node = of_find_node_by_phandle(*phandle_prop);
		ret = of_property_read_u32_array(mac_node, "sleep", value, 2);
		of_node_put(mac_node);
		if (ret)
			goto err;
	}

	/* get the second value, it is a mask */
	pw_mask = value[1];
	qoriq_pm_ops->set_ip_power(on, pw_mask);

	return;

err:
	dev_err(dev, "Can not set wakeup sources\n");
	return;
}
EXPORT_SYMBOL_GPL(fsl_set_power_except);

void qoriq_set_wakeup_source(struct device *dev, void *enable)
{
	if (!device_may_wakeup(dev))
		return;

	fsl_set_power_except(dev, *((int *)enable));
}

static int qoriq_suspend_enter(suspend_state_t state)
{
	int ret = 0;
	int cpu;

	switch (state) {
	case PM_SUSPEND_STANDBY:

		if (cur_cpu_spec->cpu_flush_caches)
			cur_cpu_spec->cpu_flush_caches();

		ret = qoriq_pm_ops->plat_enter_state(sleep_pm_state);

		break;

	case PM_SUSPEND_MEM:

		cpu = smp_processor_id();
		qoriq_pm_ops->irq_mask(cpu);

		ret = fsl_enter_deepsleep();

		qoriq_pm_ops->irq_unmask(cpu);

		break;

	default:
		ret = -EINVAL;

	}

	return ret;
}

static int qoriq_suspend_valid(suspend_state_t state)
{
	set_pm_suspend_state(state);

	if (state == PM_SUSPEND_STANDBY && (sleep_modes & FSL_SLEEP))
		return 1;

	if (state == PM_SUSPEND_MEM && (sleep_modes & FSL_DEEP_SLEEP))
		return 1;

	set_pm_suspend_state(PM_SUSPEND_ON);
	return 0;
}

static int qoriq_suspend_begin(suspend_state_t state)
{
	const int enable = 1;

	dpm_for_each_dev((void *)&enable, qoriq_set_wakeup_source);

	if (state == PM_SUSPEND_MEM)
		return fsl_dp_iomap();

	return 0;
}

static void qoriq_suspend_end(void)
{
	const int enable = 0;

	dpm_for_each_dev((void *)&enable, qoriq_set_wakeup_source);

	set_pm_suspend_state(PM_SUSPEND_ON);
	fsl_dp_iounmap();
}

static const struct platform_suspend_ops qoriq_suspend_ops = {
	.valid = qoriq_suspend_valid,
	.enter = qoriq_suspend_enter,
	.begin = qoriq_suspend_begin,
	.end = qoriq_suspend_end,
};

static const struct of_device_id deepsleep_matches[] = {
	{
		.compatible = "fsl,t1040-rcpm",
	},
	{
		.compatible = "fsl,t1024-rcpm",
	},
	{},
};

static int __init qoriq_suspend_init(void)
{
	struct device_node *np;

	sleep_modes = FSL_SLEEP;
	sleep_pm_state = PLAT_PM_SLEEP;

	np = of_find_compatible_node(NULL, NULL, "fsl,qoriq-rcpm-2.0");
	if (np)
		sleep_pm_state = PLAT_PM_LPM20;

	np = of_find_matching_node_and_match(NULL, deepsleep_matches, NULL);
	if (np) {
		fsl_enter_deepsleep = fsl_enter_epu_deepsleep;
		sleep_modes |= FSL_DEEP_SLEEP;
	}

	suspend_set_ops(&qoriq_suspend_ops);
	set_pm_suspend_state(PM_SUSPEND_ON);

	return 0;
}
arch_initcall(qoriq_suspend_init);
