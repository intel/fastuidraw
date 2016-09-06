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

  class PainterShaderPrivate
  {
  public:
    PainterShaderPrivate(unsigned int num_sub_shaders):
      m_registered_to(NULL),
      m_number_sub_shaders(num_sub_shaders),
      m_sub_shader_ID(0)
    {
    }

    fastuidraw::PainterShader::Tag m_tag;
    const fastuidraw::PainterBackend *m_registered_to;

    //for when shader has sub-shaders
    unsigned int m_number_sub_shaders;

    //for when shader is a sub-shader
    fastuidraw::reference_counted_ptr<fastuidraw::PainterShader> m_parent;
    unsigned int m_sub_shader_ID;
  };
}

///////////////////////////////////////////
// fastuidraw::PainterShader methods
fastuidraw::PainterShader::
PainterShader(unsigned int num_sub_shaders)
{
  m_d = FASTUIDRAWnew PainterShaderPrivate(num_sub_shaders);
}

fastuidraw::PainterShader::
PainterShader(unsigned int sub_shader,
              reference_counted_ptr<PainterShader> parent)
{
  PainterShaderPrivate *d;
  m_d = d = FASTUIDRAWnew PainterShaderPrivate(1);

  assert(parent);
  assert(sub_shader < parent->number_sub_shaders());

  d->m_parent = parent;
  d->m_sub_shader_ID = sub_shader;
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
sub_shader(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  return d->m_sub_shader_ID;
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

unsigned int
fastuidraw::PainterShader::
number_sub_shaders(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  return d->m_number_sub_shaders;
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterShader>&
fastuidraw::PainterShader::
parent(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  return d->m_parent;
}

void
fastuidraw::PainterShader::
register_shader(Tag tg, const PainterBackend *p)
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  assert(d->m_registered_to == NULL);
  assert(!d->m_parent);
  d->m_tag = tg;
  d->m_registered_to = p;
}

void
fastuidraw::PainterShader::
set_group_of_sub_shader(uint32_t gr)
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);

  assert(d->m_parent);
  PainterShaderPrivate *pd;
  pd = reinterpret_cast<PainterShaderPrivate*>(d->m_parent->m_d);

  /* the parent must all-ready be registered.
   */
  assert(pd->m_registered_to != NULL);

  //but this shader is not yet registered!
  assert(d->m_registered_to == NULL);

  d->m_registered_to = pd->m_registered_to;
  d->m_tag.m_ID = pd->m_tag.m_ID + d->m_sub_shader_ID;
  d->m_tag.m_group = gr;
}

const fastuidraw::PainterBackend*
fastuidraw::PainterShader::
registered_to(void) const
{
  PainterShaderPrivate *d;
  d = reinterpret_cast<PainterShaderPrivate*>(m_d);
  return d->m_registered_to;
}
