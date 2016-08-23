# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

LIBRARY_RESOURCE_STRING += $(call filelist, fastuidraw_painter_anisotropic.frag.glsl.resource_string \
	fastuidraw_painter_glyph_coverage.vert.glsl.resource_string \
	fastuidraw_painter_glyph_coverage.frag.glsl.resource_string \
	fastuidraw_painter_glyph_distance_field.vert.glsl.resource_string \
	fastuidraw_painter_glyph_distance_field.frag.glsl.resource_string \
	fastuidraw_painter_glyph_distance_field_anisotropic.frag.glsl.resource_string \
	fastuidraw_painter_glyph_curve_pair.vert.glsl.resource_string \
	fastuidraw_painter_glyph_curve_pair.frag.glsl.resource_string \
	fastuidraw_painter_glyph_curve_pair_anisotropic.frag.glsl.resource_string \
	)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
