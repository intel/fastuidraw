/*!
 * \file painter_attribute_data.cpp
 * \brief file painter_attribute_data.cpp
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


#include <algorithm>
#include <vector>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include "../private/util_private.hpp"

namespace
{
  inline
  uint32_t
  filter_atlas_layer(int layer)
  {
    assert(layer >= -1);
    return (layer != -1) ? static_cast<uint32_t>(layer) : ~0u;
  }

  inline
  void
  pack_glyph_indices(fastuidraw::c_array<fastuidraw::PainterIndex> dst, unsigned int aa)
  {
    assert(dst.size() == 6);
    dst[0] = aa;
    dst[1] = aa + 1;
    dst[2] = aa + 2;
    dst[3] = aa;
    dst[4] = aa + 2;
    dst[5] = aa + 3;
  }

  inline
  void
  pack_glyph_attributes(enum fastuidraw::PainterEnums::glyph_orientation orientation,
                        fastuidraw::vec2 p, fastuidraw::Glyph glyph, float SCALE,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> dst)
  {
    assert(glyph.valid());

    fastuidraw::GlyphLocation atlas(glyph.atlas_location());
    fastuidraw::GlyphLocation secondary_atlas(glyph.secondary_atlas_location());
    fastuidraw::uvec4 uint_values;
    fastuidraw::vec2 tex_size(atlas.size());
    fastuidraw::vec2 tex_xy(atlas.location());
    fastuidraw::vec2 secondary_tex_xy(secondary_atlas.location());
    fastuidraw::vec2 t_bl(tex_xy), t_tr(t_bl + tex_size);
    fastuidraw::vec2 t2_bl(secondary_tex_xy), t2_tr(t2_bl + tex_size);
    fastuidraw::vec2 glyph_size(SCALE * glyph.layout().m_size);
    fastuidraw::vec2 p_bl, p_tr;

    /* ISSUE: we are assuming horizontal layout; we should probably
       change the inteface so that caller chooses how to adjust
       positions with the choices:
         adjust_using_horizontal,
         adjust_using_vertical,
         no_adjust
     */
    if(orientation == fastuidraw::PainterEnums::y_increases_downwards)
      {
        p_bl.x() = p.x() + SCALE * glyph.layout().m_horizontal_layout_offset.x();
        p_tr.x() = p_bl.x() + glyph_size.x();

        p_bl.y() = p.y() - SCALE * glyph.layout().m_horizontal_layout_offset.y();
        p_tr.y() = p_bl.y() - glyph_size.y();
      }
    else
      {
        p_bl = p + SCALE * glyph.layout().m_horizontal_layout_offset;
        p_tr = p_bl + glyph_size;
      }

    /* secondary_atlas.layer() can be -1 to indicate that
       the glyph does not have secondary atlas, when changed
       to an unsigned value it is ungood, to compensate we
       will "do something".
     */
    uint_values.x() = 0u;
    uint_values.y() = glyph.geometry_offset();
    uint_values.z() = filter_atlas_layer(atlas.layer());
    uint_values.w() = filter_atlas_layer(secondary_atlas.layer());

    dst[0].m_attrib0 = fastuidraw::pack_vec4(t_bl.x(), t_bl.y(), t2_bl.x(), t2_bl.y());
    dst[0].m_attrib1 = fastuidraw::pack_vec4(p_bl.x(), p_bl.y(), 0.0f, 0.0f);
    dst[0].m_attrib2 = uint_values;

    dst[1].m_attrib0 = fastuidraw::pack_vec4(t_tr.x(), t_bl.y(), t2_tr.x(), t2_bl.y());
    dst[1].m_attrib1 = fastuidraw::pack_vec4(p_tr.x(), p_bl.y(), 0.0f, 0.0f);
    dst[1].m_attrib2 = uint_values;

    dst[2].m_attrib0 = fastuidraw::pack_vec4(t_tr.x(), t_tr.y(), t2_tr.x(), t2_tr.y());
    dst[2].m_attrib1 = fastuidraw::pack_vec4(p_tr.x(), p_tr.y(), 0.0f, 0.0f);
    dst[2].m_attrib2 = uint_values;

    dst[3].m_attrib0 = fastuidraw::pack_vec4(t_bl.x(), t_tr.y(), t2_bl.x(), t2_tr.y());
    dst[3].m_attrib1 = fastuidraw::pack_vec4(p_bl.x(), p_tr.y(), 0.0f, 0.0f);
    dst[3].m_attrib2 = uint_values;
  }

  unsigned int
  number_uploadable(fastuidraw::const_c_array<fastuidraw::Glyph> glyphs,
                    std::vector<unsigned int> &cnt_by_type)
  {
    unsigned int return_value, i, endi;

    for(return_value = 0, i = 0, endi = glyphs.size(); i < endi; ++i)
      {
        enum fastuidraw::return_code R;

        if(glyphs[i].valid())
          {
            R = glyphs[i].upload_to_atlas();
            if(R != fastuidraw::routine_success)
              {
                return return_value;
              }
            ++return_value;

            if(cnt_by_type.size() <= glyphs[i].type())
              {
                cnt_by_type.resize(1 + glyphs[i].type(), 0);
              }
            ++cnt_by_type[glyphs[i].type()];
          }
      }
    return return_value;
  }

  class PainterAttributeDataPrivate
  {
  public:
    unsigned int
    prepare_arrays_for_text(fastuidraw::const_c_array<fastuidraw::Glyph> glyphs);

    void
    ready_non_empty_index_data_chunks(void);

    std::vector<fastuidraw::PainterAttribute> m_attribute_data;
    std::vector<fastuidraw::PainterIndex> m_index_data;

    std::vector<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > m_attribute_chunks;
    std::vector<fastuidraw::const_c_array<fastuidraw::PainterIndex> > m_index_chunks;
    std::vector<unsigned int> m_increment_z;
    std::vector<unsigned int> m_non_empty_index_data_chunks;
  };
}

void
PainterAttributeDataPrivate::
ready_non_empty_index_data_chunks(void)
{
  m_non_empty_index_data_chunks.clear();
  for(unsigned int i = 0; i < m_index_chunks.size(); ++i)
    {
      if(!m_index_chunks[i].empty())
        {
          m_non_empty_index_data_chunks.push_back(i);
        }
    }
}

unsigned int
PainterAttributeDataPrivate::
prepare_arrays_for_text(fastuidraw::const_c_array<fastuidraw::Glyph> glyphs)
{
  unsigned int cnt, num_glyph_types;
  std::vector<unsigned int> cnt_by_type;

  cnt = number_uploadable(glyphs, cnt_by_type);
  num_glyph_types = cnt_by_type.size();

  m_attribute_data.resize(4 * cnt);
  m_index_data.resize(6 * cnt);

  m_attribute_chunks.resize(num_glyph_types);
  m_index_chunks.resize(num_glyph_types);
  m_increment_z.resize(num_glyph_types, 0);

  for(unsigned int i = 0, c = 0; i < num_glyph_types; ++i)
    {
      m_attribute_chunks[i] = fastuidraw::make_c_array(m_attribute_data).sub_array(4 * c, 4 * cnt_by_type[i]);
      m_index_chunks[i] = fastuidraw::make_c_array(m_index_data).sub_array(6 * c, 6 * cnt_by_type[i]);
      c += cnt_by_type[i];
    }
  return cnt;
}

//////////////////////////////////////////////
// fastuidraw::PainterAttributeData methods
fastuidraw::PainterAttributeData::
PainterAttributeData(void)
{
  m_d = FASTUIDRAWnew PainterAttributeDataPrivate();
}

fastuidraw::PainterAttributeData::
~PainterAttributeData()
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::PainterAttributeData::
set_data(const PainterAttributeDataFiller &filler)
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);

  unsigned int number_attributes(0), number_indices(0);
  unsigned int number_attribute_chunks(0), number_index_chunks(0);
  unsigned int number_z_increments(0);

  filler.compute_sizes(number_attributes, number_indices,
                       number_attribute_chunks, number_index_chunks,
                       number_z_increments);

  d->m_attribute_data.resize(number_attributes);
  d->m_index_data.resize(number_indices);

  d->m_attribute_chunks.clear();
  d->m_attribute_chunks.resize(number_attribute_chunks);

  d->m_index_chunks.clear();
  d->m_index_chunks.resize(number_index_chunks);

  d->m_increment_z.clear();
  d->m_increment_z.resize(number_z_increments, 0u);

  filler.fill_data(make_c_array(d->m_attribute_data),
                   make_c_array(d->m_index_data),
                   make_c_array(d->m_attribute_chunks),
                   make_c_array(d->m_index_chunks),
                   make_c_array(d->m_increment_z));

  d->ready_non_empty_index_data_chunks();
}


unsigned int
fastuidraw::PainterAttributeData::
set_data(const_c_array<vec2> glyph_positions,
         const_c_array<Glyph> glyphs,
         const_c_array<float> scale_factors,
         enum PainterEnums::glyph_orientation orientation)
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);

  assert(glyph_positions.size() == glyphs.size());
  assert(scale_factors.empty() || scale_factors.size() == glyphs.size());

  std::vector<unsigned int> current;
  unsigned int cnt;

  cnt = d->prepare_arrays_for_text(glyphs);
  current.resize(d->m_attribute_chunks.size(), 0);

  for(unsigned int g = 0; g < cnt; ++g)
    {
      if(glyphs[g].valid())
        {
          float SCALE;
          unsigned int t;

          SCALE = (scale_factors.empty()) ? 1.0f : scale_factors[g];
          t = glyphs[g].type();
          pack_glyph_attributes(orientation, glyph_positions[g],
                                glyphs[g], SCALE,
                                const_cast_c_array(d->m_attribute_chunks[t].sub_array(4 * current[t], 4)));
          pack_glyph_indices(const_cast_c_array(d->m_index_chunks[t].sub_array(6 * current[t], 6)), 4 * current[t]);
          ++current[t];
        }
    }
  d->ready_non_empty_index_data_chunks();
  return cnt;
}

unsigned int
fastuidraw::PainterAttributeData::
set_data(const_c_array<vec2> glyph_positions,
         const_c_array<Glyph> glyphs,
         float render_pixel_size,
         enum PainterEnums::glyph_orientation orientation)
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);

  assert(glyph_positions.size() == glyphs.size());

  std::vector<unsigned int> current;
  unsigned int cnt;

  cnt = d->prepare_arrays_for_text(glyphs);
  current.resize(d->m_attribute_chunks.size(), 0);

  for(unsigned int g = 0; g < cnt; ++g)
    {
      if(glyphs[g].valid())
        {
          float SCALE;
          unsigned int t;

          SCALE = render_pixel_size / glyphs[g].layout().m_pixel_size;
          t = glyphs[g].type();
          pack_glyph_attributes(orientation, glyph_positions[g],
                                glyphs[g], SCALE,
                                const_cast_c_array(d->m_attribute_chunks[t].sub_array(4 * current[t], 4)));
          pack_glyph_indices(const_cast_c_array(d->m_index_chunks[t].sub_array(6 * current[t], 6)), 4 * current[t]);
          ++current[t];
        }
    }
  d->ready_non_empty_index_data_chunks();
  return cnt;
}

fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> >
fastuidraw::PainterAttributeData::
attribute_data_chunks(void) const
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_attribute_chunks);
}

fastuidraw::const_c_array<fastuidraw::PainterAttribute>
fastuidraw::PainterAttributeData::
attribute_data_chunk(unsigned int i) const
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  return (i < d->m_attribute_chunks.size()) ?
    d->m_attribute_chunks[i] :
    const_c_array<PainterAttribute>();
}

fastuidraw::const_c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> >
fastuidraw::PainterAttributeData::
index_data_chunks(void) const
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_index_chunks);
}

fastuidraw::const_c_array<fastuidraw::PainterIndex>
fastuidraw::PainterAttributeData::
index_data_chunk(unsigned int i) const
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  return (i < d->m_index_chunks.size()) ?
    d->m_index_chunks[i] :
    const_c_array<PainterIndex>();
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::PainterAttributeData::
increment_z_values(void) const
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_increment_z);
}

unsigned int
fastuidraw::PainterAttributeData::
increment_z_value(unsigned int i) const
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  return (i < d->m_increment_z.size()) ?
    d->m_increment_z[i] :
    0;
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::PainterAttributeData::
non_empty_index_data_chunks(void) const
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  return make_c_array(d->m_non_empty_index_data_chunks);
}
