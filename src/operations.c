#include "operations.h"
#include "greybus_protocol.h"
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
    } else if (ret == 0) {
      // Socket was closed by peer
      return 0;
    }
    recieved += ret;
  }
  return recieved;
}

struct gb_operation *gb_operation_alloc(int sock, bool is_oneshot) {
  struct gb_operation *op = k_malloc(sizeof(struct gb_operation));
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

void gb_operation_queue(struct gb_operation *op) {
  sys_dlist_append(&greybus_operations_list, &op->node);
}

void gb_operation_dealloc(struct gb_operation *op) {
  if (op == NULL) {
    return;
  }

  gb_message_dealloc(op->request);
  gb_message_dealloc(op->response);

  sys_dlist_remove(&op->node);

  k_free(op);
}

int gb_message_send(const struct gb_message *msg) {
  int ret;
  struct gb_operation *op = msg->operation;

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

struct gb_message *gb_message_receive(int sock, bool *flag) {
  int ret;
  struct gb_message *msg = k_malloc(sizeof(struct gb_message));
  if (msg == NULL) {
    LOG_ERR("Failed to allocate greybus message");
    goto early_exit;
  }

  ret = read_data(sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
  if (ret <= 0) {
    *flag = ret == 0;
    goto free_msg;
  }

  if (gb_message_is_response(msg) && msg->header.status != GB_OP_SUCCESS) {
      goto free_msg;
  }

  msg->payload_size = msg->header.size - sizeof(struct gb_operation_msg_hdr);
  msg->payload = k_malloc(msg->payload_size);
  if (msg->payload == NULL) {
    LOG_ERR("Failed to allocate message payload");
    goto free_msg;
  }

  ret = read_data(sock, msg->payload, msg->payload_size);
  if (ret <= 0) {
    *flag = ret == 0;
    goto free_payload;
  }

  return msg;

free_payload:
  k_free(msg->payload);
free_msg:
  k_free(msg);
early_exit:
  return NULL;
}

int gb_operation_request_alloc(struct gb_operation *op, const void *payload,
                               size_t payload_len, uint8_t request_type,
                               greybus_operation_callback_t callback) {
  op->request = k_malloc(sizeof(struct gb_message));
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

sys_dlist_t *gb_operation_queue_get() { return &greybus_operations_list; }

void gb_operation_finish(struct gb_operation *op) {
  if (op->callback != NULL) {
    op->callback(op);
  }

  gb_operation_dealloc(op);
}

int gb_operation_send_request(struct gb_operation *op) {
  if (gb_operation_request_sent(op)) {
    return -E_ALREADY_SENT;
  }

  if (op->request == NULL) {
    return -E_NULL_REQUEST;
  }

  int ret = gb_message_send(op->request);
  if (ret < 0) {
    return ret;
  }

  op->request_sent = true;

  if (gb_operation_is_unidirectional(op)) {
    gb_operation_finish(op);
  }

  return SUCCESS;
}

struct gb_operation *gb_operation_find_by_id(uint16_t operation_id) {
  struct gb_operation *op;

  SYS_DLIST_FOR_EACH_CONTAINER(&greybus_operations_list, op, node) {
    if (op->operation_id == operation_id) {
      return op;
    }
  }

  return NULL;
}

void gb_message_dealloc(struct gb_message *msg) {
  if (msg == NULL) {
    return;
  }

  k_free(msg->payload);
  k_free(msg);
}
