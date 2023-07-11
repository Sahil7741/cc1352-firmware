#include "svc.h"
#include "greybus_protocol.h"
#include "operations.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct gb_common_version_request {
  uint8_t major;
  uint8_t minor;
} __packed;

static void gb_common_version_callback(struct gb_message *msg) {
  struct gb_common_version_request *response =
      (struct gb_common_version_request *)msg->payload;
  LOG_DBG("SVC Protocol Version %u.%u", response->major, response->minor);
}

static void gb_handle_msg(struct gb_message *msg) {
  switch (msg->header.type) {
  case GB_SVC_TYPE_PROTOCOL_VERSION_RESPONSE:
    gb_common_version_callback(msg);
    break;
  default:
    LOG_WRN("Handling SVC operation Type %u not supported yet",
            msg->header.type);
  }

  gb_message_dealloc(msg);
}

struct svc_control_data {
  struct k_fifo pending_read;
};

static struct svc_control_data svc_ctrl_data;

static struct gb_message *svc_inf_read(struct gb_controller *ctrl,
                                       uint16_t cport_id) {
  struct gb_message *msg = k_fifo_get(&svc_ctrl_data.pending_read, K_NO_WAIT);
  return msg;
}

static int svc_inf_write(struct gb_controller *ctrl, struct gb_message *msg,
                         uint16_t cport_id) {
  gb_handle_msg(msg);
  return 0;
}

static struct gb_interface intf = {.id = SVC_INF_ID,
                                   .controller = {.read = svc_inf_read,
                                                  .write = svc_inf_write,
                                                  .ctrl_data = &svc_ctrl_data}};

static void svc_ping_callback(struct gb_operation *op) {
  LOG_DBG("Received Pong");
}

static int control_send_request(void *payload, size_t payload_len,
                                uint8_t request_type) {
  struct gb_message *msg;
  msg = gb_message_request_alloc(payload, payload_len, request_type, false);
  if (msg == NULL) {
    return -ENOMEM;
  }

  k_fifo_put(&svc_ctrl_data.pending_read, msg);

  return SUCCESS;
}

int svc_send_version() {
  struct gb_common_version_request req = {.major = GB_SVC_VERSION_MAJOR,
                                          .minor = GB_SVC_VERSION_MINOR};
  return control_send_request(&req, sizeof(struct gb_common_version_request),
                              GB_COMMON_TYPE_PROTOCOL_VERSION);
}

int svc_send_ping() { return control_send_request(NULL, 0, GB_SVC_TYPE_PING); }

struct gb_interface *svc_init() {
  k_fifo_init(&svc_ctrl_data.pending_read);
  return &intf;
}
