#include "operations.h"
#include <limits.h>
#include <stdint.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static atomic_t operation_id_counter = ATOMIC_INIT(1);
static sys_dlist_t greybus_operations_list =
    SYS_DLIST_STATIC_INIT(&greybus_operations_list);

static int write_data(int sock, const void *data, size_t len) {
  int ret;
  int transmitted = 0;
  while (transmitted < len) {
    ret = zsock_send(sock, transmitted + (char *)data, len - transmitted, 0);
    if (ret < 0) {
      LOG_ERR("Failed to transmit data");
      return -1;
    }
    transmitted += ret;
  }
  return transmitted;
}

static int read_data(int sock, void *data, size_t len) {
  int ret;
  int recieved = 0;
  while (recieved < len) {
    ret = zsock_recv(sock, recieved + (char *)data, len - recieved, 0);
    if (ret < 0) {
      LOG_ERR("Failed to recieve data");
      return -1;
    }
    recieved += ret;
  }
  return recieved;
}

static void greybus_dealloc_message(struct greybus_message *msg) {
  if (msg == NULL) {
    return;
  }

  k_free(msg->payload);
  k_free(msg);
}

struct greybus_operation *greybus_operation_alloc(int sock, bool is_oneshot) {
  struct greybus_operation *op = k_malloc(sizeof(struct greybus_operation));
  if (!op) {
    LOG_ERR("Failed to allocate Greybus Operation");
    return NULL;
  }
  op->sock = sock;
  op->response = NULL;
  op->request = NULL;
  op->request_sent = false;
  op->callback = NULL;

  if (is_oneshot) {
    op->operation_id = 0;
  } else {
    atomic_val_t temp = atomic_inc(&operation_id_counter);
    if (temp == UINT16_MAX) {
      atomic_set(&operation_id_counter, 1);
    }
    op->operation_id = temp;
  }

  return op;
}

void greybus_operation_queue(struct greybus_operation *op) {
  sys_dlist_append(&greybus_operations_list, &op->node);
}

void greybus_operation_dealloc(struct greybus_operation *op) {
  if (op == NULL) {
    return;
  }

  greybus_dealloc_message(op->request);
  greybus_dealloc_message(op->response);

  sys_dlist_remove(&op->node);

  k_free(op);
}

int greybus_message_send(const struct greybus_message *msg) {
  int ret;
  struct greybus_operation *op = msg->operation;

  ret = write_data(op->sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
  if (ret < 0) {
    return -1;
  }

  ret = write_data(op->sock, msg->payload, msg->payload_size);
  if (ret < 0) {
    return -1;
  }

  return 0;
}

struct greybus_message *greybus_message_receive(int sock) {
  struct greybus_message *msg = k_malloc(sizeof(struct greybus_message));
  if (msg == NULL) {
    LOG_ERR("Failed to allocate greybus message");
    return NULL;
  }
  int ret;

  ret = read_data(sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
  if (ret < 0) {
    return NULL;
  }

  msg->payload_size = msg->header.size - sizeof(struct gb_operation_msg_hdr);
  msg->payload = k_malloc(msg->payload_size);
  if (msg->payload == NULL) {
    LOG_ERR("Failed to allocate message payload");
    return NULL;
  }

  ret = read_data(sock, msg->payload, msg->payload_size);
  if (ret < 0) {
    return NULL;
  }

  return msg;
}

int greybus_operation_request_alloc(struct greybus_operation *op, const void *payload,
                          size_t payload_len, uint8_t request_type, greybus_operation_callback_t callback) {
  op->request = k_malloc(sizeof(struct greybus_message));
  if (op->request == NULL) {
    LOG_WRN("Failed to allocate Greybus request message");
    return -1;
  }

  op->request->payload = k_malloc(payload_len);
  if (op->request->payload == NULL) {
    LOG_WRN("Failed to allocate Greybus request payload");
    return -1;
  }

  op->request->header.size = sizeof(struct gb_operation_msg_hdr) + payload_len;
  op->request->header.id = op->operation_id;
  op->request->header.type = request_type;
  op->request->header.status = 0;

  memcpy(op->request->payload, payload, payload_len);
  op->request->payload_size = payload_len;

  op->request->operation = op;
  op->callback = callback;

  return 0;
}

sys_dlist_t *greybus_operation_queue_get() {
  return &greybus_operations_list;
}

void greybus_operation_finish(struct greybus_operation *op) {
  if (op->callback != NULL) {
    op->callback(op);
  }

  greybus_operation_dealloc(op);
}
