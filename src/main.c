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
#include <zephyr/net/net_ip.h>

LOG_MODULE_REGISTER(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define NODE_DISCOVERY_INTERVAL 5000
#define MAX_GREYBUS_NODES CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

K_MSGQ_DEFINE(uart_msgq, sizeof(char), 10, 4);

static struct sockaddr greybus_nodes[MAX_GREYBUS_NODES];
static size_t greybus_nodes_pos = 0;
K_MUTEX_DEFINE(greybus_nodes_mutex);

// Add node to the active nodes
static bool add_node(const struct sockaddr *node_addr) {
  if (greybus_nodes_pos >= MAX_GREYBUS_NODES) {
    LOG_WRN("Reached max greybus nodes limit");
    return false;
  }

  memcpy(&greybus_nodes[greybus_nodes_pos], node_addr, sizeof(struct sockaddr));
  greybus_nodes_pos++;
  return true;
}

static int find_node(const struct sockaddr *node_addr) {
  for (size_t i = 0; i < greybus_nodes_pos; ++i) {
    if (greybus_nodes[i].sa_family == node_addr->sa_family &&
        memcmp(node_addr->data, greybus_nodes[i].data,
               NET_SOCKADDR_MAX_SIZE - sizeof(sa_family_t)) == 0) {
      return i;
    }
  }
  return -1;
}

// Check if node is active
static bool is_node_active(const struct sockaddr *node_addr) {
  return find_node(node_addr) != -1;
}

// Remove deactive nodes
static bool remove_node(const struct sockaddr *node_addr) {
  if (greybus_nodes_pos <= 0) {
    return false;
  }

  int pos = find_node(node_addr);
  if (pos == -1) {
    return false;
  }

  greybus_nodes_pos--;
  memcpy(&greybus_nodes[pos], &greybus_nodes[greybus_nodes_pos],
         sizeof(struct sockaddr));
  return true;
}

// This function probes for all greybus nodes.
// Currently just using static IP for nodes.
//
// @return number of discovered nodes
int get_all_nodes(struct sockaddr *node_array, const size_t node_array_len) {
  if (node_array_len < 1) {
    return -1;
  }

  static const char *node_addr = "2001:db8::1\0";

  int ret = net_ipaddr_parse(node_addr, strlen(node_addr), &node_array[0]);
  if (!ret) {
    LOG_WRN("Failed to parse address: %s", node_addr);
  }

  return 1;
}

void node_discovery_entry(void *p1, void *p2, void *p3) {
  // Peform node discovery in infinte loop
  int ret;
  struct sockaddr node_array[MAX_GREYBUS_NODES];

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
        }
      }
      k_mutex_unlock(&greybus_nodes_mutex);
    }

    // Crete a new thread for handling the node.

    // Put the thread to sleep for an interval
    LOG_DBG("Going to sleep");
    k_msleep(NODE_DISCOVERY_INTERVAL);
  }
}

// Thread responseible for beagleconnect node discovery.
K_THREAD_DEFINE(node_discovery, 1024, node_discovery_entry, NULL, NULL, NULL, 5,
                0, 0);

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
