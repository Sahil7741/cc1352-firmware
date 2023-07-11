#ifndef _AP_H_
#define _AP_H_

#include "operations.h"

#define AP_INF_ID 1
#define AP_SVC_CPORT_ID 0

struct gb_interface *ap_init();

int ap_rx_submit(struct gb_message *);

struct gb_interface *ap_interface();

#endif
