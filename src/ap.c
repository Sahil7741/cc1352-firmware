#include "ap.h"
#include "operations.h"

struct ap_controller_data {
  struct k_fifo pending_read;
};

static int ap_inf_write(struct gb_controller *ctrl, struct gb_message *msg,
                        uint16_t cport_id) {
  memcpy(msg->header.pad, &cport_id, sizeof(uint16_t));
  gb_message_hdlc_send(msg);
  gb_message_dealloc(msg);
  return 0;
}

static struct gb_message *ap_inf_read(struct gb_controller *ctrl,
                                      uint16_t cport_id) {
  struct gb_message *msg;
  struct ap_controller_data *ctrl_data = ctrl->ctrl_data;

  msg = k_fifo_peek_head(&ctrl_data->pending_read);
  if (msg != NULL) {
    if (memcmp(msg->header.pad, &cport_id, sizeof(uint16_t)) == 0) {
      msg = k_fifo_get(&ctrl_data->pending_read, K_NO_WAIT);
    }
  }
  return msg;
}

static struct ap_controller_data ap_ctrl_data;

static struct gb_interface intf = {.id = AP_INF_ID,
                                  .controller = {
                                      .write = ap_inf_write,
                                      .read = ap_inf_read,
                                      .ctrl_data = &ap_ctrl_data,
                                  }};

struct gb_interface *ap_init() {
  k_fifo_init(&ap_ctrl_data.pending_read);
  return &intf;
}

int ap_rx_submit(struct gb_message *msg) {
  k_fifo_put(&ap_ctrl_data.pending_read, msg);
  return 0;
}
