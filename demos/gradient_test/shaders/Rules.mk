# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

gradient-test_RESOURCE_STRING := $(call filelist, linear_gradient.vert.glsl.resource_string linear_gradient.frag.glsl.resource_string draw_pt.vert.glsl.resource_string draw_pt.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
