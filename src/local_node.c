// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2016 Alexandre Bailon
 *
 * Modifications Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "greybus_messages.h"
#include "greybus_interfaces.h"
#include "greybus_protocol.h"
#include "local_node.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define LOCAL_NODE_ID 2
#define CPORTS_NUM    1

#define CONTROL_PROTOCOL_CPORT 0
#define LOG_PROTOCOL_CPORT     1

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct local_node_controller_data {
	struct k_fifo queues[CPORTS_NUM];
	uint16_t log_buffer_pos;
	uint8_t log_buffer[256];
};

struct gb_control_version_response {
	uint8_t major;
	uint8_t minor;
} __packed;

struct gb_control_get_manifest_size_response {
	uint8_t manifest_size;
} __packed;

/* Control protocol manifest get request has no payload */
struct gb_control_get_manifest_response {
	uint8_t data[0];
} __packed;

/* Control protocol [dis]connected request */
struct gb_control_connected_request {
	uint16_t cport_id;
} __packed;

struct gb_control_disconnecting_request {
	uint16_t cport_id;
} __packed;
/* disconnecting response has no payload */

struct gb_control_disconnected_request {
	uint16_t cport_id;
} __packed;

struct gb_log_send_request {
	uint16_t len;
	uint8_t log[];
} __packed;

static const uint8_t manifest[] = {0x4c, 0x00, 0x00, 0x01, 0x08, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00,
				   0x00, 0x18, 0x00, 0x02, 0x00, 0x11, 0x01, 0x42, 0x65, 0x61, 0x67,
				   0x6c, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x20, 0x43, 0x43, 0x31, 0x33,
				   0x35, 0x32, 0x00, 0x18, 0x00, 0x02, 0x00, 0x11, 0x02, 0x42, 0x65,
				   0x61, 0x67, 0x6c, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x20, 0x43, 0x43,
				   0x31, 0x33, 0x35, 0x32, 0x00, 0x08, 0x00, 0x03, 0x00, 0x01, 0x1a,
				   0x00, 0x00, 0x08, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01, 0x1a};

K_HEAP_DEFINE(log_heap, 1024);

static void queue_drain(struct k_fifo *queue)
{
	struct gb_message *msg;

	while ((msg = k_fifo_get(queue, K_NO_WAIT))) {
		gb_message_dealloc(msg);
	}
}

static void response_helper(struct gb_controller *ctrl, struct gb_message *msg, const void *payload,
			    size_t payload_len, uint8_t status, uint16_t cport_id)
{
	struct local_node_controller_data *ctrl_data = ctrl->ctrl_data;
	struct gb_message *resp = gb_message_response_alloc(payload, payload_len, msg->header.type,
							    msg->header.id, status);

	if (resp == NULL) {
		LOG_ERR("Failed to allocate response for %X", msg->header.type);
		return;
	}
	k_fifo_put(&ctrl_data->queues[cport_id], resp);
}

static int request_helper(struct gb_controller *ctrl, const void *payload, size_t payload_len,
			  uint8_t request_type, uint16_t cport_id)
{
	struct gb_message *msg;
	struct local_node_controller_data *ctrl_data = ctrl->ctrl_data;

	msg = gb_message_request_alloc(payload, payload_len, request_type, false);
	if (msg == NULL) {
		return -ENOMEM;
	}

	k_fifo_put(&ctrl_data->queues[cport_id], msg);

	return msg->header.id;
}

static struct gb_message *intf_read(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct local_node_controller_data *ctrl_data = ctrl->ctrl_data;

	return k_fifo_get(&ctrl_data->queues[cport_id], K_NO_WAIT);
}

static void control_protocol_cport_shutdown_handler(struct gb_controller *ctrl,
						    struct gb_message *msg)
{
	size_t i;
	struct local_node_controller_data *ctrl_data = ctrl->ctrl_data;

	for (i = 0; i < CPORTS_NUM; i++) {
		queue_drain(&ctrl_data->queues[i]);
	}

	response_helper(ctrl, msg, NULL, 0, GB_OP_SUCCESS, CONTROL_PROTOCOL_CPORT);
}

static void control_protocol_version_handler(struct gb_controller *ctrl, struct gb_message *msg)
{
	struct gb_control_version_response response = {
		.major = 0,
		.minor = 1,
	};

	response_helper(ctrl, msg, &response, sizeof(response), GB_OP_SUCCESS,
			CONTROL_PROTOCOL_CPORT);
}

static void control_protocol_get_manifest_size_handler(struct gb_controller *ctrl,
						       struct gb_message *msg)
{
	struct gb_control_get_manifest_size_response response = {
		.manifest_size = sizeof(manifest),
	};

	response_helper(ctrl, msg, &response, sizeof(response), GB_OP_SUCCESS,
			CONTROL_PROTOCOL_CPORT);
}

static void control_protocol_get_manifest_handler(struct gb_controller *ctrl,
						  struct gb_message *msg)
{
	response_helper(ctrl, msg, manifest, sizeof(manifest), GB_OP_SUCCESS,
			CONTROL_PROTOCOL_CPORT);
}

static void control_protocol_empty_handler(struct gb_controller *ctrl, struct gb_message *msg)
{
	response_helper(ctrl, msg, NULL, 0, GB_OP_SUCCESS, CONTROL_PROTOCOL_CPORT);
}

static void control_protocol_handle(struct gb_controller *ctrl, struct gb_message *msg)
{
	switch (gb_message_type(msg)) {
	case GB_COMMON_TYPE_CPORT_SHUTDOWN_REQUEST:
		control_protocol_cport_shutdown_handler(ctrl, msg);
		break;
	case GB_CONTROL_TYPE_VERSION_REQUEST:
		control_protocol_version_handler(ctrl, msg);
		break;
	case GB_CONTROL_TYPE_GET_MANIFEST_SIZE_REQUEST:
		control_protocol_get_manifest_size_handler(ctrl, msg);
		break;
	case GB_CONTROL_TYPE_GET_MANIFEST_REQUEST:
		control_protocol_get_manifest_handler(ctrl, msg);
		break;
	case GB_CONTROL_TYPE_CONNECTED_REQUEST:
	case GB_CONTROL_TYPE_DISCONNECTING_REQUEST:
	case GB_CONTROL_TYPE_DISCONNECTED_REQUEST:
	case GB_CONTROL_TYPE_TIMESYNC_ENABLE_REQUEST:
	case GB_CONTROL_TYPE_TIMESYNC_DISABLE_REQUEST:
	case GB_CONTROL_TYPE_TIMESYNC_AUTHORITATIVE_REQUEST:
	case GB_CONTROL_TYPE_INTF_HIBERNATE_ABORT_REQUEST:
		control_protocol_empty_handler(ctrl, msg);
		break;
	default:
		LOG_ERR("Unimplemented control protocol request %X", gb_message_type(msg));
	}
}

static void log_protocol_cport_shutdown_handler(struct gb_controller *ctrl, struct gb_message *msg)
{
	struct local_node_controller_data *ctrl_data = ctrl->ctrl_data;

	ctrl_data->log_buffer_pos = 0;
	response_helper(ctrl, msg, NULL, 0, GB_OP_SUCCESS, LOG_PROTOCOL_CPORT);
}

static void log_protocol_handler(struct gb_controller *ctrl, struct gb_message *msg)
{
	switch (gb_message_type(msg)) {
	case GB_COMMON_TYPE_CPORT_SHUTDOWN_REQUEST:
		log_protocol_cport_shutdown_handler(ctrl, msg);
		break;
	case GB_LOG_TYPE_SEND_LOG_RESPONSE:
		LOG_DBG("Log Message Response %d", gb_message_is_success(msg));
		break;
	default:
		LOG_ERR("Unimplemented log protocol request %X", gb_message_type(msg));
	}

	gb_message_dealloc(msg);
}

static int intf_write(struct gb_controller *ctrl, struct gb_message *msg, uint16_t cport_id)
{
	switch (cport_id) {
	case CONTROL_PROTOCOL_CPORT:
		control_protocol_handle(ctrl, msg);
		break;
	case LOG_PROTOCOL_CPORT:
		log_protocol_handler(ctrl, msg);
		break;
	}

	gb_message_dealloc(msg);

	return 0;
}

static int intf_create_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct local_node_controller_data *ctrl_data = ctrl->ctrl_data;

	k_fifo_init(&ctrl_data->queues[cport_id]);
	return 0;
}

static void intf_destroy_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct local_node_controller_data *ctrl_data = ctrl->ctrl_data;

	queue_drain(&ctrl_data->queues[cport_id]);
}

static struct local_node_controller_data local_node_ctrl_data = {.log_buffer_pos = 0,
								 .log_buffer = {0}};

static struct gb_interface intf = {.id = LOCAL_NODE_ID,
				   .controller = {.read = intf_read,
						  .write = intf_write,
						  .create_connection = intf_create_connection,
						  .destroy_connection = intf_destroy_connection,
						  .ctrl_data = &local_node_ctrl_data}};

uint16_t log_protocol_send(uint8_t log_data[], uint16_t len)
{
	size_t log_req_size = sizeof(struct gb_log_send_request) + len;
	struct gb_log_send_request *request = k_heap_alloc(&log_heap, log_req_size, K_NO_WAIT);
	int res;

	if (!request) {
		/* Logging from Log protocol might not be the best idea */
		return 0;
	}

	request->len = len;
	memcpy(request->log, log_data, len);

	res = request_helper(&intf.controller, request, log_req_size, GB_LOG_TYPE_SEND_LOG_REQUEST,
			     LOG_PROTOCOL_CPORT);

	k_heap_free(&log_heap, request);

	return (res < 0) ? 0 : len;
}
