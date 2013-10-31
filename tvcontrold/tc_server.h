#ifndef TC_SERVER_H_INCLUDED
#define TC_SERVER_H_INCLUDED

#include <tc_types.h>

/**
 *  Initialize the server
 *
 *  \retval -1 on error (with a log entry).
 *  \retval 0 on success.
 */
int tc_server_init(void);

/**
 *  Release the server resources.
 */
void tc_server_release(void);

/**
 *  Execute the TV control server process.
 */
void tc_server_exec(void);

/**
 *  Enqueue a new event.
 *
 *  \param buffer   Buffer with the new event enqueued.
 *  \param len      Length of the event to enqueue.
 *  \retval -1 on error (with a log entry).
 *  \retval 0 on success.
 */
int tc_server_event(const char *buffer, uint8_t len);

#endif /* TC_SERVER_H_INCLUDED */
