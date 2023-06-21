#include "node_table.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static struct in6_addr greybus_nodes[MAX_NODE_TABLE_LEN];
static size_t greybus_nodes_pos = 0;

bool node_table_add_node(const struct in6_addr *node_addr) {
  if (greybus_nodes_pos >= MAX_NODE_TABLE_LEN) {
    LOG_WRN("Reached max greybus nodes limit");
    return false;
  }

  memcpy(&greybus_nodes[greybus_nodes_pos], node_addr, sizeof(struct in6_addr));
  greybus_nodes_pos++;
  return true;
}

static int sock_addr_cmp_addr(const struct in6_addr *sa,
                              const struct in6_addr *sb) {
  return memcmp(sa, sb, sizeof(struct in6_addr));
}

static int find_node(const struct in6_addr *node_addr) {
  for (size_t i = 0; i < greybus_nodes_pos; ++i) {
    if (sock_addr_cmp_addr(node_addr, &greybus_nodes[i]) == 0) {
      return i;
    }
  }
  return -1;
}

bool node_table_is_active(const struct in6_addr *node_addr) {
  return find_node(node_addr) != -1;
}

bool node_table_remove_node(const struct in6_addr *node_addr) {
  if (greybus_nodes_pos <= 0) {
    return false;
  }

  int pos = find_node(node_addr);
  if (pos == -1) {
    return false;
  }

  greybus_nodes_pos--;
  if (pos != greybus_nodes_pos) {
    memcpy(&greybus_nodes[pos], &greybus_nodes[greybus_nodes_pos],
           sizeof(struct in6_addr));
  }
  return true;
}
