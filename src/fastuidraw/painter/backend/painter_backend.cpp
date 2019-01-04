/*!
 * \file painter_backend.cpp
 * \brief file painter_backend.cpp
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


#include <fastuidraw/painter/backend/painter_backend.hpp>
#include "../../private/util_private.hpp"

namespace
{
  class PerformanceHintsPrivate
  {
  public:
    PerformanceHintsPrivate(void):
      m_clipping_via_hw_clip_planes(true)
    {}

    bool m_clipping_via_hw_clip_planes;
  };

  class PainterBackendPrivate
  {
  public:
    PainterBackendPrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> glyph_atlas,
                          fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> image_atlas,
                          fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> colorstop_atlas,
                          fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar> shader_registrar,
                          const fastuidraw::PainterBackend::ConfigurationBase &config,
                          const fastuidraw::PainterShaderSet &pdefault_shaders):
      m_glyph_atlas(glyph_atlas),
      m_image_atlas(image_atlas),
      m_colorstop_atlas(colorstop_atlas),
      m_painter_shader_registrar(shader_registrar),
      m_config(config),
      m_default_shaders(pdefault_shaders)
    {}

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_glyph_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_image_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> m_colorstop_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar> m_painter_shader_registrar;
    fastuidraw::PainterBackend::ConfigurationBase m_config;
    fastuidraw::PainterBackend::PerformanceHints m_hints;
    fastuidraw::PainterShaderSet m_default_shaders;
  };

  class ConfigurationPrivate
  {
  public:
    ConfigurationPrivate(void):
      m_brush_shader_mask(0),
      m_composite_type(fastuidraw::PainterCompositeShader::dual_src),
      m_supports_bindless_texturing(false)
    {}

    uint32_t m_brush_shader_mask;
    enum fastuidraw::PainterCompositeShader::shader_type m_composite_type;
    bool m_supports_bindless_texturing;
  };
}

//////////////////////////////////////////////////
// fastuidraw::PainterBackend::PerformanceHints methods
fastuidraw::PainterBackend::PerformanceHints::
PerformanceHints(void)
{
  m_d = FASTUIDRAWnew PerformanceHintsPrivate();
}

fastuidraw::PainterBackend::PerformanceHints::
PerformanceHints(const PerformanceHints &obj)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PerformanceHintsPrivate(*d);
}

fastuidraw::PainterBackend::PerformanceHints::
~PerformanceHints(void)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterBackend::PerformanceHints)
setget_implement(fastuidraw::PainterBackend::PerformanceHints,
                 PerformanceHintsPrivate,
                 bool, clipping_via_hw_clip_planes)

///////////////////////////////////////////////////
// fastuidraw::PainterBackend::ConfigurationBase methods
fastuidraw::PainterBackend::ConfigurationBase::
ConfigurationBase(void)
{
  m_d = FASTUIDRAWnew ConfigurationPrivate();
}

fastuidraw::PainterBackend::ConfigurationBase::
ConfigurationBase(const ConfigurationBase &obj)
{
  ConfigurationPrivate *d;
  d = static_cast<ConfigurationPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationPrivate(*d);
}

fastuidraw::PainterBackend::ConfigurationBase::
~ConfigurationBase()
{
  ConfigurationPrivate *d;
  d = static_cast<ConfigurationPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterBackend::ConfigurationBase)
setget_implement(fastuidraw::PainterBackend::ConfigurationBase,
                 ConfigurationPrivate,
                 uint32_t, brush_shader_mask)
setget_implement(fastuidraw::PainterBackend::ConfigurationBase,
                 ConfigurationPrivate,
                 enum fastuidraw::PainterCompositeShader::shader_type, composite_type)
setget_implement(fastuidraw::PainterBackend::ConfigurationBase,
                 ConfigurationPrivate,
                 bool, supports_bindless_texturing)

/////////////////////////////////////////////////////////
// fastuidraw::PainterBackend::Surface::Viewport methods
void
fastuidraw::PainterBackend::Surface::Viewport::
compute_clip_equations(ivec2 surface_dims,
                       vecN<vec3, 4> *out_clip_equations) const
{
  Rect Rndc;

  compute_normalized_clip_rect(surface_dims, &Rndc);

  /* The clip equations are:
   *
   *  Rndc.m_min_point.x() <= x <= Rndc.m_max_point.x()
   *  Rndc.m_min_point.y() <= y <= Rndc.m_max_point.y()
   *
   * Which converts to
   *
   *  +1.0 * x - Rndc.m_min_point.x() * w >= 0
   *  +1.0 * y - Rndc.m_min_point.y() * w >= 0
   *  -1.0 * x + Rndc.m_max_point.x() * w >= 0
   *  -1.0 * y + Rndc.m_max_point.y() * w >= 0
   */
  (*out_clip_equations)[0] = vec3(+1.0f, 0.0f, -Rndc.m_min_point.x());
  (*out_clip_equations)[1] = vec3(0.0f, +1.0f, -Rndc.m_min_point.y());
  (*out_clip_equations)[2] = vec3(-1.0f, 0.0f, +Rndc.m_max_point.x());
  (*out_clip_equations)[3] = vec3(0.0f, -1.0f, +Rndc.m_max_point.y());
}

void
fastuidraw::PainterBackend::Surface::Viewport::
compute_normalized_clip_rect(ivec2 surface_dims,
                             Rect *out_rect) const
{
  out_rect->m_min_point = compute_normalized_device_coords(vec2(0.0f, 0.0f));
  out_rect->m_max_point = compute_normalized_device_coords(vec2(surface_dims));

  out_rect->m_min_point.x() = t_max(-1.0f, out_rect->m_min_point.x());
  out_rect->m_max_point.x() = t_min(+1.0f, out_rect->m_max_point.x());

  out_rect->m_min_point.y() = t_max(-1.0f, out_rect->m_min_point.y());
  out_rect->m_max_point.y() = t_min(+1.0f, out_rect->m_max_point.y());
}

////////////////////////////////////
// fastuidraw::PainterBackend methods
fastuidraw::PainterBackend::
PainterBackend(reference_counted_ptr<GlyphAtlas> glyph_atlas,
               reference_counted_ptr<ImageAtlas> image_atlas,
               reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
               reference_counted_ptr<PainterShaderRegistrar> shader_registrar,
               const ConfigurationBase &config,
               const PainterShaderSet &pdefault_shaders)
{
  PainterBackendPrivate *d;
  m_d = d = FASTUIDRAWnew PainterBackendPrivate(glyph_atlas, image_atlas, colorstop_atlas,
                                            shader_registrar, config, pdefault_shaders);
  d->m_painter_shader_registrar->register_shader(d->m_default_shaders);
}

fastuidraw::PainterBackend::
~PainterBackend()
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::PainterBackend::PerformanceHints&
fastuidraw::PainterBackend::
set_hints(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_hints;
}

const fastuidraw::PainterBackend::PerformanceHints&
fastuidraw::PainterBackend::
hints(void) const
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_hints;
}

const fastuidraw::PainterShaderSet&
fastuidraw::PainterBackend::
default_shaders(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_default_shaders;
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas>&
fastuidraw::PainterBackend::
glyph_atlas(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_glyph_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::PainterBackend::
image_atlas(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_image_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::PainterBackend::
colorstop_atlas(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_colorstop_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar>&
fastuidraw::PainterBackend::
painter_shader_registrar(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_painter_shader_registrar;
}

const fastuidraw::PainterBackend::ConfigurationBase&
fastuidraw::PainterBackend::
configuration_base(void) const
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  return d->m_config;
}
