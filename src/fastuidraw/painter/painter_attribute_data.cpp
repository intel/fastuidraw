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
#include <fastuidraw/text/glyph_layout.hpp>
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

    dst[0].m_secondary_attrib = fastuidraw::vec4(p_bl.x(), p_bl.y(), 0.0f, 0.0f);
    dst[0].m_primary_attrib = fastuidraw::vec4(t_bl.x(), t_bl.y(), t2_bl.x(), t2_bl.y());
    dst[0].m_uint_attrib = uint_values;

    dst[1].m_secondary_attrib = fastuidraw::vec4(p_tr.x(), p_bl.y(), 0.0f, 0.0f);
    dst[1].m_primary_attrib = fastuidraw::vec4(t_tr.x(), t_bl.y(), t2_tr.x(), t2_bl.y());
    dst[1].m_uint_attrib = uint_values;

    dst[2].m_secondary_attrib = fastuidraw::vec4(p_tr.x(), p_tr.y(), 0.0f, 0.0f);
    dst[2].m_primary_attrib = fastuidraw::vec4(t_tr.x(), t_tr.y(), t2_tr.x(), t2_tr.y());
    dst[2].m_uint_attrib = uint_values;

    dst[3].m_secondary_attrib = fastuidraw::vec4(p_bl.x(), p_tr.y(), 0.0f, 0.0f);
    dst[3].m_primary_attrib = fastuidraw::vec4(t_bl.x(), t_tr.y(), t2_bl.x(), t2_tr.y());
    dst[3].m_uint_attrib = uint_values;
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

  fastuidraw::PainterAttribute
  generate_attribute_fill(const fastuidraw::vec2 &src)
  {
    fastuidraw::PainterAttribute dst;

    dst.m_primary_attrib.x() = src.x();
    dst.m_primary_attrib.y() = src.y();
    dst.m_primary_attrib.z() = 0.0f;
    dst.m_primary_attrib.w() = 0.0f;

    dst.m_secondary_attrib.x() = 0.0f;
    dst.m_secondary_attrib.y() = 0.0f;
    dst.m_secondary_attrib.z() = 0.0f;
    dst.m_secondary_attrib.w() = 0.0f;

    dst.m_uint_attrib = fastuidraw::uvec4(0u, 0u, 0u, 0u);

    return dst;
  }

  fastuidraw::PainterAttribute
  generate_attribute_stroke(const fastuidraw::StrokedPath::point &src)
  {
    fastuidraw::PainterAttribute dst;

    dst.m_primary_attrib.x() = src.m_position.x();
    dst.m_primary_attrib.y() = src.m_position.y();
    dst.m_primary_attrib.z() = src.m_pre_offset.x();
    dst.m_primary_attrib.w() = src.m_pre_offset.y();

    dst.m_secondary_attrib.x() = src.m_distance_from_edge_start;
    dst.m_secondary_attrib.y() = src.m_distance_from_contour_start;
    dst.m_secondary_attrib.z() = src.m_auxilary_offset.x();
    dst.m_secondary_attrib.w() = src.m_auxilary_offset.y();

    int v;
    v = src.m_on_boundary + 1;
    assert(v >= 0);
    dst.m_uint_attrib = fastuidraw::uvec4(src.m_depth, src.m_tag, v, 0u);

    return dst;
  }

  void
  grab_attribute_index_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attrib_dst,
                            unsigned int &attr_loc,
                            fastuidraw::const_c_array<fastuidraw::StrokedPath::point> attr_src,
                            fastuidraw::c_array<fastuidraw::PainterIndex> idx_dst, unsigned int &idx_loc,
                            fastuidraw::const_c_array<unsigned int> idx_src,
                            fastuidraw::const_c_array<fastuidraw::PainterAttribute> &attrib_chunks,
                            fastuidraw::const_c_array<fastuidraw::PainterIndex> &index_chunks)
  {
    attrib_dst = attrib_dst.sub_array(attr_loc, attr_src.size());
    idx_dst = idx_dst.sub_array(idx_loc, idx_src.size());

    attrib_chunks = attrib_dst;
    index_chunks = idx_dst;

    std::copy(idx_src.begin(), idx_src.end(), idx_dst.begin());
    std::transform(attr_src.begin(), attr_src.end(), attrib_dst.begin(), generate_attribute_stroke);

    attr_loc += attr_src.size();
    idx_loc += idx_src.size();
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

unsigned int
fastuidraw::PainterAttributeData::
index_chunk_from_winding_number(int winding_number)
{
  /* basic idea:
      - start counting at fill_rule_data_count
      - ordering is: 1, -1, 2, -2, ...
   */
  int value, sg;

  if(winding_number == 0)
    {
      return PainterEnums::complement_nonzero_fill_rule;
    }

  value = std::abs(winding_number);
  sg = (winding_number < 0) ? 1 : 0;
  return PainterEnums::fill_rule_data_count + sg + 2 * (value - 1);
}

int
fastuidraw::PainterAttributeData::
winding_number_from_index_chunk(unsigned int idx)
{
  int abs_winding;

  if(idx == PainterEnums::complement_nonzero_fill_rule)
    {
      return 0;
    }

  assert(idx >= PainterEnums::fill_rule_data_count);
  idx -= PainterEnums::fill_rule_data_count;
  abs_winding = 1 + idx / 2;
  return (idx & 1) ? -abs_winding : abs_winding;
}

void
fastuidraw::PainterAttributeData::
set_data(const reference_counted_ptr<const FilledPath> &path)
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);
  const_c_array<int> winding_numbers(path->winding_numbers());

  if(winding_numbers.empty())
    {
      d->m_attribute_data.clear();
      d->m_attribute_chunks.clear();
      d->m_increment_z.clear();
      d->m_index_data.clear();
      d->m_index_chunks.clear();
      return;
    }

  d->m_attribute_data.resize(path->points().size());
  std::transform(path->points().begin(), path->points().end(), d->m_attribute_data.begin(), generate_attribute_fill);

  /* winding_numbers is already sorted, so largest and smallest
     winding numbers are at back and front
   */
  int largest_winding(winding_numbers.back());
  int smallest_winding(winding_numbers.front());

  /* now get how big the index_chunks really needs to be
   */
  unsigned int largest_winding_idx(index_chunk_from_winding_number(largest_winding));
  unsigned int smallest_winding_idx(index_chunk_from_winding_number(smallest_winding));
  unsigned int num_idx_chunks(1 + std::max(largest_winding_idx, smallest_winding_idx));

  d->m_attribute_chunks.resize(1);
  d->m_index_chunks.resize(num_idx_chunks);
  d->m_increment_z.resize(d->m_index_chunks.size(), 0);

  /* the attribute data is the same for all elements
   */
  d->m_attribute_chunks[0] = make_c_array(d->m_attribute_data);

  unsigned int num_indices, current(0);
  c_array<const_c_array<PainterIndex> > index_chunks;

  index_chunks = make_c_array(d->m_index_chunks);
  num_indices = path->odd_winding_indices().size()
    + path->nonzero_winding_indices().size()
    + path->even_winding_indices().size()
    + path->zero_winding_indices().size();

  for(const_c_array<int>::iterator iter = path->winding_numbers().begin(),
        end = path->winding_numbers().end(); iter != end; ++iter)
    {
      if(*iter != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          num_indices += path->indices(*iter).size();
        }
    }
  d->m_index_data.resize(num_indices);

#define GRAB_MACRO(enum_name, function_name) do {               \
    c_array<PainterIndex> dst;                                  \
    dst = make_c_array(d->m_index_data);                        \
    dst = dst.sub_array(current, path->function_name().size()); \
    std::copy(path->function_name().begin(),                    \
              path->function_name().end(), dst.begin());        \
    index_chunks[PainterEnums::enum_name] = dst;           \
    current += dst.size();                                      \
  } while(0)

  GRAB_MACRO(odd_even_fill_rule, odd_winding_indices);
  GRAB_MACRO(nonzero_fill_rule, nonzero_winding_indices);
  GRAB_MACRO(complement_odd_even_fill_rule, even_winding_indices);
  GRAB_MACRO(complement_nonzero_fill_rule, zero_winding_indices);

#undef GRAB_MACRO

  for(const_c_array<int>::iterator iter = winding_numbers.begin(),
        end = winding_numbers.end(); iter != end; ++iter)
    {
      if(*iter != 0) //winding number 0 is by complement_nonzero_fill_rule
        {
          c_array<PainterIndex> dst;
          const_c_array<unsigned int> src;
          unsigned int idx;

          idx = index_chunk_from_winding_number(*iter);
          assert(*iter == winding_number_from_index_chunk(idx));

          src = path->indices(*iter);
          dst = make_c_array(d->m_index_data).sub_array(current, src.size());
          assert(dst.size() == src.size());

          std::copy(src.begin(), src.end(), dst.begin());

          index_chunks[idx] = dst;
          current += dst.size();
        }
    }

  d->ready_non_empty_index_data_chunks();
}

void
fastuidraw::PainterAttributeData::
set_data(const reference_counted_ptr<const StrokedPath> &path)
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);

  unsigned int num_attributes, num_indices;
  unsigned int attr_loc(0), idx_loc(0);

  num_attributes = path->edge_points(true).size() + path->bevel_joins_points(true).size()
    + path->miter_joins_points(true).size() + path->rounded_joins_points(true).size()
    + path->rounded_cap_points().size() + path->square_cap_points().size();

  num_indices = path->edge_indices(true).size() + path->bevel_joins_indices(true).size()
    + path->miter_joins_indices(true).size() + path->rounded_joins_indices(true).size()
    + path->rounded_cap_indices().size() + path->square_cap_indices().size();;

  d->m_attribute_data.resize(num_attributes);
  d->m_index_data.resize(num_indices);
  d->m_attribute_chunks.resize(stroking_data_count);
  d->m_index_chunks.resize(stroking_data_count);

  d->m_increment_z.resize(stroking_data_count, 0);
  d->m_increment_z[rounded_joins_closing_edge] = path->rounded_join_number_depth(true);
  d->m_increment_z[bevel_joins_closing_edge] = path->bevel_join_number_depth(true);
  d->m_increment_z[miter_joins_closing_edge] = path->miter_join_number_depth(true);
  d->m_increment_z[edge_closing_edge] = path->edge_number_depth(true);
  d->m_increment_z[rounded_joins_no_closing_edge] = path->rounded_join_number_depth(false);
  d->m_increment_z[bevel_joins_no_closing_edge] = path->bevel_join_number_depth(false);
  d->m_increment_z[miter_joins_no_closing_edge] = path->miter_join_number_depth(false);
  d->m_increment_z[edge_no_closing_edge] = path->edge_number_depth(false);
  d->m_increment_z[rounded_cap] = path->cap_number_depth();
  d->m_increment_z[square_cap] = path->cap_number_depth();

  grab_attribute_index_data(make_c_array(d->m_attribute_data), attr_loc, path->rounded_cap_points(),
                            make_c_array(d->m_index_data), idx_loc, path->rounded_cap_indices(),
                            d->m_attribute_chunks[rounded_cap], d->m_index_chunks[rounded_cap]);

  grab_attribute_index_data(make_c_array(d->m_attribute_data), attr_loc, path->square_cap_points(),
                            make_c_array(d->m_index_data), idx_loc, path->square_cap_indices(),
                            d->m_attribute_chunks[square_cap], d->m_index_chunks[square_cap]);

#define GRAB_MACRO(enum_root_name, method_root_name) do {               \
    grab_attribute_index_data(make_c_array(d->m_attribute_data), attr_loc, \
                              path->method_root_name##_points(true),    \
                              make_c_array(d->m_index_data), idx_loc, \
                              path->method_root_name##_indices(true),   \
                              d->m_attribute_chunks[enum_root_name##_closing_edge], \
                              d->m_index_chunks[enum_root_name##_closing_edge]); \
    d->m_attribute_chunks[enum_root_name##_no_closing_edge] =              \
      d->m_attribute_chunks[enum_root_name##_closing_edge].sub_array(0, path->method_root_name##_points(false).size()); \
    unsigned int with, without;                                         \
    with = path->method_root_name##_indices(true).size();               \
    without = path->method_root_name##_indices(false).size();           \
    assert(with >= without);                                            \
    d->m_index_chunks[enum_root_name##_no_closing_edge] = d->m_index_chunks[enum_root_name##_closing_edge].sub_array(with - without); \
  } while(0)

  GRAB_MACRO(rounded_joins, rounded_joins);
  GRAB_MACRO(bevel_joins, bevel_joins);
  GRAB_MACRO(miter_joins, miter_joins);
  GRAB_MACRO(edge, edge);

#undef GRAB_MACRO

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
