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
#include <fastuidraw/painter/shader/painter_shader.hpp>
#include <fastuidraw/painter/backend/painter_shader_registrar.hpp>

namespace
{
  class PerRegistrar
  {
  public:
    PerRegistrar(void):
      m_registered(false)
    {}

    fastuidraw::PainterShader::Tag m_tag;
    bool m_registered;
  };

  class PainterShaderPrivate
  {
  public:
    PainterShaderPrivate(unsigned int num_sub_shaders):
      m_number_sub_shaders(num_sub_shaders),
      m_sub_shader_ID(0)
    {
    }

    const fastuidraw::PainterShader::Tag&
    get_tag(const fastuidraw::PainterShaderRegistrar &sh)
    {
      unsigned int N(sh.unique_id());

      FASTUIDRAWassert(m_tags.size() > N);
      FASTUIDRAWassert(m_tags[N].m_registered);
      return m_tags[N].m_tag;
    }

    PerRegistrar&
    create_get(const fastuidraw::PainterShaderRegistrar &sh)
    {
      unsigned int N(sh.unique_id());

      if (N >= m_tags.size())
        {
          m_tags.resize(N + 1);
        }
      return m_tags[N];
    }

    std::vector<PerRegistrar> m_tags;

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
PainterShader(reference_counted_ptr<PainterShader> parent,
              unsigned int sub_shader)
{
  PainterShaderPrivate *d;
  m_d = d = FASTUIDRAWnew PainterShaderPrivate(1);

  FASTUIDRAWassert(parent);
  FASTUIDRAWassert(sub_shader < parent->number_sub_shaders());

  d->m_parent = parent;
  d->m_sub_shader_ID = sub_shader;
}

fastuidraw::PainterShader::
~PainterShader()
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

uint32_t
fastuidraw::PainterShader::
sub_shader(void) const
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);
  return d->m_sub_shader_ID;
}

uint32_t
fastuidraw::PainterShader::
ID(const PainterShaderRegistrar &sh) const
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);
  return d->get_tag(sh).m_ID;
}

uint32_t
fastuidraw::PainterShader::
group(const PainterShaderRegistrar &sh) const
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);
  return d->get_tag(sh).m_group;
}

fastuidraw::PainterShader::Tag
fastuidraw::PainterShader::
tag(const PainterShaderRegistrar &sh) const
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);
  return d->get_tag(sh);
}

unsigned int
fastuidraw::PainterShader::
number_sub_shaders(void) const
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);
  return d->m_number_sub_shaders;
}

const fastuidraw::reference_counted_ptr<fastuidraw::PainterShader>&
fastuidraw::PainterShader::
parent(void) const
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);
  return d->m_parent;
}

void
fastuidraw::PainterShader::
register_shader(Tag tg, const PainterShaderRegistrar &p)
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);

  PerRegistrar &pr(d->create_get(p));

  FASTUIDRAWassert(!pr.m_registered);
  pr.m_tag = tg;
  pr.m_registered = true;
}

bool
fastuidraw::PainterShader::
registered_to(const PainterShaderRegistrar &sh) const
{
  unsigned int N(sh.unique_id());
  PainterShaderPrivate *d;

  d = static_cast<PainterShaderPrivate*>(m_d);
  return d->m_tags.size() > N && d->m_tags[N].m_registered;
}

void
fastuidraw::PainterShader::
set_group_of_sub_shader(const PainterShaderRegistrar &p, uint32_t gr)
{
  PainterShaderPrivate *d;
  d = static_cast<PainterShaderPrivate*>(m_d);

  FASTUIDRAWassert(d->m_parent);
  PainterShaderPrivate *pd;
  pd = static_cast<PainterShaderPrivate*>(d->m_parent->m_d);

  PerRegistrar &dpr(d->create_get(p));
  const Tag &pdtag(pd->get_tag(p));

  /* this shader is not yet registered! */
  FASTUIDRAWassert(!dpr.m_registered);

  dpr.m_tag.m_ID = pdtag.m_ID + d->m_sub_shader_ID;
  dpr.m_tag.m_group = gr;
  dpr.m_registered = true;
}
