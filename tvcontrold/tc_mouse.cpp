#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void tc_mouse_move(void)
{
	Display *dpy;
	//Window root_window;
	dpy = XOpenDisplay(0);
	//root_window = XRootWindow(dpy, 0);
	//XSelectInput(dpy, root_window, KeyReleaseMask);
	//XWarpPointer(dpy, None, root_window, 0, 0, 0, 0, 100, 100);
	XWarpPointer(dpy, None, None, 0, 0, 0, 0, 100, 100);
	XFlush(dpy);
}
