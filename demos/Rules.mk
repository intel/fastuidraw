# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/common
include $(dir)/Rules.mk

dir := $(d)/painter_test
include $(dir)/Rules.mk

dir := $(d)/painter_path_test
include $(dir)/Rules.mk

dir := $(d)/painter_glyph_test
include $(dir)/Rules.mk

dir := $(d)/painter_cliprect_test
include $(dir)/Rules.mk

dir := $(d)/painter_clippath_test
include $(dir)/Rules.mk

dir := $(d)/painter_cells
include $(dir)/Rules.mk

dir := $(d)/painter_simple_test
include $(dir)/Rules.mk

dir := $(d)/painter_custom_brush_test
include $(dir)/Rules.mk

dir := $(d)/tutorial
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
