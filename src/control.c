#include "control.h"
#include "node_handler.h"
#include "greybus_protocol.h"
#include "list.h"
#include "manifest.h"
#include "node_table.h"
#include "operations.h"
#include "zephyr/sys/slist.h"
#include <stdint.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

struct gb_control_version_request {
  uint8_t major;
  uint8_t minor;
} __packed;

struct gb_control_get_manifest_size_response {
  uint16_t manifest_size;
} __packed;

struct gb_control_get_manifest_response {
  uint8_t *data;
} __packed;

static void gb_control_get_manifest_size_callback(struct gb_operation *op) {
  int ret;

  if (op->response == NULL) {
    LOG_DBG("Null Response");
    return;
  }

  struct gb_control_get_manifest_size_response *response =
      op->response->payload;
  LOG_DBG("Manifest Size: %u bytes", response->manifest_size);

  ret = control_send_get_manifest_request(op->sock);
  if (ret >= 0) {
    LOG_DBG("Sent control get manifest request");
  }
}

static void gb_control_get_manifest_callback(struct gb_operation *op) {
  if (op->response == NULL) {
    LOG_DBG("Null Response");
    return;
  }

  sys_slist_t cports = gb_manifest_get_cports(op->response->payload, op->response->payload_size);
  struct gb_cport *cport;
  size_t max_cport = 0;
  struct in6_addr addr;
  int ret;

  SYS_SLIST_FOR_EACH_CONTAINER(&cports, cport, node) {
    max_cport = MAX(max_cport, cport->id);
  }

  ret = node_table_get_addr_by_cport0(op->sock, &addr);
  if (ret < 0) {
    goto early_exit;
  }

  ret = node_table_alloc_cports_by_addr(&addr, max_cport);
  if (ret < 0) {
    goto early_exit;
  }

  SYS_SLIST_FOR_EACH_CONTAINER(&cports, cport, node) {
    LOG_DBG("CPort: ID %u, Protocol: %u, Bundle: %u", cport->id, cport->protocol, cport->bundle);
    if (cport->id > 0) {
      node_handler_setup_node_queue(&addr, cport->id);
    }
  }

early_exit:
  gb_cports_free(cports);
}

int control_send_protocol_version_request(int sock) {
  int ret;
  struct gb_operation *op = gb_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  struct gb_control_version_request req;

  req.major = GB_SVC_VERSION_MAJOR;
  req.minor = GB_SVC_VERSION_MINOR;

  ret = gb_operation_request_alloc(op, &req,
                                   sizeof(struct gb_control_version_request),
                                   GB_CONTROL_TYPE_PROTOCOL_VERSION, NULL);
  if (ret != 0) {
    return -1;
  }

  gb_operation_queue(op);

  return 0;
}

int control_send_get_manifest_size_request(int sock) {
  int ret;
  struct gb_operation *op = gb_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  ret =
      gb_operation_request_alloc(op, NULL, 0, GB_CONTROL_TYPE_GET_MANIFEST_SIZE,
                                 gb_control_get_manifest_size_callback);
  if (ret != 0) {
    return -1;
  }

  gb_operation_queue(op);

  return 0;
}

int control_send_get_manifest_request(int sock) {
  int ret;
  struct gb_operation *op = gb_operation_alloc(sock, false);
  if (op == NULL) {
    return -1;
  }

  ret = gb_operation_request_alloc(op, NULL, 0, GB_CONTROL_TYPE_GET_MANIFEST,
                                   gb_control_get_manifest_callback);
  if (ret != 0) {
    return -1;
  }

  gb_operation_queue(op);

  return 0;
}
