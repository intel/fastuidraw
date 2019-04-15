ifeq ($(MAKECMDGOALS),check)

CHECK_FREETYPE_CFLAGS_CODE := $(shell pkg-config freetype2 --cflags 1> /dev/null 2> /dev/null; echo $$?)
ifeq ($(CHECK_FREETYPE_CFLAGS_CODE), 0)
CHECK_FREETYPE_CFLAGS := "Found cflags for freetype2: $(shell pkg-config freetype2 --cflags)"
else
CHECK_FREETYPE_CFLAGS := "Cannot build FastUIDraw: Unable to find freetype2 cflags from pkg-config: $(shell pkg-config freetype2 --cflags 2>$1)"
FASTUIDRAW_CAN_BUILD := 0
endif

CHECK_FREETYPE_LIBS_CODE := $(shell pkg-config freetype2 --libs 1> /dev/null 2> /dev/null; echo $$?)
ifeq ($(CHECK_FREETYPE_LIBS_CODE), 0)
CHECK_FREETYPE_LIBS := "Found libs for freetype2: $(shell pkg-config freetype2 --libs)"
else
CHECK_FREETYPE_LIBS := "Cannot build FastUIDraw: Unable to find freetype2 libs from pkg-config: $(shell pkg-config freetype2 --libs 2>$1)"
endif

#####################################
## check for each of the GL headers
define checkglheader
$(eval FILE_$(1)_$(2):="$(GL_INCLUDEPATH)/$$(2)"
EXISTS_$(1)_$(2):= $$(shell test -e $$(FILE_$(1)_$(2)) && echo -n yes)
ifeq ($$(EXISTS_$(1)_$(2)),yes)
FOUND_$(1)_HEADERS += $$(FILE_$(1)_$(2))
HAVE_FOUND_$(1)_HEADERS := 1
else
MISSING_$(1)_HEADERS += $$(FILE_$(1)_$(2))
HAVE_MISSING_$(1)_HEADERS:=1
endif
)
endef

ifeq ($(BUILD_GL), 1)
FOUND_GL_HEADERS:=
HAVE_FOUND_GL_HEADERS:=0
MISSING_GL_HEADERS:=
HAVE_MISSING_GL_HEADERS:=0
$(foreach headerfile,$(GL_RAW_HEADER_FILES),$(call checkglheader,GL,$(headerfile)))

ifneq ($(HAVE_FOUND_GL_HEADERS), 0)
CHECK_GL_HEADERS := "GL headers found: $(FOUND_GL_HEADERS)"
endif
ifneq ($(HAVE_MISSING_GL_HEADERS), 0)
CHECK_GL_HEADERS += "GL headers missing: $(MISSING_GL_HEADERS)"
endif

endif

ifeq ($(BUILD_GLES), 1)
FOUND_GLES_HEADERS:=
HAVE_FOUND_GLES_HEADERS:=0
MISSING_GLES_HEADERS:=
HAVE_MISSING_GLES_HEADERS:=0
$(foreach headerfile,$(GLES_RAW_HEADER_FILES),$(call checkglheader,GLES,$(headerfile)))

ifneq ($(HAVE_FOUND_GLES_HEADERS), 0)
CHECK_GLES_HEADERS := "GLES headers found: $(FOUND_GLES_HEADERS)"
endif
ifneq ($(HAVE_MISSING_GLES_HEADERS), 0)
CHECK_GLES_HEADERS += "GLES headers missing: $(MISSING_GLES_HEADERS)"
endif

endif

###################################################
## Check for demo requirements: SDL2 and fontconfig
CHECK_SDL_CFLAGS_CODE := $(shell pkg-config sdl2 --cflags 1> /dev/null 2> /dev/null; echo $$?)
ifeq ($(CHECK_SDL_CFLAGS_CODE), 0)
CHECK_SDL_CFLAGS := "Found cflags for SDL2: $(shell pkg-config sdl2 --cflags)"
else
CHECK_SDL_CFLAGS := "Cannot build demos: Unable to find SDL2 cflags from pkg-config sdl2: $(shell pkg-config sdl2 --cflags 2>$1)"
endif

CHECK_SDL_LIBS_CODE := $(shell pkg-config sdl2 --libs 1> /dev/null 2> /dev/null; echo $$?)
ifeq ($(CHECK_SDL_LIBS_CODE), 0)
CHECK_SDL_LIBS := "Found libs for SDL2: $(shell pkg-config sdl2 --libs)"
else
CHECK_SDL_LIBS := "Cannot build demos: Unable to find SDL2 libs from pkg-config sdl2: $(shell pkg-config sdl2 --libs 2>$1)"
endif

ifeq ($(DEMOS_HAVE_FONT_CONFIG),1)
CHECK_FONTCONFIG_CFLAGS_CODE := $(shell pkg-config fontconfig --cflags 1> /dev/null 2> /dev/null; echo $$?)
ifeq ($(CHECK_FONTCONFIG_CFLAGS_CODE), 0)
CHECK_FONTCONFIG_CFLAGS := "Found cflags for fontconfig: $(shell pkg-config fontconfig --cflags)"
else
CHECK_FONTCONFIG_CFLAGS := "Cannot build demos: Unable to find fontconfig cflags from pkg-config: $(shell pkg-config fontconfig --cflags 2>$1)"
endif

CHECK_FONTCONFIG_LIBS_CODE := $(shell pkg-config fontconfig --libs 1> /dev/null 2> /dev/null; echo $$?)
ifeq ($(CHECK_FONTCONFIG_LIBS_CODE), 0)
CHECK_FONTCONFIG_LIBS := "Found libs for fontconfig: $(shell pkg-config fontconfig --libs)"
else
CHECK_FONTCONFIG_LIBS := "Cannot build demos: Unable to find fontconfig libs from pkg-config: $(shell pkg-config fontconfig --libs 2>$1)"
endif

endif

check:
	@echo "$(CHECK_SDL_CFLAGS)"
	@echo "$(CHECK_SDL_LIBS)"
ifeq ($(DEMOS_HAVE_FONT_CONFIG),1)
	@echo "$(CHECK_FONTCONFIG_CFLAGS)"
	@echo "$(CHECK_FONTCONFIG_LIBS)"
endif
	@echo "$(CHECK_FREETYPE_CFLAGS)"
	@echo "$(CHECK_FREETYPE_LIBS)"
ifeq ($(BUILD_GL), 1)
	@echo "$(CHECK_GL_HEADERS)"
endif
ifeq ($(BUILD_GLES), 1)
	@echo "$(CHECK_GLES_HEADERS)"
endif

.PHONY: check

endif
