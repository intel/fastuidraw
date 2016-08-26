# We need to initialize the variables with :=
# so that as GNU make walks the directory structure
# it correctly observes the output of $(call filelist)
# to get paths correct.
CLEAN_FILES :=
SUPER_CLEAN_FILES :=

LIBRARY_SOURCES :=
LIBRARY_RESOURCE_STRING :=
LIBRARY_PRIVATE_SOURCES :=
