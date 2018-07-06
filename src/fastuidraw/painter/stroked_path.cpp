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
    using namespace fastuidraw;
    FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);
    return pack_bits(StrokedPoint::offset_type_bit0,
                     StrokedPoint::offset_type_num_bits, pp)
      | pack_bits(StrokedPoint::boundary_bit, 1u, bb)
      | pack_bits(StrokedPoint::depth_bit0,
                  StrokedPoint::depth_num_bits, depth);
  }

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

  class SingleSubEdge
  {
  public:
    enum
      {
        first_segment_of_edge = 1,
        last_segment_of_edge  = 2,
      };

    SingleSubEdge()
    {}

    static
    void
    add_sub_edge(const SingleSubEdge *prev_subedge_of_edge,
                 const fastuidraw::TessellatedPath::segment &seg,
                 bool is_closing_edge,
                 std::vector<SingleSubEdge> &dst,
                 fastuidraw::BoundingBox<float> &bx,
                 uint32_t flags);

    void
    split_sub_edge(int splitting_coordinate,
                   std::vector<SingleSubEdge> *dst_before_split_value,
                   std::vector<SingleSubEdge> *dst_after_split_value,
                   float split_value) const;

    fastuidraw::vec2
    unit_vector_into_segment(void) const
    {
      return fastuidraw::vec2(m_begin_normal.y(), -m_begin_normal.x());
    }

    fastuidraw::vec2
    unit_vector_leaving_segment(void) const
    {
      return fastuidraw::vec2(m_end_normal.y(), -m_end_normal.x());
    }

    bool m_from_line_segment;

    fastuidraw::vec2 m_pt0, m_pt1;
    float m_distance_from_edge_start;
    float m_distance_from_contour_start;
    float m_edge_length;
    float m_open_contour_length;
    float m_closed_contour_length;

    bool m_of_closing_edge;
    float m_sub_edge_length;
    fastuidraw::vec2 m_begin_normal, m_end_normal;

    bool m_has_bevel;
    float m_bevel_lambda;
    fastuidraw::vec2 m_bevel_normal;
    bool m_use_arc_for_bevel;

    fastuidraw::BoundingBox<float> m_bounding_box;

    //only make sense when m_from_line_segment is true
    fastuidraw::vec2 m_delta;

    //only makes sense when m_from_line_segment is false
    fastuidraw::vec2 m_center;
    fastuidraw::range_type<float> m_arc_angle;
    float m_radius;

    // only needed when arc-stroking where an edge begins or ends.
    bool m_has_start_dashed_capper;
    bool m_has_end_dashed_capper;

  private:
    SingleSubEdge(const fastuidraw::TessellatedPath::segment &seg,
                  bool is_closing_edge, uint32_t flags);

    bool
    intersection_point_in_arc(fastuidraw::vec2 pt, float *out_arc_angle) const;

    static
    float
    compute_lambda(const fastuidraw::vec2 &n0, const fastuidraw::vec2 &n1)
    {
      fastuidraw::vec2 v1(n1.y(), -n1.x());
      float d;

      d = fastuidraw::dot(v1, n0);
      return (d > 0.0f) ? -1.0f : 1.0f;
    }
  };

  /* A SubEdgeCullingHierarchy represents a hierarchy choice of
   * what sub-edges land in each element of a hierarchy.
   */
  class SubEdgeCullingHierarchy:fastuidraw::noncopyable
  {
  public:
    static
    SubEdgeCullingHierarchy*
    create(const fastuidraw::TessellatedPath &P);

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
       * is only in the back
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
        splitting_threshhold = 50,
        max_recursion_depth = 10
      };

    SubEdgeCullingHierarchy(unsigned int recursion_depth,
                            const fastuidraw::BoundingBox<float> &start_box,
                            std::vector<SingleSubEdge> &data, unsigned int num_non_closing_edges);

    /* a value of -1 means to NOT split.
     */
    static
    int
    choose_splitting_coordinate(const fastuidraw::BoundingBox<float> &start_box,
                                fastuidraw::c_array<const SingleSubEdge> data,
                                float &split_value);

    static
    void
    create_lists(const fastuidraw::TessellatedPath &P,
                 std::vector<SingleSubEdge> &data, unsigned int &num_non_closing_edges,
                 fastuidraw::BoundingBox<float> &bx);

    static
    void
    process_edge(const fastuidraw::TessellatedPath &P,
                 unsigned int contour, unsigned int edge,
                 std::vector<SingleSubEdge> &dst,
                 fastuidraw::BoundingBox<float> &bx);

    template<typename T>
    static
    void
    check_closing_at_end(const std::vector<T> &data, unsigned int num_non_closing);

    /* children, if any
     */
    fastuidraw::vecN<SubEdgeCullingHierarchy*, 2> m_children;

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
     * of indices and vertices above.
     */
    unsigned int m_chunk;

    /* source of data, only non-empty if EdgeAttributeFiller
     * should create attribute/index data at the ranges
     * above.
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

  class ChunkSetPrivate:fastuidraw::noncopyable
  {
  public:
    void
    reset(void)
    {
      m_edge_chunks.clear();
    }

    void
    add_edge_chunk(const EdgeRanges &ed);

    fastuidraw::c_array<const unsigned int>
    edge_chunks(void) const
    {
      return fastuidraw::make_c_array(m_edge_chunks);
    }

    fastuidraw::StrokedCapsJoins::ChunkSet m_caps_joins;

  private:
    std::vector<unsigned int> m_edge_chunks;
  };

  /* Subset of a StrokedPath. Edges are to be placed into
   * the store as follows:
   *   1. child0 edges
   *   2. child1 edges
   *   3. edges (i.e. from SubEdgeCullingHierarchy::m_sub_edges)
   *
   * with the invariant thats that
   */
  class StrokedPathSubset
  {
  public:
    enum
      {
        points_per_segment = 6,
        triangles_per_segment = points_per_segment - 2,
        indices_per_segment = 3 * triangles_per_segment,

        points_per_arc_segment = 12,
        triangles_per_arc_segment = 8,
        indices_per_arc_segment = 3 * triangles_per_arc_segment,
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
        m_closing_edge_chunk_cnt(0)
      {}

      unsigned int m_non_closing_edge_vertex_cnt;
      unsigned int m_non_closing_edge_index_cnt;
      unsigned int m_non_closing_edge_chunk_cnt;

      unsigned int m_closing_edge_vertex_cnt;
      unsigned int m_closing_edge_index_cnt;
      unsigned int m_closing_edge_chunk_cnt;
    };

    static
    StrokedPathSubset*
    create(const SubEdgeCullingHierarchy *src,
           CreationValues &out_values);

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

  private:
    class PostProcessVariables
    {
    public:
      PostProcessVariables(void):
        m_edge_depth(0),
        m_closing_edge_depth(0)
      {}

      unsigned int m_edge_depth, m_closing_edge_depth;
    };

    StrokedPathSubset(CreationValues &out_values,
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
    unsigned int
    compute_edge_depth(fastuidraw::c_array<const SingleSubEdge> edges);

    static
    void
    increment_vertices_indices(fastuidraw::c_array<const SingleSubEdge> src,
                               unsigned int &vertex_cnt,
                               unsigned int &index_cnt);
    void
    post_process(PostProcessVariables &variables,
                 const CreationValues &constants);

    fastuidraw::vecN<StrokedPathSubset*, 2> m_children;

    /* book keeping for edges. */
    EdgeRanges m_non_closing_edges, m_closing_edges;

    fastuidraw::BoundingBox<float> m_bb;

    bool m_empty_subset;
  };

  class EdgeAttributeFillerBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    EdgeAttributeFillerBase(const StrokedPathSubset *src,
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

    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const = 0;

    const StrokedPathSubset *m_src;
    const fastuidraw::TessellatedPath &m_P;
    unsigned int m_total_vertex_cnt, m_total_index_cnt;
    unsigned int m_total_number_chunks;
  };

  class LineEdgeAttributeFiller:public EdgeAttributeFillerBase
  {
  public:
    LineEdgeAttributeFiller(const StrokedPathSubset *src,
                        const fastuidraw::TessellatedPath &P,
                        const StrokedPathSubset::CreationValues &cnts):
      EdgeAttributeFillerBase(src, P, cnts)
    {}

  private:
    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const;

    void
    build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                       fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                       fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                       unsigned int &vertex_offset, unsigned int &index_offset) const;

    void
    build_line_bevel(bool is_inner_bevel,
                     const SingleSubEdge &sub_edge, unsigned int &depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const;
  };

  class ArcEdgeAttributeFiller:public EdgeAttributeFillerBase
  {
  public:
    ArcEdgeAttributeFiller(const StrokedPathSubset *src,
                        const fastuidraw::TessellatedPath &P,
                        const StrokedPathSubset::CreationValues &cnts):
      EdgeAttributeFillerBase(src, P, cnts)
    {}

  private:
    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const;

    void
    build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                       fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                       fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                       unsigned int &vertex_offset, unsigned int &index_offset) const;

    void
    build_arc_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                      fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                      fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                      unsigned int &vertex_offset, unsigned int &index_offset) const;

    void
    build_line_bevel(bool is_inner_bevel,
                     const SingleSubEdge &sub_edge, unsigned int &depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const;

    void
    build_arc_bevel(bool is_inner_bevel,
                    const SingleSubEdge &sub_edge, unsigned int &depth,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                    fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                    unsigned int &vertex_offset, unsigned int &index_offset) const;

    void
    build_dashed_capper(bool for_start_dashed_capper,
                        const SingleSubEdge &sub_edge, unsigned int &depth,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                        fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;
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
    data(const StrokedPathSubset *st)
    {
      if (!m_ready)
        {
          m_data.set_data(T(st));
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
    StrokedPathPrivate(const fastuidraw::TessellatedPath &P,
                       const fastuidraw::StrokedCapsJoins::Builder &b);
    ~StrokedPathPrivate();

    void
    create_edges(const fastuidraw::TessellatedPath &P);

    static
    void
    ready_builder(const fastuidraw::TessellatedPath *tess,
                  fastuidraw::StrokedCapsJoins::Builder &b);

    static
    void
    ready_builder_contour(const fastuidraw::TessellatedPath *tess,
                          unsigned int contour,
                          fastuidraw::StrokedCapsJoins::Builder &b);

    bool m_has_arcs;
    fastuidraw::StrokedCapsJoins m_caps_joins;
    StrokedPathSubset* m_subset;
    fastuidraw::PainterAttributeData m_edges;

    fastuidraw::vecN<unsigned int, 2> m_chunk_of_edges;

    bool m_empty_path;
    fastuidraw::PainterAttributeData m_empty_data;
  };

}

////////////////////////////////////////
// SingleSubEdge methods
SingleSubEdge::
SingleSubEdge(const fastuidraw::TessellatedPath::segment &seg,
              bool is_closing_edge, uint32_t flags):
  m_from_line_segment(seg.m_type == fastuidraw::TessellatedPath::line_segment),
  m_pt0(seg.m_start_pt),
  m_pt1(seg.m_end_pt),
  m_distance_from_edge_start(seg.m_distance_from_edge_start),
  m_distance_from_contour_start(seg.m_distance_from_contour_start),
  m_edge_length(seg.m_edge_length),
  m_open_contour_length(seg.m_open_contour_length),
  m_closed_contour_length(seg.m_closed_contour_length),
  m_of_closing_edge(is_closing_edge),
  m_sub_edge_length(seg.m_length),
  m_begin_normal(-seg.m_enter_segment_unit_vector.y(),
                 seg.m_enter_segment_unit_vector.x()),
  m_end_normal(-seg.m_leaving_segment_unit_vector.y(),
               seg.m_leaving_segment_unit_vector.x()),
  m_delta(m_pt1 - m_pt0),
  m_center(seg.m_center),
  m_arc_angle(seg.m_arc_angle),
  m_radius(seg.m_radius),
  m_has_start_dashed_capper(flags & first_segment_of_edge),
  m_has_end_dashed_capper(flags & last_segment_of_edge)
{
  /* We know that the even if it is an arc, the edge is monotonic,
   * thus the bounding box of the arc is given by the bounding box
   * containing the end points.
   */
  m_bounding_box.union_point(m_pt0);
  m_bounding_box.union_point(m_pt1);
}

bool
SingleSubEdge::
intersection_point_in_arc(fastuidraw::vec2 pt, float *out_arc_angle) const
{
  using namespace fastuidraw;

  vec2 v0(m_pt0 - m_center), v1(m_pt1 - m_center);
  vec2 n0(-v0.y(), v0.x()), n1(-v1.y(), v1.x());

  /* The arc's radial edges coincide with the edges of the
   * triangle [m_center, m_pt0, m_pt1], as such we can use the
   * edges that use m_center define the clipping planes to
   * check if an intersection point is inside the arc.
   */
  pt -= m_center;
  if ((dot(n0, pt) > 0.0) == (dot(n0, m_pt1 - m_center) > 0.0)
      && (dot(n1, pt) > 0.0) == (dot(n1, m_pt0 - m_center) > 0.0))
    {
      /* pt is within the arc; compute the angle */
      float theta;

      theta = t_atan2(pt.y(), pt.x());
      if (theta < t_min(m_arc_angle.m_begin, m_arc_angle.m_end))
        {
          theta += 2.0f * static_cast<float>(M_PI);
        }
      else if (theta > t_max(m_arc_angle.m_begin, m_arc_angle.m_end))
        {
          theta -= 2.0f * static_cast<float>(M_PI);
        }

      FASTUIDRAWassert(theta >= t_min(m_arc_angle.m_begin, m_arc_angle.m_end));
      FASTUIDRAWassert(theta <= t_max(m_arc_angle.m_begin, m_arc_angle.m_end));
      *out_arc_angle = theta;

      return true;
    }

  return false;
}

void
SingleSubEdge::
add_sub_edge(const SingleSubEdge *prev_subedge_of_edge,
             const fastuidraw::TessellatedPath::segment &seg,
             bool is_closing_edge,
             std::vector<SingleSubEdge> &dst,
             fastuidraw::BoundingBox<float> &bx,
             uint32_t flags)
{
  SingleSubEdge sub_edge(seg, is_closing_edge, flags);

  /* If the dot product between the normal is negative, then
   * chances are there is a cusp; we do NOT want a bevel (or
   * arc) join then.
   */
  if (!prev_subedge_of_edge
      || seg.m_tangent_with_predecessor
      || fastuidraw::dot(prev_subedge_of_edge->m_end_normal,
                         sub_edge.m_begin_normal) < 0.0f)
    {
      sub_edge.m_bevel_lambda = 0.0f;
      sub_edge.m_has_bevel = false;
      sub_edge.m_use_arc_for_bevel = false;
      sub_edge.m_bevel_normal = fastuidraw::vec2(0.0f, 0.0f);
    }
  else
    {
      sub_edge.m_bevel_lambda = compute_lambda(prev_subedge_of_edge->m_end_normal,
                                               sub_edge.m_begin_normal);
      sub_edge.m_has_bevel = true;
      sub_edge.m_bevel_normal = prev_subedge_of_edge->m_end_normal;
      /* make them meet at an arc-join if one of them is not
       * a line-segment
       */
      sub_edge.m_use_arc_for_bevel = !(sub_edge.m_from_line_segment && prev_subedge_of_edge->m_from_line_segment);
    }

  bx.union_box(sub_edge.m_bounding_box);
  dst.push_back(sub_edge);
}

void
SingleSubEdge::
split_sub_edge(int splitting_coordinate,
               std::vector<SingleSubEdge> *dst_before_split_value,
               std::vector<SingleSubEdge> *dst_after_split_value,
               float split_value) const
{
  float s, t, mid_angle;
  fastuidraw::vec2 p, mid_normal;
  fastuidraw::vecN<SingleSubEdge, 2> out_edges;

  if (m_bounding_box.max_point()[splitting_coordinate] <= split_value)
    {
      FASTUIDRAWassert(m_pt0[splitting_coordinate] <= split_value);
      FASTUIDRAWassert(m_pt1[splitting_coordinate] <= split_value);
      dst_before_split_value->push_back(*this);
      return;
    }

  if (m_bounding_box.min_point()[splitting_coordinate] >= split_value)
    {
      FASTUIDRAWassert(m_pt0[splitting_coordinate] >= split_value);
      FASTUIDRAWassert(m_pt1[splitting_coordinate] >= split_value);
      dst_after_split_value->push_back(*this);
      return;
    }

  if (m_from_line_segment)
    {
      float v0, v1;

      v0 = m_pt0[splitting_coordinate];
      v1 = m_pt1[splitting_coordinate];
      t = (split_value - v0) / (v1 - v0);

      s = 1.0 - t;
      p = (1.0f - t) * m_pt0 + t * m_pt1;
      mid_normal = m_begin_normal;

      /* strictly speaking this is non-sense, but lets silence
       * any writes with uninitialized data here.
       */
      mid_angle = 0.5f * (m_arc_angle.m_begin + m_arc_angle.m_end);
    }
  else
    {
      /* compute t, s, p, mid_normal; We are gauranteed
       * that the arc is monotonic, thus the splitting will
       * split it into at most two pieces. In addition, because
       * it is monotonic, we know it will split it in exactly
       * two pieces.
       */
      float radical, d;
      fastuidraw::vec2 test_pt;

      d = split_value - m_center[splitting_coordinate];
      radical = m_radius * m_radius - d * d;

      FASTUIDRAWassert(radical > 0.0f);
      radical = fastuidraw::t_sqrt(radical);

      test_pt[splitting_coordinate] = split_value;
      test_pt[1 - splitting_coordinate] = m_center[1 - splitting_coordinate] - radical;

      if (!intersection_point_in_arc(test_pt, &mid_angle))
        {
          bool result;

          test_pt[1 - splitting_coordinate] = m_center[1 - splitting_coordinate] + radical;
          result = intersection_point_in_arc(test_pt, &mid_angle);

          FASTUIDRAWassert(result);
          FASTUIDRAWunused(result);
        }

      p = test_pt;
      t = (mid_angle - m_arc_angle.m_begin) / (m_arc_angle.m_end - m_arc_angle.m_begin);
      s = 1.0f - t;
    }

  out_edges[0].m_pt0 = m_pt0;
  out_edges[0].m_pt1 = p;
  out_edges[0].m_begin_normal = m_begin_normal;
  out_edges[0].m_end_normal = mid_normal;
  out_edges[0].m_arc_angle.m_begin = m_arc_angle.m_begin;
  out_edges[0].m_arc_angle.m_end = mid_angle;
  out_edges[0].m_distance_from_edge_start = m_distance_from_edge_start;
  out_edges[0].m_distance_from_contour_start = m_distance_from_contour_start;
  out_edges[0].m_sub_edge_length = t * m_sub_edge_length;
  out_edges[0].m_bounding_box.union_point(m_pt0);
  out_edges[0].m_bounding_box.union_point(p);
  out_edges[0].m_has_start_dashed_capper = m_has_start_dashed_capper;
  out_edges[0].m_has_end_dashed_capper = false;

  out_edges[1].m_pt0 = p;
  out_edges[1].m_pt1 = m_pt1;
  out_edges[1].m_begin_normal = mid_normal;
  out_edges[1].m_end_normal = m_end_normal;
  out_edges[1].m_arc_angle.m_begin = mid_angle;
  out_edges[1].m_arc_angle.m_end = m_arc_angle.m_end;
  out_edges[1].m_distance_from_edge_start = m_distance_from_edge_start + out_edges[0].m_sub_edge_length;
  out_edges[1].m_distance_from_contour_start = m_distance_from_contour_start + out_edges[0].m_sub_edge_length;
  out_edges[1].m_sub_edge_length = s * m_sub_edge_length;
  out_edges[1].m_bounding_box.union_point(p);
  out_edges[1].m_bounding_box.union_point(m_pt1);
  out_edges[1].m_has_start_dashed_capper = false;
  out_edges[1].m_has_end_dashed_capper = m_has_end_dashed_capper;

  for(unsigned int i = 0; i < 2; ++i)
    {
      out_edges[i].m_from_line_segment = m_from_line_segment;
      out_edges[i].m_delta = out_edges[i].m_pt1 - out_edges[i].m_pt0;
      out_edges[i].m_of_closing_edge = m_of_closing_edge;
      out_edges[i].m_has_bevel = m_has_bevel && (i == 0);
      out_edges[i].m_bevel_lambda = (i == 0) ? m_bevel_lambda : 0.0f;
      out_edges[i].m_bevel_normal = (i == 0) ? m_bevel_normal : fastuidraw::vec2(0.0f, 0.0f);
      out_edges[i].m_use_arc_for_bevel = m_use_arc_for_bevel;

      out_edges[i].m_center = m_center;
      out_edges[i].m_radius = m_radius;

      out_edges[i].m_edge_length = m_edge_length;
      out_edges[i].m_open_contour_length = m_open_contour_length;
      out_edges[i].m_closed_contour_length = m_closed_contour_length;
    }

  dst_before_split_value->push_back(out_edges[0]);
  dst_after_split_value->push_back(out_edges[1]);
}

//////////////////////////////////////////////
// SubEdgeCullingHierarchy methods
SubEdgeCullingHierarchy*
SubEdgeCullingHierarchy::
create(const fastuidraw::TessellatedPath &P)
{
  std::vector<SingleSubEdge> data;
  fastuidraw::BoundingBox<float> bx;
  unsigned int num_non_closing_edges;
  SubEdgeCullingHierarchy *return_value;

  create_lists(P, data, num_non_closing_edges, bx);
  return_value =  FASTUIDRAWnew SubEdgeCullingHierarchy(0, bx, data, num_non_closing_edges);
  return return_value;
}

void
SubEdgeCullingHierarchy::
create_lists(const fastuidraw::TessellatedPath &P,
             std::vector<SingleSubEdge> &data, unsigned int &num_non_closing_edges,
             fastuidraw::BoundingBox<float> &bx)
{
  /* place data of closing sub-edges at the tail of the lists. */
  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      for(unsigned int e = 0, ende = P.number_edges(o); e + 1 < ende; ++e)
        {
          process_edge(P, o, e, data, bx);
        }
    }

  num_non_closing_edges = data.size();
  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      if (P.number_edges(o) > 0)
        {
          process_edge(P, o, P.number_edges(o) - 1, data, bx);
        }
    }
}

void
SubEdgeCullingHierarchy::
process_edge(const fastuidraw::TessellatedPath &P,
             unsigned int contour, unsigned int edge,
             std::vector<SingleSubEdge> &dst, fastuidraw::BoundingBox<float> &bx)
{
  fastuidraw::range_type<unsigned int> R;
  fastuidraw::c_array<const fastuidraw::TessellatedPath::segment> src_segments(P.segment_data());
  bool is_closing_edge, has_arcs(P.has_arcs());
  const SingleSubEdge *prev(nullptr);

  is_closing_edge = (edge + 1 == P.number_edges(contour));
  R = P.edge_range(contour, edge);
  FASTUIDRAWassert(R.m_end >= R.m_begin);

  for(unsigned int i = R.m_begin; i < R.m_end; ++i)
    {
      uint32_t flags;

      flags = 0u;
      if (has_arcs && i == R.m_begin)
        {
          flags |= SingleSubEdge::first_segment_of_edge;
        }
      if (has_arcs && i + 1 == R.m_end)
        {
          flags |= SingleSubEdge::last_segment_of_edge;
        }

      SingleSubEdge::add_sub_edge(prev, src_segments[i],
                                  is_closing_edge, dst, bx,
                                  flags);
      prev = &dst.back();
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
SubEdgeCullingHierarchy(unsigned int recursion_depth,
                        const fastuidraw::BoundingBox<float> &start_box,
                        std::vector<SingleSubEdge> &edges, unsigned int num_non_closing_edges):
  m_children(nullptr, nullptr),
  m_bb(start_box)
{
  int c;
  float mid_point;

  FASTUIDRAWassert(!start_box.empty());
  check_closing_at_end(edges, num_non_closing_edges);

  c = (recursion_depth <= max_recursion_depth) ?
    choose_splitting_coordinate(start_box, fastuidraw::make_c_array(edges), mid_point):
    -1;

  if (c != -1)
    {
      fastuidraw::vecN<fastuidraw::BoundingBox<float>, 2> child_boxes;
      fastuidraw::vecN<std::vector<SingleSubEdge>, 2> child_sub_edges;
      fastuidraw::vecN<unsigned int, 2> child_num_non_closing_edges(0, 0);
      fastuidraw::vecN<unsigned int, 2> child_num_non_closing_joins(0, 0);

      for(const SingleSubEdge &sub_edge : edges)
        {
          fastuidraw::vecN<unsigned int, 2> last;

          last[0] = child_sub_edges[0].size();
          last[1] = child_sub_edges[1].size();
          sub_edge.split_sub_edge(c, &child_sub_edges[0], &child_sub_edges[1], mid_point);

          for (unsigned int i = 0; i < 2; ++i)
            {
              for (unsigned int j = last[i], endj = child_sub_edges[i].size(); j < endj; ++j)
                {
                  child_boxes[i].union_box(child_sub_edges[i][j].m_bounding_box);
                  if (!child_sub_edges[i][j].m_of_closing_edge)
                    {
                      ++child_num_non_closing_edges[i];
                    }
                }
            }
        }

      for (int i = 0; i < 2; ++i)
        {
          m_children[i] =
            FASTUIDRAWnew SubEdgeCullingHierarchy(recursion_depth + 1,
                                                  child_boxes[i],
                                                  child_sub_edges[i],
                                                  child_num_non_closing_edges[i]);
        }
    }
  else
    {
      /* steal the data */
      m_sub_edges.init(edges, num_non_closing_edges);
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

  /* NOTE: we can use the end points of the SingleSubEdge
   * to count on what side or sides a SingleSubEdge
   * will land even when arcs are present because we
   * have the guarantee that a SingleSubEdge that is an
   * arc is a monotonic-arc.
   */
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
          qs.push_back(sub_edge.m_pt0[c]);
        }
      qs.push_back(data.back().m_pt1[c]);
      qs.push_back(start_box.max_point()[c]);

      std::sort(qs.begin(), qs.end());
      split_values[c] = qs[qs.size() / 2];

      for(const SingleSubEdge &sub_edge : data)
        {
          bool sA, sB;
          sA = (sub_edge.m_pt0[c] < split_values[c]);
          sB = (sub_edge.m_pt1[c] < split_values[c]);
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

  /* choose the coordiante that splits the smallest number of edges */
  int canidate;
  canidate = (split_counters[0] < split_counters[1]) ? 0 : 1;

  /* we require that both sides will have fewer edges
   * than the parent size.
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
       CreationValues &out_values)
{
  StrokedPathSubset *return_value;
  PostProcessVariables vars;

  return_value = FASTUIDRAWnew StrokedPathSubset(out_values, src);
  return_value->post_process(vars, out_values);
  return return_value;
}

unsigned int
StrokedPathSubset::
compute_edge_depth(fastuidraw::c_array<const SingleSubEdge> edges)
{
  unsigned int return_value(0);
  for (const SingleSubEdge &E : edges)
    {
      // for the actual edge.
      ++return_value;

      if (E.m_has_bevel)
        {
          // for the inner bevel.
          ++return_value;
          if (E.m_use_arc_for_bevel)
            {
              // the arc-outer bevel needs a depth.
              ++return_value;
            }
        }

      if (E.m_has_start_dashed_capper)
        {
          ++return_value;
        }

      if (E.m_has_end_dashed_capper)
        {
          ++return_value;
        }
    }
  return return_value;
}

void
StrokedPathSubset::
post_process(PostProcessVariables &variables, const CreationValues &constants)
{
  /* We want the depth to go in the reverse order as the
   * draw order. The Draw order is child(0), child(1)
   * Thus, we first handle depth child(1) and then child(0).
   */
  m_non_closing_edges.m_depth_range.m_begin = variables.m_edge_depth;
  m_closing_edges.m_depth_range.m_begin = variables.m_closing_edge_depth;

  if (have_children())
    {
      FASTUIDRAWassert(m_children[0] != nullptr);
      FASTUIDRAWassert(m_children[1] != nullptr);
      FASTUIDRAWassert(m_non_closing_edges.m_src.empty());
      FASTUIDRAWassert(m_closing_edges.m_src.empty());

      m_children[1]->post_process(variables, constants);
      m_children[0]->post_process(variables, constants);
    }
  else
    {
      FASTUIDRAWassert(m_children[0] == nullptr);
      FASTUIDRAWassert(m_children[1] == nullptr);

      variables.m_edge_depth += compute_edge_depth(m_non_closing_edges.m_src);
      variables.m_closing_edge_depth += compute_edge_depth(m_closing_edges.m_src);
    }
  m_non_closing_edges.m_depth_range.m_end = variables.m_edge_depth;
  m_closing_edges.m_depth_range.m_end = variables.m_closing_edge_depth;

  /* make the closing edge chunks start after the
   * non-closing edge chunks.
   */
  m_closing_edges.m_chunk += constants.m_non_closing_edge_chunk_cnt;

  /* make vertices and indices of closing edges appear
   * after those of non-closing edges
   */
  m_closing_edges.m_vertex_data_range += constants.m_non_closing_edge_vertex_cnt;
  m_closing_edges.m_index_data_range += constants.m_non_closing_edge_index_cnt;

  m_empty_subset = !m_non_closing_edges.non_empty()
    && !m_closing_edges.non_empty();
}

StrokedPathSubset::
StrokedPathSubset(CreationValues &out_values, const SubEdgeCullingHierarchy *src):
  m_children(nullptr, nullptr),
  m_bb(src->bounding_box())
{
  /* Draw order is:
   *   child(0)
   *   child(1)
   */
  m_non_closing_edges.m_vertex_data_range.m_begin = out_values.m_non_closing_edge_vertex_cnt;
  m_non_closing_edges.m_index_data_range.m_begin = out_values.m_non_closing_edge_index_cnt;

  m_closing_edges.m_vertex_data_range.m_begin = out_values.m_closing_edge_vertex_cnt;
  m_closing_edges.m_index_data_range.m_begin = out_values.m_closing_edge_index_cnt;

  if (src->has_children())
    {
      for(unsigned int i = 0; i < 2; ++i)
        {
          FASTUIDRAWassert(src->child(i) != nullptr);
          m_children[i] = FASTUIDRAWnew StrokedPathSubset(out_values, src->child(i));
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
    }

  m_non_closing_edges.m_vertex_data_range.m_end = out_values.m_non_closing_edge_vertex_cnt;
  m_non_closing_edges.m_index_data_range.m_end = out_values.m_non_closing_edge_index_cnt;
  m_closing_edges.m_vertex_data_range.m_end = out_values.m_closing_edge_vertex_cnt;
  m_closing_edges.m_index_data_range.m_end = out_values.m_closing_edge_index_cnt;

  m_non_closing_edges.m_chunk = out_values.m_non_closing_edge_chunk_cnt;
  m_closing_edges.m_chunk = out_values.m_closing_edge_chunk_cnt;

  ++out_values.m_non_closing_edge_chunk_cnt;
  ++out_values.m_closing_edge_chunk_cnt;
}

void
StrokedPathSubset::
increment_vertices_indices(fastuidraw::c_array<const SingleSubEdge> src,
                           unsigned int &vertex_cnt,
                           unsigned int &index_cnt)
{
  for(const SingleSubEdge &S : src)
    {
      if (S.m_has_bevel)
        {
          if (S.m_use_arc_for_bevel)
            {
              unsigned int i, v;

              fastuidraw::detail::compute_arc_join_size(1, &v, &i);
              vertex_cnt += 2 * v;
              index_cnt += 2 * i;
            }
          else
            {
              vertex_cnt += 2 * 3;
              index_cnt += 2 * 3;
            }
        }

      if (S.m_from_line_segment)
        {
          vertex_cnt += points_per_segment;
          index_cnt += indices_per_segment;
        }
      else
        {
          vertex_cnt += points_per_arc_segment;
          index_cnt += indices_per_arc_segment;
        }

      if (S.m_has_start_dashed_capper)
        {
          vertex_cnt += 6;
          index_cnt += 12;
        }

      if (S.m_has_end_dashed_capper)
        {
          vertex_cnt += 6;
          index_cnt += 12;
        }
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
      if (include_closing_edge)
        {
          dst.add_edge_chunk(m_closing_edges);
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

  /* clip the bounding box of this StrokedPathSubset */
  vecN<vec2, 4> bb;
  bool unclipped;

  m_bb.inflated_polygon(bb, item_space_additional_room);
  unclipped = clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                  bb, scratch.m_clipped_rect,
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

      if (include_closing_edge)
        {
          FASTUIDRAWassert(m_closing_edges.chunk_fits(max_attribute_cnt, max_index_cnt));
          dst.add_edge_chunk(m_closing_edges);
        }
    }
}

////////////////////////////////////////
// EdgeAttributeFillerBase methods
EdgeAttributeFillerBase::
EdgeAttributeFillerBase(const StrokedPathSubset *src,
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
EdgeAttributeFillerBase::
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
EdgeAttributeFillerBase::
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
EdgeAttributeFillerBase::
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
EdgeAttributeFillerBase::
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
       * therefor they need to have a smaller depth
       */
      unsigned int d = edge.m_depth_range.m_end - 1;
      for(unsigned int k = 0,
            v = edge.m_vertex_data_range.m_begin,
            i = edge.m_index_data_range.m_begin;
          k < edge.m_src.size(); ++k)
        {
          FASTUIDRAWassert(d >= edge.m_depth_range.m_begin);
          process_sub_edge(edge.m_src[k], d, attribute_data, index_data, v, i);
        }
      FASTUIDRAWassert(d + 1 == edge.m_depth_range.m_begin);
    }
}

///////////////////////////////////////////////
// LineEdgeAttributeFiller methods
void
LineEdgeAttributeFiller::
build_line_bevel(bool is_inner_bevel,
                 const SingleSubEdge &sub_edge, unsigned int &depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                 unsigned int &vert_offset, unsigned int &index_offset) const
{
  using namespace fastuidraw;
  using namespace detail;

  vecN<StrokedPoint, 3> pts;
  float sgn;

  if (is_inner_bevel)
    {
      sgn = -sub_edge.m_bevel_lambda;
    }
  else
    {
      sgn = sub_edge.m_bevel_lambda;
    }

  indices[index_offset + 0] = vert_offset + 0;
  indices[index_offset + 1] = vert_offset + 1;
  indices[index_offset + 2] = vert_offset + 2;
  index_offset += 3;

  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_open_contour_length = sub_edge.m_open_contour_length;
      pts[k].m_closed_contour_length = sub_edge.m_closed_contour_length;
      pts[k].m_auxiliary_offset = vec2(0.0f, 0.0f);
    }

  pts[0].m_pre_offset = vec2(0.0f, 0.0f);
  pts[0].m_packed_data = pack_data(0, StrokedPoint::offset_sub_edge, depth)
    | StrokedPoint::bevel_edge_mask;

  pts[1].m_pre_offset = sgn * sub_edge.m_bevel_normal;
  pts[1].m_packed_data = pack_data(1, StrokedPoint::offset_sub_edge, depth)
    | StrokedPoint::bevel_edge_mask;

  pts[2].m_pre_offset = sgn * sub_edge.m_begin_normal;
  pts[2].m_packed_data = pack_data(1, StrokedPoint::offset_sub_edge, depth)
    | StrokedPoint::bevel_edge_mask;

  for(unsigned int i = 0; i < 3; ++i)
    {
      pts[i].pack_point(&attribute_data[vert_offset + i]);
    }

  vert_offset += 3;

  if (is_inner_bevel)
    {
      --depth;
    }
}

void
LineEdgeAttributeFiller::
build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                   fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                   fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                   unsigned int &vert_offset, unsigned int &index_offset) const
{
  using namespace fastuidraw;
  const int boundary_values[3] = { 1, 1, 0 };
  const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
  vecN<StrokedPoint, 6> pts;

  /* The quad is:
   * (p, n, delta,  1),
   * (p,-n, delta,  1),
   * (p, 0,     0,  0),
   * (p_next,  n, -delta, 1),
   * (p_next, -n, -delta, 1),
   * (p_next,  0, 0)
   *
   * Notice that we are encoding if it is
   * start or end of edge from the sign of
   * m_on_boundary.
   */
  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_open_contour_length = sub_edge.m_open_contour_length;
      pts[k].m_closed_contour_length = sub_edge.m_closed_contour_length;
      pts[k].m_pre_offset = normal_sign[k] * sub_edge.m_begin_normal;
      pts[k].m_auxiliary_offset = sub_edge.m_delta;
      pts[k].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, depth);

      pts[k + 3].m_position = sub_edge.m_pt1;
      pts[k + 3].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_edge_length = sub_edge.m_edge_length;
      pts[k + 3].m_open_contour_length = sub_edge.m_open_contour_length;
      pts[k + 3].m_closed_contour_length = sub_edge.m_closed_contour_length;
      pts[k + 3].m_pre_offset = normal_sign[k] * sub_edge.m_end_normal;
      pts[k + 3].m_auxiliary_offset = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, depth)
        | StrokedPoint::end_sub_edge_mask;
    }

  for(unsigned int i = 0; i < 6; ++i)
    {
      pts[i].pack_point(&attribute_data[vert_offset + i]);
    }

  const unsigned int tris[12] =
    {
      0, 2, 5,
      0, 5, 3,
      2, 1, 4,
      2, 4, 5
    };

  for(unsigned int i = 0; i < 12; ++i)
    {
      indices[index_offset + i] = vert_offset + tris[i];
    }

  index_offset += StrokedPathSubset::indices_per_segment;
  vert_offset += StrokedPathSubset::points_per_segment;
  --depth;
}

void
LineEdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                 unsigned int &vert_offset, unsigned int &index_offset) const
{
  FASTUIDRAWassert(sub_edge.m_from_line_segment);
  FASTUIDRAWassert(!sub_edge.m_has_start_dashed_capper);
  FASTUIDRAWassert(!sub_edge.m_has_end_dashed_capper);

  if (sub_edge.m_has_bevel)
    {
      build_line_bevel(false,
                       sub_edge, depth, attribute_data,
                       indices, vert_offset, index_offset);
    }
  build_line_segment(sub_edge, depth, attribute_data,
                     indices, vert_offset, index_offset);

  if (sub_edge.m_has_bevel)
    {
      build_line_bevel(true,
                       sub_edge, depth, attribute_data,
                       indices, vert_offset, index_offset);
    }
}

////////////////////////////////////
// ArcEdgeAttributeFiller methods
void
ArcEdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                 unsigned int &vert_offset, unsigned int &index_offset) const
{
  if (sub_edge.m_has_bevel)
    {
      if (sub_edge.m_use_arc_for_bevel)
        {
          build_arc_bevel(false, sub_edge, depth, attribute_data,
                          indices, vert_offset, index_offset);
        }
      else
        {
          build_line_bevel(false, sub_edge, depth, attribute_data,
                           indices, vert_offset, index_offset);
        }
    }

  if (sub_edge.m_has_start_dashed_capper)
    {
      build_dashed_capper(true, sub_edge, depth, attribute_data,
                         indices, vert_offset, index_offset);
    }

  if (sub_edge.m_from_line_segment)
    {
      build_line_segment(sub_edge, depth, attribute_data,
                         indices, vert_offset, index_offset);
    }
  else
    {
      build_arc_segment(sub_edge, depth, attribute_data,
                        indices, vert_offset, index_offset);
    }

  if (sub_edge.m_has_bevel)
    {
      if (sub_edge.m_use_arc_for_bevel)
        {
          build_arc_bevel(true, sub_edge, depth, attribute_data,
                          indices, vert_offset, index_offset);
        }
      else
        {
          build_line_bevel(true, sub_edge, depth, attribute_data,
                           indices, vert_offset, index_offset);
        }
    }

  if (sub_edge.m_has_end_dashed_capper)
    {
      build_dashed_capper(false, sub_edge, depth, attribute_data,
                         indices, vert_offset, index_offset);
    }
}

void
ArcEdgeAttributeFiller::
build_dashed_capper(bool for_start_dashed_capper,
                    const SingleSubEdge &sub_edge, unsigned int &depth,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                    fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  using namespace fastuidraw;
  using namespace detail;

  const PainterIndex tris[12] =
    {
      0, 3, 4,
      0, 4, 1,
      1, 4, 5,
      1, 5, 2,
    };

  for (unsigned int i = 0; i < 12; ++i, ++index_offset)
    {
      index_data[index_offset] = vertex_offset + tris[i];
    }

  vec2 tangent, normal, pos;
  uint32_t packed_data, packed_data_mid;
  float d;

  packed_data = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point_dashed_capper, depth);
  packed_data_mid = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point_dashed_capper, depth);
  if (for_start_dashed_capper)
    {
      normal = sub_edge.m_begin_normal;
      tangent = -sub_edge.unit_vector_into_segment();
      pos = sub_edge.m_pt0;
      d = 0.0f;
    }
  else
    {
      normal = sub_edge.m_end_normal;
      tangent = sub_edge.unit_vector_leaving_segment();
      pos = sub_edge.m_pt1;
      packed_data |= ArcStrokedPoint::end_segment_mask;
      packed_data_mid |= ArcStrokedPoint::end_segment_mask;
      d = sub_edge.m_sub_edge_length;
    }

  ArcStrokedPoint pt;

  pt.m_position = pos;
  pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + d;
  pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + d;
  pt.m_edge_length = sub_edge.m_edge_length;
  pt.m_open_contour_length = sub_edge.m_open_contour_length;
  pt.m_closed_contour_length = sub_edge.m_closed_contour_length;
  pt.m_data = tangent;

  pt.m_packed_data = packed_data;
  pt.m_offset_direction = normal;
  pt.pack_point(&attribute_data[vertex_offset++]);

  pt.m_packed_data = packed_data_mid;
  pt.m_offset_direction = vec2(0.0f, 0.0f);
  pt.pack_point(&attribute_data[vertex_offset++]);

  pt.m_packed_data = packed_data;
  pt.m_offset_direction = -normal;
  pt.pack_point(&attribute_data[vertex_offset++]);

  pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = normal;
  pt.pack_point(&attribute_data[vertex_offset++]);

  pt.m_packed_data = packed_data_mid | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = vec2(0.0f, 0.0f);
  pt.pack_point(&attribute_data[vertex_offset++]);

  pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = -normal;
  pt.pack_point(&attribute_data[vertex_offset++]);

  --depth;
}

void
ArcEdgeAttributeFiller::
build_arc_bevel(bool is_inner_bevel,
                const SingleSubEdge &sub_edge, unsigned int &depth,
                fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                unsigned int &vert_offset, unsigned int &index_offset) const
{
  fastuidraw::vec2 n_start, n_end;
  fastuidraw::ArcStrokedPoint pt;

  if (is_inner_bevel)
    {
      n_start = -sub_edge.m_bevel_lambda * sub_edge.m_begin_normal;
      n_end = -sub_edge.m_bevel_lambda * sub_edge.m_bevel_normal;
    }
  else
    {
      n_start = sub_edge.m_bevel_lambda * sub_edge.m_bevel_normal;
      n_end = sub_edge.m_bevel_lambda * sub_edge.m_begin_normal;
    }

  pt.m_position = sub_edge.m_pt0;
  pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
  pt.m_edge_length = sub_edge.m_edge_length;
  pt.m_open_contour_length = sub_edge.m_open_contour_length;
  pt.m_closed_contour_length = sub_edge.m_closed_contour_length;

  fastuidraw::detail::pack_arc_join(pt, 1, n_start, n_end, depth,
                                    attribute_data, vert_offset,
                                    indices, index_offset, false);
  --depth;
}

void
ArcEdgeAttributeFiller::
build_line_bevel(bool is_inner_bevel,
                 const SingleSubEdge &sub_edge, unsigned int &depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                 unsigned int &vert_offset, unsigned int &index_offset) const
{
  using namespace fastuidraw;
  using namespace detail;

  vecN<ArcStrokedPoint, 3> pts;
  float sgn;

  if (is_inner_bevel)
    {
      sgn = -sub_edge.m_bevel_lambda;
    }
  else
    {
      sgn = sub_edge.m_bevel_lambda;
    }

  indices[index_offset + 0] = vert_offset + 0;
  indices[index_offset + 1] = vert_offset + 1;
  indices[index_offset + 2] = vert_offset + 2;
  index_offset += 3;

  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_open_contour_length = sub_edge.m_open_contour_length;
      pts[k].m_closed_contour_length = sub_edge.m_closed_contour_length;
      pts[k].m_data = vec2(0.0f, 0.0f);
    }

  pts[0].m_offset_direction = vec2(0.0f, 0.0f);
  pts[0].m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
    | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_line_segment, depth);

  pts[1].m_offset_direction = sgn * sub_edge.m_bevel_normal;
  pts[1].m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_line_segment, depth);

  pts[2].m_offset_direction = sgn * sub_edge.m_begin_normal;
  pts[2].m_packed_data = ArcStrokedPoint::distance_constant_on_primitive_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_line_segment, depth);

  for(unsigned int i = 0; i < 3; ++i)
    {
      pts[i].pack_point(&attribute_data[vert_offset + i]);
    }

  vert_offset += 3;
  if (is_inner_bevel)
    {
      --depth;
    }
}

void
ArcEdgeAttributeFiller::
build_arc_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                  fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                  fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                  unsigned int &vert_offset, unsigned int &index_offset) const
{
  using namespace fastuidraw;
  using namespace detail;

  ArcStrokedPoint begin_pt, end_pt;
  vec2 begin_radial, end_radial;

  const unsigned int tris[24] =
    {
      4, 6, 7,
      4, 7, 5,
      2, 4, 5,
      2, 5, 3,
      8, 0, 1,
      9, 2, 3,

      0,  1, 10,
      1, 10, 11
    };

  for (unsigned int i = 0; i < StrokedPathSubset::indices_per_arc_segment; ++i, ++index_offset)
    {
      indices[index_offset] = tris[i] + vert_offset;
    }

  begin_pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
  begin_pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
  begin_pt.m_edge_length = sub_edge.m_edge_length;
  begin_pt.m_open_contour_length = sub_edge.m_open_contour_length;
  begin_pt.m_closed_contour_length = sub_edge.m_closed_contour_length;
  begin_pt.radius() = sub_edge.m_radius;
  begin_pt.arc_angle() = sub_edge.m_arc_angle.m_end - sub_edge.m_arc_angle.m_begin;

  end_pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
  end_pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
  end_pt.m_edge_length = sub_edge.m_edge_length;
  end_pt.m_open_contour_length = sub_edge.m_open_contour_length;
  end_pt.m_closed_contour_length = sub_edge.m_closed_contour_length;
  end_pt.radius() = sub_edge.m_radius;
  end_pt.arc_angle() = sub_edge.m_arc_angle.m_end - sub_edge.m_arc_angle.m_begin;

  begin_radial = vec2(t_cos(sub_edge.m_arc_angle.m_begin), t_sin(sub_edge.m_arc_angle.m_begin));
  end_radial = vec2(t_cos(sub_edge.m_arc_angle.m_end), t_sin(sub_edge.m_arc_angle.m_end));

  /* inner stroking boundary points (points 0, 1) */
  begin_pt.m_packed_data = ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&attribute_data[vert_offset++]);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&attribute_data[vert_offset++]);

  /* the points that are on the arc (points 2, 3) */
  begin_pt.m_packed_data = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&attribute_data[vert_offset++]);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&attribute_data[vert_offset++]);

  /* outer stroking boundary points (points 4, 5) */
  begin_pt.m_packed_data = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&attribute_data[vert_offset++]);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&attribute_data[vert_offset++]);

  /* beyond outer stroking boundary points (points 6, 7) */
  begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&attribute_data[vert_offset++]);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&attribute_data[vert_offset++]);

  /* points that move to origin when stroking radius is larger than arc radius (points 8, 9) */
  begin_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
    | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&attribute_data[vert_offset++]);

  end_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
    | ArcStrokedPoint::end_segment_mask
    | ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&attribute_data[vert_offset++]);

  /* beyond inner stroking boundary (points 10, 11) */
  begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
    | ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&attribute_data[vert_offset++]);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&attribute_data[vert_offset++]);

  --depth;
}

void
ArcEdgeAttributeFiller::
build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                   fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                   fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                   unsigned int &vert_offset, unsigned int &index_offset) const
{
  using namespace fastuidraw;
  using namespace detail;
  const int boundary_values[3] = { 1, 1, 0 };
  const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
  vecN<ArcStrokedPoint, 6> pts;

  /* The quad is:
   * (p, n, delta,  1),
   * (p,-n, delta,  1),
   * (p, 0,     0,  0),
   * (p_next,  n, -delta, 1),
   * (p_next, -n, -delta, 1),
   * (p_next,  0, 0)
   *
   * Notice that we are encoding if it is
   * start or end of edge from the sign of
   * m_on_boundary.
   */
  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_open_contour_length = sub_edge.m_open_contour_length;
      pts[k].m_closed_contour_length = sub_edge.m_closed_contour_length;
      pts[k].m_offset_direction = normal_sign[k] * sub_edge.m_begin_normal;
      pts[k].m_data = sub_edge.m_delta;
      pts[k].m_packed_data = arc_stroked_point_pack_bits(boundary_values[k],
                                                         ArcStrokedPoint::offset_line_segment,
                                                         depth);

      pts[k + 3].m_position = sub_edge.m_pt1;
      pts[k + 3].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_edge_length = sub_edge.m_edge_length;
      pts[k + 3].m_open_contour_length = sub_edge.m_open_contour_length;
      pts[k + 3].m_closed_contour_length = sub_edge.m_closed_contour_length;
      pts[k + 3].m_offset_direction = normal_sign[k] * sub_edge.m_end_normal;
      pts[k + 3].m_data = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = arc_stroked_point_pack_bits(boundary_values[k],
                                                             ArcStrokedPoint::offset_line_segment,
                                                             depth)
        | ArcStrokedPoint::end_segment_mask;
    }

  for(unsigned int i = 0; i < 6; ++i)
    {
      pts[i].pack_point(&attribute_data[vert_offset + i]);
    }

  const unsigned int tris[12] =
    {
      0, 2, 5,
      0, 5, 3,
      2, 1, 4,
      2, 4, 5
    };

  for(unsigned int i = 0; i < 12; ++i)
    {
      indices[index_offset + i] = vert_offset + tris[i];
    }

  index_offset += StrokedPathSubset::indices_per_segment;
  vert_offset += StrokedPathSubset::points_per_segment;
  --depth;
}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(const fastuidraw::TessellatedPath &P,
                   const fastuidraw::StrokedCapsJoins::Builder &b):
  m_has_arcs(P.has_arcs()),
  m_caps_joins(b),
  m_subset(nullptr)
{
  if (!P.segment_data().empty())
    {
      m_empty_path = false;
      create_edges(P);
    }
  else
    {
      m_empty_path = true;
      std::fill(m_chunk_of_edges.begin(), m_chunk_of_edges.end(), 0);
    }
}

StrokedPathPrivate::
~StrokedPathPrivate()
{
  if (!m_empty_path)
    {
      FASTUIDRAWdelete(m_subset);
    }
}

void
StrokedPathPrivate::
ready_builder(const fastuidraw::TessellatedPath *tess,
              fastuidraw::StrokedCapsJoins::Builder &b)
{
  for(unsigned int c = 0; c < tess->number_contours(); ++c)
    {
      ready_builder_contour(tess, c, b);
    }
}

void
StrokedPathPrivate::
ready_builder_contour(const fastuidraw::TessellatedPath *tess,
                      unsigned int c,
                      fastuidraw::StrokedCapsJoins::Builder &b)
{
  fastuidraw::c_array<const fastuidraw::TessellatedPath::segment> last_segs;

  last_segs = tess->edge_segment_data(c, 0);
  b.begin_contour(last_segs.front().m_start_pt,
                  last_segs.front().m_enter_segment_unit_vector);

  for(unsigned int e = 1, ende = tess->number_edges(c); e < ende; ++e)
    {
      fastuidraw::c_array<const fastuidraw::TessellatedPath::segment> segs;
      fastuidraw::vec2 delta_into, delta_leaving;

      segs = tess->edge_segment_data(c, e);
      /* Leaving the previous segment is entering the join;
       * Entering the segment is leaving the join
       */
      delta_into = last_segs.back().m_leaving_segment_unit_vector;
      delta_leaving = segs.front().m_enter_segment_unit_vector;

      b.add_join(segs.front().m_start_pt,
                 last_segs.back().m_edge_length,
                 delta_into, delta_leaving);
      last_segs = segs;
    }

  b.end_contour(last_segs.back().m_edge_length,
                last_segs.back().m_leaving_segment_unit_vector);
}

void
StrokedPathPrivate::
create_edges(const fastuidraw::TessellatedPath &P)
{
  SubEdgeCullingHierarchy *s;
  StrokedPathSubset::CreationValues cnts;

  FASTUIDRAWassert(!m_empty_path);
  s = SubEdgeCullingHierarchy::create(P);
  m_subset = StrokedPathSubset::create(s, cnts);

  if (m_has_arcs)
    {
      m_edges.set_data(ArcEdgeAttributeFiller(m_subset, P, cnts));
    }
  else
    {
      m_edges.set_data(LineEdgeAttributeFiller(m_subset, P, cnts));
    }

  /* the chunks of the root element have the data for everything. */
  m_chunk_of_edges[fastuidraw::StrokedPath::all_non_closing] = m_subset->non_closing_edges().m_chunk;
  m_chunk_of_edges[fastuidraw::StrokedPath::all_closing] = m_subset->closing_edges().m_chunk;

  FASTUIDRAWdelete(s);
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

//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methods
fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  StrokedCapsJoins::Builder b;
  StrokedPathPrivate::ready_builder(&P, b);
  m_d = FASTUIDRAWnew StrokedPathPrivate(P, b);
}

fastuidraw::StrokedPath::
~StrokedPath()
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

bool
fastuidraw::StrokedPath::
has_arcs(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_has_arcs;
}

void
fastuidraw::StrokedPath::
compute_chunks(ScratchSpace &scratch_space,
               c_array<const vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               const vec2 &recip_dimensions,
               float pixels_additional_room,
               float item_space_additional_room,
               bool include_closing_edges,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
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
                              *chunk_set_ptr);
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

const fastuidraw::StrokedCapsJoins&
fastuidraw::StrokedPath::
caps_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_caps_joins;
}
