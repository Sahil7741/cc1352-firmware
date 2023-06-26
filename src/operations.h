/*
 * This file contains functions to manage greybus operations queue.
 * This allows asynchronous processing of greybus operations.
 */

#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "greybus_protocol.h"
#include <zephyr/sys/dlist.h>

/* Return codes for the functions defined here */
#define SUCCESS 0
#define E_NULL_REQUEST 1
#define E_ALREADY_SENT 2

struct gb_operation;
struct gb_message;

typedef void (*greybus_operation_callback_t)(struct gb_operation *);

/*
 * Struct to represent greybus message
 *
 * @param operation: greybus operation this message is associated with. Can be
 * NULL in case of message received.
 * @param header: greybus msg header.
 * @param payload: heap allocated payload.
 * @param payload_size: size of payload in bytes
 */
struct gb_message {
  struct gb_operation *operation;
  struct gb_operation_msg_hdr header;
  void *payload;
  size_t payload_size;
};

/*
 * Struct to represent a greybus operation.
 *
 * @param sock: socket to perform this operation on.
 * @param operation_id: the unique id for this operation.
 * @param request_sent: flag to check if the request has been sent.
 * @param request: pointer to greybus request message.
 * @param response: pointer to greybus response message.
 * @param callback: callback function called when operation is completed.
 * @param node: operation dlist node.
 */
struct gb_operation {
  int sock;
  uint16_t operation_id;
  bool request_sent;
  struct gb_message *request;
  struct gb_message *response;
  greybus_operation_callback_t callback;
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
gb_operation_is_unidirectional(const struct gb_operation *op) {
  return op->operation_id == 0;
}

/*
 * Check if the greybus message is a response.
 *
 * @param msg: greybus message
 *
 * @return true if message is response, else false.
 */
static inline bool gb_message_is_response(const struct gb_message *msg) {
  return msg->header.type & GB_TYPE_RESPONSE_FLAG;
}

/*
 * Get socket for the operation.
 *
 * @param op: greybus operation
 *
 * @return socket for this operation. -1 if socket has not been allocated yet.
 */
static inline int gb_operation_socket(const struct gb_operation *op) {
  return op->sock;
}

/*
 * Check if the request for the operation has been sent.
 *
 * @param op: greybus operation
 *
 * @return true if already sent, else false.
 */
static inline bool gb_operation_request_sent(const struct gb_operation *op) {
  return op->request_sent;
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
struct gb_operation *gb_operation_alloc(int, bool);

/*
 * Allocate a request on a pre-allocated greybus operation
 *
 * @param op: pointer to greybus operation
 * @param data: pointer to payload for request
 * @param payload_len: size of payload
 * @param type: type of greybus operation
 * @param callback: greybus callback to call on operation completion.
 *
 * @return 0 on success, else error
 */
int gb_operation_request_alloc(struct gb_operation *, const void *, size_t,
                               uint8_t, greybus_operation_callback_t);

/*
 * Deallocate greybus operation. It recursively deallocates any allocated
 * request and response. It also removes the operation from operation queue.
 *
 * Note: you probably want to use greybus_operation_finish instead of this.
 *
 * @param op: greybus operation to deallocate
 */
void gb_operation_dealloc(struct gb_operation *);

/*
 * Queue an operation to be executed.
 *
 * Note: This is asynchronous.
 *
 * @param op: greybus operation to execute
 */
void gb_operation_queue(struct gb_operation *);

/*
 * Send a greybus message. This only works for messages associated with a
 * greybus operation for now.
 *
 * @param msg: greybus message to send.
 */
int gb_message_send(const struct gb_message *);

/*
 * Receive a greybus message over a socket.
 *
 * @param sock: Greybus communication socket.
 *
 * @return a heap allocated greybus message
 */
struct gb_message *gb_message_receive(int);

/*
 * Get the greybus operation queue. This use useful for enumerating over the
 * queue.
 *
 * @return greybus operation queue
 */
sys_dlist_t *gb_operation_queue_get();

/*
 * Mark a greybus operation as complete. Call the callback and deallocate the
 * resources.
 *
 * @param op: greybus operation.
 */
void gb_operation_finish(struct gb_operation *);

/*
 * Send greybus request for this operation. This is just a helper function that
 * does a bunch of checking internally. If the request has already been sent,
 * then this does nothing.
 *
 * Note: Do not use operation directly after this request. In case of
 * unidirectional requests, this call will deallocate the request.
 *
 * @param op: greybus operation.
 */
int gb_operation_send_request(struct gb_operation *);

/*
 * Find the greybus operation by operation_id.
 *
 * @param operation id
 *
 * @return greybus operation if found, else return NULL.
 */
struct gb_operation *gb_operation_find_by_id(uint16_t);

/*
 * Deallocate a greybus message.
 *
 * @param pointer to the message to deallcate
 */
void gb_message_dealloc(struct gb_message *);

#endif
