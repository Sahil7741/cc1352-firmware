// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "operations.h"
#include "ap.h"
#include "greybus_protocol.h"
#include "hdlc.h"
#include "node.h"
#include "svc.h"
#include "zephyr/kernel.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/dlist.h>

#define OPERATION_ID_START     1
#define INTERFACE_ID_START     2
#define MAX_GREYBUS_INTERFACES CONFIG_BEAGLEPLAY_GREYBUS_MAX_INTERFACES

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

K_MEM_SLAB_DEFINE_STATIC(gb_interface_slab, sizeof(struct gb_interface), MAX_GREYBUS_INTERFACES, 8);

static atomic_t operation_id_counter = ATOMIC_INIT(OPERATION_ID_START);
static atomic_t interface_id_counter = ATOMIC_INIT(INTERFACE_ID_START);

static uint16_t new_operation_id(void)
{
	atomic_val_t temp = atomic_inc(&operation_id_counter);

	if (temp == UINT16_MAX) {
		atomic_set(&operation_id_counter, OPERATION_ID_START);
	}
	return temp;
}

static uint8_t new_interface_id(void)
{
	atomic_val_t temp = atomic_inc(&interface_id_counter);

	if (temp == UINT8_MAX) {
		atomic_set(&interface_id_counter, INTERFACE_ID_START);
	}
	return temp;
}

void gb_message_dealloc(struct gb_message *msg)
{
	k_free(msg);
}

struct gb_message *gb_message_request_alloc(const void *payload, size_t payload_len,
					    uint8_t request_type, bool is_oneshot)
{
	uint16_t operation_id = is_oneshot ? 0 : new_operation_id();

	struct gb_message *msg = gb_message_alloc(payload_len, request_type, operation_id, 0);
	memcpy(msg->payload, payload, payload_len);
	return msg;
}

struct gb_interface *gb_interface_alloc(gb_controller_read_callback_t read_cb,
					gb_controller_write_callback_t write_cb,
					gb_controller_create_connection_t create_connection,
					gb_controller_destroy_connection_t destroy_connection,
					void *ctrl_data)
{
	int ret;
	struct gb_interface *intf;

	ret = k_mem_slab_alloc(&gb_interface_slab, (void **)&intf, K_NO_WAIT);
	if (ret < 0) {
		return NULL;
	}

	intf->id = new_interface_id();
	intf->controller.read = read_cb;
	intf->controller.write = write_cb;
	intf->controller.create_connection = create_connection;
	intf->controller.destroy_connection = destroy_connection;
	intf->controller.ctrl_data = ctrl_data;
	sys_dnode_init(&intf->node);

	return intf;
}

void gb_interface_dealloc(struct gb_interface *intf)
{
	k_mem_slab_free(&gb_interface_slab, (void **)&intf);
}

struct gb_interface *find_interface_by_id(uint8_t intf_id)
{
	switch (intf_id) {
	case SVC_INF_ID:
		return svc_interface();
	case AP_INF_ID:
		return ap_interface();
	default:
		return node_find_by_id(intf_id);
	}
}

struct gb_message *gb_message_alloc(size_t payload_len, uint8_t message_type, uint16_t operation_id,
				    uint8_t status)
{
	struct gb_message *msg;

	msg = k_malloc(sizeof(struct gb_message) + payload_len);
	if (msg == NULL) {
		LOG_WRN("Failed to allocate Greybus request message");
		return NULL;
	}

	msg->header.size = sizeof(struct gb_operation_msg_hdr) + payload_len;
	msg->header.id = operation_id;
	msg->header.type = message_type;
	msg->header.status = status;
	msg->payload_size = payload_len;

	return msg;
}
