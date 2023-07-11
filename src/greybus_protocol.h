/*
 * This file contains definations of greybus data structures
 */

#ifndef GREYBUS_PROTOCOL_H
#define GREYBUS_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

/* Version of the Greybus SVC protocol we support */
#define GB_SVC_VERSION_MAJOR 0x00
#define GB_SVC_VERSION_MINOR 0x01


/* Greybus SVC request types */
#define GB_SVC_TYPE_PROTOCOL_VERSION_REQUEST 0x01
#define GB_SVC_TYPE_PROTOCOL_VERSION_RESPONSE 0x81
#define GB_SVC_TYPE_PING_REQUEST 0x13
#define GB_SVC_TYPE_PING_RESPONSE 0x93

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
};

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

#endif
