// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "tcp_discovery.h"
#include "node.h"
#include "mdns.h"
#include <zephyr/kernel.h>

#define MAX_GREYBUS_NODES CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct tcp_discovery_data {
	int sock;
};

static struct tcp_discovery_data tcp_discovery_data = {.sock = -1};

static void handler(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;
	struct in6_addr node_array[MAX_GREYBUS_NODES];
	char query[] = "_greybus._tcp.local\0";

	while (1) {
		k_msleep(NODE_DISCOVERY_INTERVAL);

		if (tcp_discovery_data.sock < 0) {
			continue;
		}

		ret = mdns_query_send(tcp_discovery_data.sock, query, strlen(query));
		if (ret < 0) {
			LOG_WRN("Failed to get greybus nodes");
			continue;
		}

		ret = mdns_query_recv(tcp_discovery_data.sock, node_array, MAX_GREYBUS_NODES, query,
				      strlen(query), 2000);
		if (ret < 0) {
			LOG_WRN("Failed to get greybus nodes");
			continue;
		}

		node_filter(node_array, ret);
	}
}

K_THREAD_DEFINE(tcp_discovery, 2048, handler, NULL, NULL, NULL, 6, 0, 0);

void tcp_discovery_start(void)
{
	tcp_discovery_data.sock = mdns_socket_open_ipv6(&mdns_addr);
	k_thread_resume(tcp_discovery);
}

void tcp_discovery_stop(void)
{
	k_thread_suspend(tcp_discovery);
	if (tcp_discovery_data.sock >= 0) {
		mdns_socket_close(tcp_discovery_data.sock);
		tcp_discovery_data.sock = -1;
	}
}
