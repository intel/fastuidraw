# We need to initialize the variables with :=
# so that as GNU make walks the directory structure
# it correctly observes the output of $(call filelist)
# to get paths correct.
LIBRARY_GL_SOURCES :=
LIBRARY_GL_RESOURCE_STRING :=
LIBRARY_PRIVATE_GL_SOURCES :=
NGL_COMMON_SRCS :=
