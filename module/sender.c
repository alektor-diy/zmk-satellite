/* Copyright (c) 2026 ZMK Satellite contributors
 * SPDX-License-Identifier: MIT
 *
 * Compile the pinned Prospector sender with small compatibility adaptations.
 * The generated upstream source retains its own copyright and MIT notice.
 */
#include <zephyr/kernel.h>
#include <zmk/prospector_compat.h>

/* SYS_INIT already takes void in ZMK's Zephyr 3.5, not only in Zephyr 4.x. */
#if KERNEL_VERSION_MAJOR >= 4 || (KERNEL_VERSION_MAJOR == 3 && KERNEL_VERSION_MINOR >= 5)
#undef PROSPECTOR_SYS_INIT_ARGS
#undef PROSPECTOR_SYS_INIT_UNUSED
#define PROSPECTOR_SYS_INIT_ARGS void
#define PROSPECTOR_SYS_INIT_UNUSED
#endif

#ifdef ZMK_SATELLITE_RESOLVED_SIDE
#undef CONFIG_ZMK_STATUS_ADV_CENTRAL_SIDE
#define CONFIG_ZMK_STATUS_ADV_CENTRAL_SIDE ZMK_SATELLITE_RESOLVED_SIDE
#endif

#include ZMK_SATELLITE_UPSTREAM_SOURCE
