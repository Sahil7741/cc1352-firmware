#include "greybus_protocol.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static int write_data(int sock, void *data, size_t len) {
  int ret;
  int transmitted = 0;
  while(transmitted < len) {
    ret = zsock_send(sock, transmitted + (char*)data, len - transmitted, 0);
    if(ret < 0) {
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

static void greybus_dealloc_message(struct gb_message* msg) {
  if(!msg) {
    return;
  }

  k_free(msg->payload);
  k_free(msg);
}

struct gb_operation* greybus_alloc_operation(int sock, sys_dlist_t *list_head) {
  struct gb_operation *op = k_malloc(sizeof(struct gb_operation));
  if(!op) {
    LOG_ERR("Failed to allocate Greybus Operation");
    return NULL;
  }
  op->sock = sock;
  op->request_sent = false;
  op->response_recieved = false;
  op->operation_id = 1;
  sys_dlist_append(list_head, &op->node);
  op->response = NULL;
  op->request = NULL;

  return op;
}

void greybus_dealloc_operation(struct gb_operation *op) {
  if(!op) {
    return;
  }

  greybus_dealloc_message(op->request);
  greybus_dealloc_message(op->response);

  sys_dlist_remove(&op->node);

  k_free(op);
}

int greybus_send_message(struct gb_message *msg) {
  int ret;
  struct gb_operation *op = msg->operation;

  ret = write_data(op->sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
  if(ret < 0) {
    return -1;
  }

  ret = write_data(op->sock, msg->payload, msg->payload_size);
  if(ret < 0) {
    return -1;
  }

  return 0;
}

struct gb_message *greybus_recieve_message(int sock) {
  struct gb_message *msg = k_malloc(sizeof(struct gb_message));
  if (msg == NULL) {
    LOG_ERR("Failed to allocate greybus message");
    return NULL;
  }
  int ret;

  ret = read_data(sock, &msg->header, sizeof(struct gb_operation_msg_hdr));
  if(ret < 0) {
    return NULL;
  }

  msg->payload_size = msg->header.size - sizeof(struct gb_operation_msg_hdr);
  msg->payload = k_malloc(msg->payload_size);
  if (msg->payload == NULL) {
    LOG_ERR("Failed to allocate message payload");
    return NULL;
  }

  ret = read_data(sock, msg->payload, msg->payload_size);
  if(ret < 0) {
    return NULL;
  }

  return msg;
}


int greybus_alloc_request(struct gb_operation *op, void *payload, size_t payload_len, uint8_t request_type) {
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

  memcpy(&op->request->payload, payload, payload_len);
  op->request->payload_size = payload_len;

  op->request->operation = op;

  return 0;
}
