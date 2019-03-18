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
#include <fastuidraw/painter/attribute_data/stroked_path.hpp>
#include <fastuidraw/painter/attribute_data/stroked_caps_joins.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute_data_filler.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>
#include <private/util_private.hpp>
#include <private/util_private_ostream.hpp>
#include <private/bounding_box.hpp>
#include <private/path_util_private.hpp>
#include <private/point_attribute_data_merger.hpp>
#include <private/clip.hpp>

namespace
{
  enum
    {
      splitting_threshhold = 50,
      max_recursion_depth = 10
    };

  inline
  uint32_t
  pack_data(int on_boundary,
            enum fastuidraw::StrokedPoint::offset_type_t pt,
            uint32_t depth)
  {
    using namespace fastuidraw;
    FASTUIDRAWassert(on_boundary == 0 || on_boundary == 1);

    uint32_t bb(on_boundary), pp(pt);

    /* clamp depth to maximum allowed value */
    depth = t_min(depth,
                  FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(StrokedPoint::depth_num_bits));

    return pack_bits(StrokedPoint::offset_type_bit0,
                     StrokedPoint::offset_type_num_bits, pp)
      | pack_bits(StrokedPoint::boundary_bit, 1u, bb)
      | pack_bits(StrokedPoint::depth_bit0,
                  StrokedPoint::depth_num_bits, depth);
  }

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
    float m_contour_length;

    float m_sub_edge_length;
    fastuidraw::vec2 m_begin_normal, m_end_normal;

    bool m_has_bevel;
    float m_bevel_lambda;
    fastuidraw::vec2 m_bevel_normal;
    bool m_use_arc_for_bevel;

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
    SingleSubEdge(const fastuidraw::TessellatedPath::segment &seg, uint32_t flags);

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

  class SubPath:fastuidraw::noncopyable
  {
  public:
    explicit
    SubPath(const fastuidraw::TessellatedPath &P);

    fastuidraw::c_array<const SingleSubEdge>
    edges(void) const
    {
      return fastuidraw::make_c_array(m_edges);
    }

    const fastuidraw::BoundingBox<float>&
    bounding_box(void) const
    {
      return m_bounding_box;
    }

    bool
    has_arcs(void) const
    {
      return m_has_arcs;
    }

    /* a value of -1 means to NOT split. */
    int
    choose_splitting_coordinate(float &split_value) const;

    fastuidraw::vecN<SubPath*, 2>
    split(int splitting_coordinate, float spliting_value) const;

  private:
    explicit
    SubPath(bool phas_arcs);

    void
    process_edge(const fastuidraw::TessellatedPath &P,
                 fastuidraw::range_type<unsigned int> R,
                 const SingleSubEdge *prev,
                 std::vector<SingleSubEdge> &dst);

    static
    void
    split_segments(int splitting_coordinate,
                   const std::vector<SingleSubEdge> &src,
                   float splitting_value,
                   std::vector<SingleSubEdge> &dstA,
                   fastuidraw::BoundingBox<float> &boxA,
                   std::vector<SingleSubEdge> &dstB,
                   fastuidraw::BoundingBox<float> &boxB);

    static
    void
    update_bounding_box(unsigned int &begin,
                        const std::vector<SingleSubEdge> &segs,
                        fastuidraw::BoundingBox<float> &box);

    std::vector<SingleSubEdge> m_edges;
    fastuidraw::BoundingBox<float> m_bounding_box;
    bool m_has_arcs;
  };

  class ScratchSpacePrivate:fastuidraw::noncopyable
  {
  public:
    std::vector<fastuidraw::vec3> m_adjusted_clip_eqs;
    fastuidraw::c_array<const fastuidraw::vec2> m_clipped_rect;

    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_clip_scratch_vec2s;
  };

  class SubsetPrivate:fastuidraw::noncopyable
  {
  public:
    ~SubsetPrivate();

    unsigned int
    select_subsets(ScratchSpacePrivate &scratch,
                   fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
                   const fastuidraw::float3x3 &clip_matrix_local,
                   const fastuidraw::vec2 &one_pixel_width,
                   float pixels_additional_room,
                   float item_space_additional_room,
                   unsigned int max_attribute_cnt,
                   unsigned int max_index_cnt,
                   fastuidraw::c_array<unsigned int> dst);

    void
    select_subsets_all_unculled(fastuidraw::c_array<unsigned int> dst,
                                unsigned int max_attribute_cnt,
                                unsigned int max_index_cnt,
                                unsigned int &current);

    void
    make_ready(void);

    const fastuidraw::Path&
    bounding_path(void) const
    {
      return m_bounding_path;
    }

    const fastuidraw::Rect&
    bounding_box(void) const
    {
      return m_bounding_box.as_rect();
    }

    const fastuidraw::PainterAttributeData&
    painter_data(void) const
    {
      FASTUIDRAWassert(m_painter_data != nullptr);
      return *m_painter_data;
    }

    bool
    have_children(void) const
    {
      return m_children[0] != nullptr;
    }

    unsigned int
    ID(void) const
    {
      return m_ID;
    }

    static
    SubsetPrivate*
    create_root_subset(const fastuidraw::TessellatedPath &P,
                       std::vector<SubsetPrivate*> &out_values);

  private:
    /* creation of SubsetPrivate has that it takes ownership of data
     * it might delete the object or save it for later use.
     */
    SubsetPrivate(int recursion_depth, SubPath *data,
                  std::vector<SubsetPrivate*> &out_values);

    void
    select_subsets_implement(ScratchSpacePrivate &scratch,
                             fastuidraw::c_array<unsigned int> dst,
                             float item_space_additional_room,
                             unsigned int max_attribute_cnt,
                             unsigned int max_index_cnt,
                             unsigned int &current);

    void
    make_ready_from_children(void);

    void
    make_ready_from_sub_path(void);

    void
    ready_sizes_from_children(void);

    unsigned int m_ID;
    fastuidraw::vecN<SubsetPrivate*, 2> m_children;
    fastuidraw::BoundingBox<float> m_bounding_box;
    fastuidraw::Path m_bounding_path;
    fastuidraw::PainterAttributeData *m_painter_data;
    unsigned int m_num_attributes, m_num_indices;
    bool m_sizes_ready, m_ready, m_has_arcs;
    SubPath *m_sub_path;
  };

  class EdgeAttributeFillerBase:public fastuidraw::PainterAttributeDataFiller
  {
  public:
    explicit
    EdgeAttributeFillerBase(const SubPath *src):
      m_src(*src),
      m_ready(false)
    {}

    virtual
    void
    compute_sizes(unsigned int &num_attributes,
                  unsigned int &num_indices,
                  unsigned int &num_attribute_chunks,
                  unsigned int &num_index_chunks,
                  unsigned int &number_z_ranges) const
    {
      ready_data();
      num_attributes = m_attributes.size();
      num_indices = m_indices.size();
      num_attribute_chunks = num_index_chunks = number_z_ranges = 1;
    }

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
    ready_data(void) const;

    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const = 0;

    const SubPath &m_src;

    mutable bool m_ready;
    mutable std::vector<fastuidraw::PainterAttribute> m_attributes;
    mutable std::vector<fastuidraw::PainterIndex> m_indices;
    mutable unsigned int m_vertex_count, m_index_count;
    mutable unsigned int m_depth_count;
  };

  class LineEdgeAttributeFiller:public EdgeAttributeFillerBase
  {
  public:
    explicit
    LineEdgeAttributeFiller(const SubPath *src):
      EdgeAttributeFillerBase(src)
    {}

  private:
    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                       std::vector<fastuidraw::PainterAttribute> &attribute_data,
                       std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_bevel(bool is_inner_bevel,
                     const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;
  };

  class ArcEdgeAttributeFiller:public EdgeAttributeFillerBase
  {
  public:
    explicit
    ArcEdgeAttributeFiller(const SubPath *src):
      EdgeAttributeFillerBase(src)
    {}

  private:
    virtual
    void
    process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                       std::vector<fastuidraw::PainterAttribute> &attribute_data,
                       std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_arc_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                      std::vector<fastuidraw::PainterAttribute> &attribute_data,
                      std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_line_bevel(bool is_inner_bevel,
                     const SingleSubEdge &sub_edge, unsigned int &depth,
                     std::vector<fastuidraw::PainterAttribute> &attribute_data,
                     std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_arc_bevel(bool is_inner_bevel,
                    const SingleSubEdge &sub_edge, unsigned int &depth,
                    std::vector<fastuidraw::PainterAttribute> &attribute_data,
                    std::vector<fastuidraw::PainterIndex> &index_data) const;

    void
    build_dashed_capper(bool for_start_dashed_capper,
                        const SingleSubEdge &sub_edge, unsigned int &depth,
                        std::vector<fastuidraw::PainterAttribute> &attribute_data,
                        std::vector<fastuidraw::PainterIndex> &index_data) const;
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
    SubsetPrivate* m_root;
    std::vector<SubsetPrivate*> m_subsets;
  };

}

////////////////////////////////////////
// SingleSubEdge methods
SingleSubEdge::
SingleSubEdge(const fastuidraw::TessellatedPath::segment &seg, uint32_t flags):
  m_from_line_segment(seg.m_type == fastuidraw::TessellatedPath::line_segment),
  m_pt0(seg.m_start_pt),
  m_pt1(seg.m_end_pt),
  m_distance_from_edge_start(seg.m_distance_from_edge_start),
  m_distance_from_contour_start(seg.m_distance_from_contour_start),
  m_edge_length(seg.m_edge_length),
  m_contour_length(seg.m_contour_length),
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
          theta += 2.0f * static_cast<float>(FASTUIDRAW_PI);
        }
      else if (theta > t_max(m_arc_angle.m_begin, m_arc_angle.m_end))
        {
          theta -= 2.0f * static_cast<float>(FASTUIDRAW_PI);
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
             std::vector<SingleSubEdge> &dst,
             fastuidraw::BoundingBox<float> &bx,
             uint32_t flags)
{
  SingleSubEdge sub_edge(seg, flags);

  /* If the dot product between the normal is negative, then
   * chances are there is a cusp; we do NOT want a bevel (or
   * arc) join then.
   */
  if (!prev_subedge_of_edge
      || seg.m_continuation_with_predecessor
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

  /* When the edge is an arc, we have that it is monotonic
   * in both coordinates, thus regardless if the edge is
   * a line segment or arc, the bounding box is given
   * by the bounding box of the end-points.
   */
  bx.union_point(sub_edge.m_pt0);
  bx.union_point(sub_edge.m_pt1);

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

  if (m_pt0[splitting_coordinate] <= split_value && m_pt1[splitting_coordinate] <= split_value)
    {
      dst_before_split_value->push_back(*this);
      return;
    }

  if (m_pt0[splitting_coordinate] >= split_value && m_pt1[splitting_coordinate] >= split_value)
    {
      dst_after_split_value->push_back(*this);
      return;
    }

  if (m_from_line_segment)
    {
      float v0, v1;

      v0 = m_pt0[splitting_coordinate];
      v1 = m_pt1[splitting_coordinate];
      t = (split_value - v0) / (v1 - v0);

      FASTUIDRAWassert(t >= 0.0f && t <= 1.0f);

      s = 1.0 - t;
      p[1 - splitting_coordinate] = s * m_pt0[1 - splitting_coordinate] + t * m_pt1[1 - splitting_coordinate];
      p[splitting_coordinate] = split_value;
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

      mid_normal.x() = m_radius * fastuidraw::t_cos(mid_angle);
      mid_normal.y() = m_radius * fastuidraw::t_sin(mid_angle);
      if (m_arc_angle.m_begin > m_arc_angle.m_end)
        {
          mid_normal *= -1.0f;
        }
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
  out_edges[1].m_has_start_dashed_capper = false;
  out_edges[1].m_has_end_dashed_capper = m_has_end_dashed_capper;

  int i_before, i_after;
  if (m_pt0[splitting_coordinate] < split_value)
    {
      i_before = 0;
      i_after = 1;
    }
  else
    {
      i_before = 1;
      i_after = 0;
    }

  for(unsigned int i = 0; i < 2; ++i)
    {
      out_edges[i].m_from_line_segment = m_from_line_segment;
      out_edges[i].m_delta = out_edges[i].m_pt1 - out_edges[i].m_pt0;
      out_edges[i].m_has_bevel = m_has_bevel && (i == 0);
      out_edges[i].m_bevel_lambda = (i == 0) ? m_bevel_lambda : 0.0f;
      out_edges[i].m_bevel_normal = (i == 0) ? m_bevel_normal : fastuidraw::vec2(0.0f, 0.0f);
      out_edges[i].m_use_arc_for_bevel = m_use_arc_for_bevel;

      out_edges[i].m_center = m_center;
      out_edges[i].m_radius = m_radius;

      out_edges[i].m_edge_length = m_edge_length;
      out_edges[i].m_contour_length = m_contour_length;
    }

  dst_before_split_value->push_back(out_edges[i_before]);
  dst_after_split_value->push_back(out_edges[i_after]);
}

////////////////////////////////////////////
// SubPath methods
SubPath::
SubPath(bool phas_arcs):
  m_has_arcs(phas_arcs)
{
}

SubPath::
SubPath(const fastuidraw::TessellatedPath &P):
  m_has_arcs(P.has_arcs())
{
  using namespace fastuidraw;

  for(unsigned int o = 0; o < P.number_contours(); ++o)
    {
      for(unsigned int e = 0, ende = P.number_edges(o); e < ende; ++e)
        {
          const SingleSubEdge *prev(nullptr);
          fastuidraw::range_type<unsigned int> R;

          R = P.edge_range(o, e);
          if (e != 0 && P.edge_type(o, e) != PathEnums::starts_new_edge)
            {
              prev = &m_edges.back();
            }

          process_edge(P, R, prev, m_edges);
        }
    }
}

void
SubPath::
process_edge(const fastuidraw::TessellatedPath &P,
             fastuidraw::range_type<unsigned int> R,
             const SingleSubEdge *prev,
             std::vector<SingleSubEdge> &dst)
{
  using namespace fastuidraw;

  c_array<const TessellatedPath::segment> src_segments(P.segment_data());
  bool has_arcs(P.has_arcs());

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
                                  dst, m_bounding_box, flags);
      prev = &dst.back();
    }
}

int
SubPath::
choose_splitting_coordinate(float &split_value) const
{
  using namespace fastuidraw;

  unsigned int data_size(m_edges.size());

  if (data_size < splitting_threshhold)
    {
      return -1;
    }

  /* NOTE: we can use the end points of the SingleSubEdge
   * to count on what side or sides a SingleSubEdge
   * will land even when arcs are present because we
   * have the guarantee that a SingleSubEdge that is an
   * arc is a monotonic-arc.
   */
  vecN<float, 2> split_values;
  vecN<unsigned int, 2> split_counters(0, 0);
  vecN<uvec2, 2> child_counters(uvec2(0, 0), uvec2(0, 0));

  std::vector<float> qs;
  qs.reserve(m_edges.size());

  /* The split canidates are the median's of the x and y
   * coordinates of all the SingleSubEdge values.
   */
  for (int c = 0; c < 2; ++c)
    {
      qs.clear();
      qs.push_back(m_bounding_box.min_point()[c]);
      qs.push_back(m_bounding_box.max_point()[c]);
      for(const SingleSubEdge &sub_edge : m_edges)
        {
              qs.push_back(sub_edge.m_pt0[c]);
        }
      std::sort(qs.begin(), qs.end());
      split_values[c] = qs[qs.size() / 2];

      for(const SingleSubEdge &sub_edge : m_edges)
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
  if (child_counters[canidate][0] < data_size
      && child_counters[canidate][1] < data_size)
    {
      split_value = split_values[canidate];
      return canidate;
    }
  else
    {
      return -1;
    }
}

fastuidraw::vecN<SubPath*, 2>
SubPath::
split(int splitting_coordinate, float spliting_value) const
{
  using namespace fastuidraw;

  vecN<SubPath*, 2> children;

  FASTUIDRAWassert(splitting_coordinate != -1);
  children[0] = FASTUIDRAWnew SubPath(m_has_arcs);
  children[1] = FASTUIDRAWnew SubPath(m_has_arcs);

  split_segments(splitting_coordinate,
                 m_edges,
                 spliting_value,
                 children[0]->m_edges, children[0]->m_bounding_box,
                 children[1]->m_edges, children[1]->m_bounding_box);

  return children;
}

void
SubPath::
split_segments(int splitting_coordinate,
               const std::vector<SingleSubEdge> &src,
               float splitting_value,
               std::vector<SingleSubEdge> &dstA,
               fastuidraw::BoundingBox<float> &boxA,
               std::vector<SingleSubEdge> &dstB,
               fastuidraw::BoundingBox<float> &boxB)
{
  using namespace fastuidraw;

  unsigned int lastA(dstA.size());
  unsigned int lastB(dstB.size());

  for(const SingleSubEdge &sub_edge : src)
    {
      sub_edge.split_sub_edge(splitting_coordinate,
                              &dstA, &dstB,
                              splitting_value);
      update_bounding_box(lastA, dstA, boxA);
      update_bounding_box(lastB, dstB, boxB);
    }
}

void
SubPath::
update_bounding_box(unsigned int &begin,
                    const std::vector<SingleSubEdge> &segs,
                    fastuidraw::BoundingBox<float> &box)
{
  for (unsigned int end = segs.size(); begin < end; ++begin)
    {
      box.union_point(segs[begin].m_pt0);
      box.union_point(segs[begin].m_pt1);
    }
}

///////////////////////////////////////////////
// SubsetPrivate methods
SubsetPrivate*
SubsetPrivate::
create_root_subset(const fastuidraw::TessellatedPath &P,
                   std::vector<SubsetPrivate*> &out_values)
{
  SubPath *d;
  SubsetPrivate *return_value;

  d = FASTUIDRAWnew SubPath(P);
  return_value = FASTUIDRAWnew SubsetPrivate(0, d, out_values);
  return return_value;
}

SubsetPrivate::
SubsetPrivate(int recursion_depth, SubPath *data,
              std::vector<SubsetPrivate*> &out_values):
  m_ID(out_values.size()),
  m_children(nullptr, nullptr),
  m_bounding_box(data->bounding_box()),
  m_painter_data(nullptr),
  m_num_attributes(0),
  m_num_indices(0),
  m_sizes_ready(false),
  m_ready(false),
  m_has_arcs(data->has_arcs()),
  m_sub_path(nullptr)
{
  using namespace fastuidraw;

  int splitting_coordinate(-1);
  float splitting_value;

  out_values.push_back(this);
  if (recursion_depth < max_recursion_depth)
    {
      splitting_coordinate = data->choose_splitting_coordinate(splitting_value);
    }

  if (splitting_coordinate != -1)
    {
      vecN<SubPath*, 2> child_data;

      child_data = data->split(splitting_coordinate, splitting_value);
      FASTUIDRAWdelete(data);

      m_children[0] = FASTUIDRAWnew SubsetPrivate(recursion_depth + 1, child_data[0], out_values);
      m_children[1] = FASTUIDRAWnew SubsetPrivate(recursion_depth + 1, child_data[1], out_values);
    }
  else
    {
      m_sub_path = data;
    }

  const fastuidraw::vec2 &m(m_bounding_box.min_point());
  const fastuidraw::vec2 &M(m_bounding_box.max_point());
  m_bounding_path << vec2(m.x(), m.y())
                  << vec2(m.x(), M.y())
                  << vec2(M.x(), M.y())
                  << vec2(M.x(), m.y())
                  << Path::contour_close();
}

SubsetPrivate::
~SubsetPrivate()
{
  if (m_sub_path)
    {
      FASTUIDRAWdelete(m_sub_path);
    }

  if (m_painter_data)
    {
      FASTUIDRAWdelete(m_painter_data);
    }

  if (m_children[0])
    {
      FASTUIDRAWassert(m_children[1]);

      FASTUIDRAWdelete(m_children[0]);
      FASTUIDRAWdelete(m_children[1]);
    }
}

void
SubsetPrivate::
make_ready(void)
{
  if (!m_ready)
    {
      if (m_sub_path != nullptr)
        {
          make_ready_from_sub_path();
          FASTUIDRAWassert(m_painter_data != nullptr);
        }
      else
        {
          make_ready_from_children();
        }
    }
  FASTUIDRAWassert(m_ready);
}

void
SubsetPrivate::
make_ready_from_children(void)
{
  using namespace fastuidraw;

  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  FASTUIDRAWassert(m_sub_path == nullptr);
  FASTUIDRAWassert(m_painter_data == nullptr);

  m_children[0]->make_ready();
  m_children[1]->make_ready();
  m_ready = true;

  if (m_children[0]->m_painter_data == nullptr
      || m_children[1]->m_painter_data == nullptr)
    {
      return;
    }

  /* Do NOT allow merging if the depth requirements
   * summed between the two children is too great.
   */
  const unsigned int arc_max_depth(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(ArcStrokedPoint::depth_num_bits));
  const unsigned int linear_max_depth(FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(StrokedPoint::depth_num_bits));
  unsigned int needed_zrange;
  bool mergable;

  needed_zrange = m_children[0]->painter_data().z_range(0).difference()
    + m_children[1]->painter_data().z_range(0).difference();

  mergable = (m_has_arcs && needed_zrange < arc_max_depth)
    || (!m_has_arcs && needed_zrange < linear_max_depth);

  if (!mergable)
    {
      return;
    }

  m_painter_data = FASTUIDRAWnew PainterAttributeData();
  if (m_has_arcs)
    {
      detail::PointAttributeDataMerger<ArcStrokedPoint> merger(m_children[0]->painter_data(),
                                                               m_children[1]->painter_data());
      m_painter_data->set_data(merger);
    }
  else
    {
      detail::PointAttributeDataMerger<StrokedPoint> merger(m_children[0]->painter_data(),
                                                            m_children[1]->painter_data());
      m_painter_data->set_data(merger);
    }

  if (!m_sizes_ready)
    {
      ready_sizes_from_children();
    }
  FASTUIDRAWassert(m_num_attributes == m_painter_data->attribute_data_chunk(0).size());
  FASTUIDRAWassert(m_num_indices == m_painter_data->index_data_chunk(0).size());
}

void
SubsetPrivate::
make_ready_from_sub_path(void)
{
  using namespace fastuidraw;

  FASTUIDRAWassert(m_children[0] == nullptr);
  FASTUIDRAWassert(m_children[1] == nullptr);
  FASTUIDRAWassert(m_sub_path != nullptr);
  FASTUIDRAWassert(m_painter_data == nullptr);

  m_ready = true;
  m_painter_data = FASTUIDRAWnew PainterAttributeData();
  if (m_has_arcs)
    {
      m_painter_data->set_data(ArcEdgeAttributeFiller(m_sub_path));
    }
  else
    {
      m_painter_data->set_data(LineEdgeAttributeFiller(m_sub_path));
    }

  FASTUIDRAWdelete(m_sub_path);
  m_sub_path = nullptr;
  m_num_attributes = m_painter_data->attribute_data_chunk(0).size();
  m_num_indices = m_painter_data->index_data_chunk(0).size();
  m_sizes_ready = true;
}

void
SubsetPrivate::
ready_sizes_from_children(void)
{
  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  FASTUIDRAWassert(!m_sizes_ready);

  m_sizes_ready = true;
  FASTUIDRAWassert(m_children[0]->m_sizes_ready);
  FASTUIDRAWassert(m_children[1]->m_sizes_ready);
  m_num_attributes = m_children[0]->m_num_attributes + m_children[1]->m_num_attributes;
  m_num_indices = m_children[0]->m_num_indices + m_children[1]->m_num_indices;
}

unsigned int
SubsetPrivate::
select_subsets(ScratchSpacePrivate &scratch,
               fastuidraw::c_array<const fastuidraw::vec3> clip_equations,
               const fastuidraw::float3x3 &clip_matrix_local,
               const fastuidraw::vec2 &one_pixel_width,
               float pixels_additional_room,
               float item_space_additional_room,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               fastuidraw::c_array<unsigned int> dst)
{
  using namespace fastuidraw;

  scratch.m_adjusted_clip_eqs.resize(clip_equations.size());
  for(unsigned int i = 0; i < clip_equations.size(); ++i)
    {
      vec3 c(clip_equations[i]);
      float f;

      /* make "w" larger by the named number of pixels. */
      f = t_abs(c.x()) * one_pixel_width.x()
        + t_abs(c.y()) * one_pixel_width.y();

      c.z() += pixels_additional_room * f;

      /* transform clip equations from clip coordinates to
       * local coordinates.
       */
      scratch.m_adjusted_clip_eqs[i] = c * clip_matrix_local;
    }

  unsigned int return_value(0);

  select_subsets_implement(scratch, dst,
                           item_space_additional_room,
                           max_attribute_cnt, max_index_cnt,
                           return_value);

  return return_value;
}

void
SubsetPrivate::
select_subsets_implement(ScratchSpacePrivate &scratch,
                         fastuidraw::c_array<unsigned int> dst,
                         float item_space_additional_room,
                         unsigned int max_attribute_cnt,
                         unsigned int max_index_cnt,
                         unsigned int &current)
{
  using namespace fastuidraw;

  if (m_bounding_box.empty())
    {
      return;
    }

  /* clip the bounding box of this StrokedPathSubset */
  vecN<vec2, 4> bb;
  bool unclipped;

  m_bounding_box.inflated_polygon(bb, item_space_additional_room);
  unclipped = detail::clip_against_planes(make_c_array(scratch.m_adjusted_clip_eqs),
                                          bb, &scratch.m_clipped_rect,
                                          scratch.m_clip_scratch_vec2s);

  //completely clipped
  if (scratch.m_clipped_rect.empty())
    {
      return;
    }

  //completely unclipped.
  if (unclipped || !have_children())
    {
      select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      return;
    }

  FASTUIDRAWassert(m_children[0] != nullptr);
  FASTUIDRAWassert(m_children[1] != nullptr);
  m_children[0]->select_subsets_implement(scratch, dst, item_space_additional_room,
                                          max_attribute_cnt, max_index_cnt,
                                          current);
  m_children[1]->select_subsets_implement(scratch, dst, item_space_additional_room,
                                          max_attribute_cnt, max_index_cnt,
                                          current);
}

void
SubsetPrivate::
select_subsets_all_unculled(fastuidraw::c_array<unsigned int> dst,
                            unsigned int max_attribute_cnt,
                            unsigned int max_index_cnt,
                            unsigned int &current)
{
  using namespace fastuidraw;

  if (!m_sizes_ready && !have_children() && m_sub_path != nullptr)
    {
      /* We need to make this one ready because it will be selected. */
      make_ready_from_sub_path();
      FASTUIDRAWassert(m_painter_data != nullptr);
      FASTUIDRAWassert(m_sizes_ready);
    }

  if (m_sizes_ready
      && m_num_attributes <= max_attribute_cnt
      && m_num_indices <= max_index_cnt)
    {
      make_ready();

      /* if this Subset has two children but is unable
       * to merge them, then m_painter_data will still
       * be nullptr after make_ready().
       */
      if (m_painter_data)
        {
          dst[current] = m_ID;
          ++current;
          return;
        }
    }

  if (have_children())
    {
      m_children[0]->select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      m_children[1]->select_subsets_all_unculled(dst, max_attribute_cnt, max_index_cnt, current);
      if (!m_sizes_ready)
        {
          ready_sizes_from_children();
        }
    }
  else
    {
      FASTUIDRAWassert(m_sizes_ready);
      FASTUIDRAWassert(!"Childless StrokedPath::Subset has too many attributes or indices");
    }
}

////////////////////////////////////////
// EdgeAttributeFillerBase methods
void
EdgeAttributeFillerBase::
ready_data(void) const
{
  if (m_ready)
    {
      return;
    }

  m_ready = true;

  /* To get it so that the depth values are in non-increasing order
   * with respect to the triangles, we will reverse the triangle
   * other AFTER filling the data.
   */
  unsigned int d(0);
  for (const SingleSubEdge &sub_edge : m_src.edges())
    {
      process_sub_edge(sub_edge, d, m_attributes, m_indices);
    }
  m_vertex_count = m_attributes.size();
  m_index_count = m_indices.size();
  m_depth_count = d;

  /* now reverse the triangles in m_indices. */
  std::reverse(m_indices.begin(), m_indices.end());
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
  using namespace fastuidraw;

  index_adjusts[0] = 0;
  zranges[0].m_begin = 0;
  zranges[0].m_end = m_depth_count;
  attribute_chunks[0] = attribute_data;
  index_chunks[0] = index_data;

  std::copy(m_attributes.begin(), m_attributes.end(), attribute_data.begin());
  std::copy(m_indices.begin(), m_indices.end(), index_data.begin());
}

///////////////////////////////////////////////
// LineEdgeAttributeFiller methods
void
LineEdgeAttributeFiller::
build_line_bevel(bool is_inner_bevel,
                 const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  vecN<StrokedPoint, 3> pts;
  float sgn;
  unsigned int vert_offset(attribute_data.size());

  if (is_inner_bevel)
    {
      sgn = -sub_edge.m_bevel_lambda;
    }
  else
    {
      sgn = sub_edge.m_bevel_lambda;
    }

  indices.push_back(vert_offset + 0);
  indices.push_back(vert_offset + 1);
  indices.push_back(vert_offset + 2);

  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_contour_length = sub_edge.m_contour_length;
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
      PainterAttribute A;

      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  if (is_inner_bevel)
    {
      ++depth;
    }
}

void
LineEdgeAttributeFiller::
build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                   std::vector<fastuidraw::PainterAttribute> &attribute_data,
                   std::vector<fastuidraw::PainterIndex> &indices) const
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
      pts[k].m_contour_length = sub_edge.m_contour_length;
      pts[k].m_pre_offset = normal_sign[k] * sub_edge.m_begin_normal;
      pts[k].m_auxiliary_offset = sub_edge.m_delta;
      pts[k].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, depth);

      pts[k + 3].m_position = sub_edge.m_pt1;
      pts[k + 3].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_edge_length = sub_edge.m_edge_length;
      pts[k + 3].m_contour_length = sub_edge.m_contour_length;
      pts[k + 3].m_pre_offset = normal_sign[k] * sub_edge.m_end_normal;
      pts[k + 3].m_auxiliary_offset = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = pack_data(boundary_values[k], StrokedPoint::offset_sub_edge, depth)
        | StrokedPoint::end_sub_edge_mask;
    }

  unsigned int vert_offset(attribute_data.size());
  const unsigned int tris[12] =
    {
      0, 2, 5,
      0, 5, 3,
      2, 1, 4,
      2, 4, 5
    };

  for(unsigned int i = 0; i < 12; ++i)
    {
      indices.push_back(vert_offset + tris[i]);
    }

  for(unsigned int i = 0; i < 6; ++i)
    {
      PainterAttribute A;

      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  ++depth;
}

void
LineEdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
{
  FASTUIDRAWassert(sub_edge.m_from_line_segment);
  FASTUIDRAWassert(!sub_edge.m_has_start_dashed_capper);
  FASTUIDRAWassert(!sub_edge.m_has_end_dashed_capper);

  if (sub_edge.m_has_bevel)
    {
      build_line_bevel(false,
                       sub_edge, depth,
                       attribute_data, indices);
    }
  build_line_segment(sub_edge, depth, attribute_data, indices);

  if (sub_edge.m_has_bevel)
    {
      build_line_bevel(true,
                       sub_edge, depth, attribute_data, indices);
    }
}

////////////////////////////////////
// ArcEdgeAttributeFiller methods
void
ArcEdgeAttributeFiller::
process_sub_edge(const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
{
  if (sub_edge.m_has_bevel)
    {
      if (sub_edge.m_use_arc_for_bevel)
        {
          build_arc_bevel(false, sub_edge, depth, attribute_data, indices);
        }
      else
        {
          build_line_bevel(false, sub_edge, depth, attribute_data, indices);
        }
    }

  if (sub_edge.m_has_start_dashed_capper)
    {
      build_dashed_capper(true, sub_edge, depth, attribute_data, indices);
    }

  if (sub_edge.m_from_line_segment)
    {
      build_line_segment(sub_edge, depth, attribute_data, indices);
    }
  else
    {
      build_arc_segment(sub_edge, depth, attribute_data, indices);
    }

  if (sub_edge.m_has_bevel)
    {
      if (sub_edge.m_use_arc_for_bevel)
        {
          build_arc_bevel(true, sub_edge, depth, attribute_data, indices);
        }
      else
        {
          build_line_bevel(true, sub_edge, depth, attribute_data, indices);
        }
    }

  if (sub_edge.m_has_end_dashed_capper)
    {
      build_dashed_capper(false, sub_edge, depth, attribute_data, indices);
    }
}

void
ArcEdgeAttributeFiller::
build_dashed_capper(bool for_start_dashed_capper,
                    const SingleSubEdge &sub_edge, unsigned int &depth,
                    std::vector<fastuidraw::PainterAttribute> &attribute_data,
                    std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  unsigned int vert_offset(attribute_data.size());
  const PainterIndex tris[12] =
    {
      0, 3, 4,
      0, 4, 1,
      1, 4, 5,
      1, 5, 2,
    };

  for (unsigned int i = 0; i < 12; ++i)
    {
      indices.push_back(vert_offset + tris[i]);
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
  PainterAttribute A;

  pt.m_position = pos;
  pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + d;
  pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + d;
  pt.m_edge_length = sub_edge.m_edge_length;
  pt.m_contour_length = sub_edge.m_contour_length;
  pt.m_data = tangent;

  pt.m_packed_data = packed_data;
  pt.m_offset_direction = normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data_mid;
  pt.m_offset_direction = vec2(0.0f, 0.0f);
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data;
  pt.m_offset_direction = -normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data_mid | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = vec2(0.0f, 0.0f);
  pt.pack_point(&A);
  attribute_data.push_back(A);

  pt.m_packed_data = packed_data | ArcStrokedPoint::extend_mask;
  pt.m_offset_direction = -normal;
  pt.pack_point(&A);
  attribute_data.push_back(A);

  ++depth;
}

void
ArcEdgeAttributeFiller::
build_arc_bevel(bool is_inner_bevel,
                const SingleSubEdge &sub_edge, unsigned int &depth,
                std::vector<fastuidraw::PainterAttribute> &attribute_data,
                std::vector<fastuidraw::PainterIndex> &indices) const
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
  pt.m_contour_length = sub_edge.m_contour_length;

  fastuidraw::detail::pack_arc_join(pt, 1, n_start, n_end, depth,
                                    attribute_data, indices, false);
  ++depth;
}

void
ArcEdgeAttributeFiller::
build_line_bevel(bool is_inner_bevel,
                 const SingleSubEdge &sub_edge, unsigned int &depth,
                 std::vector<fastuidraw::PainterAttribute> &attribute_data,
                 std::vector<fastuidraw::PainterIndex> &indices) const
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

  unsigned int vert_offset(attribute_data.size());
  for (unsigned int i = 0; i < 3; ++i)
    {
      indices.push_back(vert_offset + i);
    }

  for(unsigned int k = 0; k < 3; ++k)
    {
      pts[k].m_position = sub_edge.m_pt0;
      pts[k].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
      pts[k].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
      pts[k].m_edge_length = sub_edge.m_edge_length;
      pts[k].m_contour_length = sub_edge.m_contour_length;
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
      PainterAttribute A;
      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  if (is_inner_bevel)
    {
      ++depth;
    }
}

void
ArcEdgeAttributeFiller::
build_arc_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                  std::vector<fastuidraw::PainterAttribute> &attribute_data,
                  std::vector<fastuidraw::PainterIndex> &indices) const
{
  using namespace fastuidraw;
  using namespace detail;

  ArcStrokedPoint begin_pt, end_pt;
  vec2 begin_radial, end_radial;
  PainterAttribute A;
  unsigned int vert_offset(attribute_data.size());

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

  for (unsigned int i = 0; i < 24; ++i)
    {
      indices.push_back(tris[i] + vert_offset);
    }

  begin_pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start;
  begin_pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start;
  begin_pt.m_edge_length = sub_edge.m_edge_length;
  begin_pt.m_contour_length = sub_edge.m_contour_length;
  begin_pt.radius() = sub_edge.m_radius;
  begin_pt.arc_angle() = sub_edge.m_arc_angle.m_end - sub_edge.m_arc_angle.m_begin;

  end_pt.m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
  end_pt.m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
  end_pt.m_edge_length = sub_edge.m_edge_length;
  end_pt.m_contour_length = sub_edge.m_contour_length;
  end_pt.radius() = sub_edge.m_radius;
  end_pt.arc_angle() = sub_edge.m_arc_angle.m_end - sub_edge.m_arc_angle.m_begin;

  begin_radial = vec2(t_cos(sub_edge.m_arc_angle.m_begin), t_sin(sub_edge.m_arc_angle.m_begin));
  end_radial = vec2(t_cos(sub_edge.m_arc_angle.m_end), t_sin(sub_edge.m_arc_angle.m_end));

  /* inner stroking boundary points (points 0, 1) */
  begin_pt.m_packed_data = ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* the points that are on the arc (points 2, 3) */
  begin_pt.m_packed_data = arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* outer stroking boundary points (points 4, 5) */
  begin_pt.m_packed_data = arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* beyond outer stroking boundary points (points 6, 7) */
  begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* points that move to origin when stroking radius is larger than arc radius (points 8, 9) */
  begin_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
    | arc_stroked_point_pack_bits(0, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = ArcStrokedPoint::move_to_arc_center_mask
    | ArcStrokedPoint::end_segment_mask
    | ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  /* beyond inner stroking boundary (points 10, 11) */
  begin_pt.m_packed_data = ArcStrokedPoint::beyond_boundary_mask
    | ArcStrokedPoint::inner_stroking_mask
    | arc_stroked_point_pack_bits(1, ArcStrokedPoint::offset_arc_point, depth);
  begin_pt.m_position = sub_edge.m_pt0;
  begin_pt.m_offset_direction = begin_radial;
  begin_pt.pack_point(&A);
  attribute_data.push_back(A);

  end_pt.m_packed_data = begin_pt.m_packed_data | ArcStrokedPoint::end_segment_mask;
  end_pt.m_position = sub_edge.m_pt1;
  end_pt.m_offset_direction = end_radial;
  end_pt.pack_point(&A);
  attribute_data.push_back(A);

  ++depth;
}

void
ArcEdgeAttributeFiller::
build_line_segment(const SingleSubEdge &sub_edge, unsigned int &depth,
                   std::vector<fastuidraw::PainterAttribute> &attribute_data,
                   std::vector<fastuidraw::PainterIndex> &indices) const
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
      pts[k].m_contour_length = sub_edge.m_contour_length;
      pts[k].m_offset_direction = normal_sign[k] * sub_edge.m_begin_normal;
      pts[k].m_data = sub_edge.m_delta;
      pts[k].m_packed_data = arc_stroked_point_pack_bits(boundary_values[k],
                                                         ArcStrokedPoint::offset_line_segment,
                                                         depth);

      pts[k + 3].m_position = sub_edge.m_pt1;
      pts[k + 3].m_distance_from_edge_start = sub_edge.m_distance_from_edge_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_distance_from_contour_start = sub_edge.m_distance_from_contour_start + sub_edge.m_sub_edge_length;
      pts[k + 3].m_edge_length = sub_edge.m_edge_length;
      pts[k + 3].m_contour_length = sub_edge.m_contour_length;
      pts[k + 3].m_offset_direction = normal_sign[k] * sub_edge.m_end_normal;
      pts[k + 3].m_data = -sub_edge.m_delta;
      pts[k + 3].m_packed_data = arc_stroked_point_pack_bits(boundary_values[k],
                                                             ArcStrokedPoint::offset_line_segment,
                                                             depth)
        | ArcStrokedPoint::end_segment_mask;
    }

  const unsigned int tris[12] =
    {
      0, 2, 5,
      0, 5, 3,
      2, 1, 4,
      2, 4, 5
    };

  unsigned int vert_offset(attribute_data.size());
  for(unsigned int i = 0; i < 12; ++i)
    {
      indices.push_back(vert_offset + tris[i]);
    }

  for(unsigned int i = 0; i < 6; ++i)
    {
      PainterAttribute A;

      pts[i].pack_point(&A);
      attribute_data.push_back(A);
    }

  ++depth;
}

/////////////////////////////////////////////
// StrokedPathPrivate methods
StrokedPathPrivate::
StrokedPathPrivate(const fastuidraw::TessellatedPath &P,
                   const fastuidraw::StrokedCapsJoins::Builder &b):
  m_has_arcs(P.has_arcs()),
  m_caps_joins(b),
  m_root(nullptr)
{
  if (!P.segment_data().empty())
    {
      m_root = SubsetPrivate::create_root_subset(P, m_subsets);
    }
}

StrokedPathPrivate::
~StrokedPathPrivate()
{
  if (m_root)
    {
      FASTUIDRAWdelete(m_root);
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
  float distance_accumulated(0.0f);

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
      distance_accumulated += last_segs.back().m_edge_length;

      if (tess->edge_type(c, e) == fastuidraw::PathEnums::starts_new_edge)
        {
          b.add_join(segs.front().m_start_pt,
                     distance_accumulated,
                     delta_into, delta_leaving);
          distance_accumulated = 0.0f;
        }
      last_segs = segs;
    }

  if (tess->contour_closed(c))
    {
      b.close_contour(last_segs.back().m_edge_length + distance_accumulated,
                      last_segs.back().m_leaving_segment_unit_vector);
    }
  else
    {
      b.end_contour(last_segs.back().m_end_pt,
                    distance_accumulated,
                    last_segs.back().m_leaving_segment_unit_vector);
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

/////////////////////////////////
// fastuidraw::StrokedPath::Subset methods
fastuidraw::StrokedPath::Subset::
Subset(void *d):
  m_d(d)
{
}

const fastuidraw::PainterAttributeData&
fastuidraw::StrokedPath::Subset::
painter_data(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->painter_data();
}

const fastuidraw::Path&
fastuidraw::StrokedPath::Subset::
bounding_path(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->bounding_path();
}

const fastuidraw::Rect&
fastuidraw::StrokedPath::Subset::
bounding_box(void) const
{
  SubsetPrivate *d;
  d = static_cast<SubsetPrivate*>(m_d);
  return d->bounding_box();
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

unsigned int
fastuidraw::StrokedPath::
number_subsets(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_subsets.size();
}

fastuidraw::StrokedPath::Subset
fastuidraw::StrokedPath::
subset(unsigned int I) const
{
  StrokedPathPrivate *d;
  SubsetPrivate *p;

  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWassert(I < d->m_subsets.size());
  p = d->m_subsets[I];
  p->make_ready();

  return Subset(p);
}

unsigned int
fastuidraw::StrokedPath::
select_subsets(ScratchSpace &scratch_space,
               c_array<const vec3> clip_equations,
               const float3x3 &clip_matrix_local,
               const vec2 &one_pixel_width,
               float pixels_additional_room,
               float item_space_additional_room,
               unsigned int max_attribute_cnt,
               unsigned int max_index_cnt,
               c_array<unsigned int> dst) const
{
  StrokedPathPrivate *d;
  ScratchSpacePrivate *scratch_space_ptr;
  unsigned int return_value;

  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWassert(dst.size() >= d->m_subsets.size());

  if (d->m_root)
    {
      scratch_space_ptr = static_cast<ScratchSpacePrivate*>(scratch_space.m_d);
      return_value = d->m_root->select_subsets(*scratch_space_ptr,
                                               clip_equations,
                                               clip_matrix_local,
                                               one_pixel_width,
                                               pixels_additional_room,
                                               item_space_additional_room,
                                               max_attribute_cnt,
                                               max_index_cnt,
                                               dst);
    }
  else
    {
      return_value = 0;
    }

  return return_value;
}

unsigned int
fastuidraw::StrokedPath::
select_subsets_no_culling(unsigned int max_attribute_cnt,
                          unsigned int max_index_cnt,
                          c_array<unsigned int> dst) const
{
  StrokedPathPrivate *d;
  unsigned int return_value(0);

  d = static_cast<StrokedPathPrivate*>(m_d);
  FASTUIDRAWassert(dst.size() >= d->m_subsets.size());

  if (d->m_root)
    {
      d->m_root->select_subsets_all_unculled(dst, max_attribute_cnt,
                                             max_index_cnt, return_value);
    }
  else
    {
      return_value = 0;
    }

  return return_value;
}

const fastuidraw::StrokedCapsJoins&
fastuidraw::StrokedPath::
caps_joins(void) const
{
  StrokedPathPrivate *d;
  d = static_cast<StrokedPathPrivate*>(m_d);
  return d->m_caps_joins;
}
