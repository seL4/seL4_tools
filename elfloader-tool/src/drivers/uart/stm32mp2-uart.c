/*
 * Copyright 2026, STMicroelectronics
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <devices_gen.h>
#include <drivers/common.h>
#include <drivers/uart.h>

#include <elfloader_common.h>

#define UART_REG(mmio, x) ((volatile uint32_t *)(mmio + (x)))

#define USART_CR1               0x00
#define USART_CR2               0x04
#define USART_ISR               0x1C
#define USART_TDR               0x28

/* USART_CR1 register fields */
#define USART_CR1_UE            1
#define USART_CR1_TE            8
#define USART_CR1_FIFOEN        0x20000000U

/* USART_CR2 register fields */
#define USART_CR2_STOP          0x3000U

/* USART_ISR register fields */
#define USART_ISR_TXE           0x80U

static int stm32mp2_uart_putchar(struct elfloader_device *dev, unsigned int c)
{
    volatile void *mmio = dev->region_bases[0];

    /* Wait to be able to transmit. */
    while (!(*UART_REG(mmio, USART_ISR) & USART_ISR_TXE));

    /* Transmit. */
    *UART_REG(mmio, USART_TDR) = c;

    return 0;
}

static int stm32mp2_uart_init(struct elfloader_device *dev, UNUSED void *match_data)
{
    volatile void *mmio = dev->region_bases[0];
    uint32_t v;

    /* Disable UART */
    v = *UART_REG(mmio, USART_CR1);
    v &= ~USART_CR1_UE;
    *UART_REG(mmio, USART_CR1) = v;

    /* Configure UART */
    v |= (USART_CR1_TE | USART_CR1_FIFOEN);
    *UART_REG(mmio, USART_CR1) = v;

    v = *UART_REG(mmio, USART_CR2);
    v &= ~USART_CR2_STOP;
    *UART_REG(mmio, USART_CR2) = v;

    /* Enable UART */
    v = *UART_REG(mmio, USART_CR1);
    v |= USART_CR1_UE;
    *UART_REG(mmio, USART_CR1) = v;

    uart_set_out(dev);

    return 0;
}

static const struct dtb_match_table stm32mp2_uart_matches[] = {
    { .compatible = "st,stm32h7-uart" },
    { .compatible = NULL /* sentinel */ },
};

static const struct elfloader_uart_ops stm32mp2_uart_ops = {
    .putc = &stm32mp2_uart_putchar,
};

static const struct elfloader_driver stm32mp2_uart = {
    .match_table = stm32mp2_uart_matches,
    .type = DRIVER_UART,
    .init = &stm32mp2_uart_init,
    .ops = &stm32mp2_uart_ops,
};

ELFLOADER_DRIVER(stm32mp2_uart);
