#include <iterator>
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
                                  fastuidraw::vecN<fastuidraw::c_array<int>, 2> &curve)
  {
    fastuidraw::vecN<fastuidraw::ivec2, 4> q;

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

  inline
  uint8_t
  pixel_value_from_distance(float dist, bool outside)
  {
    uint8_t v;

    dist = fastuidraw::t_min(1.0f, fastuidraw::t_max(0.0f, dist));
    if(outside)
      {
        dist = -dist;
      }

    dist = (dist + 1.0f) * 0.5f;
    v = static_cast<uint8_t>(255.0f * dist);
    return v;
  }

  fastuidraw::GlyphRenderDataCurvePair::entry
  extract_entry(const fastuidraw::detail::IntBezierCurve::transformation<int> &tr,
                const fastuidraw::ivec2 &step,
                fastuidraw::const_c_array<fastuidraw::ivec2> c0,
                fastuidraw::const_c_array<fastuidraw::ivec2> c1)
  {
    unsigned int total_cnt(0);
    fastuidraw::vecN<fastuidraw::vec2, 5> pts;
    fastuidraw::vec2 dividier;

    FASTUIDRAWassert(c0.back() == c1.front());
    FASTUIDRAWassert(c0.size() == 2 || c0.size() == 3);
    FASTUIDRAWassert(c1.size() == 2 || c1.size() == 3);

    dividier = fastuidraw::vec2(1.0f) / fastuidraw::vec2(step);

    for(const fastuidraw::ivec2 &pt : c0)
      {
        pts[total_cnt] = dividier * fastuidraw::vec2(tr(pt));
        ++total_cnt;
      }

    c1 = c1.sub_array(1);
    for(const fastuidraw::ivec2 &pt : c1)
      {
        pts[total_cnt] = dividier * fastuidraw::vec2(tr(pt));
        ++total_cnt;
      }

    return fastuidraw::GlyphRenderDataCurvePair::entry(pts, c0.size());
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

  class CubicBezierCurve:public fastuidraw::vecN<fastuidraw::ivec2, 4>
  {
  public:
    CubicBezierCurve(const fastuidraw::ivec2 &p0,
                     const fastuidraw::ivec2 &c0,
                     const fastuidraw::ivec2 &c1,
                     const fastuidraw::ivec2 &p1):
      fastuidraw::vecN<fastuidraw::ivec2, 4>(p0, c0, c1, p1)
    {}

    void
    approximate_with_quadratics(fastuidraw::vecN<QuadraticBezierCurve, 4> *out_curves) const;

    void
    approximate_with_quadratics(fastuidraw::vecN<QuadraticBezierCurve, 2> *out_curves) const;

    void
    approximate_with_quadratics(fastuidraw::vecN<QuadraticBezierCurve, 1> *out_curves) const;
  };

  class BezierCurvePts
  {
  public:
    typedef fastuidraw::ivec2 point;

    BezierCurvePts(point p0, point p1):
      m_pts(p0, p1, point()),
      m_number_pts(2)
    {}

    BezierCurvePts(point p0, point c, point p1):
      m_pts(p0, c, p1),
      m_number_pts(3)
    {}

    BezierCurvePts(const QuadraticBezierCurve &Q):
      m_pts(Q),
      m_number_pts(3)
    {}

    BezierCurvePts(fastuidraw::const_c_array<point> pts):
      m_number_pts(pts.size())
    {
      FASTUIDRAWassert(m_pts.size() <= 3);
      std::copy(pts.begin(), pts.end(), m_pts.begin());
    }

    fastuidraw::c_array<point>
    control_pts(void)
    {
      return fastuidraw::c_array<point>(&m_pts[0], m_number_pts);
    }

    fastuidraw::const_c_array<point>
    control_pts(void) const
    {
      return fastuidraw::const_c_array<point>(&m_pts[0], m_number_pts);
    }

    /* Takes a list of fastuidraw::detail::IntContour::IntBezierCurve
       and produces a sequence of BezierCurvePts so that:
        1. Cubics are collapsed to quadratics
        2. Curves of low curvature are collapsed to lines
     */
    static
    void
    simplify(float curvature_collapse,
             const fastuidraw::detail::IntBezierCurve::transformation<int> &tr,
             const std::vector<fastuidraw::detail::IntBezierCurve> &curves,
             fastuidraw::ivec2 texel_size,
             std::vector<BezierCurvePts> *dst);

    /* Collapses those curves whose end points are within a texel
       and creates a list of those curves that are not collapsed
     */
    static
    void
    collapse(std::vector<BezierCurvePts> &src,
             const fastuidraw::detail::IntBezierCurve::transformation<int> &tr,
             fastuidraw::ivec2 texel_size,
             std::vector<unsigned int> *dst);

  private:
    fastuidraw::vecN<point, 3> m_pts;
    unsigned int m_number_pts;
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

  template<typename interator>
  class poly_solutions
  {
  public:
    explicit
    poly_solutions(interator iter):
      m_iter(iter),
      m_size(0),
      m_multiplicity_0_solution(0),
      m_multiplicity_1_solution(0)
    {}

    void
    add_solution_if_acceptable(uint32_t flags, float t, int mult = 1)
    {
      enum fastuidraw::detail::IntBezierCurve::solution_type tp;
      tp = (t < 1.0f && t > 0.0f) ?
        fastuidraw::detail::IntBezierCurve::within_0_1 :
        fastuidraw::detail::IntBezierCurve::outside_0_1;
      if(flags & tp)
        {
          poly_solution P;
          P.m_t = t;
          P.m_multiplicity = mult;
          P.m_type = tp;
          *m_iter++ = P;
          ++m_size;
        }
    }

    void
    add_0_solution(uint32_t flags)
    {
      if(flags & fastuidraw::detail::IntBezierCurve::on_0_boundary)
        {
          ++m_multiplicity_0_solution;
        }
    }

    void
    add_1_solution(uint32_t flags)
    {
      if(flags & fastuidraw::detail::IntBezierCurve::on_1_boundary)
        {
          ++m_multiplicity_1_solution;
        }
    }

    void
    finalize(void)
    {
      FASTUIDRAWassert(m_multiplicity_0_solution >= 0);
      FASTUIDRAWassert(m_multiplicity_1_solution >= 0);
      if(m_multiplicity_0_solution > 0)
        {
          poly_solution P;
          P.m_t = 0.0f;
          P.m_multiplicity = m_multiplicity_0_solution;
          P.m_type = fastuidraw::detail::IntBezierCurve::on_0_boundary;
          *m_iter++ = P;
        }

      if(m_multiplicity_1_solution > 1)
        {
          poly_solution P;
          P.m_t = 1.0f;
          P.m_multiplicity = m_multiplicity_1_solution;
          P.m_type = fastuidraw::detail::IntBezierCurve::on_1_boundary;
          *m_iter++ = P;
        }
      m_multiplicity_0_solution = -1;
      m_multiplicity_1_solution = -1;
    }

    bool
    finalized(void) const
    {
      FASTUIDRAWassert((m_multiplicity_0_solution == -1) == (m_multiplicity_1_solution == -1));
      return m_multiplicity_0_solution == -1;
    }

    unsigned int
    size(void) const
    {
      FASTUIDRAWassert(finalized());
      return m_size;
    }

  private:
    interator m_iter;
    unsigned int m_size;
    int m_multiplicity_0_solution;
    int m_multiplicity_1_solution;
  };

  template<typename T, typename iterator>
  void
  solve_linear(fastuidraw::const_c_array<T> poly,
               uint32_t accepted_solutions,
               poly_solutions<iterator> *solutions)
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

  template<typename T, typename iterator>
  void
  solve_quadratic(fastuidraw::const_c_array<T> poly,
                  uint32_t accepted_solutions,
                  poly_solutions<iterator> *solutions)
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

  template<typename T, typename iterator>
  void
  solve_cubic(fastuidraw::const_c_array<T> poly,
              uint32_t accepted_solutions,
              poly_solutions<iterator> *solutions)
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

  template<typename T, typename iterator>
  void
  solve_polynomial(fastuidraw::const_c_array<T> poly,
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

  template<typename T>
  void
  increment_p_and_p_t(const fastuidraw::vecN<fastuidraw::const_c_array<T>, 2> &curve,
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

  template<typename T, typename iterator, typename solutions_iterator>
  int
  compute_solution_points(fastuidraw::detail::IntBezierCurve::ID_t src,
                          const fastuidraw::vecN<fastuidraw::const_c_array<T>, 2> &curve,
                          solutions_iterator solutions_begin, solutions_iterator solutions_end,
                          const fastuidraw::detail::IntBezierCurve::transformation<float> &tr,
                          iterator out_pts)
  {
    int R(0);
    for(;solutions_begin != solutions_end; ++solutions_begin)
      {
        const poly_solution &S(*solutions_begin);
        fastuidraw::vec2 p(0.0f, 0.0f), p_t(0.0f, 0.0f);
        fastuidraw::detail::IntBezierCurve::solution_pt v;

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
}

//////////////////////////////////////
// CubicBezierCurve methods
void
CubicBezierCurve::
approximate_with_quadratics(fastuidraw::vecN<QuadraticBezierCurve, 4> *out_curves) const
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
  const CubicBezierCurve &in_curve(*this);
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
  (*out_curves)[0] = QuadraticBezierCurve(this_curve.p[0], pA, alpha.pMid);
  (*out_curves)[1] = QuadraticBezierCurve(alpha.pMid, pB, this_curve.pMid);
  (*out_curves)[2] = QuadraticBezierCurve(this_curve.pMid, pC, beta.pMid);
  (*out_curves)[3] = QuadraticBezierCurve(beta.pMid, pD, this_curve.p[3]);
}

void
CubicBezierCurve::
approximate_with_quadratics(fastuidraw::vecN<QuadraticBezierCurve, 2> *out_curves) const
{
  const CubicBezierCurve &in_curve(*this);
  CubicDecomposerHelper<int> this_curve(in_curve[0], in_curve[1], in_curve[2], in_curve[3]);

  (*out_curves)[0] = QuadraticBezierCurve(this_curve.p[0], this_curve.p[1], this_curve.pMid);
  (*out_curves)[1] = QuadraticBezierCurve(this_curve.pMid, this_curve.p[2], this_curve.p[3]);
}

void
CubicBezierCurve::
approximate_with_quadratics(fastuidraw::vecN<QuadraticBezierCurve, 1> *out_curves) const
{
  fastuidraw::ivec2 q;
  const CubicBezierCurve &in_curve(*this);

  q = compute_midpoint(in_curve[1], in_curve[2]);
  (*out_curves)[0] = QuadraticBezierCurve(in_curve[0], q, in_curve[3]);
}

//////////////////////////////////
// BezierCurvePts methods
void
BezierCurvePts::
simplify(float curvature_collapse,
         const fastuidraw::detail::IntBezierCurve::transformation<int> &tr,
         const std::vector<fastuidraw::detail::IntBezierCurve> &curves,
         fastuidraw::ivec2 texel_size,
         std::vector<BezierCurvePts> *dst)
{
  for(const fastuidraw::detail::IntBezierCurve &in_curve : curves)
    {
      if(in_curve.degree() == 3)
        {
          fastuidraw::const_c_array<fastuidraw::ivec2> pts(in_curve.control_pts());
          CubicBezierCurve cubic(pts[0], pts[1], pts[2], pts[3]);
          fastuidraw::ivec2 t0, t1;
          fastuidraw::vecN<QuadraticBezierCurve, 4> quads_4;
          fastuidraw::vecN<QuadraticBezierCurve, 2> quads_2;
          fastuidraw::vecN<QuadraticBezierCurve, 1> quads_1;
          fastuidraw::c_array<QuadraticBezierCurve> quads;
          int l1_dist;

          t0 = tr(in_curve.control_pts().front()) / texel_size;
          t1 = tr(in_curve.control_pts().back())  / texel_size;
          l1_dist = (t0 - t1).L1norm();

          if(l1_dist > 6)
            {
              quads = quads_4;
              cubic.approximate_with_quadratics(&quads_4);
            }
          else if(l1_dist > 4)
            {
              quads = quads_2;
              cubic.approximate_with_quadratics(&quads_2);
            }
          else
            {
              quads = quads_1;
              cubic.approximate_with_quadratics(&quads_1);
            }

          for(const QuadraticBezierCurve q : quads)
            {
              dst->push_back(q);
            }
        }
      else
        {
          bool collapse_to_line(false);
          if(in_curve.degree() == 2)
            {
              fastuidraw::const_c_array<fastuidraw::ivec2> pts(in_curve.control_pts());

              QuadraticBezierCurve Q(pts[0], pts[1], pts[2]);
              collapse_to_line = Q.compute_curvature() < curvature_collapse;
            }

          if(!collapse_to_line)
            {
              dst->push_back(BezierCurvePts(in_curve.control_pts()));
            }
          else
            {
              dst->push_back(BezierCurvePts(in_curve.control_pts().front(),
                                            in_curve.control_pts().back()));
            }
        }
    }
}

void
BezierCurvePts::
collapse(std::vector<BezierCurvePts> &src,
         const fastuidraw::detail::IntBezierCurve::transformation<int> &tr,
         fastuidraw::ivec2 texel_size,
         std::vector<unsigned int> *dst_ptr)
{
  std::vector<unsigned int> &non_collapsed_curves(*dst_ptr);

  for(unsigned int i = 0, endi = src.size(); i < endi; ++i)
    {
      const BezierCurvePts &in_curve(src[i]);
      fastuidraw::ivec2 p0, p1, t0, t1;

      p0 = in_curve.control_pts().front();
      p1 = in_curve.control_pts().back();
      t0 = tr(p0) / texel_size;
      t1 = tr(p1) / texel_size;
      if(t0 != t1)
        {
          non_collapsed_curves.push_back(i);
        }
    }

  if(non_collapsed_curves.size() < 2)
    {
      //entire contour collapsed!
      non_collapsed_curves.clear();
      return;
    }

  if(non_collapsed_curves.front() != 0
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
      src[non_collapsed_curves.back()].control_pts().back() = pt;
      src[non_collapsed_curves.front()].control_pts().front() = pt;
    }

  for(unsigned int C = 0, endC = non_collapsed_curves.size(); C + 1 < endC; ++C)
    {
      unsigned int number(1);
      fastuidraw::ivec2 pt(src[non_collapsed_curves[C]].control_pts().back());

      for(unsigned int k = non_collapsed_curves[C] + 1; k < non_collapsed_curves[C + 1]; ++k, ++number)
        {
          pt += src[k].control_pts().back();
        }

      if(number > 1)
        {
          pt /= number;
          src[non_collapsed_curves[C]].control_pts().back() = pt;
          src[non_collapsed_curves[C + 1]].control_pts().front() = pt;
        }
    }
}

/////////////////////////////////////
// fastuidraw::detail::IntBezierCurve methods
void
fastuidraw::detail::IntBezierCurve::
process_control_pts(void)
{
  FASTUIDRAWassert(m_num_control_pts >= 2);
  FASTUIDRAWassert(m_num_control_pts <= 4);

  /* check if is quadratic that should be collapsed to line
   */
  if(m_num_control_pts == 3)
    {
      ivec2 p1, p2, p2orig;
      p1 = m_control_pts[1] - m_control_pts[0];
      p2 = m_control_pts[2] - m_control_pts[0];
      p2orig = m_control_pts[2];

      if(p1.x() * p2.y() == p2.x() * p1.y())
        {
          m_control_pts[1] = p2orig;
          m_num_control_pts = 2;
        }
    }

  const_c_array<ivec2> ct_pts(control_pts());
  vecN<c_array<int>, 2> poly_ptr(as_polynomial(0), as_polynomial(1));

  m_bb.union_points(ct_pts.begin(), ct_pts.end());
  generate_polynomial_from_bezier(ct_pts, poly_ptr);
  compute_derivatives_cancel_pts();
}

void
fastuidraw::detail::IntBezierCurve::
compute_derivatives_cancel_pts(void)
{
  if(degree() < 2)
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
      for(int k = 0, end_k = m_num_control_pts; k < end_k; ++k)
        {
          deriv[coord][k] = (k + 1) * m_as_polynomial_fcn[coord][k + 1];
        }
    }
  sum = deriv[0] + deriv[1];
  difference = deriv[0] - deriv[1];

  vecN<poly_solution, 6> solution_holder;
  poly_solutions<vecN<poly_solution, 6>::iterator> solutions(solution_holder.begin());

  solve_polynomial(const_c_array<int>(sum.c_ptr(), m_num_control_pts), within_0_1, &solutions);
  solve_polynomial(const_c_array<int>(difference.c_ptr(), m_num_control_pts), within_0_1, &solutions);
  solutions.finalize();
  FASTUIDRAWassert(solutions.size() <= m_derivatives_cancel.size());
  m_num_derivatives_cancel = compute_solution_points(m_ID, as_polynomial(),
                                                     solution_holder.begin(), solution_holder.begin() + solutions.size(),
                                                     transformation<float>(),
                                                     m_derivatives_cancel.begin());
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
  const_c_array<int> poly(as_polynomial(coord));
  c_array<int64_t> tmp(work_room.c_ptr(), poly.size());

  std::vector<poly_solution> solution_holder;
  std::back_insert_iterator<std::vector<poly_solution> > solutions_iter(std::back_inserter(solution_holder));
  poly_solutions<std::back_insert_iterator<std::vector<poly_solution> > > solutions(solutions_iter);

  /* transform m_as_polynomial via transformation tr;
     that transformation is tr(p) = tr.translate() + tr.scale() * p,
     thus we multiply each coefficient of m_as_polynomial by
     tr.scale(), but add tr.translate() only to the constant term.
   */
  std::transform(poly.begin(), poly.end(), tmp.begin(), MultiplierFunctor<int>(tr.scale()));
  tmp[0] += tr.translate()[coord];

  /* solve for f(t) = pt, which is same as solving for
     f(t) - pt = 0.
   */
  tmp[0] -= pt;
  solve_polynomial(const_c_array<int64_t>(tmp), solution_types_accepted, &solutions);
  solutions.finalize();
  FASTUIDRAWassert(solutions.size() == solution_holder.size());

  /* compute solution point values from polynomial solutions */
  compute_solution_points(m_ID, as_polynomial(),
                          solution_holder.begin(), solution_holder.end(),
                          tr.cast<float>(),
                          std::back_inserter(*out_value));
}

/////////////////////////////////////
// fastuidraw::detail::IntContour methods
void
fastuidraw::detail::IntContour::
filter(float curvature_collapse,
       const IntBezierCurve::transformation<int> &tr,
       ivec2 texel_size)
{
  if(m_curves.empty())
    {
      return;
    }

  /* step 1: convert cubics to quadratics and
             convert tiny curvature curves to
             lines
   */
  std::vector<BezierCurvePts> tmp;
  BezierCurvePts::simplify(curvature_collapse, tr, m_curves, texel_size, &tmp);

  /* step 2, collapse curves within a texel.
      - When a sequence of curves is collapsed, we collapse
        that sequence of curves to a single point whose value
        is the average of the end points of the curves
        to remove; the tricky part if to correctly handle
        the case where a curve collapse sequence starts
        towards the end of the contour and ends at the
        beginning (i.e. roll over).
   */
  std::vector<unsigned int> non_collapsed_curves;
  BezierCurvePts::collapse(tmp, tr, texel_size, &non_collapsed_curves);

  /* now finally overwrite m_curves with the curves in tmp
     that are listed by non_collapsed_curves
   */
  IntBezierCurve::ID_t id;
  id.m_curveID = 0;
  id.m_contourID = m_curves.front().ID().m_contourID;

  m_curves.clear();
  for(unsigned int I : non_collapsed_curves)
    {
      const_c_array<ivec2> pts(tmp[I].control_pts());
      if(pts.size() == 2)
        {
          m_curves.push_back(IntBezierCurve(id, pts[0], pts[1]));
        }
      else
        {
          FASTUIDRAWassert(pts.size() == 3);
          m_curves.push_back(IntBezierCurve(id, pts[0], pts[1], pts[2]));
        }
      ++id.m_curveID;
    }

}

void
fastuidraw::detail::IntContour::
add_to_path(const IntBezierCurve::transformation<float> &tr, Path *dst) const
{
  if(m_curves.empty())
    {
      return;
    }

  for(const IntBezierCurve &curve : m_curves)
    {
      const_c_array<ivec2> pts;

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
  *dst << Path::contour_end();
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
  m_bb.union_box(curve.bounding_box());

  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
conic_to(const ivec2 &control_pt, const ivec2 &pt)
{
  IntBezierCurve curve(computeID(), m_last_pt, control_pt, pt);
  m_bb.union_box(curve.bounding_box());

  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
cubic_to(const ivec2 &control_pt0, const ivec2 &control_pt1, const ivec2 &pt)
{
  IntBezierCurve curve(computeID(), m_last_pt, control_pt0, control_pt1, pt);
  m_bb.union_box(curve.bounding_box());

  FASTUIDRAWassert(!m_contours.empty());
  m_contours.back().add_curve(curve);
  m_last_pt = pt;
}

void
fastuidraw::detail::IntPath::
extract_render_data(const ivec2 &step, const ivec2 &image_sz,
                    float max_distance,
                    const IntBezierCurve::transformation<int> &tr,
                    GlyphRenderDataDistanceField *dst) const
{
  array2d<distance_value> dist_values(image_sz.x(), image_sz.y());
  int radius(2);

  compute_distance_values(step, image_sz, tr, radius, dist_values);

  dst->resize(image_sz + ivec2(1, 1));
  std::fill(dst->distance_values().begin(), dst->distance_values().end(), 0);
  for(int y = 0; y < image_sz.y(); ++y)
    {
      for(int x = 0; x < image_sz.x(); ++x)
        {
          bool outside;
          float dist;
          uint8_t v;
          int w1, w2;
          unsigned int location;

          w1 = dist_values(x, y).winding_number(IntBezierCurve::x_fixed);
          w2 = dist_values(x, y).winding_number(IntBezierCurve::y_fixed);

          outside = (w1 == 0);
          dist = dist_values(x, y).distance(max_distance) / max_distance;
          if(w1 != w2)
            {
              /* if the windings do not match, then a curve is going through
                 the test point of the texel, thus make the distance 0
              */
              dist = 0.0f;
            }

          v = pixel_value_from_distance(dist, outside);
          location = x + y * (1 + image_sz.x());
          dst->distance_values()[location] = v;
        }
    }
}

void
fastuidraw::detail::IntPath::
extract_render_data(const ivec2 &step, const ivec2 &count,
                    const IntBezierCurve::transformation<int> &tr,
                    GlyphRenderDataCurvePair *dst) const
{
  /* create table to go from curve ID's to global indices */
  unsigned int curve_count(0);
  std::vector<unsigned int> cumulated_curve_counts;

  cumulated_curve_counts.reserve(m_contours.size());
  for(const IntContour &contour : m_contours)
    {
      cumulated_curve_counts.push_back(curve_count);
      curve_count += contour.curves().size();
    }

  /* extract curve data into the GlyphRenderDataCurvePair geometry data*/
  dst->resize_geometry_data(curve_count);
  c_array<GlyphRenderDataCurvePair::entry> dst_entries(dst->geometry_data());

  for(const IntContour &contour : m_contours)
    {
      const std::vector<IntBezierCurve> &curves(contour.curves());
      for(const IntBezierCurve &c : curves)
        {
          IntBezierCurve::ID_t ID, next_ID;
          unsigned int idx;
          const_c_array<ivec2> src_pts;

          ID = c.ID();
          next_ID = next_neighbor(ID);
          idx = cumulated_curve_counts[ID.m_contourID] + ID.m_curveID;

          dst_entries[idx] = extract_entry(tr, step, c.control_pts(),
                                           curve(next_ID).control_pts());
        }
    }

  /* TODO: figure out which curve-pair to use for each index */
  dst->resize_active_curve_pair(count + ivec2(1, 1));


  std::fill(dst->active_curve_pair().begin(),
            dst->active_curve_pair().end(),
            GlyphRenderDataCurvePair::completely_empty_texel);
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
      const std::vector<IntBezierCurve> &curves(contour.curves());
      for(const IntBezierCurve &curve : curves)
        {
          record_distance_value_from_canidate(tr(curve.control_pts().front()),
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
      const std::vector<IntBezierCurve> &curves(contour.curves());
      for(const IntBezierCurve &curve : curves)
        {
          const_c_array<IntBezierCurve::solution_pt> derivatives_cancel(curve.derivatives_cancel());
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
      const std::vector<IntBezierCurve> &curves(contour.curves());
      for(const IntBezierCurve &curve : curves)
        {
          BoundingBox<int> bb;
          int cstart, cend, bbmin, bbmax;

          bb = curve.bounding_box(tr);

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
              curve.compute_line_intersection(v, tp, &work_room[c], IntBezierCurve::within_0_1, tr);
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
