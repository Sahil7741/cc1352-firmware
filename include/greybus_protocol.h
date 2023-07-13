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

#define OP_RESPONSE 0x80

/* Greybus SVC request types */
#define GB_SVC_TYPE_PROTOCOL_VERSION_REQUEST      0x01
#define GB_SVC_TYPE_PROTOCOL_VERSION_RESPONSE     OP_RESPONSE | GB_SVC_TYPE_PROTOCOL_VERSION_REQUEST
#define GB_SVC_TYPE_HELLO_REQUEST                 0x02
#define GB_SVC_TYPE_HELLO_RESPONSE                OP_RESPONSE | GB_SVC_TYPE_HELLO_REQUEST
#define GB_SVC_TYPE_INTF_DEVICE_ID_REQUEST        0x03
#define GB_SVC_TYPE_INTF_DEVICE_ID_RESPONSE       OP_RESPONSE | GB_SVC_TYPE_INTF_DEVICE_ID_REQUEST
#define GB_SVC_TYPE_CONN_CREATE_REQUEST           0x07
#define GB_SVC_TYPE_CONN_CREATE_RESPONSE          OP_RESPONSE | GB_SVC_TYPE_CONN_CREATE_REQUEST
#define GB_SVC_TYPE_CONN_DESTROY_REQUEST          0x08
#define GB_SVC_TYPE_CONN_DESTROY_RESPONSE         OP_RESPONSE | GB_SVC_TYPE_CONN_DESTROY_REQUEST
#define GB_SVC_TYPE_DME_PEER_GET_REQUEST          0x09
#define GB_SVC_TYPE_DME_PEER_GET_RESPONSE         OP_RESPONSE | GB_SVC_TYPE_DME_PEER_GET_REQUEST
#define GB_SVC_TYPE_DME_PEER_SET_REQUEST          0x0a
#define GB_SVC_TYPE_DME_PEER_SET_RESPONSE         OP_RESPONSE | GB_SVC_TYPE_DME_PEER_SET_REQUEST
#define GB_SVC_TYPE_ROUTE_CREATE_REQUEST          0x0b
#define GB_SVC_TYPE_ROUTE_CREATE_RESPONSE         OP_RESPONSE | GB_SVC_TYPE_ROUTE_CREATE_REQUEST
#define GB_SVC_TYPE_ROUTE_DESTROY_REQUEST         0x0c
#define GB_SVC_TYPE_ROUTE_DESTROY_RESPONSE        OP_RESPONSE | GB_SVC_TYPE_ROUTE_DESTROY_REQUEST
#define GB_SVC_TYPE_INTF_SET_PWRM_REQUEST         0x10
#define GB_SVC_TYPE_INTF_SET_PWRM_RESPONSE        OP_RESPONSE | GB_SVC_TYPE_INTF_SET_PWRM_REQUEST
#define GB_SVC_TYPE_PING_REQUEST                  0x13
#define GB_SVC_TYPE_PING_RESPONSE                 OP_RESPONSE | GB_SVC_TYPE_PING_REQUEST
#define GB_SVC_TYPE_PWRMON_RAIL_COUNT_GET_REQUEST 0x14
#define GB_SVC_TYPE_PWRMON_RAIL_COUNT_GET_RESPONSE                                                 \
	OP_RESPONSE | GB_SVC_TYPE_PWRMON_RAIL_COUNT_GET_REQUEST
#define GB_SVC_TYPE_MODULE_INSERTED_REQUEST      0x1f
#define GB_SVC_TYPE_MODULE_INSERTED_RESPONSE     OP_RESPONSE | GB_SVC_TYPE_MODULE_INSERTED_REQUEST
#define GB_SVC_TYPE_INTF_VSYS_ENABLE_REQUEST     0x21
#define GB_SVC_TYPE_INTF_VSYS_ENABLE_RESPONSE    OP_RESPONSE | GB_SVC_TYPE_INTF_VSYS_ENABLE_REQUEST
#define GB_SVC_TYPE_INTF_VSYS_DISABLE_REQUEST    0x22
#define GB_SVC_TYPE_INTF_VSYS_DISABLE_RESPONSE   OP_RESPONSE | GB_SVC_TYPE_INTF_VSYS_DISABLE_REQUEST
#define GB_SVC_TYPE_INTF_REFCLK_ENABLE_REQUEST   0x23
#define GB_SVC_TYPE_INTF_REFCLK_ENABLE_RESPONSE  OP_RESPONSE | GB_SVC_TYPE_INTF_REFCLK_ENABLE_REQUEST
#define GB_SVC_TYPE_INTF_REFCLK_DISABLE_REQUEST  0x24
#define GB_SVC_TYPE_INTF_REFCLK_DISABLE_RESPONSE OP_RESPONSE | GB_SVC_TYPE_INTF_VSYS_DISABLE_REQUEST
#define GB_SVC_TYPE_INTF_UNIPRO_ENABLE_REQUEST   0x25
#define GB_SVC_TYPE_INTF_UNIPRO_ENABLE_RESPONSE  OP_RESPONSE | GB_SVC_TYPE_INTF_UNIPRO_ENABLE_REQUEST
#define GB_SVC_TYPE_INTF_UNIPRO_DISABLE_REQUEST  0x26
#define GB_SVC_TYPE_INTF_UNIPRO_DISABLE_RESPONSE                                                   \
	OP_RESPONSE | GB_SVC_TYPE_INTF_UNIPRO_DISABLE_REQUEST
#define GB_SVC_TYPE_INTF_ACTIVATE_REQUEST  0x27
#define GB_SVC_TYPE_INTF_ACTIVATE_RESPONSE OP_RESPONSE | GB_SVC_TYPE_INTF_ACTIVATE_REQUEST
#define GB_SVC_TYPE_INTF_RESUME_REQUEST    0x28
#define GB_SVC_TYPE_INTF_RESUME_RESPONSE   OP_RESPONSE | GB_SVC_TYPE_INTF_RESUME_REQUEST

#define GB_SVC_UNIPRO_HIBERNATE_MODE 0x11

#define GB_SVC_SETPWRM_PWR_OK    0x00
#define GB_SVC_SETPWRM_PWR_LOCAL 0x01

#define GB_SVC_INTF_VSYS_OK 0x00

#define GB_SVC_INTF_REFCLK_OK 0x00

#define GB_SVC_INTF_UNIPRO_OK 0x00

#define GB_SVC_INTF_TYPE_GREYBUS 0x03

/* Greybus SVC protocol status values */
#define GB_SVC_OP_SUCCESS               0x00
#define GB_SVC_OP_UNKNOWN_ERROR         0x01
#define GB_SVC_INTF_NOT_DETECTED        0x02
#define GB_SVC_INTF_NO_UPRO_LINK        0x03
#define GB_SVC_INTF_UPRO_NOT_DOWN       0x04
#define GB_SVC_INTF_UPRO_NOT_HIBERNATED 0x05
#define GB_SVC_INTF_NO_V_SYS            0x06
#define GB_SVC_INTF_V_CHG               0x07
#define GB_SVC_INTF_WAKE_BUSY           0x08
#define GB_SVC_INTF_NO_REFCLK           0x09
#define GB_SVC_INTF_RELEASING           0x0a
#define GB_SVC_INTF_NO_ORDER            0x0b
#define GB_SVC_INTF_MBOX_SET            0x0c
#define GB_SVC_INTF_BAD_MBOX            0x0d
#define GB_SVC_INTF_OP_TIMEOUT          0x0e
#define GB_SVC_PWRMON_OP_NOT_PRESENT    0x0f

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
