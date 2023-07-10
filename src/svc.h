#ifndef SVC_H
#define SVC_H

#include "operations.h"
#define SVC_INF_ID 0

/*
 * Create CONTROL_TYPE_PING greybus operation and queue it for sending.
 *
 * Note: This does not immediately send the request.
 *
 * @param sock: Socket for the cport
 *
 * @return 0 if successful, else error.
 */
int svc_send_ping();

int svc_send_version();

struct gb_interface *svc_init();

#endif
