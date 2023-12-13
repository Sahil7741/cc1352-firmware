// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "greybus_interfaces.h"
#include "ap.h"
#include "node.h"
#include "svc.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/dlist.h>

#define INTERFACE_ID_START     3
#define MAX_GREYBUS_INTERFACES CONFIG_BEAGLEPLAY_GREYBUS_MAX_INTERFACES

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

K_MEM_SLAB_DEFINE_STATIC(gb_interface_slab, sizeof(struct gb_interface), MAX_GREYBUS_INTERFACES, 8);

static atomic_t interface_id_counter = ATOMIC_INIT(INTERFACE_ID_START);

static uint8_t new_interface_id(void)
{
	atomic_val_t temp = atomic_inc(&interface_id_counter);

	if (temp == UINT8_MAX) {
		atomic_set(&interface_id_counter, INTERFACE_ID_START);
	}
	return temp;
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

struct gb_interface *gb_interface_find_by_id(uint8_t intf_id)
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
