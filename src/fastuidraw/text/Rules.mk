# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/private
include $(dir)/Rules.mk

LIBRARY_SOURCES += $(call filelist, glyph_atlas.cpp \
	glyph_render_data.cpp \
	glyph_render_data_curve_pair.cpp \
	glyph_render_data_distance_field.cpp \
	glyph_render_data_coverage.cpp \
	glyph_cache.cpp glyph_selector.cpp \
	freetype_font.cpp freetype_lib.cpp \
	font_properties.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
