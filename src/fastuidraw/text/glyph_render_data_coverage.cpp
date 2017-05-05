/*!
 * \file glyph_render_data_coverage.cpp
 * \brief file glyph_render_data_coverage.cpp
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
#include <fastuidraw/text/glyph_render_data_coverage.hpp>
#include "../private/util_private.hpp"

namespace
{
  class GlyphDataPrivate
  {
  public:
    GlyphDataPrivate(void):
      m_resolution(0, 0)
    {}

    void
    resize(fastuidraw::ivec2 sz)
    {
      FASTUIDRAWassert(sz.x() >= 0);
      FASTUIDRAWassert(sz.y() >= 0);
      m_texels.resize(sz.x() * sz.y());
      m_resolution = sz;
    }

    fastuidraw::ivec2 m_resolution;
    std::vector<uint8_t> m_texels;
  };
}

////////////////////////////////////
// fastuidraw::GlyphRenderDataCoverage methods
fastuidraw::GlyphRenderDataCoverage::
GlyphRenderDataCoverage(void)
{
  m_d = FASTUIDRAWnew GlyphDataPrivate();
}

fastuidraw::GlyphRenderDataCoverage::
~GlyphRenderDataCoverage(void)
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec2
fastuidraw::GlyphRenderDataCoverage::
resolution(void) const
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  return d->m_resolution;
}

fastuidraw::const_c_array<uint8_t>
fastuidraw::GlyphRenderDataCoverage::
coverage_values(void) const
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  return make_c_array(d->m_texels);
}

fastuidraw::c_array<uint8_t>
fastuidraw::GlyphRenderDataCoverage::
coverage_values(void)
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  return make_c_array(d->m_texels);
}

void
fastuidraw::GlyphRenderDataCoverage::
resize(fastuidraw::ivec2 sz)
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  d->resize(sz);
}

enum fastuidraw::return_code
fastuidraw::GlyphRenderDataCoverage::
upload_to_atlas(const reference_counted_ptr<GlyphAtlas> &atlas,
                GlyphLocation &atlas_location,
                GlyphLocation &secondary_atlas_location,
                int &geometry_offset,
                int &geometry_length) const
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);

  GlyphAtlas::Padding padding;
  padding.m_right = 1;
  padding.m_bottom = 1;
  atlas_location = atlas->allocate(d->m_resolution, make_c_array(d->m_texels), padding);
  secondary_atlas_location = GlyphLocation();
  geometry_offset = -1;
  geometry_length = 0;

  return atlas_location.valid() ?
    routine_success :
    routine_fail;
}
