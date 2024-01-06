// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "hdlc.h"
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_backend_std.h>

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define BUFFER_LEN       200

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
static uint8_t hdlc_uart_buffer[BUFFER_LEN];

static int hdlc_uart_out(uint8_t *data, size_t length, void *ctx)
{
	ARG_UNUSED(ctx);

	hdlc_block_send_sync(data, length, ADDRESS_DBG, 0x03);
	return length;
}

LOG_OUTPUT_DEFINE(hdlc_uart_output, hdlc_uart_out, hdlc_uart_buffer, BUFFER_LEN);

static void hdlc_uart_backend_process(const struct log_backend *const backend,
				      union log_msg_generic *msg)
{
	ARG_UNUSED(backend);

	uint32_t flags = log_backend_std_get_flags();

	log_output_msg_process(&hdlc_uart_output, &msg->log, flags);
}

static void hdlc_uart_backend_dropped(const struct log_backend *const backend, uint32_t cnt)
{
	ARG_UNUSED(backend);

	log_backend_std_dropped(&hdlc_uart_output, cnt);
}

static void hdlc_uart_backend_panic(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);

	log_backend_std_panic(&hdlc_uart_output);
}

static void hdlc_uart_backend_init(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);
}

static int hdlc_uart_backend_is_ready(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);

	if (device_is_ready(uart_dev)) {
		return 0;
	} else {
		return -EBUSY;
	}
}

static int hdlc_uart_backend_format_set(const struct log_backend *const backend, uint32_t log_type)
{
	ARG_UNUSED(backend);
	ARG_UNUSED(log_type);

	return 0;
}

static void hdlc_uart_backend_notify(const struct log_backend *const backend,
				     enum log_backend_evt event, union log_backend_evt_arg *arg)
{
	ARG_UNUSED(backend);
	ARG_UNUSED(event);
	ARG_UNUSED(arg);
}

const struct log_backend_api hdlc_uart_backend_api = {.process = hdlc_uart_backend_process,
						      .dropped = hdlc_uart_backend_dropped,
						      .panic = hdlc_uart_backend_panic,
						      .init = hdlc_uart_backend_init,
						      .is_ready = hdlc_uart_backend_is_ready,
						      .format_set = hdlc_uart_backend_format_set,
						      .notify = hdlc_uart_backend_notify};

LOG_BACKEND_DEFINE(hdlc_uart_backend, hdlc_uart_backend_api, true);
