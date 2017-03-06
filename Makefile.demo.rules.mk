DEMO_COMMON_RESOURCE_STRING_SRCS = $(patsubst %.resource_string, string_resources_cpp/%.resource_string.cpp, $(COMMON_DEMO_RESOURCE_STRINGS))

# This is awful. Makes me wish I used cmake.
DEMO_COMMON_LIBS := $(shell sdl2-config --libs) -lSDL2_image $(LIBRARY_LIBS)
ifeq ($(MINGW_BUILD),1)
  TEMP := $(DEMO_COMMON_LIBS)
  DEMO_COMMON_LIBS := $(subst -mwindows, ,$(TEMP))
endif
DEMO_GL_LIBS := $(DEMO_COMMON_LIBS)
DEMO_GLES_LIBS := $(DEMO_COMMON_LIBS) -lEGL

DEMO_release_CFLAGS_GL = $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $(LIBRARY_GL_release_CFLAGS) $(shell sdl2-config --cflags) -Idemos/common
DEMO_debug_CFLAGS_GL = -g $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $(LIBRARY_GL_debug_CFLAGS) $(shell sdl2-config --cflags) -Idemos/common
DEMO_release_CFLAGS_GLES = $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $(LIBRARY_GLES_release_CFLAGS) $(shell sdl2-config --cflags) -Idemos/common
DEMO_debug_CFLAGS_GLES = -g $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $(LIBRARY_GLES_debug_CFLAGS) $(shell sdl2-config --cflags) -Idemos/common

MAKEDEPEND = ./makedepend.sh

# how to build each demo:
# $1 --> Demo name
# $2 --> GL or GLES
# $3 --> release or debug
# $4 --> 0: skip targets, otherwise dont skip targets
define demorule
$(eval $(3)/$(2)/demos/%.o: demos/%.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $$(DEMO_$(3)_CFLAGS_$(2)) -c $$< -o $$@
$(3)/$(2)/demos/%.dd: demos/%.cpp
	@mkdir -p $$(dir $$@)
	@echo Generating $$@
	@$(MAKEDEPEND) "$$(CXX)" "$$(DEMO_$(3)_CFLAGS_$(2))" $(3)/$(2)/demos "$$*" "$$<" "$$@"
THISDEMO_$(1)_RESOURCE_STRING_SRCS = $$(patsubst %.resource_string, string_resources_cpp/%.resource_string.cpp, $$($(1)_RESOURCE_STRING))
THISDEMO_$(1)_$(2)_$(3)_SOURCES = $$($(1)_SOURCES) $$(THISDEMO_$(1)_RESOURCE_STRING_SRCS) $$(COMMON_DEMO_SOURCES) $$(DEMO_COMMON_RESOURCE_STRING_SRCS)
THISDEMO_$(1)_$(2)_$(3)_DEPS_RAW = $$(patsubst %.cpp, %.dd, $$($(1)_SOURCES) $$(COMMON_DEMO_SOURCES))
THISDEMO_$(1)_$(2)_$(3)_OBJS_RAW = $$(patsubst %.cpp, %.o, $$(THISDEMO_$(1)_$(2)_$(3)_SOURCES))
THISDEMO_$(1)_$(2)_$(3)_DEPS = $$(addprefix $(3)/$(2)/, $$(THISDEMO_$(1)_$(2)_$(3)_DEPS_RAW))
THISDEMO_$(1)_$(2)_$(3)_OBJS = $$(addprefix $(3)/$(2)/, $$(THISDEMO_$(1)_$(2)_$(3)_OBJS_RAW))
THISDEMO_$(1)_$(2)_$(3)_EXE = $(1)-$(2)-$(3)
CLEAN_FILES += $$(THISDEMO_$(1)_$(2)_$(3)_OBJS) $$(THISDEMO_$(1)_$(2)_$(3)_EXE) $$(THISDEMO_$(1)_$(2)_$(3)_EXE).exe
SUPER_CLEAN_FILES += $$(THISDEMO_$(1)_$(2)_$(3)_DEPS)
ifeq ($(4),1)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clean-all)
ifneq ($(MAKECMDGOALS),targets)
ifneq ($(MAKECMDGOALS),docs)
ifneq ($(MAKECMDGOALS),clean-docs)
ifneq ($(MAKECMDGOALS),install-docs)
ifneq ($(MAKECMDGOALS),uninstall-docs)
-include $$(THISDEMO_$(1)_$(2)_$(3)_DEPS)
endif
endif
endif
endif
endif
endif
endif
demos-$(2)-$(3): $$(THISDEMO_$(1)_$(2)_$(3)_EXE)
DEMO_TARGETLIST += $$(THISDEMO_$(1)_$(2)_$(3)_EXE)
$$(THISDEMO_$(1)_$(2)_$(3)_EXE): libFastUIDraw$(2)_$(3) $$(THISDEMO_$(1)_$(2)_$(3)_OBJS) $$(THISDEMO_$(1)_$(2)_$(3)_DEPS)
	$$(CXX) -o $$@ $$(THISDEMO_$(1)_$(2)_$(3)_OBJS) $$(DEMO_$(2)_LIBS) -L. $$(FASTUIDRAW_$(2)_$(3)_LIBS)
endif
)
endef

# $1 --> GL or GLES
# $2 --> release or debug
# $3 --> (0: skip build targets, 1: ad build targets)
define demoset
$(eval $(foreach demoname,$(DEMOS),$(call demorule,$(demoname),$(1),$(2),$(3)))
ifeq ($(3),1)
.PHONY: demos-$(1)-$(2)
TARGETLIST += demos-$(1)-$(2)
endif
)
endef

# $1 --> GL or GLES
# $2 --> (0: skip build targets, 1: ad build targets)
define demosapi
$(eval $(call demoset,$(1),release,$(2))
$(call demoset,$(1),debug,$(2))
ifeq ($(2),1)
demos-$(1): demos-$(1)-release demos-$(1)-debug
demos-release: demos-$(1)-release
demos-debug: demos-$(1)-debug
.PHONY: demos-$(1)
TARGETLIST += demos-$(1)
endif
)
endef


$(call demosapi,GL,$(BUILD_GL))
$(call demosapi,GLES,$(BUILD_GLES))
demos: demos-debug demos-release
TARGETLIST+=demos demos-debug demos-release
