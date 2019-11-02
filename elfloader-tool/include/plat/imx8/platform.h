/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#include <autoconf.h>
#include <elfloader/gen_config.h>

/*
 * UART Hardware Constants
 *
 */

#if defined(CONFIG_PLAT_IMX8MM_EVK)
    #define IMX8_UART1_PADDR   0x30890000
#elif defined(CONFIG_PLAT_IMX8MQ_EVK)
    #define IMX8_UART1_PADDR   0x30860000
#else
    #error "unusupported IMX8 platform"
#endif


#define UART_PPTR          IMX8_UART1_PADDR

