/*
 * This file contains functions for managing the nodes.
 */

#ifndef NODE_HANDLER_H
#define NODE_HANDLER_H

#include <stdint.h>
#include "error_handling.h"
#include <zephyr/net/socket.h>

/*
 * Queue a new node to be setup. This function is async and thus does not do the
 * setup itself. This will only add the node to global queue.
 *
 * @param IPv6 address of the node.
 * @param cport number to connect to.
 *
 * @return 0 if success, negative for failure.
 */
int node_handler_setup_node_queue(const struct in6_addr *, uint8_t);

#endif
