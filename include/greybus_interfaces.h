/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _OPERATIONS_H_
#define _OPERATIONS_H_

#include "greybus_protocol.h"
#include <zephyr/sys/dlist.h>

struct gb_controller;

/*
 * Callabck for reading from an interface
 *
 * @param controller
 * @param Cport to read from
 *
 * @return greybus message if available. Else NULL
 */
typedef struct gb_message *(*gb_controller_read_callback_t)(struct gb_controller *, uint16_t);

/*
 * Callback for writing to an interface
 *
 * @param controller
 * @param greybus message to send
 * @param Cport to write to
 *
 * @return 0 if successful. Negative in case of error
 */
typedef int (*gb_controller_write_callback_t)(struct gb_controller *, struct gb_message *,
					      uint16_t);

/*
 * Callback to create new connection with a Cport in the interface
 *
 * @param controller
 * @param cport
 *
 * @return 0 if successful. Negative in case of error
 */
typedef int (*gb_controller_create_connection_t)(struct gb_controller *, uint16_t);

/*
 * Callback to destroy connection with a Cport in the interface
 *
 * @param controller
 * @param cport
 */
typedef void (*gb_controller_destroy_connection_t)(struct gb_controller *, uint16_t);

/*
 * Controller for each greybus interface
 *
 * @param read: a non-blocking read function
 * @param write: a non-blocking write function. The ownership of message is
 * transferred.
 * @param ctrl_data: private controller data
 */
struct gb_controller {
	gb_controller_read_callback_t read;
	gb_controller_write_callback_t write;
	gb_controller_create_connection_t create_connection;
	gb_controller_destroy_connection_t destroy_connection;
	void *ctrl_data;
};

/*
 * A greybus interface. Can have multiple Cports
 *
 * @param id: Interface ID
 * @param controller: A controller which provides operations for this interface
 */
struct gb_interface {
	uint8_t id;
	struct gb_controller controller;
	sys_dnode_t node;
};

/*
 * Allocate a greybus interface
 *
 * @param read callback
 * @param write callback
 * @param create connection callback
 * @param destroy connection callback
 * @param controller data
 *
 * @return allocated greybus interface. NULL in case of error
 */
struct gb_interface *gb_interface_alloc(gb_controller_read_callback_t read_cb,
					gb_controller_write_callback_t write_cb,
					gb_controller_create_connection_t create_connection_cb,
					gb_controller_destroy_connection_t destroy_connection_cb,
					void *ctrl_data);

/*
 * Deallocate a greybus interface
 *
 * @param greybus interface
 */
void gb_interface_dealloc(struct gb_interface *intf);

/*
 * Get interface associated with interface id.
 *
 * @param interface id
 *
 * @param greybus interface
 */
struct gb_interface *gb_interface_find_by_id(uint8_t intf_id);

#endif
