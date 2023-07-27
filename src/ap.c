/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ap.h"
#include "operations.h"

#define AP_MAX_NODES CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

struct ap_controller_data {
	struct k_fifo pending_read[AP_MAX_NODES];
};

static int ap_inf_write(struct gb_controller *ctrl, struct gb_message *msg, uint16_t cport_id)
{
	ARG_UNUSED(ctrl);

	memcpy(msg->header.pad, &cport_id, sizeof(uint16_t));
	gb_message_hdlc_send(msg);
	gb_message_dealloc(msg);
	return 0;
}

static struct gb_message *ap_inf_read(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct ap_controller_data *ctrl_data = ctrl->ctrl_data;

	return k_fifo_get(&ctrl_data->pending_read[cport_id], K_NO_WAIT);
}

static int ap_inf_create_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct ap_controller_data *ctrl_data = ctrl->ctrl_data;

	if (cport_id < AP_MAX_NODES) {
		k_fifo_init(&ctrl_data->pending_read[cport_id]);
		return 0;
	} else {
		return -1;
	}
}

static void ap_inf_destroy_connection(struct gb_controller *ctrl, uint16_t cport_id)
{
	struct gb_message *msg;
	struct ap_controller_data *ctrl_data = ctrl->ctrl_data;

	if (cport_id >= AP_MAX_NODES) {
		return;
	}

	msg = k_fifo_get(&ctrl_data->pending_read[cport_id], K_NO_WAIT);
	while (msg) {
		gb_message_dealloc(msg);
		msg = k_fifo_get(&ctrl_data->pending_read[cport_id], K_NO_WAIT);
	}
}

static struct ap_controller_data ap_ctrl_data;

static struct gb_interface intf = {.id = AP_INF_ID,
				   .controller = {
					   .write = ap_inf_write,
					   .read = ap_inf_read,
					   .create_connection = ap_inf_create_connection,
					   .destroy_connection = ap_inf_destroy_connection,
					   .ctrl_data = &ap_ctrl_data,
				   }};

struct gb_interface *ap_init(void)
{
	return &intf;
}

int ap_rx_submit(struct gb_message *msg)
{
	uint16_t cport_id;

	memcpy(&cport_id, msg->header.pad, sizeof(uint16_t));
	k_fifo_put(&ap_ctrl_data.pending_read[cport_id], msg);
	return 0;
}

struct gb_interface *ap_interface(void)
{
	return &intf;
}
