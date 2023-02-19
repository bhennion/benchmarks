#include <cairo-xcb.h>

#include "demo.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

struct xcb_device {
    struct device base;

    struct framebuffer fb;

    xcb_connection_t *connection;
    xcb_window_t drawable;

    int double_buffered;
    int async;
};

static void
destroy (struct framebuffer *fb)
{
}

static void
show (struct framebuffer *fb)
{
    struct xcb_device *device = (struct xcb_device *) fb->device;

    if (device->double_buffered) {
	cairo_t *cr = cairo_create (device->base.scanout);
	cairo_set_source_surface (cr, fb->surface, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);
    }

    if (device->async) {
	xcb_flush (device->connection);
    } else {
	cairo_rectangle_int_t r;

	r.x = r.y = 0;
	r.width = r.height = 1;

	cairo_surface_unmap_image(device->base.scanout,
				  cairo_surface_map_to_image(device->base.scanout, &r));
    }
    //sleep(1);
}

static struct framebuffer *
get_fb (struct device *abstract_device)
{
    struct xcb_device *device = (struct xcb_device *) abstract_device;
    return &device->fb;
}

static xcb_visualtype_t *
lookup_visual (xcb_screen_t *s, xcb_visualid_t visual)
{
    xcb_depth_iterator_t d;

    d = xcb_screen_allowed_depths_iterator (s);
    for (; d.rem; xcb_depth_next (&d)) {
	xcb_visualtype_iterator_t v = xcb_depth_visuals_iterator (d.data);
	for (; v.rem; xcb_visualtype_next (&v)) {
	    if (v.data->visual_id == visual)
		return v.data;
	}
    }

    return 0;
}

struct device *
xcb_open (int argc, char **argv)
{
    struct xcb_device *device;
    xcb_connection_t *c;
    xcb_screen_t *s;
    xcb_visualtype_t *v;
    uint32_t values[] = { 1 };
    int i;

    c = xcb_connect (NULL, NULL);
    if (c == NULL)
	return NULL;

    device = malloc (sizeof (struct xcb_device));
    device->base.name = "xcb";
    device->base.get_framebuffer = get_fb;
    device->connection = c;
    device->drawable = xcb_generate_id (c);

    device->double_buffered = 1;
    device->async = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp (argv[i], "--single") == 0)
	    device->double_buffered = 0;
	else if (strcmp (argv[i], "--async") == 0)
	    device->async = 1;
    }

    s = xcb_setup_roots_iterator (xcb_get_setup (c)).data;
    v = lookup_visual (s, s->root_visual);

    device->base.width = s->width_in_pixels;
    device->base.height = s->height_in_pixels;
    device_get_size (argc, argv, &device->base.width, &device->base.height);

    xcb_create_window (c,
		       s->root_depth,
		       device->drawable,
		       s->root,
		       0, 0, device->base.width, device->base.height, 0,
		       XCB_WINDOW_CLASS_INPUT_OUTPUT,
		       s->root_visual,
		       XCB_CW_OVERRIDE_REDIRECT,
		       values);
    xcb_map_window (c, device->drawable);

    device->base.scanout = cairo_xcb_surface_create (c,
						     device->drawable,
						     v,
						     device->base.width,
						     device->base.height);
    if (device->double_buffered) {
	device->fb.surface = cairo_surface_create_similar (device->base.scanout,
							   CAIRO_CONTENT_COLOR,
							   device->base.width,
							   device->base.height);
    } else {
	device->fb.surface = cairo_surface_reference (device->base.scanout);
    }
    device->fb.device = &device->base;
    device->fb.show = show;
    device->fb.destroy = destroy;

    return &device->base;
}

