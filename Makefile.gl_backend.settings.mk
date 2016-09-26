#########################################
## System specifics related to GL headers

###################################################
## Location in file system of GL (or GLES3) headers
ifeq ($(MINGW_BUILD),0)
  GL_INCLUDEPATH=/usr/include
else ifeq ($(MINGW_MODE),MINGW64)
  GL_INCLUDEPATH=/mingw64/include
else ifeq ($(MINGW_MODE),MINGW32)
  GL_INCLUDEPATH=/mingw32/include
else
  GL_INCLUDEPATH=/mingw/include
endif

############################
## headers for GL and GLES
GL_RAW_HEADER_FILES=GL/glcorearb.h
GLES_RAW_HEADER_FILES=GLES3/gl3platform.h GLES3/gl3.h GLES3/gl31.h GLES3/gl32.h GLES2/gl2ext.h
