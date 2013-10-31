#ifndef TC_CMD_H_INCLUDED
#define TC_CMD_H_INCLUDED

#include <tc_types.h>

/**
 *  Initialize the command system.
 *
 *  \param readhome  Read the home configurations
 *  \retval -1 on error (with a log entry).
 *  \retval 0 on success
 */
int tc_cmd_init(bool readhome);

/**
 *  Execute a command
 *
 *  \param buf   Buffer with the command to execute.
 *  \param len   Length of the command to execute.
 *  \retval 0 on success of normal command.
 *  \retval 1 on exit command.
 *  \retval -1 on error in command.
 */
int tc_cmd(const char *buf, uint32_t len);

#endif /* TC_CMD_H_INCLUDED */
