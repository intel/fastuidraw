/*!
 * \file path.cpp
 * \brief file path.cpp
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
#include <cmath>
#include <vector>
#include <fastuidraw/path.hpp>
#include <fastuidraw/tessellated_path.hpp>
#include "private/util_private.hpp"
#include "private/path_util_private.hpp"
#include "private/bounding_box.hpp"

namespace
{
  inline
  float
  compute_distance(const fastuidraw::vec2 &a,
                   const fastuidraw::vec2 &p,
                   const fastuidraw::vec2 &b)
  {
    fastuidraw::vec2 p_a, b_a, p_b;
    float d, p_a_mag_sq;

    p_a = p - a;
    b_a = b - a;
    p_b = p - b;
    d = fastuidraw::dot(p_a, b_a);
    p_a_mag_sq = p_a.magnitudeSq();

    if (fastuidraw::dot(p_b, b_a) <= 0.0f && d >= 0.0f)
      {
        float d_sq, r, b_a_mag_sq;

        b_a_mag_sq = b_a.magnitudeSq();
        d_sq = d * d;
        r = p_a_mag_sq - d_sq / b_a_mag_sq;
        r = fastuidraw::t_max(0.0f, r);
        return fastuidraw::t_sqrt(r);
      }
    else
      {
        float r, b_p_mag_sq;

        b_p_mag_sq = (b - p).magnitudeSq();
        r = fastuidraw::t_min(p_a_mag_sq, b_p_mag_sq);
        r = fastuidraw::t_sqrt(r);
        return r;
      }
  }

  class ArcSegment
  {
  public:
    ArcSegment(void)
    {}

    ArcSegment(const fastuidraw::vec2 &start_pt,
               const fastuidraw::vec2 &mid_pt,
               const fastuidraw::vec2 &end_pt);

    float
    distance(fastuidraw::vec2 pt) const;

    bool m_too_flat;
    fastuidraw::vec2 m_center;
    fastuidraw::range_type<float> m_angle;
    float m_radius;

    fastuidraw::vec2 m_circle_sector_boundary[2];
    fastuidraw::vec2 m_circle_sector_center;
    float m_circle_sector_cos_angle;
  };

  class ArcTessellatorStateNode
  {
  public:
    explicit
    ArcTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h);

    float
    max_distance(void) const
    {
      return m_max_distance;
    }

    void
    add_segment(fastuidraw::TessellatedPath::SegmentStorage *out_data) const;

    unsigned int
    recursion_depth(void) const
    {
      return m_recursion_depth;
    }

    ArcTessellatorStateNode
    splitL(const fastuidraw::PathContour::interpolator_generic *h) const;

    ArcTessellatorStateNode
    splitR(const fastuidraw::PathContour::interpolator_generic *h) const;

  private:
    ArcTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h,
                            const fastuidraw::vec2 &p0,
                            const fastuidraw::vec2 &p1,
                            fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> rgn,
                            unsigned int depth);

    void
    compute_values(void);

    fastuidraw::vec2 m_start, m_end, m_mid;
    fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> m_L, m_R;

    float m_max_distance;
    unsigned int m_recursion_depth;

    /* The arc connecting m_start to m_end going through m_mid. */
    ArcSegment m_arc;
  };

  class TessellationState:public fastuidraw::PathContour::tessellation_state
  {
  public:
    explicit
    TessellationState(const fastuidraw::PathContour::interpolator_generic *h);

    virtual
    unsigned int
    recursion_depth(void) const
    {
      return m_recursion_depth;
    }

    virtual
    void
    resume_tessellation(const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                        fastuidraw::TessellatedPath::SegmentStorage *out_data,
                        float *out_max_distance);
  private:
    void
    resume_tessellation_worker(const ArcTessellatorStateNode &node,
                               const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                               std::vector<ArcTessellatorStateNode> *dst);


    fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_generic> m_h;
    std::vector<ArcTessellatorStateNode> m_nodes;
    unsigned int m_recursion_depth;
    unsigned int m_minimum_tessellation_recursion;
  };

  /*
   * A FakeInterpolator is used only for the fake interpolator
   * that starts a contour; it never gets used in a final
   * contour. In addition, an interpolator is the first edge
   * of a contour if and only if its previous is a FakeInterpolator
   */
  class FakeInterpolator:public fastuidraw::PathContour::flat
  {
  public:
    explicit
    FakeInterpolator(const fastuidraw::vec2 &pt):
      fastuidraw::PathContour::flat(nullptr, pt,
                                    fastuidraw::PathEnums::starts_new_edge)
    {}

    virtual
    bool
    is_flat(void) const
    {
      FASTUIDRAWassert("Should never be called");
      return flat::is_flat();
    }

    virtual
    fastuidraw::reference_counted_ptr<fastuidraw::PathContour::tessellation_state>
    produce_tessellation(const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                         fastuidraw::TessellatedPath::SegmentStorage *out_data,
                         float *out_max_distance) const
    {
      FASTUIDRAWassert("Should never be called");
      return flat::produce_tessellation(tess_params, out_data, out_max_distance);
    }

    virtual
    fastuidraw::PathContour::interpolator_base*
    deep_copy(const fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base> &) const
    {
      FASTUIDRAWassert("Should never be called");
      return FASTUIDRAWnew FakeInterpolator(end_pt());
    }

    virtual
    void
    approximate_bounding_box(fastuidraw::vec2 *out_min_bb,
                             fastuidraw::vec2 *out_max_bb) const
    {
      FASTUIDRAWassert("Should never be called");
      flat::approximate_bounding_box(out_min_bb, out_max_bb);
    }
  };

  class InterpolatorBasePrivate
  {
  public:
    //note is weak pointer to prevent circular pain.
    const fastuidraw::PathContour::interpolator_base *m_prev;
    fastuidraw::vec2 m_end;
    enum fastuidraw::PathEnums::edge_type_t m_type;
  };

  class BezierTessRegion:
    public fastuidraw::PathContour::interpolator_generic::tessellated_region
  {
  public:
    explicit
    BezierTessRegion(const BezierTessRegion *parent, bool is_region_start);

    explicit
    BezierTessRegion(fastuidraw::BoundingBox<float> &bb,
                     const fastuidraw::vec2 &start,
                     fastuidraw::c_array<const fastuidraw::vec2> ct,
                     const fastuidraw::vec2 &end);

    virtual
    float
    distance_to_line_segment(void) const;

    virtual
    float
    distance_to_arc(float arc_radius, fastuidraw::vec2 arc_center,
                    fastuidraw::vec2 unit_vector_arc_middle,
                    float cos_arc_angle) const;

    const fastuidraw::reference_counted_ptr<BezierTessRegion>&
    left_child(void) const
    {
      create_children();
      return m_L;
    }

    const fastuidraw::reference_counted_ptr<BezierTessRegion>&
    right_child(void) const
    {
      create_children();
      return m_R;
    }

    const fastuidraw::vec2&
    front(void) const
    {
      return m_pts.front();
    }

    const fastuidraw::vec2&
    back(void) const
    {
      return m_pts.back();
    }

    const std::vector<fastuidraw::vec2>&
    pts(void) const
    {
      return m_pts;
    }

  private:
    float
    distance_to_line_segment_raw(const fastuidraw::vec2 &A,
                                 const fastuidraw::vec2 &B) const;

    float
    distance_to_arc_raw(unsigned int depth, const ArcSegment &A) const;

    void
    create_children(void) const;

    mutable fastuidraw::reference_counted_ptr<BezierTessRegion> m_L, m_R;
    std::vector<fastuidraw::vec2> m_pts;
    float m_start, m_end;
    int m_arc_distance_depth;
  };

  class BezierPrivate
  {
  public:
    fastuidraw::BoundingBox<float> m_bb;
    fastuidraw::reference_counted_ptr<BezierTessRegion> m_start_region;
  };

  class ArcPrivate
  {
  public:
    float m_radius, m_angle_speed;
    float m_start_angle;
    fastuidraw::vec2 m_center;
    fastuidraw::BoundingBox<float> m_bb;
  };

  class PathContourPrivate
  {
  public:
    PathContourPrivate(void):
      m_is_flat(true)
    {}

    fastuidraw::vec2 m_start_pt;
    std::vector<fastuidraw::vec2> m_current_control_points;
    fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base> m_end_to_start;
    std::vector<fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base> > m_interpolators;

    fastuidraw::BoundingBox<float> m_bb;
    bool m_is_flat;
  };

  class PathPrivate;

  class TessellatedPathList
  {
  public:
    typedef fastuidraw::TessellatedPath TessellatedPath;
    typedef typename TessellatedPath::TessellationParams TessellationParams;
    typedef fastuidraw::reference_counted_ptr<const TessellatedPath> TessellatedPathRef;

    explicit
    TessellatedPathList(void):
      m_done(false)
    {}

    const TessellatedPathRef&
    tessellation(const fastuidraw::Path &path, float max_distance);

    void
    clear(void)
    {
      m_data.clear();
      m_refiner = nullptr;
      m_done = false;
    }

  private:
    bool m_done;
    fastuidraw::reference_counted_ptr<TessellatedPath::Refiner> m_refiner;
    std::vector<TessellatedPathRef> m_data;
  };

  class PathPrivate:fastuidraw::noncopyable
  {
  public:
    explicit
    PathPrivate(fastuidraw::Path *p);

    PathPrivate(fastuidraw::Path *p, const PathPrivate &obj);

    const fastuidraw::reference_counted_ptr<fastuidraw::PathContour>&
    current_contour(void);

    void
    move_common(const fastuidraw::vec2 &pt);

    void
    clear_tesses(void);

    void
    start_contour_if_necessary(void);

    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PathContour> > m_contours;
    enum fastuidraw::PathEnums::edge_type_t m_next_edge_type;

    TessellatedPathList m_tess_list;

    /* m_start_check_bb gives the index into m_contours that
     * have not had their bounding box absorbed m_bb
     */
    unsigned int m_start_check_bb;
    fastuidraw::BoundingBox<float> m_bb;
    bool m_is_flat;
    fastuidraw::Path *m_p;
  };
}

/////////////////////////////////
// ArcSegment methods
ArcSegment::
ArcSegment(const fastuidraw::vec2 &start,
           const fastuidraw::vec2 &mid,
           const fastuidraw::vec2 &end)
{
  using namespace fastuidraw;

  vec2 v0, v1, n0, n1, p0, p1;
  float det;
  static float tol(0.00001f);

  p0 = 0.5f * (start + mid);
  p1 = 0.5f * (mid + end);

  v0 = start - mid;
  n0 = vec2(-v0.y(), v0.x());

  v1 = mid - end;
  n1 = vec2(-v1.y(), v1.x());

  det = n1.y() * n0.x() - n0.y() * n1.x();
  m_too_flat = (t_abs(det) < tol);

  if (!m_too_flat)
    {
      float s, invRadius;
      vec2 cs, ce;
      const float pi(FASTUIDRAW_PI), two_pi(2.0f * pi);

      s = dot(v1, p1 - p0) / det;
      m_center = p0 + s * n0;
      m_radius = (m_center - mid).magnitude();
      invRadius = 1.0f / m_radius;
      cs = (start - m_center) * invRadius;
      ce = (end - m_center) * invRadius;
      m_angle.m_begin = cs.atan();
      m_angle.m_end = ce.atan();

      /* Under linear tessellation the points from start to _end
       * would be approximated by a line segment. In that spirit,
       * take the smaller arc always.
       */
      if (t_abs(m_angle.m_begin - m_angle.m_end) > pi)
        {
          if (m_angle.m_begin < m_angle.m_end)
            {
              m_angle.m_begin += two_pi;
            }
          else
            {
              m_angle.m_end += two_pi;
            }
        }

      float theta;

      theta = 0.5f * (m_angle.m_begin + m_angle.m_end);
      m_circle_sector_center.x() = t_cos(theta);
      m_circle_sector_center.y() = t_sin(theta);
      m_circle_sector_cos_angle = dot(cs, m_circle_sector_center);
      m_circle_sector_boundary[0] = cs;
      m_circle_sector_boundary[1] = ce;
    }
}

float
ArcSegment::
distance(fastuidraw::vec2 pt) const
{
  using namespace fastuidraw;

  float d, ptmag;

  pt -= m_center;
  ptmag = pt.magnitude();
  d = dot(m_circle_sector_center, pt / ptmag);
  if (d >=  m_circle_sector_cos_angle)
    {
      return t_abs(m_radius - ptmag);
    }
  else
    {
      vec2 a(pt - m_circle_sector_boundary[0]);
      vec2 b(pt - m_circle_sector_boundary[1]);
      return t_sqrt(t_min(dot(a, a), dot(b, b)));
    }
}

///////////////////////////////////
// ArcTessellatorStateNode methods
ArcTessellatorStateNode::
ArcTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h):
  m_start(h->start_pt()),
  m_end(h->end_pt()),
  m_recursion_depth(0)
{
  h->tessellate(nullptr, &m_L, &m_R, &m_mid);
  compute_values();
}

ArcTessellatorStateNode::
ArcTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h,
                        const fastuidraw::vec2 &start,
                        const fastuidraw::vec2 &end,
                        fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> rgn,
                        unsigned int depth):
  m_start(start),
  m_end(end),
  m_recursion_depth(depth)
{
  h->tessellate(rgn, &m_L, &m_R, &m_mid);
  compute_values();
}

void
ArcTessellatorStateNode::
compute_values(void)
{
  m_arc = ArcSegment(m_start, m_mid, m_end);
  if (m_arc.m_too_flat)
    {
      m_max_distance = fastuidraw::t_max(m_L->distance_to_line_segment(),
                                         m_R->distance_to_line_segment());
    }
  else
    {
      m_max_distance = fastuidraw::t_max(m_L->distance_to_arc(m_arc.m_radius, m_arc.m_center,
                                                              m_arc.m_circle_sector_center,
                                                              m_arc.m_circle_sector_cos_angle),
                                         m_R->distance_to_arc(m_arc.m_radius, m_arc.m_center,
                                                              m_arc.m_circle_sector_center,
                                                              m_arc.m_circle_sector_cos_angle));
    }
}

void
ArcTessellatorStateNode::
add_segment(fastuidraw::TessellatedPath::SegmentStorage *out_data) const
{
  if (m_arc.m_too_flat)
    {
      out_data->add_line_segment(m_start, m_mid);
      out_data->add_line_segment(m_mid, m_end);
    }
  else
    {
      out_data->add_arc_segment(m_start, m_end,
                                m_arc.m_center,
                                m_arc.m_radius,
                                m_arc.m_angle);
    }
}

ArcTessellatorStateNode
ArcTessellatorStateNode::
splitL(const fastuidraw::PathContour::interpolator_generic *h) const
{
  return ArcTessellatorStateNode(h, m_start, m_mid, m_L, m_recursion_depth + 1);
}

ArcTessellatorStateNode
ArcTessellatorStateNode::
splitR(const fastuidraw::PathContour::interpolator_generic *h) const
{
  return ArcTessellatorStateNode(h, m_mid, m_end, m_R, m_recursion_depth + 1);
}

/////////////////////////////////
// TessellationState methods
TessellationState::
TessellationState(const fastuidraw::PathContour::interpolator_generic *h):
  m_h(h),
  m_recursion_depth(0),
  m_minimum_tessellation_recursion(h->minimum_tessellation_recursion())
{
  m_nodes.push_back(ArcTessellatorStateNode(h));
}

void
TessellationState::
resume_tessellation(const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                    fastuidraw::TessellatedPath::SegmentStorage *out_data,
                    float *out_max_distance)
{
  std::vector<ArcTessellatorStateNode> new_nodes;

  for(const ArcTessellatorStateNode &node : m_nodes)
    {
      resume_tessellation_worker(node, tess_params, &new_nodes);
    }

  std::swap(new_nodes, m_nodes);
  *out_max_distance = 0.0f;
  m_recursion_depth = 0;
  for(const ArcTessellatorStateNode &node : m_nodes)
    {
      node.add_segment(out_data);
      m_recursion_depth = fastuidraw::t_max(m_recursion_depth, node.recursion_depth());
      *out_max_distance = fastuidraw::t_max(*out_max_distance, node.max_distance());
    }
}

void
TessellationState::
resume_tessellation_worker(const ArcTessellatorStateNode &node,
                           const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                           std::vector<ArcTessellatorStateNode> *dst)
{
  unsigned int recurse_level;

  recurse_level = node.recursion_depth();

  if (recurse_level == 0
      || recurse_level < m_minimum_tessellation_recursion
      || (tess_params.m_max_distance > 0.0f
	  && recurse_level <= tess_params.m_max_recursion
	  && node.max_distance() > tess_params.m_max_distance))
    {
      resume_tessellation_worker(node.splitL(m_h.get()), tess_params, dst);
      resume_tessellation_worker(node.splitR(m_h.get()), tess_params, dst);
    }
  else
    {
      dst->push_back(node);
    }
}

////////////////////////////////////////////
// fastuidraw::PathContour::interpolator_base methods
fastuidraw::PathContour::interpolator_base::
interpolator_base(const reference_counted_ptr<const interpolator_base> &prev,
                  const vec2 &end, enum PathEnums::edge_type_t tp)
{
  InterpolatorBasePrivate *d;
  d = FASTUIDRAWnew InterpolatorBasePrivate();
  m_d = d;
  d->m_prev = prev.get();
  d->m_end = end;

  if (prev.dynamic_cast_ptr<const FakeInterpolator>())
    {
      d->m_type = PathEnums::starts_new_edge;
    }
  else
    {
      d->m_type = tp;
    }
}

fastuidraw::PathContour::interpolator_base::
~interpolator_base(void)
{
  InterpolatorBasePrivate *d;
  d = static_cast<InterpolatorBasePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base>
fastuidraw::PathContour::interpolator_base::
prev_interpolator(void) const
{
  InterpolatorBasePrivate *d;
  d = static_cast<InterpolatorBasePrivate*>(m_d);
  return d->m_prev;
}

const fastuidraw::vec2&
fastuidraw::PathContour::interpolator_base::
start_pt(void) const
{
  InterpolatorBasePrivate *d;
  d = static_cast<InterpolatorBasePrivate*>(m_d);
  return (d->m_prev) ?  d->m_prev->end_pt() : d->m_end;
}

const fastuidraw::vec2&
fastuidraw::PathContour::interpolator_base::
end_pt(void) const
{
  InterpolatorBasePrivate *d;
  d = static_cast<InterpolatorBasePrivate*>(m_d);
  return d->m_end;
}

enum fastuidraw::PathEnums::edge_type_t
fastuidraw::PathContour::interpolator_base::
edge_type(void) const
{
  InterpolatorBasePrivate *d;
  d = static_cast<InterpolatorBasePrivate*>(m_d);
  return d->m_type;
}

//////////////////////////////////////////////
// fastuidraw::PathContour::interpolator_generic methods
fastuidraw::reference_counted_ptr<fastuidraw::PathContour::tessellation_state>
fastuidraw::PathContour::interpolator_generic::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     TessellatedPath::SegmentStorage *out_data,
                     float *out_max_distance) const
{
  fastuidraw::reference_counted_ptr<fastuidraw::PathContour::tessellation_state> return_value;

  return_value = FASTUIDRAWnew TessellationState(this);
  return_value->resume_tessellation(tess_params, out_data, out_max_distance);
  return return_value;
}

///////////////////////////////////
// BezierTessRegion methods
BezierTessRegion::
BezierTessRegion(const BezierTessRegion *parent, bool is_region_start):
  m_arc_distance_depth(parent->m_arc_distance_depth)
{
  float mid;

  m_pts.reserve(parent->m_pts.size());
  mid = 0.5f * (parent->m_start + parent->m_end);
  if (is_region_start)
    {
      m_start = parent->m_start;
      m_end = mid;
    }
  else
    {
      m_start = mid;
      m_end = parent->m_end;
    }
}

BezierTessRegion::
BezierTessRegion(fastuidraw::BoundingBox<float> &bb,
                 const fastuidraw::vec2 &start,
                 fastuidraw::c_array<const fastuidraw::vec2> ct,
                 const fastuidraw::vec2 &end):
  m_start(0.0f),
  m_end(1.0f)
{
  m_pts.reserve(ct.size() + 2);
  m_pts.push_back(start);
  for (const auto &pt : ct)
    {
      m_pts.push_back(pt);
    }
  m_pts.push_back(end);
  m_arc_distance_depth = fastuidraw::uint32_log2(m_pts.size());

  for(const fastuidraw::vec2 &pt : m_pts)
    {
      bb.union_point(pt);
    }
}

void
BezierTessRegion::
create_children(void) const
{
  using namespace fastuidraw;

  if (m_L)
    {
      FASTUIDRAWassert(m_R);
      return;
    }

  FASTUIDRAWassert(!m_R);
  m_L = FASTUIDRAWnew BezierTessRegion(this, true);
  m_R = FASTUIDRAWnew BezierTessRegion(this, false);

  c_array<vec2> dst;
  c_array<const vec2> src;
  vecN<std::vector<vec2>, 2> work_room;

  work_room[0].resize(m_pts.size());
  work_room[1].resize(m_pts.size());

  src = make_c_array(m_pts);
  m_L->m_pts.push_back(src.front());
  m_R->m_pts.push_back(src.back());

  /* For a Bezier curve, given by points p(0), .., p(n),
   * and a time 0 <= t <= 1, De Casteljau's algorithm is
   * the following.
   *
   * Let
   *   q(0, j) = p(j) for 0 <= j <= n,
   *   q(i + 1, j) = (1 - t) * q(i, j) + t * q(i, j + 1) for 0 <= i <= n, 0 <= j <= n - i
   * then
   *   The curve split at time t is given by
   *     A = { q(0, 0), q(1, 0), q(2, 0), ... , q(n, 0) }
   *     B = { q(n, 0), q(n - 1, 1), q(n - 2, 2), ... , q(0, n) }
   *   and
   *     the curve evaluated at t is given by q(n, 0).
   * We use t = 0.5 because we are always doing mid-point cutting.
   */
  for(unsigned int i = 0, endi = src.size(), sz = endi - 1; sz > 0 && i < endi; ++i, --sz)
    {
      dst = make_c_array(work_room[i & 1]).sub_array(0, sz);
      for(unsigned int j = 0; j < dst.size(); ++j)
        {
          dst[j] = 0.5f * src[j] + 0.5f * src[j + 1];
        }
      m_L->m_pts.push_back(dst.front());
      m_R->m_pts.push_back(dst.back());
      src = dst;
    }
  std::reverse(m_R->m_pts.begin(), m_R->m_pts.end());
}

float
BezierTessRegion::
distance_to_line_segment(void) const
{
  create_children();
  return fastuidraw::t_max(m_L->distance_to_line_segment_raw(front(), back()),
                           m_R->distance_to_line_segment_raw(front(), back()));
}

float
BezierTessRegion::
distance_to_arc(float arc_radius, fastuidraw::vec2 arc_center,
                fastuidraw::vec2 unit_vector_arc_middle,
                float cos_arc_angle) const
{
  ArcSegment A;

  A.m_too_flat = false;
  A.m_center = arc_center;
  A.m_radius = arc_radius;
  A.m_circle_sector_boundary[0] = m_pts.front();
  A.m_circle_sector_boundary[1] = m_pts.back();
  A.m_circle_sector_center = unit_vector_arc_middle;
  A.m_circle_sector_cos_angle = cos_arc_angle;

  return distance_to_arc_raw(m_arc_distance_depth, A);
}

float
BezierTessRegion::
distance_to_line_segment_raw(const fastuidraw::vec2 &A,
                             const fastuidraw::vec2 &B) const
{
  using namespace fastuidraw;

  /* Recall that a Bezier curve is bounded by its convex
   * hull. Thus an upper bound on the distance between
   * a line segment and the curve is just the maximum of
   * the distances of each of the points (control and end
   * points) to the line segment.
   */
  float return_value(0.0f);
  for(unsigned int i = 0, endi = m_pts.size(); i < endi; ++i)
    {
      float v;
      v = compute_distance(A, m_pts[i], B);
      return_value = t_max(return_value, v);
    }
  return return_value;
}

float
BezierTessRegion::
distance_to_arc_raw(unsigned int depth, const ArcSegment &A) const
{
  using namespace fastuidraw;

  create_children();
  if (depth <= 1)
    {
      return A.distance(m_L->back());
    }
  else
    {
      return t_max(m_L->distance_to_arc_raw(depth - 1, A),
		   m_R->distance_to_arc_raw(depth - 1, A));
    }
}

////////////////////////////////////
// fastuidraw::PathContour::bezier methods
fastuidraw::PathContour::bezier::
bezier(const reference_counted_ptr<const interpolator_base> &start,
       const vec2 &ct, const vec2 &end, enum PathEnums::edge_type_t tp):
  interpolator_generic(start, end, tp)
{
  BezierPrivate *d;
  vecN<vec2, 1> ctl(ct);

  d = FASTUIDRAWnew BezierPrivate();
  d->m_start_region = FASTUIDRAWnew BezierTessRegion(d->m_bb, start_pt(), ctl, end_pt());
  m_d = d;
}

fastuidraw::PathContour::bezier::
bezier(const reference_counted_ptr<const interpolator_base> &start, const vec2 &ct1,
       const vec2 &ct2, const vec2 &end, enum PathEnums::edge_type_t tp):
  interpolator_generic(start, end, tp)
{
  BezierPrivate *d;
  vecN<vec2, 2> ctl(ct1, ct2);

  d = FASTUIDRAWnew BezierPrivate();
  d->m_start_region = FASTUIDRAWnew BezierTessRegion(d->m_bb, start_pt(), ctl, end_pt());
  m_d = d;
}

fastuidraw::PathContour::bezier::
bezier(const reference_counted_ptr<const interpolator_base> &start,
       c_array<const vec2> ctl,
       const vec2 &end, enum PathEnums::edge_type_t tp):
  interpolator_generic(start, end, tp)
{
  BezierPrivate *d;

  d = FASTUIDRAWnew BezierPrivate();
  d->m_start_region = FASTUIDRAWnew BezierTessRegion(d->m_bb, start_pt(), ctl, end_pt());
  m_d = d;
}

fastuidraw::PathContour::bezier::
bezier(const bezier &q, const reference_counted_ptr<const interpolator_base> &prev):
  fastuidraw::PathContour::interpolator_generic(prev, q.end_pt(), q.edge_type())
{
  BezierPrivate *qd;
  qd = static_cast<BezierPrivate*>(q.m_d);
  m_d = FASTUIDRAWnew BezierPrivate(*qd);
}

fastuidraw::PathContour::bezier::
~bezier()
{
  BezierPrivate *d;
  d = static_cast<BezierPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

bool
fastuidraw::PathContour::bezier::
is_flat(void) const
{
  BezierPrivate *d;
  d = static_cast<BezierPrivate*>(m_d);
  return d->m_start_region->pts().size() <= 2;
}

fastuidraw::c_array<const fastuidraw::vec2>
fastuidraw::PathContour::bezier::
pts(void) const
{
  BezierPrivate *d;
  d = static_cast<BezierPrivate*>(m_d);
  return make_c_array(d->m_start_region->pts());
}

void
fastuidraw::PathContour::bezier::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  BezierPrivate *d;
  d = static_cast<BezierPrivate*>(m_d);
  *out_min_bb = d->m_bb.min_point();
  *out_max_bb = d->m_bb.max_point();
}

void
fastuidraw::PathContour::bezier::
tessellate(reference_counted_ptr<tessellated_region> in_region,
           reference_counted_ptr<tessellated_region> *out_regionA,
           reference_counted_ptr<tessellated_region> *out_regionB,
           vec2 *out_p) const
{
  BezierPrivate *d;
  d = static_cast<BezierPrivate*>(m_d);

  if (!in_region)
    {
      in_region = d->m_start_region;
    }

  BezierTessRegion *in_region_casted;
  FASTUIDRAWassert(dynamic_cast<BezierTessRegion*>(in_region.get()) != nullptr);
  in_region_casted = static_cast<BezierTessRegion*>(in_region.get());

  *out_regionA = in_region_casted->left_child();
  *out_regionB = in_region_casted->right_child();
  *out_p = in_region_casted->left_child()->back();
}

fastuidraw::PathContour::interpolator_base*
fastuidraw::PathContour::bezier::
deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const
{
  return FASTUIDRAWnew bezier(*this, prev);
}

unsigned int
fastuidraw::PathContour::bezier::
minimum_tessellation_recursion(void) const
{
  BezierPrivate *d;
  d = static_cast<BezierPrivate*>(m_d);

  return 1 + uint32_log2(d->m_start_region->pts().size());
}

//////////////////////////////////////
// fastuidraw::PathContour::flat methods
bool
fastuidraw::PathContour::flat::
is_flat(void) const
{
  return true;
}

fastuidraw::reference_counted_ptr<fastuidraw::PathContour::tessellation_state>
fastuidraw::PathContour::flat::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     TessellatedPath::SegmentStorage *out_data,
                     float *out_max_distance) const
{
  FASTUIDRAWunused(tess_params);

  out_data->add_line_segment(start_pt(), end_pt());
  *out_max_distance = 0.0f;
  return nullptr;
}

fastuidraw::PathContour::interpolator_base*
fastuidraw::PathContour::flat::
deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const
{
  return FASTUIDRAWnew flat(prev, end_pt(), edge_type());
}

void
fastuidraw::PathContour::flat::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  const vec2 &p0(start_pt());
  const vec2 &p1(end_pt());

  out_min_bb->x() = fastuidraw::t_min(p0.x(), p1.x());
  out_min_bb->y() = fastuidraw::t_min(p0.y(), p1.y());

  out_max_bb->x() = fastuidraw::t_max(p0.x(), p1.x());
  out_max_bb->y() = fastuidraw::t_max(p0.y(), p1.y());
}

//////////////////////////////////////
// fastuidraw::PathContour::arc methods
fastuidraw::PathContour::arc::
arc(const reference_counted_ptr<const interpolator_base> &start,
    float angle, const vec2 &end, enum PathEnums::edge_type_t tp):
  fastuidraw::PathContour::interpolator_base(start, end, tp)
{
  ArcPrivate *d;
  d = FASTUIDRAWnew ArcPrivate();
  m_d = d;

  float angle_coeff_dir;
  vec2 end_start, mid, n;
  float s, c, t;

  angle_coeff_dir = (angle > 0.0f) ? 1.0f : -1.0f;

  /* find the center of the circle. The center is
   * on the perpindicular bisecter of start and end.
   * The perpindicular bisector is given by
   * { t*n + mid | t real }
   */
  angle = fastuidraw::t_abs(angle);
  end_start = end_pt() - start_pt();
  mid = (end_pt() + start_pt()) * 0.5f;
  n = vec2(-end_start.y(), end_start.x());
  s = fastuidraw::t_sin(angle * 0.5f);
  c = fastuidraw::t_cos(angle * 0.5f);

  /* Let t be the point so that m_center = t*n + mid
   * Then
   *   tan(angle/2) = 0.5 * ||end - start|| / || m_center - mid ||
   *                = 0.5 * ||end - start|| / || t * n ||
   *                = 0.5 * || n || / || t * n||
   * thus
   *   |t| = 0.5/tan(angle/2) = 0.5 * c / s
   */
  t = angle_coeff_dir * 0.5f * c / s;
  d->m_center = mid + (t * n);

  vec2 start_center(start_pt() - d->m_center);

  d->m_radius = start_center.magnitude();
  d->m_start_angle = start_center.atan();
  d->m_angle_speed = angle_coeff_dir * angle;

  detail::bouding_box_union_arc(d->m_center, d->m_radius,
                                d->m_start_angle,
                                d->m_start_angle + d->m_angle_speed,
                                &d->m_bb);
}

fastuidraw::PathContour::arc::
arc(const arc &q, const reference_counted_ptr<const interpolator_base> &prev):
  fastuidraw::PathContour::interpolator_base(prev, q.end_pt(), q.edge_type())
{
  ArcPrivate *qd;
  qd = static_cast<ArcPrivate*>(q.m_d);
  m_d = FASTUIDRAWnew ArcPrivate(*qd);
}

fastuidraw::PathContour::arc::
~arc()
{
  ArcPrivate *d;
  d = static_cast<ArcPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::vec2
fastuidraw::PathContour::arc::
center(void) const
{
  ArcPrivate *d;
  d = static_cast<ArcPrivate*>(m_d);
  return d->m_center;
}

fastuidraw::range_type<float>
fastuidraw::PathContour::arc::
angle(void) const
{
  ArcPrivate *d;
  d = static_cast<ArcPrivate*>(m_d);
  return range_type<float>(d->m_start_angle,
                           d->m_start_angle + d->m_angle_speed);
}

bool
fastuidraw::PathContour::arc::
is_flat(void) const
{
  return false;
}

fastuidraw::reference_counted_ptr<fastuidraw::PathContour::tessellation_state>
fastuidraw::PathContour::arc::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     TessellatedPath::SegmentStorage *out_data,
                     float *out_max_distance) const
{
  ArcPrivate *d;

  d = static_cast<ArcPrivate*>(m_d);
  out_data->add_arc_segment(start_pt(), end_pt(),
                            d->m_center, d->m_radius,
                            range_type<float>(d->m_start_angle,
                                              d->m_start_angle + d->m_angle_speed));
  *out_max_distance = 0.0f;
  FASTUIDRAWunused(tess_params);

  return nullptr;
}

void
fastuidraw::PathContour::arc::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  ArcPrivate *d;
  d = static_cast<ArcPrivate*>(m_d);
  *out_min_bb = d->m_bb.min_point();
  *out_max_bb = d->m_bb.max_point();
}

fastuidraw::PathContour::interpolator_base*
fastuidraw::PathContour::arc::
deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const
{
  return FASTUIDRAWnew arc(*this, prev);
}

///////////////////////////////////
// fastuidraw::PathContour methods
fastuidraw::PathContour::
PathContour(void)
{
  m_d = FASTUIDRAWnew PathContourPrivate();
}

fastuidraw::PathContour::
~PathContour(void)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::PathContour::
start(const vec2 &start_pt)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  FASTUIDRAWassert(d->m_interpolators.empty());
  FASTUIDRAWassert(!d->m_end_to_start);

  d->m_start_pt = start_pt;

  /* m_interpolators[0] is an "empty" interpolator whose only purpose
   * it to provide a "previous" for the first interpolator added.
   */
  reference_counted_ptr<const interpolator_base> h;
  h = FASTUIDRAWnew FakeInterpolator(d->m_start_pt);
  d->m_interpolators.push_back(h);
}

void
fastuidraw::PathContour::
add_control_point(const fastuidraw::vec2 &pt)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_end_to_start);
  d->m_current_control_points.push_back(pt);
}

void
fastuidraw::PathContour::
clear_control_points(void)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);
  d->m_current_control_points.clear();
}

void
fastuidraw::PathContour::
to_point(const fastuidraw::vec2 &pt, enum PathEnums::edge_type_t etp)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;

  if (d->m_current_control_points.empty())
    {
      h = FASTUIDRAWnew flat(prev_interpolator(), pt, etp);
    }
  else
    {
      h = FASTUIDRAWnew bezier(prev_interpolator(),
                               make_c_array(d->m_current_control_points),
                               pt, etp);
    }
  d->m_current_control_points.clear();
  to_generic(h);
}

void
fastuidraw::PathContour::
to_arc(float angle, const vec2 &pt, enum PathEnums::edge_type_t etp)
{
  reference_counted_ptr<const interpolator_base> h;
  h = FASTUIDRAWnew arc(prev_interpolator(), angle, pt, etp);
  to_generic(h);
}

void
fastuidraw::PathContour::
to_generic(const reference_counted_ptr<const PathContour::interpolator_base> &p)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_interpolators.empty());
  FASTUIDRAWassert(d->m_current_control_points.empty());
  FASTUIDRAWassert(!d->m_end_to_start);
  FASTUIDRAWassert(p->prev_interpolator() == prev_interpolator());

  d->m_is_flat = d->m_is_flat && p->is_flat();
  d->m_interpolators.push_back(p);

  vec2 p0, p1;
  p->approximate_bounding_box(&p0, &p1);
  d->m_bb.union_point(p0);
  d->m_bb.union_point(p1);
}

void
fastuidraw::PathContour::
close_generic(reference_counted_ptr<const interpolator_base> p)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_end_to_start);
  FASTUIDRAWassert(d->m_current_control_points.empty());
  FASTUIDRAWassert(!d->m_interpolators.empty());
  FASTUIDRAWassert(p->prev_interpolator() == prev_interpolator());

  if (d->m_interpolators.size() == 1)
    {
      /* we have only the fake interpolator which will be replaced
       * by p; to avoid needing to handle the corner cases of
       * having just one interpolator we will add an additional
       * interpolator -after- p which starts and ends on the
       * end point of p.
       */
      reference_counted_ptr<const interpolator_base> h;

      to_generic(p);
      h = FASTUIDRAWnew flat(p, p->end_pt(), PathEnums::starts_new_edge);
      p = h;
    }

  /* hack-evil: we are going to replace m_interpolator[0]
   * with p, we also need to change m_interpolator[1]->m_prev
   * as well to p.
   */
  FASTUIDRAWassert(d->m_interpolators.size() > 1);

  InterpolatorBasePrivate *q;
  q = static_cast<InterpolatorBasePrivate*>(d->m_interpolators[1]->m_d);
  q->m_prev = p.get();
  FASTUIDRAWassert(q->m_type == PathEnums::starts_new_edge);

  d->m_interpolators[0] = p;
  d->m_end_to_start = p;
  d->m_is_flat = d->m_is_flat && p->is_flat();
}

void
fastuidraw::PathContour::
close(enum PathEnums::edge_type_t etp)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;

  if (d->m_current_control_points.empty())
    {
      h = FASTUIDRAWnew flat(prev_interpolator(), d->m_start_pt, etp);
    }
  else
    {
      h = FASTUIDRAWnew bezier(prev_interpolator(),
                               make_c_array(d->m_current_control_points),
                               d->m_start_pt, etp);
    }

  d->m_current_control_points.clear();
  close_generic(h);
}

void
fastuidraw::PathContour::
close_arc(float angle, enum PathEnums::edge_type_t etp)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;
  h = FASTUIDRAWnew arc(prev_interpolator(), angle, d->m_start_pt, etp);
  close_generic(h);
}

unsigned int
fastuidraw::PathContour::
number_points(void) const
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);
  return d->m_interpolators.size();
}

unsigned int
fastuidraw::PathContour::
number_interpolators(void) const
{
  unsigned int b(!closed()), n(number_points());
  return (n != 0) ? n - b : 0;
}

const fastuidraw::vec2&
fastuidraw::PathContour::
point(unsigned int I) const
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);
  return d->m_interpolators[I]->end_pt();
}

const fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base>&
fastuidraw::PathContour::
interpolator(unsigned int I) const
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  /* m_interpolator[I + 1] connects point(I) to point(I + 1). */
  unsigned int J(I + 1);
  FASTUIDRAWassert(J != 0u);
  FASTUIDRAWassert(J <= d->m_interpolators.size());

  /* J == m_interpolators.size() is the interpolator
   * connecting the last point added to the first
   * point of the contour, i.e. is given by m_end_to_start
   */
  return (J == d->m_interpolators.size())?
    d->m_end_to_start:
    d->m_interpolators[J];
}

const fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base>&
fastuidraw::PathContour::
prev_interpolator(void)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  FASTUIDRAWassert(!d->m_interpolators.empty());
  return d->m_interpolators[d->m_interpolators.size() - 1];
}

bool
fastuidraw::PathContour::
closed(void) const
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  return d->m_end_to_start;
}

bool
fastuidraw::PathContour::
is_flat(void) const
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);
  return d->m_is_flat;
}

fastuidraw::PathContour*
fastuidraw::PathContour::
deep_copy(void)
{
  PathContour *return_value;
  return_value = FASTUIDRAWnew PathContour();

  PathContourPrivate *d, *r;
  d = static_cast<PathContourPrivate*>(m_d);
  r = static_cast<PathContourPrivate*>(return_value->m_d);

  r->m_start_pt = d->m_start_pt;
  r->m_current_control_points = d->m_current_control_points;
  r->m_is_flat = d->m_is_flat;

  /* now we need to do the deep copies of the interpolator. eww. */
  r->m_interpolators.resize(d->m_interpolators.size());

  r->m_interpolators[0] = FASTUIDRAWnew FakeInterpolator(r->m_start_pt);
  for(unsigned int i = 1, endi = d->m_interpolators.size(); i < endi; ++i)
    {
      r->m_interpolators[i] = d->m_interpolators[i]->deep_copy(r->m_interpolators[i-1]);
    }

  if (d->m_end_to_start)
    {
      r->m_interpolators[0] = d->m_end_to_start->deep_copy(r->m_interpolators.back());
      r->m_end_to_start = r->m_interpolators[0];

      //we also need to replace r->m_interpolators[1]->m_prev with
      //the new value for r->m_interpolators[0]
      FASTUIDRAWassert(r->m_interpolators.size() > 1);
      InterpolatorBasePrivate *q;
      q = static_cast<InterpolatorBasePrivate*>(r->m_interpolators[1]->m_d);
      q->m_prev = r->m_end_to_start.get();
    }
  return return_value;
}

bool
fastuidraw::PathContour::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  *out_min_bb = d->m_bb.min_point();
  *out_max_bb = d->m_bb.max_point();

  return !d->m_bb.empty();
}

/////////////////////////////////
// TessellatedPathList methods
const typename TessellatedPathList::TessellatedPathRef&
TessellatedPathList::
tessellation(const fastuidraw::Path &path, float max_distance)
{
  using namespace fastuidraw;
  using namespace detail;

  if (m_data.empty())
    {
      TessellationParams params;
      m_data.push_back(FASTUIDRAWnew TessellatedPath(path, params, &m_refiner));
    }

  if (max_distance <= 0.0 || path.is_flat())
    {
      return m_data.front();
    }

  if (m_data.back()->max_distance() <= max_distance)
    {
      typename std::vector<TessellatedPathRef>::const_iterator iter;
      iter = std::lower_bound(m_data.begin(),
                              m_data.end(),
                              max_distance,
                              reverse_compare_max_distance);

      FASTUIDRAWassert(iter != m_data.end());
      FASTUIDRAWassert(*iter);
      FASTUIDRAWassert((*iter)->max_distance() <= max_distance);
      return *iter;
    }

  if (m_done)
    {
      return m_data.back();
    }

  float current_max_distance;

  current_max_distance = m_data.back()->max_distance();

  while(!m_done && m_data.back()->max_distance() > max_distance)
    {
      current_max_distance *= 0.5f;
      while(!m_done && m_data.back()->max_distance() > current_max_distance)
        {
          TessellatedPathRef ref;

          m_refiner->refine_tessellation(current_max_distance, 1);
          ref = m_refiner->tessellated_path();

          /* we only add a tessellation if it is finer than the last one
           * added. However, we do not abort if it is not as sometimes
           * (especially with arc-tessellation) more refinement can make
           * the tessellation improve.
           */
          if (m_data.back()->max_distance() > ref->max_distance())
            {
              m_data.push_back(ref);
            }

          /* We set an absolute abort at max_refine_recursion_limit
           * which represents that after sub-dividing that much, one
           * is just handling numerical garbage.
           */
          if (ref->max_recursion() > MAX_REFINE_RECURSION_LIMIT)
            {
              m_done = true;
              m_refiner = nullptr;
            }
        }
    }

  return m_data.back();
}

/////////////////////////////////
// PathPrivate methods
PathPrivate::
PathPrivate(fastuidraw::Path *p):
  m_next_edge_type(fastuidraw::PathEnums::starts_new_edge),
  m_tess_list(),
  m_start_check_bb(0),
  m_is_flat(true),
  m_p(p)
{
}

PathPrivate::
PathPrivate(fastuidraw::Path *p, const PathPrivate &obj):
  m_contours(obj.m_contours),
  m_next_edge_type(obj.m_next_edge_type),
  m_tess_list(obj.m_tess_list),
  m_start_check_bb(obj.m_start_check_bb),
  m_bb(obj.m_bb),
  m_is_flat(obj.m_is_flat),
  m_p(p)
{
  /* if the last contour is not closed, we need to do a
   * deep copy on it.
   */
  if (!m_contours.empty() && !m_contours.back()->closed())
    {
      m_contours.back() = m_contours.back()->deep_copy();
      m_is_flat = m_is_flat && m_contours.back()->is_flat();
    }
}

const fastuidraw::reference_counted_ptr<fastuidraw::PathContour>&
PathPrivate::
current_contour(void)
{
  start_contour_if_necessary();
  FASTUIDRAWassert(!m_contours.empty());
  clear_tesses();
  return m_contours.back();
}

void
PathPrivate::
move_common(const fastuidraw::vec2 &pt)
{
  bool last_contour_flat;

  clear_tesses();
  last_contour_flat = m_contours.empty() || m_contours.back()->is_flat();
  m_is_flat = m_is_flat && last_contour_flat;
  m_contours.push_back(FASTUIDRAWnew fastuidraw::PathContour());
  m_contours.back()->start(pt);
}

void
PathPrivate::
clear_tesses(void)
{
  m_tess_list.clear();
}

void
PathPrivate::
start_contour_if_necessary(void)
{
  if (!m_contours.empty() && !m_contours.back()->closed())
    {
      return;
    }

  fastuidraw::vec2 pt;
  if (m_contours.empty())
    {
      pt = fastuidraw::vec2(0.0f, 0.0f);
    }
  else
    {
      pt = m_contours.back()->point(m_contours.back()->number_points() - 1);
    }
  move_common(pt);
}

/////////////////////////////////////////
// fastuidraw::Path methods
fastuidraw::Path::
Path(void)
{
  m_d = FASTUIDRAWnew PathPrivate(this);
}

fastuidraw::Path::
Path(const Path &obj)
{
  PathPrivate *obj_d;
  obj_d = static_cast<PathPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PathPrivate(this, *obj_d);
}

void
fastuidraw::Path::
swap(Path &obj)
{
  PathPrivate *obj_d, *d;

  std::swap(obj.m_d, m_d);
  d = static_cast<PathPrivate*>(m_d);
  obj_d = static_cast<PathPrivate*>(obj.m_d);

  d->m_p = this;
  obj_d->m_p = &obj;
}

const fastuidraw::Path&
fastuidraw::Path::
operator=(const Path &rhs)
{
  Path temp(rhs);
  swap(temp);
  return *this;
}

fastuidraw::Path::
~Path()
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

bool
fastuidraw::Path::
is_flat(void) const
{
  bool last_contour_flat;
  PathPrivate *d;

  d = static_cast<PathPrivate*>(m_d);
  last_contour_flat = d->m_contours.empty() || d->m_contours.back()->is_flat();
  return d->m_is_flat && last_contour_flat;
}

void
fastuidraw::Path::
clear(void)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->clear_tesses();
  d->m_contours.clear();
  d->m_start_check_bb = 0u;
}

fastuidraw::Path&
fastuidraw::Path::
add_contour(const reference_counted_ptr<const PathContour> &pcontour)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  reference_counted_ptr<PathContour> contour, r;

  contour = pcontour.const_cast_ptr<PathContour>();
  if (!contour->closed())
    {
      contour = contour->deep_copy();
    }

  d->m_is_flat = d->m_is_flat && contour->is_flat();
  d->clear_tesses();

  if (!d->m_contours.empty())
    {
      r = d->m_contours.back();
      d->m_contours.back() = contour;
      d->m_contours.push_back(r);
    }
  else
    {
      d->m_contours.push_back(contour);
    }

  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
add_contours(const Path &path)
{
  PathPrivate *d, *pd;
  reference_counted_ptr<PathContour> r;

  d = static_cast<PathPrivate*>(m_d);
  pd = static_cast<PathPrivate*>(path.m_d);
  if (pd->m_contours.empty())
    {
      return *this;
    }

  if (!d->m_contours.empty())
    {
      r = d->m_contours.back();
      d->m_contours.pop_back();
    }

  for (reference_counted_ptr<PathContour> c : pd->m_contours)
    {
      d->m_is_flat = d->m_is_flat && c->is_flat();
      if (!c->closed())
        {
          c = c->deep_copy();
        }
      d->m_contours.push_back(c);
    }

  if (r)
    {
      d->m_contours.push_back(r);
    }

  d->clear_tesses();
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
move(const fastuidraw::vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->move_common(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
close_contour(enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->close(etp);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->m_next_edge_type = etp;
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(const fastuidraw::vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  if (d->m_contours.empty() || d->m_contours.back()->closed())
    {
      d->move_common(pt);
    }
  else
    {
      d->current_contour()->to_point(pt, d->m_next_edge_type);
    }
  d->m_next_edge_type = PathEnums::starts_new_edge;
  return *this;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath>&
fastuidraw::Path::
tessellation(void) const
{
  return tessellation(-1.0f);
}

const fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath>&
fastuidraw::Path::
tessellation(float max_distance) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  return d->m_tess_list.tessellation(*this, max_distance);
}

bool
fastuidraw::Path::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  for(unsigned endi = d->m_contours.size();
      d->m_start_check_bb < endi; ++d->m_start_check_bb)
    {
      vec2 p0, p1;

      if(d->m_contours[d->m_start_check_bb]->approximate_bounding_box(&p0, &p1))
        {
          d->m_bb.union_point(p0);
          d->m_bb.union_point(p1);
        }
    }

  *out_min_bb = d->m_bb.min_point();
  *out_max_bb = d->m_bb.max_point();
  return !d->m_bb.empty();
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(const control_point &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->current_contour()->add_control_point(pt.m_location);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(const arc &a)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->current_contour()->to_arc(a.m_angle, a.m_pt, d->m_next_edge_type);
  d->m_next_edge_type = PathEnums::starts_new_edge;
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(contour_close)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->current_contour()->close(d->m_next_edge_type);
  d->m_next_edge_type = PathEnums::starts_new_edge;
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(contour_close_arc a)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->current_contour()->close_arc(a.m_angle, d->m_next_edge_type);
  d->m_next_edge_type = PathEnums::starts_new_edge;
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
line_to(const vec2 &pt,
        enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->current_contour()->to_point(pt, etp);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
quadratic_to(const vec2 &ct, const vec2 &pt,
             enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->clear_control_points();
  h->add_control_point(ct);
  h->to_point(pt, etp);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
cubic_to(const vec2 &ct1, const vec2 &ct2, const vec2 &pt,
         enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->clear_control_points();
  h->add_control_point(ct1);
  h->add_control_point(ct2);
  h->to_point(pt, etp);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
arc_to(float angle, const vec2 &pt,
       enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->to_arc(angle, pt, etp);
  return *this;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base>&
fastuidraw::Path::
prev_interpolator(void)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  return d->current_contour()->prev_interpolator();
}

fastuidraw::Path&
fastuidraw::Path::
custom_to(const reference_counted_ptr<const PathContour::interpolator_base> &p)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->current_contour()->to_generic(p);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
close_contour_arc(float angle,
                  enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->close_arc(angle, etp);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
close_contour_quadratic(const vec2 &ct,
                        enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->clear_control_points();
  h->add_control_point(ct);
  h->close(etp);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
close_contour_cubic(const vec2 &ct1, const vec2 &ct2,
                    enum PathEnums::edge_type_t etp)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->clear_control_points();
  h->add_control_point(ct1);
  h->add_control_point(ct2);
  h->close(etp);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
close_contour_custom(const reference_counted_ptr<const PathContour::interpolator_base> &p)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->current_contour()->close_generic(p);
  return *this;
}

unsigned int
fastuidraw::Path::
number_contours(void) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  return d->m_contours.size();
}

fastuidraw::reference_counted_ptr<const fastuidraw::PathContour>
fastuidraw::Path::
contour(unsigned int i) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  return d->m_contours[i];
}
