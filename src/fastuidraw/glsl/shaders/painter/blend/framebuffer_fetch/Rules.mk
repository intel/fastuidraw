# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_RESOURCE_STRING += $(call filelist, \
	fastuidraw_fbf_w3c_normal.glsl.resource_string \
	fastuidraw_fbf_w3c_multiply.glsl.resource_string \
	fastuidraw_fbf_w3c_screen.glsl.resource_string \
	)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
