# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_RESOURCE_STRING += $(call filelist, \
	fastuidraw_painter_glyph_coverage.vert.glsl.resource_string \
	fastuidraw_painter_glyph_coverage.frag.glsl.resource_string \
	fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string \
	fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string \
	fastuidraw_painter_glyph_restricted_rays.vert.glsl.resource_string \
	fastuidraw_painter_glyph_restricted_rays.frag.glsl.resource_string \
	)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
