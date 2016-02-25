# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/ngl_generator
include $(dir)/Rules.mk

dir := $(d)/ngl
include $(dir)/Rules.mk

dir := $(d)/shaders
include $(dir)/Rules.mk



LIBRARY_GL_SOURCES += $(call filelist, image_gl.cpp gl_program.cpp gl_context_properties.cpp \
	colorstop_atlas_gl.cpp glyph_atlas_gl.cpp painter_shader_gl.cpp \
	painter_blend_shader_gl.cpp painter_backend_gl.cpp)

NGL_COMMON_SRCS += $(call filelist, gl_binding.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
