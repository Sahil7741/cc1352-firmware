#include "node_handler.h"
#include "node_table.h"
#include "svc.h"
#include "zephyr/kernel.h"
#include "zephyr/net/net_ip.h"
#include <zephyr/logging/log.h>

#define GB_TRANSPORT_TCPIP_BASE_PORT 4242

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static void node_setup_work_handler(struct k_work *);

K_WORK_DEFINE(node_setup_work, node_setup_work_handler);

struct gb_cport_connection {
  struct in6_addr addr;
  uint8_t cport_num;
};

K_MSGQ_DEFINE(node_setup_queue, sizeof(struct gb_cport_connection), 10, 4);

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
    goto fail;
  }

  LOG_INF("Trying to connect to node from socket %d", sock);
  ret = zsock_connect(sock, addr, addr_size);

  if (ret) {
    LOG_WRN("Failed to connect to node %d", errno);
    goto fail;
  }

  LOG_INF("Connected to Greybus Node");
  return sock;

fail:
  zsock_close(sock);
  return -1;
}

void node_setup(struct in6_addr *node_ip_addr, uint8_t cport_num) {
  struct sockaddr_in6 node_addr;
  int ret;

  memcpy(&node_addr.sin6_addr, node_ip_addr, sizeof(struct in6_addr));
  node_addr.sin6_family = AF_INET6;
  node_addr.sin6_scope_id = 0;
  node_addr.sin6_port = htons(GB_TRANSPORT_TCPIP_BASE_PORT + cport_num);

  ret = connect_to_node((struct sockaddr *)&node_addr);
  if (ret < 0) {
    LOG_WRN("Failed to connect to node");
    goto fail;
  }

  ret = node_table_add_cport_by_addr(&node_addr.sin6_addr, ret, cport_num);
  if (ret < 0) {
    LOG_WRN("Failed to add cport %u to node table with error %d", cport_num, ret);
    goto fail;
  }
  LOG_DBG("Added Cport %u", cport_num);

  // Create an Interface

  return;

fail:
  if (cport_num == 0) {
    node_table_remove_node_by_addr(&node_addr.sin6_addr);
  }
}

static void node_setup_work_handler(struct k_work *work) {
  struct gb_cport_connection cport;
  while (k_msgq_get(&node_setup_queue, &cport, K_NO_WAIT) == 0) {
    node_setup(&cport.addr, cport.cport_num);
  }
}

int node_handler_setup_node_queue(const struct in6_addr *addr,
                                  uint8_t cport_num) {
  struct gb_cport_connection cport;
  memcpy(&cport.addr, addr, sizeof(struct in6_addr));
  cport.cport_num = cport_num;
  k_msgq_put(&node_setup_queue, &cport, K_FOREVER);
  k_work_submit(&node_setup_work);

  return SUCCESS;
}
