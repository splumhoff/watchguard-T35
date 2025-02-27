/*
 * drivers/spi/spi-fsl-dspi.c
 *
 * Copyright 2013 Freescale Semiconductor, Inc.
 *
 * Freescale DSPI driver
 * This file contains a driver for the Freescale DSPI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>

#define DRIVER_NAME "fsl-dspi"

#define TRAN_STATE_RX_VOID		0x01
#define TRAN_STATE_TX_VOID		0x02
#define TRAN_STATE_WORD_ODD_NUM	0x04

#define DSPI_FIFO_SIZE			4

#define SPI_MCR		0x00
#define SPI_MCR_MASTER		(1 << 31)
#define SPI_MCR_PCSIS		(0x3F << 16)
#define SPI_MCR_CLR_TXF	(1 << 11)
#define SPI_MCR_CLR_RXF	(1 << 10)

#define SPI_TCR			0x08

#define SPI_CTAR(x)		(0x0c + (x * 4))
#define SPI_CTAR_FMSZ(x)	(((x) & 0x0000000f) << 27)
#define SPI_CTAR_CPOL(x)	((x) << 26)
#define SPI_CTAR_CPHA(x)	((x) << 25)
#define SPI_CTAR_LSBFE(x)	((x) << 24)
#define SPI_CTAR_PCSSCR(x)	(((x) & 0x00000003) << 22)
#define SPI_CTAR_PASC(x)	(((x) & 0x00000003) << 20)
#define SPI_CTAR_PDT(x)	(((x) & 0x00000003) << 18)
#define SPI_CTAR_PBR(x)	(((x) & 0x00000003) << 16)
#define SPI_CTAR_CSSCK(x)	(((x) & 0x0000000f) << 12)
#define SPI_CTAR_ASC(x)	(((x) & 0x0000000f) << 8)
#define SPI_CTAR_DT(x)		(((x) & 0x0000000f) << 4)
#define SPI_CTAR_BR(x)		((x) & 0x0000000f)

#define SPI_CTAR0_SLAVE	0x0c

#define SPI_SR			0x2c
#define SPI_SR_TCFQF		0x80000000
#define SPI_SR_EOQF		0x10000000

#define SPI_RSER		0x30
#define SPI_RSER_TCFQE		0x80000000
#define SPI_RSER_EOQFE		0x10000000

#define SPI_PUSHR		0x34
#define SPI_PUSHR_CONT		(1 << 31)
#define SPI_PUSHR_CTAS(x)	(((x) & 0x00000007) << 28)
#define SPI_PUSHR_EOQ		(1 << 27)
#define SPI_PUSHR_CTCNT	(1 << 26)
#define SPI_PUSHR_PCS(x)	(((1 << x) & 0x0000003f) << 16)
#define SPI_PUSHR_TXDATA(x)	((x) & 0x0000ffff)

#define SPI_PUSHR_SLAVE	0x34

#define SPI_POPR		0x38
#define SPI_POPR_RXDATA(x)	((x) & 0x0000ffff)

#define SPI_TXFR0		0x3c
#define SPI_TXFR1		0x40
#define SPI_TXFR2		0x44
#define SPI_TXFR3		0x48
#define SPI_RXFR0		0x7c
#define SPI_RXFR1		0x80
#define SPI_RXFR2		0x84
#define SPI_RXFR3		0x88

#define SPI_FRAME_BITS(bits)	SPI_CTAR_FMSZ((bits) - 1)
#define SPI_FRAME_BITS_MASK	SPI_CTAR_FMSZ(0xf)
#define SPI_FRAME_BITS_16	SPI_CTAR_FMSZ(0xf)
#define SPI_FRAME_BITS_8	SPI_CTAR_FMSZ(0x7)

#define SPI_CS_INIT		0x01
#define SPI_CS_ASSERT		0x02
#define SPI_CS_DROP		0x04

struct chip_data {
	u32 mcr_val;
	u32 ctar_val;
	u16 void_write_data;
};

enum dspi_trans_mode {
	DSPI_EOQ_MODE = 0,
	DSPI_TCFQ_MODE,
	DSPI_DMA_MODE, /*TODO*/
};

struct fsl_dspi {
	struct spi_master	*master;
	struct platform_device	*pdev;

	struct regmap		*regmap;
	int			irq;
	struct clk		*clk;

	struct spi_transfer	*cur_transfer;
	struct spi_message	*cur_msg;
	struct chip_data	*cur_chip;
	size_t			len;
	void			*tx;
	void			*tx_end;
	void			*rx;
	void			*rx_end;
	char			dataflags;
	u8			cs;
	u16			void_write_data;
	u32			cs_change;
	enum dspi_trans_mode	trans_mode;

	wait_queue_head_t	waitq;
	u32			waitflags;
};

static inline int is_double_byte_mode(struct fsl_dspi *dspi)
{
	unsigned int val;

	regmap_read(dspi->regmap, SPI_CTAR(dspi->cs), &val);

	return ((val & SPI_FRAME_BITS_MASK) == SPI_FRAME_BITS(8)) ? 0 : 1;
}

static void hz_to_spi_baud(char *pbr, char *br, int speed_hz,
		unsigned long clkrate)
{
	/* Valid baud rate pre-scaler values */
	int pbr_tbl[4] = {2, 3, 5, 7};
	int brs[16] = {	2,	4,	6,	8,
		16,	32,	64,	128,
		256,	512,	1024,	2048,
		4096,	8192,	16384,	32768 };
	int temp, i = 0, j = 0;

	temp = clkrate / 2 / speed_hz;

	for (i = 0; i < ARRAY_SIZE(pbr_tbl); i++)
		for (j = 0; j < ARRAY_SIZE(brs); j++) {
			if (pbr_tbl[i] * brs[j] >= temp) {
				*pbr = i;
				*br = j;
				return;
			}
		}

	pr_warn("Can not find valid baud rate,speed_hz is %d,clkrate is %ld\
		,we use the max prescaler value.\n", speed_hz, clkrate);
	*pbr = ARRAY_SIZE(pbr_tbl) - 1;
	*br =  ARRAY_SIZE(brs) - 1;
}

static u32 dspi_data_to_pushr(struct fsl_dspi *dspi, int tx_word)
{
	u16 d16;
	u8 d8;

	if (tx_word) {
		if (!(dspi->dataflags & TRAN_STATE_TX_VOID)) {
			d16 = *(u16 *)dspi->tx;
			dspi->tx += 2;
		} else {
			d16 = dspi->void_write_data;
		}

		dspi->len -= 2;

		return	SPI_PUSHR_TXDATA(d16) |
			SPI_PUSHR_PCS(dspi->cs) |
			SPI_PUSHR_CTAS(dspi->cs) |
			SPI_PUSHR_CONT;
	} else {
		if (!(dspi->dataflags & TRAN_STATE_TX_VOID)) {

			d8 = *(u8 *)dspi->tx;
			dspi->tx++;
		} else {
			d8 = (u8)dspi->void_write_data;
		}

		dspi->len--;

		return SPI_PUSHR_TXDATA(d8) |
			SPI_PUSHR_PCS(dspi->cs) |
			SPI_PUSHR_CTAS(dspi->cs) |
			SPI_PUSHR_CONT;
	}
}

static void dspi_data_from_popr(struct fsl_dspi *dspi, int rx_word)
{
	u16 d;
	unsigned int val;

	if (rx_word) {
		regmap_read(dspi->regmap, SPI_POPR, &val);
		d = SPI_POPR_RXDATA(val);

		if (!(dspi->dataflags & TRAN_STATE_RX_VOID))
			*(u16 *)dspi->rx = d;
		dspi->rx += 2;
	} else {
		regmap_read(dspi->regmap, SPI_POPR, &val);
		d = SPI_POPR_RXDATA(val);
		if (!(dspi->dataflags & TRAN_STATE_RX_VOID))
			*(u8 *)dspi->rx = d;
		dspi->rx++;
	}
}

static int dspi_eoq_write(struct fsl_dspi *dspi)
{
	int tx_count = 0;
	int tx_word;
	u32 dspi_pushr = 0;
	int first = 1;

	tx_word = is_double_byte_mode(dspi);

	/* If we are in word mode, but only have a single byte to transfer
	 * then switch to byte mode temporarily.  Will switch back at the
	 * end of the transfer.
	 */
	if (tx_word && (dspi->len == 1)) {
		dspi->dataflags |= TRAN_STATE_WORD_ODD_NUM;
		regmap_update_bits(dspi->regmap, SPI_CTAR(dspi->cs),
				SPI_FRAME_BITS_MASK, SPI_FRAME_BITS(8));
		tx_word = 0;
	}

	while (dspi->len && (tx_count < DSPI_FIFO_SIZE)) {
		dspi_pushr = dspi_data_to_pushr(dspi, tx_word);

		if (dspi->len == 0 || tx_count == DSPI_FIFO_SIZE - 1) {
			/* last transfer in the transfer */
			dspi_pushr |= SPI_PUSHR_EOQ;
			if ((dspi->cs_change) && (!dspi->len))
				dspi_pushr &= ~SPI_PUSHR_CONT;
		} else if (tx_word && (dspi->len == 1))
			dspi_pushr |= SPI_PUSHR_EOQ;

		if (first) {
			first = 0;
			dspi_pushr |= SPI_PUSHR_CTCNT; /* clear counter */
		}

		regmap_write(dspi->regmap, SPI_PUSHR, dspi_pushr);

		tx_count++;
	}

	return tx_count * (tx_word + 1);
}

static int dspi_eoq_read(struct fsl_dspi *dspi)
{
	int rx_count = 0;
	int rx_word = is_double_byte_mode(dspi);

	while ((dspi->rx < dspi->rx_end)
			&& (rx_count < DSPI_FIFO_SIZE)) {
		dspi_data_from_popr(dspi, rx_word);
		rx_count++;
	}

	return rx_count;
}

static int dspi_tcfq_write(struct fsl_dspi *dspi)
{
	int tx_word;
	u32 dspi_pushr = 0;

	tx_word = is_double_byte_mode(dspi);

	if (tx_word && (dspi->len == 1)) {
		dspi->dataflags |= TRAN_STATE_WORD_ODD_NUM;
		regmap_update_bits(dspi->regmap, SPI_CTAR(dspi->cs),
				SPI_FRAME_BITS_MASK, SPI_FRAME_BITS(8));
		tx_word = 0;
	}

	dspi_pushr = dspi_data_to_pushr(dspi, tx_word);

	if ((dspi->cs_change) && (!dspi->len))
		dspi_pushr &= ~SPI_PUSHR_CONT;

	regmap_write(dspi->regmap, SPI_PUSHR, dspi_pushr);

	return tx_word + 1;
}

static void dspi_tcfq_read(struct fsl_dspi *dspi)
{
	int rx_word = is_double_byte_mode(dspi);

	dspi_data_from_popr(dspi, rx_word);
}

static int dspi_transfer_one_message(struct spi_master *master,
		struct spi_message *message)
{
	struct fsl_dspi *dspi = spi_master_get_devdata(master);
	struct spi_device *spi = message->spi;
	struct spi_transfer *transfer;
	int status = 0;
	message->actual_length = 0;

	list_for_each_entry(transfer, &message->transfers, transfer_list) {
		dspi->cur_transfer = transfer;
		dspi->cur_msg = message;
		dspi->cur_chip = spi_get_ctldata(spi);
		dspi->cs = spi->chip_select;
		if (dspi->cur_transfer->transfer_list.next
				== &dspi->cur_msg->transfers)
			transfer->cs_change = 1;
		dspi->cs_change = transfer->cs_change;
		dspi->void_write_data = dspi->cur_chip->void_write_data;

		dspi->dataflags = 0;
		dspi->tx = (void *)transfer->tx_buf;
		dspi->tx_end = dspi->tx + transfer->len;
		dspi->rx = transfer->rx_buf;
		dspi->rx_end = dspi->rx + transfer->len;
		dspi->len = transfer->len;

		if (!dspi->rx)
			dspi->dataflags |= TRAN_STATE_RX_VOID;

		if (!dspi->tx)
			dspi->dataflags |= TRAN_STATE_TX_VOID;

		regmap_write(dspi->regmap, SPI_MCR, dspi->cur_chip->mcr_val);
		regmap_update_bits(dspi->regmap, SPI_MCR,
				SPI_MCR_CLR_TXF | SPI_MCR_CLR_RXF,
				SPI_MCR_CLR_TXF | SPI_MCR_CLR_RXF);
		regmap_write(dspi->regmap, SPI_CTAR(dspi->cs),
				dspi->cur_chip->ctar_val);
		if (transfer->speed_hz)
			regmap_write(dspi->regmap, SPI_CTAR(dspi->cs),
					dspi->cur_chip->ctar_val);

		if (dspi->trans_mode == DSPI_EOQ_MODE) {
			regmap_write(dspi->regmap, SPI_RSER, SPI_RSER_EOQFE);
			message->actual_length += dspi_eoq_write(dspi);
		} else if (dspi->trans_mode == DSPI_TCFQ_MODE) {
			regmap_write(dspi->regmap, SPI_RSER, SPI_RSER_TCFQE);
			message->actual_length += dspi_tcfq_write(dspi);
		}

		if (wait_event_interruptible(dspi->waitq, dspi->waitflags))
			dev_err(&dspi->pdev->dev, "wait transfer complete fail!\n");
		dspi->waitflags = 0;

		if (transfer->delay_usecs)
			udelay(transfer->delay_usecs);

	}

	message->status = status;
	spi_finalize_current_message(master);

	return status;

}

static int dspi_setup(struct spi_device *spi)
{
	struct chip_data *chip;
	struct fsl_dspi *dspi = spi_master_get_devdata(spi->master);
	unsigned char br = 0, pbr = 0, fmsz = 0;

	/* Only alloc on first setup */
	chip = spi_get_ctldata(spi);
	if (chip == NULL) {
		chip = kcalloc(1, sizeof(struct chip_data), GFP_KERNEL);
		if (!chip)
			return -ENOMEM;
	}

	chip->mcr_val = SPI_MCR_MASTER | SPI_MCR_PCSIS |
		SPI_MCR_CLR_TXF | SPI_MCR_CLR_RXF;
	if ((spi->bits_per_word >= 4) && (spi->bits_per_word <= 16)) {
		fmsz = spi->bits_per_word - 1;
	} else {
		pr_err("Invalid wordsize\n");
		kfree(chip);
		return -ENODEV;
	}

	chip->void_write_data = 0;

	hz_to_spi_baud(&pbr, &br,
			spi->max_speed_hz, clk_get_rate(dspi->clk));

	chip->ctar_val =  SPI_CTAR_FMSZ(fmsz)
		| SPI_CTAR_CPOL(spi->mode & SPI_CPOL ? 1 : 0)
		| SPI_CTAR_CPHA(spi->mode & SPI_CPHA ? 1 : 0)
		| SPI_CTAR_LSBFE(spi->mode & SPI_LSB_FIRST ? 1 : 0)
		| SPI_CTAR_PBR(pbr)
		| SPI_CTAR_BR(br);

	spi_set_ctldata(spi, chip);

	return 0;
}

static void dspi_cleanup(struct spi_device *spi)
{
	struct chip_data *chip = spi_get_ctldata((struct spi_device *)spi);

	dev_dbg(&spi->dev, "spi_device %u.%u cleanup\n",
			spi->master->bus_num, spi->chip_select);

	kfree(chip);
}

static irqreturn_t dspi_interrupt(int irq, void *dev_id)
{
	struct fsl_dspi *dspi = (struct fsl_dspi *)dev_id;
	struct spi_message *msg = dspi->cur_msg;

	if (dspi->trans_mode == DSPI_EOQ_MODE) {
		regmap_write(dspi->regmap, SPI_SR, SPI_SR_EOQF);
		dspi_eoq_read(dspi);
	} else if (dspi->trans_mode == DSPI_TCFQ_MODE) {
		regmap_write(dspi->regmap, SPI_SR, SPI_SR_TCFQF);
		dspi_tcfq_read(dspi);
	}

	if (!dspi->len) {
		if (dspi->dataflags & TRAN_STATE_WORD_ODD_NUM)
			regmap_update_bits(dspi->regmap, SPI_CTAR(dspi->cs),
				SPI_FRAME_BITS_MASK, SPI_FRAME_BITS(16));

		dspi->waitflags = 1;
		wake_up_interruptible(&dspi->waitq);
	} else {
		if (dspi->trans_mode == DSPI_EOQ_MODE)
			msg->actual_length += dspi_eoq_write(dspi);
		else if (dspi->trans_mode == DSPI_TCFQ_MODE)
			msg->actual_length += dspi_tcfq_write(dspi);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id fsl_dspi_dt_ids[] = {
	{ .compatible = "fsl,vf610-dspi", .data = NULL, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fsl_dspi_dt_ids);

#ifdef CONFIG_PM_SLEEP
static int dspi_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct fsl_dspi *dspi = spi_master_get_devdata(master);

	spi_master_suspend(master);
	clk_disable_unprepare(dspi->clk);

	return 0;
}

static int dspi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct fsl_dspi *dspi = spi_master_get_devdata(master);

	clk_prepare_enable(dspi->clk);
	spi_master_resume(master);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(dspi_pm, dspi_suspend, dspi_resume);

static struct regmap_config dspi_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0x88,
};

static int dspi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct spi_master *master;
	struct fsl_dspi *dspi;
	struct resource *res;
	void __iomem *base;
	int ret = 0, cs_num, bus_num;

	master = spi_alloc_master(&pdev->dev, sizeof(struct fsl_dspi));
	if (!master)
		return -ENOMEM;

	dspi = spi_master_get_devdata(master);
	dspi->pdev = pdev;
	dspi->master = master;

	master->transfer = NULL;
	master->cleanup = dspi_cleanup;
	master->setup = dspi_setup;
	master->transfer_one_message = dspi_transfer_one_message;
	master->dev.of_node = pdev->dev.of_node;

	master->mode_bits = SPI_CPOL | SPI_CPHA;
	master->bits_per_word_mask = SPI_BPW_MASK(4) | SPI_BPW_MASK(8) |
					SPI_BPW_MASK(16);

	ret = of_property_read_u32(np, "spi-num-chipselects", &cs_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't get spi-num-chipselects\n");
		goto out_master_put;
	}
	master->num_chipselect = cs_num;

	ret = of_property_read_u32(np, "bus-num", &bus_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't get bus-num\n");
		goto out_master_put;
	}
	master->bus_num = bus_num;

	if (of_property_read_bool(np, "eoq-mode"))
		dspi->trans_mode = DSPI_EOQ_MODE;
	else if (of_property_read_bool(np, "tcfq-mode"))
		dspi->trans_mode = DSPI_TCFQ_MODE;
	else {
		dev_err(&pdev->dev, "can't get dspi transfer mode\n");
		goto out_master_put;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		ret = PTR_ERR(base);
		goto out_master_put;
	}

	dspi_regmap_config.lock_arg = dspi;
	dspi->regmap = devm_regmap_init_mmio_clk(&pdev->dev, NULL, base,
						&dspi_regmap_config);
	if (IS_ERR(dspi->regmap)) {
		dev_err(&pdev->dev, "failed to init regmap: %ld\n",
				PTR_ERR(dspi->regmap));
		return PTR_ERR(dspi->regmap);
	}

	dspi->irq = platform_get_irq(pdev, 0);
	if (dspi->irq < 0) {
		dev_err(&pdev->dev, "can't get platform irq\n");
		ret = dspi->irq;
		goto out_master_put;
	}

	ret = devm_request_irq(&pdev->dev, dspi->irq, dspi_interrupt, 0,
			pdev->name, dspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to attach DSPI interrupt\n");
		goto out_master_put;
	}

	dspi->clk = devm_clk_get(&pdev->dev, "dspi");
	if (IS_ERR(dspi->clk)) {
		ret = PTR_ERR(dspi->clk);
		dev_err(&pdev->dev, "unable to get clock\n");
		goto out_master_put;
	}
	clk_prepare_enable(dspi->clk);

	init_waitqueue_head(&dspi->waitq);
	platform_set_drvdata(pdev, master);

	ret = spi_register_master(master);
	if (ret != 0) {
		dev_err(&pdev->dev, "Problem registering DSPI master\n");
		goto out_clk_put;
	}

	pr_info(KERN_INFO "Freescale DSPI master initialized\n");
	return ret;

out_clk_put:
	clk_disable_unprepare(dspi->clk);
out_master_put:
	spi_master_put(master);

	return ret;
}

static int dspi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct fsl_dspi *dspi = spi_master_get_devdata(master);

	/* Disconnect from the SPI framework */
	clk_disable_unprepare(dspi->clk);
	spi_unregister_master(dspi->master);
	spi_master_put(dspi->master);

	return 0;
}

static struct platform_driver fsl_dspi_driver = {
	.driver.name    = DRIVER_NAME,
	.driver.of_match_table = fsl_dspi_dt_ids,
	.driver.owner   = THIS_MODULE,
	.driver.pm = &dspi_pm,
	.probe          = dspi_probe,
	.remove		= dspi_remove,
};
module_platform_driver(fsl_dspi_driver);

MODULE_DESCRIPTION("Freescale DSPI Controller Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
