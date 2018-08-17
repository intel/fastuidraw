/*!
 * \file painter_shader_registrar_glsl.cpp
 * \brief file painter_shader_registrar_glsl.cpp
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
#include <algorithm>

#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/painter/stroked_point.hpp>
#include <fastuidraw/painter/arc_stroked_point.hpp>
#include <fastuidraw/painter/painter_header.hpp>
#include <fastuidraw/painter/painter_item_matrix.hpp>
#include <fastuidraw/painter/painter_clip_equations.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/painter_shader_data.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>
#include <fastuidraw/painter/painter_stroke_params.hpp>
#include <fastuidraw/glsl/painter_composite_shader_glsl.hpp>
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

  class CompositeShaderGroup
  {
  public:
    typedef fastuidraw::glsl::PainterCompositeShaderGLSL Shader;
    typedef fastuidraw::reference_counted_ptr<Shader> Ref;
    std::vector<Ref> m_shaders;
  };

  class BackendConstantsPrivate
  {
  public:
    BackendConstantsPrivate(void):
      m_data_store_alignment(0),
      m_glyph_atlas_geometry_store_alignment(0),
      m_glyph_atlas_texel_store_width(0),
      m_glyph_atlas_texel_store_height(0),
      m_image_atlas_color_store_width(0),
      m_image_atlas_color_store_height(0),
      m_image_atlas_index_tile_size(0),
      m_image_atlas_color_tile_size(0),
      m_colorstop_atlas_store_width(0)
    {}

    int m_data_store_alignment;
    int m_glyph_atlas_geometry_store_alignment;
    int m_glyph_atlas_texel_store_width;
    int m_glyph_atlas_texel_store_height;
    int m_image_atlas_color_store_width;
    int m_image_atlas_color_store_height;
    int m_image_atlas_index_tile_size;
    int m_image_atlas_color_tile_size;
    int m_colorstop_atlas_store_width;
  };

  class BindingPointsPrivate
  {
  public:
    BindingPointsPrivate(void):
      m_colorstop_atlas(0),
      m_image_atlas_color_tiles_nearest(1),
      m_image_atlas_color_tiles_linear(2),
      m_image_atlas_index_tiles(3),
      m_glyph_atlas_texel_store_uint(4),
      m_glyph_atlas_texel_store_float(5),
      m_glyph_atlas_geometry_store_texture(6),
      m_data_store_buffer_tbo(7),
      m_external_texture(8),
      m_data_store_buffer_ubo(0),
      m_uniforms_ubo(1),
      m_glyph_atlas_geometry_store_ssbo(0),
      m_data_store_buffer_ssbo(1),
      m_auxiliary_image_buffer(0),
      m_color_interlock_image_buffer(1)
    {}

    // texture units
    unsigned int m_colorstop_atlas;
    unsigned int m_image_atlas_color_tiles_nearest;
    unsigned int m_image_atlas_color_tiles_linear;
    unsigned int m_image_atlas_index_tiles;
    unsigned int m_glyph_atlas_texel_store_uint;
    unsigned int m_glyph_atlas_texel_store_float;
    unsigned int m_glyph_atlas_geometry_store_texture;
    unsigned int m_data_store_buffer_tbo;
    unsigned int m_external_texture;

    // UBO units
    unsigned int m_data_store_buffer_ubo;
    unsigned int m_uniforms_ubo;

    // SSBO units
    unsigned int m_glyph_atlas_geometry_store_ssbo;
    unsigned int m_data_store_buffer_ssbo;

    // image units
    unsigned int m_auxiliary_image_buffer;
    unsigned int m_color_interlock_image_buffer;
  };

  class UberShaderParamsPrivate
  {
  public:
    UberShaderParamsPrivate(void):
      m_compositing_type(fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_dual_src),
      m_supports_bindless_texturing(false),
      m_clipping_type(fastuidraw::glsl::PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance),
      m_z_coordinate_convention(fastuidraw::glsl::PainterShaderRegistrarGLSL::z_minus_1_to_1),
      m_negate_normalized_y_coordinate(false),
      m_assign_layout_to_vertex_shader_inputs(true),
      m_assign_layout_to_varyings(true),
      m_assign_binding_points(true),
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_composite_shader_use_switch(false),
      m_blend_shader_use_switch(false),
      m_unpack_header_and_brush_in_frag_shader(false),
      m_data_store_backing(fastuidraw::glsl::PainterShaderRegistrarGLSL::data_store_tbo),
      m_data_blocks_per_store_buffer(-1),
      m_glyph_geometry_backing(fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_tbo),
      m_glyph_geometry_backing_log2_dims(-1, -1),
      m_have_float_glyph_texture_atlas(true),
      m_colorstop_atlas_backing(fastuidraw::glsl::PainterShaderRegistrarGLSL::colorstop_texture_1d_array),
      m_use_ubo_for_uniforms(true),
      m_provide_auxiliary_image_buffer(fastuidraw::glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer),
      m_use_uvec2_for_bindless_handle(true)
    {}

    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_type_t m_compositing_type;
    bool m_supports_bindless_texturing;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::clipping_type_t m_clipping_type;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::z_coordinate_convention_t m_z_coordinate_convention;
    bool m_negate_normalized_y_coordinate;
    bool m_assign_layout_to_vertex_shader_inputs;
    bool m_assign_layout_to_varyings;
    bool m_assign_binding_points;
    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_composite_shader_use_switch;
    bool m_blend_shader_use_switch;
    bool m_unpack_header_and_brush_in_frag_shader;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::data_store_backing_t m_data_store_backing;
    int m_data_blocks_per_store_buffer;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_backing_t m_glyph_geometry_backing;
    fastuidraw::ivec2 m_glyph_geometry_backing_log2_dims;
    bool m_have_float_glyph_texture_atlas;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::colorstop_backing_t m_colorstop_atlas_backing;
    bool m_use_ubo_for_uniforms;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_t m_provide_auxiliary_image_buffer;
    fastuidraw::glsl::PainterShaderRegistrarGLSL::BindingPoints m_binding_points;
    bool m_use_uvec2_for_bindless_handle;
  };

  class PainterShaderRegistrarGLSLPrivate
  {
  public:
    explicit
    PainterShaderRegistrarGLSLPrivate(void);
    ~PainterShaderRegistrarGLSLPrivate();

    void
    construct_shader(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &constants,
                     fastuidraw::glsl::ShaderSource &out_vertex,
                     fastuidraw::glsl::ShaderSource &out_fragment,
                     const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &contruct_params,
                     const fastuidraw::glsl::PainterShaderRegistrarGLSL::ItemShaderFilter *item_shader_filter,
                     fastuidraw::c_string discard_macro_value);

    void
    update_varying_size(const fastuidraw::glsl::varying_list &plist);

    std::string
    declare_shader_uniforms(const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params);

    void
    ready_main_varyings(void);

    void
    ready_brush_varyings(void);

    void
    add_enums(fastuidraw::glsl::ShaderSource &src);

    void
    add_backend_constants(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &constants,
                          fastuidraw::glsl::ShaderSource &src);

    void
    stream_unpack_code(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &constants,
                       fastuidraw::glsl::ShaderSource &src);

    enum fastuidraw::PainterCompositeShader::shader_type m_composite_type;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> > m_item_shaders;
    unsigned int m_next_item_shader_ID;
    fastuidraw::vecN<CompositeShaderGroup, fastuidraw::PainterCompositeShader::number_types> m_composite_shaders;
    unsigned int m_next_composite_shader_ID;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterBlendShaderGLSL> > m_blend_shaders;
    unsigned int m_next_blend_shader_ID;
    fastuidraw::glsl::ShaderSource m_constant_code;
    fastuidraw::glsl::ShaderSource m_vert_shader_utils;
    fastuidraw::glsl::ShaderSource m_frag_shader_utils;

    fastuidraw::vecN<size_t, fastuidraw::glsl::varying_list::interpolation_number_types> m_number_float_varyings;
    size_t m_number_uint_varyings;
    size_t m_number_int_varyings;

    fastuidraw::glsl::varying_list m_main_varyings_header_only;
    fastuidraw::glsl::varying_list m_main_varyings_shaders_and_shader_datas;
    fastuidraw::glsl::varying_list m_clip_varyings;
    fastuidraw::glsl::varying_list m_brush_varyings;
  };
}

/////////////////////////////////////
// PainterShaderRegistrarGLSLPrivate methods
PainterShaderRegistrarGLSLPrivate::
PainterShaderRegistrarGLSLPrivate(void):
  m_next_item_shader_ID(1),
  m_next_composite_shader_ID(1),
  m_next_blend_shader_ID(1),
  m_number_float_varyings(0),
  m_number_uint_varyings(0),
  m_number_int_varyings(0)
{
  /* add varyings needed by fastuidraw_painter_main
   */
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  ready_main_varyings();
  ready_brush_varyings();

  add_enums(m_constant_code);

  m_vert_shader_utils
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_anisotropic.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_unpack_unit_vector.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_compute_local_distance_from_pixel_distance.glsl.resource_string",
                ShaderSource::from_resource)
    .add_source("fastuidraw_align.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_stroke_util.constants.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_stroke_util.vert.glsl.resource_string", ShaderSource::from_resource);

  m_frag_shader_utils
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_anisotropic.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_macro("FASTUIDRAW_PORTER_DUFF_MACRO(src_factor, dst_factor)", "( (src_factor) * in_src + (dst_factor) * in_fb )")
    .add_source("fastuidraw_painter_stroke_util.constants.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_stroke_util.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_blend_util.frag.glsl.resource_string", ShaderSource::from_resource);
}

PainterShaderRegistrarGLSLPrivate::
~PainterShaderRegistrarGLSLPrivate()
{
}

void
PainterShaderRegistrarGLSLPrivate::
ready_main_varyings(void)
{
  m_main_varyings_header_only
    .add_uint_varying("fastuidraw_header_varying")
    .add_float_varying("fastuidraw_brush_p_x")
    .add_float_varying("fastuidraw_brush_p_y");

  m_main_varyings_shaders_and_shader_datas
    .add_uint_varying("fastuidraw_frag_shader")
    .add_uint_varying("fastuidraw_frag_shader_data_location")
    .add_uint_varying("fastuidraw_composite_shader")
    .add_uint_varying("fastuidraw_composite_shader_data_location")
    .add_uint_varying("fastuidraw_blend_shader")
    .add_uint_varying("fastuidraw_blend_shader_data_location")
    .add_float_varying("fastuidraw_brush_p_x")
    .add_float_varying("fastuidraw_brush_p_y");

  m_clip_varyings
    .add_float_varying("fastuidraw_clip_plane0")
    .add_float_varying("fastuidraw_clip_plane1")
    .add_float_varying("fastuidraw_clip_plane2")
    .add_float_varying("fastuidraw_clip_plane3");
}

void
PainterShaderRegistrarGLSLPrivate::
ready_brush_varyings(void)
{
  using namespace fastuidraw::glsl;

  m_brush_varyings
    /* specifies what features are active on the brush
     * through values on its bits.
     */
    .add_uint_varying("fastuidraw_brush_shader")

    /* Repeat window paremters:
     * - fastuidraw_brush_repeat_window_xy (x,y) coordinate of repeat window
     * - fastuidraw_brush_repeat_window_wh dimensions of repeat window
     * (all in brush coordinate)
     */
    .add_float_varying("fastuidraw_brush_repeat_window_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_w", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_h", varying_list::interpolation_flat)

    /* Gradient paremters (all in brush coordinates)
     * - fastuidraw_brush_gradient_p0 start point of gradient
     * - fastuidraw_brush_gradient_p1 end point of gradient
     * - fastuidraw_brush_gradient_r0 start radius (radial gradients only)
     * - fastuidraw_brush_gradient_r1 end radius (radial gradients only)
     */
    .add_float_varying("fastuidraw_brush_gradient_p0_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p0_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p1_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p1_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_r0", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_r1", varying_list::interpolation_flat)

    /* image parameters
     * - fastuidraw_brush_image_xy (x,y) texel coordinate in INDEX texture
     *                             of start of image
     * - fastuidraw_brush_image_layer layer texel coordinate in INDEX
     *                                texture of start of image
     * - fastuidraw_brush_image_size size of image (needed for when brush
     *                               coordinate goes beyond image size)
     * - fastuidraw_brush_image_factor ratio of master index tile size to
     *                                 dimension of image
     */
    .add_float_varying("fastuidraw_brush_image_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_size_x", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_size_y", varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_factor", varying_list::interpolation_flat)
    .add_uint_varying("fastuidraw_brush_image_layer")
    .add_uint_varying("fastuidraw_brush_image_slack")
    .add_uint_varying("fastuidraw_brush_image_number_index_lookups")

    /* ColorStop paremeters (only active if gradient active)
     * - fastuidraw_brush_color_stop_xy (x,y) texture coordinates of start of color stop
     *                                 sequence
     * - fastuidraw_brush_color_stop_length length of color stop sequence in normalized
     *                                     texture coordinates
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
PainterShaderRegistrarGLSLPrivate::
add_backend_constants(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &backend,
                      fastuidraw::glsl::ShaderSource &src)
{
  using namespace fastuidraw;
  unsigned int alignment;

  alignment = backend.data_store_alignment();

  src
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_SIZE", backend.image_atlas_index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", uint32_log2(backend.image_atlas_index_tile_size()))
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", backend.image_atlas_color_tile_size())
    .add_macro("fastuidraw_glyphTexelStore_size_x", backend.glyph_atlas_texel_store_width())
    .add_macro("fastuidraw_glyphTexelStore_size_y", backend.glyph_atlas_texel_store_height())
    .add_macro("fastuidraw_glyphTexelStore_size",
               "ivec2(fastuidraw_glyphTexelStore_size_x, fastuidraw_glyphTexelStore_size_y)")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_x", "(1.0 / float(fastuidraw_glyphTexelStore_size_x) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_y", "(1.0 / float(fastuidraw_glyphTexelStore_size_y) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal",
               "vec2(fastuidraw_glyphTexelStore_size_reciprocal_x, fastuidraw_glyphTexelStore_size_reciprocal_y)")

    .add_macro("fastuidraw_imageAtlasLinear_size_x", backend.image_atlas_color_store_width())
    .add_macro("fastuidraw_imageAtlasLinear_size_y", backend.image_atlas_color_store_height())
    .add_macro("fastuidraw_imageAtlasLinear_size", "ivec2(fastuidraw_imageAtlasLinear_size_x, fastuidraw_imageAtlasLinear_size_y)")
    .add_macro("fastuidraw_imageAtlasLinear_size_reciprocal_x", "(1.0 / float(fastuidraw_imageAtlasLinear_size_x) )")
    .add_macro("fastuidraw_imageAtlasLinear_size_reciprocal_y", "(1.0 / float(fastuidraw_imageAtlasLinear_size_y) )")
    .add_macro("fastuidraw_imageAtlasLinear_size_reciprocal", "vec2(fastuidraw_imageAtlasLinear_size_reciprocal_x, fastuidraw_imageAtlasLinear_size_reciprocal_y)")

    .add_macro("fastuidraw_colorStopAtlas_size", backend.colorstop_atlas_store_width())
    .add_macro("fastuidraw_colorStopAtlas_size_reciprocal", "(1.0 / float(fastuidraw_colorStopAtlas_size) )")

    /* and finally the data store alingment */
    .add_macro("fastuidraw_data_store_alignment", alignment)

    .add_macro("fastuidraw_shader_pen_num_blocks", number_blocks(alignment, PainterBrush::pen_data_size))
    .add_macro("fastuidraw_shader_image_num_blocks", number_blocks(alignment, PainterBrush::image_data_size))
    .add_macro("fastuidraw_shader_linear_gradient_num_blocks", number_blocks(alignment, PainterBrush::linear_gradient_data_size))
    .add_macro("fastuidraw_shader_radial_gradient_num_blocks", number_blocks(alignment, PainterBrush::radial_gradient_data_size))
    .add_macro("fastuidraw_shader_repeat_window_num_blocks", number_blocks(alignment, PainterBrush::repeat_window_data_size))
    .add_macro("fastuidraw_shader_transformation_matrix_num_blocks",
               number_blocks(alignment, PainterBrush::transformation_matrix_data_size))
    .add_macro("fastuidraw_shader_transformation_translation_num_blocks",
               number_blocks(alignment, PainterBrush::transformation_translation_data_size))
    .add_macro("fastuidraw_stroke_dashed_stroking_params_header_num_blocks",
               number_blocks(alignment, PainterDashedStrokeParams::stroke_static_data_size));
}

void
PainterShaderRegistrarGLSLPrivate::
add_enums(fastuidraw::glsl::ShaderSource &src)
{
  using namespace fastuidraw;
  using namespace fastuidraw::PainterEnums;

  /* fp32 can store a 24-bit integer exactly,
   * however, the operation of converting from
   * uint to normalized fp32 may lose a bit,
   * so 23-bits it is.
   * TODO: go through the requirements of IEEE754,
   * what a compiler of a driver might do and
   * what a GPU does to see how many bits we
   * really have.
   */
  uint32_t z_bits_supported;
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

    .add_macro("fastuidraw_shader_image_type_mask", PainterBrush::image_type_mask)
    .add_macro("fastuidraw_image_type_bit0", PainterBrush::image_type_bit0)
    .add_macro("fastuidraw_image_type_num_bits", PainterBrush::image_type_num_bits)
    .add_macro("fastuidraw_image_type_on_atlas", Image::on_atlas)
    .add_macro("fastuidraw_image_type_bindless_texture2d", Image::bindless_texture2d)
    .add_macro("fastuidraw_image_type_context_texture2d", Image::context_texture2d)

    .add_macro("fastuidraw_image_mipmap_mask", PainterBrush::image_mipmap_mask)
    .add_macro("fastuidraw_image_mipmap_bit0", PainterBrush::image_mipmap_bit0)
    .add_macro("fastuidraw_image_mipmap_num_bits", PainterBrush::image_mipmap_num_bits)

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
    .add_macro("fastuidraw_color_stop_y_num_bits", PainterBrush::gradient_color_stop_y_num_bits);
}

void
PainterShaderRegistrarGLSLPrivate::
stream_unpack_code(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &backend,
                   fastuidraw::glsl::ShaderSource &str)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;

  unsigned int alignment;
  alignment = backend.data_store_alignment();

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
     * one sees the transposing to the loads
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
      .set(PainterBrush::image_slack_number_lookups_offset, ".image_slack_number_lookups", shader_unpack_value::uint_type)
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
      .set(PainterHeader::composite_shader_data_location_offset, ".composite_shader_data_location", shader_unpack_value::uint_type)
      .set(PainterHeader::blend_shader_data_location_offset, ".blend_shader_data_location", shader_unpack_value::uint_type)
      .set(PainterHeader::brush_shader_offset, ".brush_shader", shader_unpack_value::uint_type)
      .set(PainterHeader::z_offset, ".z", shader_unpack_value::int_type)
      .set(PainterHeader::item_shader_offset, ".item_shader", shader_unpack_value::uint_type)
      .set(PainterHeader::composite_shader_offset, ".composite_shader", shader_unpack_value::uint_type)
      .set(PainterHeader::blend_shader_offset, ".blend_shader", shader_unpack_value::uint_type)
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
     * one sees the transposing to the loads
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
      .set(PainterDashedStrokeParams::stroke_first_interval_start_on_looping_offset, ".first_interval_start_on_looping")
      .set(PainterDashedStrokeParams::stroke_number_intervals_offset, ".number_intervals", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_dashed_stroking_params_header",
                              "fastuidraw_dashed_stroking_params_header",
                              true);
  }
}

void
PainterShaderRegistrarGLSLPrivate::
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
PainterShaderRegistrarGLSLPrivate::
declare_shader_uniforms(const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  std::ostringstream ostr;

  if (params.use_ubo_for_uniforms())
    {
      c_string ext="xyzw";
      /* Mesa packs UBO data float[N] as really vec4[N],
       * so instead realize the data directly as vec4[K]
       */
      ostr << "FASTUIDRAW_LAYOUT_BINDING("
           << params.binding_points().uniforms_ubo()
           << ") " << " uniform fastuidraw_uniform_block {\n"
           << "vec4 fastuidraw_shader_uniforms["
           << PainterShaderRegistrarGLSL::ubo_size() / 4 << "];\n"
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
           << PainterShaderRegistrarGLSL::ubo_size() << "];\n"
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
PainterShaderRegistrarGLSLPrivate::
construct_shader(const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants &backend,
                 fastuidraw::glsl::ShaderSource &vert,
                 fastuidraw::glsl::ShaderSource &frag,
                 const fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams &params,
                 const fastuidraw::glsl::PainterShaderRegistrarGLSL::ItemShaderFilter *item_shader_filter,
                 fastuidraw::c_string discard_macro_value)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;

  std::string varying_layout_macro, binding_layout_macro;
  std::string declare_varyings;
  UberShaderVaryings uber_shader_varyings;
  AliasVaryingLocation main_varying_datum, brush_varying_datum;
  AliasVaryingLocation clip_varying_datum, shader_varying_datum;
  std::string declare_vertex_shader_ins;
  std::string declare_uniforms;
  const varying_list *main_varyings;
  const PainterShaderRegistrarGLSL::BindingPoints &binding_params(params.binding_points());
  std::vector<reference_counted_ptr<PainterItemShaderGLSL> > work_shaders;
  c_array<const reference_counted_ptr<PainterItemShaderGLSL> > item_shaders;
  enum PainterCompositeShader::shader_type composite_type;

  composite_type = params.composite_type();
  if (item_shader_filter)
    {
      for(const auto &sh : m_item_shaders)
        {
          if (item_shader_filter->use_shader(sh))
            {
              work_shaders.push_back(sh);
            }
        }
      item_shaders = make_c_array(work_shaders);
    }
  else
    {
      item_shaders = make_c_array(m_item_shaders);
    }

  if (params.assign_layout_to_vertex_shader_inputs())
    {
      std::ostringstream ostr;
      ostr << "layout(location = " << PainterShaderRegistrarGLSL::primary_attrib_slot
           << ") in uvec4 fastuidraw_primary_attribute;\n"
           << "layout(location = " << PainterShaderRegistrarGLSL::secondary_attrib_slot
           << ") in uvec4 fastuidraw_secondary_attribute;\n"
           << "layout(location = " << PainterShaderRegistrarGLSL::uint_attrib_slot
           << ") in uvec4 fastuidraw_uint_attribute;\n"
           << "layout(location = " << PainterShaderRegistrarGLSL::header_attrib_slot
           << ") in uint fastuidraw_header_attribute;\n";
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

  if (params.assign_layout_to_varyings())
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_VARYING(X) layout(location = X)\n";
      varying_layout_macro = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_VARYING(X)\n";
      varying_layout_macro = ostr.str();
    }

  if (params.assign_binding_points())
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_BINDING(X) layout(binding = X)\n"
           << "#define FASTUIDRAW_LAYOUT_BINDING_ARGS(X, Y) layout(binding = X, Y)\n";
      binding_layout_macro = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "#define FASTUIDRAW_LAYOUT_BINDING(X)\n"
           << "#define FASTUIDRAW_LAYOUT_BINDING_ARGS(X, Y) layout(Y)\n";
      binding_layout_macro = ostr.str();
    }

  if (params.clipping_type() != PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
    {
      uber_shader_varyings.add_varyings("clip", m_clip_varyings, &clip_varying_datum);
    }

  if (params.unpack_header_and_brush_in_frag_shader())
    {
      main_varyings = &m_main_varyings_header_only;
    }
  else
    {
      main_varyings = &m_main_varyings_shaders_and_shader_datas;
      uber_shader_varyings.add_varyings("brush", m_brush_varyings, &brush_varying_datum);
    }

  uber_shader_varyings.add_varyings("main", *main_varyings, &main_varying_datum);

  uber_shader_varyings.add_varyings("shader",
                                    m_number_uint_varyings,
                                    m_number_int_varyings,
                                    m_number_float_varyings,
                                    &shader_varying_datum);

  declare_uniforms = declare_shader_uniforms(params);
  declare_varyings = uber_shader_varyings.declare_varyings("fastuidraw_varying");

  if (params.unpack_header_and_brush_in_frag_shader())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
      frag.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
    }

  if (params.negate_normalized_y_coordinate())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NEGATE_POSITION_Y_COORDINATE");
      frag.add_macro("FASTUIDRAW_PAINTER_NEGATE_POSITION_Y_COORDINATE");
    }

  if (params.z_coordinate_convention() == PainterShaderRegistrarGLSL::z_minus_1_to_1)
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_Z_MINUS_1_TO_1");
      frag.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_Z_MINUS_1_TO_1");
    }
  else
    {
      vert.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_0_TO_1");
      frag.add_macro("FASTUIDRAW_PAINTER_NORMALIZED_0_TO_1");
    }

  switch(params.clipping_type())
    {
    case PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance:
      vert.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_GL_CLIP_DISTACE");
      break;

    case PainterShaderRegistrarGLSL::clipping_via_discard:
      frag.add_macro("FASTUIDRAW_PAINTER_CLIPPING_USE_DISCARD");
      break;

    case PainterShaderRegistrarGLSL::clipping_via_skip_color_write:
      FASTUIDRAWassert(composite_type == PainterCompositeShader::framebuffer_fetch);
      frag.add_macro("FASTUIDRAW_PAINTER_CLIPPING_SKIP_COLOR_WRITE");
      break;
    }

  if (params.supports_bindless_texturing())
    {
      vert.add_macro("FASTUIDRAW_SUPPORT_BINDLESS_TEXTURE");
      frag.add_macro("FASTUIDRAW_SUPPORT_BINDLESS_TEXTURE");
    }

  if (params.use_uvec2_for_bindless_handle())
    {
      vert.add_macro("FASTUIDRAW_BINDLESS_HANDLE_UVEC2");
      frag.add_macro("FASTUIDRAW_BINDLESS_HANDLE_UVEC2");
    }

  switch(params.colorstop_atlas_backing())
    {
    case PainterShaderRegistrarGLSL::colorstop_texture_1d_array:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_1D_ARRAY");
        frag.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_1D_ARRAY");
      }
    break;

    case PainterShaderRegistrarGLSL::colorstop_texture_2d_array:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_2D_ARRAY");
        frag.add_macro("FASTUIDRAW_PAINTER_COLORSTOP_ATLAS_2D_ARRAY");
      }
    break;

    default:
      FASTUIDRAWassert(!"Invalid colorstop_atlas_backing() value");
    }

  switch(params.data_store_backing())
    {
    case PainterShaderRegistrarGLSL::data_store_ubo:
      {
        FASTUIDRAWassert(backend.data_store_alignment() == 4);

        vert
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer());

        frag
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer());
      }
      break;

    case PainterShaderRegistrarGLSL::data_store_tbo:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
        frag.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
      }
      break;

    case PainterShaderRegistrarGLSL::data_store_ssbo:
      {
        FASTUIDRAWassert(backend.data_store_alignment() == 4);

        vert.add_macro("FASTUIDRAW_PAINTER_USE_DATA_SSBO");
        frag.add_macro("FASTUIDRAW_PAINTER_USE_DATA_SSBO");
      }
      break;

    default:
      FASTUIDRAWassert(!"Invalid data_store_backing() value");
    }

  if (!params.have_float_glyph_texture_atlas())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
      frag.add_macro("FASTUIDRAW_PAINTER_EMULATE_GLYPH_TEXEL_STORE_FLOAT");
    }

  switch(params.glyph_geometry_backing())
    {
    case PainterShaderRegistrarGLSL::glyph_geometry_texture_array:
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

    case PainterShaderRegistrarGLSL::glyph_geometry_tbo:
      {
        vert.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
        frag.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_TEXTURE_BUFFER");
      }
      break;

    case PainterShaderRegistrarGLSL::glyph_geometry_ssbo:
      {
        vert.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_SSBO");
        frag.add_macro("FASTUIDRAW_GLYPH_DATA_STORE_SSBO");
      }
      break;
    }

  switch(params.provide_auxiliary_image_buffer())
    {
    case PainterShaderRegistrarGLSL::auxiliary_buffer_atomic:
      frag.add_macro("FASTUIDRAW_PAINTER_AUXILIARY_BUFFER_ATOMIC");
      frag.add_macro("FASTUIDRAW_PAINTER_HAVE_AUXILIARY_BUFFER");
      break;

    case PainterShaderRegistrarGLSL::auxiliary_buffer_interlock:
      frag.add_macro("FASTUIDRAW_PAINTER_AUXILIARY_BUFFER_INTERLOCK");
      frag.add_macro("FASTUIDRAW_PAINTER_HAVE_AUXILIARY_BUFFER");
      break;
      break;

    case PainterShaderRegistrarGLSL::auxiliary_buffer_interlock_main_only:
      frag.add_macro("FASTUIDRAW_PAINTER_AUXILIARY_BUFFER_INTERLOCK_MAIN_ONLY");
      frag.add_macro("FASTUIDRAW_PAINTER_HAVE_AUXILIARY_BUFFER");
      break;

    case PainterShaderRegistrarGLSL::auxiliary_buffer_framebuffer_fetch:
      frag.add_macro("FASTUIDRAW_PAINTER_AUXILIARY_BUFFER_FRAMEBUFFER_FETCH");
      frag.add_macro("FASTUIDRAW_PAINTER_HAVE_AUXILIARY_BUFFER");
      break;

    default:
      break;
    }

  add_backend_constants(backend, vert);
  vert
    .add_source(m_constant_code)
    .add_source(varying_layout_macro.c_str(), ShaderSource::from_string)
    .add_source(binding_layout_macro.c_str(), ShaderSource::from_string)
    .add_macro("FASTUIDRAW_COLORSTOP_ATLAS_BINDING", binding_params.colorstop_atlas())
    .add_macro("FASTUIDRAW_COLOR_TILE_LINEAR_BINDING", binding_params.image_atlas_color_tiles_linear())
    .add_macro("FASTUIDRAW_COLOR_TILE_NEAREST_BINDING", binding_params.image_atlas_color_tiles_nearest())
    .add_macro("FASTUIDRAW_INDEX_TILE_BINDING", binding_params.image_atlas_index_tiles())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_UINT_BINDING", binding_params.glyph_atlas_texel_store_uint())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_FLOAT_BINDING", binding_params.glyph_atlas_texel_store_float())
    .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_STORE_BINDING", binding_params.glyph_atlas_geometry_store(params.glyph_geometry_backing()))
    .add_macro("FASTUIDRAW_PAINTER_STORE_TBO_BINDING", binding_params.data_store_buffer_tbo())
    .add_macro("FASTUIDRAW_PAINTER_STORE_UBO_BINDING", binding_params.data_store_buffer_ubo())
    .add_macro("FASTUIDRAW_PAINTER_STORE_SSBO_BINDING", binding_params.data_store_buffer_ssbo())
    .add_macro("FASTUIDRAW_PAINTER_AUXILIARY_BUFFER_BINDING", binding_params.auxiliary_image_buffer())
    .add_macro("FASTUIDRAW_PAINTER_BLEND_INTERLOCK_BINDING", binding_params.color_interlock_image_buffer())
    .add_macro("FASTUIDRAW_PAINTER_EXTERNAL_TEXTURE_BINDING", binding_params.external_texture())
    .add_macro("fastuidraw_varying", "out")
    .add_source(declare_vertex_shader_ins.c_str(), ShaderSource::from_string)
    .add_source(declare_varyings.c_str(), ShaderSource::from_string);

  if (params.clipping_type() != PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
    {
      uber_shader_varyings.stream_alias_varyings(vert, m_clip_varyings, true, clip_varying_datum);
    }
  uber_shader_varyings.stream_alias_varyings(vert, *main_varyings, true, main_varying_datum);

  if (params.unpack_header_and_brush_in_frag_shader())
    {
      /* we need to declare the values named in m_brush_varyings */
      stream_as_local_variables(vert, m_brush_varyings);
    }
  else
    {
      uber_shader_varyings.stream_alias_varyings(vert, m_brush_varyings, true, brush_varying_datum);
    }

  vert
    .add_source(declare_uniforms.c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_globals.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(code::compute_interval("fastuidraw_compute_interval", backend.data_store_alignment()))
    .add_source(m_vert_shader_utils)
    .add_source("fastuidraw_painter_main.vert.glsl.resource_string", ShaderSource::from_resource);

  stream_unpack_code(backend, vert);
  stream_uber_vert_shader(params.vert_shader_use_switch(), vert, item_shaders,
                          uber_shader_varyings, shader_varying_datum);

  bool blending_supported(false);
  c_string shader_composite_macro;
  switch(params.compositing_type())
    {
    case PainterShaderRegistrarGLSL::compositing_framebuffer_fetch:
      shader_composite_macro = "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH";
      blending_supported = true;
      break;

    case PainterShaderRegistrarGLSL::compositing_interlock:
      shader_composite_macro = "FASTUIDRAW_PAINTER_BLEND_INTERLOCK";
      blending_supported = true;
      break;

    case PainterShaderRegistrarGLSL::compositing_dual_src:
      shader_composite_macro = "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND";
      break;

    case PainterShaderRegistrarGLSL::compositing_single_src:
      shader_composite_macro = "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND";
      break;

    default:
      shader_composite_macro = "FASTUIDRAW_PAINTER_BLEND_INVALID_BLEND";
      FASTUIDRAWassert(!"Invalid composite_type");
    }

  add_backend_constants(backend, frag);
  frag
    .add_source(m_constant_code)
    .add_source(varying_layout_macro.c_str(), ShaderSource::from_string)
    .add_source(binding_layout_macro.c_str(), ShaderSource::from_string)
    .add_macro("FASTUIDRAW_DISCARD", discard_macro_value)
    .add_macro(shader_composite_macro)
    .add_macro("FASTUIDRAW_COLORSTOP_ATLAS_BINDING", binding_params.colorstop_atlas())
    .add_macro("FASTUIDRAW_COLOR_TILE_LINEAR_BINDING", binding_params.image_atlas_color_tiles_linear())
    .add_macro("FASTUIDRAW_COLOR_TILE_NEAREST_BINDING", binding_params.image_atlas_color_tiles_nearest())
    .add_macro("FASTUIDRAW_INDEX_TILE_BINDING", binding_params.image_atlas_index_tiles())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_UINT_BINDING", binding_params.glyph_atlas_texel_store_uint())
    .add_macro("FASTUIDRAW_GLYPH_TEXEL_ATLAS_FLOAT_BINDING", binding_params.glyph_atlas_texel_store_float())
    .add_macro("FASTUIDRAW_GLYPH_GEOMETRY_STORE_BINDING", binding_params.glyph_atlas_geometry_store(params.glyph_geometry_backing()))
    .add_macro("FASTUIDRAW_PAINTER_STORE_TBO_BINDING", binding_params.data_store_buffer_tbo())
    .add_macro("FASTUIDRAW_PAINTER_STORE_UBO_BINDING", binding_params.data_store_buffer_ubo())
    .add_macro("FASTUIDRAW_PAINTER_STORE_SSBO_BINDING", binding_params.data_store_buffer_ssbo())
    .add_macro("FASTUIDRAW_PAINTER_AUXILIARY_BUFFER_BINDING", binding_params.auxiliary_image_buffer())
    .add_macro("FASTUIDRAW_PAINTER_BLEND_INTERLOCK_BINDING", binding_params.color_interlock_image_buffer())
    .add_macro("FASTUIDRAW_PAINTER_EXTERNAL_TEXTURE_BINDING", binding_params.external_texture())
    .add_macro("fastuidraw_varying", "in")
    .add_source(declare_varyings.c_str(), ShaderSource::from_string);

  if (params.clipping_type() != PainterShaderRegistrarGLSL::clipping_via_gl_clip_distance)
    {
      uber_shader_varyings.stream_alias_varyings(frag, m_clip_varyings, true, clip_varying_datum);
    }
  uber_shader_varyings.stream_alias_varyings(frag, *main_varyings, true, main_varying_datum);
  if (params.unpack_header_and_brush_in_frag_shader())
    {
      /* we need to declare the values named in m_brush_varyings */
      stream_as_local_variables(frag, m_brush_varyings);
    }
  else
    {
      uber_shader_varyings.stream_alias_varyings(frag, m_brush_varyings, true, brush_varying_datum);
    }

  frag
    .add_source(declare_uniforms.c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_painter_globals.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_auxiliary_image_buffer.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.frag.glsl.resource_string", ShaderSource::from_resource);

  if (params.unpack_header_and_brush_in_frag_shader())
    {
      frag
        .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", ShaderSource::from_resource)
        .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", ShaderSource::from_resource);
    }

  frag
    .add_source("fastuidraw_painter_brush.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source(code::compute_interval("fastuidraw_compute_interval", backend.data_store_alignment()))
    .add_source(m_frag_shader_utils)
    .add_source(code::image_atlas_compute_coord("fastuidraw_compute_image_atlas_coord",
                                                "fastuidraw_imageIndexAtlas",
                                                backend.image_atlas_index_tile_size(),
                                                backend.image_atlas_color_tile_size()))
    .add_source(code::curvepair_compute_pseudo_distance(backend.glyph_atlas_geometry_store_alignment(),
                                                        "fastuidraw_curvepair_pseudo_distance",
                                                        "fastuidraw_fetch_glyph_data",
                                                        false))
    .add_source(code::curvepair_compute_pseudo_distance(backend.glyph_atlas_geometry_store_alignment(),
                                                        "fastuidraw_curvepair_pseudo_distance",
                                                        "fastuidraw_fetch_glyph_data",
                                                        true))
    .add_source("fastuidraw_painter_main.frag.glsl.resource_string", ShaderSource::from_resource);

  stream_unpack_code(backend, frag);
  stream_uber_frag_shader(params.frag_shader_use_switch(), frag, item_shaders,
                          uber_shader_varyings, shader_varying_datum);
  stream_uber_composite_shader(params.composite_shader_use_switch(), frag,
                               make_c_array(m_composite_shaders[composite_type].m_shaders),
                               composite_type);

  if (blending_supported)
    {
      stream_uber_blend_shader(params.blend_shader_use_switch(), frag,
                               make_c_array(m_blend_shaders));
    }
}

//////////////////////////////////////////////////////
// fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants methods
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
BackendConstants(PainterBackend *p)
{
  m_d = FASTUIDRAWnew BackendConstantsPrivate();
  set_from_backend(p);
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
BackendConstants(const BackendConstants &obj)
{
  BackendConstantsPrivate *d;
  d = static_cast<BackendConstantsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew BackendConstantsPrivate(*d);
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
~BackendConstants()
{
  BackendConstantsPrivate *d;
  d = static_cast<BackendConstantsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants&
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
set_from_backend(PainterBackend *p)
{
  if (p)
    {
      BackendConstantsPrivate *d;
      d = static_cast<BackendConstantsPrivate*>(m_d);

      d->m_data_store_alignment = p->configuration_base().alignment();
      set_from_atlas(p->glyph_atlas());
      set_from_atlas(p->image_atlas());
      set_from_atlas(p->colorstop_atlas());
    }
  return *this;
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants&
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
set_from_atlas(const reference_counted_ptr<GlyphAtlas> &p)
{
  if (p)
    {
      BackendConstantsPrivate *d;
      d = static_cast<BackendConstantsPrivate*>(m_d);
      d->m_glyph_atlas_texel_store_width = p->texel_store()->dimensions().x();
      d->m_glyph_atlas_texel_store_height = p->texel_store()->dimensions().y();
      d->m_glyph_atlas_geometry_store_alignment = p->geometry_store()->alignment();
    }
  return *this;
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants&
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
set_from_atlas(const reference_counted_ptr<ImageAtlas> &p)
{
  if (p)
    {
      BackendConstantsPrivate *d;
      d = static_cast<BackendConstantsPrivate*>(m_d);
      d->m_image_atlas_color_store_width = p->color_store()->dimensions().x();
      d->m_image_atlas_color_store_height = p->color_store()->dimensions().y();
      d->m_image_atlas_index_tile_size = p->index_tile_size();
      d->m_image_atlas_color_tile_size = p->color_tile_size();
    }
  return *this;
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants&
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants::
set_from_atlas(const reference_counted_ptr<ColorStopAtlas> &p)
{
  if (p)
    {
      BackendConstantsPrivate *d;
      d = static_cast<BackendConstantsPrivate*>(m_d);
      d->m_colorstop_atlas_store_width = p->backing_store()->dimensions().x();
    }
  return *this;
}

assign_swap_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants)

setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, data_store_alignment)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, glyph_atlas_geometry_store_alignment)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, glyph_atlas_texel_store_width)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, glyph_atlas_texel_store_height)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_color_store_width)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_color_store_height)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_index_tile_size)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, image_atlas_color_tile_size)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BackendConstants,
                 BackendConstantsPrivate, int, colorstop_atlas_store_width)

/////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints methods
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints::
BindingPoints(void)
{
  m_d = FASTUIDRAWnew BindingPointsPrivate();
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints::
BindingPoints(const BindingPoints &obj)
{
  BindingPointsPrivate *d;
  d = static_cast<BindingPointsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew BindingPointsPrivate(*d);
}

fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints::
~BindingPoints()
{
  BindingPointsPrivate *d;
  d = static_cast<BindingPointsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints)

unsigned int
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints::
glyph_atlas_geometry_store(enum glyph_geometry_backing_t tp) const
{
  return (tp == glyph_geometry_ssbo) ?
    glyph_atlas_geometry_store_ssbo() :
    glyph_atlas_geometry_store_texture();
}

unsigned int
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints::
data_store_buffer(enum data_store_backing_t tp) const
{
  switch (tp)
    {
    case data_store_tbo:
      return data_store_buffer_tbo();

    case data_store_ubo:
      return data_store_buffer_ubo();

    case data_store_ssbo:
      return data_store_buffer_ssbo();
    }

  FASTUIDRAWassert(!"Bad data_store_backing_t value");
  return ~0u;
}

setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, colorstop_atlas)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, image_atlas_color_tiles_linear)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, image_atlas_color_tiles_nearest)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, image_atlas_index_tiles)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, glyph_atlas_texel_store_uint)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, glyph_atlas_texel_store_float)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, glyph_atlas_geometry_store_texture)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, glyph_atlas_geometry_store_ssbo)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, data_store_buffer_tbo)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, data_store_buffer_ubo)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, data_store_buffer_ssbo)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, auxiliary_image_buffer)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, uniforms_ubo)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, color_interlock_image_buffer)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints,
                 BindingPointsPrivate, unsigned int, external_texture)

////////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams methods
fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::
UberShaderParams(void)
{
  m_d = FASTUIDRAWnew UberShaderParamsPrivate();
}

fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::
UberShaderParams(const UberShaderParams &obj)
{
  UberShaderParamsPrivate *d;
  d = static_cast<UberShaderParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew UberShaderParamsPrivate(*d);
}

fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams::
~UberShaderParams()
{
  UberShaderParamsPrivate *d;
  d = static_cast<UberShaderParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

enum fastuidraw::PainterCompositeShader::shader_type
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::UberShaderParams::
composite_type(void) const
{
  return detail::shader_composite_type(compositing_type());
}

fastuidraw::PainterShaderSet
fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::UberShaderParams::
default_shaders(enum PainterStrokeShader::type_t stroke_tp,
                const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass1,
                const reference_counted_ptr<const PainterDraw::Action> &stroke_action_pass2) const
{
  detail::ShaderSetCreator S(composite_type(), stroke_tp,
                             stroke_action_pass1, stroke_action_pass2);
  return S.create_shader_set();
}

assign_swap_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams)

setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate,
                 enum fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_type_t,
                 compositing_type)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, supports_bindless_texturing)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate,
                 enum fastuidraw::glsl::PainterShaderRegistrarGLSL::clipping_type_t,
                 clipping_type)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate,
                 enum fastuidraw::glsl::PainterShaderRegistrarGLSL::z_coordinate_convention_t,
                 z_coordinate_convention)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, negate_normalized_y_coordinate)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, assign_layout_to_vertex_shader_inputs)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, assign_layout_to_varyings)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, assign_binding_points)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, vert_shader_use_switch)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, frag_shader_use_switch)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, composite_shader_use_switch)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, blend_shader_use_switch)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, unpack_header_and_brush_in_frag_shader)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, enum fastuidraw::glsl::PainterShaderRegistrarGLSL::data_store_backing_t, data_store_backing)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, int, data_blocks_per_store_buffer)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, enum fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_backing_t, glyph_geometry_backing)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, fastuidraw::ivec2, glyph_geometry_backing_log2_dims)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, have_float_glyph_texture_atlas)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, enum fastuidraw::glsl::PainterShaderRegistrarGLSL::colorstop_backing_t, colorstop_atlas_backing)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, use_ubo_for_uniforms)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, enum fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_t, provide_auxiliary_image_buffer)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, const fastuidraw::glsl::PainterShaderRegistrarGLSLTypes::BindingPoints&, binding_points)
setget_implement(fastuidraw::glsl::PainterShaderRegistrarGLSL::UberShaderParams,
                 UberShaderParamsPrivate, bool, use_uvec2_for_bindless_handle)

//////////////////////////////////////////////
// fastuidraw::glsl::PainterShaderRegistrarGLSL methods
fastuidraw::glsl::PainterShaderRegistrarGLSL::
PainterShaderRegistrarGLSL(void)
{
  m_d = FASTUIDRAWnew PainterShaderRegistrarGLSLPrivate();
}

fastuidraw::glsl::PainterShaderRegistrarGLSL::
~PainterShaderRegistrarGLSL()
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
add_vertex_shader_util(const ShaderSource &src)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  d->m_vert_shader_utils.add_source(src);
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
add_fragment_shader_util(const ShaderSource &src)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  d->m_frag_shader_utils.add_source(src);
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_item_shader_group(PainterShader::Tag tag,
                          const reference_counted_ptr<PainterItemShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_composite_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterCompositeShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_blend_shader_group(PainterShader::Tag tag,
                           const reference_counted_ptr<PainterBlendShader> &shader)
{
  FASTUIDRAWunused(shader);
  FASTUIDRAWunused(tag);
  return 0u;
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterShaderRegistrarGLSL::
absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);

  reference_counted_ptr<glsl::PainterItemShaderGLSL> h;
  PainterShader::Tag return_value;

  FASTUIDRAWassert(!shader->parent());
  FASTUIDRAWassert(shader.dynamic_cast_ptr<PainterItemShaderGLSL>());
  h = shader.static_cast_ptr<PainterItemShaderGLSL>();

  d->m_item_shaders.push_back(h);
  d->update_varying_size(h->varyings());

  return_value.m_ID = d->m_next_item_shader_ID;
  return_value.m_group = 0;
  d->m_next_item_shader_ID += h->number_sub_shaders();
  return_value.m_group = compute_item_shader_group(return_value, shader);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag());
  tg.m_ID += shader->sub_shader();
  return compute_item_shader_group(tg, shader);
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterShaderRegistrarGLSL::
absorb_composite_shader(const reference_counted_ptr<PainterCompositeShader> &shader)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);

  reference_counted_ptr<PainterCompositeShaderGLSL> h;
  fastuidraw::PainterShader::Tag return_value;

  FASTUIDRAWassert(!shader->parent());
  FASTUIDRAWassert(shader.dynamic_cast_ptr<PainterCompositeShaderGLSL>());
  h = shader.static_cast_ptr<PainterCompositeShaderGLSL>();

  d->m_composite_shaders[h->type()].m_shaders.push_back(h);

  return_value.m_ID = d->m_next_composite_shader_ID;
  return_value.m_group = 0;
  d->m_next_composite_shader_ID += h->number_sub_shaders();
  return_value.m_group = compute_composite_shader_group(return_value, h);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_composite_sub_shader_group(const reference_counted_ptr<PainterCompositeShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag());
  tg.m_ID += shader->sub_shader();
  return compute_composite_shader_group(tg, shader);
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterShaderRegistrarGLSL::
absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);

  reference_counted_ptr<glsl::PainterBlendShaderGLSL> h;
  PainterShader::Tag return_value;

  FASTUIDRAWassert(!shader->parent());
  FASTUIDRAWassert(shader.dynamic_cast_ptr<PainterBlendShaderGLSL>());
  h = shader.static_cast_ptr<PainterBlendShaderGLSL>();

  d->m_blend_shaders.push_back(h);

  return_value.m_ID = d->m_next_blend_shader_ID;
  return_value.m_group = 0;
  d->m_next_blend_shader_ID += h->number_sub_shaders();
  return_value.m_group = compute_blend_shader_group(return_value, shader);

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterShader::Tag tg(shader->parent()->tag());
  tg.m_ID += shader->sub_shader();
  return compute_blend_shader_group(tg, shader);
}

unsigned int
fastuidraw::glsl::PainterShaderRegistrarGLSL::
registered_shader_count(void)
{
  unsigned int return_value;
  PainterShaderRegistrarGLSLPrivate *d;

  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  return_value = d->m_item_shaders.size() + d->m_blend_shaders.size();
  for (unsigned int i = 0; i < PainterCompositeShader::number_types; ++i)
    {
      return_value += d->m_composite_shaders[i].m_shaders.size();
    }

  return return_value;
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
construct_shader(const BackendConstants &backend_constants,
                 ShaderSource &out_vertex,
                 ShaderSource &out_fragment,
                 const UberShaderParams &construct_params,
                 const ItemShaderFilter *item_shader_filter,
                 c_string discard_macro_value)
{
  PainterShaderRegistrarGLSLPrivate *d;
  d = static_cast<PainterShaderRegistrarGLSLPrivate*>(m_d);
  d->construct_shader(backend_constants, out_vertex, out_fragment,
                      construct_params, item_shader_filter,
                      discard_macro_value);
}

uint32_t
fastuidraw::glsl::PainterShaderRegistrarGLSL::
ubo_size(void)
{
  return round_up_to_multiple(uniform_ubo_number_entries, 4);
}

void
fastuidraw::glsl::PainterShaderRegistrarGLSL::
fill_uniform_buffer(const PainterBackend::Surface::Viewport &vwp,
                    c_array<generic_data> p)
{
  int w, h;
  vec2 wh, recip;

  w = t_max(1, vwp.m_dimensions.x());
  h = t_max(1, vwp.m_dimensions.y());

  wh = vec2(w, h);
  recip = vec2(1.0f, 1.0f) / wh;

  p[uniform_ubo_resolution_x_offset].f = wh.x();
  p[uniform_ubo_resolution_y_offset].f = wh.y();
  p[uniform_ubo_recip_resolution_x_offset].f = recip.x();
  p[uniform_ubo_recip_resolution_y_offset].f = recip.y();
  p[uniform_ubo_recip_magnitude_offset].f = recip.magnitude();
}
