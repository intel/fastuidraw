/*!
 * \file painter_blend_shader_set.cpp
 * \brief file painter_blend_shader_set.cpp
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
#include <fastuidraw/painter/painter_blend_shader_set.hpp>
#include "../private/util_private.hpp"

namespace
{
  class PainterBlendShaderSetPrivate
  {
  public:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader> shader_ref;
    typedef std::pair<shader_ref, fastuidraw::BlendMode::packed_value> entry;
    std::vector<entry> m_shaders;
    entry m_null;
  };
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
  d = static_cast<PainterBlendShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterBlendShaderSetPrivate(*d);
}

fastuidraw::PainterBlendShaderSet::
~PainterBlendShaderSet()
{
  PainterBlendShaderSetPrivate *d;
  d = static_cast<PainterBlendShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterBlendShaderSet)

const fastuidraw::reference_counted_ptr<fastuidraw::PainterBlendShader>&
fastuidraw::PainterBlendShaderSet::
shader(enum PainterEnums::blend_mode_t tp) const
{
  PainterBlendShaderSetPrivate *d;
  d = static_cast<PainterBlendShaderSetPrivate*>(m_d);
  return (tp < d->m_shaders.size()) ? d->m_shaders[tp].first : d->m_null.first;
}

fastuidraw::BlendMode::packed_value
fastuidraw::PainterBlendShaderSet::
blend_mode(enum PainterEnums::blend_mode_t tp) const
{
  PainterBlendShaderSetPrivate *d;
  d = static_cast<PainterBlendShaderSetPrivate*>(m_d);
  return (tp < d->m_shaders.size()) ? d->m_shaders[tp].second : d->m_null.second;
}

fastuidraw::PainterBlendShaderSet&
fastuidraw::PainterBlendShaderSet::
shader(enum PainterEnums::blend_mode_t tp,
       const BlendMode &mode,
       const reference_counted_ptr<PainterBlendShader> &sh)
{
  PainterBlendShaderSetPrivate *d;
  d = static_cast<PainterBlendShaderSetPrivate*>(m_d);
  if(tp >= d->m_shaders.size())
    {
      d->m_shaders.resize(tp + 1);
    }
  d->m_shaders[tp] = PainterBlendShaderSetPrivate::entry(sh, mode.packed());
  return *this;
}

unsigned int
fastuidraw::PainterBlendShaderSet::
shader_count(void) const
{
  PainterBlendShaderSetPrivate *d;
  d = static_cast<PainterBlendShaderSetPrivate*>(m_d);
  return d->m_shaders.size();
}
