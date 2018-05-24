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
#include "private/util_private_ostream.hpp"
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

  class LinearTessellatorStateNode
  {
  public:
    explicit
    LinearTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h);

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

    LinearTessellatorStateNode
    splitL(const fastuidraw::PathContour::interpolator_generic *h) const;

    LinearTessellatorStateNode
    splitR(const fastuidraw::PathContour::interpolator_generic *h) const;

  private:
    LinearTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h,
                               const fastuidraw::vec2 &p0,
                               const fastuidraw::vec2 &p1,
                               fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> rgn,
                               unsigned int depth);

    fastuidraw::vec2 m_pt0, m_pt1, m_mid_pt;
    fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> m_L, m_R;

    float m_max_distance;
    unsigned int m_recursion_depth;
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
                            const fastuidraw::vec2 &start,
                            const fastuidraw::vec2 &mid,
                            const fastuidraw::vec2 &end,
                            fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> L,
                            fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> R,
                            float partition_max_distance,
                            unsigned int depth);
    void
    compute_circle(const fastuidraw::PathContour::interpolator_generic *h,
                   fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> L,
                   fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> R,
                   float partition_max_distance);

    fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> m_L0, m_L1;
    fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> m_R0, m_R1;
    fastuidraw::vec2 m_start, m_midL, m_mid, m_midR, m_end;
    float m_partition_max_distanceL, m_partition_max_distanceR;

    float m_max_distance;
    unsigned int m_recursion_depth;

    /* segment data computed from m_start, m_mid, m_end.
     * the value of m_max_distance comes from the distance
     * between the circle and m_midL and m_midR.
     */
    bool m_too_flat;
    float m_radius;
    fastuidraw::vec2 m_center;
    fastuidraw::range_type<float> m_angle;
  };

  template<typename T>
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
    resume_tessellation_worker(const T &node,
                               const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                               std::vector<T> *dst);


    fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_generic> m_h;
    std::vector<T> m_nodes;
    unsigned int m_recursion_depth;
    unsigned int m_minimum_tessellation_recursion;
  };

  class InterpolatorBasePrivate
  {
  public:
    //note is weak pointer to prevent circular pain.
    const fastuidraw::PathContour::interpolator_base *m_prev;
    fastuidraw::vec2 m_end;
  };

  class BezierTessRegion:
    public fastuidraw::PathContour::interpolator_generic::tessellated_region
  {
  public:
    explicit
    BezierTessRegion(BezierTessRegion *parent, bool is_region_start)
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

    explicit
    BezierTessRegion(void):
      m_start(0.0f),
      m_end(1.0f)
    {}

    float
    compute_curve_distance(void)
    {
      /* Compute the maximum distance between the points
       * of a BezierTessRegion and the line segment between
       * the start and  end point of the BezierTessRegion.
       * The curve is contained within the convex hull of the
       * points, so this computation is fast, conservative
       * value for getting the curve_distance.
       */
      float return_value(0.0f);
      for(unsigned int i = 1, endi = m_pts.size(); i + 1 < endi; ++i)
        {
          float v;
          v = compute_distance(m_pts.front(), m_pts[i], m_pts.back());
          return_value = fastuidraw::t_max(return_value, v);
        }
      return return_value;
    }

    std::vector<fastuidraw::vec2> m_pts;
    float m_start, m_end;
  };

  class BezierPrivate
  {
  public:
    void
    init(void);

    fastuidraw::BoundingBox<float> m_bb;
    fastuidraw::reference_counted_ptr<BezierTessRegion> m_start_region;
    std::vector<fastuidraw::vec2> m_pts;
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_work_room;
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
    TessellatedPathList(bool allow_arcs):
      m_allow_arcs(allow_arcs),
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
    class reverse_compare_max_distance
    {
    public:
      bool
      operator()(const TessellatedPathRef &lhs, float rhs) const
      {
        return lhs->max_distance() > rhs;
      }

      bool
      operator()(const TessellatedPathRef &lhs,
                 const TessellatedPathRef &rhs) const
      {
        return lhs->max_distance() > rhs->max_distance();
      }
    };

    bool m_allow_arcs, m_done;
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
    current_contour(void)
    {
      FASTUIDRAWassert(!m_contours.empty());
      clear_tesses();
      return m_contours.back();
    }

    void
    move_common(const fastuidraw::vec2 &pt);

    void
    clear_tesses(void);

    void
    close_back_contour(void);

    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PathContour> > m_contours;
    TessellatedPathList m_tess_list;
    TessellatedPathList m_arc_tess_list;

    /* m_start_check_bb gives the index into m_contours that
     * have not had their bounding box absorbed m_bb
     */
    unsigned int m_start_check_bb;
    fastuidraw::BoundingBox<float> m_bb;
    bool m_is_flat;
    fastuidraw::Path *m_p;
  };
}

///////////////////////////////////
// LinearTessellatorStateNode methods
LinearTessellatorStateNode::
LinearTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h):
  m_pt0(h->start_pt()),
  m_pt1(h->end_pt()),
  m_recursion_depth(0)
{
  h->tessellate(nullptr, &m_L, &m_R, &m_mid_pt, &m_max_distance);
}

LinearTessellatorStateNode::
LinearTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h,
                           const fastuidraw::vec2 &p0,
                           const fastuidraw::vec2 &p1,
                           fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> rgn,
                           unsigned int depth):
  m_pt0(p0),
  m_pt1(p1),
  m_recursion_depth(depth)
{
  FASTUIDRAWassert(rgn);
  h->tessellate(rgn, &m_L, &m_R, &m_mid_pt, &m_max_distance);
}

void
LinearTessellatorStateNode::
add_segment(fastuidraw::TessellatedPath::SegmentStorage *out_data) const
{
  /* We add the two segments because the value of m_max_distance
   * is the error estimate using the two segments.
   */
  out_data->add_line_segment(m_pt0, m_mid_pt);
  out_data->add_line_segment(m_mid_pt, m_pt1);
}

LinearTessellatorStateNode
LinearTessellatorStateNode::
splitL(const fastuidraw::PathContour::interpolator_generic *h) const
{
  return LinearTessellatorStateNode(h, m_pt0, m_mid_pt, m_L, m_recursion_depth + 1);
}

LinearTessellatorStateNode
LinearTessellatorStateNode::
splitR(const fastuidraw::PathContour::interpolator_generic *h) const
{
  return LinearTessellatorStateNode(h, m_mid_pt, m_pt1, m_R, m_recursion_depth + 1);
}

///////////////////////////////////
// ArcTessellatorStateNode methods
ArcTessellatorStateNode::
ArcTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h):
  m_start(h->start_pt()),
  m_end(h->end_pt()),
  m_recursion_depth(0)
{
  using namespace fastuidraw;

  reference_counted_ptr<PathContour::interpolator_generic::tessellated_region> L, R;
  float partition_max_distance;

  h->tessellate(nullptr, &L, &R, &m_mid, &partition_max_distance);
  compute_circle(h, L, R, partition_max_distance);
}

ArcTessellatorStateNode::
ArcTessellatorStateNode(const fastuidraw::PathContour::interpolator_generic *h,
                        const fastuidraw::vec2 &start,
                        const fastuidraw::vec2 &mid,
                        const fastuidraw::vec2 &end,
                        fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> L,
                        fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> R,
                        float partition_max_distance,
                        unsigned int depth):
  m_start(start),
  m_mid(mid),
  m_end(end),
  m_recursion_depth(depth)
{
  compute_circle(h, L, R, partition_max_distance);
}

void
ArcTessellatorStateNode::
compute_circle(const fastuidraw::PathContour::interpolator_generic *h,
               fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> L,
               fastuidraw::reference_counted_ptr<fastuidraw::PathContour::interpolator_generic::tessellated_region> R,
               float partition_max_distance)
{
  using namespace fastuidraw;

  FASTUIDRAWassert(L);
  FASTUIDRAWassert(R);

  h->tessellate(L, &m_L0, &m_L1, &m_midL, &m_partition_max_distanceL);
  h->tessellate(R, &m_R0, &m_R1, &m_midR, &m_partition_max_distanceR);

  /* compute the center of the circle going through m_start, m_mid
   * and m_end by computing the intersection of the perpindicular
   * bisectors of the line segments [m_start, m_mid] and [m_mid, m_end]
   */
  vec2 v0, v1, n0, n1, p0, p1;
  float det;
  static float tol(0.00001f);

  p0 = 0.5f * (m_start + m_mid);
  p1 = 0.5f * (m_mid + m_end);

  v0 = m_start - m_mid;
  n0 = vec2(-v0.y(), v0.x());

  v1 = m_mid - m_end;
  n1 = vec2(-v1.y(), v1.x());

  det = n1.y() * n0.x() - n0.y() * n1.x();
  m_too_flat = (t_abs(det) < tol);

  if (m_too_flat)
    {
      m_max_distance = partition_max_distance;
    }
  else
    {
      float s, tL, tR;
      vec2 cs, ce;
      const float pi(M_PI), two_pi(2.0f * pi);

      s = dot(v1, p1 - p0) / det;
      m_center = p0 + s * n0;
      m_radius = (m_center - m_mid).magnitude();

      tL = t_abs(m_radius - (m_center - m_midL).magnitude());
      tR = t_abs(m_radius - (m_center - m_midR).magnitude());
      m_max_distance = t_max(tL, tR);

      cs = m_start - m_center;
      ce = m_end - m_center;

      m_angle.m_begin = cs.atan();
      m_angle.m_end = ce.atan();

      /* Under linear tessellation the points from m_start to m_end
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
    }
}

void
ArcTessellatorStateNode::
add_segment(fastuidraw::TessellatedPath::SegmentStorage *out_data) const
{
  if (m_too_flat)
    {
      out_data->add_line_segment(m_start, m_mid);
      out_data->add_line_segment(m_mid, m_end);
    }
  else
    {
      out_data->add_arc_segment(m_start, m_end, m_center, m_radius, m_angle);
    }
}

ArcTessellatorStateNode
ArcTessellatorStateNode::
splitL(const fastuidraw::PathContour::interpolator_generic *h) const
{
  return ArcTessellatorStateNode(h, m_start, m_midL, m_mid, m_L0, m_L1,
                                 m_partition_max_distanceL, m_recursion_depth + 1);
}

ArcTessellatorStateNode
ArcTessellatorStateNode::
splitR(const fastuidraw::PathContour::interpolator_generic *h) const
{
  return ArcTessellatorStateNode(h, m_mid, m_midR, m_end, m_R0, m_R1,
                                 m_partition_max_distanceL, m_recursion_depth + 1);
}

/////////////////////////////////
// TessellationState methods
template<typename T>
TessellationState<T>::
TessellationState(const fastuidraw::PathContour::interpolator_generic *h):
  m_h(h),
  m_recursion_depth(0),
  m_minimum_tessellation_recursion(h->minimum_tessellation_recursion())
{
  m_nodes.push_back(T(h));
}

template<typename T>
void
TessellationState<T>::
resume_tessellation(const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                    fastuidraw::TessellatedPath::SegmentStorage *out_data,
                    float *out_max_distance)
{
  std::vector<T> new_nodes;

  for(const T &node : m_nodes)
    {
      resume_tessellation_worker(node, tess_params, &new_nodes);
    }

  std::swap(new_nodes, m_nodes);
  *out_max_distance = 0.0f;
  m_recursion_depth = 0;
  for(const T &node : m_nodes)
    {
      node.add_segment(out_data);
      m_recursion_depth = fastuidraw::t_max(m_recursion_depth, node.recursion_depth());
      *out_max_distance = fastuidraw::t_max(*out_max_distance, node.max_distance());
    }
}

template<typename T>
void
TessellationState<T>::
resume_tessellation_worker(const T &node,
                           const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                           std::vector<T> *dst)
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

////////////////////////////////////////
// BezierPrivate methods
void
BezierPrivate::
init(void)
{
  FASTUIDRAWassert(!m_pts.empty());

  for(const fastuidraw::vec2 &pt : m_pts)
    {
      m_bb.union_point(pt);
    }
  m_start_region = FASTUIDRAWnew BezierTessRegion();
  m_start_region->m_pts = m_pts; //original region uses original points.

  m_work_room[0].resize(m_pts.size());
  m_work_room[1].resize(m_pts.size());
}

////////////////////////////////////////////
// fastuidraw::PathContour::interpolator_base methods
fastuidraw::PathContour::interpolator_base::
interpolator_base(const reference_counted_ptr<const interpolator_base> &prev, const vec2 &end)
{
  InterpolatorBasePrivate *d;
  d = FASTUIDRAWnew InterpolatorBasePrivate();
  m_d = d;
  d->m_prev = prev.get();
  d->m_end = end;
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

//////////////////////////////////////////////
// fastuidraw::PathContour::interpolator_generic methods
fastuidraw::reference_counted_ptr<fastuidraw::PathContour::tessellation_state>
fastuidraw::PathContour::interpolator_generic::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     TessellatedPath::SegmentStorage *out_data,
                     float *out_max_distance) const
{
  fastuidraw::reference_counted_ptr<fastuidraw::PathContour::tessellation_state> return_value;

  if (tess_params.m_allow_arcs)
    {
      return_value = FASTUIDRAWnew TessellationState<ArcTessellatorStateNode>(this);
    }
  else
    {
      return_value = FASTUIDRAWnew TessellationState<LinearTessellatorStateNode>(this);
    }
  return_value->resume_tessellation(tess_params, out_data, out_max_distance);
  return return_value;
}

////////////////////////////////////
// fastuidraw::PathContour::bezier methods
fastuidraw::PathContour::bezier::
bezier(const reference_counted_ptr<const interpolator_base> &start, const vec2 &ct, const vec2 &end):
  interpolator_generic(start, end)
{
  BezierPrivate *d;
  d = FASTUIDRAWnew BezierPrivate();
  m_d = d;
  d->m_pts.resize(3);
  d->m_pts[0] = start_pt();
  d->m_pts[1] = ct;
  d->m_pts[2] = end_pt();
  d->init();
}

fastuidraw::PathContour::bezier::
bezier(const reference_counted_ptr<const interpolator_base> &start, const vec2 &ct1,
       const vec2 &ct2, const vec2 &end):
  interpolator_generic(start, end)
{
  BezierPrivate *d;
  d = FASTUIDRAWnew BezierPrivate();
  m_d = d;
  d->m_pts.resize(4);
  d->m_pts[0] = start_pt();
  d->m_pts[1] = ct1;
  d->m_pts[2] = ct2;
  d->m_pts[3] = end_pt();
  d->init();
}

fastuidraw::PathContour::bezier::
bezier(const reference_counted_ptr<const interpolator_base> &start,
       c_array<const vec2> control_pts,
       const vec2 &end):
  interpolator_generic(start, end)
{
  BezierPrivate *d;
  d = FASTUIDRAWnew BezierPrivate();
  m_d = d;
  d->m_pts.resize(control_pts.size() + 2);
  std::copy(control_pts.begin(), control_pts.end(), d->m_pts.begin() + 1);
  d->m_pts.front() = start_pt();
  d->m_pts.back() = end;
  d->init();
}

fastuidraw::PathContour::bezier::
bezier(const bezier &q, const reference_counted_ptr<const interpolator_base> &prev):
  fastuidraw::PathContour::interpolator_generic(prev, q.end_pt())
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
  return d->m_pts.size() <= 2;
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
           vec2 *out_p, float *out_max_distance) const
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

  reference_counted_ptr<BezierTessRegion> newA, newB;
  newA = FASTUIDRAWnew BezierTessRegion(in_region_casted, true);
  newB = FASTUIDRAWnew BezierTessRegion(in_region_casted, false);

  c_array<vec2> dst, src;
  src = make_c_array(in_region_casted->m_pts);

  newA->m_pts.push_back(src.front());
  newB->m_pts.push_back(src.back());

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
      dst = make_c_array(d->m_work_room[i & 1]).sub_array(0, sz);
      for(unsigned int j = 0; j < dst.size(); ++j)
        {
          dst[j] = 0.5f * src[j] + 0.5f * src[j + 1];
        }
      newA->m_pts.push_back(dst.front());
      newB->m_pts.push_back(dst.back());
      src = dst;
    }
  std::reverse(newB->m_pts.begin(), newB->m_pts.end());

  *out_regionA = newA;
  *out_regionB = newB;
  *out_p = newA->m_pts.back();

  FASTUIDRAWassert(out_max_distance);
  *out_max_distance = t_max(newA->compute_curve_distance(), newB->compute_curve_distance());
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

  return 1 + uint32_log2(d->m_pts.size());
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
  return FASTUIDRAWnew flat(prev, end_pt());
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
arc(const reference_counted_ptr<const interpolator_base> &start, float angle, const vec2 &end):
  fastuidraw::PathContour::interpolator_base(start, end)
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
  fastuidraw::PathContour::interpolator_base(prev, q.end_pt())
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
  TessellatedPath::segment S;
  d = static_cast<ArcPrivate*>(m_d);

  if (tess_params.m_allow_arcs)
    {
      out_data->add_arc_segment(start_pt(), end_pt(),
                                d->m_center, d->m_radius,
                                range_type<float>(d->m_start_angle,
                                                  d->m_start_angle + d->m_angle_speed));
      *out_max_distance = 0.0f;
    }
  else
    {
      float a, da, delta_angle;
      unsigned int needed_size;
      vec2 prev_pt;

      needed_size = detail::number_segments_for_tessellation(d->m_radius, t_abs(d->m_angle_speed), tess_params);
      delta_angle = d->m_angle_speed / static_cast<float>(needed_size);

      a = d->m_start_angle + delta_angle;
      da = delta_angle;
      prev_pt = start_pt();

      for(unsigned int i = 1; i <= needed_size; ++i, a += delta_angle, da += delta_angle)
        {
          TessellatedPath::segment S;
          vec2 p;

          if (i == needed_size)
            {
              p = end_pt();
            }
          else
            {
              float c, s;

              c = d->m_radius * t_cos(a);
              s = d->m_radius * t_sin(a);
              p = d->m_center + vec2(c, s);
            }
          out_data->add_line_segment(prev_pt, p);
          prev_pt = p;
        }

      const float pi(M_PI);
      float eff(t_min(pi, t_abs(0.5f * delta_angle)));

      *out_max_distance = d->m_radius * (1.0f - t_cos(eff));
    }
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
  h = FASTUIDRAWnew flat(reference_counted_ptr<const interpolator_base>(), d->m_start_pt);
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
to_point(const fastuidraw::vec2 &pt)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;

  if (d->m_current_control_points.empty())
    {
      h = FASTUIDRAWnew flat(prev_interpolator(), pt);
    }
  else
    {
      h = FASTUIDRAWnew bezier(prev_interpolator(),
                               make_c_array(d->m_current_control_points),
                               pt);
    }
  d->m_current_control_points.clear();
  to_generic(h);
}

void
fastuidraw::PathContour::
to_arc(float angle, const vec2 &pt)
{
  reference_counted_ptr<const interpolator_base> h;
  h = FASTUIDRAWnew arc(prev_interpolator(), angle, pt);
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
}

void
fastuidraw::PathContour::
end_generic(reference_counted_ptr<const interpolator_base> p)
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
      h = FASTUIDRAWnew flat(p, p->end_pt());
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

  d->m_interpolators[0] = p;
  d->m_end_to_start = p;
  d->m_is_flat = d->m_is_flat && p->is_flat();

  /* compute bounding box after ending the PathContour.  */
  for(unsigned int i = 0, endi = d->m_interpolators.size(); i < endi; ++i)
    {
      vec2 p0, p1;
      d->m_interpolators[i]->approximate_bounding_box(&p0, &p1);
      d->m_bb.union_point(p0);
      d->m_bb.union_point(p1);
    }
}

void
fastuidraw::PathContour::
end(void)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;

  if (d->m_current_control_points.empty())
    {
      h = FASTUIDRAWnew flat(prev_interpolator(), d->m_start_pt);
    }
  else
    {
      h = FASTUIDRAWnew bezier(prev_interpolator(),
                               make_c_array(d->m_current_control_points),
                               d->m_start_pt);
    }

  d->m_current_control_points.clear();
  end_generic(h);
}

void
fastuidraw::PathContour::
end_arc(float angle)
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;
  h = FASTUIDRAWnew arc(prev_interpolator(), angle, d->m_start_pt);
  end_generic(h);
}

unsigned int
fastuidraw::PathContour::
number_points(void) const
{
  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);
  return d->m_interpolators.size();
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

  /* m_interpolator[I+1] connects point(I) to point(I+1).
   */
  unsigned int J(I+1);
  FASTUIDRAWassert(J <= d->m_interpolators.size());

  /* interpolator(number_points()) is the interpolator
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
ended(void) const
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

  /* now we need to do the deep copies of the interpolator. eww.
   */
  r->m_interpolators.resize(d->m_interpolators.size());

  r->m_interpolators[0] = FASTUIDRAWnew flat(reference_counted_ptr<const interpolator_base>(), r->m_start_pt);
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
  if (!ended())
    {
      return false;
    }

  PathContourPrivate *d;
  d = static_cast<PathContourPrivate*>(m_d);

  *out_min_bb = d->m_bb.min_point();
  *out_max_bb = d->m_bb.max_point();

  return true;
}

/////////////////////////////////
// TessellatedPathList methods
const typename TessellatedPathList::TessellatedPathRef&
TessellatedPathList::
tessellation(const fastuidraw::Path &path, float max_distance)
{
  using namespace fastuidraw;

  if (m_data.empty())
    {
      TessellationParams params;

      params.allow_arcs(m_allow_arcs);
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
                              reverse_compare_max_distance());

      FASTUIDRAWassert(iter != m_data.end());
      FASTUIDRAWassert(*iter);
      FASTUIDRAWassert((*iter)->max_distance() <= max_distance);
      return *iter;
    }

  if (m_done)
    {
      return m_data.back();
    }

  TessellatedPathRef ref;
  float current_max_distance;

  ref = m_data.back();
  current_max_distance = ref->max_distance();

  while(!m_done && ref->max_distance() > max_distance)
    {
      current_max_distance *= 0.5f;
      while(!m_done && ref->max_distance() > current_max_distance)
        {
          float last_tess;

          last_tess = ref->max_distance();
          m_refiner->refine_tessellation(current_max_distance, 1);
          ref = m_refiner->tessellated_path();

          if (last_tess > ref->max_distance())
            {
              m_data.push_back(ref);

              /*
              std::cout << "Allow arcs = " << m_allow_arcs
                        << "(max_segs = "  << ref->max_segments() << ", tess_factor = "
                        << ref->max_distance()
                        << "), aiming for " << current_max_distance << "\n";
              */
            }
          else
            {
              m_done = true;
              m_refiner = nullptr;

              /**/
              std::cout << "Tapped out on allow arcs = " << m_allow_arcs
                        << "(max_segs = "  << ref->max_segments() << ", tess_factor = "
                        << ref->max_distance()
                        << "), aiming for " << current_max_distance << "\n";
              /**/
            }
        }
    }

  return m_data.back();
}

/////////////////////////////////
// PathPrivate methods
PathPrivate::
PathPrivate(fastuidraw::Path *p):
  m_tess_list(false),
  m_arc_tess_list(true),
  m_start_check_bb(0),
  m_is_flat(true),
  m_p(p)
{
}

PathPrivate::
PathPrivate(fastuidraw::Path *p, const PathPrivate &obj):
  m_contours(obj.m_contours),
  m_tess_list(obj.m_tess_list),
  m_arc_tess_list(obj.m_arc_tess_list),
  m_start_check_bb(obj.m_start_check_bb),
  m_bb(obj.m_bb),
  m_is_flat(obj.m_is_flat),
  m_p(p)
{
  /* if the last contour is not ended, we need to do a
   * deep copy on it.
   */
  if (!m_contours.empty() && !m_contours.back()->ended())
    {
      m_contours.back() = m_contours.back()->deep_copy();
      m_is_flat = m_is_flat && m_contours.back()->is_flat();
    }
}


void
PathPrivate::
close_back_contour(void)
{
  if (!m_contours.empty() && !m_contours.back()->ended())
    {
      m_contours.back()->end();
    }
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
  m_arc_tess_list.clear();
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
  if (!pcontour || !pcontour->ended())
    {
      return *this;
    }

  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  reference_counted_ptr<PathContour> contour;
  contour = pcontour.const_cast_ptr<PathContour>();
  d->m_is_flat = d->m_is_flat && contour->is_flat();

  d->clear_tesses();
  if (d->m_contours.empty() || d->m_contours.back()->ended())
    {
      d->m_contours.push_back(contour);
    }
  else
    {
      reference_counted_ptr<PathContour> r;
      r = d->m_contours.back();
      d->m_contours.back() = contour;
      d->m_contours.push_back(r);
    }

  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
add_contours(const Path &path)
{
  PathPrivate *d, *pd;
  d = static_cast<PathPrivate*>(m_d);
  pd = static_cast<PathPrivate*>(path.m_d);

  if (d != pd && !pd->m_contours.empty())
    {
      d->clear_tesses();
      d->m_contours.reserve(d->m_contours.size() + pd->m_contours.size());
      d->m_is_flat = d->m_is_flat && pd->m_is_flat;

      reference_counted_ptr<PathContour> r;
      if (!d->m_contours.empty() && !d->m_contours.back()->ended())
        {
          r = d->m_contours.back();
          d->m_contours.pop_back();
        }

      unsigned int endi(pd->m_contours.size());
      FASTUIDRAWassert(endi > 0u);

      if (!pd->m_contours.back()->ended())
        {
          --endi;
        }

      for(unsigned int i = 0; i < endi; ++i)
        {
          d->m_contours.push_back(pd->m_contours[i]);
        }

      if (r)
        {
          d->m_contours.push_back(r);
        }
    }
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
move(const fastuidraw::vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  if (!d->m_contours.empty())
    {
      const reference_counted_ptr<PathContour> &h(d->current_contour());
      if (!h->ended())
        {
          h->end();
        }
    }
  d->move_common(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(const fastuidraw::vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  if (d->m_contours.empty() || d->m_contours.back()->ended())
    {
      d->move_common(pt);
    }
  else
    {
      d->current_contour()->to_point(pt);
    }
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
  d->close_back_contour();
  return d->m_tess_list.tessellation(*this, max_distance);
}

const fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath>&
fastuidraw::Path::
arc_tessellation(void) const
{
  return arc_tessellation(-1.0f);
}

const fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath>&
fastuidraw::Path::
arc_tessellation(float max_distance) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  d->close_back_contour();
  return d->m_arc_tess_list.tessellation(*this, max_distance);
}

bool
fastuidraw::Path::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  for(unsigned endi = d->m_contours.size();
      d->m_start_check_bb < endi && d->m_contours[d->m_start_check_bb]->ended();
      ++d->m_start_check_bb)
    {
      vec2 p0, p1;
      bool value_valid;

      value_valid = d->m_contours[d->m_start_check_bb]->approximate_bounding_box(&p0, &p1);
      FASTUIDRAWassert(value_valid);
      FASTUIDRAWunused(value_valid);

      d->m_bb.union_point(p0);
      d->m_bb.union_point(p1);
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
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->add_control_point(pt.m_location);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(const arc &a)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->to_arc(a.m_angle, a.m_pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(contour_end)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->end();
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(contour_end_arc a)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->end_arc(a.m_angle);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
line_to(const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->to_point(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
quadratic_to(const vec2 &ct, const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->add_control_point(ct);
  h->to_point(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
cubic_to(const vec2 &ct1, const vec2 &ct2, const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->add_control_point(ct1);
  h->add_control_point(ct2);
  h->to_point(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
arc_to(float angle, const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->to_arc(angle, pt);
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
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->to_generic(p);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
arc_move(float angle, const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->end_arc(angle);
  d->move_common(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
end_contour_arc(float angle)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->end_arc(angle);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
quadratic_move(const vec2 &ct, const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->add_control_point(ct);
  h->end();
  d->move_common(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
end_contour_quadratic(const vec2 &ct)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->add_control_point(ct);
  h->end();
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
cubic_move(const vec2 &ct1, const vec2 &ct2, const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->add_control_point(ct1);
  h->add_control_point(ct2);
  h->end();
  d->move_common(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
end_contour_cubic(const vec2 &ct1, const vec2 &ct2)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->add_control_point(ct1);
  h->add_control_point(ct2);
  h->end();
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
custom_move(const reference_counted_ptr<const PathContour::interpolator_base> &p, const vec2 &pt)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->end_generic(p);
  d->move_common(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
end_contour_custom(const reference_counted_ptr<const PathContour::interpolator_base> &p)
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  FASTUIDRAWassert(!d->current_contour()->ended());
  d->current_contour()->end_generic(p);
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
