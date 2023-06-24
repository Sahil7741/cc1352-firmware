#include "svc.h"
#include "greybus_protocol.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct gb_svc_version_request {
	uint8_t major;
	uint8_t minor;
} __packed;

int svc_send_protocol_version_request(int sock, sys_dlist_t *operations_list) {
  int ret;
  struct gb_operation *op = greybus_alloc_operation(sock, false);
  if (op == NULL) {
    return -1;
  }

  struct gb_svc_version_request req;

  req.major = GB_SVC_VERSION_MAJOR;
  req.minor = GB_SVC_VERSION_MINOR;

  ret = greybus_alloc_request(op, &req, sizeof(struct gb_svc_version_request), GB_SVC_TYPE_PROTOCOL_VERSION);
  if(ret != 0) {
    return -1;
  }

  greybus_operation_ready(op, operations_list);

  return 0;
}
