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
#define MAX_GREYBUS_NODES      CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES
#define MAX_GREYBUS_INTERFACES CONFIG_BEAGLEPLAY_GREYBUS_MAX_INTERFACES

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

K_MEM_SLAB_DEFINE_STATIC(gb_connection_slab, sizeof(struct gb_connection), MAX_GREYBUS_NODES, 4);
K_MEM_SLAB_DEFINE_STATIC(gb_interface_slab, sizeof(struct gb_interface), MAX_GREYBUS_INTERFACES, 8);

static atomic_t operation_id_counter = ATOMIC_INIT(OPERATION_ID_START);
static atomic_t interface_id_counter = ATOMIC_INIT(INTERFACE_ID_START);
static sys_dlist_t gb_connections_list = SYS_DLIST_STATIC_INIT(&gb_connections_list);

static struct gb_message *gb_message_alloc(const void *payload, size_t payload_len,
					   uint8_t message_type, uint16_t operation_id,
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
	memcpy(msg->payload, payload, msg->payload_size);

	return msg;
}

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

int gb_message_hdlc_send(const struct gb_message *msg)
{
	char buffer[HDLC_MAX_BLOCK_SIZE];

	memcpy(buffer, &msg->header, sizeof(struct gb_operation_msg_hdr));
	memcpy(&buffer[sizeof(struct gb_operation_msg_hdr)], msg->payload, msg->payload_size);

	hdlc_block_send_sync(buffer, msg->header.size, ADDRESS_GREYBUS, 0x03);

	return 0;
}

struct gb_connection *gb_create_connection(struct gb_interface *inf_ap,
					   struct gb_interface *inf_peer, uint16_t ap_cport,
					   uint16_t peer_cport)
{
	int ret;
	struct gb_connection *conn;

	ret = inf_ap->controller.create_connection(&inf_ap->controller, ap_cport);
	if (ret < 0) {
		LOG_ERR("Failed to create Greybus ap connection");
		return NULL;
	}

	ret = inf_peer->controller.create_connection(&inf_peer->controller, peer_cport);
	if (ret < 0) {
		LOG_ERR("Failed to create Greybus peer connection");
		return NULL;
	}

	ret = k_mem_slab_alloc(&gb_connection_slab, (void **)&conn, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to allocate Greybus connection");
		return NULL;
	}

	conn->inf_ap = inf_ap;
	conn->inf_peer = inf_peer;
	conn->ap_cport_id = ap_cport;
	conn->peer_cport_id = peer_cport;

	sys_dnode_init(&conn->node);
	sys_dlist_append(&gb_connections_list, &conn->node);

	return conn;
}

static void gb_flush_connection(struct gb_connection *conn)
{
	struct gb_message *msg;
	bool flag;

	do {
		flag = false;
		msg = conn->inf_ap->controller.read(&conn->inf_ap->controller, conn->ap_cport_id);
		if (msg != NULL) {
			conn->inf_peer->controller.write(&conn->inf_peer->controller, msg,
							 conn->peer_cport_id);
			flag = true;
		}

		msg = conn->inf_peer->controller.read(&conn->inf_peer->controller,
						      conn->peer_cport_id);
		if (msg != NULL) {
			conn->inf_ap->controller.write(&conn->inf_ap->controller, msg,
						       conn->ap_cport_id);
			flag = true;
		}
	} while (flag);
}

static void gb_connection_dealloc(struct gb_connection *conn)
{
	sys_dlist_remove(&conn->node);
	k_mem_slab_free(&gb_connection_slab, (void **)&conn);
}

int gb_destroy_connection(struct gb_interface *inf_ap, struct gb_interface *inf_peer,
			  uint16_t ap_cport, uint16_t peer_cport)
{
	struct gb_connection *conn;

	SYS_DLIST_FOR_EACH_CONTAINER(&gb_connections_list, conn, node) {
		/*
		 * While the names are inf_peer and inf_ap, they are just arbitrary. So do
		 * comparisons in reverse as well
		 */
		if ((conn->inf_peer == inf_peer && conn->inf_ap == inf_ap &&
		     conn->peer_cport_id == peer_cport && conn->ap_cport_id == ap_cport)) {
			gb_flush_connection(conn);
			conn->inf_ap->controller.destroy_connection(&conn->inf_ap->controller,
								    ap_cport);
			conn->inf_peer->controller.destroy_connection(&conn->inf_peer->controller,
								      peer_cport);
			goto cleanup;
		} else if ((conn->inf_peer == inf_ap && conn->inf_ap == inf_peer &&
			    conn->peer_cport_id == ap_cport && conn->ap_cport_id == peer_cport)) {
			gb_flush_connection(conn);
			conn->inf_ap->controller.destroy_connection(&conn->inf_ap->controller,
								    peer_cport);
			conn->inf_peer->controller.destroy_connection(&conn->inf_peer->controller,
								      ap_cport);
			goto cleanup;
		}
	}

	return -1;

cleanup:
	gb_connection_dealloc(conn);
	return 0;
}

struct gb_message *gb_message_request_alloc(const void *payload, size_t payload_len,
					    uint8_t request_type, bool is_oneshot)
{
	uint16_t operation_id = is_oneshot ? 0 : new_operation_id();

	return gb_message_alloc(payload, payload_len, request_type, operation_id, 0);
}

struct gb_message *gb_message_response_alloc(const void *payload, size_t payload_len,
					     uint8_t request_type, uint16_t operation_id,
					     uint8_t status)
{
	return gb_message_alloc(payload, payload_len, OP_RESPONSE | request_type, operation_id,
				status);
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

void gb_connections_process_all(gb_connection_callback cb)
{
	struct gb_connection *conn;

	SYS_DLIST_FOR_EACH_CONTAINER(&gb_connections_list, conn, node) {
		cb(conn);
	}
}

static void gb_connection_destroy(struct gb_connection *conn)
{
	gb_flush_connection(conn);
	conn->inf_ap->controller.destroy_connection(&conn->inf_ap->controller, conn->ap_cport_id);
	conn->inf_peer->controller.destroy_connection(&conn->inf_peer->controller,
						      conn->peer_cport_id);
	gb_connection_dealloc(conn);
}

void gb_interface_destroy(struct gb_interface *intf)
{
	struct gb_connection *conn, *conn_safe;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&gb_connections_list, conn, conn_safe, node) {
		if (conn->inf_ap == intf || conn->inf_peer == intf) {
			gb_connection_destroy(conn);
		}
	}

	// TODO: Maybe move this function to controller
	node_destroy_interface(intf);
}
