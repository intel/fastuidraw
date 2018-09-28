# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_PRIVATE_SOURCES += $(call filelist, \
	interval_allocator.cpp \
	path_util_private.cpp \
	clip.cpp int_path.cpp \
	util_private_math.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
