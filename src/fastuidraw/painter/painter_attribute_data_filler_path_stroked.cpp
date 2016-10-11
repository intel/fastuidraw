/*!
 * \file painter_attribute_data_filler_path_stroked.cpp
 * \brief file painter_attribute_data_filler_path_stroked.cpp
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
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_path_stroked.hpp>

namespace
{
  template<typename T>
  class PrivateWith
  {
  public:
    explicit
    PrivateWith(const T &data):
      m_data(data)
    {}

    const T &m_data;
  };

  typedef PrivateWith<fastuidraw::StrokedPath::Caps> PrivateCaps;
  typedef PrivateWith<fastuidraw::StrokedPath::Joins> PrivateJoins;
  typedef PrivateWith<fastuidraw::StrokedPath::Edges> PrivateEdges;

  fastuidraw::PainterAttribute
  generate_attribute(const fastuidraw::StrokedPath::point &src)
  {
    fastuidraw::PainterAttribute dst;
    src.pack_point(&dst);
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
    std::transform(attr_src.begin(), attr_src.end(), attrib_dst.begin(),
                   generate_attribute);

    attr_loc += attr_src.size();
    idx_loc += idx_src.size();
  }

  void
  grab_specific_join(const fastuidraw::StrokedPath::Joins &src,
                     unsigned int src_attrib_chunk,
                     unsigned int C, unsigned int J,
                     unsigned int &gJ, unsigned int Kc,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
                     fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                     unsigned int &idx_loc)
  {
    fastuidraw::range_type<unsigned int> Ri, Ra;
    fastuidraw::const_c_array<fastuidraw::PainterIndex> srcI;
    fastuidraw::c_array<fastuidraw::PainterIndex> dst;

    Ra = src.points_range(C, J);
    Ri = src.indices_range(C, J);
    srcI = src.indices(true).sub_array(Ri);
    attribute_chunks[Kc] = attribute_chunks[src_attrib_chunk].sub_array(Ra);
    dst = index_data.sub_array(idx_loc, srcI.size());
    index_chunks[Kc] =  dst;
    for(unsigned int I = 0; I < srcI.size(); ++I)
      {
        dst[I] = srcI[I] - Ra.m_begin;
      }
    idx_loc += srcI.size();
    ++gJ;
  }
}

//////////////////////////////////////////////
// fastuidraw::PainterAttributeDataFillerPathEdges methods
fastuidraw::PainterAttributeDataFillerPathEdges::
PainterAttributeDataFillerPathEdges(const fastuidraw::StrokedPath::Edges &edges)
{
  m_d = FASTUIDRAWnew PrivateEdges(edges);
}

fastuidraw::PainterAttributeDataFillerPathEdges::
~PainterAttributeDataFillerPathEdges()
{
  PrivateEdges *d;
  d = reinterpret_cast<PrivateEdges*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::StrokedPath::Edges&
fastuidraw::PainterAttributeDataFillerPathEdges::
edges(void) const
{
  PrivateEdges *d;
  d = reinterpret_cast<PrivateEdges*>(m_d);
  assert(d != NULL);
  return d->m_data;
}

void
fastuidraw::PainterAttributeDataFillerPathEdges::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_increments) const
{
  const StrokedPath::Edges &p(edges());
  num_attributes = p.points(true).size();
  num_indices =  p.indices(true).size();
  number_z_increments = num_attribute_chunks = num_index_chunks = 2;
}

void
fastuidraw::PainterAttributeDataFillerPathEdges::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<const_c_array<PainterAttribute> > attribute_chunks,
          c_array<const_c_array<PainterIndex> > index_chunks,
          c_array<unsigned int> zincrements) const
{
  const StrokedPath::Edges &p(edges());
  unsigned int attr_loc(0), idx_loc(0);

  zincrements[without_closing_edge] = p.number_depth(false);
  zincrements[with_closing_edge] = p.number_depth(true);

  grab_attribute_index_data(attribute_data, attr_loc, p.points(true),
                            index_data, idx_loc, p.indices(true),
                            attribute_chunks[with_closing_edge],
                            index_chunks[with_closing_edge]);

  attribute_chunks[without_closing_edge] =
    attribute_chunks[with_closing_edge].sub_array(0, p.points(false).size());

  unsigned int with, without;

  with = p.indices(true).size();
  without = p.indices(false).size();
  assert(with >= without);

  index_chunks[without_closing_edge] =
    index_chunks[with_closing_edge].sub_array(with - without);
}

/////////////////////////////////////////////////////////
// fastuidraw::PainterAttributeDataFillerPathCaps methods
fastuidraw::PainterAttributeDataFillerPathCaps::
PainterAttributeDataFillerPathCaps(const fastuidraw::StrokedPath::Caps &caps)
{
  m_d = FASTUIDRAWnew PrivateCaps(caps);
}

fastuidraw::PainterAttributeDataFillerPathCaps::
~PainterAttributeDataFillerPathCaps()
{
  PrivateCaps *d;
  d = reinterpret_cast<PrivateCaps*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::StrokedPath::Caps&
fastuidraw::PainterAttributeDataFillerPathCaps::
caps(void) const
{
  PrivateCaps *d;
  d = reinterpret_cast<PrivateCaps*>(m_d);
  assert(d != NULL);
  return d->m_data;
}

void
fastuidraw::PainterAttributeDataFillerPathCaps::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_increments) const
{
  const StrokedPath::Caps &p(caps());
  num_attributes = p.points().size();
  num_indices =  p.indices().size();
  number_z_increments = num_attribute_chunks = num_index_chunks = 1;
}

void
fastuidraw::PainterAttributeDataFillerPathCaps::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<const_c_array<PainterAttribute> > attribute_chunks,
          c_array<const_c_array<PainterIndex> > index_chunks,
          c_array<unsigned int> zincrements) const
{
  const StrokedPath::Caps &p(caps());
  unsigned int attr_loc(0), idx_loc(0);

  zincrements[0] = p.number_depth();
  grab_attribute_index_data(attribute_data, attr_loc, p.points(),
                            index_data, idx_loc, p.indices(),
                            attribute_chunks[0], index_chunks[0]);
}

/////////////////////////////////////////////////////////
// fastuidraw::PainterAttributeDataFillerPathJoins methods
fastuidraw::PainterAttributeDataFillerPathJoins::
PainterAttributeDataFillerPathJoins(const fastuidraw::StrokedPath::Joins &joins)
{
  m_d = FASTUIDRAWnew PrivateJoins(joins);
}

fastuidraw::PainterAttributeDataFillerPathJoins::
~PainterAttributeDataFillerPathJoins()
{
  PrivateJoins *d;
  d = reinterpret_cast<PrivateJoins*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::StrokedPath::Joins&
fastuidraw::PainterAttributeDataFillerPathJoins::
joins(void) const
{
  PrivateJoins *d;
  d = reinterpret_cast<PrivateJoins*>(m_d);
  assert(d != NULL);
  return d->m_data;
}

void
fastuidraw::PainterAttributeDataFillerPathJoins::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_increments) const
{
  const StrokedPath::Joins &p(joins());
  unsigned int numJoins(0);

  num_attributes = p.points(true).size();
  num_indices = p.indices(true).size();
  for(unsigned int C = 0, endC = p.number_contours(); C < endC; ++C)
    {
      unsigned int endJ;

      endJ = p.number_joins(C);
      numJoins += endJ;
      for(unsigned int J = 0; J < endJ; ++J)
        {
          /* indices for joins appear twice, once all together
             and then again as seperate chunks.
           */
          num_indices += p.indices_range(C, J).difference();
        }
    }
  num_attribute_chunks = num_index_chunks = number_z_increments = specific_join0 + numJoins;
}

void
fastuidraw::PainterAttributeDataFillerPathJoins::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<const_c_array<PainterAttribute> > attribute_chunks,
          c_array<const_c_array<PainterIndex> > index_chunks,
          c_array<unsigned int> zincrements) const
{
  const StrokedPath::Joins &p(joins());
  unsigned int attr_loc(0), idx_loc(0);

  zincrements[without_closing_edge] = p.number_depth(false);
  zincrements[with_closing_edge] = p.number_depth(true);

  grab_attribute_index_data(attribute_data,  attr_loc, p.points(true),
                            index_data, idx_loc, p.indices(true),
                            attribute_chunks[with_closing_edge],
                            index_chunks[with_closing_edge]);

  attribute_chunks[without_closing_edge] =
    attribute_chunks[with_closing_edge].sub_array(0, p.points(false).size());

  unsigned int with, without;

  with = p.indices(true).size();
  without = p.indices(false).size();
  assert(with >= without);

  index_chunks[without_closing_edge] =
    index_chunks[with_closing_edge].sub_array(with - without);

  unsigned int gJ = 0;
  for(unsigned int C = 0, endC = p.number_contours(); C < endC; ++C)
    {
      assert(p.number_joins(C) >= 2);
      for(unsigned int J = 0, endJ = p.number_joins(C) - 2; J < endJ; ++J)
        {
          grab_specific_join(p, with_closing_edge, C, J, gJ, gJ + specific_join0,
                             index_data, attribute_chunks,
                             index_chunks, idx_loc);
        }
    }

  for(unsigned int C = 0, endC = p.number_contours(); C < endC; ++C)
    {
      unsigned int J;
      J = p.number_joins(C) - 1;
      grab_specific_join(p, with_closing_edge, C, J, gJ, gJ + specific_join0,
                         index_data, attribute_chunks,
                         index_chunks, idx_loc);
      J = p.number_joins(C) - 2;
      grab_specific_join(p, with_closing_edge, C, J, gJ, gJ + specific_join0,
                         index_data, attribute_chunks,
                         index_chunks, idx_loc);
    }
}

unsigned int
fastuidraw::PainterAttributeDataFillerPathJoins::
number_joins(const PainterAttributeData &data, bool edge_closed)
{
  unsigned int K;
  K = edge_closed ? with_closing_edge : without_closing_edge;
  return data.increment_z_value(K);
}


unsigned int
fastuidraw::PainterAttributeDataFillerPathJoins::
named_join_chunk(unsigned int J)
{
  return J + specific_join0;
}
