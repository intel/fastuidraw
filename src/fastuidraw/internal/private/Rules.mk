# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/glsl
include $(dir)/Rules.mk

dir := $(d)/painter_backend
include $(dir)/Rules.mk

dir := $(d)/gl_backend
include $(dir)/Rules.mk

FASTUIDRAW_PRIVATE_SOURCES += $(call filelist, \
	interval_allocator.cpp \
	path_util_private.cpp \
	clip.cpp int_path.cpp \
	util_private_math.cpp \
	pack_texels.cpp rect_atlas.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
