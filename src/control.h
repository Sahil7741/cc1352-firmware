/*
 * This file contains Control Opertion Type.
 */

#ifndef CONTROL_H
#define CONTROL_H

/*
 * Create SVC_TYPE_PROTOCOL_VERSION greybus operation and queue it for sending.
 *
 * Note: This does not immediately send the request.
 *
 * @param sock: Socket for the cport
 *
 * @return 0 if successful, else error.
 */
int control_send_protocol_version_request(int);

int control_send_get_manifest_size_request(int);

#endif
