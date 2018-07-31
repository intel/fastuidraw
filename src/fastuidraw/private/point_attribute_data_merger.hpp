#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler.hpp>

namespace fastuidraw { namespace detail {

template<typename Point>
class PointAttributeDataMerger:public PainterAttributeDataFiller
{
public:
  PointAttributeDataMerger(const PainterAttributeData &srcA,
                           const PainterAttributeData &srcB,
                           unsigned int all_edges_chunk,
                           unsigned int only_non_closing_edges_chunk):
    m_srcA(srcA),
    m_srcB(srcB),
    m_all_edges(all_edges_chunk),
    m_only_non_closing_edges(only_non_closing_edges_chunk)
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
  fill_data_helper_idx(c_array<const PainterIndex> src,
                       unsigned int src_vert_start,
                       c_array<PainterIndex> dst,
                       unsigned int &idx_dst,
                       unsigned int dst_vert_start);

  const PainterAttributeData &m_srcA, &m_srcB;
  unsigned int m_all_edges, m_only_non_closing_edges;
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
  const unsigned int chk(m_all_edges);

  num_attributes = m_srcA.attribute_data_chunk(chk).size()
    + m_srcB.attribute_data_chunk(chk).size();

  num_indices = m_srcA.index_data_chunk(chk).size()
    + m_srcB.index_data_chunk(chk).size();

  num_attribute_chunks = num_index_chunks = number_z_ranges = 2;
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
   *   m_srcA non-closing edges
   *   m_srcB non-closing edges
   *   m_srcA closing edges
   *   m_srcB closing edges
   *
   * Index data is to be packed as
   *  m_srcA closing edges
   *  m_srcB closing edges
   *  m_srcA non-closing edges
   *  m_srcB non-closing edges
   *
   * The depth values need to be adjusted with respect to the
   * index data packing order, with the largest values
   * coming first.
   */

  range_type<int> srcA_non_closing_depth;
  range_type<int> srcB_non_closing_depth;
  range_type<int> srcA_closing_depth;
  range_type<int> srcB_closing_depth;
  range_type<int> dstA_non_closing_depth;
  range_type<int> dstB_non_closing_depth;
  range_type<int> dstA_closing_depth;
  range_type<int> dstB_closing_depth;

  srcA_non_closing_depth = m_srcA.z_range(m_only_non_closing_edges);
  srcA_closing_depth.m_begin = srcA_non_closing_depth.m_end;
  srcA_closing_depth.m_end = m_srcA.z_range(m_all_edges).m_end;

  srcB_non_closing_depth = m_srcB.z_range(m_only_non_closing_edges);
  srcB_closing_depth.m_begin = srcB_non_closing_depth.m_end;
  srcB_closing_depth.m_end = m_srcB.z_range(m_all_edges).m_end;

  dstB_non_closing_depth.m_begin = 0;
  dstB_non_closing_depth.m_end = dstB_non_closing_depth.m_begin + srcB_non_closing_depth.difference();

  dstA_non_closing_depth.m_begin = dstB_non_closing_depth.m_end;
  dstA_non_closing_depth.m_end = dstA_non_closing_depth.m_begin + srcA_non_closing_depth.difference();

  dstB_closing_depth.m_begin = dstA_non_closing_depth.m_end;
  dstB_closing_depth.m_end = dstB_closing_depth.m_begin + srcB_closing_depth.difference();

  dstA_closing_depth.m_begin = dstB_closing_depth.m_end;
  dstA_closing_depth.m_end = dstA_closing_depth.m_begin + srcA_closing_depth.difference();

  unsigned int q, vert_dst(0), idx_dst(0);
  unsigned int dstA_closing_vert_begin, dstB_closing_vert_begin;
  unsigned int dstA_non_closing_vert_begin, dstB_non_closing_vert_begin;
  unsigned int dst_non_closing_idx_begin;

  /////////////////////////////////////////////////////////
  // Fill the attribute buffer with the vertices with their
  // depth values shifted.
  dstA_non_closing_vert_begin = vert_dst;
  fill_data_helper_attr(m_srcA.attribute_data_chunk(m_only_non_closing_edges),
                        srcA_non_closing_depth,
                        attribute_data, vert_dst,
                        dstA_non_closing_depth);

  dstB_non_closing_vert_begin = vert_dst;
  fill_data_helper_attr(m_srcB.attribute_data_chunk(m_only_non_closing_edges),
                        srcB_non_closing_depth,
                        attribute_data, vert_dst,
                        dstB_non_closing_depth);


  q = m_srcA.attribute_data_chunk(m_only_non_closing_edges).size();
  dstA_closing_vert_begin = vert_dst;
  fill_data_helper_attr(m_srcA.attribute_data_chunk(m_all_edges).sub_array(q),
                        srcA_closing_depth,
                        attribute_data, vert_dst,
                        dstA_closing_depth);


  q = m_srcB.attribute_data_chunk(m_only_non_closing_edges).size();
  dstB_closing_vert_begin = vert_dst;
  fill_data_helper_attr(m_srcB.attribute_data_chunk(m_all_edges).sub_array(q),
                        srcB_closing_depth,
                        attribute_data, vert_dst,
                        dstB_closing_depth);

  ////////////////////////////////////////////////////
  // Fill the index buffers.
  q = m_srcA.index_data_chunk(m_all_edges).size()
    - m_srcA.index_data_chunk(m_only_non_closing_edges).size();
  fill_data_helper_idx(m_srcA.index_data_chunk(m_all_edges).sub_array(0, q),
                       m_srcA.attribute_data_chunk(m_only_non_closing_edges).size(),
                       index_data, idx_dst,
                       dstA_closing_vert_begin);

  q = m_srcB.index_data_chunk(m_all_edges).size()
    - m_srcB.index_data_chunk(m_only_non_closing_edges).size();
  fill_data_helper_idx(m_srcB.index_data_chunk(m_all_edges).sub_array(0, q),
                       m_srcB.attribute_data_chunk(m_only_non_closing_edges).size(),
                       index_data, idx_dst,
                       dstB_closing_vert_begin);

  dst_non_closing_idx_begin = idx_dst;
  fill_data_helper_idx(m_srcA.index_data_chunk(m_only_non_closing_edges),
                       0,
                       index_data, idx_dst,
                       dstA_non_closing_vert_begin);

  fill_data_helper_idx(m_srcB.index_data_chunk(m_only_non_closing_edges),
                       0,
                       index_data, idx_dst,
                       dstB_non_closing_vert_begin);

  ////////////////////////////////////////////////////////////////////////////
  // assign the chunks pointers and zrange values along with the index_adjusts
  attribute_chunks[m_all_edges] = attribute_data;
  index_chunks[m_all_edges] = index_data;

  attribute_chunks[m_only_non_closing_edges] = attribute_data.sub_array(0, dstA_closing_vert_begin);
  index_chunks[m_only_non_closing_edges] = index_data.sub_array(dst_non_closing_idx_begin);

  zranges[m_all_edges].m_begin = 0;
  zranges[m_all_edges].m_end = dstA_closing_depth.m_end;

  zranges[m_only_non_closing_edges].m_begin = 0;
  zranges[m_only_non_closing_edges].m_end = dstA_non_closing_depth.m_end;

  index_adjusts[m_all_edges] = 0;
  index_adjusts[m_only_non_closing_edges] = 0;
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
fill_data_helper_idx(c_array<const PainterIndex> src,
                     unsigned int src_vert_start,
                     c_array<PainterIndex> dst,
                     unsigned int &idx_dst,
                     unsigned int dst_vert_start)
{
  for (unsigned int i = 0; i < src.size(); ++i, ++idx_dst)
    {
      PainterIndex v;

      v = src[i];
      FASTUIDRAWassert(v >= src_vert_start);

      v -= src_vert_start;
      v += dst_vert_start;
      dst[idx_dst] = v;
    }
}


}}
