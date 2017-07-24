# if 1, build/install GL libs on install
BUILD_GL ?= 1
ENVIRONMENTALDESCRIPTIONS += "BUILD_GL: if set to 1 build GL backend to FastUIDraw (default 1)"

# if 1, build/install GLES libs on install
BUILD_GLES ?= 0
ENVIRONMENTALDESCRIPTIONS += "BUILD_GLES: if set to 1 build GLES backend to FastUIDraw (default 0)"

#########################################
## System specifics related to GL headers

###################################################
## Location in file system of GL (or GLES3) headers
ifeq ($(MINGW_BUILD),0)
  GL_DEFAULT_INCLUDEPATH = /usr/include
else ifeq ($(MINGW_MODE),MINGW64)
  GL_DEFAULT_INCLUDEPATH = /mingw64/include
else ifeq ($(MINGW_MODE),MINGW32)
  GL_DEFAULT_INCLUDEPATH = /mingw32/include
else
  GL_DEFAULT_INCLUDEPATH = /mingw/include
endif

GL_INCLUDEPATH ?= $(GL_DEFAULT_INCLUDEPATH)
ENVIRONMENTALDESCRIPTIONS += "GL_INCLUDEPATH: directory where GL/GLES headers are located (default $(GL_DEFAULT_INCLUDEPATH))"

############################
## headers for GL and GLES
GL_DEFAULT_RAW_HEADER_FILES = GL/glcorearb.h
GLES_DEFAULT_RAW_HEADER_FILES = GLES3/gl3platform.h GLES3/gl3.h GLES3/gl31.h GLES3/gl32.h GLES2/gl2ext.h

GL_RAW_HEADER_FILES ?= $(GL_DEFAULT_RAW_HEADER_FILES)
ENVIRONMENTALDESCRIPTIONS += "GL_RAW_HEADER_FILES: GL header files (default $(GL_DEFAULT_RAW_HEADER_FILES))"

GLES_RAW_HEADER_FILES ?= $(GLES_DEFAULT_RAW_HEADER_FILES)
ENVIRONMENTALDESCRIPTIONS += "GLES_RAW_HEADER_FILES: GLES header files (default $(GLES_DEFAULT_RAW_HEADER_FILES))"
