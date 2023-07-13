/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MCUMGR_H_
#define _MCUMGR_H_

#include <stddef.h>

int mcumgr_init();

int mcumgr_process_frame(const void *, size_t);

#endif
