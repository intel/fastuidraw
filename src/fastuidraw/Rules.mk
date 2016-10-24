# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


dir := $(d)/util
include $(dir)/Rules.mk

dir := $(d)/text
include $(dir)/Rules.mk

dir := $(d)/painter
include $(dir)/Rules.mk

dir := $(d)/private
include $(dir)/Rules.mk

dir := $(d)/glsl
include $(dir)/Rules.mk

dir := $(d)/gl_backend
include $(dir)/Rules.mk

LIBRARY_SOURCES += $(call filelist, image.cpp colorstop.cpp colorstop_atlas.cpp path.cpp tessellated_path.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
