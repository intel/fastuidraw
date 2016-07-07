CXX=g++

CAIRO_PKG_STUFF= cairo-xlib

# For many distros, Cairo is built without cairo-gl.
# If using a custom cairo installed outside of default
# distro paths, one needs to do:
#  when building: export PKG_CONFIG_PATH=/path/to/custom/cairo/lib/pkgconfig
#  when running: export LD_LIBRARY_PATH=/path/to/custom/cairo/lib/
# where /path/to/custom is the value of --prefix used when running configure
# script of building Cairo; the LD_LIBRARY_PATH is needed because the linker
# loader ends up prefering the system Cairo over the one in path although
# pkgconfig --libs includes -L/path/to/custom/cairo
CAIRO_GL=$(shell pkg-config --exists cairo-gl && echo 1 || echo 0)
ifneq ($(CAIRO_GL),0)
DEFINES+=-DHAVE_CAIRO_GL=1
CAIRO_PKG_STUFF+=cairo-gl
else
DEFINES+=-DHAVE_CAIRO_GL=0
endif

SOURCES=generic_command_line.cpp random.cpp sdl_cairo_demo.cpp \
	PainterWidget.cpp PanZoomTracker.cpp ImageLoader.cpp \
	cell.cpp \
	test.cpp

CFLAGS_BASE=$(shell pkg-config --cflags $(CAIRO_PKG_STUFF)) $(shell sdl2-config --cflags) -Wall -Wextra -Wcast-qual -Wwrite-strings $(DEFINES)
LIBS=-lm $(shell pkg-config --libs $(CAIRO_PKG_STUFF)) $(shell sdl2-config --libs) -lSDL2_image

DEBUG_FLAGS=-g $(CFLAGS_BASE)
RELEASE_FLAGS=-O3 -DNDEBUG $(CFLAGS_BASE)

RELEASE_OBJS=$(patsubst %.cpp, release/%.o, $(SOURCES))
DEBUG_OBJS=$(patsubst %.cpp, debug/%.o, $(SOURCES))

all: cairo-painter-cells-release cairo-painter-cells-debug

cairo-painter-cells-release: $(RELEASE_OBJS)
	$(CXX) -o $@ $(RELEASE_OBJS) $(LIBS)

cairo-painter-cells-debug: $(DEBUG_OBJS)
	$(CXX) -o $@ $(DEBUG_OBJS) $(LIBS)

release/%.o: %.cpp
	@mkdir -p release
	$(CXX) $(RELEASE_FLAGS) -c $< -o $@

debug/%.o: %.cpp
	@mkdir -p debug
	$(CXX) $(DEBUG_FLAGS) -c $< -o $@

clean:
	-rm $(RELEASE_OBJS) $(DEBUG_OBJS)  *~ cairo-painter-cells-release cairo-painter-cells-debug
