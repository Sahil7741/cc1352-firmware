/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _HDLC_H_
#define _HDLC_H_

#include "operations.h"
#include <stdint.h>
#include <zephyr/device.h>

#define HDLC_MAX_BLOCK_SIZE CONFIG_BEAGLEPLAY_HDLC_MAX_BLOCK_SIZE

#define ADDRESS_GREYBUS 0x01
#define ADDRESS_DBG     0x02
#define ADDRESS_MCUMGR  0x03
#define ADDRESS_CONTROL 0x04

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

typedef void (*greybus_message_callback)(struct gb_message *);
typedef int (*hdlc_process_frame_callback)(const void *, size_t, uint8_t);

/*
 * HDLC block
 *
 * @param HDLC address
 * @param HDLC control
 * @param hdlc block buffer length
 * @param hdlc block buffer
 */
struct hdlc_block {
	uint8_t address;
	uint8_t control;
	uint8_t length;
	uint8_t buffer[];
};

/*
 * Initialize internal HDLC stuff
 *
 * @return 0 if successful. Negative in case of error.
 */
int hdlc_init(hdlc_process_frame_callback cb);

/*
 * Submit an HDLC Block syncronously
 *
 * @param buffer
 * @param buffer_length
 * @param address
 * @param control
 *
 * @return block size (> 0) if successful. Negative in case of error
 */
int hdlc_block_send_sync(const uint8_t *buffer, size_t buffer_len, uint8_t address,
			 uint8_t control);

/*
 * Get a buffer to write HDLC message recieved for processing. Make HDLC transport agnostic.
 *
 * @param the pointer to underlying buffer which can be userd to write.
 *
 * @return number of bytes that can be written
 */
uint32_t hdlc_rx_start(uint8_t **buffer);

/*
 * Finish writing to rx buffer. Also queues rx buffer for processing
 *
 * @param number of bytes written
 *
 * @return 0 if successful. Negative in case of error.
 */
int hdlc_rx_finish(uint32_t written);

#endif
