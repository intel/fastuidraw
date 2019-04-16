/*!
 * \file painter_backend_factory.cpp
 * \brief file painter_backend_factory.cpp
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

#include <fastuidraw/painter/backend/painter_backend_factory.hpp>
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

  class PainterBackendFactoryPrivate
  {
  public:
    PainterBackendFactoryPrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> glyph_atlas,
                                 fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> image_atlas,
                                 fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> colorstop_atlas,
                                 fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar> shader_registrar,
                                 const fastuidraw::PainterBackendFactory::ConfigurationBase &config,
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
    fastuidraw::PainterBackendFactory::ConfigurationBase m_config;
    fastuidraw::PainterBackendFactory::PerformanceHints m_hints;
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
// fastuidraw::PainterBackendFactory::PerformanceHints methods
fastuidraw::PainterBackendFactory::PerformanceHints::
PerformanceHints(void)
{
  m_d = FASTUIDRAWnew PerformanceHintsPrivate();
}

fastuidraw::PainterBackendFactory::PerformanceHints::
PerformanceHints(const PerformanceHints &obj)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PerformanceHintsPrivate(*d);
}

fastuidraw::PainterBackendFactory::PerformanceHints::
~PerformanceHints(void)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterBackendFactory::PerformanceHints)
setget_implement(fastuidraw::PainterBackendFactory::PerformanceHints,
                 PerformanceHintsPrivate,
                 bool, clipping_via_hw_clip_planes)
setget_implement(fastuidraw::PainterBackendFactory::PerformanceHints,
                 PerformanceHintsPrivate,
                 int, max_z)

///////////////////////////////////////////////////
// fastuidraw::PainterBackendFactory::ConfigurationBase methods
fastuidraw::PainterBackendFactory::ConfigurationBase::
ConfigurationBase(void)
{
  m_d = FASTUIDRAWnew ConfigurationPrivate();
}

fastuidraw::PainterBackendFactory::ConfigurationBase::
ConfigurationBase(const ConfigurationBase &obj)
{
  ConfigurationPrivate *d;
  d = static_cast<ConfigurationPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationPrivate(*d);
}

fastuidraw::PainterBackendFactory::ConfigurationBase::
~ConfigurationBase()
{
  ConfigurationPrivate *d;
  d = static_cast<ConfigurationPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterBackendFactory::ConfigurationBase)
setget_implement(fastuidraw::PainterBackendFactory::ConfigurationBase,
                 ConfigurationPrivate,
                 bool, supports_bindless_texturing)

////////////////////////////////////
// fastuidraw::PainterBackendFactory methods
fastuidraw::PainterBackendFactory::
PainterBackendFactory(reference_counted_ptr<GlyphAtlas> glyph_atlas,
               reference_counted_ptr<ImageAtlas> image_atlas,
               reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
               reference_counted_ptr<PainterShaderRegistrar> shader_registrar,
               const ConfigurationBase &config,
               const PainterShaderSet &pdefault_shaders)
{
  PainterBackendFactoryPrivate *d;
  m_d = d = FASTUIDRAWnew PainterBackendFactoryPrivate(glyph_atlas, image_atlas, colorstop_atlas,
                                            shader_registrar, config, pdefault_shaders);
  d->m_painter_shader_registrar->register_shader(d->m_default_shaders);
}

fastuidraw::PainterBackendFactory::
~PainterBackendFactory()
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::PainterBackendFactory::PerformanceHints&
fastuidraw::PainterBackendFactory::
set_hints(void)
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_hints;
}

const fastuidraw::PainterBackendFactory::PerformanceHints&
fastuidraw::PainterBackendFactory::
hints(void) const
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_hints;
}

const fastuidraw::PainterShaderSet&
fastuidraw::PainterBackendFactory::
default_shaders(void) const
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_default_shaders;
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas>&
fastuidraw::PainterBackendFactory::
glyph_atlas(void) const
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_glyph_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::PainterBackendFactory::
image_atlas(void) const
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_image_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::PainterBackendFactory::
colorstop_atlas(void) const
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_colorstop_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterShaderRegistrar>&
fastuidraw::PainterBackendFactory::
painter_shader_registrar(void) const
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_painter_shader_registrar;
}

const fastuidraw::PainterBackendFactory::ConfigurationBase&
fastuidraw::PainterBackendFactory::
configuration_base(void) const
{
  PainterBackendFactoryPrivate *d;
  d = static_cast<PainterBackendFactoryPrivate*>(m_d);
  return d->m_config;
}
