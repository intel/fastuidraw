/*!
 * \file painter_composite_shader_set.cpp
 * \brief file painter_composite_shader_set.cpp
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
#include <fastuidraw/painter/painter_composite_shader_set.hpp>
#include "../private/util_private.hpp"

namespace
{
  class PainterCompositeShaderSetPrivate
  {
  public:
    typedef fastuidraw::reference_counted_ptr<fastuidraw::PainterCompositeShader> shader_ref;
    typedef std::pair<shader_ref, fastuidraw::BlendMode> entry;
    std::vector<entry> m_shaders;
    entry m_null;
  };
}

///////////////////////////////////////
// fastuidraw::PainterCompositeShaderSet methods
fastuidraw::PainterCompositeShaderSet::
PainterCompositeShaderSet(void)
{
  m_d = FASTUIDRAWnew PainterCompositeShaderSetPrivate();
}

fastuidraw::PainterCompositeShaderSet::
PainterCompositeShaderSet(const PainterCompositeShaderSet &obj)
{
  PainterCompositeShaderSetPrivate *d;
  d = static_cast<PainterCompositeShaderSetPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PainterCompositeShaderSetPrivate(*d);
}

fastuidraw::PainterCompositeShaderSet::
~PainterCompositeShaderSet()
{
  PainterCompositeShaderSetPrivate *d;
  d = static_cast<PainterCompositeShaderSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::PainterCompositeShaderSet)

const fastuidraw::reference_counted_ptr<fastuidraw::PainterCompositeShader>&
fastuidraw::PainterCompositeShaderSet::
shader(enum PainterEnums::composite_mode_t tp) const
{
  PainterCompositeShaderSetPrivate *d;
  d = static_cast<PainterCompositeShaderSetPrivate*>(m_d);
  return (tp < d->m_shaders.size()) ? d->m_shaders[tp].first : d->m_null.first;
}

fastuidraw::BlendMode
fastuidraw::PainterCompositeShaderSet::
composite_mode(enum PainterEnums::composite_mode_t tp) const
{
  PainterCompositeShaderSetPrivate *d;
  d = static_cast<PainterCompositeShaderSetPrivate*>(m_d);
  return (tp < d->m_shaders.size()) ? d->m_shaders[tp].second : d->m_null.second;
}

fastuidraw::PainterCompositeShaderSet&
fastuidraw::PainterCompositeShaderSet::
shader(enum PainterEnums::composite_mode_t tp,
       const BlendMode &mode,
       const reference_counted_ptr<PainterCompositeShader> &sh)
{
  PainterCompositeShaderSetPrivate *d;
  d = static_cast<PainterCompositeShaderSetPrivate*>(m_d);
  if (tp >= d->m_shaders.size())
    {
      d->m_shaders.resize(tp + 1);
    }
  d->m_shaders[tp] = PainterCompositeShaderSetPrivate::entry(sh, mode);
  return *this;
}

unsigned int
fastuidraw::PainterCompositeShaderSet::
shader_count(void) const
{
  PainterCompositeShaderSetPrivate *d;
  d = static_cast<PainterCompositeShaderSetPrivate*>(m_d);
  return d->m_shaders.size();
}
