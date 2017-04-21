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
            enum fastuidraw::StrokedPath::offset_type_t pt,
            uint32_t depth)
  {
    FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);
    return fastuidraw::pack_bits(fastuidraw::StrokedPath::offset_type_bit0, fastuidraw::StrokedPath::offset_type_num_bits, pp)
      | fastuidraw::pack_bits(fastuidraw::StrokedPath::boundary_bit, 1u, bb)
      | fastuidraw::pack_bits(fastuidraw::StrokedPath::depth_bit0, fastuidraw::StrokedPath::depth_num_bits, depth);
  }

  inline
  uint32_t
  pack_data_join(int on_boundary,
                 enum fastuidraw::StrokedPath::offset_type_t pt,
                 uint32_t depth)
  {
    return pack_data(on_boundary, pt, depth) | fastuidraw::StrokedPath::join_mask;
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

  class PerEdgeData
  {
  public:
    fastuidraw::vec2 m_begin_normal, m_end_normal;
    fastuidraw::TessellatedPath::point m_start_pt, m_end_pt;
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

  class PathData:fastuidraw::noncopyable
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

  class SingleSubEdge
  {
  public:
    unsigned int m_pt0, m_pt1; //index into TessellatedPath::points
    fastuidraw::vec2 m_normal, m_delta;

    bool m_has_bevel;
    float m_bevel_lambda;
    fastuidraw::vec2 m_bevel_normal;

    /* if non-negative, indicates that m_pt1
       is where an edge ends and is the join-ID
       from the join ordering comiing naturally
       from TessellatedPath. The natural ordering
       is that first we 
     */
    int m_join_id;
  };

  /* An EdgeStore stores the temporary data of the data in
     a TessellatedPath converted to a collection an array
     of SingleSubEdge values.
   */
  class EdgeStore
  {
  public:
    EdgeStore(const fastuidraw::TessellatedPath &P, PathData &path_data);

    fastuidraw::const_c_array<SingleSubEdge>
    sub_edges(bool with_closing_edges)
    {
      return m_sub_edges[with_closing_edges];
    }

    const fastuidraw::BoundingBox<float>&
    bounding_box(bool with_closing_edges)
    {
      return m_sub_edges_bb[with_closing_edges];
    }

  private:

    void
    process_edge(int join_id,
                 const fastuidraw::TessellatedPath &P, PathData &path_data,
                 unsigned int contour, unsigned int edge,
                 std::vector<SingleSubEdge> &dst, fastuidraw::BoundingBox<float> &bx);

    static const float sm_mag_tol;

    std::vector<SingleSubEdge> m_all_edges;
    fastuidraw::vecN<fastuidraw::const_c_array<SingleSubEdge>, 2> m_sub_edges;
    fastuidraw::vecN<fastuidraw::BoundingBox<float>, 2> m_sub_edges_bb;
  };

  /* A SubEdgeCullingHierarchy represents a hierarchy choice of
     what sub-edges land in each element of a hierarchy.
   */
  class SubEdgeCullingHierarchy:fastuidraw::noncopyable
  {
  public:
    enum
      {
        splitting_threshhold = 100
      };

    explicit
    SubEdgeCullingHierarchy(const fastuidraw::BoundingBox<float> &start_box,
                            fastuidraw::const_c_array<SingleSubEdge> data,
                            fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts);

    ~SubEdgeCullingHierarchy();

    fastuidraw::vecN<SubEdgeCullingHierarchy*, 2> m_children;

    /* what edges that intersect both children;
       these edges are placed in the parent instead
       of with a child.
     */
    std::vector<SingleSubEdge> m_sub_edges;

    /* bounding box of m_sub_edges
     */
    fastuidraw::BoundingBox<float> m_sub_edges_bb;

    /* bounding box containing bounding boxes
       of both children and m_sub_edges_bb
     */
    fastuidraw::BoundingBox<float> m_entire_bb;
    
  private:
    int
    choose_splitting_coordinate(const fastuidraw::BoundingBox<float> &start_box,
                                fastuidraw::const_c_array<SingleSubEdge> data,
                                fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts);

    SubEdgeCullingHierarchy*
    create(const fastuidraw::BoundingBox<float> &start_box,
           const std::vector<SingleSubEdge> &data,
           fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts);
  };

  class ScratchSpacePrivate
  {
  public:
    std::vector<fastuidraw::vec3> m_adjusted_clip_eqs;
    std::vector<fastuidraw::vec2> m_clipped_rect;

    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_clip_scratch_vec2s;
    std::vector<float> m_clip_scratch_floats;
  };

  class Flopper
  {
  public:
    /* what chunk a given cap/join will reside on
     */
    unsigned int m_chunk;

    /* the depth value to use for the cap's/join's
       attribute data
     */
    unsigned int m_depth_value;

    /* the join ID from the canonical join ordering
     */
    unsigned int m_join_id;
    
    /* attribute value to pass to DashEvaluator
       when deciding if to keep the join
     */
    fastuidraw::PainterAttribute m_attrib;
  };

  class ChunkSetPrivate
  {
  public:
    std::vector<unsigned int> m_edge_chunks, m_join_chunks, m_cap_chunks;
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

    static
    StrokedPathSubset*
    create(SubEdgeCullingHierarchy *src)
    {
      unsigned int total_chunks(0);
      return FASTUIDRAWnew StrokedPathSubset(0, 0, src, total_chunks, 0, 0, 0);
    }

    ~StrokedPathSubset();

    void
    dump(std::ostream &str, int depth = 0) const;
    
    void
    compute_chunks(ScratchSpacePrivate &work_room,
                   const fastuidraw::DashEvaluatorBase *dash_evaluator,
                   fastuidraw::const_c_array<fastuidraw::vec3> clip_equations,
                   const fastuidraw::float3x3 &clip_matrix_local,
                   const fastuidraw::vec2 &recip_dimensions,
                   float pixels_additional_room,
                   float item_space_additional_room,
                   unsigned int max_attribute_cnt,
                   unsigned int max_index_cnt,
                   bool take_joins_outside_of_region,
                   ChunkSetPrivate &dst);

    const StrokedPathSubset*
    child(unsigned int I) const
    {
      FASTUIDRAWassert(I == 0 || I == 1);
      return m_children[I];
    }

    fastuidraw::range_type<unsigned int>
    vertex_data_range(void) const
    {
      return m_vertex_data_range;
    }

    fastuidraw::range_type<unsigned int>
    index_data_range(void) const
    {
      return m_index_data_range;
    }

    fastuidraw::range_type<unsigned int>
    depth_range(void) const
    {
      return m_depth;
    }

    unsigned int
    data_chunk(void) const
    {
      return m_data_chunk;
    }

    fastuidraw::range_type<unsigned int>
    vertex_data_range_with_children(void) const
    {
      return m_vertex_data_range_with_children;
    }

    fastuidraw::range_type<unsigned int>
    index_data_range_with_children(void) const
    {
      return m_index_data_range_with_children;
    }

    fastuidraw::range_type<unsigned int>
    depth_range_with_children(void) const
    {
      return m_depth_with_children;
    }

    unsigned int
    data_chunk_with_children(void) const
    {
      return m_data_chunk_with_children;
    }

    fastuidraw::const_c_array<SingleSubEdge>
    data_src(void) const
    {
      return m_data_src;
    }

    
  private:
    StrokedPathSubset(unsigned int vertex_st, unsigned int index_st,
                      SubEdgeCullingHierarchy *src, unsigned int &total_chunks,
                      unsigned int depth, unsigned int join_depth, unsigned int cap_depth);

    void
    compute_chunks_implement(ScratchSpacePrivate &work_room,
                             float item_space_additional_room,
                             unsigned int max_attribute_cnt,
                             unsigned int max_index_cnt,
                             ChunkSetPrivate &dst);

    void
    compute_chunks_take_all(unsigned int max_attribute_cnt,
                            unsigned int max_index_cnt,
                            ChunkSetPrivate &dst);

    void
    count_vertices_indices(SubEdgeCullingHierarchy *src,
                           unsigned int &vertex_cnt,
                           unsigned int &index_cnt,
                           unsigned int &depth_cnt,
                           unsigned int &join_depth,
                           unsigned int &cap_depth);

    fastuidraw::vecN<StrokedPathSubset*, 2> m_children;

    /* book keeping for edges.
     */
    fastuidraw::range_type<unsigned int> m_vertex_data_range;
    fastuidraw::range_type<unsigned int> m_index_data_range;
    fastuidraw::range_type<unsigned int> m_depth;
    unsigned int m_data_chunk;
    fastuidraw::BoundingBox<float> m_data_bb;
    fastuidraw::const_c_array<SingleSubEdge> m_data_src;

    fastuidraw::range_type<unsigned int> m_vertex_data_range_with_children;
    fastuidraw::range_type<unsigned int> m_index_data_range_with_children;
    fastuidraw::range_type<unsigned int> m_depth_with_children;
    unsigned int m_data_chunk_with_children;
    fastuidraw::BoundingBox<float> m_data_with_children_bb;

    /* book keeping for joins.
     */
    std::vector<Flopper> m_joins;
    fastuidraw::range_type<unsigned int> m_joins_depth;

    /* book keeping for caps.
     */
    std::vector<Flopper> m_caps;
    fastuidraw::range_type<unsigned int> m_caps_depth;
  };

  class EdgeAttributeFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    EdgeAttributeFiller(const StrokedPathSubset *src,
                        const fastuidraw::TessellatedPath &P);

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
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;
  private:
    void
    fill_data_worker(const StrokedPathSubset *e,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
                     fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                     fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
                     fastuidraw::c_array<int> index_adjusts) const;
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const;

    const StrokedPathSubset *m_src;
    const fastuidraw::TessellatedPath &m_P;
  };

  class JoinCount
  {
  public:
    explicit
    JoinCount(const PathData &P);

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
    JoinCreatorBase(const PathData &P);

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
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
              fastuidraw::c_array<int> index_adjusts) const;

  protected:

    void
    post_ctor_initalize(void);

  private:

    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count) const = 0;


    void
    fill_join(unsigned int join_id,
              unsigned int contour, unsigned int edge,
              fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
              fastuidraw::c_array<unsigned int> indices,
              unsigned int &vertex_offset, unsigned int &index_offset,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<int> index_adjusts) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const = 0;

    const PathData &m_P;
    unsigned int m_num_non_closed_verts, m_num_non_closed_indices;
    unsigned int m_num_closed_verts, m_num_closed_indices;
    unsigned int m_num_joins, m_num_joins_without_closing_edge;
    bool m_post_ctor_initalized_called;
  };


  class RoundedJoinCreator:public JoinCreatorBase
  {
  public:
    RoundedJoinCreator(const PathData &P, float thresh);

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
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
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
    BevelJoinCreator(const PathData &P);

  private:

    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
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
    MiterClipJoinCreator(const PathData &P);

  private:
    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
                        fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                        unsigned int depth,
                        fastuidraw::c_array<unsigned int> indices,
                        unsigned int &vertex_offset, unsigned int &index_offset) const;

    mutable std::vector<fastuidraw::vec2> m_n0, m_n1;
  };

  template<enum fastuidraw::StrokedPath::offset_type_t tp>
  class MiterJoinCreator:public JoinCreatorBase
  {
  public:
    explicit
    MiterJoinCreator(const PathData &P);

  private:
    virtual
    void
    add_join(unsigned int join_id,
             const PathData &path,
             const fastuidraw::vec2 &n0_from_stroking,
             const fastuidraw::vec2 &n1_from_stroking,
             unsigned int contour, unsigned int edge,
             unsigned int &vert_count, unsigned int &index_count) const;

    virtual
    void
    fill_join_implement(unsigned int join_id,
                        const PathData &path,
                        unsigned int contour, unsigned int edge,
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
    CapCreatorBase(const PathData &P, PointIndexCapSize sz);

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
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
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

    const PathData &m_P;
    PointIndexCapSize m_size;
  };

  class RoundedCapCreator:public CapCreatorBase
  {
  public:
    RoundedCapCreator(const PathData &P, float thresh);

  private:
    static
    PointIndexCapSize
    compute_size(const PathData &P, float thresh);

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
    SquareCapCreator(const PathData &P):
      CapCreatorBase(P, compute_size(P))
    {}

  private:
    static
    PointIndexCapSize
    compute_size(const PathData &P);

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
    AdjustableCapCreator(const PathData &P):
      CapCreatorBase(P, compute_size(P))
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
             enum fastuidraw::StrokedPath::offset_type_t type,
             const fastuidraw::TessellatedPath::point &edge_pt,
             const fastuidraw::vec2 &stroking_normal,
             unsigned int depth,
             fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
             unsigned int &vertex_offset,
             fastuidraw::c_array<unsigned int> indices,
             unsigned int &index_offset);

    static
    PointIndexCapSize
    compute_size(const PathData &P);

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
    data(const PathData &P)
    {
      if(!m_ready)
        {
          m_data.set_data(T(P));
          m_ready = true;
        }
      return m_data;
    }

  private:
    fastuidraw::PainterAttributeData m_data;
    bool m_ready;
  };

  class StrokedPathPrivate
  {
  public:
    explicit
    StrokedPathPrivate(const fastuidraw::TessellatedPath &P);
    ~StrokedPathPrivate();

    void
    create_edges(const fastuidraw::TessellatedPath &P);

    template<typename T>
    const fastuidraw::PainterAttributeData&
    fetch_create(float thresh, std::vector<ThreshWithData> &values);

    fastuidraw::vecN<StrokedPathSubset*, 2> m_subset;
    fastuidraw::vecN<fastuidraw::PainterAttributeData, 2> m_edges;

    PreparedAttributeData<BevelJoinCreator> m_bevel_joins;
    PreparedAttributeData<MiterClipJoinCreator> m_miter_clip_joins;
    PreparedAttributeData<MiterJoinCreator<fastuidraw::StrokedPath::offset_miter_join> > m_miter_joins;
    PreparedAttributeData<MiterJoinCreator<fastuidraw::StrokedPath::offset_miter_bevel_join> > m_miter_bevel_joins;
    PreparedAttributeData<SquareCapCreator> m_square_caps;
    PreparedAttributeData<AdjustableCapCreator> m_adjustable_caps;

    PathData m_path_data;

    std::vector<ThreshWithData> m_rounded_joins;
    std::vector<ThreshWithData> m_rounded_caps;

    bool m_empty_path;
    float m_effective_curve_distance_threshhold;
  };

}


//////////////////////////////////////////////
// JoinCount methods
JoinCount::
JoinCount(const PathData &P):
  m_number_close_joins(0),
  m_number_non_close_joins(0)
{
  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      if(P.number_edges(o) >= 2)
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

      When stroking we have four auxilary curves:
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
  if(m_det > 0.0f)
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
  if(d > 0.0f)
    {
      return -1.0f;
    }
  else
    {
      return 1.0f;
    }
}

//////////////////////////////////////////////
// EdgeStore methods
const float EdgeStore::sm_mag_tol = 0.000001f;

EdgeStore::
EdgeStore(const fastuidraw::TessellatedPath &P, PathData &path_data)
{
  std::vector<SingleSubEdge> closing_edges, non_closing_edges;
  fastuidraw::BoundingBox<float> closing_edges_bb, non_closing_edges_bb;

  path_data.m_per_contour_data.resize(P.number_contours());
  for(unsigned int o = 0, join_id = 0; o < P.number_contours(); ++o)
    {
      path_data.m_per_contour_data[o].m_edge_data_store.resize(P.number_edges(o));
      path_data.m_per_contour_data[o].m_start_contour_pt = P.unclosed_contour_point_data(o).front();
      path_data.m_per_contour_data[o].m_end_contour_pt = P.unclosed_contour_point_data(o).back();
      for(unsigned int e = 1; e + 1 < P.number_edges(o); ++e, ++join_id)
        {
          process_edge(join_id, P, path_data, o, e, non_closing_edges, non_closing_edges_bb);
        }
    }

  for(unsigned int o = 0, join_id = 0; o < P.number_contours(); ++o, join_id += 2)
    {
      if(P.number_edges(o) >= 2)
        {
          process_edge(join_id + 0, P, path_data, o, 0, non_closing_edges, non_closing_edges_bb);
          process_edge(join_id + 1, P, path_data, o, P.number_edges(o) - 1, non_closing_edges, non_closing_edges_bb);
        }
    }

  unsigned int num_closing, num_non_closing;

  num_non_closing = non_closing_edges.size();
  num_closing = closing_edges.size();
  m_all_edges.resize(num_non_closing + num_closing);
  std::copy(non_closing_edges.begin(), non_closing_edges.end(), m_all_edges.begin());
  std::copy(closing_edges.begin(), closing_edges.end(), m_all_edges.begin() + num_non_closing);

  fastuidraw::const_c_array<SingleSubEdge> tmp;
  tmp = fastuidraw::make_c_array(m_all_edges);
  m_sub_edges[false] = tmp.sub_array(0, num_non_closing);
  m_sub_edges[true] = tmp;

  m_sub_edges_bb[false] = m_sub_edges_bb[true] = non_closing_edges_bb;
  m_sub_edges_bb[true].union_box(closing_edges_bb);
}

void
EdgeStore::
process_edge(int join_id,
             const fastuidraw::TessellatedPath &P, PathData &path_data,
             unsigned int contour, unsigned int edge,
             std::vector<SingleSubEdge> &dst, fastuidraw::BoundingBox<float> &bx)
{
  fastuidraw::range_type<unsigned int> R;
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(P.point_data());
  fastuidraw::vec2 normal(1.0f, 0.0f), last_normal(1.0f, 0.0f);

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

      if(delta.magnitude() >= sm_mag_tol)
        {
          normal = fastuidraw::vec2(-delta.y(), delta.x()) / delta_magnitude;
        }
      else
        {
          delta_magnitude = 0.0;
          if(src_pts[i].m_p_t.magnitudeSq() >= sm_mag_tol * sm_mag_tol)
            {
              normal = fastuidraw::vec2(-src_pts[i].m_p_t.y(), src_pts[i].m_p_t.x());
              normal.normalize();
            }
        }

      if(i == R.m_begin)
        {
          sub_edge.m_join_id = join_id;
          sub_edge.m_bevel_lambda = 0.0f;
          sub_edge.m_has_bevel = false;
          path_data.m_per_contour_data[contour].write_edge_data(edge).m_begin_normal = normal;
          path_data.m_per_contour_data[contour].write_edge_data(edge).m_start_pt = src_pts[i];
          if(edge == 0)
            {
              path_data.m_per_contour_data[contour].m_begin_cap_normal = normal;
            }
        }
      else
        {
          sub_edge.m_join_id = -1;
          sub_edge.m_bevel_lambda = CommonJoinData::compute_lambda(last_normal, normal);
          sub_edge.m_has_bevel = true;
          sub_edge.m_bevel_normal = last_normal;
        }

      sub_edge.m_pt0 = i;
      sub_edge.m_pt1 = i + 1;
      sub_edge.m_normal = normal;
      sub_edge.m_delta = delta;

      dst.push_back(sub_edge);
      bx.union_point(src_pts[i].m_p);
      bx.union_point(src_pts[i + 1].m_p);

      last_normal = normal;
    }

  if(R.m_begin + 1 >= R.m_end)
    {
      normal = fastuidraw::vec2(-src_pts[R.m_begin].m_p_t.y(), src_pts[R.m_begin].m_p_t.x());
      normal.normalize();
      path_data.m_per_contour_data[contour].write_edge_data(edge).m_begin_normal = normal;
      path_data.m_per_contour_data[contour].write_edge_data(edge).m_start_pt = src_pts[R.m_begin];
      if(edge == 0)
        {
          path_data.m_per_contour_data[contour].m_begin_cap_normal = normal;
        }
    }

  path_data.m_per_contour_data[contour].write_edge_data(edge).m_end_normal = normal;
  path_data.m_per_contour_data[contour].write_edge_data(edge).m_end_pt = src_pts[R.m_end - 1];
  if(edge + 2 == P.number_edges(contour))
    {
      path_data.m_per_contour_data[contour].m_end_cap_normal = normal;
    }
}

//////////////////////////////////////////////////
// SubEdgeCullingHierarchy methods
SubEdgeCullingHierarchy::
SubEdgeCullingHierarchy(const fastuidraw::BoundingBox<float> &start_box,
                        fastuidraw::const_c_array<SingleSubEdge> data,
                        fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts)
{
  int c;

  FASTUIDRAWassert(!start_box.empty());
  c = choose_splitting_coordinate(start_box, data, src_pts);

  if(data.size() >= splitting_threshhold)
    {
      fastuidraw::vecN<fastuidraw::BoundingBox<float>, 2> child_boxes;
      fastuidraw::vecN<std::vector<SingleSubEdge>, 2> child_sub_edges;
      float mid_point;

      mid_point = 0.5f * (start_box.min_point()[c] + start_box.max_point()[c]);
      for(unsigned int i = 0; i < data.size(); ++i)
        {
          const SingleSubEdge &sub_edge(data[i]);
          bool sA, sB;

          sA = (src_pts[sub_edge.m_pt0].m_p[c] < mid_point);
          sB = (src_pts[sub_edge.m_pt1].m_p[c] < mid_point);
          if(sA == sB)
            {
              child_boxes[sA].union_point(src_pts[sub_edge.m_pt0].m_p);
              child_boxes[sA].union_point(src_pts[sub_edge.m_pt1].m_p);
              child_sub_edges[sA].push_back(sub_edge);
            }
          else
            {
              m_sub_edges_bb.union_point(src_pts[sub_edge.m_pt0].m_p);
              m_sub_edges_bb.union_point(src_pts[sub_edge.m_pt1].m_p);
              m_sub_edges.push_back(sub_edge);
            }
        }
      m_children[0] = create(child_boxes[0], child_sub_edges[0], src_pts);
      m_children[1] = create(child_boxes[1], child_sub_edges[1], src_pts);
    }
  else
    {
      m_children[0] = m_children[1] = nullptr;
      for(unsigned int i = 0; i < data.size(); ++i)
        {
          const SingleSubEdge &sub_edge(data[i]);
          m_sub_edges_bb.union_point(src_pts[sub_edge.m_pt0].m_p);
          m_sub_edges_bb.union_point(src_pts[sub_edge.m_pt1].m_p);
          m_sub_edges.push_back(sub_edge);
        }
    }

  if(m_children[0] != nullptr)
    {
      m_entire_bb.union_box(m_children[0]->m_entire_bb);
    }

  if(m_children[1] != nullptr)
    {
      m_entire_bb.union_box(m_children[1]->m_entire_bb);
    }

  m_entire_bb.union_box(m_sub_edges_bb);
}

SubEdgeCullingHierarchy::
~SubEdgeCullingHierarchy(void)
{
  if(m_children[0] != nullptr)
    {
      FASTUIDRAWdelete(m_children[0]);
    }

  if(m_children[1] != nullptr)
    {
      FASTUIDRAWdelete(m_children[1]);
    }
}

int
SubEdgeCullingHierarchy::
choose_splitting_coordinate(const fastuidraw::BoundingBox<float> &start_box,
                            fastuidraw::const_c_array<SingleSubEdge> data,
                            fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts)
{
  fastuidraw::vec2 mid_pt;
  fastuidraw::ivec2 counter(0, 0);
  fastuidraw::vecN<bool, 2> sA, sB;

  mid_pt = 0.5f * (start_box.min_point() + start_box.max_point());
  for(unsigned int i = 0; i < data.size(); ++i)
    {
      const SingleSubEdge &sub_edge(data[i]);
      for(unsigned int c = 0; c < 2; ++c)
        {
          sA[c] = (src_pts[sub_edge.m_pt0].m_p[c] < mid_pt[c]);
          sB[c] = (src_pts[sub_edge.m_pt1].m_p[c] < mid_pt[c]);
          if(sA[c] != sB[c])
            {
              ++counter[c];
            }
        }
    }
  return (counter[0] < counter[1]) ? 0 : 1;
}

SubEdgeCullingHierarchy*
SubEdgeCullingHierarchy::
create(const fastuidraw::BoundingBox<float> &start_box,
       const std::vector<SingleSubEdge> &data,
       fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts)
{
  if(!data.empty())
    {
      fastuidraw::const_c_array<SingleSubEdge> d;
      d = fastuidraw::make_c_array(data);
      return FASTUIDRAWnew SubEdgeCullingHierarchy(start_box, d, src_pts);
    }
  else
    {
      return nullptr;
    }
}

////////////////////////////////////////////
// StrokedPathSubset methods
StrokedPathSubset::
~StrokedPathSubset()
{
  if(m_children[0] != nullptr)
    {
      FASTUIDRAWdelete(m_children[0]);
    }
  if(m_children[1] != nullptr)
    {
      FASTUIDRAWdelete(m_children[1]);
    }
}

StrokedPathSubset::
StrokedPathSubset(unsigned int vertex_st, unsigned int index_st,
                  SubEdgeCullingHierarchy *src, unsigned int &total_chunks,
                  unsigned int depth, unsigned int join_depth, unsigned int cap_depth):
  m_children(nullptr, nullptr)
{
  unsigned int index_cnt, vertex_cnt, depth_cnt;

  count_vertices_indices(src, vertex_cnt, index_cnt, depth_cnt, join_depth, cap_depth);

  /* we want the depth values of this to come after the children (so
     it is drawn below), but we want it to be drawn before, thus
     we set the index and vertex buffers locations to before adding
     the children, but set the depths after adding the children.
   */
  m_vertex_data_range_with_children.m_begin = vertex_st;
  m_index_data_range_with_children.m_begin = index_st;
  m_vertex_data_range.m_begin = vertex_st;
  m_vertex_data_range.m_end = vertex_st + vertex_cnt;
  m_index_data_range.m_begin = index_st;
  m_index_data_range.m_end = index_st + index_cnt;

  m_depth_with_children.m_begin = depth;
  m_joins_depth.m_begin = join_depth;
  m_caps_depth.m_begin = cap_depth;

  vertex_st += vertex_cnt;
  index_st += index_cnt;
  
  if(src->m_children[0] != nullptr)
    {
      m_children[0] = FASTUIDRAWnew StrokedPathSubset(vertex_st, index_st, src->m_children[0],
                                                      total_chunks, depth, join_depth, cap_depth);
      vertex_st = m_children[0]->m_vertex_data_range_with_children.m_end;
      index_st = m_children[0]->m_index_data_range_with_children.m_end;
      depth = m_children[0]->m_depth_with_children.m_end;
      join_depth = m_children[0]->m_joins_depth.m_end;
      cap_depth = m_children[0]->m_caps_depth.m_end;
    }

  if(src->m_children[1] != nullptr)
    {
      m_children[1] = FASTUIDRAWnew StrokedPathSubset(vertex_st, index_st, src->m_children[1],
                                                      total_chunks, depth, join_depth, cap_depth);
      vertex_st = m_children[1]->m_vertex_data_range_with_children.m_end;
      index_st = m_children[1]->m_index_data_range_with_children.m_end;
      depth = m_children[1]->m_depth_with_children.m_end;
      join_depth = m_children[1]->m_joins_depth.m_end;
      cap_depth = m_children[1]->m_caps_depth.m_end;
    }
  
  m_data_chunk = total_chunks;
  m_data_bb = src->m_sub_edges_bb;
  m_data_src = fastuidraw::make_c_array(src->m_sub_edges);
  m_depth.m_begin = depth;
  m_depth.m_end = depth + depth_cnt;

  m_joins_depth.m_begin = join_depth;
  m_joins_depth.m_end = join_depth + join_depth;
  m_caps_depth.m_begin = cap_depth;
  m_caps_depth.m_end = cap_depth + cap_depth;

  m_data_chunk_with_children = total_chunks + 1;
  m_data_with_children_bb = src->m_entire_bb;
  m_vertex_data_range_with_children.m_end = vertex_st;
  m_index_data_range_with_children.m_end = index_st;
  m_depth_with_children.m_end = m_depth.m_end;

  total_chunks += 2u;
}

void
StrokedPathSubset::
dump(std::ostream &str, int depth) const
{
  std::string tabs(depth, '\t');
  str << tabs << this << ":\n"
      << tabs << "\tdepth = " << m_depth << "\n"
      << tabs << "\tdepth_children = " << m_depth_with_children << "\n"
      << tabs << "\tvertex_range = " << m_vertex_data_range << "\n"
      << tabs << "\tvertex_range_children = " << m_vertex_data_range_with_children << "\n"
      << tabs << "\tindex_range = " << m_index_data_range << "\n"
      << tabs << "\tindex_range_children = " << m_index_data_range_with_children << "\n";
  if(m_children[0])
    {
      m_children[0]->dump(str, depth + 1);
    }
  if(m_children[1])
    {
      m_children[1]->dump(str, depth + 1);
    }
}

void
StrokedPathSubset::
count_vertices_indices(SubEdgeCullingHierarchy *src,
                       unsigned int &vertex_cnt,
                       unsigned int &index_cnt,
                       unsigned int &depth_cnt,
                       unsigned int &join_depth,
                       unsigned int &cap_depth)
{
  vertex_cnt = 0u;
  index_cnt = 0u;
  depth_cnt = src->m_sub_edges.size();
  join_depth = 0;
  cap_depth = 0;
  for(unsigned int i = 0, endi = depth_cnt; i < endi; ++i)
    {
      const SingleSubEdge &v(src->m_sub_edges[i]);
      if(v.m_has_bevel)
        {
          vertex_cnt += 3;
          index_cnt += 3;
        }

      if(v.m_join_id != -1)
        {
          Flopper entry;

          entry.m_chunk = join_depth;
          entry.m_depth_value = join_depth;
          entry.m_join_id = v.m_join_id;
          m_joins.push_back(entry);
          ++join_depth;
        }

      vertex_cnt += points_per_segment;
      index_cnt += indices_per_segment_without_bevel;
    }
}

void
StrokedPathSubset::
compute_chunks(ScratchSpacePrivate &scratch,
               const fastuidraw::DashEvaluatorBase *dash_evaluator,
               fastuidraw::const_c_array<fastuidraw::vec3> clip_equations,
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

  dst.m_edge_chunks.clear();
  dst.m_join_chunks.clear();
  dst.m_cap_chunks.clear();

  compute_chunks_implement(scratch, item_space_additional_room,
                           max_attribute_cnt, max_index_cnt, dst);
}

void
StrokedPathSubset::
compute_chunks_take_all(unsigned int max_attribute_cnt,
                        unsigned int max_index_cnt,
                        ChunkSetPrivate &dst)
{
  if(m_vertex_data_range_with_children.difference() <= max_attribute_cnt
     && m_index_data_range_with_children.difference() <= max_index_cnt)
    {
      dst.m_edge_chunks.push_back(m_data_chunk_with_children);
    }
  else
    {
      if(m_vertex_data_range.difference() <= max_attribute_cnt
         && m_index_data_range.difference() <= max_index_cnt)
        {
          dst.m_edge_chunks.push_back(m_data_chunk);
        }
      else
        {
          FASTUIDRAWassert(!"StrokedPath: Edge chunk has too many attribute and indices");
        }

      if(m_children[0] != nullptr)
        {
          m_children[0]->compute_chunks_take_all(max_attribute_cnt, max_index_cnt, dst);
        }

      if(m_children[1] != nullptr)
        {
          m_children[1]->compute_chunks_take_all(max_attribute_cnt, max_index_cnt, dst);
        }
    }
}

void
StrokedPathSubset::
compute_chunks_implement(ScratchSpacePrivate &scratch,
                         float item_space_additional_room,
                         unsigned int max_attribute_cnt,
                         unsigned int max_index_cnt,
                         ChunkSetPrivate &dst)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;

  if(m_data_with_children_bb.empty())
    {
      return;
    }

  /* clip the bounding box of this StrokedPathSubset
   */
  vecN<vec2, 4> bb;
  bool unclipped;

  m_data_with_children_bb.inflated_polygon(bb, item_space_additional_room);
  unclipped = clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                  bb, scratch.m_clipped_rect,
                                  scratch.m_clip_scratch_floats,
                                  scratch.m_clip_scratch_vec2s);
  //completely unclipped.
  if(unclipped)
    {
      compute_chunks_take_all(max_attribute_cnt, max_index_cnt, dst);
      return;
    }

  //completely clipped
  if(scratch.m_clipped_rect.empty())
    {
      return;
    }

  if(m_children[0] != nullptr)
    {
      m_children[0]->compute_chunks_implement(scratch, item_space_additional_room,
                                              max_attribute_cnt, max_index_cnt,
                                              dst);
    }

  if(m_children[1] != nullptr)
    {
      m_children[1]->compute_chunks_implement(scratch, item_space_additional_room,
                                              max_attribute_cnt, max_index_cnt,
                                              dst);
    }

  if(!m_data_bb.empty())
    {
      m_data_bb.inflated_polygon(bb, item_space_additional_room);
      clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                          bb, scratch.m_clipped_rect,
                          scratch.m_clip_scratch_floats,
                          scratch.m_clip_scratch_vec2s);

      if(!scratch.m_clipped_rect.empty())
        {
          dst.m_edge_chunks.push_back(m_data_chunk);
        }
    }
}

////////////////////////////////////////
// EdgeAttributeFiller methods
EdgeAttributeFiller::
EdgeAttributeFiller(const StrokedPathSubset *src,
                    const fastuidraw::TessellatedPath &P):
  m_src(src),
  m_P(P)
{
  m_src->dump(std::cout);
}

void
EdgeAttributeFiller::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_ranges) const
{
  /*
    the chunk at m_data_chunk_with_children of the root node
    is the LAST chunk at it contains all of the data of the
    edge data.
   */
  num_attribute_chunks = num_index_chunks = number_z_ranges = m_src->data_chunk_with_children() + 1;
  num_attributes = m_src->vertex_data_range_with_children().m_end;
  num_indices = m_src->index_data_range_with_children().m_end;
}

void
EdgeAttributeFiller::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
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
                 fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
                 fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                 fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
                 fastuidraw::c_array<int> index_adjusts) const
{
  if(e->child(0) != nullptr)
    {
      fill_data_worker(e->child(0), attribute_data, index_data,
                       attribute_chunks, index_chunks, zranges, index_adjusts);
    }

  if(e->child(1) != nullptr)
    {
      fill_data_worker(e->child(1), attribute_data, index_data,
                       attribute_chunks, index_chunks, zranges, index_adjusts);
    }

  fastuidraw::c_array<fastuidraw::PainterAttribute> ad;
  fastuidraw::c_array<fastuidraw::PainterIndex> id;
  unsigned int K;
  fastuidraw::range_type<unsigned int> vertex_data_range;
  fastuidraw::range_type<unsigned int> index_data_range;
  fastuidraw::range_type<unsigned int> depth;
  fastuidraw::const_c_array<SingleSubEdge> data_src;
  
  vertex_data_range = e->vertex_data_range();
  index_data_range = e->index_data_range();
  depth = e->depth_range();
  data_src = e->data_src();
  K = e->data_chunk();

  ad = attribute_data.sub_array(vertex_data_range);
  id = index_data.sub_array(index_data_range);
  attribute_chunks[K] = ad;
  index_chunks[K] = id;
  index_adjusts[K] = -int(vertex_data_range.m_begin);
  zranges[K] = fastuidraw::range_type<int>(depth.m_begin, depth.m_end);

  /* these elements are drawn AFTER the child elements,
     therefor they need to have a smaller depth
   */
  for(unsigned int k = 0,
        d = depth.m_end - 1,
        v = vertex_data_range.m_begin,
        i = index_data_range.m_begin;
      k < data_src.size(); ++k, --d)
    {
      process_sub_edge(data_src[k], d, attribute_data, index_data, v, i);
    }

  for(unsigned int v = vertex_data_range.m_begin; v < vertex_data_range.m_end; ++v)
    {
      fastuidraw::StrokedPath::point P;
      fastuidraw::StrokedPath::point::unpack_point(&P, attribute_data[v]);
      FASTUIDRAWassert(P.depth() >= depth.m_begin);
      FASTUIDRAWassert(P.depth() < depth.m_end);
    }

  K = e->data_chunk_with_children();
  vertex_data_range = e->vertex_data_range_with_children();
  index_data_range = e->index_data_range_with_children();
  depth = e->depth_range_with_children();

  ad = attribute_data.sub_array(vertex_data_range);
  id = index_data.sub_array(index_data_range);
  attribute_chunks[K] = ad;
  index_chunks[K] = id;
  index_adjusts[K] = -int(vertex_data_range.m_begin);
  zranges[K] = fastuidraw::range_type<int>(depth.m_begin, depth.m_end);

  for(unsigned int v = vertex_data_range.m_begin; v < vertex_data_range.m_end; ++v)
    {
      fastuidraw::StrokedPath::point P;
      fastuidraw::StrokedPath::point::unpack_point(&P, attribute_data[v]);
      FASTUIDRAWassert(P.depth() >= depth.m_begin);
      FASTUIDRAWassert(P.depth() < depth.m_end);
    }
}

void
EdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                 unsigned int &vert_offset, unsigned int &index_offset) const
{
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(m_P.point_data());
  const int boundary_values[3] = { 1, 1, 0 };
  const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
  fastuidraw::vecN<fastuidraw::StrokedPath::point, 6> pts;

  if(sub_edge.m_has_bevel)
    {
      indices[index_offset + 0] = vert_offset + 0;
      indices[index_offset + 1] = vert_offset + 1;
      indices[index_offset + 2] = vert_offset + 2;
      index_offset += 3;

      for(unsigned int k = 0; k < 3; ++k)
        {
          pts[k].m_position = src_pts[sub_edge.m_pt0].m_p;
          pts[k].m_distance_from_edge_start = src_pts[sub_edge.m_pt0].m_distance_from_edge_start;
          pts[k].m_distance_from_contour_start = src_pts[sub_edge.m_pt0].m_distance_from_contour_start;
          pts[k].m_edge_length = src_pts[sub_edge.m_pt0].m_edge_length;
          pts[k].m_open_contour_length = src_pts[sub_edge.m_pt0].m_open_contour_length;
          pts[k].m_closed_contour_length = src_pts[sub_edge.m_pt0].m_closed_contour_length;
          pts[k].m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
        }

      pts[0].m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
      pts[0].m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_start_sub_edge, depth)
        | fastuidraw::StrokedPath::bevel_edge_mask;

      pts[1].m_pre_offset = sub_edge.m_bevel_lambda * sub_edge.m_bevel_normal;
      pts[1].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_start_sub_edge, depth)
        | fastuidraw::StrokedPath::bevel_edge_mask;

      pts[2].m_pre_offset = sub_edge.m_bevel_lambda * sub_edge.m_normal;
      pts[2].m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_start_sub_edge, depth)
        | fastuidraw::StrokedPath::bevel_edge_mask;

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
      pts[k].m_position = src_pts[sub_edge.m_pt0].m_p;
      pts[k].m_distance_from_edge_start = src_pts[sub_edge.m_pt0].m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = src_pts[sub_edge.m_pt0].m_distance_from_contour_start;
      pts[k].m_edge_length = src_pts[sub_edge.m_pt0].m_edge_length;
      pts[k].m_open_contour_length = src_pts[sub_edge.m_pt0].m_open_contour_length;
      pts[k].m_closed_contour_length = src_pts[sub_edge.m_pt0].m_closed_contour_length;
      pts[k].m_pre_offset = normal_sign[k] * sub_edge.m_normal;
      pts[k].m_auxilary_offset = sub_edge.m_delta;
      pts[k].m_packed_data = pack_data(boundary_values[k],
                                       fastuidraw::StrokedPath::offset_start_sub_edge,
                                       depth);

      pts[k + 3].m_position = src_pts[sub_edge.m_pt1].m_p;
      pts[k + 3].m_distance_from_edge_start = src_pts[sub_edge.m_pt1].m_distance_from_edge_start;
      pts[k + 3].m_distance_from_contour_start = src_pts[sub_edge.m_pt1].m_distance_from_contour_start;
      pts[k + 3].m_edge_length = src_pts[sub_edge.m_pt1].m_edge_length;
      pts[k + 3].m_open_contour_length = src_pts[sub_edge.m_pt1].m_open_contour_length;
      pts[k + 3].m_closed_contour_length = src_pts[sub_edge.m_pt1].m_closed_contour_length;
      pts[k + 3].m_pre_offset = normal_sign[k] * sub_edge.m_normal;
      pts[k + 3].m_auxilary_offset = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = pack_data(boundary_values[k],
                                           fastuidraw::StrokedPath::offset_end_sub_edge,
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

/////////////////////////////////////////////////
// JoinCreatorBase methods
JoinCreatorBase::
JoinCreatorBase(const PathData &P):
  m_P(P),
  m_num_non_closed_verts(0u),
  m_num_non_closed_indices(0u),
  m_num_closed_verts(0u),
  m_num_closed_indices(0u),
  m_num_joins(0u),
  m_num_joins_without_closing_edge(0u),
  m_post_ctor_initalized_called(false)
{}

void
JoinCreatorBase::
post_ctor_initalize(void)
{
  FASTUIDRAWassert(!m_post_ctor_initalized_called);
  m_post_ctor_initalized_called = true;
  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      for(unsigned int e = 1; e + 1 < m_P.number_edges(o); ++e, ++m_num_joins)
        {
          add_join(m_num_joins, m_P,
                   m_P.m_per_contour_data[o].edge_data(e - 1).m_end_normal,
                   m_P.m_per_contour_data[o].edge_data(e).m_begin_normal,
                   o, e, m_num_non_closed_verts, m_num_non_closed_indices);
        }
    }

  m_num_joins_without_closing_edge = m_num_joins;

  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      if(m_P.number_edges(o) >= 2)
        {
          add_join(m_num_joins, m_P,
                   m_P.m_per_contour_data[o].edge_data(m_P.number_edges(o) - 2).m_end_normal,
                   m_P.m_per_contour_data[o].edge_data(m_P.number_edges(o) - 1).m_begin_normal,
                   o, m_P.number_edges(o) - 1,
                   m_num_closed_verts, m_num_closed_indices);

          add_join(m_num_joins + 1, m_P,
                   m_P.m_per_contour_data[o].m_edge_data_store.back().m_end_normal,
                   m_P.m_per_contour_data[o].m_edge_data_store.front().m_begin_normal,
                   o, m_P.number_edges(o),
                   m_num_closed_verts, m_num_closed_indices);

          m_num_joins += 2;
        }
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
  num_attributes = m_num_non_closed_verts + m_num_closed_verts;
  num_indices = m_num_non_closed_indices + m_num_closed_indices;
  num_attribute_chunks = num_index_chunks = m_num_joins + 2;
  number_z_ranges = 2;
}

void
JoinCreatorBase::
fill_join(unsigned int join_id,
          unsigned int contour, unsigned int edge,
          fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
          fastuidraw::c_array<unsigned int> indices,
          unsigned int &vertex_offset, unsigned int &index_offset,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int v(vertex_offset), i(index_offset);
  unsigned int depth, K;

  FASTUIDRAWassert(join_id < m_num_joins);
  depth = m_num_joins - 1 - join_id;
  fill_join_implement(join_id, m_P, contour, edge, pts, depth, indices, vertex_offset, index_offset);

  K = join_id + fastuidraw::StrokedPath::join_chunk_start_individual_joins;
  attribute_chunks[K] = pts.sub_array(v, vertex_offset - v);
  index_chunks[K] = indices.sub_array(i, index_offset - i);
  index_adjusts[K] = -int(v);
}

void
JoinCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0), index_offset(0), join_id(0);

  FASTUIDRAWassert(attribute_data.size() == m_num_non_closed_verts + m_num_closed_verts);
  FASTUIDRAWassert(index_data.size() == m_num_non_closed_indices + m_num_closed_indices);

  index_adjusts[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = 0;
  zranges[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = fastuidraw::range_type<int>(0, m_num_joins_without_closing_edge);
  attribute_chunks[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = attribute_data.sub_array(0, m_num_non_closed_verts);
  index_chunks[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = index_data.sub_array(0, m_num_non_closed_indices);

  index_adjusts[fastuidraw::StrokedPath::join_chunk_with_closing_edge] = 0;
  zranges[fastuidraw::StrokedPath::join_chunk_with_closing_edge] = fastuidraw::range_type<int>(0, m_num_joins);
  attribute_chunks[fastuidraw::StrokedPath::join_chunk_with_closing_edge] = attribute_data.sub_array(0, m_num_non_closed_verts + m_num_closed_verts);
  index_chunks[fastuidraw::StrokedPath::join_chunk_with_closing_edge] = index_data.sub_array(0, m_num_non_closed_indices + m_num_closed_indices);

  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      for(unsigned int e = 1; e + 1 < m_P.number_edges(o); ++e, ++join_id)
        {
          fill_join(join_id, o, e,
                    attribute_data, index_data,
                    vertex_offset, index_offset,
                    attribute_chunks, index_chunks,
                    index_adjusts);
        }
    }
  FASTUIDRAWassert(vertex_offset == m_num_non_closed_verts);
  FASTUIDRAWassert(index_offset == m_num_non_closed_indices);

  for(unsigned int o = 0; o < m_P.number_contours(); ++o)
    {
      if(m_P.number_edges(o) >= 2)
        {
          fill_join(join_id, o, m_P.number_edges(o) - 1,
                    attribute_data, index_data,
                    vertex_offset, index_offset,
                    attribute_chunks, index_chunks,
                    index_adjusts);

          fill_join(join_id + 1, o, m_P.number_edges(o),
                    attribute_data, index_data,
                    vertex_offset, index_offset,
                    attribute_chunks, index_chunks,
                    index_adjusts);

          join_id += 2;
        }
    }
  FASTUIDRAWassert(vertex_offset == m_num_non_closed_verts + m_num_closed_verts);
  FASTUIDRAWassert(index_offset == m_num_non_closed_indices + m_num_closed_indices);
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = m_p0;
  pt.m_pre_offset = m_lambda * m_n0;
  pt.m_distance_from_edge_start = m_distance_from_edge_start;
  pt.m_distance_from_contour_start = m_distance_from_contour_start;
  pt.m_edge_length = m_edge_length;
  pt.m_open_contour_length = m_open_contour_length;
  pt.m_closed_contour_length = m_closed_contour_length;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
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
      pt.m_auxilary_offset = fastuidraw::vec2(t, cs_as_complex.real());
      pt.m_distance_from_edge_start = m_distance_from_edge_start;
      pt.m_distance_from_contour_start = m_distance_from_contour_start;
      pt.m_edge_length = m_edge_length;
      pt.m_open_contour_length = m_open_contour_length;
      pt.m_closed_contour_length = m_closed_contour_length;
      pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_rounded_join, depth);

      if(m_lambda * m_n0.y() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPath::normal0_y_sign_mask;
        }

      if(m_lambda * m_n1.y() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPath::normal1_y_sign_mask;
        }

      if(cs_as_complex.imag() < 0.0f)
        {
          pt.m_packed_data |= fastuidraw::StrokedPath::sin_sign_mask;
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// RoundedJoinCreator methods
RoundedJoinCreator::
RoundedJoinCreator(const PathData &P, float thresh):
  JoinCreatorBase(P),
  m_thresh(thresh)
{
  JoinCount J(P);
  m_per_join_data.reserve(J.m_number_close_joins + J.m_number_non_close_joins);
  post_ctor_initalize();
}

void
RoundedJoinCreator::
add_join(unsigned int join_id,
         const PathData &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count) const
{
  (void)join_id;
  PerJoinData J(path.m_per_contour_data[contour].edge_data(edge - 1).m_end_pt,
                path.m_per_contour_data[contour].edge_data(edge).m_start_pt,
                n0_from_stroking, n1_from_stroking, m_thresh);

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
                    const PathData &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  (void)contour;
  (void)edge;
  (void)path;

  FASTUIDRAWassert(join_id < m_per_join_data.size());
  m_per_join_data[join_id].add_data(depth, pts, vertex_offset, indices, index_offset);
}

///////////////////////////////////////////////////
// BevelJoinCreator methods
BevelJoinCreator::
BevelJoinCreator(const PathData &P):
  JoinCreatorBase(P)
{
  post_ctor_initalize();
}

void
BevelJoinCreator::
add_join(unsigned int join_id,
         const PathData &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count) const
{
  (void)join_id;
  (void)path;
  (void)contour;
  (void)edge;

  /* one triangle per bevel join
   */
  vert_count += 3;
  index_count += 3;

  m_n0.push_back(n0_from_stroking);
  m_n1.push_back(n1_from_stroking);
}

void
BevelJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PathData &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  const fastuidraw::TessellatedPath::point &prev_pt(path.m_per_contour_data[contour].edge_data(edge - 1).m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(path.m_per_contour_data[contour].edge_data(edge).m_start_pt);
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 0]);

  pt.m_position = J.m_p0;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 1]);

  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_lambda * J.m_n1;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset + 2]);

  add_triangle_fan(vertex_offset, vertex_offset + 3, indices, index_offset);

  vertex_offset += 3;
}

///////////////////////////////////////////////////
// MiterClipJoinCreator methods
MiterClipJoinCreator::
MiterClipJoinCreator(const PathData &P):
  JoinCreatorBase(P)
{
  post_ctor_initalize();
}

void
MiterClipJoinCreator::
add_join(unsigned int join_id,
         const PathData &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count) const
{
  /* Each join is a triangle fan from 5 points
     (thus 3 triangles, which is 9 indices)
   */
  (void)join_id;
  (void)path;
  (void)contour;
  (void)edge;

  vert_count += 5;
  index_count += 9;

  m_n0.push_back(n0_from_stroking);
  m_n1.push_back(n1_from_stroking);
}


void
MiterClipJoinCreator::
fill_join_implement(unsigned int join_id,
                    const PathData &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join_id);

  const fastuidraw::TessellatedPath::point &prev_pt(path.m_per_contour_data[contour].edge_data(edge - 1).m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(path.m_per_contour_data[contour].edge_data(edge).m_start_pt);
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve into join
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_lambda * J.m_n0;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point A
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_n0;
  pt.m_auxilary_offset = J.m_n1;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_miter_clip_join, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point B
  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_n1;
  pt.m_auxilary_offset = J.m_n0;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_miter_clip_join_lambda_negated, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve out from join
  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_lambda * J.m_n1;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

////////////////////////////////////
// MiterJoinCreator methods
template<enum fastuidraw::StrokedPath::offset_type_t tp>
MiterJoinCreator<tp>::
MiterJoinCreator(const PathData &P):
  JoinCreatorBase(P)
{
  post_ctor_initalize();
}

template<enum fastuidraw::StrokedPath::offset_type_t tp>
void
MiterJoinCreator<tp>::
add_join(unsigned int join_id,
         const PathData &path,
         const fastuidraw::vec2 &n0_from_stroking,
         const fastuidraw::vec2 &n1_from_stroking,
         unsigned int contour, unsigned int edge,
         unsigned int &vert_count, unsigned int &index_count) const
{
  /* Each join is a triangle fan from 4 points
     (thus 2 triangles, which is 6 indices)
   */
  (void)join_id;
  (void)path;
  (void)contour;
  (void)edge;

  vert_count += 4;
  index_count += 6;

  m_n0.push_back(n0_from_stroking);
  m_n1.push_back(n1_from_stroking);
}

template<enum fastuidraw::StrokedPath::offset_type_t tp>
void
MiterJoinCreator<tp>::
fill_join_implement(unsigned int join_id,
                    const PathData &path,
                    unsigned int contour, unsigned int edge,
                    fastuidraw::c_array<fastuidraw::PainterAttribute> pts,
                    unsigned int depth,
                    fastuidraw::c_array<unsigned int> indices,
                    unsigned int &vertex_offset, unsigned int &index_offset) const
{
  FASTUIDRAWunused(join_id);

  const fastuidraw::TessellatedPath::point &prev_pt(path.m_per_contour_data[contour].edge_data(edge - 1).m_end_pt);
  const fastuidraw::TessellatedPath::point &next_pt(path.m_per_contour_data[contour].edge_data(edge).m_start_pt);
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(0, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // join point from curve into join
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_lambda * J.m_n0;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point
  pt.m_position = J.m_p0;
  pt.m_pre_offset = J.m_n0;
  pt.m_auxilary_offset = J.m_n1;
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}
  

///////////////////////////////////////////////
// CapCreatorBase methods
CapCreatorBase::
CapCreatorBase(const PathData &P, PointIndexCapSize sz):
  m_P(P),
  m_size(sz)
{}

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
  number_z_ranges = num_attribute_chunks = num_index_chunks = 1;
}

void
CapCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<fastuidraw::range_type<int> > zranges,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0u), index_offset(0u), depth;

  depth = 2 * m_P.number_contours();
  for(unsigned int o = 0; o < m_P.number_contours(); ++o, depth -= 2u)
    {
      FASTUIDRAWassert(depth >= 2);
      add_cap(m_P.m_per_contour_data[o].m_begin_cap_normal,
              true, depth - 1, m_P.m_per_contour_data[o].m_start_contour_pt,
              attribute_data, index_data,
              vertex_offset, index_offset);

      add_cap(m_P.m_per_contour_data[o].m_end_cap_normal,
              false, depth - 2, m_P.m_per_contour_data[o].m_end_contour_pt,
              attribute_data, index_data,
              vertex_offset, index_offset);
    }

  FASTUIDRAWassert(vertex_offset == m_size.m_verts);
  FASTUIDRAWassert(index_offset == m_size.m_indices);
  attribute_chunks[0] = attribute_data;
  index_chunks[0] = index_data;
  zranges[0] = fastuidraw::range_type<int>(0, 2 * m_P.number_contours());
  index_adjusts[0] = 0;
}

///////////////////////////////////////////////////
// RoundedCapCreator methods
RoundedCapCreator::
RoundedCapCreator(const PathData &P, float thresh):
  CapCreatorBase(P, compute_size(P, thresh))
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  for(i = 1, theta = m_delta_theta; i < m_num_arc_points_per_cap - 1; ++i, theta += m_delta_theta, ++vertex_offset)
    {
      float s, c;

      s = std::sin(theta);
      c = std::cos(theta);
      pt.m_position = C.m_p;
      pt.m_pre_offset = C.m_n;
      pt.m_auxilary_offset = fastuidraw::vec2(s, c);
      pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
      pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
      pt.m_edge_length = p.m_edge_length;
      pt.m_open_contour_length = p.m_open_contour_length;
      pt.m_closed_contour_length = p.m_closed_contour_length;
      pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_rounded_cap, depth);
      pt.pack_point(&pts[vertex_offset]);
    }

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
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
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(0, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxilary_offset = C.m_v;
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_square_cap, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxilary_offset = C.m_v;
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_square_cap, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_auxilary_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_packed_data = pack_data(1, fastuidraw::StrokedPath::offset_shared_with_edge, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  add_triangle_fan(first, vertex_offset, indices, index_offset);
}

//////////////////////////////////////
// AdjustableCapCreator methods
PointIndexCapSize
AdjustableCapCreator::
compute_size(const PathData &P)
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
         enum fastuidraw::StrokedPath::offset_type_t type,
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
  pt.m_auxilary_offset = C.m_v;
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
  pt.m_auxilary_offset = C.m_v;
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
  pt.m_auxilary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(1, type, depth) | fastuidraw::StrokedPath::adjustable_cap_ending_mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = fastuidraw::vec2(0.0f, 0.0f);
  pt.m_auxilary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(0, type, depth) | fastuidraw::StrokedPath::adjustable_cap_ending_mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_auxilary_offset = C.m_v;
  pt.m_distance_from_edge_start = p.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = p.m_distance_from_contour_start;
  pt.m_edge_length = p.m_edge_length;
  pt.m_open_contour_length = p.m_open_contour_length;
  pt.m_closed_contour_length = p.m_closed_contour_length;
  pt.m_packed_data = pack_data(1, type, depth) | fastuidraw::StrokedPath::adjustable_cap_ending_mask;
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  pt.m_position = C.m_p;
  pt.m_pre_offset = -C.m_n;
  pt.m_auxilary_offset = C.m_v;
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
  enum fastuidraw::StrokedPath::offset_type_t tp;
  tp = (is_starting_cap) ?
    fastuidraw::StrokedPath::offset_adjustable_cap_contour_start :
    fastuidraw::StrokedPath::offset_adjustable_cap_contour_end;

  pack_fan(is_starting_cap, tp,
           p0, normal_from_stroking, depth,
           pts, vertex_offset, indices, index_offset);
}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(const fastuidraw::TessellatedPath &P)
{
  if(!P.point_data().empty())
    {
      m_empty_path = false;
      create_edges(P);
      m_effective_curve_distance_threshhold = P.effective_curve_distance_threshhold();
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
      m_effective_curve_distance_threshhold = 0.0f;
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

  if(!m_empty_path)
    {
      FASTUIDRAWdelete(m_subset[0]);
      FASTUIDRAWdelete(m_subset[1]);
    }
}

void
StrokedPathPrivate::
create_edges(const fastuidraw::TessellatedPath &P)
{
  EdgeStore edge_store(P, m_path_data);

  FASTUIDRAWassert(!m_empty_path);
  for(unsigned int i = 0; i < 2; ++i)
    {
      SubEdgeCullingHierarchy *s;
      s = FASTUIDRAWnew SubEdgeCullingHierarchy(edge_store.bounding_box(i != 0),
                                                edge_store.sub_edges(i != 0),
                                                P.point_data());
      m_subset[i] = StrokedPathSubset::create(s);
      m_edges[i].set_data(EdgeAttributeFiller(m_subset[i], P));
      FASTUIDRAWdelete(s);
    }
}

template<typename T>
const fastuidraw::PainterAttributeData&
StrokedPathPrivate::
fetch_create(float thresh, std::vector<ThreshWithData> &values)
{
  if(values.empty())
    {
      fastuidraw::PainterAttributeData *newD;
      newD = FASTUIDRAWnew fastuidraw::PainterAttributeData();
      newD->set_data(T(m_path_data, 1.0f));
      values.push_back(ThreshWithData(newD, 1.0f));
    }

  /* we set a hard tolerance of 1e-6. Should we
     set it as a ratio of the bounding box of
     the underlying tessellated path?
   */
  thresh = fastuidraw::t_max(thresh, float(1e-6));
  if(values.back().m_thresh <= thresh)
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
          newD->set_data(T(m_path_data, t));
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
                             m_auxilary_offset.x(),
                             m_auxilary_offset.y());

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
  dst->m_auxilary_offset.x() = unpack_float(a.m_attrib1.z());
  dst->m_auxilary_offset.y() = unpack_float(a.m_attrib1.w());

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
      return m_pre_offset + m_auxilary_offset;
          
    case offset_rounded_cap:
      {
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        return m_auxilary_offset.x() * v + m_auxilary_offset.y() * n;
      }

    case offset_miter_clip_join:
    case offset_miter_clip_join_lambda_negated:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxilary_offset), Jn1(n1.y(), -n1.x());
        float r, det, lambda;

        det = dot(Jn1, n0);
        lambda = -t_sign(det);
        r = (det != 0.0f) ? (dot(n0, n1) - 1.0f) / det : 0.0f;
      
        if(tp == offset_miter_clip_join_lambda_negated)
          {
            lambda = -lambda;
          }

        return lambda * (n0 + r * Jn0);
      }

    case offset_miter_bevel_join:
    case offset_miter_join:
      {
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxilary_offset), Jn1(n1.y(), -n1.x());
        float r, lambda, den;

        lambda = t_sign(dot(Jn0, n1));
        den = 1.0f + dot(n0, n1);
        r = (den != 0.0f) ? lambda / den : 0.0f;
        return r * (n0 + n1);
      }
          
    case offset_rounded_join:
      {
        vec2 cs;

        cs.x() = m_auxilary_offset.y();
        cs.y() = sqrt(1.0 - cs.x() * cs.x());
        if(m_packed_data & sin_sign_mask)
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
        vec2 n1(m_auxilary_offset), Jn1(n1.y(), -n1.x());
        float r, det;

        det = dot(Jn1, n0);
        r = (det != 0.0f) ? (dot(n0, n1) - 1.0f) / det : 0.0f;
        return t_sqrt(1.0f + r * r);
      }

    case offset_miter_bevel_join:
    case offset_miter_join:
      {
        vec2 n0(m_pre_offset), n1(m_auxilary_offset);
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

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::ChunkSet::
edge_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return make_c_array(d->m_edge_chunks);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::ChunkSet::
join_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return make_c_array(d->m_join_chunks);
}

fastuidraw::const_c_array<unsigned int>
fastuidraw::StrokedPath::ChunkSet::
cap_chunks(void) const
{
  ChunkSetPrivate *d;
  d = static_cast<ChunkSetPrivate*>(m_d);
  return make_c_array(d->m_cap_chunks);
}


//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methods
fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  FASTUIDRAWassert(number_offset_types < FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(offset_type_num_bits));
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

float
fastuidraw::StrokedPath::
effective_curve_distance_threshhold(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_effective_curve_distance_threshhold;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
edges(bool include_closing_edges) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_edges[include_closing_edges];
}

void
fastuidraw::StrokedPath::
compute_chunks(ScratchSpace &scratch_space,
               const DashEvaluatorBase *dash_evaluator,
               const_c_array<vec3> clip_equations,
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

  d->m_subset[include_closing_edges]->compute_chunks(*scratch_space_ptr,
                                                     dash_evaluator,
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

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
square_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_square_caps.data(d->m_path_data);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
adjustable_caps(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_adjustable_caps.data(d->m_path_data);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_bevel_joins.data(d->m_path_data);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_clip_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_clip_joins.data(d->m_path_data);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_bevel_joins.data(d->m_path_data);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_joins.data(d->m_path_data);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_joins(float thresh) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);

  return (!d->m_empty_path) ?
    d->fetch_create<RoundedJoinCreator>(thresh, d->m_rounded_joins) :
    d->m_bevel_joins.data(d->m_path_data);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_caps(float thresh) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return (!d->m_empty_path) ?
    d->fetch_create<RoundedCapCreator>(thresh, d->m_rounded_caps) :
    d->m_square_caps.data(d->m_path_data);
}
