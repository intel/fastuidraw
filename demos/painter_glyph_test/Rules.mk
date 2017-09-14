# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

DEMOS += painter-glyph-test
painter-glyph-test_SOURCES := $(call filelist, main.cpp glyph_finder.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
