#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>
#include <random>
#include <vector>
#include <array>

#include "demo.h"

#define N_STROKES 100
#define PTS_PER_STROKE 100
#define STRETCH_FACTOR 1.
#define STROKE_FIRST_WIDTH 10.
#define SPEED_FACTOR 1
//#define DEBUG

struct Point {
		Point(double x, double y, double z): x(x), y(y), z(z) {}
		double x = 0;
		double y = 0;
		double z = -1;
};

struct MathVect2 {
public:
		MathVect2() = default;
		MathVect2(const Point& p, const Point& q);
		MathVect2(double dx, double dy);
		
		double dx{};
		double dy{};
		static double scalarProduct(const MathVect2 u, const MathVect2 v);
		double norm() const;
		double squaredNorm() const;
		bool isZero() const;
		MathVect2 normal_left() const;
		MathVect2 normal_right() const;
		MathVect2 operator+(const MathVect2& u) const;
		MathVect2 operator-(const MathVect2& u) const;
		double argument() const;
};

MathVect2 operator*(const double c, const MathVect2& u);

MathVect2::MathVect2(const Point& p, const Point& q): dx(q.x - p.x), dy(q.y - p.y) {}
MathVect2::MathVect2(double dx, double dy): dx(dx), dy(dy) {}

double MathVect2::scalarProduct(const MathVect2 u, const MathVect2 v) { return u.dx * v.dx + u.dy * v.dy; }
double MathVect2::norm() const { return std::hypot(dx, dy); }
MathVect2 MathVect2::operator+(const MathVect2& u) const { return {dx + u.dx, dy + u.dy}; }
MathVect2 MathVect2::operator-(const MathVect2& u) const { return {dx - u.dx, dy - u.dy}; }

MathVect2 operator*(const double c, const MathVect2& u) { return {c * u.dx, c * u.dy}; }

MathVect2 MathVect2::normal_right() const {
		double n = norm();
		return MathVect2(-dy / n, dx / n);
}
MathVect2 MathVect2::normal_left() const {
		double n = norm();
		return MathVect2(dy / n, -dx / n);
}

double MathVect2::argument() const {
		return std::atan2(dx, dy);
}

Point operator+(const Point& p, const MathVect2& v) {
		return {p.x + v.dx, p.y + v.dy, -1.0};
}
Point operator-(const Point& p, const MathVect2& v) {
		return {p.x - v.dx, p.y - v.dy, -1.0};
}

/* No science here, just a hack from toying */
struct Color {
    double red, green, blue;
};

static const Color colors[] = {
    { 0.71, 0.81, 0.83 },
    { 1.00, 0.78, 0.57 },
    { 0.64, 0.30, 0.35 },
    { 0.73, 0.40, 0.39 },
    { 0.91, 0.56, 0.64 },
    { 0.70, 0.47, 0.45 },
    { 0.92, 0.75, 0.60 },
    { 0.82, 0.86, 0.85 },
    { 0.51, 0.56, 0.67 },
    { 1.00, 0.79, 0.58 },
};

struct Border {
		std::vector<Point> forward;
		std::vector<Point> backward;
		double angleStart;
		double angleEnd;
};

struct Stroke {
    std::vector<Point> path;
		const Color* color; 
		int x, y, v;
		double rot, rv;
		Border border;
};

static std::array<Stroke, N_STROKES> strokes;

static std::vector<Point> getRandomPath(size_t nbPts) {
    std::random_device rd;
    std::default_random_engine e(rd());
    std::uniform_real_distribution<double> angle(-M_PI, M_PI);
    std::normal_distribution<double> normalD(0.0, 1.0);

    std::vector<Point> pts;
    pts.reserve(nbPts);
		pts.emplace_back(0.0, 0.0, STROKE_FIRST_WIDTH);
    double a = angle(e);
		pts.emplace_back(STRETCH_FACTOR*cos(a), STRETCH_FACTOR*sin(a), STROKE_FIRST_WIDTH*(1. + 0.1*normalD(e)));

    auto beforeLast = pts.begin();
    auto last = std::prev(pts.end());
    while (pts.size() != nbPts) {
        MathVect2 v(*beforeLast, *last);
        double b = angle(e);
        double c = STRETCH_FACTOR * 0.3 * normalD(e);
        v = v + MathVect2(c * cos(b), c * sin(b));
				double dp = normalD(e);
				double m = std::min(0.5*STROKE_FIRST_WIDTH, dp > 0 ? 2. * STROKE_FIRST_WIDTH - last->z : last->z);
				dp *= 0.2 * m;
        pts.emplace_back(last->x + v.dx, last->y + v.dy, std::abs(last->z + dp));
        beforeLast = last++;
				// printf("pt (%f ; %f ; %f)\n", pts.back().x, pts.back().y, pts.back().z);
    }
    return pts;
}

static Border makeBorder(const std::vector<Point>& path) {
		std::vector<Point> forward;
		std::vector<Point> backward;
		
		forward.reserve(2*path.size());
		backward.reserve(2*path.size());
				
		MathVect2 n = 0.5*path.front().z * MathVect2(path.front(), path[1]).normal_left();
		forward.emplace_back(path.front() + n);
		backward.emplace_back(path.front() - n);
		
		for (auto it1 = path.begin(), it2 = it1 + 1, it3 = it2 + 1; it3 != path.end(); it1++, it2++, it3++) {
				const auto& p1 = *it1;
				const auto& p2 = *it2;
				const auto& p3 = *it3;
				
				MathVect2 v1(p2,p1);
				MathVect2 v3(p2,p3);
				
				MathVect2 n1 = 0.5*p2.z*v1.normal_left();
				MathVect2 n3 = 0.5*p2.z*v3.normal_left();
				if (auto ps = MathVect2::scalarProduct(n1, v3); ps > 0.0) {
						forward.emplace_back(p2 - n1);
						forward.emplace_back(p2 + n3);
						backward.emplace_back(p2 + 0.5*(n1 - n3));
				} else if (ps < 0.0) {
						backward.emplace_back(p2 + n1);
						backward.emplace_back(p2 - n3);
						forward.emplace_back(p2 - 0.5*(n1 - n3));
				} else {
						forward.emplace_back(p2 - n1);
						backward.emplace_back(p2 + n1);
				}
		}

		n = 0.5*path.back().z*MathVect2(path.back(), path[path.size() - 2]).normal_right();
		forward.emplace_back(path.back() + n);
		backward.emplace_back(path.back() - n);
		
		return {std::move(forward), std::move(backward), MathVect2(path[1], path[0]).argument(), MathVect2(path[path.size() - 2], path[path.size() - 1]).argument()};
}

static void
make_stroke (Stroke& s, int w, int h)
{
    s.path = getRandomPath(PTS_PER_STROKE);
    s.color = &colors[rand() % 10];
		
		s.x = rand() % w;
		s.y = rand() % h;
		s.rot = fmod (rand(), 2 * M_PI);
		s.rv = SPEED_FACTOR * (fmod (rand(), 5.) + 1) * M_PI /360.;
		s.v  = SPEED_FACTOR * (rand() % 10 + 2);
		
		s.border = makeBorder(s.path);
}

static void
xopp_stroke_draw (cairo_t *cr)
{
		for (auto& stroke: strokes) {
				cairo_save (cr);
				
				cairo_translate (cr, stroke.x, stroke.y);
				cairo_rotate (cr, stroke.rot);
				cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
				cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
				cairo_set_source_rgba(cr, stroke.color->red, stroke.color->green, stroke.color->blue, 1.);
				
				for (auto it1 = stroke.path.begin(), it2 = std::next(it1); it2 != stroke.path.end() ; it1++, it2++) {
						cairo_set_line_width(cr, it1->z);
						cairo_move_to(cr, it1->x, it1->y);
						cairo_line_to(cr, it2->x, it2->y);
						cairo_stroke(cr);
				}
				cairo_restore (cr);
		}
}

static void
fast_stroke_draw (cairo_t *cr)
{
		for (auto& stroke: strokes) {
				cairo_save (cr);
				
				cairo_translate (cr, stroke.x, stroke.y);
				cairo_rotate (cr, stroke.rot);
				cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
				cairo_set_source_rgba(cr, stroke.color->red, stroke.color->green, stroke.color->blue, 1.);
				
				for (auto& p: stroke.border.forward) {
						cairo_line_to(cr, p.x, p.y);
				}
				cairo_arc(cr, stroke.path.back().x, stroke.path.back().y, 0.5*stroke.path.back().z, -stroke.border.angleEnd, -stroke.border.angleEnd + M_PI);
				
				for (auto rit = stroke.border.backward.rbegin(); rit != stroke.border.backward.rend() ; rit++) {
						cairo_line_to(cr, rit->x, rit->y);
				}
				cairo_arc(cr, stroke.path.front().x, stroke.path.front().y, 0.5*stroke.path.front().z, -stroke.border.angleStart, -stroke.border.angleStart + M_PI);
				
				cairo_close_path(cr);

#ifndef DEBUG
				cairo_fill(cr);
#else
				cairo_set_line_width(cr, 4);
				cairo_stroke(cr);
				
				
				for (auto& p: stroke.path) {
						cairo_line_to(cr, p.x, p.y);
				}
				cairo_set_line_width(cr, 2);
				cairo_set_source_rgba(cr, 0.5,0.5,0.5, 1.);
				cairo_stroke(cr);
				
				cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
				cairo_set_line_width(cr, 10);
				cairo_set_source_rgba(cr, 1.,0.,0., 1.);
				
				for (auto& p: stroke.border.forward) {
						cairo_move_to(cr, p.x, p.y);
						cairo_line_to(cr, p.x, p.y);
				}
				cairo_stroke(cr);
				cairo_set_source_rgba(cr, 0.,0.,1., 1.);
				for (auto& p: stroke.border.backward) {
						cairo_move_to(cr, p.x, p.y);
						cairo_line_to(cr, p.x, p.y);
				}
				cairo_stroke(cr);
				cairo_set_source_rgba(cr, 0.,1.,0., 1.);
				for (auto& p: stroke.path) {
						cairo_move_to(cr, p.x, p.y);
						cairo_line_to(cr, p.x, p.y);
				}
				cairo_stroke(cr);
				cairo_set_source_rgba(cr, 0.,1.,1., 1.);
				const Point& p = stroke.path.front();
						cairo_move_to(cr, p.x, p.y);
						cairo_line_to(cr, p.x, p.y);
				cairo_stroke(cr);
#endif
				
				cairo_restore (cr);
		}
}

static void
strokes_update (int width, int height)
{
		for (auto& stroke: strokes) {
	stroke.y   += stroke.v;
	if (stroke.y > height || stroke.y < 0)
	    stroke.v = -stroke.v;

	stroke.rot += stroke.rv;
    }
}

int main (int argc, char **argv)
{
	struct device *device;
	struct timeval start, last_tty, last_fps, now;

	double delta;
	int frame = 0;
	int frames = 0;
	int benchmark;
	enum clip clip;
	void (*draw) (cairo_t *);
	int i;

	device = device_open(argc, argv);
	clip = device_get_clip(argc, argv);
	benchmark = device_get_benchmark(argc, argv);
	if (benchmark == 0)
		benchmark = 20;

	bool xopp = false;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--xopp") == 0)
			xopp = true;
	}


	for (auto& stroke: strokes) {
			make_stroke(stroke, device->width, device->height);
	}
	draw = xopp ? xopp_stroke_draw : fast_stroke_draw;

	gettimeofday(&start, 0); now = last_tty = last_fps = start;
	do {
		struct framebuffer *fb = device->get_framebuffer (device);
		cairo_t *cr;

		cr = cairo_create(fb->surface);

		cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
		cairo_paint (cr);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

		device_apply_clip (device, cr, clip);
		draw(cr);

		gettimeofday(&now, NULL);
		if (benchmark < 0 && last_fps.tv_sec)
			fps_draw(cr, device->name, &last_fps, &now);
		last_fps = now;

		cairo_destroy(cr);

		fb->show (fb);
		fb->destroy (fb);

		strokes_update(device->width, device->height);

		if (benchmark < 0) {
			delta = now.tv_sec - last_tty.tv_sec;
			delta += (now.tv_usec - last_tty.tv_usec)*1e-6;
			frames++;
			if (delta >  5) {
				printf("%.2f fps\n", frames/delta);
				last_tty = now;
				frames = 0;
			}
		}

		frame++;
		if (benchmark > 0) {
			delta = now.tv_sec - start.tv_sec;
			delta += (now.tv_usec - start.tv_usec)*1e-6;
			if (delta > benchmark) {
				printf("strokes: %.2f fps\n", frame / delta);
				break;
			}
		}
	} while (1);

	return 0;
}
