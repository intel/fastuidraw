###########################################
# Setting for building libFastUIDraw.so base library
STRING_RESOURCE_CC = shell_scripts/fastuidraw-create-resource-cpp-file.sh

#######################################
# Platform specific libraries needed
ifeq ($(MINGW_BUILD),1)
  FASTUIDRAW_DEPS_LIBS += -lmingw32
  FASTUIDRAW_DEPS_STATIC_LIBS += -lmingw32
endif

#############################################
# Compile flags needed for hidden and shared
ifeq ($(MINGW_BUILD),1)
fPIC =
fHIDDEN =
fHIDDEN_INLINES =
else ifeq ($(DARWIN_BUILD),1)
fPIC = -fPIC
fHIDDEN =
fHIDDEN_INLINES =
else
fPIC = -fPIC
fHIDDEN_INLINES = -fvisibility-inlines-hidden
fHIDDEN = -fvisibility=hidden
endif

ifeq ($(DARWIN_BUILD),0)
SONAME = -soname
SED_INPLACE_REPLACE=sed -i
else
SONAME = -install_name
SED_INPLACE_REPLACE=sed -i ''
endif
