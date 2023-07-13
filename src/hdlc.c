#include "hdlc.h"
#include "ap.h"
#include "greybus_protocol.h"
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/transport/serial.h>
#include <zephyr/mgmt/mcumgr/transport/smp.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/ring_buffer.h>

#define HDLC_RX_BUF_SIZE 1024

#define HDLC_FRAME 0x7E
#define HDLC_ESC 0x7D
#define HDLC_ESC_FRAME 0x5E
#define HDLC_ESC_ESC 0x5D

static void hdlc_rx_handler(struct k_work *);

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

// TODO: Probably switch to a higher priority thread than apbridge
K_WORK_DEFINE(hdlc_rx_work, hdlc_rx_handler);
RING_BUF_DECLARE(hdlc_rx_ringbuf, HDLC_RX_BUF_SIZE);

struct hdlc_driver {
  struct mcumgr_serial_rx_ctxt smp_rx_ctx;
  struct smp_transport smp_transport;

  uint16_t crc;
  bool next_escaped;
  uint8_t rx_send_seq;
  uint8_t send_seq;
  uint16_t rx_buffer_len;
  uint8_t rx_buffer[HDLC_MAX_BLOCK_SIZE];
} hdlc_driver;

static void uart_poll_out_crc(const struct device *dev, uint8_t byte,
                              uint16_t *crc) {
  *crc = crc16_ccitt(*crc, &byte, 1);
  if (byte == HDLC_FRAME || byte == HDLC_ESC) {
    uart_poll_out(dev, HDLC_ESC);
    byte ^= 0x20;
  }
  uart_poll_out(dev, byte);
}

static void block_out(struct hdlc_driver *drv, const struct hdlc_block *block) {
  uint16_t crc = 0xffff;

  uart_poll_out(uart_dev, HDLC_FRAME);
  uart_poll_out_crc(uart_dev, block->address, &crc);

  if (block->control == 0) {
    uart_poll_out_crc(uart_dev, drv->send_seq << 1, &crc);
  } else {
    uart_poll_out_crc(uart_dev, block->control, &crc);
  }

  for (int i = 0; i < block->length; i++) {
    uart_poll_out_crc(uart_dev, block->buffer[i], &crc);
  }

  uint16_t crc_calc = crc ^ 0xffff;
  uart_poll_out_crc(uart_dev, crc_calc, &crc);
  uart_poll_out_crc(uart_dev, crc_calc >> 8, &crc);
  uart_poll_out(uart_dev, HDLC_FRAME);
}

static void hdlc_process_greybus_frame(struct hdlc_driver *drv,
                                       const char *buffer, size_t buffer_len) {
  // Do something with hdlc information. Starts at hdlc->rx_buffer[3]
  // Can be variable length
  struct gb_operation_msg_hdr hdr;
  struct gb_message *msg;
  size_t payload_size;

  memcpy(&hdr, buffer, sizeof(struct gb_operation_msg_hdr));
  if (gb_hdr_is_response(&hdr) && hdr.status != GB_OP_SUCCESS) {
    LOG_ERR("Greybus operation %u failed", hdr.id);
    return;
  }

  if (hdr.size > buffer_len) {
    LOG_ERR("Greybus Message size is greater than received buffer.");
    return;
  }

  payload_size = hdr.size - sizeof(struct gb_operation_msg_hdr);
  msg = k_malloc(sizeof(struct gb_message) + payload_size);
  if (msg == NULL) {
    LOG_ERR("Failed to allocate greybus message");
    return;
  }
  msg->payload_size = payload_size;
  memcpy(&msg->header, &hdr, sizeof(struct gb_operation_msg_hdr));
  memcpy(msg->payload, &buffer[sizeof(struct gb_operation_msg_hdr)],
         msg->payload_size);
  ap_rx_submit(msg);

  return;
}

static void hdlc_process_mcumgr_frame(struct hdlc_driver *drv, void *buffer,
                                      size_t buffer_len) {
  struct net_buf *nb;
  LOG_DBG("Got MCUmgr frame");

  nb = mcumgr_serial_process_frag(&drv->smp_rx_ctx, buffer, buffer_len);

  if (nb != NULL) {
    LOG_DBG("Successfull in processing mcumgr framg");
    smp_rx_req(&drv->smp_transport, nb);
  }
}

static void hdlc_process_complete_frame(struct hdlc_driver *drv) {
  uint8_t address = drv->rx_buffer[0];
  size_t len = drv->rx_buffer_len - 4;
  void *buffer = &drv->rx_buffer[2];

  switch (address) {
  case ADDRESS_GREYBUS:
    hdlc_process_greybus_frame(drv, buffer, len);
    break;
  case ADDRESS_MCUMGR:
    hdlc_process_mcumgr_frame(drv, buffer, len);
    break;
  case ADDRESS_DBG:
    LOG_WRN("Ignore DBG Frame");
    break;
  default:
    LOG_ERR("Dropped HDLC addr:%x ctrl:%x", address, drv->rx_buffer[1]);
    LOG_HEXDUMP_DBG(drv->rx_buffer, drv->rx_buffer_len, "rx_buffer");
  }
}

static void hdlc_process_frame(struct hdlc_driver *drv) {
  if (drv->rx_buffer[0] == 0xEE) {
    LOG_HEXDUMP_ERR(drv->rx_buffer, 8, "HDLC ERROR");
  } else if (drv->rx_buffer_len > 3 && drv->crc == 0xf0b8) {
    uint8_t ctrl = drv->rx_buffer[1];

    if ((ctrl & 1) == 0) {
      drv->rx_send_seq = (ctrl >> 5) & 0x07;
    } else {
      hdlc_process_complete_frame(drv);
    }
  } else {
    LOG_ERR("Dropped HDLC crc:%04x len:%d", drv->crc, drv->rx_buffer_len);
  }

  drv->crc = 0xffff;
  drv->rx_buffer_len = 0;
}

static int hdlc_save_byte(struct hdlc_driver *drv, uint8_t byte) {
  if (drv->rx_buffer_len >= HDLC_MAX_BLOCK_SIZE) {
    LOG_ERR("HDLC RX Buffer Overflow");
    drv->crc = 0xffff;
    drv->rx_buffer_len = 0;
  }

  drv->rx_buffer[drv->rx_buffer_len++] = byte;

  return 0;
}

static void hdlc_rx_input_byte(struct hdlc_driver *drv, uint8_t byte) {
  if (byte == HDLC_FRAME) {
    if (drv->rx_buffer_len) {
      hdlc_process_frame(drv);
    }
  } else if (byte == HDLC_ESC) {
    drv->next_escaped = true;
  } else {
    if (drv->next_escaped) {
      byte ^= 0x20;
      drv->next_escaped = false;
    }
    drv->crc = crc16_ccitt(drv->crc, &byte, 1);
    hdlc_save_byte(drv, byte);
  }
}

static int hdlc_process_buffer(uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    hdlc_rx_input_byte(&hdlc_driver, buf[i]);
  }
  return 0;
}

static void hdlc_rx_handler(struct k_work *work) {
  size_t len;
  uint8_t *data;
  int ret;

  len = ring_buf_get_claim(&hdlc_rx_ringbuf, &data, HDLC_RX_BUF_SIZE);
  ret = hdlc_process_buffer(data, len);
  if (ret < 0) {
    LOG_ERR("Error processing HDLC buffer");
  }

  ret = ring_buf_get_finish(&hdlc_rx_ringbuf, len);
  if (ret < 0) {
    LOG_ERR("Cannot flush ring buffer (%d)", ret);
  }
}

static int smp_hdlc_tx_cb(const void *data, int len) {
  hdlc_block_send_sync(data, len, ADDRESS_MCUMGR, 0x03);
  return 0;
}

static int smp_hdlc_tx_pkt(struct net_buf *nb) {
  int rc;

  rc = mcumgr_serial_tx_pkt(nb->data, nb->len, smp_hdlc_tx_cb);
  smp_packet_free(nb);

  LOG_DBG("SMP TX %d", rc);
  return rc;
}

static uint16_t smp_hdlc_get_mtu(const struct net_buf *nb) { return 256; }

int hdlc_block_send_sync(const uint8_t *buffer, size_t buffer_len,
                         uint8_t address, uint8_t control) {
  size_t block_size = sizeof(struct hdlc_block) + sizeof(uint8_t) * buffer_len;
  struct hdlc_block *block = k_malloc(block_size);
  if (block == NULL) {
    return -1;
  }

  block->length = buffer_len;
  memcpy(block->buffer, buffer, buffer_len);
  block->address = address;
  block->control = control;

  block_out(&hdlc_driver, block);

  k_free(block);

  return block_size;
}

int hdlc_init() {
  int rc;

  hdlc_driver.crc = 0xffff;
  hdlc_driver.send_seq = 0;
  hdlc_driver.rx_send_seq = 0;
  hdlc_driver.next_escaped = false;
  hdlc_driver.rx_buffer_len = 0;

  hdlc_driver.smp_transport.functions.output = smp_hdlc_tx_pkt;
  hdlc_driver.smp_transport.functions.get_mtu = smp_hdlc_get_mtu;

  rc = smp_transport_init(&hdlc_driver.smp_transport);
  if (rc) {
    LOG_ERR("Failed to init SMP Transport");
    return -1;
  }

  return 0;
}

int hdlc_rx_submit() {
  uint8_t *buf;
  int ret;

  if (!uart_irq_update(uart_dev) && !uart_irq_rx_ready(uart_dev)) {
    return -EBUSY;
  }

  ret = ring_buf_put_claim(&hdlc_rx_ringbuf, &buf, HDLC_RX_BUF_SIZE);
  if (ret <= 0) {
    // No space
    return 0;
  }

  ret = uart_fifo_read(uart_dev, buf, ret);
  if (ret < 0) {
    // Something went wrong
    return -EAGAIN;
  }

  ret = ring_buf_put_finish(&hdlc_rx_ringbuf, ret);
  if (ret < 0) {
    // Some error
    return -EAGAIN;
  }

  k_work_submit(&hdlc_rx_work);

  return ret;
}
