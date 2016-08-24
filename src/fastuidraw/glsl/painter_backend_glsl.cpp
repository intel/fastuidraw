
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

  class UberShaderParamsPrivate
  {
  public:
    UberShaderParamsPrivate(void):
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_blend_shader_use_switch(false),
      m_unpack_header_and_brush_in_frag_shader(false),
      m_data_store_backing(fastuidraw::glsl::PainterBackendGLSL::data_store_tbo),
      m_data_blocks_per_store_buffer(-1),
      m_glyph_geometry_backing(fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_tbo),
      m_glyph_geometry_backing_log2_dims(-1, -1),
      m_have_float_glyph_texture_atlas(true),
      m_blend_type(fastuidraw::PainterBlendShader::dual_src)
    {}

    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_blend_shader_use_switch;
    bool m_unpack_header_and_brush_in_frag_shader;
    enum fastuidraw::glsl::PainterBackendGLSL::data_store_backing_t m_data_store_backing;
    int m_data_blocks_per_store_buffer;
    enum fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_backing_t m_glyph_geometry_backing;
    fastuidraw::ivec2 m_glyph_geometry_backing_log2_dims;
    bool m_have_float_glyph_texture_atlas;
    enum fastuidraw::PainterBlendShader::shader_type m_blend_type;
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

  std::ostringstream declare_varyings;

  const GlyphAtlas *glyph_atlas;
  glyph_atlas = m_p->glyph_atlas().get();

  const ColorStopAtlas *colorstop_atlas;
  colorstop_atlas = m_p->colorstop_atlas().get();

  const ImageAtlas *image_atlas;
  image_atlas = m_p->image_atlas().get();

  if(params.unpack_header_and_brush_in_frag_shader())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
      frag.add_macro("FASTUIDRAW_PAINTER_UNPACK_AT_FRAGMENT_SHADER");
    }

  stream_declare_varyings(declare_varyings, m_number_uint_varyings,
                          m_number_int_varyings, m_number_float_varyings);

  if(m_config.use_hw_clip_planes())
    {
      vert.add_macro("FASTUIDRAW_PAINTER_USE_HW_CLIP_PLANES");
      frag.add_macro("FASTUIDRAW_PAINTER_USE_HW_CLIP_PLANES");
    }

  switch(params.data_store_backing())
    {
    case PainterBackendGLSL::data_store_ubo:
      {
        const char *uint_types[]=
          {
            "",
            "uint",
            "uvec2",
            "uvec3",
            "uvec4"
          };

        unsigned int alignment(m_p->configuration_base().alignment());
        assert(alignment <= 4 && alignment > 0);

        vert
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer())
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_UINT_TYPE", uint_types[alignment]);

        frag
          .add_macro("FASTUIDRAW_PAINTER_USE_DATA_UBO")
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_ARRAY_SIZE", params.data_blocks_per_store_buffer())
          .add_macro("FASTUIDRAW_PAINTER_DATA_STORE_UINT_TYPE", uint_types[alignment]);
      }
      break;

    case PainterBackendGLSL::data_store_tbo:
      {
        vert.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
        frag.add_macro("FASTUIDRAW_PAINTER_USE_DATA_TBO");
      }
      break;
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
    .add_source("fastuidraw_painter_gles_precision.glsl.resource_string", ShaderSource::from_resource)
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_SIZE", image_atlas->index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", uint32_log2(image_atlas->index_tile_size()))
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", image_atlas->color_tile_size())
    .add_macro("fastuidraw_varying", "out")
    .add_source(declare_varyings.str().c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpacked_values.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_forward_declares.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack_forward_declares.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpack.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_main.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source(m_vert_shader_utils);
  stream_unpack_code(m_p->configuration_base().alignment(), vert);
  stream_uber_vert_shader(params.vert_shader_use_switch(), vert, make_c_array(m_item_shaders));

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
    .add_macro(shader_blend_macro)
    .add_source("fastuidraw_painter_gles_precision.glsl.resource_string", ShaderSource::from_resource)
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_SIZE", image_atlas->index_tile_size())
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_INDEX_TILE_LOG2_SIZE", uint32_log2(image_atlas->index_tile_size()))
    .add_macro("FASTUIDRAW_PAINTER_IMAGE_ATLAS_COLOR_TILE_SIZE", image_atlas->color_tile_size())
    .add_macro("fastuidraw_varying", "in")
    .add_source(declare_varyings.str().c_str(), ShaderSource::from_string)
    .add_source("fastuidraw_painter_uniforms.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_macros.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_types.vert.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_types.glsl.resource_string", ShaderSource::from_resource)
    .add_source("fastuidraw_painter_brush_unpacked_values.glsl.resource_string", ShaderSource::from_resource)
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

  stream_uber_frag_shader(params.frag_shader_use_switch(), frag, make_c_array(m_item_shaders));
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

setget_implement(bool, vert_shader_use_switch)
setget_implement(bool, frag_shader_use_switch)
setget_implement(bool, blend_shader_use_switch)
setget_implement(bool, unpack_header_and_brush_in_frag_shader)
setget_implement(enum fastuidraw::glsl::PainterBackendGLSL::data_store_backing_t, data_store_backing)
setget_implement(int, data_blocks_per_store_buffer)
setget_implement(enum fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_backing_t, glyph_geometry_backing)
setget_implement(fastuidraw::ivec2, glyph_geometry_backing_log2_dims)
setget_implement(enum fastuidraw::PainterBlendShader::shader_type, blend_type)
setget_implement(bool, have_float_glyph_texture_atlas)

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
