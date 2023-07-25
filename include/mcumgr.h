/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MCUMGR_H_
#define _MCUMGR_H_

#include <stddef.h>
#include <zephyr/toolchain.h>

#ifdef CONFIG_BEAGLEPLAY_GREYBUS_MCUMGR
int mcumgr_init();
#else
static int mcumgr_init() {
  return 0;
}
#endif

#ifdef CONFIG_BEAGLEPLAY_GREYBUS_MCUMGR
int mcumgr_process_frame(const void *, size_t);
#else
static int mcumgr_process_frame(const void *buffer, size_t buffer_len) {
  ARG_UNUSED(buffer);
  ARG_UNUSED(buffer_len);
  return 0;
}
#endif

#endif
