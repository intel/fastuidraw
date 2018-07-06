/*!
 * \file stroked_caps_joins.cpp
 * \brief file stroked_caps_joins.cpp
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

#include <vector>
#include <complex>
#include <algorithm>

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/stroked_point.hpp>
#include <fastuidraw/painter/arc_stroked_point.hpp>
#include <fastuidraw/painter/stroked_caps_joins.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler.hpp>
#include <fastuidraw/painter/painter_dashed_stroke_shader_set.hpp>
#include "../private/util_private.hpp"
#include "../private/util_private_ostream.hpp"
#include "../private/bounding_box.hpp"
#include "../private/path_util_private.hpp"
#include "../private/clip.hpp"


namespace
{
  inline
  uint32_t
  pack_data(int on_boundary,
            enum fastuidraw::StrokedPoint::offset_type_t pt,
            uint32_t depth)
  {
    return fastuidraw::detail::stroked_point_pack_bits(on_boundary, pt, depth);
  }

  inline
  uint32_t
  pack_data_join(int on_boundary,
                 enum fastuidraw::StrokedPoint::offset_type_t pt,
                 uint32_t depth)
  {
    return pack_data(on_boundary, pt, depth) | fastuidraw::StrokedPoint::join_mask;
  }

  class PerJoinData
  {
  public:
    PerJoinData(const fastuidraw::vec2 &p,
                float distance_from_previous_join,
                const fastuidraw::vec2 &tangent_into_join,
                const fastuidraw::vec2 &tangent_leaving_join);

    const fastuidraw::vec2&
    n0(void) const
    {
      return m_normal_into_join;
    }

    const fastuidraw::vec2&
    n1(void) const
    {
      return m_normal_leaving_join;
    }

    void
    set_distance_values(fastuidraw::StrokedPoint *pt) const
    {
      pt->m_distance_from_edge_start = m_distance_from_previous_join;
      pt->m_edge_length = m_distance_from_previous_join;
      pt->m_open_contour_length = m_open_contour_length;
      pt->m_closed_contour_length = m_closed_contour_length;
      pt->m_distance_from_contour_start = m_distance_from_contour_start;
    }

    void
    set_distance_values(fastuidraw::ArcStrokedPoint *pt) const
    {
      pt->m_distance_from_edge_start = m_distance_from_previous_join;
      pt->m_edge_length = m_distance_from_previous_join;
      pt->m_open_contour_length = m_open_contour_length;
      pt->m_closed_contour_length = m_closed_contour_length;
      pt->m_distance_from_contour_start = m_distance_from_contour_start;
    }

    /* given values from creation */
    bool m_of_closing_edge;
    fastuidraw::vec2 m_p;
    fastuidraw::vec2 m_tangent_into_join, m_tangent_leaving_join;
    float m_distance_from_previous_join;
    float m_distance_from_contour_start;
    float m_open_contour_length;
    float m_closed_contour_length;

    /* derived values from creation */
    fastuidraw::vec2 m_normal_into_join, m_normal_leaving_join;
    float m_det, m_lambda;
  };

  class PerCapData
  {
  public:
    fastuidraw::vec2 m_tangent_into_cap;
    fastuidraw::vec2 m_p;
    float m_distance_from_edge_start;
    float m_distance_from_contour_start;
    float m_open_contour_length;
    float m_closed_contour_length;
    bool m_is_starting_cap;

    void
    set_distance_values(fastuidraw::StrokedPoint *pt) const
    {
      pt->m_distance_from_edge_start = m_distance_from_edge_start;
      pt->m_edge_length = m_distance_from_edge_start;
      pt->m_open_contour_length = m_open_contour_length;
      pt->m_closed_contour_length = m_closed_contour_length;
      pt->m_distance_from_contour_start = m_distance_from_contour_start;
    }

    void
    set_distance_values(fastuidraw::ArcStrokedPoint *pt) const
    {
      pt->m_distance_from_edge_start = m_distance_from_edge_start;
      pt->m_edge_length = m_distance_from_edge_start;
      pt->m_open_contour_length = m_open_contour_length;
      pt->m_closed_contour_length = m_closed_contour_length;
      pt->m_distance_from_contour_start = m_distance_from_contour_start;
    }
  };

  class PerContourData
  {
  public:
    PerContourData(void);

    void
    start(const fastuidraw::vec2 &p,
          const fastuidraw::vec2 &tangent_along_curve);

    void
    end(float distance_from_previous_join,
        const fastuidraw::vec2 &direction_into_join);

    void
    add_join(const fastuidraw::vec2 &p,
             float distance_from_previous_join,
             const fastuidraw::vec2 &tangent_into_join,
             const fastuidraw::vec2 &tangent_leaving_join);

    const PerCapData&
    cap(unsigned int I) const
    {
      FASTUIDRAWassert(m_cap_count == 2);
      return m_caps[I];
    }

    fastuidraw::c_array<const PerJoinData>
    joins_of_non_closing_edges(void) const
    {
      FASTUIDRAWassert(m_cap_count == 2);
      return m_joins_of_non_closing_edges;
    }

    fastuidraw::c_array<const PerJoinData>
    joins_of_closing_edge(void) const
    {
      FASTUIDRAWassert(m_cap_count == 2);
      return m_joins_of_closing_edge;
    }

  private:
    std::vector<PerJoinData> m_joins;
    fastuidraw::c_array<const PerJoinData> m_joins_of_non_closing_edges;
    fastuidraw::c_array<const PerJoinData> m_joins_of_closing_edge;
    fastuidraw::vecN<PerCapData, 2> m_caps;
    unsigned int m_cap_count;
  };

  class ContourData:fastuidraw::noncopyable
  {
  public:
    fastuidraw::BoundingBox<float> m_bounding_box;
    std::vector<PerContourData> m_per_contour_data;
  };

  template<typename T>
  class OrderingEntry:public T
  {
  public:
    OrderingEntry(const T &src, unsigned int chunk):
      T(src),
      m_chunk(chunk),
      m_depth(0)
    {}

    unsigned int m_chunk;
    unsigned int m_depth;
  };

  /* provides in what chunk each Join is located. */
  class JoinOrdering
  {
  public:
    fastuidraw::c_array<const OrderingEntry<PerJoinData> >
    non_closing_edge(void) const
    {
      return fastuidraw::make_c_array(m_non_closing_edge);
    }

    fastuidraw::c_array<const OrderingEntry<PerJoinData> >
    closing_edge(void) const
    {
      return fastuidraw::make_c_array(m_closing_edge);
    }

    void
    add_join(const PerJoinData &src, unsigned int &chunk);

    void
    post_process_non_closing(fastuidraw::range_type<unsigned int> join_range,
                             unsigned int &depth);

    void
    post_process_closing(unsigned int non_closing_edge_chunk_count,
                         fastuidraw::range_type<unsigned int> join_range,
                         unsigned int &depth);

    unsigned int
    number_joins(bool include_joins_of_closing_edge) const
    {
      return include_joins_of_closing_edge ?
        m_non_closing_edge.size() + m_closing_edge.size() :
        m_non_closing_edge.size();
    }

    unsigned int
    join_chunk(unsigned int J) const;

  private:
    std::vector<OrderingEntry<PerJoinData> > m_non_closing_edge;
    std::vector<OrderingEntry<PerJoinData> > m_closing_edge;
  };

  class CapOrdering
  {
  public:
    fastuidraw::c_array<const OrderingEntry<PerCapData> >
    caps(void) const
    {
      return fastuidraw::make_c_array(m_caps);
    }

    void
    add_cap(const PerCapData &src, unsigned int &chunk)
    {
      OrderingEntry<PerCapData> C(src, chunk);
      m_caps.push_back(C);
      ++chunk;
    }

    void
    post_process(fastuidraw::range_type<unsigned int> range,
                 unsigned int &depth);

  private:
    std::vector<OrderingEntry<PerCapData> > m_caps;
  };

  class PathData:fastuidraw::noncopyable
  {
  public:
    fastuidraw::BoundingBox<float> m_bounding_box;

    JoinOrdering m_join_ordering;
    unsigned int m_number_join_chunks;
    CapOrdering m_cap_ordering;
    unsigned int m_number_cap_chunks;
  };

  /* A CullingHierarchy represents a hierarchy choice of
   * what joins and caps in each element of a hierarchy.
   */
  class CullingHierarchy:fastuidraw::noncopyable
  {
  public:
    static
    CullingHierarchy*
    create(const ContourData &contour_data);

    ~CullingHierarchy();

    bool
    has_children(void) const
    {
      bool b0(m_children[0] != nullptr);
      bool b1(m_children[1] != nullptr);

      FASTUIDRAWassert(b0 == b1);
      FASTUIDRAWunused(b0);
      FASTUIDRAWunused(b1);
      return m_children[0] != nullptr;
    }

    const CullingHierarchy*
    child(unsigned int i) const
    {
      return m_children[i];
    }

    fastuidraw::c_array<const PerJoinData>
    non_closing_joins(void) const
    {
      return m_joins_non_closing_edge;
    }

    fastuidraw::c_array<const PerJoinData>
    closing_joins(void) const
    {
      return m_joins_closing_edge;
    }

    fastuidraw::c_array<const PerCapData>
    caps(void) const
    {
      return fastuidraw::make_c_array(m_caps);
    }

    const fastuidraw::BoundingBox<float>&
    bounding_box(void) const
    {
      return m_bb;
    }

  private:
    enum
      {
        splitting_threshhold = 10
      };

    CullingHierarchy(const fastuidraw::BoundingBox<float> &bb,
                     std::vector<PerJoinData> &joins, std::vector<PerCapData> &caps);

    /* a value of -1 means to NOT split.
     */
    static
    int
    choose_splitting_coordinate(const fastuidraw::BoundingBox<float> &start_box,
                                fastuidraw::c_array<const PerJoinData> joins,
                                fastuidraw::c_array<const PerCapData> caps,
                                float &split_value);

    /* children, if any  */
    fastuidraw::vecN<CullingHierarchy*, 2> m_children;

    /* Caps inside, only non-empty if has no children. */
    std::vector<PerCapData> m_caps;

    /* Joins inside, only non-empty if has no children. */
    std::vector<PerJoinData> m_joins;
    fastuidraw::c_array<const PerJoinData> m_joins_non_closing_edge;
    fastuidraw::c_array<const PerJoinData> m_joins_closing_edge;

    /* bounding box of this
     */
    fastuidraw::BoundingBox<float> m_bb;
  };

  class ScratchSpacePrivate:fastuidraw::noncopyable
  {
  public:
    std::vector<fastuidraw::vec3> m_adjusted_clip_eqs;
    std::vector<fastuidraw::vec2> m_clipped_rect;

    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_clip_scratch_vec2s;
  };

  class RangeAndChunk
  {
  public:
    /* what range of Joins/Caps hit by the chunk.
     */
    fastuidraw::range_type<unsigned int> m_elements;

    /* depth range of Joins/Caps hit
     */
    fastuidraw::range_type<unsigned int> m_depth_range;

    /* what the chunk is
     */
    unsigned int m_chunk;

    bool
    non_empty(void) const
    {
      return m_depth_range.m_end > m_depth_range.m_begin;
    }
  };

  class ChunkSetPrivate:fastuidraw::noncopyable
  {
  public:
    void
    reset(void)
    {
      m_ignore_join_adds = false;
      m_join_chunks.clear();
      m_join_ranges.clear();
      m_cap_chunks.clear();
    }

    void
    add_join_chunk(const RangeAndChunk &j);

    void
    add_cap_chunk(const RangeAndChunk &c);

    fastuidraw::c_array<const unsigned int>
    join_chunks(void) const
    {
      return fastuidraw::make_c_array(m_join_chunks);
    }

    fastuidraw::c_array<const unsigned int>
    cap_chunks(void) const
    {
      return fastuidraw::make_c_array(m_cap_chunks);
    }

    void
    ignore_join_adds(void)
    {
      m_ignore_join_adds = true;
    }

  private:
    std::vector<unsigned int> m_join_chunks, m_cap_chunks;
    std::vector<fastuidraw::range_type<unsigned int> > m_join_ranges;
    bool m_ignore_join_adds;
  };

  /* Subset of a StrokedCapsJoins. Edges are to be placed into
   * the store as follows:
   *   1. child0
   *   2. child1
   *   3. caps/joins (i.e. from CullingHierarchy)
   */
  class SubsetPrivate
  {
  public:

    class CreationValues
    {
    public:
      CreationValues(void):
        m_non_closing_join_chunk_cnt(0),
        m_closing_join_chunk_cnt(0),
        m_cap_chunk_cnt(0)
      {}

      unsigned int m_non_closing_join_chunk_cnt;
      unsigned int m_closing_join_chunk_cnt;

      unsigned int m_cap_chunk_cnt;
    };

    static
    SubsetPrivate*
    create(const CullingHierarchy *src,
           CreationValues &out_values,
           JoinOrdering &join_ordering,
           CapOrdering &cap_ordering);

    ~SubsetPrivate();

    void
    compute_chunks(bool include_closing_edge,
                   ScratchSpacePrivate &work_room,
                   fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
                   const fastuidraw::float3x3 &clip_matrix_local,
                   const fastuidraw::vec2 &recip_dimensions,
                   float pixels_additional_room,
                   float item_space_additional_room,
                   unsigned int max_attribute_cnt,
                   unsigned int max_index_cnt,
                   bool take_joins_outside_of_region,
                   ChunkSetPrivate &dst);

    bool
    have_children(void) const
    {
      bool b0(m_children[0] != nullptr);
      bool b1(m_children[1] != nullptr);

      FASTUIDRAWassert(b0 == b1);
      FASTUIDRAWunused(b0);
      FASTUIDRAWunused(b1);
      return m_children[0] != nullptr;
    }

    const SubsetPrivate*
    child(unsigned int I) const
    {
      FASTUIDRAWassert(have_children());
      FASTUIDRAWassert(I == 0 || I == 1);
      return m_children[I];
    }

    const RangeAndChunk&
    non_closing_joins(void) const
    {
      return m_non_closing_joins;
    }

    const RangeAndChunk&
    closing_joins(void) const
    {
      return m_closing_joins;
    }

    const RangeAndChunk&
    caps(void) const
    {
      return m_caps;
    }

  private:
    class PostProcessVariables
    {
    public:
      PostProcessVariables(void):
        m_join_depth(0),
        m_closing_join_depth(0),
        m_cap_depth(0)
      {}

      unsigned int m_join_depth, m_closing_join_depth;
      unsigned int m_cap_depth;
    };

    SubsetPrivate(CreationValues &out_values,
                  JoinOrdering &join_ordering,
                  CapOrdering &cap_ordering,
                  const CullingHierarchy *src);

    void
    compute_chunks_implement(bool include_closing_edge,
                             ScratchSpacePrivate &work_room,
                             float item_space_additional_room,
                             unsigned int max_attribute_cnt,
                             unsigned int max_index_cnt,
                             ChunkSetPrivate &dst);

    void
    compute_chunks_take_all(bool include_closing_edge,
                            unsigned int max_attribute_cnt,
                            unsigned int max_index_cnt,
                            ChunkSetPrivate &dst);
    void
    post_process(PostProcessVariables &variables,
                 const CreationValues &constants,
                 JoinOrdering &join_ordering,
                 CapOrdering &cap_ordering);

    fastuidraw::vecN<SubsetPrivate*, 2> m_children;

    /* book keeping for joins */
    RangeAndChunk m_non_closing_joins, m_closing_joins;

    /* book keeping for caps */
    RangeAndChunk m_caps;

    fastuidraw::BoundingBox<float> m_bb;

    bool m_empty_subset;
  };

  class JoinCount
  {
  public:
    explicit
    JoinCount(const JoinOrdering &J):
      m_number_close_joins(J.closing_edge().size()),
      m_number_non_close_joins(J.non_closing_edge().size())
    {}

    unsigned int m_number_close_joins;
    unsigned int m_number_non_close_joins;
  };

  class JoinCreatorBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    JoinCreatorBase(const PathData &P, const SubsetPrivate *st);

    virtual
    ~JoinCreatorBase() {}

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_ranges) const;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;

  protected:

    void
    post_ctor_initalize(void);

  private:

    void
    fill_join(unsigned int join_id, const PerJoinData &join_data,
              unsigned int chunk, unsigned int depth,
              fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
              fastuidraw::c_array<unsigned int> indices,
              unsigned int &vertex_offset, unsigned int &index_offset,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts,
              fastuidraw::c_array<fastuidraw::range_type<int> > join_vertex_ranges,
              fastuidraw::c_array<fastuidraw::range_type<int> > join_index_ranges) const;

    static
    void
    set_chunks(const SubsetPrivate *st,
               fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
               fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
               fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
               fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
               fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
               fastuidraw::c_array<int> index_adjusts);

    static
    void
    process_chunk(const RangeAndChunk &ch,
                  fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
                  fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
                  fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
                  fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
                  fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
                  fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                  fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
                  fastuidraw::c_array<int> index_adjusts);

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const = 0;

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const = 0;

    const JoinOrdering &m_ordering;
    const SubsetPrivate *m_st;
    unsigned int m_num_verts, m_num_indices, m_num_chunks, m_num_joins;
    bool m_post_ctor_initalized_called;
  };

  class ArcRoundedJoinCreator:public JoinCreatorBase
  {
  public:
    ArcRoundedJoinCreator(const PathData &P, const SubsetPrivate *st);

  private:
    class PerArcRoundedJoin:public PerJoinData
    {
    public:
      explicit
      PerArcRoundedJoin(const PerJoinData &J);

      void
      add_data(unsigned int depth,
               fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
               unsigned int &vertex_offset,
               fastuidraw::c_array<unsigned int> indices,
               unsigned int &index_offset) const;

      /* how many arc-joins are used to realize the join */
      unsigned int m_count;

      /* number of vertices and indices needed */
      unsigned int m_vertex_count, m_index_count;

      float m_delta_angle;
      std::complex<float> m_arc_start;
    };

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;

    mutable std::vector<PerArcRoundedJoin> m_per_join_data;
  };

  class RoundedJoinCreator:public JoinCreatorBase
  {
  public:
    RoundedJoinCreator(const PathData &P,
                       const SubsetPrivate *st,
                       float thresh);

  private:

    class PerRoundedJoin:public PerJoinData
    {
    public:
      PerRoundedJoin(const PerJoinData &J, float thresh);

      void
      add_data(unsigned int depth,
               fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
               unsigned int &vertex_offset,
               fastuidraw::c_array<unsigned int> indices,
               unsigned int &index_offset) const;

      std::complex<float> m_arc_start;
      float m_delta_theta;
      unsigned int m_num_arc_points;
    };

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;

    float m_thresh;
    mutable std::vector<PerRoundedJoin> m_per_join_data;
  };

  class BevelJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    BevelJoinCreator(const PathData &P, const SubsetPrivate *st);

  private:

    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;
  };

  class MiterClipJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    MiterClipJoinCreator(const PathData &P, const SubsetPrivate *st);

  private:
    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;
  };

  template<enum fastuidraw::StrokedPoint::offset_type_t tp>
  class MiterJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    MiterJoinCreator(const PathData &P, const SubsetPrivate *st);

  private:
    virtual
    void
    add_join(unsigned int join_id, const PerJoinData &join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id, const PerJoinData &join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;
  };

  class PointIndexCapSize
  {
  public:
    PointIndexCapSize(void):
      m_verts(0),
      m_indices(0)
    {}

    unsigned int m_verts, m_indices;
  };

  class CapCreatorBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    CapCreatorBase(const PathData &P,
                   const SubsetPrivate *st,
                   PointIndexCapSize sz);

    virtual
    ~CapCreatorBase()
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
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;

  private:
    virtual
    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const = 0;

    static
    void
    set_chunks(const SubsetPrivate *st,
               fastuidraw::c_array<const fastuidraw::range_type<int> > cap_vertex_ranges,
               fastuidraw::c_array<const fastuidraw::range_type<int> > cap_index_ranges,
               fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
               fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
               fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
               fastuidraw::c_array<int> index_adjusts);

    const CapOrdering &m_ordering;
    const SubsetPrivate *m_st;

    unsigned int m_num_chunks;
    PointIndexCapSize m_size;
  };

  class RoundedCapCreator:public CapCreatorBase
  {
  public:
    RoundedCapCreator(const PathData &P,
                      const SubsetPrivate *st,
                      float thresh);

  private:
    static
    PointIndexCapSize
    compute_size(const PathData &P, float thresh);

    virtual
    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;

    float m_delta_theta;
    unsigned int m_num_arc_points_per_cap;
  };

  class SquareCapCreator:public CapCreatorBase
  {
  public:
    explicit
    SquareCapCreator(const PathData &P,
                     const SubsetPrivate *st):
      CapCreatorBase(P, st, compute_size(P))
    {}

  private:
    static
    PointIndexCapSize
    compute_size(const PathData &P);

    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;
  };

  class AdjustableCapCreator:public CapCreatorBase
  {
  public:
    AdjustableCapCreator(const PathData &P,
                         const SubsetPrivate *st):
      CapCreatorBase(P, st, compute_size(P))
    {}

  private:
    enum
      {
        number_points_per_fan = 6,
        number_triangles_per_fan = number_points_per_fan - 2,
        number_indices_per_fan = 3 * number_triangles_per_fan,
      };

    static
    PointIndexCapSize
    compute_size(const PathData &P);

    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;
  };

  class ArcRoundedCapCreator:public CapCreatorBase
  {
  public:
    ArcRoundedCapCreator(const PathData &P,
                         const SubsetPrivate *st);

  private:
    enum
      {
        /* a cap is 180 degrees, make each arc-join as 45 degrees */
        num_arcs_per_cap = 4
      };

    static
    PointIndexCapSize
    compute_size(const PathData &P);

    virtual
    void
    add_cap(const PerCapData &C, unsigned int depth,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;
  };

  class ThreshWithData
  {
  public:
    ThreshWithData(void):
      m_data(nullptr),
      m_thresh(-1)
    {}

    ThreshWithData(fastuidraw::PainterAttributeData *d, float t):
      m_data(d), m_thresh(t)
    {}

    static
    bool
    reverse_compare_against_thresh(const ThreshWithData &lhs, float rhs)
    {
      return lhs.m_thresh > rhs;
    }

    fastuidraw::PainterAttributeData *m_data;
    float m_thresh;
  };

  template<typename T>
  class PreparedAttributeData
  {
  public:
    PreparedAttributeData(void):
      m_ready(false)
    {}

    /* must be called before the first call to data().
     */
    void
    mark_as_empty(void)
    {
      m_ready = true;
    }

    const fastuidraw::PainterAttributeData&
    data(const PathData &P, const SubsetPrivate *st)
    {
      if (!m_ready)
        {
          m_data.set_data(T(P, st));
          m_ready = true;
        }
      return m_data;
    }

  private:
    fastuidraw::PainterAttributeData m_data;
    bool m_ready;
  };

  class StrokedCapsJoinsPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    StrokedCapsJoinsPrivate(const ContourData &P);
    ~StrokedCapsJoinsPrivate();

    void
    create_joins_caps(const ContourData &P);

    template<typename T>
    const fastuidraw::PainterAttributeData&
    fetch_create(float thresh,
                 std::vector<ThreshWithData> &values);

    SubsetPrivate* m_subset;

    PreparedAttributeData<BevelJoinCreator> m_bevel_joins;
    PreparedAttributeData<MiterClipJoinCreator> m_miter_clip_joins;
    PreparedAttributeData<MiterJoinCreator<fastuidraw::StrokedPoint::offset_miter_join> > m_miter_joins;
    PreparedAttributeData<MiterJoinCreator<fastuidraw::StrokedPoint::offset_miter_bevel_join> > m_miter_bevel_joins;
    PreparedAttributeData<ArcRoundedJoinCreator> m_arc_rounded_joins;
    PreparedAttributeData<SquareCapCreator> m_square_caps;
    PreparedAttributeData<AdjustableCapCreator> m_adjustable_caps;
    PreparedAttributeData<ArcRoundedCapCreator> m_arc_rounded_caps;

    PathData m_path_data;
    fastuidraw::vecN<unsigned int, 2> m_chunk_of_joins;
    unsigned int m_chunk_of_caps;

    std::vector<ThreshWithData> m_rounded_joins;
    std::vector<ThreshWithData> m_rounded_caps;

    bool m_empty_path;
    fastuidraw::PainterAttributeData m_empty_data;
  };

}

/////////////////////////////////
// PerContourData methods
PerContourData::
PerContourData(void):
  m_cap_count(0)
{}

void
PerContourData::
start(const fastuidraw::vec2 &p,
      const fastuidraw::vec2 &tangent_along_curve)
{
  FASTUIDRAWassert(m_cap_count == 0);
  m_caps[0].m_p = p;
  m_caps[0].m_tangent_into_cap = -tangent_along_curve;
  m_caps[0].m_distance_from_edge_start = 0.0f;
  m_caps[0].m_distance_from_contour_start = 0.0f;
  m_caps[0].m_is_starting_cap = true;
  m_cap_count = 1;
}

void
PerContourData::
end(float distance_from_previous_join,
    const fastuidraw::vec2 &direction_into_join)
{
  fastuidraw::vec2 end_cap_pt, end_cap_direction;
  float end_cap_edge_distance;

  /* the last added join starts the closing edge */
  if (!m_joins.empty())
    {
      end_cap_pt = m_joins.back().m_p;
      end_cap_direction = m_joins.back().m_tangent_into_join;
      end_cap_edge_distance = m_joins.back().m_distance_from_previous_join;
      m_joins.back().m_of_closing_edge = true;

      /* add the join ending the closing edge */
      add_join(m_caps[0].m_p, distance_from_previous_join,
               direction_into_join,
               -m_caps[0].m_tangent_into_cap);
      m_joins.back().m_of_closing_edge = true;
    }
  else
    {
      end_cap_edge_distance = 0;
      end_cap_pt = m_caps[0].m_p;
      end_cap_direction = m_caps[0].m_tangent_into_cap;
    }

  /* compute the contour lengths */
  float d(0.0f);
  for(unsigned int i = 0, endi = m_joins.size(); i < endi; ++i)
    {
      d += m_joins[i].m_distance_from_previous_join;
      m_joins[i].m_distance_from_contour_start = d;
    }

  for (PerJoinData &J : m_joins)
    {
      J.m_closed_contour_length = d;
      J.m_open_contour_length = d - distance_from_previous_join;
    }

  for (PerCapData &C : m_caps)
    {
      C.m_closed_contour_length = m_joins.empty() ? 0.0f : d;
      C.m_open_contour_length = m_joins.empty() ? distance_from_previous_join : d - distance_from_previous_join;
    }

  m_caps[1].m_p = end_cap_pt;
  m_caps[1].m_tangent_into_cap = end_cap_direction;
  m_caps[1].m_distance_from_edge_start = end_cap_edge_distance;
  m_caps[1].m_distance_from_contour_start = m_caps[1].m_open_contour_length;
  m_caps[1].m_is_starting_cap = false;

  m_cap_count = 2;

  fastuidraw::c_array<const PerJoinData> all_joins;
  all_joins = fastuidraw::make_c_array(m_joins);
  if (all_joins.size() > 2)
    {
      m_joins_of_non_closing_edges = all_joins.sub_array(0, all_joins.size() - 2);
      m_joins_of_closing_edge = all_joins.sub_array(all_joins.size() - 2, 2);
    }
  else
    {
      m_joins_of_closing_edge = all_joins;
    }
}

void
PerContourData::
add_join(const fastuidraw::vec2 &p,
         float distance_from_previous_join,
         const fastuidraw::vec2 &tangent_into_join,
         const fastuidraw::vec2 &tangent_leaving_join)
{
  FASTUIDRAWassert(m_cap_count == 1);
  m_joins.push_back(PerJoinData(p, distance_from_previous_join,
                                tangent_into_join, tangent_leaving_join));
}

//////////////////////////////////
// JoinOrdering methods
void
JoinOrdering::
add_join(const PerJoinData &src, unsigned int &chunk)
{
  OrderingEntry<PerJoinData> J(src, chunk);
  if (src.m_of_closing_edge)
    {
      m_closing_edge.push_back(J);
    }
  else
    {
      m_non_closing_edge.push_back(J);
    }
  ++chunk;
}

void
JoinOrdering::
post_process_non_closing(fastuidraw::range_type<unsigned int> join_range,
                         unsigned int &depth)
{
  unsigned int R, d;

  /* the depth values of the joins must come in reverse
   * so that higher depth values occlude later elements
   */
  R = join_range.difference();
  d = depth + R - 1;

  for(unsigned int i = join_range.m_begin; i < join_range.m_end; ++i, --d)
    {
      FASTUIDRAWassert(i < m_non_closing_edge.size());
      m_non_closing_edge[i].m_depth = d;
    }
  depth += R;
}

void
JoinOrdering::
post_process_closing(unsigned int non_closing_edge_chunk_count,
                     fastuidraw::range_type<unsigned int> join_range,
                     unsigned int &depth)
{
  unsigned int R, d;

  /* the depth values of the joins must come in reverse
   * so that higher depth values occlude later elements
   */
  R = join_range.difference();
  d = depth + R - 1;

  for(unsigned int i = join_range.m_begin; i < join_range.m_end; ++i, --d)
    {
      FASTUIDRAWassert(i < m_closing_edge.size());
      m_closing_edge[i].m_depth = d;
      m_closing_edge[i].m_chunk += non_closing_edge_chunk_count;
    }
  depth += R;
}

unsigned int
JoinOrdering::
join_chunk(unsigned int J) const
{
  FASTUIDRAWassert(J < number_joins(true));
  if (J >= m_non_closing_edge.size())
    {
      J -= m_non_closing_edge.size();
      return m_closing_edge[J].m_chunk;
    }
  else
    {
      return m_non_closing_edge[J].m_chunk;
    }
}

///////////////////////////
// CapOrdering methods
void
CapOrdering::
post_process(fastuidraw::range_type<unsigned int> range,
             unsigned int &depth)
{
  unsigned int R, d;

  /* the depth values of the caps must come in reverse
   * so that higher depth values occlude later elements
   */
  R = range.difference();
  d = depth + R - 1;
  for(unsigned int i = range.m_begin; i < range.m_end; ++i, --d)
    {
      FASTUIDRAWassert(i < m_caps.size());
      m_caps[i].m_depth = d;
    }
  depth += R;
}

/////////////////////////////////
// CullingHierarchy methods
CullingHierarchy*
CullingHierarchy::
create(const ContourData &path_data)
{
  std::vector<PerJoinData> joins;
  std::vector<PerCapData> caps;
  CullingHierarchy *return_value;

  /* fill joins and caps from the data in path_data */
  for(const PerContourData &C : path_data.m_per_contour_data)
    {
      caps.push_back(C.cap(0));
      caps.push_back(C.cap(1));
      /* add all but the joins of the closing edge of C, i.e. all but
       * the last two joins
       */
      for(const PerJoinData &J : C.joins_of_non_closing_edges())
        {
          joins.push_back(J);
        }
    }

  /* now add the joins from the closing edges */
  for(const PerContourData &C : path_data.m_per_contour_data)
    {
      /* add all but the joins of the closing edge of C, i.e. all but
       * the last two joins
       */
      for(const PerJoinData &J : C.joins_of_closing_edge())
        {
          joins.push_back(J);
        }
    }

  return_value = FASTUIDRAWnew CullingHierarchy(path_data.m_bounding_box, joins, caps);
  return return_value;
}

CullingHierarchy::
CullingHierarchy(const fastuidraw::BoundingBox<float> &start_box,
                 std::vector<PerJoinData> &joins, std::vector<PerCapData> &caps):
  m_children(nullptr, nullptr),
  m_bb(start_box)
{
  int c;
  float mid_point;

  FASTUIDRAWassert(!start_box.empty());
  c = choose_splitting_coordinate(start_box,
                                  fastuidraw::make_c_array(joins),
                                  fastuidraw::make_c_array(caps),
                                  mid_point);
  if (c != -1)
    {
      fastuidraw::vecN<fastuidraw::BoundingBox<float>, 2> child_boxes;
      fastuidraw::vecN<std::vector<PerJoinData>, 2> child_joins;
      fastuidraw::vecN<std::vector<PerCapData>, 2> child_caps;

      /* NOTE: since the joins come in the order of non-closing first
       * and we process them in that order, the non-closing joins
       * for child_joins[] will also come first
       */
      for(const PerJoinData &J : joins)
        {
          bool s;
          s = J.m_p[c] < mid_point;
          child_joins[s].push_back(J);
          child_boxes[s].union_point(J.m_p);
        }

      for(const PerCapData &C : caps)
        {
          bool s;
          s = C.m_p[c] < mid_point;
          child_caps[s].push_back(C);
          child_boxes[s].union_point(C.m_p);
        }
      FASTUIDRAWassert(child_joins[0].size() + child_joins[1].size() == joins.size());
      FASTUIDRAWassert(child_caps[0].size() + child_caps[1].size() == caps.size());

      if (child_joins[0].size() + child_caps[0].size() < joins.size() + caps.size())
        {
          m_children[0] = FASTUIDRAWnew CullingHierarchy(child_boxes[0], child_joins[0], child_caps[0]);
          m_children[1] = FASTUIDRAWnew CullingHierarchy(child_boxes[1], child_joins[1], child_caps[1]);
        }
    }

  if (m_children[0] == nullptr)
    {
      FASTUIDRAWassert(m_children[1] == nullptr);

      /* steal the data */
      m_caps.swap(caps);
      m_joins.swap(joins);

      fastuidraw::c_array<const PerJoinData> all_joins;
      all_joins = fastuidraw::make_c_array(m_joins);

      unsigned int non_closing_cnt;
      for(non_closing_cnt = 0;
          non_closing_cnt < m_joins.size() && !m_joins[non_closing_cnt].m_of_closing_edge;
          ++non_closing_cnt)
        {}
      m_joins_non_closing_edge = all_joins.sub_array(0, non_closing_cnt);
      m_joins_closing_edge = all_joins.sub_array(non_closing_cnt);
    }
}

CullingHierarchy::
~CullingHierarchy(void)
{
  if (m_children[0] != nullptr)
    {
      FASTUIDRAWdelete(m_children[0]);
    }

  if (m_children[1] != nullptr)
    {
      FASTUIDRAWdelete(m_children[1]);
    }
}

int
CullingHierarchy::
choose_splitting_coordinate(const fastuidraw::BoundingBox<float> &start_box,
                            fastuidraw::c_array<const PerJoinData> joins,
                            fastuidraw::c_array<const PerCapData> caps,
                            float &split_value)
{
  if (joins.size() + caps.size() < splitting_threshhold)
    {
      return -1;
    }

  std::vector<float> qs;
  fastuidraw::vec2 sz;
  int c;

  /* the dimension we choose is whever is larger */
  sz = start_box.max_point() - start_box.min_point();
  c = (sz.x() > sz.y()) ? 0 : 1;

  qs.reserve(joins.size() + caps.size());
  for(const PerJoinData &d : joins)
    {
      qs.push_back(d.m_p[c]);
    }
  for(const PerCapData & d : caps)
    {
      qs.push_back(d.m_p[c]);
    }
  std::sort(qs.begin(), qs.end());

  split_value = (qs[qs.size() / 2] + qs[qs.size() / 2 + 1]) * 0.5f;
  return c;
}

//////////////////////////////////////////
// SubsetPrivate methods
SubsetPrivate::
~SubsetPrivate()
{
  if (m_children[0] != nullptr)
    {
      FASTUIDRAWdelete(m_children[0]);
    }
  if (m_children[1] != nullptr)
    {
      FASTUIDRAWdelete(m_children[1]);
    }
}

SubsetPrivate*
SubsetPrivate::
create(const CullingHierarchy *src,
       CreationValues &out_values,
       JoinOrdering &join_ordering,
       CapOrdering &cap_ordering)
{
  SubsetPrivate *return_value;
  PostProcessVariables vars;

  return_value = FASTUIDRAWnew SubsetPrivate(out_values, join_ordering, cap_ordering, src);
  return_value->post_process(vars, out_values, join_ordering, cap_ordering);
  return return_value;
}

void
SubsetPrivate::
post_process(PostProcessVariables &variables,
             const CreationValues &constants,
             JoinOrdering &join_ordering,
             CapOrdering &cap_ordering)
{
  /* We want the depth to go in the reverse order as the
   * draw order. The Draw order is child(0), child(1)
   * Thus, we first handle depth child(1) and then child(0).
   */
  m_non_closing_joins.m_depth_range.m_begin = variables.m_join_depth;
  m_closing_joins.m_depth_range.m_begin = variables.m_closing_join_depth;

  m_caps.m_depth_range.m_begin = variables.m_cap_depth;

  if (have_children())
    {
      FASTUIDRAWassert(m_children[0] != nullptr);
      FASTUIDRAWassert(m_children[1] != nullptr);

      m_children[1]->post_process(variables, constants, join_ordering, cap_ordering);
      m_children[0]->post_process(variables, constants, join_ordering, cap_ordering);
    }
  else
    {
      FASTUIDRAWassert(m_children[0] == nullptr);
      FASTUIDRAWassert(m_children[1] == nullptr);

      join_ordering.post_process_non_closing(m_non_closing_joins.m_elements,
                                             variables.m_join_depth);

      join_ordering.post_process_closing(constants.m_non_closing_join_chunk_cnt,
                                         m_closing_joins.m_elements,
                                         variables.m_closing_join_depth);

      cap_ordering.post_process(m_caps.m_elements, variables.m_cap_depth);
    }

  m_non_closing_joins.m_depth_range.m_end = variables.m_join_depth;
  m_closing_joins.m_depth_range.m_end = variables.m_closing_join_depth;

  m_caps.m_depth_range.m_end = variables.m_cap_depth;

  FASTUIDRAWassert(m_non_closing_joins.m_elements.difference() == m_non_closing_joins.m_depth_range.difference());
  FASTUIDRAWassert(m_closing_joins.m_elements.difference() == m_closing_joins.m_depth_range.difference());

  /* the joins are ordered so that the joins of the non-closing
   * edges appear first.
   */
  m_closing_joins.m_elements += join_ordering.non_closing_edge().size();

  /* make the chunks of closing edges come AFTER
   * chunks of non-closing edge
   */
  m_closing_joins.m_chunk += constants.m_non_closing_join_chunk_cnt;

  m_empty_subset = !m_non_closing_joins.non_empty()
    && !m_closing_joins.non_empty()
    && !m_caps.non_empty();
}

SubsetPrivate::
SubsetPrivate(CreationValues &out_values,
              JoinOrdering &join_ordering, CapOrdering &cap_ordering,
              const CullingHierarchy *src):
  m_children(nullptr, nullptr),
  m_bb(src->bounding_box())
{
  /* Draw order is:
   *   child(0)
   *   child(1)
   */
  m_non_closing_joins.m_elements.m_begin = join_ordering.non_closing_edge().size();
  m_closing_joins.m_elements.m_begin = join_ordering.closing_edge().size();
  m_caps.m_elements.m_begin = cap_ordering.caps().size();

  if (src->has_children())
    {
      FASTUIDRAWassert(src->caps().empty());
      FASTUIDRAWassert(src->non_closing_joins().empty());
      FASTUIDRAWassert(src->closing_joins().empty());
      for(unsigned int i = 0; i < 2; ++i)
        {
          FASTUIDRAWassert(src->child(i) != nullptr);
          m_children[i] = FASTUIDRAWnew SubsetPrivate(out_values, join_ordering, cap_ordering, src->child(i));
        }
    }
  else
    {
      for(const PerJoinData &J : src->non_closing_joins())
        {
          join_ordering.add_join(J, out_values.m_non_closing_join_chunk_cnt);
        }

      for(const PerJoinData &J : src->closing_joins())
        {
          join_ordering.add_join(J, out_values.m_closing_join_chunk_cnt);
        }

      for(const PerCapData &C : src->caps())
        {
          cap_ordering.add_cap(C, out_values.m_cap_chunk_cnt);
        }
    }

  m_non_closing_joins.m_elements.m_end = join_ordering.non_closing_edge().size();
  m_closing_joins.m_elements.m_end = join_ordering.closing_edge().size();
  m_caps.m_elements.m_end = cap_ordering.caps().size();

  m_non_closing_joins.m_chunk = out_values.m_non_closing_join_chunk_cnt;
  m_closing_joins.m_chunk = out_values.m_closing_join_chunk_cnt;
  m_caps.m_chunk = out_values.m_cap_chunk_cnt;

  ++out_values.m_non_closing_join_chunk_cnt;
  ++out_values.m_closing_join_chunk_cnt;
  ++out_values.m_cap_chunk_cnt;
}

void
SubsetPrivate::
compute_chunks(bool include_closing_edge,
               ScratchSpacePrivate &scratch,
               fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
               const fastuidraw::float3x3 &clip_matrix_local,
               const fastuidraw::vec2 &recip_dimensions,
               float pixels_additional_room,
               float item_space_additional_room,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               bool take_joins_outside_of_region,
               ChunkSetPrivate &dst)
{
  scratch.m_adjusted_clip_eqs.resize(clip_equations.size());
  for(unsigned int i = 0; i < clip_equations.size(); ++i)
    {
      fastuidraw::vec3 c(clip_equations[i]);
      float f;

      /* make "w" larger by the named number of pixels.
       */
      f = fastuidraw::t_abs(c.x()) * recip_dimensions.x()
        + fastuidraw::t_abs(c.y()) * recip_dimensions.y();

      c.z() += pixels_additional_room * f;

      /* transform clip equations from clip coordinates to
       * local coordinates.
       */
      scratch.m_adjusted_clip_eqs[i] = c * clip_matrix_local;
    }

  dst.reset();
  if (take_joins_outside_of_region)
    {
      dst.add_join_chunk(m_non_closing_joins);
      if (include_closing_edge)
        {
          dst.add_join_chunk(m_closing_joins);
        }
      dst.ignore_join_adds();
    }
  compute_chunks_implement(include_closing_edge,
                           scratch, item_space_additional_room,
                           max_attribute_cnt, max_index_cnt, dst);
}

void
SubsetPrivate::
compute_chunks_take_all(bool include_closing_edge,
                        unsigned int max_attribute_cnt,
                        unsigned int max_index_cnt,
                        ChunkSetPrivate &dst)
{
  if (m_empty_subset)
    {
      return;
    }

  /*
   * TODO: take into account  max_attribute_cnt/max_indent
   * ISSUE: rounded joins/caps have variable attribue/index count,
   *       solve this by giving cap coefficient and join coefficient
   *       and multiplying the number of caps and joins by their
   *       coefficients to get the correct value.
   */
  FASTUIDRAWunused(max_attribute_cnt);
  FASTUIDRAWunused(max_index_cnt);

  dst.add_join_chunk(m_non_closing_joins);
  if (include_closing_edge)
    {
      dst.add_join_chunk(m_closing_joins);
    }
  else
    {
      dst.add_cap_chunk(m_caps);
    }
}

void
SubsetPrivate::
compute_chunks_implement(bool include_closing_edge,
                         ScratchSpacePrivate &scratch,
                         float item_space_additional_room,
                         unsigned int max_attribute_cnt,
                         unsigned int max_index_cnt,
                         ChunkSetPrivate &dst)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;

  if (m_bb.empty() || m_empty_subset)
    {
      return;
    }

  /* clip the bounding box of this SubsetPrivate */
  vecN<vec2, 4> bb;
  bool unclipped;

  m_bb.inflated_polygon(bb, item_space_additional_room);
  unclipped = clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                  bb, scratch.m_clipped_rect,
                                  scratch.m_clip_scratch_vec2s);
  //completely unclipped.
  if (unclipped || !have_children())
    {
      compute_chunks_take_all(include_closing_edge, max_attribute_cnt, max_index_cnt, dst);
      return;
    }

  //completely clipped
  if (scratch.m_clipped_rect.empty())
    {
      return;
    }

  FASTUIDRAWassert(have_children());

  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  m_children[0]->compute_chunks_implement(include_closing_edge,
                                          scratch, item_space_additional_room,
                                          max_attribute_cnt, max_index_cnt,
                                          dst);
  m_children[1]->compute_chunks_implement(include_closing_edge,
                                          scratch, item_space_additional_room,
                                          max_attribute_cnt, max_index_cnt,
                                          dst);
}

////////////////////////////////////////////////
// PerJoinData methods
PerJoinData::
PerJoinData(const fastuidraw::vec2 &p,
            float distance_from_previous_join,
            const fastuidraw::vec2 &tangent_into_join,
            const fastuidraw::vec2 &tangent_leaving_join):
  m_of_closing_edge(false),
  m_p(p),
  m_tangent_into_join(tangent_into_join),
  m_tangent_leaving_join(tangent_leaving_join),
  m_distance_from_previous_join(distance_from_previous_join),
  m_distance_from_contour_start(0.0f),
  m_open_contour_length(0.0f),
  m_closed_contour_length(0.0f),
  m_normal_into_join(-tangent_into_join.y(), tangent_into_join.x()),
  m_normal_leaving_join(-tangent_leaving_join.y(), tangent_leaving_join.x())
{
  /* Explanation:
   *  We have two curves, a(t) and b(t) with a(1) = b(0)
   *  The point p0 represents the end of a(t) and the
   *  point p1 represents the start of b(t).
   *
   *  When stroking we have four auxiliary curves:
   *    a0(t) = a(t) + w * a_n(t)
   *    a1(t) = a(t) - w * a_n(t)
   *    b0(t) = b(t) + w * b_n(t)
   *    b1(t) = b(t) - w * b_n(t)
   *  where
   *    w = width of stroking
   *    a_n(t) = J( a'(t) ) / || a'(t) ||
   *    b_n(t) = J( b'(t) ) / || b'(t) ||
   *  when
   *    J(x, y) = (-y, x).
   *
   *  A Bevel join is a triangle that connects
   *  consists of p, A and B where p is a(1)=b(0),
   *  A is one of a0(1) or a1(1) and B is one
   *  of b0(0) or b1(0). Now if we use a0(1) for
   *  A then we will use b0(0) for B because
   *  the normals are generated the same way for
   *  a(t) and b(t). Then, the questions comes
   *  down to, do we wish to add or subtract the
   *  normal. That value is represented by m_lambda.
   *
   *  Now to figure out m_lambda. Let q0 be a point
   *  on a(t) before p=a(1). The q0 is given by
   *
   *    q0 = p - s * m_v0
   *
   *  and let q1 be a point on b(t) after p=b(0),
   *
   *    q1 = p + t * m_v1
   *
   *  where both s, t are positive. Let
   *
   *    z = (q0+q1) / 2
   *
   *  the point z is then on the side of the join
   *  of the acute angle of the join.
   *
   *  With this in mind, if either of <z-p, m_n0>
   *  or <z-p, m_n1> is positive then we want
   *  to add by -w * n rather than  w * n.
   *
   *  Note that:
   *
   *  <z-p, m_n1> = 0.5 * < -s * m_v0 + t * m_v1, m_n1 >
   *              = -0.5 * s * <m_v0, m_n1> + 0.5 * t * <m_v1, m_n1>
   *              = -0.5 * s * <m_v0, m_n1>
   *              = -0.5 * s * <m_v0, J(m_v1) >
   *
   *  and
   *
   *  <z-p, m_n0> = 0.5 * < -s * m_v0 + t * m_v1, m_n0 >
   *              = -0.5 * s * <m_v0, m_n0> + 0.5 * t * <m_v1, m_n0>
   *              = 0.5 * t * <m_v1, m_n0>
   *              = 0.5 * t * <m_v1, J(m_v0) >
   *              = -0.5 * t * <J(m_v1), m_v0>
   *
   *  (the last line because transpose(J) = -J). Notice
   *  that the sign of <z-p, m_n1> and the sign of <z-p, m_n0>
   *  is then the same.
   *
   *  thus m_lambda is positive if <m_v1, m_n0> is negative.
   */

  m_det = fastuidraw::dot(m_tangent_leaving_join, m_normal_into_join);
  if (m_det > 0.0f)
    {
      m_lambda = -1.0f;
    }
  else
    {
      m_lambda = 1.0f;
    }
}

/////////////////////////////////////////////////
// JoinCreatorBase methods
JoinCreatorBase::
JoinCreatorBase(const PathData &P, const SubsetPrivate *st):
  m_ordering(P.m_join_ordering),
  m_st(st),
  m_num_verts(0),
  m_num_indices(0),
  m_num_chunks(P.m_number_join_chunks),
  m_num_joins(0),
  m_post_ctor_initalized_called(false)
{}

void
JoinCreatorBase::
post_ctor_initalize(void)
{
  FASTUIDRAWassert(!m_post_ctor_initalized_called);
  m_post_ctor_initalized_called = true;

  for(const PerJoinData &J : m_ordering.non_closing_edge())
    {
      add_join(m_num_joins, J, m_num_verts, m_num_indices);
      ++m_num_joins;
    }

  for(const PerJoinData &J : m_ordering.closing_edge())
    {
      add_join(m_num_joins, J, m_num_verts, m_num_indices);
      ++m_num_joins;
    }
}

void
JoinCreatorBase::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_ranges) const
{
  FASTUIDRAWassert(m_post_ctor_initalized_called);
  num_attributes = m_num_verts;
  num_indices = m_num_indices;
  number_z_ranges = num_attribute_chunks = num_index_chunks = m_num_chunks;
}

void
JoinCreatorBase::
fill_join(unsigned int join_id,
          const PerJoinData &join_data,
          unsigned int chunk, unsigned int depth,
          fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &vertex_offset, unsigned int &index_offset,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts,
          fastuidraw::c_array<fastuidraw::range_type<int> > join_vertex_ranges,
          fastuidraw::c_array<fastuidraw::range_type<int> > join_index_ranges) const
{
  unsigned int v(vertex_offset), i(index_offset);

  FASTUIDRAWassert(join_id < m_num_joins);

  fill_join_implement(join_id, join_data, pts, depth, indices, vertex_offset, index_offset);

  join_vertex_ranges[join_id].m_begin = v;
  join_vertex_ranges[join_id].m_end = vertex_offset;

  join_index_ranges[join_id].m_begin = i;
  join_index_ranges[join_id].m_end = index_offset;

  attribute_chunks[chunk] = pts.sub_array(v, vertex_offset - v);
  index_chunks[chunk] = indices.sub_array(i, index_offset - i);
  index_adjusts[chunk] = -int(v);
  zranges[chunk] = fastuidraw::range_type<int>(depth, depth + 1);
}

void
JoinCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0), index_offset(0), join_id(0);
  std::vector<fastuidraw::range_type<int> > jvr(m_num_joins), jir(m_num_joins);
  fastuidraw::c_array<fastuidraw::range_type<int> > join_vertex_ranges, join_index_ranges;

  join_vertex_ranges = fastuidraw::make_c_array(jvr);
  join_index_ranges = fastuidraw::make_c_array(jir);

  FASTUIDRAWassert(attribute_data.size() == m_num_verts);
  FASTUIDRAWassert(index_data.size() == m_num_indices);
  FASTUIDRAWassert(attribute_chunks.size() == m_num_chunks);
  FASTUIDRAWassert(index_chunks.size() == m_num_chunks);
  FASTUIDRAWassert(zranges.size() == m_num_chunks);
  FASTUIDRAWassert(index_adjusts.size() == m_num_chunks);

  /* Note that we reverse the the depth value, we need to do this because
   * we want the joins draw first to obscure the joins drawn later.
   */
  for(const OrderingEntry<PerJoinData> &J : m_ordering.non_closing_edge())
    {
      fill_join(join_id, J, J.m_chunk, J.m_depth,
                attribute_data, index_data,
                vertex_offset, index_offset,
                attribute_chunks, index_chunks,
                zranges, index_adjusts,
                join_vertex_ranges,
                join_index_ranges);
      ++join_id;
    }

  for(const OrderingEntry<PerJoinData> &J : m_ordering.closing_edge())
    {
      fill_join(join_id, J, J.m_chunk, J.m_depth,
                attribute_data, index_data,
                vertex_offset, index_offset,
                attribute_chunks, index_chunks,
                zranges, index_adjusts,
                join_vertex_ranges,
                join_index_ranges);
      ++join_id;
    }

  FASTUIDRAWassert(vertex_offset == m_num_verts);
  FASTUIDRAWassert(index_offset == m_num_indices);
  set_chunks(m_st,
             join_vertex_ranges, join_index_ranges,
             attribute_data, index_data,
             attribute_chunks, index_chunks,
             zranges, index_adjusts);
}

void
JoinCreatorBase::
set_chunks(const SubsetPrivate *st,
           fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
           fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
           fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
           fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
           fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
           fastuidraw::c_array<int> index_adjusts)
{
  process_chunk(st->non_closing_joins(),
                join_vertex_ranges, join_index_ranges,
                attribute_data, index_data,
                attribute_chunks, index_chunks,
                zranges, index_adjusts);

  process_chunk(st->closing_joins(),
                join_vertex_ranges, join_index_ranges,
                attribute_data, index_data,
                attribute_chunks, index_chunks,
                zranges, index_adjusts);

  if (st->have_children())
    {
      set_chunks(st->child(0),
                 join_vertex_ranges, join_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);

      set_chunks(st->child(1),
                 join_vertex_ranges, join_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);
    }
}

void
JoinCreatorBase::
process_chunk(const RangeAndChunk &ch,
              fastuidraw::c_array<const fastuidraw::range_type<int> > join_vertex_ranges,
              fastuidraw::c_array<const fastuidraw::range_type<int> > join_index_ranges,
              fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts)
{
  unsigned int K;
  fastuidraw::range_type<int> vr, ir;

  if (ch.m_elements.m_begin < ch.m_elements.m_end)
    {
      vr.m_begin = join_vertex_ranges[ch.m_elements.m_begin].m_begin;
      vr.m_end = join_vertex_ranges[ch.m_elements.m_end - 1].m_end;

      ir.m_begin = join_index_ranges[ch.m_elements.m_begin].m_begin;
      ir.m_end = join_index_ranges[ch.m_elements.m_end - 1].m_end;
    }
  else
    {
      vr.m_begin = vr.m_end = 0;
      ir.m_begin = ir.m_end = 0;
    }

  K = ch.m_chunk;
  attribute_chunks[K] = attribute_data.sub_array(vr);
  index_chunks[K] = index_data.sub_array(ir);
  zranges[K] = fastuidraw::range_type<int>(ch.m_depth_range.m_begin, ch.m_depth_range.m_end);
  index_adjusts[K] = -int(vr.m_begin);
}

/////////////////////////////////////////////////
// RoundedJoinCreator::PerRoundedJoin methods
RoundedJoinCreator::PerRoundedJoin::
PerRoundedJoin(const PerJoinData &J, float thresh):
  PerJoinData(J)
{
  /* n0z represents the start point of the rounded join in the complex plane
   * as if the join was at the origin, n1z represents the end point of the
   * rounded join in the complex plane as if the join was at the origin.
   */
  std::complex<float> n0z(m_lambda * n0().x(), m_lambda * n0().y());
  std::complex<float> n1z(m_lambda * n1().x(), m_lambda * n1().y());

  /* n1z_times_conj_n0z satisfies:
   * n1z = n1z_times_conj_n0z * n0z
   * i.e. it represents the arc-movement from n0z to n1z
   */
  std::complex<float> n1z_times_conj_n0z(n1z * std::conj(n0z));

  m_arc_start = n0z;
  m_delta_theta = fastuidraw::t_atan2(n1z_times_conj_n0z.imag(), n1z_times_conj_n0z.real());
  m_num_arc_points = fastuidraw::detail::number_segments_for_tessellation(m_delta_theta, thresh);
  m_delta_theta /= static_cast<float>(m_num_arc_points - 1);
}

void
RoundedJoinCreator::PerRoundedJoin::
add_data(unsigned int depth,
         fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
         unsigned int &vertex_offset,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &index_offset) const
{
  unsigned int i, first;
  float theta;
  fastuidraw::StrokedPoint pt;

  first = vertex_offset;
  set_distance_values(&pt);

  pt.m_position = m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = m_p;
  pt.m_pre_offset = m_lambda * n0();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float t, c, s;
      std::complex<float> cs_as_complex;

      t = static_cast<float>(i) / static_cast<float>(m_num_arc_points - 1);
      c = fastuidraw::t_cos(theta);
      s = fastuidraw::t_sin(theta);
      cs_as_complex = std::complex<float>(c, s) * m_arc_start;

      pt.m_position = m_p;
      pt.m_pre_offset = m_lambda * fastuidraw::vec2(n0().x(), n1().x());
      pt.m_auxiliary_offset = fastuidraw::vec2(t, cs_as_complex.real());
      pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_rounded_join, depth);

      if (m_lambda * n0().y() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPoint::normal0_y_sign_mask;
        }

      if (m_lambda * n1().y() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPoint::normal1_y_sign_mask;
        }

      if (cs_as_complex.imag() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPoint::sin_sign_mask;
        }
      pt.pack_point(&pts[vertex_offset]);
    }

  pt.m_position = m_p;
  pt.m_pre_offset = m_lambda * n1();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  fastuidraw::detail::add_triangle_fan(first, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// RoundedJoinCreator methods
RoundedJoinCreator::
RoundedJoinCreator(const PathData &P,
                   const SubsetPrivate *st,
                   float thresh):
  JoinCreatorBase(P, st),
  m_thresh(thresh)
{
  JoinCount J(P.m_join_ordering);
  m_per_join_data.reserve(J.m_number_close_joins + J.m_number_non_close_joins);
  post_ctor_initalize();
}

void
RoundedJoinCreator::
add_join(unsigned int join_id, const PerJoinData &join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  FASTUIDRAWunused(join_id);
  PerRoundedJoin J(join, m_thresh);

  m_per_join_data.push_back(J);

  /* a triangle fan centered at J.m_p with
   * J.m_num_arc_points along an edge
   */
  vert_count += (1 + J.m_num_arc_points);
  index_count += 3 * (J.m_num_arc_points - 1);
}

void
RoundedJoinCreator::
fill_join_implement(unsigned int join_id, const PerJoinData &join,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join);
  FASTUIDRAWassert(join_id < m_per_join_data.size());
  m_per_join_data[join_id].add_data(depth, pts, vertex_offset, indices, index_offset);
}

////////////////////////////////////////////////
// ArcRoundedJoinCreator::PerArcRoundedJoin methods
ArcRoundedJoinCreator::PerArcRoundedJoin::
PerArcRoundedJoin(const PerJoinData &J):
  PerJoinData(J)
{
  const float per_arc_angle_max(M_PI / 4.0);
  float delta_angle_mag;

  std::complex<float> n0z(m_lambda * n0().x(), m_lambda * n0().y());
  std::complex<float> n1z(m_lambda * n1().x(), m_lambda * n1().y());
  std::complex<float> n1z_times_conj_n0z(n1z * std::conj(n0z));

  m_arc_start = n0z;
  m_delta_angle = fastuidraw::t_atan2(n1z_times_conj_n0z.imag(),
                                      n1z_times_conj_n0z.real());
  delta_angle_mag = fastuidraw::t_abs(m_delta_angle);

  m_count = 1u + static_cast<unsigned int>(delta_angle_mag / per_arc_angle_max);
  fastuidraw::detail::compute_arc_join_size(m_count, &m_vertex_count, &m_index_count);
}

void
ArcRoundedJoinCreator::PerArcRoundedJoin::
add_data(unsigned int depth,
         fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
         unsigned int &vertex_offset,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &index_offset) const
{
  fastuidraw::ArcStrokedPoint pt;

  set_distance_values(&pt);
  pt.m_position = m_p;
  fastuidraw::detail::pack_arc_join(pt, m_count, m_lambda * n0(),
                                    m_delta_angle, m_lambda * n1(),
                                    depth, pts, vertex_offset,
                                    indices, index_offset, true);
}

///////////////////////////////////////////////////
// ArcRoundedJoinCreator methods
ArcRoundedJoinCreator::
ArcRoundedJoinCreator(const PathData &P, const SubsetPrivate *st):
  JoinCreatorBase(P, st)
{
  JoinCount J(P.m_join_ordering);
  m_per_join_data.reserve(J.m_number_close_joins + J.m_number_non_close_joins);
  post_ctor_initalize();
}

void
ArcRoundedJoinCreator::
add_join(unsigned int join_id, const PerJoinData &join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  FASTUIDRAWunused(join_id);
  PerArcRoundedJoin J(join);

  m_per_join_data.push_back(J);

  vert_count += J.m_vertex_count;
  index_count += J.m_index_count;
}

void
ArcRoundedJoinCreator::
fill_join_implement(unsigned int join_id, const PerJoinData &join,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join);
  FASTUIDRAWassert(join_id < m_per_join_data.size());
  m_per_join_data[join_id].add_data(depth, pts, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// BevelJoinCreator methods
BevelJoinCreator::
BevelJoinCreator(const PathData &P, const SubsetPrivate *st):
  JoinCreatorBase(P, st)
{
  post_ctor_initalize();
}

void
BevelJoinCreator::
add_join(unsigned int join_id, const PerJoinData &join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  FASTUIDRAWunused(join_id);
  FASTUIDRAWunused(join);

  /* one triangle per bevel join */
  vert_count += 3;
  index_count += 3;
}

void
BevelJoinCreator::
fill_join_implement(unsigned int join_id, const PerJoinData &J,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join_id);

  fastuidraw::StrokedPoint pt;
  J.set_distance_values(&pt);

  pt.m_position = J.m_p;
  pt.m_pre_offset = J.m_lambda * J.n0();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 0]);

  pt.m_position = J.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 1]);

  pt.m_position = J.m_p;
  pt.m_pre_offset = J.m_lambda * J.n1();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 2]);

  fastuidraw::detail::add_triangle_fan(vertex_offset, vertex_offset + 3, indices, index_offset);

  vertex_offset += 3;
}



///////////////////////////////////////////////////
// MiterClipJoinCreator methods
MiterClipJoinCreator::
MiterClipJoinCreator(const PathData &P, const SubsetPrivate *st):
  JoinCreatorBase(P, st)
{
  post_ctor_initalize();
}

void
MiterClipJoinCreator::
add_join(unsigned int join_id, const PerJoinData &join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  FASTUIDRAWunused(join_id);
  FASTUIDRAWunused(join);

  /* Each join is a triangle fan from 5 points
   * (thus 3 triangles, which is 9 indices)
   */
  vert_count += 5;
  index_count += 9;
}


void
MiterClipJoinCreator::
fill_join_implement(unsigned int join_id, const PerJoinData &J,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join_id);
  fastuidraw::StrokedPoint pt;
  unsigned int first;

  /* The miter point is given by where the two boundary
   * curves intersect. The two curves are given by:
   *
   * a(t) = J.m_p0 + stroke_width * J.m_lamba * J.m_n0 + t * J.m_v0
   * b(s) = J.m_p1 + stroke_width * J.m_lamba * J.m_n1 - s * J.m_v1
   *
   * With J.m_0 is  the location of the join.
   *
   * We need to solve a(t) = b(s) and compute that location.
   * Linear algebra gives us that:
   *
   * t = - stroke_width * J.m_lamba * r
   * s = - stroke_width * J.m_lamba * r
   * where
   * r = (<J.m_v1, J.m_v0> - 1) / <J.m_v0, J.m_n1>
   *
   * thus
   *
   * a(t) = J.m_p + stroke_width * ( J.m_lamba * J.m_n0 -  r * J.m_lamba * J.m_v0)
   *     = b(s)
   *     = J.m_p + stroke_width * ( J.m_lamba * J.m_n1 +  r * J.m_lamba * J.m_v1)
   */

  first = vertex_offset;
  J.set_distance_values(&pt);

  // join center point.
  pt.m_position = J.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve into join
  pt.m_position = J.m_p;
  pt.m_pre_offset = J.m_lambda * J.n0();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point A
  pt.m_position = J.m_p;
  pt.m_pre_offset = J.n0();
  pt.m_auxiliary_offset = J.n1();
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_miter_clip_join, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point B
  pt.m_position = J.m_p;
  pt.m_pre_offset = J.n1();
  pt.m_auxiliary_offset = J.n0();
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_miter_clip_join, depth);
  pt.m_packed_data |= fastuidraw::StrokedPoint::lambda_negated_mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve out from join
  pt.m_position = J.m_p;
  pt.m_pre_offset = J.m_lambda * J.n1();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  fastuidraw::detail::add_triangle_fan(first, vertex_offset, indices, index_offset);
}


////////////////////////////////////
// MiterJoinCreator methods
template<enum fastuidraw::StrokedPoint::offset_type_t tp>
MiterJoinCreator<tp>::
MiterJoinCreator(const PathData &P, const SubsetPrivate *st):
  JoinCreatorBase(P, st)
{
  post_ctor_initalize();
}

template<enum fastuidraw::StrokedPoint::offset_type_t tp>
void
MiterJoinCreator<tp>::
add_join(unsigned int join_id, const PerJoinData &J,
         unsigned int &vert_count, unsigned int &index_count) const
{
  /* Each join is a triangle fan from 4 points
   * (thus 2 triangles, which is 6 indices)
   */
  FASTUIDRAWunused(join_id);
  FASTUIDRAWunused(J);

  vert_count += 4;
  index_count += 6;
}

template<enum fastuidraw::StrokedPoint::offset_type_t tp>
void
MiterJoinCreator<tp>::
fill_join_implement(unsigned int join_id, const PerJoinData &J,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join_id);

  fastuidraw::StrokedPoint pt;
  unsigned int first;

  first = vertex_offset;
  J.set_distance_values(&pt);

  // join center point.
  pt.m_position = J.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve into join
  pt.m_position = J.m_p;
  pt.m_pre_offset = J.m_lambda * J.n0();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point
  pt.m_position = J.m_p;
  pt.m_pre_offset = J.n0();
  pt.m_auxiliary_offset = J.n1();
  pt.m_packed_data = pack_data_join(1, tp, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve out from join
  pt.m_position = J.m_p;
  pt.m_pre_offset = J.m_lambda * J.n1();
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  fastuidraw::detail::add_triangle_fan(first, vertex_offset, indices, index_offset);
}


///////////////////////////////////////////////
// CapCreatorBase methods
CapCreatorBase::
CapCreatorBase(const PathData &P,
               const SubsetPrivate *st,
               PointIndexCapSize sz):
  m_ordering(P.m_cap_ordering),
  m_st(st),
  m_num_chunks(P.m_number_cap_chunks),
  m_size(sz)
{
}

void
CapCreatorBase::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_ranges) const
{
  num_attributes = m_size.m_verts;
  num_indices = m_size.m_indices;
  number_z_ranges = num_attribute_chunks = num_index_chunks = m_num_chunks;
}

void
CapCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0u), index_offset(0u), cap_id(0u);
  std::vector<fastuidraw::range_type<int> > cvr(m_num_chunks), cir(m_num_chunks);

  for(const OrderingEntry<PerCapData> &C : m_ordering.caps())
    {
      unsigned int v(vertex_offset), i(index_offset);

      add_cap(C, C.m_depth, attribute_data, index_data, vertex_offset, index_offset);

      cvr[cap_id].m_begin = v;
      cvr[cap_id].m_end = vertex_offset;

      cir[cap_id].m_begin = i;
      cir[cap_id].m_end = index_offset;

      attribute_chunks[C.m_chunk] = attribute_data.sub_array(cvr[cap_id]);
      index_chunks[C.m_chunk] = index_data.sub_array(cir[cap_id]);
      zranges[C.m_chunk] = fastuidraw::range_type<int>(C.m_depth, C.m_depth + 1);
      index_adjusts[C.m_chunk] = -int(v);

      ++cap_id;
    }

  FASTUIDRAWassert(vertex_offset == m_size.m_verts);
  FASTUIDRAWassert(index_offset == m_size.m_indices);

  set_chunks(m_st,
             fastuidraw::make_c_array(cvr),
             fastuidraw::make_c_array(cir),
             attribute_data, index_data,
             attribute_chunks, index_chunks,
             zranges, index_adjusts);
}

void
CapCreatorBase::
set_chunks(const SubsetPrivate *st,
           fastuidraw::c_array<const fastuidraw::range_type<int> > cap_vertex_ranges,
           fastuidraw::c_array<const fastuidraw::range_type<int> > cap_index_ranges,
           fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
           fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
           fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
           fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
           fastuidraw::c_array<int> index_adjusts)
{
  const RangeAndChunk &ch(st->caps());
  fastuidraw::range_type<int> vr, ir;
  unsigned int K;

  if (ch.m_elements.m_begin < ch.m_elements.m_end)
    {
      vr.m_begin = cap_vertex_ranges[ch.m_elements.m_begin].m_begin;
      vr.m_end = cap_vertex_ranges[ch.m_elements.m_end - 1].m_end;

      ir.m_begin = cap_index_ranges[ch.m_elements.m_begin].m_begin;
      ir.m_end = cap_index_ranges[ch.m_elements.m_end - 1].m_end;

      FASTUIDRAWassert(ch.m_depth_range.m_begin < ch.m_depth_range.m_end);
    }
  else
    {
      vr.m_begin = vr.m_end = 0;
      ir.m_begin = ir.m_end = 0;
      FASTUIDRAWassert(ch.m_depth_range.m_begin == ch.m_depth_range.m_end);
    }

  K = ch.m_chunk;
  attribute_chunks[K] = attribute_data.sub_array(vr);
  index_chunks[K] = index_data.sub_array(ir);
  zranges[K] = fastuidraw::range_type<int>(ch.m_depth_range.m_begin, ch.m_depth_range.m_end);
  index_adjusts[K] = -int(vr.m_begin);

  if (st->have_children())
    {
      set_chunks(st->child(0),
                 cap_vertex_ranges, cap_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);

      set_chunks(st->child(1),
                 cap_vertex_ranges, cap_index_ranges,
                 attribute_data, index_data,
                 attribute_chunks, index_chunks,
                 zranges, index_adjusts);
    }
}

///////////////////////////////////////////////////
// RoundedCapCreator methods
RoundedCapCreator::
RoundedCapCreator(const PathData &P,
                  const SubsetPrivate *st,
                  float thresh):
  CapCreatorBase(P, st, compute_size(P, thresh))
{
  m_num_arc_points_per_cap = fastuidraw::detail::number_segments_for_tessellation(M_PI, thresh);
  m_delta_theta = static_cast<float>(M_PI) / static_cast<float>(m_num_arc_points_per_cap - 1);
}

PointIndexCapSize
RoundedCapCreator::
compute_size(const PathData &P, float thresh)
{
  unsigned int num_caps, num_arc_points_per_cap;
  PointIndexCapSize return_value;

  num_arc_points_per_cap = fastuidraw::detail::number_segments_for_tessellation(M_PI, thresh);

  /* each cap is a triangle fan centered at the cap point. */
  num_caps = P.m_cap_ordering.caps().size();
  return_value.m_verts = (1 + num_arc_points_per_cap) * num_caps;
  return_value.m_indices = 3 * (num_arc_points_per_cap - 1) * num_caps;

  return return_value;
}

void
RoundedCapCreator::
add_cap(const PerCapData &C, unsigned int depth,
        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  fastuidraw::vec2 n, v;
  unsigned int first, i;
  float theta;
  fastuidraw::StrokedPoint pt;

  first = vertex_offset;
  v = C.m_tangent_into_cap;
  n = fastuidraw::vec2(-v.y(), v.x());
  C.set_distance_values(&pt);

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = n;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points_per_cap - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float s, c;

      s = fastuidraw::t_sin(theta);
      c = fastuidraw::t_cos(theta);
      pt.m_position = C.m_p;
      pt.m_pre_offset = n;
      pt.m_auxiliary_offset = fastuidraw::vec2(s, c);
      pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_rounded_cap, depth);
      pt.pack_point(&pts[vertex_offset]);
    }

  pt.m_position = C.m_p;
  pt.m_pre_offset = -n;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  fastuidraw::detail::add_triangle_fan(first, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// SquareCapCreator methods
PointIndexCapSize
SquareCapCreator::
compute_size(const PathData &P)
{
  PointIndexCapSize return_value;
  unsigned int num_caps;

  /* each square cap generates 5 new points
   * and 3 triangles (= 9 indices)
   */
  num_caps = P.m_cap_ordering.caps().size();
  return_value.m_verts = 5 * num_caps;
  return_value.m_indices = 9 * num_caps;

  return return_value;
}

void
SquareCapCreator::
add_cap(const PerCapData &C, unsigned int depth,
        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  unsigned int first;
  fastuidraw::vec2 n, v;
  fastuidraw::StrokedPoint pt;

  first = vertex_offset;
  v = C.m_tangent_into_cap;
  n = fastuidraw::vec2(-v.y(), v.x());
  C.set_distance_values(&pt);

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(0, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = n;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = n;
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_square_cap, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -n;
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_square_cap, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -n;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPoint::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  fastuidraw::detail::add_triangle_fan(first, vertex_offset, indices, index_offset);
}

//////////////////////////////////////
// AdjustableCapCreator methods
PointIndexCapSize
AdjustableCapCreator::
compute_size(const PathData &P)
{
  PointIndexCapSize return_value;
  unsigned int num_caps;

  num_caps = P.m_cap_ordering.caps().size();
  return_value.m_verts = number_points_per_fan * num_caps;
  return_value.m_indices = number_indices_per_fan * num_caps;

  return return_value;
}

void
AdjustableCapCreator::
add_cap(const PerCapData &C, unsigned int depth,
        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  using namespace fastuidraw;
  enum StrokedPoint::offset_type_t type;
  vec2 n, v;
  unsigned int first;
  uint32_t mask;
  StrokedPoint pt;

  mask = (C.m_is_starting_cap) ? 0u :
    uint32_t(StrokedPoint::adjustable_cap_is_end_contour_mask);
  type = StrokedPoint::offset_adjustable_cap;

  first = vertex_offset;
  v = C.m_tangent_into_cap;
  n = vec2(-v.y(), v.x());
  C.set_distance_values(&pt);

  pt.m_position = C.m_p;
  pt.m_pre_offset = vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(0, type, depth) | mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = n;
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(1, type, depth) | mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = n;
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(1, type, depth) | StrokedPoint::adjustable_cap_ending_mask | mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(0, type, depth) | StrokedPoint::adjustable_cap_ending_mask | mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -n;
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(1, type, depth) | StrokedPoint::adjustable_cap_ending_mask | mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -n;
  pt.m_auxiliary_offset = v;
  pt.m_packed_data = pack_data(1, type, depth | mask);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  fastuidraw::detail::add_triangle_fan(first, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////
// ArcRoundedCapCreator methods
ArcRoundedCapCreator::
ArcRoundedCapCreator(const PathData &P,
                     const SubsetPrivate *st):
  CapCreatorBase(P, st, compute_size(P))
{
}

PointIndexCapSize
ArcRoundedCapCreator::
compute_size(const PathData &P)
{
  unsigned int num_caps;
  PointIndexCapSize return_value;
  unsigned int verts_per_cap, indices_per_cap;

  /* each cap is a triangle fan centered at the cap point. */
  num_caps = P.m_cap_ordering.caps().size();
  fastuidraw::detail::compute_arc_join_size(num_arcs_per_cap, &verts_per_cap, &indices_per_cap);
  return_value.m_verts = verts_per_cap * num_caps;
  return_value.m_indices = indices_per_cap * num_caps;

  return return_value;
}

void
ArcRoundedCapCreator::
add_cap(const PerCapData &C, unsigned int depth,
        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  fastuidraw::ArcStrokedPoint pt;
  fastuidraw::vec2 n, v;

  pt.m_position = C.m_p;
  C.set_distance_values(&pt);
  v = C.m_tangent_into_cap;
  n = fastuidraw::vec2(v.y(), -v.x());

  fastuidraw::detail::pack_arc_join(pt, num_arcs_per_cap,
                                    n, M_PI, -n,
                                    depth, pts, vertex_offset,
                                    indices, index_offset, true);
}

/////////////////////////////////////////////
// StrokedCapsJoinsPrivate methods
StrokedCapsJoinsPrivate::
StrokedCapsJoinsPrivate(const ContourData &P):
  m_subset(nullptr)
{
  if (!P.m_per_contour_data.empty())
    {
      m_empty_path = false;
      create_joins_caps(P);
    }
  else
    {
      m_empty_path = true;
      m_bevel_joins.mark_as_empty();
      m_miter_clip_joins.mark_as_empty();
      m_miter_joins.mark_as_empty();
      m_miter_bevel_joins.mark_as_empty();
      m_square_caps.mark_as_empty();
      m_adjustable_caps.mark_as_empty();
      std::fill(m_chunk_of_joins.begin(), m_chunk_of_joins.end(), 0);
      m_chunk_of_caps = 0;
    }
}

StrokedCapsJoinsPrivate::
~StrokedCapsJoinsPrivate()
{
  for(unsigned int i = 0, endi = m_rounded_joins.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_rounded_joins[i].m_data);
    }

  for(unsigned int i = 0, endi = m_rounded_caps.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_rounded_caps[i].m_data);
    }

  if (!m_empty_path)
    {
      FASTUIDRAWdelete(m_subset);
    }
}

void
StrokedCapsJoinsPrivate::
create_joins_caps(const ContourData &P)
{
  CullingHierarchy *s;
  SubsetPrivate::CreationValues cnts;

  FASTUIDRAWassert(!m_empty_path);
  s = CullingHierarchy::create(P);
  m_subset = SubsetPrivate::create(s, cnts, m_path_data.m_join_ordering, m_path_data.m_cap_ordering);

  m_path_data.m_number_join_chunks = cnts.m_non_closing_join_chunk_cnt + cnts.m_closing_join_chunk_cnt;
  m_path_data.m_number_cap_chunks = cnts.m_cap_chunk_cnt;

  /* the chunks of the root element have the data for everything. */
  m_chunk_of_joins[fastuidraw::StrokedCapsJoins::all_non_closing] = m_subset->non_closing_joins().m_chunk;
  m_chunk_of_joins[fastuidraw::StrokedCapsJoins::all_closing] = m_subset->closing_joins().m_chunk;

  m_chunk_of_caps = m_subset->caps().m_chunk;

  FASTUIDRAWdelete(s);
}

template<typename T>
const fastuidraw::PainterAttributeData&
StrokedCapsJoinsPrivate::
fetch_create(float thresh, std::vector<ThreshWithData> &values)
{
  if (values.empty())
    {
      fastuidraw::PainterAttributeData *newD;
      newD = FASTUIDRAWnew fastuidraw::PainterAttributeData();
      newD->set_data(T(m_path_data, m_subset, 1.0f));
      values.push_back(ThreshWithData(newD, 1.0f));
    }

  /* we set a hard tolerance of 1e-6. Should we
   * set it as a ratio of the bounding box of
   * the underlying tessellated path?
   */
  thresh = fastuidraw::t_max(thresh, float(1e-6));
  if (values.back().m_thresh <= thresh)
    {
      std::vector<ThreshWithData>::const_iterator iter;
      iter = std::lower_bound(values.begin(), values.end(), thresh,
                              ThreshWithData::reverse_compare_against_thresh);
      FASTUIDRAWassert(iter != values.end());
      FASTUIDRAWassert(iter->m_thresh <= thresh);
      FASTUIDRAWassert(iter->m_data != nullptr);
      return *iter->m_data;
    }
  else
    {
      float t;
      t = values.back().m_thresh;
      while(t > thresh)
        {
          fastuidraw::PainterAttributeData *newD;

          t *= 0.5f;
          newD = FASTUIDRAWnew fastuidraw::PainterAttributeData();
          newD->set_data(T(m_path_data, m_subset, t));
          values.push_back(ThreshWithData(newD, t));
        }
      return *values.back().m_data;
    }
}

//////////////////////////////////////////////
// fastuidraw::StrokedCapsJoins::ScratchSpace methods
fastuidraw::StrokedCapsJoins::ScratchSpace::
ScratchSpace(void)
{
  m_d = FASTUIDRAWnew ScratchSpacePrivate();
}

fastuidraw::StrokedCapsJoins::ScratchSpace::
~ScratchSpace(void)
{
  ScratchSpacePrivate *d;
  d = static_cast<ScratchSpacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

///////////////////////////////////////
// ChunkSetPrivate methods
void
ChunkSetPrivate::
add_join_chunk(const RangeAndChunk &j)
{
  if (j.non_empty() && !m_ignore_join_adds)
    {
      m_join_chunks.push_back(j.m_chunk);
      m_join_ranges.push_back(j.m_elements);
    }
}

void
ChunkSetPrivate::
add_cap_chunk(const RangeAndChunk &c)
{
  if (c.non_empty())
    {
      m_cap_chunks.push_back(c.m_chunk);
    }
}

//////////////////////////////////////////
// fastuidraw::StrokedCapsJoins::ChunkSet methods
fastuidraw::StrokedCapsJoins::ChunkSet::
ChunkSet(void)
{
  m_d = FASTUIDRAWnew ChunkSetPrivate();
}

fastuidraw::StrokedCapsJoins::ChunkSet::
~ChunkSet(void)
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::c_array<const unsigned int>
fastuidraw::StrokedCapsJoins::ChunkSet::
join_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return d->join_chunks();
}

fastuidraw::c_array<const unsigned int>
fastuidraw::StrokedCapsJoins::ChunkSet::
cap_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return d->cap_chunks();
}

////////////////////////////////////////
// fastuidraw::StrokedCapsJoins::Builder methods
fastuidraw::StrokedCapsJoins::Builder::
Builder(void)
{
  m_d = FASTUIDRAWnew ContourData();
}

fastuidraw::StrokedCapsJoins::Builder::
~Builder()
{
  ContourData *d;
  d = static_cast<ContourData*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::StrokedCapsJoins::Builder::
begin_contour(const vec2 &start_pt,
              const vec2 &path_direction)
{
  ContourData *d;
  d = static_cast<ContourData*>(m_d);

  d->m_bounding_box.union_point(start_pt);
  d->m_per_contour_data.push_back(PerContourData());
  d->m_per_contour_data.back().start(start_pt, path_direction);
}

void
fastuidraw::StrokedCapsJoins::Builder::
add_join(const vec2 &join_pt,
         float distance_from_previous_join,
         const vec2 &direction_into_join,
         const vec2 &direction_leaving_join)
{
  ContourData *d;
  d = static_cast<ContourData*>(m_d);

  FASTUIDRAWassert(!d->m_per_contour_data.empty());
  d->m_bounding_box.union_point(join_pt);
  d->m_per_contour_data.back().add_join(join_pt, distance_from_previous_join,
                                        direction_into_join, direction_leaving_join);
}

void
fastuidraw::StrokedCapsJoins::Builder::
end_contour(float distance_from_previous_join,
            const vec2 &tangent_along_curve)
{
  ContourData *d;
  d = static_cast<ContourData*>(m_d);

  FASTUIDRAWassert(!d->m_per_contour_data.empty());
  d->m_per_contour_data.back().end(distance_from_previous_join,
                                   tangent_along_curve);
}

//////////////////////////////////////////////////////////////
// fastuidraw::StrokedCapsJoins methods
fastuidraw::StrokedCapsJoins::
StrokedCapsJoins(const Builder &B)
{
  ContourData *C;
  C = static_cast<ContourData*>(B.m_d);

  m_d = FASTUIDRAWnew StrokedCapsJoinsPrivate(*C);
}

fastuidraw::StrokedCapsJoins::
~StrokedCapsJoins()
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::StrokedCapsJoins::
compute_chunks(ScratchSpace &scratch_space,
               c_array<const vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               const vec2 &recip_dimensions,
               float pixels_additional_room,
               float item_space_additional_room,
               bool include_closing_edges,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               bool take_joins_outside_of_region,
               ChunkSet &dst) const
{
  StrokedCapsJoinsPrivate *d;
  ScratchSpacePrivate *scratch_space_ptr;
  ChunkSetPrivate *chunk_set_ptr;

  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  scratch_space_ptr = static_cast<ScratchSpacePrivate*>(scratch_space.m_d);
  chunk_set_ptr = static_cast<ChunkSetPrivate*>(dst.m_d);

  if (d->m_empty_path)
    {
      chunk_set_ptr->reset();
      return;
    }

  d->m_subset->compute_chunks(include_closing_edges,
                              *scratch_space_ptr,
                              clip_equations,
                              clip_matrix_local,
                              recip_dimensions,
                              pixels_additional_room,
                              item_space_additional_room,
                              max_attribute_cnt,
                              max_index_cnt,
                              take_joins_outside_of_region,
                              *chunk_set_ptr);
}

unsigned int
fastuidraw::StrokedCapsJoins::
number_joins(bool include_joins_of_closing_edge) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_path_data.m_join_ordering.number_joins(include_joins_of_closing_edge);
}

unsigned int
fastuidraw::StrokedCapsJoins::
join_chunk(unsigned int J) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_path_data.m_join_ordering.join_chunk(J);
}

unsigned int
fastuidraw::StrokedCapsJoins::
chunk_of_joins(enum chunk_selection c) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_chunk_of_joins[c];
}

unsigned int
fastuidraw::StrokedCapsJoins::
chunk_of_caps(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_chunk_of_caps;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
square_caps(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_square_caps.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
adjustable_caps(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_adjustable_caps.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
bevel_joins(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_bevel_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
miter_clip_joins(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_miter_clip_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
miter_bevel_joins(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_miter_bevel_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
miter_joins(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_miter_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
arc_rounded_joins(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_arc_rounded_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
arc_rounded_caps(void) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return d->m_arc_rounded_caps.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
rounded_joins(float thresh) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);

  return (!d->m_empty_path) ?
    d->fetch_create<RoundedJoinCreator>(thresh, d->m_rounded_joins) :
    d->m_empty_data;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedCapsJoins::
rounded_caps(float thresh) const
{
  StrokedCapsJoinsPrivate *d;
  d = static_cast<StrokedCapsJoinsPrivate*>(m_d);
  return (!d->m_empty_path) ?
    d->fetch_create<RoundedCapCreator>(thresh, d->m_rounded_caps) :
    d->m_empty_data;
}
