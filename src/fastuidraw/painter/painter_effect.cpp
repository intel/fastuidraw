/*!
 * \file painter_effect.cpp
 * \brief file painter_effect.cpp
 *
 * Copyright 2019 by Intel.
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

#include <fastuidraw/painter/painter_effect.hpp>
#include <private/util_private.hpp>
#include <vector>

namespace
{
  class PainterEffectPrivate
  {
  public:
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PainterEffectPass> > m_passes;
  };
}

///////////////////////////////////////
// fastuidraw::PainterEffect methods
fastuidraw::PainterEffect::
PainterEffect(void)
{
  m_d = FASTUIDRAWnew PainterEffectPrivate();
}

fastuidraw::PainterEffect::
~PainterEffect()
{
  PainterEffectPrivate *d;
  d = static_cast<PainterEffectPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::PainterEffect::
add_pass(const reference_counted_ptr<PainterEffectPass> &effect)
{
  PainterEffectPrivate *d;
  d = static_cast<PainterEffectPrivate*>(m_d);
  d->m_passes.push_back(effect);
}

fastuidraw::c_array<fastuidraw::reference_counted_ptr<fastuidraw::PainterEffectPass> >
fastuidraw::PainterEffect::
passes(void) const
{
  PainterEffectPrivate *d;
  d = static_cast<PainterEffectPrivate*>(m_d);
  return make_c_array(d->m_passes);
}
