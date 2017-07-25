DEMO_COMMON_RESOURCE_STRING_SRCS = $(patsubst %.resource_string, string_resources_cpp/%.resource_string.cpp, $(COMMON_DEMO_RESOURCE_STRINGS))
CLEAN_FILES += $(DEMO_COMMON_RESOURCE_STRING_SRCS)

# This is awful. Makes me wish I used cmake.
DEMO_COMMON_LIBS := $(shell sdl2-config --libs) -lSDL2_image $(FASTUIDRAW_LIBS)
ifeq ($(MINGW_BUILD),1)
  TEMP := $(DEMO_COMMON_LIBS)
  DEMO_COMMON_LIBS := $(subst -mwindows, ,$(TEMP))
endif

DEMO_COMMON_LIBS_GL := $(DEMO_COMMON_LIBS)
DEMO_COMMON_LIBS_GLES := $(DEMO_COMMON_LIBS) -lEGL

DEMO_COMMON_CFLAGS = $(shell sdl2-config --cflags) -Idemos/common
DEMO_release_CFLAGS = -O3 -fstrict-aliasing $(DEMO_COMMON_CFLAGS)
DEMO_debug_CFLAGS = -g $(DEMO_COMMON_CFLAGS)


MAKEDEPEND = ./makedepend.sh

# $1 --> gl or gles
# $2 --> debug or release
define demobuildrules
$(eval DEMO_$(2)_CFLAGS_$(1) := $$(DEMO_$(2)_CFLAGS) -Iinc $$(FASTUIDRAW_$(1)_$(2)_CFLAGS) $$(FASTUIDRAW_$(2)_CFLAGS)
DEMO_$(2)_LIBS_$(1) := $$(DEMO_COMMON_LIBS_$(1)) -L. $$(FASTUIDRAW_$(1)_$(2)_LIBS) $$(FASTUIDRAW_$(2)_LIBS)
build/demo/$(2)/$(1)/%.resource_string.o: build/string_resources_cpp/%.resource_string.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $$(DEMO_$(2)_CFLAGS_$(1)) -c $$< -o $$@

build/demo/$(2)/$(1)/%.o: %.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $$(DEMO_$(2)_CFLAGS_$(1)) -c $$< -o $$@
build/demo/$(2)/$(1)/%.d: %.cpp
	@mkdir -p $$(dir $$@)
	@echo Generating $$@
	@$(MAKEDEPEND) "$(CXX)" "$$(DEMO_$(2)_CFLAGS_$(1))" build/demo/$(2)/$(1) "$$*" "$$<" "$$@"
)
endef

# how to build each demo:
# $1 --> Demo name
# $2 --> gl or gles
# $3 --> release or debug
# $4 --> 0: skip targets, otherwise dont skip targets
define demorule
$(eval THISDEMO_$(1)_RESOURCE_STRING_SRCS = $$(patsubst %.resource_string, build/string_resources_cpp/%.resource_string.cpp, $$($(1)_RESOURCE_STRING))
THISDEMO_$(1)_$(2)_$(3)_SOURCES = $$($(1)_SOURCES) $$(COMMON_DEMO_SOURCES)
THISDEMO_$(1)_$(2)_$(3)_RESOURCES = $$($(1)_RESOURCE_STRING) $$(COMMON_DEMO_RESOURCE_STRINGS)
THISDEMO_$(1)_$(2)_$(3)_RESOURCE_OBJS = $$(patsubst %.resource_string, build/demo/$(3)/$(2)/%.resource_string.o, $$(THISDEMO_$(1)_$(2)_$(3)_RESOURCES))
THISDEMO_$(1)_$(2)_$(3)_DEPS_RAW = $$(patsubst %.cpp, %.d, $$($(1)_SOURCES) $$(COMMON_DEMO_SOURCES))
THISDEMO_$(1)_$(2)_$(3)_OBJS_RAW = $$(patsubst %.cpp, %.o, $$(THISDEMO_$(1)_$(2)_$(3)_SOURCES))
THISDEMO_$(1)_$(2)_$(3)_DEPS = $$(addprefix build/demo/$(3)/$(2)/, $$(THISDEMO_$(1)_$(2)_$(3)_DEPS_RAW))
THISDEMO_$(1)_$(2)_$(3)_OBJS = $$(addprefix build/demo/$(3)/$(2)/, $$(THISDEMO_$(1)_$(2)_$(3)_OBJS_RAW))
THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS = $$(THISDEMO_$(1)_$(2)_$(3)_OBJS) $$(THISDEMO_$(1)_$(2)_$(3)_RESOURCE_OBJS)
THISDEMO_$(1)_$(2)_$(3)_EXE = $(1)-$(2)-$(3)
CLEAN_FILES += $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS) $$(THISDEMO_$(1)_$(2)_$(3)_EXE) $$(THISDEMO_$(1)_$(2)_$(3)_EXE).exe $$(THISDEMO_$(1)_RESOURCE_STRING_SRCS)
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
$$(THISDEMO_$(1)_$(2)_$(3)_EXE): libFastUIDraw$(2)_$(3) $$(THISDEMO_$(1)_RESOURCE_STRING_SRCS) $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS) $$(THISDEMO_$(1)_$(2)_$(3)_DEPS)
	$$(CXX) -o $$@ $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS) $$(DEMO_$(3)_LIBS_$(2))
endif
)
endef

# $1 --> gl or gles
# $2 --> release or debug
# $3 --> (0: skip build targets, 1: ad build targets)
define demoset
$(eval $(call demobuildrules,$(1),$(2))
$(foreach demoname,$(DEMOS),$(call demorule,$(demoname),$(1),$(2),$(3)))
ifeq ($(3),1)
.PHONY: demos-$(1)-$(2)
TARGETLIST += demos-$(1)-$(2)
endif
)
endef

# $1 --> gl or gles
# $2 --> (0: skip build targets, 1: add build targets)
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
all: demos