#include "int_path.hpp"

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

  class poly_solution
  {
  public:
    float m_t;
    enum fastuidraw::detail::IntBezierCurve::solution_type m_type;
    int m_multiplicity;
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

  void
  solve_linear(fastuidraw::c_array<int> poly,
               uint32_t accepted_solutions,
               poly_solutions *solutions)
  {
    FASTUIDRAWassert(poly.size() == 2);

    if(poly[1] == 0)
      {
        return;
      }

    /* solving: poly[1] * t + poly[0] = 0,
       is same as solving with both coefficients
       negative. The below makes sure that poly[1]
       is positive
     */
    if(poly[1] < 0)
      {
        poly[1] = -poly[1];
        poly[0] = -poly[0];
      }

    if(poly[0] == 0)
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

  void
  solve_quadratic(fastuidraw::c_array<int> poly,
                  uint32_t accepted_solutions,
                  poly_solutions *solutions)
  {
    FASTUIDRAWassert(poly.size() == 3);
    if(poly[2] == 0)
      {
        solve_linear(poly.sub_array(0, 2), accepted_solutions, solutions);
        return;
      }

    // check for t = 0 solution
    if(poly[0] == 0)
      {
        solutions->add_0_solution(accepted_solutions);
        solve_linear(poly.sub_array(1), accepted_solutions, solutions);
        return;
      }

    int sum;
    sum = poly[0] + poly[1] + poly[2];
    if(sum == 0)
      {
        /* thus p(t) = a * t^2 + b * t + -(a+b)
                     = (t - 1)(at + a + b)
         */
        fastuidraw::vecN<int, 2> tmp;
        tmp[0] = poly[1] + poly[2];
        tmp[1] = poly[0];

        solutions->add_1_solution(accepted_solutions);
        solve_linear(tmp, accepted_solutions, solutions);
        return;
      }

    int desc;
    desc = poly[1] * poly[1] - 4 * poly[0] * poly[2];
    if(desc < 0)
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

    //make leading coeffient positive
    if(poly[2] < 0)
      {
        poly[2] = -poly[2];
        poly[1] = -poly[1];
        poly[0] = -poly[0];
        sum = -sum;
      }

    float a, b, radical;

    a = static_cast<float>(poly[2]);
    b = static_cast<float>(poly[1]);
    radical = sqrtf(static_cast<float>(desc));
    solutions->add_solution_if_acceptable(accepted_solutions, (-b - radical) / (2.0f * a));
    solutions->add_solution_if_acceptable(accepted_solutions, (-b + radical) / (2.0f * a));
  }

  void
  solve_cubic(fastuidraw::c_array<int> poly,
              uint32_t accepted_solutions,
              poly_solutions *solutions)
  {
    if(poly[3] == 0)
      {
        solve_quadratic(poly.sub_array(0, 3), accepted_solutions, solutions);
        return;
      }

    if(poly[0] == 0)
      {
        solutions->add_0_solution(accepted_solutions);
        solve_quadratic(poly.sub_array(1), accepted_solutions, solutions);
        return;
      }

    int sum;
    sum = poly[3] + poly[2] + poly[1] + poly[0];
    if(sum == 0)
      {
        //t = 1 is a solution.
        fastuidraw::vecN<int, 3> tmp;
        tmp[0] = poly[3] + poly[2] + poly[1];
        tmp[1] = poly[3] + poly[2];
        tmp[2] = poly[3];

        solutions->add_1_solution(accepted_solutions);
        solve_quadratic(tmp, accepted_solutions, solutions);
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

    if(3 * poly[1] * poly[3] == poly[2] * poly[2])
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

  void
  solve_polynomial(fastuidraw::c_array<int> poly,
                   uint32_t accepted_solutions,
                   poly_solutions *solutions)
  {
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

  void
  increment_p_and_p_t(const fastuidraw::vecN<std::vector<int>, 2> &curve,
                      float t, fastuidraw::vec2 &p, fastuidraw::vec2 &p_t)
  {
    for(int coord = 0; coord < 2; ++coord)
      {
        float powt(1.0f), powt_deriv(1.0f);
        int K(0);

        for(int coeff : curve[coord])
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

  void
  compute_solution_points(const fastuidraw::vecN<std::vector<int>, 2> &curve,
                          const poly_solutions &solutions,
                          std::vector<fastuidraw::detail::IntBezierCurve::solution_pt> *out_pts)
  {
    for(const poly_solution &S : solutions.solutions())
      {
        fastuidraw::vec2 p(0.0f, 0.0f), p_t(0.0f, 0.0f);
        fastuidraw::detail::IntBezierCurve::solution_pt T;

        increment_p_and_p_t(curve, S.m_t, p, p_t);
        T.m_multiplicity = S.m_multiplicity;
        T.m_type = S.m_type;
        T.m_t = S.m_t;
        T.m_p = p;
        T.m_p_t = p_t;
        out_pts->push_back(T);
      }
  }

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
      ivec2 p1, p2;
      p1 = m_control_pts[1] - m_control_pts[0];
      p2 = m_control_pts[2] - m_control_pts[0];

      if(p1.x() * p2.y() == p2.x() * p1.y())
        {
          m_control_pts.pop_back();
          m_control_pts[1] = m_control_pts[0] + p2;
        }
    }

  m_bb.union_points(m_control_pts.begin(), m_control_pts.end());
  generate_polynomial_from_bezier(make_c_array(m_control_pts), m_as_polynomial);
  compute_derivatives_cancel_pts();
  compute_derivative_zero_pts();
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
  solve_polynomial(sum, within_0_1, &solutions);
  solve_polynomial(difference, within_0_1, &solutions);
  compute_solution_points(m_as_polynomial, solutions, &m_derivatives_cancel);
}

void
fastuidraw::detail::IntBezierCurve::
compute_derivative_zero_pts(void)
{
  if(degree() < 2)
    {
      return;
    }

  /* compute where dx/dt or dy/dt are zero;
   */
  for(int coord = 0; coord < 2; ++coord)
    {
      poly_solutions solutions;
      vecN<int, 3> work_room;
      c_array<int> poly_to_solve(work_room);

      poly_to_solve = poly_to_solve.sub_array(0,  m_as_polynomial[coord].size() - 1);
      for(int k = 1, end_k = m_as_polynomial[coord].size(); k < end_k; ++k)
        {
          poly_to_solve[k - 1] = k * m_as_polynomial[coord][k];
        }

      solve_polynomial(poly_to_solve, within_0_1, &solutions);
      compute_solution_points(m_as_polynomial, solutions, &m_derivative_zero[coord]);
    }
}

enum fastuidraw::return_code
fastuidraw::detail::IntBezierCurve::
approximate_cubic_with_quadratics(vecN<reference_counted_ptr<IntBezierCurve>, 4> &out_curves) const
{
  if(degree() != 3)
    {
      out_curves = vecN<reference_counted_ptr<IntBezierCurve>, 4>();
      return routine_fail;
    }

  /*
    should we do the arithmatic in integer or float?

    Should we do the arithmatic in 64bit ints
    and scale the imput before and after hand
    to avoid successive rounding uglies?

    To get perfect avoiding of such, requires multiplying
    by _64_ since this_curve.pMid has an 8 in the denomitor
    of the source m_raw_curve and each of alpha and beta
    are from that, another factor of 8, together it is 64.
  */
  CubicDecomposerHelper<int> this_curve(m_control_pts[0], m_control_pts[1], m_control_pts[2], m_control_pts[3]);
  CubicDecomposerHelper<int> alpha(this_curve.p[0], this_curve.p0_1, this_curve.p01_12, this_curve.pMid);
  CubicDecomposerHelper<int> beta(this_curve.pMid, this_curve.p12_23, this_curve.p2_3, this_curve.p[3]);

  ivec2 pA, pB, pC, pD;

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

  out_curves[0] = FASTUIDRAWnew IntBezierCurve(this_curve.p[0], pA, alpha.pMid);
  out_curves[1] = FASTUIDRAWnew IntBezierCurve(alpha.pMid, pB, this_curve.pMid);
  out_curves[2] = FASTUIDRAWnew IntBezierCurve(this_curve.pMid, pC, beta.pMid);
  out_curves[3] = FASTUIDRAWnew IntBezierCurve(beta.pMid, pD, this_curve.p[3]);
  return routine_success;
}

enum fastuidraw::return_code
fastuidraw::detail::IntBezierCurve::
approximate_cubic_with_quadratics(vecN<reference_counted_ptr<IntBezierCurve>, 2> &out_curves) const
{
  if(degree() != 3)
    {
      out_curves = vecN<reference_counted_ptr<IntBezierCurve>, 2>();
      return routine_fail;
    }

  CubicDecomposerHelper<int> this_curve(m_control_pts[0], m_control_pts[1], m_control_pts[2], m_control_pts[3]);
  out_curves[0] = FASTUIDRAWnew IntBezierCurve(this_curve.p[0], this_curve.p[1], this_curve.pMid);
  out_curves[1] = FASTUIDRAWnew IntBezierCurve(this_curve.pMid, this_curve.p[2], this_curve.p[3]);
  return routine_success;
}

enum fastuidraw::return_code
fastuidraw::detail::IntBezierCurve::
approximate_cubic_with_quadratics(vecN<reference_counted_ptr<IntBezierCurve>, 1> &out_curves) const
{
  if(degree() != 3)
    {
      out_curves = vecN<reference_counted_ptr<IntBezierCurve>, 1>();
      return routine_fail;
    }

  ivec2 q;
  q = compute_midpoint(m_control_pts[1], m_control_pts[2]);
  out_curves[0] = FASTUIDRAWnew IntBezierCurve(m_control_pts[0], q, m_control_pts[3]);
  return routine_success;
}


void
fastuidraw::detail::IntBezierCurve::
compute_line_intersection(int pt, enum coordinate_type line_type,
                          std::vector<solution_pt> *out_value,
                          uint32_t solution_types_accepted) const
{
  vecN<int, 4> work_room;
  c_array<int> tmp(work_room.c_ptr(), m_as_polynomial[line_type].size());
  poly_solutions solutions;

  /* if x-fixed, means vertical line at x = pt,
     which means we want to know when curve has x = pt,
     thus we want the curve's x-polynomial
   */
  int coord(fixed_coordinate(line_type));
  std::copy(m_as_polynomial[coord].begin(), m_as_polynomial[coord].end(), tmp.begin());
  tmp[0] -= pt;

  solve_polynomial(tmp, solution_types_accepted, &solutions);
  compute_solution_points(m_as_polynomial, solutions, out_value);
}

//////////////////////////////////////
// fastuidraw::detail::IntPath methods
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
  curve = FASTUIDRAWnew IntBezierCurve(m_last_pt, pt);
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
  curve = FASTUIDRAWnew IntBezierCurve(m_last_pt, control_pt, pt);
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
  curve = FASTUIDRAWnew IntBezierCurve(m_last_pt, control_pt0, control_pt1, pt);
  m_bb.union_box(curve->bounding_box());

  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}


void
fastuidraw::detail::IntPath::
compute_distance_values(const ivec2 &step, const ivec2 &count,
                        array2d<distance_value> &out_values)
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
}
