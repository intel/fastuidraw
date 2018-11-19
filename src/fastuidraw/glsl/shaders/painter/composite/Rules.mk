# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/single_src
include $(dir)/Rules.mk

dir := $(d)/dual_src
include $(dir)/Rules.mk

dir := $(d)/framebuffer_fetch
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
