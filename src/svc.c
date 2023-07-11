#include "svc.h"
#include "ap.h"
#include "greybus_protocol.h"
#include "operations.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#define ENDO_ID 0x4755

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct svc_control_data {
  struct k_fifo pending_read;
};

static struct svc_control_data svc_ctrl_data;

struct gb_svc_version_request {
  uint8_t major;
  uint8_t minor;
} __packed;

/* SVC protocol hello request */
struct gb_svc_hello_request {
  uint16_t endo_id;
  uint8_t interface_id;
} __packed;

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
  struct gb_svc_version_request req = {.major = GB_SVC_VERSION_MAJOR,
                                       .minor = GB_SVC_VERSION_MINOR};
  return control_send_request(&req, sizeof(struct gb_svc_version_request),
                              GB_SVC_TYPE_PROTOCOL_VERSION_REQUEST);
}

int svc_send_ping() {
  return control_send_request(NULL, 0, GB_SVC_TYPE_PING_REQUEST);
}

int svc_send_hello() {
  struct gb_svc_hello_request req = {.endo_id = ENDO_ID,
                                     .interface_id = AP_INF_ID};
  return control_send_request(&req, sizeof(struct gb_svc_hello_request),
                              GB_SVC_TYPE_HELLO_REQUEST);
}

static void svc_version_response_handler(struct gb_message *msg) {
  struct gb_svc_version_request *response =
      (struct gb_svc_version_request *)msg->payload;
  LOG_DBG("SVC Protocol Version %u.%u", response->major, response->minor);
  svc_send_hello();
}

static void svc_ping_response_handler(struct gb_message *msg) {
  ARG_UNUSED(msg);
  LOG_DBG("Received Pong");
}

static void svc_hello_response_handler(struct gb_message *msg) {
  LOG_DBG("Hello Response Success");
}

static void svc_empty_request_handler(struct gb_message *msg) {
  struct gb_message *resp =
      gb_message_response_alloc(NULL, 0, msg->header.type, msg->header.id);
  if (resp == NULL) {
    LOG_DBG("Failed to allocate response for %X", msg->header.type);
    return;
  }
  k_fifo_put(&svc_ctrl_data.pending_read, resp);
}

static void gb_handle_msg(struct gb_message *msg) {
  switch (msg->header.type) {
  case GB_SVC_TYPE_INTF_DEVICE_ID_REQUEST:
  case GB_SVC_TYPE_ROUTE_CREATE_REQUEST:
  case GB_SVC_TYPE_ROUTE_DESTROY_REQUEST:
  case GB_SVC_TYPE_PING_REQUEST:
    svc_empty_request_handler(msg);
    break;
  case GB_SVC_TYPE_PROTOCOL_VERSION_RESPONSE:
    svc_version_response_handler(msg);
    break;
  case GB_SVC_TYPE_PING_RESPONSE:
    svc_ping_response_handler(msg);
    break;
  case GB_SVC_TYPE_HELLO_RESPONSE:
    svc_hello_response_handler(msg);
    break;
  default:
    LOG_WRN("Handling SVC operation Type %X not supported yet",
            msg->header.type);
  }

  gb_message_dealloc(msg);
}

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

struct gb_interface *svc_init() {
  k_fifo_init(&svc_ctrl_data.pending_read);
  return &intf;
}
