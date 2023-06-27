#include "operations.h"
#include "error_handling.h"
#include "greybus_protocol.h"
#include "zephyr/kernel.h"
#include <limits.h>
#include <stdint.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static void gb_operations_callback_entry(void *, void *, void *);

K_THREAD_DEFINE(gb_operations_callback_thread, 1024,
                gb_operations_callback_entry, NULL, NULL, NULL, 6, 0, 0);

K_MSGQ_DEFINE(gb_operations_callback_msgq, sizeof(struct gb_operation *), 10,
              4);

K_MUTEX_DEFINE(gb_operations_mutex);

static void gb_operation_dealloc(struct gb_operation *op) {
  if (op == NULL) {
    return;
  }

  gb_message_dealloc(op->request);
  gb_message_dealloc(op->response);

  sys_dlist_remove(&op->node);

  k_free(op);
}

static void gb_operations_callback_entry(void *p1, void *p2, void *p3) {
  struct gb_operation *op;

  while (k_msgq_get(&gb_operations_callback_msgq, &op, K_FOREVER) == 0) {
    if (op->callback != NULL) {
      op->callback(op);
    }

    k_mutex_lock(&gb_operations_mutex, K_FOREVER);
    gb_operation_dealloc(op);
    k_mutex_unlock(&gb_operations_mutex);
  }
}

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

static int gb_message_send(const struct gb_message *msg) {
  int ret;
  struct gb_operation *op = msg->operation;

  ret = write_data(op->sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
  if (ret < 0) {
    return -E_SEND_HEADER;
  }

  ret = write_data(op->sock, msg->payload, msg->payload_size);
  if (ret < 0) {
    return -E_SEND_PAYLOAD;
  }

  return SUCCESS;
}

static int gb_operation_send_request(struct gb_operation *op) {
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
  op->response_received = false;
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
  k_mutex_lock(&gb_operations_mutex, K_FOREVER);
  sys_dlist_append(&greybus_operations_list, &op->node);
  k_mutex_unlock(&gb_operations_mutex);
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
  int ret;

  k_mutex_lock(&gb_operations_mutex, K_FOREVER);
  op->request = k_malloc(sizeof(struct gb_message));
  if (op->request == NULL) {
    LOG_WRN("Failed to allocate Greybus request message");
    ret = -E_NO_HEAP_MEM;
    goto early_exit;
  }

  op->request->payload = k_malloc(payload_len);
  if (op->request->payload == NULL) {
    LOG_WRN("Failed to allocate Greybus request payload");
    ret = -E_NO_HEAP_MEM;
    goto early_exit;
  }

  op->request->header.size = sizeof(struct gb_operation_msg_hdr) + payload_len;
  op->request->header.id = op->operation_id;
  op->request->header.type = request_type;
  op->request->header.status = 0;

  memcpy(op->request->payload, payload, payload_len);
  op->request->payload_size = payload_len;

  op->request->operation = op;
  op->callback = callback;

  ret = SUCCESS;

early_exit:
  k_mutex_unlock(&gb_operations_mutex);
  return ret;
}

void gb_operation_finish(struct gb_operation *op) {
  k_msgq_put(&gb_operations_callback_msgq, &op, K_FOREVER);
}

struct gb_operation *gb_operation_find_by_id(uint16_t operation_id) {
  struct gb_operation *op;

  k_mutex_lock(&gb_operations_mutex, K_FOREVER);
  SYS_DLIST_FOR_EACH_CONTAINER(&greybus_operations_list, op, node) {
    if (op->operation_id == operation_id) {
      k_mutex_unlock(&gb_operations_mutex);
      return op;
    }
  }
  k_mutex_unlock(&gb_operations_mutex);
  return NULL;
}

void gb_message_dealloc(struct gb_message *msg) {
  if (msg == NULL) {
    return;
  }

  k_free(msg->payload);
  k_free(msg);
}

size_t gb_operation_send_request_all(struct zsock_pollfd *fds, size_t fds_len) {
  size_t i, count = 0;
  struct gb_operation *op, *op_safe;
  int ret;

  k_mutex_lock(&gb_operations_mutex, K_FOREVER);
  SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&greybus_operations_list, op, op_safe,
                                    node) {
    if (gb_operation_request_sent(op)) {
      continue;
    }

    for (i = 0; i < fds_len; ++i) {
      if (gb_operation_socket(op) == fds[i].fd &&
          fds[i].revents & ZSOCK_POLLOUT) {
        ret = gb_operation_send_request(op);
        if (ret == 0) {
          LOG_DBG("Request %u sent", op->operation_id);
          count++;
        } else {
          LOG_WRN("Error in sending request %d", ret);
        }
        break;
      }
    }
  }
  k_mutex_unlock(&gb_operations_mutex);

  return count;
}
