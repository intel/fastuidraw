/*!
 * \file glyph_layout_data.cpp
 * \brief file glyph_layout_data.cpp
 *
 * Copyright 2018 by Intel.
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

#include <fastuidraw/text/glyph_layout_data.hpp>
#include <fastuidraw/text/font.hpp>
#include "../private/util_private.hpp"

namespace
{
  class GlyphLayoutDataPrivate
  {
  public:
    GlyphLayoutDataPrivate(void):
      m_glyph_code(0),
      m_font(),
      m_horizontal_layout_offset(0.0f, 0.0f),
      m_vertical_layout_offset(0.0f, 0.0f),
      m_size(0.0f, 0.0f),
      m_advance(0.0f, 0.0f),
      m_units_per_EM(0u)
    {}

    uint32_t m_glyph_code;
    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> m_font;
    fastuidraw::vec2 m_horizontal_layout_offset;
    fastuidraw::vec2 m_vertical_layout_offset;
    fastuidraw::vec2 m_size, m_advance;
    float m_units_per_EM;
  };
}

//////////////////////////////////////////
// fastuidraw::GlyphLayoutData methods
fastuidraw::GlyphLayoutData::
GlyphLayoutData(void)
{
  m_d = FASTUIDRAWnew GlyphLayoutDataPrivate();
}

fastuidraw::GlyphLayoutData::
GlyphLayoutData(const GlyphLayoutData &obj)
{
  GlyphLayoutDataPrivate *d;
  d = static_cast<GlyphLayoutDataPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew GlyphLayoutDataPrivate(*d);
}

fastuidraw::GlyphLayoutData::
~GlyphLayoutData()
{
  GlyphLayoutDataPrivate *d;
  d = static_cast<GlyphLayoutDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::GlyphLayoutData);
setget_implement(fastuidraw::GlyphLayoutData, GlyphLayoutDataPrivate, uint32_t, glyph_code)
setget_implement(fastuidraw::GlyphLayoutData, GlyphLayoutDataPrivate, fastuidraw::vec2, horizontal_layout_offset)
setget_implement(fastuidraw::GlyphLayoutData, GlyphLayoutDataPrivate, fastuidraw::vec2, vertical_layout_offset)
setget_implement(fastuidraw::GlyphLayoutData, GlyphLayoutDataPrivate, fastuidraw::vec2, size)
setget_implement(fastuidraw::GlyphLayoutData, GlyphLayoutDataPrivate, fastuidraw::vec2, advance)
setget_implement(fastuidraw::GlyphLayoutData, GlyphLayoutDataPrivate, float, units_per_EM)
setget_implement(fastuidraw::GlyphLayoutData, GlyphLayoutDataPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>&, font)
