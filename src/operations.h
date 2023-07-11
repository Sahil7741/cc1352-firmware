/*
 * This file contains functions to manage greybus operations queue.
 * This allows asynchronous processing of greybus operations.
 * This API is thread-safe
 */

#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "greybus_protocol.h"
#include <stdbool.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/dlist.h>

/* Return codes for the functions defined here */
#define SUCCESS 0
#define E_NULL_REQUEST 1
#define E_ALREADY_SENT 2

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
  struct gb_message *(*read)(struct gb_controller *, uint16_t);
  int (*write)(struct gb_controller *, struct gb_message *, uint16_t);
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
static inline bool gb_hdr_is_response(const struct gb_operation_msg_hdr *hdr) {
  return hdr->type & GB_TYPE_RESPONSE_FLAG;
}

/*
 * Check if the greybus message header is a successful.
 *
 * @param hdr: greybus header
 *
 * @return true if message is successful, else false.
 */
static inline bool gb_hdr_is_success(const struct gb_operation_msg_hdr *hdr) {
  return hdr->status == 0x00;
}

/*
 * Check if the greybus message is a response.
 *
 * @param msg: greybus message
 *
 * @return true if message is response, else false.
 */
static inline bool gb_message_is_response(const struct gb_message *msg) {
  return gb_hdr_is_response(&msg->header);
}

/*
 * Check if the greybus message is a successful.
 *
 * @param msg: greybus message
 *
 * @return true if message is successful, else false.
 */
static inline bool gb_message_is_success(const struct gb_message *msg) {
  return gb_hdr_is_success(&msg->header);
}

/*
 * Deallocate a greybus message.
 *
 * @param pointer to the message to deallcate
 */
void gb_message_dealloc(struct gb_message *);

/*
 * Send a greybus message over HDLC
 *
 * @param Greybus message
 */
int gb_message_hdlc_send(const struct gb_message *);

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
struct gb_connection *gb_create_connection(struct gb_interface *,
                                           struct gb_interface *, uint16_t,
                                           uint16_t);

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
struct gb_message *gb_message_request_alloc(const void *, size_t, uint8_t,
                                            bool);

/*
 * Allocate a greybus response message
 *
 * @param Payload
 * @param Payload len
 * @param Request Type
 * @param Operation ID
 *
 * @return greybus message allocated on heap. Null in case of errro
 */
struct gb_message *gb_message_response_alloc(const void *, size_t, uint8_t,
                                             uint16_t);

#endif
