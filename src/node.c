/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "node.h"
#include "greybus_protocol.h"
#include "operations.h"
#include "zephyr/sys/dlist.h"
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct node_control_data {
	int *cports;
	uint16_t cports_len;
	struct in6_addr addr;
};

static sys_dlist_t node_interface_list = SYS_DLIST_STATIC_INIT(&node_interface_list);

static int write_data(int sock, const void *data, size_t len)
{
	int ret;
	int transmitted = 0;
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
	int ret;
	int recieved = 0;
	while (recieved < len) {
		ret = zsock_recv(sock, recieved + (char *)data, len - recieved, 0);
		if (ret < 0) {
			LOG_ERR("Failed to recieve data");
			return ret;
		} else if (ret == 0) {
			// Socket was closed by peer
			return 0;
		}
		recieved += ret;
	}
	return recieved;
}

static struct gb_message *gb_message_receive(int sock, bool *flag)
{
	int ret;
	struct gb_operation_msg_hdr hdr;
	struct gb_message *msg;
	size_t payload_size;

	ret = read_data(sock, &hdr, sizeof(struct gb_operation_msg_hdr));
	if (ret <= 0) {
		*flag = ret == 0;
		goto early_exit;
	}

	payload_size = hdr.size - sizeof(struct gb_operation_msg_hdr);
	msg = k_malloc(sizeof(struct gb_message) + payload_size);
	if (msg == NULL) {
		LOG_ERR("Failed to allocate node message");
		goto free_msg;
	}

	memcpy(&msg->header, &hdr, sizeof(struct gb_operation_msg_hdr));
	msg->payload_size = payload_size;
	ret = read_data(sock, msg->payload, msg->payload_size);
	if (ret < 0) {
		goto free_msg;
	}

	return msg;

free_msg:
	k_free(msg);
early_exit:
	return NULL;
}

static int gb_message_send(int sock, const struct gb_message *msg)
{
	int ret;

	ret = write_data(sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
	if (ret < 0) {
		goto early_exit;
	}

	ret = write_data(sock, msg->payload, msg->payload_size);
	if (ret < 0) {
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
		ret = sock;
		goto fail;
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

static int *cports_alloc(size_t len)
{
	int *cports;
	size_t i;
	size_t size_in_bytes = sizeof(int) * len;
	cports = k_malloc(size_in_bytes);
	if (!cports) {
		return NULL;
	}

	for (i = 0; i < len; i++) {
		cports[i] = -1;
	}

	return cports;
}

static int *cports_realloc(int *original_cports, size_t original_length, size_t new_length)
{
	if (new_length == 0) {
		free(original_cports);
		return NULL;
	}

	if (!original_cports) {
		return cports_alloc(new_length);
	}

	if (new_length <= original_length) {
		return original_cports;
	}

	int *cports = cports_alloc(new_length);
	if (cports) {
		memcpy(cports, original_cports, sizeof(int) * original_length);
		k_free(original_cports);
	}

	return cports;
}

static int node_intf_create_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	int ret;
	struct sockaddr_in6 node_addr;
	struct node_control_data *ctrl_data = ctrl->ctrl_data;

	memcpy(&node_addr.sin6_addr, &ctrl_data->addr, sizeof(struct in6_addr));
	node_addr.sin6_family = AF_INET6;
	node_addr.sin6_scope_id = 0;
	node_addr.sin6_port = htons(GB_TRANSPORT_TCPIP_BASE_PORT + cport_id);

	ctrl_data->cports = cports_realloc(ctrl_data->cports, ctrl_data->cports_len, cport_id + 1);
	if (!ctrl_data->cports) {
		return -ENOMEM;
	}
	ctrl_data->cports_len = cport_id + 1;

	if (ctrl_data->cports[cport_id] != -1) {
		LOG_ERR("Cannot create multiple connections to a cport");
		ret = -EEXIST;
		goto early_exit;
	}

	ret = connect_to_node((struct sockaddr *)&node_addr);
	if (ret < 0) {
		return ret;
	}

	ctrl_data->cports[cport_id] = ret;

early_exit:
	return ret;
}

static void node_intf_destroy_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct node_control_data *ctrl_data = ctrl->ctrl_data;

	if (cport_id >= ctrl_data->cports_len) {
		return;
	}

	zsock_close(ctrl_data->cports[cport_id]);
	ctrl_data->cports[cport_id] = -1;
}

static struct gb_message *node_inf_read(struct gb_controller *ctrl, uint16_t cport_id)
{
	int ret;
	struct zsock_pollfd fd[1];
	bool flag = false;
	struct gb_message *msg = NULL;
	struct node_control_data *ctrl_data = ctrl->ctrl_data;

	if (cport_id >= ctrl_data->cports_len) {
		LOG_ERR("Cport ID greater than Cports Length");
		goto early_exit;
	}

	if (ctrl_data->cports[cport_id] < 0) {
		LOG_ERR("Cport ID %u is not active for reading", cport_id);
		goto early_exit;
	}

	fd[0].fd = ctrl_data->cports[cport_id];
	fd[0].events = ZSOCK_POLLIN;

	ret = zsock_poll(fd, 1, 0);
	if (ret <= 0) {
		goto early_exit;
	}

	if (fd[0].revents & ZSOCK_POLLIN) {
		msg = gb_message_receive(fd[0].fd, &flag);
		if (flag) {
			LOG_ERR("Socket of Cport %u closed by Peer Node", cport_id);
		}
	}

early_exit:
	return msg;
}

static int node_inf_write(struct gb_controller *ctrl, struct gb_message *msg, uint16_t cport_id)
{
  int ret;
	struct node_control_data *ctrl_data = ctrl->ctrl_data;
	if (cport_id >= ctrl_data->cports_len) {
    ret = -1;
    goto free_msg;
	}

	if (ctrl_data->cports[cport_id] < 0) {
		LOG_ERR("Cport ID %u is not active for writing", cport_id);
    ret = -1;
    goto free_msg;
	}

	ret = gb_message_send(ctrl_data->cports[cport_id], msg);

free_msg:
  gb_message_dealloc(msg);
  return ret;
}

struct gb_interface *node_create_interface(struct in6_addr *addr)
{
	struct node_control_data *ctrl_data = k_malloc(sizeof(struct node_control_data));
	if (ctrl_data == NULL) {
		goto early_exit;
	}
	ctrl_data->cports = NULL;
	ctrl_data->cports_len = 0;
	memcpy(&ctrl_data->addr, addr, sizeof(struct in6_addr));

	struct gb_interface *inf =
		gb_interface_alloc(node_inf_read, node_inf_write, node_intf_create_connection,
				   node_intf_destroy_connection, ctrl_data);
	if (inf == NULL) {
		goto free_ctrl_data;
	}

	sys_dlist_append(&node_interface_list, &inf->node);

	return inf;

free_ctrl_data:
	k_free(ctrl_data);
early_exit:
	return NULL;
}

void node_destroy_interface(struct gb_interface *inf)
{
	if (inf == NULL) {
		return;
	}

	sys_dlist_remove(&inf->node);
	k_free(inf->controller.ctrl_data);
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

struct gb_interface *node_find_by_addr(struct in6_addr *addr)
{
	struct gb_interface *inf;
	struct node_control_data *ctrl_data;

	SYS_DLIST_FOR_EACH_CONTAINER(&node_interface_list, inf, node) {
		ctrl_data = inf->controller.ctrl_data;
		if (memcmp(&ctrl_data->addr, addr, sizeof(struct in6_addr)) == 0) {
			return inf;
		}
	}

	return NULL;
}
