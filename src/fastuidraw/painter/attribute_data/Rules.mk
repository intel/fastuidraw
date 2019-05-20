# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_SOURCES += $(call filelist, \
	painter_attribute_data.cpp \
	stroked_point.cpp \
	stroked_path.cpp filled_path.cpp \
	glyph_sequence.cpp glyph_run.cpp \
	glyph_attribute_packer.cpp \
	arc_stroked_point.cpp \
	stroking_attribute_writer.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
