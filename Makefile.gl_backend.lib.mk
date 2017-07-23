FASTUIDRAW_GL_COMMON_CFLAGS =
FASTUIDRAW_GLES_COMMON_CFLAGS = -DFASTUIDRAW_GL_USE_GLES

FASTUIDRAW_GL_GLES_debug_CFLAGS = -DGL_DEBUG
FASTUIDRAW_GL_GLES_release_CFLAGS =

FASTUIDRAW_GL_LIBS =

ifeq ($(MINGW_BUILD),1)
FASTUIDRAW_GLES_LIBS =
else
FASTUIDRAW_GLES_LIBS = -lGLESv2
endif

FASTUIDRAW_GL_STRING_RESOURCES_SRCS = $(patsubst %.resource_string, string_resources_cpp/%.resource_string.cpp, $(FASTUIDRAW_GL_RESOURCE_STRING) )
CLEAN_FILES += $(FASTUIDRAW_GL_STRING_RESOURCES_SRCS)

NGL_GL_SRCS = $(NGL_COMMON_SRCS) $(NGL_GL_CPP)
NGL_GLES_SRCS = $(NGL_COMMON_SRCS) $(NGL_GLES_CPP)

# $1 --> GL or GLES
# $2 --> debug or release
# $3 --> (0: skip build target 1: add build target)
define glrule
$(eval FASTUIDRAW_$(1)_$(2)_CFLAGS=$$(FASTUIDRAW_$(1)_COMMON_CFLAGS) $$(FASTUIDRAW_$(2)_CFLAGS) $$(FASTUIDRAW_GL_GLES_$(2)_CFLAGS)
COMPILE_$(1)_$(2)_CFLAGS=$$(FASTUIDRAW_BUILD_$(2)_FLAGS) $(FASTUIDRAW_BUILD_WARN_FLAGS) $(FASTUIDRAW_BUILD_INCLUDES_CFLAGS) $$(FASTUIDRAW_$(1)_$(2)_CFLAGS)
build/$(2)/$(1)/%.o: %.cpp build/$(2)/$(1)/%.d $$(NGL_$(1)_HPP)
	@mkdir -p $$(dir $$@)
	$(CXX) $$(COMPILE_$(1)_$(2)_CFLAGS)  $(fPIC) -c $$< -o $$@
build/$(2)/$(1)/%.d: %.cpp $$(NGL_$(1)_HPP)
	@mkdir -p $$(dir $$@)
	@echo Generating $$@
	@./makedepend.sh "$(CXX)" "$$(COMPILE_$(1)_$(2)_CFLAGS)" build/$(2)/$(1) "$$*" "$$<" "$$@"

build/$(2)/private/$(1)/%.o: %.cpp build/$(2)/private/$(1)/%.d $$(NGL_$(1)_HPP)
	@mkdir -p $$(dir $$@)
	$(CXX) $$(COMPILE_$(1)_$(2)_CFLAGS) $(fHIDDEN) $(fPIC) -c $$< -o $$@
build/$(2)/private/$(1)/%.d: %.cpp $$(NGL_$(1)_HPP)
	@mkdir -p $$(dir $$@)
	@echo Generating $$@
	@./makedepend.sh "$(CXX)" "$$(COMPILE_$(1)_$(2)_CFLAGS)" build/$(2)/$(1) "$$*" "$$<" "$$@"

FASTUIDRAW_$(1)_$(2)_OBJS = $$(patsubst %.cpp, build/$(2)/$(1)/%.o, $(FASTUIDRAW_GL_SOURCES))
FASTUIDRAW_$(1)_$(2)_PRIVATE_OBJS = $$(patsubst %.cpp, build/$(2)/private/$(1)/%.o, $(FASTUIDRAW_PRIVATE_GL_SOURCES))
NGL_$(1)_$(2)_OBJ = $$(patsubst %.cpp, build/$(2)/$(1)/%.o, $$(NGL_$(1)_SRCS))
FASTUIDRAW_$(1)_$(2)_DEPS = $$(patsubst %.cpp, build/$(2)/$(1)/%.d, $$(FASTUIDRAW_GL_SOURCES))
FASTUIDRAW_$(1)_$(2)_RESOURCE_OBJS = $$(patsubst %.cpp, build/$(2)/%.o, $$(FASTUIDRAW_GL_STRING_RESOURCES_SRCS))
FASTUIDRAW_$(1)_$(2)_ALL_OBJS = $$(FASTUIDRAW_$(1)_$(2)_OBJS) $$(FASTUIDRAW_$(1)_$(2)_PRIVATE_OBJS) $$(FASTUIDRAW_$(1)_$(2)_RESOURCE_OBJS)
FASTUIDRAW_$(1)_$(2)_LIBS = -lFastUIDraw$(1)_$(2) -lN$(1)_$(2) $$(FASTUIDRAW_$(2)_LIBS) $$(FASTUIDRAW_$(1)_LIBS)
CLEAN_FILES += $$(FASTUIDRAW_$(1)_$(2)_ALL_OBJS) $$(FASTUIDRAW_$(1)_$(2)_ALL_OBJS)
SUPER_CLEAN_FILES += $$(FASTUIDRAW_$(1)_$(2)_DEPS) $$(FASTUIDRAW_$(1)_$(2)_DEPS) $$(NGL_$(1)_$(2)_OBJ)
CLEAN_FILES += libFastUIDraw$(1)_$(2).dll libFastUIDraw$(1)_$(2).dll.a libN$(1)_$(2).dll libN$(1)_$(2).dll.a
CLEAN_FILES += libFastUIDraw$(1)_$(2).so libN$(1)_$(2).so
ifeq ($(3),1)
ifneq ($$(MAKECMDGOALS),clean)
ifneq ($$(MAKECMDGOALS),targets)
ifneq ($$(MAKECMDGOALS),clean-all)
ifneq ($$(MAKECMDGOALS),docs)
ifneq ($$(MAKECMDGOALS),clean-docs)
ifneq ($$(MAKECMDGOALS),install-docs)
ifneq ($$(MAKECMDGOALS),uninstall-docs)
-include $$(FASTUIDRAW_$(1)_$(2)_DEPS)
endif
endif
endif
endif
endif
endif
endif
ifeq ($(MINGW_BUILD),1)
libFastUIDraw$(1)_$(2): libFastUIDraw$(1)_$(2).dll
libFastUIDraw$(1)_$(2).dll.a: libFastUIDraw$(1)_$(2).dll
libFastUIDraw$(1)_$(2).dll: libFastUIDraw_$(2).dll libN$(1)_$(2).dll $$(FASTUIDRAW_$(1)_$(2)_ALL_OBJS)
	$(CXX) -shared -Wl,--out-implib,libFastUIDraw$(1)_$(2).dll.a -o libFastUIDraw$(1)_$(2).dll $$(FASTUIDRAW_$(1)_$(2)_ALL_OBJS) $$(FASTUIDRAW_$(2)_LIBS) -L. -lN$(1)_$(2) $$(FASTUIDRAW_$(1)_LIBS)
libN$(1)_$(2): libN$(1)_$(2).dll.a
libN$(1)_$(2).dll.a: libN$(1)_$(2).dll
libN$(1)_$(2).dll: $$(NGL_$(1)_$(2)_OBJ)
	$(CXX) -shared -Wl,--out-implib,libN$(1)_$(2).dll.a -o libN$(1)_$(2).dll $$(NGL_$(1)_$(2)_OBJ)
LIBFASTUIDRAW_$(1)_$(2) = libFastUIDraw$(1)_$(2).dll.a
LIBN$(1)_$(2) = libN$(1)_$(2).dll.a
INSTALL_LIBS += libFastUIDraw$(1)_$(2).dll.a libN$(1)_$(2).dll.a
INSTALL_EXES += libFastUIDraw$(1)_$(2).dll libN$(1)_$(2).dll
else
libFastUIDraw$(1)_$(2): libFastUIDraw$(1)_$(2).so
libFastUIDraw$(1)_$(2).so: libFastUIDraw_$(2).so libN$(1)_$(2).so $$(FASTUIDRAW_$(1)_$(2)_ALL_OBJS)
	$(CXX) -shared -Wl,-soname,libFastUIDraw$(1)_$(2).so -o libFastUIDraw$(1)_$(2).so $$(FASTUIDRAW_$(1)_$(2)_ALL_OBJS) $$(FASTUIDRAW_$(2)_LIBS) -L. -lN$(1)_$(2) $$(FASTUIDRAW_$(1)_LIBS)
libN$(1)_$(2): libN$(1)_$(2).so
libN$(1)_$(2).so: $$(NGL_$(1)_$(2)_OBJ)
	$(CXX) -shared -Wl,-soname,libN$(1)_$(2).so -o libN$(1)_$(2).so $$(NGL_$(1)_$(2)_OBJ)
LIBFASTUIDRAW_$(1)_$(2) = libFastUIDraw$(1)_$(2).so
LIBN$(1)_$(2) = libN$(1)_$(2).so
INSTALL_LIBS += libFastUIDraw$(1)_$(2).so libN$(1)_$(2).so
endif

endif
)
endef

# $1 --> GL or GLES
# $2 --> (0: skip build target 1: add build target)
define glrules
$(eval $(call glrule,$(1),release,$(2))
$(call glrule,$(1),debug,$(2))
ifeq ($(2),1)
TARGETLIST += libFastUIDraw$(1) libFastUIDraw$(1)_debug libFastUIDraw$(1)_release
libFastUIDraw$(1): libFastUIDraw$(1)_debug libFastUIDraw$(1)_release
.PHONY: libFastUIDraw$(1)
.PHONY: libFastUIDraw$(1)_release
.PHONY: libFastUIDraw$(1)_debug
TARGETLIST += libN$(1) libN$(1)_debug libN$(1)_release
libN$(1): libN$(1)_debug libN$(1)_release
.PHONY: libN$(1)
.PHONY: libN$(1)_debug
.PHONY: libN$(1)_release
endif
)
endef

$(call glrules,GL,$(BUILD_GL))
$(call glrules,GLES,$(BUILD_GLES))
