#include "svc.h"
#include "greybus_protocol.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

int svc_send_protocol_version_request(int sock, sys_dlist_t *operations_list) {
  int ret;

  struct gb_operation *op = greybus_alloc_operation(sock, operations_list);
  struct gb_svc_version_request req;

  req.major = GB_SVC_VERSION_MAJOR;
  req.minor = GB_SVC_VERSION_MINOR;

  greybus_alloc_request(op, &req, sizeof(struct gb_svc_version_request), GB_SVC_TYPE_PROTOCOL_VERSION);

  ret = greybus_send_message(op->request);
  if(ret < 0) {
    LOG_ERR("Failed to send greybus svc version message");
    return -1;
  }

  return 0;
}
