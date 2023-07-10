#include "svc.h"
#include "greybus_protocol.h"
#include "operations.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct svc_control_data {
  struct k_fifo pending_read;
};

static struct svc_control_data svc_ctrl_data;

static struct gb_message *svc_inf_read(struct gb_controller *ctrl,
                                       uint16_t cport_id) {
  struct gb_message *msg = k_fifo_get(&svc_ctrl_data.pending_read, K_NO_WAIT);
  if (msg != NULL) {
    msg->operation->request_sent = true;
  }
  return msg;
}

static int svc_inf_write(struct gb_controller *ctrl, struct gb_message *msg,
                         uint16_t cport_id) {
  if (gb_message_is_response(msg)) {
    // Find associated Greybus operation
    gb_operation_set_response(msg);
  } else {
    // Handle Request
    LOG_DBG("Handle Greybus request with ID: %u", msg->header.id);
    gb_message_dealloc(msg);
  }
  return 0;
}

static struct gb_interface intf = {.id = SVC_INF_ID,
                                   .controller = {.read = svc_inf_read,
                                                  .write = svc_inf_write,
                                                  .ctrl_data = &svc_ctrl_data}};

struct gb_common_version_request {
  uint8_t major;
  uint8_t minor;
} __packed;

static void svc_ping_callback(struct gb_operation *op) {
  LOG_DBG("Received Pong");
}

static int control_send_request(void *payload, size_t payload_len,
                                uint8_t request_type,
                                greybus_operation_callback_t callback) {
  int ret;
  struct gb_operation *op = gb_operation_alloc(false);
  if (op == NULL) {
    return -ENOMEM;
  }

  ret = gb_operation_request_alloc(op, payload, payload_len, request_type,
                                   callback);
  if (ret < 0) {
    LOG_DBG("Failed to allocate Greybus Request");
    return ret;
  }

  k_fifo_put(&svc_ctrl_data.pending_read, op->request);
  gb_operation_queue(op);

  return SUCCESS;
}

static void gb_common_version_callback(struct gb_operation *op) {
  if (op->response == NULL) {
    LOG_DBG("Null Response");
    return;
  }

  struct gb_common_version_request *response = (struct gb_common_version_request*)op->response->payload;
  LOG_DBG("SVC Protocol Version %u.%u", response->major, response->minor);
}

int svc_send_version() {
  struct gb_common_version_request req = {.major = GB_SVC_VERSION_MAJOR,
                                          .minor = GB_SVC_VERSION_MINOR};
  return control_send_request(&req, sizeof(struct gb_common_version_request),
                              GB_COMMON_TYPE_PROTOCOL_VERSION,
                              gb_common_version_callback);
}

int svc_send_ping() {
  return control_send_request(NULL, 0, GB_SVC_TYPE_PING, svc_ping_callback);
}

struct gb_interface *svc_init() {
  k_fifo_init(&svc_ctrl_data.pending_read);
  return &intf;
}
