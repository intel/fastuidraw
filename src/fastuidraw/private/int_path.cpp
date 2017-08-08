#include "int_path.hpp"
#include "util_private_ostream.hpp"

namespace
{
  template<typename T, size_t N>
  fastuidraw::vecN<T, N>
  compute_midpoint(const fastuidraw::vecN<T, N> &a, const fastuidraw::vecN<T, N> &b)
  {
    fastuidraw::vecN<T, N> c(a + b);
    return c / T(2);
  }

  void
  generate_polynomial_from_bezier(fastuidraw::const_c_array<fastuidraw::ivec2> pts,
                                  fastuidraw::vecN<std::vector<int>, 2> &curve)
  {
    fastuidraw::vecN<fastuidraw::ivec2, 4> q;

    FASTUIDRAWassert(pts.size() == 2 || pts.size() == 3 || pts.size() == 4);
    curve.x().resize(pts.size());
    curve.y().resize(pts.size());

    switch(pts.size())
      {
      case 2:
        q[0] = pts[0];
        q[1] = pts[1] - pts[0];
        break;
      case 3:
        q[0] = pts[0];
        q[1] = -2 * pts[0] + 2 * pts[1];
        q[2] = pts[0] - 2 * pts[1] + pts[2];
        break;
      case 4:
        q[0] = pts[0];
        q[1] = -3 * pts[0] + 3 * pts[1];
        q[2] = 3 * pts[0] - 6 * pts[1] + 3 * pts[2];
        q[3] = -pts[0] + pts[3] + 3 * pts[1] - 3 * pts[2];
        break;
      }

    for(unsigned int coord = 0; coord < 2; ++coord)
      {
        for(unsigned int d = 0; d < pts.size(); ++d)
          {
            curve[coord][d] = q[d][coord];
          }
      }
  }

  void
  record_distance_value_from_canidate(const fastuidraw::ivec2 &p, int radius,
                                      const fastuidraw::ivec2 &step,
                                      const fastuidraw::ivec2 &count,
                                      fastuidraw::array2d<fastuidraw::detail::IntPath::distance_value> &dst)
  {
    for(int x = fastuidraw::t_max(0, p.x() - radius),
          maxx = fastuidraw::t_min(count.x(), p.x() + radius);
        x < maxx; ++x)
      {
        for(int y = fastuidraw::t_max(0, p.y() - radius),
              maxy = fastuidraw::t_min(count.y(), p.y() + radius);
            y < maxy; ++y)
          {
            int v;

            v = fastuidraw::t_abs(x * step.x() - p.x())
              + fastuidraw::t_abs(y * step.y() - p.y());
            dst(x, y).record_distance_value(static_cast<float>(v));
          }
      }
  }

  template<typename T>
  class MultiplierFunctor
  {
  public:
    explicit
    MultiplierFunctor(T v):
      m_value(v)
    {}

    template<typename U>
    U
    operator()(const U &p) const
    {
      return m_value * p;
    }

  private:
    T m_value;
  };

  template<typename T>
  class CubicDecomposerHelper
  {
  public:
    typedef fastuidraw::vecN<T, 2> point;

    template<typename P>
    CubicDecomposerHelper(const P &q0,
                          const P &q1,
                          const P &q2,
                          const P &q3):
      p(point(q0), point(q1), point(q2), point(q3))
    {
       p0_1 = compute_midpoint(p[0], p[1]);
       p1_2 = compute_midpoint(p[1], p[2]);
       p2_3 = compute_midpoint(p[2], p[3]);

       p01_12 = compute_midpoint(p0_1, p1_2);
       p12_23 = compute_midpoint(p1_2, p2_3);

       pMid = compute_midpoint(p01_12, p12_23);
    }

    fastuidraw::vecN<point, 4> p;
    point p0_1, p1_2, p2_3; // p_i_j --> midpoint of pi and pj
    point p01_12, p12_23;   // p_ab_ij --> midpoint of p_a_b and p_i_j
    point pMid; // midpoint of p01_12 and p12_23
  };

  typedef fastuidraw::vecN<fastuidraw::ivec2, 3> Quadratic;
  class Cubic:public fastuidraw::vecN<fastuidraw::ivec2, 4>
  {
  public:
    Cubic(const fastuidraw::ivec2 &p0,
          const fastuidraw::ivec2 &c0,
          const fastuidraw::ivec2 &c1,
          const fastuidraw::ivec2 &p1):
      fastuidraw::vecN<fastuidraw::ivec2, 4>(p0, c0, c1, p1)
    {}

    void
    approximate_with_quadratics(fastuidraw::vecN<Quadratic, 4> *out_curves) const;

    void
    approximate_with_quadratics(fastuidraw::vecN<Quadratic, 2> *out_curves) const;

    void
    approximate_with_quadratics(fastuidraw::vecN<Quadratic, 1> *out_curves) const;
  };

  class CompareSolutions
  {
  public:
    explicit
    CompareSolutions(int coord):
      m_coord(coord)
    {
      FASTUIDRAWassert(m_coord == 0 || m_coord == 1);
    }

    bool
    operator()(const fastuidraw::detail::IntBezierCurve::solution_pt &lhs,
               const fastuidraw::detail::IntBezierCurve::solution_pt &rhs) const
    {
      return lhs.m_p[m_coord] < rhs.m_p[m_coord];
    }

  private:
    int m_coord;
  };

  class poly_solution
  {
  public:
    float m_t;
    enum fastuidraw::detail::IntBezierCurve::solution_type m_type;
    int m_multiplicity;

    friend
    std::ostream&
    operator<<(std::ostream &str, const poly_solution &obj)
    {
      str << "{t = " << obj.m_t << ", # = " << obj.m_multiplicity << "}";
      return str;
    }
  };

  class poly_solutions
  {
  public:
    poly_solutions(void):
      m_0_solution_location(-1),
      m_1_solution_location(-1)
    {}

    void
    add_solution_if_acceptable(uint32_t flags, float t, int mult = 1)
    {
      poly_solution P;
      P.m_t = t;
      P.m_multiplicity = mult;
      P.m_type = (P.m_t < 1.0f && P.m_t > 0.0f) ?
        fastuidraw::detail::IntBezierCurve::within_0_1 :
        fastuidraw::detail::IntBezierCurve::outside_0_1;

      if(flags & P.m_type)
        {
          m_solutions.push_back(P);
        }
    }

    void
    add_0_solution(uint32_t flags)
    {
      if(flags & fastuidraw::detail::IntBezierCurve::on_0_boundary)
        {
          if(m_0_solution_location == -1)
            {
              poly_solution P;

              P.m_t = 0.0f;
              P.m_multiplicity = 0;
              P.m_type = fastuidraw::detail::IntBezierCurve::on_0_boundary;
              m_0_solution_location = m_solutions.size();
              m_solutions.push_back(P);
            }
          ++m_solutions[m_0_solution_location].m_multiplicity;
        }
    }

    void
    add_1_solution(uint32_t flags)
    {
      if(flags & fastuidraw::detail::IntBezierCurve::on_1_boundary)
        {
          if(m_1_solution_location == -1)
            {
              poly_solution P;

              P.m_t = 1.0f;
              P.m_multiplicity = 0;
              P.m_type = fastuidraw::detail::IntBezierCurve::on_1_boundary;
              m_1_solution_location = m_solutions.size();
              m_solutions.push_back(P);
            }
          ++m_solutions[m_1_solution_location].m_multiplicity;
        }
    }

    const std::vector<poly_solution>&
    solutions(void) const
    {
      return m_solutions;
    }

  private:
    std::vector<poly_solution> m_solutions;
    int m_0_solution_location;
    int m_1_solution_location;
  };

  template<typename T>
  void
  solve_linear(fastuidraw::const_c_array<T> poly,
               uint32_t accepted_solutions,
               poly_solutions *solutions)
  {
    FASTUIDRAWassert(poly.size() == 2);
    if(poly[0] == T(0))
      {
        solutions->add_0_solution(accepted_solutions);
      }
    else if(poly[0] == -poly[1])
      {
        solutions->add_1_solution(accepted_solutions);
      }
    else
      {
        float t;
        t = static_cast<float>(-poly[0]) / static_cast<float>(poly[1]);
        solutions->add_solution_if_acceptable(accepted_solutions, t);
      }
  }

  template<typename T>
  void
  solve_quadratic(fastuidraw::const_c_array<T> poly,
                  uint32_t accepted_solutions,
                  poly_solutions *solutions)
  {
    FASTUIDRAWassert(poly.size() == 3);

    // check for t = 0 solution
    if(poly[0] == T(0))
      {
        solutions->add_0_solution(accepted_solutions);
        solve_linear(poly.sub_array(1), accepted_solutions, solutions);
        return;
      }

    int sum;
    sum = poly[0] + poly[1] + poly[2];
    if(sum == T(0))
      {
        /* thus p(t) = a * t^2 + b * t + -(a+b)
                     = (t - 1)(at + a + b)
         */
        fastuidraw::vecN<T, 2> tmp;
        tmp[0] = poly[1] + poly[2];
        tmp[1] = poly[0];

        solutions->add_1_solution(accepted_solutions);
        solve_linear(fastuidraw::const_c_array<T>(tmp), accepted_solutions, solutions);
        return;
      }

    T desc;
    desc = poly[1] * poly[1] - T(4) * poly[0] * poly[2];
    if(desc < T(0))
      {
        //both roots are imaginary
        return;
      }

    if(desc == 0)
      {
        //double root given by -poly[1] / (2 * poly[2])
        float t;
        t = 0.5f * static_cast<float>(-poly[1]) / static_cast<float>(poly[2]);
        solutions->add_solution_if_acceptable(accepted_solutions, t, 2);
        return;
      }

    float a, b, radical;
    a = static_cast<float>(poly[2]);
    b = static_cast<float>(poly[1]);
    radical = sqrtf(static_cast<float>(desc));
    solutions->add_solution_if_acceptable(accepted_solutions, (-b - radical) / (2.0f * a));
    solutions->add_solution_if_acceptable(accepted_solutions, (-b + radical) / (2.0f * a));
  }

  template<typename T>
  void
  solve_cubic(fastuidraw::const_c_array<T> poly,
              uint32_t accepted_solutions,
              poly_solutions *solutions)
  {
    if(poly[0] == T(0))
      {
        solutions->add_0_solution(accepted_solutions);
        solve_quadratic(poly.sub_array(1), accepted_solutions, solutions);
        return;
      }

    T sum;
    sum = poly[3] + poly[2] + poly[1] + poly[0];
    if(sum == T(0))
      {
        //t = 1 is a solution.
        fastuidraw::vecN<T, 3> tmp;
        tmp[0] = poly[3] + poly[2] + poly[1];
        tmp[1] = poly[3] + poly[2];
        tmp[2] = poly[3];

        solutions->add_1_solution(accepted_solutions);
        solve_quadratic(fastuidraw::const_c_array<T>(tmp), accepted_solutions, solutions);
        return;
      }

    float L, p, q, C, temp, dd;
    fastuidraw::vecN<float, 3> a;

    L = static_cast<float>(poly[3]);
    a[2] = static_cast<float>(poly[2]) / L;
    a[1] = static_cast<float>(poly[1]) / L;
    a[0] = static_cast<float>(poly[0]) / L;

    p = (3.0f * a[1] - a[2] * a[2]) / 3.0f;
    q = (9.0f * a[1] * a[2] - 27 * a[0]- 2 * a[2] * a[2] * a[2]) / 27.0f;
    dd = a[2] / 3.0f;

    if(T(3) * poly[1] * poly[3] == poly[2] * poly[2])
      {
        solutions->add_solution_if_acceptable(accepted_solutions, -dd + cbrtf(q));
        return;
      }

    temp = sqrtf(3.0f / fabs(p));
    C = 0.5f * q * temp * temp * temp;

    temp = 1.0f / temp;
    temp *= 2.0f;

    if(p > 0.0f)
      {
        float v0, tau;

        tau = cbrtf(C + sqrtf(1.0f + C * C));
        v0 = temp * (tau - 1.0f / tau) * 0.5f - dd;
        //Question: which is faster on device: using cbrtf and sqrtf or using sinhf and asinhf?
        //v0 = temp*sinhf( asinhf(C)/3.0f) -  dd;
        solutions->add_solution_if_acceptable(accepted_solutions, v0);
      }
    else
      {
        if(C >= 1.0f)
          {
            float v0, tau;
            tau = cbrtf(C + sqrtf(C * C - 1.0f));
            v0 = temp * (tau + 1.0/tau) * 0.5f - dd;
            //Question: which is faster on device: using cbrtf and sqrtf or using coshf and acoshf?
            //v0=temp*coshf( acoshf(C)/3.0f) - dd;
            solutions->add_solution_if_acceptable(accepted_solutions, v0);
          }
        else if(C <= -1.0f)
          {
            float v0, tau;
            tau = cbrtf(-C+ sqrtf(C * C - 1.0f));
            v0 = -temp * ( tau + 1.0 / tau) * 0.5f - dd;
            //Question: which is faster on device: using cbrtf and sqrtf or using coshf and acoshf?
            //v0= -temp*coshf( acoshf(-C)/3.0f) - dd;
            solutions->add_solution_if_acceptable(accepted_solutions, v0);
          }
        else
          {
            float v0, v1, v2, theta;

            //todo: replace with using cubrt, etc.
            //not clear if that would be faster or slower though.
            theta = acosf(C);
            v0 = temp * cosf( (theta          ) / 3.0f) - dd;
            v1 = temp * cosf( (theta + 2.0f * M_PI) / 3.0f) - dd;
            v2 = temp * cosf( (theta + 4.0f * M_PI) / 3.0f) - dd;

            solutions->add_solution_if_acceptable(accepted_solutions, v0);
            solutions->add_solution_if_acceptable(accepted_solutions, v1);
            solutions->add_solution_if_acceptable(accepted_solutions, v2);
          }
      }
  }

  template<typename T>
  void
  solve_polynomial(fastuidraw::const_c_array<T> poly,
                   uint32_t accepted_solutions,
                   poly_solutions *solutions)
  {
    while(!poly.empty() && poly.back() == T(0))
      {
        poly = poly.sub_array(0, poly.size() - 1);
      }

    if(poly.size() <= 1)
      {
        return;
      }

    FASTUIDRAWassert(poly.size() >= 2 && poly.size() <= 4);
    switch(poly.size())
      {
      case 2:
        solve_linear(poly, accepted_solutions, solutions);
        break;

      case 3:
        solve_quadratic(poly, accepted_solutions, solutions);
        break;

      case 4:
        solve_cubic(poly, accepted_solutions, solutions);
        break;
      }
  }

  template<typename T>
  void
  increment_p_and_p_t(const fastuidraw::vecN<std::vector<T>, 2> &curve,
                      float t, fastuidraw::vec2 &p, fastuidraw::vec2 &p_t)
  {
    for(int coord = 0; coord < 2; ++coord)
      {
        float powt(1.0f), powt_deriv(1.0f);
        int K(0);

        for(T coeff : curve[coord])
          {
            float fcoeff(coeff);
            p[coord] += fcoeff * powt;
            powt *= t;
            if(K != 0)
              {
                p_t[coord] += static_cast<float>(K) * fcoeff * powt_deriv;
                powt_deriv *= t;
              }
            ++K;
          }
      }
  }

  template<typename T>
  void
  compute_solution_points(fastuidraw::detail::IntBezierCurve::ID_t src,
                          const fastuidraw::vecN<std::vector<T>, 2> &curve,
                          const poly_solutions &solutions,
                          const fastuidraw::detail::IntBezierCurve::transformation<float> &tr,
                          std::vector<fastuidraw::detail::IntBezierCurve::solution_pt> *out_pts)
  {
    for(const poly_solution &S : solutions.solutions())
      {
        fastuidraw::vec2 p(0.0f, 0.0f), p_t(0.0f, 0.0f);
        fastuidraw::detail::IntBezierCurve::solution_pt v;

        increment_p_and_p_t<T>(curve, S.m_t, p, p_t);
        v.m_multiplicity = S.m_multiplicity;
        v.m_type = S.m_type;
        v.m_t = S.m_t;
        v.m_p = tr(p);
        v.m_p_t = tr.scale() * p_t;
        v.m_src = src;
        out_pts->push_back(v);
      }
  }
}

//////////////////////////////////////
// Cubic methods
void
Cubic::
approximate_with_quadratics(fastuidraw::vecN<Quadratic, 4> *out_curves) const
{
  /*
    Should we do the arithmatic in 64bit ints
    and scale the input before and after hand
    to avoid successive rounding uglies?

    To get perfect avoiding of such, requires multiplying
    by _64_ since this_curve.pMid has an 8 in the denomitor
    of the source m_raw_curve and each of alpha and beta
    are from that, another factor of 8, together it is 64.
  */
  const Cubic &in_curve(*this);
  CubicDecomposerHelper<int> this_curve(in_curve[0], in_curve[1], in_curve[2], in_curve[3]);
  CubicDecomposerHelper<int> alpha(this_curve.p[0], this_curve.p0_1, this_curve.p01_12, this_curve.pMid);
  CubicDecomposerHelper<int> beta(this_curve.pMid, this_curve.p12_23, this_curve.p2_3, this_curve.p[3]);

  fastuidraw::ivec2 pA, pB, pC, pD;

  pA = compute_midpoint(this_curve.p0_1, compute_midpoint(this_curve.p0_1, this_curve.p[0]));
  pB = compute_midpoint(this_curve.p01_12, compute_midpoint(this_curve.p01_12, this_curve.pMid));
  pC = compute_midpoint(this_curve.p12_23, compute_midpoint(this_curve.p12_23, this_curve.pMid));
  pD = compute_midpoint(this_curve.p2_3, compute_midpoint(this_curve.p2_3, this_curve.p[3]));

  /*
    the curves are:
      [p0, pA, alpha.pMid]
      [alpha.pMid, pB, pMid]
      [pMid, pC, beta.pMid]
      [beta.pMid, pD, p3]
  */
  (*out_curves)[0] = Quadratic(this_curve.p[0], pA, alpha.pMid);
  (*out_curves)[1] = Quadratic(alpha.pMid, pB, this_curve.pMid);
  (*out_curves)[2] = Quadratic(this_curve.pMid, pC, beta.pMid);
  (*out_curves)[3] = Quadratic(beta.pMid, pD, this_curve.p[3]);
}

void
Cubic::
approximate_with_quadratics(fastuidraw::vecN<Quadratic, 2> *out_curves) const
{
  const Cubic &in_curve(*this);
  CubicDecomposerHelper<int> this_curve(in_curve[0], in_curve[1], in_curve[2], in_curve[3]);

  (*out_curves)[0] = Quadratic(this_curve.p[0], this_curve.p[1], this_curve.pMid);
  (*out_curves)[1] = Quadratic(this_curve.pMid, this_curve.p[2], this_curve.p[3]);
}

void
Cubic::
approximate_with_quadratics(fastuidraw::vecN<Quadratic, 1> *out_curves) const
{
  fastuidraw::ivec2 q;
  const Cubic &in_curve(*this);

  q = compute_midpoint(in_curve[1], in_curve[2]);
  (*out_curves)[0] = Quadratic(in_curve[0], q, in_curve[3]);
}

/////////////////////////////////////
// fastuidraw::detail::IntBezierCurve methods
void
fastuidraw::detail::IntBezierCurve::
process_control_pts(void)
{
  FASTUIDRAWassert(!m_control_pts.empty());
  FASTUIDRAWassert(m_control_pts.size() <= 4);

  /* check if is quadratic that should be collapsed to line
   */
  if(m_control_pts.size() == 3)
    {
      ivec2 p1, p2, p2orig;
      p1 = m_control_pts[1] - m_control_pts[0];
      p2 = m_control_pts[2] - m_control_pts[0];
      p2orig = m_control_pts[2];

      if(p1.x() * p2.y() == p2.x() * p1.y())
        {
          m_control_pts.pop_back();
          m_control_pts[1] = p2orig;
        }
    }

  m_bb.union_points(m_control_pts.begin(), m_control_pts.end());
  generate_polynomial_from_bezier(make_c_array(m_control_pts), m_as_polynomial);
  compute_derivatives_cancel_pts();
}

void
fastuidraw::detail::IntBezierCurve::
compute_derivatives_cancel_pts(void)
{
  if(degree() < 2)
    {
      return;
    }

  /* compute where dx/dt has same magnitude as dy/dt.
   */
  vecN<vecN<int, 3>, 2> deriv(vecN<int, 3>(0, 0, 0), vecN<int, 3>(0, 0, 0));
  vecN<int, 3> sum, difference;

  for(int coord = 0; coord < 2; ++coord)
    {
      for(int k = 1, end_k = m_as_polynomial[coord].size(); k < end_k; ++k)
        {
           deriv[coord][k - 1] = k * m_as_polynomial[coord][k];
        }
    }
  sum = deriv[0] + deriv[1];
  difference = deriv[0] - deriv[1];

  poly_solutions solutions;
  solve_polynomial(const_c_array<int>(sum), within_0_1, &solutions);
  solve_polynomial(const_c_array<int>(difference), within_0_1, &solutions);
  compute_solution_points(m_ID, m_as_polynomial, solutions, transformation<float>(), &m_derivatives_cancel);
}

void
fastuidraw::detail::IntBezierCurve::
compute_line_intersection(int pt, enum coordinate_type line_type,
                          std::vector<solution_pt> *out_value,
                          uint32_t solution_types_accepted,
                          const transformation<int> &tr) const
{
  vecN<int64_t, 4> work_room;
  int coord(fixed_coordinate(line_type));
  c_array<int64_t> tmp(work_room.c_ptr(), m_as_polynomial[coord].size());
  poly_solutions solutions;

  /* transform m_as_polynomial via transformation tr;
     that transformation is tr(p) = tr.translate() + tr.scale() * p,
     thus we multiply each coefficient of m_as_polynomial by
     tr.scale(), but add tr.translate() only to the constant term.
   */
  std::transform(m_as_polynomial[coord].begin(), m_as_polynomial[coord].end(),
                 tmp.begin(), MultiplierFunctor<int>(tr.scale()));
  tmp[0] += tr.translate()[coord];

  /* solve for f(t) = pt, which is same as solving for
     f(t) - pt = 0.
   */
  tmp[0] -= pt;
  solve_polynomial(const_c_array<int64_t>(tmp), solution_types_accepted, &solutions);

  /* compute solution point values from polynomial solutions */
  compute_solution_points(m_ID, m_as_polynomial, solutions, tr.cast<float>(), out_value);
}

//////////////////////////////////////
// fastuidraw::detail::IntPath methods
void
fastuidraw::detail::IntPath::
add_to_path(const vec2 &offset, const vec2 &scale, Path *dst) const
{
  for(const IntContour &contour : m_contours)
    {
      const std::vector<reference_counted_ptr<const IntBezierCurve> > &curves(contour.curves());
      for(const reference_counted_ptr<const IntBezierCurve> &curve : curves)
        {
          const_c_array<ivec2> pts;

          pts = curve->control_pts().sub_array(0, curve->control_pts().size() - 1);
          *dst << offset + scale * vec2(pts.front());
          pts = pts.sub_array(1);
          while(!pts.empty())
            {
              *dst << Path::control_point(offset + scale * vec2(pts.front()));
              pts = pts.sub_array(1);
            }
        }
      *dst << Path::contour_end();
    }
}

fastuidraw::detail::IntBezierCurve::ID_t
fastuidraw::detail::IntPath::
computeID(void)
{
  IntBezierCurve::ID_t v;

  FASTUIDRAWassert(!m_contours.empty());
  v.m_contourID = m_contours.size() - 1;
  v.m_curveID = m_contours.back().curves().size();
  return v;
}

void
fastuidraw::detail::IntPath::
move_to(const ivec2 &pt)
{
  FASTUIDRAWassert(m_contours.empty() || m_contours.back().closed());
  m_contours.push_back(IntContour());
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
line_to(const ivec2 &pt)
{
  reference_counted_ptr<const IntBezierCurve> curve;
  curve = FASTUIDRAWnew IntBezierCurve(computeID(), m_last_pt, pt);
  m_bb.union_box(curve->bounding_box());

  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
conic_to(const ivec2 &control_pt, const ivec2 &pt)
{
  reference_counted_ptr<const IntBezierCurve> curve;
  curve = FASTUIDRAWnew IntBezierCurve(computeID(), m_last_pt, control_pt, pt);
  m_bb.union_box(curve->bounding_box());

  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
cubic_to(const ivec2 &control_pt0, const ivec2 &control_pt1, const ivec2 &pt)
{
  reference_counted_ptr<const IntBezierCurve> curve;
  curve = FASTUIDRAWnew IntBezierCurve(computeID(), m_last_pt, control_pt0, control_pt1, pt);
  m_bb.union_box(curve->bounding_box());

  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}


void
fastuidraw::detail::IntPath::
compute_distance_values(const ivec2 &step, const ivec2 &count,
                        const IntBezierCurve::transformation<int> &tr,
                        int radius, array2d<distance_value> &dst) const
{
  /* We are computing the L1-distance from the path. For a given
     curve C, that value is given by

       d(x, y) = inf { | x - C_x(t) | + | y - C_y(t) | : 0 <= t <= 1 }

     the function f(t) = | x - C_x(t) | + | y - C_y(t) | is not C1
     everywhere. However we can still compute its minimum by noting
     that it is C1 outside of where x = C_x(t) and y = C_y(t) and
     thus the set of points to check where the minumum occurs
     is given by the following list:
       1) those t for which x = C_x(t)
       2) those t for which y = C_y(t)
       3) those t for which dC_x/dt + dC_y/dt = 0
       4) those t for which dC_x/dt - dC_y/dt = 0
       5) t = 0 or t = 1

     Note that the actual number of polynomial solves needed is
     then just count.x (for 1) plus count.y (for 2). The items
     from (3) and (4) are already stored in IntBezierCurve
   */
  compute_outline_point_values(step, count, tr, radius, dst);
  compute_derivative_cancel_values(step, count, tr, radius, dst);
  compute_fixed_line_values(step, count, tr, dst);
}

void
fastuidraw::detail::IntPath::
compute_outline_point_values(const ivec2 &step, const ivec2 &count,
                             const IntBezierCurve::transformation<int> &tr,
                             int radius, array2d<distance_value> &dst) const
{
  for(const IntContour &contour: m_contours)
    {
      const std::vector<reference_counted_ptr<const IntBezierCurve> > &curves(contour.curves());
      for(const reference_counted_ptr<const IntBezierCurve> &curve : curves)
        {
          record_distance_value_from_canidate(tr(curve->control_pts().front()),
                                              radius, step, count, dst);
        }
    }
}

void
fastuidraw::detail::IntPath::
compute_derivative_cancel_values(const ivec2 &step, const ivec2 &count,
                                 const IntBezierCurve::transformation<int> &tr,
                                 int radius, array2d<distance_value> &dst) const
{
  for(const IntContour &contour: m_contours)
    {
      const std::vector<reference_counted_ptr<const IntBezierCurve> > &curves(contour.curves());
      for(const reference_counted_ptr<const IntBezierCurve> &curve : curves)
        {
          const std::vector<IntBezierCurve::solution_pt> &derivatives_cancel(curve->derivatives_cancel());
          for(const IntBezierCurve::solution_pt &S : derivatives_cancel)
            {
              ivec2 p(S.m_p.x(), S.m_p.y());
              record_distance_value_from_canidate(tr(p), radius, step, count, dst);
            }
        }
    }
}

void
fastuidraw::detail::IntPath::
compute_fixed_line_values(const ivec2 &step, const ivec2 &count,
                          const IntBezierCurve::transformation<int> &tr,
                          array2d<distance_value> &dst) const
{
  std::vector<std::vector<IntBezierCurve::solution_pt> > work_room0;
  std::vector<std::vector<IntBezierCurve::solution_pt> > work_room1;

  compute_fixed_line_values(IntBezierCurve::x_fixed, work_room0, step, count, tr, dst);
  compute_fixed_line_values(IntBezierCurve::y_fixed, work_room1, step, count, tr, dst);
}


void
fastuidraw::detail::IntPath::
compute_fixed_line_values(enum IntBezierCurve::coordinate_type tp,
                          std::vector<std::vector<IntBezierCurve::solution_pt> > &work_room,
                          const ivec2 &step, const ivec2 &count,
                          const IntBezierCurve::transformation<int> &tr,
                          array2d<distance_value> &dst) const
{
  enum distance_value::winding_ray_t ray_types[2][2] =
    {
      {distance_value::from_pt_to_y_negative_infinity, distance_value::from_pt_to_y_positive_infinity}, //fixed-coordinate 0
      {distance_value::from_pt_to_x_negative_infinity, distance_value::from_pt_to_x_positive_infinity}, //fixed-coordinate 1
    };

  int fixed_coord(IntBezierCurve::fixed_coordinate(tp));
  int varying_coord(IntBezierCurve::varying_coordinate(tp));

  work_room.resize(count[fixed_coord]);
  for(int i = 0; i < count[fixed_coord]; ++i)
    {
      work_room[i].clear();
    }

  /* record the solutions for each fixed line */
  for(const IntContour &contour: m_contours)
    {
      const std::vector<reference_counted_ptr<const IntBezierCurve> > &curves(contour.curves());
      for(const reference_counted_ptr<const IntBezierCurve> &curve : curves)
        {
          BoundingBox<int> bb;
          int cstart, cend, bbmin, bbmax;

          bb = curve->bounding_box(tr);

          FASTUIDRAWassert(!bb.empty());
          bbmin = bb.min_point()[fixed_coord];
          bbmax = bb.max_point()[fixed_coord];

          /* we do not need to solve the polynomial over the entire field, only
             the range of the bounding box of the curve, point at c:
                 step * c
             we want
                 bbmin <= step * c <= bbmax
             which becomes (assuming step > 0) to:
                 bbmin / step <= c <= bbmax / step
           */
          cstart = t_max(0, -1 + bbmin / step[fixed_coord]);
          cend = t_min(count[fixed_coord], 2 + bbmax / step[fixed_coord]);
          for(int c = cstart; c < cend; ++c)
            {
              int v;
              v = c * step[fixed_coord];
              curve->compute_line_intersection(v, tp, &work_room[c], IntBezierCurve::within_0_1, tr);
            }
        }
    }

  /* now for each line, do the distance computation along the line.
   */
  for(int c = 0; c < count[fixed_coord]; ++c)
    {
      std::vector<IntBezierCurve::solution_pt> &L(work_room[c]);
      int total_cnt(0);

      /* sort by the value in the varying coordinate
       */
      std::sort(L.begin(), L.end(), CompareSolutions(varying_coord));
      for(const IntBezierCurve::solution_pt &S : L)
        {
          FASTUIDRAWassert(S.m_multiplicity > 0);
          FASTUIDRAWassert(S.m_type != IntBezierCurve::on_1_boundary);
          FASTUIDRAWassert(S.m_t < 1.0f && S.m_t >= 0.0f);
          total_cnt += S.m_multiplicity;
        }

      for(int v = 0, current_cnt = 0, current_idx = 0, sz = L.size();
          v < count[varying_coord]; ++v)
        {
          ivec2 pixel;
          float p;
          int prev_idx;

          p = static_cast<float>(step[varying_coord] * v);
          pixel[fixed_coord] = c;
          pixel[varying_coord] = v;

          prev_idx = current_idx;

          /* advance to the next along the line L just after p
           */
          while(current_idx < sz && L[current_idx].m_p[varying_coord] < p)
            {
              FASTUIDRAWassert(L[current_idx].m_multiplicity > 0);
              current_cnt += L[current_idx].m_multiplicity;
              ++current_idx;
            }

          /* update the distance values for all those points between
             the point on the line we were at before the loop start
             and the point on the line we are at now
           */
          for(int idx = t_max(0, prev_idx - 1), end_idx = t_min(sz, current_idx + 2);
              idx < end_idx; ++idx)
            {
              dst(pixel.x(), pixel.y()).record_distance_value(t_abs(p - L[idx].m_p[varying_coord]));
            }

          /* update the ray-intersection counts */
          dst(pixel.x(), pixel.y()).increment_ray_intersection_count(ray_types[fixed_coord][0], current_cnt);
          dst(pixel.x(), pixel.y()).increment_ray_intersection_count(ray_types[fixed_coord][1], total_cnt - current_cnt);
        }

      compute_winding_values(tp, c, L, step[varying_coord], count[varying_coord], dst);
    }
}

void
fastuidraw::detail::IntPath::
compute_winding_values(enum IntBezierCurve::coordinate_type tp, int c,
                       const std::vector<IntBezierCurve::solution_pt> &L,
                       int step, int count,
                       array2d<distance_value> &dst) const
{
  int v, winding, sgn;
  unsigned int idx, end_idx;
  int varying_coord(IntBezierCurve::varying_coordinate(tp));
  int fixed_coord(IntBezierCurve::fixed_coordinate(tp));
  ivec2 pixel;

  sgn = (tp == IntBezierCurve::x_fixed) ? 1 : -1;
  pixel[fixed_coord] = c;

  for(winding = 0, v = 0, idx = 0, end_idx = L.size(); v < count; ++v)
    {
      float p;
      p = static_cast<float>(step * v);

      /* update the winding number to the last intersection before p */
      for(;idx < end_idx && L[idx].m_p[varying_coord] < p; ++idx)
        {
          /* should we disregard solutions with even multiplicity? */
          if(L[idx].m_p_t[fixed_coord] > 0.0f)
            {
              winding += L[idx].m_multiplicity;
            }
          else if(L[idx].m_p_t[fixed_coord] < 0.0f)
            {
              winding -= L[idx].m_multiplicity;
            }
        }
      pixel[varying_coord] = v;
      dst(pixel.x(), pixel.y()).set_winding_number(tp, sgn * winding);
    }
}
