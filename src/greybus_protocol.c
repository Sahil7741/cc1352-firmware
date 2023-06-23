#include "greybus_protocol.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static void greybus_dealloc_message(struct gb_message* msg) {
  if(!msg) {
    return;
  }

  k_free(msg->payload);
}

struct gb_operation* greybus_alloc_operation(int sock) {
  struct gb_operation *op = k_malloc(sizeof(struct gb_operation));
  if(!op) {
    LOG_ERR("Failed to allocate Greybus Operation");
    return NULL;
  }
  op->sock = sock;
  op->response = NULL;
  op->request = NULL;
  return op;
}

int greybus_alloc_request(struct gb_operation* op, void* payload, size_t payload_size) {
  struct gb_message* msg = k_malloc(sizeof(struct gb_message));
  if (!msg) {
    LOG_ERR("Failed to allocate greybus message");
    return -1;
  }

  msg->payload = k_malloc(payload_size);
  if(!msg->payload) {
    LOG_ERR("Failed to allocate greybus message payload");
    return -1;
  }

  memcpy(&msg->payload, payload, payload_size);
  
  msg->payload_size = payload_size;
  msg->operation = op;

  op->request = msg;

  return 0;
}

void greybus_dealloc_operation(struct gb_operation *op) {
  if(!op) {
    return;
  }

  greybus_dealloc_message(op->request);
  greybus_dealloc_message(op->response);
}
