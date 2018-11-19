# if 1, build/install GL libs on install
BUILD_GL ?= 1
ENVIRONMENTALDESCRIPTIONS += "BUILD_GL: if set to 1 build GL backend to FastUIDraw (default 1)"

# if 1, build/install GLES libs on install
BUILD_GLES ?= 0
ENVIRONMENTALDESCRIPTIONS += "BUILD_GLES: if set to 1 build GLES backend to FastUIDraw (default 0)"

ifeq ($(MINGW_BUILD),0)
  BUILD_NEGL_DEFAULT = 1
else
  BUILD_NEGL_DEFAULT = 0
endif

BUILD_NEGL ?= $(BUILD_NEGL_DEFAULT)
ENVIRONMENTALDESCRIPTIONS += "BUILD_NEGL: if set to 1 build NEGL (default $(BUILD_NEGL_DEFAULT))"

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

EGL_INCLUDEPATH ?= $(GL_DEFAULT_INCLUDEPATH)
ENVIRONMENTALDESCRIPTIONS += "EGL_INCLUDEPATH: directory where EGL headers are located (default $(GL_DEFAULT_INCLUDEPATH))"


############################
## headers for GL and GLES
GL_DEFAULT_RAW_HEADER_FILES = GL/glcorearb.h
GLES_DEFAULT_RAW_HEADER_FILES = GLES3/gl3platform.h GLES3/gl3.h GLES3/gl31.h GLES3/gl32.h GLES2/gl2ext.h
EGL_DEFAULT_RAW_HEADER_FILES = EGL/egl.h EGL/eglext.h


################################
## Set the values
GL_RAW_HEADER_FILES ?= $(GL_DEFAULT_RAW_HEADER_FILES)
ENVIRONMENTALDESCRIPTIONS += "GL_RAW_HEADER_FILES: GL header files (default $(GL_DEFAULT_RAW_HEADER_FILES))"

GLES_RAW_HEADER_FILES ?= $(GLES_DEFAULT_RAW_HEADER_FILES)
ENVIRONMENTALDESCRIPTIONS += "GLES_RAW_HEADER_FILES: GLES header files (default $(GLES_DEFAULT_RAW_HEADER_FILES))"

EGL_RAW_HEADER_FILES ?= $(EGL_DEFAULT_RAW_HEADER_FILES)
ENVIRONMENTALDESCRIPTIONS += "EGL_RAW_HEADER_FILES: EGL header files (default $(EGL_DEFAULT_RAW_HEADER_FILES))"
