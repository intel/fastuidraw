# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header


DEMOS += painter-cells
painter-cells_SOURCES := $(call filelist, main.cpp cell.cpp table.cpp cell_group.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
