/*
 * This file contains definations of greybus data structures
 */

#ifndef GREYBUS_PROTOCOL_H
#define GREYBUS_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>

/* Version of the Greybus SVC protocol we support */
#define GB_SVC_VERSION_MAJOR 0x00
#define GB_SVC_VERSION_MINOR 0x01

/* Greybus SVC request types */
#define GB_SVC_TYPE_PROTOCOL_VERSION 0x01

/*
 * All operation messages (both requests and responses) begin with
 * a header that encodes the size of the message (header included).
 * This header also contains a unique identifier, that associates a
 * response message with its operation.  The header contains an
 * operation type field, whose interpretation is dependent on what
 * type of protocol is used over the connection.  The high bit
 * (0x80) of the operation type field is used to indicate whether
 * the message is a request (clear) or a response (set).
 *
 * Response messages include an additional result byte, which
 * communicates the result of the corresponding request.  A zero
 * result value means the operation completed successfully.  Any
 * other value indicates an error; in this case, the payload of the
 * response message (if any) is ignored.  The result byte must be
 * zero in the header for a request message.
 *
 * The wire format for all numeric fields in the header is little
 * endian.  Any operation-specific data begins immediately after the
 * header.
 */
struct gb_operation_msg_hdr {
  uint16_t size;  /* Size in bytes of header + payload */
  uint16_t id;    /* Operation unique id */
  uint8_t type;   /* E.g GB_I2C_TYPE_TRANSFER */
  uint8_t status; /* Result of request (in responses only) */
  uint8_t pad[2]; /* must be zero (ignore when read) */
} __packed;

enum gb_operation_type {
  GB_TYPE_RESPONSE_FLAG = 0x80,
};

enum gb_operation_result {
  GB_OP_SUCCESS = 0x00,
  GB_OP_INTERRUPTED = 0x01,
  GB_OP_TIMEOUT = 0x02,
  GB_OP_NO_MEMORY = 0x03,
  GB_OP_PROTOCOL_BAD = 0x04,
  GB_OP_OVERFLOW = 0x05,
  GB_OP_INVALID = 0x06,
  GB_OP_RETRY = 0x07,
  GB_OP_NONEXISTENT = 0x08,
  GB_OP_INVALID_STATE = 0x09,
  GB_OP_UNKNOWN_ERROR = 0xfe,
  GB_OP_INTERNAL = 0xff,
};

struct gb_message {
  struct gb_operation *operation;
  struct gb_operation_msg_hdr header;
  void *payload;
  size_t payload_size;
};

struct gb_operation {
  int sock;
  uint16_t operation_id;
  bool request_sent;
  struct gb_message *request;
  struct gb_message *response;
  sys_dnode_t node;
};

/*
 * Check if the greybus operation is unidirectional.
 * These messages will not have a response and thus should be freed once request
 * is sent.
 *
 * @param op: greybus operation
 *
 * @return true if operation is unidirectional, else false.
 */
static inline bool is_operation_unidirectional(struct gb_operation *op) {
  return op->operation_id == 0;
}

/*
 * Check if the greybus message is a response.
 *
 * @param msg: greybus message
 *
 * @return true if message is response, else false.
 */
static inline bool is_message_response(struct gb_message *msg) {
  return msg->header.type & GB_TYPE_RESPONSE_FLAG;
}

/*
 * Allocate a greybus operation.
 *
 * Note: This does not allocate a request or a response.
 *
 * @param sock: The socket for this operation
 * @param is_oneshot: flag to indicate if the request is unidirectional.
 *
 * @return heap allocated gb_operation
 */
struct gb_operation *greybus_alloc_operation(int, bool);

/*
 * Allocate a request on a pre-allocated greybus operation
 *
 * @param op: pointer to greybus operation
 * @param data: pointer to payload for request
 * @param payload_len: size of payload
 * @param type: type of greybus operation
 *
 * @return 0 on success, else error
 */
int greybus_alloc_request(struct gb_operation *, const void *, size_t, uint8_t);

/*
 * Deallocate greybus operation. It recursively deallocates any allocated
 * request and response.
 */
void greybus_dealloc_operation(struct gb_operation *);

/*
 * Add greybus operation to the list of in-flight operations.
 *
 * Note: This is not thread safe.
 */
void greybus_operation_ready(struct gb_operation *, sys_dlist_t *);

/*
 * Send a greybus message. This only works for messages associated with a
 * greybus operation for now.
 */
int greybus_send_message(const struct gb_message *);

/*
 * Receive a greybus message over a socket.
 *
 * @param sock: Greybus communication socket.
 *
 * @return a heap allocated greybus message
 */
struct gb_message *greybus_recieve_message(int);

#endif
