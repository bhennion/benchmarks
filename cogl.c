#include <cairo-cogl.h>

#include "demo.h"

#include <stdlib.h>

#define container_of(ptr, type, member) ({ \
    const __typeof__ (((type *) 0)->member) *mptr__ = (ptr); \
    (type *) ((char *) mptr__ - offsetof (type, member)); \
})

struct cogl_device {
    struct device base;
    struct framebuffer fb;

    CoglFramebuffer *cogl;
    cairo_device_t *device;
};

static struct framebuffer *
get_fb (struct device *abstract_device)
{
    struct cogl_device *device = (struct cogl_device *) abstract_device;

    return &device->fb;
}

static struct cogl_device *
get_device (struct framebuffer *fb)
{
    return container_of(fb, struct cogl_device, fb);
}

static void
destroy (struct framebuffer *fb)
{
}

static void
show (struct framebuffer *fb)
{
    struct cogl_device *device = get_device (fb);

    cairo_cogl_surface_end_frame (fb->surface);
    cogl_framebuffer_swap_buffers (device->cogl);
}

struct device *
cogl_open (int argc, char **argv)
{
	struct cogl_device *device;
	CoglContext *context;
	CoglOnscreen *onscreen;
	int width = 800, height = 600;

	device = malloc(sizeof(struct cogl_device));
	device->base.name = "cogl";
	device->base.get_framebuffer = get_fb;
	device->base.width = width;
	device->base.height = height;

	device->fb.device = &device->base;
	device->fb.show = show;
	device->fb.destroy = destroy;

	context = cogl_context_new (NULL, NULL);

	device->device = cairo_cogl_device_create (context);
	onscreen = cogl_onscreen_new (context, width, height);
	device->cogl = COGL_FRAMEBUFFER (onscreen);

	cogl_onscreen_show (onscreen);

	cogl_push_framebuffer (device->cogl);
	cogl_ortho (0, width, height, 0, -1, 100);
	cogl_pop_framebuffer ();

	device->base.scanout =
		cairo_cogl_surface_create (device->device, device->cogl);
	device->fb.surface = device->base.scanout;

	return &device->base;
}
