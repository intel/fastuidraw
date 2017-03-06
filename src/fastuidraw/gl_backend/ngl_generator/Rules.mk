# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

NGL_FILTER := $(call filelist, filter)
NGL_EXTRACTOR := $(call filelist, extractor)

ngl_generator = $(NGL_FILTER) $(NGL_FILTER)
.PHONY:  ngl_generator

NGL_EXTRACTOR_LDFLAGS :=
NGL_LL := flex

$(NGL_FILTER): $(call filelist, filter.cpp)
	$(CXX) -o $@ $^

$(NGL_EXTRACTOR): $(call filelist, gl_flex.o HeaderCreator.o)
	$(CXX) -o $@ $^ $(NGL_EXTRACTOR_LDFLAGS)

$(call filelist, gl_flex.cpp): $(call filelist, gl_flex.fl.cpp)
	$(NGL_LL) -o $@ $^

$(call filelist, gl_flex.o): $(call filelist, gl_flex.cpp, HeaderCreator.hpp)
	$(CXX) -o $@ -c $^

$(call filelist, HeaderCreator.o): $(call filelist, HeaderCreator.cpp, HeaderCreator.hpp)
	$(CXX) -o $@ -c $^

SUPER_CLEAN_FILES += $(call filelist, extractor filter gl_flex.cpp lex.yy.c)
SUPER_CLEAN_FILES += $(call filelist, *.o *.exe)

.SECONDARY: $(NGL_FILTER) $(NGL_EXTRACTOR)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
