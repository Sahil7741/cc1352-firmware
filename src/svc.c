#include "svc.h"
#include "operations.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct gb_svc_version_request {
	uint8_t major;
	uint8_t minor;
} __packed;

int svc_send_protocol_version_request(int sock) {
  int ret;
  struct greybus_operation *op = greybus_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  struct gb_svc_version_request req;

  req.major = GB_SVC_VERSION_MAJOR;
  req.minor = GB_SVC_VERSION_MINOR;

  ret = greybus_operation_request_alloc(op, &req, sizeof(struct gb_svc_version_request), GB_CONTROL_TYPE_PROTOCOL_VERSION);
  if(ret != 0) {
    return -1;
  }

  greybus_operation_queue(op);

  return 0;
}

int svc_send_ping(int sock) {
  int ret;
  struct greybus_operation *op = greybus_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  ret = greybus_operation_request_alloc(op, NULL, 0, GB_CONTROL_TYPE_PROTOCOL_VERSION);
  if(ret != 0) {
    return -1;
  }

  greybus_operation_queue(op);

  return 0;
}
