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


#include <fastuidraw/painter/packing/painter_backend.hpp>

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
                          const fastuidraw::PainterBackend::Configuration &config,
                          const fastuidraw::PainterShaderSet &shaders):
      m_glyph_atlas(glyph_atlas),
      m_image_atlas(image_atlas),
      m_colorstop_atlas(colorstop_atlas),
      m_config(config),
      m_default_shaders(shaders),
      m_default_shaders_registered(false)
    {}

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_glyph_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_image_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> m_colorstop_atlas;
    fastuidraw::PainterBackend::Configuration m_config;
    fastuidraw::PainterBackend::PerformanceHints m_hints;
    fastuidraw::PainterShaderSet m_default_shaders;
    bool m_default_shaders_registered;
  };

  class ConfigurationPrivate
  {
  public:
    ConfigurationPrivate(void):
      m_brush_shader_mask(0),
      m_alignment(4)
    {}

    uint32_t m_brush_shader_mask;
    int m_alignment;
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
~PerformanceHints(void)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

bool
fastuidraw::PainterBackend::PerformanceHints::
clipping_via_hw_clip_planes(void) const
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(m_d);
  return d->m_clipping_via_hw_clip_planes;
}

fastuidraw::PainterBackend::PerformanceHints&
fastuidraw::PainterBackend::PerformanceHints::
clipping_via_hw_clip_planes(bool v)
{
  PerformanceHintsPrivate *d;
  d = static_cast<PerformanceHintsPrivate*>(m_d);
  d->m_clipping_via_hw_clip_planes = v;
  return *this;
}

///////////////////////////////////////////////////
// fastuidraw::PainterBackend::Configuration methods
fastuidraw::PainterBackend::Configuration::
Configuration(void)
{
  m_d = FASTUIDRAWnew ConfigurationPrivate();
}

fastuidraw::PainterBackend::Configuration::
Configuration(const Configuration &obj)
{
  ConfigurationPrivate *d;
  d = reinterpret_cast<ConfigurationPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationPrivate(*d);
}

fastuidraw::PainterBackend::Configuration::
~Configuration()
{
  ConfigurationPrivate *d;
  d = reinterpret_cast<ConfigurationPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterBackend::Configuration&
fastuidraw::PainterBackend::Configuration::
operator=(const Configuration &obj)
{
  ConfigurationPrivate *d, *obj_d;
  d = reinterpret_cast<ConfigurationPrivate*>(m_d);
  obj_d = reinterpret_cast<ConfigurationPrivate*>(obj.m_d);
  *d = *obj_d;
  return *this;
}

uint32_t
fastuidraw::PainterBackend::Configuration::
brush_shader_mask(void) const
{
  ConfigurationPrivate *d;
  d = reinterpret_cast<ConfigurationPrivate*>(m_d);
  return d->m_brush_shader_mask;
}

fastuidraw::PainterBackend::Configuration&
fastuidraw::PainterBackend::Configuration::
brush_shader_mask(uint32_t v)
{
  ConfigurationPrivate *d;
  d = reinterpret_cast<ConfigurationPrivate*>(m_d);
  d->m_brush_shader_mask = v;
  return *this;
}

int
fastuidraw::PainterBackend::Configuration::
alignment(void) const
{
  ConfigurationPrivate *d;
  d = reinterpret_cast<ConfigurationPrivate*>(m_d);
  return d->m_alignment;
}

fastuidraw::PainterBackend::Configuration&
fastuidraw::PainterBackend::Configuration::
alignment(int v)
{
  ConfigurationPrivate *d;
  d = reinterpret_cast<ConfigurationPrivate*>(m_d);
  d->m_alignment = v;
  return *this;
}

////////////////////////////////////
// fastuidraw::PainterBackend methods
fastuidraw::PainterBackend::
PainterBackend(reference_counted_ptr<GlyphAtlas> glyph_atlas,
               reference_counted_ptr<ImageAtlas> image_atlas,
               reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
               const Configuration &config,
               const PainterShaderSet &shaders)
{
  m_d = FASTUIDRAWnew PainterBackendPrivate(glyph_atlas, image_atlas, colorstop_atlas,
                                           config, shaders);
}

fastuidraw::PainterBackend::
~PainterBackend()
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterBackend::PerformanceHints&
fastuidraw::PainterBackend::
set_hints(void)
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_hints;
}

const fastuidraw::PainterBackend::PerformanceHints&
fastuidraw::PainterBackend::
hints(void) const
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_hints;
}

void
fastuidraw::PainterBackend::
register_shader(const reference_counted_ptr<PainterItemShader> &shader)
{
  if(!shader)
    {
      return;
    }
  assert(shader->registered_to() == NULL || shader->registered_to() == this);
  if(shader->registered_to() == NULL)
    {
      PainterShader::Tag tag;

      tag = absorb_item_shader(shader);
      shader->register_shader(tag, this);
    }
}

void
fastuidraw::PainterBackend::
register_shader(const reference_counted_ptr<PainterBlendShader> &shader)
{
  if(!shader)
    {
      return;
    }
  assert(shader->registered_to() == NULL || shader->registered_to() == this);
  if(shader->registered_to() == NULL)
    {
      PainterShader::Tag tag;

      tag = absorb_blend_shader(shader);
      shader->register_shader(tag, this);
    }
}

void
fastuidraw::PainterBackend::
register_shader(const PainterGlyphShader &shader)
{
  for(unsigned int i = 0, endi = shader.shader_count(); i < endi; ++i)
    {
      register_shader(shader.shader(static_cast<enum glyph_type>(i)));
    }
}

void
fastuidraw::PainterBackend::
register_shader(const PainterBlendShaderSet &p)
{
  for(unsigned int i = 0, endi = p.shader_count(); i < endi; ++i)
    {
      enum PainterEnums::blend_mode_t tp;
      tp = static_cast<enum PainterEnums::blend_mode_t>(i);

      const reference_counted_ptr<PainterBlendShader> &sh(p.shader(tp));
      register_shader(sh);
    }
}

void
fastuidraw::PainterBackend::
register_shader(const PainterShaderSet &shaders)
{
  register_shader(shaders.stroke_shader());
  register_shader(shaders.pixel_width_stroke_shader());
  register_shader(shaders.dashed_stroke_shader());
  register_shader(shaders.pixel_width_dashed_stroke_shader());
  register_shader(shaders.fill_shader());
  register_shader(shaders.glyph_shader());
  register_shader(shaders.glyph_shader_anisotropic());
  register_shader(shaders.blend_shaders());
}


const fastuidraw::PainterShaderSet&
fastuidraw::PainterBackend::
default_shaders(void)
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);

  if(!d->m_default_shaders_registered)
    {
      register_shader(d->m_default_shaders);
      d->m_default_shaders_registered = true;
    }
  return d->m_default_shaders;
}

void
fastuidraw::PainterBackend::
register_shader(const PainterStrokeShader &p)
{
  register_shader(p.non_aa_shader());
  register_shader(p.aa_shader_pass1());
  register_shader(p.aa_shader_pass2());
}

void
fastuidraw::PainterBackend::
register_shader(const PainterDashedStrokeShaderSet &p)
{
  for(int i = 0; i < PainterEnums::number_dashed_cap_styles; ++i)
    {
      enum PainterEnums::dashed_cap_style c;
      c = static_cast<enum PainterEnums::dashed_cap_style>(i);
      register_shader(p.shader(c));
    }
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas>&
fastuidraw::PainterBackend::
glyph_atlas(void)
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_glyph_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas>&
fastuidraw::PainterBackend::
image_atlas(void)
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_image_atlas;
}

const fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas>&
fastuidraw::PainterBackend::
colorstop_atlas(void)
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_colorstop_atlas;
}

const fastuidraw::PainterBackend::Configuration&
fastuidraw::PainterBackend::
configuration(void) const
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_config;
}
