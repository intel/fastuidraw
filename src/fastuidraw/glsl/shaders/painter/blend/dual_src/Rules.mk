# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


LIBRARY_RESOURCE_STRING += $(call filelist, fastuidraw_porter_duff_clear.glsl.resource_string \
	fastuidraw_porter_duff_dst_out.glsl.resource_string \
	fastuidraw_porter_duff_src_in.glsl.resource_string \
	fastuidraw_porter_duff_dst_atop.glsl.resource_string \
	fastuidraw_porter_duff_dst_over.glsl.resource_string \
	fastuidraw_porter_duff_src_out.glsl.resource_string \
	fastuidraw_porter_duff_dst.glsl.resource_string \
	fastuidraw_porter_duff_src_atop.glsl.resource_string \
	fastuidraw_porter_duff_src_over.glsl.resource_string \
	fastuidraw_porter_duff_dst_in.glsl.resource_string \
	fastuidraw_porter_duff_src.glsl.resource_string \
	fastuidraw_porter_duff_xor.glsl.resource_string \
	)


# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
