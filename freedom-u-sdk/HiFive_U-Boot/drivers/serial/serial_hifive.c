/*
 * Copyright (C) 2018 Microsemi Corporation
 *
 * Modified to support C structur SoC access by
 * Padmarao Begari <padmarao.begari@microsemi.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <watchdog.h>
#include <serial.h>
#include <linux/compiler.h>

#include <asm/io.h>
#ifdef CONFIG_DM_SERIAL
#include <asm/arch/msc_serial.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

/* HiFive unleashed UART register footprint */
typedef struct hifive_uart {
    u32 txdata;
    u32 rxdata;
    u32 txctrl;
    u32 rxctrl;
    u32 ie;
    u32 ip;
    u32 div;
} hifive_uart_t;

/* Information about a serial port */
struct hive_serial_platdata {
	uint32_t base_addr;
};

/* TXCTRL register */
#define UART_TXEN               0x1
#define UART_TXWM(x)            (((x) & 0xffff) << 16)

/* RXCTRL register */
#define UART_RXEN               0x1
#define UART_RXWM(x)            (((x) & 0xffff) << 16)

/* IP register */
#define UART_IP_TXWM            0x1
#define UART_IP_RXWM            0x2

#define UART_TXFIFO_FULL			0x80000000
#define UART_RXFIFO_EMPTY			0x80000000

/* If Driver Model for Serial is not defined */
#ifndef CONFIG_DM_SERIAL
/* Set HiFive UART baud rate */
static void hifive_uart_setbrg(void)
{
	hifive_uart_t *usart = (hifive_uart_t *)HIFIVE_UART_BASE_ADDR;
    u32 baud_value;
   /*
    * BAUD_VALUE = (CLOCK / BAUD_RATE) - 1
    */
    baud_value = (HIFIVE_PERIPH_CLK_FREQ / CONFIG_BAUDRATE) -1;

    writel(baud_value, &usart->div);
}

static int hifive_uart_init(void)
{
	hifive_uart_t *usart = (hifive_uart_t *)HIFIVE_UART_BASE_ADDR;

	hifive_uart_setbrg();
	writel(UART_TXEN, &usart->txctrl);
	writel(UART_RXEN, &usart->rxctrl);

    return 0;
}

static void hifive_uart_putc(char ch)
{
	hifive_uart_t *usart = (hifive_uart_t *)HIFIVE_UART_BASE_ADDR;

    if (ch == '\n')
       serial_putc('\r');

	while (readl(&usart->txdata) & UART_TXFIFO_FULL);
	writel(ch, &usart->txdata);
}

static int hifive_uart_getc(void)
{
	hifive_uart_t *usart = (hifive_uart_t *)HIFIVE_UART_BASE_ADDR;
	int ch;
	ch = readl(&usart->rxdata);
	while((ch & UART_RXFIFO_EMPTY) != 0)
	{
		ch = readl(&usart->rxdata);
	}
	return ch;
}

static int hifive_uart_tstc(void)
{
	hifive_uart_t *usart = (hifive_uart_t *)HIFIVE_UART_BASE_ADDR;
	return (readl(&usart->ip) & UART_IP_RXWM);
}

static struct serial_device hifive_uart_drv = {
    .name = "hifive_uart",
    .start = hifive_uart_init,
    .stop = NULL,
    .setbrg = hifive_uart_setbrg,
    .putc = hifive_uart_putc,
    .puts = default_serial_puts,
    .getc = hifive_uart_getc,
    .tstc = hifive_uart_tstc,
};

void hifive_uart_initialize(void)
{
    serial_register(&hifive_uart_drv);
}

struct serial_device *default_serial_console(void)
{
    return &hifive_uart_drv;
}
#endif

#ifdef CONFIG_DM_SERIAL
enum serial_clk_type {
	CLK_TYPE_NORMAL = 0,
	CLK_TYPE_DBGU,
};

struct hifive_serial_priv {
	hifive_uart_t *usart;
	ulong usart_clk_rate;
};

static void _hifive_serial_set_brg(hifive_uart_t *usart,
				  ulong usart_clk_rate, int baudrate)
{
    u32 divisor;
   /*
    * BAUD_VALUE = (CLOCK / BAUD_RATE) - 1
    */
    divisor = (usart_clk_rate / baudrate) -1;

    writel(divisor, &usart->div);

}

void _hifive_serial_init(hifive_uart_t *usart,
			ulong usart_clk_rate, int baudrate)
{

	hifive_uart_setbrg(usart, usart_clk_rate, baudrate);
	writel(UART_TXEN, &usart->txctrl);
	writel(UART_RXEN, &usart->rxctrl);

}

int hifive_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct hifive_serial_priv *priv = dev_get_priv(dev);

	_hifive_serial_set_brg(priv->usart, priv->usart_clk_rate, baudrate);

	return 0;
}

static int hifive_serial_getc(struct udevice *dev)
{
	struct hifive_serial_priv *priv = dev_get_priv(dev);

	int ch;
	ch = readl(&priv->usart->rxdata);

	while((ch & UART_RXFIFO_EMPTY) != 0)
	{
		ch = readl(&priv->usart->rxdata);
	}
	return ch;
}

static int hifive_serial_putc(struct udevice *dev, const char ch)
{
	struct hifive_serial_priv *priv = dev_get_priv(dev);

	while (readl(&priv->usart->txdata) & UART_TXFIFO_FULL);
	writel(ch, &priv->usart->txdata);

	return 0;
}

static int hifive_serial_pending(struct udevice *dev, bool input)
{
	struct hifive_serial_priv *priv = dev_get_priv(dev);
	uint32_t csr = readl(&priv->usart->csr);

	if (input)
		return readl(&priv->usart->rxdata) & UART_RXFIFO_EMPTY ? 0 : 1;
	else
		return readl(&priv->usart->txdata) & UART_TXFIFO_FULL ? 0 : 1;
}

static const struct dm_serial_ops atmel_serial_ops = {
	.putc = hifive_serial_putc,
/*	.pending = hifive_serial_pending,*/
	.getc = hifive_serial_getc,
	.setbrg = hifive_serial_setbrg
};
#if 0 //padma
static int atmel_serial_enable_clk(struct udevice *dev)
{
	struct atmel_serial_priv *priv = dev_get_priv(dev);
	struct clk clk;
	ulong clk_rate;
	int ret;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return -EINVAL;

	if (dev_get_driver_data(dev) == CLK_TYPE_NORMAL) {
		ret = clk_enable(&clk);
		if (ret)
			return ret;
	}

	clk_rate = clk_get_rate(&clk);
	if (!clk_rate)
		return -EINVAL;

	priv->usart_clk_rate = clk_rate;

	clk_free(&clk);

	return 0;
}
#endif
static int hifive_serial_probe(struct udevice *dev)
{
	struct hifive_serial_platdata *plat = dev->platdata;
	struct hifive_serial_priv *priv = dev_get_priv(dev);
	int ret;
#if CONFIG_IS_ENABLED(OF_CONTROL)
	fdt_addr_t addr_base;

	addr_base = devfdt_get_addr(dev);
	if (addr_base == FDT_ADDR_T_NONE)
		return -ENODEV;

	plat->base_addr = (uint32_t)addr_base;
#endif
	priv->usart = (hifive_uart_t *)plat->base_addr;

/*	ret = atmel_serial_enable_clk(dev);
	if (ret)
		return ret;
*/
	_hifive_serial_init(priv->usart, HIFIVE_PERIPH_CLK_FREQ, gd->baudrate);

	return 0;
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static const struct udevice_id hifive_serial_ids[] = {
	{
		.compatible = "sifive,uart0"
	},
	{ }
};
#endif

U_BOOT_DRIVER(serial_hifive) = {
	.name	= "serial_hifive",
	.id	= UCLASS_SERIAL,
#if CONFIG_IS_ENABLED(OF_CONTROL)
	.of_match = hifive_serial_ids,
	.platdata_auto_alloc_size = sizeof(struct hifive_serial_platdata),
#endif
	.probe = hifive_serial_probe,
	.ops	= &hifive_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto_alloc_size	= sizeof(struct hifive_serial_priv),
};
#endif

