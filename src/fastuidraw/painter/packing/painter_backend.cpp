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
  class PainterBackendPrivate
  {
  public:
    PainterBackendPrivate(fastuidraw::GlyphAtlas::handle glyph_atlas,
                          fastuidraw::ImageAtlas::handle image_atlas,
                          fastuidraw::ColorStopAtlas::handle colorstop_atlas,
                          const fastuidraw::PainterBackend::Configuration &config,
                          const fastuidraw::PainterShaderSet &shaders):
      m_glyph_atlas(glyph_atlas),
      m_image_atlas(image_atlas),
      m_colorstop_atlas(colorstop_atlas),
      m_config(config),
      m_default_shaders(shaders),
      m_default_shaders_registered(false)
    {}

    fastuidraw::GlyphAtlas::handle m_glyph_atlas;
    fastuidraw::ImageAtlas::handle m_image_atlas;
    fastuidraw::ColorStopAtlas::handle m_colorstop_atlas;
    fastuidraw::PainterBackend::Configuration m_config;
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
PainterBackend(GlyphAtlas::handle glyph_atlas,
               ImageAtlas::handle image_atlas,
               ColorStopAtlas::handle colorstop_atlas,
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

void
fastuidraw::PainterBackend::
register_vert_shader(const PainterShader::handle &shader)
{
  assert(shader->registered_to() == NULL || shader->registered_to() == this);
  if(shader->registered_to() == NULL)
    {
      PainterShader::Tag tag;

      tag = absorb_vert_shader(shader);
      shader->register_shader(tag, this);
    }
}

void
fastuidraw::PainterBackend::
register_frag_shader(const PainterShader::handle &shader)
{
  assert(shader->registered_to() == NULL || shader->registered_to() == this);
  if(shader->registered_to() == NULL)
    {
      PainterShader::Tag tag;

      tag = absorb_frag_shader(shader);
      shader->register_shader(tag, this);
    }
}

void
fastuidraw::PainterBackend::
register_blend_shader(const PainterShader::handle &shader)
{
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

      const PainterShader::handle &sh(p.shader(tp));
      if(sh)
        {
          register_blend_shader(sh);
        }
    }
}

void
fastuidraw::PainterBackend::
register_shader(const PainterShaderSet &shaders)
{
  register_shader(shaders.stroke_shader());
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
register_shader(const PainterItemShader &p)
{
  register_vert_shader(p.vert_shader());
  register_frag_shader(p.frag_shader());
}

void
fastuidraw::PainterBackend::
register_shader(const PainterStrokeShader &p)
{
  register_shader(p.non_aa_shader());
  register_shader(p.aa_shader_pass1());
  register_shader(p.aa_shader_pass2());
}

const fastuidraw::GlyphAtlas::handle&
fastuidraw::PainterBackend::
glyph_atlas(void)
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_glyph_atlas;
}

const fastuidraw::ImageAtlas::handle&
fastuidraw::PainterBackend::
image_atlas(void)
{
  PainterBackendPrivate *d;
  d = reinterpret_cast<PainterBackendPrivate*>(m_d);
  return d->m_image_atlas;
}

const fastuidraw::ColorStopAtlas::handle&
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
