#include <cairo-xlib.h>

#include "demo.h"

#include <stdlib.h>

#include <X11/Xutil.h>

struct xlib_device {
    struct device base;

    struct framebuffer fb;

    Display *display;
    Window drawable;
    Pixmap pixmap;
};

static void
destroy (struct framebuffer *fb)
{
}

static void
show (struct framebuffer *fb)
{
    struct xlib_device *device = (struct xlib_device *) fb->device;
    cairo_t *cr;

    cr = cairo_create (device->base.scanout);
    cairo_set_source_surface (cr, fb->surface, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    if (1) {
	    XImage *image = XGetImage(device->display,
				      device->pixmap,
				      0, 0, 1, 1,
				      AllPlanes, ZPixmap);
	    if (image)
		    XDestroyImage(image);
    }
}

static struct framebuffer *
get_fb (struct device *abstract_device)
{
    struct xlib_device *device = (struct xlib_device *) abstract_device;
    return &device->fb;
}

struct device *
xlib_open (int argc, char **argv)
{
    struct xlib_device *device;
    Display *dpy;
    Screen *scr;
    int screen;
    XSetWindowAttributes attr;

    dpy = XOpenDisplay (NULL);
    if (dpy == NULL)
	return NULL;

    device = malloc (sizeof (struct xlib_device));
    device->base.name = "xlib";
    device->base.get_framebuffer = get_fb;
    device->display = dpy;

    screen = DefaultScreen (dpy);
    scr = XScreenOfDisplay (dpy, screen);
    device->base.width = WidthOfScreen (scr);
    device->base.height = HeightOfScreen (scr);
    device_get_size (argc, argv, &device->base.width, &device->base.height);

    attr.override_redirect = True;
    device->drawable = XCreateWindow (dpy, DefaultRootWindow (dpy),
				      0, 0,
				      device->base.width, device->base.height, 0,
				      DefaultDepth (dpy, screen),
				      InputOutput,
				      DefaultVisual (dpy, screen),
				      CWOverrideRedirect, &attr);
    XMapWindow (dpy, device->drawable);

    device->base.scanout = cairo_xlib_surface_create (dpy, device->drawable,
						      DefaultVisual (dpy, screen),
						      device->base.width, device->base.height);

    device->pixmap = XCreatePixmap(dpy, device->drawable,
				   device->base.width, device->base.height,
				   DefaultDepth (dpy, screen));
    device->fb.surface = cairo_xlib_surface_create (dpy, device->pixmap,
						    DefaultVisual (dpy, screen),
						    device->base.width, device->base.height);

    device->fb.device = &device->base;
    device->fb.show = show;
    device->fb.destroy = destroy;

    return &device->base;
}
