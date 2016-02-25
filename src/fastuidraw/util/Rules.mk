# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

LIBRARY_SOURCES += $(call filelist, static_resource.cpp \
	fastuidraw_memory.cpp util.cpp math.cpp \
	reference_count_mutex.cpp reference_count_atomic.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
