/*
 * coh901327_wdt.c
 *
 * Copyright (C) 2008-2009 ST-Ericsson AB
 * License terms: GNU General Public License (GPL) version 2
 * Watchdog driver for the ST-Ericsson AB COH 901 327 IP core
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>

#define DRV_NAME "WDOG COH 901 327"


#define U300_WDOG_FR							0x00
#define U300_WDOG_FR_FEED_RESTART_TIMER					0xFEEDU
#define U300_WDOG_TR							0x04
#define U300_WDOG_TR_TIMEOUT_MASK					0x7FFFU
#define U300_WDOG_D1R							0x08
#define U300_WDOG_D1R_DISABLE1_DISABLE_TIMER				0x2BADU
#define U300_WDOG_D2R							0x0C
#define U300_WDOG_D2R_DISABLE2_DISABLE_TIMER				0xCAFEU
#define U300_WDOG_D2R_DISABLE_STATUS_DISABLED				0xDABEU
#define U300_WDOG_D2R_DISABLE_STATUS_ENABLED				0x0000U
#define U300_WDOG_SR							0x10
#define U300_WDOG_SR_STATUS_TIMED_OUT					0xCFE8U
#define U300_WDOG_SR_STATUS_NORMAL					0x0000U
#define U300_WDOG_SR_RESET_STATUS_RESET					0xE8B4U
#define U300_WDOG_CR							0x14
#define U300_WDOG_CR_VALID_IND						0x8000U
#define U300_WDOG_CR_VALID_STABLE					0x0000U
#define U300_WDOG_CR_COUNT_VALUE_MASK					0x7FFFU
#define U300_WDOG_JOR							0x18
#define U300_WDOG_JOR_JTAG_MODE_IND					0x0002U
#define U300_WDOG_JOR_JTAG_WATCHDOG_ENABLE				0x0001U
#define U300_WDOG_RR							0x1C
#define U300_WDOG_RR_RESTART_VALUE_RESUME				0xACEDU
#define U300_WDOG_IER							0x20
#define U300_WDOG_IER_WILL_BARK_IRQ_EVENT_IND				0x0001U
#define U300_WDOG_IER_WILL_BARK_IRQ_ACK_ENABLE				0x0001U
#define U300_WDOG_IMR							0x24
#define U300_WDOG_IMR_WILL_BARK_IRQ_ENABLE				0x0001U
#define U300_WDOG_IFR							0x28
#define U300_WDOG_IFR_WILL_BARK_IRQ_FORCE_ENABLE			0x0001U

static unsigned int margin = 60;
static resource_size_t phybase;
static resource_size_t physize;
static int irq;
static void __iomem *virtbase;
static struct device *parent;

static struct clk *clk;

static void coh901327_enable(u16 timeout)
{
	u16 val;
	unsigned long freq;
	unsigned long delay_ns;

	clk_enable(clk);
	
	val = readw(virtbase + U300_WDOG_D2R);
	if (val == U300_WDOG_D2R_DISABLE_STATUS_DISABLED)
		writew(U300_WDOG_RR_RESTART_VALUE_RESUME,
		       virtbase + U300_WDOG_RR);
	
	writew(U300_WDOG_IER_WILL_BARK_IRQ_ACK_ENABLE,
	       virtbase + U300_WDOG_IER);
	freq = clk_get_rate(clk);
	delay_ns = DIV_ROUND_UP(1000000000, freq); 
	delay_ns = 3 * delay_ns; 
	ndelay(delay_ns);
	
	writew(U300_WDOG_IMR_WILL_BARK_IRQ_ENABLE, virtbase + U300_WDOG_IMR);
	
	writew(timeout, virtbase + U300_WDOG_TR);
	
	writew(U300_WDOG_FR_FEED_RESTART_TIMER, virtbase + U300_WDOG_FR);
	(void) readw(virtbase + U300_WDOG_CR);
	val = readw(virtbase + U300_WDOG_D2R);
	clk_disable(clk);
	if (val != U300_WDOG_D2R_DISABLE_STATUS_ENABLED)
		dev_err(parent,
			"%s(): watchdog not enabled! D2R value %04x\n",
			__func__, val);
}

static void coh901327_disable(void)
{
	u16 val;

	clk_enable(clk);
	
	writew(0x0000U, virtbase + U300_WDOG_IMR);
	
	val = readw(virtbase + U300_WDOG_D2R);
	if (val != U300_WDOG_D2R_DISABLE_STATUS_DISABLED) {
		writew(U300_WDOG_D1R_DISABLE1_DISABLE_TIMER,
		       virtbase + U300_WDOG_D1R);
		writew(U300_WDOG_D2R_DISABLE2_DISABLE_TIMER,
		       virtbase + U300_WDOG_D2R);
		
		writew(U300_WDOG_D2R_DISABLE2_DISABLE_TIMER,
		       virtbase + U300_WDOG_D2R);
	}
	val = readw(virtbase + U300_WDOG_D2R);
	clk_disable(clk);
	if (val != U300_WDOG_D2R_DISABLE_STATUS_DISABLED)
		dev_err(parent,
			"%s(): watchdog not disabled! D2R value %04x\n",
			__func__, val);
}

static int coh901327_start(struct watchdog_device *wdt_dev)
{
	coh901327_enable(wdt_dev->timeout * 100);
	return 0;
}

static int coh901327_stop(struct watchdog_device *wdt_dev)
{
	coh901327_disable();
	return 0;
}

static int coh901327_ping(struct watchdog_device *wdd)
{
	clk_enable(clk);
	
	writew(U300_WDOG_FR_FEED_RESTART_TIMER,
	       virtbase + U300_WDOG_FR);
	clk_disable(clk);
	return 0;
}

static int coh901327_settimeout(struct watchdog_device *wdt_dev,
				unsigned int time)
{
	wdt_dev->timeout = time;
	clk_enable(clk);
	
	writew(time * 100, virtbase + U300_WDOG_TR);
	
	writew(U300_WDOG_FR_FEED_RESTART_TIMER,
	       virtbase + U300_WDOG_FR);
	clk_disable(clk);
	return 0;
}

static unsigned int coh901327_gettimeleft(struct watchdog_device *wdt_dev)
{
	u16 val;

	clk_enable(clk);
	
	val = readw(virtbase + U300_WDOG_CR);
	while (val & U300_WDOG_CR_VALID_IND)
		val = readw(virtbase + U300_WDOG_CR);
	val &= U300_WDOG_CR_COUNT_VALUE_MASK;
	clk_disable(clk);
	if (val != 0)
		val /= 100;

	return val;
}

static irqreturn_t coh901327_interrupt(int irq, void *data)
{
	u16 val;

	clk_enable(clk);
	val = readw(virtbase + U300_WDOG_IER);
	if (val == U300_WDOG_IER_WILL_BARK_IRQ_EVENT_IND)
		writew(U300_WDOG_IER_WILL_BARK_IRQ_ACK_ENABLE,
		       virtbase + U300_WDOG_IER);
	writew(0x0000U, virtbase + U300_WDOG_IMR);
	clk_disable(clk);
	dev_crit(parent, "watchdog is barking!\n");
	return IRQ_HANDLED;
}

static const struct watchdog_info coh901327_ident = {
	.options = WDIOF_CARDRESET | WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.identity = DRV_NAME,
};

static struct watchdog_ops coh901327_ops = {
	.owner = THIS_MODULE,
	.start = coh901327_start,
	.stop = coh901327_stop,
	.ping = coh901327_ping,
	.set_timeout = coh901327_settimeout,
	.get_timeleft = coh901327_gettimeleft,
};

static struct watchdog_device coh901327_wdt = {
	.info = &coh901327_ident,
	.ops = &coh901327_ops,
	.min_timeout = 0,
	.max_timeout = 327,
};

static int __exit coh901327_remove(struct platform_device *pdev)
{
	watchdog_unregister_device(&coh901327_wdt);
	coh901327_disable();
	free_irq(irq, pdev);
	clk_put(clk);
	iounmap(virtbase);
	release_mem_region(phybase, physize);
	return 0;
}

static int __init coh901327_probe(struct platform_device *pdev)
{
	int ret;
	u16 val;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOENT;

	parent = &pdev->dev;
	physize = resource_size(res);
	phybase = res->start;

	if (request_mem_region(phybase, physize, DRV_NAME) == NULL) {
		ret = -EBUSY;
		goto out;
	}

	virtbase = ioremap(phybase, physize);
	if (!virtbase) {
		ret = -ENOMEM;
		goto out_no_remap;
	}

	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		dev_err(&pdev->dev, "could not get clock\n");
		goto out_no_clk;
	}
	ret = clk_enable(clk);
	if (ret) {
		dev_err(&pdev->dev, "could not enable clock\n");
		goto out_no_clk_enable;
	}

	val = readw(virtbase + U300_WDOG_SR);
	switch (val) {
	case U300_WDOG_SR_STATUS_TIMED_OUT:
		dev_info(&pdev->dev,
			"watchdog timed out since last chip reset!\n");
		coh901327_wdt.bootstatus |= WDIOF_CARDRESET;
		
		break;
	case U300_WDOG_SR_STATUS_NORMAL:
		dev_info(&pdev->dev,
			"in normal status, no timeouts have occurred.\n");
		break;
	default:
		dev_info(&pdev->dev,
			"contains an illegal status code (%08x)\n", val);
		break;
	}

	val = readw(virtbase + U300_WDOG_D2R);
	switch (val) {
	case U300_WDOG_D2R_DISABLE_STATUS_DISABLED:
		dev_info(&pdev->dev, "currently disabled.\n");
		break;
	case U300_WDOG_D2R_DISABLE_STATUS_ENABLED:
		dev_info(&pdev->dev,
			 "currently enabled! (disabling it now)\n");
		coh901327_disable();
		break;
	default:
		dev_err(&pdev->dev,
			"contains an illegal enable/disable code (%08x)\n",
			val);
		break;
	}

	
	writew(U300_WDOG_SR_RESET_STATUS_RESET, virtbase + U300_WDOG_SR);

	irq = platform_get_irq(pdev, 0);
	if (request_irq(irq, coh901327_interrupt, 0,
			DRV_NAME " Bark", pdev)) {
		ret = -EIO;
		goto out_no_irq;
	}

	clk_disable(clk);

	if (margin < 1 || margin > 327)
		margin = 60;
	coh901327_wdt.timeout = margin;

	ret = watchdog_register_device(&coh901327_wdt);
	if (ret == 0)
		dev_info(&pdev->dev,
			 "initialized. timer margin=%d sec\n", margin);
	else
		goto out_no_wdog;

	return 0;

out_no_wdog:
	free_irq(irq, pdev);
out_no_irq:
	clk_disable(clk);
out_no_clk_enable:
	clk_put(clk);
out_no_clk:
	iounmap(virtbase);
out_no_remap:
	release_mem_region(phybase, SZ_4K);
out:
	return ret;
}

#ifdef CONFIG_PM

static u16 wdogenablestore;
static u16 irqmaskstore;

static int coh901327_suspend(struct platform_device *pdev, pm_message_t state)
{
	irqmaskstore = readw(virtbase + U300_WDOG_IMR) & 0x0001U;
	wdogenablestore = readw(virtbase + U300_WDOG_D2R);
	
	if (wdogenablestore == U300_WDOG_D2R_DISABLE_STATUS_ENABLED)
		coh901327_disable();
	return 0;
}

static int coh901327_resume(struct platform_device *pdev)
{
	
	writew(irqmaskstore, virtbase + U300_WDOG_IMR);
	if (wdogenablestore == U300_WDOG_D2R_DISABLE_STATUS_ENABLED) {
		
		writew(U300_WDOG_RR_RESTART_VALUE_RESUME,
		       virtbase + U300_WDOG_RR);
		writew(U300_WDOG_FR_FEED_RESTART_TIMER,
		       virtbase + U300_WDOG_FR);
	}
	return 0;
}
#else
#define coh901327_suspend NULL
#define coh901327_resume  NULL
#endif

void coh901327_watchdog_reset(void)
{
	
	writew(U300_WDOG_JOR_JTAG_WATCHDOG_ENABLE,
	       virtbase + U300_WDOG_JOR);
	coh901327_enable(500);
	
}

static struct platform_driver coh901327_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "coh901327_wdog",
	},
	.remove		= __exit_p(coh901327_remove),
	.suspend	= coh901327_suspend,
	.resume		= coh901327_resume,
};

static int __init coh901327_init(void)
{
	return platform_driver_probe(&coh901327_driver, coh901327_probe);
}
module_init(coh901327_init);

static void __exit coh901327_exit(void)
{
	platform_driver_unregister(&coh901327_driver);
}
module_exit(coh901327_exit);

MODULE_AUTHOR("Linus Walleij <linus.walleij@stericsson.com>");
MODULE_DESCRIPTION("COH 901 327 Watchdog");

module_param(margin, uint, 0);
MODULE_PARM_DESC(margin, "Watchdog margin in seconds (default 60s)");

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:coh901327-watchdog");
