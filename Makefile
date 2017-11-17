# Heavily inspired by build system in WRATH:
#
# Copyright 2013 by Nomovok Ltd.
# Contact: info@nomovok.com
# This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with
# this file, You can obtain one at
# http://mozilla.org/MPL/2.0/.

#Init TARGETLIST
TARGETLIST := all

#Init ENVIRONMENTALDESCRIPTIONS
ENVIRONMENTALDESCRIPTIONS :=

#install location
INSTALL_LOCATION ?= /usr/local
ENVIRONMENTALDESCRIPTIONS += "INSTALL_LOCATION:  provides install location (default /usr/local)"
# Mark all intermediate files as secondary and precious
.PRECIOUS:
.SECONDARY:

default: targets
targets:
	@echo
	@echo "Individual Demos available:"
	@echo "=============================="
	@printf "%s\n" $(DEMO_TARGETLIST)
	@echo
	@echo "Targets available:"
	@echo "=============================="
	@printf "%s\n" $(TARGETLIST)
	@echo
	@echo "Environmental variables:"
	@echo "=============================="
	@printf "%s\n" $(ENVIRONMENTALDESCRIPTIONS)
	@echo
.PHONY: targets

all:
.PHONY: all

include make/Makefile.settings.mk
include make/Makefile.gl_backend.settings.mk
include make/Makefile.functions.mk

include make/Makefile.base.pre.mk
include make/Makefile.gl_backend.pre.mk

include make/Makefile.sources.mk

include make/Makefile.base.lib.mk
include make/Makefile.egl.lib.mk
include make/Makefile.gl_backend.lib.mk

include make/Makefile.demo.sources.mk
include make/Makefile.demo.rules.mk

include make/Makefile.docs.mk
include make/Makefile.install.mk

include make/Makefile.clean.mk
