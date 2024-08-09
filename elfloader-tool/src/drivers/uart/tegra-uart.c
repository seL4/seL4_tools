/*
 * Copyright 2023， NIO
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <devices_gen.h>
#include <drivers/common.h>
#include <drivers/uart.h>
#include <elfloader_common.h>

#define NUM_BYTES_FIELD_BIT     (24U)
#define FLUSH_BIT               (26U)
#define INTR_TRIGGER_BIT        (31U)
#define UART_REG(mmio, x)       ((volatile uint32_t *)(mmio + (x)))

static int tegra_uart_putchar(struct elfloader_device *dev, unsigned int c)
{
    uint32_t reg_val;

    reg_val = (uint32_t)(1UL << NUM_BYTES_FIELD_BIT);
    reg_val |= BIT(INTR_TRIGGER_BIT);
    reg_val |= c;

    if (c == '\r' || c == '\n') {
        reg_val |= BIT(FLUSH_BIT);
    }

    while (*UART_REG(dev->region_bases[0], 0) & BIT(INTR_TRIGGER_BIT));

    *UART_REG(dev->region_bases[0], 0) = reg_val;
    return 0;
}

static int tegra_uart_init(struct elfloader_device *dev, UNUSED void *match_data)
{
    uart_set_out(dev);
    *UART_REG(dev->region_bases[0], 0) = 0;

    return 0;
}

static const struct dtb_match_table tegra_uart_matches[] = {
    { .compatible = "nvidia,tegra194-tcu" },
    { .compatible = NULL /* sentinel */ },
};

static const struct elfloader_uart_ops tegra_uart_ops = {
    .putc = &tegra_uart_putchar,
};

static const struct elfloader_driver tegra_uart = {
    .match_table = tegra_uart_matches,
    .type = DRIVER_UART,
    .init = &tegra_uart_init,
    .ops = &tegra_uart_ops,
};

ELFLOADER_DRIVER(tegra_uart);
