glsl_sources = \
	src/fastuidraw/glsl/shaders/fastuidraw_bit_utils.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_atlas_image_fetch.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_spread.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_anisotropic.frag.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_circular_interpolate.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_unpack_unit_vector.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_read_texels_from_data.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_texture_fetch.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_compute_local_distance_from_pixel_distance.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_align.vert.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_banded_rays.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_restricted_rays.glsl.resource_string \
	src/fastuidraw/glsl/shaders/fastuidraw_atlases.glsl.resource_string \
	src/fastuidraw/glsl/shaders/painter/fastuidraw_painter_uniforms.glsl.resource_string \
	src/fastuidraw/glsl/shaders/painter/fastuidraw_painter_globals.vert.glsl.resource_string \
	src/fastuidraw/glsl/shaders/painter/fastuidraw_painter_types.glsl.resource_string \
	src/fastuidraw/glsl/shaders/painter/blend/framebuffer_fetch/fastuidraw_blend_util.frag.glsl.resource_string

# TODO:
#  - various constants/values made in backend_constants
#  - uniforms accessible to shaders
#  - globals accessible to shaders
#  - all the jazz in fastuidraw_atlases, fastuidraw_painter_uniforms, fastuidraw_painter_globals,
#    fastuidraw_brush_utils, fastuidraw_painter_deferred_coverage_buffer, others?
#  - the macros: FASTUIDRAW_RENDER_TO_DEFERRED_COVERAGE_BUFFER, FASTUIDRAW_SUPPORT_BINDLESS_TEXTURE,
#    FASTUIDRAW_DISCARD, FASTUIDRAW_PAINTER_NUMBER_EXTERNAL_TEXTURES, FASTUIDRAW_DEBUG,
#    fastuidraw_extract_bits(), FASTUIDRAW_EXTRACT_BITS(), fastuidraw_do_nothing(), others?
#


glsl_processed_sources = $(patsubst %.glsl.resource_string, docs/doxy/glsl/%.glsl.hpp, $(glsl_sources))

docs/doxy/glsl/%.glsl.hpp: %.glsl.resource_string
	@mkdir -p $(dir $@)
	@sed 's/$(notdir $<)/$(notdir $@)/g' $< >> $@
	@echo "Creating $@ from $<"

docs: docs/doxy/html/index.html
.PHONY: docs
docs/doxy/html/index.html: $(glsl_processed_sources)
	@mkdir -p $(dir $@)
	-cd docs && doxygen Doxyfile
TARGETLIST += docs
CLEAN_FILES += docs/doxy/html $(glsl_processed_sources)
