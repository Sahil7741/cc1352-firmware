#include "svc.h"
#include "operations.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static void svc_ping_callback(struct gb_operation *op) {
  LOG_DBG("Received Pong");
}

int svc_send_ping(int sock) {
  int ret;
  struct gb_operation *op = gb_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  ret = gb_operation_request_alloc(op, NULL, 0, GB_CONTROL_TYPE_PROTOCOL_VERSION, svc_ping_callback);
  if(ret != 0) {
    return -1;
  }

  gb_operation_queue(op);

  return 0;
}
