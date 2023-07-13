/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NODE_H_
#define _NODE_H_

#include <zephyr/net/net_ip.h>

#define GB_TRANSPORT_TCPIP_BASE_PORT 4242

/*
 * Find tcp greybus node by IPv6 address
 *
 * @param address to search for
 */
struct gb_interface *node_find_by_addr(struct in6_addr *);

/*
 * Destroy a tcp greybus inteface
 *
 * @return greybus interface
 */
void node_destroy_interface(struct gb_interface *);

/*
 * Create a new tcp greybus node interface
 *
 * @param IPv6 address of new interface
 *
 * @return allocated greybus interface
 */
struct gb_interface *node_create_interface(struct in6_addr *);

/*
 * Find greybus node by interface ID
 *
 * @param interface id
 *
 * @return greybus interface
 */
struct gb_interface *node_find_by_id(uint8_t);

#endif
