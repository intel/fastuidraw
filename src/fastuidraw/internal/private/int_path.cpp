/*!
 * \file int_path.cpp
 * \brief file int_path.cpp
 *
 * Copyright 2017 by Intel.
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
 * Much of the code is inspired and lifted from WRATHFreeTypeSupport.cpp,
 * WRATHPolynomial.cpp, WRATHPolynomial.hpp and WRATHPolynonialImplement
 * of WRATH, whose copyright and liscence is below:
 *
 * Copyright 2013 by Nomovok Ltd.
 *
 * Contact: info@nomovok.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 *
 */

#include <iterator>
#include <set>
#include "int_path.hpp"
#include "bezier_util.hpp"
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
  generate_polynomial_from_bezier(fastuidraw::c_array<const fastuidraw::ivec2> pts,
                                  fastuidraw::vecN<fastuidraw::c_array<int>, 2> &curve)
  {
    fastuidraw::vecN<fastuidraw::ivec2, 4> q(fastuidraw::ivec2(0));

    FASTUIDRAWassert(pts.size() == 2 || pts.size() == 3 || pts.size() == 4);
    FASTUIDRAWassert(curve.x().size() == pts.size());
    FASTUIDRAWassert(curve.y().size() == pts.size());

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

  fastuidraw::vecN<fastuidraw::ivec2, 3>
  quadratic_from_cubic(fastuidraw::c_array<const fastuidraw::ivec2> pts)
  {
    return fastuidraw::detail::quadratic_from_cubicT<int>(pts);
  }

  fastuidraw::vecN<fastuidraw::vecN<fastuidraw::ivec2, 4>, 2>
  split_cubic(fastuidraw::c_array<const fastuidraw::ivec2> pts)
  {
    return fastuidraw::detail::split_cubicT<int>(pts);
  }

  class QuadraticBezierCurve:public fastuidraw::vecN<fastuidraw::ivec2, 3>
  {
  public:
    QuadraticBezierCurve(void)
    {}

    QuadraticBezierCurve(const fastuidraw::ivec2 &p0,
                         const fastuidraw::ivec2 &c,
                         const fastuidraw::ivec2 &p1):
      fastuidraw::vecN<fastuidraw::ivec2, 3>(p0, c, p1)
    {}

    const fastuidraw::ivec2&
    control_pt(void) const
    {
      return operator[](1);
    }

    float
    compute_curvature(void) const
    {
      const QuadraticBezierCurve &pts(*this);
      fastuidraw::vecN<fastuidraw::ivec2, 3> as_poly;

      as_poly[0] = pts[0];
      as_poly[1] = -2 * pts[0] + 2 * pts[1];
      as_poly[2] = pts[0] - 2 * pts[1] + pts[2];

      fastuidraw::vec2 a1(as_poly[1]), a2(as_poly[2]);
      float a, b, c, R, desc, tt;

      R = fastuidraw::t_abs(a1.x() * a2.y() - a1.y() * a2.x());
      a = fastuidraw::dot(a1, a1);
      b = 2.0f * fastuidraw::dot(a1, a2);
      c = fastuidraw::dot(a2, a2);

      const float epsilon(0.000001f), epsilon2(epsilon * epsilon);

      desc = fastuidraw::t_sqrt(fastuidraw::t_max(epsilon2, 4.0f * a * c - b * b));
      tt = desc / fastuidraw::t_max(epsilon, fastuidraw::t_abs(2.0f * a + b));
      return 2.0 * R * atanf(tt) / desc;
    }
  };

  class Solver
  {
  public:
    typedef fastuidraw::detail::IntBezierCurve IntBezierCurve;

    enum coordinate_type
      {
        x_fixed = 0,
        y_fixed = 1,
        x_varying = y_fixed,
        y_varying = x_fixed
      };

    enum solution_type
      {
        within_0_1    = 1,
        on_0_boundary = 2,
        on_1_boundary = 4,
        outside_0_1   = 8,
      };

    class evaluated_pt
    {
    public:
      float m_t;
      fastuidraw::vec2 m_p;
      fastuidraw::vec2 m_p_t;
    };

    class solution_pt:public evaluated_pt
    {
    public:
      int m_multiplicity;
      enum solution_type m_type;
      IntBezierCurve::ID_t m_src;
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
      operator()(const solution_pt &lhs,
                 const solution_pt &rhs) const
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
      enum solution_type m_type;
      int m_multiplicity;
    };

    template<typename interator>
    class poly_solutions
    {
    public:
      explicit
      poly_solutions(interator iter);

      void
      add_solution_if_acceptable(uint32_t flags, float t, int mult = 1);

      void
      add_0_solution(uint32_t flags);

      void
      add_1_solution(uint32_t flags);

      void
      finalize(void);

      bool
      finalized(void) const;

      unsigned int
      size(void) const;

    private:
      interator m_iter;
      unsigned int m_size;
      int m_multiplicity_0_solution;
      int m_multiplicity_1_solution;
    };

    explicit
    Solver(const IntBezierCurve &curve):
      m_curve(curve)
    {}

    /* returns what coordinate is fixed for a
     * given coordinate_type
     */
    static
    int
    fixed_coordinate(enum coordinate_type tp)
    {
      return tp;
    }

    /* returns what coordinate is varying for a
     * given coordinate_type
     */
    static
    int
    varying_coordinate(enum coordinate_type tp)
    {
      return 1 - fixed_coordinate(tp);
    }

    template<typename T, typename iterator, typename solutions_iterator>
    static
    int
    compute_solution_points(IntBezierCurve::ID_t src,
                            const fastuidraw::vecN<fastuidraw::c_array<const T>, 2> &curve,
                            solutions_iterator solutions_begin, solutions_iterator solutions_end,
                            const IntBezierCurve::transformation<float> &tr,
                            iterator out_pts);

    template<typename T, typename iterator>
    static
    void
    solve_polynomial(fastuidraw::c_array<const T> poly,
                     uint32_t accepted_solutions,
                     poly_solutions<iterator> *solutions);

    /*
     * \param pt fixed value of line
     * \param line_type if line if vertical (x-fixed) or horizontal (y-fixed)
     * \param out_value location to which to put values
     * \param solution_types_accepted bit mask using enums of solution_type
     *                               of what solutions are accepted
     * \param tr transformation to apply to the IntPath
     */
    void
    compute_line_intersection(int pt, enum coordinate_type line_type,
                              uint32_t solution_types_accepted,
                              const IntBezierCurve::transformation<int> &tr,
                              std::vector<solution_pt> *out_value) const;

    void
    compute_lines_intersection(enum coordinate_type line_type,
                               int step, int count,
                               uint32_t solution_types_accepted,
                               const IntBezierCurve::transformation<int> &tr,
                               std::vector<std::vector<solution_pt> > *out_value) const;

  private:
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

    template<typename T, typename iterator>
    static
    void
    solve_linear(fastuidraw::c_array<const T> poly,
                 uint32_t accepted_solutions,
                 poly_solutions<iterator> *solutions);

    template<typename T, typename iterator>
    static
    void
    solve_quadratic(fastuidraw::c_array<const T> poly,
                    uint32_t accepted_solutions,
                    poly_solutions<iterator> *solutions);

    template<typename T, typename iterator>
    static
    void
    solve_cubic(fastuidraw::c_array<const T> poly,
                uint32_t accepted_solutions,
                poly_solutions<iterator> *solutions);

    template<typename T>
    static
    void
    increment_p_and_p_t(const fastuidraw::vecN<fastuidraw::c_array<const T>, 2> &curve,
                        float t, fastuidraw::vec2 &p, fastuidraw::vec2 &p_t);

    const IntBezierCurve &m_curve;
  };

  class distance_value
  {
  public:
    typedef fastuidraw::detail::IntBezierCurve IntBezierCurve;

    enum winding_ray_t
      {
        from_pt_to_x_negative_infinity,
        from_pt_to_x_positive_infinity,
        from_pt_to_y_negative_infinity,
        from_pt_to_y_positive_infinity,
      };

    distance_value(void):
      m_distance(-1.0f),
      m_ray_intersection_counts(0, 0, 0, 0),
      m_winding_numbers(0, 0)
    {}

    void
    record_distance_value(float v)
    {
      FASTUIDRAWassert(v >= 0.0f);
      m_distance = (m_distance >= 0.0f) ?
        fastuidraw::t_min(v, m_distance) :
        v;
    }

    void
    increment_ray_intersection_count(enum winding_ray_t tp, int mult)
    {
      FASTUIDRAWassert(mult >= 0);
      m_ray_intersection_counts[tp] += mult;
    }

    void
    set_winding_number(enum Solver::coordinate_type tp, int w)
    {
      m_winding_numbers[tp] = w;
    }

    float
    distance(float max_distance) const
    {
      return (m_distance < 0.0f) ?
        max_distance :
        fastuidraw::t_min(max_distance, m_distance);
    }

    float
    raw_distance(void) const
    {
      return m_distance;
    }

    int
    ray_intersection_count(enum winding_ray_t tp) const
    {
      return m_ray_intersection_counts[tp];
    }

    int
    winding_number(enum Solver::coordinate_type tp = Solver::x_fixed) const
    {
      return m_winding_numbers[tp];
    }

  private:
    /* unsigned distance in IntPath coordinates,
     * a negative value indicates value is not
     * assigned
     */
    float m_distance;

    /* number of intersection (counted with multiplicity)
     * of a ray agains the path.
     */
    fastuidraw::vecN<int, 4> m_ray_intersection_counts;

    /* winding number computed from horizontal or vertical lines
     */
    fastuidraw::vecN<int, 2> m_winding_numbers;
  };

  class DistanceFieldGenerator
  {
  public:
    typedef fastuidraw::detail::IntContour IntContour;
    typedef fastuidraw::detail::IntBezierCurve IntBezierCurve;
    typedef fastuidraw::ivec2 ivec2;
    typedef fastuidraw::vec2 vec2;

    explicit
    DistanceFieldGenerator(const std::vector<IntContour> &p):
      m_contours(p)
    {}

    /*
     * Compute distance_value for the domain
     *  D = { (x(i), y(j)) : 0 <= i < count.x(), 0 <= j < count.y() }
     * where
     *  x(i) = step.x() * i
     *  y(j) = step.y() * j
     * One can get translation via using the transformation argument, tr.
     */
    void
    compute_distance_values(const ivec2 &step, const ivec2 &count,
                            const IntBezierCurve::transformation<int> &tr,
                            int radius,
                            fastuidraw::array2d<distance_value> &out_values) const;

    static
    uint8_t
    pixel_value_from_distance(float dist, bool outside);

  private:
    template<typename T>
    static
    void
    record_distance_value_from_canidate(const fastuidraw::vecN<T, 2> &p, int radius,
                                        const ivec2 &step,
                                        const ivec2 &count,
                                        fastuidraw::array2d<distance_value> &dst);

    void
    compute_outline_point_values(const ivec2 &step, const ivec2 &count,
                                 const IntBezierCurve::transformation<int> &tr,
                                 int radius,
                                 fastuidraw::array2d<distance_value> &dst) const;
    void
    compute_derivative_cancel_values(const ivec2 &step, const ivec2 &count,
                                     const IntBezierCurve::transformation<int> &tr,
                                     int radius,
                                     fastuidraw::array2d<distance_value> &dst) const;
    void
    compute_fixed_line_values(const ivec2 &step, const ivec2 &count,
                              const IntBezierCurve::transformation<int> &tr,
                              fastuidraw::array2d<distance_value> &dst) const;
    void
    compute_fixed_line_values(enum Solver::coordinate_type tp,
                              std::vector<std::vector<Solver::solution_pt> > &work_room,
                              const ivec2 &step, const ivec2 &count,
                              const IntBezierCurve::transformation<int> &tr,
                              fastuidraw::array2d<distance_value> &dst) const;

    const std::vector<fastuidraw::detail::IntContour> &m_contours;
  };
}

//////////////////////////////////////////////
// Solver::poly_solutions methods
template<typename iterator>
Solver::poly_solutions<iterator>::
poly_solutions(iterator iter):
  m_iter(iter),
  m_size(0),
  m_multiplicity_0_solution(0),
  m_multiplicity_1_solution(0)
{}

template<typename iterator>
void
Solver::poly_solutions<iterator>::
add_solution_if_acceptable(uint32_t flags, float t, int mult)
{
  enum solution_type tp;
  tp = (t < 1.0f && t > 0.0f) ?
    within_0_1 :
    outside_0_1;
  if (flags & tp)
    {
      poly_solution P;
      P.m_t = t;
      P.m_multiplicity = mult;
      P.m_type = tp;
      *m_iter++ = P;
      ++m_size;
    }
}

template<typename iterator>
void
Solver::poly_solutions<iterator>::
add_0_solution(uint32_t flags)
{
  if (flags & on_0_boundary)
    {
      ++m_multiplicity_0_solution;
    }
}

template<typename iterator>
void
Solver::poly_solutions<iterator>::
add_1_solution(uint32_t flags)
{
  if (flags & on_1_boundary)
    {
      ++m_multiplicity_1_solution;
    }
}

template<typename iterator>
void
Solver::poly_solutions<iterator>::
finalize(void)
{
  FASTUIDRAWassert(m_multiplicity_0_solution >= 0);
  FASTUIDRAWassert(m_multiplicity_1_solution >= 0);
  if (m_multiplicity_0_solution > 0)
    {
      poly_solution P;
      P.m_t = 0.0f;
      P.m_multiplicity = m_multiplicity_0_solution;
      P.m_type = on_0_boundary;
      *m_iter++ = P;
    }

  if (m_multiplicity_1_solution > 1)
    {
      poly_solution P;
      P.m_t = 1.0f;
      P.m_multiplicity = m_multiplicity_1_solution;
      P.m_type = on_1_boundary;
      *m_iter++ = P;
    }
  m_multiplicity_0_solution = -1;
  m_multiplicity_1_solution = -1;
}

template<typename iterator>
bool
Solver::poly_solutions<iterator>::
finalized(void) const
{
  FASTUIDRAWassert((m_multiplicity_0_solution == -1) == (m_multiplicity_1_solution == -1));
  return m_multiplicity_0_solution == -1;
}

template<typename iterator>
unsigned int
Solver::poly_solutions<iterator>::
size(void) const
{
  FASTUIDRAWassert(finalized());
  return m_size;
}

////////////////////////////////////////////////////
// Solver methods
template<typename T, typename iterator>
void
Solver::
solve_polynomial(fastuidraw::c_array<const T> poly,
                 uint32_t accepted_solutions,
                 poly_solutions<iterator> *solutions)
{
  while(!poly.empty() && poly.back() == T(0))
    {
      poly = poly.sub_array(0, poly.size() - 1);
    }

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

template<typename T, typename iterator>
void
Solver::
solve_linear(fastuidraw::c_array<const T> poly,
             uint32_t accepted_solutions,
             poly_solutions<iterator> *solutions)
{
  FASTUIDRAWassert(poly.size() == 2);
  if (poly[0] == T(0))
    {
      solutions->add_0_solution(accepted_solutions);
    }
  else if (poly[0] == -poly[1])
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

template<typename T, typename iterator>
void
Solver::
solve_quadratic(fastuidraw::c_array<const T> poly,
                uint32_t accepted_solutions,
                poly_solutions<iterator> *solutions)
{
  FASTUIDRAWassert(poly.size() == 3);

  // check for t = 0 solution
  if (poly[0] == T(0))
    {
      solutions->add_0_solution(accepted_solutions);
      solve_linear(poly.sub_array(1), accepted_solutions, solutions);
      return;
    }

  int sum;
  sum = poly[0] + poly[1] + poly[2];
  if (sum == T(0))
    {
      /* thus p(t) = a * t^2 + b * t + -(a+b)
       * = (t - 1)(at + a + b)
       */
      fastuidraw::vecN<T, 2> tmp;
      tmp[0] = poly[1] + poly[2];
      tmp[1] = poly[0];

      solutions->add_1_solution(accepted_solutions);
      solve_linear(fastuidraw::c_array<const T>(tmp), accepted_solutions, solutions);
      return;
    }

  T desc;
  desc = poly[1] * poly[1] - T(4) * poly[0] * poly[2];
  if (desc < T(0))
    {
      //both roots are imaginary
      return;
    }

  if (desc == 0)
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

template<typename T, typename iterator>
void
Solver::
solve_cubic(fastuidraw::c_array<const T> poly,
            uint32_t accepted_solutions,
            poly_solutions<iterator> *solutions)
{
  if (poly[0] == T(0))
    {
      solutions->add_0_solution(accepted_solutions);
      solve_quadratic(poly.sub_array(1), accepted_solutions, solutions);
      return;
    }

  T sum;
  sum = poly[3] + poly[2] + poly[1] + poly[0];
  if (sum == T(0))
    {
      //t = 1 is a solution.
      fastuidraw::vecN<T, 3> tmp;
      tmp[0] = poly[3] + poly[2] + poly[1];
      tmp[1] = poly[3] + poly[2];
      tmp[2] = poly[3];

      solutions->add_1_solution(accepted_solutions);
      solve_quadratic(fastuidraw::c_array<const T>(tmp), accepted_solutions, solutions);
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

  if (T(3) * poly[1] * poly[3] == poly[2] * poly[2])
    {
      solutions->add_solution_if_acceptable(accepted_solutions, -dd + cbrtf(q));
      return;
    }

  temp = sqrtf(3.0f / fabs(p));
  C = 0.5f * q * temp * temp * temp;

  temp = 1.0f / temp;
  temp *= 2.0f;

  if (p > 0.0f)
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
      if (C >= 1.0f)
        {
          float v0, tau;
          tau = cbrtf(C + sqrtf(C * C - 1.0f));
          v0 = temp * (tau + 1.0 / tau) * 0.5f - dd;
          //Question: which is faster on device: using cbrtf and sqrtf or using coshf and acoshf?
          //v0=temp*coshf( acoshf(C)/3.0f) - dd;
          solutions->add_solution_if_acceptable(accepted_solutions, v0);
        }
      else if (C <= -1.0f)
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
          v1 = temp * cosf( (theta + 2.0f * FASTUIDRAW_PI) / 3.0f) - dd;
          v2 = temp * cosf( (theta + 4.0f * FASTUIDRAW_PI) / 3.0f) - dd;

          solutions->add_solution_if_acceptable(accepted_solutions, v0);
          solutions->add_solution_if_acceptable(accepted_solutions, v1);
          solutions->add_solution_if_acceptable(accepted_solutions, v2);
        }
    }
}

template<typename T>
void
Solver::
increment_p_and_p_t(const fastuidraw::vecN<fastuidraw::c_array<const T>, 2> &curve,
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
          if (K != 0)
            {
              p_t[coord] += static_cast<float>(K) * fcoeff * powt_deriv;
              powt_deriv *= t;
            }
          ++K;
        }
    }
}

template<typename T, typename iterator, typename solutions_iterator>
int
Solver::
compute_solution_points(fastuidraw::detail::IntBezierCurve::ID_t src,
                        const fastuidraw::vecN<fastuidraw::c_array<const T>, 2> &curve,
                        solutions_iterator solutions_begin, solutions_iterator solutions_end,
                        const IntBezierCurve::transformation<float> &tr,
                        iterator out_pts)
{
  int R(0);
  for(;solutions_begin != solutions_end; ++solutions_begin)
    {
      const poly_solution &S(*solutions_begin);
      fastuidraw::vec2 p(0.0f, 0.0f), p_t(0.0f, 0.0f);
      Solver::solution_pt v;

      increment_p_and_p_t<T>(curve, S.m_t, p, p_t);
      v.m_multiplicity = S.m_multiplicity;
      v.m_type = S.m_type;
      v.m_t = S.m_t;
      v.m_p = tr(p);
      v.m_p_t = tr.scale() * p_t;
      v.m_src = src;
      *out_pts = v;
      ++out_pts;
      ++R;
    }
  return R;
}

void
Solver::
compute_line_intersection(int pt, enum coordinate_type line_type,
                          uint32_t solution_types_accepted,
                          const IntBezierCurve::transformation<int> &tr,
                          std::vector<solution_pt> *out_value) const
{
  fastuidraw::vecN<int64_t, 4> work_room;
  int coord(fixed_coordinate(line_type));
  fastuidraw::c_array<const int> poly(m_curve.as_polynomial(coord));
  fastuidraw::c_array<int64_t> tmp(work_room.c_ptr(), poly.size());

  typedef std::vector<poly_solution> solution_holder_t;
  typedef std::back_insert_iterator<solution_holder_t> solution_iter;

  solution_holder_t solution_holder;
  solution_iter solutions_iter(std::back_inserter(solution_holder));
  poly_solutions<solution_iter> solutions(solutions_iter);

  /* transform m_as_polynomial via transformation tr;
   * that transformation is tr(p) = tr.translate() + tr.scale() * p,
   * thus we multiply each coefficient of m_as_polynomial by
   * tr.scale(), but add tr.translate() only to the constant term.
   */
  std::transform(poly.begin(), poly.end(), tmp.begin(), MultiplierFunctor<int>(tr.scale()));
  tmp[0] += tr.translate()[coord];

  /* solve for f(t) = pt, which is same as solving for
   * f(t) - pt = 0.
   */
  tmp[0] -= pt;
  solve_polynomial(fastuidraw::c_array<const int64_t>(tmp), solution_types_accepted, &solutions);
  solutions.finalize();
  FASTUIDRAWassert(solutions.size() == solution_holder.size());

  /* compute solution point values from polynomial solutions */
  compute_solution_points(m_curve.ID(), m_curve.as_polynomial(),
                          solution_holder.begin(), solution_holder.end(),
                          tr.cast<float>(),
                          std::back_inserter(*out_value));
}

void
Solver::
compute_lines_intersection(enum coordinate_type tp, int step, int count,
                           uint32_t solution_types_accepted,
                           const IntBezierCurve::transformation<int> &tr,
                           std::vector<std::vector<solution_pt> > *out_value) const
{
  int cstart, cend;
  int fixed_coord(fixed_coordinate(tp));

  FASTUIDRAWassert(out_value->size() == static_cast<unsigned int>(count));

  if ((solution_types_accepted & outside_0_1) == 0)
    {
      fastuidraw::BoundingBox<int> bb;
      int bbmin, bbmax;

      bb = m_curve.bounding_box(tr);

      FASTUIDRAWassert(!bb.empty());
      bbmin = bb.min_point()[fixed_coord];
      bbmax = bb.max_point()[fixed_coord];

      /* we do not need to solve the polynomial over the entire field, only
       * the range of the bounding box of the curve, point at c:
       *    step * c
       * we want
       *    bbmin <= step * c <= bbmax
       * which becomes (assuming step > 0) to:
       *    bbmin / step <= c <= bbmax / step
       */

      cstart = fastuidraw::t_max(0, bbmin / step);
      cend = fastuidraw::t_min(count, 2 + bbmax / step);
    }
  else
    {
      cstart = 0;
      cend = count;
    }

  for(int c = cstart; c < cend; ++c)
    {
      int v;

      v = c * step;
      compute_line_intersection(v, tp, solution_types_accepted, tr, &(*out_value)[c]);
    }
}

//////////////////////////////////////////////
// DistanceFieldGenerator methods
template<typename T>
void
DistanceFieldGenerator::
record_distance_value_from_canidate(const fastuidraw::vecN<T, 2> &p, int radius,
                                    const ivec2 &step,
                                    const ivec2 &count,
                                    fastuidraw::array2d<distance_value> &dst)
{
  ivec2 ip(p);
  for(int x = fastuidraw::t_max(0, ip.x() - radius),
        maxx = fastuidraw::t_min(count.x(), ip.x() + radius);
      x < maxx; ++x)
    {
      for(int y = fastuidraw::t_max(0, ip.y() - radius),
            maxy = fastuidraw::t_min(count.y(), ip.y() + radius);
          y < maxy; ++y)
        {
          T v;

          v = fastuidraw::t_abs(T(x * step.x()) - p.x())
            + fastuidraw::t_abs(T(y * step.y()) - p.y());
          dst(x, y).record_distance_value(static_cast<float>(v));
        }
    }
}

uint8_t
DistanceFieldGenerator::
pixel_value_from_distance(float dist, bool outside)
{
  uint8_t v;

  dist = fastuidraw::t_min(1.0f, fastuidraw::t_max(0.0f, dist));
  if (outside)
    {
      dist = -dist;
    }

  dist = (dist + 1.0f) * 0.5f;
  v = static_cast<uint8_t>(255.0f * dist);
  return v;
}

void
DistanceFieldGenerator::
compute_distance_values(const ivec2 &step, const ivec2 &count,
                        const IntBezierCurve::transformation<int> &tr,
                        int radius, fastuidraw::array2d<distance_value> &dst) const
{
  /* We are computing the L1-distance from the path. For a given
   * curve C, that value is given by
   *
   *   d(x, y) = inf { | x - C_x(t) | + | y - C_y(t) | : 0 <= t <= 1 }
   *
   * the function f(t) = | x - C_x(t) | + | y - C_y(t) | is not C1
   * everywhere. However we can still compute its minimum by noting
   * that it is C1 outside of where x = C_x(t) and y = C_y(t) and
   * thus the set of points to check where the minumum occurs
   * is given by the following list:
   *   1) those t for which x = C_x(t)
   *   2) those t for which y = C_y(t)
   *   3) those t for which dC_x/dt + dC_y/dt = 0
   *   4) those t for which dC_x/dt - dC_y/dt = 0
   *   5) t = 0 or t = 1
   *
   * Note that the actual number of polynomial solves needed is
   * then just count.x (for 1) plus count.y (for 2). The items
   * from (3) and (4) are already stored in IntBezierCurve
   */
  compute_outline_point_values(step, count, tr, radius, dst);
  compute_derivative_cancel_values(step, count, tr, radius, dst);
  compute_fixed_line_values(step, count, tr, dst);
}

void
DistanceFieldGenerator::
compute_outline_point_values(const ivec2 &step, const ivec2 &count,
                             const IntBezierCurve::transformation<int> &tr,
                             int radius, fastuidraw::array2d<distance_value> &dst) const
{
  for(const IntContour &contour: m_contours)
    {
      const std::vector<IntBezierCurve> &curves(contour.curves());
      for(const IntBezierCurve &curve : curves)
        {
          record_distance_value_from_canidate(tr(curve.control_pts().front()),
                                              radius, step, count, dst);
        }
    }
}

void
DistanceFieldGenerator::
compute_derivative_cancel_values(const ivec2 &step, const ivec2 &count,
                                 const IntBezierCurve::transformation<int> &tr,
                                 int radius,
                                 fastuidraw::array2d<distance_value> &dst) const
{
  IntBezierCurve::transformation<float> ftr(tr.cast<float>());
  for(const IntContour &contour: m_contours)
    {
      const std::vector<IntBezierCurve> &curves(contour.curves());
      for(const IntBezierCurve &curve : curves)
        {
          fastuidraw::c_array<const vec2> derivatives_cancel(curve.derivatives_cancel());
          for(const vec2 &p : derivatives_cancel)
            {
              record_distance_value_from_canidate(ftr(p), radius, step, count, dst);
            }
        }
    }
}

void
DistanceFieldGenerator::
compute_fixed_line_values(const ivec2 &step, const ivec2 &count,
                          const IntBezierCurve::transformation<int> &tr,
                          fastuidraw::array2d<distance_value> &dst) const
{
  std::vector<std::vector<Solver::solution_pt> > work_room0;
  std::vector<std::vector<Solver::solution_pt> > work_room1;

  compute_fixed_line_values(Solver::x_fixed, work_room0, step, count, tr, dst);
  compute_fixed_line_values(Solver::y_fixed, work_room1, step, count, tr, dst);
}


void
DistanceFieldGenerator::
compute_fixed_line_values(enum Solver::coordinate_type tp,
                          std::vector<std::vector<Solver::solution_pt> > &work_room,
                          const ivec2 &step, const ivec2 &count,
                          const IntBezierCurve::transformation<int> &tr,
                          fastuidraw::array2d<distance_value> &dst) const
{
  const enum distance_value::winding_ray_t ray_types[2][2] =
    {
      {distance_value::from_pt_to_y_negative_infinity, distance_value::from_pt_to_y_positive_infinity}, //fixed-coordinate 0
      {distance_value::from_pt_to_x_negative_infinity, distance_value::from_pt_to_x_positive_infinity}, //fixed-coordinate 1
    };

  const int fixed_coord(Solver::fixed_coordinate(tp));
  const int varying_coord(Solver::varying_coordinate(tp));
  const int winding_sgn((tp == Solver::x_fixed) ? 1 : -1);

  work_room.resize(count[fixed_coord]);
  for(int i = 0; i < count[fixed_coord]; ++i)
    {
      work_room[i].clear();
    }

  /* record the solutions for each fixed line */
  for(const IntContour &contour: m_contours)
    {
      const std::vector<IntBezierCurve> &curves(contour.curves());
      for(const IntBezierCurve &curve : curves)
        {
          Solver(curve).compute_lines_intersection(tp, step[fixed_coord],
                                                   count[fixed_coord],
                                                   Solver::within_0_1,
                                                   tr, &work_room);
        }
    }

  /* now for each line, do the distance computation along the line. */
  for(int c = 0; c < count[fixed_coord]; ++c)
    {
      std::vector<Solver::solution_pt> &L(work_room[c]);
      int total_cnt(0), winding(0);

      /* sort by the value in the varying coordinate
       */
      std::sort(L.begin(), L.end(), Solver::CompareSolutions(varying_coord));
      for(const Solver::solution_pt &S : L)
        {
          FASTUIDRAWassert(S.m_multiplicity > 0);
          FASTUIDRAWassert(S.m_type != Solver::on_1_boundary);
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

              if (L[current_idx].m_p_t[fixed_coord] > 0.0f)
                {
                  winding += 1;
                }
              else if (L[current_idx].m_p_t[fixed_coord] < 0.0f)
                {
                  winding -= 1;
                }
              ++current_idx;
            }

          /* update the distance values for all those points between
           * the point on the line we were at before the loop start
           * and the point on the line we are at now
           */
          for(int idx = fastuidraw::t_max(0, prev_idx - 1),
                end_idx = fastuidraw::t_min(sz, current_idx + 1);
              idx < end_idx; ++idx)
            {
              float f;
              f = fastuidraw::t_abs(p - L[idx].m_p[varying_coord]);
              dst(pixel.x(), pixel.y()).record_distance_value(f);
            }

          /* update the ray-intersection counts */
          dst(pixel.x(), pixel.y()).increment_ray_intersection_count(ray_types[fixed_coord][0], current_cnt);
          dst(pixel.x(), pixel.y()).increment_ray_intersection_count(ray_types[fixed_coord][1], total_cnt - current_cnt);

          /* set winding number */
          dst(pixel.x(), pixel.y()).set_winding_number(tp, winding_sgn * winding);
        }

    }
}

//////////////////////////////////////////////
// fastuidraw::detail::IntBezierCurve methods
fastuidraw::vec2
fastuidraw::detail::IntBezierCurve::
eval(float t) const
{
  vec2 R(0.0f, 0.0f);
  vecN<c_array<const int>, 2> poly(as_polynomial());

  for(int c = 0; c < 2; ++c)
    {
      float pow_t(1.0f);
      for(int v : poly[c])
        {
          R[c] += pow_t * static_cast<float>(v);
          pow_t *= t;
        }
    }
  return R;
}

void
fastuidraw::detail::IntBezierCurve::
process_control_pts(void)
{
  FASTUIDRAWassert(m_num_control_pts >= 2);
  FASTUIDRAWassert(m_num_control_pts <= 4);

  /* check if is quadratic that should be collapsed to line
   */
  if (m_num_control_pts == 3)
    {
      ivec2 p1, p2, p2orig;
      p1 = m_control_pts[1] - m_control_pts[0];
      p2 = m_control_pts[2] - m_control_pts[0];
      p2orig = m_control_pts[2];

      if (p1.x() * p2.y() == p2.x() * p1.y())
        {
          m_control_pts[1] = p2orig;
          m_num_control_pts = 2;
        }
    }

  c_array<const ivec2> ct_pts(control_pts());
  vecN<c_array<int>, 2> poly_ptr(as_polynomial(0), as_polynomial(1));

  m_bb.union_points(ct_pts.begin(), ct_pts.end());
  generate_polynomial_from_bezier(ct_pts, poly_ptr);
  compute_derivatives_cancel_pts();
}

void
fastuidraw::detail::IntBezierCurve::
compute_derivatives_cancel_pts(void)
{
  if (degree() < 2)
    {
      m_num_derivatives_cancel = 0;
      return;
    }

  /* compute where dx/dt has same magnitude as dy/dt.
   */
  vecN<vecN<int, 3>, 2> deriv(vecN<int, 3>(0, 0, 0), vecN<int, 3>(0, 0, 0));
  vecN<int, 3> sum, difference;

  for(int coord = 0; coord < 2; ++coord)
    {
      for(int k = 0, end_k = m_num_control_pts - 1; k < end_k; ++k)
        {
          deriv[coord][k] = (k + 1) * m_as_polynomial_fcn[coord][k + 1];
        }
    }
  sum = deriv[0] + deriv[1];
  difference = deriv[0] - deriv[1];

  typedef vecN<Solver::poly_solution, 6> solution_holder_t;
  solution_holder_t solution_holder;
  Solver::poly_solutions<solution_holder_t::iterator> solutions(solution_holder.begin());

  Solver::solve_polynomial(c_array<const int>(sum.c_ptr(), m_num_control_pts),
                           Solver::within_0_1, &solutions);
  Solver::solve_polynomial(c_array<const int>(difference.c_ptr(), m_num_control_pts),
                           Solver::within_0_1, &solutions);
  solutions.finalize();
  FASTUIDRAWassert(solutions.size() <= m_derivatives_cancel.size());

  m_num_derivatives_cancel = solutions.size();
  for(unsigned int i = 0; i < m_num_derivatives_cancel; ++i)
    {
      m_derivatives_cancel[i] = eval(solution_holder[i].m_t);
    }
}


/////////////////////////////////////
// fastuidraw::detail::IntContour methods
void
fastuidraw::detail::IntContour::
replace_cubics_with_quadratics(void)
{
  IntBezierCurve::transformation<int> tr;
  replace_cubics_with_quadratics(tr, -1, -1, ivec2(1, 1));
}

void
fastuidraw::detail::IntContour::
replace_cubics_with_quadratics(const IntBezierCurve::transformation<int> &tr,
                               int thresh_4_quads, int thresh_2_quads,
                               ivec2 texel_size)
{
  /* Perform surgery on each IntCurve in this IntContour */
  if (m_curves.empty())
    {
      return;
    }

  IntBezierCurve::ID_t id;
  std::vector<IntBezierCurve> src;

  std::swap(m_curves, src);
  id.m_curveID = 0;
  id.m_contourID = src.front().ID().m_contourID;
  for(unsigned int i = 0, endi = src.size(); i < endi; ++i)
    {
      const IntBezierCurve &curve(src[i]);
      if (curve.degree() == 3)
        {
          fastuidraw::c_array<const fastuidraw::ivec2> pts(curve.control_pts());
          int l1_dist;
          fastuidraw::ivec2 t0, t1;

          t0 = tr(curve.control_pts().front()) / texel_size;
          t1 = tr(curve.control_pts().back())  / texel_size;
          l1_dist = (t0 - t1).L1norm();

          if (l1_dist > thresh_2_quads)
            {
              fastuidraw::vecN<fastuidraw::vecN<ivec2, 4>, 2> split;

              split = split_cubic(pts);
              if (l1_dist > thresh_4_quads)
                {
                  for (int j = 0; j < 2; ++j)
                    {
                      fastuidraw::vecN<fastuidraw::vecN<ivec2, 4>, 2> S;

                      S = split_cubic(split[j]);
                      for (int k = 0; k < 2; ++k)
                        {
                          fastuidraw::vecN<fastuidraw::ivec2, 3> q;

                          q = quadratic_from_cubic(S[k]);
                          m_curves.push_back(IntBezierCurve(id, q));
                          ++id.m_curveID;
                        }
                    }
                }
              else
                {
                  for (int j = 0; j < 2; ++j)
                    {
                      fastuidraw::vecN<fastuidraw::ivec2, 3> q;

                      q = quadratic_from_cubic(split[j]);
                      m_curves.push_back(IntBezierCurve(id, q));
                      ++id.m_curveID;
                    }
                }
            }
          else
            {
              m_curves.push_back(IntBezierCurve(id, quadratic_from_cubic(pts)));
              ++id.m_curveID;
            }
        }
      else
        {
          m_curves.push_back(IntBezierCurve(id, curve));
          ++id.m_curveID;
        }
    }
}

void
fastuidraw::detail::IntContour::
convert_flat_quadratics_to_lines(float thresh)
{
  for(IntBezierCurve &C : m_curves)
    {
      if (C.degree() == 2)
        {
          fastuidraw::c_array<const fastuidraw::ivec2> pts(C.control_pts());
          QuadraticBezierCurve Q(pts[0], pts[1], pts[2]);
          if (Q.compute_curvature() < thresh)
            {
              C = IntBezierCurve(C.ID(), pts[0], pts[2]);
            }
        }
    }
}

void
fastuidraw::detail::IntContour::
collapse_small_curves(const IntBezierCurve::transformation<int> &tr,
                      ivec2 texel_size)
{
  if (m_curves.empty())
    {
      return;
    }

  IntBezierCurve::ID_t id;
  id.m_curveID = 0;
  id.m_contourID = m_curves.front().ID().m_contourID;

  /* When a sequence of curves is collapsed, we collapse
   * that sequence of curves to a single point whose value
   * is the average of the end points of the curves
   * to remove; the tricky part if to correctly handle
   * the case where a curve collapse sequence starts
   * towards the end of the contour and ends at the
   * beginning (i.e. roll over).
   */
  std::vector<IntBezierCurve> src;
  std::vector<unsigned int> non_collapsed_curves;

  /* step 1: identify those curves that should NOT be collapsed */
  src.swap(m_curves);
  for(unsigned int i = 0, endi = src.size(); i < endi; ++i)
    {
      c_array<const ivec2> pts(src[i].control_pts());
      BoundingBox<int> bb;
      ivec2 p0, p1;

      bb.union_points(pts.begin(), pts.end());
      p0 = tr(bb.min_point()) / texel_size;
      p1 = tr(bb.max_point()) / texel_size;

      if (p0 != p1)
        {
          non_collapsed_curves.push_back(i);
        }
    }

  if (non_collapsed_curves.size() < 2)
    {
      /* entire contour collapsed, leave m_curves empty */
      return;
    }

  /* correctly handle if a sequence of curves are collapsed and
   * that sequence walks over the end-begin boundary (i.e. rollover)
   */
  if (non_collapsed_curves.front() != 0
     || non_collapsed_curves.back() != src.size() - 1)
    {
      /* collapse the sequence that rolls over */
      fastuidraw::ivec2 pt(src[non_collapsed_curves.back()].control_pts().back());
      int number(1);

      for(unsigned int k = non_collapsed_curves.back() + 1, endk = src.size(); k < endk; ++k, ++number)
        {
          pt += src[k].control_pts().back();
        }

      for(unsigned int k = 0; k < non_collapsed_curves.front(); ++k, ++number)
        {
          pt += src[k].control_pts().back();
        }

      pt /= number;
      src[non_collapsed_curves.back()].set_back_pt(pt);
      src[non_collapsed_curves.front()].set_front_pt(pt);
    }

  /* collapse the curves to points using the average as the new end point */
  for(unsigned int C = 0, endC = non_collapsed_curves.size(); C + 1 < endC; ++C)
    {
      unsigned int number(1);
      fastuidraw::ivec2 pt(src[non_collapsed_curves[C]].control_pts().back());

      for(unsigned int k = non_collapsed_curves[C] + 1; k < non_collapsed_curves[C + 1]; ++k, ++number)
        {
          pt += src[k].control_pts().back();
        }

      if (number > 1)
        {
          pt /= number;
          src[non_collapsed_curves[C]].set_back_pt(pt);
          src[non_collapsed_curves[C + 1]].set_front_pt(pt);
        }
    }

  /* now overwrite m_curves with the curves listed in non_collapsed_curves */
  FASTUIDRAWassert(m_curves.empty());
  for(unsigned int i = 0, endi = non_collapsed_curves.size(); i < endi; ++i, ++id.m_curveID)
    {
      unsigned int I(non_collapsed_curves[i]);
      m_curves.push_back(IntBezierCurve(id, src[I]));
    }
}

void
fastuidraw::detail::IntContour::
filter(float curvature_collapse,
       const IntBezierCurve::transformation<int> &tr,
       ivec2 texel_size)
{
  if (m_curves.empty())
    {
      return;
    }
  replace_cubics_with_quadratics(tr, 6, 4, texel_size);
  convert_flat_quadratics_to_lines(curvature_collapse);
  collapse_small_curves(tr, texel_size);
}

void
fastuidraw::detail::IntContour::
add_to_path(const IntBezierCurve::transformation<float> &tr, Path *dst) const
{
  if (m_curves.empty())
    {
      return;
    }

  for(const IntBezierCurve &curve : m_curves)
    {
      c_array<const ivec2> pts;

      pts = curve.control_pts();
      pts = pts.sub_array(0, pts.size() - 1);
      *dst << tr(vec2(pts.front()));
      pts = pts.sub_array(1);
      while(!pts.empty())
        {
          vec2 pt(tr(vec2(pts.front())));
          *dst << Path::control_point(pt);
          pts = pts.sub_array(1);
        }
    }
  *dst << Path::contour_close();
}

//////////////////////////////////////
// fastuidraw::detail::IntPath methods
void
fastuidraw::detail::IntPath::
add_to_path(const IntBezierCurve::transformation<float> &tr, Path *dst) const
{
  for(const IntContour &contour : m_contours)
    {
      contour.add_to_path(tr, dst);
    }
}

void
fastuidraw::detail::IntPath::
replace_cubics_with_quadratics(const IntBezierCurve::transformation<int> &tr,
                               int thresh_4_quads, int thresh_2_quads,
                               ivec2 texel_size)
{
  for(IntContour &contour : m_contours)
    {
      contour.replace_cubics_with_quadratics(tr, thresh_4_quads, thresh_2_quads, texel_size);
    }
}

void
fastuidraw::detail::IntPath::
replace_cubics_with_quadratics(void)
{
  for(IntContour &contour : m_contours)
    {
      contour.replace_cubics_with_quadratics();
    }
}

void
fastuidraw::detail::IntPath::
convert_flat_quadratics_to_lines(float thresh)
{
  for(IntContour &contour : m_contours)
    {
      contour.convert_flat_quadratics_to_lines(thresh);
    }
}

void
fastuidraw::detail::IntPath::
collapse_small_curves(const IntBezierCurve::transformation<int> &tr,
                      ivec2 texel_size)
{
  for(IntContour &contour : m_contours)
    {
      contour.collapse_small_curves(tr, texel_size);
    }
}

void
fastuidraw::detail::IntPath::
filter(float curvature_collapse,
       const IntBezierCurve::transformation<int> &tr,
       ivec2 texel_size)
{
  for(IntContour &contour : m_contours)
    {
      contour.filter(curvature_collapse, tr, texel_size);
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
  IntBezierCurve curve(computeID(), m_last_pt, pt);
  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
conic_to(const ivec2 &control_pt, const ivec2 &pt)
{
  IntBezierCurve curve(computeID(), m_last_pt, control_pt, pt);
  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
cubic_to(const ivec2 &control_pt0, const ivec2 &control_pt1, const ivec2 &pt)
{
  IntBezierCurve curve(computeID(), m_last_pt, control_pt0, control_pt1, pt);
  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
extract_render_data(const ivec2 &step, const ivec2 &image_sz,
                    float max_distance,
                    IntBezierCurve::transformation<int> tr,
                    const CustomFillRuleBase &fill_rule,
                    GlyphRenderDataTexels *dst) const
{
  DistanceFieldGenerator compute(m_contours);
  array2d<distance_value> dist_values(image_sz.x(), image_sz.y());
  int radius(2);

  /* change tr to be offset by half a texel, so that the
   * distance value is sampled at the center of the texel;
   * we also push it off by 1 more unit to guarnantee that
   * the sample points' x and y coordinates are different
   * from the x and y coordinates of all end points of the
   * curves of the path after transformation.
   */
  int tr_scale(tr.scale());
  ivec2 tr_translate(tr.translate() - step / 2 - ivec2(1, 1));
  tr = IntBezierCurve::transformation<int>(tr_scale, tr_translate);

  compute.compute_distance_values(step, image_sz, tr, radius, dist_values);

  dst->resize(image_sz);
  c_array<uint8_t> texel_data(dst->texel_data());

  std::fill(texel_data.begin(), texel_data.end(), 0);
  for(int y = 0; y < image_sz.y(); ++y)
    {
      for(int x = 0; x < image_sz.x(); ++x)
        {
          bool outside1, outside2;
          float dist;
          uint8_t v;
          int w1, w2;
          unsigned int location;

          w1 = dist_values(x, y).winding_number(Solver::x_fixed);
          w2 = dist_values(x, y).winding_number(Solver::y_fixed);

          outside1 = !fill_rule(w1);
          outside2 = !fill_rule(w2);

          dist = dist_values(x, y).distance(max_distance) / max_distance;
          if (outside1 != outside2)
            {
              /* if the fills do not match, then a curve is going through
               * the test point of the texel, thus make the distance 0
               */
              dist = 0.0f;
            }
          v = DistanceFieldGenerator::pixel_value_from_distance(dist, outside1);
          location = x + y * image_sz.x();
          texel_data[location] = v;
        }
    }
}
