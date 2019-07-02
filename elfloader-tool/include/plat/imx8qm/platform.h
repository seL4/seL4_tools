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


#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <autoconf.h>

/*
 * UART Hardware Constants
 *
 */

#define IMX8_UART5_PADDR   0x5A0A0000
#define IMX8_UART4_PADDR   0x5A090000
#define IMX8_UART3_PADDR   0x5A080000
#define IMX8_UART2_PADDR   0x5A070000
#define IMX8_UART1_PADDR   0x5A060000

#define UART_PPTR          IMX8_UART1_PADDR

#endif /* _PLATFORM_H_ */
