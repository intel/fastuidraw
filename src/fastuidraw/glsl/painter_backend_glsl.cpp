
#include <list>
#include <map>
#include <sstream>
#include <vector>

#include <fastuidraw/glsl/painter_backend_glsl.hpp>
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
      m_unique_group_per_item_shader(false),
      m_unique_group_per_blend_shader(false),
      m_use_hw_clip_planes(true),
      m_default_blend_shader_type(fastuidraw::PainterBlendShader::dual_src)
    {}

    bool m_unique_group_per_item_shader;
    bool m_unique_group_per_blend_shader;
    bool m_use_hw_clip_planes;
    enum fastuidraw::PainterBlendShader::shader_type m_default_blend_shader_type;
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
                     const fastuidraw::glsl::PainterBackendGLSL::UberShaderParams &contruct_params);

    void
    update_varying_size(const fastuidraw::glsl::varying_list &plist);

    std::string
    declare_shader_uniforms(const fastuidraw::glsl::PainterBackendGLSL::UberShaderParams &params);

    fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL m_config;

    bool m_shader_code_added;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> > m_item_shaders;
    unsigned int m_next_item_shader_ID;
    fastuidraw::vecN<BlendShaderGroup, fastuidraw::PainterBlendShader::number_types> m_blend_shaders;
    unsigned int m_next_blend_shader_ID;
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
    .add_float_varying("fastuidraw_brush_repeat_window_x", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_y", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_w", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_repeat_window_h", fastuidraw::glsl::varying_list::interpolation_flat)

    /* Gradient paremters (all in brush coordinates)
       - fastuidraw_brush_gradient_p0 start point of gradient
       - fastuidraw_brush_gradient_p1 end point of gradient
       - fastuidraw_brush_gradient_r0 start radius (radial gradients only)
       - fastuidraw_brush_gradient_r1 end radius (radial gradients only)
    */
    .add_float_varying("fastuidraw_brush_gradient_p0_x", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p0_y", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p1_x", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_p1_y", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_r0", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_gradient_r1", fastuidraw::glsl::varying_list::interpolation_flat)

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
    .add_float_varying("fastuidraw_brush_image_x", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_y", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_layer", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_size_x", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_size_y", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_image_factor", fastuidraw::glsl::varying_list::interpolation_flat)

    /* ColorStop paremeters (only active if gradient active)
       - fastuidraw_brush_color_stop_xy (x,y) texture coordinates of start of color stop
                                       sequence
       - fastuidraw_brush_color_stop_length length of color stop sequence in normalized
                                           texture coordinates
    */
    .add_float_varying("fastuidraw_brush_color_stop_x", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_color_stop_y", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_color_stop_length", fastuidraw::glsl::varying_list::interpolation_flat)

    /* Pen color (RGBA)
     */
    .add_float_varying("fastuidraw_brush_pen_color_x", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_pen_color_y", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_pen_color_z", fastuidraw::glsl::varying_list::interpolation_flat)
    .add_float_varying("fastuidraw_brush_pen_color_w", fastuidraw::glsl::varying_list::interpolation_flat);

  m_vert_shader_utils
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string", fastuidraw::glsl::ShaderSource::from_resource)
    .add_source("fastuidraw_painter_compute_local_distance_from_pixel_distance.glsl.resource_string", fastuidraw::glsl::ShaderSource::from_resource)
    .add_source("fastuidraw_painter_align.vert.glsl.resource_string", fastuidraw::glsl::ShaderSource::from_resource);

  m_frag_shader_utils
    .add_source("fastuidraw_circular_interpolate.glsl.resource_string",
                fastuidraw::glsl::ShaderSource::from_resource)
    .add_source(fastuidraw::glsl::code::curvepair_compute_pseudo_distance(m_p->glyph_atlas()->geometry_store()->alignment(),
                                                                          "fastuidraw_curvepair_pseudo_distance",
                                                                          "fastuidraw_fetch_glyph_data",
                                                                          false))
    .add_source(fastuidraw::glsl::code::curvepair_compute_pseudo_distance(m_p->glyph_atlas()->geometry_store()->alignment(),
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
                 const fastuidraw::glsl::PainterBackendGLSL::UberShaderParams &params)
{
  using namespace fastuidraw;
  using namespace fastuidraw::glsl;
  using namespace fastuidraw::glsl::detail;
  using namespace fastuidraw::PainterPacking;

  std::string varying_layout_macro, binding_layout_macro;
  std::string declare_shader_varyings, declare_main_varyings, declare_brush_varyings;
  std::string declare_vertex_shader_ins;
  std::string declare_uniforms;
  const varying_list *main_varyings;
  DeclareVaryingsStringDatum main_varying_datum, brush_varying_datum, shader_varying_datum;
  const PainterBackendGLSL::BindingPoints &binding_params(params.binding_points());

  const GlyphAtlas *glyph_atlas;
  glyph_atlas = m_p->glyph_atlas().get();

  const ColorStopAtlas *colorstop_atlas;
  colorstop_atlas = m_p->colorstop_atlas().get();

  const ImageAtlas *image_atlas;
  image_atlas = m_p->image_atlas().get();

  if(params.assign_layout_to_vertex_shader_inputs())
    {
      std::ostringstream ostr;
      ostr << "layout(location = " << PainterBackendGLSL::primary_attrib_slot << ") in vec4 fastuidraw_primary_attribute;\n"
           << "layout(location = " << PainterBackendGLSL::secondary_attrib_slot << ") in vec4 fastuidraw_secondary_attribute;\n"
           << "layout(location = " << PainterBackendGLSL::uint_attrib_slot << ") in uvec4 fastuidraw_uint_attribute;\n"
           << "layout(location = " << PainterBackendGLSL::header_attrib_slot << ") in uint fastuidraw_header_attribute;\n";
      declare_vertex_shader_ins = ostr.str();
    }
  else
    {
      std::ostringstream ostr;
      ostr << "in vec4 fastuidraw_primary_attribute;\n"
           << "in vec4 fastuidraw_secondary_attribute;\n"
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


  add_enums(m_p->configuration_base().alignment(), vert);
  add_texture_size_constants(vert, glyph_atlas, image_atlas, colorstop_atlas);
  vert
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
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_SIZE", image_atlas->index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", uint32_log2(image_atlas->index_tile_size()))
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", image_atlas->color_tile_size())
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
    .add_source("fastuidraw_painter_types.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_main.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(m_vert_shader_utils);
  stream_unpack_code(m_p->configuration_base().alignment(), vert);
  stream_uber_vert_shader(params.vert_shader_use_switch(), vert, make_c_array(m_item_shaders), shader_varying_datum);

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

  add_enums(m_p->configuration_base().alignment(), frag);
  add_texture_size_constants(frag, glyph_atlas, image_atlas, colorstop_atlas);
  frag
    .add_source(varying_layout_macro.c_str(), ShaderSource::from_string)
    .add_source(binding_layout_macro.c_str(), ShaderSource::from_string)
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
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_SIZE", image_atlas->index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", uint32_log2(image_atlas->index_tile_size()))
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", image_atlas->color_tile_size())
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
    .add_source("fastuidraw_painter_types.vert.glsl.resource_string", ShaderSource::from_resource)
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
    .add_source(code::image_atlas_compute_coord("fastuidraw_compute_image_atlas_coord",
                                                "fastuidraw_imageIndexAtlas",
                                                image_atlas->index_tile_size(), image_atlas->color_tile_size()))
    .add_source("fastuidraw_painter_main.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_anisotropic.frag.glsl.resource_string", ShaderSource::from_resource)
    .add_source(m_frag_shader_utils);

  if(params.unpack_header_and_brush_in_frag_shader())
    {
      stream_unpack_code(m_p->configuration_base().alignment(), frag);
    }

  stream_uber_frag_shader(params.frag_shader_use_switch(), frag, make_c_array(m_item_shaders), shader_varying_datum);
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

setget_implement(bool, unique_group_per_item_shader)
setget_implement(bool, unique_group_per_blend_shader)
setget_implement(bool, use_hw_clip_planes)
setget_implement(enum fastuidraw::PainterBlendShader::shader_type, default_blend_shader_type)

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
                   const ConfigurationGLSL &config):
  PainterBackend(glyph_atlas, image_atlas, colorstop_atlas, config.m_config)
{
  m_d = FASTUIDRAWnew PainterBackendGLSLPrivate(this, config);
  set_hints().clipping_via_hw_clip_planes(config.use_hw_clip_planes());
  glsl::detail::ShaderSetCreator cr(config.default_blend_shader_type());
  set_default_shaders(cr.create_shader_set());
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

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterBackendGLSL::
absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);

  reference_counted_ptr<glsl::PainterItemShaderGLSL> h;
  fastuidraw::PainterShader::Tag return_value;

  assert(!shader->parent());
  assert(shader.dynamic_cast_ptr<glsl::PainterItemShaderGLSL>());
  h = shader.static_cast_ptr<glsl::PainterItemShaderGLSL>();

  d->m_shader_code_added = true;
  d->m_item_shaders.push_back(h);
  d->update_varying_size(h->varyings());

  return_value.m_ID = d->m_next_item_shader_ID;
  d->m_next_item_shader_ID += h->number_sub_shaders();
  return_value.m_group = d->m_config.unique_group_per_item_shader() ? return_value.m_ID : 0;
  return return_value;
}

uint32_t
fastuidraw::glsl::PainterBackendGLSL::
compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);

  return d->m_config.unique_group_per_item_shader() ? shader->ID() : 0;
}

fastuidraw::PainterShader::Tag
fastuidraw::glsl::PainterBackendGLSL::
absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);

  reference_counted_ptr<glsl::PainterBlendShaderGLSL> h;
  fastuidraw::PainterShader::Tag return_value;

  assert(!shader->parent());
  assert(shader.dynamic_cast_ptr<glsl::PainterBlendShaderGLSL>());
  h = shader.static_cast_ptr<glsl::PainterBlendShaderGLSL>();

  d->m_shader_code_added = true;
  d->m_blend_shaders[h->type()].m_shaders.push_back(h);

  return_value.m_ID = d->m_next_blend_shader_ID;
  d->m_next_blend_shader_ID += h->number_sub_shaders();
  return_value.m_group = d->m_config.unique_group_per_blend_shader() ? return_value.m_ID : 0;

  return return_value;
}

uint32_t
fastuidraw::glsl::PainterBackendGLSL::
compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);

  return d->m_config.unique_group_per_blend_shader() ? shader->ID() : 0;
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
                 const UberShaderParams &construct_params)
{
  PainterBackendGLSLPrivate *d;
  d = reinterpret_cast<PainterBackendGLSLPrivate*>(m_d);
  d->construct_shader(out_vertex, out_fragment, construct_params);
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
