/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <devices_gen.h>
#include <drivers/common.h>
#include <drivers/uart.h>

#include <elfloader_common.h>

#define UART_REG_DAT 0x00
#define UART_REG_IER 0x01
#define UART_REG_IIR 0x02
#define UART_REG_FCR 0x02
#define UART_REG_LCR 0x03
#define UART_REG_MCR 0x04
#define UART_REG_LSR 0x05
#define UART_REG_MSR 0x06

#define UART_REG_LSR_TE BIT(6)
#define UART_REG_LSR_TFE BIT(5)

#define UART_REG(mmio, x) ((volatile uint8_t *)(((unsigned long)(mmio) + (x)) | 0x6000000000000000))

static int loongson3A5000_uart_putchar(struct elfloader_device *dev, unsigned int c)
{
    volatile void *mmio = dev->region_bases[0];
    while (!(*UART_REG(mmio, UART_REG_LSR) & UART_REG_LSR_TE));
    asm volatile(
        "csrwr  $t0, 0x34  \n"
        "csrwr  $t1, 0x35  \n"
        "li.d  $t1, 0x1fe001e5  \n"
        "iocsrrd.b $t0, $t1  \n"
        "csrrd  $t0, 0x34  \n"
        "csrrd  $t1, 0x35  \n");
    *UART_REG(mmio, UART_REG_DAT) = (c & 0xff);

    return 0;
}

static int loongson3A5000_uart_init(struct elfloader_device *dev, UNUSED void *match_data)
{
    uart_set_out(dev);
    return 0;
}

static const struct dtb_match_table loongson3A5000_uart_matches[] = {
    { .compatible = "3A5000,loongson3A5000-uart" },
    { .compatible = NULL /* sentinel */ },
};

static const struct elfloader_uart_ops loongson3A5000_uart_ops = {
    .putc = &loongson3A5000_uart_putchar,
};

static const struct elfloader_driver loongson3A5000_uart = {
    .match_table = loongson3A5000_uart_matches,
    .type = DRIVER_UART,
    .init = &loongson3A5000_uart_init,
    .ops = &loongson3A5000_uart_ops,
};

ELFLOADER_DRIVER(loongson3A5000_uart);
