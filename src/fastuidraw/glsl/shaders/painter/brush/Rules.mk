# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_RESOURCE_STRING += $(call filelist, \
	fastuidraw_brush_utils.glsl.resource_string \
	fastuidraw_painter_brush.vert.glsl.resource_string \
	fastuidraw_painter_brush.frag.glsl.resource_string \
	fastuidraw_image_brush_utils.glsl.resource_string \
	fastuidraw_painter_image_brush.vert.glsl.resource_string \
	fastuidraw_painter_image_brush.frag.glsl.resource_string)


# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
