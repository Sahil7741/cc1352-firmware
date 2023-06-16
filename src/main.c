/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/net/net_ip.h>
#include <stdbool.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define MAX_NODES 1
#define NODE_DISCOVERY_INTERVAL 5000

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

K_MSGQ_DEFINE(uart_msgq, sizeof(char), 10, 4);

void node_discovery_entry(void *p1, void *p2, void *p3) {
  // Peform node discovery in infinte loop
  struct in6_addr nodes_table[MAX_NODES];
  int ret;
  static const char *node_addr = "2001:db8::1\0";

  while(1) {
    // Search for all `_greybus._tcp` devices on the network. Currently just using a static address.
    static struct sockaddr addr;
    ret = net_ipaddr_parse(node_addr, strlen(node_addr), &addr);
    if (!ret) {
      LOG_WRN("Failed to parse address: %s", node_addr);
    }
    LOG_INF("Discoverd node: %s", node_addr);

    // Crete a new thread for handling the node.

    // Put the thread to sleep for an interval
    k_msleep(NODE_DISCOVERY_INTERVAL);
  }
}

// Thread responseible for beagleconnect node discovery.
K_THREAD_DEFINE(node_discovery, 512, node_discovery_entry, NULL, NULL, NULL, 5, 0, 0);

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
