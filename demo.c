#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "demo.h"

int device_get_size(int argc, char **argv, int *width, int *height)
{
	int w, h, n;

	for (n = 1; n < argc; n++) {
		if (strncmp (argv[n], "--size=", 7) == 0) {
			if (sscanf (argv[n]+7, "%dx%d", &w, &h) == 2) {
				*width = w;
				*height = h;
				return 1;
			}
		} else if (strcmp (argv[n], "--size") == 0) {
			if (n + 1 < argc &&
			    sscanf (argv[++n], "%dx%d", &w, &h) == 2) {
				*width = w;
				*height = h;
				return 1;
			}
		}
	}

	return 0;
}

int device_get_benchmark(int argc, char **argv)
{
	int n, count;

	count = -1;
	for (n = 1; n < argc; n++) {
		if (strncmp (argv[n], "--benchmark=", 12) == 0) {
			return atoi(argv[n]+12);
		} else if (strcmp (argv[n], "--benchmark") == 0) {
			if (n + 1 < argc)
				return atoi(argv[++n]);
			else
				return 0;
		}
	}

	return count;
}

static cairo_antialias_t str_to_antialias(const char *str)
{
	return CAIRO_ANTIALIAS_NONE;
}

cairo_antialias_t device_get_antialias(int argc, char **argv)
{
	int n;

	for (n = 1; n < argc; n++) {
		if (strncmp (argv[n], "--antialias=", 12) == 0) {
			return str_to_antialias(argv[n]+12);
		} else if (strcmp (argv[n], "--antialias") == 0) {
			if (n + 1 < argc)
				return str_to_antialias(argv[++n]);
			else
				return CAIRO_ANTIALIAS_DEFAULT;
		}
	}

	return CAIRO_ANTIALIAS_DEFAULT;
}

struct device *device_open(int argc, char **argv)
{
    struct device *device = 0;
    enum {
	AUTO,
	XLIB,
	XIMAGE,
	XCB,
	GLX,
	COGL,
	DRM,
    } backend = AUTO;
    int n;

    for (n = 1; n < argc; n++) {
	if (strcmp (argv[n], "--xlib") == 0) {
	    backend = XLIB;
	} else if (strcmp (argv[n], "--xcb") == 0) {
	    backend = XCB;
	} else if (strcmp (argv[n], "--ximage") == 0) {
	    backend = XIMAGE;
	} else if (strcmp (argv[n], "--drm") == 0) {
	    backend = DRM;
	} else if (strcmp (argv[n], "--glx") == 0) {
	    backend = GLX;
	} else if (strcmp (argv[n], "--cogl") == 0) {
	    backend = COGL;
	}
    }

    if (backend == AUTO) {
	if (device == 0 && HAVE_DRM)
	    device = drm_open (argc, argv);
	if (device == 0 && HAVE_XCB)
	    device = xcb_open (argc, argv);
	if (device == 0 && HAVE_XLIB)
	    device = xlib_open (argc, argv);
	if (device == 0 && HAVE_GLX)
	    device = glx_open (argc, argv);
	if (device == 0 && HAVE_COGL)
	    device = cogl_open (argc, argv);
	if (device == 0 && HAVE_XIMAGE)
	    device = ximage_open (argc, argv);
    } else switch (backend) {
    case AUTO:
    case XLIB:
	device = xlib_open (argc, argv);
	break;
    case XCB:
	device = xcb_open (argc, argv);
	break;
    case XIMAGE:
	device = ximage_open (argc, argv);
	break;
    case GLX:
	device = glx_open (argc, argv);
	break;
    case COGL:
	device = cogl_open (argc, argv);
	break;
    case DRM:
#if HAVE_DRM
	device = drm_open (argc, argv);
#endif
	break;
    }

    if (device == 0) {
	fprintf(stderr, "Failed to open a drawing device\n");
	exit(1);
    }

    printf("Using backend \"%s\"\n", device->name);
    return device;
}

cairo_surface_t *
_cairo_image_surface_create_from_pixbuf(const GdkPixbuf *pixbuf)
{
	gint width = gdk_pixbuf_get_width (pixbuf);
	gint height = gdk_pixbuf_get_height (pixbuf);
	guchar *gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
	int gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	int cairo_stride;
	guchar *cairo_pixels;
	cairo_format_t format;
	cairo_surface_t *surface;
	int j;

	if (n_channels == 3)
		format = CAIRO_FORMAT_RGB24;
	else
		format = CAIRO_FORMAT_ARGB32;

	surface = cairo_image_surface_create(format, width, height);
	if (cairo_surface_status(surface))
		return surface;

	cairo_stride = cairo_image_surface_get_stride (surface);
	cairo_pixels = cairo_image_surface_get_data(surface);

	for (j = height; j; j--) {
		guchar *p = gdk_pixels;
		guchar *q = cairo_pixels;

		if (n_channels == 3) {
			guchar *end = p + 3 * width;

			while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				q[0] = p[2];
				q[1] = p[1];
				q[2] = p[0];
#else
				q[1] = p[0];
				q[2] = p[1];
				q[3] = p[2];
#endif
				p += 3;
				q += 4;
			}
		} else {
			guchar *end = p + 4 * width;
			guint t1,t2,t3;

#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END

			while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				MULT(q[0], p[2], p[3], t1);
				MULT(q[1], p[1], p[3], t2);
				MULT(q[2], p[0], p[3], t3);
				q[3] = p[3];
#else
				q[0] = p[3];
				MULT(q[1], p[0], p[3], t1);
				MULT(q[2], p[1], p[3], t2);
				MULT(q[3], p[2], p[3], t3);
#endif

				p += 4;
				q += 4;
			}
#undef MULT
		}

		gdk_pixels += gdk_rowstride;
		cairo_pixels += cairo_stride;
	}

	return surface;
}

void
fps_draw (cairo_t *cr, const char *name,
	  const struct timeval *last,
	  const struct timeval *now)
{
#define N_FILTER 25
    static double filter[25];
    static int filter_pos;
    cairo_text_extents_t extents;
    char buf[80];
    double fps, avg;
    int n, max;

    fps = now->tv_sec - last->tv_sec;
    fps += (now->tv_usec - last->tv_usec) / 1000000.;

    max = N_FILTER;
    avg = fps;
    if (filter_pos < max)
	max = filter_pos;
    for (n = 0; n < max; n++)
	avg += filter[n];
    avg /= max + 1;
    filter[filter_pos++ % N_FILTER] = fps;
    if (filter_pos < 5)
	    return;

    snprintf (buf, sizeof (buf), "%s: %.1f fps", name, 1. / avg);
    cairo_set_font_size (cr, 18);
    cairo_text_extents (cr, buf, &extents);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_rectangle (cr, 4-1, 4-1, extents.width+2, extents.height+2);
    cairo_set_source_rgba (cr, .0, .0, .0, .5);
    cairo_fill (cr);

    cairo_move_to (cr, 4 - extents.x_bearing, 4 - extents.y_bearing);
    cairo_set_source_rgb (cr, .95, .95, .95);
    cairo_show_text (cr, buf);
}

#ifndef min
#define min(a, b) ((a) <= (b) ? (a) : (b))
#endif

enum clip device_get_clip(int argc, char **argv)
{
	const char *clip = NULL;
	enum clip c;
	int n;

	for (n = 1; n < argc; n++) {
		if (strcmp (argv[n], "--clip") == 0 && n < argc - 1)
			clip = argv[++n];
		else if (strncmp (argv[n], "--clip=", 7) == 0)
			clip = argv[n] + 7;
	}

	c = CLIP_NONE;
	if (clip) {
		if (strcmp (clip, "none") == 0) {
		} else if (strcmp (clip, "region") == 0 || strcmp (clip, "region1") == 0) {
			c = CLIP_REGION1;
		} else if (strcmp (clip, "box") == 0 || strcmp (clip, "box1") == 0) {
			c = CLIP_BOX1;
		} else if (strcmp (clip, "region2") == 0) {
			c = CLIP_REGION2;
		} else if (strcmp (clip, "box2") == 0) {
			c = CLIP_BOX2;
		} else if (strcmp (clip, "diamond") == 0) {
			c = CLIP_DIAMOND;
		} else if (strcmp (clip, "circle") == 0) {
			c = CLIP_CIRCLE;
		}
	}

	return c;
}

void device_apply_clip(struct device *device, cairo_t *cr, enum clip c)
{
	cairo_new_path (cr);
	switch (c) {
	case CLIP_NONE:
		return;
	case CLIP_REGION1:
		cairo_rectangle (cr,
				 device->width/4, device->height/4,
				 device->width/2, device->height/2);
		break;
	case CLIP_BOX1:
		cairo_rectangle (cr,
				 device->width/4+.25, device->height/4+.25,
				 device->width/2, device->height/2);
		break;
	case CLIP_REGION2:
		cairo_rectangle (cr,
				 0, 0, 3*device->width/4, 3*device->height/4);
		cairo_rectangle (cr,
				 device->width/4, device->height/4,
				 3*device->width/4, 3*device->height/4);
		break;
	case CLIP_BOX2:
		cairo_rectangle (cr,
				 0.25, 0.25, 3*device->width/4, 3*device->height/4);
		cairo_rectangle (cr,
				 device->width/4-.25, device->height/4-.25,
				 3*device->width/4 + .25, 3*device->height/4 + .25);
		break;
	case CLIP_DIAMOND:
		cairo_move_to (cr, device->width/2, 0);
		cairo_line_to (cr, device->width, device->height/2);
		cairo_line_to (cr, device->width/2, device->height);
		cairo_line_to (cr, 0, device->height/2);
		cairo_close_path (cr);
		break;
	case CLIP_CIRCLE:
		cairo_arc (cr, device->width/2, device->height/2,
			   min(device->width/2, device->height/2),
			   0, 2 * M_PI);
		break;
	}
	cairo_clip (cr);
}
