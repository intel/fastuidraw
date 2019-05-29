/*!
 * \file colorstop.cpp
 * \brief file colorstop.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#include <algorithm>
#include <vector>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/colorstop.hpp>
#include <private/util_private.hpp>

namespace
{
  class ColorStopArrayPrivate
  {
  public:
    ColorStopArrayPrivate(void):
      m_dirty(true)
    {}

    explicit
    ColorStopArrayPrivate(int rev):
      m_dirty(true)
    {
      m_values.reserve(rev);
    }

    std::vector<fastuidraw::ColorStop> m_values;
    bool m_dirty;
  };
}


/////////////////////////////////////
// fastuidraw::ColorStopArray methods
fastuidraw::ColorStopArray::
ColorStopArray(void):
  m_d(FASTUIDRAWnew ColorStopArrayPrivate())
{}

fastuidraw::ColorStopArray::
ColorStopArray(int reserve):
  m_d(FASTUIDRAWnew ColorStopArrayPrivate(reserve))
{}

fastuidraw::ColorStopArray::
~ColorStopArray()
{
  ColorStopArrayPrivate *d;
  d = static_cast<ColorStopArrayPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::ColorStopArray::
add(const ColorStop &c)
{
  ColorStopArrayPrivate *d;
  d = static_cast<ColorStopArrayPrivate*>(m_d);
  d->m_dirty = true;
  d->m_values.push_back(c);
}

void
fastuidraw::ColorStopArray::
clear(void)
{
  ColorStopArrayPrivate *d;
  d = static_cast<ColorStopArrayPrivate*>(m_d);
  d->m_dirty = true;
  d->m_values.clear();
}

fastuidraw::c_array<const fastuidraw::ColorStop>
fastuidraw::ColorStopArray::
values(void) const
{
  ColorStopArrayPrivate *d;
  d = static_cast<ColorStopArrayPrivate*>(m_d);
  if (d->m_dirty)
    {
      d->m_dirty = false;
      std::stable_sort(d->m_values.begin(), d->m_values.end());
    }
  return make_c_array(d->m_values);
}
