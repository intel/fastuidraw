#pragma once

#include <iostream>
#include <cmath>

#include <cairo.h>

template<typename T>
class Tvec2
{
public:
  T m_x, m_y;

  Tvec2(void):
    m_x(0.0), m_y(0.0)
  {}

  Tvec2(T px, T py):
    m_x(px),
    m_y(py)
  {}

  template<typename S>
  explicit
  Tvec2(const Tvec2<S> &obj):
    m_x(obj.m_x),
    m_y(obj.m_y)
  {}

  T
  x(void) const { return m_x; }

  T
  y(void) const { return m_y; }

  T&
  x(void) { return m_x; }

  T&
  y(void) { return m_y; }

  T&
  operator[](int i)
  {
    return (i == 0) ? m_x : m_y;
  }

  const T&
  operator[](int i) const
  {
    return (i == 0) ? m_x : m_y;
  }

  Tvec2
  operator+(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x += rhs.m_x;
    v.m_y += rhs.m_y;
    return v;
  }

  Tvec2
  operator-(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x -= rhs.m_x;
    v.m_y -= rhs.m_y;
    return v;
  }

  Tvec2
  operator*(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x *= rhs.m_x;
    v.m_y *= rhs.m_y;
    return v;
  }

  Tvec2
  operator/(const Tvec2 &rhs) const
  {
    Tvec2 v(*this);
    v.m_x /= rhs.m_x;
    v.m_y /= rhs.m_y;
    return v;
  }

  Tvec2
  operator*(T s) const
  {
    Tvec2 v(*this);
    v.m_x *= s;
    v.m_y *= s;
    return v;
  }

  Tvec2
  operator/(T s) const
  {
    Tvec2 v(*this);
    v.m_x /= s;
    v.m_y /= s;
    return v;
  }
};


typedef Tvec2<double> vec2;
typedef Tvec2<int> ivec2;

template<typename T>
std::ostream&
operator<<(std::ostream &ostr, const Tvec2<T> &obj)
{
  ostr << "(" << obj.x() << ", " << obj.y() << ")";
  return ostr;
}

inline
vec2
operator*(const cairo_matrix_t &lhs, const vec2 &rhs)
{
  vec2 r(rhs);
  cairo_matrix_transform_point(&lhs, &r.m_x, &r.m_y);
  return r;
}

inline
cairo_matrix_t
operator*(const cairo_matrix_t &lhs, const cairo_matrix_t &rhs)
{
  cairo_matrix_t R;
  cairo_matrix_multiply(&R, &lhs, &rhs);
  return R;
}

template<typename T>
inline
void
cairo_matrix_translate(cairo_matrix_t *m, const Tvec2<T> &t)
{
  cairo_matrix_translate(m, t.x(), t.y());
}

template<typename T>
inline
void
cairo_translate(cairo_t *m, const Tvec2<T> &t)
{
  cairo_translate(m, t.x(), t.y());
}

inline
void
cairo_bezier_to(cairo_t *cr,
                double x1, double y1,
                double x2, double y2)
{
  double x0, y0;
  cairo_get_current_point (cr, &x0, &y0);
  cairo_curve_to(cr,
                 2.0 / 3.0 * x1 + 1.0 / 3.0 * x0,
                 2.0 / 3.0 * y1 + 1.0 / 3.0 * y0,
                 2.0 / 3.0 * x1 + 1.0 / 3.0 * x2,
                 2.0 / 3.0 * y1 + 1.0 / 3.0 * y2,
                 y1, y2);
}

inline
void
cairo_arc_to(cairo_t *cr,
             double angle,
             double x1, double y1)
{
  double angle_coeff_dir;
  vec2 end_start, mid, n;
  double s, c, t, start_angle, end_angle, radius;
  vec2 start_pt, end_pt, circle_center;

  cairo_get_current_point (cr, &start_pt.x(), &start_pt.y());
  end_pt = vec2(x1, y1);

  angle_coeff_dir = (angle > 0.0) ? 1.0 : -1.0;
  angle = angle_coeff_dir * angle;
  end_start = end_pt - start_pt;
  mid = (end_pt + start_pt) * 0.5;
  n = vec2(-end_start.y(), end_start.x());
  s = std::sin(angle * 0.5);
  c = std::cos(angle * 0.5);
  t = angle_coeff_dir * 0.5 * c / s;

  circle_center = mid + (n * t);
  vec2 start_center(start_pt - circle_center);

  radius = std::sqrt(start_center.x() * start_center.x() + start_center.y() * start_center.y());
  start_angle = std::atan2(start_center.y(), start_center.x());
  end_angle = start_angle + angle_coeff_dir * angle;

  if(start_angle > end_angle)
    {
      cairo_arc_negative(cr, circle_center.x(), circle_center.y(), radius, start_angle, end_angle);
    }
  else
    {
      cairo_arc(cr, circle_center.x(), circle_center.y(), radius, start_angle, end_angle);
    }
}

inline
void
cairo_arc_degrees_to(cairo_t *cr,
                     double angle,
                     double x1, double y1)
{
  cairo_arc_to(cr, angle * M_PI / 180.0, x1, y1);
}
