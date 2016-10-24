# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


dir := $(d)/packing
include $(dir)/Rules.mk

LIBRARY_SOURCES += $(call filelist, \
	stroked_path.cpp filled_path.cpp \
	painter_attribute_data.cpp \
	painter_attribute_data_filler_path_fill.cpp \
	painter_attribute_data_filler_glyphs.cpp \
	painter_brush.cpp painter_stroke_params.cpp \
	painter_dashed_stroke_params.cpp \
	painter.cpp painter_enums.cpp \
	painter_shader_data.cpp \
	painter_clip_equations.cpp \
	painter_item_matrix.cpp painter_header.cpp \
	painter_shader.cpp painter_shader_set.cpp \
	painter_dashed_stroke_shader_set.cpp painter_stroke_shader.cpp \
	painter_glyph_shader.cpp painter_blend_shader_set.cpp \
	painter_fill_shader.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
