/*!
 * \file painter_effect_color_modulate.cpp
 * \brief file painter_effect_color_modulate.cpp
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

#include <fastuidraw/painter/painter_effect_color_modulate.hpp>
#include <fastuidraw/painter/painter_brush.hpp>

namespace
{
  class ModulatePass:public fastuidraw::PainterEffectPass
  {
  public:
    virtual
    fastuidraw::PainterData::brush_value
    brush(const fastuidraw::reference_counted_ptr<const fastuidraw::Image> &image) override
    {
      m_brush.image(image);
    }

    fastuidraw::PainterBrush m_brush;
  };
}

////////////////////////////////////////////////
// fastuidraw::PainterEffectColorModulate methods
fastuidraw::PainterEffectColorModulate::
PainterEffectColorModulate(void)
{
  add_pass(FASTUIDRAWnew ModulatePass());
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterEffect>
fastuidraw::PainterEffectColorModulate::
copy(void) const
{
  PainterEffectColorModulate *p;

  p = FASTUIDRAWnew PainterEffectColorModulate();
  p->color(color());
  return p;
}

void
fastuidraw::PainterEffectColorModulate::
color(const vec4 &color)
{
  ModulatePass *q;
  c_array<reference_counted_ptr<PainterEffectPass> > p(passes());

  FASTUIDRAWassert(p[0].dynamic_cast_ptr<ModulatePass>());
  q = static_cast<ModulatePass*>(p[0].get());
  q->m_brush.color(color);
}

const fastuidraw::vec4&
fastuidraw::PainterEffectColorModulate::
color(void) const
{
  ModulatePass *q;
  c_array<reference_counted_ptr<PainterEffectPass> > p(passes());

  FASTUIDRAWassert(p[0].dynamic_cast_ptr<ModulatePass>());
  q = static_cast<ModulatePass*>(p[0].get());
  return q->m_brush.color();
}
