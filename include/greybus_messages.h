/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _GREYBUS_MESSAGES_H_
#define _GREYBUS_MESSAGES_H_

#include <zephyr/types.h>
#include "greybus_protocol.h"
#include <string.h>

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
	uint8_t payload[];
};

/*
 * Return the paylaod length of a greybus message from message header.
 *
 * @param hdr: greybus header
 *
 * @return payload length
 */
static inline size_t gb_hdr_payload_len(const struct gb_operation_msg_hdr *hdr) {
	return hdr->size - sizeof(struct gb_operation_msg_hdr);
}

/*
 * Return the paylaod length of a greybus message.
 *
 * @param msg: Greybus message
 *
 * @return payload length
 */
static inline size_t gb_message_payload_len(const struct gb_message *msg) {
	return gb_hdr_payload_len(&msg->header);
}

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
 * Allocate Greybus message
 *
 * @param Payload len
 * @param Response Type
 * @param Operation ID of Request
 * @param Status
 *
 * @return greybus message allocated on heap. Null in case of error
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
 * @return greybus message allocated on heap. Null in case of error
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
 * @return greybus message allocated on heap. Null in case of error
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

#endif
