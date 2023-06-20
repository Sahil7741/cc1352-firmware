/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define NODE_DISCOVERY_INTERVAL 5000
#define MAX_GREYBUS_NODES CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES
#define GB_TRANSPORT_TCPIP_BASE_PORT 4242

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

K_MSGQ_DEFINE(uart_msgq, sizeof(char), 10, 4);
K_MSGQ_DEFINE(node_discovery_msgq, sizeof(struct in6_addr), 10, 4);

static struct in6_addr greybus_nodes[MAX_GREYBUS_NODES];
static size_t greybus_nodes_pos = 0;
K_MUTEX_DEFINE(greybus_nodes_mutex);

void node_discovery_entry(void *, void *, void *);
void node_handler_entry(void *, void *, void *);

// Thread responseible for beagleconnect node discovery.
K_THREAD_DEFINE(node_discovery, 1024, node_discovery_entry, NULL, NULL, NULL, 5,
                0, 0);

K_THREAD_DEFINE(node_handler, 2048, node_handler_entry, NULL, NULL, NULL, 5, 0,
                0);

// Add node to the active nodes
static bool add_node(const struct in6_addr *node_addr) {
  if (greybus_nodes_pos >= MAX_GREYBUS_NODES) {
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

// Check if node is active
static bool is_node_active(const struct in6_addr *node_addr) {
  return find_node(node_addr) != -1;
}

// Remove deactive nodes
static bool remove_node(const struct in6_addr *node_addr) {
  if (greybus_nodes_pos <= 0) {
    return false;
  }

  int pos = find_node(node_addr);
  if (pos == -1) {
    return false;
  }

  greybus_nodes_pos--;
  memcpy(&greybus_nodes[pos], &greybus_nodes[greybus_nodes_pos],
         sizeof(struct in6_addr));
  return true;
}

void print_sockaddr(const struct sockaddr *addr) {
  if (addr->sa_family == AF_INET) {
    const struct sockaddr_in *a = (struct sockaddr_in *)addr;
    LOG_DBG("IPV4 addr: Port %u, Address %d.%d.%d.%d", a->sin_port,
            a->sin_addr.s4_addr[0], a->sin_addr.s4_addr[1],
            a->sin_addr.s4_addr[2], a->sin_addr.s4_addr[3]);
  } else if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6 *a = (struct sockaddr_in6 *)addr;
    LOG_DBG("IPV6 addr: Port %u, Address "
            "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%"
            "02x%02x",
            a->sin6_port, a->sin6_addr.s6_addr[0], a->sin6_addr.s6_addr[1],
            a->sin6_addr.s6_addr[2], a->sin6_addr.s6_addr[3],
            a->sin6_addr.s6_addr[4], a->sin6_addr.s6_addr[5],
            a->sin6_addr.s6_addr[6], a->sin6_addr.s6_addr[7],
            a->sin6_addr.s6_addr[8], a->sin6_addr.s6_addr[9],
            a->sin6_addr.s6_addr[10], a->sin6_addr.s6_addr[11],
            a->sin6_addr.s6_addr[12], a->sin6_addr.s6_addr[13],
            a->sin6_addr.s6_addr[14], a->sin6_addr.s6_addr[15]);
  }
}

// This function probes for all greybus nodes.
// Currently just using static IP for nodes.
//
// @return number of discovered nodes
int get_all_nodes(struct in6_addr *node_array, const size_t node_array_len) {
  if (node_array_len < 1) {
    return -1;
  }

  memset(&node_array[0], 0, sizeof(struct in6_addr));
  inet_pton(AF_INET6, CONFIG_NET_CONFIG_PEER_IPV6_ADDR, &node_array[0]);

  return 1;
}

void node_discovery_entry(void *p1, void *p2, void *p3) {
  // Peform node discovery in infinte loop
  int ret;
  struct in6_addr node_array[MAX_GREYBUS_NODES];

  while (1) {
    ret = get_all_nodes(node_array, MAX_GREYBUS_NODES);
    if (ret < 0) {
      LOG_WRN("Failed to get greybus nodes");
      continue;
    }
    LOG_INF("Discoverd %u nodes", ret);

    for (size_t i = 0; i < ret; ++i) {
      k_mutex_lock(&greybus_nodes_mutex, K_FOREVER);
      if (!is_node_active(&node_array[i])) {
        if (!add_node(&node_array[i])) {
          LOG_WRN("Failed to add node");
        } else {
          LOG_INF("Added Greybus Node");
          k_msgq_put(&node_discovery_msgq, &node_array[i], K_FOREVER);
        }
      }
      k_mutex_unlock(&greybus_nodes_mutex);
    }

    // Put the thread to sleep for an interval
    LOG_DBG("Going to sleep");
    k_msleep(NODE_DISCOVERY_INTERVAL);
  }
}

int connect_to_node(const struct sockaddr *addr) {
  int ret, sock;
  size_t addr_size;

  if (addr->sa_family == AF_INET6) {
    sock = zsock_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    addr_size = sizeof(struct sockaddr_in6);
  } else {
    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr_size = sizeof(struct sockaddr_in);
  }

  if (sock < 0) {
    LOG_WRN("Failed to create socket %d", errno);
    return -1;
  }

  LOG_INF("Trying to connect to node from socket %d", sock);
  print_sockaddr(addr);

  ret = zsock_connect(sock, addr, addr_size);

  if (ret) {
    LOG_WRN("Failed to connect to node %d", errno);
    zsock_close(sock);
    return -1;
  }

  LOG_INF("Connected to Greybus Node");
  return sock;
}

void node_handler_entry(void *p1, void *p2, void *p3) {
  int ret;
  struct sockaddr_in6 addr;
  struct zsock_pollfd fds[MAX_GREYBUS_NODES];
  size_t fds_len = 0;
  size_t i;

  addr.sin6_port = htons(GB_TRANSPORT_TCPIP_BASE_PORT);
  addr.sin6_family = AF_INET6;
  addr.sin6_scope_id = 0;

  while (1) {
    // Check messageque for new nodes
    ret = k_msgq_get(&node_discovery_msgq, &addr.sin6_addr, K_MSEC(500));
    if (ret == 0) {
      ret = connect_to_node((struct sockaddr *)&addr);
      if (ret >= 0) {
        fds[fds_len].fd = ret;
        fds[fds_len].events = ZSOCK_POLLIN | ZSOCK_POLLOUT;
        fds_len++;
      } else {
        k_mutex_lock(&greybus_nodes_mutex, K_FOREVER);
        remove_node(&addr.sin6_addr);
        k_mutex_unlock(&greybus_nodes_mutex);
      }
    }

    // Reset events for all sockets
    for (i = 0; i < fds_len; ++i) {
      fds[i].events = ZSOCK_POLLIN | ZSOCK_POLLOUT;
    }

    // Poll all active nodes
    LOG_DBG("Poll Sockets %u", fds_len);
    ret = zsock_poll(fds, fds_len, 500);
    if (ret > 0) {
      for (size_t i = 0; i < fds_len; ++i) {
        if (fds[i].revents & ZSOCK_POLLIN) {
          LOG_DBG("Some data is available to be read");
        }
        if (fds[i].revents & ZSOCK_POLLOUT) {
          LOG_DBG("Data can be written");
        }
      }
    }

    // Handle all active nodes
  }
}


void serial_callback(const struct device *dev, void *user_data) {
  char c;

  if (!uart_irq_update(uart_dev)) {
    return;
  }

  if (!uart_irq_rx_ready(uart_dev)) {
    return;
  }

  while (uart_fifo_read(uart_dev, &c, 1) == 1) {
    if (c == '1' || c == '2') {
      k_msgq_put(&uart_msgq, &c, K_NO_WAIT);
    } else {
      LOG_DBG("Invalid Input: %u", c);
    }
  }
}

void print_uart(char *buf) {
  int msg_len = strlen(buf);

  for (int i = 0; i < msg_len; i++) {
    uart_poll_out(uart_dev, buf[i]);
  }
}

void main(void) {
  LOG_INF("Starting BeaglePlay Greybus");
  int ret;

  char tx;

  if (!device_is_ready(uart_dev)) {
    LOG_ERR("UART device not found!");
    return;
  }

  ret = uart_irq_callback_user_data_set(uart_dev, serial_callback, NULL);
  if (ret < 0) {
    if (ret == -ENOTSUP) {
      LOG_ERR("Interrupt-driven UART API support not enabled\n");
    } else if (ret == -ENOSYS) {
      LOG_ERR("UART device does not support interrupt-driven API\n");
    } else {
      LOG_ERR("Error setting UART callback: %d\n", ret);
    }
    return;
  }

  uart_irq_rx_enable(uart_dev);

  while (k_msgq_get(&uart_msgq, &tx, K_FOREVER) == 0) {
    switch (tx) {
    case '1': {
      k_mutex_lock(&greybus_nodes_mutex, K_FOREVER);
      greybus_nodes_pos = 0;
      k_mutex_unlock(&greybus_nodes_mutex);
      LOG_ERR("Reset Greybus Nodes");
      break;
    }
    }
    LOG_DBG("Pressed: %c", tx);
  }
}
