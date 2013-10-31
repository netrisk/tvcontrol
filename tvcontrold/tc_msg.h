#ifndef TC_MSG_H_INCLUDED
#define TC_MSG_H_INCLUDED

#include <tc_types.h>

/**
 *  Message queue to wait for.
 */
typedef struct tc_msg_queue_t {
	int fd[2];
} tc_msg_queue_t;

/** Constant to initialize a queue variable */
#define TC_MSG_QUEUE_INIT { { -1, -1 } }

/** File descriptor of a message queue to poll */
#define TC_MSG_QUEUE_POLLFD(_queue) ((_queue)->fd[0])

/**
 *  Message to be received.
 */
typedef struct tc_msg_t {
	uint8_t buf[257]; /**< Buffer to be filled with the message */
	uint8_t len;      /**< Length of the message                */
} tc_msg_t;

/**
 *  Create the message queue.
 * 
 *  \param queue   Queue to create.
 *  \return 0 on succes, -1 on error.
 */
int tc_msg_queue_create(tc_msg_queue_t *queue);

/**
 *  Close a message queue.
 * 
 *  \param queue   Queue to close.
 */
void tc_msg_queue_close(tc_msg_queue_t *queue);

/**
 *  Receive a message through a queue.
 * 
 *  \param queue  Queue to receive the message through.
 *  \param msg    Received message to be filled.
 *  \return 0 on success, -1 on error.
 *  \remarks The buffer will be zero terminated.
 */
int tc_msg_recv(tc_msg_queue_t *queue, tc_msg_t *msg);

/**
 *  Send a message through a queue.
 *
 *  \param queue  Queue to send a message through.
 *  \param buf    Buffer with the message to send.
 *  \param len    Length of the message.
 *  \return 0 on success, -1 on error.
 */
int tc_msg_send(tc_msg_queue_t *queue, const void *buf, uint8_t len);

#endif /* TC_MSG_H_INCLUDED */
