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
    fastuidraw::vec2 a_p, b_a;
    float d, d_sq, b_a_mag_sq, a_p_mag_sq;

    a_p = a - p;
    b_a = b - a;
    d = fastuidraw::dot(a_p, b_a);
    d_sq = d * d;
    a_p_mag_sq = a_p.magnitudeSq();
    b_a_mag_sq = b_a.magnitudeSq();
    return fastuidraw::t_sqrt(fastuidraw::t_max(0.0f, a_p_mag_sq - d_sq / b_a_mag_sq));
  }

  class Tessellator:fastuidraw::noncopyable
  {
  public:
    Tessellator(const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                const fastuidraw::PathContour::interpolator_generic *h):
      m_h(h),
      m_thresh(tess_params),
      m_max_recursion(fastuidraw::uint32_log2(tess_params.m_max_segments)),
      m_max_size(tess_params.m_max_segments + 1)
    {
    }

    unsigned int
    dump(fastuidraw::c_array<fastuidraw::TessellatedPath::point> out_data,
         float *out_threshhold);

  private:
    void
    tessellation_worker(unsigned int recurse_level,
                        fastuidraw::PathContour::interpolator_generic::tessellated_region *in_src,
                        float *out_threshhold);

    unsigned int
    fill_data(fastuidraw::c_array<fastuidraw::TessellatedPath::point> out_data,
              float *out_threshhold);

    void
    add_point(const fastuidraw::vec2 &pt)
    {
      fastuidraw::TessellatedPath::point P;
      P.m_p = pt;
      m_data.push_back(P);
    }

    const fastuidraw::PathContour::interpolator_generic *m_h;
    fastuidraw::TessellatedPath::TessellationParams m_thresh;
    unsigned int m_max_recursion, m_max_size;
    std::vector<fastuidraw::TessellatedPath::point> m_data;
  };

  class ArcTessellator
  {
  public:
    ArcTessellator(const fastuidraw::ArcTessellatedPath::TessellationParams &tess_params,
                   const fastuidraw::PathContour::interpolator_generic *h):
      m_h(h),
      m_thresh_sq(tess_params.m_threshhold * tess_params.m_threshhold),
      m_max_recursion(fastuidraw::uint32_log2(tess_params.m_max_segments)),
      m_max_size(tess_params.m_max_segments + 1)
    {
    }

    unsigned int
    dump(fastuidraw::c_array<fastuidraw::ArcTessellatedPath::segment> out_data,
         float *out_threshhold);

  private:
    void
    tessellation_worker(unsigned int recurse_level,
                        fastuidraw::PathContour::interpolator_generic::tessellated_region *in_L,
                        fastuidraw::PathContour::interpolator_generic::tessellated_region *in_R,
                        fastuidraw::vec2 start, fastuidraw::vec2 mid, fastuidraw::vec2 end,
                        float *out_threshhold_sq);

    unsigned int
    fill_data(fastuidraw::c_array<fastuidraw::ArcTessellatedPath::segment> out_data,
              float *out_threshhold);

    const fastuidraw::PathContour::interpolator_generic *m_h;
    float m_thresh_sq;
    unsigned int m_max_recursion, m_max_size;
    std::vector<fastuidraw::ArcTessellatedPath::segment> m_data;
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
       *  of a BezierTessRegion and the line segment between
       *  the start and  end point of the BezierTessRegion.
       *  The curve is contained within the convex hull of the
       *  points, so this computation is fast, conservative
       *  value for getting the curve_distance.
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

    fastuidraw::vec2 m_min_bb, m_max_bb;
    BezierTessRegion m_start_region;
    std::vector<fastuidraw::vec2> m_pts;
    fastuidraw::vecN<std::vector<fastuidraw::vec2>, 2> m_work_room;
  };

  class ArcPrivate
  {
  public:
    float m_radius, m_angle_speed;
    float m_start_angle;
    fastuidraw::vec2 m_center;
    fastuidraw::vec2 m_min_bb, m_max_bb;

    void
    compute_bb(void)
    {
      using namespace fastuidraw;

      vec2 p0, p1, z;
      float d;

      p0 = vec2(t_cos(m_start_angle),
                t_sin(m_start_angle));

      p1 = vec2(t_cos(m_start_angle + m_angle_speed),
                t_sin(m_start_angle + m_angle_speed));

      d = 1.0f - t_cos(m_angle_speed * 0.5f);

      /*
       * bb represents the bounding box of an arc
       * [m_start_angle, m_start_angle + m_angle_speed)
       * with radius one centered at the origin
       */
      BoundingBox<float> bb;
      bb.union_point(p0);
      bb.union_point(p1);
      bb.union_point(p0 + d * z);
      bb.union_point(p1 + d * z);

      m_min_bb = m_center + m_radius * bb.min_point();
      m_max_bb = m_center + m_radius * bb.max_point();
    }
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

    fastuidraw::vec2 m_min_bb, m_max_bb;
    bool m_is_flat;
  };

  class PathPrivate;

  class TessellatedPathList
  {
  public:
    typedef fastuidraw::TessellatedPath TessellatedPath;
    typedef fastuidraw::reference_counted_ptr<const TessellatedPath> tessellated_path_ref;

    explicit
    TessellatedPathList(void):
      m_done(false)
    {}

    const tessellated_path_ref&
    tessellation(const fastuidraw::Path &path, float thresh);

    void
    clear(void)
    {
      m_data.clear();
      m_done = false;
    }

  private:
    bool m_done;
    std::vector<tessellated_path_ref> m_data;
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

    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PathContour> > m_contours;
    TessellatedPathList m_tess_list;

    /* m_start_check_bb gives the index into m_contours that
     *  have not had their bounding box absorbed into
     *  m_max_bb and m_min_bb.
     */
    unsigned int m_start_check_bb;
    fastuidraw::vec2 m_max_bb, m_min_bb;
    bool m_is_flat;
    fastuidraw::Path *m_p;
  };

  class reverse_compare_thresh
  {
  public:
    bool
    operator()(const TessellatedPathList::tessellated_path_ref &lhs, float rhs) const
    {
      return lhs->effective_threshhold() > rhs;
    }

    bool
    operator()(const TessellatedPathList::tessellated_path_ref &lhs,
               const TessellatedPathList::tessellated_path_ref &rhs) const
    {
      return lhs->effective_threshhold() > rhs->effective_threshhold();
    }
  };
}

/////////////////////////////////
// Tessellator methods
unsigned int
Tessellator::
dump(fastuidraw::c_array<fastuidraw::TessellatedPath::point> out_data,
     float *out_threshhold)
{
  unsigned int return_value;

  *out_threshhold = -1.0f;
  return_value = fill_data(out_data, out_threshhold);
  out_data = out_data.sub_array(0, return_value);

  /* enforce start and end point values */
  out_data.front().m_p = m_h->start_pt();
  out_data.back().m_p = m_h->end_pt();

  return return_value;
}

unsigned int
Tessellator::
fill_data(fastuidraw::c_array<fastuidraw::TessellatedPath::point> out_data,
          float *out_threshhold)
{
  /*
   * tessellate: note that we add the start point, then tessellate
   * and then at the end add the end point. By doing so, the points
   * are added to m_data in order of time automatically.
   */
  add_point(m_h->start_pt());
  tessellation_worker(0, nullptr, out_threshhold);
  add_point(m_h->end_pt());

  FASTUIDRAWassert(m_data.size() <= out_data.size());
  std::copy(m_data.begin(), m_data.end(), out_data.begin());
  return m_data.size();
}

void
Tessellator::
tessellation_worker(unsigned int recurse_level,
                    fastuidraw::PathContour::interpolator_generic::tessellated_region *in_src,
                    float *out_threshhold)
{
  using namespace fastuidraw;

  PathContour::interpolator_generic::tessellated_region *rgnA(nullptr);
  PathContour::interpolator_generic::tessellated_region *rgnB(nullptr);
  vec2 p;

  m_h->tessellate(in_src, &rgnA, &rgnB, &p, out_threshhold);

  if (recurse_level + 1u < m_max_recursion
      && *out_threshhold > m_thresh.m_threshhold)
    {
      float tmpA(-1.0f), tmpB(-1.0f);

      /* NOTE the order of recursing and adding to m_data:
       *  - first we recurse into the left side
       *  - second we add the mid-point
       *  - last we recurse into the right side
       * By doing so, we keep the order in time of the points,
       * since all points on the left side come before the
       * mid point, and all points in the right side come
       * after the midpoint.
       */
      tessellation_worker(recurse_level + 1, rgnA, &tmpA);
      add_point(p);
      tessellation_worker(recurse_level + 1, rgnB, &tmpB);

      *out_threshhold = t_max(tmpA, tmpB);
    }
  else
    {
      add_point(p);
    }


  if (rgnA)
    {
      FASTUIDRAWdelete(rgnA);
    }

  if (rgnB)
    {
      FASTUIDRAWdelete(rgnB);
    }
}

///////////////////////////////////////
// ArcTessellator methods
unsigned int
ArcTessellator::
dump(fastuidraw::c_array<fastuidraw::ArcTessellatedPath::segment> out_data,
     float *out_threshhold)
{
  using namespace fastuidraw;

  PathContour::interpolator_generic::tessellated_region *L, *R;
  vec2 mid;
  float thresh_sq(0.0f);

  m_h->tessellate(nullptr, &L, &R, &mid, nullptr);
  tessellation_worker(1, L, R, m_h->start_pt(), mid, m_h->end_pt(), &thresh_sq);
  *out_threshhold = t_sqrt(thresh_sq);

  FASTUIDRAWassert(m_data.size() <= out_data.size());
  std::copy(m_data.begin(), m_data.end(), out_data.begin());
  return m_data.size();
}

void
ArcTessellator::
tessellation_worker(unsigned int recurse_level,
                    fastuidraw::PathContour::interpolator_generic::tessellated_region *inL,
                    fastuidraw::PathContour::interpolator_generic::tessellated_region *inR,
                    fastuidraw::vec2 start, fastuidraw::vec2 mid, fastuidraw::vec2 end,
                    float *out_threshhold_sq)
{
  using namespace fastuidraw;
  PathContour::interpolator_generic::tessellated_region *L0(nullptr), *L1(nullptr);
  PathContour::interpolator_generic::tessellated_region *R0(nullptr), *R1(nullptr);
  vec2 midL, midR;
  static float tol(0.00001f);

  m_h->tessellate(inL, &L0, &L1, &midL, nullptr);
  m_h->tessellate(inR, &R0, &R1, &midR, nullptr);

  /* compute the circle going through start, mid, end */
  vec2 c, v0, v1, n0, n1, p0, p1;
  float s, det;

  p0 = 0.5f * (start + mid);
  p1 = 0.5f * (mid + end);

  v0 = start - mid;
  n0 = vec2(-v0.y(), v0.x());

  v1 = mid - start;
  n1 = vec2(-v1.y(), v1.x());

  det = n1.y() * n0.x() - n0.y() * n1.x();
  if (t_abs(det) < tol)
    {
      /* practically flat anyways, just add a line segment */
      ArcTessellatedPath::segment S;

      S.m_type = ArcTessellatedPath::line_segment;
      S.m_p = start;
      S.m_data = end;
      S.m_radius = 0.0f;
      m_data.push_back(S);

      /* TODO: compute the distance of mid, midL and midR from
       * the segment and use that as the output threshhold
       */
      *out_threshhold_sq = 0.0f;
      return;
    }

  s = dot(v1, p1 - p0) / det;
  c = p0 + s * n0;

  /* we are done if both midL and midR are close enough to C */
  float threshLsq, threshRsq;

  threshLsq = (c - midL).magnitudeSq();
  threshRsq = (c - midR).magnitudeSq();
  *out_threshhold_sq = t_max(threshLsq, threshRsq);

  if (recurse_level + 1u < m_max_recursion
      && *out_threshhold_sq > m_thresh_sq)
    {
      tessellation_worker(recurse_level + 1u, L0, L1,
                          start, midL, mid, &threshLsq);
      tessellation_worker(recurse_level + 1u, R0, R1,
                          mid, midR, end, &threshRsq);
      *out_threshhold_sq = t_max(threshLsq, threshRsq);
    }
  else
    {
      ArcTessellatedPath::segment S;

      S.m_type = ArcTessellatedPath::arc_segment;
      S.m_p = c;
      S.m_radius = (mid - c).magnitude();

      S.m_data.x() = std::atan2(start.y() - c.y(), start.x() - c.x());
      S.m_data.y() = std::atan2(end.y() - c.y(), end.x() - c.x());
      m_data.push_back(S);
    }
}

////////////////////////////////////////
// BezierPrivate methods
void
BezierPrivate::
init(void)
{
  /* TODO: silly to recompute the binomial coefficients
   *  every time a bezier object is created since creation
   *  of it is O(n^2) where as once it is made, the creation
   *  of bezier is O(n)
   */
  FASTUIDRAWassert(!m_pts.empty());

  m_min_bb = m_max_bb = m_pts[0];
  for(unsigned int i = 1, endi = m_pts.size(); i < endi; ++i)
    {
      m_min_bb.x() = fastuidraw::t_min(m_min_bb.x(), m_pts[i].x());
      m_min_bb.y() = fastuidraw::t_min(m_min_bb.y(), m_pts[i].y());

      m_max_bb.x() = fastuidraw::t_max(m_max_bb.x(), m_pts[i].x());
      m_max_bb.y() = fastuidraw::t_max(m_max_bb.y(), m_pts[i].y());
    }
  m_start_region.m_pts = m_pts; //original region uses original points.

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
unsigned int
fastuidraw::PathContour::interpolator_generic::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     c_array<TessellatedPath::point> out_data,
                     float *out_threshhold) const
{
  Tessellator tesser(tess_params, this);
  return tesser.dump(out_data, out_threshhold);
}

unsigned int
fastuidraw::PathContour::interpolator_generic::
produce_tessellation(const ArcTessellatedPath::TessellationParams &tess_params,
                     c_array<ArcTessellatedPath::segment> out_data,
                     float *out_threshhold) const
{
  ArcTessellator tesser(tess_params, this);
  return tesser.dump(out_data, out_threshhold);
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
  *out_min_bb = d->m_min_bb;
  *out_max_bb = d->m_max_bb;
}

void
fastuidraw::PathContour::bezier::
tessellate(tessellated_region *in_region,
           tessellated_region **out_regionA, tessellated_region **out_regionB,
           vec2 *out_p, float *out_threshhold) const
{
  BezierPrivate *d;
  d = static_cast<BezierPrivate*>(m_d);

  if (in_region == nullptr)
    {
      in_region = &d->m_start_region;
    }

  BezierTessRegion *in_region_casted;
  FASTUIDRAWassert(dynamic_cast<BezierTessRegion*>(in_region) != nullptr);
  in_region_casted = static_cast<BezierTessRegion*>(in_region);

  BezierTessRegion *newA, *newB;
  newA = FASTUIDRAWnew BezierTessRegion(in_region_casted, true);
  newB = FASTUIDRAWnew BezierTessRegion(in_region_casted, false);

  c_array<vec2> dst, src;
  src = make_c_array(in_region_casted->m_pts);

  newA->m_pts.push_back(src.front());
  newB->m_pts.push_back(src.back());

  /* For a Bezier curve, given by points p(0), .., p(n),
   *  and a time 0 <= t <= 1, De Casteljau's algorithm is
   *  the following.
   *
   *  Let
   *    q(0, j) = p(j) for 0 <= j <= n,
   *    q(i + 1, j) = (1 - t) * q(i, j) + t * q(i, j + 1) for 0 <= i <= n, 0 <= j <= n - i
   *  then
   *    The curve split at time t is given by
   *      A = { q(0, 0), q(1, 0), q(2, 0), ... , q(n, 0) }
   *      B = { q(n, 0), q(n - 1, 1), q(n - 2, 2), ... , q(0, n) }
   *    and
   *      the curve evaluated at t is given by q(n, 0).
   *  We use t = 0.5 because we are always doing mid-point cutting.
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

  if (out_threshhold)
    {
      *out_threshhold = t_max(newA->compute_curve_distance(), newB->compute_curve_distance());
    }
}

fastuidraw::PathContour::interpolator_base*
fastuidraw::PathContour::bezier::
deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const
{
  return FASTUIDRAWnew bezier(*this, prev);
}

//////////////////////////////////////
// fastuidraw::PathContour::flat methods
bool
fastuidraw::PathContour::flat::
is_flat(void) const
{
  return true;
}

unsigned int
fastuidraw::PathContour::flat::
produce_tessellation(const TessellatedPath::TessellationParams&,
                     c_array<TessellatedPath::point> out_data,
                     float *out_threshhold) const
{
  vec2 delta(end_pt() - start_pt());
  float mag(delta.magnitude());

  out_data[0].m_p = start_pt();
  out_data[0].m_distance_from_edge_start = 0.0f;

  out_data[1].m_p = end_pt();
  out_data[1].m_distance_from_edge_start = mag;

  *out_threshhold = 0.0f;

  return 2;
}

unsigned int
fastuidraw::PathContour::flat::
produce_tessellation(const ArcTessellatedPath::TessellationParams &tess_params,
                     c_array<ArcTessellatedPath::segment> out_data,
                     float *out_threshhold) const
{
  FASTUIDRAWunused(tess_params);
  out_data[0].m_type = ArcTessellatedPath::line_segment;
  out_data[0].m_p = start_pt();
  out_data[0].m_data = end_pt();
  out_data[0].m_radius = 0.0f;

  *out_threshhold = 0.0f;
  return 1;
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
   *  on the perpindicular bisecter of start and end.
   *  The perpindicular bisector is given by
   *  { t*n + mid | t real }
   */
  angle = fastuidraw::t_abs(angle);
  end_start = end_pt() - start_pt();
  mid = (end_pt() + start_pt()) * 0.5f;
  n = vec2(-end_start.y(), end_start.x());
  s = std::sin(angle * 0.5f);
  c = std::cos(angle * 0.5f);

  /* Let t be the point so that m_center = t*n + mid
   *  Then
   *    tan(angle/2) = 0.5 * ||end - start|| / || m_center - mid ||
   *                 = 0.5 * ||end - start|| / || t * n ||
   *                 = 0.5 * || n || / || t * n||
   *  thus
   *    |t| = 0.5/tan(angle/2) = 0.5 * c / s
   */
  t = angle_coeff_dir * 0.5f * c / s;
  d->m_center = mid + (t * n);

  vec2 start_center(start_pt() - d->m_center);

  d->m_radius = start_center.magnitude();
  d->m_start_angle = std::atan2(start_center.y(), start_center.x());
  d->m_angle_speed = angle_coeff_dir * angle;

  d->compute_bb();
}

fastuidraw::PathContour::arc::
arc(const reference_counted_ptr<const interpolator_base> &start,
    const vec2 &center, const vec2 &end):
  fastuidraw::PathContour::interpolator_base(start, end)
{
  ArcPrivate *d;
  d = FASTUIDRAWnew ArcPrivate();
  m_d = d;

  vec2 start_center(start_pt() - center);
  vec2 end_center(end - center);
  float end_angle;

  end_angle = std::atan2(end_center.y(), end_center.x());

  d->m_center = center;
  d->m_radius = start_center.magnitude();
  d->m_start_angle = std::atan2(start_center.y(), start_center.x());
  d->m_angle_speed = end_angle - d->m_start_angle;

  d->compute_bb();
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

unsigned int
fastuidraw::PathContour::arc::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     c_array<TessellatedPath::point> out_data,
                     float *out_threshhold) const
{
  ArcPrivate *d;
  d = static_cast<ArcPrivate*>(m_d);
  unsigned int return_value;
  float s, c, a, da;
  unsigned int needed_size;
  float delta_angle;

  needed_size = detail::number_segments_for_tessellation(d->m_radius, t_abs(d->m_angle_speed), tess_params);
  delta_angle = d->m_angle_speed / static_cast<float>(needed_size);

  a = d->m_start_angle;
  da = 0.0f;
  for(unsigned int i = 0; i <= needed_size; ++i, a += delta_angle, da += delta_angle)
    {
      s = d->m_radius * std::sin(a);
      c = d->m_radius * std::cos(a);
      out_data[i].m_p = d->m_center + vec2(c, s);
      out_data[i].m_distance_from_edge_start = t_abs(da) * d->m_radius;
    }
  out_data[0].m_p = start_pt();
  out_data[needed_size].m_p = end_pt();

  *out_threshhold = d->m_radius * (1.0f - t_cos(delta_angle * 0.5f));

  return_value = needed_size + 1;
  return return_value;
}

unsigned int
fastuidraw::PathContour::arc::
produce_tessellation(const ArcTessellatedPath::TessellationParams &tess_params,
                     c_array<ArcTessellatedPath::segment> out_data,
                     float *out_threshhold) const
{
  ArcPrivate *d;
  d = static_cast<ArcPrivate*>(m_d);

  FASTUIDRAWunused(tess_params);
  out_data[0].m_type = ArcTessellatedPath::arc_segment;
  out_data[0].m_p = d->m_center;
  out_data[0].m_data.x() = d->m_start_angle;
  out_data[0].m_data.y() = d->m_start_angle + d->m_angle_speed;
  out_data[0].m_radius = d->m_radius;

  *out_threshhold = 0.0f;
  return 1;
}

void
fastuidraw::PathContour::arc::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  ArcPrivate *d;
  d = static_cast<ArcPrivate*>(m_d);
  *out_min_bb = d->m_min_bb;
  *out_max_bb = d->m_max_bb;
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
   *  it to provide a "previous" for the first interpolator added.
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
       *  by p; to avoid needing to handle the corner cases of
       *  having just one interpolator we will add an additional
       *  interpolator -after- p which starts and ends on the
       *  end point of p.
       */
      reference_counted_ptr<const interpolator_base> h;

      to_generic(p);
      h = FASTUIDRAWnew flat(p, p->end_pt());
      p = h;
    }

  /* hack-evil: we are going to replace m_interpolator[0]
   *  with p, we also need to change m_interpolator[1]->m_prev
   *  as well to p.
   */
  FASTUIDRAWassert(d->m_interpolators.size() > 1);

  InterpolatorBasePrivate *q;
  q = static_cast<InterpolatorBasePrivate*>(d->m_interpolators[1]->m_d);
  q->m_prev = p.get();

  d->m_interpolators[0] = p;
  d->m_end_to_start = p;
  d->m_is_flat = d->m_is_flat && p->is_flat();

  /* compute bounding box after ending the PathContour.
   */
  d->m_interpolators[0]->approximate_bounding_box(&d->m_min_bb, &d->m_max_bb);
  for(unsigned int i = 1, endi = d->m_interpolators.size(); i < endi; ++i)
    {
      vec2 p0, p1;
      d->m_interpolators[i]->approximate_bounding_box(&p0, &p1);

      d->m_min_bb.x() = fastuidraw::t_min(d->m_min_bb.x(), p0.x());
      d->m_min_bb.y() = fastuidraw::t_min(d->m_min_bb.y(), p0.y());

      d->m_max_bb.x() = fastuidraw::t_max(d->m_max_bb.x(), p1.x());
      d->m_max_bb.y() = fastuidraw::t_max(d->m_max_bb.y(), p1.y());
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
   *  connecting the last point added to the first
   *  point of the contour, i.e. is given by m_end_to_start
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

  *out_min_bb = d->m_min_bb;
  *out_max_bb = d->m_max_bb;

  return true;
}

/////////////////////////////////
// TessellatedPathList methods
const TessellatedPathList::tessellated_path_ref&
TessellatedPathList::
tessellation(const fastuidraw::Path &path, float thresh)
{
  using namespace fastuidraw;

  if (m_data.empty())
    {
      TessellatedPath::TessellationParams params;
      vec2 bb_min, bb_max, bb_size;

      /* always prefer to use distance threshholds over
       * curvature; use the size of the bounding box times
       * 1/500 as teh default tessellation factor.
       */
      if (path.approximate_bounding_box(&bb_min, &bb_max))
        {
          vec2 bb_size;
          float d;
          bb_size = bb_max - bb_min;
          d = t_max(bb_size.x(), bb_size.y());
          if (d > 0.0f)
            {
              params.threshhold(d / 500.0f);
            }
        }
      m_data.push_back(FASTUIDRAWnew TessellatedPath(path, params));
    }

  if (thresh <= 0.0 || path.is_flat())
    {
      return m_data.front();
    }

  if (m_data.back()->effective_threshhold() <= thresh)
    {
      std::vector<tessellated_path_ref>::const_iterator iter;
      iter = std::lower_bound(m_data.begin(),
                              m_data.end(),
                              thresh,
                              reverse_compare_thresh());

      FASTUIDRAWassert(iter != m_data.end());
      FASTUIDRAWassert(*iter);
      FASTUIDRAWassert((*iter)->effective_threshhold() <= thresh);
      return *iter;
    }

  if (m_done)
    {
      return m_data.back();
    }

  tessellated_path_ref prev_ref, ref;
  TessellatedPath::TessellationParams params;

  ref = m_data.back();
  params
    .max_segments(ref->max_segments())
    .threshhold(ref->effective_threshhold());

  while(!m_done && ref->effective_threshhold() > thresh)
    {
      params.m_threshhold *= 0.5f;
      while(!m_done
            && ref->effective_threshhold() > params.m_threshhold)
        {
          float last_tess;

          params.m_max_segments *= 2;
          last_tess = ref->effective_threshhold();
          ref = FASTUIDRAWnew TessellatedPath(path, params);
          if (last_tess > ref->effective_threshhold())
            {
              m_data.push_back(ref);
            }
          else
            {
              m_done = true;

              /*
               *std::cout << "Tapped out on type = (max_segs = "
               *         << ref->max_segments() << ", tess_factor = "
               *         << ref->effective_threshhold(m_type)
               *         << ", num_points = " << ref->point_data().size()
               *         << ")\n";
               */
            }
        }
    }

  return m_data.back();
}

/////////////////////////////////
// PathPrivate methods
PathPrivate::
PathPrivate(fastuidraw::Path *p):
  m_start_check_bb(0),
  m_is_flat(true),
  m_p(p)
{
}

PathPrivate::
PathPrivate(fastuidraw::Path *p, const PathPrivate &obj):
  m_contours(obj.m_contours),
  m_tess_list(obj.m_tess_list),
  m_start_check_bb(obj.m_start_check_bb),
  m_max_bb(obj.m_max_bb),
  m_min_bb(obj.m_min_bb),
  m_is_flat(obj.m_is_flat),
  m_p(p)
{
  /* if the last contour is not ended, we need to do a
   *  deep copy on it.
   */
  if (!m_contours.back()->ended())
    {
      m_contours.back() = m_contours.back()->deep_copy();
      m_is_flat = m_is_flat && m_contours.back()->is_flat();
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
tessellation(float thresh) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);
  return d->m_tess_list.tessellation(*this, thresh);
}

bool
fastuidraw::Path::
approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const
{
  PathPrivate *d;
  d = static_cast<PathPrivate*>(m_d);

  bool assigned_value(d->m_start_check_bb != 0u);
  for(unsigned endi = d->m_contours.size();
      d->m_start_check_bb < endi && d->m_contours[d->m_start_check_bb]->ended();
      ++d->m_start_check_bb)
    {
      vec2 p0, p1;
      bool value_valid;

      value_valid = d->m_contours[d->m_start_check_bb]->approximate_bounding_box(&p0, &p1);
      FASTUIDRAWassert(value_valid);
      FASTUIDRAWunused(value_valid);

      if (assigned_value)
        {
          d->m_min_bb.x() = fastuidraw::t_min(d->m_min_bb.x(), p0.x());
          d->m_min_bb.y() = fastuidraw::t_min(d->m_min_bb.y(), p0.y());

          d->m_max_bb.x() = fastuidraw::t_max(d->m_max_bb.x(), p1.x());
          d->m_max_bb.y() = fastuidraw::t_max(d->m_max_bb.y(), p1.y());
        }
      else
        {
          d->m_min_bb = p0;
          d->m_max_bb = p1;
          assigned_value = true;
        }
    }

  if (assigned_value)
    {
      *out_min_bb = d->m_min_bb;
      *out_max_bb = d->m_max_bb;
      return true;
    }
  else
    {
      return false;
    }
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
