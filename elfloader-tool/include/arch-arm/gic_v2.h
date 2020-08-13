/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

/* Distributor Registers Offset */
#define GICD_CTLR          0x0
#define GICD_TYPER         0x4
#define GICD_IGROUPR(n)    (0x80 + n * 4)
#define GICD_IPRIORITYR(n) (0x400 + n * 4)

/* CPU Interface Registers Offset */
#define GICC_CTLR 0x0
#define GICC_PMR  0x4
