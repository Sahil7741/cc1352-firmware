/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _MCUMGR_H_
#define _MCUMGR_H_

#include <stddef.h>
#include <zephyr/toolchain.h>

#ifdef CONFIG_BEAGLEPLAY_GREYBUS_MCUMGR
int mcumgr_init(void);
#else
static int mcumgr_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_BEAGLEPLAY_GREYBUS_MCUMGR
int mcumgr_process_frame(const void *buffer, size_t buffer_len);
#else
static int mcumgr_process_frame(const void *buffer, size_t buffer_len)
{
	ARG_UNUSED(buffer);
	ARG_UNUSED(buffer_len);
	return 0;
}
#endif

#endif
