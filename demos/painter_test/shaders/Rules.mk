# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

painter-test_RESOURCE_STRING := $(call filelist, vert0.glsl.resource_string vert1.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
