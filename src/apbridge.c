// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "ap.h"
#include "apbridge.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "hdlc.h"
#include "greybus_interfaces.h"

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct ap_to_node_item {
	uint16_t node_cport;
	struct gb_interface *node_intf;
};

struct node_to_ap_item {
	uint8_t node_id;
	uint16_t node_cport;
	uint16_t ap_cport;
};

static struct ap_to_node_item ap_to_node[AP_MAX_NODES] = {0};
static struct node_to_ap_item node_to_ap[AP_MAX_NODES] = {0};
static size_t node_to_ap_count = 0;

static int ap_to_node_add(uint16_t ap_cport, uint16_t node_cport, struct gb_interface *node_intf)
{
	if (ap_cport >= AP_MAX_NODES) {
		return -E2BIG;
	}

	if (ap_to_node[ap_cport].node_intf) {
		return -EALREADY;
	}

	ap_to_node[ap_cport].node_cport = node_cport;
	ap_to_node[ap_cport].node_intf = node_intf;

	return 0;
}

static int ap_to_node_remove(uint16_t ap_cport)
{
	if (ap_cport >= AP_MAX_NODES) {
		return -E2BIG;
	}

	ap_to_node[ap_cport].node_cport = 0;
	ap_to_node[ap_cport].node_intf = NULL;

	return 0;
}

static int node_to_ap_add(uint8_t node_id, uint16_t node_cport, uint16_t ap_cport)
{
	if (node_to_ap_count >= AP_MAX_NODES) {
		return -E2BIG;
	}

	node_to_ap[node_to_ap_count].node_id = node_id;
	node_to_ap[node_to_ap_count].node_cport = node_cport;
	node_to_ap[node_to_ap_count].ap_cport = ap_cport;

	node_to_ap_count++;

	return 0;
}

static int node_to_ap_remove(uint16_t ap_cport)
{
	for (size_t i = 0; i < node_to_ap_count; i++) {
		if (node_to_ap[i].ap_cport == ap_cport) {
			if (i != node_to_ap_count - 1) {
				memcpy(&node_to_ap[i], &node_to_ap[node_to_ap_count - 1],
				       sizeof(struct node_to_ap_item));
			}
			node_to_ap_count--;
			return 0;
		}
	}

	return -EINVAL;
}

static int node_to_ap_cport(uint8_t node_id, uint16_t node_cport)
{
	for (size_t i = 0; i < node_to_ap_count; i++) {
		if (node_to_ap[i].node_id == node_id && node_to_ap[i].node_cport == node_cport) {
			return node_to_ap[i].ap_cport;
		}
	}

	return -EINVAL;
}

void apbridge_init(void)
{
}

void apbridge_deinit(void)
{
	for (size_t i = 0; i < AP_MAX_NODES; ++i) {
		ap_to_node[i].node_cport = 0;
		ap_to_node[i].node_intf = NULL;
	}

	node_to_ap_count = 0;
}

int connection_create(uint8_t intf1_id, uint16_t intf1_cport, uint8_t intf2_id,
		      uint16_t intf2_cport)
{
	struct gb_interface *intf;
	uint8_t node_id;
	uint16_t ap_cport, node_cport;
	int ret;

	if (intf1_id == AP_INF_ID) {
		node_id = intf2_id;
		node_cport = intf2_cport;
		ap_cport = intf1_cport;
	} else if (intf2_id == AP_INF_ID) {
		node_id = intf1_id;
		node_cport = intf1_cport;
		ap_cport = intf2_cport;
	} else {
		LOG_ERR("Cannot create connection between two non-AP");
		return -EINVAL;
	}

	intf = gb_interface_find_by_id(node_id);
	if (!intf) {
		LOG_ERR("Failed to find node interface");
		return -EINVAL;
	}

	ret = intf->create_connection(intf, node_cport);
	if (ret < 0) {
		LOG_ERR("Failed to create node connection");
		return ret;
	}

	ret = ap_to_node_add(ap_cport, node_cport, intf);
	if (ret < 0) {
		LOG_ERR("Failed to add AP to node");
		return ret;
	}

	ret = node_to_ap_add(node_id, node_cport, ap_cport);
	if (ret < 0) {
		LOG_ERR("Failed to add node to AP");
		ap_to_node_remove(ap_cport);
		return ret;
	}

	return 0;
}

int connection_destroy(uint8_t intf1_id, uint16_t intf1_cport, uint8_t intf2_id,
		       uint16_t intf2_cport)
{
	uint8_t node_id;
	uint16_t ap_cport, node_cport;
	struct gb_interface *intf;

	if (intf1_id == AP_INF_ID) {
		node_id = intf2_id;
		node_cport = intf2_cport;
		ap_cport = intf1_cport;
	} else if (intf2_id == AP_INF_ID) {
		node_id = intf1_id;
		node_cport = intf1_cport;
		ap_cport = intf2_cport;
	} else {
		LOG_ERR("Cannot destroy connection between two non-AP");
		return -EINVAL;
	}

	intf = gb_interface_find_by_id(node_id);
	if (!intf) {
		LOG_ERR("Failed to find node interface");
		return -EINVAL;
	}

	intf->destroy_connection(intf, node_cport);

	ap_to_node_remove(ap_cport);
	node_to_ap_remove(ap_cport);

	return 0;
}

int connection_send(uint8_t intf_id, uint16_t intf_cport, struct gb_message *msg)
{
	struct gb_interface *intf;
	int ret;

	if (intf_id == AP_INF_ID) {
		intf = ap_to_node[intf_cport].node_intf;
		ret = intf->write(intf, msg, ap_to_node[intf_cport].node_cport);
	} else {
		ret = node_to_ap_cport(intf_id, intf_cport);
		if (ret < 0) {
			LOG_ERR("Failed to find AP cport");
			goto early_exit;
		}

		ret = ap_send(msg, ret);
	}

early_exit:
	return ret;
}
