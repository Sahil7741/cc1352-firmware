#ifndef SVC_H
#define SVC_H

#include "operations.h"

#define SVC_INF_ID 0

/*
 * Create CONTROL_TYPE_PING greybus message and queue it for sending.
 *
 * Note: This does not immediately send the request.
 *
 * @return 0 if successful, else error.
 */
int svc_send_ping();

/*
 * Create SVC_TYPE_VERSION greybus message and queue it for sending.
 *
 * @return 0 if successful, else error.
 */
int svc_send_version();

/*
 * Initialize SVC Interface. Should be called before sending any greybus
 * request.
 */
struct gb_interface *svc_init();

/*
 * Check if SVC is ready. This mostly means if SVC Hello was successfuly
 * executed.
 */
bool svc_is_ready();

#endif
