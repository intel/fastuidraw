# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/attribute_data
include $(dir)/Rules.mk

dir := $(d)/backend
include $(dir)/Rules.mk

dir := $(d)/shader
include $(dir)/Rules.mk

dir := $(d)/effects
include $(dir)/Rules.mk

FASTUIDRAW_SOURCES += $(call filelist, fill_rule.cpp \
	painter_packed_value.cpp \
	painter_brush.cpp \
	painter_stroke_params.cpp \
	painter_dashed_stroke_params.cpp \
	painter.cpp painter_enums.cpp \
	shader_filled_path.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
