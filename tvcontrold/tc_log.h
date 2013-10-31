#ifndef TC_LOG_H_INCLUDED
#define TC_LOG_H_INCLUDED

#include <tc_types.h>

#define TC_LOG_ERR   1
#define TC_LOG_WARN  2
#define TC_LOG_INFO  3
#define TC_LOG_DEBUG 4

/**
 *  Initialize the log system.
 */
void tc_log_init(void);

/**
 *  Show a log entry.
 *
 *  \param severity  Severity of the log entry.
 *  \param fmt       printf like format of the log.
 */
void tc_log(uint8_t severity, const char *fmt, ...);

#endif // TC_LOG_H_INCLUDED
