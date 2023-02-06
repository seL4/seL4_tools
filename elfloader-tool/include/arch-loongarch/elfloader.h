/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * qtliu@mail.ustc.edu.cn, le24@mail.ustc.edu.cn, chenyangcs@mail.ustc.edu.cn
 * 
 * Derived from:
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include <autoconf.h>
#include <elfloader_common.h>

/* This is a low level binary interface, thus we do not preserve the type
 * information here. All parameters are just register values (or stack values
 * that are register-sized).
 */
typedef void (*init_loongarch_kernel_t)(word_t ui_p_reg_start,
                                    word_t ui_p_reg_end,
                                    word_t pv_offset,
                                    word_t v_entry,
                                    word_t dtb_addr_p,
                                    word_t dtb_size
#if CONFIG_MAX_NUM_NODES > 1
                                    ,
                                    word_t hart_id,
                                    word_t core_id
#endif
                                   );
