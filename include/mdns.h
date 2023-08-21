/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _MDNS_H
#define _MDNS_H

#include <zephyr/net/socket.h>

static const struct in6_addr mdns_addr = {
	{{0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFB}}};

int mdns_socket_open_ipv6(const struct in6_addr *saddr);

void mdns_socket_close(int sock);

int mdns_query_send(int sock, const char *name, size_t length);

size_t mdns_query_recv(int sock, struct in6_addr *addr_list, size_t addr_list_len,
		       const char *query, size_t query_len, int timeout);

#endif
