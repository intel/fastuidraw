/*!
 * \file painter_effect_brush.cpp
 * \brief file painter_effect_brush.cpp
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

#include <fastuidraw/painter/effects/painter_effect_brush.hpp>
#include <fastuidraw/painter/painter_brush.hpp>

////////////////////////////////////////////////
// fastuidraw::PainterEffectBrush methods
unsigned int
fastuidraw::PainterEffectBrush::
number_passes(void) const
{
  return 1;
}

fastuidraw::PainterData::brush_value
fastuidraw::PainterEffectBrush::
brush(unsigned pass,
      const reference_counted_ptr<const Image> &image,
      const Rect &brush_rect,
      PainterEffectParams &params) const
{
  PainterEffectBrushParams *v;

  FASTUIDRAWassert(pass == 0);
  FASTUIDRAWunused(pass);
  FASTUIDRAWunused(brush_rect);
  FASTUIDRAWassert(dynamic_cast<PainterEffectBrushParams*>(&params));

  v = static_cast<PainterEffectBrushParams*>(&params);
  v->m_brush.image(image,
                   PainterBrush::filter_nearest,
                   PainterBrush::dont_apply_mipmapping);
  return fastuidraw::PainterData::brush_value(&v->m_brush);
}
