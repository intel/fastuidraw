# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_SOURCES += $(call filelist, \
	painter_backend.cpp painter_header.cpp \
	painter_clip_equations.cpp \
	painter_item_matrix.cpp \
	painter_shader_registrar.cpp \
	painter_draw.cpp painter_packer.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
