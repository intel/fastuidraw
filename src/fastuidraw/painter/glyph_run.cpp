/*!
 * \file glyph_run.cpp
 * \brief file glyph_run.cpp
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

#include <fastuidraw/painter/glyph_run.hpp>
#include <vector>
#include <map>
#include <algorithm>
#include "../private/util_private.hpp"

namespace
{
  class GlyphRunPrivate;

  class PerGlyphRender
  {
  public:
    void
    set_values(GlyphRunPrivate *p,
               fastuidraw::GlyphRenderer renderer);

    std::vector<fastuidraw::PainterAttribute> m_attribs;
    std::vector<fastuidraw::PainterIndex> m_indices;
  };

  class SubSequence:public fastuidraw::PainterPacker::DataWriter
  {
  public:
    void
    set_src(const PerGlyphRender *data, unsigned int begin, unsigned int cnt);

    virtual
    unsigned int
    number_attribute_chunks(void) const;

    virtual
    unsigned int
    number_attributes(unsigned int) const;

    virtual
    unsigned int
    number_index_chunks(void) const;

    virtual
    unsigned int
    number_indices(unsigned int) const;

    virtual
    unsigned int
    attribute_chunk_selection(unsigned int) const;

    virtual
    void
    write_indices(fastuidraw::c_array<fastuidraw::PainterIndex> dst,
                  unsigned int index_offset_value,
                  unsigned int) const;

    virtual
    void
    write_attributes(fastuidraw::c_array<fastuidraw::PainterAttribute> dst,
                     unsigned int attribute_chunk) const;

  private:
    fastuidraw::c_array<const fastuidraw::PainterIndex> m_indices;
    fastuidraw::c_array<const fastuidraw::PainterAttribute> m_attributes;
  };

  class GlyphLocation
  {
  public:
    fastuidraw::vec2 m_position;
    float m_scale;
  };

  class GlyphRunPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    GlyphRunPrivate(float pixel_size,
                    enum fastuidraw::PainterEnums::screen_orientation orientation,
                    const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> &cache,
                    enum fastuidraw::PainterEnums::glyph_layout_type layout):
      m_pixel_size(pixel_size),
      m_orientation(orientation),
      m_layout(layout),
      m_cache(cache)
    {
      FASTUIDRAWassert(cache);
    }

    float m_pixel_size;
    enum fastuidraw::PainterEnums::screen_orientation m_orientation;
    enum fastuidraw::PainterEnums::glyph_layout_type m_layout;
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_cache;

    std::vector<GlyphLocation> m_glyph_locations;
    std::vector<fastuidraw::GlyphMetrics> m_glyphs;

    std::map<fastuidraw::GlyphRenderer, PerGlyphRender> m_data;
    SubSequence m_subsequence;
  };

}

///////////////////////////////////////////
// SubSequence methods
void
SubSequence::
set_src(const PerGlyphRender *data, unsigned int begin, unsigned int cnt)
{
  m_indices = fastuidraw::make_c_array(data->m_indices).sub_array(6 * begin, 6 * cnt);
  m_attributes = fastuidraw::make_c_array(data->m_attribs).sub_array(4 * begin, 4 * cnt);
}

unsigned int
SubSequence::
number_attribute_chunks(void) const
{
  return 1;
}

unsigned int
SubSequence::
number_attributes(unsigned int) const
{
  return m_attributes.size();
}

unsigned int
SubSequence::
number_index_chunks(void) const
{
  return 1;
}

unsigned int
SubSequence::
number_indices(unsigned int) const
{
  return m_indices.size();
}

unsigned int
SubSequence::
attribute_chunk_selection(unsigned int) const
{
  return 0;
}

void
SubSequence::
write_indices(fastuidraw::c_array<fastuidraw::PainterIndex> dst,
              unsigned int index_offset_value,
              unsigned int) const
{
  for (unsigned int i = 0; i < dst.size(); ++i)
    {
      dst[i] = index_offset_value + m_indices[i];
    }
}

void
SubSequence::
write_attributes(fastuidraw::c_array<fastuidraw::PainterAttribute> dst,
                 unsigned int) const
{
  std::copy(m_attributes.begin(), m_attributes.end(), dst.begin());
}

//////////////////////////////////////////
// PerGlyphRender methods
void
PerGlyphRender::
set_values(GlyphRunPrivate *p,
           fastuidraw::GlyphRenderer renderer)
{
  using namespace fastuidraw;

  unsigned int num(p->m_glyph_locations.size());
  std::vector<Glyph> tmp_glyphs(num);
  c_array<const GlyphMetrics> glyph_metrics(make_c_array(p->m_glyphs));
  c_array<Glyph> glyphs(make_c_array(tmp_glyphs));

  m_attribs.resize(4 * num);
  m_indices.resize(6 * num);
  p->m_cache->fetch_glyphs(renderer, glyph_metrics, glyphs, true);

  for (unsigned int g = 0; g < num; ++g)
    {
      glyphs[g].pack_glyph(4 * g, fastuidraw::make_c_array(m_attribs),
                           6 * g, fastuidraw::make_c_array(m_indices),
                           p->m_glyph_locations[g].m_position,
                           p->m_glyph_locations[g].m_scale,
                           p->m_orientation,
                           p->m_layout);
    }
}

////////////////////////////////
// fastuidraw::GlyphRun methods
fastuidraw::GlyphRun::
GlyphRun(float pixel_size,
              enum PainterEnums::screen_orientation orientation,
              const reference_counted_ptr<GlyphCache> &cache,
              enum PainterEnums::glyph_layout_type layout)
{
  m_d = FASTUIDRAWnew GlyphRunPrivate(pixel_size, orientation, cache, layout);
}

fastuidraw::GlyphRun::
~GlyphRun()
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

float
fastuidraw::GlyphRun::
pixel_size(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return d->m_pixel_size;
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache>&
fastuidraw::GlyphRun::
glyph_cache(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return d->m_cache;
}

enum fastuidraw::PainterEnums::screen_orientation
fastuidraw::GlyphRun::
orientation(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return d->m_orientation;
}

enum fastuidraw::PainterEnums::glyph_layout_type
fastuidraw::GlyphRun::
layout(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return d->m_layout;
}

void
fastuidraw::GlyphRun::
add_glyphs(c_array<const GlyphSource> sources,
           c_array<const vec2> positions)
{
  unsigned int old_sz;
  GlyphRunPrivate *d;

  d = static_cast<GlyphRunPrivate*>(m_d);
  c_array<GlyphMetrics> dst_glyphs;

  old_sz = d->m_glyphs.size();
  d->m_glyphs.resize(sources.size() + old_sz);
  d->m_glyph_locations.reserve(sources.size() + old_sz);

  dst_glyphs = fastuidraw::make_c_array(d->m_glyphs).sub_array(old_sz);
  d->m_cache->fetch_glyph_metrics(sources, dst_glyphs);

  for (unsigned int i = 0; i < positions.size(); ++i)
    {
      GlyphLocation L;

      L.m_position = positions[i];
      if (dst_glyphs[i].valid())
        {
          L.m_scale = d->m_pixel_size / dst_glyphs[i].units_per_EM();
        }
      else
        {
          L.m_scale = 1.0f;
        }
      d->m_glyph_locations.push_back(L);
    }
  d->m_data.clear();
}

unsigned int
fastuidraw::GlyphRun::
number_glyphs(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return d->m_glyphs.size();
}

void
fastuidraw::GlyphRun::
added_glyph(unsigned int I,
	    GlyphMetrics *out_glyph_metrics,
	    vec2 *out_position) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);

  FASTUIDRAWassert(I < number_glyphs());
  *out_glyph_metrics = d->m_glyphs[I];
  *out_position = d->m_glyph_locations[I].m_position;
}

const fastuidraw::PainterPacker::DataWriter&
fastuidraw::GlyphRun::
subsequence(GlyphRenderer renderer, unsigned int begin, unsigned int cnt) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);

  if (d->m_glyphs.empty())
    {
      begin = 0u;
      cnt = 0u;
    }
  else
    {
      unsigned int max_v(d->m_glyphs.size() - 1u);

      begin = t_min(begin, max_v);
      cnt = t_min(cnt, max_v - begin);
    }

  const PerGlyphRender *data;
  std::map<fastuidraw::GlyphRenderer, PerGlyphRender>::const_iterator iter;

  iter = d->m_data.find(renderer);
  if (iter == d->m_data.end())
    {
      PerGlyphRender &p(d->m_data[renderer]);
      p.set_values(d, renderer);
      data = &p;
    }
  else
    {
      data = &iter->second;
    }

  d->m_subsequence.set_src(data, begin, cnt);
  return d->m_subsequence;
}

const fastuidraw::PainterPacker::DataWriter&
fastuidraw::GlyphRun::
subsequence(GlyphRenderer renderer) const
{
  return subsequence(renderer, 0, number_glyphs());
}
