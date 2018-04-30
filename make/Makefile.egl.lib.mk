define egllibrules
$(eval NEGL_OBJS_$(1) = $$(patsubst %.cpp, build/NEGL/$(1)/%.o, $(NEGL_SRCS))
NEGL_DEPS_$(1) = $$(patsubst %.cpp, build/NEGL/$(1)/%.d, $(NEGL_SRCS))
CLEAN_FILES += $$(NEGL_OBJS_$(1))
SUPER_CLEAN_FILES += $$(NEGL_DEPS_$(1))

build/NEGL/$(1)/%.o: %.cpp build/NEGL/$(1)/%.d $(NGL_EGL_HPP)
	@mkdir -p $$(dir $$@)
	$(CXX) $$(COMPILE_$(1)_CFLAGS) $(fPIC) -MT $$@ -MMD -MP -MF build/NEGL/$(1)/$$*.d -c $$< -o $$@
build/NEGL/$(1)/%.d: ;
.PRECIOUS: build/NEGL/$(1)/%.d

-include $$(NEGL_DEPS_$(1))

libNEGL_$(1): libNEGL_$(1).so
libNEGL_$(1).so: $$(NEGL_OBJS_$(1)) libFastUIDraw_$(1).so
	$(CXX) -shared -Wl,-soname,libNEGL_$(1).so -o libNEGL_$(1).so $$(NEGL_OBJS_$(1)) -lEGL -L. -lFastUIDraw_$(1) $(FASTUIDRAW_DEPS_LIBS)
CLEAN_FILES += libNEGL_$(1).so
INSTALL_LIBS += libNEGL_$(1).so
libNEGL: libNEGL_$(1)
.PHONY: libNEGL_$(1) libNEGL

libNEGL_$(1):libNEGL_$(1).a
libNEGL_$(1).a: $$(NEGL_OBJS_$(1))
	ar rcs $$@ $$(NEGL_OBJS_$(1))
CLEAN_FILES += libNEGL_$(1).a

TARGETLIST += libNEGL_$(1)-static
libNEGL_$(1)-static: libNEGL_$(1).a
.PHONY: libNEGL_$(1)-static
libNEGL-static: libNEGL_$(1).a
.PHONY: libNEGL
)
endef

ifeq ($(BUILD_NEGL),1)
$(call egllibrules,release)
$(call egllibrules,debug)
TARGETLIST += libNEGL-static
endif
