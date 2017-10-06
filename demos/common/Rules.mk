# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/shaders
include $(dir)/Rules.mk

COMMON_DEMO_SOURCES := $(call filelist, generic_command_line.cpp colorstop_command_line.cpp \
	sdl_benchmark.cpp sdl_demo.cpp sdl_painter_demo.cpp PanZoomTracker.cpp \
	ImageLoader.cpp read_colorstops.cpp read_path.cpp text_helper.cpp \
	PainterWidget.cpp cycle_value.cpp random.cpp read_dash_pattern.cpp \
	egl_helper.cpp)


# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
