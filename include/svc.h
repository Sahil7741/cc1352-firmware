/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SVC_H_
#define _SVC_H_

#include "operations.h"

#define SVC_INF_ID 0

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

/*
 * Create SVC_TYPE_VERSION greybus message and queue it for sending.
 *
 * @return 0 if successful, else error.
 */
int svc_send_version();

/*
 * Send the SVC module inserted event.
 *
 * @param interface id of the new module
 *
 * @return 0 if successfuly, negative in case of error
 */
int svc_send_module_inserted(uint8_t);

/*
 * Get the SVC interface
 *
 * @return pointer to svc interface
 */
struct gb_interface *svc_interface();

#endif
