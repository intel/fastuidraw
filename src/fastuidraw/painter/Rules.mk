# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


dir := $(d)/packing
include $(dir)/Rules.mk

LIBRARY_SOURCES += $(call filelist, painter_attribute_data.cpp painter_brush.cpp \
	painter.cpp painter_enums.cpp painter_state.cpp painter_shader.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
