/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _TCP_DISCOVERY_H_
#define _TCP_DISCOVERY_H_

#define NODE_DISCOVERY_INTERVAL 5000

/*
 * Start Greybus Node discovery over network
 */
void tcp_discovery_start(void);

/*
 * Stop Greybus Node discovery over network
 */
void tcp_discovery_stop(void);

#endif
