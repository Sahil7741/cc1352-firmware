// SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "greybus_connections.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);
K_MEM_SLAB_DEFINE_STATIC(gb_connection_slab, sizeof(struct gb_connection), MAX_GREYBUS_CONNECTIONS,
			 4);

static sys_dlist_t gb_connections_list = SYS_DLIST_STATIC_INIT(&gb_connections_list);

static uint8_t gb_connection_process(struct gb_connection *conn)
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

static void gb_flush_connection(struct gb_connection *conn)
{
	while (gb_connection_process(conn)) {
	}
}

static void gb_connection_dealloc(struct gb_connection *conn)
{
	sys_dlist_remove(&conn->node);
	k_mem_slab_free(&gb_connection_slab, (void **)&conn);
}

static void gb_connection_destroy_internal(struct gb_connection *conn)
{
	gb_flush_connection(conn);
	conn->inf_ap->controller.destroy_connection(&conn->inf_ap->controller, conn->ap_cport_id);
	conn->inf_peer->controller.destroy_connection(&conn->inf_peer->controller,
						      conn->peer_cport_id);
	gb_connection_dealloc(conn);
}

struct gb_connection *gb_connection_create(struct gb_interface *inf_ap,
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
		goto destroy_ap_connection;
	}

	ret = k_mem_slab_alloc(&gb_connection_slab, (void **)&conn, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to allocate Greybus connection");
		goto destroy_peer_connection;
	}

	conn->inf_ap = inf_ap;
	conn->inf_peer = inf_peer;
	conn->ap_cport_id = ap_cport;
	conn->peer_cport_id = peer_cport;

	sys_dnode_init(&conn->node);
	sys_dlist_append(&gb_connections_list, &conn->node);

	return conn;

destroy_peer_connection:
	inf_peer->controller.destroy_connection(&inf_peer->controller, peer_cport);
destroy_ap_connection:
	inf_ap->controller.destroy_connection(&inf_ap->controller, ap_cport);
	return NULL;
}

int gb_connection_destroy(struct gb_interface *inf_ap, struct gb_interface *inf_peer,
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

void gb_connection_process_all()
{
	struct gb_connection *conn;

	SYS_DLIST_FOR_EACH_CONTAINER(&gb_connections_list, conn, node) {
		gb_connection_process(conn);
	}
}

void gb_connection_destroy_all(void)
{
	struct gb_connection *conn, *conn_safe;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&gb_connections_list, conn, conn_safe, node) {
		gb_connection_destroy_internal(conn);
	}
}

void gb_connection_destroy_by_interface(struct gb_interface *intf)
{
	struct gb_connection *conn, *conn_safe;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&gb_connections_list, conn, conn_safe, node) {
		if (conn->inf_ap == intf || conn->inf_peer == intf) {
			gb_connection_destroy_internal(conn);
		}
	}
}
