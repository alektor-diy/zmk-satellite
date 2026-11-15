/*
 * SPDX-License-Identifier: MIT
 * Derived from Prospector ZMK Module v2.2.3; original notice in
 * licenses/Prospector-Module.txt. Locally maintained Zephyr compatibility.
 *
 * Provides compatibility macros for building with both
 * Zephyr 3.5 (ZMK 0.3) and Zephyr 4.x (ZMK main).
 */

#ifndef ZMK_PROSPECTOR_COMPAT_H
#define ZMK_PROSPECTOR_COMPAT_H

#include <version.h>

/* Zephyr 3.5+ SYS_INIT uses int (*)(void). */
#if KERNEL_VERSION_MAJOR >= 4 || (KERNEL_VERSION_MAJOR == 3 && KERNEL_VERSION_MINOR >= 5)
#define PROSPECTOR_SYS_INIT_ARGS    void
#define PROSPECTOR_SYS_INIT_UNUSED  /* nothing */
#else
#define PROSPECTOR_SYS_INIT_ARGS    const struct device *dev
#define PROSPECTOR_SYS_INIT_UNUSED  ARG_UNUSED(dev)
#endif

#endif /* ZMK_PROSPECTOR_COMPAT_H */
