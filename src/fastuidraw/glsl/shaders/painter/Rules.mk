# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


dir := $(d)/glyph
include $(dir)/Rules.mk

dir := $(d)/brush
include $(dir)/Rules.mk

dir := $(d)/blend
include $(dir)/Rules.mk

FASTUIDRAW_RESOURCE_STRING += $(call filelist, \
	fastuidraw_painter_types.glsl.resource_string \
	fastuidraw_painter_forward_declares.vert.glsl.resource_string \
	fastuidraw_painter_uniforms.glsl.resource_string \
	fastuidraw_painter_auxiliary_image_buffer.glsl.resource_string \
	fastuidraw_painter_forward_declares.frag.glsl.resource_string \
	fastuidraw_painter_main.frag.glsl.resource_string \
	fastuidraw_painter_main.geom.glsl.resource_string \
	fastuidraw_painter_main.vert.glsl.resource_string \
	fastuidraw_painter_stroke_util.vert.glsl.resource_string \
	fastuidraw_painter_stroke_util.frag.glsl.resource_string \
	fastuidraw_painter_stroke_util.constants.glsl.resource_string \
	fastuidraw_painter_stroke.vert.glsl.resource_string \
	fastuidraw_painter_stroke_compute_offset.vert.glsl.resource_string \
	fastuidraw_painter_stroke.frag.glsl.resource_string \
	fastuidraw_painter_arc_stroke.vert.glsl.resource_string \
	fastuidraw_painter_arc_stroke.frag.glsl.resource_string \
	fastuidraw_painter_fill.vert.glsl.resource_string \
	fastuidraw_painter_fill.frag.glsl.resource_string \
	fastuidraw_painter_fill_aa_fuzz.vert.glsl.resource_string \
	fastuidraw_painter_fill_aa_fuzz.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
