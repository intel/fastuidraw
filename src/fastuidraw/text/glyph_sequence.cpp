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
#include <vector>
#include <map>
#include <algorithm>
#include "../private/util_private.hpp"

namespace
{
  typedef std::map<fastuidraw::GlyphRender, std::vector<fastuidraw::Glyph> > glyph_storage;
  class GlyphSequencePrivate
  {
  public:
    explicit
    GlyphSequencePrivate(const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> &cache):
      m_cache(cache)
    {
      FASTUIDRAWassert(cache);
    }

    std::vector<fastuidraw::GlyphSource> m_glyph_sources;
    std::vector<fastuidraw::vec2> m_glyph_positions;
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_cache;
    glyph_storage m_glyphs;
  };
}

fastuidraw::GlyphSequence::
GlyphSequence(const reference_counted_ptr<GlyphCache> &cache)
{
  m_d = FASTUIDRAWnew GlyphSequencePrivate(cache);
}

fastuidraw::GlyphSequence::
~GlyphSequence()
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::GlyphSequence::
add_glyphs(c_array<const GlyphSource> sources,
           c_array<const vec2> positions)
{
  GlyphSequencePrivate *d;
  unsigned int old_sz;

  d = static_cast<GlyphSequencePrivate*>(m_d);
  old_sz = d->m_glyph_sources.size();

  FASTUIDRAWassert(d->m_glyph_positions.size() == old_sz);
  FASTUIDRAWassert(sources.size() == positions.size());

  if (sources.empty())
    {
      return;
    }

  d->m_glyph_sources.resize(old_sz + sources.size());
  d->m_glyph_positions.resize(old_sz + positions.size());

  d->m_glyphs.clear();
  std::copy(sources.begin(), sources.end(), d->m_glyph_sources.begin() + old_sz);
  std::copy(positions.begin(), positions.end(), d->m_glyph_positions.begin() + old_sz);
}

fastuidraw::c_array<const fastuidraw::GlyphSource>
fastuidraw::GlyphSequence::
glyph_sources(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return make_c_array(d->m_glyph_sources);
}

fastuidraw::c_array<const fastuidraw::vec2>
fastuidraw::GlyphSequence::
glyph_positions(void) const
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);
  return make_c_array(d->m_glyph_positions);
}

fastuidraw::c_array<const fastuidraw::Glyph>
fastuidraw::GlyphSequence::
glyph_sequence(GlyphRender render)
{
  GlyphSequencePrivate *d;
  d = static_cast<GlyphSequencePrivate*>(m_d);

  glyph_storage::iterator iter;
  iter = d->m_glyphs.find(render);

  if (iter != d->m_glyphs.end())
    {
      return make_c_array(iter->second);
    }

  std::vector<fastuidraw::Glyph> &dst(d->m_glyphs[render]);
  dst.resize(d->m_glyph_sources.size());
  d->m_cache->fetch_glyphs(render,
                           make_c_array(d->m_glyph_sources),
                           make_c_array(dst));

  return make_c_array(dst);
}
