#include <config.h>

#ifdef ENABLE_OSD

#include <tc_osd.h>
#include <tc_log.h>
#include <tc_msg.h>
#include <libaosd/aosd.h>
#include <pthread.h>
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>
#include <librsvg/rsvg.h>
#include <cairo/cairo.h>
#include <sys/types.h>
#include <sys/syscall.h>

/* Enable this macro for debugging */
/* #define TC_OSD_DEBUG */

/** Thread for OSD painting */
static pthread_t tc_osd_thread;

/** Pipe for OSD communication */
static tc_msg_queue_t tc_osd_queue = TC_MSG_QUEUE_INIT;

/* Times for appearance, fade in and fadeout */
#define TC_OSD_TIME_TOTAL   (1500)
#define TC_OSD_TIME_FADEIN  (200)
#define TC_OSD_TIME_FADEOUT (200)

/** Command to run an SVG file */
#define TC_OSD_CMD_SVG (0)
#define TC_OSD_CMD_PNG (1)

typedef struct tc_osd_data_t {
	Aosd* aosd;               /**< Aosd object.           */
	cairo_surface_t *surface; /**< Surface to paint into  */
	RsvgHandle *rsvg;         /**< RSVG handle to paint   */
	uint32_t width;           /**< Width of the drawing.  */
	uint32_t height;          /**< Height of the drawing. */
	tc_msg_t msg;             /**< Received message       */
	uint32_t time;            /**< Time in milliseconds   */
} tc_osd_data_t;

#if 0
static void
tc_osd_round_rect(cairo_t* cr, int x, int y, int w, int h, int r)
{
	cairo_move_to(cr, x+r, y);
	cairo_line_to(cr, x+w-r, y); /* top edge */
	cairo_curve_to(cr, x+w, y, x+w, y, x+w, y+r);
	cairo_line_to(cr, x+w, y+h-r); /* right edge */
	cairo_curve_to(cr, x+w, y+h, x+w, y+h, x+w-r, y+h);
	cairo_line_to(cr, x+r, y+h); /* bottom edge */
	cairo_curve_to(cr, x, y+h, x, y+h, x, y+h-r);
	cairo_line_to(cr, x, y+r); /* left edge */
	cairo_curve_to(cr, x, y, x, y, x+r, y);
	cairo_close_path(cr);
}
#endif

static void tc_osd_paint(cairo_t *cr, tc_osd_data_t *d)
{
	#if 0
	cairo_set_source_rgba(cr, 0.5, 0, 0, 0.7);
	cairo_new_path(cr);
	uint32_t radius = 40;
	tc_osd_round_rect(cr, 0, 0, d->width, d->height, radius);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 1, 1, 1, 1.0);
	cairo_new_path(cr);
	tc_osd_round_rect(cr, 10, 10, d->width - 20, d->height - 20, radius);
	cairo_stroke(cr);
	#endif
	if (d->rsvg)
		rsvg_handle_render_cairo(d->rsvg, cr);
}

/**
 *  Create the new OSD using the message information.
 *
 *  \param d  Pointer to the data to initialize and the message
 *  \return -1 on error or 0 on success.
 */
static int tc_osd_start(tc_osd_data_t *d)
{
	switch (d->msg.buf[0]) {
	case TC_OSD_CMD_SVG:
		d->rsvg = rsvg_handle_new_from_file((const char *)(d->msg.buf + 1), NULL);
		if (!d->rsvg) {
			tc_log(TC_LOG_ERR, "osd: Error reading svg file \"%s\"",
			       (const char *)(d->msg.buf + 1));
			return -1;
		}
		RsvgDimensionData dimensions;
		rsvg_handle_get_dimensions(d->rsvg, &dimensions);
		d->width = dimensions.width;
		d->height = dimensions.height;
		#ifdef TC_OSD_DEBUG
		tc_log(TC_LOG_DEBUG, "osd: svg: file:%s width:%u height:%u",
		       (const char *)(d->msg.buf + 1), d->width, d->height);
		#endif /* TC_OSD_DEBUG */
		return 0;
	case TC_OSD_CMD_PNG:
		d->surface = cairo_image_surface_create_from_png((const char *)(d->msg.buf + 1));
		if (!d->surface) {
			tc_log(TC_LOG_ERR, "osd: Error reading png file \"%s\"",
			       (const char *)(d->msg.buf + 1));
			return -1;
		}
		d->width = cairo_image_surface_get_width(d->surface);
		d->height = cairo_image_surface_get_height(d->surface);
		#ifdef TC_OSD_DEBUG
		tc_log(TC_LOG_DEBUG, "osd: png: file:%s width:%u height:%u",
		       (const char *)(d->msg.buf + 1), d->width, d->height);
		#endif /* TC_OSD_DEBUG */
		return 0;
	default:
		return -1;
	}
}

static void tc_osd_render(cairo_t *cr, void *data)
{
	tc_osd_data_t *d = (tc_osd_data_t *)data;
	if (!d->surface) {
		#ifdef TC_OSD_DEBUG
		tc_log(TC_LOG_DEBUG, "osd: paint");
		#endif /* TC_OSD_DEBUG */
		cairo_t *rendered_cr;
		d->surface = cairo_surface_create_similar(cairo_get_target(cr),
			CAIRO_CONTENT_COLOR_ALPHA, d->width, d->height);
		rendered_cr = cairo_create(d->surface);
		tc_osd_paint(rendered_cr, d);
 		cairo_destroy(rendered_cr);
	}
	cairo_set_source_surface(cr, d->surface, 0, 0);
	double alpha = 1.0;
	if (d->time < TC_OSD_TIME_FADEIN) {
		alpha = d->time;
		alpha /= TC_OSD_TIME_FADEIN;
	} else if (d->time > TC_OSD_TIME_TOTAL - TC_OSD_TIME_FADEOUT) {
		alpha = TC_OSD_TIME_TOTAL - d->time;
		alpha /= TC_OSD_TIME_FADEOUT;
	}
	cairo_paint_with_alpha(cr, alpha);
}

static void *tc_osd_exec(void *arg)
{
	tc_log(TC_LOG_INFO, "osd: thread pid:%u", (unsigned)syscall(SYS_gettid));

	tc_osd_data_t data = {0};

	while (true) {
		/* If there is any osd to render */
		bool torelease = false;
		if (data.aosd) {
			uint32_t inctime = 50;
			aosd_render(data.aosd);
			aosd_loop_for(data.aosd, inctime);
			if (!aosd_get_is_shown(data.aosd))
				torelease = true;
			data.time += inctime;
		}
		/* Try to receive data from the input */
		if (!data.msg.len) {
			struct pollfd fds[1];
			fds[0].fd = TC_MSG_QUEUE_POLLFD(&tc_osd_queue);
			fds[0].events = POLLIN;
			int r = poll(fds, 1, data.aosd ? 0 : -1);
			if (fds[0].revents & POLLIN) {
				/* We have received an event */
				int r = tc_msg_recv(&tc_osd_queue, &data.msg);
				if (r) {
					tc_log(TC_LOG_ERR, "osd: Error reading from event queue");
					break;
				}
			}
		}
		/* Release the current one if any message received */
		if (data.msg.len && data.aosd)
			torelease = true;
		/* Release the current one if time expired */
		if (data.time > TC_OSD_TIME_TOTAL)
			torelease = true;
		/* Release it */
		if (torelease && data.aosd) {
			if (data.surface) {
				cairo_surface_destroy(data.surface);
				data.surface = NULL;
			}
			if (data.rsvg) {
				g_object_unref(data.rsvg);
				data.rsvg = NULL;
			}
			aosd_destroy(data.aosd);
			data.aosd = NULL;
		}
		/* If there is nothing shown and we have a message start it */
		if (data.msg.len && !data.aosd) {
			data.msg.len = 0;
			data.time = 0;
			data.surface = NULL;
			if (!tc_osd_start(&data)) {
				data.aosd = aosd_new();
				aosd_set_transparency(data.aosd, TRANSPARENCY_COMPOSITE);
				aosd_set_hide_upon_mouse_event(data.aosd, True);
				aosd_set_geometry(data.aosd, 50, 50, data.width, data.height);
				aosd_set_renderer(data.aosd, tc_osd_render, &data);
				aosd_show(data.aosd);
				aosd_loop_once(data.aosd);
			}
		}
	}
	return NULL;
}

int tc_osd_init(void)
{
	if (tc_msg_queue_create(&tc_osd_queue)) {
		tc_log(TC_LOG_ERR, "osd: msg queue creation error");
		return -1;
	}
	if (pthread_create(&tc_osd_thread, NULL, tc_osd_exec, NULL)) {
		tc_log(TC_LOG_ERR, "osd: thread create error");
		return -1;
	}
	return 0;
}

void tc_osd_release(void)
{
	pthread_cancel(tc_osd_thread);
	pthread_join(tc_osd_thread, NULL);
}

int tc_osd_svg(const char *file, uint8_t len)
{
	uint8_t msg[256];
	if (len > sizeof(msg) - 1) {
		tc_log(TC_LOG_ERR, "osd: File name too long");
		return -1;
	}
	msg[0] = TC_OSD_CMD_SVG;
	memcpy(msg + 1, file, len);
	if (tc_msg_send(&tc_osd_queue, msg, 1 + len)) {
		tc_log(TC_LOG_ERR, "osd: Error sending message");
		return -1;
	}
	return 0;
}

int tc_osd_png(const char *file, uint8_t len)
{
	uint8_t msg[256];
	if (len > sizeof(msg) - 1) {
		tc_log(TC_LOG_ERR, "osd: File name too long");
		return -1;
	}
	msg[0] = TC_OSD_CMD_PNG;
	memcpy(msg + 1, file, len);
	if (tc_msg_send(&tc_osd_queue, msg, 1 + len)) {
		tc_log(TC_LOG_ERR, "osd: Error sending message");
		return -1;
	}
	return 0;
}

#endif /* ENABLE_OSD */
