#ifndef TC_OSD_H_INCLUDED
#define TC_OSD_H_INCLUDED

#include <config.h>

#ifdef ENABLE_OSD

#include <tc_types.h>

/**
 *  Initialize the OSD interface.
 *
 *  \retval -1 on error (with a log entry)
 *  \retval 0 on success.
 */
int tc_osd_init(void);

/**
 *  Release the OSD interface.
 */
void tc_osd_release(void);

/**
 *  Show an SVG command through OSD.
 *
 *  \param file  SVG file to show.
 *  \param len   Length of the file name
 *  \retval -1 on error (with a log entry)
 *  \retval 0 on success.
 */
int tc_osd_svg(const char *file, uint8_t len);

/**
 *  Show a PNG command through OSD.
 *
 *  \param file  PNG file to show.
 *  \param len   Length of the file name
 *  \retval -1 on error (with a log entry)
 *  \retval 0 on success.
 */
int tc_osd_png(const char *file, uint8_t len);

#endif /* ENABLE_OSD */

#endif /* TC_OSD_H_INCLUDED */
