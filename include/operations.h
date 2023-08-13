/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _OPERATIONS_H_
#define _OPERATIONS_H_

#include "greybus_protocol.h"
#include <stdbool.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/dlist.h>

struct gb_controller;
struct gb_connection;

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
 * Callback to process an active greybus connection
 *
 * @param active greybus connection
 */
typedef void (*gb_connection_callback)(struct gb_connection *);

/*
 * Struct to represent greybus message. This is a variable sized type.
 *
 * @param fifo_reserved: reserved for fifo
 * @param header: greybus msg header.
 * @param payload_size: size of payload in bytes
 * @param payload: heap allocated payload.
 */
struct gb_message {
	void *fifo_reserved;
	struct gb_operation_msg_hdr header;
	size_t payload_size;
	uint8_t payload[];
};

/*
 * Controller for each greybus interface
 *
 * @param read: a non-blocking read function
 * @param write: a non-blocking write function. The ownership of message is
 * trasnferred.
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
 * A connection between two greybus interfaces
 *
 * @param inf_ap: Greybus interface of AP.
 * @param inf_peer: Greybus interface of the peer
 * @param ap_cport_id: Cport of AP to connect to.
 * @param peer_cport_id: Cport of Peer to connect to.
 */
struct gb_connection {
	struct gb_interface *inf_ap;
	struct gb_interface *inf_peer;
	uint16_t ap_cport_id;
	uint16_t peer_cport_id;
	sys_dnode_t node;
};

/*
 * Check if the greybus message header is a response.
 *
 * @param hdr: greybus header
 *
 * @return true if message is response, else false.
 */
static inline bool gb_hdr_is_response(const struct gb_operation_msg_hdr *hdr)
{
	return hdr->type & GB_TYPE_RESPONSE_FLAG;
}

/*
 * Check if the greybus message header is a successful.
 *
 * @param hdr: greybus header
 *
 * @return true if message is successful, else false.
 */
static inline bool gb_hdr_is_success(const struct gb_operation_msg_hdr *hdr)
{
	return hdr->status == 0x00;
}

/*
 * Check if the greybus message is a response.
 *
 * @param msg: greybus message
 *
 * @return true if message is response, else false.
 */
static inline bool gb_message_is_response(const struct gb_message *msg)
{
	return gb_hdr_is_response(&msg->header);
}

/*
 * Check if the greybus message is a successful.
 *
 * @param msg: greybus message
 *
 * @return true if message is successful, else false.
 */
static inline bool gb_message_is_success(const struct gb_message *msg)
{
	return gb_hdr_is_success(&msg->header);
}

/*
 * Create a greybus connection between two interfaces
 *
 * @param Greybus AP Interface
 * @param Greybus Peer Interface
 * @param Greybus AP Interface Cport ID
 * @param Greybus Peer Interface Cport ID
 *
 * @return greybus connection allocated on heap. Null in case of errro
 */
struct gb_connection *gb_create_connection(struct gb_interface *intf1, struct gb_interface *intf2,
					   uint16_t intf1_cport_id, uint16_t intf2_cport_id);

/*
 * Destroy greybus connection
 *
 * @param interface 1
 * @param interface 2
 * @param interface 1 cport
 * @param interface 2 cport
 *
 * @return 0 on success. Negative in case of error
 */
int gb_destroy_connection(struct gb_interface *intf1, struct gb_interface *intf2,
			  uint16_t intf1_cport_id, uint16_t intf2_cport_id);

/*
 * Allocate Greybus message
 *
 * @param Payload len
 * @param Response Type
 * @param Operation ID of Request
 * @param Status
 *
 * @return greybus message allocated on heap. Null in case of errro
 */
struct gb_message *gb_message_alloc(size_t payload_len, uint8_t message_type, uint16_t operation_id,
				    uint8_t status);

/*
 * Deallocate a greybus message.
 *
 * @param pointer to the message to deallcate
 */
void gb_message_dealloc(struct gb_message *msg);

/*
 * Allocate a greybus request message
 *
 * @param Payload
 * @param Payload len
 * @param Request Type
 * @param Is one shot
 *
 * @return greybus message allocated on heap. Null in case of errro
 */
struct gb_message *gb_message_request_alloc(const void *payload, size_t payload_len,
					    uint8_t request_type, bool is_oneshot);

/*
 * Allocate a greybus response message
 *
 * @param Payload
 * @param Payload len
 * @param Request Type
 * @param Operation ID of Request
 * @param Status
 *
 * @return greybus message allocated on heap. Null in case of errro
 */
static inline struct gb_message *gb_message_response_alloc(const void *payload, size_t payload_len,
							   uint8_t request_type,
							   uint16_t operation_id, uint8_t status)
{
	struct gb_message *msg =
		gb_message_alloc(payload_len, OP_RESPONSE | request_type, operation_id, status);
	memcpy(msg->payload, payload, payload_len);
	return msg;
}

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
struct gb_interface *find_interface_by_id(uint8_t intf_id);

/*
 * Execute a function on all active connections
 */
void gb_connection_process_all();

/*
 * This removes all the connections associated with an interface before deallocting it.
 *
 * @param interface
 */
void gb_interface_destroy(struct gb_interface *intf);

/*
 * Remove all greybus connections
 *
 * Note: This does not remove the interfaces
 */
void gb_connection_destroy_all(void);

/*
 * Process a Greybus connection. This means passing messages between the 2 interfaces
 *
 * @param greybus connection
 *
 * @return Number of messages exchanged
 */
static inline uint8_t gb_connection_process(struct gb_connection *conn)
{
	uint8_t count = 0;
	struct gb_message *msg;

	msg = conn->inf_ap->controller.read(&conn->inf_ap->controller, conn->ap_cport_id);
	if (msg) {
		conn->inf_peer->controller.write(&conn->inf_peer->controller, msg,
						 conn->peer_cport_id);
		count++;
	}

	msg = conn->inf_peer->controller.read(&conn->inf_peer->controller, conn->peer_cport_id);
	if (msg) {
		conn->inf_ap->controller.write(&conn->inf_ap->controller, msg, conn->ap_cport_id);
		count++;
	}

	return count;
}

#endif
