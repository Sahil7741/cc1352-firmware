// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "ap.h"
#include "hdlc.h"
#include "mcumgr.h"
#include "node.h"
#include "operations.h"
#include "svc.h"
#include <stdbool.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/dlist.h>

#define UART_DEVICE_NODE        DT_CHOSEN(zephyr_shell_uart)
#define NODE_DISCOVERY_INTERVAL 5000
#define MAX_GREYBUS_NODES       CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

LOG_MODULE_REGISTER(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static void apbridge_entry(void *, void *, void *);

K_THREAD_DEFINE(apbridge, 2048, apbridge_entry, NULL, NULL, NULL, 6, 0, 0);

static void connection_callback(struct gb_connection *conn)
{
	struct gb_message *msg;

	msg = conn->inf_ap->controller.read(&conn->inf_ap->controller, conn->ap_cport_id);
	if (msg != NULL) {
		conn->inf_peer->controller.write(&conn->inf_peer->controller, msg,
						 conn->peer_cport_id);
	}

	msg = conn->inf_peer->controller.read(&conn->inf_peer->controller, conn->peer_cport_id);
	if (msg != NULL) {
		conn->inf_ap->controller.write(&conn->inf_ap->controller, msg, conn->ap_cport_id);
	}
}

static void apbridge_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* Go through all connections */
		gb_connections_process_all(connection_callback);
		k_yield();
	}
}

/* This function probes for all greybus nodes.
 * Currently just using static IP for nodes.
 *
 * @return number of discovered nodes
 */
static int get_all_nodes(struct in6_addr *node_array, const size_t node_array_len)
{
	if (node_array_len < 1) {
		return -1;
	}

	memset(&node_array[0], 0, sizeof(struct in6_addr));
	inet_pton(AF_INET6, CONFIG_NET_CONFIG_PEER_IPV6_ADDR, &node_array[0]);

	return 1;
}

static void serial_callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	uint8_t *buf;
	int ret;

	if (!uart_irq_update(dev) && !uart_irq_rx_ready(dev)) {
		return;
	}

	ret = hdlc_rx_start(&buf);
	if (ret == 0) {
		/* No space */
		LOG_ERR("No more space for HDLC receive");
		return;
	}

	ret = uart_fifo_read(dev, buf, ret);
	if (ret < 0) {
		/* Something went wrong */
		LOG_ERR("Failed to read UART");
		return;
	}

	ret = hdlc_rx_finish(ret);
	if (ret < 0) {
		/* Some error */
		LOG_ERR("Filed to write data to hdlc buffer");
		return;
	}
}

static int hdlc_process_greybus_frame(const char *buffer, size_t buffer_len)
{
	struct gb_operation_msg_hdr hdr;
	struct gb_message *msg;
	size_t payload_size;

	memcpy(&hdr, buffer, sizeof(struct gb_operation_msg_hdr));

	if (hdr.size > buffer_len) {
		LOG_ERR("Greybus Message size is greater than received buffer.");
		return -1;
	}

	payload_size = hdr.size - sizeof(struct gb_operation_msg_hdr);
	msg = k_malloc(sizeof(struct gb_message) + payload_size);
	if (msg == NULL) {
		LOG_ERR("Failed to allocate greybus message");
		return -1;
	}
	msg->payload_size = payload_size;
	memcpy(&msg->header, &hdr, sizeof(struct gb_operation_msg_hdr));
	memcpy(msg->payload, &buffer[sizeof(struct gb_operation_msg_hdr)], msg->payload_size);
	ap_rx_submit(msg);

	return 0;
}

static int hdlc_process_complete_frame(const void *buffer, size_t len, uint8_t address)
{
	switch (address) {
	case ADDRESS_GREYBUS:
		return hdlc_process_greybus_frame(buffer, len);
	case ADDRESS_MCUMGR:
		return mcumgr_process_frame(buffer, len);
	case ADDRESS_DBG:
		LOG_WRN("Ignore DBG Frame");
		return 0;
	}

	return -1;
}

void main(void)
{
	int ret;
	struct gb_connection *conn;
	struct in6_addr node_array[MAX_GREYBUS_NODES];
	struct gb_interface *intf;

	LOG_INF("Starting BeaglePlay Greybus");

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not found!");
		return;
	}

	mcumgr_init();

	hdlc_init(hdlc_process_complete_frame);
	struct gb_interface *ap = ap_init();
	struct gb_interface *svc = svc_init();

	conn = gb_create_connection(ap, svc, 0, 0);

	ret = uart_irq_callback_user_data_set(uart_dev, serial_callback, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			LOG_ERR("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			LOG_ERR("UART device does not support interrupt-driven API\n");
		} else {
			LOG_ERR("Error setting UART callback: %d\n", ret);
		}
		return;
	}

	uart_irq_rx_enable(uart_dev);

	svc_send_version();

	/* Wait until SVC is ready */
	while (!svc_is_ready()) {
		k_sleep(K_MSEC(NODE_DISCOVERY_INTERVAL));
	}

	while (1) {
		LOG_DBG("Try Node Discovery");
		ret = get_all_nodes(node_array, MAX_GREYBUS_NODES);
		if (ret < 0) {
			LOG_WRN("Failed to get greybus nodes");
			continue;
		}

		for (size_t i = 0; i < ret; ++i) {
			intf = node_find_by_addr(&node_array[i]);
			if (!intf) {
				LOG_DBG("Found new node");
				intf = node_create_interface(&node_array[i]);
				svc_send_module_inserted(intf->id);
			}
		}

		/* Put the thread to sleep for an interval */
		k_msleep(NODE_DISCOVERY_INTERVAL);
	}
}
