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
#include <mutex>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_render_data.hpp>
#include "../private/util_private.hpp"


namespace
{

  class GlyphCachePrivate;

  class GlyphDataAlloc
  {
  public:
    int m_location;
    int m_size;
  };

  class GlyphAtlasProxyPrivate
  {
  public:
    explicit
    GlyphAtlasProxyPrivate(GlyphCachePrivate *c):
      m_total_allocated(0),
      m_cache(c)
    {}

    unsigned int m_total_allocated;
    std::vector<GlyphDataAlloc> m_data_locations;
    GlyphCachePrivate *m_cache;
  };

  class GlyphMetricsPrivate
  {
  public:
    GlyphMetricsPrivate(GlyphCachePrivate *c, unsigned int I):
      m_cache(c),
      m_cache_location(I),
      m_ready(false),
      m_glyph_code(0),
      m_font(),
      m_horizontal_layout_offset(0.0f, 0.0f),
      m_vertical_layout_offset(0.0f, 0.0f),
      m_size(0.0f, 0.0f),
      m_advance(0.0f, 0.0f),
      m_units_per_EM(0.0f)
    {}

    GlyphMetricsPrivate():
      m_cache(nullptr),
      m_cache_location(~0u),
      m_ready(false),
      m_glyph_code(0),
      m_font(),
      m_horizontal_layout_offset(0.0f, 0.0f),
      m_vertical_layout_offset(0.0f, 0.0f),
      m_size(0.0f, 0.0f),
      m_advance(0.0f, 0.0f),
      m_units_per_EM(0.0f)
    {}

    void
    clear(void)
    {}

    /* owner */
    GlyphCachePrivate *m_cache;

    /* location into m_cache->m_glyphs  */
    unsigned int m_cache_location;

    /* if values are assigned */
    bool m_ready;

    uint32_t m_glyph_code;
    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> m_font;
    fastuidraw::vec2 m_horizontal_layout_offset;
    fastuidraw::vec2 m_vertical_layout_offset;
    fastuidraw::vec2 m_size, m_advance;
    float m_units_per_EM;
  };

  class GlyphDataPrivate:public GlyphAtlasProxyPrivate
  {
  public:
    GlyphDataPrivate(GlyphCachePrivate *c, unsigned int I);
    GlyphDataPrivate(void);
    ~GlyphDataPrivate();

    void
    clear(void);

    void
    remove_from_atlas(void);

    enum fastuidraw::return_code
    upload_to_atlas(fastuidraw::GlyphMetrics metrics,
                    fastuidraw::GlyphAtlasProxy &S,
                    fastuidraw::GlyphAttribute::Array &T);

    /* location into m_cache->m_glyphs  */
    unsigned int m_cache_location;

    fastuidraw::GlyphRenderer m_render;
    GlyphMetricsPrivate *m_metrics;

    std::vector<fastuidraw::GlyphAttribute> m_attributes;
    bool m_uploaded_to_atlas;

    /* Path of the glyph */
    fastuidraw::Path m_path;

    /* rendeirng size of the glyph */
    fastuidraw::vec2 m_render_size;

    /* data to generate glyph data */
    fastuidraw::GlyphRenderData *m_glyph_data;

    std::vector<fastuidraw::GlyphRenderCostInfo> m_render_cost_info;
  };

  template<typename K, typename T>
  class Store
  {
  public:
    fastuidraw::c_array<T*>
    data(void)
    {
      return fastuidraw::make_c_array(m_data);
    }

    enum fastuidraw::return_code
    take(T *d, GlyphCachePrivate *c, const K &key)
    {
      if (m_map.find(key) != m_map.end())
        {
          return fastuidraw::routine_fail;
        }

      d->m_cache = c;
      d->m_cache_location = m_data.size();
      m_map[key] = m_data.size();
      m_data.push_back(d);
      return fastuidraw::routine_success;
    }

    T*
    fetch_or_allocate(GlyphCachePrivate *c, const K &key)
    {
      typename map::iterator iter;

      iter = m_map.find(key);
      if (iter != m_map.end())
        {
          return m_data[iter->second];
        }

      T *p;
      unsigned int slot;

      if (!m_free_slots.empty())
        {
          slot = m_free_slots.back();
          p = m_data[slot];
          m_free_slots.pop_back();
        }
      else
        {
          p = FASTUIDRAWnew T(c, m_data.size());
          slot = m_data.size();
          m_data.push_back(p);
        }
      m_map[key] = slot;
      return p;
    }

    void
    remove_value(const K &key)
    {
      typename map::iterator iter;

      iter = m_map.find(key);
      FASTUIDRAWassert(iter != m_map.end());

      m_data[iter->second]->clear();
      m_free_slots.push_back(iter->second);
      m_map.erase(iter);
    }

    void
    clear(void)
    {
      for (const auto &m : m_map)
        {
          m_data[m.second]->clear();
          m_free_slots.push_back(m.second);
          FASTUIDRAWassert(m.second == m_data[m.second]->m_cache_location);
        }
      m_map.clear();
    }

  private:
    typedef std::map<K, unsigned int> map;

    map m_map;
    std::vector<T*> m_data;
    std::vector<unsigned int> m_free_slots;
  };

  class glyph_key
  {
  public:
    glyph_key(void):
      m_font(nullptr),
      m_glyph_code(0)
    {}

    glyph_key(const fastuidraw::FontBase *f,
	      uint32_t gc, fastuidraw::GlyphRenderer r):
      m_font(f),
      m_glyph_code(gc),
      m_render(r)
    {
      FASTUIDRAWassert(m_render.valid());
    }

    bool
    operator<(const glyph_key &rhs) const
    {
      return (m_font != rhs.m_font) ? m_font < rhs.m_font :
        (m_glyph_code != rhs.m_glyph_code) ? m_glyph_code < rhs.m_glyph_code :
        m_render < rhs.m_render;
    }

    const fastuidraw::FontBase *m_font;
    uint32_t m_glyph_code;
    fastuidraw::GlyphRenderer m_render;
  };

  class glyph_metrics_key
  {
  public:
    glyph_metrics_key(void):
      m_font(nullptr),
      m_glyph_code(0)
    {}

    glyph_metrics_key(const fastuidraw::FontBase *f, uint32_t gc):
      m_font(f),
      m_glyph_code(gc)
    {}

    glyph_metrics_key(uint32_t gc, const fastuidraw::FontBase *f):
      m_font(f),
      m_glyph_code(gc)
    {}

    explicit
    glyph_metrics_key(const fastuidraw::GlyphSource &src):
      m_font(src.m_font),
      m_glyph_code(src.m_glyph_code)
    {}

    bool
    operator<(const glyph_metrics_key &rhs) const
    {
      return (m_font != rhs.m_font) ? m_font < rhs.m_font :
        m_glyph_code < rhs.m_glyph_code;
    }

    const fastuidraw::FontBase *m_font;
    uint32_t m_glyph_code;
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

    std::mutex m_glyphs_mutex, m_glyphs_metrics_mutex;
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_atlas;
    Store<glyph_key, GlyphDataPrivate> m_glyphs;
    Store<glyph_metrics_key, GlyphMetricsPrivate> m_glyph_metrics;
    fastuidraw::GlyphCache *m_p;
  };
}

/////////////////////////////////////////////////////////
// GlyphDataPrivate methods
GlyphDataPrivate::
GlyphDataPrivate(GlyphCachePrivate *c, unsigned int I):
  GlyphAtlasProxyPrivate(c),
  m_cache_location(I),
  m_metrics(nullptr),
  m_uploaded_to_atlas(false),
  m_glyph_data(nullptr)
{}

GlyphDataPrivate::
GlyphDataPrivate(void):
  GlyphAtlasProxyPrivate(nullptr),
  m_cache_location(~0u),
  m_metrics(nullptr),
  m_uploaded_to_atlas(false),
  m_glyph_data(nullptr)
{}

GlyphDataPrivate::
~GlyphDataPrivate()
{
  if (m_metrics && !m_metrics->m_cache)
    {
      FASTUIDRAWdelete(m_metrics);
    }
}

void
GlyphDataPrivate::
remove_from_atlas(void)
{
  if (m_cache)
    {
      for (const GlyphDataAlloc &g : m_data_locations)
        {
          m_cache->m_atlas->deallocate_data(g.m_location, g.m_size);
        }
      m_data_locations.clear();
    }
  m_uploaded_to_atlas = false;
}

void
GlyphDataPrivate::
clear(void)
{
  m_render = fastuidraw::GlyphRenderer();
  FASTUIDRAWassert(!m_render.valid());

  remove_from_atlas();
  if (m_glyph_data)
    {
      FASTUIDRAWdelete(m_glyph_data);
      m_glyph_data = nullptr;
    }
  if (m_metrics && !m_metrics->m_cache)
    {
      FASTUIDRAWdelete(m_metrics);
    }
  m_attributes.clear();
  m_metrics = nullptr;
  m_path.clear();
}

enum fastuidraw::return_code
GlyphDataPrivate::
upload_to_atlas(fastuidraw::GlyphMetrics metrics,
                fastuidraw::GlyphAtlasProxy &S,
                fastuidraw::GlyphAttribute::Array &T)
{
  enum fastuidraw::return_code return_value;

  if (m_uploaded_to_atlas)
    {
      return fastuidraw::routine_success;
    }

  if (!m_cache)
    {
      return fastuidraw::routine_fail;
    }

  FASTUIDRAWassert(m_glyph_data);
  FASTUIDRAWassert(m_attributes.empty());

  if (!m_glyph_data)
    {
      m_glyph_data = m_metrics->m_font->compute_rendering_data(m_render, metrics, m_path, m_render_size);
    }

  fastuidraw::c_array<const fastuidraw::c_string> render_cost_labels(m_glyph_data->render_info_labels());
  std::vector<float> tmp(render_cost_labels.size(), 0.0f);

  return_value = m_glyph_data->upload_to_atlas(S, T, fastuidraw::make_c_array(tmp));
  if (return_value == fastuidraw::routine_success)
    {
      m_render_cost_info.resize(render_cost_labels.size() + 1);
      for (unsigned int i = 0; i < render_cost_labels.size(); ++i)
        {
          m_render_cost_info[i].m_label = render_cost_labels[i];
          m_render_cost_info[i].m_value = tmp[i];
        }
      m_render_cost_info.back().m_label = "SizeOnCacheInKB";
      m_render_cost_info.back().m_value = static_cast<float>(S.total_allocated() * 4) / 1024.0f;
      m_uploaded_to_atlas = true;
    }
  else
    {
      remove_from_atlas();
    }

  FASTUIDRAWdelete(m_glyph_data);
  m_glyph_data = nullptr;

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
  for(GlyphDataPrivate *p : m_glyphs.data())
    {
      p->clear();
      FASTUIDRAWdelete(p);
    }

  for(GlyphMetricsPrivate *p : m_glyph_metrics.data())
    {
      FASTUIDRAWdelete(p);
    }
}

//////////////////////////////////////////////
// fastuidraw::GlyphAtlasProxy methods
int
fastuidraw::GlyphAtlasProxy::
allocate_data(c_array<const generic_data> pdata)
{
  int L;
  GlyphAtlasProxyPrivate *d;

  d = static_cast<GlyphAtlasProxyPrivate*>(m_d);
  L = d->m_cache->m_atlas->allocate_data(pdata);
  if (L != -1)
    {
      GlyphDataAlloc A;
      A.m_location = L;
      A.m_size = pdata.size();
      d->m_total_allocated += A.m_size;
      d->m_data_locations.push_back(A);
    }
  return L;
}

unsigned int
fastuidraw::GlyphAtlasProxy::
total_allocated(void) const
{
  GlyphAtlasProxyPrivate *d;
  d = static_cast<GlyphAtlasProxyPrivate*>(m_d);
  return d->m_total_allocated;
}

//////////////////////////////////////////////
// fastuidraw::GlyphAttribute::Array methods
unsigned int
fastuidraw::GlyphAttribute::Array::
size(void) const
{
  std::vector<GlyphAttribute> *d;
  d = static_cast<std::vector<GlyphAttribute>*>(m_d);
  return d->size();
}

void
fastuidraw::GlyphAttribute::Array::
resize(unsigned int N)
{
  std::vector<GlyphAttribute> *d;
  d = static_cast<std::vector<GlyphAttribute>*>(m_d);
  d->resize(N);
}

fastuidraw::GlyphAttribute&
fastuidraw::GlyphAttribute::Array::
operator[](unsigned int N)
{
  std::vector<GlyphAttribute> *d;
  d = static_cast<std::vector<GlyphAttribute>*>(m_d);
  FASTUIDRAWassert(N < d->size());
  return d->operator[](N);
}

const fastuidraw::GlyphAttribute&
fastuidraw::GlyphAttribute::Array::
operator[](unsigned int N) const
{
  std::vector<GlyphAttribute> *d;
  d = static_cast<std::vector<GlyphAttribute>*>(m_d);
  FASTUIDRAWassert(N < d->size());
  return d->operator[](N);
}

fastuidraw::c_array<fastuidraw::GlyphAttribute>
fastuidraw::GlyphAttribute::Array::
data(void)
{
  std::vector<GlyphAttribute> *d;
  d = static_cast<std::vector<GlyphAttribute>*>(m_d);
  return make_c_array(*d);
}

fastuidraw::c_array<const fastuidraw::GlyphAttribute>
fastuidraw::GlyphAttribute::Array::
data(void) const
{
  std::vector<GlyphAttribute> *d;
  d = static_cast<std::vector<GlyphAttribute>*>(m_d);
  return make_c_array(*d);
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

fastuidraw::GlyphRenderer
fastuidraw::Glyph::
renderer(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr);
  return p->m_render;
}

fastuidraw::GlyphMetrics
fastuidraw::Glyph::
metrics(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return GlyphMetrics(p->m_metrics);
}

fastuidraw::GlyphCache*
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

fastuidraw::c_array<const fastuidraw::GlyphAttribute>
fastuidraw::Glyph::
attributes(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return make_c_array(p->m_attributes);
}

enum fastuidraw::return_code
fastuidraw::Glyph::
upload_to_atlas(void) const
{
  GlyphDataPrivate *p;

  p = static_cast<GlyphDataPrivate*>(m_opaque);
  if (!p || !p->m_render.valid() || !p->m_cache)
    {
      return routine_fail;
    }

  std::lock_guard<std::mutex> m(p->m_cache->m_glyphs_mutex);
  GlyphAtlasProxy S(p);
  GlyphAttribute::Array T(&p->m_attributes);
  return p->upload_to_atlas(metrics(), S, T);
}

bool
fastuidraw::Glyph::
uploaded_to_atlas(void) const
{
  GlyphDataPrivate *p;

  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->m_uploaded_to_atlas;
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

fastuidraw::vec2
fastuidraw::Glyph::
render_size(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return p->m_render_size;
}

fastuidraw::c_array<const fastuidraw::GlyphRenderCostInfo>
fastuidraw::Glyph::
render_cost(void) const
{
  GlyphDataPrivate *p;
  p = static_cast<GlyphDataPrivate*>(m_opaque);
  FASTUIDRAWassert(p != nullptr && p->m_render.valid());
  return make_c_array(p->m_render_cost_info);
}

fastuidraw::Glyph
fastuidraw::Glyph::
create_glyph(GlyphRenderer render,
             const reference_counted_ptr<const FontBase> &font,
             uint32_t glyph_code)
{
  GlyphDataPrivate *d;
  d = FASTUIDRAWnew GlyphDataPrivate();

  d->m_metrics = FASTUIDRAWnew GlyphMetricsPrivate();
  d->m_metrics->m_font = font;
  d->m_metrics->m_glyph_code = glyph_code;

  GlyphMetricsValue v(d->m_metrics);
  GlyphMetrics cv(d->m_metrics);

  d->m_render = render;
  font->compute_metrics(glyph_code, v);
  d->m_glyph_data = font->compute_rendering_data(d->m_render, cv, d->m_path,
						 d->m_render_size);
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

//////////////////////////////////////////
// GlyphMetrics methods
get_implement(fastuidraw::GlyphMetrics, GlyphMetricsPrivate, uint32_t, glyph_code)
get_implement(fastuidraw::GlyphMetrics, GlyphMetricsPrivate,
              const fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>&, font)
get_implement(fastuidraw::GlyphMetrics, GlyphMetricsPrivate, fastuidraw::vec2, horizontal_layout_offset)
get_implement(fastuidraw::GlyphMetrics, GlyphMetricsPrivate, fastuidraw::vec2, vertical_layout_offset)
get_implement(fastuidraw::GlyphMetrics, GlyphMetricsPrivate, fastuidraw::vec2, size)
get_implement(fastuidraw::GlyphMetrics, GlyphMetricsPrivate, fastuidraw::vec2, advance)
get_implement(fastuidraw::GlyphMetrics, GlyphMetricsPrivate, float, units_per_EM)

////////////////////////////////////////
// GlyphMetricsValue methods
set_implement(fastuidraw::GlyphMetricsValue, GlyphMetricsPrivate, fastuidraw::vec2, horizontal_layout_offset)
set_implement(fastuidraw::GlyphMetricsValue, GlyphMetricsPrivate, fastuidraw::vec2, vertical_layout_offset)
set_implement(fastuidraw::GlyphMetricsValue, GlyphMetricsPrivate, fastuidraw::vec2, size)
set_implement(fastuidraw::GlyphMetricsValue, GlyphMetricsPrivate, fastuidraw::vec2, advance)
set_implement(fastuidraw::GlyphMetricsValue, GlyphMetricsPrivate, float, units_per_EM)

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

fastuidraw::GlyphMetrics
fastuidraw::GlyphCache::
fetch_glyph_metrics(const FontBase *font, uint32_t glyph_code)
{
  if (!font || glyph_code >= font->number_glyphs())
    {
      return GlyphMetrics();
    }

  GlyphCachePrivate *d;
  GlyphMetricsPrivate *p;

  d = static_cast<GlyphCachePrivate*>(m_d);
  glyph_metrics_key K(glyph_code, font);

  std::lock_guard<std::mutex> m(d->m_glyphs_metrics_mutex);
  p = d->m_glyph_metrics.fetch_or_allocate(d, K);
  if (!p->m_ready)
    {
      GlyphMetricsValue v(p);
      p->m_font = font;
      p->m_glyph_code = glyph_code;
      font->compute_metrics(glyph_code, v);
      p->m_ready = true;
    }
  return GlyphMetrics(p);
}

void
fastuidraw::GlyphCache::
fetch_glyph_metrics(const FontBase *font,
                    c_array<const uint32_t> glyph_codes,
                    c_array<GlyphMetrics> out_metrics)
{

  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  if (!font)
    {
      return std::fill(out_metrics.begin(), out_metrics.end(), GlyphMetrics());
    }

  unsigned int num_glyphs_of_font(font->number_glyphs());
  std::lock_guard<std::mutex> m(d->m_glyphs_metrics_mutex);

  for (unsigned int i = 0; i < glyph_codes.size(); ++i)
    {
      if (glyph_codes[i] < num_glyphs_of_font)
	{
	  GlyphMetricsPrivate *p;
	  glyph_metrics_key K(glyph_codes[i], font);

	  p = d->m_glyph_metrics.fetch_or_allocate(d, K);
	  if (!p->m_ready)
	    {
	      GlyphMetricsValue v(p);
	      p->m_font = font;
	      p->m_glyph_code = glyph_codes[i];
	      font->compute_metrics(glyph_codes[i], v);
	      p->m_ready = true;
	    }
	  out_metrics[i] = GlyphMetrics(p);
	}
      else
	{
	  out_metrics[i] = GlyphMetrics();
	}
    }
}

void
fastuidraw::GlyphCache::
fetch_glyph_metrics(c_array<const GlyphSource> glyph_sources,
                    c_array<GlyphMetrics> out_metrics)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);
  std::lock_guard<std::mutex> m(d->m_glyphs_metrics_mutex);

  for (unsigned int i = 0; i < glyph_sources.size(); ++i)
    {
      GlyphMetricsPrivate *p;

      if (glyph_sources[i].m_font)
        {
	  glyph_metrics_key K(glyph_sources[i]);
          p = d->m_glyph_metrics.fetch_or_allocate(d, K);
          if (!p->m_ready)
            {
              GlyphMetricsValue v(p);
              p->m_font = glyph_sources[i].m_font;
              p->m_glyph_code = glyph_sources[i].m_glyph_code;
              glyph_sources[i].m_font->compute_metrics(glyph_sources[i].m_glyph_code, v);
              p->m_ready = true;
            }
        }
      else
        {
          p = nullptr;
        }
      out_metrics[i] = GlyphMetrics(p);
    }
}

fastuidraw::Glyph
fastuidraw::GlyphCache::
fetch_glyph(GlyphRenderer render, const FontBase *font,
            uint32_t glyph_code, bool upload_to_atlas)
{
  if (!font
      || !font->can_create_rendering_data(render.m_type)
      || glyph_code >= font->number_glyphs())
    {
      return Glyph();
    }

  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  GlyphDataPrivate *q;
  glyph_key src(font, glyph_code, render);

  std::lock_guard<std::mutex> m(d->m_glyphs_mutex);
  q = d->m_glyphs.fetch_or_allocate(d, src);

  if (!q->m_render.valid())
    {
      GlyphMetrics m;

      q->m_render = render;
      FASTUIDRAWassert(!q->m_glyph_data);
      m = fetch_glyph_metrics(font, glyph_code);
      q->m_metrics = static_cast<GlyphMetricsPrivate*>(m.m_d);
      q->m_glyph_data = font->compute_rendering_data(q->m_render, m,
						     q->m_path, q->m_render_size);
    }

  if (upload_to_atlas)
    {
      GlyphAtlasProxy S(q);
      GlyphAttribute::Array T(&q->m_attributes);
      q->upload_to_atlas(GlyphMetrics(q->m_metrics), S, T);
    }

  return Glyph(q);
}

void
fastuidraw::GlyphCache::
fetch_glyphs(GlyphRenderer render, const FontBase *font,
             c_array<const uint32_t> glyph_codes,
             c_array<Glyph> out_glyphs,
             bool upload_to_atlas)
{
  std::vector<GlyphMetrics> tmp_metrics_store(glyph_codes.size());
  c_array<GlyphMetrics> tmp_metrics(make_c_array(tmp_metrics_store));
  c_array<const GlyphMetrics> tmp_metrics_c(tmp_metrics);

  fetch_glyph_metrics(font, glyph_codes, tmp_metrics);
  fetch_glyphs(render, tmp_metrics_c, out_glyphs, upload_to_atlas);
}

void
fastuidraw::GlyphCache::
fetch_glyphs(GlyphRenderer render,
             c_array<const GlyphSource> glyph_sources,
             c_array<Glyph> out_glyphs,
             bool upload_to_atlas)
{
  std::vector<GlyphMetrics> tmp_metrics_store(glyph_sources.size());
  c_array<GlyphMetrics> tmp_metrics(make_c_array(tmp_metrics_store));
  c_array<const GlyphMetrics> tmp_metrics_c(tmp_metrics);

  fetch_glyph_metrics(glyph_sources, tmp_metrics);
  fetch_glyphs(render, tmp_metrics_c, out_glyphs, upload_to_atlas);
}

void
fastuidraw::GlyphCache::
fetch_glyphs(GlyphRenderer render,
	     c_array<const GlyphMetrics> glyph_metrics,
	     c_array<Glyph> out_glyphs,
	     bool upload_to_atlas)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_glyphs_mutex);
  for(unsigned int i = 0; i < glyph_metrics.size(); ++i)
    {
      if (glyph_metrics[i].valid())
        {
          glyph_key src(glyph_metrics[i].font().get(),
			glyph_metrics[i].glyph_code(),
			render);
          GlyphDataPrivate *q;

          q = d->m_glyphs.fetch_or_allocate(d, src);
          if (!q->m_render.valid())
            {
              GlyphMetrics m(glyph_metrics[i]);

              q->m_render = render;
              FASTUIDRAWassert(!q->m_glyph_data);
              q->m_metrics = static_cast<GlyphMetricsPrivate*>(m.m_d);
              q->m_glyph_data = src.m_font->compute_rendering_data(q->m_render, m,
								   q->m_path, q->m_render_size);
            }

          if (upload_to_atlas)
            {
	      GlyphAtlasProxy S(q);
	      GlyphAttribute::Array T(&q->m_attributes);
              q->upload_to_atlas(GlyphMetrics(q->m_metrics), S, T);
            }

          out_glyphs[i] = Glyph(q);
        }
      else
        {
          out_glyphs[i] = Glyph();
        }
    }
}

enum fastuidraw::return_code
fastuidraw::GlyphCache::
add_glyph(Glyph glyph, bool upload_to_atlas)
{
  GlyphDataPrivate *g;
  g = static_cast<GlyphDataPrivate*>(glyph.m_opaque);

  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  if (!g)
    {
      return routine_fail;
    }

  if (g->m_cache && g->m_cache != d)
    {
      return routine_fail;
    }

  std::lock_guard<std::mutex> m(d->m_glyphs_mutex);
  if (g->m_cache)
    {
      /* already part of this cache, upload if necessary */
      if (upload_to_atlas)
        {
	  GlyphAtlasProxy S(g);
	  GlyphAttribute::Array T(&g->m_attributes);
	  g->upload_to_atlas(GlyphMetrics(g->m_metrics), S, T);
        }
      return routine_success;
    }

  glyph_key src(g->m_metrics->m_font.get(),
		g->m_metrics->m_glyph_code,
		g->m_render);

  std::lock_guard<std::mutex> m2(d->m_glyphs_metrics_mutex);

  /* Take the metrics if we can */
  glyph_metrics_key metrics_src(g->m_metrics->m_font.get(),
				g->m_metrics->m_glyph_code);
  d->m_glyph_metrics.take(g->m_metrics, d, metrics_src);

  /* take the glyph */
  if (d->m_glyphs.take(g, d, src) == routine_fail)
    {
      return routine_fail;
    }

  if (upload_to_atlas)
    {
      GlyphAtlasProxy S(g);
      GlyphAttribute::Array T(&g->m_attributes);
      g->upload_to_atlas(GlyphMetrics(g->m_metrics), S, T);
    }

  return routine_success;
}

void
fastuidraw::GlyphCache::
delete_glyph(Glyph G)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  GlyphDataPrivate *g;
  g = static_cast<GlyphDataPrivate*>(G.m_opaque);
  FASTUIDRAWassert(g != nullptr);
  FASTUIDRAWassert(g->m_cache == d);
  FASTUIDRAWassert(g->m_render.valid());

  glyph_key src(g->m_metrics->m_font.get(),
		g->m_metrics->m_glyph_code,
		g->m_render);
  std::lock_guard<std::mutex> m(d->m_glyphs_mutex);
  d->m_glyphs.remove_value(src);
}

void
fastuidraw::GlyphCache::
clear_atlas(void)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  d->m_atlas->clear();
  std::lock_guard<std::mutex> m(d->m_glyphs_mutex);
  for(GlyphDataPrivate *g : d->m_glyphs.data())
    {
      /* setting m_uploaded_to_atlas marks the Glyph
       * as not uploaded. Clearing m_data_locations
       * prevents calling GlyphAtlas::deallocate_data().
       */
      g->m_uploaded_to_atlas = false;
      g->m_data_locations.clear();
    }
}

void
fastuidraw::GlyphCache::
clear_cache(void)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  std::lock_guard<std::mutex> m1(d->m_glyphs_mutex);
  std::lock_guard<std::mutex> m2(d->m_glyphs_metrics_mutex);
  d->m_atlas->clear();
  d->m_glyphs.clear();
  d->m_glyph_metrics.clear();
}

unsigned int
fastuidraw::GlyphCache::
number_times_atlas_cleared(void)
{
  /* should we make this thread safe? Its purpose
   * is to allow for classes that have derived
   * data from glyph locations in an atlas, to
   * know that the data needs to be regenerated.
   */
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);
  return d->m_atlas->number_times_cleared();
}

fastuidraw::GlyphCache::AllocationHandle
fastuidraw::GlyphCache::
allocate_data(c_array<const generic_data> pdata)
{
  AllocationHandle A;
  GlyphCachePrivate *d;
  int L;

  d = static_cast<GlyphCachePrivate*>(m_d);
  L = d->m_atlas->allocate_data(pdata);
  if (L >= 0)
    {
      A.m_size = pdata.size();
      A.m_location = L;
    }
  return A;
}

void
fastuidraw::GlyphCache::
deallocate_data(AllocationHandle h)
{
  GlyphCachePrivate *d;
  d = static_cast<GlyphCachePrivate*>(m_d);

  if (h.m_size > 0)
    {
      d->m_atlas->deallocate_data(h.m_location, h.m_size);
    }
}
