/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>
#include <printf.h>
#include <psci.h>

#ifdef CONFIG_ARCH_AARCH64
#define SMC_FID_VER           0x84000000
#define SMC_FID_CPU_SUSPEND   0xc4000001
#define SMC_FID_CPU_OFF       0x84000002
#define SMC_FID_CPU_ON        0xc4000003
#define SMC_FID_SYSTEM_RESET  0x84000009
#else
#define SMC_FID_VER           0x80000000
#define SMC_FID_CPU_SUSPEND   0x80000001
#define SMC_FID_CPU_OFF       0x80000002
#define SMC_FID_CPU_ON        0x80000003
#define SMC_FID_SYSTEM_RESET  0x80000009
#endif


extern int psci_smc_func(unsigned int id, unsigned long param1,
                         unsigned long param2, unsigned long param3);

extern int psci_hvc_func(unsigned int id, unsigned long param1,
                         unsigned long param2, unsigned long param3);

int psci_func(unsigned int method, unsigned int id, unsigned long param1,
              unsigned long param2, unsigned long param3)
{
    if (method == PSCI_METHOD_HVC) {
        return psci_hvc_func(id, param1, param2, param3);
    } else if (method == PSCI_METHOD_SMC) {
        return psci_smc_func(id, param1, param2, param3);
    } else {
        printf("ERROR: PSCI method %u is unsupported\n", method);
        return -1;
    }
}

int psci_version(unsigned int method)
{
    int ver = psci_func(method, SMC_FID_VER, 0, 0, 0);
    return ver;
}


int psci_cpu_suspend(unsigned int method, int power_state, unsigned long entry_point,
                     unsigned long context_id)
{
    int ret = psci_func(method, SMC_FID_CPU_SUSPEND, power_state, entry_point, context_id);
    return ret;
}

/* this function does not return when successful */
int psci_cpu_off(unsigned int method)
{
    int ret = psci_func(method, SMC_FID_CPU_OFF, 0, 0, 0);
    return ret;
}

int psci_cpu_on(unsigned int method, unsigned long target_cpu, unsigned long entry_point,
                unsigned long context_id)
{
    int ret = psci_func(method, SMC_FID_CPU_ON, target_cpu, entry_point, context_id);
    return ret;
}

int psci_system_reset(unsigned int method)
{
    int ret = psci_func(method, SMC_FID_SYSTEM_RESET, 0, 0, 0);
    return ret;
}

