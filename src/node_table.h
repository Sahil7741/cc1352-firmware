/*
 * This file is ment to hold functions to maintain a table of active nodes
 */

#ifndef NODE_TABLE_H
#define NODE_TABLE_H

#include <stdbool.h>
#include <zephyr/net/net_ip.h>


#define MAX_NODE_TABLE_LEN CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

/*
 * Add a new node to the table
 *
 * @returns true if successful, false in case of error
 */
bool node_table_add_node(const struct in6_addr*);

/*
 * Check if a node is already present in the table
 *
 * @returns true if successful, false in case of error
 */
bool node_table_is_active(const struct in6_addr*);

/*
 * Remove the node from node table
 *
 * @returns true if successful, false in case of error
 */
bool node_table_remove_node(const struct in6_addr*);

#endif
