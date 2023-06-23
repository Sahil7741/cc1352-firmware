#include "svc.h"
#include "greybus_protocol.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

int svc_send_protocol_version_request(int sock) {
  int ret;
  struct gb_svc_version_request req;
  struct gb_operation_msg_hdr hdr;

  req.major = GB_SVC_VERSION_MAJOR;
  req.minor = GB_SVC_VERSION_MINOR;

  hdr.id = 1;
  hdr.type = GB_SVC_TYPE_PROTOCOL_VERSION;
  hdr.status = 0;
  hdr.size = sizeof(struct gb_svc_version_request) + sizeof(struct gb_operation_msg_hdr);

  ret = zsock_send(sock, &hdr, sizeof(struct gb_operation_msg_hdr), 0);
  if(ret < 0) {
    LOG_ERR("Failed to send greybus header: %d", errno);
    return -1;
  }

  ret = zsock_send(sock, &req, sizeof(struct gb_svc_version_request), 0);
  if(ret < 0) {
    LOG_ERR("Failed to send greybus svc version data");
    return -1;
  }

  return 0;
}
