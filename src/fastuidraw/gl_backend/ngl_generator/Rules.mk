# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

NGL_DIRECTORY := $(d)
NGL_FILTER := $(NGL_BUILD)/filter
NGL_EXTRACTOR := $(NGL_BUILD)/extractor

ngl_generator = $(NGL_FILTER) $(NGL_FILTER)

NGL_EXTRACTOR_LDFLAGS :=

$(NGL_FILTER): $(call filelist, filter.cpp)
	mkdir -p $(dir $@)
	$(HOST_CXX) -o $@ $<

$(NGL_EXTRACTOR): $(NGL_BUILD)/gl_flex.o $(NGL_BUILD)/HeaderCreator.o
	$(HOST_CXX) -o $@ $^ $(NGL_EXTRACTOR_LDFLAGS)

$(NGL_BUILD)/gl_flex.cpp: $(call filelist, gl_flex.fl.cpp)
	mkdir -p $(dir $@)
	$(LEX) -o $@ $<

$(NGL_BUILD)/gl_flex.o: $(NGL_BUILD)/gl_flex.cpp $(call filelist, HeaderCreator.hpp)
	mkdir -p $(dir $@)
	$(HOST_CXX) -I$(NGL_DIRECTORY) $(NGL_GENERATOR_FLAGS) -c $< -o $@

$(NGL_BUILD)/HeaderCreator.o: $(call filelist, HeaderCreator.cpp HeaderCreator.hpp)
	mkdir -p $(dir $@)
	$(HOST_CXX) $(NGL_GENERATOR_FLAGS) -c $< -o $@

SUPER_CLEAN_FILES += $(NGL_FILTER) $(NGL_EXTRACTOR)
SUPER_CLEAN_FILES += $(NGL_BUILD)/gl_flex.cpp $(NGL_BUILD)/gl_flex.o $(NGL_BUILD)/HeaderCreator.o

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
