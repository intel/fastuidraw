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
#include "private/util_private.hpp"

namespace
{
  class poly
  {
  public:

    static
    void
    compute_bernstein_derivative(const std::vector<fastuidraw::vec2> &input,
                                 std::vector<fastuidraw::vec2> &output);

    static
    fastuidraw::vec2
    compute_poly(float t, fastuidraw::const_c_array<fastuidraw::vec2> poly);
  };


  class binomial_coeff
  {
  public:
    explicit
    binomial_coeff(unsigned int N);

    void
    prepare_bernstein(std::vector<fastuidraw::vec2> &victim) const;

    float
    operator[](unsigned int k) const
    {
      return m_values.back()[k];
    }

  private:
    std::vector< std::vector<float> > m_values;
  };

  class analytic_point_data:public fastuidraw::TessellatedPath::point
  {
  public:
    analytic_point_data(float time, const fastuidraw::PathContour::interpolator_generic *h);

    bool
    operator<(const analytic_point_data &rhs) const
    {
      return m_time < rhs.m_time;
    }

    float m_time;
    float m_K_times_speed;
  };


  class Tessellator:fastuidraw::noncopyable
  {
  public:
    Tessellator(const fastuidraw::TessellatedPath::TessellationParams &tess_params,
                const fastuidraw::PathContour::interpolator_generic *h);

    unsigned int
    dump(fastuidraw::c_array<fastuidraw::TessellatedPath::point> out_data) const;


  private:
    unsigned int m_max_recursion, m_max_size;
    const fastuidraw::PathContour::interpolator_generic *m_h;
    float m_thresh_times_six;
    std::vector<analytic_point_data> m_data;

    void
    tessellation_worker(unsigned int idx_p, unsigned int idx_q,
                        unsigned int recursion_level);


    bool
    requires_recursion(float delta_t,
                       unsigned int idx_p,
                       unsigned int idx_mid,
                       unsigned int idx_q);
  };

  class InterpolatorBasePrivate
  {
  public:
    //note is weak pointer to prevent circular pain.
    const fastuidraw::PathContour::interpolator_base *m_prev;
    fastuidraw::vec2 m_end;
  };

  class BezierPrivate
  {
  public:
    void
    init(void);

    std::vector<fastuidraw::vec2> m_poly;
    std::vector<fastuidraw::vec2> m_poly_prime;
    std::vector<fastuidraw::vec2> m_poly_prime_prime;
  };

  class ArcPrivate
  {
  public:
    float m_radius, m_angle_speed;
    float m_start_angle;
    fastuidraw::vec2 m_center;
  };

  class PathContourPrivate
  {
  public:
    fastuidraw::vec2 m_start_pt;
    std::vector<fastuidraw::vec2> m_current_control_points;
    fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base> m_end_to_start;
    std::vector<fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base> > m_interpolators;
  };

  class PathPrivate
  {
  public:
    PathPrivate(const fastuidraw::TessellatedPath::TessellationParams &tess_params):
      m_tessellation_params(tess_params)
    {}

    PathPrivate(const PathPrivate &obj);

    const fastuidraw::reference_counted_ptr<fastuidraw::PathContour>&
    current_contour(void)
    {
      assert(!m_contours.empty());
      m_tessellation.clear();
      return m_contours.back();
    }

    void
    move_common(const fastuidraw::vec2 &pt)
    {
      m_tessellation.clear();
      m_contours.push_back(FASTUIDRAWnew fastuidraw::PathContour());
      m_contours.back()->start(pt);
    }

    fastuidraw::TessellatedPath::TessellationParams m_tessellation_params;
    fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath> m_tessellation;
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::PathContour> > m_contours;
  };
}





////////////////////////////////////
// poly methods
void
poly::
compute_bernstein_derivative(const std::vector<fastuidraw::vec2> &input,
                             std::vector<fastuidraw::vec2> &output)
{
  assert(output.empty());
  if(input.empty())
    {
      return;
    }

  /* the std::vector<vec2> p represents
       f(t) = sum(0 <= k <=n) { C(n,k) t^k t^(n-k) p[k] }
     where C(n,k) = n!/(k! (n-k)! ) and n = p.size() - 1 .
     Let
      g_k(t) = C(n,k) t^k (1-t)^(n-k) p[k]
     then
      g_k'(t) = C(n,k) k t^(k-1) (1-t)^(n-k) p[k] - C(n,k) (n-k) t^k t^(n-1-k)
              = n C(n-1, k-1) t^(k-1) (1-t)^(n-k) p[k] - n C(n-1, k) t^k t^(n-1-k) p[k]
     thus:
      f'(t) = sum(0 <= k <= n-1) { C(n-1,k) t^k t^(n-k) n ( p[k+1] - p[k] ) }
   */

  float degree;
  degree = static_cast<float>(input.size() - 1);

  output.resize(input.size() - 1);
  for(int k = 0, endk = output.size(); k < endk; ++k)
    {
      output[k] = degree * (input[k+1] - input[k]);
    }
}


fastuidraw::vec2
poly::
compute_poly(float t, fastuidraw::const_c_array<fastuidraw::vec2> poly)
{
  float s(1.0f-t);
  float st(s*t);
  float s2(s*s), t2(t*t);
  float tn, sn;
  unsigned int poly_size(poly.size());
  unsigned int l, r;
  fastuidraw::vec2 work;

  /* compute_poly is to evaluate
     v = sum(0 <= k <= n) t^k s^(n-k) poly[k]
       = t^n poly[0] + s^n poly[n] + sum(1 <= k <= n-1) t^k s^(n-k) poly[k]
       = t^n poly[0] + s^n poly[n] + st sum(1 <= k <= n-1) t^(k-1) s^(n-1-k) poly[k]

     the above gives us an iterative method, the below starts in the middle
     of the polynomial and grows out 1 term on each side.
   */

  if(poly_size == 0)
    {
      return fastuidraw::vec2(0.0f, 0.0f);
    }

  if(poly_size & 1u)
    {
      unsigned int m;
      m = (poly_size - 1) / 2;
      work = poly[m];
      l = m - 1;
      r = m + 1;
      sn = 1.0f;
      tn = 1.0f;
    }
  else
    {
      unsigned int m;
      m = poly_size / 2;

      work = s * poly[m-1] + t * poly[m];
      l = m - 2;
      r = m + 1;
      sn = s;
      tn = t;
    }

  while(r < poly_size)
    {
      assert(l < poly_size);
      sn *= s2;
      tn *= t2;
      work = (poly[r] * tn) + (poly[l] * sn) + (st * work);

      ++r;
      --l;
    }

  return work;
}


///////////////////////////////////////////
// binomial_coeff methods
binomial_coeff::
binomial_coeff(unsigned int N):
  m_values(N+1)
{
  m_values[0].resize(1, 1.0f);

  for(unsigned int n = 1; n <= N; ++n)
    {
      m_values[n].resize(n+1);
      m_values[n][0] = 1.0f;
      m_values[n][n] = 1.0f;
      for(unsigned int k = 1; k < n; ++k)
        {
          m_values[n][k] = m_values[n-1][k-1] + m_values[n-1][k];
        }
    }
}


void
binomial_coeff::
prepare_bernstein(std::vector<fastuidraw::vec2> &victim) const
{
  unsigned int degree;

  if(victim.empty())
    {
      return;
    }

  degree = victim.size() - 1;
  assert(degree < m_values.size());
  assert(degree == m_values[degree].size() - 1);

  for(unsigned int k = 0; k <= degree; ++k)
    {
      victim[k] *= m_values[degree][k];
    }
}

////////////////////////////////////
// analytic_point_data methods
analytic_point_data::
analytic_point_data(float time, const fastuidraw::PathContour::interpolator_generic *h)
{
  assert(h);

  fastuidraw::vec2 p_tt;
  float cross_mag, speed_sq;
  const float epsilon(0.000001f);
  const float epsilon_sq(epsilon * epsilon);

  /* curvature at a point on a curve is given by
     K = || p_t X p_tt || / ||p_t||^3
     thus
     K ||p_t || = || p_t x p_tt || / ||p_t||^2
  */
  m_time = time;
  h->compute(time, &m_p, &m_p_t, &p_tt);

  cross_mag = std::abs(m_p_t.x() * p_tt.y() - p_tt.x() * m_p_t.y());
  speed_sq = std::max(dot(m_p_t, m_p_t), epsilon_sq);
  m_K_times_speed= cross_mag / speed_sq;
}

/////////////////////////////////////
// Tessellator methods
Tessellator::
Tessellator(const fastuidraw::TessellatedPath::TessellationParams &tess_params,
            const fastuidraw::PathContour::interpolator_generic *h):
  m_max_recursion(fastuidraw::uint32_log2(tess_params.m_max_segments)),
  m_max_size(tess_params.m_max_segments + 1),
  m_h(h),
  m_thresh_times_six(tess_params.m_curve_tessellation * 6.0f)
{
  assert(m_h);

  m_data.push_back(analytic_point_data(0.0f, m_h));
  m_data.push_back(analytic_point_data(1.0f, m_h));
  tessellation_worker(0, 1, 0);
  std::sort(m_data.begin(), m_data.end());

  /*! enforce start and end point values
   */
  m_data.front().m_p = h->start_pt();
  m_data.back().m_p = h->end_pt();

  /* compute distance values along edge
   */
  m_data[0].m_distance_from_edge_start = 0.0f;
  for(unsigned int i = 1, endi = m_data.size(); i < endi; ++i)
    {
      fastuidraw::vec2 delta;

      delta = m_data[i].m_p - m_data[i-1].m_p;
      m_data[i].m_distance_from_edge_start = delta.magnitude()
        + m_data[i-1].m_distance_from_edge_start;
    }
}

inline
bool
Tessellator::
requires_recursion(float delta_t,
                   unsigned int idx_p,
                   unsigned int idx_mid,
                   unsigned int idx_q)

{
  /* Use simpson's Rule on the integral:
       integral_[t, t + delta_t] K_times_speed(t) dt
  */
  float K0, K1, K;

  K0 = m_data[idx_p].m_K_times_speed;
  K = m_data[idx_mid].m_K_times_speed;
  K1 = m_data[idx_q].m_K_times_speed;

  return (K0 + K1 + 4.0f * K ) * delta_t > m_thresh_times_six;
}

void
Tessellator::
tessellation_worker(unsigned int idx_start, unsigned int idx_end,
                    unsigned int recurse_level)
{

  if(recurse_level >= m_max_recursion)
    {
      return;
    }

  float delta_t, start_t, end_t, mid_t;
  unsigned int idx_mid(m_data.size());

  start_t = m_data[idx_start].m_time;
  end_t = m_data[idx_end].m_time;
  mid_t = 0.5f * (end_t + start_t);
  delta_t = (end_t - start_t);

  m_data.push_back(analytic_point_data(mid_t, m_h));


  if(requires_recursion(delta_t, idx_start, idx_mid, idx_end))
    {
      tessellation_worker(idx_start, idx_mid,
                          recurse_level + 1);

      tessellation_worker(idx_mid, idx_end,
                          recurse_level + 1);

    }

}


unsigned int
Tessellator::
dump(fastuidraw::c_array<fastuidraw::TessellatedPath::point> out_data) const
{
  assert(out_data.size() >= m_data.size());
  std::copy(m_data.begin(), m_data.end(), out_data.begin());
  return m_data.size();
}

////////////////////////////////////////
// BezierPrivate methods
void
BezierPrivate::
init(void)
{
  /* TODO: silly to recompute the binomial coefficients
     every time a bezier object is created since creation
     of it is O(n^2) where as once it is made, the creation
     of bezier is O(n)
   */
  unsigned int degree = m_poly.size() - 1;
  binomial_coeff BC(degree);

  poly::compute_bernstein_derivative(m_poly, m_poly_prime);
  poly::compute_bernstein_derivative(m_poly_prime, m_poly_prime_prime);

  BC.prepare_bernstein(m_poly);
  BC.prepare_bernstein(m_poly_prime);
  BC.prepare_bernstein(m_poly_prime_prime);
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
  d = reinterpret_cast<InterpolatorBasePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base>
fastuidraw::PathContour::interpolator_base::
prev_interpolator(void) const
{
  InterpolatorBasePrivate *d;
  d = reinterpret_cast<InterpolatorBasePrivate*>(m_d);
  return d->m_prev;
}

const fastuidraw::vec2&
fastuidraw::PathContour::interpolator_base::
start_pt(void) const
{
  InterpolatorBasePrivate *d;
  d = reinterpret_cast<InterpolatorBasePrivate*>(m_d);
  return (d->m_prev) ?  d->m_prev->end_pt() : d->m_end;
}

const fastuidraw::vec2&
fastuidraw::PathContour::interpolator_base::
end_pt(void) const
{
  InterpolatorBasePrivate *d;
  d = reinterpret_cast<InterpolatorBasePrivate*>(m_d);
  return d->m_end;
}

//////////////////////////////////////////////
// fastuidraw::PathContour::interpolator_generic methods
unsigned int
fastuidraw::PathContour::interpolator_generic::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     c_array<TessellatedPath::point> out_data) const
{
  Tessellator tesser(tess_params, this);
  return tesser.dump(out_data);
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
  d->m_poly.resize(3);
  d->m_poly[0] = start_pt();
  d->m_poly[1] = ct;
  d->m_poly[2] = end_pt();
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
  d->m_poly.resize(4);
  d->m_poly[0] = start_pt();
  d->m_poly[1] = ct1;
  d->m_poly[2] = ct2;
  d->m_poly[3] = end_pt();
  d->init();
}

fastuidraw::PathContour::bezier::
bezier(const reference_counted_ptr<const interpolator_base> &start,
       const_c_array<vec2> control_pts,
       const vec2 &end):
  interpolator_generic(start, end)
{
  BezierPrivate *d;
  d = FASTUIDRAWnew BezierPrivate();
  m_d = d;
  d->m_poly.resize(control_pts.size() + 2);
  std::copy(control_pts.begin(), control_pts.end(), d->m_poly.begin() + 1);
  d->m_poly.front() = start_pt();
  d->m_poly.back() = end;
  d->init();
}

fastuidraw::PathContour::bezier::
bezier(const bezier &q, const reference_counted_ptr<const interpolator_base> &prev):
  fastuidraw::PathContour::interpolator_generic(prev, q.end_pt())
{
  BezierPrivate *qd;
  qd = reinterpret_cast<BezierPrivate*>(q.m_d);
  m_d = FASTUIDRAWnew BezierPrivate(*qd);
}

fastuidraw::PathContour::bezier::
~bezier()
{
  BezierPrivate *d;
  d = reinterpret_cast<BezierPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}


void
fastuidraw::PathContour::bezier::
compute(float t, vec2 *outp, vec2 *outp_t, vec2 *outp_tt) const
{
  BezierPrivate *d;
  d = reinterpret_cast<BezierPrivate*>(m_d);
  *outp = poly::compute_poly(t, make_c_array(d->m_poly));
  *outp_t = poly::compute_poly(t, make_c_array(d->m_poly_prime));
  *outp_tt = poly::compute_poly(t, make_c_array(d->m_poly_prime_prime));
}

fastuidraw::PathContour::interpolator_base*
fastuidraw::PathContour::bezier::
deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const
{
  return FASTUIDRAWnew bezier(*this, prev);
}

//////////////////////////////////////
// fastuidraw::PathContour::flat methods
unsigned int
fastuidraw::PathContour::flat::
produce_tessellation(const TessellatedPath::TessellationParams&,
                     c_array<TessellatedPath::point> out_data) const
{
  vec2 delta(end_pt() - start_pt());
  float mag(delta.magnitude());

  out_data[0].m_p = start_pt();
  out_data[0].m_p_t = delta;
  out_data[0].m_distance_from_edge_start = 0.0f;

  out_data[1].m_p = end_pt();
  out_data[1].m_p_t = delta;
  out_data[1].m_distance_from_edge_start = mag;
  return 2;
}

fastuidraw::PathContour::interpolator_base*
fastuidraw::PathContour::flat::
deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const
{
  return FASTUIDRAWnew flat(prev, end_pt());
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
     on the perpindicular bisecter of start and end.
     The perpindicular bisector is given by
     { t*n + mid | t real }
   */
  angle = std::abs(angle);
  end_start = end_pt() - start_pt();
  mid = (end_pt() + start_pt()) * 0.5f;
  n = vec2(-end_start.y(), end_start.x());
  s = std::sin(angle * 0.5f);
  c = std::cos(angle * 0.5f);

  /* Let t be the point so that m_center = t*n + mid
     Then
       tan(angle/2) = 0.5 * ||end - start|| / || m_center - mid ||
                    = 0.5 * ||end - start|| / || t * n ||
                    = 0.5 * || n || / || t * n||
     thus
       |t| = 0.5/tan(angle/2) = 0.5 * c / s
   */
  t = angle_coeff_dir * 0.5f * c / s;
  d->m_center = mid + (t * n);

  vec2 start_center(start_pt() - d->m_center);

  d->m_radius = start_center.magnitude();
  d->m_start_angle = std::atan2(start_center.y(), start_center.x());
  d->m_angle_speed = angle_coeff_dir * angle;
}

fastuidraw::PathContour::arc::
arc(const arc &q, const reference_counted_ptr<const interpolator_base> &prev):
  fastuidraw::PathContour::interpolator_base(prev, q.end_pt())
{
  ArcPrivate *qd;
  qd = reinterpret_cast<ArcPrivate*>(q.m_d);
  m_d = FASTUIDRAWnew ArcPrivate(*qd);
}

fastuidraw::PathContour::arc::
~arc()
{
  ArcPrivate *d;
  d = reinterpret_cast<ArcPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

unsigned int
fastuidraw::PathContour::arc::
produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                     c_array<TessellatedPath::point> out_data) const
{
  ArcPrivate *d;
  d = reinterpret_cast<ArcPrivate*>(m_d);

  float s, c, a, da;
  unsigned int needed_size;
  float needed_sizef, delta_angle, sgn, sgn_radius;

  needed_sizef = std::abs(d->m_angle_speed) / tess_params.m_curve_tessellation;
  needed_size = static_cast<unsigned int>(needed_sizef);
  needed_size = std::min(needed_size, tess_params.m_max_segments);
  delta_angle = d->m_angle_speed / static_cast<float>(needed_size);
  sgn = d->m_angle_speed > 0.0 ? 1.0 : -1.0;
  sgn_radius = sgn * d->m_radius;

  a = d->m_start_angle;
  da = 0.0f;
  for(unsigned int i = 0; i <= needed_size; ++i, a += delta_angle, da += delta_angle)
    {
      s = d->m_radius * std::sin(a);
      c = d->m_radius * std::cos(a);
      out_data[i].m_p = d->m_center + vec2(c, s);
      out_data[i].m_p_t = sgn_radius * vec2(-s, c);
      out_data[i].m_distance_from_edge_start = da * d->m_radius;
    }
  out_data[0].m_p = start_pt();
  out_data[needed_size].m_p = end_pt();
  return needed_size + 1;
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
  d = reinterpret_cast<PathContourPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::PathContour::
start(const vec2 &start_pt)
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  assert(d->m_interpolators.empty());
  assert(!d->m_end_to_start);

  d->m_start_pt = start_pt;

  /* m_interpolators[0] is an "empty" interpolator whose only purpose
     it to provide a "previous" for the first interpolator added.
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
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  assert(!d->m_end_to_start);
  d->m_current_control_points.push_back(pt);
}

void
fastuidraw::PathContour::
to_point(const fastuidraw::vec2 &pt)
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;

  if(d->m_current_control_points.empty())
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
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  assert(!d->m_interpolators.empty());
  assert(d->m_current_control_points.empty());
  assert(!d->m_end_to_start);
  assert(p->prev_interpolator() == prev_interpolator());

  d->m_interpolators.push_back(p);
}

void
fastuidraw::PathContour::
end_generic(reference_counted_ptr<const interpolator_base> p)
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  assert(!d->m_end_to_start);
  assert(d->m_current_control_points.empty());
  assert(!d->m_interpolators.empty());
  assert(p->prev_interpolator() == prev_interpolator());

  if(d->m_interpolators.size() == 1)
    {
      /* we have only the fake interpolator which will be replaced
         by p; to avoid needing to handle the corner cases of
         having just one interpolator we will add an additional
         interpolator -after- p which starts and ends on the
         end point of p.
       */
      reference_counted_ptr<const interpolator_base> h;

      to_generic(p);
      h = FASTUIDRAWnew flat(p, p->end_pt());
      p = h;
    }

  /* hack-evil: we are going to replace m_interpolator[0]
     with p, we also need to change m_interpolator[1]->m_prev
     as well to p.
   */
  assert(d->m_interpolators.size() > 1);

  InterpolatorBasePrivate *q;
  q = reinterpret_cast<InterpolatorBasePrivate*>(d->m_interpolators[1]->m_d);
  q->m_prev = p.get();

  d->m_interpolators[0] = p;
  d->m_end_to_start = p;
}

void
fastuidraw::PathContour::
end(void)
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;

  if(d->m_current_control_points.empty())
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
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  reference_counted_ptr<const interpolator_base> h;
  h = FASTUIDRAWnew arc(prev_interpolator(), angle, d->m_start_pt);
  end_generic(h);
}

unsigned int
fastuidraw::PathContour::
number_points(void) const
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);
  return d->m_interpolators.size();
}

const fastuidraw::vec2&
fastuidraw::PathContour::
point(unsigned int I) const
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);
  return d->m_interpolators[I]->end_pt();
}

const fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base>&
fastuidraw::PathContour::
interpolator(unsigned int I) const
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  /* m_interpolator[I+1] connects point(I) to point(I+1).
   */
  unsigned int J(I+1);
  assert(J <= d->m_interpolators.size());

  /* interpolator(number_points()) is the interpolator
     connecting the last point added to the first
     point of the contour, i.e. is given by m_end_to_start
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
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  assert(!d->m_interpolators.empty());
  return d->m_interpolators[d->m_interpolators.size() - 1];
}

bool
fastuidraw::PathContour::
ended(void) const
{
  PathContourPrivate *d;
  d = reinterpret_cast<PathContourPrivate*>(m_d);

  return d->m_end_to_start;
}

fastuidraw::PathContour*
fastuidraw::PathContour::
deep_copy(void)
{
  PathContour *return_value;
  return_value = FASTUIDRAWnew PathContour();

  PathContourPrivate *d, *r;
  d = reinterpret_cast<PathContourPrivate*>(m_d);
  r = reinterpret_cast<PathContourPrivate*>(return_value->m_d);

  r->m_start_pt = d->m_start_pt;
  r->m_current_control_points = d->m_current_control_points;

  /* now we need to do the deep copies of the interpolator. eww.
   */
  r->m_interpolators.resize(d->m_interpolators.size());

  r->m_interpolators[0] = FASTUIDRAWnew flat(reference_counted_ptr<const interpolator_base>(), r->m_start_pt);
  for(unsigned int i = 1, endi = d->m_interpolators.size(); i < endi; ++i)
    {
      r->m_interpolators[i] = d->m_interpolators[i]->deep_copy(r->m_interpolators[i-1]);
    }

  if(d->m_end_to_start)
    {
      r->m_interpolators[0] = d->m_end_to_start->deep_copy(r->m_interpolators.back());
      r->m_end_to_start = r->m_interpolators[0];

      //we also need to replace r->m_interpolators[1]->m_prev with
      //the new value for r->m_interpolators[0]
      assert(r->m_interpolators.size() > 1);
      InterpolatorBasePrivate *q;
      q = reinterpret_cast<InterpolatorBasePrivate*>(r->m_interpolators[1]->m_d);
      q->m_prev = r->m_end_to_start.get();
    }
  return return_value;
}

/////////////////////////////////
// PathPrivate methods
PathPrivate::
PathPrivate(const PathPrivate &obj):
  m_tessellation_params(obj.m_tessellation_params),
  m_tessellation(obj.m_tessellation),
  m_contours(obj.m_contours)
{
  /* if the last contour is not ended, we need to do a
     deep copy on it.
   */
  if(!m_contours.back()->ended())
    {
      m_contours.back() = m_contours.back()->deep_copy();
    }
}

/////////////////////////////////////////
// fastuidraw::Path methods
fastuidraw::Path::
Path(const TessellatedPath::TessellationParams &tess_params)
{
  m_d = FASTUIDRAWnew PathPrivate(tess_params);
}

fastuidraw::Path::
Path(const Path &obj)
{
  PathPrivate *obj_d;
  obj_d = reinterpret_cast<PathPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PathPrivate(*obj_d);
}

fastuidraw::Path::
Path(const Path &obj, const TessellatedPath::TessellationParams &tess_param)
{
  PathPrivate *obj_d;
  obj_d = reinterpret_cast<PathPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PathPrivate(*obj_d);
  tessellation_params(tess_param);
}

void
fastuidraw::Path::
swap(Path &obj)
{
  std::swap(obj.m_d, m_d);
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::Path::
clear(void)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  d->m_tessellation.clear();
  d->m_contours.clear();
}

fastuidraw::Path&
fastuidraw::Path::
add_contour(const reference_counted_ptr<const PathContour> &pcontour)
{
  if(!pcontour || !pcontour->ended())
    {
      return *this;
    }

  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);

  reference_counted_ptr<PathContour> contour;
  contour = pcontour.const_cast_ptr<PathContour>();

  d->m_tessellation.clear();
  if(d->m_contours.empty() || d->m_contours.back()->ended())
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  pd = reinterpret_cast<PathPrivate*>(path.m_d);

  if(d != pd && !pd->m_contours.empty())
    {
      d->m_tessellation.clear();
      d->m_contours.reserve(d->m_contours.size() + pd->m_contours.size());

      reference_counted_ptr<PathContour> r;
      if(!d->m_contours.empty() && !d->m_contours.back()->ended())
        {
          r = d->m_contours.back();
          d->m_contours.pop_back();
        }

      unsigned int endi(pd->m_contours.size());
      assert(endi > 0u);

      if(!pd->m_contours.back()->ended())
        {
          --endi;
        }

      for(unsigned int i = 0; i < endi; ++i)
        {
          d->m_contours.push_back(pd->m_contours[i]);
        }

      if(r)
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
  d = reinterpret_cast<PathPrivate*>(m_d);

  if(!d->m_contours.empty())
    {
      const reference_counted_ptr<PathContour> &h(d->current_contour());
      if(!h->ended())
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
  d = reinterpret_cast<PathPrivate*>(m_d);

  if(d->m_contours.empty() || d->m_contours.back()->ended())
    {
      d->move_common(pt);
    }
  else
    {
      d->current_contour()->to_point(pt);
    }
  return *this;
}

void
fastuidraw::Path::
tessellation_params(const fastuidraw::TessellatedPath::TessellationParams &p)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);

  /* TODO:
      rather than checking for inequality, we should just check if the
      new tessellation parameters are finer.
   */
  if(p != d->m_tessellation_params)
    {
      d->m_tessellation = reference_counted_ptr<const TessellatedPath>();
    }
  d->m_tessellation_params = p;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::TessellatedPath>&
fastuidraw::Path::
tessellation(void) const
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  if(!d->m_tessellation)
    {
      d->m_tessellation = FASTUIDRAWnew TessellatedPath(*this,
                                                        d->m_tessellation_params);
    }
  return d->m_tessellation;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(const control_point &pt)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->add_control_point(pt.m_location);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(const arc &a)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->to_arc(a.m_angle, a.m_pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(contour_end)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->end();
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
operator<<(contour_end_arc a)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->end_arc(a.m_angle);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
line_to(const vec2 &pt)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->to_point(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
quadratic_to(const vec2 &ct, const vec2 &pt)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->to_arc(angle, pt);
  return *this;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::PathContour::interpolator_base>&
fastuidraw::Path::
prev_interpolator(void)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  return d->current_contour()->prev_interpolator();
}

fastuidraw::Path&
fastuidraw::Path::
custom_to(const reference_counted_ptr<const PathContour::interpolator_base> &p)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->to_generic(p);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
arc_move(float angle, const vec2 &pt)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  const reference_counted_ptr<PathContour> &h(d->current_contour());
  h->end_arc(angle);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
quadratic_move(const vec2 &ct, const vec2 &pt)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
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
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->end_generic(p);
  d->move_common(pt);
  return *this;
}

fastuidraw::Path&
fastuidraw::Path::
end_contour_custom(const reference_counted_ptr<const PathContour::interpolator_base> &p)
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  assert(!d->current_contour()->ended());
  d->current_contour()->end_generic(p);
  return *this;
}

unsigned int
fastuidraw::Path::
number_contours(void) const
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  return d->m_contours.size();
}

fastuidraw::reference_counted_ptr<const fastuidraw::PathContour>
fastuidraw::Path::
contour(unsigned int i) const
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);
  return d->m_contours[i];
}

const fastuidraw::TessellatedPath::TessellationParams&
fastuidraw::Path::
tessellation_params(void) const
{
  PathPrivate *d;
  d = reinterpret_cast<PathPrivate*>(m_d);

  return d->m_tessellation_params;
}
