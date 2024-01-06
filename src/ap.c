// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "ap.h"
#include "hdlc.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

void ap_init(void)
{
}

void ap_deinit(void)
{
}

int ap_send(struct gb_message *msg, uint16_t cport) {
	int ret = gb_message_hdlc_send(msg, cport);
	gb_message_dealloc(msg);

	return ret;
}
