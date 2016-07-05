# Location of SKIA using insrtuctions at
# https://sites.google.com/site/skiadocs/user-documentation/downloading
# Tested against 07e2692da3971960f8ae8adcd78ccb4bae7ab446
CXX = c++
SKIA_DIR = /home/kevin/skia_depot



# WARNING: this makefile is HACKED together from copy-pasting the compile
# and link commands from building SKIA. You have been warned!
#
#
#


SOURCES = sdl_demo.cpp sdl_skia_demo.cpp main.cpp generic_command_line.cpp \
	PainterWidget.cpp PanZoomTracker.cpp cell.cpp random.cpp table.cpp \
	cell_group.cpp ImageLoader.cpp


RELEASE_OBJS = $(patsubst %.cpp, release/%.o, $(SOURCES))
DEBUG_OBJS = $(patsubst %.cpp, debug/%.o, $(SOURCES))

SKIA_DEBUG_BUILD_DIR = $(SKIA_DIR)/out/Debug
SKIA_RELEASE_BUILD_DIR = $(SKIA_DIR)/out/Release

INCLUDES = -I$(SKIA_DIR)/include/gpu -I$(SKIA_DIR)/include/c -I$(SKIA_DIR)/include/config -I$(SKIA_DIR)/include/core -I$(SKIA_DIR)/include/pathops -I$(SKIA_DIR)/include/pipe -I$(SKIA_DIR)/include/codec -I$(SKIA_DIR)/include/effects -I$(SKIA_DIR)/include/images -I$(SKIA_DIR)/include/ports -I$(SKIA_DIR)/src/sfnt -I$(SKIA_DIR)/include/utils -I$(SKIA_DIR)/src/utils -I$(SKIA_DIR)/include/views -I$(SKIA_DIR)/include/xml

DEFINES = -DSK_INTERNAL -DSK_GAMMA_SRGB -DSK_GAMMA_APPLY_TO_A8 -DSK_ALLOW_STATIC_GLOBAL_INITIALIZERS=1 -DSK_SUPPORT_GPU=1 -DSK_SUPPORT_OPENCL=0 -DSK_FORCE_DISTANCE_FIELD_TEXT=0 -DSK_SAMPLES_FOR_X -DSK_BUILD_FOR_UNIX -DSK_DEVELOPER=1

COMMON_FLAGS = -g -fstrict-aliasing -Wall -Wextra -Winit-self -Wpointer-arith -Wsign-compare -Wno-unused-parameter -m64 -Werror -std=c++11 -fno-rtti -fno-threadsafe-statics -Wnon-virtual-dtor $(DEFINES) `sdl2-config --cflags` -fno-exceptions

SKIA_DEBUG_FLAGS = $(COMMON_FLAGS) $(INCLUDES)
SKIA_RELEASE_FLAGS = -DNDEBUG $(COMMON_FLAGS) $(INCLUDES) -O3

SKIA_SHARED_LIBS = -lpthread -lz -ldl -lexpat -lfontconfig -lfreetype -lGL -lGLU -lX11 `sdl2-config --libs` -lSDL2_image


SKIA_LIBS = libskia_core.a libskia_codec.a obj/gyp/libgiflib.a obj/gyp/libjpeg-turbo.a libpng_static.a obj/gyp/libpng_static_neon.a obj/gyp/libwebp_dec.a obj/gyp/libwebp_demux.a obj/gyp/libwebp_dsp.a obj/gyp/libwebp_enc.a obj/gyp/libwebp_dsp_enc.a obj/gyp/libwebp_utils.a libskia_effects.a libskia_images.a obj/gyp/libetc1.a obj/gyp/libSkKTX.a libskia_utils.a libskia_opts.a libskia_opts_ssse3.a libskia_opts_sse41.a libskia_opts_sse42.a libskia_opts_avx.a libskia_opts_avx2.a libskia_ports.a libskia_sfnt.a libskia_skgpu.a libskia_xml.a
# libskia_views.a

SKIA_DEBUG_LIBS = $(patsubst %.a, $(SKIA_DEBUG_BUILD_DIR)/%.a, $(SKIA_LIBS))
SKIA_RELEASE_LIBS = $(patsubst %.a, $(SKIA_RELEASE_BUILD_DIR)/%.a, $(SKIA_LIBS))

all: skia-painter-cells-release skia-painter-cells-debug

skia-painter-cells-release: $(RELEASE_OBJS)
	$(CXX) -m64 -o skia-painter-cells-release -Wl,--start-group $(RELEASE_OBJS) $(SKIA_RELEASE_LIBS) -Wl,--end-group $(SKIA_SHARED_LIBS)


skia-painter-cells-debug: $(DEBUG_OBJS)
	$(CXX) -m64 -o skia-painter-cells-debug -Wl,--start-group $(DEBUG_OBJS) $(SKIA_DEBUG_LIBS) -Wl,--end-group $(SKIA_SHARED_LIBS)

release/%.o: %.cpp
	@mkdir -p release
	$(CXX) $(SKIA_RELEASE_FLAGS) -c $< -o $@

debug/%.o: %.cpp
	@mkdir -p debug
	$(CXX) $(SKIA_DEBUG_FLAGS) -c $< -o $@

clean:
	-rm release/*.o debug/*.o
	-rm *~
	-rm skia-painter-cells-release skia-painter-cells-debug
