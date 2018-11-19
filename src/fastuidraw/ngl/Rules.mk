# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

EGL_HEADER_FILES=$(EGL_RAW_HEADER_FILES:%.h=$(EGL_INCLUDEPATH)/%.h)

NGL_EGL_CPP = src/fastuidraw/ngl_egl.cpp
NGL_EGL_HPP = inc/fastuidraw/ngl_egl.hpp

NEGL_SRCS += $(NGL_EGL_CPP)

$(NGL_EGL_CPP): $(NGL_EGL_HPP) $(NGL_FILTER) $(NGL_EXTRACTOR)
$(NGL_EGL_HPP): $(NGL_FILTER) $(NGL_EXTRACTOR)
	$(NGL_FILTER) $(EGL_HEADER_FILES) \
	| $(NGL_EXTRACTOR) macro_prefix=FASTUIDRAWegl \
namespace="fastuidraw::egl_binding" path=$(EGL_INCLUDEPATH) \
output_cpp=$(NGL_EGL_CPP) output_hpp=$(NGL_EGL_HPP) $(EGL_RAW_HEADER_FILES)


SUPER_CLEAN_FILES += $(NGL_EGL_CPP) $(NGL_EGL_HPP)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
