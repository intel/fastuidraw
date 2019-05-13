/*!
 * \file glyph_render_data_texels.cpp
 * \brief file glyph_render_data_texles.cpp
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
#include <fastuidraw/text/glyph_render_data_texels.hpp>
#include <private/pack_texels.hpp>
#include <private/util_private.hpp>
#include <private/util_private_ostream.hpp>

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
// fastuidraw::GlyphRenderDataTexels methods
fastuidraw::GlyphRenderDataTexels::
GlyphRenderDataTexels(void)
{
  m_d = FASTUIDRAWnew GlyphDataPrivate();
}

fastuidraw::GlyphRenderDataTexels::
~GlyphRenderDataTexels(void)
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::ivec2
fastuidraw::GlyphRenderDataTexels::
resolution(void) const
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  return d->m_resolution;
}

fastuidraw::c_array<const uint8_t>
fastuidraw::GlyphRenderDataTexels::
texel_data(void) const
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  return make_c_array(d->m_texels);
}

fastuidraw::c_array<uint8_t>
fastuidraw::GlyphRenderDataTexels::
texel_data(void)
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  return make_c_array(d->m_texels);
}

void
fastuidraw::GlyphRenderDataTexels::
resize(fastuidraw::ivec2 sz)
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);
  d->resize(sz);
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::GlyphRenderDataTexels::
render_info_labels(void) const
{
  return c_array<const c_string>();
}

enum fastuidraw::return_code
fastuidraw::GlyphRenderDataTexels::
upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                GlyphAttribute::Array &attributes,
                c_array<float> /* render_costs */) const
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(m_d);

  attributes.resize(2);
  attributes[0].pack_texel_rect(d->m_resolution.x(),
                                d->m_resolution.y());

  if (d->m_texels.empty())
    {
      attributes[1].m_data = vecN<uint32_t, 4>(0u);
      return routine_success;
    }

  std::vector<uint32_t> data;
  int location;

  detail::pack_texels(uvec2(d->m_resolution),
                      make_c_array(d->m_texels),
                      &data);
  location = atlas_proxy.allocate_data(make_c_array(data));
  if (location == -1)
    {
      return routine_fail;
    }
  attributes[1].m_data = vecN<uint32_t, 4>(location);

  return routine_success;
}
