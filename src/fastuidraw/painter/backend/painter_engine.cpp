/*!
 * \file painter_engine.cpp
 * \brief file painter_engine.cpp
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

#include <fastuidraw/painter/backend/painter_engine.hpp>
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

  class PainterEnginePrivate
  {
  public:
    PainterEnginePrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> glyph_atlas,
                         fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> image_atlas,
                         fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> colorstop_atlas,
                         fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar> shader_registrar,
                         const fastuidraw::PainterEngine::ConfigurationBase &config,
                         const fastuidraw::PainterShaderSet &pdefault_shaders):
      m_glyph_atlas(glyph_atlas),
      m_image_atlas(image_atlas),
      m_colorstop_atlas(colorstop_atlas),
      m_painter_shader_registrar(shader_registrar),
      m_config(config),
      m_default_shaders(pdefault_shaders)
    {
      m_glyph_cache = FASTUIDRAWnew fastuidraw::GlyphCache(m_glyph_atlas);
    }

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_glyph_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_image_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> m_colorstop_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_glyph_cache;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar> m_painter_shader_registrar;
    fastuidraw::PainterEngine::ConfigurationBase m_config;
    fastuidraw::PainterEngine::PerformanceHints m_hints;
    fastuidraw::PainterShaderSet m_default_shaders;
  };

  class ConfigurationPrivate
  {
  public:
    ConfigurationPrivate(void):
      m_supports_bindless_texturing(false)
    {}

    bool m_supports_bindless_texturing;
  };
}

//////////////////////////////////////////////////
// fastuidraw::PainterEngine::PerformanceHints methods
fastuidraw::PainterEngine::PerformanceHints::
PerformanceHints(void)
{
  m_d = FASTUIDRAWnew PerformanceHintsPrivate();
}

fastuidraw::PainterEngine::PerformanceHints::
PerformanceHints(const PerformanceHints &obj)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PerformanceHintsPrivate(*d);
}

fastuidraw::PainterEngine::PerformanceHints::
~PerformanceHints(void)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterEngine::PerformanceHints)
setget_implement(fastuidraw::PainterEngine::PerformanceHints,
                 PerformanceHintsPrivate,
                 bool, clipping_via_hw_clip_planes)
setget_implement(fastuidraw::PainterEngine::PerformanceHints,
                 PerformanceHintsPrivate,
                 int, max_z)

///////////////////////////////////////////////////
// fastuidraw::PainterEngine::ConfigurationBase methods
fastuidraw::PainterEngine::ConfigurationBase::
ConfigurationBase(void)
{
  m_d = FASTUIDRAWnew ConfigurationPrivate();
}

fastuidraw::PainterEngine::ConfigurationBase::
ConfigurationBase(const ConfigurationBase &obj)
{
  ConfigurationPrivate *d;
  d = static_cast<ConfigurationPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationPrivate(*d);
}

fastuidraw::PainterEngine::ConfigurationBase::
~ConfigurationBase()
{
  ConfigurationPrivate *d;
  d = static_cast<ConfigurationPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterEngine::ConfigurationBase)
setget_implement(fastuidraw::PainterEngine::ConfigurationBase,
                 ConfigurationPrivate,
                 bool, supports_bindless_texturing)

////////////////////////////////////
// fastuidraw::PainterEngine methods
fastuidraw::PainterEngine::
PainterEngine(reference_counted_ptr<GlyphAtlas> glyph_atlas,
               reference_counted_ptr<ImageAtlas> image_atlas,
               reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
               reference_counted_ptr<PainterShaderRegistrar> shader_registrar,
               const ConfigurationBase &config,
               const PainterShaderSet &pdefault_shaders)
{
  PainterEnginePrivate *d;
  m_d = d = FASTUIDRAWnew PainterEnginePrivate(glyph_atlas, image_atlas, colorstop_atlas,
                                               shader_registrar, config, pdefault_shaders);
  d->m_painter_shader_registrar->register_shader(d->m_default_shaders);
}

fastuidraw::PainterEngine::
~PainterEngine()
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::PainterEngine::PerformanceHints&
fastuidraw::PainterEngine::
set_hints(void)
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return d->m_hints;
}

const fastuidraw::PainterEngine::PerformanceHints&
fastuidraw::PainterEngine::
hints(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return d->m_hints;
}

const fastuidraw::PainterShaderSet&
fastuidraw::PainterEngine::
default_shaders(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return d->m_default_shaders;
}

fastuidraw::GlyphAtlas&
fastuidraw::PainterEngine::
glyph_atlas(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return *d->m_glyph_atlas;
}

fastuidraw::GlyphCache&
fastuidraw::PainterEngine::
glyph_cache(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return *d->m_glyph_cache;
}

fastuidraw::ImageAtlas&
fastuidraw::PainterEngine::
image_atlas(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return *d->m_image_atlas;
}

fastuidraw::ColorStopAtlas&
fastuidraw::PainterEngine::
colorstop_atlas(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return *d->m_colorstop_atlas;
}

fastuidraw::PainterShaderRegistrar&
fastuidraw::PainterEngine::
painter_shader_registrar(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return *d->m_painter_shader_registrar;
}

const fastuidraw::PainterEngine::ConfigurationBase&
fastuidraw::PainterEngine::
configuration_base(void) const
{
  PainterEnginePrivate *d;
  d = static_cast<PainterEnginePrivate*>(m_d);
  return d->m_config;
}
