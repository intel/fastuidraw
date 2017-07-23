# We need to initialize the variables with :=
# so that as GNU make walks the directory structure
# it correctly observes the output of $(call filelist)
# to get paths correct.
CLEAN_FILES :=
SUPER_CLEAN_FILES :=

FASTUIDRAW_SOURCES :=
FASTUIDRAW_RESOURCE_STRING :=
FASTUIDRAW_PRIVATE_SOURCES :=
