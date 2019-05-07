# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/painter
include $(dir)/Rules.mk

FASTUIDRAW_RESOURCE_STRING += $(call filelist, \
	fastuidraw_bit_utils.glsl.resource_string \
	fastuidraw_atlases.glsl.resource_string \
	fastuidraw_atlas_image_fetch.glsl.resource_string \
	fastuidraw_spread.glsl.resource_string \
	fastuidraw_gradient.glsl.resource_string \
	fastuidraw_compute_interval.glsl.resource_string \
	fastuidraw_circular_interpolate.glsl.resource_string \
	fastuidraw_anisotropic.frag.glsl.resource_string \
	fastuidraw_unpack_unit_vector.glsl.resource_string \
	fastuidraw_align.vert.glsl.resource_string \
	fastuidraw_read_texels_from_data.glsl.resource_string \
	fastuidraw_texture_fetch.glsl.resource_string \
	fastuidraw_restricted_rays.glsl.resource_string \
	fastuidraw_banded_rays.glsl.resource_string \
	fastuidraw_compute_local_distance_from_pixel_distance.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
