# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_PRIVATE_GL_SOURCES += $(call filelist, \
	tex_buffer.cpp texture_gl.cpp \
	image_gl.cpp colorstop_atlas_gl.cpp \
	texture_view.cpp bindless.cpp \
	painter_backend_gl_config.cpp \
	painter_shader_registrar_gl.cpp \
	painter_surface_gl_private.cpp \
	glyph_atlas_gl.cpp \
	painter_backend_gl.cpp \
	painter_vao_pool.cpp)


# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
