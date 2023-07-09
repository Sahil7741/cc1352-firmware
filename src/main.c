/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "control.h"
#include "hdlc.h"
#include "node_handler.h"
#include "node_table.h"
#include "operations.h"
#include "svc.h"
#include <stdbool.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define NODE_DISCOVERY_INTERVAL 5000
#define MAX_GREYBUS_NODES CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES
#define MAX_NUMBER_OF_SOCKETS CONFIG_NET_SOCKETS_POLL_MAX
#define NODE_READER_INTERVAL 500
#define NODE_WRITER_INTERVAL 500
#define NODE_POLL_TIMEOUT 500

LOG_MODULE_REGISTER(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static void node_discovery_entry(void *, void *, void *);
static void node_reader_entry(void *, void *, void *);
static void node_writer_entry(void *, void *, void *);

// Thread responsible for beagleconnect node discovery.
K_THREAD_DEFINE(node_discovery, 1024, node_discovery_entry, NULL, NULL, NULL, 5,
                0, 0);
// Thread responsible for reading from greybus nodes
K_THREAD_DEFINE(node_reader, 2048, node_reader_entry, NULL, NULL, NULL, 5, 0,
                0);
// Thread responsible for writing to greybus nodes
K_THREAD_DEFINE(node_writer, 2048, node_writer_entry, NULL, NULL, NULL, 5, 0,
                0);

static void node_reader_entry(void *p1, void *p2, void *p3) {
  struct zsock_pollfd fds[MAX_NUMBER_OF_SOCKETS];
  size_t len = 0;
  size_t i;
  int ret;
  struct gb_message *msg;
  bool peer_closed_flag = false;

  while (1) {
    len = node_table_get_all_cports_pollfd(fds, MAX_NUMBER_OF_SOCKETS);
    if (len <= 0) {
      goto sleep_label;
    }

    // LOG_DBG("Polling %u sockets", len);
    for (i = 0; i < len; ++i) {
      fds[i].events = ZSOCK_POLLIN;
    }

    ret = zsock_poll(fds, len, NODE_POLL_TIMEOUT);
    if (ret <= 0) {
      goto sleep_label;
    }

    // Read any available responses
    for (i = 0; i < len; ++i) {
      if (fds[i].revents & ZSOCK_POLLIN) {
        msg = gb_message_receive(fds[i].fd, &peer_closed_flag);
        if (msg == NULL) {
          if (peer_closed_flag) {
            peer_closed_flag = false;
            node_table_remove_cport_by_socket(fds[i].fd);
          }
          continue;
        }

        // Handle if the msg is a response to an operation
        ret = gb_operation_set_response(msg);
        if (ret < 0) {
          gb_message_dealloc(msg);
        }
      }
    }

  sleep_label:
    // Not sure why this is needed.
    k_sleep(K_MSEC(NODE_WRITER_INTERVAL));
  }
}

static void node_writer_entry(void *p1, void *p2, void *p3) {
  struct zsock_pollfd fds[MAX_NUMBER_OF_SOCKETS];
  size_t len = 0;
  size_t i;
  int ret;

  while (1) {
    len = node_table_get_all_cports_pollfd(fds, MAX_NUMBER_OF_SOCKETS);
    if (len <= 0) {
      goto sleep_label;
    }

    // LOG_DBG("Polling %u sockets", len);
    for (i = 0; i < len; ++i) {
      fds[i].events = ZSOCK_POLLOUT;
    }

    ret = zsock_poll(fds, len, NODE_POLL_TIMEOUT);
    if (ret <= 0) {
      goto sleep_label;
    }

    /// Send all pending requests
    ret = gb_operation_send_request_all(fds, len);
    // LOG_DBG("Written %d operations", ret);

  sleep_label:
    // Not sure why this is needed.
    k_sleep(K_MSEC(NODE_WRITER_INTERVAL));
  }
}

// This function probes for all greybus nodes.
// Currently just using static IP for nodes.
//
// @return number of discovered nodes
static int get_all_nodes(struct in6_addr *node_array,
                         const size_t node_array_len) {
  if (node_array_len < 1) {
    return -1;
  }

  memset(&node_array[0], 0, sizeof(struct in6_addr));
  inet_pton(AF_INET6, CONFIG_NET_CONFIG_PEER_IPV6_ADDR, &node_array[0]);

  return 1;
}

static void node_discovery_entry(void *p1, void *p2, void *p3) {
  int ret;
  struct in6_addr node_array[MAX_GREYBUS_NODES];

  while (1) {
    ret = get_all_nodes(node_array, MAX_GREYBUS_NODES);
    if (ret < 0) {
      LOG_WRN("Failed to get greybus nodes");
      continue;
    }
    // LOG_INF("Discoverd %u nodes", ret);

    for (size_t i = 0; i < ret; ++i) {
      if (!node_table_is_active_by_addr(&node_array[i])) {
        if (node_table_add_node(&node_array[i]) < 0) {
          LOG_WRN("Failed to add node");
        } else {
          node_handler_setup_node_queue(&node_array[i], 0);
          LOG_INF("Added Greybus Node");
        }
      }
    }

    // Put the thread to sleep for an interval
    // LOG_DBG("Going to sleep");
    k_msleep(NODE_DISCOVERY_INTERVAL);
  }
}

static void serial_callback(const struct device *dev, void *user_data) {
  hdlc_rx_submit();
}

static void gb_ap_msg_cb(struct gb_message *msg) {
  int ret = gb_operation_set_response_hdlc(msg);
  if (ret) {
    LOG_ERR("Geybus Opertation Set response failed %d", ret);
  } else {
    LOG_DBG("Greybus Operation Set successfully");
  }
}

void main(void) {
  LOG_INF("Starting BeaglePlay Greybus");
  int ret;

  if (!device_is_ready(uart_dev)) {
    LOG_ERR("UART device not found!");
    return;
  }

  hdlc_init(gb_ap_msg_cb);

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

  svc_send_version();

  k_sleep(K_FOREVER);
}
