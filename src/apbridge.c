// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "ap.h"
#include "apbridge.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "greybus_interfaces.h"

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct node_ap_item {
	uint8_t node_id;
	uint16_t node_cport;
	struct gb_interface *node_intf;
};

static struct node_ap_item node_ap_map[AP_MAX_NODES] = {0};

static int node_ap_add(uint16_t ap_cport, uint16_t node_cport, struct gb_interface *node_intf)
{
	if (ap_cport >= AP_MAX_NODES) {
		return -E2BIG;
	}

	if (node_ap_map[ap_cport].node_intf) {
		return -EALREADY;
	}

	node_ap_map[ap_cport].node_id = node_intf->id;
	node_ap_map[ap_cport].node_cport = node_cport;
	node_ap_map[ap_cport].node_intf = node_intf;

	return 0;
}

static int node_ap_remove(uint16_t ap_cport)
{
	if (ap_cport >= AP_MAX_NODES) {
		return -E2BIG;
	}

	node_ap_map[ap_cport].node_id = 0;
	node_ap_map[ap_cport].node_cport = 0;
	node_ap_map[ap_cport].node_intf = NULL;

	return 0;
}

static int node_to_ap_cport(uint8_t node_id, uint16_t node_cport)
{
	for (size_t i = 0; i < AP_MAX_NODES; i++) {
		if (node_ap_map[i].node_id == node_id && node_ap_map[i].node_cport == node_cport) {
			return i;
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
		node_ap_map[i].node_id = 0;
		node_ap_map[i].node_cport = 0;
		node_ap_map[i].node_intf = NULL;
	}
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

	ret = node_ap_add(ap_cport, node_cport, intf);
	if (ret < 0) {
		LOG_ERR("Failed to add AP to node");
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

	node_ap_remove(ap_cport);

	return 0;
}

int connection_send(uint8_t intf_id, uint16_t intf_cport, struct gb_message *msg)
{
	struct gb_interface *intf;
	int ret;

	if (intf_id == AP_INF_ID) {
		intf = node_ap_map[intf_cport].node_intf;
		ret = intf->write(intf, msg, node_ap_map[intf_cport].node_cport);
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
