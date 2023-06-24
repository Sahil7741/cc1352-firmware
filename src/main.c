/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_protocol.h"
#include "node_table.h"
#include "svc.h"
#include "zephyr/sys/dlist.h"
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

static sys_dlist_t greybus_operations_list =
    SYS_DLIST_STATIC_INIT(&greybus_operations_list);

K_MSGQ_DEFINE(uart_msgq, sizeof(char), 10, 4);
K_MSGQ_DEFINE(socket_msgq, sizeof(int), 10, 4);
K_MSGQ_DEFINE(discovered_node_msgq, sizeof(struct in6_addr), 10, 4);

K_MUTEX_DEFINE(greybus_nodes_mutex);
K_MUTEX_DEFINE(greybus_operations_mutex);

void node_discovery_entry(void *, void *, void *);
void node_manager_entry(void *, void *, void *);
void node_setup_entry(void *, void *, void *);

// Thread responsible for beagleconnect node discovery.
K_THREAD_DEFINE(node_discovery, 1024, node_discovery_entry, NULL, NULL, NULL, 5,
                0, 0);

// Thread responsible for reading and writing to greybus nodes
K_THREAD_DEFINE(node_manager, 2048, node_manager_entry, NULL, NULL, NULL, 5, 0,
                0);

// Thread responsible for setting up a newly discovered node
K_THREAD_DEFINE(node_setup, 2048, node_setup_entry, NULL, NULL, NULL, 5, 0, 0);

void node_manager_entry(void *p1, void *p2, void *p3) {
  struct zsock_pollfd fds[5];
  size_t len = 0;
  size_t i;
  int ret;
  struct gb_operation *op, *op_safe;
  struct gb_message *msg;

  while (1) {
    if (k_msgq_get(&socket_msgq, &fds[len], K_MSEC(100)) == 0) {
      len++;
    }

    for (i = 0; i < len; ++i) {
      fds[i].events = POLLIN | POLLOUT | POLLHUP;
    }

    ret = zsock_poll(fds, len, 100);
    if (ret > 0) {
      for (i = 0; i < len; ++i) {
        if (fds[i].revents & ZSOCK_POLLIN) {
          msg = greybus_recieve_message(fds[i].fd);
          if (msg != NULL) {
            k_mutex_lock(&greybus_operations_mutex, K_FOREVER);
            SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&greybus_operations_list, op, op_safe, node) {
              if (!op->response_recieved &&
                  msg->header.id == op->operation_id) {
                op->response_recieved = true;
                op->response = msg;
                sys_dlist_remove(&op->node);
                LOG_DBG("Operation with ID %u completed", msg->header.id);
              }
            }
            k_mutex_unlock(&greybus_operations_mutex);
          }
        }
        if (fds[i].revents & ZSOCK_POLLOUT) {
          k_mutex_lock(&greybus_operations_mutex, K_FOREVER);
          SYS_DLIST_FOR_EACH_CONTAINER(&greybus_operations_list, op, node) {
            if (op->request_sent == false && op->sock == fds[i].fd) {
              ret = greybus_send_message(op->request);
              if (ret == 0) {
                LOG_DBG("Request sent");
                op->request_sent = true;
              }
            }
          }
          k_mutex_unlock(&greybus_operations_mutex);
        }
      }
    }
  }
}

static int connect_to_node(const struct sockaddr *addr) {
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

  ret = zsock_connect(sock, addr, addr_size);

  if (ret) {
    LOG_WRN("Failed to connect to node %d", errno);
    zsock_close(sock);
    return -1;
  }

  LOG_INF("Connected to Greybus Node");
  return sock;
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

void node_setup_entry(void *p1, void *p2, void *p3) {
  struct sockaddr_in6 node_addr;
  int ret;

  while (k_msgq_get(&discovered_node_msgq, &node_addr.sin6_addr, K_FOREVER) ==
         0) {
    node_addr.sin6_family = AF_INET6;
    node_addr.sin6_scope_id = 0;
    node_addr.sin6_port = htons(GB_TRANSPORT_TCPIP_BASE_PORT);

    ret = connect_to_node((struct sockaddr *)&node_addr);
    if (ret < 0) {
      LOG_WRN("Failed to connect to node");
      k_mutex_lock(&greybus_nodes_mutex, K_FOREVER);
      node_table_remove_node(&node_addr.sin6_addr);
      k_mutex_unlock(&greybus_nodes_mutex);
      continue;
    }

    k_msgq_put(&socket_msgq, &ret, K_FOREVER);

    k_mutex_lock(&greybus_operations_mutex, K_FOREVER);
    ret = svc_send_protocol_version_request(ret, &greybus_operations_list);
    k_mutex_unlock(&greybus_operations_mutex);
    if (!ret) {
      LOG_DBG("Sent svc protocol version request");
    }
  }
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
      if (!node_table_is_active(&node_array[i])) {
        if (!node_table_add_node(&node_array[i])) {
          LOG_WRN("Failed to add node");
        } else {
          LOG_INF("Added Greybus Node");
          k_msgq_put(&discovered_node_msgq, &node_array[i], K_FOREVER);
        }
      }
      k_mutex_unlock(&greybus_nodes_mutex);
    }

    // Put the thread to sleep for an interval
    LOG_DBG("Going to sleep");
    k_msleep(NODE_DISCOVERY_INTERVAL);
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
    LOG_DBG("Pressed: %c", tx);
  }
}
