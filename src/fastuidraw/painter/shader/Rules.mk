# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_SOURCES += $(call filelist, \
	painter_shader.cpp painter_shader_set.cpp \
	painter_dashed_stroke_shader_set.cpp \
	painter_stroke_shader.cpp \
	painter_glyph_shader.cpp \
	painter_blend_shader_set.cpp \
	painter_brush_shader_set.cpp \
	painter_fill_shader.cpp \
	painter_image_brush_shader.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
