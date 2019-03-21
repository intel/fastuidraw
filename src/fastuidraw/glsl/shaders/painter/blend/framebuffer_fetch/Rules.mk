# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


FASTUIDRAW_RESOURCE_STRING += $(call filelist, fastuidraw_fbf_porter_duff_clear.glsl.resource_string \
	fastuidraw_fbf_porter_duff_dst_out.glsl.resource_string \
	fastuidraw_fbf_porter_duff_src_in.glsl.resource_string \
	fastuidraw_fbf_porter_duff_dst_atop.glsl.resource_string \
	fastuidraw_fbf_porter_duff_dst_over.glsl.resource_string \
	fastuidraw_fbf_porter_duff_src_out.glsl.resource_string \
	fastuidraw_fbf_porter_duff_dst.glsl.resource_string \
	fastuidraw_fbf_porter_duff_src_atop.glsl.resource_string \
	fastuidraw_fbf_porter_duff_src_over.glsl.resource_string \
	fastuidraw_fbf_porter_duff_dst_in.glsl.resource_string \
	fastuidraw_fbf_porter_duff_src.glsl.resource_string \
	fastuidraw_fbf_porter_duff_xor.glsl.resource_string \
	fastuidraw_fbf_porter_duff_plus.glsl.resource_string \
	fastuidraw_fbf_porter_duff_modulate.glsl.resource_string \
	fastuidraw_blend_util.frag.glsl.resource_string \
	fastuidraw_fbf_w3c_normal.glsl.resource_string \
	fastuidraw_fbf_w3c_multiply.glsl.resource_string \
	fastuidraw_fbf_w3c_screen.glsl.resource_string \
	fastuidraw_fbf_w3c_overlay.glsl.resource_string \
	fastuidraw_fbf_w3c_darken.glsl.resource_string \
	fastuidraw_fbf_w3c_lighten.glsl.resource_string \
	fastuidraw_fbf_w3c_color_dodge.glsl.resource_string \
	fastuidraw_fbf_w3c_color_burn.glsl.resource_string \
	fastuidraw_fbf_w3c_hardlight.glsl.resource_string \
	fastuidraw_fbf_w3c_softlight.glsl.resource_string \
	fastuidraw_fbf_w3c_difference.glsl.resource_string \
	fastuidraw_fbf_w3c_exclusion.glsl.resource_string \
	fastuidraw_fbf_w3c_hue.glsl.resource_string \
	fastuidraw_fbf_w3c_saturation.glsl.resource_string \
	fastuidraw_fbf_w3c_color.glsl.resource_string \
	fastuidraw_fbf_w3c_luminosity.glsl.resource_string \
	)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
