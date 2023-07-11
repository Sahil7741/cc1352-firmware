#include "operations.h"
#include "error_handling.h"
#include "greybus_protocol.h"
#include "hdlc.h"
#include "zephyr/kernel.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/dlist.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static atomic_t operation_id_counter = ATOMIC_INIT(1);

static uint16_t new_operation_id() {
  atomic_val_t temp = atomic_inc(&operation_id_counter);
  if (temp == UINT16_MAX) {
    atomic_set(&operation_id_counter, 1);
  }
  return temp;
}

void gb_message_dealloc(struct gb_message *msg) {
  if (msg == NULL) {
    return;
  }

  k_free(msg);
}

int gb_message_hdlc_send(const struct gb_message *msg) {
  char buffer[50];

  memcpy(buffer, &msg->header, sizeof(struct gb_operation_msg_hdr));
  memcpy(&buffer[sizeof(struct gb_operation_msg_hdr)], msg->payload,
         msg->payload_size);

  hdlc_block_send_sync(buffer, msg->header.size, ADDRESS_GREYBUS, 0x03);

  return SUCCESS;
}

struct gb_connection *gb_create_connection(struct gb_interface *inf_ap,
                                           struct gb_interface *inf_peer,
                                           uint16_t ap_cport,
                                           uint16_t peer_cport) {
  struct gb_connection *conn = k_malloc(sizeof(struct gb_connection));
  if (conn == NULL) {
    LOG_ERR("Failed to create Greybus connection");
    return NULL;
  }

  conn->inf_ap = inf_ap;
  conn->inf_peer = inf_peer;
  conn->peer_cport_id = peer_cport;
  conn->ap_cport_id = ap_cport;

  return conn;
}

static struct gb_message *gb_message_alloc(const void *payload,
                                           size_t payload_len,
                                           uint8_t message_type,
                                           uint16_t operation_id) {
  struct gb_message *msg;

  msg = k_malloc(sizeof(struct gb_message) + payload_len);
  if (msg == NULL) {
    LOG_WRN("Failed to allocate Greybus request message");
    return NULL;
  }

  msg->header.size = sizeof(struct gb_operation_msg_hdr) + payload_len;
  msg->header.id = operation_id;
  msg->header.type = message_type;
  msg->header.status = 0;
  msg->payload_size = payload_len;
  memcpy(msg->payload, payload, msg->payload_size);

  return msg;
}

struct gb_message *gb_message_request_alloc(const void *payload,
                                            size_t payload_len,
                                            uint8_t request_type,
                                            bool is_oneshot) {
  uint16_t operation_id = is_oneshot ? 0 : new_operation_id();
  return gb_message_alloc(payload, payload_len, request_type, operation_id);
}

struct gb_message *gb_message_response_alloc(const void *payload,
                                             size_t payload_len,
                                             uint8_t request_type,
                                             uint16_t operation_id) {
  return gb_message_alloc(payload, payload_len, OP_RESPONSE | request_type,
                          operation_id);
}
