#include "operations.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static int write_data(int sock, const void *data, size_t len) {
  int ret;
  int transmitted = 0;
  while (transmitted < len) {
    ret = zsock_send(sock, transmitted + (char *)data, len - transmitted, 0);
    if (ret < 0) {
      LOG_ERR("Failed to transmit data");
      return -1;
    }
    transmitted += ret;
  }
  return transmitted;
}

static int read_data(int sock, void *data, size_t len) {
  int ret;
  int recieved = 0;
  while (recieved < len) {
    ret = zsock_recv(sock, recieved + (char *)data, len - recieved, 0);
    if (ret < 0) {
      LOG_ERR("Failed to recieve data");
      return -1;
    } else if (ret == 0) {
      // Socket was closed by peer
      return 0;
    }
    recieved += ret;
  }
  return recieved;
}

struct node_control_data {
};


static struct gb_message *node_inf_read(struct gb_controller *ctrl,
                                       uint16_t cport_id) {
  return NULL;
}

static int node_inf_write(struct gb_controller *ctrl, struct gb_message *msg,
                         uint16_t cport_id) {
  return -1;
}


struct gb_interface *node_create_interface() {
  struct node_control_data *ctrl_data = k_malloc(sizeof(struct node_control_data));
  if (ctrl_data == NULL) {
    return NULL;
  }
  struct gb_interface *inf = k_malloc(sizeof(struct gb_interface));
  if (inf == NULL) {
    goto free_ctrl_data;
  }

  inf->controller.ctrl_data = ctrl_data;
  inf->controller.read = node_inf_read;
  inf->controller.write = node_inf_write;

  return inf;

free_ctrl_data:
  k_free(ctrl_data);
  return NULL;
}

void node_destroy_interface(struct gb_interface *inf) {
  if (inf == NULL) {
    return;
  }

  k_free(inf->controller.ctrl_data);
  k_free(inf);
}
