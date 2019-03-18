# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/glu-tess
include $(dir)/Rules.mk

dir := $(d)/ieeehalfprecision
include $(dir)/Rules.mk


# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
