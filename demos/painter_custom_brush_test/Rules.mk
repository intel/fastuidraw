# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

DEMOS += painter-custom-brush-test
painter-custom-brush-test_SOURCES := $(call filelist, main.cpp)
painter-custom-brush-test_RESOURCE_STRING := $(call filelist, \
	custom_brush_example.vert.glsl.resource_string \
	custom_brush_example.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
