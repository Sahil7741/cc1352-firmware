/*
 * This file contains functions to manage greybus operations queue.
 * This allows asynchronous processing of greybus operations.
 */

#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "greybus_protocol.h"
#include <zephyr/sys/dlist.h>

struct greybus_message {
  struct greybus_operation *operation;
  struct gb_operation_msg_hdr header;
  void *payload;
  size_t payload_size;
};

struct greybus_operation {
  int sock;
  uint16_t operation_id;
  bool request_sent;
  struct greybus_message *request;
  struct greybus_message *response;
  sys_dnode_t node;
};

/*
 * Check if the greybus operation is unidirectional.
 * These messages will not have a response and thus should be freed once request
 * is sent.
 *
 * @param op: greybus operation
 *
 * @return true if operation is unidirectional, else false.
 */
static inline bool
greybus_operation_is_unidirectional(struct greybus_operation *op) {
  return op->operation_id == 0;
}

/*
 * Check if the greybus message is a response.
 *
 * @param msg: greybus message
 *
 * @return true if message is response, else false.
 */
static inline bool greybus_message_is_response(struct greybus_message *msg) {
  return msg->header.type & GB_TYPE_RESPONSE_FLAG;
}

/*
 * Allocate a greybus operation.
 *
 * Note: This does not allocate a request or a response.
 *
 * @param sock: The socket for this operation
 * @param is_oneshot: flag to indicate if the request is unidirectional.
 *
 * @return heap allocated gb_operation
 */
struct greybus_operation *greybus_operation_alloc(int, bool);

/*
 * Allocate a request on a pre-allocated greybus operation
 *
 * @param op: pointer to greybus operation
 * @param data: pointer to payload for request
 * @param payload_len: size of payload
 * @param type: type of greybus operation
 *
 * @return 0 on success, else error
 */
int greybus_operation_request_alloc(struct greybus_operation *, const void *,
                                    size_t, uint8_t);

/*
 * Deallocate greybus operation. It recursively deallocates any allocated
 * request and response. It also removes the operation from operation queue.
 *
 * @param op: greybus operation to deallocate
 */
void greybus_operation_dealloc(struct greybus_operation *);

/*
 * Queue an operation to be executed.
 *
 * Note: This is asynchronous.
 *
 * @param op: greybus operation to execute
 */
void greybus_operation_queue(struct greybus_operation *);

/*
 * Send a greybus message. This only works for messages associated with a
 * greybus operation for now.
 *
 * @param msg: greybus message to send.
 */
int greybus_message_send(const struct greybus_message *);

/*
 * Receive a greybus message over a socket.
 *
 * @param sock: Greybus communication socket.
 *
 * @return a heap allocated greybus message
 */
struct greybus_message *greybus_message_receive(int);

/*
 * Get the greybus operation queue. This use useful for enumerating over the
 * queue.
 *
 * @return greybus operation queue
 */
sys_dlist_t *greybus_operation_queue_get();

#endif
