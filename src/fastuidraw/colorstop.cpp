/*!
 * \file colorstop.cpp
 * \brief file colorstop.cpp
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


#include <algorithm>
#include <vector>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/colorstop.hpp>
#include "private/util_private.hpp"

namespace
{
  class ColorStopSequencePrivate
  {
  public:
    ColorStopSequencePrivate(void):
      m_dirty(true)
    {}

    explicit
    ColorStopSequencePrivate(int rev):
      m_dirty(true)
    {
      m_values.reserve(rev);
    }

    std::vector<fastuidraw::ColorStop> m_values;
    bool m_dirty;
  };
}


/////////////////////////////////////
// fastuidraw::ColorStopSequence methods
fastuidraw::ColorStopSequence::
ColorStopSequence(void):
  m_d(FASTUIDRAWnew ColorStopSequencePrivate())
{}

fastuidraw::ColorStopSequence::
ColorStopSequence(int reserve):
  m_d(FASTUIDRAWnew ColorStopSequencePrivate(reserve))
{}

fastuidraw::ColorStopSequence::
~ColorStopSequence()
{
  ColorStopSequencePrivate *d;
  d = static_cast<ColorStopSequencePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::ColorStopSequence::
add(const ColorStop &c)
{
  ColorStopSequencePrivate *d;
  d = static_cast<ColorStopSequencePrivate*>(m_d);
  d->m_dirty = true;
  d->m_values.push_back(c);
}

void
fastuidraw::ColorStopSequence::
clear(void)
{
  ColorStopSequencePrivate *d;
  d = static_cast<ColorStopSequencePrivate*>(m_d);
  d->m_dirty = true;
  d->m_values.clear();
}

fastuidraw::const_c_array<fastuidraw::ColorStop>
fastuidraw::ColorStopSequence::
values(void) const
{
  ColorStopSequencePrivate *d;
  d = static_cast<ColorStopSequencePrivate*>(m_d);
  if(d->m_dirty)
    {
      d->m_dirty = false;
      std::stable_sort(d->m_values.begin(), d->m_values.end());
    }
  return make_c_array(d->m_values);
}
