# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/common
include $(dir)/Rules.mk

dir := $(d)/path
include $(dir)/Rules.mk

dir := $(d)/text
include $(dir)/Rules.mk

dir := $(d)/gradient
include $(dir)/Rules.mk

dir := $(d)/image
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
