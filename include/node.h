#ifndef _NODE_H_
#define _NODE_H_

#include <zephyr/net/net_ip.h>

#define GB_TRANSPORT_TCPIP_BASE_PORT 4242

struct gb_interface *node_find_by_addr(struct in6_addr *);

struct gb_interface *node_create_interface(struct in6_addr *);

struct gb_interface *node_find_by_id(uint8_t);

#endif
