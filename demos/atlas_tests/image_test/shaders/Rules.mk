# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

image-test_RESOURCE_STRING := $(call filelist, layer_texture_blit.vert.glsl.resource_string layer_texture_blit.frag.glsl.resource_string atlas_image_blit.frag.glsl.resource_string atlas_image_blit.vert.glsl.resource_string detect_boundary.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
