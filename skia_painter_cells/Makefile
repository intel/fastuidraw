# SKIA Perliminaries
#
# 1. git clone https://skia.googlesource.com/skia.git
# 2. bin/fetch-gn
# 3. tools/install_dependencies.sh
# 4. python tools/git-sync-deps
# 5. bin/gn gen out/Shared --args='is_official_build=true is_component_build=true is_debug=false cc="clang" cxx="clang++" extra_cflags_cc=["-frtti"] extra_cflags = [ "-w" ] skia_use_icu=false'
# 6. ninja -C out/Shared/
#
# Then set SKIA_DIR = $(LOCATION)/skia
#
# Tested against 01979132133e474427a5e615cb05d9e203ae676e
CXX = c++
SKIA_DIR = /home/kevin/github/skia

SOURCES = sdl_demo.cpp sdl_skia_demo.cpp main.cpp generic_command_line.cpp \
	PainterWidget.cpp PanZoomTracker.cpp cell.cpp random.cpp table.cpp \
	cell_group.cpp ImageLoader.cpp


RELEASE_OBJS = $(patsubst %.cpp, release/%.o, $(SOURCES))
DEBUG_OBJS = $(patsubst %.cpp, debug/%.o, $(SOURCES))

SKIA_BUILD_DIR = $(SKIA_DIR)/out/Shared

INCLUDES = -I$(SKIA_DIR)/include/gpu -I$(SKIA_DIR)/include/c -I$(SKIA_DIR)/include/config -I$(SKIA_DIR)/include/core -I$(SKIA_DIR)/include/pathops -I$(SKIA_DIR)/include/pipe -I$(SKIA_DIR)/include/codec -I$(SKIA_DIR)/include/effects -I$(SKIA_DIR)/include/images -I$(SKIA_DIR)/include/ports -I$(SKIA_DIR)/src/sfnt -I$(SKIA_DIR)/include/utils -I$(SKIA_DIR)/src/utils -I$(SKIA_DIR)/include/views -I$(SKIA_DIR)/include/xml

COMMON_FLAGS = -std=c++11 `sdl2-config --cflags` -DGL_GLEXT_PROTOTYPES -fno-exceptions

DEBUG_FLAGS = $(COMMON_FLAGS)  -g $(INCLUDES)
RELEASE_FLAGS = -DNDEBUG $(COMMON_FLAGS) $(INCLUDES) -O3

LIBS = -lpthread -lz -ldl -lexpat -lfontconfig -lfreetype -lGL -lGLU -lX11 `sdl2-config --libs` -lSDL2_image -L$(SKIA_BUILD_DIR) -lskia

all: skia-painter-cells-release skia-painter-cells-debug

skia-painter-cells-release: $(RELEASE_OBJS)
	$(CXX) -m64 -o skia-painter-cells-release -Wl,--start-group $(RELEASE_OBJS) $(LIBS)


skia-painter-cells-debug: $(DEBUG_OBJS)
	$(CXX) -m64 -o skia-painter-cells-debug -Wl,--start-group $(DEBUG_OBJS) $(LIBS)

release/%.o: %.cpp
	@mkdir -p release
	$(CXX) $(RELEASE_FLAGS) -c $< -o $@

debug/%.o: %.cpp
	@mkdir -p debug
	$(CXX) $(DEBUG_FLAGS) -c $< -o $@

clean:
	-rm -f release/*.o debug/*.o
	-rm -f *~
	-rm -f skia-painter-cells-release skia-painter-cells-debug
