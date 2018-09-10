# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

DEMOS += painter-glyph-test
painter-glyph-test_SOURCES := $(call filelist, main.cpp)
painter-glyph-test_RESOURCE_STRING := $(call filelist, show_atlas.frag.glsl.resource_string \
	show_atlas.vert.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
