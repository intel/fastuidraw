/*!
 * \file painter_shader.cpp
 * \brief file painter_shader.cpp
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


#include <vector>
#include <fastuidraw/painter/painter_shader.hpp>

namespace
{
  class PainterGlyphShaderPrivate
  {
  public:
    std::vector<fastuidraw::PainterItemShader> m_shaders;
    fastuidraw::PainterItemShader m_null;
  };

  class PainterBlendShaderSetPrivate
  {
  public:
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterShader> > m_shaders;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterShader> m_null;
  };

  class PainterShaderSetPrivate
  {
  public:
    fastuidraw::PainterGlyphShader m_glyph_shader, m_glyph_shader_anisotropic;
    fastuidraw::PainterStrokeShader m_stroke_shader;
    fastuidraw::PainterStrokeShader m_pixel_width_stroke_shader;
    fastuidraw::PainterItemShader m_fill_shader;
    fastuidraw::PainterBlendShaderSet m_blend_shaders;
  };

  class PainterShaderPrivate
  {
  public:
    PainterShaderPrivate(void):
      m_registered_to(NULL)
    {
    }

    fastuidraw::PainterShader::Tag m_tag;
    const fastuidraw::PainterBackend *m_registered_to;
  };

  class PainterItemShaderPrivate
  {
  public:
    fastuidraw::reference_counted_ptr<fastuidraw::PainterShader> m_vert_shader;
    fastuidraw::reference_counted_ptr<fastuidraw::PainterShader> m_frag_shader;
  };

  class PainterStrokeShaderPrivate
  {
  public:
    PainterStrokeShaderPrivate(void):
      m_aa_type(fastuidraw::PainterStrokeShader::draws_solid_then_fuzz)
    {}

    fastuidraw::PainterItemShader m_aa_shader_pass1;
    fastuidraw::PainterItemShader m_aa_shader_pass2;
    fastuidraw::PainterItemShader m_non_aa_shader;
    enum fastuidraw::PainterStrokeShader::type_t m_aa_type;
  };
}

///////////////////////////////////////////
// fastuidraw::PainterShader methods
fastuidraw::PainterShader::
PainterShader(void)
{
  m_d = FASTUIDRAWnew PainterShaderPrivate();
}

fastuidraw::PainterShader::
~PainterShader()
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

uint32_t
fastuidraw::PainterShader::
ID(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  assert(d->m_registered_to != NULL);
  return d->m_tag.m_ID;
}

uint32_t
fastuidraw::PainterShader::
group(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  assert(d->m_registered_to != NULL);
  return d->m_tag.m_group;
}

fastuidraw::PainterShader::Tag
fastuidraw::PainterShader::
tag(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  assert(d->m_registered_to != NULL);
  return d->m_tag;
}

void
fastuidraw::PainterShader::
register_shader(Tag tg, const PainterBackend *p)
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  assert(d->m_registered_to == NULL);
  d->m_tag = tg;
  d->m_registered_to = p;
}

const fastuidraw::PainterBackend*
fastuidraw::PainterShader::
registered_to(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  return d->m_registered_to;
}

///////////////////////////////////////
// fastuidraw::PainterGlyphShader methods
fastuidraw::PainterGlyphShader::
PainterGlyphShader(void)
{
  m_d = FASTUIDRAWnew PainterGlyphShaderPrivate();
}

fastuidraw::PainterGlyphShader::
PainterGlyphShader(const PainterGlyphShader &obj)
{
  PainterGlyphShaderPrivate *d;
  d = reinterpret_cast<PainterGlyphShaderPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterGlyphShaderPrivate(*d);
}

fastuidraw::PainterGlyphShader::
~PainterGlyphShader()
{
  PainterGlyphShaderPrivate *d;
  d = reinterpret_cast<PainterGlyphShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterGlyphShader&
fastuidraw::PainterGlyphShader::
operator=(const PainterGlyphShader &rhs)
{
  if(this != &rhs)
    {
      PainterGlyphShaderPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterGlyphShaderPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterGlyphShaderPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

const fastuidraw::PainterItemShader&
fastuidraw::PainterGlyphShader::
shader(enum glyph_type tp) const
{
  PainterGlyphShaderPrivate *d;
  d = reinterpret_cast<PainterGlyphShaderPrivate*>(m_d);
  return (tp < d->m_shaders.size()) ? d->m_shaders[tp] : d->m_null;
}


void
fastuidraw::PainterGlyphShader::
shader(enum glyph_type tp, const PainterItemShader &sh)
{
  PainterGlyphShaderPrivate *d;
  d = reinterpret_cast<PainterGlyphShaderPrivate*>(m_d);
  if(tp >= d->m_shaders.size())
    {
      d->m_shaders.resize(tp + 1);
    }
  d->m_shaders[tp] = sh;
}

unsigned int
fastuidraw::PainterGlyphShader::
shader_count(void) const
{
  PainterGlyphShaderPrivate *d;
  d = reinterpret_cast<PainterGlyphShaderPrivate*>(m_d);
  return d->m_shaders.size();
}



///////////////////////////////////////
// fastuidraw::PainterBlendShaderSet methods
fastuidraw::PainterBlendShaderSet::
PainterBlendShaderSet(void)
{
  m_d = FASTUIDRAWnew PainterBlendShaderSetPrivate();
}

fastuidraw::PainterBlendShaderSet::
PainterBlendShaderSet(const PainterBlendShaderSet &obj)
{
  PainterBlendShaderSetPrivate *d;
  d = reinterpret_cast<PainterBlendShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterBlendShaderSetPrivate(*d);
}

fastuidraw::PainterBlendShaderSet::
~PainterBlendShaderSet()
{
  PainterBlendShaderSetPrivate *d;
  d = reinterpret_cast<PainterBlendShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterBlendShaderSet&
fastuidraw::PainterBlendShaderSet::
operator=(const PainterBlendShaderSet &rhs)
{
  if(this != &rhs)
    {
      PainterBlendShaderSetPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterBlendShaderSetPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterBlendShaderSetPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterShader>&
fastuidraw::PainterBlendShaderSet::
shader(enum PainterEnums::blend_mode_t tp) const
{
  PainterBlendShaderSetPrivate *d;
  d = reinterpret_cast<PainterBlendShaderSetPrivate*>(m_d);
  return (tp < d->m_shaders.size()) ? d->m_shaders[tp] : d->m_null;
}


void
fastuidraw::PainterBlendShaderSet::
shader(enum PainterEnums::blend_mode_t tp,
       const reference_counted_ptr<PainterShader> &sh)
{
  PainterBlendShaderSetPrivate *d;
  d = reinterpret_cast<PainterBlendShaderSetPrivate*>(m_d);
  if(tp >= d->m_shaders.size())
    {
      d->m_shaders.resize(tp + 1);
    }
  d->m_shaders[tp] = sh;
}

unsigned int
fastuidraw::PainterBlendShaderSet::
shader_count(void) const
{
  PainterBlendShaderSetPrivate *d;
  d = reinterpret_cast<PainterBlendShaderSetPrivate*>(m_d);
  return d->m_shaders.size();
}


//////////////////////////////////////////////
// fastuidraw::PainterShaderSet methods
fastuidraw::PainterShaderSet::
PainterShaderSet(void)
{
  m_d = FASTUIDRAWnew PainterShaderSetPrivate();
}

fastuidraw::PainterShaderSet::
PainterShaderSet(const PainterShaderSet &obj)
{
  PainterShaderSetPrivate *d;
  d = reinterpret_cast<PainterShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterShaderSetPrivate(*d);
}

fastuidraw::PainterShaderSet::
~PainterShaderSet()
{
  PainterShaderSetPrivate *d;
  d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterShaderSet&
fastuidraw::PainterShaderSet::
operator=(const PainterShaderSet &rhs)
{
  if(this != &rhs)
    {
      PainterShaderSetPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterShaderSetPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                            \
  fastuidraw::PainterShaderSet&                                  \
  fastuidraw::PainterShaderSet::                                 \
  name(const type &v)                                           \
  {                                                             \
    PainterShaderSetPrivate *d;                                 \
    d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);        \
    d->m_##name = v;                                            \
    return *this;                                               \
  }                                                             \
                                                                \
  const type&                                                   \
  fastuidraw::PainterShaderSet::                                 \
  name(void) const                                              \
  {                                                             \
    PainterShaderSetPrivate *d;                                 \
    d = reinterpret_cast<PainterShaderSetPrivate*>(m_d);        \
    return d->m_##name;                                         \
  }

setget_implement(fastuidraw::PainterGlyphShader, glyph_shader)
setget_implement(fastuidraw::PainterGlyphShader, glyph_shader_anisotropic)
setget_implement(fastuidraw::PainterStrokeShader, stroke_shader)
setget_implement(fastuidraw::PainterStrokeShader, pixel_width_stroke_shader)
setget_implement(fastuidraw::PainterItemShader, fill_shader)
setget_implement(fastuidraw::PainterBlendShaderSet, blend_shaders)

#undef setget_implement

//////////////////////////////////////////
// fastuidraw::PainterItemShader methods
fastuidraw::PainterItemShader::
PainterItemShader(void)
{
  m_d = FASTUIDRAWnew PainterItemShaderPrivate();
}

fastuidraw::PainterItemShader::
PainterItemShader(const PainterItemShader &obj)
{
  PainterItemShaderPrivate *d;
  d = reinterpret_cast<PainterItemShaderPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterItemShaderPrivate(*d);
}

fastuidraw::PainterItemShader::
~PainterItemShader()
{
  PainterItemShaderPrivate *d;
  d = reinterpret_cast<PainterItemShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterItemShader&
fastuidraw::PainterItemShader::
operator=(const PainterItemShader &rhs)
{
  if(this != &rhs)
    {
      PainterItemShaderPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterItemShaderPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterItemShaderPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                             \
  fastuidraw::PainterItemShader&                                  \
  fastuidraw::PainterItemShader::                                 \
  name(const type &v)                                            \
  {                                                              \
    PainterItemShaderPrivate *d;                                 \
    d = reinterpret_cast<PainterItemShaderPrivate*>(m_d);        \
    d->m_##name = v;                                             \
      return *this;                                              \
  }                                                              \
                                                                 \
  const type&                                                    \
  fastuidraw::PainterItemShader::                                 \
  name(void) const                                               \
  {                                                              \
    PainterItemShaderPrivate *d;                                 \
    d = reinterpret_cast<PainterItemShaderPrivate*>(m_d);        \
    return d->m_##name;                                          \
  }

setget_implement(fastuidraw::reference_counted_ptr<fastuidraw::PainterShader>, vert_shader)
setget_implement(fastuidraw::reference_counted_ptr<fastuidraw::PainterShader>, frag_shader)

#undef setget_implement



//////////////////////////////////////////
// fastuidraw::PainterStrokeShader methods
fastuidraw::PainterStrokeShader::
PainterStrokeShader(void)
{
  m_d = FASTUIDRAWnew PainterStrokeShaderPrivate();
}

fastuidraw::PainterStrokeShader::
PainterStrokeShader(const PainterStrokeShader &obj)
{
  PainterStrokeShaderPrivate *d;
  d = reinterpret_cast<PainterStrokeShaderPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterStrokeShaderPrivate(*d);
}

fastuidraw::PainterStrokeShader::
~PainterStrokeShader()
{
  PainterStrokeShaderPrivate *d;
  d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::PainterStrokeShader&
fastuidraw::PainterStrokeShader::
operator=(const PainterStrokeShader &rhs)
{
  if(this != &rhs)
    {
      PainterStrokeShaderPrivate *d, *rhs_d;
      d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);
      rhs_d = reinterpret_cast<PainterStrokeShaderPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

#define setget_implement(type, name)                               \
  fastuidraw::PainterStrokeShader&                                  \
  fastuidraw::PainterStrokeShader::                                 \
  name(const type &v)                                              \
  {                                                                \
    PainterStrokeShaderPrivate *d;                                 \
    d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);        \
    d->m_##name = v;                                               \
      return *this;                                                \
  }                                                                \
                                                                   \
  const type&                                                      \
  fastuidraw::PainterStrokeShader::                                 \
  name(void) const                                                 \
  {                                                                \
    PainterStrokeShaderPrivate *d;                                 \
    d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);        \
    return d->m_##name;                                            \
  }

#define setget_non_ref_implement(type, name)                        \
  fastuidraw::PainterStrokeShader&                                  \
  fastuidraw::PainterStrokeShader::                                 \
  name(type v)                                                      \
  {                                                                 \
    PainterStrokeShaderPrivate *d;                                  \
    d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);         \
    d->m_##name = v;                                                \
      return *this;                                                 \
  }                                                                 \
                                                                    \
  type                                                              \
  fastuidraw::PainterStrokeShader::                                 \
  name(void) const                                                  \
  {                                                                 \
    PainterStrokeShaderPrivate *d;                                  \
    d = reinterpret_cast<PainterStrokeShaderPrivate*>(m_d);         \
    return d->m_##name;                                             \
  }

setget_implement(fastuidraw::PainterItemShader, aa_shader_pass1)
setget_implement(fastuidraw::PainterItemShader, aa_shader_pass2)
setget_implement(fastuidraw::PainterItemShader, non_aa_shader)
setget_non_ref_implement(enum fastuidraw::PainterStrokeShader::type_t, aa_type);

#undef setget_implement
#undef setget_non_ref_implement
