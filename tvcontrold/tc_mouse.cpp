#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *tc_mouse_dpy = NULL;

int tc_mouse_init(void)
{
	tc_mouse_dpy = XOpenDisplay(0);
	return tc_mouse_dpy ? 0 : -1;
}

void tc_mouse_move(void)
{
	//Window root_window;
	//root_window = XRootWindow(dpy, 0);
	//XSelectInput(dpy, root_window, KeyReleaseMask);
	//XWarpPointer(dpy, None, root_window, 0, 0, 0, 0, 100, 100);
	XWarpPointer(tc_mouse_dpy, None, None, 0, 0, 0, 0, 100, 100);
	XFlush(tc_mouse_dpy);
}

void tc_mouse_release(void)
{
	if (tc_mouse_dpy) {
		XCloseDisplay(tc_mouse_dpy);
		tc_mouse_dpy = NULL;
	}
}

/* The mouse click is done with XSendEvent */
