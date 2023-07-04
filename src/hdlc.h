#ifndef HDLC_H
#define HDLC_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

#define ADDRESS_GREYBUS 0x01
#define ADDRESS_DBG 0x02
#define ADDRESS_MCUMGR 0x03

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

struct hdlc_block {
  void *fifo_reserved;
  uint8_t address;
  uint8_t control;
  uint8_t length;
  uint8_t buffer[];
};

int hdlc_init();

/*
 * @param buffer
 * @param buffer_length
 * @param address
 * @param control
 */
int hdlc_block_submit(uint8_t *, size_t, uint8_t, uint8_t);

#endif
