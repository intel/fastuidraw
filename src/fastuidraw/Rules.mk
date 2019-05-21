# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_SOURCES += $(call filelist, image.cpp colorstop.cpp \
	colorstop_atlas.cpp path.cpp tessellated_path.cpp \
	partitioned_tessellated_path.cpp path_effect.cpp)

dir := $(d)/util
include $(dir)/Rules.mk

dir := $(d)/text
include $(dir)/Rules.mk

dir := $(d)/painter
include $(dir)/Rules.mk

dir := $(d)/internal
include $(dir)/Rules.mk

dir := $(d)/glsl
include $(dir)/Rules.mk

dir := $(d)/gl_backend
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
