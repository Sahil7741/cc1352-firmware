#ifndef _AP_H_
#define _AP_H_

#include "operations.h"

#define AP_INF_ID 1
#define AP_SVC_CPORT_ID 0

/*
 * Initialize AP interface
 *
 * @return AP Interface
 */
struct gb_interface *ap_init();

/*
 * Submit message received by AP from transport
 *
 * @param greybus message
 *
 * @return 0 if successfull, negative in case of error
 */
int ap_rx_submit(struct gb_message *);

/*
 * Get AP Interface
 *
 * @return AP Interface
 */
struct gb_interface *ap_interface();

#endif
