DEMO_COMMON_RESOURCE_STRING_SRCS = $(patsubst %.resource_string, string_resources_cpp/%.resource_string.cpp, $(COMMON_DEMO_RESOURCE_STRINGS))
CLEAN_FILES += $(DEMO_COMMON_RESOURCE_STRING_SRCS)

DEMO_COMMON_LIBS := $(shell pkg-config SDL2_image --libs)
DEMO_COMMON_CFLAGS = $(shell pkg-config SDL2_image --cflags) -Idemos/common -Idemos/tutorial/common


ifeq ($(MINGW_BUILD),1)
  TEMP := $(DEMO_COMMON_LIBS)
  DEMO_COMMON_LIBS := $(subst -mwindows, ,$(TEMP))
endif

ifeq ($(DEMOS_HAVE_FONT_CONFIG),1)
  DEMO_COMMON_CFLAGS += $(shell pkg-config fontconfig --cflags) -DHAVE_FONT_CONFIG
  DEMO_COMMON_LIBS += $(shell pkg-config fontconfig --libs)
endif

DEMO_release_CFLAGS = -O3 -fstrict-aliasing $(DEMO_COMMON_CFLAGS)
DEMO_debug_CFLAGS = -g $(DEMO_COMMON_CFLAGS)

# $1 --> gl or gles
# $2 --> debug or release
define demobuildrules
$(eval DEMO_$(2)_CFLAGS_$(1) = $$(DEMO_$(2)_CFLAGS) $$(shell ./fastuidraw-config.nodir --$(1) --$(2) --cflags --incdir=inc)
DEMO_$(2)_LIBS_$(1) = $$(shell ./fastuidraw-config.nodir --$(1) --$(2) --libs --libdir=.)
DEMO_$(2)_LIBS_STATIC_$(1) = $$(shell ./fastuidraw-config.nodir --$(1) --$(2) --static --libs --libdir=.)

build/demo/$(2)/$(1)/%.resource_string.o: build/string_resources_cpp/%.resource_string.cpp fastuidraw-config.nodir
	@mkdir -p $$(dir $$@)
	$(CXX) $$(DEMO_$(2)_CFLAGS_$(1)) -c $$< -o $$@

build/demo/$(2)/$(1)/%.o: %.cpp $$(NGL_$(1)_HPP) build/demo/$(2)/$(1)/%.d fastuidraw-config.nodir
	@mkdir -p $$(dir $$@)
	$(CXX) $$(DEMO_$(2)_CFLAGS_$(1)) -MT $$@ -MMD -MP -MF build/demo/$(2)/$(1)/$$*.d  -c $$< -o $$@

build/demo/$(2)/$(1)/%.d: ;
.PRECIOUS: build/demo/$(2)/$(1)/%.d
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
CLEAN_FILES += $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS)  $$(THISDEMO_$(1)_RESOURCE_STRING_SRCS)
CLEAN_FILES += $(1)-$(2)-$(3) $(1)-$(2)-$(3).exe
CLEAN_FILES += $(1)-$(2)-$(3)-static $(1)-$(2)-$(3)-static.exe
SUPER_CLEAN_FILES += $$(THISDEMO_$(1)_$(2)_$(3)_DEPS)
ifeq ($(4),1)
-include $$(THISDEMO_$(1)_$(2)_$(3)_DEPS)
demos-$(2)-$(3): $(1)-$(2)-$(3)
.PHONY: demos-$(2)-$(3)
$(1)-$(2): $(1)-$(2)-$(3)
.PHONY: $(1)-$(2)
$(1)-$(3): $(1)-$(2)-$(3)
.PHONY: $(1)-$(3)
$(1): $(1)-$(2)
.PHONY: $(1)
$(1)-$(2)-$(3): libFastUIDraw$(2)_$(3) $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS)
	$$(CXX) -o $$@ $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS) $$(DEMO_$(3)_LIBS_$(2)) $(DEMO_COMMON_LIBS)
DEMO_EXES += $(1)-$(2)-$(3)

demos-$(2)-$(3)-static: $(1)-$(2)-$(3)-static
.PHONY: demos-$(2)-$(3)-static
$(1)-$(2)-static: $(1)-$(2)-$(3)-static
.PHONY: $(1)-$(2)-static
$(1)-$(3)-static: $(1)-$(2)-$(3)-static
.PHONY: $(1)-$(3)-static
$(1)-static: $(1)-$(2)-static
.PHONY: $(1)-static
$(1)-$(2)-$(3)-static: libFastUIDraw_$(3).a libFastUIDraw$(2)_$(3).a libN$(2)_$(3).a $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS)
	$$(CXX) -o $$@ $$(THISDEMO_$(1)_$(2)_$(3)_ALL_OBJS) $(DEMO_COMMON_LIBS) $$(DEMO_$(3)_LIBS_STATIC_$(2))
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
endif
)
endef

define adddemotarget
$(eval DEMO_TARGETLIST+=$(1))
endef

$(foreach demoname,$(DEMOS),$(call adddemotarget,$(demoname)))


TARGETLIST+=demos demos-debug demos-release
TARGETLIST+=demos-static demos-debug-static demos-release-static

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
demos-$(1)-static: demos-$(1)-release-static demos-$(1)-debug-static
demos-release-static: demos-$(1)-release-static
demos-debug-static: demos-$(1)-debug-static
.PHONY: demos-$(1)-static
demos-static: demos-$(1)-static
.PHONY: demos-static
TARGETLIST += demos-$(1)-static
endif
)
endef


$(call demosapi,GL,$(BUILD_GL))
$(call demosapi,GLES,$(BUILD_GLES))
demos: demos-debug demos-release
all: demos
