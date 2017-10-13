# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

GL_HEADER_FILES=$(GL_RAW_HEADER_FILES:%.h=$(GL_INCLUDEPATH)/%.h)
GLES_HEADER_FILES=$(GLES_RAW_HEADER_FILES:%.h=$(GL_INCLUDEPATH)/%.h)

NGL_GL_CPP = src/fastuidraw/gl_backend/ngl_gl.cpp
NGL_GL_HPP = inc/fastuidraw/gl_backend/ngl_gl.hpp

NGL_GLES_CPP = src/fastuidraw/gl_backend/ngl_gles3.cpp
NGL_GLES_HPP = inc/fastuidraw/gl_backend/ngl_gles3.hpp

$(NGL_GL_CPP): $(NGL_GL_HPP) $(NGL_FILTER) $(NGL_EXTRACTOR)
$(NGL_GL_HPP): $(NGL_FILTER) $(NGL_EXTRACTOR)
	$(NGL_FILTER) $(GL_HEADER_FILES) \
	| $(NGL_EXTRACTOR) macro_prefix=FASTUIDRAWgl \
namespace="fastuidraw::gl_binding" path=$(GL_INCLUDEPATH) \
output_cpp=$(NGL_GL_CPP) output_hpp=$(NGL_GL_HPP) $(GL_RAW_HEADER_FILES)

$(NGL_GLES_CPP): $(NGL_GLES_HPP) $(NGL_FILTER) $(NGL_EXTRACTOR)
$(NGL_GLES_HPP): $(NGL_FILTER) $(NGL_EXTRACTOR)
	$(NGL_FILTER) $(GLES_HEADER_FILES) \
	| $(NGL_EXTRACTOR) macro_prefix=FASTUIDRAWgl \
namespace="fastuidraw::gl_binding" path=$(GL_INCLUDEPATH) \
output_cpp=$(NGL_GLES_CPP) output_hpp=$(NGL_GLES_HPP) $(GLES_RAW_HEADER_FILES)

SUPER_CLEAN_FILES += $(NGL_GL_CPP) $(NGL_GL_HPP) $(NGL_GLES_CPP) $(NGL_GLES_HPP)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
