/*
 * This file contains Control Opertion Type.
 * The operations are executed asynchronously.
 */

#ifndef CONTROL_H
#define CONTROL_H

#include "error_handling.h"

/*
 * Create CONTROL_TYPE_PROTOCOL_VERSION greybus operation and queue it for
 * sending.
 *
 * @param sock: Socket for the cport
 *
 * @return 0 if successful, negative in case of error.
 */
int control_send_protocol_version_request(int);

/*
 * Create CONTROL_TYPE_GET_MANIFEST_SIZE greybus operation and queue it for
 * sending.
 *
 * @param sock: Socket for the cport
 *
 * @return 0 if successful, negative in case of error.
 */
int control_send_get_manifest_size_request(int);

/*
 * Create CONTROL_TYPE_GET_MANIFEST greybus operation and queue it for sending.
 *
 * @param sock: Socket for the cport
 *
 * @return 0 if successful, negative in case of error.
 */
int control_send_get_manifest_request(int);

#endif
