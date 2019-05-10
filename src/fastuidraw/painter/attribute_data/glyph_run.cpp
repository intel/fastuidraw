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

#include <fastuidraw/painter/attribute_data/glyph_run.hpp>
#include <vector>
#include <map>
#include <algorithm>
#include <private/util_private.hpp>

namespace
{
  class GlyphRunPrivate;

  class PerGlyphRender
  {
  public:
    void
    set_values(GlyphRunPrivate *p,
               const fastuidraw::GlyphAttributePacker &packer,
               fastuidraw::GlyphRenderer renderer);

    std::vector<fastuidraw::PainterAttribute> m_attribs;
    std::vector<fastuidraw::PainterIndex> m_indices;

    std::vector<unsigned int> m_glyph_attribs_start;
    std::vector<unsigned int> m_glyph_indices_start;
  };

  class SubSequence:public fastuidraw::PainterAttributeWriter
  {
  public:
    void
    set_src(const PerGlyphRender *data, unsigned int begin, unsigned int cnt);

    virtual
    unsigned int
    state_length(void) const override;

    virtual
    bool
    initialize_state(fastuidraw::PainterAttributeWriter::WriteState *dst) const override;

    virtual
    void
    on_new_store(fastuidraw::PainterAttributeWriter::WriteState *) const override
    {}

    virtual
    bool
    write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attributs,
               fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
               unsigned int index_addition,
               fastuidraw::PainterAttributeWriter::WriteState *state,
               unsigned int *num_attributes_written,
               unsigned int *num_indices_written) const override;

  private:
    fastuidraw::c_array<const fastuidraw::PainterIndex> m_indices;
    fastuidraw::c_array<const fastuidraw::PainterAttribute> m_attributes;
    unsigned int m_attribute_start, m_number_glyphs;
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
    GlyphRunPrivate(float format_size,
                    const fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> &cache,
                    const fastuidraw::reference_counted_ptr<const fastuidraw::GlyphAttributePacker> &packer):
      m_format_size(format_size),
      m_cache(cache),
      m_packer(packer),
      m_atlas_clear_count(0)
    {
      FASTUIDRAWassert(cache);
    }

    template<typename T>
    void
    grab_metrics(const fastuidraw::FontBase*,
                 fastuidraw::c_array<const T>,
                 fastuidraw::c_array<fastuidraw::GlyphMetrics>)
    {
      FASTUIDRAWassert(!"default grab_metrics called");
    }

    void
    grab_metrics(const fastuidraw::FontBase*,
                 fastuidraw::c_array<const fastuidraw::GlyphSource> sources,
                 fastuidraw::c_array<fastuidraw::GlyphMetrics> dst_glyphs)
    {
      m_cache->fetch_glyph_metrics(sources, dst_glyphs);
    }

    void
    grab_metrics(const fastuidraw::FontBase*,
                 fastuidraw::c_array<const fastuidraw::GlyphMetrics> metrics,
                 fastuidraw::c_array<fastuidraw::GlyphMetrics> dst_glyphs)
    {
      std::copy(metrics.begin(), metrics.end(), dst_glyphs.begin());
    }

    void
    grab_metrics(const fastuidraw::FontBase *font,
                 fastuidraw::c_array<const uint32_t> glyph_codes,
                 fastuidraw::c_array<fastuidraw::GlyphMetrics> dst_glyphs)
    {
      m_cache->fetch_glyph_metrics(font, glyph_codes, dst_glyphs);
    }

    template<typename T>
    void
    add_glyphs(const fastuidraw::FontBase *font,
               fastuidraw::c_array<const T> sources,
               fastuidraw::c_array<const fastuidraw::vec2> positions);

    const PerGlyphRender*
    fetch_render_data(const fastuidraw::GlyphRenderer &renderer);

    float m_format_size;
    fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_cache;
    fastuidraw::reference_counted_ptr<const fastuidraw::GlyphAttributePacker> m_packer;
    SubSequence m_subsequence;

    std::vector<GlyphLocation> m_glyph_locations;
    std::vector<fastuidraw::GlyphMetrics> m_glyphs;
    std::map<fastuidraw::GlyphRenderer, PerGlyphRender> m_data;
    unsigned int m_atlas_clear_count;
  };

}

///////////////////////////////////////////
// SubSequence methods
void
SubSequence::
set_src(const PerGlyphRender *data, unsigned int begin, unsigned int cnt)
{
  using namespace fastuidraw;

  m_indices = make_c_array(data->m_indices);
  m_indices = m_indices.sub_array(data->m_glyph_indices_start[begin],
                                  data->m_glyph_indices_start[begin + cnt] - data->m_glyph_indices_start[begin]);

  m_attributes = make_c_array(data->m_attribs);
  m_attributes = m_attributes.sub_array(data->m_glyph_attribs_start[begin],
                                        data->m_glyph_attribs_start[begin + cnt] - data->m_glyph_attribs_start[begin]);
  m_attribute_start = data->m_glyph_attribs_start[begin];
  m_number_glyphs = cnt;
}

unsigned int
SubSequence::
state_length(void) const
{
  return 1;
}

bool
SubSequence::
initialize_state(fastuidraw::PainterAttributeWriter::WriteState *dst) const
{
  dst->m_state[0] = 0;
  dst->m_min_attributes_for_next = 4;
  dst->m_min_indices_for_next = 6;
  return m_number_glyphs > 0;
}

bool
SubSequence::
write_data(fastuidraw::c_array<fastuidraw::PainterAttribute> dst_attribs,
           fastuidraw::c_array<fastuidraw::PainterIndex> dst_indices,
           unsigned int index_addition,
           fastuidraw::PainterAttributeWriter::WriteState *state,
           unsigned int *num_attribs_written,
           unsigned int *num_indices_written) const
{
  using namespace fastuidraw;

  c_array<const PainterIndex> src_indices;
  c_array<const PainterAttribute> src_attributes;
  unsigned int &a(*num_attribs_written);
  unsigned int &i(*num_indices_written);
  unsigned int &glyph(state->m_state[0]);

  src_attributes = m_attributes.sub_array(4 * glyph);
  src_indices = m_indices.sub_array(6 * glyph);

  for (a = 0, i = 0;
       a + 4u < dst_attribs.size() && i + 6u < dst_indices.size() && glyph < m_number_glyphs; ++glyph)
    {
      for (unsigned int aa = 0; aa < 4; ++aa, ++a)
        {
          dst_attribs[a] = src_attributes[a];
        }

      for (unsigned int ii = 0; ii < 6; ++ii, ++i)
        {
          dst_indices[i] = src_indices[i] + index_addition - m_attribute_start;
        }
    }

  return (glyph < m_number_glyphs);
}

//////////////////////////////////////////
// PerGlyphRender methods
void
PerGlyphRender::
set_values(GlyphRunPrivate *p,
           const fastuidraw::GlyphAttributePacker &packer,
           fastuidraw::GlyphRenderer renderer)
{
  using namespace fastuidraw;

  unsigned int num(p->m_glyph_locations.size());
  unsigned int num_indices(0), num_attribs(0);
  std::vector<Glyph> tmp_glyphs(num);
  c_array<const GlyphMetrics> glyph_metrics(make_c_array(p->m_glyphs));
  c_array<Glyph> glyphs(make_c_array(tmp_glyphs));

  m_glyph_indices_start.resize(num + 1);
  m_glyph_attribs_start.resize(num + 1);
  p->m_cache->fetch_glyphs(renderer, glyph_metrics, glyphs, true);
  for (unsigned int g = 0; g < num; ++g)
    {
      unsigned int i(0), a(0);

      m_glyph_indices_start[g] = num_indices;
      m_glyph_attribs_start[g] = num_attribs;
      if (glyphs[g].valid())
        {
          packer.compute_needed_room(glyphs[g].renderer(), glyphs[g].attributes(), &i, &a);
        }
      num_indices += i;
      num_attribs += a;
    }
  m_glyph_indices_start.back() = num_indices;
  m_glyph_attribs_start.back() = num_attribs;
  m_attribs.resize(num_attribs);
  m_indices.resize(num_indices);

  c_array<PainterAttribute> attribs(make_c_array(m_attribs));
  c_array<PainterIndex> indices(make_c_array(m_indices));

  for (unsigned int g = 0, ii = 0, aa = 0; g < num; ++g)
    {
      unsigned int i(0), a(0);

      if (glyphs[g].valid())
        {
          packer.compute_needed_room(glyphs[g].renderer(), glyphs[g].attributes(), &i, &a);
        }

      if (a > 0 && i > 0)
        {
          vec2 p_bl, p_tr;
          c_array<PainterIndex> sub_i;
          c_array<PainterAttribute> sub_a;

          sub_i = indices.sub_array(ii, i);
          sub_a = attribs.sub_array(aa, a);
          packer.glyph_position(glyphs[g],
                                p->m_glyph_locations[g].m_position,
                                p->m_glyph_locations[g].m_scale,
                                &p_bl, &p_tr);
          packer.realize_attribute_data(glyphs[g].renderer(), glyphs[g].attributes(),
                                        sub_i, sub_a, p_bl, p_tr);
          for (PainterIndex &value : sub_i)
            {
              value += aa;
            }
          ii += i;
          aa += a;
        }
    }
}

////////////////////////////////////////
// GlyphRunPrivate methods
template<typename T>
void
GlyphRunPrivate::
add_glyphs(const fastuidraw::FontBase *font,
           fastuidraw::c_array<const T> sources,
           fastuidraw::c_array<const fastuidraw::vec2> positions)
{
  unsigned int old_sz;
  fastuidraw::c_array<fastuidraw::GlyphMetrics> dst_glyphs;

  old_sz = m_glyphs.size();
  m_glyphs.resize(sources.size() + old_sz);
  m_glyph_locations.reserve(sources.size() + old_sz);
  dst_glyphs = fastuidraw::make_c_array(m_glyphs).sub_array(old_sz);
  this->grab_metrics(font, sources, dst_glyphs);

  for (unsigned int i = 0; i < positions.size(); ++i)
    {
      GlyphLocation L;

      L.m_position = positions[i];
      if (dst_glyphs[i].valid())
        {
          L.m_scale = m_format_size / dst_glyphs[i].units_per_EM();
        }
      else
        {
          L.m_scale = 1.0f;
        }
      m_glyph_locations.push_back(L);
    }
  m_data.clear();
}

const PerGlyphRender*
GlyphRunPrivate::
fetch_render_data(const fastuidraw::GlyphRenderer &renderer)
{
  const PerGlyphRender *data;
  std::map<fastuidraw::GlyphRenderer, PerGlyphRender>::const_iterator iter;

  if (!m_data.empty() && m_atlas_clear_count != m_cache->number_times_atlas_cleared())
    {
      m_atlas_clear_count = m_cache->number_times_atlas_cleared();
      m_data.clear();
    }

  iter = m_data.find(renderer);
  if (iter == m_data.end())
    {
      PerGlyphRender &p(m_data[renderer]);
      p.set_values(this, *m_packer, renderer);
      data = &p;
    }
  else
    {
      data = &iter->second;
    }
  return data;
}

////////////////////////////////
// fastuidraw::GlyphRun methods
fastuidraw::GlyphRun::
GlyphRun(float format_size,
         enum PainterEnums::screen_orientation orientation,
         GlyphCache &cache,
         enum PainterEnums::glyph_layout_type layout)
{
  m_d = FASTUIDRAWnew GlyphRunPrivate(format_size, &cache,
                                      &GlyphAttributePacker::standard_packer(orientation, layout));
}

fastuidraw::GlyphRun::
GlyphRun(float format_size, GlyphCache &cache,
         const reference_counted_ptr<const GlyphAttributePacker> &packer)
{
  m_d = FASTUIDRAWnew GlyphRunPrivate(format_size, &cache, packer);
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
format_size(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return d->m_format_size;
}

const fastuidraw::GlyphAttributePacker&
fastuidraw::GlyphRun::
attribute_packer(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return *d->m_packer;
}

fastuidraw::GlyphCache&
fastuidraw::GlyphRun::
glyph_cache(void) const
{
  GlyphRunPrivate *d;
  d = static_cast<GlyphRunPrivate*>(m_d);
  return *d->m_cache;
}

void
fastuidraw::GlyphRun::
add_glyphs(c_array<const GlyphSource> sources,
           c_array<const vec2> positions)
{
  GlyphRunPrivate *d;

  d = static_cast<GlyphRunPrivate*>(m_d);
  d->add_glyphs<GlyphSource>(nullptr, sources, positions);
}

void
fastuidraw::GlyphRun::
add_glyphs(c_array<const GlyphMetrics> glyph_metrics,
           c_array<const vec2> positions)
{
  GlyphRunPrivate *d;

  d = static_cast<GlyphRunPrivate*>(m_d);
  d->add_glyphs<GlyphMetrics>(nullptr, glyph_metrics, positions);
}

void
fastuidraw::GlyphRun::
add_glyphs(const FontBase *font,
           c_array<const uint32_t> glyph_codes,
           c_array<const vec2> positions)
{
  GlyphRunPrivate *d;

  d = static_cast<GlyphRunPrivate*>(m_d);
  d->add_glyphs<uint32_t>(font, glyph_codes, positions);
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

const fastuidraw::PainterAttributeWriter&
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
      unsigned int max_v(d->m_glyphs.size());

      begin = t_min(begin, max_v - 1u);
      cnt = t_min(cnt, max_v - begin);
    }

  const PerGlyphRender *data;
  data = d->fetch_render_data(renderer);
  d->m_subsequence.set_src(data, begin, cnt);

  return d->m_subsequence;
}

const fastuidraw::PainterAttributeWriter&
fastuidraw::GlyphRun::
subsequence(GlyphRenderer renderer, unsigned int begin) const
{
  unsigned int count, num;

  num = number_glyphs();
  if (begin < num)
    {
      count = number_glyphs() - begin;
    }
  else
    {
      count = 0;
    }
  return subsequence(renderer, begin, count);
}

const fastuidraw::PainterAttributeWriter&
fastuidraw::GlyphRun::
subsequence(GlyphRenderer renderer) const
{
  return subsequence(renderer, 0, number_glyphs());
}
