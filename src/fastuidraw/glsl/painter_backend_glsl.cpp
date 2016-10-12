/*!
 * \file painter_backend_glsl.cpp
 * \brief file painter_backend_glsl.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#include <sstream>
#include <vector>

#include <fastuidraw/glsl/painter_backend_glsl.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
#include <fastuidraw/painter/painter_header.hpp>
#include <fastuidraw/painter/painter_item_matrix.hpp>
#include <fastuidraw/painter/painter_clip_equations.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/shader_code.hpp>

#include "private/uber_shader_builder.hpp"
#include "private/backend_shaders.hpp"
#include "../private/util_private.hpp"

namespace
{
  enum uniform_ubo_layout
    {
      uniform_ubo_resolution_x_offset,
      uniform_ubo_resolution_y_offset,
      uniform_ubo_recip_resolution_x_offset,
      uniform_ubo_recip_resolution_y_offset,
      uniform_ubo_recip_magnitude_offset,

      uniform_ubo_number_entries
    };

  enum stroke_shader_dash_bits_t
    {
      stroke_gauranteed_to_be_covered_bit,
      stroke_skip_dash_interval_lookup_bit,

      stroke_gauranteed_to_be_covered_mask = FASTUIDRAW_MASK(stroke_gauranteed_to_be_covered_bit, 1),
      stroke_skip_dash_interval_lookup_mask = FASTUIDRAW_MASK(stroke_skip_dash_interval_lookup_bit, 1)
    };

  class BlendShaderGroup
  {
  public:
    typedef fastuidraw::glsl::PainterBlendShaderGLSL Shader;
    typedef fastuidraw::reference_counted_ptr<Shader> Ref;
    std::vector<Ref> m_shaders;
  };

  class ConfigurationGLSLPrivate
  {
  public:
    ConfigurationGLSLPrivate(void):
      m_use_hw_clip_planes(true),
      m_default_blend_shader_type(fastuidraw::PainterBlendShader::dual_src),
      m_non_dashed_stroke_shader_uses_discard(false)
    {}

    bool m_use_hw_clip_planes;
    enum fastuidraw::PainterBlendShader::shader_type m_default_blend_shader_type;
    bool m_non_dashed_stroke_shader_uses_discard;
  };

  class BindingPointsPrivate
  {
  public:
    BindingPointsPrivate(void):
      m_colorstop_atlas(0),
      m_image_atlas_color_tiles_unfiltered(1),
      m_image_atlas_color_tiles_filtered(2),
      m_image_atlas_index_tiles(3),
      m_glyph_atlas_texel_store_uint(4),
      m_glyph_atlas_texel_store_float(5),
      m_glyph_atlas_geometry_store(6),
      m_data_store_buffer_tbo(7),
      m_data_store_buffer_ubo(0),
      m_uniforms_ubo(1)
    {}

    unsigned int m_colorstop_atlas;
    unsigned int m_image_atlas_color_tiles_unfiltered;
    unsigned int m_image_atlas_color_tiles_filtered;
    unsigned int m_image_atlas_index_tiles;
    unsigned int m_glyph_atlas_texel_store_uint;
    unsigned int m_glyph_atlas_texel_store_float;
    unsigned int m_glyph_atlas_geometry_store;
    unsigned int m_data_store_buffer_tbo;
    unsigned int m_data_store_buffer_ubo;
    unsigned int m_uniforms_ubo;
  };

  class UberShaderParamsPrivate
  {
  public:
    UberShaderParamsPrivate(void):
      m_z_coordinate_convention(fastuidraw::glsl::PainterBackendGLSL::z_minus_1_to_1),
      m_negate_normalized_y_coordinate(false),
      m_assign_layout_to_vertex_shader_inputs(true),
      m_assign_layout_to_varyings(true),
      m_assign_binding_points(true),
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_blend_shader_use_switch(false),
      m_unpack_header_and_brush_in_frag_shader(false),
      m_data_store_backing(fastuidraw::glsl::PainterBackendGLSL::data_store_tbo),
      m_data_blocks_per_store_buffer(-1),
      m_glyph_geometry_backing(fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_tbo),
      m_glyph_geometry_backing_log2_dims(-1, -1),
      m_have_float_glyph_texture_atlas(true),
      m_colorstop_atlas_backing(fastuidraw::glsl::PainterBackendGLSL::colorstop_texture_1d_array),
      m_use_ubo_for_uniforms(true),
      m_blend_type(fastuidraw::PainterBlendShader::dual_src)
    {}

    enum fastuidraw::glsl::PainterBackendGLSL::z_coordinate_convention_t m_z_coordinate_convention;
    bool m_negate_normalized_y_coordinate;
    bool m_assign_layout_to_vertex_shader_inputs;
    bool m_assign_layout_to_varyings;
    bool m_assign_binding_points;
    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_blend_shader_use_switch;
    bool m_unpack_header_and_brush_in_frag_shader;
    enum fastuidraw::glsl::PainterBackendGLSL::data_store_backing_t m_data_store_backing;
    int m_data_blocks_per_store_buffer;
    enum fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_backing_t m_glyph_geometry_backing;
    fastuidraw::ivec2 m_glyph_geometry_backing_log2_dims;
    bool m_have_float_glyph_texture_atlas;
    enum fastuidraw::glsl::PainterBackendGLSL::colorstop_backing_t m_colorstop_atlas_backing;
    bool m_use_ubo_for_uniforms;
    enum fastuidraw::PainterBlendShader::shader_type m_blend_type;
    fastuidraw::glsl::PainterBackendGLSL::BindingPoints m_binding_points;
  };

  class PainterBackendGLSLPrivate
  {
  public:
    explicit
    PainterBackendGLSLPrivate(fastuidraw::glsl::PainterBackendGLSL *p,
                              const fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL &config);
    ~PainterBackendGLSLPrivate();

    void
    construct_shader(fastuidraw::glsl::ShaderSource &out_vertex,
                     fastuidraw::glsl::ShaderSource &out_fragment,
                     const fastuidraw::glsl::PainterBackendGLSL::UberShaderParams &contruct_params,
                     const fastuidraw::glsl::PainterBackendGLSL::ItemShaderFilter *item_shader_filter,
                     const char *discard_macro_value);

    void
    update_varying_size(const fastuidraw::glsl::varying_list &plist);

    std::string
    declare_shader_uniforms(const fastuidraw::glsl::PainterBackendGLSL::UberShaderParams &params);

    void
    ready_main_varyings(void);

    void
    ready_brush_varyings(void);

    void
    add_enums(fastuidraw::glsl::ShaderSource &src);

    void
    add_texture_size_constants(fastuidraw::glsl::ShaderSource &src);

    void
    stream_unpack_code(fastuidraw::glsl::ShaderSource &src);

    fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL m_config;
    bool m_shader_code_added;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> > m_item_shaders;
    unsigned int m_next_item_shader_ID;
    fastuidraw::vecN<BlendShaderGroup, fastuidraw::PainterBlendShader::number_types> m_blend_shaders;
    unsigned int m_next_blend_shader_ID;
    fastuidraw::glsl::ShaderSource m_constant_code;
    fastuidraw::glsl::ShaderSource m_vert_shader_utils;
    fastuidraw::glsl::ShaderSource m_frag_shader_utils;

    fastuidraw::vecN<size_t, fastuidraw::glsl::varying_list::interpolation_number_types> m_number_float_varyings;
    size_t m_number_uint_varyings;
    size_t m_number_int_varyings;

    fastuidraw::glsl::varying_list m_main_varyings_header_only;
    fastuidraw::glsl::varying_list m_main_varyings_shaders_and_shader_datas;
    fastuidraw::glsl::varying_list m_brush_varyings;

    fastuidraw::vec2 m_target_resolution;
    fastuidraw::vec2 m_target_resolution_recip;
    float m_target_resolution_recip_magnitude;

    fastuidraw::glsl::PainterBackendGLSL *m_p;
  };
}

/////////////////////////////////////
// PainterBackendGLSLPrivate methods
PainterBackendGLSLPrivate::
PainterBackendGLSLPrivate(fastuidraw::glsl::PainterBackendGLSL *p,
                          const fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL &config):
  m_config(config),
  m_shader_code_added(false),
  m_next_item_shader_ID(1),
  m_next_blend_shader_ID(1),
  m_number_float_varyings(0),
  m_number_uint_varyings(0),
  m_number_int_varyings(0),
  m_p(p)
{
  /* add varyings needed by fastuidraw_painter_main
   */
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  ready_main_varyings();
  ready_brush_varyings();

  ShaderSetCreatorConstants shader_constants;

  m_constant_code
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_SIZE", m_p->image_atlas()->index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", uint32_log2(m_p->image_atlas()->index_tile_size()))
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", m_p->image_atlas()->color_tile_size());
  add_enums(m_constant_code);
  shader_constants.add_constants(m_constant_code);
  add_texture_size_constants(m_constant_code);

  m_vert_shader_utils
    .add_source("fastuidraw_do_nothing.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_anisotropic.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_unpack_unit_vector.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_compute_local_distance_from_pixel_distance.glsl.resource_string",
                ShaderSource::from_resource)
    .add_source("fastuidraw_align.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(code::compute_interval("fastuidraw_compute_interval", m_p->configuration_base().alignment()));

  m_frag_shader_utils
    .add_source("fastuidraw_do_nothing.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_anisotropic.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source(code::compute_interval("fastuidraw_compute_interval", m_p->configuration_base().alignment()))
    .add_source(code::image_atlas_compute_coord("fastuidraw_compute_image_atlas_coord",
                                                "fastuidraw_imageIndexAtlas",
                                                m_p->image_atlas()->index_tile_size(),
                                                m_p->image_atlas()->color_tile_size()))
    .add_source(code::curvepair_compute_pseudo_distance(m_p->glyph_atlas()->geometry_store()->alignment(),
                                                                          "fastuidraw_curvepair_pseudo_distance",
                                                                          "fastuidraw_fetch_glyph_data",
                                                                          false))
    .add_source(code::curvepair_compute_pseudo_distance(m_p->glyph_atlas()->geometry_store()->alignment(),
                                                                          "fastuidraw_curvepair_pseudo_distance",
                                                                          "fastuidraw_fetch_glyph_data",
                                                                          true));
}

PainterBackendGLSLPrivate::
~PainterBackendGLSLPrivate()
{
}

void
PainterBackendGLSLPrivate::
ready_main_varyings(void)
{
  m_main_varyings_header_only
    .add_uint_varying("fastuidraw_header_varying")
    .add_float_varying("fastuidraw_brush_p_x")
    .add_float_varying("fastuidraw_brush_p_y");

  m_main_varyings_shaders_and_shader_datas
    .add_uint_varying("fastuidraw_frag_shader")
    .add_uint_varying("fastuidraw_frag_shader_data_location")
    .add_uint_varying("fastuidraw_blend_shader")
    .add_uint_varying("fastuidraw_blend_shader_data_location")
    .add_float_varying("fastuidraw_brush_p_x")
    .add_float_varying("fastuidraw_brush_p_y");

  if(!m_config.use_hw_clip_planes())
    {
      m_main_varyings_header_only
        .add_float_varying("fastuidraw_clip_plane0")
        .add_float_varying("fastuidraw_clip_plane1")
        .add_float_varying("fastuidraw_clip_plane2")
        .add_float_varying("fastuidraw_clip_plane3");

      m_main_varyings_shaders_and_shader_datas
        .add_float_varying("fastuidraw_clip_plane0")
        .add_float_varying("fastuidraw_clip_plane1")
        .add_float_varying("fastuidraw_clip_plane2")
        .add_float_varying("fastuidraw_clip_plane3");
    }
}

void
PainterBackendGLSLPrivate::
ready_brush_varyings(void)
{
  using namespace fastuidraw::glsl;

  m_brush_varyings
    /* specifies what features are active on the brush
       through values on its bits.
    */
    .add_uint_varying("fastuidraw_brush_shader")

    /* Repeat window paremters:
       - fastuidraw_brush_repeat_window_xy (x,y) coordinate of repeat window
       - fastuidraw_brush_repeat_window_wh dimensions of repeat window
       (all in brush coordinate)
    */
    .add_float_varying("fastuidraw_brush_repeat_window_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_w", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_h", varying_list::interpolation_flat)

    /* Gradient paremters (all in brush coordinates)
       - fastuidraw_brush_gradient_p0 start point of gradient
       - fastuidraw_brush_gradient_p1 end point of gradient
       - fastuidraw_brush_gradient_r0 start radius (radial gradients only)
       - fastuidraw_brush_gradient_r1 end radius (radial gradients only)
    */
    .add_float_varying("fastuidraw_brush_gradient_p0_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p0_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p1_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p1_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_r0", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_r1", varying_list::interpolation_flat)

    /* image parameters
       - fastuidraw_brush_image_xy (x,y) texel coordinate in INDEX texture
                                   of start of image
       - fastuidraw_brush_image_layer layer texel coordinate in INDEX
                                      texture of start of image
       - fastuidraw_brush_image_size size of image (needed for when brush
                                     coordinate goes beyond image size)
       - fastuidraw_brush_image_factor ratio of master index tile size to
                                       dimension of image
    */
    .add_float_varying("fastuidraw_brush_image_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_layer", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_size_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_size_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_factor", varying_list::interpolation_flat)

    /* ColorStop paremeters (only active if gradient active)
       - fastuidraw_brush_color_stop_xy (x,y) texture coordinates of start of color stop
                                       sequence
       - fastuidraw_brush_color_stop_length length of color stop sequence in normalized
                                           texture coordinates
    */
    .add_float_varying("fastuidraw_brush_color_stop_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_color_stop_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_color_stop_length", varying_list::interpolation_flat)

    /* Pen color (RGBA)
     */
    .add_float_varying("fastuidraw_brush_pen_color_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_pen_color_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_pen_color_z", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_pen_color_w", varying_list::interpolation_flat);
}

void
PainterBackendGLSLPrivate::
add_texture_size_constants(fastuidraw::glsl::ShaderSource &src)
{
  fastuidraw::ivec3 glyph_atlas_size, image_atlas_size;
  unsigned int colorstop_atlas_size;

  glyph_atlas_size = m_p->glyph_atlas()->texel_store()->dimensions();
  image_atlas_size = m_p->image_atlas()->color_store()->dimensions();
  colorstop_atlas_size = m_p->colorstop_atlas()->backing_store()->dimensions().x();

  src
    .add_macro("fastuidraw_glyphTexelStore_size_x", glyph_atlas_size.x())
    .add_macro("fastuidraw_glyphTexelStore_size_y", glyph_atlas_size.y())
    .add_macro("fastuidraw_glyphTexelStore_size", "ivec2(fastuidraw_glyphTexelStore_size_x, fastuidraw_glyphTexelStore_size_y)")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_x", "(1.0 / float(fastuidraw_glyphTexelStore_size_x) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_y", "(1.0 / float(fastuidraw_glyphTexelStore_size_y) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal",
               "vec2(fastuidraw_glyphTexelStore_size_reciprocal_x, fastuidraw_glyphTexelStore_size_reciprocal_y)")

    .add_macro("fastuidraw_imageAtlas_size_x", image_atlas_size.x())
    .add_macro("fastuidraw_imageAtlas_size_y", image_atlas_size.y())
    .add_macro("fastuidraw_imageAtlas_size", "ivec2(fastuidraw_imageAtlas_size_x, fastuidraw_imageAtlas_size_y)")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal_x", "(1.0 / float(fastuidraw_imageAtlas_size_x) )")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal_y", "(1.0 / float(fastuidraw_imageAtlas_size_y) )")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal", "vec2(fastuidraw_imageAtlas_size_reciprocal_x, fastuidraw_imageAtlas_size_reciprocal_y)")

    .add_macro("fastuidraw_colorStopAtlas_size", colorstop_atlas_size)
    .add_macro("fastuidraw_colorStopAtlas_size_reciprocal", "(1.0 / float(fastuidraw_colorStopAtlas_size) )");
}

void
PainterBackendGLSLPrivate::
add_enums(fastuidraw::glsl::ShaderSource &src)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterEnums;

  /* fp32 can store a 24-bit integer exactly,
     however, the operation of converting from
     uint to normalized fp32 may lose a bit,
     so 23-bits it is.
     TODO: go through the requirements of IEEE754,
     what a compiler of a driver might do and
     what a GPU does to see how many bits we
     really have.
  */
  uint32_t z_bits_supported;
  unsigned int alignment;

  alignment = m_p->configuration_base().alignment();
  z_bits_supported = 23u;

  src
    .add_macro("fastuidraw_half_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported - 1))
    .add_macro("fastuidraw_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported))
    .add_macro("fastuidraw_shader_image_mask", PainterBrush::image_mask)
    .add_macro("fastuidraw_shader_image_filter_bit0", PainterBrush::image_filter_bit0)
    .add_macro("fastuidraw_shader_image_filter_num_bits", PainterBrush::image_filter_num_bits)
    .add_macro("fastuidraw_shader_image_filter_nearest", PainterBrush::image_filter_nearest)
    .add_macro("fastuidraw_shader_image_filter_linear", PainterBrush::image_filter_linear)
    .add_macro("fastuidraw_shader_image_filter_cubic", PainterBrush::image_filter_cubic)
    .add_macro("fastuidraw_shader_linear_gradient_mask", PainterBrush::gradient_mask)
    .add_macro("fastuidraw_shader_radial_gradient_mask", PainterBrush::radial_gradient_mask)
    .add_macro("fastuidraw_shader_gradient_repeat_mask", PainterBrush::gradient_repeat_mask)
    .add_macro("fastuidraw_shader_repeat_window_mask", PainterBrush::repeat_window_mask)
    .add_macro("fastuidraw_shader_transformation_translation_mask", PainterBrush::transformation_translation_mask)
    .add_macro("fastuidraw_shader_transformation_matrix_mask", PainterBrush::transformation_matrix_mask)
    .add_macro("fastuidraw_image_number_index_lookup_bit0", PainterBrush::image_number_index_lookups_bit0)
    .add_macro("fastuidraw_image_number_index_lookup_num_bits", PainterBrush::image_number_index_lookups_num_bits)
    .add_macro("fastuidraw_image_slack_bit0", PainterBrush::image_slack_bit0)
    .add_macro("fastuidraw_image_slack_num_bits", PainterBrush::image_slack_num_bits)
    .add_macro("fastuidraw_image_master_index_x_bit0",     PainterBrush::image_atlas_location_x_bit0)
    .add_macro("fastuidraw_image_master_index_x_num_bits", PainterBrush::image_atlas_location_x_num_bits)
    .add_macro("fastuidraw_image_master_index_y_bit0",     PainterBrush::image_atlas_location_y_bit0)
    .add_macro("fastuidraw_image_master_index_y_num_bits", PainterBrush::image_atlas_location_y_num_bits)
    .add_macro("fastuidraw_image_master_index_z_bit0",     PainterBrush::image_atlas_location_z_bit0)
    .add_macro("fastuidraw_image_master_index_z_num_bits", PainterBrush::image_atlas_location_z_num_bits)
    .add_macro("fastuidraw_image_size_x_bit0",     PainterBrush::image_size_x_bit0)
    .add_macro("fastuidraw_image_size_x_num_bits", PainterBrush::image_size_x_num_bits)
    .add_macro("fastuidraw_image_size_y_bit0",     PainterBrush::image_size_y_bit0)
    .add_macro("fastuidraw_image_size_y_num_bits", PainterBrush::image_size_y_num_bits)
    .add_macro("fastuidraw_color_stop_x_bit0",     PainterBrush::gradient_color_stop_x_bit0)
    .add_macro("fastuidraw_color_stop_x_num_bits", PainterBrush::gradient_color_stop_x_num_bits)
    .add_macro("fastuidraw_color_stop_y_bit0",     PainterBrush::gradient_color_stop_y_bit0)
    .add_macro("fastuidraw_color_stop_y_num_bits", PainterBrush::gradient_color_stop_y_num_bits)

    .add_macro("fastuidraw_shader_pen_num_blocks", number_blocks(alignment, PainterBrush::pen_data_size))
    .add_macro("fastuidraw_shader_image_num_blocks", number_blocks(alignment, PainterBrush::image_data_size))
    .add_macro("fastuidraw_shader_linear_gradient_num_blocks", number_blocks(alignment, PainterBrush::linear_gradient_data_size))
    .add_macro("fastuidraw_shader_radial_gradient_num_blocks", number_blocks(alignment, PainterBrush::radial_gradient_data_size))
    .add_macro("fastuidraw_shader_repeat_window_num_blocks", number_blocks(alignment, PainterBrush::repeat_window_data_size))
    .add_macro("fastuidraw_shader_transformation_matrix_num_blocks", number_blocks(alignment, PainterBrush::transformation_matrix_data_size))
    .add_macro("fastuidraw_shader_transformation_translation_num_blocks", number_blocks(alignment, PainterBrush::transformation_translation_data_size))
    .add_macro("fastuidraw_stroke_dashed_stroking_params_header_num_blocks",
               number_blocks(alignment, PainterDashedStrokeParams::stroke_static_data_size))

    .add_macro("fastuidraw_item_shader_bit0", PainterHeader::item_shader_bit0)
    .add_macro("fastuidraw_item_shader_num_bits", PainterHeader::item_shader_num_bits)
    .add_macro("fastuidraw_blend_shader_bit0", PainterHeader::blend_shader_bit0)
    .add_macro("fastuidraw_blend_shader_num_bits", PainterHeader::blend_shader_num_bits)

    /* offset types for stroking.
     */
    .add_macro("fastuidraw_stroke_offset_start_sub_edge", StrokedPath::offset_start_sub_edge)
    .add_macro("fastuidraw_stroke_offset_end_sub_edge", StrokedPath::offset_end_sub_edge)
    .add_macro("fastuidraw_stroke_offset_shared_with_edge", StrokedPath::offset_shared_with_edge)
    .add_macro("fastuidraw_stroke_offset_rounded_join", StrokedPath::offset_rounded_join)
    .add_macro("fastuidraw_stroke_offset_miter_join", StrokedPath::offset_miter_join)
    .add_macro("fastuidraw_stroke_offset_rounded_cap", StrokedPath::offset_rounded_cap)
    .add_macro("fastuidraw_stroke_offset_square_cap", StrokedPath::offset_square_cap)
    .add_macro("fastuidraw_stroke_offset_adjustable_cap_contour_start", StrokedPath::offset_adjustable_cap_contour_start)
    .add_macro("fastuidraw_stroke_offset_adjustable_cap_contour_end", StrokedPath::offset_adjustable_cap_contour_end)
    .add_macro("fastuidraw_stroke_offset_type_bit0", StrokedPath::offset_type_bit0)
    .add_macro("fastuidraw_stroke_offset_type_num_bits", StrokedPath::offset_type_num_bits)

    /* bit masks for StrokedPath::point::m_packed_data
     */
    .add_macro("fastuidraw_stroke_sin_sign_mask", StrokedPath::sin_sign_mask)
    .add_macro("fastuidraw_stroke_normal0_y_sign_mask", StrokedPath::normal0_y_sign_mask)
    .add_macro("fastuidraw_stroke_normal1_y_sign_mask", StrokedPath::normal1_y_sign_mask)
    .add_macro("fastuidraw_stroke_boundary_bit", StrokedPath::boundary_bit)
    .add_macro("fastuidraw_stroke_join_mask", StrokedPath::join_mask)
    .add_macro("fastuidraw_stroke_bevel_edge_mask", StrokedPath::bevel_edge_mask)
    .add_macro("fastuidraw_stroke_adjustable_cap_ending_mask", StrokedPath::adjustable_cap_ending_mask)
    .add_macro("fastuidraw_stroke_depth_bit0", StrokedPath::depth_bit0)
    .add_macro("fastuidraw_stroke_depth_num_bits", StrokedPath::depth_num_bits)

    /* dash shader modes.
     */
    .add_macro("fastuidraw_stroke_dashed_flat_caps", PainterEnums::flat_caps)
    .add_macro("fastuidraw_stroke_dashed_rounded_caps", PainterEnums::rounded_caps)
    .add_macro("fastuidraw_stroke_dashed_square_caps", PainterEnums::square_caps)

    .add_macro("fastuidraw_stroke_gauranteed_to_be_covered_mask", stroke_gauranteed_to_be_covered_mask)
    .add_macro("fastuidraw_stroke_skip_dash_interval_lookup_mask", stroke_skip_dash_interval_lookup_mask)

    .add_macro("fastuidraw_data_store_alignment", alignment);
}

void
PainterBackendGLSLPrivate::
stream_unpack_code(fastuidraw::glsl::ShaderSource &str)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;

  unsigned int alignment;
  alignment = m_p->configuration_base().alignment();

  {
    shader_unpack_value_set<PainterBrush::pen_data_size> labels;
    labels
      .set(PainterBrush::pen_red_offset, ".r")
      .set(PainterBrush::pen_green_offset, ".g")
      .set(PainterBrush::pen_blue_offset, ".b")
      .set(PainterBrush::pen_alpha_offset, ".a")
      .stream_unpack_function(alignment, str, "fastuidraw_read_pen_color", "vec4");
  }

  {
    /* Matrics in GLSL are [column][row], that is why
       one sees the transposing to the loads
    */
    shader_unpack_value_set<PainterBrush::transformation_matrix_data_size> labels;
    labels
      .set(PainterBrush::transformation_matrix_m00_offset, "[0][0]")
      .set(PainterBrush::transformation_matrix_m10_offset, "[0][1]")
      .set(PainterBrush::transformation_matrix_m01_offset, "[1][0]")
      .set(PainterBrush::transformation_matrix_m11_offset, "[1][1]")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_transformation_matrix",
                              "mat2");
  }

  {
    shader_unpack_value_set<PainterBrush::transformation_translation_data_size> labels;
    labels
      .set(PainterBrush::transformation_translation_x_offset, ".x")
      .set(PainterBrush::transformation_translation_y_offset, ".y")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_transformation_translation",
                              "vec2");
  }

  {
    shader_unpack_value_set<PainterBrush::repeat_window_data_size> labels;
    labels
      .set(PainterBrush::repeat_window_x_offset, ".xy.x")
      .set(PainterBrush::repeat_window_y_offset, ".xy.y")
      .set(PainterBrush::repeat_window_width_offset, ".wh.x")
      .set(PainterBrush::repeat_window_height_offset, ".wh.y")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_repeat_window",
                              "fastuidraw_brush_repeat_window");
  }

  {
    shader_unpack_value_set<PainterBrush::image_data_size> labels;
    labels
      .set(PainterBrush::image_atlas_location_xyz_offset, ".image_atlas_location_xyz", shader_unpack_value::uint_type)
      .set(PainterBrush::image_size_xy_offset, ".image_size_xy", shader_unpack_value::uint_type)
      .set(PainterBrush::image_start_xy_offset, ".image_start_xy", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_image_raw_data",
                              "fastuidraw_brush_image_data_raw");
  }

  {
    shader_unpack_value_set<PainterBrush::linear_gradient_data_size> labels;
    labels
      .set(PainterBrush::gradient_p0_x_offset, ".p0.x")
      .set(PainterBrush::gradient_p0_y_offset, ".p0.y")
      .set(PainterBrush::gradient_p1_x_offset, ".p1.x")
      .set(PainterBrush::gradient_p1_y_offset, ".p1.y")
      .set(PainterBrush::gradient_color_stop_xy_offset, ".color_stop_sequence_xy", shader_unpack_value::uint_type)
      .set(PainterBrush::gradient_color_stop_length_offset, ".color_stop_sequence_length", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_linear_gradient_data",
                              "fastuidraw_brush_gradient_raw");
  }

  {
    shader_unpack_value_set<PainterBrush::radial_gradient_data_size> labels;
    labels
      .set(PainterBrush::gradient_p0_x_offset, ".p0.x")
      .set(PainterBrush::gradient_p0_y_offset, ".p0.y")
      .set(PainterBrush::gradient_p1_x_offset, ".p1.x")
      .set(PainterBrush::gradient_p1_y_offset, ".p1.y")
      .set(PainterBrush::gradient_color_stop_xy_offset, ".color_stop_sequence_xy", shader_unpack_value::uint_type)
      .set(PainterBrush::gradient_color_stop_length_offset, ".color_stop_sequence_length", shader_unpack_value::uint_type)
      .set(PainterBrush::gradient_start_radius_offset, ".r0")
      .set(PainterBrush::gradient_end_radius_offset, ".r1")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_radial_gradient_data",
                              "fastuidraw_brush_gradient_raw");
  }

  {
    shader_unpack_value_set<PainterHeader::header_size> labels;
    labels
      .set(PainterHeader::clip_equations_location_offset, ".clipping_location", shader_unpack_value::uint_type)
      .set(PainterHeader::item_matrix_location_offset, ".item_matrix_location", shader_unpack_value::uint_type)
      .set(PainterHeader::brush_shader_data_location_offset, ".brush_shader_data_location", shader_unpack_value::uint_type)
      .set(PainterHeader::item_shader_data_location_offset, ".item_shader_data_location", shader_unpack_value::uint_type)
      .set(PainterHeader::blend_shader_data_location_offset, ".blend_shader_data_location", shader_unpack_value::uint_type)
      .set(PainterHeader::brush_shader_offset, ".brush_shader", shader_unpack_value::uint_type)
      .set(PainterHeader::z_offset, ".z", shader_unpack_value::uint_type)
      .set(PainterHeader::item_blend_shader_offset, ".item_blend_shader_packed", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_header",
                              "fastuidraw_shader_header", false);
  }

  {
    shader_unpack_value_set<PainterClipEquations::clip_data_size> labels;
    labels
      .set(PainterClipEquations::clip0_coeff_x, ".clip0.x")
      .set(PainterClipEquations::clip0_coeff_y, ".clip0.y")
      .set(PainterClipEquations::clip0_coeff_w, ".clip0.z")

      .set(PainterClipEquations::clip1_coeff_x, ".clip1.x")
      .set(PainterClipEquations::clip1_coeff_y, ".clip1.y")
      .set(PainterClipEquations::clip1_coeff_w, ".clip1.z")

      .set(PainterClipEquations::clip2_coeff_x, ".clip2.x")
      .set(PainterClipEquations::clip2_coeff_y, ".clip2.y")
      .set(PainterClipEquations::clip2_coeff_w, ".clip2.z")

      .set(PainterClipEquations::clip3_coeff_x, ".clip3.x")
      .set(PainterClipEquations::clip3_coeff_y, ".clip3.y")
      .set(PainterClipEquations::clip3_coeff_w, ".clip3.z")

      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_clipping",
                              "fastuidraw_clipping_data", false);
  }

  {
    /* Matrics in GLSL are [column][row], that is why
       one sees the transposing to the loads
    */
    shader_unpack_value_set<PainterItemMatrix::matrix_data_size> labels;
    labels
      .set(PainterItemMatrix::matrix00_offset, "[0][0]")
      .set(PainterItemMatrix::matrix10_offset, "[0][1]")
      .set(PainterItemMatrix::matrix20_offset, "[0][2]")
      .set(PainterItemMatrix::matrix01_offset, "[1][0]")
      .set(PainterItemMatrix::matrix11_offset, "[1][1]")
      .set(PainterItemMatrix::matrix21_offset, "[1][2]")
      .set(PainterItemMatrix::matrix02_offset, "[2][0]")
      .set(PainterItemMatrix::matrix12_offset, "[2][1]")
      .set(PainterItemMatrix::matrix22_offset, "[2][2]")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_item_matrix", "mat3", false);
  }

  {
    shader_unpack_value_set<PainterStrokeParams::stroke_data_size> labels;
    labels
      .set(PainterStrokeParams::stroke_radius_offset, ".radius")
      .set(PainterStrokeParams::stroke_miter_limit_offset, ".miter_limit")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_stroking_params",
                              "fastuidraw_stroking_params",
                              true);
  }

  {
    shader_unpack_value_set<PainterDashedStrokeParams::stroke_static_data_size> labels;
    labels
      .set(PainterDashedStrokeParams::stroke_radius_offset, ".radius")
      .set(PainterDashedStrokeParams::stroke_miter_limit_offset, ".miter_limit")
      .set(PainterDashedStrokeParams::stroke_dash_offset_offset, ".dash_offset")
      .set(PainterDashedStrokeParams::stroke_total_length_offset, ".total_length")
      .set(PainterDashedStrokeParams::stroke_first_interval_start_offset, ".first_interval_start")
      .set(PainterDashedStrokeParams::stroke_number_intervals_offset, ".number_intervals", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_dashed_stroking_params_header",
                              "fastuidraw_dashed_stroking_params_header",
                              true);
  }
}

void
PainterBackendGLSLPrivate::
update_varying_size(const fastuidraw::glsl::varying_list &plist)
{
  m_number_uint_varyings = std::max(m_number_uint_varyings, plist.uints().size());
  m_number_int_varyings = std::max(m_number_int_varyings, plist.ints().size());
  for(unsigned int i = 0; i < fastuidraw::glsl::varying_list::interpolation_number_types; ++i)
    {
      enum fastuidraw::glsl::varying_list::interpolation_qualifier_t q;
      q = static_cast<enum fastuidraw::glsl::varying_list::interpolation_qualifier_t>(i);
      m_number_float_varyings[q] = std::max(m_number_float_varyings[q], plist.floats(q).size());
    }
}

std::string
PainterBackendGLSLPrivate::
declare_shader_uniforms(const fastuidraw::glsl::PainterBackendGLSL::UberShaderParams &params)
{
  std::ostringstream ostr;

  if(params.use_ubo_for_uniforms())
    {
      const char *ext="xyzw";
      /* Mesa packs UBO data float[N] as really vec4[N],
         so instead realize the data directly as vec4[K]
       */
      ostr << "FASTUIDRAW_LAYOUT_BINDING("
           << params.binding_points().uniforms_ubo()
           << ") " << " uniform fastuidraw_uniform_block {\n"
           << "vec4 fastuidraw_shader_uniforms["
           << m_p->ubo_size() / 4 << "];\n"
           << "};\n"
           << "#define fastuidraw_viewport_pixels vec2(fastuidraw_shader_uniforms["
           << uniform_ubo_resolution_x_offset / 4 << "]."
           << ext[uniform_ubo_resolution_x_offset % 4]
           << ", fastuidraw_shader_uniforms[" << uniform_ubo_resolution_y_offset / 4 << "]."
           << ext[uniform_ubo_resolution_y_offset % 4] << ")\n"
           << "#define fastuidraw_viewport_recip_pixels vec2(fastuidraw_shader_uniforms["
           << uniform_ubo_recip_resolution_x_offset / 4
           << "]."<< ext[uniform_ubo_recip_resolution_x_offset % 4]
           << ", fastuidraw_shader_uniforms["
           << uniform_ubo_recip_resolution_y_offset / 4 << "]."
           << ext[uniform_ubo_recip_resolution_y_offset % 4] << ")\n"
           << "#define fastuidraw_viewport_recip_pixels_magnitude fastuidraw_shader_uniforms["
           << uniform_ubo_recip_magnitude_offset / 4 << "]."
           << ext[uniform_ubo_recip_magnitude_offset % 4] << "\n";
    }
  else
    {
      ostr << "uniform float fastuidraw_shader_uniforms["
           << m_p->ubo_size() << "];\n"
           << "#define fastuidraw_viewport_pixels vec2(fastuidraw_shader_uniforms["
           << uniform_ubo_resolution_x_offset << "], fastuidraw_shader_uniforms["
           << uniform_ubo_resolution_y_offset << "])\n"
           << "#define fastuidraw_viewport_recip_pixels vec2(fastuidraw_shader_uniforms["
           << uniform_ubo_recip_resolution_x_offset
           << "], fastuidraw_shader_uniforms["
           << uniform_ubo_recip_resolution_y_offset << "])\n"
           << "#define fastuidraw_viewport_recip_pixels_magnitude fastuidraw_shader_uniforms["
           << uniform_ubo_recip_magnitude_offset << "]\n";
    }

  return ostr.str();
}

void
PainterBackendGLSLPrivate::
construct_shader(fastuidraw::glsl::ShaderSource &vert,
                 fastuidraw::glsl::ShaderSource &frag,
                 const fastuidraw::glsl::PainterBackendGLSL::UberShaderParams &params,
                 const fastuidraw::glsl::PainterBackendGLSL::ItemShaderFilter *item_shader_filter,
                 const char *discard_macro_value)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  std::string varying_layout_macro, binding_layout_macro;
  std::string declare_shader_varyings, declare_main_varyings, declare_brush_varyings;
  std::string declare_vertex_shader_ins;
  std::string declare_uniforms;
  const varying_list *main_varyings;
  DeclareVaryingsStringDatum main_varying_datum, brush_varying_datum, shader_varying_datum;
  const PainterBackendGLSL::BindingPoints &binding_params(params.binding_points());
  std::vector<reference_counted_ptr<PainterItemShaderGLSL> > work_shaders;
  const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders;

  if(item_shader_filter)
    {
      for(unsigned int i = 0, endi = m_item_shaders.size(); i < endi; ++i)
        {
          if(item_shader_filter->use_shader(m_item_shaders[i]))
            {
              work_shaders.push_back(m_item_shaders[i]);
            }
        }
      item_shaders = make_c_array(work_shaders);
    }
  else
    {
      item_shaders = make_c_array(m_item_shaders);
    }

  if(params.assign_layout_to_vertex_shader_inputs())
    {
      std::ostringstream ostr;
      ostr << "layout(location = " << PainterBackendGLSL::primary_attrib_slot << ") in uvec4 fastuidraw_primary_attribute;\n"
           << "layout(location = " << PainterBackendGLSL::secondary_attrib_slot << ") in uvec4 fastuidraw_secondary_attribute;\n"
           << "layout(location = " << PainterBackendGLSL::uint_attrib_slot << ") in uvec4 fastuidraw_uint_attribute;\n"
           << "layout(location = " << PainterBackendGLSL::header_attrib_slot << ") in uint fastuidraw_header_attribute;\n";
      declare_vertex_shader_ins = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "in uvec4 fastuidraw_primary_attribute;\n"
           << "in uvec4 fastuidraw_secondary_attribute;\n"
           << "in uvec4 fastuidraw_uint_attribute;\n"
           << "in uint fastuidraw_header_attribute;\n";
      declare_vertex_shader_ins = ostr.str();
    }

  if(params.assign_layout_to_varyings())
    {
      std::ostringstream ostr;
      ostr << "\n#define FASTUIDRAW_LAYOUT_VARYING(X) layout(location = X)";
      varying_layout_macro = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "\n#define FASTUIDRAW_LAYOUT_VARYING(X)\n";
      varying_layout_macro = ostr.str();
    }

  if(params.assign_binding_points())
    {
      std::ostringstream ostr;
      ostr << "\n#define FASTUIDRAW_LAYOUT_BINDING(X) layout(binding = X)";
      ostr << "\n#define FASTUIDRAW_LAYOUT_BINDING_ARGS(X, Y) layout(binding = X, Y)";
      binding_layout_macro = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "\n#define FASTUIDRAW_LAYOUT_BINDING(X)";
      ostr << "\n#define FASTUIDRAW_LAYOUT_BINDING_ARGS(X, Y) layout(Y)";
      binding_layout_macro = ostr.str();
    }

  unsigned int varying_slot(0);
  if(params.unpack_header_and_brush_in_frag_shader())
    {
      main_varyings = &m_main_varyings_header_only;
    }
  else
    {
      main_varyings = &m_main_varyings_shaders_and_shader_datas;
      declare_brush_varyings = declare_varyings_string("_brush",
                                                       m_brush_varyings.uints().size(),
                                                       m_brush_varyings.ints().size(),
                                                       m_brush_varyings.float_counts(),
                                                       &varying_slot,
                                                       &brush_varying_datum);
    }

  declare_main_varyings = declare_varyings_string("_main",
                                                  main_varyings->uints().size(),
                                                  main_varyings->ints().size(),
                                                  main_varyings->float_counts(),
                                                  &varying_slot,
                                                  &main_varying_datum);

  declare_shader_varyings = declare_varyings_string("_shader",
                                                    m_number_uint_varyings,
                                                    m_number_int_varyings,
                                                    m_number_float_varyings,
                                                    &varying_slot,
                                                    &shader_varying_datum);

  declare_uniforms = declare_shader_uniforms(params);

  if(params.unpack_header_and_brush_in_frag_shader())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
      frag.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
    }

  if(params.negate_normalized_y_coordinate())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NEGATE_POSITION_Y_COORDINATE");
      frag.add_macro("FASTUIDRAW_PAINTER_NEGATE_POSITION_Y_COORDINATE");
    }

  if(params.z_coordinate_convention() == PainterBackendGLSL::z_minus_1_to_1)
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_Z_MINUS_1_TO_1");
      frag.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_Z_MINUS_1_TO_1");
    }
  else
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_0_TO_1");
      frag.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_0_TO_1");
    }

  if(m_config.use_hw_clip_planes())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_USE_HW_CLIP_PLANES");
      frag.add_macro("FASTUIDRAW_PAINTER_USE_HW_CLIP_PLANES");
    }

  switch(params.colorstop_atlas_backing())
    {
    case PainterBackendGLSL::colorstop_texture_1d_array:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_1D_ARRAY");
        frag.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_1D_ARRAY");
      }
    break;

    case PainterBackendGLSL::colorstop_texture_2d_array:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_2D_ARRAY");
        frag.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_2D_ARRAY");
      }
    break;

    default:
      assert(!"Invalid colorstop_atlas_backing() value");
    }

  switch(params.data_store_backing())
    {
    case PainterBackendGLSL::data_store_ubo:
      {
        unsigned int alignment(m_p->configuration_base().alignment());
        assert(alignment == 4);
        FASTUIDRAWunused(alignment);

        vert
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer());

        frag
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer());
      }
      break;

    case PainterBackendGLSL::data_store_tbo:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
        frag.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
      }
      break;

    default:
      assert(!"Invalid data_store_backing() value");
    }

  if(!params.have_float_glyph_texture_atlas())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
      frag.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
    }

  switch(params.glyph_geometry_backing())
    {
    case PainterBackendGLSL::glyph_geometry_texture_array:
      {
        vert
          .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_ARRAY")
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_WIDTH_LOG2", params.glyph_geometry_backing_log2_dims().x())
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_HEIGHT_LOG2", params.glyph_geometry_backing_log2_dims().y());

        frag
          .add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_ARRAY")
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_WIDTH_LOG2", params.glyph_geometry_backing_log2_dims().x())
          .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_HEIGHT_LOG2", params.glyph_geometry_backing_log2_dims().y());
      }
      break;

    case PainterBackendGLSL::glyph_geometry_tbo:
      {
        vert.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
        frag.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
      }
      break;
    }

  vert
    .add_source(m_constant_code)
    .add_source(varying_layout_macro.c_str(), ShaderSource::from_string)
    .add_source(binding_layout_macro.c_str(), ShaderSource::from_string)
    .add_macro("FASTUIDRAW_COLORSTOP_ATLAS_BINDING", binding_params.colorstop_atlas())
    .add_macro("FASTUIDRAW_COLOR_TILE_UNFILTERED_BINDING", binding_params.image_atlas_color_tiles_unfiltered())
    .add_macro("FASTUIDRAW_COLOR_TILE_FILTERED_BINDING", binding_params.image_atlas_color_tiles_filtered())
    .add_macro("FASTUIDRAW_INDEX_TILE_BINDING", binding_params.image_atlas_index_tiles())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_UINT_BINDING", binding_params.glyph_atlas_texel_store_uint())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_FLOAT_BINDING", binding_params.glyph_atlas_texel_store_float())
    .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_STORE_BINDING", binding_params.glyph_atlas_geometry_store())
    .add_macro("FASTUIDRAW_PAINTER_STORE_TBO_BINDING", binding_params.data_store_buffer_tbo())
    .add_macro("FASTUIDRAW_PAINTER_STORE_UBO_BINDING", binding_params.data_store_buffer_ubo())
    .add_macro("fastuidraw_varying", "out")
    .add_source(declare_vertex_shader_ins.c_str(), ShaderSource::from_string)
    .add_source(declare_brush_varyings.c_str(), ShaderSource::from_string)
    .add_source(declare_main_varyings.c_str(), ShaderSource::from_string)
    .add_source(declare_shader_varyings.c_str(), ShaderSource::from_string);

  stream_alias_varyings("_main", vert, *main_varyings, true, main_varying_datum);
  if(params.unpack_header_and_brush_in_frag_shader())
    {
      /* we need to declare the values named in m_brush_varyings
       */
      stream_varyings_as_local_variables(vert, m_brush_varyings);
    }
  else
    {
      stream_alias_varyings("_brush", vert, m_brush_varyings, true, brush_varying_datum);
    }

  vert
    .add_source(declare_uniforms.c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_main.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(m_vert_shader_utils);

  stream_unpack_code(vert);
  stream_uber_vert_shader(params.vert_shader_use_switch(), vert, item_shaders,
                          shader_varying_datum);

  const char *shader_blend_macro;
  switch(params.blend_type())
    {
    case PainterBlendShader::framebuffer_fetch:
      shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH";
      break;

    case PainterBlendShader::dual_src:
      shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND";
      break;

    case PainterBlendShader::single_src:
      shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND";
      break;

    default:
      shader_blend_macro = "FASTUIDRAW_PAINTER_BLEND_INVALID_BLEND";
      assert(!"Invalid blend_type");
    }

  frag
    .add_source(m_constant_code)
    .add_source(varying_layout_macro.c_str(), ShaderSource::from_string)
    .add_source(binding_layout_macro.c_str(), ShaderSource::from_string)
    .add_macro("FASTUIDRAW_DISCARD", discard_macro_value)
    .add_macro(shader_blend_macro)
    .add_macro("FASTUIDRAW_COLORSTOP_ATLAS_BINDING", binding_params.colorstop_atlas())
    .add_macro("FASTUIDRAW_COLOR_TILE_UNFILTERED_BINDING", binding_params.image_atlas_color_tiles_unfiltered())
    .add_macro("FASTUIDRAW_COLOR_TILE_FILTERED_BINDING", binding_params.image_atlas_color_tiles_filtered())
    .add_macro("FASTUIDRAW_INDEX_TILE_BINDING", binding_params.image_atlas_index_tiles())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_UINT_BINDING", binding_params.glyph_atlas_texel_store_uint())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_FLOAT_BINDING", binding_params.glyph_atlas_texel_store_float())
    .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_STORE_BINDING", binding_params.glyph_atlas_geometry_store())
    .add_macro("FASTUIDRAW_PAINTER_STORE_TBO_BINDING", binding_params.data_store_buffer_tbo())
    .add_macro("FASTUIDRAW_PAINTER_STORE_UBO_BINDING", binding_params.data_store_buffer_ubo())
    .add_macro("fastuidraw_varying", "in")
    .add_source(declare_brush_varyings.c_str(), ShaderSource::from_string)
    .add_source(declare_main_varyings.c_str(), ShaderSource::from_string)
    .add_source(declare_shader_varyings.c_str(), ShaderSource::from_string);

  stream_alias_varyings("_main", frag, *main_varyings, true, main_varying_datum);
  if(params.unpack_header_and_brush_in_frag_shader())
    {
      /* we need to declare the values named in m_brush_varyings
       */
      stream_varyings_as_local_variables(frag, m_brush_varyings);
    }
  else
    {
      stream_alias_varyings("_brush", frag, m_brush_varyings, true, brush_varying_datum);
    }

  frag
    .add_source(declare_uniforms.c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.frag.glsl.resource_string", ShaderSource::from_resource);

  if(params.unpack_header_and_brush_in_frag_shader())
    {
      frag
        .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", ShaderSource::from_resource)
        .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", ShaderSource::from_resource);
    }

  frag
    .add_source("fastuidraw_painter_brush.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_main.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source(m_frag_shader_utils);

  stream_unpack_code(frag);
  stream_uber_frag_shader(params.frag_shader_use_switch(), frag, item_shaders,
                          shader_varying_datum);
  stream_uber_blend_shader(params.blend_shader_use_switch(), frag,
                           make_c_array(m_blend_shaders[params.blend_type()].m_shaders),
                           params.blend_type());
}

/////////////////////////////////////////////////////////////////
//fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL methods
fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
ConfigurationGLSL(void)
{
  m_d = FASTUIDRAWnew ConfigurationGLSLPrivate();
}

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
ConfigurationGLSL(const ConfigurationGLSL &obj)
{
  ConfigurationGLSLPrivate *d;
  d = reinterpret_cast<ConfigurationGLSLPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationGLSLPrivate(*d);
}

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
~ConfigurationGLSL()
{
  ConfigurationGLSLPrivate *d;
  d = reinterpret_cast<ConfigurationGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL&
fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
operator=(const ConfigurationGLSL &rhs)
{
  if(this != &rhs)
    {
      ConfigurationGLSLPrivate *d, *rhs_d;
      d = reinterpret_cast<ConfigurationGLSLPrivate*>(m_d);
      rhs_d = reinterpret_cast<ConfigurationGLSLPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                                    \
  fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL&              \
  fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::             \
  name(type v)                                                          \
  {                                                                     \
    ConfigurationGLSLPrivate *d;                                        \
    d = reinterpret_cast<ConfigurationGLSLPrivate*>(m_d);               \
    d->m_##name = v;                                                    \
    return *this;                                                       \
  }                                                                     \
                                                                        \
  type                                                                  \
  fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::             \
  name(void) const                                                      \
  {                                                                     \
    ConfigurationGLSLPrivate *d;                                        \
    d = reinterpret_cast<ConfigurationGLSLPrivate*>(m_d);               \
    return d->m_##name;                                                 \
  }

setget_implement(bool, use_hw_clip_planes)
setget_implement(enum fastuidraw::PainterBlendShader::shader_type, default_blend_shader_type)
setget_implement(bool, non_dashed_stroke_shader_uses_discard)

#undef setget_implement

/////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterBackendGLSL::BindingPoints methods
fastuidraw::glsl::PainterBackendGLSL::BindingPoints::
BindingPoints(void)
{
  m_d = FASTUIDRAWnew BindingPointsPrivate();
}

fastuidraw::glsl::PainterBackendGLSL::BindingPoints::
BindingPoints(const BindingPoints &obj)
{
  BindingPointsPrivate *d;
  d = reinterpret_cast<BindingPointsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew BindingPointsPrivate(*d);
}

fastuidraw::glsl::PainterBackendGLSL::BindingPoints::
~BindingPoints()
{
  BindingPointsPrivate *d;
  d = reinterpret_cast<BindingPointsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::glsl::PainterBackendGLSL::BindingPoints&
fastuidraw::glsl::PainterBackendGLSL::BindingPoints::
operator=(const BindingPoints &rhs)
{
  if(this != &rhs)
    {
      BindingPointsPrivate *d, *rhs_d;
      d = reinterpret_cast<BindingPointsPrivate*>(m_d);
      rhs_d = reinterpret_cast<BindingPointsPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                                    \
  fastuidraw::glsl::PainterBackendGLSL::BindingPoints&                  \
  fastuidraw::glsl::PainterBackendGLSL::BindingPoints::                 \
  name(type v)                                                          \
  {                                                                     \
    BindingPointsPrivate *d;                                            \
    d = reinterpret_cast<BindingPointsPrivate*>(m_d);                   \
    d->m_##name = v;                                                    \
    return *this;                                                       \
  }                                                                     \
                                                                        \
  type                                                                  \
  fastuidraw::glsl::PainterBackendGLSL::BindingPoints::                 \
  name(void) const                                                      \
  {                                                                     \
    BindingPointsPrivate *d;                                            \
    d = reinterpret_cast<BindingPointsPrivate*>(m_d);                   \
    return d->m_##name;                                                 \
  }

setget_implement(unsigned int, colorstop_atlas)
setget_implement(unsigned int, image_atlas_color_tiles_unfiltered)
setget_implement(unsigned int, image_atlas_color_tiles_filtered)
setget_implement(unsigned int, image_atlas_index_tiles)
setget_implement(unsigned int, glyph_atlas_texel_store_uint)
setget_implement(unsigned int, glyph_atlas_texel_store_float)
setget_implement(unsigned int, glyph_atlas_geometry_store)
setget_implement(unsigned int, data_store_buffer_tbo)
setget_implement(unsigned int, data_store_buffer_ubo)
setget_implement(unsigned int, uniforms_ubo)

#undef setget_implement

////////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterBackendGLSL::UberShaderParams methods
fastuidraw::glsl::PainterBackendGLSL::UberShaderParams::
UberShaderParams(void)
{
  m_d = FASTUIDRAWnew UberShaderParamsPrivate();
}

fastuidraw::glsl::PainterBackendGLSL::UberShaderParams::
UberShaderParams(const UberShaderParams &obj)
{
  UberShaderParamsPrivate *d;
  d = reinterpret_cast<UberShaderParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew UberShaderParamsPrivate(*d);
}

fastuidraw::glsl::PainterBackendGLSL::UberShaderParams::
~UberShaderParams()
{
  UberShaderParamsPrivate *d;
  d = reinterpret_cast<UberShaderParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::glsl::PainterBackendGLSL::UberShaderParams&
fastuidraw::glsl::PainterBackendGLSL::UberShaderParams::
operator=(const UberShaderParams &rhs)
{
  if(this != &rhs)
    {
      UberShaderParamsPrivate *d, *rhs_d;
      d = reinterpret_cast<UberShaderParamsPrivate*>(m_d);
      rhs_d = reinterpret_cast<UberShaderParamsPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                                    \
  fastuidraw::glsl::PainterBackendGLSL::UberShaderParams&               \
  fastuidraw::glsl::PainterBackendGLSL::UberShaderParams::              \
  name(type v)                                                          \
  {                                                                     \
    UberShaderParamsPrivate *d;                                         \
    d = reinterpret_cast<UberShaderParamsPrivate*>(m_d);                \
    d->m_##name = v;                                                    \
    return *this;                                                       \
  }                                                                     \
                                                                        \
  type                                                                  \
  fastuidraw::glsl::PainterBackendGLSL::UberShaderParams::              \
  name(void) const                                                      \
  {                                                                     \
    UberShaderParamsPrivate *d;                                         \
    d = reinterpret_cast<UberShaderParamsPrivate*>(m_d);                \
    return d->m_##name;                                                 \
  }

setget_implement(enum fastuidraw::glsl::PainterBackendGLSL::z_coordinate_convention_t, z_coordinate_convention)
setget_implement(bool, negate_normalized_y_coordinate)
setget_implement(bool, assign_layout_to_vertex_shader_inputs)
setget_implement(bool, assign_layout_to_varyings)
setget_implement(bool, assign_binding_points)
setget_implement(bool, vert_shader_use_switch)
setget_implement(bool, frag_shader_use_switch)
setget_implement(bool, blend_shader_use_switch)
setget_implement(bool, unpack_header_and_brush_in_frag_shader)
setget_implement(enum fastuidraw::glsl::PainterBackendGLSL::data_store_backing_t, data_store_backing)
setget_implement(int, data_blocks_per_store_buffer)
setget_implement(enum fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_backing_t, glyph_geometry_backing)
setget_implement(fastuidraw::ivec2, glyph_geometry_backing_log2_dims)
setget_implement(bool, have_float_glyph_texture_atlas)
setget_implement(enum fastuidraw::glsl::PainterBackendGLSL::colorstop_backing_t, colorstop_atlas_backing)
setget_implement(bool, use_ubo_for_uniforms)
setget_implement(enum fastuidraw::PainterBlendShader::shader_type, blend_type)
setget_implement(const fastuidraw::glsl::PainterBackendGLSL::BindingPoints&, binding_points)
#undef setget_implement

//////////////////////////////////////////////
// fastuidraw::glsl::PainterBackendGLSL methods
fastuidraw::glsl::PainterBackendGLSL::
PainterBackendGLSL(reference_counted_ptr<GlyphAtlas> glyph_atlas,
                   reference_counted_ptr<ImageAtlas> image_atlas,
                   reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
                   const ConfigurationGLSL &config_glsl,
                   const ConfigurationBase &config_base):
  PainterBackend(glyph_atlas, image_atlas, colorstop_atlas, config_base,
                 detail::ShaderSetCreator(config_glsl.default_blend_shader_type(),
                                          config_glsl.non_dashed_stroke_shader_uses_discard())
                 .create_shader_set())
{
  m_d = FASTUIDRAWnew PainterBackendGLSLPrivate(this, config_glsl);
  set_hints().clipping_via_hw_clip_planes(config_glsl.use_hw_clip_planes());
}

fastuidraw::glsl::PainterBackendGLSL::
~PainterBackendGLSL()
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::glsl::PainterBackendGLSL::
add_vertex_shader_util(const ShaderSource &src)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  d->m_vert_shader_utils.add_source(src);
}

void
fastuidraw::glsl::PainterBackendGLSL::
add_fragment_shader_util(const ShaderSource &src)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  d->m_frag_shader_utils.add_source(src);
}

uint32_t
fastuidraw::glsl::PainterBackendGLSL::
compute_item_shader_group(PainterShader::Tag tag,
                          const reference_counted_ptr<PainterItemShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

uint32_t
fastuidraw::glsl::PainterBackendGLSL::
compute_blend_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterBlendShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterBackendGLSL::
absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);

  reference_counted_ptr<glsl::PainterItemShaderGLSL> h;
  PainterShader::Tag return_value;

  assert(!shader->parent());
  assert(shader.dynamic_cast_ptr<PainterItemShaderGLSL>());
  h = shader.static_cast_ptr<PainterItemShaderGLSL>();

  d->m_shader_code_added = true;
  d->m_item_shaders.push_back(h);
  d->update_varying_size(h->varyings());

  return_value.m_ID = d->m_next_item_shader_ID;
  return_value.m_group = 0;
  d->m_next_item_shader_ID += h->number_sub_shaders();
  return_value.m_group = compute_item_shader_group(return_value, shader);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterBackendGLSL::
compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag());
  tg.m_ID += shader->sub_shader();
  return compute_item_shader_group(tg, shader);
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterBackendGLSL::
absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);

  reference_counted_ptr<PainterBlendShaderGLSL> h;
  fastuidraw::PainterShader::Tag return_value;

  assert(!shader->parent());
  assert(shader.dynamic_cast_ptr<PainterBlendShaderGLSL>());
  h = shader.static_cast_ptr<PainterBlendShaderGLSL>();

  d->m_shader_code_added = true;
  d->m_blend_shaders[h->type()].m_shaders.push_back(h);

  return_value.m_ID = d->m_next_blend_shader_ID;
  return_value.m_group = 0;
  d->m_next_blend_shader_ID += h->number_sub_shaders();
  return_value.m_group = compute_blend_shader_group(return_value, h);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterBackendGLSL::
compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag());
  tg.m_ID += shader->sub_shader();
  return compute_blend_shader_group(tg, shader);
}

void
fastuidraw::glsl::PainterBackendGLSL::
target_resolution(int w, int h)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);

  w = std::max(1, w);
  h = std::max(1, h);

  d->m_target_resolution = vec2(w, h);
  d->m_target_resolution_recip = vec2(1.0f, 1.0f) / vec2(d->m_target_resolution);
  d->m_target_resolution_recip_magnitude = d->m_target_resolution_recip.magnitude();
}

bool
fastuidraw::glsl::PainterBackendGLSL::
shader_code_added(void)
{
  bool return_value;
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  return_value = d->m_shader_code_added;
  d->m_shader_code_added = false;
  return return_value;
}

const fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL&
fastuidraw::glsl::PainterBackendGLSL::
configuration_glsl(void) const
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  return d->m_config;
}

void
fastuidraw::glsl::PainterBackendGLSL::
construct_shader(ShaderSource &out_vertex,
                 ShaderSource &out_fragment,
                 const UberShaderParams &construct_params,
                 const ItemShaderFilter *item_shader_filter,
                 const char *discard_macro_value)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  d->construct_shader(out_vertex, out_fragment, construct_params,
                      item_shader_filter, discard_macro_value);
}

uint32_t
fastuidraw::glsl::PainterBackendGLSL::
ubo_size(void)
{
  return round_up_to_multiple(uniform_ubo_number_entries, 4);
}

void
fastuidraw::glsl::PainterBackendGLSL::
fill_uniform_buffer(c_array<generic_data> p)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  p[uniform_ubo_resolution_x_offset].f = d->m_target_resolution.x();
  p[uniform_ubo_resolution_y_offset].f = d->m_target_resolution.y();
  p[uniform_ubo_recip_resolution_x_offset].f = d->m_target_resolution_recip.x();
  p[uniform_ubo_recip_resolution_y_offset].f = d->m_target_resolution_recip.y();
  p[uniform_ubo_recip_magnitude_offset].f = d->m_target_resolution_recip_magnitude;
}
