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
ifeq ($(DARWIN_BUILD),1)
  GL_DEFAULT_INCLUDEPATH = /usr/local/include
else ifeq ($(MINGW_BUILD),0)
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

################################
## Set the values
GL_RAW_HEADER_FILES ?= $(GL_DEFAULT_RAW_HEADER_FILES)
ENVIRONMENTALDESCRIPTIONS += "GL_RAW_HEADER_FILES: GL header files (default $(GL_DEFAULT_RAW_HEADER_FILES))"

GLES_RAW_HEADER_FILES ?= $(GLES_DEFAULT_RAW_HEADER_FILES)
ENVIRONMENTALDESCRIPTIONS += "GLES_RAW_HEADER_FILES: GLES header files (default $(GLES_DEFAULT_RAW_HEADER_FILES))"

#####################################################
## Names for macine generated header and source files
NGL_GL_CPP = src/fastuidraw/gl_backend/ngl_gl.cpp
NGL_GL_HPP = inc/fastuidraw/gl_backend/ngl_gl.hpp

NGL_GLES_CPP = src/fastuidraw/gl_backend/ngl_gles3.cpp
NGL_GLES_HPP = inc/fastuidraw/gl_backend/ngl_gles3.hpp

##################################
## how and where to build the generator
NGL_BUILD = build/ngl_generator
NGL_GENERATOR_FLAGS=
