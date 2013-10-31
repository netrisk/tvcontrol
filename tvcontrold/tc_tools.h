#ifndef TC_TOOLS_H_INCLUDED
#define TC_TOOLS_H_INCLUDED

/**
 *  Read all the data from a buffer to a descriptor.
 *
 *  \param fd   File descriptor to read from.
 *  \param buf  Buffer to read the data into.
 *  \param len  Length of the data to read.
 *  \return 0 on success, -1 on error.
 */
int tc_read_all(int fd, void *buf, int len);

/**
 *  Write all the data from a buffer to a descriptor.
 *
 *  \param fd   File descriptor to write to.
 *  \param buf  Buffer with the data to write.
 *  \param len  Length of the data to write.
 *  \return 0 on success, -1 on error.
 */
int tc_write_all(int fd, const void *buf, int len);

#endif /* TC_TOOLS_H_INCLUDED */
