/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _AP_H_
#define _AP_H_

#include "operations.h"

#define AP_INF_ID       1
#define AP_SVC_CPORT_ID 0

/*
 * Initialize AP interface
 *
 * @return AP Interface
 */
struct gb_interface *ap_init(void);

/*
 * Submit message received by AP from transport
 *
 * @param greybus message
 *
 * @return 0 if successfull, negative in case of error
 */
int ap_rx_submit(struct gb_message *msg);

/*
 * Get AP Interface
 *
 * @return AP Interface
 */
struct gb_interface *ap_interface(void);

#endif
