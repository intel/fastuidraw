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
#include <fastuidraw/painter/painter_attribute_data_filler_path_fill.hpp>
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

  fastuidraw::PainterAttribute
  generate_attribute_stroke(const fastuidraw::StrokedPath::point &src)
  {
    fastuidraw::PainterAttribute dst;

    dst.m_attrib0 = fastuidraw::pack_vec4(src.m_position.x(),
                                          src.m_position.y(),
                                          src.m_pre_offset.x(),
                                          src.m_pre_offset.y());

    dst.m_attrib1 = fastuidraw::pack_vec4(src.m_distance_from_edge_start,
                                          src.m_distance_from_contour_start,
                                          src.m_auxilary_offset.x(),
                                          src.m_auxilary_offset.y());

    dst.m_attrib2 = fastuidraw::uvec4(src.m_packed_data,
                                      fastuidraw::pack_float(src.m_edge_length),
                                      fastuidraw::pack_float(src.m_open_contour_length),
                                      fastuidraw::pack_float(src.m_closed_contour_length));

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
    enum { number_join_types = 3 };

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
  return PainterAttributeDataFilledPathFill::index_chunk_from_winding_number(winding_number);
}

int
fastuidraw::PainterAttributeData::
winding_number_from_index_chunk(unsigned int idx)
{
  return PainterAttributeDataFilledPathFill::winding_number_from_index_chunk(idx);
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

void
fastuidraw::PainterAttributeData::
set_data(const reference_counted_ptr<const FilledPath> &path)
{
  PainterAttributeDataFilledPathFill filler(path);
  set_data(filler);
}

void
fastuidraw::PainterAttributeData::
set_data(const reference_counted_ptr<const StrokedPath> &path)
{
  PainterAttributeDataPrivate *d;
  d = reinterpret_cast<PainterAttributeDataPrivate*>(m_d);

  unsigned int num_attributes(0), num_indices(0);
  unsigned int attr_loc(0), idx_loc(0);
  unsigned int numJoins(0);

  for(unsigned int i = 0; i < StrokedPath::number_point_set_types; ++i)
    {
      enum StrokedPath::point_set_t tp;
      tp = static_cast<enum StrokedPath::point_set_t>(i);
      num_attributes += path->points(tp, true).size();
      num_indices += path->indices(tp, true).size();
    }

  for(unsigned int C = 0, endC = path->number_contours(); C < endC; ++C)
    {
      unsigned int endJ;

      endJ = path->number_joins(C);
      numJoins += endJ;
      for(unsigned int J = 0; J < endJ; ++J)
        {
          /* indices for joins appear twice, once all together
             and then again as seperate chunks.
           */
          num_indices += path->indices_range(StrokedPath::bevel_join_point_set, C, J).difference();
          num_indices += path->indices_range(StrokedPath::rounded_join_point_set, C, J).difference();
          num_indices += path->indices_range(StrokedPath::miter_join_point_set, C, J).difference();
        }
    }

  d->m_attribute_data.resize(num_attributes);
  d->m_index_data.resize(num_indices);
  d->m_attribute_chunks.resize(stroking_data_count + 1 + PainterAttributeDataPrivate::number_join_types * numJoins);
  d->m_index_chunks.resize(stroking_data_count + 1 + PainterAttributeDataPrivate::number_join_types * numJoins);

  d->m_increment_z.resize(stroking_data_count, 0);
  d->m_increment_z[edge_closing_edge] = path->number_depth(StrokedPath::edge_point_set, true);
  d->m_increment_z[bevel_joins_closing_edge] = path->number_depth(StrokedPath::bevel_join_point_set, true);
  d->m_increment_z[rounded_joins_closing_edge] = path->number_depth(StrokedPath::rounded_join_point_set, true);
  d->m_increment_z[miter_joins_closing_edge] = path->number_depth(StrokedPath::miter_join_point_set, true);

  d->m_increment_z[edge_no_closing_edge] = path->number_depth(StrokedPath::edge_point_set, false);
  d->m_increment_z[bevel_joins_no_closing_edge] = path->number_depth(StrokedPath::bevel_join_point_set, false);
  d->m_increment_z[rounded_joins_no_closing_edge] = path->number_depth(StrokedPath::rounded_join_point_set, false);
  d->m_increment_z[miter_joins_no_closing_edge] = path->number_depth(StrokedPath::miter_join_point_set, false);

  d->m_increment_z[square_cap] = path->number_depth(StrokedPath::square_cap_point_set, false);
  d->m_increment_z[rounded_cap] = path->number_depth(StrokedPath::rounded_cap_point_set, false);

  grab_attribute_index_data(make_c_array(d->m_attribute_data), attr_loc,
                            path->points(StrokedPath::square_cap_point_set, false),
                            make_c_array(d->m_index_data), idx_loc,
                            path->indices(StrokedPath::square_cap_point_set, false),
                            d->m_attribute_chunks[square_cap], d->m_index_chunks[square_cap]);

  grab_attribute_index_data(make_c_array(d->m_attribute_data), attr_loc,
                            path->points(StrokedPath::rounded_cap_point_set, false),
                            make_c_array(d->m_index_data), idx_loc,
                            path->indices(StrokedPath::rounded_cap_point_set, false),
                            d->m_attribute_chunks[rounded_cap], d->m_index_chunks[rounded_cap]);

#define GRAB_MACRO(dst_root_name, src_name) do {                        \
                                                                        \
    enum stroking_data_t closing_edge = dst_root_name##_closing_edge;   \
    enum stroking_data_t no_closing_edge = dst_root_name##_no_closing_edge; \
                                                                        \
    grab_attribute_index_data(make_c_array(d->m_attribute_data),        \
                              attr_loc,                                 \
                              path->points(src_name, true),             \
                              make_c_array(d->m_index_data), idx_loc,   \
                              path->indices(src_name, true),            \
                              d->m_attribute_chunks[closing_edge],      \
                              d->m_index_chunks[closing_edge]);         \
    d->m_attribute_chunks[no_closing_edge] =                            \
      d->m_attribute_chunks[closing_edge].sub_array(0, path->points(src_name, false).size()); \
                                                                        \
    unsigned int with, without;                                         \
    with = path->indices(src_name, true).size();                        \
    without = path->indices(src_name, false).size();                    \
    assert(with >= without);                                            \
                                                                        \
    d->m_index_chunks[no_closing_edge] =                                \
      d->m_index_chunks[closing_edge].sub_array(with - without);        \
  } while(0)

#define GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ) do {   \
    unsigned int Kc;                                                    \
    range_type<unsigned int> Ri, Ra;                                    \
    const_c_array<unsigned int> srcI;                                   \
    c_array<unsigned int> dst;                                          \
                                                                        \
    Ra = path->points_range(src_name, C, J);                            \
    Ri = path->indices_range(src_name, C, J);                           \
    srcI = path->indices(src_name, true).sub_array(Ri);                 \
    Kc = chunk_from_join(closing_edge, gJ);                             \
    d->m_attribute_chunks[Kc] = d->m_attribute_chunks[closing_edge].sub_array(Ra); \
    dst = make_c_array(d->m_index_data).sub_array(idx_loc, srcI.size()); \
    d->m_index_chunks[Kc] =  dst;                                       \
    for(unsigned int I = 0; I < srcI.size(); ++I)                       \
      {                                                                 \
        dst[I] = srcI[I] - Ra.m_begin;                                  \
      }                                                                 \
    idx_loc += srcI.size();                                             \
    ++gJ;                                                               \
  } while(0)

  /* note that the last two joins of each contour are grabbed last,
     this is to make sure that the joins for the closing edge are
     always at the end of our join list.
   */
#define GRAB_JOIN_MACRO(dst_root_name, src_name) do {                   \
    enum stroking_data_t closing_edge = dst_root_name##_closing_edge;   \
    unsigned int gJ = 0;                                                \
    for(unsigned int C = 0, endC = path->number_contours();             \
        C < endC; ++C)                                                  \
      {                                                                 \
        for(unsigned int J = 0, endJ = path->number_joins(C) - 2;       \
            J < endJ; ++J)                                        \
          {                                                             \
            GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ);   \
          }                                                             \
      }                                                                 \
    for(unsigned int C = 0, endC = path->number_contours();             \
        C < endC; ++C)                                                  \
      {                                                                 \
        if(path->number_joins(C) >= 2)                                  \
          {                                                             \
            unsigned int J;                                             \
            J = path->number_joins(C) - 1;                              \
            GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ);   \
            J = path->number_joins(C) - 2;                              \
            GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ);   \
          }                                                             \
      }                                                                 \
  } while(0)

  //grab the data for edges and joint together
  GRAB_MACRO(edge, StrokedPath::edge_point_set);
  GRAB_MACRO(bevel_joins, StrokedPath::bevel_join_point_set);
  GRAB_MACRO(rounded_joins, StrokedPath::rounded_join_point_set);
  GRAB_MACRO(miter_joins, StrokedPath::miter_join_point_set);

  // then grab individual joins, this must be done after
  // all the blocks because although it does not generate new
  // attribute data, it does generate new index data
  GRAB_JOIN_MACRO(rounded_joins, StrokedPath::rounded_join_point_set);
  GRAB_JOIN_MACRO(bevel_joins, StrokedPath::bevel_join_point_set);
  GRAB_JOIN_MACRO(miter_joins, StrokedPath::miter_join_point_set);

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

unsigned int
fastuidraw::PainterAttributeData::
chunk_from_join(enum stroking_data_t tp, unsigned int J)
{
  /*
    There are number_join_types joins, the chunk
    for the J'th join for type tp is located
    at number_join_types * J + t + stroking_data_count
    where 0 <=t < 6 is derived from tp.
   */
  unsigned int t;
  switch(tp)
    {
    case rounded_joins_closing_edge:
    case rounded_joins_no_closing_edge:
      t = 0u;
      break;

    case bevel_joins_closing_edge:
    case bevel_joins_no_closing_edge:
      t = 1u;
      break;

    case miter_joins_closing_edge:
    case miter_joins_no_closing_edge:
      t = 2u;
      break;

    default:
      assert(!"Type not mapping to join type");
      t = 0u;
    }

  /* The +1 is so that the chunk at stroking_data_count
     is left as empty for PainterAttributeData set
     from a StrokedPath (Painter relies on this!).
   */
  return stroking_data_count + t + 1
    + J * PainterAttributeDataPrivate::number_join_types;
}
