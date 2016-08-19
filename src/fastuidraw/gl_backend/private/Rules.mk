# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

LIBRARY_PRIVATE_GL_SOURCES += $(call filelist, tex_buffer.cpp texture_gl.cpp texture_view.cpp \
	backend_shaders.cpp uber_shader_builder.cpp)


# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
