#include "svc.h"
#include "greybus_protocol.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static int write_data(int sock, void *data, size_t len) {
  int transmitted = 0;
  while(transmitted < len) {
    transmitted += zsock_send(sock, transmitted + (char*)data, len - transmitted, 0);
    if(transmitted < 0) {
      LOG_ERR("Failed to transmit data");
      return -1;
    }
  }
  return transmitted;
}

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

  ret = write_data(sock, &hdr, sizeof(struct gb_operation_msg_hdr));
  if(ret < 0) {
    LOG_ERR("Failed to send greybus header: %d", errno);
    return -1;
  }

  ret = write_data(sock, &req, sizeof(struct gb_svc_version_request));
  if(ret < 0) {
    LOG_ERR("Failed to send greybus svc version data");
    return -1;
  }

  return 0;
}
