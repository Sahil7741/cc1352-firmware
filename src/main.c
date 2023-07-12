/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ap.h"
#include "hdlc.h"
#include "node.h"
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
#include <zephyr/sys/dlist.h>

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define NODE_DISCOVERY_INTERVAL 5000
#define MAX_GREYBUS_NODES CONFIG_BEAGLEPLAY_GREYBUS_MAX_NODES

LOG_MODULE_REGISTER(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static void node_discovery_entry(void *, void *, void *);
static void apbridge_entry(void *, void *, void *);

// Thread responsible for beagleconnect node discovery.
K_THREAD_DEFINE(node_discovery, 1024, node_discovery_entry, NULL, NULL, NULL, 4,
                0, 0);

K_THREAD_DEFINE(apbridge, 2048, apbridge_entry, NULL, NULL, NULL, 5, 0, 0);

static void apbridge_entry(void *p1, void *p2, void *p3) {
  struct gb_connection *conn;
  struct gb_message *msg;

  while (1) {
    // Go through all connections
    SYS_DLIST_FOR_EACH_CONTAINER(gb_connections_list_get(), conn, node) {
      msg = conn->inf_ap->controller.read(&conn->inf_ap->controller,
                                          conn->ap_cport_id);
      if (msg != NULL) {
        conn->inf_peer->controller.write(&conn->inf_peer->controller, msg,
                                         conn->peer_cport_id);
      }
      msg = conn->inf_peer->controller.read(&conn->inf_peer->controller,
                                            conn->peer_cport_id);
      if (msg != NULL) {
        conn->inf_ap->controller.write(&conn->inf_ap->controller, msg,
                                       conn->ap_cport_id);
      }
    }
    k_yield();
    // k_sleep(K_MSEC(50));
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
  struct gb_interface *intf;

  // Wait until SVC is ready
  while (!svc_is_ready()) {
    k_sleep(K_MSEC(1000));
  }

  while (1) {
    ret = get_all_nodes(node_array, MAX_GREYBUS_NODES);
    if (ret < 0) {
      LOG_WRN("Failed to get greybus nodes");
      continue;
    }
    // LOG_INF("Discoverd %u nodes", ret);

    for (size_t i = 0; i < ret; ++i) {
      intf = node_find_by_addr(&node_array[i]);
      if (intf == NULL) {
        intf = node_create_interface(&node_array[i]);
        svc_send_module_inserted(intf->id);
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

void main(void) {
  int ret;
  struct gb_connection *conn;

  LOG_INF("Starting BeaglePlay Greybus");

  if (!device_is_ready(uart_dev)) {
    LOG_ERR("UART device not found!");
    return;
  }

  hdlc_init();
  struct gb_interface *ap = ap_init();
  struct gb_interface *svc = svc_init();

  conn = gb_create_connection(ap, svc, 0, 0);

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
