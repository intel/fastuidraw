
fastuidraw-config: fastuidraw-config.in
	@echo Generating $@
	@cp $< $@
	@sed -i 's!@FASTUIDRAW_release_LIBS@!$(FASTUIDRAW_release_LIBS)!' $@
	@sed -i 's!@FASTUIDRAW_GLES_release_LIBS@!$(FASTUIDRAW_GLES_release_LIBS)!' $@
	@sed -i 's!@FASTUIDRAW_GL_release_LIBS@!$(FASTUIDRAW_GL_release_LIBS)!' $@
	@sed -i 's!@FASTUIDRAW_debug_LIBS@!$(FASTUIDRAW_debug_LIBS)!' $@
	@sed -i 's!@FASTUIDRAW_GLES_debug_LIBS@!$(FASTUIDRAW_GLES_debug_LIBS)!' $@
	@sed -i 's!@FASTUIDRAW_GL_debug_LIBS@!$(FASTUIDRAW_GL_debug_LIBS)!' $@
	@sed -i 's!@LIBRARY_release_CFLAGS@!$(LIBRARY_release_CFLAGS)!' $@
	@sed -i 's!@LIBRARY_GLES_release_CFLAGS@!$(LIBRARY_GLES_release_CFLAGS)!' $@
	@sed -i 's!@LIBRARY_GL_release_CFLAGS@!$(LIBRARY_GL_release_CFLAGS)!' $@
	@sed -i 's!@LIBRARY_debug_CFLAGS@!$(LIBRARY_debug_CFLAGS)!' $@
	@sed -i 's!@LIBRARY_GLES_debug_CFLAGS@!$(LIBRARY_GLES_debug_CFLAGS)!' $@
	@sed -i 's!@LIBRARY_GL_debug_CFLAGS@!$(LIBRARY_GL_debug_CFLAGS)!' $@
	@sed -i 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION)!' $@
	@sed -i 's!@BUILD_GLES!$(BUILD_GLES)!' $@
	@sed -i 's!@BUILD_GL!$(BUILD_GL)!' $@
	@chmod a+x $@
# added to .PHONY to force regeneration so that if an environmental
# variable (BUILD_GL, BUILD_GLES, INSTALL_LOCATION) changes, we can
# guarantee that fastuidraw-config reflects it correctly.
.PHONY: fastuidraw-config
CLEAN_FILES+=fastuidraw-config
INSTALL_EXES+=fastuidraw-config shell_scripts/fastuidraw-create-resource-cpp-file.sh

install: $(INSTALL_LIBS) $(INSTALL_EXES)
	-install -d $(INSTALL_LOCATION)/lib
	-install -d $(INSTALL_LOCATION)/bin
	-install -d $(INSTALL_LOCATION)/include
	-install -t $(INSTALL_LOCATION)/lib $(INSTALL_LIBS)
	-install -t $(INSTALL_LOCATION)/bin $(INSTALL_EXES)
	-find inc/ -type d -printf '%P\n' | xargs -I '{}' install -d $(INSTALL_LOCATION)/include/'{}'
	-find inc/ -type f -printf '%P\n' | xargs -I '{}' install -m 644 inc/'{}' $(INSTALL_LOCATION)/include/'{}'
TARGETLIST+=install

uninstall:
	-rm -r $(INSTALL_LOCATION)/include/fastuidraw
	-rm $(addprefix $(INSTALL_LOCATION)/lib/,$(notdir $(INSTALL_LIBS)))
	-rm $(addprefix $(INSTALL_LOCATION)/bin/,$(notdir $(INSTALL_EXES)))
TARGETLIST+=uninstall

install-docs: docs
	-install -d $(INSTALL_LOCATION)/share/doc/fastuidraw/
	-install -t $(INSTALL_LOCATION)/share/doc/fastuidraw docs/*.txt
	-cp -r docs/doxy/html $(INSTALL_LOCATION)/share/doc/fastuidraw/
	-find $(INSTALL_LOCATION)/share/doc/fastuidraw/html/ -type f -exec chmod a+r {} \;
	-find $(INSTALL_LOCATION)/share/doc/fastuidraw/html/ -type d -exec chmod a+rx {} \;
	-chown -R root $(INSTALL_LOCATION)/share/doc/fastuidraw/html/
TARGETLIST+=install-docs
