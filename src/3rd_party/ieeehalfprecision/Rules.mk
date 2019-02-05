# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_PRIVATE_SOURCES += $(call filelist, ieeehalfprecision.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
