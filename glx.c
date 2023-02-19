#include <cairo-gl.h>

#include "demo.h"

#include <stdlib.h>
#include <string.h>
#include <X11/X.h>

struct glx_device {
    struct device base;
    struct framebuffer fb;

    Display *display;
    Window drawable;
    GLXContext ctx;
    cairo_device_t *device;

    int double_buffered;
};

static void
destroy (struct framebuffer *fb)
{
}

static void
show (struct framebuffer *fb)
{
    struct glx_device *device = (struct glx_device *) fb->device;

    if (device->double_buffered) {
	cairo_t *cr = cairo_create (device->base.scanout);
	cairo_set_source_surface (cr, fb->surface, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);
    }

    cairo_gl_surface_swapbuffers (device->base.scanout);
}

static struct framebuffer *
get_fb (struct device *abstract_device)
{
    struct glx_device *device = (struct glx_device *) abstract_device;

    return &device->fb;
}

struct device *
glx_open (int argc, char **argv)
{
	int rgba_attribs[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_ALPHA_SIZE, 1,
		GLX_DOUBLEBUFFER,
		None };
	XVisualInfo *vi;
	struct glx_device *device;
	Display *dpy;
	Screen *scr;
	int screen;
	XSetWindowAttributes attr;
	int i;

	dpy = XOpenDisplay (NULL);
	if (dpy == NULL)
		return NULL;

	device = malloc(sizeof(struct glx_device));
	device->base.name = "glx";
	device->base.get_framebuffer = get_fb;
	device->display = dpy;

	device->fb.device = &device->base;
	device->fb.show = show;
	device->fb.destroy = destroy;

	device->double_buffered = 0;
	for (i = 1; i < argc; i++) {
		if (strcmp (argv[i], "--double") == 0)
			device->double_buffered = 1;
	}


	screen = DefaultScreen (dpy);
	scr = XScreenOfDisplay (dpy, screen);
	device->base.width = WidthOfScreen (scr);
	device->base.height = HeightOfScreen (scr);
	device_get_size (argc, argv,
			 &device->base.width, &device->base.height);

	vi = glXChooseVisual (dpy, DefaultScreen (dpy), rgba_attribs);
	if (vi == NULL) {
		XCloseDisplay (dpy);
		free (device);
		return NULL;
	}

	attr.colormap = XCreateColormap (dpy,
					 RootWindow (dpy, vi->screen),
					 vi->visual,
					 AllocNone);

	attr.border_pixel = 0;
	attr.override_redirect = True;
	device->drawable = XCreateWindow (dpy, DefaultRootWindow (dpy),
					  0, 0,
					  device->base.width, device->base.height, 0,
					  vi->depth, InputOutput, vi->visual,
					  CWOverrideRedirect | CWBorderPixel | CWColormap,
					  &attr);
	XMapWindow (dpy, device->drawable);

	device->ctx = glXCreateContext(dpy, vi, NULL, True);
	XFree(vi);

	device->device = cairo_glx_device_create(dpy, device->ctx);

	device->base.scanout =
		cairo_gl_surface_create_for_window (device->device,
						    device->drawable,
						    device->base.width,
						    device->base.height);
    if (device->double_buffered) {
	device->fb.surface = cairo_surface_create_similar (device->base.scanout,
							   CAIRO_CONTENT_COLOR,
							   device->base.width,
							   device->base.height);
    } else {
	device->fb.surface = device->base.scanout;
    }

	return &device->base;
}
