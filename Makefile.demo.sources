# The Rules.mk file for each demo needs to do:
#  1. add its name to DEMOS. Lets say the name of the demo is foo
#  2. add to foo_SOURCES the sources the demo has
#  3. add to foo_RESOURCE_STRING the string resources of the demo
#
#  example:
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
dir := demos
include $(dir)/Rules.mk
