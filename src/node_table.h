/*
 * This file is ment to hold functions to maintain a table of active nodes.
 * THe functions defined here are not thread safe.
 */

#ifndef NODE_TABLE_H
#define NODE_TABLE_H

#include <stdbool.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#define MAX_NODE_TABLE_LEN CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

/* The return codes for the functions */
#define SUCCESS 0
#define E_NOT_FOUND 1
#define E_INVALID_CPORT_ALLOC 2

/*
 * Add a new node to the table.
 *
 * @returns true if successful, false in case of error
 */
bool node_table_add_node(const struct in6_addr *);

/*
 * Check if a node is already present in the table
 *
 * @param addr: IPV6 address of node.
 *
 * @returns true if successful, false in case of error
 */
bool node_table_is_active_by_addr(const struct in6_addr *);

/*
 * Remove the node from node table. This also closes all the cports for this
 * node.
 *
 * @param addr: IPV6 address of node.
 *
 * @returns true if successful, false in case of error
 */
bool node_table_remove_node_by_addr(const struct in6_addr *);

/*
 * Allocate memory for the cports for a node.
 *
 * @param addr: IPV6 address of node.
 * @param num_cports: number of cports. Note, this should not include Cport0.
 *
 * @returns true if successful, false in case of error
 */
bool node_table_alloc_cports_by_addr(const struct in6_addr *, size_t);

/*
 * Add Cport to a node in the table.
 *
 * @param sock: Socket for Cport0.
 * @param sock: Socket to access the cport.
 * @param cport_num: The cport to add the address to.
 *
 * @returns true if successful, false in case of error
 */
bool node_table_add_cport_by_cport0(int, int, size_t);

/*
 * Add Cport to a node in the table.
 *
 * @param sock: Socket for Cport0.
 * @param sock: Socket to access the cport.
 * @param cport_num: The cport to add the address to.
 *
 * @returns sock if successful, negative in case of error
 */
int node_table_add_cport_by_addr(const struct in6_addr *, int, size_t);

/*
 * Make a Cport inactive.
 * If the cport is Cport0, this will also remove the node.
 *
 * @param sock: Socket to access the cport
 *
 * @returns true if successful, false in case of error
 */
bool node_table_remove_cport_by_socket(int);

/*
 * Get all cports(sockets) that are currently in the table
 *
 * @param array: array to copy cports to.
 * @param array_len: length of array.
 *
 * @returns the number of cports write.
 */
size_t node_table_get_all_cports(int *, size_t);

/*
 * Get pollfd for all cports
 *
 * @param array: pollfd array to copy cports to.
 * @param array_len: length of array.
 *
 * @returns the number of cports write.
 */
size_t node_table_get_all_cports_pollfd(struct zsock_pollfd *, size_t);

#endif
