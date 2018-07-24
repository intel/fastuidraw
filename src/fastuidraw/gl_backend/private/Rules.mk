# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_PRIVATE_GL_SOURCES += $(call filelist, tex_buffer.cpp \
	texture_gl.cpp texture_view.cpp bindless.cpp \
	painter_backend_gl_config.cpp)


# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
