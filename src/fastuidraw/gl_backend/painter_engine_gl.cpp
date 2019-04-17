/*!
 * \file painter_engine_gl.cpp
 * \brief file painter_engine_gl.cpp
 *
 * Copyright 2019 by Intel.
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


#include <list>
#include <map>
#include <sstream>
#include <vector>
#include <iostream>

#include <fastuidraw/gl_backend/painter_engine_gl.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

#include <private/util_private.hpp>
#include "private/image_gl.hpp"
#include "private/painter_backend_gl.hpp"
#include "private/painter_backend_gl_config.hpp"
#include "private/painter_surface_gl_private.hpp"
#include "private/painter_shader_registrar_gl.hpp"
#include "private/binding_points.hpp"

namespace
{
  class ImageAtlasParamsPrivate
  {
  public:
    ImageAtlasParamsPrivate(void):
      m_log2_color_tile_size(5),
      m_log2_num_color_tiles_per_row_per_col(8),
      m_num_color_layers(1),
      m_log2_index_tile_size(2),
      m_log2_num_index_tiles_per_row_per_col(6),
      m_num_index_layers(4)
    {}

    int m_log2_color_tile_size;
    int m_log2_num_color_tiles_per_row_per_col;
    int m_num_color_layers;
    int m_log2_index_tile_size;
    int m_log2_num_index_tiles_per_row_per_col;
    int m_num_index_layers;
  };

  class ColorStopAtlasParamsPrivate
  {
  public:
    ColorStopAtlasParamsPrivate(void):
      m_width(1024),
      m_num_layers(32)
    {}

    int m_width;
    int m_num_layers;
  };

  class ConfigurationGLPrivate
  {
  public:
    ConfigurationGLPrivate(void):
      m_attributes_per_buffer(512 * 512),
      m_indices_per_buffer((m_attributes_per_buffer * 6) / 4),
      m_data_blocks_per_store_buffer(1024 * 64),
      m_data_store_backing(fastuidraw::gl::PainterEngineGL::data_store_tbo),
      m_number_pools(3),
      m_break_on_shader_change(false),
      m_clipping_type(fastuidraw::gl::PainterEngineGL::clipping_via_gl_clip_distance),
      m_number_external_textures(8),
      /* on Mesa/i965 using switch statement gives much slower
       * performance than using if/else chain.
       */
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_blend_shader_use_switch(false),
      m_assign_layout_to_vertex_shader_inputs(true),
      m_assign_layout_to_varyings(false),
      m_assign_binding_points(true),
      m_separate_program_for_discard(true),
      m_preferred_blend_type(fastuidraw::PainterBlendShader::dual_src),
      m_fbf_blending_type(fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_not_supported),
      m_allow_bindless_texture_from_surface(true),
      m_support_dual_src_blend_shaders(true),
      m_use_uber_item_shader(true)
    {}

    unsigned int m_attributes_per_buffer;
    unsigned int m_indices_per_buffer;
    unsigned int m_data_blocks_per_store_buffer;
    enum fastuidraw::gl::PainterEngineGL::data_store_backing_t m_data_store_backing;
    unsigned int m_number_pools;
    bool m_break_on_shader_change;
    enum fastuidraw::gl::PainterEngineGL::clipping_type_t m_clipping_type;
    unsigned int m_number_external_textures;
    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_blend_shader_use_switch;
    bool m_assign_layout_to_vertex_shader_inputs;
    bool m_assign_layout_to_varyings;
    bool m_assign_binding_points;
    bool m_separate_program_for_discard;
    enum fastuidraw::PainterBlendShader::shader_type m_preferred_blend_type;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t m_fbf_blending_type;
    bool m_allow_bindless_texture_from_surface;
    bool m_support_dual_src_blend_shaders;
    bool m_use_uber_item_shader;

    std::string m_glsl_version_override;
    fastuidraw::gl::PainterEngineGL::ImageAtlasParams m_image_atlas_params;
    fastuidraw::gl::GlyphAtlasGL::params m_glyph_atlas_params;
    fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams m_colorstop_atlas_params;

    fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_image_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> m_colorstop_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL> m_glyph_atlas;
  };

  class PainterEngineGLPrivate
  {
  public:
    PainterEngineGLPrivate(fastuidraw::gl::PainterEngineGL *p)
    {
      m_reg_gl = p->painter_shader_registrar().static_cast_ptr<fastuidraw::gl::detail::PainterShaderRegistrarGL>();

      m_binding_points.m_num_ubo_units = m_reg_gl->uber_shader_builder_params().num_ubo_units();
      m_binding_points.m_num_ssbo_units = m_reg_gl->uber_shader_builder_params().num_ssbo_units();
      m_binding_points.m_num_texture_units = m_reg_gl->uber_shader_builder_params().num_texture_units();
      m_binding_points.m_num_image_units = m_reg_gl->uber_shader_builder_params().num_image_units();

      m_binding_points.m_colorstop_atlas_binding = m_reg_gl->uber_shader_builder_params().colorstop_atlas_binding();
      m_binding_points.m_image_atlas_color_tiles_nearest_binding = m_reg_gl->uber_shader_builder_params().image_atlas_color_tiles_nearest_binding();
      m_binding_points.m_image_atlas_color_tiles_linear_binding = m_reg_gl->uber_shader_builder_params().image_atlas_color_tiles_linear_binding();
      m_binding_points.m_image_atlas_index_tiles_binding = m_reg_gl->uber_shader_builder_params().image_atlas_index_tiles_binding();
      m_binding_points.m_glyph_atlas_store_binding = m_reg_gl->uber_shader_builder_params().glyph_atlas_store_binding();
      m_binding_points.m_glyph_atlas_store_binding_fp16 = m_reg_gl->uber_shader_builder_params().glyph_atlas_store_binding_fp16x2();
      m_binding_points.m_data_store_buffer_binding = m_reg_gl->uber_shader_builder_params().data_store_buffer_binding();
      m_binding_points.m_color_interlock_image_buffer_binding = m_reg_gl->uber_shader_builder_params().color_interlock_image_buffer_binding();
      m_binding_points.m_external_texture_binding = m_reg_gl->uber_shader_builder_params().external_texture_binding();
      m_binding_points.m_coverage_buffer_texture_binding = m_reg_gl->uber_shader_builder_params().coverage_buffer_texture_binding();
      m_binding_points.m_uniforms_ubo_binding = m_reg_gl->uber_shader_builder_params().uniforms_ubo_binding();
    }

    static
    void
    compute_uber_shader_params(const fastuidraw::gl::PainterEngineGL::ConfigurationGL &P,
                               const fastuidraw::gl::ContextProperties &ctx,
                               fastuidraw::gl::PainterEngineGL::UberShaderParams &out_value,
                               fastuidraw::PainterShaderSet &out_shaders);

    fastuidraw::gl::detail::BindingPoints m_binding_points;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::detail::PainterShaderRegistrarGL> m_reg_gl;
  };
}

//////////////////////////////////////////
// PainterEngineGLPrivate methods
void
PainterEngineGLPrivate::
compute_uber_shader_params(const fastuidraw::gl::PainterEngineGL::ConfigurationGL &params,
                           const fastuidraw::gl::ContextProperties &ctx,
                           fastuidraw::gl::PainterEngineGL::UberShaderParams &out_params,
                           fastuidraw::PainterShaderSet &out_shaders)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  using namespace fastuidraw::gl::detail;

  bool supports_bindless;

  supports_bindless = ctx.has_extension("GL_ARB_bindless_texture")
    || ctx.has_extension("GL_NV_bindless_texture");

  ColorStopAtlasGL *color;
  FASTUIDRAWassert(dynamic_cast<ColorStopAtlasGL*>(params.colorstop_atlas().get()));
  color = static_cast<ColorStopAtlasGL*>(params.colorstop_atlas().get());
  enum PainterEngineGL::colorstop_backing_t colorstop_tp;
  if (color->texture_bind_target() == GL_TEXTURE_2D_ARRAY)
    {
      colorstop_tp = PainterEngineGL::colorstop_texture_2d_array;
    }
  else
    {
      colorstop_tp = PainterEngineGL::colorstop_texture_1d_array;
    }

  out_params
    .fbf_blending_type(params.fbf_blending_type())
    .preferred_blend_type(params.preferred_blend_type())
    .supports_bindless_texturing(supports_bindless)
    .assign_layout_to_vertex_shader_inputs(params.assign_layout_to_vertex_shader_inputs())
    .assign_layout_to_varyings(params.assign_layout_to_varyings())
    .assign_binding_points(params.assign_binding_points())
    .use_ubo_for_uniforms(true)
    .clipping_type(params.clipping_type())
    .z_coordinate_convention(PainterEngineGL::z_minus_1_to_1)
    .vert_shader_use_switch(params.vert_shader_use_switch())
    .frag_shader_use_switch(params.frag_shader_use_switch())
    .number_external_textures(params.number_external_textures())
    .blend_shader_use_switch(params.blend_shader_use_switch())
    .data_store_backing(params.data_store_backing())
    .data_blocks_per_store_buffer(params.data_blocks_per_store_buffer())
    .glyph_data_backing(params.glyph_atlas()->param_values().glyph_data_backing_store_type())
    .glyph_data_backing_log2_dims(params.glyph_atlas()->param_values().texture_2d_array_store_log2_dims())
    .colorstop_atlas_backing(colorstop_tp)
    .use_uvec2_for_bindless_handle(ctx.has_extension("GL_ARB_bindless_texture"));

  out_shaders = out_params.default_shaders();
}

///////////////////////////////////////////////
// fastuidraw::gl::PainterSurfaceGL methods
fastuidraw::gl::PainterSurfaceGL::
PainterSurfaceGL(ivec2 dims, const PainterEngineGL &backend,
                 enum PainterSurface::render_type_t render_type)
{
  m_d = FASTUIDRAWnew detail::PainterSurfaceGLPrivate(render_type, 0u, dims,
                                                      backend.configuration_gl().allow_bindless_texture_from_surface());
}

fastuidraw::gl::PainterSurfaceGL::
PainterSurfaceGL(ivec2 dims, GLuint color_buffer_texture,
                 const PainterEngineGL &backend,
                 enum PainterSurface::render_type_t render_type)
{
  m_d = FASTUIDRAWnew detail::PainterSurfaceGLPrivate(render_type, color_buffer_texture, dims,
                                                      backend.configuration_gl().allow_bindless_texture_from_surface());
}

fastuidraw::gl::PainterSurfaceGL::
~PainterSurfaceGL()
{
  detail::PainterSurfaceGLPrivate *d;
  d = static_cast<detail::PainterSurfaceGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

GLuint
fastuidraw::gl::PainterSurfaceGL::
texture(void) const
{
  detail::PainterSurfaceGLPrivate *d;
  d = static_cast<detail::PainterSurfaceGLPrivate*>(m_d);
  return d->color_buffer();
}

void
fastuidraw::gl::PainterSurfaceGL::
blit_surface(const Viewport &src,
             const Viewport &dst,
             GLenum filter) const
{
  detail::PainterSurfaceGLPrivate *d;
  d = static_cast<detail::PainterSurfaceGLPrivate*>(m_d);
  GLint old_fbo;

  fastuidraw_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo);
  fastuidraw_glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo(true));
  fastuidraw_glBlitFramebuffer(src.m_origin.x(),
                               src.m_origin.y(),
                               src.m_origin.x() + src.m_dimensions.x(),
                               src.m_origin.y() + src.m_dimensions.y(),
                               dst.m_origin.x(),
                               dst.m_origin.y(),
                               dst.m_origin.x() + dst.m_dimensions.x(),
                               dst.m_origin.y() + dst.m_dimensions.y(),
                               GL_COLOR_BUFFER_BIT, filter);
  fastuidraw_glBindFramebuffer(GL_READ_FRAMEBUFFER, old_fbo);
}

void
fastuidraw::gl::PainterSurfaceGL::
blit_surface(GLenum filter) const
{
  ivec2 dims(dimensions());
  Viewport vwp(0, 0, dims.x(), dims.y());
  blit_surface(vwp, vwp, filter);
}

fastuidraw::reference_counted_ptr<const fastuidraw::Image>
fastuidraw::gl::PainterSurfaceGL::
image(const reference_counted_ptr<ImageAtlas> &atlas) const
{
  detail::PainterSurfaceGLPrivate *d;
  d = static_cast<detail::PainterSurfaceGLPrivate*>(m_d);
  return d->image(atlas);
}

void
fastuidraw::gl::PainterSurfaceGL::
viewport(const Viewport &vwp)
{
  detail::PainterSurfaceGLPrivate *d;
  d = static_cast<detail::PainterSurfaceGLPrivate*>(m_d);
  d->m_viewport = vwp;
}

void
fastuidraw::gl::PainterSurfaceGL::
clear_color(const vec4 &c)
{
  detail::PainterSurfaceGLPrivate *d;
  d = static_cast<detail::PainterSurfaceGLPrivate*>(m_d);
  d->m_clear_color = c;
}

get_implement(fastuidraw::gl::PainterSurfaceGL,
              fastuidraw::gl::detail::PainterSurfaceGLPrivate,
              fastuidraw::ivec2, dimensions)

get_implement(fastuidraw::gl::PainterSurfaceGL,
              fastuidraw::gl::detail::PainterSurfaceGLPrivate,
              const fastuidraw::PainterSurface::Viewport&, viewport)

get_implement(fastuidraw::gl::PainterSurfaceGL,
              fastuidraw::gl::detail::PainterSurfaceGLPrivate,
              const fastuidraw::vec4&, clear_color)

get_implement(fastuidraw::gl::PainterSurfaceGL,
              fastuidraw::gl::detail::PainterSurfaceGLPrivate,
              enum fastuidraw::PainterSurface::render_type_t, render_type)

//////////////////////////////////////////////
// fastuidraw::gl::PainterEngineGL::ImageAtlasParams methods
fastuidraw::gl::PainterEngineGL::ImageAtlasParams::
ImageAtlasParams(void)
{
  m_d = FASTUIDRAWnew ImageAtlasParamsPrivate();
}

fastuidraw::gl::PainterEngineGL::ImageAtlasParams::
ImageAtlasParams(const ImageAtlasParams &obj)
{
  ImageAtlasParamsPrivate *d;
  d = static_cast<ImageAtlasParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ImageAtlasParamsPrivate(*d);
}

fastuidraw::gl::PainterEngineGL::ImageAtlasParams::
~ImageAtlasParams()
{
  ImageAtlasParamsPrivate *d;
  d = static_cast<ImageAtlasParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::gl::PainterEngineGL::ImageAtlasParams&
fastuidraw::gl::PainterEngineGL::ImageAtlasParams::
optimal_color_sizes(int log2_color_tile_size)
{
  int m, log2m, c;
  m = context_get<GLint>(GL_MAX_TEXTURE_SIZE);
  log2m = uint32_log2(m);
  c = std::min(8, std::max(1, log2m - log2_color_tile_size));
  return log2_num_color_tiles_per_row_per_col(c);
}

assign_swap_implement(fastuidraw::gl::PainterEngineGL::ImageAtlasParams)
setget_implement(fastuidraw::gl::PainterEngineGL::ImageAtlasParams,
                 ImageAtlasParamsPrivate,
                 int, log2_color_tile_size)
setget_implement(fastuidraw::gl::PainterEngineGL::ImageAtlasParams,
                 ImageAtlasParamsPrivate,
                 int, log2_num_color_tiles_per_row_per_col)
setget_implement(fastuidraw::gl::PainterEngineGL::ImageAtlasParams,
                 ImageAtlasParamsPrivate,
                 int, num_color_layers)
setget_implement(fastuidraw::gl::PainterEngineGL::ImageAtlasParams,
                 ImageAtlasParamsPrivate,
                 int, log2_index_tile_size)
setget_implement(fastuidraw::gl::PainterEngineGL::ImageAtlasParams,
                 ImageAtlasParamsPrivate,
                 int, log2_num_index_tiles_per_row_per_col)
setget_implement(fastuidraw::gl::PainterEngineGL::ImageAtlasParams,
                 ImageAtlasParamsPrivate,
                 int, num_index_layers)



///////////////////////////////////////////////
// fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams methods
fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams::
ColorStopAtlasParams(void)
{
  m_d = FASTUIDRAWnew ColorStopAtlasParamsPrivate();
}

fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams::
ColorStopAtlasParams(const ColorStopAtlasParams &obj)
{
  ColorStopAtlasParamsPrivate *d;
  d = static_cast<ColorStopAtlasParamsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ColorStopAtlasParamsPrivate(*d);
}

fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams::
~ColorStopAtlasParams()
{
  ColorStopAtlasParamsPrivate *d;
  d = static_cast<ColorStopAtlasParamsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams)
setget_implement(fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams,
                 ColorStopAtlasParamsPrivate,
                 int, width)
setget_implement(fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams,
                 ColorStopAtlasParamsPrivate,
                 int, num_layers)

fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams&
fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams::
optimal_width(void)
{
  return width(fastuidraw::gl::context_get<GLint>(GL_MAX_TEXTURE_SIZE));
}

///////////////////////////////////////////////
// fastuidraw::gl::PainterEngineGL::ConfigurationGL methods
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
ConfigurationGL(void)
{
  m_d = FASTUIDRAWnew ConfigurationGLPrivate();
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL::
ConfigurationGL(const ConfigurationGL &obj)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationGLPrivate(*d);
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL::
~ConfigurationGL()
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::c_string
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
glsl_version_override(void) const
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);
  return d->m_glsl_version_override.c_str();
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
glsl_version_override(c_string v)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);
  d->m_glsl_version_override = (v) ? v : "";
  return *this;
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
configure_from_context(bool choose_optimal_rendering_quality,
                       const ContextProperties &ctx)
{
  using namespace fastuidraw::gl::detail;

  ConfigurationGLPrivate *d;
  enum interlock_type_t interlock_type;

  d = static_cast<ConfigurationGLPrivate*>(m_d);
  interlock_type = compute_interlock_type(ctx);

  d->m_break_on_shader_change = false;
  d->m_clipping_type = clipping_via_gl_clip_distance;

  /* These do not impact performance, but they make
   * cleaner initialization.
   */
  d->m_assign_layout_to_vertex_shader_inputs = true;
  d->m_assign_layout_to_varyings = true;
  d->m_assign_binding_points = true;

  /* Generally, we want to allow for early-Z as
   * much as possible, so we have a different
   * program for those shaders that use discard
   */
  d->m_separate_program_for_discard = true;

  /* Adjust blending type from GL context properties */
  d->m_fbf_blending_type =
    compute_fbf_blending_type(interlock_type, fbf_blending_framebuffer_fetch, ctx);

  d->m_preferred_blend_type = compute_preferred_blending_type(d->m_fbf_blending_type,
                                                              PainterBlendShader::dual_src,
                                                              d->m_support_dual_src_blend_shaders,
                                                              ctx);

  /* pay attention to the context for m_data_store_backing.
   * Generally speaking, for caching UBO > SSBO > TBO,
   * but max UBO size might be too tiny, we are arbitrarily
   * guessing that the data store buffer should be 64K blocks
   * (which is 256KB); what size is good is really depends on
   * how much data each frame will have which mostly depends
   * on how often the brush and transformations and clipping
   * change.
   */
  d->m_data_blocks_per_store_buffer = 1024 * 64;
  d->m_data_store_backing = data_store_ubo;

  unsigned int max_ubo_size, max_num_blocks, block_size;
  block_size = 4 * sizeof(generic_data);
  max_ubo_size = context_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE);
  max_num_blocks = max_ubo_size / block_size;
  if (max_num_blocks < d->m_data_blocks_per_store_buffer)
    {
      if (shader_storage_buffers_supported(ctx))
        {
          d->m_data_store_backing = data_store_ssbo;
        }
      else if (detail::compute_tex_buffer_support(ctx) != detail::tex_buffer_not_supported)
        {
          d->m_data_store_backing = data_store_tbo;
        }
    }

  /* NVIDIA's GPU's (alteast up to 700 series) gl_ClipDistance is not
   * robust enough to work  with FastUIDraw regardless of driver
   * (NVIDIA proprietary or Nouveau open source). We try to detect
   * either in the version or renderer and if so, mark gl_ClipDistance
   * as NOT supported.
   */
  bool NVIDIA_detected;
  std::string gl_version, gl_renderer, gl_vendor;

  gl_version = (const char*)fastuidraw_glGetString(GL_VERSION);
  gl_renderer = (const char*)fastuidraw_glGetString(GL_RENDERER);
  gl_vendor = (const char*)fastuidraw_glGetString(GL_VENDOR);
  NVIDIA_detected = gl_version.find("NVIDIA") != std::string::npos
    || gl_renderer.find("GeForce") != std::string::npos
    || gl_version.find("nouveau") != std::string::npos
    || gl_renderer.find("nouveau") != std::string::npos
    || gl_vendor.find("nouveau") != std::string::npos;

  d->m_clipping_type = compute_clipping_type(d->m_fbf_blending_type,
                                             d->m_clipping_type,
                                             ctx,
                                             !NVIDIA_detected);

  /* likely shader compilers like if/ese chains more than
   * switches, atleast Mesa really prefers if/else chains
   */
  d->m_vert_shader_use_switch = false;
  d->m_frag_shader_use_switch = false;
  d->m_blend_shader_use_switch = false;

  /* UI rendering is often dominated by drawing quads which
   * means for every 6 indices there are 4 attributes. However,
   * how many quads per draw-call, we just guess at 512 * 512
   * attributes
   */
  d->m_attributes_per_buffer = 512 * 512;
  d->m_indices_per_buffer = (d->m_attributes_per_buffer * 6) / 4;

  /* Very often drivers will have the previous frame still
   * in flight when a new frame is started, so we do not want
   * to modify buffers in use, so that puts the minumum number
   * of pools to 2. Also, often enough there is triple buffering
   * so we play it safe and make it 3.
   */
  d->m_number_pools = 3;

  /* For now, choosing optimal rendering quality does not
   * have impact on options.
   */
  FASTUIDRAWunused(choose_optimal_rendering_quality);

  return *this;
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
adjust_for_context(const ContextProperties &ctx)
{
  using namespace fastuidraw::gl::detail;

  ConfigurationGLPrivate *d;
  enum detail::tex_buffer_support_t tex_buffer_support;
  enum interlock_type_t interlock_type;
  unsigned int num_textures_used(0);

  d = static_cast<ConfigurationGLPrivate*>(m_d);
  interlock_type = compute_interlock_type(ctx);
  tex_buffer_support = detail::compute_tex_buffer_support(ctx);

  if (tex_buffer_support == detail::tex_buffer_not_supported)
    {
      // TBO's not supported, fall back to using SSBO's.
      if (d->m_data_store_backing == data_store_tbo)
        {
          d->m_data_store_backing = data_store_ssbo;
        }

      if (d->m_glyph_atlas_params.glyph_data_backing_store_type() == glyph_data_tbo)
        {
          d->m_glyph_atlas_params.use_storage_buffer_store();
        }
    }

  if (!shader_storage_buffers_supported(ctx))
    {
      if (d->m_data_store_backing == data_store_ssbo)
        {
          // SSBO's not supported, fall back to using TBO's then fallback to UBO's
          d->m_data_store_backing = (tex_buffer_support != detail::tex_buffer_not_supported) ?
            data_store_tbo:
            data_store_ubo;
        }

      if (d->m_glyph_atlas_params.glyph_data_backing_store_type() == glyph_data_ssbo)
        {
          // SSBO's not supported, fall back to using TBO's then fallback to texture (ICK)
          if (tex_buffer_support != detail::tex_buffer_not_supported)
            {
              d->m_glyph_atlas_params.use_texture_buffer_store();
            }
          else
            {
              d->m_glyph_atlas_params.use_texture_2d_array_store();
            }
        }
    }

  /* Query GL what is good size for data store buffer. Size is dependent
   * how the data store is backed.
   */
  switch(d->m_data_store_backing)
    {
    case data_store_tbo:
      {
        unsigned int max_texture_buffer_size(0);
        max_texture_buffer_size = context_get<GLint>(GL_MAX_TEXTURE_BUFFER_SIZE);
        d->m_data_blocks_per_store_buffer = t_min(max_texture_buffer_size,
                                                  d->m_data_blocks_per_store_buffer);
        ++num_textures_used;
      }
      break;

    case data_store_ubo:
      {
        unsigned int max_ubo_size_bytes, max_num_blocks, block_size_bytes;
        block_size_bytes = 4 * sizeof(generic_data);
        max_ubo_size_bytes = context_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE);
        max_num_blocks = max_ubo_size_bytes / block_size_bytes;
        d->m_data_blocks_per_store_buffer = t_min(max_num_blocks,
                                                  d->m_data_blocks_per_store_buffer);
      }
      break;

    case data_store_ssbo:
      {
        unsigned int max_ssbo_size_bytes, max_num_blocks, block_size_bytes;
        block_size_bytes = 4 * sizeof(generic_data);
        max_ssbo_size_bytes = context_get<GLint>(GL_MAX_SHADER_STORAGE_BLOCK_SIZE);
        max_num_blocks = max_ssbo_size_bytes / block_size_bytes;
        d->m_data_blocks_per_store_buffer = t_min(max_num_blocks,
                                                  d->m_data_blocks_per_store_buffer);
      }
      break;
    }

  interlock_type = compute_interlock_type(ctx);
  d->m_fbf_blending_type = compute_fbf_blending_type(interlock_type, d->m_fbf_blending_type, ctx);
  d->m_preferred_blend_type = compute_preferred_blending_type(d->m_fbf_blending_type,
                                                              d->m_preferred_blend_type,
                                                              d->m_support_dual_src_blend_shaders,
                                                              ctx);
  d->m_clipping_type = compute_clipping_type(d->m_fbf_blending_type, d->m_clipping_type, ctx);

  /* if have to use discard for clipping, then there is zero point to
   * separate the discarding and non-discarding item shaders.
   */
  if (d->m_clipping_type == clipping_via_discard)
    {
      d->m_separate_program_for_discard = false;
    }

  /* Some shader features require new version of GL or
   * specific extensions.
   */
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (ctx.version() < ivec2(3, 2))
        {
          d->m_assign_layout_to_varyings = d->m_assign_layout_to_varyings
            && ctx.has_extension("GL_EXT_separate_shader_objects");
        }

      if (ctx.version() <= ivec2(3, 0))
        {
          /* GL ES 3.0 does not support layout(binding=) and
           * does not support image-load-store either
           */
          d->m_assign_binding_points = false;
        }
    }
  #else
    {
      if (ctx.version() < ivec2(4, 2))
        {
          d->m_assign_layout_to_varyings = d->m_assign_layout_to_varyings
            && ctx.has_extension("GL_ARB_separate_shader_objects");

          d->m_assign_binding_points = d->m_assign_binding_points
            && ctx.has_extension("GL_ARB_shading_language_420pack");
        }
    }
  #endif

  /* if have to use discard for clipping, then there is zero point to
   * separate the discarding and non-discarding item shaders.
   */
  if (d->m_clipping_type == PainterEngineGL::clipping_via_discard)
    {
      d->m_separate_program_for_discard = false;
    }

  /* Increment number textures used for:
   *   - colorStopAtlas
   *   - imageAtlasLinear
   *   - imageAtlasNearest
   *   - imageAtlasIndex
   *   - deferredCoverageBuffer
   *   - glyphAtlas
   *   - glyphAtlasFP16x2
   */
  num_textures_used += 7;

  /* adjust m_number_external_textures taking
   * into account the number of used texture
   * slots against how many the GL implementation
   * supports.
   */
  unsigned int num_slots_left(context_get<GLint>(GL_MAX_TEXTURE_IMAGE_UNITS));

  // t_max() prevent unsigned underflow
  num_slots_left = t_max(num_textures_used, num_slots_left) - num_textures_used;
  d->m_number_external_textures = t_min(d->m_number_external_textures, num_slots_left);

  /* Don't use up all the remaining texture units, max-out at 16
   * external textures
   */
  d->m_number_external_textures = t_min(d->m_number_external_textures, 16u);

  return *this;
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
image_atlas_params(const ImageAtlasParams &params)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);

  d->m_image_atlas_params = params;
  d->m_image_atlas = nullptr;
  return *this;
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
colorstop_atlas_params(const ColorStopAtlasParams &params)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);

  d->m_colorstop_atlas_params = params;
  d->m_colorstop_atlas = nullptr;
  return *this;
}

fastuidraw::gl::PainterEngineGL::ConfigurationGL&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
glyph_atlas_params(const GlyphAtlasGL::params &params)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);

  d->m_glyph_atlas_params = params;
  d->m_glyph_atlas = nullptr;
  return *this;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
image_atlas(void) const
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);

  if (!d->m_image_atlas)
    {
      d->m_image_atlas = FASTUIDRAWnew detail::ImageAtlasGL(d->m_image_atlas_params);
    }
  return d->m_image_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL>&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
glyph_atlas(void) const
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);

  if (!d->m_glyph_atlas)
    {
      d->m_glyph_atlas = FASTUIDRAWnew GlyphAtlasGL(d->m_glyph_atlas_params);
    }
  return d->m_glyph_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::gl::PainterEngineGL::ConfigurationGL::
colorstop_atlas(void) const
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);

  if (!d->m_colorstop_atlas)
    {
      d->m_colorstop_atlas = FASTUIDRAWnew detail::ColorStopAtlasGL(d->m_colorstop_atlas_params);
    }

  return d->m_colorstop_atlas;
}

assign_swap_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL)

setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, attributes_per_buffer)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, indices_per_buffer)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, data_blocks_per_store_buffer)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, number_pools)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, break_on_shader_change)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterEngineGL::clipping_type_t, clipping_type)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, number_external_textures)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, vert_shader_use_switch)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, frag_shader_use_switch)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, blend_shader_use_switch)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterEngineGL::data_store_backing_t, data_store_backing)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_layout_to_vertex_shader_inputs)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_layout_to_varyings)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_binding_points)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, separate_program_for_discard)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::PainterBlendShader::shader_type, preferred_blend_type)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterEngineGL::fbf_blending_type_t, fbf_blending_type)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, allow_bindless_texture_from_surface)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, support_dual_src_blend_shaders)
setget_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, use_uber_item_shader)
get_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
              const fastuidraw::gl::PainterEngineGL::ImageAtlasParams&, image_atlas_params)
get_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
              const fastuidraw::gl::PainterEngineGL::ColorStopAtlasParams&, colorstop_atlas_params)
get_implement(fastuidraw::gl::PainterEngineGL::ConfigurationGL, ConfigurationGLPrivate,
              const fastuidraw::gl::GlyphAtlasGL::params&, glyph_atlas_params)

///////////////////////////////////////////////////
// fastuidraw::gl::PainterEngineGL methods
fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterEngineGL>
fastuidraw::gl::PainterEngineGL::
create(ConfigurationGL config_gl, const ContextProperties &ctx)
{
  UberShaderParams uber_params;
  PainterShaderSet shaders;
  reference_counted_ptr<glsl::PainterShaderRegistrarGLSL> reg;

  config_gl.adjust_for_context(ctx);

  PainterEngineGLPrivate::compute_uber_shader_params(config_gl, ctx, uber_params, shaders);
  return FASTUIDRAWnew PainterEngineGL(config_gl, uber_params, shaders);
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterEngineGL>
fastuidraw::gl::PainterEngineGL::
create(bool optimal_rendering_quality,
       const ContextProperties &ctx)
{
  ConfigurationGL config_gl;
  config_gl
    .configure_from_context(optimal_rendering_quality, ctx);

  return create(config_gl, ctx);
}

fastuidraw::gl::PainterEngineGL::
PainterEngineGL(const ConfigurationGL &config_gl,
                        const UberShaderParams &uber_params,
                        const PainterShaderSet &shaders):
  PainterEngine(config_gl.glyph_atlas(),
                        config_gl.image_atlas(),
                        config_gl.colorstop_atlas(),
                        FASTUIDRAWnew detail::PainterShaderRegistrarGL(config_gl, uber_params),
                        ConfigurationBase()
                        .supports_bindless_texturing(uber_params.supports_bindless_texturing()),
                        shaders)
{
  PainterEngineGLPrivate *d;
  m_d = d = FASTUIDRAWnew PainterEngineGLPrivate(this);
  d->m_reg_gl->set_hints(set_hints());
}

fastuidraw::gl::PainterEngineGL::
~PainterEngineGL()
{
  PainterEngineGLPrivate *d;
  d = static_cast<PainterEngineGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::Program>
fastuidraw::gl::PainterEngineGL::
program(enum program_type_t tp,
        enum PainterBlendShader::shader_type blend_type)
{
  PainterEngineGLPrivate *d;
  d = static_cast<PainterEngineGLPrivate*>(m_d);
  return d->m_reg_gl->programs().program(tp, blend_type);
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::Program>
fastuidraw::gl::PainterEngineGL::
program_deferred_coverage_buffer(void)
{
  PainterEngineGLPrivate *d;
  d = static_cast<PainterEngineGLPrivate*>(m_d);
  return d->m_reg_gl->programs().m_deferred_coverage_program;
}

const fastuidraw::gl::PainterEngineGL::ConfigurationGL&
fastuidraw::gl::PainterEngineGL::
configuration_gl(void) const
{
  PainterEngineGLPrivate *d;
  d = static_cast<PainterEngineGLPrivate*>(m_d);
  return d->m_reg_gl->params();
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterBackend>
fastuidraw::gl::PainterEngineGL::
create_backend(void) const
{
  return FASTUIDRAWnew detail::PainterBackendGL(this);
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterSurface>
fastuidraw::gl::PainterEngineGL::
create_surface(ivec2 dims,
               enum PainterSurface::render_type_t render_type)
{
  reference_counted_ptr<PainterSurface> S;
  S = FASTUIDRAWnew PainterSurfaceGL(dims, *this, render_type);
  return S;
}

#define binding_info_get(X)                                     \
  unsigned int                                                  \
  fastuidraw::gl::PainterEngineGL::                     \
  X(void) const                                                 \
  {                                                             \
    PainterEngineGLPrivate *d;                          \
    d = static_cast<PainterEngineGLPrivate*>(m_d);      \
    return d->m_binding_points.m_##X;                           \
  }

binding_info_get(num_ubo_units)
binding_info_get(num_ssbo_units)
binding_info_get(num_texture_units)
binding_info_get(num_image_units)
