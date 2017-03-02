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

/**
 *  Set a new environment variable.
 *
 *  \param name     Name of the environment variable.
 *  \param namelen  Length of the name of the variable.
 *  \param value    Value of the environment variable.
 *  \param valuelen Length of the value of the variable.
 *  \retval -1 on error (with a log entry).
 *  \retval 0 on success.
 */
int tc_cmd_env_set(const char *name,  uint32_t namelen,
                   const char *value, uint32_t valuelen);

/**
 *  Get an string of the environment variables in csv format.
 *
 *  \retval The pointer to the CSV with the complete environment.
 *  \remarks The memory is allocated so free should be called.
 */
const char *tc_cmd_env_csv(void);

/**
 *  Release the memory of this module
 */
void tc_cmd_release(void);

#endif /* TC_CMD_H_INCLUDED */
