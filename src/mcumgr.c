// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include <zephyr/logging/log.h>
#include <zephyr/net/buf.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/transport/serial.h>
#include <zephyr/mgmt/mcumgr/transport/smp.h>
#include "hdlc.h"

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static int smp_hdlc_tx_pkt(struct net_buf *);
static uint16_t smp_hdlc_get_mtu(const struct net_buf *);

static struct mcumgr_serial_rx_ctxt smp_rx_ctx;
static struct smp_transport smp_transport = {
	.functions = {.output = smp_hdlc_tx_pkt, .get_mtu = smp_hdlc_get_mtu}};

static int smp_hdlc_tx_cb(const void *data, int len)
{
	hdlc_block_send_sync(data, len, ADDRESS_MCUMGR, 0x03);
	return 0;
}

static int smp_hdlc_tx_pkt(struct net_buf *nb)
{
	int rc;

	rc = mcumgr_serial_tx_pkt(nb->data, nb->len, smp_hdlc_tx_cb);
	smp_packet_free(nb);

	LOG_DBG("SMP TX %d", rc);
	return rc;
}

static uint16_t smp_hdlc_get_mtu(const struct net_buf *nb)
{
	ARG_UNUSED(nb);

	return 256;
}

int mcumgr_process_frame(const void *buffer, size_t buffer_len)
{
	struct net_buf *nb;

	LOG_DBG("Got MCUmgr frame");

	nb = mcumgr_serial_process_frag(&smp_rx_ctx, buffer, buffer_len);

	if (nb == NULL) {
		return -1;
	}

	smp_rx_req(&smp_transport, nb);
	return 0;
}

int mcumgr_init(void)
{
	int rc = smp_transport_init(&smp_transport);

	if (rc) {
		LOG_ERR("Failed to init SMP Transport");
		return -1;
	}

	return 0;
}
