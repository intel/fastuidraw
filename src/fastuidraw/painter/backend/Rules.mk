# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_SOURCES += $(call filelist, \
	painter_shader_group.cpp \
	painter_surface.cpp \
	painter_backend_factory.cpp \
	painter_header.cpp \
	painter_clip_equations.cpp \
	painter_item_matrix.cpp \
	painter_shader_registrar.cpp \
	painter_brush_adjust.cpp \
	painter_draw.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
