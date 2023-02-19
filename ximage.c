#include <cairo-xlib.h>

#include "demo.h"

#include <stdlib.h>
#include <stdio.h>

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

struct ximage_device {
    struct device base;

    Display *display;
    Window drawable;

    int stride;
    Pixmap pixmap;
    GC gc;
    XShmSegmentInfo shm;
};

struct ximage_framebuffer {
    struct framebuffer base;
};

static void
destroy (struct framebuffer *abstract_framebuffer)
{
    struct ximage_framebuffer *fb = (struct ximage_framebuffer *) abstract_framebuffer;

    free (fb);
}

static cairo_bool_t
_native_byte_order_lsb (void)
{
    int	x = 1;

    return *((char *) &x) == 1;
}

static void
show (struct framebuffer *abstract_framebuffer)
{
    struct ximage_framebuffer *fb = (struct ximage_framebuffer *) abstract_framebuffer;
    struct ximage_device *device = (struct ximage_device *) fb->base.device;

    if (device->pixmap) {
	XCopyArea (device->display, device->pixmap, device->drawable, device->gc,
		   0, 0,
		   device->base.width, device->base.height,
		   0, 0);
    } else {
	XImage ximage;
	int native_byte_order = _native_byte_order_lsb () ? LSBFirst : MSBFirst;

	ximage.width = device->base.width;
	ximage.height = device->base.height;
	ximage.format = ZPixmap;
	ximage.byte_order = native_byte_order;
	ximage.bitmap_unit = 32;	/* always for libpixman */
	ximage.bitmap_bit_order = native_byte_order;
	ximage.bitmap_pad = 32;	/* always for libpixman */
	ximage.depth = 24;
	ximage.red_mask = 0xff;
	ximage.green_mask = 0xff00;
	ximage.blue_mask = 0xff000;
	ximage.xoffset = 0;
	ximage.bits_per_pixel = 32;
	ximage.data = device->shm.shmaddr;
	ximage.obdata = (char *) &device->shm;
	ximage.bytes_per_line = device->stride;

	XShmPutImage (device->display, device->drawable, device->gc, &ximage,
		      0, 0, 0, 0, device->base.width, device->base.height,
		      False);
    }

    XSync (device->display, True);
}

static void
clear(cairo_surface_t *surface)
{
	cairo_t *cr = cairo_create (surface);
	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	cairo_destroy (cr);
}

static struct framebuffer *
get_fb (struct device *abstract_device)
{
    struct ximage_device *device = (struct ximage_device *) abstract_device;
    struct ximage_framebuffer *fb;

    fb = malloc (sizeof (struct ximage_framebuffer));
    fb->base.device = &device->base;
    fb->base.show = show;
    fb->base.destroy = destroy;

    fb->base.surface = cairo_surface_reference (device->base.scanout);
    clear(fb->base.surface);

    return &fb->base;
}

struct device *
ximage_open (int argc, char **argv)
{
    struct ximage_device *device;
    Display *dpy;
    Screen *scr;
    int screen;
    XSetWindowAttributes attr;
    int major, minor, has_pixmap;
    XGCValues gcv;

    dpy = XOpenDisplay (NULL);
    if (dpy == NULL)
	return NULL;

    if (! XShmQueryExtension (dpy)) {
	XCloseDisplay (dpy);
	return NULL;
    }

    device = malloc (sizeof (struct ximage_device));
    device->base.name = "ximage";
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

    device->stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, device->base.width);

    device->shm.shmid = shmget(IPC_PRIVATE, device->stride * device->base.height, IPC_CREAT | 0777);
    device->shm.shmaddr = shmat (device->shm.shmid, NULL, 0);
    device->shm.readOnly = False;

    if (!XShmAttach (dpy, &device->shm))
	abort ();

    device->base.scanout = cairo_image_surface_create_for_data ((uint8_t *) device->shm.shmaddr,
								CAIRO_FORMAT_RGB24,
								device->base.width,
								device->base.height,
								device->stride);
    XShmQueryVersion (dpy, &major, &minor, &has_pixmap);

    if (has_pixmap) {
	printf ("Using SHM Pixmap\n");
	device->pixmap = XShmCreatePixmap (dpy,
					   device->drawable,
					   device->shm.shmaddr,
					   &device->shm,
					   device->base.width,
					   device->base.height,
					   24);
    } else {
	printf ("Using SHM PutImage\n");
	device->pixmap = 0;
    }

    gcv.graphics_exposures = False;
    device->gc = XCreateGC (dpy, device->drawable, GCGraphicsExposures, &gcv);

    return &device->base;
}
