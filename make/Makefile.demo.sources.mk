# The Rules.mk file for each demo needs to do:
#  1. Place the "standard header" at the top of the Rules.mk, this
#     header is to set Make variable(s) correctly so that the functor
#     filelist will function correctly.
#  2. add its name to DEMOS. Lets say the name of the demo is foo
#  3. Set (using := ) foo_SOURCES the sources the demo has, using filelist
#     to get path correct
#  4. Set (using :=) foo_RESOURCE_STRING the string resources of the demo,
#     using filelist to get path correct
#  5. Place the "standard footer" at the end of the Rules.mk, this
#     restores Make variable(s) correctly so that the functor filelist
#     will function correctly.
#  6. Add to demos/Rules.mk your Rules.mk (follow the form in the file)
#
# Example Rules.mk:
#
# # Begin standard header
# sp 		:= $(sp).x
# dirstack_$(sp):= $(d)
# d		:= $(dir)
# End standard header
#
# DEMOS += foo
# foo_SOURCES := $(call filelist, foo_main.cpp foo_stuff.cpp)
# foo_RESOURCE_STRING := $(call filelist, foo_resource.resource_string)
#
#
# # Begin standard footer
# d		:= $(dirstack_$(sp))
# sp		:= $(basename $(sp))
# # End standard footer

dir := demos
include $(dir)/Rules.mk
