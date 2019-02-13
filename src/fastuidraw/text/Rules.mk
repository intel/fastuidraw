# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_SOURCES += $(call filelist, glyph_atlas.cpp \
	font.cpp glyph_attribute.cpp font_database.cpp \
	glyph_renderer.cpp \
	glyph_render_data_restricted_rays.cpp \
	glyph_render_data_banded_rays.cpp \
	glyph_render_data_texels.cpp \
	glyph_cache.cpp glyph.cpp \
	freetype_face.cpp freetype_lib.cpp \
	font_freetype.cpp font_properties.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
