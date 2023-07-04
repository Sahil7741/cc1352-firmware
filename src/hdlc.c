#include <stdint.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/crc.h>

#define HDLC_BUFFER_SIZE 140

#define HDLC_FRAME 0x7E
#define HDLC_ESC 0x7D
#define HDLC_ESC_FRAME 0x5E
#define HDLC_ESC_ESC 0x5D

struct hdlc_driver {
  uint16_t crc;
  bool next_escaped;
  uint8_t rx_send_seq;
  uint8_t send_seq;
  uint8_t rx_buffer_len;
  uint8_t rx_buffer[HDLC_BUFFER_SIZE];
} hdlc_driver;

struct hdlc_block {
  uint8_t address;
  uint8_t control;
  uint8_t length;
  uint8_t buffer[HDLC_BUFFER_SIZE];
};

static void uart_poll_out_crc(const struct device *dev, uint8_t byte,
                              uint16_t *crc) {
  *crc = crc16_ccitt(*crc, &byte, 1);
  if (byte == HDLC_FRAME || byte == HDLC_ESC) {
    uart_poll_out(dev, HDLC_ESC);
    byte ^= 0x20;
  }
  uart_poll_out(dev, byte);
}

static void block_out(struct hdlc_driver *drv, const struct device *dev,
                      const struct hdlc_block *block) {
  uint16_t crc = 0xffff;

  uart_poll_out(dev, HDLC_FRAME);
  uart_poll_out_crc(dev, block->address, &crc);

  if (block->control == 0) {
    uart_poll_out_crc(dev, drv->send_seq << 1, &crc);
  } else {
    uart_poll_out_crc(dev, block->control, &crc);
  }

  for (int i = 0; i < block->length; i++) {
    uart_poll_out_crc(dev, block->buffer[i], &crc);
  }

  uint16_t crc_calc = crc ^ 0xffff;
  uart_poll_out_crc(dev, crc_calc, &crc);
  uart_poll_out_crc(dev, crc_calc >> 8, &crc);
  uart_poll_out(dev, HDLC_FRAME);
}
