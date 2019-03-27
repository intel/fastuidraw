/*!
 * \file font_metrics.cpp
 * \brief file font_metrics.cpp
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


#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/text/font_metrics.hpp>
#include <private/util_private.hpp>

namespace
{
  class FontMetricsPrivate
  {
  public:
    FontMetricsPrivate(void):
      m_height(0.0f),
      m_ascender(0.0f),
      m_descender(0.0f),
      m_units_per_EM(0.0f)
    {}

    float m_height, m_ascender, m_descender, m_units_per_EM;
  };
}

////////////////////////////////////////
// fastuidraw::FontMetrics methods
fastuidraw::FontMetrics::
FontMetrics(void)
{
  m_d = FASTUIDRAWnew FontMetricsPrivate();
}

fastuidraw::FontMetrics::
FontMetrics(const fastuidraw::FontMetrics &obj)
{
  FontMetricsPrivate *obj_d;
  obj_d = static_cast<FontMetricsPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew FontMetricsPrivate(*obj_d);
}

fastuidraw::FontMetrics::
~FontMetrics()
{
  FontMetricsPrivate *d;
  d = static_cast<FontMetricsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::FontMetrics)
setget_implement(fastuidraw::FontMetrics,
                 FontMetricsPrivate,
                 float, height)
setget_implement(fastuidraw::FontMetrics,
                 FontMetricsPrivate,
                 float, ascender)
setget_implement(fastuidraw::FontMetrics,
                 FontMetricsPrivate,
                 float, descender)
setget_implement(fastuidraw::FontMetrics,
                 FontMetricsPrivate,
                 float, units_per_EM)
