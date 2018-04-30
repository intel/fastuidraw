
INSTALL_LOCATION_VALUE=$(shell echo $(INSTALL_LOCATION))

OTHER_API_GL = GLES
OTHER_API_GLES = GL
OTHER_TYPE_release = debug
OTHER_TYPE_debug = release

fastuidraw-config.nodir: fastuidraw-config.in
	@echo Generating $@
	@cp $< $@
	@sed -i 's!@FASTUIDRAW_DEPS_LIBS@!$(FASTUIDRAW_DEPS_LIBS)!g' $@
	@sed -i 's!@FASTUIDRAW_DEPS_STATIC_LIBS@!$(FASTUIDRAW_DEPS_STATIC_LIBS)!g' $@
	@sed -i 's!@FASTUIDRAW_release_CFLAGS@!$(FASTUIDRAW_release_CFLAGS)!g' $@
	@sed -i 's!@FASTUIDRAW_debug_CFLAGS@!$(FASTUIDRAW_debug_CFLAGS)!g' $@
	@sed -i 's!@FASTUIDRAW_GLES_CFLAGS@!$(FASTUIDRAW_GLES_CFLAGS)!g' $@
	@sed -i 's!@FASTUIDRAW_GL_CFLAGS@!$(FASTUIDRAW_GL_CFLAGS)!g' $@
	@chmod a+x $@
CLEAN_FILES+=fastuidraw-config.nodir

fastuidraw-config: fastuidraw-config.nodir
	@echo Generating $@
	@cp $< $@
	@sed -i 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION_VALUE)!g' $@
	@chmod a+x $@
# added to .PHONY to force regeneration so that if an environmental
# variable (BUILD_GL, BUILD_GLES, INSTALL_LOCATION) changes, we can
# guarantee that fastuidraw-config reflects it correctly.
.PHONY: fastuidraw-config
.SECONDARY: fastuidraw-config
CLEAN_FILES+=fastuidraw-config
INSTALL_EXES+=fastuidraw-config shell_scripts/fastuidraw-create-resource-cpp-file.sh
TARGETLIST+=fastuidraw-config

# $1: release or debug
# $2: GL or GLES
# $3: (0: skip build target 1: add build target)
define pkgconfrulesapi
$(eval ifeq ($(3),1)
n$(2)-$(1).pc: n.pc.in fastuidraw-$(1).pc
	@echo Generating $$@
	@cp $$< $$@
	@sed -i 's!@TYPE@!$(1)!g' $$@
	@sed -i 's!@API@!$(2)!g' $$@
	@sed -i 's!@OTHER_API@!$$(OTHER_API_$(2))!g' $$@
	@sed -i 's!@OTHER_TYPE@!$$(OTHER_TYPE_$(1))!g' $$@
	@sed -i 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION_VALUE)!g' $$@
	@sed -i 's!@N_ADDITIONAL_LIBS@!!g' $$@

fastuidraw$(2)-$(1).pc: fastuidraw-backend.pc.in n$(2)-$(1).pc
	@echo Generating $$@
	@cp $$< $$@
	@sed -i 's!@TYPE@!$(1)!g' $$@
	@sed -i 's!@API@!$(2)!g' $$@
	@sed -i 's!@OTHER_API@!$$(OTHER_API_$(2))!g' $$@
	@sed -i 's!@OTHER_TYPE@!$$(OTHER_TYPE_$(1))!g' $$@
	@sed -i 's!@FASTUIDRAW_BACKEND_CFLAGS@!$$(FASTUIDRAW_$(2)_CFLAGS)!g' $$@
	@sed -i 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION_VALUE)!g' $$@

.PHONY:fastuidraw$(2)-$(1).pc n$(2)-$(1).pc
.SECONDARY: fastuidraw$(2)-$(1).pc n$(2)-$(1).pc
pkg-config-files: fastuidraw$(2)-$(1).pc n$(2)-$(1).pc
INSTALL_PKG_FILES+=fastuidraw$(2)-$(1).pc n$(2)-$(1).pc
endif
)
CLEAN_FILES+=fastuidraw$(2)-$(1).pc n$(2)-$(1).pc
endef

# $1: release or debug
define pkgconfrules
$(eval fastuidraw-$(1).pc: fastuidraw.pc.in
	@echo Generating $$@
	@cp $$< $$@
	@sed -i 's!@TYPE@!$(1)!g' $$@
	@sed -i 's!@OTHER_TYPE@!$$(OTHER_TYPE_$(1))!g' $$@
	@sed -i 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION_VALUE)!g' $$@
	@sed -i 's!@FASTUIDRAW_CFLAGS@!$$(FASTUIDRAW_$(1)_BASE_CFLAGS)!g' $$@
ifeq ($(BUILD_NEGL),1)
nEGL-$(1).pc: n.pc.in fastuidraw-$(1).pc
	@echo Generating $$@
	@cp $$< $$@
	@sed -i 's!@API@!EGL!g' $$@
	@sed -i 's!n@OTHER_API@-@TYPE@!!g' $$@
	@sed -i 's!n@OTHER_API@-@OTHER_TYPE@!!g' $$@
	@sed -i 's!@TYPE@!$(1)!g' $$@
	@sed -i 's!@OTHER_TYPE@!$$(OTHER_TYPE_$(1))!g' $$@
	@sed -i 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION_VALUE)!g' $$@
	@sed -i 's!@N_ADDITIONAL_LIBS@!-lEGL!g' $$@
INSTALL_PKG_FILES+=nEGL-$(1).pc
pkg-config-files: nEGL-$(1).pc
endif
.PHONY:fastuidraw-$(1).pc nEGL-$(1).pc
.SECONDARY: fastuidraw-$(1).pc nEGL-$(1).pc
CLEAN_FILES+=fastuidraw-$(1).pc nEGL-$(1).pc
INSTALL_PKG_FILES+=fastuidraw-$(1).pc
pkg-config-files: fastuidraw-$(1).pc
$(call pkgconfrulesapi,$(1),GL,$(BUILD_GL))
$(call pkgconfrulesapi,$(1),GLES,$(BUILD_GLES)))
endef

ifeq ($(INSTALL_STATIC),1)
INSTALL_LIBS += $(INSTALL_STATIC_LIBS)
endif

$(call pkgconfrules,release)
$(call pkgconfrules,debug)
TARGETLIST+=pkg-config-files
.PHONY:pkg-config-files

install: $(INSTALL_LIBS) $(INSTALL_EXES) $(INSTALL_PKG_FILES)
	-install -d $(INSTALL_LOCATION_VALUE)/lib
	-install -d $(INSTALL_LOCATION_VALUE)/lib/pkgconfig
	-install -d $(INSTALL_LOCATION_VALUE)/bin
	-install -d $(INSTALL_LOCATION_VALUE)/include
	-install -t $(INSTALL_LOCATION_VALUE)/lib $(INSTALL_LIBS)
	-install -m 644 -t $(INSTALL_LOCATION_VALUE)/lib/pkgconfig $(INSTALL_PKG_FILES)
	-install -t $(INSTALL_LOCATION_VALUE)/bin $(INSTALL_EXES)
	-find inc/ -type d -printf '%P\n' | xargs -I '{}' install -d $(INSTALL_LOCATION_VALUE)/include/'{}'
	-find inc/ -type f -printf '%P\n' | xargs -I '{}' install -m 644 inc/'{}' $(INSTALL_LOCATION_VALUE)/include/'{}'
TARGETLIST+=install

install-docs: docs
	-install -d $(INSTALL_LOCATION_VALUE)/share/doc/fastuidraw/html/
	-install -t $(INSTALL_LOCATION_VALUE)/share/doc/fastuidraw TODO.txt README.md COPYING ISSUES.txt docs/*.txt
	-find docs/doxy/html -type d -printf '%P\n' | xargs -I '{}' install -d $(INSTALL_LOCATION_VALUE)/share/doc/fastuidraw/html/'{}'
	-find docs/doxy/html -type f -printf '%P\n' | xargs -I '{}' install -m 644 docs/doxy/html/'{}' $(INSTALL_LOCATION_VALUE)/share/doc/fastuidraw/html/'{}'
TARGETLIST+=install-docs

install-demos: $(DEMO_TARGETLIST)
	-install -d $(INSTALL_LOCATION_VALUE)/bin
	-install -t $(INSTALL_LOCATION_VALUE)/bin $(DEMO_TARGETLIST)
TARGETLIST+=install-demos

uninstall:
	-rm -rf $(INSTALL_LOCATION_VALUE)/include/fastuidraw
	-rm -f $(addprefix $(INSTALL_LOCATION_VALUE)/lib/,$(notdir $(INSTALL_LIBS)))
	-rm -f $(addprefix $(INSTALL_LOCATION_VALUE)/lib/pkgconfig/,$(notdir $(INSTALL_PKG_FILES)))
	-rm -f $(addprefix $(INSTALL_LOCATION_VALUE)/bin/,$(notdir $(INSTALL_EXES)))
TARGETLIST+=uninstall

uninstall-docs:
	-rm -r $(INSTALL_LOCATION_VALUE)/share/doc/fastuidraw
TARGETLIST+=uninstall-docs

uninstall-demos:
	-rm -f $(addprefix $(INSTALL_LOCATION_VALUE)/bin/,$(notdir $(DEMO_TARGETLIST)))
TARGETLIST+=uninstall-demos
