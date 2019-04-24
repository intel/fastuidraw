CXX ?= g++

###########################################
# The demo_framework used SDL2 to handle
# window and GL context creation.
CXX_COMMON_FLAGS = $(shell pkg-config sdl2 --cflags)
CXX_COMMON_LIBS = $(shell pkg-config sdl2 --libs)

###############################################
# The debug and release libraries of FastUIDraw
# are NOT ABI compatible. Below we use using
# the GL backend of FastUIDraw. The GLES pkg-config
# modules are fastuidrawGLES-release and
# fastuidrawGLES-debug
CXX_RELEASE_FLAGS = -O3 $(CXX_COMMON_FLAGS) $(shell pkg-config fastuidrawGL-release --cflags)
CXX_RELEASE_LIBS = $(CXX_COMMON_LIBS) $(shell pkg-config fastuidrawGL-release --libs)

CXX_DEBUG_FLAGS = -g $(CXX_COMMON_FLAGS) $(shell pkg-config fastuidrawGL-debug --cflags)
CXX_DEBUG_LIBS = $(CXX_COMMON_LIBS) $(shell pkg-config fastuidrawGL-debug --libs)

all: initialization-release initialization-debug

initialization-release: release/demo_framework.o release/initialization.o release/initialization_main.o
	$(CXX) -o $@ release/demo_framework.o release/initialization.o release/initialization_main.o $(CXX_RELEASE_LIBS)

initialization-debug: debug/demo_framework.o debug/initialization.o debug/initialization_main.o
	$(CXX) -o $@ debug/demo_framework.o debug/initialization.o debug/initialization_main.o $(CXX_DEBUG_LIBS)

release/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXX_RELEASE_FLAGS) -c $< -o $@

debug/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXX_DEBUG_FLAGS) -c $< -o $@

clean:
	-rm initialization-release initialization-debug
	-rm -r debug release
