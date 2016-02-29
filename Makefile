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

include Makefile.settings
include Makefile.functions

include Makefile.base.pre
include Makefile.gl_backend.pre

include Makefile.sources

include Makefile.base.lib
include Makefile.gl_backend.lib

include Makefile.clean

include Makefile.demo.sources
include Makefile.demo.rules

include Makefile.docs
include Makefile.install
