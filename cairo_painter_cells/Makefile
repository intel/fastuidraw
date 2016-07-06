CXX=g++

SOURCES=generic_command_line.cpp random.cpp test.cpp sdl_cairo_demo.cpp

CAIRO_PKG_STUFF= cairo-xlib cairo-xcb xext
CFLAGS_BASE=$(shell pkg-config --cflags $(CAIRO_PKG_STUFF)) $(shell sdl2-config --cflags) -Wall -Wextra -Wcast-qual -Wwrite-strings
LIBS=-lm $(shell pkg-config --libs $(CAIRO_PKG_STUFF)) $(shell sdl2-config --libs)

DEBUG_FLAGS=-g $(CFLAGS_BASE)
RELEASE_FLAGS=-O3 -DNDEBUG $(CFLAGS_BASE)

RELEASE_OBJS=$(patsubst %.cpp, release/%.o, $(SOURCES))
DEBUG_OBJS=$(patsubst %.cpp, debug/%.o, $(SOURCES))

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
