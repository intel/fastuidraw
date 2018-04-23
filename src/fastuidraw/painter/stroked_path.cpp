/*!
 * \file stroked_path.cpp
 * \brief file stroked_path.cpp
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

#include <vector>
#include <complex>
#include <algorithm>

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
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
            enum fastuidraw::StrokedPath::point::offset_type_t pt,
            uint32_t depth)
  {
    using namespace fastuidraw;
    FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);
    return pack_bits(StrokedPath::point::offset_type_bit0,
                     StrokedPath::point::offset_type_num_bits, pp)
      | pack_bits(StrokedPath::point::boundary_bit, 1u, bb)
      | pack_bits(StrokedPath::point::depth_bit0,
                  StrokedPath::point::depth_num_bits, depth);
  }

  inline
  uint32_t
  pack_data_join(int on_boundary,
                 enum fastuidraw::StrokedPath::point::offset_type_t pt,
                 uint32_t depth)
  {
    return pack_data(on_boundary, pt, depth) | fastuidraw::StrokedPath::point::join_mask;
  }

  void
  add_triangle_fan(unsigned int begin, unsigned int end,
                   fastuidraw::c_array<unsigned int> indices,
                   unsigned int &index_offset)
  {
    for(unsigned int i = begin + 1; i < end - 1; ++i, index_offset += 3)
      {
        indices[index_offset + 0] = begin;
        indices[index_offset + 1] = i;
        indices[index_offset + 2] = i + 1;
      }
  }

  class JoinSource
  {
  public:
    fastuidraw::vec2 m_pt;
    unsigned int m_contour;
    unsigned int m_edge_going_into_join;
    bool m_of_closing_edge;
  };

  class CapSource
  {
  public:
    fastuidraw::vec2 m_pt;
    unsigned int m_contour;
    bool m_is_start_cap;
  };

  class PerEdgeData
  {
  public:
    fastuidraw::vec2 m_begin_normal, m_end_normal;
    fastuidraw::TessellatedPath::point m_start_pt, m_end_pt;
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

  /* provides in what chunk each Join is located.
   */
  class JoinOrdering
  {
  public:
    fastuidraw::c_array<const OrderingEntry<JoinSource> >
    non_closing_edge(void) const
    {
      return fastuidraw::make_c_array(m_non_closing_edge);
    }

    fastuidraw::c_array<const OrderingEntry<JoinSource> >
    closing_edge(void) const
    {
      return fastuidraw::make_c_array(m_closing_edge);
    }

    void
    add_join(const JoinSource &src, unsigned int &chunk)
    {
      OrderingEntry<JoinSource> J(src, chunk);
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
    number_joins(bool include_joins_of_closing_edge) const
    {
      return include_joins_of_closing_edge ?
        m_non_closing_edge.size() + m_closing_edge.size() :
        m_non_closing_edge.size();
    }

    unsigned int
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

  private:
    std::vector<OrderingEntry<JoinSource> > m_non_closing_edge;
    std::vector<OrderingEntry<JoinSource> > m_closing_edge;
  };

  class CapOrdering
  {
  public:
    fastuidraw::c_array<const OrderingEntry<CapSource> >
    caps(void) const
    {
      return fastuidraw::make_c_array(m_caps);
    }

    void
    add_cap(const CapSource &src, unsigned int &chunk)
    {
      OrderingEntry<CapSource> C(src, chunk);
      m_caps.push_back(C);
      ++chunk;
    }

    void
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

  private:
    std::vector<OrderingEntry<CapSource> > m_caps;
  };

  class PerContourData
  {
  public:
    const PerEdgeData&
    edge_data(unsigned int E) const
    {
      return (E == m_edge_data_store.size()) ?
        m_edge_data_store[0]:
        m_edge_data_store[E];
    }

    PerEdgeData&
    write_edge_data(unsigned int E)
    {
      FASTUIDRAWassert(E < m_edge_data_store.size());
      return m_edge_data_store[E];
    }

    fastuidraw::vec2 m_begin_cap_normal, m_end_cap_normal;
    fastuidraw::TessellatedPath::point m_start_contour_pt, m_end_contour_pt;
    std::vector<PerEdgeData> m_edge_data_store;
  };

  class ContourData:fastuidraw::noncopyable
  {
  public:
    unsigned int
    number_contours(void) const
    {
      return m_per_contour_data.size();
    }

    unsigned int
    number_edges(unsigned int C) const
    {
      FASTUIDRAWassert(C < m_per_contour_data.size());
      return m_per_contour_data[C].m_edge_data_store.size();
    }

    std::vector<PerContourData> m_per_contour_data;
  };

  class PathData:fastuidraw::noncopyable
  {
  public:
    ContourData m_contour_data;

    JoinOrdering m_join_ordering;
    unsigned int m_number_join_chunks;
    CapOrdering m_cap_ordering;
    unsigned int m_number_cap_chunks;
  };

  class SingleSubEdge
  {
  public:
    class point
    {
    public:
      point(const point &p0, const point &p1, float t);
      point(void) {}

      explicit
      point(const fastuidraw::TessellatedPath::point &pt);

      fastuidraw::vec2 m_pt;
      float m_distance_from_edge_start;
      float m_distance_from_contour_start;
      float m_edge_length;
      float m_open_contour_length;
      float m_closed_contour_length;
    };

    void
    split_sub_edge(int splitting_coordinate,
                   fastuidraw::vecN<SingleSubEdge, 2> &out_edges,
                   float split_value) const;

    point m_pt0, m_pt1;
    fastuidraw::vec2 m_normal, m_delta;
    bool m_of_closing_edge;

    bool m_has_bevel;
    float m_bevel_lambda;
    fastuidraw::vec2 m_bevel_normal;
  };

  /* A SubEdgeCullingHierarchy represents a hierarchy choice of
     what sub-edges land in each element of a hierarchy.
   */
  class SubEdgeCullingHierarchy:fastuidraw::noncopyable
  {
  public:
    static
    SubEdgeCullingHierarchy*
    create(const fastuidraw::TessellatedPath &P, ContourData &contour_data);

    ~SubEdgeCullingHierarchy();

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

    const SubEdgeCullingHierarchy*
    child(unsigned int i) const
    {
      return m_children[i];
    }

    fastuidraw::c_array<const SingleSubEdge>
    non_closing_edges(void) const
    {
      return m_sub_edges.m_non_closing;
    }

    fastuidraw::c_array<const SingleSubEdge>
    closing_edges(void) const
    {
      return m_sub_edges.m_closing;
    }

    fastuidraw::c_array<const JoinSource>
    non_closing_joins(void) const
    {
      return m_joins.m_non_closing;
    }

    fastuidraw::c_array<const JoinSource>
    closing_joins(void) const
    {
      return m_joins.m_closing;
    }

    fastuidraw::c_array<const CapSource>
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

    template<typename T>
    class PartitionedData
    {
    public:
      void
      init(std::vector<T> &data, unsigned int num_non_closing)
      {
        fastuidraw::c_array<const T> all_data;

        m_data.swap(data);
        all_data = fastuidraw::make_c_array(m_data);

        m_non_closing = all_data.sub_array(0, num_non_closing);
        m_closing = all_data.sub_array(num_non_closing);
      }

      /* all data, with data so that data with closing
         is only in the back
       */
      std::vector<T> m_data;

      /* array into m_data of data of non-closing edges
       */
      fastuidraw::c_array<const T> m_non_closing;

      /* array into m_data of data of ONLY closing edges
       */
      fastuidraw::c_array<const T> m_closing;
    };

    enum
      {
        splitting_threshhold = 20
      };

    SubEdgeCullingHierarchy(const fastuidraw::BoundingBox<float> &start_box,
                            std::vector<SingleSubEdge> &data, unsigned int num_non_closing_edges,
                            std::vector<JoinSource> &joins, unsigned int num_non_closing_joins,
                            std::vector<CapSource> &caps);

    /* a value of -1 means to NOT split.
     */
    static
    int
    choose_splitting_coordinate(const fastuidraw::BoundingBox<float> &start_box,
                                fastuidraw::c_array<const SingleSubEdge> data,
                                float &split_value);

    static
    void
    create_lists(const fastuidraw::TessellatedPath &P, ContourData &contour_data,
                 std::vector<SingleSubEdge> &data, unsigned int &num_non_closing_edges,
                 fastuidraw::BoundingBox<float> &bx,
                 std::vector<JoinSource> &joins, unsigned int &num_non_closing_joins,
                 std::vector<CapSource> &caps);

    static
    void
    process_edge(const fastuidraw::TessellatedPath &P, ContourData &contour_data,
                 unsigned int contour, unsigned int edge,
                 std::vector<SingleSubEdge> &dst,
                 fastuidraw::BoundingBox<float> &bx);

    template<typename T>
    static
    void
    check_closing_at_end(const std::vector<T> &data, unsigned int num_non_closing);

    /* tolerance for normal computation when creating edge list.
     */
    static const float sm_mag_tol;

    /* children, if any
     */
    fastuidraw::vecN<SubEdgeCullingHierarchy*, 2> m_children;

    /* Caps inside, only non-empty if has no children.
     */
    std::vector<CapSource> m_caps;

    /* Joins inside, only non-empty if has no children.
     */
    PartitionedData<JoinSource> m_joins;

    /* what edges of this, only non-empty if has no children.
     */
    PartitionedData<SingleSubEdge> m_sub_edges;

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
    std::vector<float> m_clip_scratch_floats;
  };

  class EdgeRanges
  {
  public:
    /* range where vertices and indices of edges are located
     */
    fastuidraw::range_type<unsigned int> m_vertex_data_range;
    fastuidraw::range_type<unsigned int> m_index_data_range;

    /* range of depth values applied to edges.
     */
    fastuidraw::range_type<unsigned int> m_depth_range;

    /* chunk in PainterAttributeData holding the range
       of indices and vertices above.
     */
    unsigned int m_chunk;

    /* source of data, only non-empty if EdgeAttributeFiller
       should create attribute/index data at the ranges
       above.
     */
    fastuidraw::c_array<const SingleSubEdge> m_src;

    bool
    chunk_fits(unsigned int max_attribute_cnt,
               unsigned int max_index_cnt) const
    {
      return m_vertex_data_range.difference() <= max_attribute_cnt
        && m_index_data_range.difference() <= max_index_cnt;
    }

    bool
    non_empty(void) const
    {
      return m_vertex_data_range.m_end > m_vertex_data_range.m_begin;
    }
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
      m_edge_chunks.clear();
      m_join_chunks.clear();
      m_join_ranges.clear();
      m_cap_chunks.clear();
    }

    void
    add_edge_chunk(const EdgeRanges &ed);

    void
    add_join_chunk(const RangeAndChunk &j);

    void
    add_cap_chunk(const RangeAndChunk &c);

    fastuidraw::c_array<const unsigned int>
    edge_chunks(void) const
    {
      return fastuidraw::make_c_array(m_edge_chunks);
    }

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

    void
    handle_dashed_evaluator(const fastuidraw::DashEvaluatorBase *dash_evaluator,
                            const fastuidraw::PainterShaderData::DataBase *dash_data,
                            const fastuidraw::StrokedPath &path);

  private:
    std::vector<unsigned int> m_edge_chunks, m_join_chunks, m_cap_chunks;
    std::vector<fastuidraw::range_type<unsigned int> > m_join_ranges;
    bool m_ignore_join_adds;
  };

  /* Subset of a StrokedPath. Edges are to be placed into
     the store as follows:
       1. child0 edges
       2. child1 edges
       3. edges (i.e. from SubEdgeCullingHierarchy::m_sub_edges)

     with the invariant thats that
   */
  class StrokedPathSubset
  {
  public:
    enum
      {
        points_per_segment = 6,
        triangles_per_segment = points_per_segment - 2,
        indices_per_segment_without_bevel = 3 * triangles_per_segment,
      };

    class CreationValues
    {
    public:
      CreationValues(void):
        m_non_closing_edge_vertex_cnt(0),
        m_non_closing_edge_index_cnt(0),
        m_non_closing_edge_chunk_cnt(0),
        m_closing_edge_vertex_cnt(0),
        m_closing_edge_index_cnt(0),
        m_closing_edge_chunk_cnt(0),
        m_non_closing_join_chunk_cnt(0),
        m_closing_join_chunk_cnt(0),
        m_cap_chunk_cnt(0)
      {}

      unsigned int m_non_closing_edge_vertex_cnt;
      unsigned int m_non_closing_edge_index_cnt;
      unsigned int m_non_closing_edge_chunk_cnt;

      unsigned int m_closing_edge_vertex_cnt;
      unsigned int m_closing_edge_index_cnt;
      unsigned int m_closing_edge_chunk_cnt;

      unsigned int m_non_closing_join_chunk_cnt;
      unsigned int m_closing_join_chunk_cnt;

      unsigned int m_cap_chunk_cnt;
    };

    static
    StrokedPathSubset*
    create(const SubEdgeCullingHierarchy *src,
           CreationValues &out_values,
           JoinOrdering &join_ordering,
           CapOrdering &cap_ordering);

    ~StrokedPathSubset();

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

    const StrokedPathSubset*
    child(unsigned int I) const
    {
      FASTUIDRAWassert(have_children());
      FASTUIDRAWassert(I == 0 || I == 1);
      return m_children[I];
    }

    const EdgeRanges&
    non_closing_edges(void) const
    {
      return m_non_closing_edges;
    }

    const EdgeRanges&
    closing_edges(void) const
    {
      return m_closing_edges;
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
        m_edge_depth(0),
        m_closing_edge_depth(0),
        m_join_depth(0),
        m_closing_join_depth(0),
        m_cap_depth(0)
      {}

      unsigned int m_edge_depth, m_closing_edge_depth;
      unsigned int m_join_depth, m_closing_join_depth;
      unsigned int m_cap_depth;
    };

    StrokedPathSubset(CreationValues &out_values,
                      JoinOrdering &join_ordering,
                      CapOrdering &cap_ordering,
                      const SubEdgeCullingHierarchy *src);

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

    static
    void
    increment_vertices_indices(fastuidraw::c_array<const SingleSubEdge> src,
                               unsigned int &vertex_cnt,
                               unsigned int &index_cnt);
    void
    post_process(PostProcessVariables &variables,
                 const CreationValues &constants,
                 JoinOrdering &join_ordering,
                 CapOrdering &cap_ordering);

    fastuidraw::vecN<StrokedPathSubset*, 2> m_children;

    /* book keeping for edges.
     */
    EdgeRanges m_non_closing_edges, m_closing_edges;

    /* book keeping for joins
     */
    RangeAndChunk m_non_closing_joins, m_closing_joins;

    /* book keeping for caps
     */
    RangeAndChunk m_caps;

    fastuidraw::BoundingBox<float> m_bb;

    bool m_empty_subset;
  };

  class EdgeAttributeFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    EdgeAttributeFiller(const StrokedPathSubset *src,
                        const fastuidraw::TessellatedPath &P,
                        const StrokedPathSubset::CreationValues &cnts);

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
    void
    fill_data_worker(const StrokedPathSubset *e,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
                     fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                     fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
                     fastuidraw::c_array<int> index_adjusts) const;

    void
    build_chunk(const EdgeRanges &edge,
                fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
                fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
                fastuidraw::c_array<int> index_adjusts) const;

    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const;

    const StrokedPathSubset *m_src;
    const fastuidraw::TessellatedPath &m_P;
    unsigned int m_total_vertex_cnt, m_total_index_cnt;
    unsigned int m_total_number_chunks;
  };

  class JoinCount
  {
  public:
    explicit
    JoinCount(const ContourData &P);

    unsigned int m_number_close_joins;
    unsigned int m_number_non_close_joins;
  };

  class CommonJoinData
  {
  public:
    CommonJoinData(const fastuidraw::vec2 &p0,
                   const fastuidraw::vec2 &n0,
                   const fastuidraw::vec2 &p1,
                   const fastuidraw::vec2 &n1,
                   float distance_from_edge_start,
                   float distance_from_contour_start,
                   float edge_length,
                   float open_contour_length,
                   float closed_contour_length);

    float m_det, m_lambda;
    fastuidraw::vec2 m_p0, m_v0, m_n0;
    fastuidraw::vec2 m_p1, m_v1, m_n1;
    float m_distance_from_edge_start;
    float m_distance_from_contour_start;
    float m_edge_length;
    float m_open_contour_length;
    float m_closed_contour_length;

    static
    float
    compute_lambda(const fastuidraw::vec2 &n0, const fastuidraw::vec2 &n1);
  };

  class JoinCreatorBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    JoinCreatorBase(const PathData &P,
                    const StrokedPathSubset *st);

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
    fill_join(unsigned int join_id,
              const PerEdgeData &into_join,
              const PerEdgeData &leaving_join,
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
    set_chunks(const StrokedPathSubset *st,
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
    add_join(unsigned int join_id,
             const PerEdgeData &into_join,
             const PerEdgeData &leaving_join,
             unsigned int &vert_count, unsigned int &index_count) const = 0;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PerEdgeData &into_join,
                        const PerEdgeData &leaving_join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const = 0;

    const ContourData &m_P;
    const JoinOrdering &m_ordering;
    const StrokedPathSubset *m_st;
    unsigned int m_num_verts, m_num_indices, m_num_chunks, m_num_joins;
    bool m_post_ctor_initalized_called;
  };


  class RoundedJoinCreator:public JoinCreatorBase
  {
  public:
    RoundedJoinCreator(const PathData &P,
                       const StrokedPathSubset *st,
                       float thresh);

  private:

    class PerJoinData:public CommonJoinData
    {
    public:
      PerJoinData(const fastuidraw::TessellatedPath::point &p0,
                  const fastuidraw::TessellatedPath::point &p1,
                  const fastuidraw::vec2 &n0_from_stroking,
                  const fastuidraw::vec2 &n1_from_stroking,
                  float thresh);

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
    add_join(unsigned int join_id,
             const PerEdgeData &into_join,
             const PerEdgeData &leaving_join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PerEdgeData &into_join,
                        const PerEdgeData &leaving_join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;

    float m_thresh;
    mutable std::vector<PerJoinData> m_per_join_data;
  };

  class BevelJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    BevelJoinCreator(const PathData &P,
                     const StrokedPathSubset *st);

  private:

    virtual
    void
    add_join(unsigned int join_id,
             const PerEdgeData &into_join,
             const PerEdgeData &leaving_join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PerEdgeData &into_join,
                        const PerEdgeData &leaving_join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;

    mutable std::vector<fastuidraw::vec2> m_n0, m_n1;
  };

  class MiterClipJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    MiterClipJoinCreator(const PathData &P,
                         const StrokedPathSubset *st);

  private:
    virtual
    void
    add_join(unsigned int join_id,
             const PerEdgeData &into_join,
             const PerEdgeData &leaving_join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PerEdgeData &into_join,
                        const PerEdgeData &leaving_join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;

    mutable std::vector<fastuidraw::vec2> m_n0, m_n1;
  };

  template<enum fastuidraw::StrokedPath::point::offset_type_t tp>
  class MiterJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    MiterJoinCreator(const PathData &P,
                     const StrokedPathSubset *st);

  private:
    virtual
    void
    add_join(unsigned int join_id,
             const PerEdgeData &into_join,
             const PerEdgeData &leaving_join,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PerEdgeData &into_join,
                        const PerEdgeData &leaving_join,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;

    mutable std::vector<fastuidraw::vec2> m_n0, m_n1;
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

  class CommonCapData
  {
  public:
    CommonCapData(bool is_start_cap,
                  const fastuidraw::vec2 &src_pt,
                  const fastuidraw::vec2 &normal_from_stroking):
      m_is_start_cap(is_start_cap),
      m_lambda((is_start_cap) ? -1.0f : 1.0f),
      m_p(src_pt),
      m_n(normal_from_stroking)
    {
      //caps at the start are on the "other side"
      m_v = fastuidraw::vec2(m_n.y(), -m_n.x());
      m_v *= m_lambda;
      m_n *= m_lambda;
    }

    bool m_is_start_cap;
    float m_lambda;
    fastuidraw::vec2 m_p, m_n, m_v;
  };

  class CapCreatorBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    CapCreatorBase(const PathData &P,
                   const StrokedPathSubset *st,
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
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap, unsigned int depth,
            const fastuidraw::TessellatedPath::point &p0,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const = 0;

    static
    void
    set_chunks(const StrokedPathSubset *st,
               fastuidraw::c_array<const fastuidraw::range_type<int> > cap_vertex_ranges,
               fastuidraw::c_array<const fastuidraw::range_type<int> > cap_index_ranges,
               fastuidraw::c_array<const fastuidraw::PainterAttribute> attribute_data,
               fastuidraw::c_array<const fastuidraw::PainterIndex> index_data,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
               fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
               fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
               fastuidraw::c_array<int> index_adjusts);

    const ContourData &m_P;
    const CapOrdering &m_ordering;
    const StrokedPathSubset *m_st;

    unsigned int m_num_chunks;
    PointIndexCapSize m_size;
  };

  class RoundedCapCreator:public CapCreatorBase
  {
  public:
    RoundedCapCreator(const PathData &P,
                      const StrokedPathSubset *st,
                      float thresh);

  private:
    static
    PointIndexCapSize
    compute_size(const ContourData &P, float thresh);

    virtual
    void
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap, unsigned int depth,
            const fastuidraw::TessellatedPath::point &p0,
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
                     const StrokedPathSubset *st):
      CapCreatorBase(P, st, compute_size(P.m_contour_data))
    {}

  private:
    static
    PointIndexCapSize
    compute_size(const ContourData &P);

    void
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap, unsigned int depth,
            const fastuidraw::TessellatedPath::point &p0,
            fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
            fastuidraw::c_array<unsigned int> indices,
            unsigned int &vertex_offset,
            unsigned int &index_offset) const;
  };

  class AdjustableCapCreator:public CapCreatorBase
  {
  public:
    AdjustableCapCreator(const PathData &P,
                         const StrokedPathSubset *st):
      CapCreatorBase(P, st, compute_size(P.m_contour_data))
    {}

  private:
    enum
      {
        number_points_per_fan = 6,
        number_triangles_per_fan = number_points_per_fan - 2,
        number_indices_per_fan = 3 * number_triangles_per_fan,
      };

    static
    void
    pack_fan(bool entering_contour,
             enum fastuidraw::StrokedPath::point::offset_type_t type,
             const fastuidraw::TessellatedPath::point &edge_pt,
             const fastuidraw::vec2 &stroking_normal,
             unsigned int depth,
             fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
             unsigned int &vertex_offset,
             fastuidraw::c_array<unsigned int> indices,
             unsigned int &index_offset);

    static
    PointIndexCapSize
    compute_size(const ContourData &P);

    void
    add_cap(const fastuidraw::vec2 &normal_from_stroking,
            bool is_starting_cap, unsigned int depth,
            const fastuidraw::TessellatedPath::point &p0,
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
    data(const PathData &P, const StrokedPathSubset *st)
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

  class StrokedPathPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    StrokedPathPrivate(const fastuidraw::TessellatedPath &P);
    ~StrokedPathPrivate();

    void
    create_edges(const fastuidraw::TessellatedPath &P);

    template<typename T>
    const fastuidraw::PainterAttributeData&
    fetch_create(float thresh,
                 std::vector<ThreshWithData> &values);

    StrokedPathSubset* m_subset;
    fastuidraw::PainterAttributeData m_edges;

    PreparedAttributeData<BevelJoinCreator> m_bevel_joins;
    PreparedAttributeData<MiterClipJoinCreator> m_miter_clip_joins;
    PreparedAttributeData<MiterJoinCreator<fastuidraw::StrokedPath::point::offset_miter_join> > m_miter_joins;
    PreparedAttributeData<MiterJoinCreator<fastuidraw::StrokedPath::point::offset_miter_bevel_join> > m_miter_bevel_joins;
    PreparedAttributeData<SquareCapCreator> m_square_caps;
    PreparedAttributeData<AdjustableCapCreator> m_adjustable_caps;

    PathData m_path_data;
    fastuidraw::vecN<unsigned int, 2> m_chunk_of_joins;
    fastuidraw::vecN<unsigned int, 2> m_chunk_of_edges;
    unsigned int m_chunk_of_caps;

    std::vector<ThreshWithData> m_rounded_joins;
    std::vector<ThreshWithData> m_rounded_caps;

    bool m_empty_path;
    fastuidraw::PainterAttributeData m_empty_data;
  };

}

///////////////////////////////////////
// SingleSubEdge::point methods
SingleSubEdge::point::
point(const fastuidraw::TessellatedPath::point &pt):
  m_pt(pt.m_p),
  m_distance_from_edge_start(pt.m_distance_from_edge_start),
  m_distance_from_contour_start(pt.m_distance_from_contour_start),
  m_edge_length(pt.m_edge_length),
  m_open_contour_length(pt.m_open_contour_length),
  m_closed_contour_length(pt.m_closed_contour_length)
{}

SingleSubEdge::point::
point(const point &p0, const point &p1, float t):
  m_edge_length(p0.m_edge_length),
  m_open_contour_length(p0.m_open_contour_length),
  m_closed_contour_length(p0.m_closed_contour_length)
{
  float s(1.0f - t);
  m_pt = s * p0.m_pt + t * p1.m_pt;
  m_distance_from_edge_start = s * p0.m_distance_from_edge_start + t * p1.m_distance_from_edge_start;
  m_distance_from_contour_start = s * p0.m_distance_from_contour_start + t * p1.m_distance_from_contour_start;
}

////////////////////////////////////////
// SingleSubEdge methods
void
SingleSubEdge::
split_sub_edge(int splitting_coordinate,
               fastuidraw::vecN<SingleSubEdge, 2> &out_edges,
               float split_value) const
{
  float t, v0, v1;
  point p;

  v0 = m_pt0.m_pt[splitting_coordinate];
  v1 = m_pt1.m_pt[splitting_coordinate];
  t = (split_value - v0) / (v1 - v0);

  p = point(m_pt0, m_pt1, t);

  out_edges[0].m_pt0 = m_pt0;
  out_edges[0].m_pt1 = p;

  out_edges[1].m_pt0 = p;
  out_edges[1].m_pt1 = m_pt1;

  for(unsigned int i = 0; i < 2; ++i)
    {
      out_edges[i].m_normal = m_normal;
      out_edges[i].m_delta = out_edges[i].m_pt1.m_pt - out_edges[i].m_pt0.m_pt;
      out_edges[i].m_of_closing_edge = m_of_closing_edge;
      out_edges[i].m_has_bevel = m_has_bevel && (i == 0);
      out_edges[i].m_bevel_lambda = (i == 0) ? m_bevel_lambda : 0.0f;
      out_edges[i].m_bevel_normal = (i == 0) ? m_bevel_normal : fastuidraw::vec2(0.0f, 0.0f);
    }
}

//////////////////////////////////////////////
// SubEdgeCullingHierarchy methods
const float SubEdgeCullingHierarchy::sm_mag_tol = 0.000001f;

SubEdgeCullingHierarchy*
SubEdgeCullingHierarchy::
create(const fastuidraw::TessellatedPath &P, ContourData &path_data)
{
  std::vector<SingleSubEdge> data;
  std::vector<JoinSource> joins;
  std::vector<CapSource> caps;
  fastuidraw::BoundingBox<float> bx;
  unsigned int num_non_closing_edges, num_non_closing_joins;
  SubEdgeCullingHierarchy *return_value;

  create_lists(P, path_data,
               data, num_non_closing_edges, bx,
               joins, num_non_closing_joins, caps);
  return_value =  FASTUIDRAWnew SubEdgeCullingHierarchy(bx, data, num_non_closing_edges,
                                                        joins, num_non_closing_joins, caps);
  return return_value;
}

void
SubEdgeCullingHierarchy::
create_lists(const fastuidraw::TessellatedPath &P, ContourData &path_data,
             std::vector<SingleSubEdge> &data, unsigned int &num_non_closing_edges,
             fastuidraw::BoundingBox<float> &bx,
             std::vector<JoinSource> &joins, unsigned int &num_non_closing_joins,
             std::vector<CapSource> &caps)
{
  path_data.m_per_contour_data.resize(P.number_contours());

  /* place data of closing sub-edges at the tail of the lists.
   */
  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      path_data.m_per_contour_data[o].m_edge_data_store.resize(P.number_edges(o));
      path_data.m_per_contour_data[o].m_start_contour_pt = P.unclosed_contour_point_data(o).front();
      path_data.m_per_contour_data[o].m_end_contour_pt = P.unclosed_contour_point_data(o).back();
      for(unsigned int e = 0, ende = P.number_edges(o); e + 1 < ende; ++e)
        {
          process_edge(P, path_data, o, e, data, bx);

          if (e + 2 != ende)
            {
              JoinSource J;
              J.m_contour = o;
              J.m_edge_going_into_join = e;
              J.m_of_closing_edge = false;
              J.m_pt = path_data.m_per_contour_data[o].m_edge_data_store[e].m_end_pt.m_p;
              joins.push_back(J);
            }
        }
    }

  num_non_closing_edges = data.size();
  num_non_closing_joins = joins.size();

  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      if (P.number_edges(o) > 0)
        {
          process_edge(P, path_data, o, P.number_edges(o) - 1, data, bx);

          if (P.number_edges(o) >= 2)
            {
              JoinSource J;
              unsigned int e;

              e = P.number_edges(o) - 2;
              J.m_contour = o;
              J.m_edge_going_into_join = e;
              J.m_of_closing_edge = true;
              J.m_pt = path_data.m_per_contour_data[o].m_edge_data_store[e].m_end_pt.m_p;
              joins.push_back(J);

              e = P.number_edges(o) - 1;
              J.m_contour = o;
              J.m_edge_going_into_join = e;
              J.m_of_closing_edge = true;
              J.m_pt = path_data.m_per_contour_data[o].m_edge_data_store[e].m_end_pt.m_p;
              joins.push_back(J);
            }
        }

      CapSource C0, C1;
      C0.m_contour = o;
      C0.m_is_start_cap = true;
      C0.m_pt = P.unclosed_contour_point_data(o).front().m_p;
      caps.push_back(C0);

      C1.m_contour = o;
      C1.m_is_start_cap = false;
      C1.m_pt = P.unclosed_contour_point_data(o).back().m_p;
      caps.push_back(C1);
    }
}

void
SubEdgeCullingHierarchy::
process_edge(const fastuidraw::TessellatedPath &P, ContourData &path_data,
             unsigned int contour, unsigned int edge,
             std::vector<SingleSubEdge> &dst, fastuidraw::BoundingBox<float> &bx)
{
  fastuidraw::range_type<unsigned int> R;
  fastuidraw::c_array<const fastuidraw::TessellatedPath::point> src_pts(P.point_data());
  fastuidraw::vec2 normal(1.0f, 0.0f), last_normal(1.0f, 0.0f);
  bool is_closing_edge;

  is_closing_edge = (edge + 1 == P.number_edges(contour));
  R = P.edge_range(contour, edge);
  FASTUIDRAWassert(R.m_end > R.m_begin);

  for(unsigned int i = R.m_begin; i + 1 < R.m_end; ++i)
    {
      /* for the edge connecting src_pts[i] to src_pts[i+1]
       */
      fastuidraw::vec2 delta;
      float delta_magnitude;
      SingleSubEdge sub_edge;

      delta = src_pts[i + 1].m_p - src_pts[i].m_p;
      delta_magnitude = delta.magnitude();

      if (delta.magnitude() >= sm_mag_tol)
        {
          normal = fastuidraw::vec2(-delta.y(), delta.x()) / delta_magnitude;
        }

      if (i == R.m_begin)
        {
          sub_edge.m_bevel_lambda = 0.0f;
          sub_edge.m_has_bevel = false;
          sub_edge.m_bevel_normal = fastuidraw::vec2(0.0f, 0.0f);
          path_data.m_per_contour_data[contour].write_edge_data(edge).m_begin_normal = normal;
          path_data.m_per_contour_data[contour].write_edge_data(edge).m_start_pt = src_pts[i];
          if (edge == 0)
            {
              path_data.m_per_contour_data[contour].m_begin_cap_normal = normal;
            }
        }
      else
        {
          sub_edge.m_bevel_lambda = CommonJoinData::compute_lambda(last_normal, normal);
          sub_edge.m_has_bevel = true;
          sub_edge.m_bevel_normal = last_normal;
        }

      sub_edge.m_of_closing_edge = is_closing_edge;
      sub_edge.m_pt0 = SingleSubEdge::point(src_pts[i]);
      sub_edge.m_pt1 = SingleSubEdge::point(src_pts[i + 1]);
      sub_edge.m_normal = normal;
      sub_edge.m_delta = delta;

      dst.push_back(sub_edge);
      bx.union_point(src_pts[i].m_p);
      bx.union_point(src_pts[i + 1].m_p);

      last_normal = normal;
    }

  if (R.m_begin + 1 >= R.m_end)
    {
      path_data.m_per_contour_data[contour].write_edge_data(edge).m_begin_normal = normal;
      path_data.m_per_contour_data[contour].write_edge_data(edge).m_start_pt = src_pts[R.m_begin];
      if (edge == 0)
        {
          path_data.m_per_contour_data[contour].m_begin_cap_normal = normal;
        }
    }

  path_data.m_per_contour_data[contour].write_edge_data(edge).m_end_normal = normal;
  path_data.m_per_contour_data[contour].write_edge_data(edge).m_end_pt = src_pts[R.m_end - 1];
  if (edge + 2 == P.number_edges(contour))
    {
      path_data.m_per_contour_data[contour].m_end_cap_normal = normal;
    }
}

template<typename T>
void
SubEdgeCullingHierarchy::
check_closing_at_end(const std::vector<T> &data, unsigned int num_non_closing)
{
  FASTUIDRAWassert(num_non_closing <= data.size());
  #ifdef FASTUIDRAW_DEBUG
    {
      for(unsigned int i = 0; i < num_non_closing; ++i)
        {
          FASTUIDRAWassert(!data[i].m_of_closing_edge);
        }
      for(unsigned int i = num_non_closing, endi = data.size(); i < endi; ++i)
        {
          FASTUIDRAWassert(data[i].m_of_closing_edge);
        }
    }
  #else
    {
      FASTUIDRAWunused(data);
      FASTUIDRAWunused(num_non_closing);
    }
  #endif
}

SubEdgeCullingHierarchy::
SubEdgeCullingHierarchy(const fastuidraw::BoundingBox<float> &start_box,
                        std::vector<SingleSubEdge> &edges, unsigned int num_non_closing_edges,
                        std::vector<JoinSource> &joins, unsigned int num_non_closing_joins,
                        std::vector<CapSource> &caps):
  m_children(nullptr, nullptr),
  m_bb(start_box)
{
  int c;
  float mid_point;

  FASTUIDRAWassert(!start_box.empty());
  check_closing_at_end(edges, num_non_closing_edges);
  check_closing_at_end(joins, num_non_closing_joins);

  c = choose_splitting_coordinate(start_box, fastuidraw::make_c_array(edges), mid_point);
  if (c != -1)
    {
      fastuidraw::vecN<fastuidraw::BoundingBox<float>, 2> child_boxes;
      fastuidraw::vecN<std::vector<SingleSubEdge>, 2> child_sub_edges;
      fastuidraw::vecN<std::vector<JoinSource>, 2> child_joins;
      fastuidraw::vecN<std::vector<CapSource>, 2> child_caps;
      fastuidraw::vecN<unsigned int, 2> child_num_non_closing_edges(0, 0);
      fastuidraw::vecN<unsigned int, 2> child_num_non_closing_joins(0, 0);

      for(const JoinSource &J : joins)
        {
          bool s;
          s = J.m_pt[c] < mid_point;
          child_joins[s].push_back(J);
          if (!J.m_of_closing_edge)
            {
              ++child_num_non_closing_joins[s];
            }
        }

      for(const CapSource &C : caps)
        {
          bool s;
          s = C.m_pt[c] < mid_point;
          child_caps[s].push_back(C);
        }

      for(const SingleSubEdge &sub_edge : edges)
        {
          bool sA, sB;

          sA = (sub_edge.m_pt0.m_pt[c] < mid_point);
          sB = (sub_edge.m_pt1.m_pt[c] < mid_point);
          if (sA == sB)
            {
              child_boxes[sA].union_point(sub_edge.m_pt0.m_pt);
              child_boxes[sA].union_point(sub_edge.m_pt1.m_pt);
              child_sub_edges[sA].push_back(sub_edge);
              if (!sub_edge.m_of_closing_edge)
                {
                  ++child_num_non_closing_edges[sA];
                }
            }
          else
            {
              fastuidraw::vecN<SingleSubEdge, 2> split;

              sub_edge.split_sub_edge(c, split, mid_point);
              for(int i = 0; i < 2; ++i)
                {
                  child_boxes[i].union_point(split[i].m_pt0.m_pt);
                  child_boxes[i].union_point(split[i].m_pt1.m_pt);
                  child_sub_edges[i].push_back(split[i]);
                  if (!split[i].m_of_closing_edge)
                    {
                      ++child_num_non_closing_edges[i];
                    }
                }
            }
        }

      m_children[0] = FASTUIDRAWnew SubEdgeCullingHierarchy(child_boxes[0],
                                                            child_sub_edges[0], child_num_non_closing_edges[0],
                                                            child_joins[0], child_num_non_closing_joins[0],
                                                            child_caps[0]);
      m_children[1] = FASTUIDRAWnew SubEdgeCullingHierarchy(child_boxes[1],
                                                            child_sub_edges[1], child_num_non_closing_edges[1],
                                                            child_joins[1], child_num_non_closing_joins[1],
                                                            child_caps[1]);
    }
  else
    {
      /* steal the data */
      m_caps.swap(caps);
      m_sub_edges.init(edges, num_non_closing_edges);
      m_joins.init(joins, num_non_closing_joins);
    }
}

SubEdgeCullingHierarchy::
~SubEdgeCullingHierarchy(void)
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
SubEdgeCullingHierarchy::
choose_splitting_coordinate(const fastuidraw::BoundingBox<float> &start_box,
                            fastuidraw::c_array<const SingleSubEdge> data,
                            float &split_value)
{
  if (data.size() < splitting_threshhold)
    {
      return -1;
    }

  fastuidraw::vecN<float, 2> split_values;
  fastuidraw::vecN<unsigned int, 2> split_counters(0, 0);
  fastuidraw::vecN<fastuidraw::uvec2, 2> child_counters(fastuidraw::uvec2(0, 0),
                                                        fastuidraw::uvec2(0, 0));
  std::vector<float> qs;
  qs.reserve(data.size());

  for(int c = 0; c < 2; ++c)
    {
      qs.clear();
      qs.push_back(start_box.min_point()[c]);
      for(const SingleSubEdge &sub_edge : data)
        {
          qs.push_back(sub_edge.m_pt0.m_pt[c]);
        }
      qs.push_back(start_box.max_point()[c]);

      std::sort(qs.begin(), qs.end());
      split_values[c] = qs[qs.size() / 2];

      for(const SingleSubEdge &sub_edge : data)
        {
          bool sA, sB;
          sA = (sub_edge.m_pt0.m_pt[c] < split_values[c]);
          sB = (sub_edge.m_pt1.m_pt[c] < split_values[c]);
          if (sA != sB)
            {
              ++split_counters[c];
              ++child_counters[c][0];
              ++child_counters[c][1];
            }
          else
            {
              ++child_counters[c][sA];
            }
        }
    }

  int canidate;
  canidate = (split_counters[0] < split_counters[1]) ? 0 : 1;

  /* we require that both sides will have fewer edges
     than the parent size.
   */
  if (child_counters[canidate][0] < data.size() && child_counters[canidate][1] < data.size())
    {
      split_value = split_values[canidate];
      return canidate;
    }
  else
    {
      return -1;
    }
}

////////////////////////////////////////////
// StrokedPathSubset methods
StrokedPathSubset::
~StrokedPathSubset()
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

StrokedPathSubset*
StrokedPathSubset::
create(const SubEdgeCullingHierarchy *src,
       CreationValues &out_values,
       JoinOrdering &join_ordering,
       CapOrdering &cap_ordering)
{
  StrokedPathSubset *return_value;
  PostProcessVariables vars;

  return_value = FASTUIDRAWnew StrokedPathSubset(out_values, join_ordering, cap_ordering, src);
  return_value->post_process(vars, out_values, join_ordering, cap_ordering);
  return return_value;
}

void
StrokedPathSubset::
post_process(PostProcessVariables &variables,
             const CreationValues &constants,
             JoinOrdering &join_ordering,
             CapOrdering &cap_ordering)
{
  /* We want the depth to go in the reverse order as the
     draw order. The Draw order is child(0), child(1)
     Thus, we first handle depth child(1) and then child(0).
   */
  m_non_closing_edges.m_depth_range.m_begin = variables.m_edge_depth;
  m_closing_edges.m_depth_range.m_begin = variables.m_closing_edge_depth;

  m_non_closing_joins.m_depth_range.m_begin = variables.m_join_depth;
  m_closing_joins.m_depth_range.m_begin = variables.m_closing_join_depth;

  m_caps.m_depth_range.m_begin = variables.m_cap_depth;

  if (have_children())
    {
      FASTUIDRAWassert(m_children[0] != nullptr);
      FASTUIDRAWassert(m_children[1] != nullptr);
      FASTUIDRAWassert(m_non_closing_edges.m_src.empty());
      FASTUIDRAWassert(m_closing_edges.m_src.empty());

      m_children[1]->post_process(variables, constants, join_ordering, cap_ordering);
      m_children[0]->post_process(variables, constants, join_ordering, cap_ordering);
    }
  else
    {
      FASTUIDRAWassert(m_children[0] == nullptr);
      FASTUIDRAWassert(m_children[1] == nullptr);

      variables.m_edge_depth += m_non_closing_edges.m_src.size();
      variables.m_closing_edge_depth += m_closing_edges.m_src.size();

      join_ordering.post_process_non_closing(m_non_closing_joins.m_elements,
                                             variables.m_join_depth);

      join_ordering.post_process_closing(constants.m_non_closing_join_chunk_cnt,
                                         m_closing_joins.m_elements,
                                         variables.m_closing_join_depth);

      cap_ordering.post_process(m_caps.m_elements, variables.m_cap_depth);
    }
  m_non_closing_edges.m_depth_range.m_end = variables.m_edge_depth;
  m_closing_edges.m_depth_range.m_end = variables.m_closing_edge_depth;

  m_non_closing_joins.m_depth_range.m_end = variables.m_join_depth;
  m_closing_joins.m_depth_range.m_end = variables.m_closing_join_depth;

  m_caps.m_depth_range.m_end = variables.m_cap_depth;

  FASTUIDRAWassert(m_non_closing_joins.m_elements.difference() == m_non_closing_joins.m_depth_range.difference());
  FASTUIDRAWassert(m_closing_joins.m_elements.difference() == m_closing_joins.m_depth_range.difference());

  /* make the closing edge chunks start after the
     non-closing edge chunks.
   */
  m_closing_edges.m_chunk += constants.m_non_closing_edge_chunk_cnt;

  /* make vertices and indices of closing edges appear
     after those of non-closing edges
   */
  m_closing_edges.m_vertex_data_range += constants.m_non_closing_edge_vertex_cnt;
  m_closing_edges.m_index_data_range += constants.m_non_closing_edge_index_cnt;

  /* the joins are ordered so that the joins of the non-closing
     edges appear first.
   */
  m_closing_joins.m_elements += join_ordering.non_closing_edge().size();

  /* make the chunks of closing edges come AFTER
     chunks of non-closing edge
   */
  m_closing_joins.m_chunk += constants.m_non_closing_join_chunk_cnt;

  m_empty_subset = !m_non_closing_edges.non_empty()
    && !m_non_closing_joins.non_empty()
    && !m_closing_edges.non_empty()
    && !m_closing_joins.non_empty();
}

StrokedPathSubset::
StrokedPathSubset(CreationValues &out_values,
                  JoinOrdering &join_ordering, CapOrdering &cap_ordering,
                  const SubEdgeCullingHierarchy *src):
  m_children(nullptr, nullptr),
  m_bb(src->bounding_box())
{
  /* Draw order is:
       child(0)
       child(1)
   */
  m_non_closing_edges.m_vertex_data_range.m_begin = out_values.m_non_closing_edge_vertex_cnt;
  m_non_closing_edges.m_index_data_range.m_begin = out_values.m_non_closing_edge_index_cnt;

  m_closing_edges.m_vertex_data_range.m_begin = out_values.m_closing_edge_vertex_cnt;
  m_closing_edges.m_index_data_range.m_begin = out_values.m_closing_edge_index_cnt;

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
          m_children[i] = FASTUIDRAWnew StrokedPathSubset(out_values, join_ordering, cap_ordering, src->child(i));
        }
    }
  else
    {
      m_non_closing_edges.m_src = src->non_closing_edges();
      m_closing_edges.m_src = src->closing_edges();

      increment_vertices_indices(m_non_closing_edges.m_src,
                                 out_values.m_non_closing_edge_vertex_cnt,
                                 out_values.m_non_closing_edge_index_cnt);
      increment_vertices_indices(m_closing_edges.m_src,
                                 out_values.m_closing_edge_vertex_cnt,
                                 out_values.m_closing_edge_index_cnt);

      for(const JoinSource &J : src->non_closing_joins())
        {
          join_ordering.add_join(J, out_values.m_non_closing_join_chunk_cnt);
        }

      for(const JoinSource &J : src->closing_joins())
        {
          join_ordering.add_join(J, out_values.m_closing_join_chunk_cnt);
        }

      for(const CapSource &C : src->caps())
        {
          cap_ordering.add_cap(C, out_values.m_cap_chunk_cnt);
        }
    }

  m_non_closing_edges.m_vertex_data_range.m_end = out_values.m_non_closing_edge_vertex_cnt;
  m_non_closing_edges.m_index_data_range.m_end = out_values.m_non_closing_edge_index_cnt;
  m_closing_edges.m_vertex_data_range.m_end = out_values.m_closing_edge_vertex_cnt;
  m_closing_edges.m_index_data_range.m_end = out_values.m_closing_edge_index_cnt;

  m_non_closing_edges.m_chunk = out_values.m_non_closing_edge_chunk_cnt;
  m_closing_edges.m_chunk = out_values.m_closing_edge_chunk_cnt;

  m_non_closing_joins.m_elements.m_end = join_ordering.non_closing_edge().size();
  m_closing_joins.m_elements.m_end = join_ordering.closing_edge().size();
  m_caps.m_elements.m_end = cap_ordering.caps().size();

  m_non_closing_joins.m_chunk = out_values.m_non_closing_join_chunk_cnt;
  m_closing_joins.m_chunk = out_values.m_closing_join_chunk_cnt;
  m_caps.m_chunk = out_values.m_cap_chunk_cnt;

  ++out_values.m_non_closing_edge_chunk_cnt;
  ++out_values.m_closing_edge_chunk_cnt;
  ++out_values.m_non_closing_join_chunk_cnt;
  ++out_values.m_closing_join_chunk_cnt;
  ++out_values.m_cap_chunk_cnt;
}

void
StrokedPathSubset::
increment_vertices_indices(fastuidraw::c_array<const SingleSubEdge> src,
                           unsigned int &vertex_cnt,
                           unsigned int &index_cnt)
{
  for(const SingleSubEdge &v : src)
    {
      if (v.m_has_bevel)
        {
          vertex_cnt += 3;
          index_cnt += 3;
        }

      vertex_cnt += points_per_segment;
      index_cnt += indices_per_segment_without_bevel;
    }
}

void
StrokedPathSubset::
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
         local coordinates.
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
StrokedPathSubset::
compute_chunks_take_all(bool include_closing_edge,
                        unsigned int max_attribute_cnt,
                        unsigned int max_index_cnt,
                        ChunkSetPrivate &dst)
{
  if (m_empty_subset)
    {
      return;
    }

  if (m_non_closing_edges.chunk_fits(max_attribute_cnt, max_index_cnt)
     && (!include_closing_edge || m_closing_edges.chunk_fits(max_attribute_cnt, max_index_cnt)))
    {
      dst.add_edge_chunk(m_non_closing_edges);
      dst.add_join_chunk(m_non_closing_joins);

      if (include_closing_edge)
        {
          dst.add_edge_chunk(m_closing_edges);
          dst.add_join_chunk(m_closing_joins);
        }
      else
        {
          dst.add_cap_chunk(m_caps);
        }
    }
  else if (have_children())
    {
      FASTUIDRAWassert(m_children[0] != nullptr);
      FASTUIDRAWassert(m_children[1] != nullptr);
      m_children[0]->compute_chunks_take_all(include_closing_edge, max_attribute_cnt, max_index_cnt, dst);
      m_children[1]->compute_chunks_take_all(include_closing_edge, max_attribute_cnt, max_index_cnt, dst);
    }
  else
    {
      FASTUIDRAWassert(!"Unable to fit stroked path chunk into max_attribute and max_index count limits!");
    }
}

void
StrokedPathSubset::
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

  /* clip the bounding box of this StrokedPathSubset
   */
  vecN<vec2, 4> bb;
  bool unclipped;

  m_bb.inflated_polygon(bb, item_space_additional_room);
  unclipped = clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                  bb, scratch.m_clipped_rect,
                                  scratch.m_clip_scratch_floats,
                                  scratch.m_clip_scratch_vec2s);
  //completely unclipped.
  if (unclipped)
    {
      compute_chunks_take_all(include_closing_edge, max_attribute_cnt, max_index_cnt, dst);
      return;
    }

  //completely clipped
  if (scratch.m_clipped_rect.empty())
    {
      return;
    }

  if (have_children())
    {
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
  else
    {
      FASTUIDRAWassert(m_non_closing_edges.chunk_fits(max_attribute_cnt, max_index_cnt));
      dst.add_edge_chunk(m_non_closing_edges);
      dst.add_join_chunk(m_non_closing_joins);

      if (include_closing_edge)
        {
          FASTUIDRAWassert(m_closing_edges.chunk_fits(max_attribute_cnt, max_index_cnt));
          dst.add_edge_chunk(m_closing_edges);
          dst.add_join_chunk(m_closing_joins);
        }
      else
        {
          dst.add_cap_chunk(m_caps);
        }
    }
}

////////////////////////////////////////
// EdgeAttributeFiller methods
EdgeAttributeFiller::
EdgeAttributeFiller(const StrokedPathSubset *src,
                    const fastuidraw::TessellatedPath &P,
                    const StrokedPathSubset::CreationValues &cnts):
  m_src(src),
  m_P(P),
  m_total_vertex_cnt(cnts.m_non_closing_edge_vertex_cnt + cnts.m_closing_edge_vertex_cnt),
  m_total_index_cnt(cnts.m_non_closing_edge_index_cnt + cnts.m_closing_edge_index_cnt),
  m_total_number_chunks(cnts.m_non_closing_edge_chunk_cnt + cnts.m_closing_edge_chunk_cnt)
{
}

void
EdgeAttributeFiller::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_ranges) const
{
  num_attribute_chunks = num_index_chunks = number_z_ranges = m_total_number_chunks;
  num_attributes = m_total_vertex_cnt;
  num_indices = m_total_index_cnt;
}

void
EdgeAttributeFiller::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  fill_data_worker(m_src, attribute_data, index_data,
                   attribute_chunks, index_chunks, zranges, index_adjusts);
}

void
EdgeAttributeFiller::
fill_data_worker(const StrokedPathSubset *e,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                 fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
                 fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
                 fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
                 fastuidraw::c_array<int> index_adjusts) const
{
  if (e->have_children())
    {
      FASTUIDRAWassert(e->child(0) != nullptr);
      FASTUIDRAWassert(e->child(1) != nullptr);
      FASTUIDRAWassert(e->non_closing_edges().m_src.empty());
      FASTUIDRAWassert(e->closing_edges().m_src.empty());

      fill_data_worker(e->child(0), attribute_data, index_data,
                       attribute_chunks, index_chunks, zranges, index_adjusts);

      fill_data_worker(e->child(1), attribute_data, index_data,
                       attribute_chunks, index_chunks, zranges, index_adjusts);
    }

  build_chunk(e->non_closing_edges(), attribute_data, index_data,
              attribute_chunks, index_chunks, zranges, index_adjusts);

  build_chunk(e->closing_edges(), attribute_data, index_data,
              attribute_chunks, index_chunks, zranges, index_adjusts);
}

void
EdgeAttributeFiller::
build_chunk(const EdgeRanges &edge,
            fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
            fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
            fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterAttribute> > attribute_chunks,
            fastuidraw::c_array<fastuidraw::c_array<const fastuidraw::PainterIndex> > index_chunks,
            fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
            fastuidraw::c_array<int> index_adjusts) const
{
  fastuidraw::c_array<fastuidraw::PainterAttribute> ad;
  fastuidraw::c_array<fastuidraw::PainterIndex> id;
  unsigned int chunk;

  ad = attribute_data.sub_array(edge.m_vertex_data_range);
  id = index_data.sub_array(edge.m_index_data_range);
  chunk = edge.m_chunk;

  attribute_chunks[chunk] = ad;
  index_chunks[chunk] = id;
  index_adjusts[chunk] = -int(edge.m_vertex_data_range.m_begin);
  zranges[chunk] = fastuidraw::range_type<int>(edge.m_depth_range.m_begin,
                                               edge.m_depth_range.m_end);

  if (!edge.m_src.empty())
    {
      /* these elements are drawn AFTER the child elements,
         therefor they need to have a smaller depth
      */
      for(unsigned int k = 0,
            d = edge.m_depth_range.m_end - 1,
            v = edge.m_vertex_data_range.m_begin,
            i = edge.m_index_data_range.m_begin;
          k < edge.m_src.size(); ++k, --d)
        {
          process_sub_edge(edge.m_src[k], d, attribute_data, index_data, v, i);
        }
    }

  #ifdef FASTUIDRAW_DEBUG
    {
      for(unsigned int v = edge.m_vertex_data_range.m_begin;  v < edge.m_vertex_data_range.m_end; ++v)
        {
          fastuidraw::StrokedPath::point P;
          fastuidraw::StrokedPath::point::unpack_point(&P, attribute_data[v]);
          FASTUIDRAWassert(P.depth() >= edge.m_depth_range.m_begin);
          FASTUIDRAWassert(P.depth() < edge.m_depth_range.m_end);
        }
    }
  #endif
}

void
EdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                 unsigned int &vert_offset, unsigned int &index_offset) const
{
  const int boundary_values[3] = { 1, 1, 0 };
  const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
  fastuidraw::vecN<fastuidraw::StrokedPath::point, 6> pts;

  if (sub_edge.m_has_bevel)
    {
      indices[index_offset + 0] = vert_offset + 0;
      indices[index_offset + 1] = vert_offset + 1;
      indices[index_offset + 2] = vert_offset + 2;
      index_offset += 3;

      for(unsigned int k = 0; k < 3; ++k)
        {
          pts[k].m_position = sub_edge.m_pt0.m_pt;
          pts[k].m_distance_from_edge_start = sub_edge.m_pt0.m_distance_from_edge_start;
          pts[k].m_distance_from_contour_start = sub_edge.m_pt0.m_distance_from_contour_start;
          pts[k].m_edge_length = sub_edge.m_pt0.m_edge_length;
          pts[k].m_open_contour_length = sub_edge.m_pt0.m_open_contour_length;
          pts[k].m_closed_contour_length = sub_edge.m_pt0.m_closed_contour_length;
          pts[k].m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
        }

      pts[0].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
      pts[0].m_packed_data = pack_data(0, fastuidraw::StrokedPath::point::offset_start_sub_edge, depth)
        | fastuidraw::StrokedPath::point::bevel_edge_mask;

      pts[1].m_pre_offset = sub_edge.m_bevel_lambda * sub_edge.m_bevel_normal;
      pts[1].m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_start_sub_edge, depth)
        | fastuidraw::StrokedPath::point::bevel_edge_mask;

      pts[2].m_pre_offset = sub_edge.m_bevel_lambda * sub_edge.m_normal;
      pts[2].m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_start_sub_edge, depth)
        | fastuidraw::StrokedPath::point::bevel_edge_mask;

      for(unsigned int i = 0; i < 3; ++i)
        {
          pts[i].fastuidraw::StrokedPath::point::pack_point(&attribute_data[vert_offset + i]);
        }

      vert_offset += 3;
    }

  /* The quad is:
     (p, n, delta,  1),
     (p,-n, delta,  1),
     (p, 0,     0,  0),
     (p_next,  n, -delta, 1),
     (p_next, -n, -delta, 1),
     (p_next,  0, 0)

     Notice that we are encoding if it is
     start or end of edge from the sign of
     m_on_boundary.
  */
  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0.m_pt;
      pts[k].m_distance_from_edge_start = sub_edge.m_pt0.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_pt0.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_pt0.m_edge_length;
      pts[k].m_open_contour_length = sub_edge.m_pt0.m_open_contour_length;
      pts[k].m_closed_contour_length = sub_edge.m_pt0.m_closed_contour_length;
      pts[k].m_pre_offset = normal_sign[k] * sub_edge.m_normal;
      pts[k].m_auxiliary_offset = sub_edge.m_delta;
      pts[k].m_packed_data = pack_data(boundary_values[k],
                                       fastuidraw::StrokedPath::point::offset_start_sub_edge,
                                       depth);

      pts[k + 3].m_position = sub_edge.m_pt1.m_pt;
      pts[k + 3].m_distance_from_edge_start = sub_edge.m_pt1.m_distance_from_edge_start;
      pts[k + 3].m_distance_from_contour_start = sub_edge.m_pt1.m_distance_from_contour_start;
      pts[k + 3].m_edge_length = sub_edge.m_pt1.m_edge_length;
      pts[k + 3].m_open_contour_length = sub_edge.m_pt1.m_open_contour_length;
      pts[k + 3].m_closed_contour_length = sub_edge.m_pt1.m_closed_contour_length;
      pts[k + 3].m_pre_offset = normal_sign[k] * sub_edge.m_normal;
      pts[k + 3].m_auxiliary_offset = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = pack_data(boundary_values[k],
                                           fastuidraw::StrokedPath::point::offset_end_sub_edge,
                                           depth);
    }

  for(unsigned int i = 0; i < 6; ++i)
    {
      pts[i].fastuidraw::StrokedPath::point::pack_point(&attribute_data[vert_offset + i]);
    }

  indices[index_offset + 0] = vert_offset + 0;
  indices[index_offset + 1] = vert_offset + 2;
  indices[index_offset + 2] = vert_offset + 5;
  indices[index_offset + 3] = vert_offset + 0;
  indices[index_offset + 4] = vert_offset + 5;
  indices[index_offset + 5] = vert_offset + 3;

  indices[index_offset + 6] = vert_offset + 2;
  indices[index_offset + 7] = vert_offset + 1;
  indices[index_offset + 8] = vert_offset + 4;
  indices[index_offset + 9] = vert_offset + 2;
  indices[index_offset + 10] = vert_offset + 4;
  indices[index_offset + 11] = vert_offset + 5;

  index_offset += StrokedPathSubset::indices_per_segment_without_bevel;
  vert_offset += StrokedPathSubset::points_per_segment;
}


//////////////////////////////////////////////
// JoinCount methods
JoinCount::
JoinCount(const ContourData &P):
  m_number_close_joins(0),
  m_number_non_close_joins(0)
{
  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      if (P.number_edges(o) >= 2)
        {
          m_number_non_close_joins += P.number_edges(o) - 2;
          m_number_close_joins += 2;
        }
    }
}

////////////////////////////////////////////////
// CommonJoinData methods
CommonJoinData::
CommonJoinData(const fastuidraw::vec2 &p0,
               const fastuidraw::vec2 &n0,
               const fastuidraw::vec2 &p1,
               const fastuidraw::vec2 &n1,
               float distance_from_edge_start,
               float distance_from_contour_start,
               float edge_length,
               float open_contour_length,
               float closed_contour_length):
  m_distance_from_edge_start(distance_from_edge_start),
  m_distance_from_contour_start(distance_from_contour_start),
  m_edge_length(edge_length),
  m_open_contour_length(open_contour_length),
  m_closed_contour_length(closed_contour_length)
{
  /* Explanation:
      We have two curves, a(t) and b(t) with a(1) = b(0)
      The point p0 represents the end of a(t) and the
      point p1 represents the start of b(t).

      When stroking we have four auxiliary curves:
        a0(t) = a(t) + w * a_n(t)
        a1(t) = a(t) - w * a_n(t)
        b0(t) = b(t) + w * b_n(t)
        b1(t) = b(t) - w * b_n(t)
      where
        w = width of stroking
        a_n(t) = J( a'(t) ) / || a'(t) ||
        b_n(t) = J( b'(t) ) / || b'(t) ||
      when
        J(x, y) = (-y, x).

      A Bevel join is a triangle that connects
      consists of p, A and B where p is a(1)=b(0),
      A is one of a0(1) or a1(1) and B is one
      of b0(0) or b1(0). Now if we use a0(1) for
      A then we will use b0(0) for B because
      the normals are generated the same way for
      a(t) and b(t). Then, the questions comes
      down to, do we wish to add or subtract the
      normal. That value is represented by m_lambda.

      Now to figure out m_lambda. Let q0 be a point
      on a(t) before p=a(1). The q0 is given by

        q0 = p - s * m_v0

      and let q1 be a point on b(t) after p=b(0),

        q1 = p + t * m_v1

      where both s, t are positive. Let

        z = (q0+q1) / 2

      the point z is then on the side of the join
      of the acute angle of the join.

      With this in mind, if either of <z-p, m_n0>
      or <z-p, m_n1> is positive then we want
      to add by -w * n rather than  w * n.

      Note that:

      <z-p, m_n1> = 0.5 * < -s * m_v0 + t * m_v1, m_n1 >
                  = -0.5 * s * <m_v0, m_n1> + 0.5 * t * <m_v1, m_n1>
                  = -0.5 * s * <m_v0, m_n1>
                  = -0.5 * s * <m_v0, J(m_v1) >

      and

      <z-p, m_n0> = 0.5 * < -s * m_v0 + t * m_v1, m_n0 >
                  = -0.5 * s * <m_v0, m_n0> + 0.5 * t * <m_v1, m_n0>
                  = 0.5 * t * <m_v1, m_n0>
                  = 0.5 * t * <m_v1, J(m_v0) >
                  = -0.5 * t * <J(m_v1), m_v0>

      (the last line because transpose(J) = -J). Notice
      that the sign of <z-p, m_n1> and the sign of <z-p, m_n0>
      is then the same.

      thus m_lambda is positive if <m_v1, m_n0> is negative.
   */
  m_p0 = p0;
  m_n0 = n0;
  m_v0 = fastuidraw::vec2(m_n0.y(), -m_n0.x());

  m_p1 = p1;
  m_n1 = n1;
  m_v1 = fastuidraw::vec2(m_n1.y(), -m_n1.x());

  m_det = fastuidraw::dot(m_v1, m_n0);
  if (m_det > 0.0f)
    {
      m_lambda = -1.0f;
    }
  else
    {
      m_lambda = 1.0f;
    }
}

float
CommonJoinData::
compute_lambda(const fastuidraw::vec2 &n0, const fastuidraw::vec2 &n1)
{
  fastuidraw::vec2 v1;
  float d;

  v1 = fastuidraw::vec2(n1.y(), -n1.x());
  d = fastuidraw::dot(v1, n0);
  if (d > 0.0f)
    {
      return -1.0f;
    }
  else
    {
      return 1.0f;
    }
}

/////////////////////////////////////////////////
// JoinCreatorBase methods
JoinCreatorBase::
JoinCreatorBase(const PathData &P,
                const StrokedPathSubset *st):
  m_P(P.m_contour_data),
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

  for(const JoinSource &J : m_ordering.non_closing_edge())
    {
      add_join(m_num_joins,
               m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join),
               m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join + 1),
               m_num_verts, m_num_indices);
      ++m_num_joins;
    }

  for(const JoinSource &J : m_ordering.closing_edge())
    {
      add_join(m_num_joins,
               m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join),
               m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join + 1),
               m_num_verts, m_num_indices);
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
          const PerEdgeData &into_join,
          const PerEdgeData &leaving_join,
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

  fill_join_implement(join_id, into_join, leaving_join,
                      pts, depth, indices, vertex_offset, index_offset);

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
     we want the joins draw first to obscure the joins drawn later.
   */
  for(const OrderingEntry<JoinSource> &J : m_ordering.non_closing_edge())
    {
      fill_join(join_id,
                m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join),
                m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join + 1),
                J.m_chunk, J.m_depth,
                attribute_data, index_data,
                vertex_offset, index_offset,
                attribute_chunks, index_chunks,
                zranges, index_adjusts,
                join_vertex_ranges,
                join_index_ranges);
      ++join_id;
    }

  for(const OrderingEntry<JoinSource> &J : m_ordering.closing_edge())
    {
      fill_join(join_id,
                m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join),
                m_P.m_per_contour_data[J.m_contour].edge_data(J.m_edge_going_into_join + 1),
                J.m_chunk, J.m_depth,
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
set_chunks(const StrokedPathSubset *st,
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
// RoundedJoinCreator::PerJoinData methods
RoundedJoinCreator::PerJoinData::
PerJoinData(const fastuidraw::TessellatedPath::point &p0,
            const fastuidraw::TessellatedPath::point &p1,
            const fastuidraw::vec2 &n0_from_stroking,
            const fastuidraw::vec2 &n1_from_stroking,
            float thresh):
  CommonJoinData(p0.m_p, n0_from_stroking, p1.m_p, n1_from_stroking,
                 p0.m_distance_from_edge_start, p0.m_distance_from_contour_start,
                 p0.m_edge_length, p0.m_open_contour_length, p0.m_closed_contour_length)
{
  /* n0z represents the start point of the rounded join in the complex plane
     as if the join was at the origin, n1z represents the end point of the
     rounded join in the complex plane as if the join was at the origin.
  */
  std::complex<float> n0z(m_lambda * m_n0.x(), m_lambda * m_n0.y());
  std::complex<float> n1z(m_lambda * m_n1.x(), m_lambda * m_n1.y());

  /* n1z_times_conj_n0z satisfies:
     n1z = n1z_times_conj_n0z * n0z
     i.e. it represents the arc-movement from n0z to n1z
  */
  std::complex<float> n1z_times_conj_n0z(n1z * std::conj(n0z));

  m_arc_start = n0z;
  m_delta_theta = std::atan2(n1z_times_conj_n0z.imag(), n1z_times_conj_n0z.real());
  m_num_arc_points = fastuidraw::detail::number_segments_for_tessellation(m_delta_theta, thresh);
  m_delta_theta /= static_cast<float>(m_num_arc_points - 1);
}

void
RoundedJoinCreator::PerJoinData::
add_data(unsigned int depth,
         fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
         unsigned int &vertex_offset,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &index_offset) const
{
  unsigned int i, first;
  float theta;
  fastuidraw::StrokedPath::point pt;

  first = vertex_offset;

  pt.m_position = m_p0;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = m_distance_from_edge_start;
  pt.m_distance_from_contour_start = m_distance_from_contour_start;
  pt.m_edge_length = m_edge_length;
  pt.m_open_contour_length = m_open_contour_length;
  pt.m_closed_contour_length = m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = m_p0;
  pt.m_pre_offset = m_lambda * m_n0;
  pt.m_distance_from_edge_start = m_distance_from_edge_start;
  pt.m_distance_from_contour_start = m_distance_from_contour_start;
  pt.m_edge_length = m_edge_length;
  pt.m_open_contour_length = m_open_contour_length;
  pt.m_closed_contour_length = m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float t, c, s;
      std::complex<float> cs_as_complex;

      t = static_cast<float>(i) / static_cast<float>(m_num_arc_points - 1);
      c = std::cos(theta);
      s = std::sin(theta);
      cs_as_complex = std::complex<float>(c, s) * m_arc_start;

      pt.m_position = m_p0;
      pt.m_pre_offset = m_lambda * fastuidraw::vec2(m_n0.x(), m_n1.x());
      pt.m_auxiliary_offset = fastuidraw::vec2(t, cs_as_complex.real());
      pt.m_distance_from_edge_start = m_distance_from_edge_start;
      pt.m_distance_from_contour_start = m_distance_from_contour_start;
      pt.m_edge_length = m_edge_length;
      pt.m_open_contour_length = m_open_contour_length;
      pt.m_closed_contour_length = m_closed_contour_length;
      pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_rounded_join, depth);

      if (m_lambda * m_n0.y() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPath::point::normal0_y_sign_mask;
        }

      if (m_lambda * m_n1.y() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPath::point::normal1_y_sign_mask;
        }

      if (cs_as_complex.imag() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPath::point::sin_sign_mask;
        }
      pt.pack_point(&pts[vertex_offset]);
    }

  pt.m_position = m_p1;
  pt.m_pre_offset = m_lambda * m_n1;
  pt.m_distance_from_edge_start = m_distance_from_edge_start;
  pt.m_distance_from_contour_start = m_distance_from_contour_start;
  pt.m_edge_length = m_edge_length;
  pt.m_open_contour_length = m_open_contour_length;
  pt.m_closed_contour_length = m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// RoundedJoinCreator methods
RoundedJoinCreator::
RoundedJoinCreator(const PathData &P,
                   const StrokedPathSubset *st,
                   float thresh):
  JoinCreatorBase(P, st),
  m_thresh(thresh)
{
  JoinCount J(P.m_contour_data);
  m_per_join_data.reserve(J.m_number_close_joins + J.m_number_non_close_joins);
  post_ctor_initalize();
}

void
RoundedJoinCreator::
add_join(unsigned int join_id,
         const PerEdgeData &into_join,
         const PerEdgeData &leaving_join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  FASTUIDRAWunused(join_id);
  PerJoinData J(into_join.m_end_pt, leaving_join.m_start_pt,
                into_join.m_end_normal, leaving_join.m_begin_normal,
                m_thresh);

  m_per_join_data.push_back(J);

  /* a triangle fan centered at p0 = p1 with
     m_num_arc_points along an edge
  */
  vert_count += (1 + J.m_num_arc_points);
  index_count += 3 * (J.m_num_arc_points - 1);
}


void
RoundedJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PerEdgeData &into_join,
                    const PerEdgeData &leaving_join,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(into_join);
  FASTUIDRAWunused(leaving_join);
  FASTUIDRAWassert(join_id < m_per_join_data.size());
  m_per_join_data[join_id].add_data(depth, pts, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// BevelJoinCreator methods
BevelJoinCreator::
BevelJoinCreator(const PathData &P,
                 const StrokedPathSubset *st):
  JoinCreatorBase(P, st)
{
  post_ctor_initalize();
}

void
BevelJoinCreator::
add_join(unsigned int join_id,
         const PerEdgeData &into_join,
         const PerEdgeData &leaving_join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  FASTUIDRAWunused(join_id);

  /* one triangle per bevel join
   */
  vert_count += 3;
  index_count += 3;

  m_n0.push_back(into_join.m_end_normal);
  m_n1.push_back(leaving_join.m_begin_normal);
}

void
BevelJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PerEdgeData &into_join,
                    const PerEdgeData &leaving_join,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  const fastuidraw::TessellatedPath::point &prev_pt(into_join.m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(leaving_join.m_start_pt);
  fastuidraw::StrokedPath::point pt;

  CommonJoinData J(prev_pt.m_p, m_n0[join_id],
                   next_pt.m_p, m_n1[join_id],
                   prev_pt.m_distance_from_edge_start,
                   prev_pt.m_distance_from_contour_start,
                   //using p0 to decide the edge length, as
                   //we think of the join as ending an edge.
                   prev_pt.m_edge_length,
                   prev_pt.m_open_contour_length,
                   prev_pt.m_closed_contour_length);

  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_lambda * J.m_n0;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 0]);

  pt.m_position = J.m_p0;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 1]);

  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_lambda * J.m_n1;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 2]);

  add_triangle_fan(vertex_offset, vertex_offset + 3, indices, index_offset);

  vertex_offset += 3;
}

///////////////////////////////////////////////////
// MiterClipJoinCreator methods
MiterClipJoinCreator::
MiterClipJoinCreator(const PathData &P,
                     const StrokedPathSubset *st):
  JoinCreatorBase(P, st)
{
  post_ctor_initalize();
}

void
MiterClipJoinCreator::
add_join(unsigned int join_id,
         const PerEdgeData &into_join,
         const PerEdgeData &leaving_join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  FASTUIDRAWunused(join_id);
  /* Each join is a triangle fan from 5 points
     (thus 3 triangles, which is 9 indices)
   */
  vert_count += 5;
  index_count += 9;

  m_n0.push_back(into_join.m_end_normal);
  m_n1.push_back(leaving_join.m_begin_normal);
}


void
MiterClipJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PerEdgeData &into_join,
                    const PerEdgeData &leaving_join,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join_id);

  const fastuidraw::TessellatedPath::point &prev_pt(into_join.m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(leaving_join.m_start_pt);
  fastuidraw::StrokedPath::point pt;
  unsigned int first;

  CommonJoinData J(prev_pt.m_p, m_n0[join_id],
                   next_pt.m_p, m_n1[join_id],
                   prev_pt.m_distance_from_edge_start,
                   prev_pt.m_distance_from_contour_start,
                   //using p0 to decide the edge length, as
                   //we think of the join as ending an edge.
                   prev_pt.m_edge_length,
                   prev_pt.m_open_contour_length,
                   prev_pt.m_closed_contour_length);

  /* The miter point is given by where the two boundary
     curves intersect. The two curves are given by:

     a(t) = J.m_p0 + stroke_width * J.m_lamba * J.m_n0 + t * J.m_v0
     b(s) = J.m_p1 + stroke_width * J.m_lamba * J.m_n1 - s * J.m_v1

    With J.m_p0 is the same value as J.m_p1, the location
    of the join.

    We need to solve a(t) = b(s) and compute that location.
    Linear algebra gives us that:

    t = - stroke_width * J.m_lamba * r
    s = - stroke_width * J.m_lamba * r
     where
    r = (<J.m_v1, J.m_v0> - 1) / <J.m_v0, J.m_n1>

    thus

    a(t) = J.m_p0 + stroke_width * ( J.m_lamba * J.m_n0 -  r * J.m_lamba * J.m_v0)
         = b(s)
         = J.m_p1 + stroke_width * ( J.m_lamba * J.m_n1 +  r * J.m_lamba * J.m_v1)
   */

  first = vertex_offset;

  // join center point.
  pt.m_position = J.m_p0;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve into join
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_lambda * J.m_n0;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point A
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_n0;
  pt.m_auxiliary_offset = J.m_n1;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_miter_clip_join, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point B
  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_n1;
  pt.m_auxiliary_offset = J.m_n0;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_miter_clip_join_lambda_negated, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve out from join
  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_lambda * J.m_n1;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

////////////////////////////////////
// MiterJoinCreator methods
template<enum fastuidraw::StrokedPath::point::offset_type_t tp>
MiterJoinCreator<tp>::
MiterJoinCreator(const PathData &P,
                 const StrokedPathSubset *st):
  JoinCreatorBase(P, st)
{
  post_ctor_initalize();
}

template<enum fastuidraw::StrokedPath::point::offset_type_t tp>
void
MiterJoinCreator<tp>::
add_join(unsigned int join_id,
         const PerEdgeData &into_join,
         const PerEdgeData &leaving_join,
         unsigned int &vert_count, unsigned int &index_count) const
{
  /* Each join is a triangle fan from 4 points
     (thus 2 triangles, which is 6 indices)
   */
  FASTUIDRAWunused(join_id);

  vert_count += 4;
  index_count += 6;

  m_n0.push_back(into_join.m_end_normal);
  m_n1.push_back(leaving_join.m_begin_normal);
}

template<enum fastuidraw::StrokedPath::point::offset_type_t tp>
void
MiterJoinCreator<tp>::
fill_join_implement(unsigned int join_id,
                    const PerEdgeData &into_join,
                    const PerEdgeData &leaving_join,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join_id);

  const fastuidraw::TessellatedPath::point &prev_pt(into_join.m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(leaving_join.m_start_pt);
  fastuidraw::StrokedPath::point pt;
  unsigned int first;

  CommonJoinData J(prev_pt.m_p, m_n0[join_id],
                   next_pt.m_p, m_n1[join_id],
                   prev_pt.m_distance_from_edge_start,
                   prev_pt.m_distance_from_contour_start,
                   //using p0 to decide the edge length, as
                   //we think of the join as ending an edge.
                   prev_pt.m_edge_length,
                   prev_pt.m_open_contour_length,
                   prev_pt.m_closed_contour_length);

  /* The miter point is given by where the two boundary
     curves intersect. The two curves are given by:

     a(t) = J.m_p0 + stroke_width * J.m_lamba * J.m_n0 + t * J.m_v0
     b(s) = J.m_p1 + stroke_width * J.m_lamba * J.m_n1 - s * J.m_v1

    With J.m_p0 is the same value as J.m_p1, the location
    of the join.

    We need to solve a(t) = b(s) and compute that location.
    Linear algebra gives us that:

    t = - stroke_width * J.m_lamba * r
    s = - stroke_width * J.m_lamba * r
     where
    r = (<J.m_v1, J.m_v0> - 1) / <J.m_v0, J.m_n1>

    thus

    a(t) = J.m_p0 + stroke_width * ( J.m_lamba * J.m_n0 -  r * J.m_lamba * J.m_v0)
         = b(s)
         = J.m_p1 + stroke_width * ( J.m_lamba * J.m_n1 +  r * J.m_lamba * J.m_v1)
   */

  first = vertex_offset;

  // join center point.
  pt.m_position = J.m_p0;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve into join
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_lambda * J.m_n0;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_n0;
  pt.m_auxiliary_offset = J.m_n1;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, tp, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve out from join
  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_lambda * J.m_n1;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////
// CapCreatorBase methods
CapCreatorBase::
CapCreatorBase(const PathData &P,
               const StrokedPathSubset *st,
               PointIndexCapSize sz):
  m_P(P.m_contour_data),
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

  for(const OrderingEntry<CapSource> C : m_ordering.caps())
    {
      unsigned int v(vertex_offset), i(index_offset);
      const fastuidraw::vec2 *normal;
      const fastuidraw::TessellatedPath::point *pt;

      normal = C.m_is_start_cap ?
        &m_P.m_per_contour_data[C.m_contour].m_begin_cap_normal:
        &m_P.m_per_contour_data[C.m_contour].m_end_cap_normal;

      pt = C.m_is_start_cap ?
        &m_P.m_per_contour_data[C.m_contour].m_start_contour_pt:
        &m_P.m_per_contour_data[C.m_contour].m_end_contour_pt;

      add_cap(*normal, C.m_is_start_cap, C.m_depth, *pt,
              attribute_data, index_data, vertex_offset, index_offset);

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
set_chunks(const StrokedPathSubset *st,
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
                  const StrokedPathSubset *st,
                  float thresh):
  CapCreatorBase(P, st, compute_size(P.m_contour_data, thresh))
{
  m_num_arc_points_per_cap = fastuidraw::detail::number_segments_for_tessellation(M_PI, thresh);
  m_delta_theta = static_cast<float>(M_PI) / static_cast<float>(m_num_arc_points_per_cap - 1);
}

PointIndexCapSize
RoundedCapCreator::
compute_size(const ContourData &P, float thresh)
{
  unsigned int num_caps, num_arc_points_per_cap;
  PointIndexCapSize return_value;

  num_arc_points_per_cap = fastuidraw::detail::number_segments_for_tessellation(M_PI, thresh);

  /* each cap is a triangle fan centered at the cap point.
   */
  num_caps = 2 * P.number_contours();
  return_value.m_verts = (1 + num_arc_points_per_cap) * num_caps;
  return_value.m_indices = 3 * (num_arc_points_per_cap - 1) * num_caps;

  return return_value;
}

void
RoundedCapCreator::
add_cap(const fastuidraw::vec2 &normal_from_stroking,
        bool is_starting_cap, unsigned int depth,
        const fastuidraw::TessellatedPath::point &p,
        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  CommonCapData C(is_starting_cap, p.m_p, normal_from_stroking);
  unsigned int first, i;
  float theta;
  fastuidraw::StrokedPath::point pt;

  first = vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(0, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points_per_cap - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float s, c;

      s = std::sin(theta);
      c = std::cos(theta);
      pt.m_position = C.m_p;
      pt.m_pre_offset = C.m_n;
      pt.m_auxiliary_offset = fastuidraw::vec2(s, c);
      pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
      pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
      pt.m_edge_length = p.m_edge_length;
      pt.m_open_contour_length = p.m_open_contour_length;
      pt.m_closed_contour_length = p.m_closed_contour_length;
      pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_rounded_cap, depth);
      pt.pack_point(&pts[vertex_offset]);
    }

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}


///////////////////////////////////////////////////
// SquareCapCreator methods
PointIndexCapSize
SquareCapCreator::
compute_size(const ContourData &P)
{
  PointIndexCapSize return_value;
  unsigned int num_caps;

  /* each square cap generates 5 new points
     and 3 triangles (= 9 indices)
   */
  num_caps = 2 * P.number_contours();
  return_value.m_verts = 5 * num_caps;
  return_value.m_indices = 9 * num_caps;

  return return_value;
}

void
SquareCapCreator::
add_cap(const fastuidraw::vec2 &normal_from_stroking,
        bool is_starting_cap, unsigned int depth,
        const fastuidraw::TessellatedPath::point &p,
        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  CommonCapData C(is_starting_cap, p.m_p, normal_from_stroking);
  unsigned int first;
  fastuidraw::StrokedPath::point pt;

  first = vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(0, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxiliary_offset = C.m_v;
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_square_cap, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxiliary_offset = C.m_v;
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_square_cap, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxiliary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::point::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

//////////////////////////////////////
// AdjustableCapCreator methods
PointIndexCapSize
AdjustableCapCreator::
compute_size(const ContourData &P)
{
  PointIndexCapSize return_value;
  unsigned int num_caps;

  num_caps = 2 * P.number_contours();
  return_value.m_verts = number_points_per_fan * num_caps;
  return_value.m_indices = number_indices_per_fan * num_caps;

  return return_value;
}

void
AdjustableCapCreator::
pack_fan(bool entering_contour,
         enum fastuidraw::StrokedPath::point::offset_type_t type,
         const fastuidraw::TessellatedPath::point &p,
         const fastuidraw::vec2 &stroking_normal,
         unsigned int depth,
         fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
         unsigned int &vertex_offset,
         fastuidraw::c_array<unsigned int> indices,
         unsigned int &index_offset)
{
  CommonCapData C(entering_contour, p.m_p, stroking_normal);
  unsigned int first(vertex_offset);
  fastuidraw::StrokedPath::point pt;

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(0, type, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_auxiliary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(1, type, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_auxiliary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(1, type, depth) | fastuidraw::StrokedPath::point::adjustable_cap_ending_mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxiliary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(0, type, depth) | fastuidraw::StrokedPath::point::adjustable_cap_ending_mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_auxiliary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(1, type, depth) | fastuidraw::StrokedPath::point::adjustable_cap_ending_mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_auxiliary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(1, type, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

void
AdjustableCapCreator::
add_cap(const fastuidraw::vec2 &normal_from_stroking,
        bool is_starting_cap, unsigned int depth,
        const fastuidraw::TessellatedPath::point &p0,
        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
        fastuidraw::c_array<unsigned int> indices,
        unsigned int &vertex_offset,
        unsigned int &index_offset) const
{
  enum fastuidraw::StrokedPath::point::offset_type_t tp;
  tp = (is_starting_cap) ?
    fastuidraw::StrokedPath::point::offset_adjustable_cap_contour_start :
    fastuidraw::StrokedPath::point::offset_adjustable_cap_contour_end;

  pack_fan(is_starting_cap, tp,
           p0, normal_from_stroking, depth,
           pts, vertex_offset, indices, index_offset);
}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(const fastuidraw::TessellatedPath &P):
  m_subset(nullptr)
{
  if (!P.point_data().empty())
    {
      m_empty_path = false;
      create_edges(P);
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
      std::fill(m_chunk_of_edges.begin(), m_chunk_of_edges.end(), 0);
      m_chunk_of_caps = 0;
    }
}

StrokedPathPrivate::
~StrokedPathPrivate()
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
StrokedPathPrivate::
create_edges(const fastuidraw::TessellatedPath &P)
{
  SubEdgeCullingHierarchy *s;
  StrokedPathSubset::CreationValues cnts;

  FASTUIDRAWassert(!m_empty_path);
  s = SubEdgeCullingHierarchy::create(P, m_path_data.m_contour_data);
  m_subset = StrokedPathSubset::create(s, cnts, m_path_data.m_join_ordering, m_path_data.m_cap_ordering);
  m_edges.set_data(EdgeAttributeFiller(m_subset, P, cnts));

  m_path_data.m_number_join_chunks = cnts.m_non_closing_join_chunk_cnt + cnts.m_closing_join_chunk_cnt;
  m_path_data.m_number_cap_chunks = cnts.m_cap_chunk_cnt;

  /* the chunks of the root element have the data for everything.
   */
  m_chunk_of_edges[fastuidraw::StrokedPath::all_non_closing] = m_subset->non_closing_edges().m_chunk;
  m_chunk_of_joins[fastuidraw::StrokedPath::all_non_closing] = m_subset->non_closing_joins().m_chunk;

  m_chunk_of_edges[fastuidraw::StrokedPath::all_closing] = m_subset->closing_edges().m_chunk;
  m_chunk_of_joins[fastuidraw::StrokedPath::all_closing] = m_subset->closing_joins().m_chunk;

  m_chunk_of_caps = m_subset->caps().m_chunk;

  FASTUIDRAWdelete(s);
}

template<typename T>
const fastuidraw::PainterAttributeData&
StrokedPathPrivate::
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
     set it as a ratio of the bounding box of
     the underlying tessellated path?
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

//////////////////////////////////////
// fastuidraw::StrokedPath::point methods
void
fastuidraw::StrokedPath::point::
pack_point(PainterAttribute *dst) const
{
  dst->m_attrib0 = pack_vec4(m_position.x(),
                             m_position.y(),
                             m_pre_offset.x(),
                             m_pre_offset.y());

  dst->m_attrib1 = pack_vec4(m_distance_from_edge_start,
                             m_distance_from_contour_start,
                             m_auxiliary_offset.x(),
                             m_auxiliary_offset.y());

  dst->m_attrib2 = uvec4(m_packed_data,
                         pack_float(m_edge_length),
                         pack_float(m_open_contour_length),
                         pack_float(m_closed_contour_length));
}

void
fastuidraw::StrokedPath::point::
unpack_point(point *dst, const PainterAttribute &a)
{
  dst->m_position.x() = unpack_float(a.m_attrib0.x());
  dst->m_position.y() = unpack_float(a.m_attrib0.y());

  dst->m_pre_offset.x() = unpack_float(a.m_attrib0.z());
  dst->m_pre_offset.y() = unpack_float(a.m_attrib0.w());

  dst->m_distance_from_edge_start = unpack_float(a.m_attrib1.x());
  dst->m_distance_from_contour_start = unpack_float(a.m_attrib1.y());
  dst->m_auxiliary_offset.x() = unpack_float(a.m_attrib1.z());
  dst->m_auxiliary_offset.y() = unpack_float(a.m_attrib1.w());

  dst->m_packed_data = a.m_attrib2.x();
  dst->m_edge_length = unpack_float(a.m_attrib2.y());
  dst->m_open_contour_length = unpack_float(a.m_attrib2.z());
  dst->m_closed_contour_length = unpack_float(a.m_attrib2.w());
}

fastuidraw::vec2
fastuidraw::StrokedPath::point::
offset_vector(void)
{
  enum offset_type_t tp;

  tp = offset_type();

  switch(tp)
    {
    case offset_start_sub_edge:
    case offset_end_sub_edge:
    case offset_shared_with_edge:
      return m_pre_offset;

    case offset_square_cap:
      return m_pre_offset + m_auxiliary_offset;

    case offset_rounded_cap:
      {
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        return m_auxiliary_offset.x() * v + m_auxiliary_offset.y() * n;
      }

    case offset_miter_clip_join:
    case offset_miter_clip_join_lambda_negated:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxiliary_offset), Jn1(n1.y(), -n1.x());
        float r, det, lambda;

        det = dot(Jn1, n0);
        lambda = -t_sign(det);
        r = (det != 0.0f) ? (dot(n0, n1) - 1.0f) / det : 0.0f;

        if (tp == offset_miter_clip_join_lambda_negated)
          {
            lambda = -lambda;
          }

        return lambda * (n0 + r * Jn0);
      }

    case offset_miter_bevel_join:
    case offset_miter_join:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxiliary_offset), Jn1(n1.y(), -n1.x());
        float r, lambda, den;

        lambda = t_sign(dot(Jn0, n1));
        den = 1.0f + dot(n0, n1);
        r = (den != 0.0f) ? lambda / den : 0.0f;
        return r * (n0 + n1);
      }

    case offset_rounded_join:
      {
        vec2 cs;

        cs.x() = m_auxiliary_offset.y();
        cs.y() = sqrt(1.0 - cs.x() * cs.x());
        if (m_packed_data & sin_sign_mask)
          {
            cs.y() = -cs.y();
          }
        return cs;
      }

    default:
      return vec2(0.0f, 0.0f);
    }
}

float
fastuidraw::StrokedPath::point::
miter_distance(void) const
{
  enum offset_type_t tp;
  tp = offset_type();

  switch(tp)
    {
    case offset_miter_clip_join:
    case offset_miter_clip_join_lambda_negated:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxiliary_offset), Jn1(n1.y(), -n1.x());
        float r, det;

        det = dot(Jn1, n0);
        r = (det != 0.0f) ? (dot(n0, n1) - 1.0f) / det : 0.0f;
        return t_sqrt(1.0f + r * r);
      }

    case offset_miter_bevel_join:
    case offset_miter_join:
      {
        vec2 n0(m_pre_offset), n1(m_auxiliary_offset);
        vec2 n0_plus_n1(n0 + n1);
        float den;

        den = 1.0f + dot(n0, n1);
        return den != 0.0f ?
          n0_plus_n1.magnitude() / den :
          0.0f;
      }

    default:
      return 0.0f;
    }
}

//////////////////////////////////////////////
// fastuidraw::StrokedPath::ScratchSpace methods
fastuidraw::StrokedPath::ScratchSpace::
ScratchSpace(void)
{
  m_d = FASTUIDRAWnew ScratchSpacePrivate();
}

fastuidraw::StrokedPath::ScratchSpace::
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
add_edge_chunk(const EdgeRanges &ed)
{
  if (ed.non_empty())
    {
      m_edge_chunks.push_back(ed.m_chunk);
    }
}

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

void
ChunkSetPrivate::
handle_dashed_evaluator(const fastuidraw::DashEvaluatorBase *dash_evaluator,
                        const fastuidraw::PainterShaderData::DataBase *dash_data,
                        const fastuidraw::StrokedPath &path)
{
  if (dash_evaluator != nullptr)
    {
      const fastuidraw::PainterAttributeData &joins(path.bevel_joins());
      unsigned int cnt(0);

      m_join_chunks.clear();
      for(const fastuidraw::range_type<unsigned int> &R : m_join_ranges)
        {
          for(unsigned int J = R.m_begin; J < R.m_end; ++J, ++cnt)
            {
              unsigned int chunk;
              fastuidraw::c_array<const fastuidraw::PainterAttribute> attribs;

              chunk = path.join_chunk(J);
              attribs = joins.attribute_data_chunk(chunk);
              FASTUIDRAWassert(!attribs.empty());
              FASTUIDRAWassert(attribs.size() == 3);
              if (dash_evaluator->covered_by_dash_pattern(dash_data, attribs[0]))
                {
                  m_join_chunks.push_back(chunk);
                }
            }
        }
    }
}

//////////////////////////////////////////
// fastuidraw::StrokedPath::ChunkSet methods
fastuidraw::StrokedPath::ChunkSet::
ChunkSet(void)
{
  m_d = FASTUIDRAWnew ChunkSetPrivate();
}

fastuidraw::StrokedPath::ChunkSet::
~ChunkSet(void)
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

fastuidraw::c_array<const unsigned int>
fastuidraw::StrokedPath::ChunkSet::
edge_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return d->edge_chunks();
}

fastuidraw::c_array<const unsigned int>
fastuidraw::StrokedPath::ChunkSet::
join_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return d->join_chunks();
}

fastuidraw::c_array<const unsigned int>
fastuidraw::StrokedPath::ChunkSet::
cap_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return d->cap_chunks();
}


//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methods
fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  FASTUIDRAWassert(point::number_offset_types < FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(point::offset_type_num_bits));
  m_d = FASTUIDRAWnew StrokedPathPrivate(P);
}

fastuidraw::StrokedPath::
~StrokedPath()
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::StrokedPath::
compute_chunks(ScratchSpace &scratch_space,
               const DashEvaluatorBase *dash_evaluator,
               const PainterShaderData::DataBase *dash_data,
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
  StrokedPathPrivate *d;
  ScratchSpacePrivate *scratch_space_ptr;
  ChunkSetPrivate *chunk_set_ptr;

  d = static_cast<StrokedPathPrivate*>(m_d);
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
  chunk_set_ptr->handle_dashed_evaluator(dash_evaluator, dash_data, *this);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
edges(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_edges;
}

unsigned int
fastuidraw::StrokedPath::
chunk_of_edges(enum chunk_selection c) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_chunk_of_edges[c];
}

unsigned int
fastuidraw::StrokedPath::
number_joins(bool include_joins_of_closing_edge) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_path_data.m_join_ordering.number_joins(include_joins_of_closing_edge);
}

unsigned int
fastuidraw::StrokedPath::
join_chunk(unsigned int J) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_path_data.m_join_ordering.join_chunk(J);
}

unsigned int
fastuidraw::StrokedPath::
chunk_of_joins(enum chunk_selection c) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_chunk_of_joins[c];
}

unsigned int
fastuidraw::StrokedPath::
chunk_of_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_chunk_of_caps;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
square_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_square_caps.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
adjustable_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_adjustable_caps.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_bevel_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_clip_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_clip_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_bevel_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_joins.data(d->m_path_data, d->m_subset);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_joins(float thresh) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);

  return (!d->m_empty_path) ?
    d->fetch_create<RoundedJoinCreator>(thresh, d->m_rounded_joins) :
    d->m_empty_data;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_caps(float thresh) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return (!d->m_empty_path) ?
    d->fetch_create<RoundedCapCreator>(thresh, d->m_rounded_caps) :
    d->m_empty_data;
}
