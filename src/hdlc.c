#include "hdlc.h"
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>

#define HDLC_FRAME 0x7E
#define HDLC_ESC 0x7D
#define HDLC_ESC_FRAME 0x5E
#define HDLC_ESC_ESC 0x5D

#define HDLC_BUFFER_SIZE 140

static void hdlc_tx_handler(struct k_work *);

K_FIFO_DEFINE(hdlc_tx_queue);
K_WORK_DEFINE(hdlc_tx_work, hdlc_tx_handler);

struct hdlc_driver {
  uint16_t crc;
  bool next_escaped;
  uint8_t rx_send_seq;
  uint8_t send_seq;
  uint8_t rx_buffer_len;
  uint8_t rx_buffer[HDLC_BUFFER_SIZE];
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

int hdlc_block_submit(uint8_t *buffer, size_t buffer_len, uint8_t address,
                      uint8_t control) {
  struct hdlc_block *block =
      k_malloc(sizeof(struct hdlc_block) + sizeof(uint8_t) * buffer_len);

  if (block == NULL) {
    return -1;
  }

  block->length = buffer_len;
  memcpy(block->buffer, buffer, buffer_len);
  block->address = address;
  block->control = control;

  k_fifo_put(&hdlc_tx_queue, block);
  k_work_submit(&hdlc_tx_work);

  return 0;
}

static void hdlc_dealloc_block(struct hdlc_block *block) { k_free(block); }

int hdlc_init() {
  hdlc_driver.crc = 0;
  hdlc_driver.send_seq = 0;
  hdlc_driver.rx_send_seq = 0;
  hdlc_driver.next_escaped = false;
  hdlc_driver.rx_buffer_len = 0;

  return 1;
}

static void hdlc_tx_handler(struct k_work *work) {
  struct hdlc_block *block = k_fifo_get(&hdlc_tx_queue, K_NO_WAIT);
  while (block) {
    block_out(&hdlc_driver, block);
    hdlc_dealloc_block(block);
    block = k_fifo_get(&hdlc_tx_queue, K_NO_WAIT);
  }
}
