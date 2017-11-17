# detect MinGW: requires tweaks to various builds
MINGW_BUILD = 0
UNAME = $(shell uname -s)
UNAMER= $(shell uname -r)
ifeq ($(findstring MINGW,$(UNAME)),MINGW)
  MINGW_BUILD = 1
  ifeq ($(findstring 1.0, $(UNAMER)), 1.0)
	MINGW_MODE = MINGW
  else ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
	MINGW_MODE = MINGW64
  else ifeq ($(findstring MINGW32,$(UNAME)),MINGW32)
	MINGW_MODE = MINGW32
  endif
endif

#TODO: detection for DARWIN
DARWIN_BUILD = 0

###########################################
# Setting for building libFastUIDraw.so base library
CXX ?= g++
CC ?= gcc
STRING_RESOURCE_CC = shell_scripts/fastuidraw-create-resource-cpp-file.sh
FASTUIDRAW_LIBS =


#######################################
# Platform specific libraries needed
ifeq ($(MINGW_BUILD),1)
  FASTUIDRAW_LIBS += -lmingw32
endif

#############################################
# Compile flags needed for hidden and shared
ifeq ($(MINGW_BUILD),1)
fPIC =
fHIDDEN =
fHIDDEN_INLINES =
else
fPIC = -fPIC
fHIDDEN_INLINES = -fvisibility-inlines-hidden
fHIDDEN = -fvisibility=hidden
endif

#########################
## for using clang-tidy
CLANG_TIDY ?= clang-tidy
CLANG_TIDY_ARGS = -header-filter=.* -checks=* -analyze-temporary-dtors
