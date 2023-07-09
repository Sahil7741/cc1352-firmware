#include "svc.h"
#include "greybus_protocol.h"
#include "operations.h"
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct gb_common_version_request {
  uint8_t major;
  uint8_t minor;
} __packed;

static void svc_ping_callback(struct gb_operation *op) {
  LOG_DBG("Received Pong");
}

int svc_send_ping() {
  int ret;
  struct gb_operation *op = gb_operation_alloc(0, false);
  if (op == NULL) {
    return -1;
  }

  ret = gb_operation_request_alloc(op, NULL, 0, GB_SVC_TYPE_PING,
                                   svc_ping_callback);
  if (ret != 0) {
    return -1;
  }

  gb_operation_queue(op);

  return 0;
}

static int control_send_request(void *payload, size_t payload_len,
                                uint8_t request_type,
                                greybus_operation_callback_t callback) {
  int ret;
  struct gb_operation *op = gb_operation_alloc(0, false);
  if (op == NULL) {
    return -ENOMEM;
  }

  ret = gb_operation_request_alloc(op, payload, payload_len, request_type,
                                   callback);
  if (ret < 0) {
    LOG_DBG("Failed to allocate Greybus Request");
    return ret;
  }

  gb_message_hdlc_send(op->request);
  gb_operation_ap_queue(op);

  return SUCCESS;
}

static void gb_common_version_callback(struct gb_operation *op) {
  if (op->response == NULL) {
    LOG_DBG("Null Response");
    return;
  }

  struct gb_common_version_request *response = op->response->payload;
  LOG_DBG("SVC Protocol Version %u.%u", response->major, response->minor);
}

int svc_send_version() {
  struct gb_common_version_request req = {.major = GB_SVC_VERSION_MAJOR,
                                          .minor = GB_SVC_VERSION_MINOR};
  return control_send_request(&req, sizeof(struct gb_common_version_request),
                              GB_COMMON_TYPE_PROTOCOL_VERSION,
                              gb_common_version_callback);
}
