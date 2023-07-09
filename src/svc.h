#ifndef SVC_H
#define SVC_H

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

#endif
