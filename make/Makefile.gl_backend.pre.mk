# We need to initialize the variables with :=
# so that as GNU make walks the directory structure
# it correctly observes the output of $(call filelist)
# to get paths correct.
FASTUIDRAW_GL_SOURCES :=
FASTUIDRAW_GL_RESOURCE_STRING :=
FASTUIDRAW_PRIVATE_GL_SOURCES :=
NGL_COMMON_SRCS :=
