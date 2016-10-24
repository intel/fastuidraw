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

#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/path.hpp>
#include <fastuidraw/painter/stroked_path.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler.hpp>
#include "../private/util_private.hpp"
#include "../private/path_util_private.hpp"
#include "../private/clip.hpp"

namespace
{
  uint32_t
  pack_data(int on_boundary,
            enum fastuidraw::StrokedPath::offset_type_t pt,
            uint32_t depth)
  {
    assert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);
    return fastuidraw::pack_bits(fastuidraw::StrokedPath::offset_type_bit0, fastuidraw::StrokedPath::offset_type_num_bits, pp)
      | fastuidraw::pack_bits(fastuidraw::StrokedPath::boundary_bit, 1u, bb)
      | fastuidraw::pack_bits(fastuidraw::StrokedPath::depth_bit0, fastuidraw::StrokedPath::depth_num_bits, depth);
  }

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
      assert(E < m_edge_data_store.size());
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
      assert(C < m_per_contour_data.size());
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
  };

  class BoundingBox
  {
  public:
    BoundingBox(void):
      m_empty(true)
    {}

    void
    inflated_polygon(fastuidraw::vecN<fastuidraw::vec2, 4> &out_data, float rad)
    {
      assert(!m_empty);
      out_data[0] = fastuidraw::vec2(m_min.x() - rad, m_min.y() - rad);
      out_data[1] = fastuidraw::vec2(m_max.x() + rad, m_min.y() - rad);
      out_data[2] = fastuidraw::vec2(m_max.x() + rad, m_max.y() + rad);
      out_data[3] = fastuidraw::vec2(m_min.x() - rad, m_max.y() + rad);
    }

    void
    union_point(const fastuidraw::vec2 &pt)
    {
      if(m_empty)
        {
          m_empty = false;
          m_min = m_max = pt;
        }
      else
        {
          m_min.x() = fastuidraw::t_min(m_min.x(), pt.x());
          m_min.y() = fastuidraw::t_min(m_min.y(), pt.y());

          m_max.x() = fastuidraw::t_max(m_max.x(), pt.x());
          m_max.y() = fastuidraw::t_max(m_max.y(), pt.y());
        }
    }

    void
    union_box(const BoundingBox &b)
    {
      if(!b.m_empty)
        {
          union_point(b.m_min);
          union_point(b.m_max);
        }
    }

    fastuidraw::vec2 m_min, m_max;
    bool m_empty;
  };

  class EdgeStore
  {
  public:
    EdgeStore(const fastuidraw::TessellatedPath &P, PathData &path_data);

    fastuidraw::const_c_array<SingleSubEdge>
    sub_edges(bool with_closing_edges)
    {
      return m_sub_edges[with_closing_edges];
    }

    fastuidraw::const_c_array<SingleSubEdge>
    sub_edges_of_closing_edges(void)
    {
      return m_sub_edges_of_closing_edges;
    }

    const BoundingBox&
    bounding_box(bool with_closing_edges)
    {
      return m_sub_edges_bb[with_closing_edges];
    }

  private:

    void
    process_edge(const fastuidraw::TessellatedPath &P, PathData &path_data,
                 unsigned int contour, unsigned int edge,
                 std::vector<SingleSubEdge> &dst, BoundingBox &bx);

    static const float sm_mag_tol;

    std::vector<SingleSubEdge> m_all_edges;
    fastuidraw::vecN<fastuidraw::const_c_array<SingleSubEdge>, 2> m_sub_edges;
    fastuidraw::const_c_array<SingleSubEdge> m_sub_edges_of_closing_edges;
    fastuidraw::vecN<BoundingBox, 2> m_sub_edges_bb;
  };

  class SubEdgeCullingHierarchy:fastuidraw::noncopyable
  {
  public:
    enum
      {
        splitting_threshhold = 100
      };

    explicit
    SubEdgeCullingHierarchy(const BoundingBox &start_box,
                            int splitting_coordinate,
                            fastuidraw::const_c_array<SingleSubEdge> data,
                            fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts);

    ~SubEdgeCullingHierarchy();

    fastuidraw::vecN<SubEdgeCullingHierarchy*, 2> m_children;
    std::vector<SingleSubEdge> m_sub_edges;
    BoundingBox m_sub_edges_bb, m_entire_bb;

  private:

    SubEdgeCullingHierarchy*
    create(const BoundingBox &start_box,
           int splitting_coordinate,
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

  class EdgesElement
  {
  public:
    enum
      {
        points_per_segment = 6,
        triangles_per_segment = points_per_segment - 2,
        indices_per_segment_without_bevel = 3 * triangles_per_segment,
      };

    static
    EdgesElement*
    create(SubEdgeCullingHierarchy *src)
    {
      unsigned int total_chunks(0);
      return FASTUIDRAWnew EdgesElement(0, 0, src, total_chunks, 0);
    }

    ~EdgesElement();

    unsigned int
    edge_chunks(ScratchSpacePrivate &work_room,
                fastuidraw::const_c_array<fastuidraw::vec3> clip_equations,
                const fastuidraw::float3x3 &clip_matrix_local,
                const fastuidraw::vec2 &recip_dimensions,
                float pixels_additional_room,
                float item_space_additional_room,
                fastuidraw::c_array<unsigned int> dst);

    unsigned int
    maximum_edge_chunks(void)
    {
      return m_data_chunk_with_children;
    }

    static
    void
    count_vertices_indices(SubEdgeCullingHierarchy *src,
                           unsigned int &vertex_cnt,
                           unsigned int &index_cnt,
                           unsigned int &depth_cnt);

    fastuidraw::vecN<EdgesElement*, 2> m_children;

    fastuidraw::range_type<unsigned int> m_vertex_data_range;
    fastuidraw::range_type<unsigned int> m_index_data_range;
    fastuidraw::range_type<unsigned int> m_depth;
    unsigned int m_data_chunk;
    BoundingBox m_data_bb;
    fastuidraw::const_c_array<SingleSubEdge> m_data_src;

    fastuidraw::range_type<unsigned int> m_vertex_data_range_with_children;
    fastuidraw::range_type<unsigned int> m_index_data_range_with_children;
    fastuidraw::range_type<unsigned int> m_depth_with_children;
    unsigned int m_data_chunk_with_children;
    BoundingBox m_data_with_children_bb;

  private:
    EdgesElement(unsigned int vertex_st, unsigned int index_st,
                 SubEdgeCullingHierarchy *src, unsigned int &total_chunks,
                 unsigned int depth);

    void
    edge_chunks_implement(ScratchSpacePrivate &work_room,
                          float item_space_additional_room,
                          fastuidraw::c_array<unsigned int> dst,
                          unsigned int &current);

  };

  class EdgesElementFiller:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    EdgesElementFiller(EdgesElement *src,
                       const fastuidraw::TessellatedPath &P);

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_increments) const;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<unsigned int> zincrements,
              fastuidraw::c_array<int> index_adjusts) const;
  private:
    void
    fill_data_worker(EdgesElement *e,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
                     fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                     fastuidraw::c_array<int> index_adjusts) const;
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int depth,
                     fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                     fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                     unsigned int &vertex_offset, unsigned int &index_offset) const;

    EdgesElement *m_src;
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
                  unsigned int &number_z_increments) const;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<unsigned int> zincrements,
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
    unsigned int m_num_joins;
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
                  unsigned int &number_z_increments) const;

    virtual
    void
    fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
              fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
              fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
              fastuidraw::c_array<unsigned int> zincrements,
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
        number_points_per_join = 2 * number_points_per_fan,
        number_indices_per_join = 2 * number_indices_per_fan
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
      m_data(NULL),
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

    fastuidraw::vecN<EdgesElement*, 2> m_edge_culler;
    fastuidraw::vecN<fastuidraw::PainterAttributeData, 2> m_edges;

    fastuidraw::PainterAttributeData m_bevel_joins, m_miter_joins;
    fastuidraw::PainterAttributeData m_square_caps, m_adjustable_caps;
    PathData m_path_data;

    std::vector<ThreshWithData> m_rounded_joins;
    std::vector<ThreshWithData> m_rounded_caps;

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
  BoundingBox closing_edges_bb, non_closing_edges_bb;

  path_data.m_per_contour_data.resize(P.number_contours());
  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      path_data.m_per_contour_data[o].m_edge_data_store.resize(P.number_edges(o));
      path_data.m_per_contour_data[o].m_start_contour_pt = P.unclosed_contour_point_data(o).front();
      path_data.m_per_contour_data[o].m_end_contour_pt = P.unclosed_contour_point_data(o).back();
      for(unsigned int e = 0; e < P.number_edges(o); ++e)
        {
          if(e + 1 == P.number_edges(o))
            {
              process_edge(P, path_data, o, e, closing_edges, closing_edges_bb);
            }
          else
            {
              process_edge(P, path_data, o, e, non_closing_edges, non_closing_edges_bb);
            }
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
  m_sub_edges_of_closing_edges = tmp.sub_array(num_non_closing);
  assert(m_sub_edges_of_closing_edges.size() == num_closing);

  m_sub_edges_bb[false] = m_sub_edges_bb[true] = non_closing_edges_bb;
  m_sub_edges_bb[true].union_box(closing_edges_bb);
}

void
EdgeStore::
process_edge(const fastuidraw::TessellatedPath &P, PathData &path_data,
             unsigned int contour, unsigned int edge,
             std::vector<SingleSubEdge> &dst, BoundingBox &bx)
{
  fastuidraw::range_type<unsigned int> R;
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(P.point_data());
  fastuidraw::vec2 normal(1.0f, 0.0f), last_normal(1.0f, 0.0f);

  R = P.edge_range(contour, edge);
  assert(R.m_end > R.m_begin);

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
SubEdgeCullingHierarchy(const BoundingBox &start_box,
                        int c,
                        fastuidraw::const_c_array<SingleSubEdge> data,
                        fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts)
{
  assert(c == 0 || c == 1);
  assert(!start_box.m_empty);

  if(data.size() >= splitting_threshhold)
    {
      fastuidraw::vecN<BoundingBox, 2> child_boxes;
      fastuidraw::vecN<std::vector<SingleSubEdge>, 2> child_sub_edges;
      float mid_point;

      mid_point = 0.5f * (start_box.m_min[c] + start_box.m_max[c]);
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
      m_children[0] = create(child_boxes[0], 1 - c, child_sub_edges[0], src_pts);
      m_children[1] = create(child_boxes[1], 1 - c, child_sub_edges[1], src_pts);
    }
  else
    {
      m_children[0] = m_children[1] = NULL;
      for(unsigned int i = 0; i < data.size(); ++i)
        {
          const SingleSubEdge &sub_edge(data[i]);
          m_sub_edges_bb.union_point(src_pts[sub_edge.m_pt0].m_p);
          m_sub_edges_bb.union_point(src_pts[sub_edge.m_pt1].m_p);
          m_sub_edges.push_back(sub_edge);
        }
    }

  if(m_children[0] != NULL)
    {
      m_entire_bb.union_box(m_children[0]->m_entire_bb);
    }

  if(m_children[1] != NULL)
    {
      m_entire_bb.union_box(m_children[1]->m_entire_bb);
    }

  m_entire_bb.union_box(m_sub_edges_bb);
}

SubEdgeCullingHierarchy::
~SubEdgeCullingHierarchy(void)
{
  if(m_children[0] != NULL)
    {
      FASTUIDRAWdelete(m_children[0]);
    }

  if(m_children[1] != NULL)
    {
      FASTUIDRAWdelete(m_children[1]);
    }
}

SubEdgeCullingHierarchy*
SubEdgeCullingHierarchy::
create(const BoundingBox &start_box, int c,
       const std::vector<SingleSubEdge> &data,
       fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts)
{
  if(!data.empty())
    {
      fastuidraw::const_c_array<SingleSubEdge> d;
      d = fastuidraw::make_c_array(data);
      return FASTUIDRAWnew SubEdgeCullingHierarchy(start_box, c, d, src_pts);
    }
  else
    {
      return NULL;
    }
}

////////////////////////////////////////////
// EdgesElement methods
EdgesElement::
EdgesElement(unsigned int vertex_st, unsigned int index_st,
             SubEdgeCullingHierarchy *src, unsigned int &total_chunks,
             unsigned int depth):
  m_children(NULL, NULL)
{
  m_vertex_data_range_with_children.m_begin = vertex_st;
  m_index_data_range_with_children.m_begin = index_st;
  m_depth_with_children.m_begin = depth;

  if(src->m_children[0] != NULL)
    {
      m_children[0] = FASTUIDRAWnew EdgesElement(vertex_st, index_st, src->m_children[0], total_chunks, depth);
      vertex_st = m_children[0]->m_vertex_data_range_with_children.m_end;
      index_st = m_children[0]->m_index_data_range_with_children.m_end;
      depth = m_children[0]->m_depth_with_children.m_end;
    }

  if(src->m_children[1] != NULL)
    {
      m_children[1] = FASTUIDRAWnew EdgesElement(vertex_st, index_st, src->m_children[1], total_chunks, depth);
      vertex_st = m_children[1]->m_vertex_data_range_with_children.m_end;
      index_st = m_children[1]->m_index_data_range_with_children.m_end;
      depth = m_children[1]->m_depth_with_children.m_end;
    }

  unsigned int index_cnt, vertex_cnt, depth_cnt;
  count_vertices_indices(src, vertex_cnt, index_cnt, depth_cnt);
  m_data_chunk = total_chunks;
  m_data_bb = src->m_sub_edges_bb;
  m_data_src = fastuidraw::make_c_array(src->m_sub_edges);
  m_vertex_data_range.m_begin = vertex_st;
  m_vertex_data_range.m_end = vertex_st + vertex_cnt;
  m_index_data_range.m_begin = index_st;
  m_index_data_range.m_end = index_st + index_cnt;
  m_depth.m_begin = depth;
  m_depth.m_end = depth + depth_cnt;

  m_data_chunk_with_children = total_chunks + 1;
  m_data_with_children_bb = src->m_entire_bb;
  m_vertex_data_range_with_children.m_end = m_vertex_data_range.m_end;
  m_index_data_range_with_children.m_end = m_index_data_range.m_end;
  m_depth_with_children.m_end = m_depth.m_end;

  total_chunks += 2u;
}

EdgesElement::
~EdgesElement()
{
  if(m_children[0] != NULL)
    {
      FASTUIDRAWdelete(m_children[0]);
    }
  if(m_children[1] != NULL)
    {
      FASTUIDRAWdelete(m_children[1]);
    }
}

unsigned int
EdgesElement::
edge_chunks(ScratchSpacePrivate &scratch,
            fastuidraw::const_c_array<fastuidraw::vec3> clip_equations,
            const fastuidraw::float3x3 &clip_matrix_local,
            const fastuidraw::vec2 &recip_dimensions,
            float pixels_additional_room,
            float item_space_additional_room,
            fastuidraw::c_array<unsigned int> dst)
{
  unsigned int return_value(0u);

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

  edge_chunks_implement(scratch, item_space_additional_room,
                        dst, return_value);
  return return_value;
}

void
EdgesElement::
edge_chunks_implement(ScratchSpacePrivate &scratch,
                      float item_space_additional_room,
                      fastuidraw::c_array<unsigned int> dst,
                      unsigned int &current)
{
  using namespace fastuidraw;
  using namespace fastuidraw::detail;

  if(m_data_with_children_bb.m_empty)
    {
      return;
    }

  /* clip the bounding box of this EdgesElement
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
      dst[current] = m_data_chunk_with_children;
      ++current;
      return;
    }

  //completely clipped
  if(scratch.m_clipped_rect.empty())
    {
      return;
    }

  if(m_children[0] != NULL)
    {
      m_children[0]->edge_chunks_implement(scratch, item_space_additional_room, dst, current);
    }

  if(m_children[1] != NULL)
    {
      m_children[1]->edge_chunks_implement(scratch, item_space_additional_room, dst, current);
    }

  if(!m_data_bb.m_empty)
    {
      m_data_bb.inflated_polygon(bb, item_space_additional_room);
      clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                          bb, scratch.m_clipped_rect,
                          scratch.m_clip_scratch_floats,
                          scratch.m_clip_scratch_vec2s);

      if(!scratch.m_clipped_rect.empty())
        {
          dst[current] = m_data_chunk;
          ++current;
        }
    }
}

void
EdgesElement::
count_vertices_indices(SubEdgeCullingHierarchy *src,
                       unsigned int &vertex_cnt,
                       unsigned int &index_cnt,
                       unsigned int &depth_cnt)
{
  vertex_cnt = 0u;
  index_cnt = 0u;
  depth_cnt = src->m_sub_edges.size();
  for(unsigned int i = 0, endi = depth_cnt; i < endi; ++i)
    {
      const SingleSubEdge &v(src->m_sub_edges[i]);
      if(v.m_has_bevel)
        {
          vertex_cnt += 3;
          index_cnt += 3;
        }
      vertex_cnt += points_per_segment;
      index_cnt += indices_per_segment_without_bevel;
    }
}

////////////////////////////////////////
// EdgesElementFiller methods
EdgesElementFiller::
EdgesElementFiller(EdgesElement *src,
                   const fastuidraw::TessellatedPath &P):
  m_src(src),
  m_P(P)
{
}

void
EdgesElementFiller::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_increments) const
{
  num_attribute_chunks = num_index_chunks = m_src->m_data_chunk_with_children + 1;
  num_attributes = m_src->m_vertex_data_range_with_children.m_end;
  num_indices = m_src->m_index_data_range_with_children.m_end;
  number_z_increments = 1;
}

void
EdgesElementFiller::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<unsigned int> zincrements,
          fastuidraw::c_array<int> index_adjusts) const
{
  zincrements[0] = m_src->m_depth_with_children.m_end;
  fill_data_worker(m_src, attribute_data, index_data,
                   attribute_chunks, index_chunks, index_adjusts);
}

void
EdgesElementFiller::
fill_data_worker(EdgesElement *e,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
                 fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
                 fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
                 fastuidraw::c_array<int> index_adjusts) const
{
  if(e->m_children[0] != NULL)
    {
      fill_data_worker(e->m_children[0], attribute_data, index_data,
                       attribute_chunks, index_chunks, index_adjusts);
    }

  if(e->m_children[1] != NULL)
    {
      fill_data_worker(e->m_children[1], attribute_data, index_data,
                       attribute_chunks, index_chunks, index_adjusts);
    }

  fastuidraw::c_array<fastuidraw::PainterAttribute> ad;
  fastuidraw::c_array<fastuidraw::PainterIndex> id;

  ad = attribute_data.sub_array(e->m_vertex_data_range);
  id = index_data.sub_array(e->m_index_data_range);
  attribute_chunks[e->m_data_chunk] = ad;
  index_chunks[e->m_data_chunk] = id;
  index_adjusts[e->m_data_chunk] = -int(e->m_vertex_data_range.m_begin);

  for(unsigned int k = 0,
        depth = e->m_depth.m_begin,
        v = e->m_vertex_data_range.m_begin,
        i = e->m_index_data_range.m_begin;
      k < e->m_data_src.size(); ++k, ++depth)
    {
      process_sub_edge(e->m_data_src[k], depth, attribute_data, index_data, v, i);
    }

  ad = attribute_data.sub_array(e->m_vertex_data_range_with_children);
  id = index_data.sub_array(e->m_index_data_range_with_children);
  attribute_chunks[e->m_data_chunk_with_children] = ad;
  index_chunks[e->m_data_chunk_with_children] = id;
  index_adjusts[e->m_data_chunk_with_children] = -int(e->m_vertex_data_range_with_children.m_begin);

  e->m_data_src = fastuidraw::const_c_array<SingleSubEdge>();
}

void
EdgesElementFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int raw_depth,
                 fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
                 fastuidraw::c_array<fastuidraw::PainterIndex> indices,
                 unsigned int &vert_offset, unsigned int &index_offset) const
{
  fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> src_pts(m_P.point_data());
  const int boundary_values[3] = { 1, 1, 0 };
  const float normal_sign[3] = { 1.0f, -1.0f, 0.0f };
  fastuidraw::vecN<fastuidraw::StrokedPath::point, 6> pts;
  unsigned int depth;

  /* we want the elements first in the index list
     to be infront of the element at the end; thus
     we reverse the depth.
   */
  assert(raw_depth < m_src->m_depth_with_children.m_end);
  depth = m_src->m_depth_with_children.m_end - 1 - raw_depth;

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

  index_offset += EdgesElement::indices_per_segment_without_bevel;
  vert_offset += EdgesElement::points_per_segment;
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
  m_post_ctor_initalized_called(false)
{}

void
JoinCreatorBase::
post_ctor_initalize(void)
{
  assert(!m_post_ctor_initalized_called);
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
              unsigned int &number_z_increments) const
{
  assert(m_post_ctor_initalized_called);
  num_attributes = m_num_non_closed_verts + m_num_closed_verts;
  num_indices = m_num_non_closed_indices + m_num_closed_indices;
  num_attribute_chunks = num_index_chunks = m_num_joins + 2;
  number_z_increments = 2;
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

  assert(join_id < m_num_joins);
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
          fastuidraw::c_array<unsigned int> zincrements,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0), index_offset(0), join_id(0);

  assert(attribute_data.size() == m_num_non_closed_verts + m_num_closed_verts);
  assert(index_data.size() == m_num_non_closed_indices + m_num_closed_indices);

  index_adjusts[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = 0;
  zincrements[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = m_num_joins;
  attribute_chunks[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = attribute_data.sub_array(0, m_num_non_closed_verts);
  index_chunks[fastuidraw::StrokedPath::join_chunk_without_closing_edge] = index_data.sub_array(0, m_num_non_closed_indices);

  index_adjusts[fastuidraw::StrokedPath::join_chunk_with_closing_edge] = 0;
  zincrements[fastuidraw::StrokedPath::join_chunk_with_closing_edge] = m_num_joins;
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
  assert(vertex_offset == m_num_non_closed_verts);
  assert(index_offset == m_num_non_closed_indices);

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
  assert(vertex_offset == m_num_non_closed_verts + m_num_closed_verts);
  assert(index_offset == m_num_non_closed_indices + m_num_closed_indices);
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

  assert(join_id < m_per_join_data.size());
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
// MiterJoinCreator methods
MiterJoinCreator::
MiterJoinCreator(const PathData &P):
  JoinCreatorBase(P)
{
  post_ctor_initalize();
}

void
MiterJoinCreator::
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
MiterJoinCreator::
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
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_miter_join, depth);
  pt.pack_point(&pts[vertex_offset]);
  ++vertex_offset;

  // miter point B
  pt.m_position = J.m_p1;
  pt.m_pre_offset = J.m_n0;
  pt.m_auxilary_offset = J.m_n1;
  pt.m_distance_from_edge_start = J.m_distance_from_edge_start;
  pt.m_distance_from_contour_start = J.m_distance_from_contour_start;
  pt.m_edge_length = J.m_edge_length;
  pt.m_open_contour_length = J.m_open_contour_length;
  pt.m_closed_contour_length = J.m_closed_contour_length;
  pt.m_packed_data = pack_data_join(1, fastuidraw::StrokedPath::offset_miter_join, depth);
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
              unsigned int &number_z_increments) const
{
  num_attributes = m_size.m_verts;
  num_indices = m_size.m_indices;
  number_z_increments = num_attribute_chunks = num_index_chunks = 1;
}

void
CapCreatorBase::
fill_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attribute_data,
          fastuidraw::c_array<fastuidraw::PainterIndex> index_data,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterAttribute> > attribute_chunks,
          fastuidraw::c_array<fastuidraw::const_c_array<fastuidraw::PainterIndex> > index_chunks,
          fastuidraw::c_array<unsigned int> zincrements,
          fastuidraw::c_array<int> index_adjusts) const
{
  unsigned int vertex_offset(0u), index_offset(0u), depth;

  depth = 2 * m_P.number_contours();
  for(unsigned int o = 0; o < m_P.number_contours(); ++o, depth -= 2u)
    {
      assert(depth >= 2);
      add_cap(m_P.m_per_contour_data[o].m_begin_cap_normal,
              true, depth - 1, m_P.m_per_contour_data[o].m_start_contour_pt,
              attribute_data, index_data,
              vertex_offset, index_offset);

      add_cap(m_P.m_per_contour_data[o].m_end_cap_normal,
              false, depth - 2, m_P.m_per_contour_data[o].m_end_contour_pt,
              attribute_data, index_data,
              vertex_offset, index_offset);
    }

  assert(vertex_offset == m_size.m_verts);
  assert(index_offset == m_size.m_indices);
  attribute_chunks[0] = attribute_data;
  index_chunks[0] = index_data;
  zincrements[0] = 2 * m_P.number_contours();
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
  create_edges(P);
  m_bevel_joins.set_data(BevelJoinCreator(m_path_data));
  m_miter_joins.set_data(MiterJoinCreator(m_path_data));
  m_square_caps.set_data(SquareCapCreator(m_path_data));
  m_adjustable_caps.set_data(AdjustableCapCreator(m_path_data));
  m_effective_curve_distance_threshhold = P.effective_curve_distance_threshhold();
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
  FASTUIDRAWdelete(m_edge_culler[0]);
  FASTUIDRAWdelete(m_edge_culler[1]);
}

void
StrokedPathPrivate::
create_edges(const fastuidraw::TessellatedPath &P)
{
  EdgeStore edge_store(P, m_path_data);

  for(unsigned int i = 0; i < 2; ++i)
    {
      SubEdgeCullingHierarchy *s;
      s = FASTUIDRAWnew SubEdgeCullingHierarchy(edge_store.bounding_box(i != 0),
                                                0, edge_store.sub_edges(i != 0),
                                                P.point_data());
      m_edge_culler[i] = EdgesElement::create(s);
      m_edges[i].set_data(EdgesElementFiller(m_edge_culler[i], P));
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
      assert(iter != values.end());
      assert(iter->m_thresh <= thresh);
      assert(iter->m_data != NULL);
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
  switch(offset_type())
    {
    case offset_start_sub_edge:
    case offset_end_sub_edge:
    case offset_shared_with_edge:
      return m_pre_offset;

    case offset_square_cap:
      return m_pre_offset + m_auxilary_offset;
      break;

    case offset_miter_join:
      {
        vec2 n(m_pre_offset), v(-n.y(), n.x());
        float r, lambda;
        if(dot(v, m_auxilary_offset) > 0.0f)
          {
            lambda = -1.0f;
          }
        else
          {
            lambda = 1.0f;
          }
        r = (dot(m_pre_offset, m_auxilary_offset) - 1.0) / dot(v, m_auxilary_offset);
        return lambda * (n - r * v);
      }

    case offset_rounded_cap:
      {
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        return m_auxilary_offset.x() * v + m_auxilary_offset.y() * n;
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
  if(offset_type() == offset_miter_join)
    {
      vec2 n0(m_pre_offset), v0(-n0.y(), n0.x());
      vec2 n1(m_auxilary_offset), v1(-n1.y(), n1.x());
      float numer, denom;

      numer = dot(v1, v0) - 1.0f;
      denom = dot(v0, n1);
      return denom != 0.0f ?
        numer / denom :
        0.0f;
    }
  else
    {
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
  d = reinterpret_cast<ScratchSpacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

//////////////////////////////////////////////////////////////
// fastuidraw::StrokedPath methodsunsigned int
unsigned int
fastuidraw::StrokedPath::
number_joins(const PainterAttributeData &join_data, bool with_closing_edges)
{
  unsigned int K;
  K = with_closing_edges ? join_chunk_with_closing_edge : join_chunk_without_closing_edge;
  return join_data.increment_z_value(K);

}

unsigned int
fastuidraw::StrokedPath::
chunk_for_named_join(unsigned int J)
{
  return join_chunk_start_individual_joins + J;
}

fastuidraw::StrokedPath::
StrokedPath(const fastuidraw::TessellatedPath &P)
{
  assert(number_offset_types < FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(offset_type_num_bits));
  m_d = FASTUIDRAWnew StrokedPathPrivate(P);
}

fastuidraw::StrokedPath::
~StrokedPath()
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

float
fastuidraw::StrokedPath::
effective_curve_distance_threshhold(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_effective_curve_distance_threshhold;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
edges(bool include_closing_edges) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_edges[include_closing_edges];
}

unsigned int
fastuidraw::StrokedPath::
edge_chunks(ScratchSpace &work_room,
            const_c_array<vec3> clip_equations,
            const float3x3 &clip_matrix_local,
            const vec2 &recip_dimensions,
            float pixels_additional_room,
            float item_space_additional_room,
            bool include_closing_edges, c_array<unsigned int> dst) const
{
  StrokedPathPrivate *d;
  EdgesElement *e;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  e = d->m_edge_culler[include_closing_edges];
  return e->edge_chunks(*reinterpret_cast<ScratchSpacePrivate*>(work_room.m_d),
                        clip_equations, clip_matrix_local,
                        recip_dimensions, pixels_additional_room,
                        item_space_additional_room,
                        dst);
}

unsigned int
fastuidraw::StrokedPath::
maximum_edge_chunks(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return t_max(d->m_edge_culler[0]->maximum_edge_chunks(),
               d->m_edge_culler[1]->maximum_edge_chunks());
}

unsigned int
fastuidraw::StrokedPath::
z_increment_edge(bool include_closing_edges) const
{
  return edges(include_closing_edges).increment_z_value(0);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
square_caps(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_square_caps;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
adjustable_caps(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_adjustable_caps;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
bevel_joins(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_bevel_joins;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
miter_joins(void) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->m_miter_joins;
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_joins(float thresh) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);

  return d->fetch_create<RoundedJoinCreator>(thresh, d->m_rounded_joins);
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::
rounded_caps(float thresh) const
{
  StrokedPathPrivate *d;
  d = reinterpret_cast<StrokedPathPrivate*>(m_d);
  return d->fetch_create<RoundedCapCreator>(thresh, d->m_rounded_caps);
}
