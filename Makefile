CC=/usr/bin/c++
SOURCES:=demo.c xlib.c ximage.c
REQUIRES:=cairo-xlib xext gdk-pixbuf-2.0
DEFINES:=-DHAVE_XLIB=1 -DHAVE_XIMAGE=1

DRM:=0
ifneq ($(DRM),0)
DEFINES+=-DHAVE_DRM=1
SOURCES+=drm.c
REQUIRES+=cairo-drm libdrm
else
DEFINES+=-DHAVE_DRM=0
endif

DEFINES+=-DHAVE_XCB=0

GLX:=$(shell pkg-config --exists cairo-gl && echo 1 || echo 0)
ifneq ($(GLX),0)
DEFINES+=-DHAVE_GLX=1
SOURCES+=glx.c
REQUIRES+=cairo-gl
else
DEFINES+=-DHAVE_GLX=0
endif

COGL:=$(shell pkg-config --exists cairo-cogl && echo 1 || echo 0)
ifneq ($(COGL),0)
DEFINES+=-DHAVE_COGL=1
SOURCES+=cogl.c
REQUIRES+=cairo-cogl
else
DEFINES+=-DHAVE_COGL=0
endif

all: stroke-demo


CFLAGS:=$(shell pkg-config --cflags $(REQUIRES)) -Wall -g3 -std=c++17 -fpermissive -O2
LIBS:=$(shell pkg-config --libs $(REQUIRES)) -ldl -lm

stroke-demo: stroke-demo.cpp $(SOURCES) demo.h Makefile
	$(CC) $(DEFINES) $(CFLAGS) -o $@ stroke-demo.cpp $(SOURCES) $(LIBS)
clean:
	rm -f *-demo

