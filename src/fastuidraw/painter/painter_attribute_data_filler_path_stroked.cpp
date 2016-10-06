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

  /*!
    Enumation values are indexes into attribute_data_chunks()
    and index_data_chunks() for different portions of
    data needed for stroking a path when the data of this
    PainterAttributeData has been set with
    set_data(const reference_counted_ptr<const StrokedPath> &).
  */
  enum stroking_data_t
    {
      rounded_joins_closing_edge, /*!< index for rounded join data with closing edge */
      bevel_joins_closing_edge, /*!< index for bevel join data with closing edge */
      miter_joins_closing_edge, /*!< index for miter join data with closing edge */
      edge_closing_edge, /*!< index for edge data including closing edge */

      number_with_closing_edge, /*!< number of types with closing edge */

      rounded_joins_no_closing_edge = number_with_closing_edge, /*!< index for rounded join data without closing edge */
      bevel_joins_no_closing_edge, /*!< index for bevel join data without closing edge */
      miter_joins_no_closing_edge, /*!< index for miter join data without closing edge */
      edge_no_closing_edge, /*!< index for edge data not including closing edge */

      rounded_cap, /*!< index for rounded cap data */
      square_cap,  /*!< index for square cap data */
      adjustable_cap,

      /*!
        count of enums, using this enumeration when on data created
        from a StrokedPath, gives empty indices and attributes.
      */
      stroking_data_count
    };

  enum stroking_data_t
  without_closing_edge(enum stroking_data_t v)
  {
    return (v < number_with_closing_edge) ?
      static_cast<enum stroking_data_t>(v + number_with_closing_edge) :
      v;
  }

  class ChunkSelector:public fastuidraw::StrokingChunkSelectorBase
  {
  public:
    static
    enum stroking_data_t
    static_cap_chunk(enum fastuidraw::PainterEnums::cap_style cp);

    static
    enum stroking_data_t
    static_edge_chunk(bool edge_closed);

    static
    enum stroking_data_t
    static_join_chunk(enum fastuidraw::PainterEnums::join_style js, bool edge_closed);

    static
    unsigned int
    static_named_join_chunk(enum stroking_data_t join, unsigned int J);

    static
    unsigned int
    static_adjustable_cap_chunk(void)
    {
      return adjustable_cap;
    }

    static
    unsigned int
    static_number_joins(const fastuidraw::PainterAttributeData &data, bool edge_closed)
    {
      /* increment_z_value() holds the number of joins
       */
      return edge_closed ?
        data.increment_z_value(rounded_joins_closing_edge) :
        data.increment_z_value(rounded_joins_no_closing_edge);
    }

    virtual
    unsigned int
    number_joins(const fastuidraw::PainterAttributeData &data, bool edge_closed) const
    {
      return static_number_joins(data, edge_closed);
    }

    virtual
    unsigned int
    cap_chunk(enum fastuidraw::PainterEnums::cap_style cp) const
    {
      return static_cap_chunk(cp);
    }

    virtual
    unsigned int
    adjustable_cap_chunk(void) const
    {
      return static_adjustable_cap_chunk();
    }

    virtual
    unsigned int
    edge_chunk(bool edge_closed) const
    {
      return static_edge_chunk(edge_closed);
    }

    virtual
    unsigned int
    join_chunk(enum fastuidraw::PainterEnums::join_style js, bool edge_closed) const
    {
      return static_join_chunk(js, edge_closed);
    }

    virtual
    unsigned int
    named_join_chunk(enum fastuidraw::PainterEnums::join_style js, unsigned int J) const
    {
      enum stroking_data_t join;
      join = static_join_chunk(js, true);
      return static_named_join_chunk(join, J);
    }
  };

  class PathStrokerPrivate
  {
  public:
    enum { number_join_types = 3 };

    /*!
      Given an enumeration of stroking_data_t, returns
      the matching enumeration for drawing without the
      closing edge.
     */
    static
    enum stroking_data_t
    without_closing_edge(enum stroking_data_t v);


    PathStrokerPrivate(const fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> &p):
      m_path(p)
    {}

    void
    grab_specific_join(const fastuidraw::StrokedPath::Joins &src,
                       enum stroking_data_t closing_edge,
                       unsigned int C, unsigned int J,
                       unsigned int &gJ,
                       fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                       fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
                       fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                       unsigned int &idx_loc);

    fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> m_path;
  };

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
                   fastuidraw::PainterAttributeDataFillerPathStroked::pack_point);

    attr_loc += attr_src.size();
    idx_loc += idx_src.size();
  }
}

/////////////////////////////////////////////////////
// ChunkSelector methods
enum stroking_data_t
ChunkSelector::
static_cap_chunk(enum fastuidraw::PainterEnums::cap_style cp)
{
  using namespace fastuidraw::PainterEnums;
  enum stroking_data_t cap;

  switch(cp)
    {
    case rounded_caps:
      cap = rounded_cap;
      break;
    case square_caps:
      cap = square_cap;
      break;
    default:
      cap = stroking_data_count;
    }
  return cap;
}

enum stroking_data_t
ChunkSelector::
static_edge_chunk(bool edge_closed)
{
  return edge_closed ?
    edge_closing_edge :
    edge_no_closing_edge;
}

enum stroking_data_t
ChunkSelector::
static_join_chunk(enum fastuidraw::PainterEnums::join_style js, bool edge_closed)
{
  using namespace fastuidraw::PainterEnums;
  enum stroking_data_t join;

  switch(js)
    {
    case rounded_joins:
      join = rounded_joins_closing_edge;
      break;
    case bevel_joins:
      join = bevel_joins_closing_edge;
      break;
    case miter_joins:
      join = miter_joins_closing_edge;
      break;
    default:
      join = stroking_data_count;
    }

  if(!edge_closed)
    {
      join = without_closing_edge(join);
    }
  return join;
}

unsigned int
ChunkSelector::
static_named_join_chunk(enum stroking_data_t join, unsigned int J)
{
  /*
    There are number_join_types joins, the chunk
    for the J'th join for type tp is located
    at number_join_types * J + t + stroking_data_count
    where 0 <=t < number_join_types is derived from tp.
   */
  unsigned int t;
  switch(join)
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
    + J * PathStrokerPrivate::number_join_types;
}

////////////////////////////////////////////////
// PathStrokerPrivate methods
void
PathStrokerPrivate::
grab_specific_join(const fastuidraw::StrokedPath::Joins &src,
                   enum stroking_data_t closing_edge,
                   unsigned int C, unsigned int J,
                   unsigned int &gJ,
                   fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                   fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
                   fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                   unsigned int &idx_loc)
{
  unsigned int Kc;
  fastuidraw::range_type<unsigned int> Ri, Ra;
  fastuidraw::const_c_array<fastuidraw::PainterIndex> srcI;
  fastuidraw::c_array<fastuidraw::PainterIndex> dst;

  Ra = src.points_range(C, J);
  Ri = src.indices_range(C, J);
  srcI = src.indices(true).sub_array(Ri);
  Kc = ChunkSelector::static_named_join_chunk(closing_edge, gJ);
  attribute_chunks[Kc] = attribute_chunks[closing_edge].sub_array(Ra);
  dst = index_data.sub_array(idx_loc, srcI.size());
  index_chunks[Kc] =  dst;
  for(unsigned int I = 0; I < srcI.size(); ++I)
    {
      dst[I] = srcI[I] - Ra.m_begin;
    }
  idx_loc += srcI.size();
  ++gJ;
}

//////////////////////////////////////////////////////////
// fastuidraw::PainterAttributeDataFillerPathStroked methods
fastuidraw::PainterAttributeDataFillerPathStroked::
PainterAttributeDataFillerPathStroked(const reference_counted_ptr<const StrokedPath> &path)
{
  m_d = FASTUIDRAWnew PathStrokerPrivate(path);
}

fastuidraw::PainterAttributeDataFillerPathStroked::
~PainterAttributeDataFillerPathStroked()
{
  PathStrokerPrivate *d;
  d = reinterpret_cast<PathStrokerPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath>&
fastuidraw::PainterAttributeDataFillerPathStroked::
path(void) const
{
  PathStrokerPrivate *d;
  d = reinterpret_cast<PathStrokerPrivate*>(m_d);
  assert(d != NULL);
  return d->m_path;
}

void
fastuidraw::PainterAttributeDataFillerPathStroked::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_increments) const
{
  const StrokedPath *p(path().get());
  if(p == NULL)
    {
      return;
    }

  unsigned int numJoins(0);

  num_attributes = p->edges().points(true).size()
    + p->bevel_joins().points(true).size()
    + p->miter_joins().points(true).size()
    + p->rounded_joins().points(true).size()
    + p->square_caps().points().size()
    + p->rounded_caps().points().size()
    + p->adjustable_caps().points().size();

  num_indices =  p->edges().indices(true).size()
    + p->bevel_joins().indices(true).size()
    + p->miter_joins().indices(true).size()
    + p->rounded_joins().indices(true).size()
    + p->square_caps().indices().size()
    + p->rounded_caps().indices().size()
    + p->adjustable_caps().indices().size();

  for(unsigned int C = 0, endC = p->bevel_joins().number_contours(); C < endC; ++C)
    {
      unsigned int endJ;

      endJ = p->bevel_joins().number_joins(C);
      assert(endJ == p->miter_joins().number_joins(C));
      assert(endJ == p->rounded_joins().number_joins(C));

      numJoins += endJ;
      for(unsigned int J = 0; J < endJ; ++J)
        {
          /* indices for joins appear twice, once all together
             and then again as seperate chunks.
           */
          num_indices += p->bevel_joins().indices_range(C, J).difference();
          num_indices += p->miter_joins().indices_range(C, J).difference();
          num_indices += p->rounded_joins().indices_range(C, J).difference();
        }
    }

  num_attribute_chunks = stroking_data_count + 1 + PathStrokerPrivate::number_join_types * numJoins;
  num_index_chunks = num_attribute_chunks;
  number_z_increments = stroking_data_count;
}


void
fastuidraw::PainterAttributeDataFillerPathStroked::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<const_c_array<PainterAttribute> > attribute_chunks,
          c_array<const_c_array<PainterIndex> > index_chunks,
          c_array<unsigned int> zincrements) const
{
  const StrokedPath *p(path().get());

  if(p == NULL)
    {
      return;
    }

  PathStrokerPrivate *d;
  d = reinterpret_cast<PathStrokerPrivate*>(m_d);

  unsigned int attr_loc(0), idx_loc(0);

  zincrements[edge_closing_edge] = p->edges().number_depth(true);
  zincrements[bevel_joins_closing_edge] = p->bevel_joins().number_depth(true);
  zincrements[rounded_joins_closing_edge] = p->rounded_joins().number_depth(true);
  zincrements[miter_joins_closing_edge] = p->miter_joins().number_depth(true);

  zincrements[edge_no_closing_edge] = p->edges().number_depth(false);
  zincrements[bevel_joins_no_closing_edge] = p->bevel_joins().number_depth(false);
  zincrements[rounded_joins_no_closing_edge] = p->rounded_joins().number_depth(false);
  zincrements[miter_joins_no_closing_edge] = p->miter_joins().number_depth(false);

  zincrements[square_cap] = p->square_caps().number_depth();
  zincrements[rounded_cap] = p->rounded_caps().number_depth();
  zincrements[adjustable_cap] = p->adjustable_caps().number_depth();

  grab_attribute_index_data(attribute_data, attr_loc, p->square_caps().points(),
                            index_data, idx_loc, p->square_caps().indices(),
                            attribute_chunks[square_cap], index_chunks[square_cap]);

  grab_attribute_index_data(attribute_data, attr_loc, p->rounded_caps().points(),
                            index_data, idx_loc, p->rounded_caps().indices(),
                            attribute_chunks[rounded_cap], index_chunks[rounded_cap]);

  grab_attribute_index_data(attribute_data, attr_loc, p->adjustable_caps().points(),
                            index_data, idx_loc, p->adjustable_caps().indices(),
                            attribute_chunks[adjustable_cap], index_chunks[adjustable_cap]);

#define GRAB_MACRO(dst_root_name, src_name) do {                        \
                                                                        \
    enum stroking_data_t closing_edge = dst_root_name##_closing_edge;   \
    enum stroking_data_t no_closing_edge = dst_root_name##_no_closing_edge; \
                                                                        \
    grab_attribute_index_data(attribute_data,  attr_loc,                \
                              p->src_name().points(true),               \
                              index_data, idx_loc,                      \
                              p->src_name().indices(true),              \
                              attribute_chunks[closing_edge],           \
                              index_chunks[closing_edge]);              \
    attribute_chunks[no_closing_edge] =                                 \
      attribute_chunks[closing_edge].sub_array(0, p->src_name().points(false).size()); \
                                                                        \
    unsigned int with, without;                                         \
    with = p->src_name().indices(true).size();                          \
    without = p->src_name().indices(false).size();                      \
    assert(with >= without);                                            \
                                                                        \
    index_chunks[no_closing_edge] =                                     \
      index_chunks[closing_edge].sub_array(with - without);             \
  } while(0)

  /* note that the last two joins of each contour are grabbed last,
     this is to make sure that the joins for the closing edge are
     always at the end of our join list.
   */
#define GRAB_JOIN_MACRO(dst_root_name, src_name) do {                   \
    enum stroking_data_t closing_edge = dst_root_name##_closing_edge;   \
    unsigned int gJ = 0;                                                \
    for(unsigned int C = 0, endC = p->src_name().number_contours();     \
        C < endC; ++C)                                                  \
      {                                                                 \
        assert(p->src_name().number_joins(C) >= 2);                     \
        for(unsigned int J = 0, endJ = p->src_name().number_joins(C) - 2; \
            J < endJ; ++J)                                              \
          {                                                             \
            d->grab_specific_join(p->src_name(), closing_edge, C, J,    \
                                  gJ, index_data, attribute_chunks,     \
                                  index_chunks, idx_loc);               \
          }                                                             \
      }                                                                 \
    for(unsigned int C = 0, endC = p->src_name().number_contours();     \
        C < endC; ++C)                                                  \
      {                                                                 \
        unsigned int J;                                                 \
        J = p->src_name().number_joins(C) - 1;                          \
        d->grab_specific_join(p->src_name(), closing_edge, C, J,        \
                              gJ, index_data, attribute_chunks,         \
                              index_chunks, idx_loc);                   \
        J = p->src_name().number_joins(C) - 2;                          \
        d->grab_specific_join(p->src_name(), closing_edge, C, J,        \
                              gJ, index_data, attribute_chunks,         \
                              index_chunks, idx_loc);                   \
      }                                                                 \
  } while(0)

  //grab the data for edges and joint together
  GRAB_MACRO(edge, edges);
  GRAB_MACRO(bevel_joins, bevel_joins);
  GRAB_MACRO(rounded_joins, rounded_joins);
  GRAB_MACRO(miter_joins, miter_joins);

  // then grab individual joins, this must be done after
  // all the blocks because although it does not generate new
  // attribute data, but it does generate new index data
  GRAB_JOIN_MACRO(bevel_joins, bevel_joins);
  GRAB_JOIN_MACRO(rounded_joins, rounded_joins);
  GRAB_JOIN_MACRO(miter_joins, miter_joins);
}


fastuidraw::reference_counted_ptr<fastuidraw::StrokingChunkSelectorBase>
fastuidraw::PainterAttributeDataFillerPathStroked::
chunk_selector(void)
{
  return FASTUIDRAWnew ChunkSelector();
}


fastuidraw::StrokedPath::point
fastuidraw::PainterAttributeDataFillerPathStroked::
unpack_point(const PainterAttribute &a)
{
  StrokedPath::point dst;

  dst.m_position.x() = unpack_float(a.m_attrib0.x());
  dst.m_position.y() = unpack_float(a.m_attrib0.y());

  dst.m_pre_offset.x() = unpack_float(a.m_attrib0.z());
  dst.m_pre_offset.y() = unpack_float(a.m_attrib0.w());

  dst.m_distance_from_edge_start = unpack_float(a.m_attrib1.x());
  dst.m_distance_from_contour_start = unpack_float(a.m_attrib1.y());
  dst.m_auxilary_offset.x() = unpack_float(a.m_attrib1.z());
  dst.m_auxilary_offset.y() = unpack_float(a.m_attrib1.w());

  dst.m_packed_data = a.m_attrib2.x();
  dst.m_edge_length = unpack_float(a.m_attrib2.y());
  dst.m_open_contour_length = unpack_float(a.m_attrib2.z());
  dst.m_closed_contour_length = unpack_float(a.m_attrib2.w());

  return dst;
}

fastuidraw::PainterAttribute
fastuidraw::PainterAttributeDataFillerPathStroked::
pack_point(const StrokedPath::point &src)
{
  PainterAttribute dst;

  dst.m_attrib0 = pack_vec4(src.m_position.x(),
                            src.m_position.y(),
                            src.m_pre_offset.x(),
                            src.m_pre_offset.y());

  dst.m_attrib1 = pack_vec4(src.m_distance_from_edge_start,
                            src.m_distance_from_contour_start,
                            src.m_auxilary_offset.x(),
                            src.m_auxilary_offset.y());

  dst.m_attrib2 = uvec4(src.m_packed_data,
                        pack_float(src.m_edge_length),
                        pack_float(src.m_open_contour_length),
                        pack_float(src.m_closed_contour_length));

  return dst;
}
