# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

NGL_DIRECTORY := $(d)
NGL_BUILD := build/ngl_generator
NGL_FILTER := $(NGL_BUILD)/filter
NGL_EXTRACTOR := $(NGL_BUILD)/extractor

ngl_generator = $(NGL_FILTER) $(NGL_FILTER)

NGL_EXTRACTOR_LDFLAGS :=
NGL_LL := flex

$(NGL_FILTER): $(call filelist, filter.cpp)
	mkdir -p $(dir $@)
	$(CXX) -o $@ $<

$(NGL_EXTRACTOR): $(NGL_BUILD)/gl_flex.o $(NGL_BUILD)/HeaderCreator.o
	$(CXX) -o $@ $^ $(NGL_EXTRACTOR_LDFLAGS)

$(NGL_BUILD)/gl_flex.cpp: $(call filelist, gl_flex.fl.cpp)
	mkdir -p $(dir $@)
	$(NGL_LL) -o $@ $<

$(NGL_BUILD)/gl_flex.o: $(NGL_BUILD)/gl_flex.cpp $(call filelist, HeaderCreator.hpp)
	mkdir -p $(dir $@)
	$(CXX) -I$(NGL_DIRECTORY) -o $@ -c $<

$(NGL_BUILD)/HeaderCreator.o: $(call filelist, HeaderCreator.cpp HeaderCreator.hpp)
	mkdir -p $(dir $@)
	$(CXX) -o $@ -c $<

SUPER_CLEAN_FILES += $(NGL_FILTER) $(NGL_EXTRACTOR)
SUPER_CLEAN_FILES += $(NGL_BUILD)/gl_flex.cpp $(NGL_BUILD)/gl_flex.o $(NGL_BUILD)/HeaderCreator.o

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
