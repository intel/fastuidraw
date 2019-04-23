glsl_vert_frag_sources = \
	src/fastuidraw/glsl/shaders/fastuidraw_atlas_image_fetch.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_spread.glsl.resource_string

glsl_vert_frag_processed_sources = $(patsubst %.glsl.resource_string, docs/doxy/glsl/vert_frag/%.glsl.hpp, $(glsl_vert_frag_sources))

docs/doxy/glsl/vert_frag/%.glsl.hpp: %.glsl.resource_string
	@mkdir -p $(dir $@)
	@sed 's/$(notdir $<)/$(notdir $@)/g' $< >> $@
	@echo "Creating $@ from $<"

docs: docs/doxy/html/index.html
.PHONY: docs
docs/doxy/html/index.html: $(glsl_vert_frag_processed_sources)
	@mkdir -p $(dir $@)
	-cd docs && doxygen Doxyfile
TARGETLIST += docs
CLEAN_FILES += docs/doxy/html $(glsl_vert_frag_processed_sources)
