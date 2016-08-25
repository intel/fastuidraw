# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

LIBRARY_RESOURCE_STRING += $(call filelist, fastuidraw_painter_brush.vert.glsl.resource_string \
	fastuidraw_painter_brush_types.glsl.resource_string \
	fastuidraw_painter_brush_unpack.glsl.resource_string \
	fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string \
	fastuidraw_painter_brush_macros.glsl.resource_string \
	fastuidraw_painter_brush.frag.glsl.resource_string \
	)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
