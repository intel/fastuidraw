/*!
 * \file glyph_cache.cpp
 * \brief file glyph_cache.cpp
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


#include <map>
#include <vector>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include "../private/util_private.hpp"


namespace
{

  class GlyphCachePrivate;

  class GlyphDataPrivate
  {
  public:
    GlyphDataPrivate(GlyphCachePrivate *c, unsigned int I);

    explicit
    GlyphDataPrivate(void);

    void
    clear(void);

    enum fastuidraw::return_code
    upload_to_atlas(void);

    /* owner
     */
    GlyphCachePrivate *m_cache;

    /* location into m_cache->m_glyphs
     */
    unsigned int m_cache_location;

    /* layout magicks
     */
    fastuidraw::GlyphLayoutData m_layout;
    fastuidraw::GlyphRender m_render;

    /* Location in atlas */
    std::vector<fastuidraw::GlyphLocation> m_atlas_locations;
    int m_geometry_offset, m_geometry_length;
    bool m_uploaded_to_atlas;

    /* Path of the glyph */
    fastuidraw::Path m_path;

    /* data to generate glyph data */
    fastuidraw::GlyphRenderData *m_glyph_data;
  };

  class GlyphSource
  {
  public:
    GlyphSource(void):
      m_glyph_code(0)
    {}

    GlyphSource(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> f,
                uint32_t gc, fastuidraw::GlyphRender r):
      m_font(f),
      m_glyph_code(gc),
      m_render(r)
    {}

    bool
    operator<(const GlyphSource &rhs) const
    {
      return (m_font != rhs.m_font) ? m_font < rhs.m_font :
        (m_glyph_code != rhs.m_glyph_code) ? m_glyph_code < rhs.m_glyph_code :
        m_render < rhs.m_render;
    }

    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> m_font;
    uint32_t m_glyph_code;
    fastuidraw::GlyphRender m_render;
  };

  class GlyphCachePrivate
  {
  public:
    explicit
    GlyphCachePrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> patlas,
                      fastuidraw::GlyphCache *p);

    ~GlyphCachePrivate();

    /* When the atlas is full, we will clear the atlas, but save
     *  the values in m_glyphs but mark them as not having been
     *  uploaded, this way returned values are safe and we do
     *  not have to regenerate data either.
     */

    GlyphDataPrivate*
    fetch_or_allocate_glyph(GlyphSource src);

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_atlas;
    std::map<GlyphSource, GlyphDataPrivate*> m_glyph_map;
    std::vector<GlyphDataPrivate*> m_glyphs;
    std::vector<unsigned int> m_free_slots;
    fastuidraw::GlyphCache *m_p;
  };
}

/////////////////////////////////////////////////////////
// GlyphDataPrivate methods
GlyphDataPrivate::
GlyphDataPrivate(GlyphCachePrivate *c, unsigned int I):
  m_cache(c),
  m_cache_location(I),
  m_geometry_offset(-1),
  m_geometry_length(0),
  m_uploaded_to_atlas(false),
  m_glyph_data(nullptr)
{}

GlyphDataPrivate::
GlyphDataPrivate(void):
  m_cache(nullptr),
  m_cache_location(~0u),
  m_geometry_offset(-1),
  m_geometry_length(0),
  m_uploaded_to_atlas(false),
  m_glyph_data(nullptr)
{}

void
GlyphDataPrivate::
clear(void)
{
  m_render = fastuidraw::GlyphRender();
  FASTUIDRAWassert(!m_render.valid());

  if (m_cache)
    {
      for (const fastuidraw::GlyphLocation &g : m_atlas_locations)
        {
          FASTUIDRAWassert(g.valid());
          m_cache->m_atlas->deallocate(g);
        }
      m_atlas_locations.clear();

      if (m_geometry_offset != -1)
        {
          m_cache->m_atlas->deallocate_geometry_data(m_geometry_offset, m_geometry_length);
          m_geometry_offset = -1;
          m_geometry_length = 0;
        }
    }

  m_uploaded_to_atlas = false;
  if (m_glyph_data)
    {
      FASTUIDRAWdelete(m_glyph_data);
      m_glyph_data = nullptr;
    }
  m_path.clear();
}

enum fastuidraw::return_code
GlyphDataPrivate::
upload_to_atlas(void)
{
  using namespace fastuidraw;

  /* TODO:
   * 1. this method is not thread safe if different threads
   *    attempt to access the same glyph (or if different
   *    threads call this routine and clear_atlas()
   *    at the same time).
   */
  enum return_code return_value;
  unsigned int num;

  if (m_uploaded_to_atlas)
    {
      return routine_success;
    }

  if (!m_cache)
    {
      return routine_fail;
    }

  FASTUIDRAWassert(m_glyph_data);
  num = m_glyph_data->number_glyph_locations();
  m_atlas_locations.resize(num);
  return_value = m_glyph_data->upload_to_atlas(m_cache->m_atlas,
                                               make_c_array(m_atlas_locations),
                                               m_geometry_offset,
                                               m_geometry_length);
  if (return_value == routine_success)
    {
      m_uploaded_to_atlas = true;
    }

  return return_value;
}



/////////////////////////////////////////////////
// GlyphCachePrivate methods
GlyphCachePrivate::
GlyphCachePrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> patlas,
                  fastuidraw::GlyphCache *p):
  m_atlas(patlas),
  m_p(p)
{}

GlyphCachePrivate::
~GlyphCachePrivate()
{
  for(unsigned int i = 0, endi = m_glyphs.size(); i < endi; ++i)
    {
      m_glyphs[i]->clear();
      FASTUIDRAWdelete(m_glyphs[i]);
    }
}


GlyphDataPrivate*
GlyphCachePrivate::
fetch_or_allocate_glyph(GlyphSource src)
{
  std::map<GlyphSource, GlyphDataPrivate*>::iterator iter;
  GlyphDataPrivate *G;

  iter = m_glyph_map.find(src);
  if (iter != m_glyph_map.end())
    {
      return iter->second;
    }

  if (m_free_slots.empty())
    {
      G = FASTUIDRAWnew GlyphDataPrivate(this, m_glyphs.size());
      m_glyphs.push_back(G);
      FASTUIDRAWassert(!G->m_render.valid());
    }
  else
    {
      unsigned int v(m_free_slots.back());
      m_free_slots.pop_back();
      G = m_glyphs[v];
      FASTUIDRAWassert(!G->m_render.valid());
    }
  m_glyph_map[src] = G;
  return G;
}

///////////////////////////////////////////////////////
// fastuidraw::Glyph methods
enum fastuidraw::glyph_type
fastuidraw::Glyph::
type(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr);
  return p->m_render.m_type;
}

fastuidraw::GlyphRender
fastuidraw::Glyph::
renderer(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr);
  return p->m_render;
}

const fastuidraw::GlyphLayoutData&
fastuidraw::Glyph::
layout(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->m_layout;
}

fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache>
fastuidraw::Glyph::
cache(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->m_cache->m_p;
}

unsigned int
fastuidraw::Glyph::
cache_location(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->m_cache_location;
}

fastuidraw::c_array<const fastuidraw::GlyphLocation>
fastuidraw::Glyph::
atlas_locations(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return make_c_array(p->m_atlas_locations);
}

int
fastuidraw::Glyph::
geometry_offset(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->m_geometry_offset;
}

enum fastuidraw::return_code
fastuidraw::Glyph::
upload_to_atlas(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->upload_to_atlas();
}

const fastuidraw::Path&
fastuidraw::Glyph::
path(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->m_path;
}

fastuidraw::Glyph
fastuidraw::Glyph::
create_glyph(GlyphRender render,
             const reference_counted_ptr<const FontBase> &font,
             uint32_t glyph_code)
{
  GlyphDataPrivate *d;
  d = FASTUIDRAWnew GlyphDataPrivate();
  d->m_render = render;
  d->m_glyph_data = font->compute_rendering_data(d->m_render, glyph_code, d->m_layout, d->m_path);
  return Glyph(d);
}

enum fastuidraw::return_code
fastuidraw::Glyph::
delete_glyph(Glyph G)
{
  GlyphDataPrivate *d;
  d = static_cast<GlyphDataPrivate*>(G.m_opaque);
  if (d->m_cache)
    {
      return routine_fail;
    }
  d->clear();
  FASTUIDRAWdelete(d);
  return routine_success;
}

//////////////////////////////////////////////////////////
// fastuidraw::GlyphCache methods
fastuidraw::GlyphCache::
GlyphCache(reference_counted_ptr<GlyphAtlas> patlas)
{
  m_d = FASTUIDRAWnew GlyphCachePrivate(patlas, this);
}

fastuidraw::GlyphCache::
~GlyphCache()
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}


fastuidraw::Glyph
fastuidraw::GlyphCache::
fetch_glyph(GlyphRender render,
            const fastuidraw::reference_counted_ptr<const FontBase> &font,
            uint32_t glyph_code)
{
  if (!font || !font->can_create_rendering_data(render.m_type))
    {
      return Glyph();
    }

  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  GlyphDataPrivate *q;
  GlyphSource src(font, glyph_code, render);

  q = d->fetch_or_allocate_glyph(src);

  if (!q->m_render.valid())
    {
      q->m_render = render;
      FASTUIDRAWassert(!q->m_glyph_data);
      q->m_glyph_data = font->compute_rendering_data(q->m_render, glyph_code, q->m_layout, q->m_path);
    }

  return Glyph(q);
}

enum fastuidraw::return_code
fastuidraw::GlyphCache::
add_glyph(Glyph glyph)
{
  GlyphDataPrivate *g;
  g = static_cast<GlyphDataPrivate*>(glyph.m_opaque);

  if (!g || g->m_cache)
    {
      return routine_fail;
    }

  GlyphSource src(g->m_layout.m_font,
                  g->m_layout.m_glyph_code,
                  g->m_render);

  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);
  if (d->m_glyph_map.find(src) != d->m_glyph_map.end())
    {
      return routine_fail;
    }

  g->m_cache = d;
  g->m_cache_location = d->m_glyphs.size();
  d->m_glyphs.push_back(g);

  return routine_success;
}

void
fastuidraw::GlyphCache::
delete_glyph(Glyph G)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(G.m_opaque);
  FASTUIDRAWassert(p != nullptr);
  FASTUIDRAWassert(p->m_cache == d);
  FASTUIDRAWassert(p->m_render.valid());

  GlyphSource src(p->m_layout.m_font, p->m_layout.m_glyph_code, p->m_render);
  d->m_glyph_map.erase(src);
  p->clear();
  d->m_free_slots.push_back(p->m_cache_location);
}

void
fastuidraw::GlyphCache::
clear_atlas(void)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  d->m_atlas->clear();
  for(unsigned int i = 0, endi = d->m_glyphs.size(); i < endi; ++i)
    {
      d->m_glyphs[i]->m_uploaded_to_atlas = false;
      d->m_glyphs[i]->m_atlas_locations.clear();
      d->m_glyphs[i]->m_geometry_offset = -1;
      d->m_glyphs[i]->m_geometry_length = 0;
    }
}


void
fastuidraw::GlyphCache::
clear_cache(void)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  d->m_atlas->clear();
  d->m_glyph_map.clear();

  for(unsigned int i = 0, endi = d->m_glyphs.size(); i < endi; ++i)
    {
      GlyphDataPrivate *p;
      p = d->m_glyphs[i];
      if (p->m_render.valid())
        {
          p->clear();
          d->m_free_slots.push_back(p->m_cache_location);
        }
    }
}
