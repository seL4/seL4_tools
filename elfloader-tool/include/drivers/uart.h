/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <autoconf.h>
#include <drivers/common.h>

#define dev_get_uart(dev) ((struct elfloader_uart_ops *)(dev->drv->ops))

struct elfloader_uart_ops {
    int (*putc)(struct elfloader_device *dev, unsigned int c);
};

volatile void *uart_get_mmio(void);
void uart_set_out(struct elfloader_device *out);
#if defined(CONFIG_ARCH_AARCH64)
/* Implemented in mmu.c */
void mmu_set_uart_base(volatile void *base);
#endif
