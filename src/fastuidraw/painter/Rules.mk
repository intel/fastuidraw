# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


dir := $(d)/backend
include $(dir)/Rules.mk

FASTUIDRAW_SOURCES += $(call filelist, fill_rule.cpp \
	painter_packed_value.cpp \
	painter_attribute_data.cpp \
	painter_brush.cpp painter_stroke_params.cpp \
	painter_dashed_stroke_params.cpp \
	painter.cpp painter_enums.cpp \
	painter_shader_data.cpp \
	painter_shader.cpp painter_shader_set.cpp \
	painter_dashed_stroke_shader_set.cpp \
	painter_stroke_shader.cpp \
	painter_glyph_shader.cpp \
	painter_composite_shader_set.cpp \
	painter_blend_shader_set.cpp \
	painter_fill_shader.cpp \
	stroked_caps_joins.cpp stroked_point.cpp \
	stroked_path.cpp filled_path.cpp \
	glyph_sequence.cpp glyph_run.cpp \
	arc_stroked_point.cpp \
	shader_filled_path.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
