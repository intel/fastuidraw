# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


dir := $(d)/packing
include $(dir)/Rules.mk

LIBRARY_SOURCES += $(call filelist, painter_attribute_data.cpp \
	painter_brush.cpp painter_stroke_value.cpp \
	painter.cpp painter_enums.cpp painter_value.cpp \
	painter_shader.cpp painter_shader_set.cpp \
	painter_dashed_stroke_shader_set.cpp painter_stroke_shader.cpp \
	painter_glyph_shader.cpp painter_blend_shader_set.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
