/*
 * Copyright 2017, DornerWorks
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_DORNERWORKS_GPL)
 */

#include <printf.h>
#include <types.h>
#include <platform.h>

/*
 * UART Hardware Constants
 */

#define UART_CTRL         0x18
#define UART_FIFO         0x28
#define UART_STAT         0x14
#define UART_DATA         0x1C

#define TDRE              (1U << 23)      // Transmit Data Register Empty
#define TE                (1U << 19)

#define UART_REG(x) ((volatile uint32_t *)(UART_PPTR + (x)))

int __fputc(int c, FILE *stream)
{
    /* Wait to be able to transmit. */
    while (!(*UART_REG(UART_STAT) & TDRE));

    /* Transmit. */
    *UART_REG(UART_DATA) = c;

    /* Send '\r' after every '\n'. */
    if (c == '\n') {
        (void)__fputc('\r', stream);
    }

    return 0;
}



void enable_uart(void)
{
    uint32_t ctrl = *UART_REG(UART_CTRL);
    ctrl |= TE; // Set Transmitter Enable bit
    *UART_REG(UART_CTRL) = ctrl;
}
