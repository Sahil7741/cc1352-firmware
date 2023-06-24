#ifndef SVC_H
#define SVC_H

#include <stdint.h>
#include <zephyr/sys/dlist.h>

/*
 * Create SVC_TYPE_PROTOCOL_VERSION greybus operation and queue it for sending.
 *
 * Note: This does not immediately send the request.
 *
 * @param sock: Socket for the cport
 * @param list: head of operations dlist.
 *
 * @return 0 if successful, else error.
 */
int svc_send_protocol_version_request(int, sys_dlist_t*);

int svc_send_ping(int, sys_dlist_t*);

#endif
