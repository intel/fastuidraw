/*!
 * \file point_attribute_data_merger.hpp
 * \brief file point_attribute_data_merger.hpp
 *
 * Copyright 2019 by Intel.
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

#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data_filler.hpp>

namespace fastuidraw { namespace detail {

template<typename Point>
class PointAttributeDataMerger:public PainterAttributeDataFiller
{
public:
  PointAttributeDataMerger(const PainterAttributeData &srcA,
                           const PainterAttributeData &srcB):
    m_srcA(srcA),
    m_srcB(srcB)
  {}

  virtual
  void
  compute_sizes(unsigned int &num_attributes,
                unsigned int &num_indices,
                unsigned int &num_attribute_chunks,
                unsigned int &num_index_chunks,
                unsigned int &number_z_ranges) const;

  virtual
  void
  fill_data(c_array<PainterAttribute> attribute_data,
            c_array<PainterIndex> index_data,
            c_array<c_array<const PainterAttribute> > attribute_chunks,
            c_array<c_array<const PainterIndex> > index_chunks,
            c_array<range_type<int> > zranges,
            c_array<int> index_adjusts) const;
private:
  static
  void
  fill_data_helper_attr(c_array<const PainterAttribute> src,
                        range_type<int> src_depth_range,
                        c_array<PainterAttribute> dst, unsigned int &vert_dst,
                        range_type<int> dst_depth_range);

  static
  void
  fill_data_helper_idx(unsigned int incr_idx,
                       c_array<const PainterIndex> src,
                       c_array<PainterIndex> dst,
                       unsigned int &idx_dst);

  const PainterAttributeData &m_srcA, &m_srcB;
};

//////////////////////////////////////////
// PointAttributeDataMerger methods
template<typename Point>
void
PointAttributeDataMerger<Point>::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_ranges) const
{
  const unsigned int chk(0);

  num_attributes = m_srcA.attribute_data_chunk(chk).size()
    + m_srcB.attribute_data_chunk(chk).size();

  num_indices = m_srcA.index_data_chunk(chk).size()
    + m_srcB.index_data_chunk(chk).size();

  num_attribute_chunks = num_index_chunks = number_z_ranges = 1;
}

template<typename Point>
void
PointAttributeDataMerger<Point>::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<c_array<const PainterAttribute> > attribute_chunks,
          c_array<c_array<const PainterIndex> > index_chunks,
          c_array<range_type<int> > zranges,
          c_array<int> index_adjusts) const
{
  /* the attribute data is ordered: first non-closing edges, then closing edges
   * the index data is ordered: first closing-edges, then non-closing edges.
   * the closing edge depth values come after the non-closing edge depth values.
   *
   * Attribute data is to be packed as
   *   m_srcA edges
   *   m_srcB edges
   *
   * Index data is to be packed as
   *  m_srcA edges
   *  m_srcB edges
   *
   * The depth values need to be adjusted with respect to the
   * index data packing order, with the largest values
   * coming first; the only modification needed then is to just
   * add to the depth values of A the max of the depth range of B
   */

  range_type<int> srcA_depth;
  range_type<int> srcB_depth;
  range_type<int> dstA_depth;
  range_type<int> dstB_depth;
  unsigned int num_vertsA, vert_dst(0), idx_dst(0);

  srcA_depth = m_srcA.z_range(0);
  srcB_depth = m_srcB.z_range(0);

  dstA_depth = srcA_depth;
  dstB_depth = srcB_depth;
  dstA_depth.m_begin += dstB_depth.m_end;
  dstA_depth.m_end += dstB_depth.m_end;

  fill_data_helper_attr(m_srcA.attribute_data_chunk(0),
                        srcA_depth,
                        attribute_data, vert_dst,
                        dstA_depth);
  num_vertsA = vert_dst;

  fill_data_helper_attr(m_srcB.attribute_data_chunk(0),
                        srcB_depth,
                        attribute_data, vert_dst,
                        dstB_depth);

  fill_data_helper_idx(0, m_srcA.index_data_chunk(0),
                       index_data, idx_dst);

  fill_data_helper_idx(num_vertsA, m_srcB.index_data_chunk(0),
                       index_data, idx_dst);

  ////////////////////////////////////////////////////////////////////////////
  // assign the chunks pointers and zrange values along with the index_adjusts
  attribute_chunks[0] = attribute_data;
  index_chunks[0] = index_data;
  zranges[0].m_begin = 0;
  zranges[0].m_end = dstA_depth.m_end;
  index_adjusts[0] = 0;
}

template<typename Point>
void
PointAttributeDataMerger<Point>::
fill_data_helper_attr(c_array<const PainterAttribute> src,
                      range_type<int> src_depth_range,
                      c_array<PainterAttribute> dst, unsigned int &vert_dst,
                      range_type<int> dst_depth_range)
{
  FASTUIDRAWassert(src_depth_range.difference() == dst_depth_range.difference());
  for (unsigned int i = 0; i < src.size(); ++i, ++vert_dst)
    {
      Point P;
      unsigned int d;

      Point::unpack_point(&P, src[i]);
      d = P.depth();

      FASTUIDRAWassert(static_cast<int>(d) >= src_depth_range.m_begin);
      FASTUIDRAWassert(static_cast<int>(d) < src_depth_range.m_end);
      d -= src_depth_range.m_begin;
      d += dst_depth_range.m_begin;
      P.depth(d);
      P.pack_point(&dst[vert_dst]);
    }
}

template<typename Point>
void
PointAttributeDataMerger<Point>::
fill_data_helper_idx(unsigned int incr_idx,
                     c_array<const PainterIndex> src,
                     c_array<PainterIndex> dst,
                     unsigned int &idx_dst)
{
  for (unsigned int i = 0; i < src.size(); ++i, ++idx_dst)
    {
      dst[idx_dst] = src[i] + incr_idx;
    }
}


}}
