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
#include <private/util_private.hpp>

namespace
{
  class PerformanceHintsPrivate
  {
  public:
    PerformanceHintsPrivate(void):
      m_clipping_via_hw_clip_planes(true),
      m_max_z(1u << 20)
    {}

    bool m_clipping_via_hw_clip_planes;
    int m_max_z;
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
      m_default_shaders(pdefault_shaders),
      m_in_use(false)
    {}

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_glyph_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_image_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> m_colorstop_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar> m_painter_shader_registrar;
    fastuidraw::PainterBackend::ConfigurationBase m_config;
    fastuidraw::PainterBackend::PerformanceHints m_hints;
    fastuidraw::PainterShaderSet m_default_shaders;
    bool m_in_use;
  };

  class ConfigurationPrivate
  {
  public:
    ConfigurationPrivate(void):
      m_brush_shader_mask(0),
      m_supports_bindless_texturing(false)
    {}

    uint32_t m_brush_shader_mask;
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
setget_implement(fastuidraw::PainterBackend::PerformanceHints,
                 PerformanceHintsPrivate,
                 int, max_z)

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
                 bool, supports_bindless_texturing)

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

void
fastuidraw::PainterBackend::
mark_as_used(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  FASTUIDRAWassert(!d->m_in_use);
  d->m_in_use = true;
}

void
fastuidraw::PainterBackend::
mark_as_free(void)
{
  PainterBackendPrivate *d;
  d = static_cast<PainterBackendPrivate*>(m_d);
  FASTUIDRAWassert(d->m_in_use);
  d->m_in_use = false;
}
