# Heavily inspired by build system in WRATH:
#
# Copyright 2013 by Nomovok Ltd.
# Contact: info@nomovok.com
# This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with
# this file, You can obtain one at
# http://mozilla.org/MPL/2.0/.

## Detect MINGW
MINGW_BUILD = 0
UNAME = $(shell uname -s)
UNAMER= $(shell uname -r)
ifeq ($(findstring MINGW,$(UNAME)),MINGW)
  MINGW_BUILD = 1
  ifeq ($(findstring 1.0, $(UNAMER)), 1.0)
	MINGW_MODE = MINGW
  else ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
	MINGW_MODE = MINGW64
  else ifeq ($(findstring MINGW32,$(UNAME)),MINGW32)
	MINGW_MODE = MINGW32
  endif
endif

## Detect Darwin (i.e. MacOS)
DARWIN_BUILD = 0
ifeq ($(UNAME),Darwin)
  DARWIN_BUILD = 1
endif

# Compiler choice
CXX ?= g++
CC ?= gcc

HOST_CXX ?= $(CXX)
HOST_CC ?= $(CC)

ECHO=$(shell which echo)

# Lex choice
LEX ?= flex

# if demos will use font-config, only affects demos and not libs
ifeq ($(DARWIN_BUILD),0)
  DEMOS_HAVE_FONT_CONFIG ?= 1
else
  DEMOS_HAVE_FONT_CONFIG ?= 0
endif

#Init TARGETLIST
TARGETLIST := all

#Init ENVIRONMENTALDESCRIPTIONS
ENVIRONMENTALDESCRIPTIONS :=

# detail about compiler and lex choices
ENVIRONMENTALDESCRIPTIONS += "CXX: choice of C++ compiler (default g++)"
ENVIRONMENTALDESCRIPTIONS += "CC: choice of C++ compiler (default gcc)"
ENVIRONMENTALDESCRIPTIONS += "LEX: choice of lex tool (default flex)"

#install location
INSTALL_LOCATION ?= /usr/local
ENVIRONMENTALDESCRIPTIONS += "INSTALL_LOCATION: provides install location (default /usr/local)"

INSTALL_STATIC ?= 0
ENVIRONMENTALDESCRIPTIONS += "INSTALL_STATIC: if 1, install static libraries (default 0). NOTE: if linking static libs, make sure one links in the entire archive (for example via the linker option --whole-archive (from g++ do -Wl,--whole-archive)"

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
	@echo "Issue make check to see if prerequisites are met for building"
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
include make/Makefile.gl_backend.lib.mk

include make/Makefile.demo.sources.mk
include make/Makefile.demo.rules.mk

include make/Makefile.docs.mk
include make/Makefile.install.mk

include make/Makefile.check.mk
include make/Makefile.clean.mk
