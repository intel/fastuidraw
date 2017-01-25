
LIBRARY_CFLAGS = $(shell freetype-config --cflags) -D_USE_MATH_DEFINES
LIBRARY_debug_CFLAGS =  $(LIBRARY_CFLAGS) -DFASTUIDRAW_VECTOR_BOUND_CHECK -DFASTUIDRAW_DEBUG
LIBRARY_release_CFLAGS = $(LIBRARY_CFLAGS) -DNDEBUG

LIBRARY_BUILD_debug_FLAGS = -g -std=c++11
LIBRARY_BUILD_release_FLAGS = -O3 -fstrict-aliasing -std=c++11
LIBRARY_BUILD_WARN_FLAGS = -Wall -Wextra -Wcast-qual -Wwrite-strings
LIBRARY_BUILD_INCLUDES_CFLAGS = -Iinc

LIBRARY_STRING_RESOURCES_SRCS = $(patsubst %.resource_string, string_resources_cpp/%.resource_string.cpp, $(LIBRARY_RESOURCE_STRING) )
CLEAN_FILES += $(LIBRARY_STRING_RESOURCES_SRCS)

#
#  STRING_RESOURCE_CC inputfile resourcename outputpath
#    inputfile has full path on it
#    resourcename name to access string resource
#    outputpath is path to place outputpath
string_resources_cpp/%.resource_string.cpp: %.resource_string
	@mkdir -p $(dir $@)
	$(STRING_RESOURCE_CC) $< $(notdir $<) $(dir $@)
.SECONDARY: string_resources_cpp/%.resource_string.cpp
.PRECIOUS: string_resources_cpp/%.resource_string.cpp

# $1 --> release or debug
define librules
$(eval LIBRARY_$(1)_OBJS = $$(patsubst %.cpp, $(1)/%.o, $(LIBRARY_SOURCES))
LIBRARY_$(1)_DEPS = $$(patsubst %.cpp, $(1)/%.d, $(LIBRARY_SOURCES))
LIBRARY_$(1)_DEPS += $$(patsubst %.cpp, $(1)/private/%.d, $(LIBRARY_PRIVATE_SOURCES))
LIBRARY_PRIVATE_$(1)_OBJS = $$(patsubst %.cpp, $(1)/private/%.o, $(LIBRARY_PRIVATE_SOURCES))
LIBRARY_$(1)_RESOURCE_OBJS = $$(patsubst %.cpp, $(1)/%.o, $(LIBRARY_STRING_RESOURCES_SRCS))
LIBRARY_$(1)_ALL_OBJS = $$(LIBRARY_$(1)_OBJS) $$(LIBRARY_PRIVATE_$(1)_OBJS) $$(LIBRARY_$(1)_RESOURCE_OBJS)
CLEAN_FILES += $$(LIBRARY_$(1)_ALL_OBJS) $$(LIBRARY_$(1)_RESOURCE_OBJS)
FASTUIDRAW_$(1)_LIBS = -lFastUIDraw_$(1) $(LIBRARY_LIBS)
SUPER_CLEAN_FILES += $$(LIBRARY_$(1)_DEPS)
$(1)/%.resource_string.o: string_resources_cpp/%.resource_string.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $$(LIBRARY_BUILD_$(1)_FLAGS) $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $$(LIBRARY_$(1)_CFLAGS) $(fPIC) -c $$< -o $$@

$(1)/%.o: %.cpp $(1)/%.d
	@mkdir -p $$(dir $$@)
	$(CXX) $$(LIBRARY_BUILD_$(1)_FLAGS) $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $$(LIBRARY_$(1)_CFLAGS) $(fPIC) -c $$< -o $$@

$(1)/private/%.o: %.cpp $(1)/private/%.d
	@mkdir -p $$(dir $$@)
	$(CXX) $$(LIBRARY_BUILD_$(1)_FLAGS) $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $$(LIBRARY_$(1)_CFLAGS) $(fPIC) $(fHIDDEN) -c $$< -o $$@

$(1)/%.d: %.cpp
	@mkdir -p $$(dir $$@)
	@echo Generating $$@
	@./makedepend.sh "$(CXX)" "$$(LIBRARY_BUILD_$(1)_FLAGS) $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $$(LIBRARY_$(1)_CFLAGS)" $(1) "$$*" "$$<" "$$@"

$(1)/private/%.d: %.cpp
	@mkdir -p $$(dir $$@)
	@echo Generating $$@
	@./makedepend.sh "$(CXX)" "$$(LIBRARY_BUILD_$(1)_FLAGS) $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $$(LIBRARY_$(1)_CFLAGS)" $(1) "$$*" "$$<" "$$@"
.SECONDARY: $(1)/%.d
.PRECIOUS: $(1)/%.d

ifeq ($(MINGW_BUILD),1)

libFastUIDraw_$(1): libFastUIDraw_$(1).dll
libFastUIDraw_$(1).dll.a: libFastUIDraw_$(1).dll
libFastUIDraw_$(1).dll: $$(LIBRARY_$(1)_ALL_OBJS)
	$(CXX) -shared -Wl,--out-implib,libFastUIDraw_$(1).dll.a -o libFastUIDraw_$(1).dll $$(LIBRARY_$(1)_ALL_OBJS) $(LIBRARY_LIBS)
CLEAN_FILES += libFastUIDraw_$(1).dll libFastUIDraw_$(1).dll.a
INSTALL_LIBS += libFastUIDraw_$(1).dll.a
INSTALL_EXES += libFastUIDraw_$(1).dll
else

libFastUIDraw_$(1): libFastUIDraw_$(1).so
libFastUIDraw_$(1).so: $$(LIBRARY_$(1)_ALL_OBJS)
	$(CXX) -shared -Wl,-soname,libFastUIDraw_$(1).so -o libFastUIDraw_$(1).so $$(LIBRARY_$(1)_ALL_OBJS) $(LIBRARY_LIBS)
CLEAN_FILES += libFastUIDraw_$(1).so
INSTALL_LIBS += libFastUIDraw_$(1).so
endif


libFastUIDraw_$(1).clang_tidy: $$(LIBRARY_$(1)_ALL_OBJS)
	$(CLANG_TIDY) $(CLANG_TIDY_ARGS) $(LIBRARY_SOURCES) $(LIBRARY_PRIVATE_SOURCES) -- $$(LIBRARY_BUILD_$(1)_FLAGS) $(LIBRARY_BUILD_WARN_FLAGS) $(LIBRARY_BUILD_INCLUDES_CFLAGS) $$(LIBRARY_$(1)_CFLAGS) $(fPIC) > $$@
CLEAN_FILES += libFastUIDraw_$(1).clang_tidy
TARGETLIST += libFastUIDraw_$(1).clang_tidy

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clean-all)
ifneq ($(MAKECMDGOALS),targets)
ifneq ($(MAKECMDGOALS),docs)
ifneq ($(MAKECMDGOALS),clean-docs)
ifneq ($(MAKECMDGOALS),install-docs)
ifneq ($(MAKECMDGOALS),uninstall-docs)
-include $$(LIBRARY_$(1)_DEPS)
endif
endif
endif
endif
endif
endif
endif
)
endef

$(call librules,release)
$(call librules,debug)

libFastUIDraw: libFastUIDraw_debug libFastUIDraw_release
.PHONY: libFastUIDraw libFastUIDraw_debug libFastUIDraw_release
TARGETLIST += libFastUIDraw libFastUIDraw_debug libFastUIDraw_release
