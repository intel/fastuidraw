# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

glyph-test_RESOURCE_STRING := $(call filelist, glyph.vert.glsl.resource_string coverage_glyph.frag.glsl.resource_string distance_glyph.frag.glsl.resource_string curvepair_glyph.frag.glsl.resource_string glyph_atlas.vert.glsl.resource_string glyph_atlas.frag.glsl.resource_string perform_aa.frag.glsl.resource_string gles_prec.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
