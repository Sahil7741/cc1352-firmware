#include "node_table.h"
#include "error_handling.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/dlist.h>
#include <zephyr/sys/mutex.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

K_MUTEX_DEFINE(nodes_table_mutex);

struct node_table_item {
  struct in6_addr addr;
  int cport0;
  size_t num_cports; /* Number of cports. This does not include Cport0 */
  int *cports;
};

static struct node_table_item nodes_table[MAX_NODE_TABLE_LEN];
static size_t nodes_pos = 0;

static int sock_addr_cmp_addr(const struct in6_addr *sa,
                              const struct in6_addr *sb) {
  return memcmp(sa, sb, sizeof(struct in6_addr));
}

static int find_node_by_addr(const struct in6_addr *node_addr) {
  for (size_t i = 0; i < nodes_pos; ++i) {
    if (sock_addr_cmp_addr(node_addr, &nodes_table[i].addr) == 0) {
      return i;
    }
  }
  return -1;
}

static int find_node_by_cport0(const int cport0) {
  for (size_t i = 0; i < nodes_pos; ++i) {
    if (nodes_table[i].cport0 == cport0) {
      return i;
    }
  }
  return -1;
}

static void deinit_node(struct node_table_item *node) {
  size_t i;
  // Close all the sockets
  if (node->cport0 >= 0) {
    zsock_close(node->cport0);
  }
  for (i = 0; i < node->num_cports; ++i) {
    if (node->cports[i] >= 0) {
      zsock_close(node->cports[i]);
    }
  }
  k_free(node->cports);
}

static void remove_node_by_pos(const size_t pos) {
  deinit_node(&nodes_table[pos]);

  nodes_pos--;
  if (pos != nodes_pos) {
    memcpy(&nodes_table[pos], &nodes_table[nodes_pos],
           sizeof(struct node_table_item));
  }
}

int node_table_add_node(const struct in6_addr *node_addr) {
  if (nodes_pos >= MAX_NODE_TABLE_LEN) {
    LOG_WRN("Reached max greybus nodes limit");
    return -E_TABLE_FULL;
  }

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  memcpy(&nodes_table[nodes_pos].addr, node_addr, sizeof(struct in6_addr));
  nodes_table[nodes_pos].cport0 = -1;
  nodes_table[nodes_pos].num_cports = 0;
  nodes_table[nodes_pos].cports = NULL;
  nodes_pos++;
  k_mutex_unlock(&nodes_table_mutex);

  return SUCCESS;
}

bool node_table_is_active_by_addr(const struct in6_addr *node_addr) {
  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  bool ret = find_node_by_addr(node_addr) >= 0;
  k_mutex_unlock(&nodes_table_mutex);

  return ret;
}

int node_table_remove_node_by_addr(const struct in6_addr *node_addr) {
  int ret;
  if (nodes_pos <= 0) {
    return -E_EMPTY;
  }

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  int pos = find_node_by_addr(node_addr);
  if (pos < 0) {
    ret = -E_NOT_FOUND;
    goto early_fail;
  }

  remove_node_by_pos(pos);
  k_mutex_unlock(&nodes_table_mutex);

  return SUCCESS;

early_fail:
  k_mutex_unlock(&nodes_table_mutex);
  return ret;
}

int node_table_alloc_cports_by_addr(const struct in6_addr *node_addr,
                                     size_t num_cports) {
  size_t i;
  int ret;

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  int pos = find_node_by_addr(node_addr);
  if (pos < 0) {
    ret = -E_NOT_FOUND;
    goto early_fail;
  }

  nodes_table[pos].cports = k_malloc(sizeof(int) * num_cports);
  if (nodes_table[pos].cports == NULL) {
    ret = -E_NO_HEAP_MEM;
    goto early_fail;
  }

  // Set all sockets to be invalid for now.
  for (i = 0; i < num_cports; ++i) {
    nodes_table[pos].cports[i] = -1;
  }

  nodes_table[pos].num_cports = num_cports;

  k_mutex_unlock(&nodes_table_mutex);
  return SUCCESS;

early_fail:
  k_mutex_unlock(&nodes_table_mutex);
  return ret;
}

int node_table_add_cport_by_cport0(int cport0, int sock, size_t cport_num) {
  int ret;

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  int pos = find_node_by_cport0(cport0);
  if (pos == -1) {
    ret = -E_NOT_FOUND;
    goto early_fail;
  }

  if (nodes_table[pos].num_cports <= 0) {
    LOG_WRN("Cports for socket %d have not been initialized", cport0);
    ret = -E_UNINITIALIZED_CPORT;
    goto early_fail;
  }

  if (nodes_table[pos].num_cports < cport_num) {
    LOG_WRN("Cport num %u exceeds the allocated cports", cport_num);
    ret = -E_INVALID_CPORT_ALLOC;
    goto early_fail;
  }

  nodes_table[pos].cports[cport_num - 1] = sock;

  k_mutex_unlock(&nodes_table_mutex);
  return SUCCESS;

early_fail:
  k_mutex_unlock(&nodes_table_mutex);
  return ret;
}

int node_table_remove_cport_by_socket(int sock) {
  size_t i, j;
  if (nodes_pos <= 0) {
    return -E_NOT_FOUND;
  }

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  for (i = 0; i < nodes_pos; ++i) {
    if (nodes_table[i].cport0 == sock) {
      // Remove node
      remove_node_by_pos(i);
      goto success;
    }

    for (j = 0; j < nodes_table[i].num_cports; ++j) {
      if (nodes_table[i].cports[j] == sock) {
        // Remove cport
        zsock_close(nodes_table[i].cports[j]);
        nodes_table[i].cports[j] = -1;
        goto success;
      }
    }
  }

  k_mutex_unlock(&nodes_table_mutex);
  return -E_NOT_FOUND;

success:
  k_mutex_unlock(&nodes_table_mutex);
  return SUCCESS;
}

size_t node_table_get_all_cports(int *arr, size_t arr_len) {
  size_t i, j;
  size_t count = 0;

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  for (i = 0; i < nodes_pos && count < arr_len; ++i) {
    if (nodes_table[i].cport0 >= 0) {
      arr[count] = nodes_table[i].cport0;
      count++;
    }

    for (j = 0; j < nodes_table[i].num_cports && count < arr_len; ++j) {
      if (nodes_table[i].cports[j] >= 0) {
        arr[count] = nodes_table[i].cports[j];
        count++;
      }
    }
  }

  k_mutex_unlock(&nodes_table_mutex);
  return count;
}

size_t node_table_get_all_cports_pollfd(struct zsock_pollfd *arr,
                                        size_t arr_len) {
  size_t i, j;
  size_t count = 0;

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  for (i = 0; i < nodes_pos && count < arr_len; ++i) {
    if (nodes_table[i].cport0 >= 0) {
      arr[count].fd = nodes_table[i].cport0;
      count++;
    }

    for (j = 0; j < nodes_table[i].num_cports && count < arr_len; ++j) {
      if (nodes_table[i].cports[j] >= 0) {
        arr[count].fd = nodes_table[i].cports[j];
        count++;
      }
    }
  }

  k_mutex_unlock(&nodes_table_mutex);
  return count;
}

int node_table_add_cport_by_addr(const struct in6_addr *node_addr, int sock,
                                 size_t cport_num) {
  int ret;

  k_mutex_lock(&nodes_table_mutex, K_FOREVER);
  int pos = find_node_by_addr(node_addr);
  if (pos < 0) {
    ret = -E_NOT_FOUND;
    goto early_fail;
  }

  if (cport_num == 0) {
    nodes_table[pos].cport0 = sock;
    goto success;
  }

  if (nodes_table[pos].num_cports <= cport_num) {
    ret = -E_INVALID_CPORT_ALLOC;
    goto early_fail;
  }

  nodes_table[pos].cports[cport_num - 1] = sock;

success:
  k_mutex_unlock(&nodes_table_mutex);
  return sock;

early_fail:
  k_mutex_unlock(&nodes_table_mutex);
  return ret;
}
