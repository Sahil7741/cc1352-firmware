#include "svc.h"
#include "operations.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

int svc_send_ping(int sock) {
  int ret;
  struct greybus_operation *op = greybus_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  ret = greybus_operation_request_alloc(op, NULL, 0, GB_CONTROL_TYPE_PROTOCOL_VERSION, NULL);
  if(ret != 0) {
    return -1;
  }

  greybus_operation_queue(op);

  return 0;
}
