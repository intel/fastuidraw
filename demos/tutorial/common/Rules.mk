# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

COMMON_DEMO_SOURCES += $(call filelist, demo_framework.cpp initialization.cpp image_loader.cpp)

TUTORIALS += example_initialization
example_initialization_SOURCES := $(call filelist, initialization_main.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
