/*!
 * \file glyph_sequence.cpp
 * \brief file glyph_sequence.cpp
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

#include <fastuidraw/text/glyph_sequence.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_glyphs.hpp>
#include <vector>
#include <map>
#include <algorithm>
#include "../private/util_private.hpp"

namespace
{
  class PerGlyphSequence:
    public std::vector<fastuidraw::Glyph>
  {
  public:
    PerGlyphSequence(void):
      m_uploaded_to_atlas(false),
      m_attribute_data(nullptr)
    {}

    PerGlyphSequence(const PerGlyphSequence &obj):
      std::vector<fastuidraw::Glyph>(obj)
    {
      FASTUIDRAWassert(obj.empty());
      FASTUIDRAWassert(!obj.m_uploaded_to_atlas);
      FASTUIDRAWassert(!obj.m_attribute_data);
    }

    ~PerGlyphSequence()
    {
      if (m_attribute_data)
        {
          FASTUIDRAWdelete(m_attribute_data);
        }
    }

    void
    upload_to_atlas(void);

    const fastuidraw::PainterAttributeData&
    attribute_data(const std::vector<fastuidraw::vec2> &glyph_positions,
                   float pixel_size,
                   enum fastuidraw::PainterEnums::screen_orientation orientation);

  private:
    bool m_uploaded_to_atlas;
    fastuidraw::PainterAttributeData *m_attribute_data;
  };

  class GlyphSequencePrivate
  {
  public:
    explicit
    GlyphSequencePrivate(float pixel_size,
                         enum fastuidraw::PainterEnums::screen_orientation orientation,
                         const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> &cache):
      m_pixel_size(pixel_size),
      m_orientation(orientation),
      m_cache(cache)
    {
      FASTUIDRAWassert(cache);
    }

    float
    pixel_size(void)
    {
      return m_pixel_size;
    }

    enum fastuidraw::PainterEnums::screen_orientation
    orientation(void)
    {
      return m_orientation;
    }

    const std::vector<fastuidraw::GlyphSource>&
    glyph_sources(void)
    {
      return m_glyph_sources;
    }

    const std::vector<fastuidraw::vec2>&
    glyph_positions(void)
    {
      return m_glyph_positions;
    }

    const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache>&
    cache(void) const
    {
      return m_cache;
    }

    PerGlyphSequence&
    glyph_data(fastuidraw::GlyphRender render);

    void
    add_glyphs(fastuidraw::c_array<const fastuidraw::GlyphSource> sources,
               fastuidraw::c_array<const fastuidraw::vec2> positions);

  private:
    float m_pixel_size;
    enum fastuidraw::PainterEnums::screen_orientation m_orientation;
    std::vector<fastuidraw::GlyphSource> m_glyph_sources;
    std::vector<fastuidraw::vec2> m_glyph_positions;
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_cache;
    std::map<fastuidraw::GlyphRender, PerGlyphSequence> m_glyphs;
  };
}

////////////////////////////////////////
// PerGlyphSequence methods
void
PerGlyphSequence::
upload_to_atlas(void)
{
  if(!m_uploaded_to_atlas)
    {
      m_uploaded_to_atlas = true;
      for (const auto &g : *this)
        {
          g.upload_to_atlas();
        }
    }
}

const fastuidraw::PainterAttributeData&
PerGlyphSequence::
attribute_data(const std::vector<fastuidraw::vec2> &glyph_positions,
               float pixel_size,
               enum fastuidraw::PainterEnums::screen_orientation orientation)
{
  using namespace fastuidraw;

  if (!m_attribute_data)
    {
      upload_to_atlas();
      PainterAttributeDataFillerGlyphs filler(make_c_array(glyph_positions),
                                              make_c_array(*this),
                                              pixel_size, orientation);
      m_attribute_data = FASTUIDRAWnew PainterAttributeData();
      m_attribute_data->set_data(filler);
    }
  return *m_attribute_data;
}

/////////////////////////////////
// GlyphSequencePrivate methods
void
GlyphSequencePrivate::
add_glyphs(fastuidraw::c_array<const fastuidraw::GlyphSource> sources,
           fastuidraw::c_array<const fastuidraw::vec2> positions)
{
  unsigned int old_sz;
  old_sz = m_glyph_sources.size();

  FASTUIDRAWassert(m_glyph_positions.size() == old_sz);
  FASTUIDRAWassert(sources.size() == positions.size());

  if (sources.empty())
    {
      return;
    }

  m_glyph_sources.resize(old_sz + sources.size());
  m_glyph_positions.resize(old_sz + positions.size());

  m_glyphs.clear();
  std::copy(sources.begin(), sources.end(), m_glyph_sources.begin() + old_sz);
  std::copy(positions.begin(), positions.end(), m_glyph_positions.begin() + old_sz);
}

PerGlyphSequence&
GlyphSequencePrivate::
glyph_data(fastuidraw::GlyphRender render)
{
  std::map<fastuidraw::GlyphRender, PerGlyphSequence>::iterator iter;
  iter = m_glyphs.find(render);

  if (iter != m_glyphs.end())
    {
      return iter->second;
    }

  PerGlyphSequence &dst(m_glyphs[render]);
  dst.resize(m_glyph_sources.size());
  m_cache->fetch_glyphs(render,
                        fastuidraw::make_c_array(m_glyph_sources),
                        fastuidraw::make_c_array(dst),
                        false);

  return dst;
}

////////////////////////////////////////
// fastuidraw::GlyphSequence methods
fastuidraw::GlyphSequence::
GlyphSequence(float pixel_size,
              enum PainterEnums::screen_orientation orientation,
              const reference_counted_ptr<GlyphCache> &cache)
{
  m_d = FASTUIDRAWnew GlyphSequencePrivate(pixel_size, orientation, cache);
}

fastuidraw::GlyphSequence::
~GlyphSequence()
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::c_array<const fastuidraw::GlyphSource>
fastuidraw::GlyphSequence::
glyph_sources(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return make_c_array(d->glyph_sources());
}

fastuidraw::c_array<const fastuidraw::vec2>
fastuidraw::GlyphSequence::
glyph_positions(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return make_c_array(d->glyph_positions());
}

float
fastuidraw::GlyphSequence::
pixel_size(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->pixel_size();
}

const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache>&
fastuidraw::GlyphSequence::
glyph_cache(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->cache();
}

enum fastuidraw::PainterEnums::screen_orientation
fastuidraw::GlyphSequence::
orientation(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return d->orientation();
}

void
fastuidraw::GlyphSequence::
add_glyphs(c_array<const GlyphSource> sources,
           c_array<const vec2> positions)
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  d->add_glyphs(sources, positions);
}

fastuidraw::c_array<const fastuidraw::Glyph>
fastuidraw::GlyphSequence::
glyph_sequence(GlyphRender render, bool upload_to_atlas)
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);

  PerGlyphSequence &v(d->glyph_data(render));
  if (upload_to_atlas)
    {
      v.upload_to_atlas();
    }

  return make_c_array(v);
}

const fastuidraw::PainterAttributeData&
fastuidraw::GlyphSequence::
painter_attribute_data(GlyphRender render)
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);

  return d->glyph_data(render).attribute_data(d->glyph_positions(),
                                              d->pixel_size(),
                                              d->orientation());
}
