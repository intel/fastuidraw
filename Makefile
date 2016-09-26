# Heavily inspired by build system in WRATH:
#
# Copyright 2013 by Nomovok Ltd.
# Contact: info@nomovok.com
# This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with
# this file, You can obtain one at
# http://mozilla.org/MPL/2.0/.

# if 1, build/install GL libs on install
BUILD_GL ?= 1

# if 1, build/install GLES libs on install
BUILD_GLES ?= 0

#install location
INSTALL_LOCATION ?= /usr/local

default: $(INSTALL_LIBS)
targets:
	@echo
	@echo "Individual Demos available:"
	@echo "=============================="
	@printf "%s\n" $(DEMO_TARGETLIST)
	@echo
	@echo "Targets available:"
	@echo "=============================="
	@printf "%s\n" $(TARGETLIST)

include Makefile.settings.mk
include Makefile.gl_backend.settings.mk
include Makefile.functions.mk

include Makefile.base.pre.mk
include Makefile.gl_backend.pre.mk

include Makefile.sources.mk

include Makefile.base.lib.mk
include Makefile.gl_backend.lib.mk

include Makefile.clean.mk

include Makefile.demo.sources.mk
include Makefile.demo.rules.mk

include Makefile.docs.mk
include Makefile.install.mk
