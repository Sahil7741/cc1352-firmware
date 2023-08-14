/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _GREYBUS_CONNECTIONS_H_
#define _GREYBUS_CONNECTIONS_H_

#include <zephyr/types.h>
#include <zephyr/sys/dlist.h>
#include "greybus_interfaces.h"

#define MAX_GREYBUS_CONNECTIONS CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

/*
 * A connection between two greybus interfaces
 *
 * @param inf_ap: Greybus interface of AP.
 * @param inf_peer: Greybus interface of the peer
 * @param ap_cport_id: Cport of AP to connect to.
 * @param peer_cport_id: Cport of Peer to connect to.
 */
struct gb_connection {
	struct gb_interface *inf_ap;
	struct gb_interface *inf_peer;
	uint16_t ap_cport_id;
	uint16_t peer_cport_id;
	sys_dnode_t node;
};

/*
 * Create a greybus connection between two interfaces
 *
 * @param Greybus AP Interface
 * @param Greybus Peer Interface
 * @param Greybus AP Interface Cport ID
 * @param Greybus Peer Interface Cport ID
 *
 * @return greybus connection allocated on heap. Null in case of errro
 */
struct gb_connection *gb_connection_create(struct gb_interface *intf1, struct gb_interface *intf2,
					   uint16_t intf1_cport_id, uint16_t intf2_cport_id);

/*
 * Destroy greybus connection
 *
 * @param interface 1
 * @param interface 2
 * @param interface 1 cport
 * @param interface 2 cport
 *
 * @return 0 on success. Negative in case of error
 */
int gb_connection_destroy(struct gb_interface *intf1, struct gb_interface *intf2,
			  uint16_t intf1_cport_id, uint16_t intf2_cport_id);

/*
 * Execute a function on all active connections
 */
void gb_connection_process_all();

/*
 * Remove all greybus connections
 *
 * Note: This does not remove the interfaces
 */
void gb_connection_destroy_all(void);

/*
 * Destroy all connections related to an interface
 *
 * @param greybus interface
 */
void gb_connection_destroy_by_interface(struct gb_interface *intf);

#endif
