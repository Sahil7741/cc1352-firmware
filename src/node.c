// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "node.h"
#include "greybus_protocol.h"
#include "greybus_messages.h"
#include "svc.h"
#include <zephyr/net/net_ip.h>
#include <zephyr/sys/dlist.h>
#include <assert.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#define MAX_GREYBUS_NODES CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct node_control_data {
	struct in6_addr addr;
	int sock;
	sys_dlist_t msgs;
};

K_MEM_SLAB_DEFINE_STATIC(node_control_data_slab, sizeof(struct node_control_data),
			 MAX_GREYBUS_NODES, 8);

static sys_dlist_t node_interface_list = SYS_DLIST_STATIC_INIT(&node_interface_list);
static struct in6_addr node_addr_cache[MAX_GREYBUS_NODES];
static size_t node_addr_cache_pos;

static void msgs_drain_all(sys_dlist_t *list)
{
	struct gb_message *msg, *msg_safe;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(list, msg, msg_safe, node) {
		sys_dlist_remove(&msg->node);
		gb_message_dealloc(msg);
	}
}

static void msgs_drain_by_cport(sys_dlist_t *list, uint16_t cport_id)
{
	struct gb_message *msg, *msg_safe;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(list, msg, msg_safe, node) {
		if (gb_message_pad_read(msg) == cport_id) {
			sys_dlist_remove(&msg->node);
			gb_message_dealloc(msg);
		}
	}
}

static struct gb_message *msgs_find(sys_dlist_t *list, uint16_t cport_id)
{
	struct gb_message *msg, *msg_safe;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(list, msg, msg_safe, node) {
		if (gb_message_pad_read(msg) == cport_id) {
			sys_dlist_remove(&msg->node);
			return msg;
		}
	}

	return NULL;
}

static int ipaddr_cmp(const struct in6_addr *a, const struct in6_addr *b)
{
	return memcmp(a->s6_addr, b->s6_addr, sizeof(struct in6_addr));
}

static int node_addr_cache_search(const struct in6_addr *addr)
{
	size_t i;
	int ret;

	for (i = 0; i < node_addr_cache_pos; ++i) {
		ret = ipaddr_cmp(&node_addr_cache[i], addr);
		if (!ret) {
			return i;
		}
	}

	return -1;
}

static int node_addr_cache_insert(const struct in6_addr *addr)
{
	if (node_addr_cache_pos >= MAX_GREYBUS_NODES) {
		return -ENOMEM;
	}

	net_ipaddr_copy(&node_addr_cache[node_addr_cache_pos++], addr);
	return 0;
}

static void node_addr_cache_remove_at(size_t pos)
{
	--node_addr_cache_pos;
	if (pos != node_addr_cache_pos) {
		net_ipaddr_copy(&node_addr_cache[pos], &node_addr_cache[node_addr_cache_pos]);
	}
}

static size_t node_addr_cache_remove(const struct in6_addr *addr)
{
	size_t i;

	for (i = 0; i < node_addr_cache_pos && ipaddr_cmp(&node_addr_cache[i], addr) <= 0; ++i) {
	}

	node_addr_cache_remove_at(i);
	return i;
}

static int write_data(int sock, const void *data, size_t len)
{
	int ret, transmitted = 0;

	while (transmitted < len) {
		ret = zsock_send(sock, transmitted + (char *)data, len - transmitted, 0);
		if (ret < 0) {
			LOG_ERR("Failed to transmit data");
			return ret;
		}
		transmitted += ret;
	}
	return transmitted;
}

static int read_data(int sock, void *data, size_t len)
{
	int ret, received = 0;

	while (received < len) {
		ret = zsock_recv(sock, received + (char *)data, len - received, 0);
		if (ret < 0) {
			LOG_ERR("Failed to receive data");
			return ret;
		} else if (ret == 0) {
			/* Socket was closed by peer */
			return 0;
		}
		received += ret;
	}
	return received;
}

static struct gb_message *gb_message_receive(int sock, bool *flag)
{
	int ret;
	struct gb_operation_msg_hdr hdr;
	struct gb_message *msg;

	ret = read_data(sock, &hdr, sizeof(struct gb_operation_msg_hdr));
	if (ret != sizeof(struct gb_operation_msg_hdr)) {
		*flag = ret == 0;
		goto early_exit;
	}

	msg = gb_message_alloc(gb_hdr_payload_len(&hdr), hdr.type, hdr.id, hdr.status);
	if (!msg) {
		LOG_ERR("Failed to allocate node message");
		goto early_exit;
	}

	memcpy(msg->header.pad, hdr.pad, sizeof(uint16_t));
	ret = read_data(sock, msg->payload, gb_message_payload_len(msg));
	if (ret != gb_message_payload_len(msg)) {
		*flag = ret == 0;
		goto free_msg;
	}

	return msg;

free_msg:
	gb_message_dealloc(msg);
early_exit:
	return NULL;
}

static int gb_message_send(int sock, const struct gb_message *msg)
{
	int ret;

	ret = write_data(sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
	if (ret != sizeof(struct gb_operation_msg_hdr)) {
		LOG_ERR("Failed to send Greybus Message Header to node");
		goto early_exit;
	}

	ret = write_data(sock, msg->payload, gb_message_payload_len(msg));
	if (ret != gb_message_payload_len(msg)) {
		LOG_ERR("Failed to send Greybus Message Payload to node");
		goto early_exit;
	}

	return 0;

early_exit:
	return ret;
}

static int connect_to_node(const struct sockaddr *addr)
{
	int ret, sock;
	size_t addr_size;

	if (addr->sa_family == AF_INET6) {
		sock = zsock_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		addr_size = sizeof(struct sockaddr_in6);
	} else {
		sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		addr_size = sizeof(struct sockaddr_in);
	}

	if (sock < 0) {
		LOG_ERR("Failed to create socket %d", errno);
		return sock;
	}

	ret = zsock_connect(sock, addr, addr_size);
	if (ret) {
		LOG_ERR("Failed to connect to node %d", errno);
		goto fail;
	}

	return sock;

fail:
	zsock_close(sock);
	return ret;
}

static int node_intf_create_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct sockaddr_in6 node_addr;
	struct node_control_data *ctrl_data = ctrl->ctrl_data;

	/* Do not create socket for cports other than 0 */
	if (cport_id != 0) {
		return 0;
	}

	memcpy(&node_addr.sin6_addr, &ctrl_data->addr, sizeof(struct in6_addr));
	node_addr.sin6_family = AF_INET6;
	node_addr.sin6_scope_id = 0;
	node_addr.sin6_port = htons(GB_TRANSPORT_TCPIP_BASE_PORT + cport_id);

	ctrl_data->sock = connect_to_node((struct sockaddr *)&node_addr);
	return ctrl_data->sock;
}

static void node_intf_destroy_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct node_control_data *ctrl_data = ctrl->ctrl_data;

	/* Cleanup queue */
	msgs_drain_by_cport(&ctrl_data->msgs, cport_id);

	/* Close socket for cport 0 */
	if (cport_id == 0) {
		zsock_close(ctrl_data->sock);
	}
}

static struct gb_message *node_base_read(int sock, bool *flag)
{
	int ret;
	struct zsock_pollfd fd = {.fd = sock, .events = ZSOCK_POLLIN};

	ret = zsock_poll(&fd, 1, 0);
	if (ret <= 0 || !(fd.revents & ZSOCK_POLLIN)) {
		return NULL;
	}

	return gb_message_receive(fd.fd, flag);
}

static struct gb_message *node_inf_read(struct gb_controller *ctrl, uint16_t cport_id)
{
	bool flag = false;
	struct gb_message *msg;
	struct node_control_data *ctrl_data = ctrl->ctrl_data;

	/* return any pending message */
	if ((msg = msgs_find(&ctrl_data->msgs, cport_id))) {
		LOG_DBG("Return Point 1");
		return msg;
	}

	msg = node_base_read(ctrl_data->sock, &flag);
	if (flag) {
		LOG_ERR("Socket of Cport %u closed by Peer Node", cport_id);
		ctrl->destroy_connection(ctrl, cport_id);
	}

	if (!msg) {
		return NULL;
	}

	/* Add to queue if the message is of different cport */
	if (gb_message_pad_read(msg) == cport_id) {
		LOG_DBG("Return Point 2");
		return msg;
	}

	sys_dlist_append(&ctrl_data->msgs, &msg->node);
	return NULL;
}

static int node_inf_write(struct gb_controller *ctrl, struct gb_message *msg, uint16_t cport_id)
{
	int ret;
	struct node_control_data *ctrl_data = ctrl->ctrl_data;

	gb_message_pad_write(msg, cport_id);
	ret = gb_message_send(ctrl_data->sock, msg);
	gb_message_dealloc(msg);

	return ret;
}

static struct gb_interface *node_create_interface(struct in6_addr *addr)
{
	int ret;
	struct node_control_data *ctrl_data;
	struct gb_interface *inf;

	ret = k_mem_slab_alloc(&node_control_data_slab, (void **)&ctrl_data, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to allocate Greybus connection");
		goto early_exit;
	}

	sys_dlist_init(&ctrl_data->msgs);
	net_ipaddr_copy(&ctrl_data->addr, addr);
	ret = node_addr_cache_insert(addr);
	if (ret) {
		LOG_ERR("Failed to create new node");
		goto free_ctrl_data;
	}

	inf = gb_interface_alloc(node_inf_read, node_inf_write, node_intf_create_connection,
				 node_intf_destroy_connection, ctrl_data);
	if (!inf) {
		goto remove_node;
	}

	LOG_DBG("Create new interface with ID %u", inf->id);
	sys_dlist_append(&node_interface_list, &inf->node);

	return inf;

remove_node:
	node_addr_cache_remove_at(ret);
free_ctrl_data:
	k_mem_slab_free(&node_control_data_slab, (void **)&ctrl_data);
early_exit:
	return NULL;
}

void node_destroy_interface(struct gb_interface *inf)
{
	struct node_control_data *ctrl_data;

	if (inf == NULL) {
		return;
	}

	ctrl_data = inf->controller.ctrl_data;

	sys_dlist_remove(&inf->node);

	msgs_drain_all(&ctrl_data->msgs);

	node_addr_cache_remove(&ctrl_data->addr);
	k_mem_slab_free(&node_control_data_slab, (void **)&ctrl_data);
	gb_interface_dealloc(inf);
}

struct gb_interface *node_find_by_id(uint8_t id)
{
	struct gb_interface *inf;

	SYS_DLIST_FOR_EACH_CONTAINER(&node_interface_list, inf, node) {
		if (inf->id == id) {
			return inf;
		}
	}

	return NULL;
}

void node_filter(struct in6_addr *active_addr, size_t active_len)
{
	size_t i;
	struct gb_interface *inf;
	int ret;

	for (i = 0; i < active_len; ++i) {
		ret = node_addr_cache_search(&active_addr[i]);
		LOG_DBG("Old Node: %d", ret);

		/* Handle New Node */
		if (ret < 0) {
			LOG_DBG("New node discovered");
			inf = node_create_interface(&active_addr[i]);
			if (!inf) {
				LOG_ERR("Failed to create interface");
				continue;
			}
			svc_send_module_inserted(inf->id);
		}
	}
}

void node_destroy_all(void)
{
	struct gb_interface *inf, *inf_safe;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&node_interface_list, inf, inf_safe, node) {
		node_destroy_interface(inf);
	}

	assert(!k_mem_slab_num_used_get(&node_control_data_slab));
}
