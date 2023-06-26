#include <stdint.h>
#include "control.h"
#include "greybus_protocol.h"
#include "operations.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct gb_control_version_request {
	uint8_t major;
	uint8_t minor;
} __packed;

struct gb_control_get_manifest_size_response {
  uint16_t manifest_size;
} __packed;

static void gb_control_get_manifest_size_callback(struct greybus_operation *op) {
  if (op->response == NULL) {
    LOG_DBG("Null Response");
    return;
  }

  struct gb_control_get_manifest_size_response *response = op->response->payload;
  LOG_DBG("Manifest Size: %u bytes", response->manifest_size);
}

int control_send_protocol_version_request(int sock) {
  int ret;
  struct greybus_operation *op = greybus_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  struct gb_control_version_request req;

  req.major = GB_SVC_VERSION_MAJOR;
  req.minor = GB_SVC_VERSION_MINOR;

  ret = greybus_operation_request_alloc(op, &req, sizeof(struct gb_control_version_request), GB_CONTROL_TYPE_PROTOCOL_VERSION, NULL);
  if(ret != 0) {
    return -1;
  }

  greybus_operation_queue(op);

  return 0;
}

int control_send_get_manifest_size_request(int sock) {
  int ret;
  struct greybus_operation *op = greybus_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  ret = greybus_operation_request_alloc(op, NULL, 0, GB_CONTROL_TYPE_GET_MANIFEST_SIZE, gb_control_get_manifest_size_callback);
  if(ret != 0) {
    return -1;
  }

  greybus_operation_queue(op);

  return 0;
}
