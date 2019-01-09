/*
 *  Atheros AR71XX/AR724X/AR913X GPIO API support
 *
 *  Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *  Copyright (C) 2010-2011 Jaiganesh Narayanan <jnarayanan@atheros.com>
 *  Copyright (C) 2008-2011 Gabor Juhos <juhosg@openwrt.org>
 *  Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 *
 *  Parts of this file are based on Atheros' 2.6.15/2.6.31 BSP
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/of.h>

#include <asm/mach-ath79/ar71xx_regs.h>
#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/irq.h>
#include "common.h"

void __iomem *ath79_gpio_base;
EXPORT_SYMBOL_GPL(ath79_gpio_base);

static unsigned long ath79_gpio_count;
static DEFINE_SPINLOCK(ath79_gpio_lock);

/*
 * gpio_both_edge is a bitmask of which gpio pins need to have
 * the detect priority flipped from the interrupt handler to
 * emulate IRQ_TYPE_EDGE_BOTH.
 */
static unsigned long gpio_both_edge = 0;

static void __ath79_gpio_set_value(unsigned gpio, int value)
{
	void __iomem *base = ath79_gpio_base;

	if (value)
		__raw_writel(1 << gpio, base + AR71XX_GPIO_REG_SET);
	else
		__raw_writel(1 << gpio, base + AR71XX_GPIO_REG_CLEAR);
}

static int __ath79_gpio_get_value(unsigned gpio)
{
	return (__raw_readl(ath79_gpio_base + AR71XX_GPIO_REG_IN) >> gpio) & 1;
}

static int ath79_gpio_get_value(struct gpio_chip *chip, unsigned offset)
{
	return __ath79_gpio_get_value(offset);
}

static void ath79_gpio_set_value(struct gpio_chip *chip,
				  unsigned offset, int value)
{
	__ath79_gpio_set_value(offset, value);
}

static int ath79_gpio_direction_input(struct gpio_chip *chip,
				       unsigned offset)
{
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_OE) & ~(1 << offset),
		     base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);

	return 0;
}

static int ath79_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	if (value)
		__raw_writel(1 << offset, base + AR71XX_GPIO_REG_SET);
	else
		__raw_writel(1 << offset, base + AR71XX_GPIO_REG_CLEAR);

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_OE) | (1 << offset),
		     base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);

	return 0;
}

static int ar934x_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_OE) | (1 << offset),
		     base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);

	return 0;
}

static int ar934x_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
					int value)
{
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	if (value)
		__raw_writel(1 << offset, base + AR71XX_GPIO_REG_SET);
	else
		__raw_writel(1 << offset, base + AR71XX_GPIO_REG_CLEAR);

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_OE) & ~(1 << offset),
		     base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);

	return 0;
}

int ath79_gpio_direction_select(unsigned gpio, bool oe)
{
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;
	bool ieq_1 = (soc_is_ar934x() ||
			soc_is_qca953x() ||
			soc_is_qca955x() ||
			soc_is_qca956x());

	if (gpio >= ath79_gpio_count)
		return -1;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	if ((ieq_1 && oe) || (!ieq_1 && !oe))
		__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_OE) & ~(1 << gpio),
				base + AR71XX_GPIO_REG_OE);
	else
		__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_OE) | (1 << gpio),
				base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);

	return 0;
}

static struct gpio_chip ath79_gpio_chip = {
	.label			= "ath79",
	.get			= ath79_gpio_get_value,
	.set			= ath79_gpio_set_value,
	.direction_input	= ath79_gpio_direction_input,
	.direction_output	= ath79_gpio_direction_output,
	.base			= 0,
};

static void __iomem *ath79_gpio_get_function_reg(void)
{
	u32 reg = 0;

	if (soc_is_ar71xx() ||
	    soc_is_ar724x() ||
	    soc_is_ar913x() ||
	    soc_is_ar933x())
		reg = AR71XX_GPIO_REG_FUNC;
	else if (soc_is_ar934x() || soc_is_qca953x() || soc_is_qca956x())
		reg = AR934X_GPIO_REG_FUNC;
	else
		BUG();

	return ath79_gpio_base + reg;
}

void ath79_gpio_function_enable(u32 mask)
{
	void __iomem *reg = ath79_gpio_get_function_reg();
	unsigned long flags;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	__raw_writel(__raw_readl(reg) | mask, reg);
	/* flush write */
	__raw_readl(reg);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);
}

void ath79_gpio_function_disable(u32 mask)
{
	void __iomem *reg = ath79_gpio_get_function_reg();
	unsigned long flags;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	__raw_writel(__raw_readl(reg) & ~mask, reg);
	/* flush write */
	__raw_readl(reg);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);
}

void ath79_gpio_function_setup(u32 set, u32 clear)
{
	void __iomem *reg = ath79_gpio_get_function_reg();
	unsigned long flags;

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	__raw_writel((__raw_readl(reg) & ~clear) | set, reg);
	/* flush write */
	__raw_readl(reg);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);
}

void __init ath79_gpio_input_select(unsigned gpio, u8 val)
{
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;
	unsigned int reg;
	u32 t, s;

	BUG_ON(!soc_is_ar934x());

	if (gpio >= ath79_gpio_count)
		return;

	reg = AR934X_GPIO_REG_IN_ENABLE0 + 4 * (val / 4);
	s = 8 * (val % 4);

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	t = __raw_readl(base + reg);
	t &= ~(0xff << s);
	t |= gpio << s;
	__raw_writel(t, base + reg);

	/* flush write */
	(void) __raw_readl(base + reg);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);
}

void __init ath79_gpio_output_select(unsigned gpio, u8 val)
{
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;
	unsigned int reg;
	u32 t, s;

	BUG_ON(!soc_is_ar934x() && !soc_is_qca953x() && !soc_is_qca956x());

	if (gpio >= ath79_gpio_count)
		return;

	reg = AR934X_GPIO_REG_OUT_FUNC0 + 4 * (gpio / 4);
	s = 8 * (gpio % 4);

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	t = __raw_readl(base + reg);
	t &= ~(0xff << s);
	t |= val << s;
	__raw_writel(t, base + reg);

	/* flush write */
	(void) __raw_readl(base + reg);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);
}

static int ath79_gpio_irq_type(struct irq_data *d, unsigned type)
{
	int offset = d->irq - ATH79_GPIO_IRQ_BASE;
	void __iomem *base = ath79_gpio_base;
	unsigned long flags;
	unsigned long int_type;
	unsigned long int_polarity;
	unsigned long bit = (1 << offset);

	spin_lock_irqsave(&ath79_gpio_lock, flags);

	int_type = __raw_readl(base + AR71XX_GPIO_REG_INT_TYPE);
	int_polarity = __raw_readl(base + AR71XX_GPIO_REG_INT_POLARITY);

	gpio_both_edge &= ~bit;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		int_type &= ~bit;
		int_polarity |= bit;
		break;

	case IRQ_TYPE_EDGE_FALLING:
		int_type &= ~bit;
		int_polarity &= ~bit;
		break;

	case IRQ_TYPE_LEVEL_HIGH:
		int_type |= bit;
		int_polarity |= bit;
		break;

	case IRQ_TYPE_LEVEL_LOW:
		int_type |= bit;
		int_polarity &= ~bit;
		break;

	case IRQ_TYPE_EDGE_BOTH:
		int_type |= bit;
		/* set polarity based on current value */
		if (gpio_get_value(offset)) {
			int_polarity &= ~bit;
		} else {
			int_polarity |= bit;
		}
		/* flip this gpio in the interrupt handler */
		gpio_both_edge |= bit;
		break;

	default:
		spin_unlock_irqrestore(&ath79_gpio_lock, flags);
		return -EINVAL;
	}

	__raw_writel(int_type, base + AR71XX_GPIO_REG_INT_TYPE);
	__raw_writel(int_polarity, base + AR71XX_GPIO_REG_INT_POLARITY);

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_INT_MODE) | (1 << offset),
		     base + AR71XX_GPIO_REG_INT_MODE);

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_INT_ENABLE) & ~(1 << offset),
		     base + AR71XX_GPIO_REG_INT_ENABLE);

	spin_unlock_irqrestore(&ath79_gpio_lock, flags);
	return 0;
}

static void ath79_gpio_irq_enable(struct irq_data *d)
{
	int offset = d->irq - ATH79_GPIO_IRQ_BASE;
	void __iomem *base = ath79_gpio_base;

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_INT_ENABLE) | (1 << offset),
		     base + AR71XX_GPIO_REG_INT_ENABLE);
}

static void ath79_gpio_irq_disable(struct irq_data *d)
{
	int offset = d->irq - ATH79_GPIO_IRQ_BASE;
	void __iomem *base = ath79_gpio_base;

	__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_INT_ENABLE) & ~(1 << offset),
		     base + AR71XX_GPIO_REG_INT_ENABLE);
}

static struct irq_chip ath79_gpio_irqchip = {
	.name = "GPIO",
	.irq_enable = ath79_gpio_irq_enable,
	.irq_disable = ath79_gpio_irq_disable,
	.irq_set_type = ath79_gpio_irq_type,
};

static irqreturn_t ath79_gpio_irq(int irq, void *dev)
{
	void __iomem *base = ath79_gpio_base;
	unsigned int stat = __raw_readl(base + AR71XX_GPIO_REG_INT_PENDING);

	int irq_base = ATH79_GPIO_IRQ_BASE;

	while (stat) {
		int bit_num = __ffs(stat);
		unsigned long bit = (1<<bit_num);

		if (bit & gpio_both_edge) {
			__raw_writel(__raw_readl(base + AR71XX_GPIO_REG_INT_POLARITY) ^ bit,
				base + AR71XX_GPIO_REG_INT_POLARITY);
		}

		handle_nested_irq(irq_base + bit_num);
		stat &= ~bit;
	}

	return IRQ_HANDLED;
}

static int __devinit ath79_gpio_irq_init(struct gpio_chip *chip)
{
	int irq;
	int irq_base = ATH79_GPIO_IRQ_BASE;

	for (irq = irq_base; irq < irq_base + chip->ngpio; irq++) {
		irq_set_chip_data(irq, chip);
		irq_set_chip_and_handler(irq, &ath79_gpio_irqchip, handle_simple_irq);
		irq_set_nested_thread(irq, 1);
		irq_set_noprobe(irq);
	}

	return 0;
}


void __init ath79_gpio_init(void)
{
	int err;

	if (soc_is_ar71xx())
		ath79_gpio_count = AR71XX_GPIO_COUNT;
	else if (soc_is_ar724x())
		ath79_gpio_count = AR724X_GPIO_COUNT;
	else if (soc_is_ar913x())
		ath79_gpio_count = AR913X_GPIO_COUNT;
	else if (soc_is_ar933x())
		ath79_gpio_count = AR933X_GPIO_COUNT;
	else if (soc_is_ar934x())
		ath79_gpio_count = AR934X_GPIO_COUNT;
	else if (soc_is_qca953x())
		ath79_gpio_count = QCA953X_GPIO_COUNT;
	else if (soc_is_qca955x())
		ath79_gpio_count = QCA955X_GPIO_COUNT;
	else if (soc_is_qca956x())
		ath79_gpio_count = QCA956X_GPIO_COUNT;
	else
		BUG();

	ath79_gpio_base = ioremap_nocache(AR71XX_GPIO_BASE, AR71XX_GPIO_SIZE);
	ath79_gpio_chip.ngpio = ath79_gpio_count;
	if (soc_is_ar934x() || soc_is_qca953x() || soc_is_qca955x() ||
	    soc_is_qca956x()) {
		ath79_gpio_chip.direction_input = ar934x_gpio_direction_input;
		ath79_gpio_chip.direction_output = ar934x_gpio_direction_output;
	}

	ath79_gpio_chip.of_node = of_find_node_by_path("/ath79-gpio");

	err = gpiochip_add(&ath79_gpio_chip);
	if (err)
		panic("cannot add AR71xx GPIO chip, error=%d", err);

	ath79_gpio_irq_init(&ath79_gpio_chip);

	if (request_threaded_irq(ATH79_MISC_IRQ_GPIO, NULL, ath79_gpio_irq, IRQF_ONESHOT, "ath79-gpio", NULL)) {
		printk(KERN_ERR "Request IRQ for GPIO fail!\n");
	}
}

int gpio_get_value(unsigned gpio)
{
	if (gpio < ath79_gpio_count)
		return __ath79_gpio_get_value(gpio);

	return __gpio_get_value(gpio);
}
EXPORT_SYMBOL(gpio_get_value);

void gpio_set_value(unsigned gpio, int value)
{
	if (gpio < ath79_gpio_count)
		__ath79_gpio_set_value(gpio, value);
	else
		__gpio_set_value(gpio, value);
}
EXPORT_SYMBOL(gpio_set_value);

int gpio_to_irq(unsigned gpio)
{
	if (gpio > ath79_gpio_count) {
		return -EINVAL;
	}

	return ATH79_GPIO_IRQ_BASE + gpio;
}
EXPORT_SYMBOL(gpio_to_irq);

int irq_to_gpio(unsigned irq)
{
	unsigned gpio = irq - ATH79_GPIO_IRQ_BASE;

	if (gpio > ath79_gpio_count) {
		return -EINVAL;
	}

	return gpio;
}
EXPORT_SYMBOL(irq_to_gpio);
