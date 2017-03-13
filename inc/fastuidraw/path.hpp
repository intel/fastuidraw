/*!
 * \file path.hpp
 * \brief file path.hpp
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



#pragma once

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/tessellated_path.hpp>

namespace fastuidraw  {

/*!\addtogroup Core
  @{
 */

/*!
  An PathContour represents a single contour within
  a Path. Ending a contour, see \ref end(), \ref
  end_generic() and end_arc(), means to specify
  the edge from the last point of the PathContour
  to the first point.
 */
class PathContour:
    public reference_counted<PathContour>::non_concurrent
{
public:

  /*!
    Base class to describe how to interpolate from one
    point of a PathContour to the next, i.e. describes
    the shape of an edge.
   */
  class interpolator_base:
    public reference_counted<interpolator_base>::non_concurrent
  {
  public:
    /*!
      Ctor.
      \param prev interpolator to edge that ends at start of edge
                  of this interpolator
      \param end end point of the edge of this interpolator
     */
    interpolator_base(const reference_counted_ptr<const interpolator_base> &prev,
                      const vec2 &end);

    virtual
    ~interpolator_base();

    /*!
      Returns the interpolator previous to this interpolator_base
      within the PathContour that this object resides.
     */
    reference_counted_ptr<const interpolator_base>
    prev_interpolator(void) const;

    /*!
      Returns the starting point of this interpolator.
     */
    const vec2&
    start_pt(void) const;

    /*!
      Returns the ending point of this interpolator
     */
    const vec2&
    end_pt(void) const;

    /*!
      To be implemented by a derived class to return true if
      the interpolator is flat, i.e. is just a line segment
      connecting start_pt() to end_pt().
     */
    virtual
    bool
    is_flat(void) const = 0;

    /*!
      To be implemented by a derived class to produce the tessellation
      from start_pt() to end_pt(). The routine must include BOTH start_pt()
      and end_pt() in the result. Only the fields TessellatedPath::point::m_p,
      TessellatedPath::point::m_p_t, TessellatedPath::point::m_distance_from_edge_start
      are to be filled; the other fields of TessellatedPath::point are
      filled by TessellatedPath using the named fields. In addition to
      filling the output array, the function shall return the number of
      points needed to perform the required tessellation.

      \param tess_params tessellation parameters
      \param out_data location to which to write the edge tessellated
      \param out_effective_curve_distance (output) location to which to write the
                                                   largest distance between sub-edges
                                                   and the actual curve.
      \param out_effective_curvature (output) location to which to write the largest
                                              cumalative curvature of a sub-edge of
                                              the created tessellation.
     */
    virtual
    unsigned int
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         c_array<TessellatedPath::point> out_data,
                         float *out_effective_curve_distance,
                         float *out_effective_curvature) const = 0;

    /*!
      To be implemented by a derived class to return a fast (and approximate)
      bounding box for the interpolator.
      \param out_min_bb (output) location to which to write the min-x and min-y
                                 coordinates of the approximate bounding box.
      \param out_max_bb (output) location to which to write the max-x and max-y
                                 coordinates of the approximate bounding box.
     */
    virtual
    void
    approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const = 0;

    /*!
      To be implemented by a derived class to create and
      return a deep copy of the interpolator object.
     */
    virtual
    interpolator_base*
    deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const = 0;

  private:
    friend class PathContour;
    void *m_d;
  };

  /*!
    A flat interpolator represents a flat edge.
   */
  class flat:public interpolator_base
  {
  public:
    /*!
      Ctor.
      \param prev interpolator to edge that ends at start of edge
                  of this interpolator
      \param end end point of the edge of this interpolator
     */
    flat(const reference_counted_ptr<const interpolator_base> &prev,
         const vec2 &end):
      interpolator_base(prev, end)
    {}

    virtual
    bool
    is_flat(void) const;

    virtual
    unsigned int
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         c_array<TessellatedPath::point> out_data,
                         float *out_effective_curve_distance,
                         float *out_effective_curvature) const;
    virtual
    void
    approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const;

    virtual
    interpolator_base*
    deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const;
  };

  /*!
    Interpolator generic implements tessellation by recursion
    and relying on analytic derivative provided by derived
    class.
   */
  class interpolator_generic:public interpolator_base
  {
  public:
    /*!
      A tessellated_region is a base class for a cookie
      used and generated by tessellate().
     */
    class tessellated_region
    {
    public:
      virtual
      ~tessellated_region()
      {}
    };

    /*!
      Ctor.
      \param prev interpolator to edge that ends at start of edge
                  of this interpolator
      \param end end point of the edge of this interpolator
     */
    interpolator_generic(const reference_counted_ptr<const interpolator_base> &prev,
                         const vec2 &end):
      interpolator_base(prev, end)
    {}

    virtual
    unsigned int
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         c_array<TessellatedPath::point> out_data,
                         float *out_effective_curve_distance,
                         float *out_effective_curvature) const;

    /*!
      To be implemented by a derived class to compute datum of the curve
      at a time t (0 <= t <= 1). This is used to tessellate according to
      curvature.
      \param in_t (input) argument fed to parameterization of which this
                  interpolator_base represents with in_t = 0.0
                  indicating the start of the curve and in_t = 1.0
                  the end of the curve
      \param outp (output) if non-NULL, location to which to write the position value
      \param outp_t (output) if non-NULL, location to which to write the first derivative value
      \param outp_tt (output) if non-NULL, location to which to write the second derivative value
     */
    virtual
    void
    compute(float in_t, vec2 *outp, vec2 *outp_t, vec2 *outp_tt) const = 0;

    /*!
      To be implemented by a derived to assist in recursive tessellation.
      \param in_region region to divide in half
      \param out_regionA location to which to write the first half
      \param out_regionB location to which to write the second half
      \param out_t location to which to write the time (parameter) for
                   in the middle of in_region
      \param out_p location to which to write the position of the point
                   on the curve in the middle of in_region
      \param out_p_t location to which to write the derivative at the curve
                     at the middle of in_region
      \param out_p_tt location to which to write the second derivative at the
                      curve at the middle of in_region
      \param out_effective_curve_distance location to which to write the distance
                                          between the actual curve and the line segment
                                          represented by in_region
     */
    virtual
    void
    tessellate(tessellated_region *in_region,
               tessellated_region **out_regionA, tessellated_region **out_regionB,
               float *out_t, vec2 *out_p, vec2 *out_p_t, vec2 *out_p_tt,
               float *out_effective_curve_distance) const = 0;
  };

  /*!
    Derived class of interpolator_base to indicate a Bezier curve.
    Supports Bezier curves of _any_ degree.
   */
  class bezier:public interpolator_generic
  {
  public:
    /*!
      Ctor. One control point, thus interpolation is a quadratic curve.
      \param start start of curve
      \param ct control point
      \param end end of curve
     */
    bezier(const reference_counted_ptr<const interpolator_base> &start,
           const vec2 &ct, const vec2 &end);

    /*!
      Ctor. Two control points, thus interpolation is a cubic curve.
      \param start start of curve
      \param ct1 1st control point
      \param ct2 2nd control point
      \param end end point of curve
     */
    bezier(const reference_counted_ptr<const interpolator_base> &start,
           const vec2 &ct1, const vec2 &ct2, const vec2 &end);

    /*!
      Ctor. Iterator range defines the control points of the bezier curve.
      \param start start of curve
      \param control_pts control points
      \param end end point of curve
     */
    bezier(const reference_counted_ptr<const interpolator_base> &start,
           const_c_array<vec2> control_pts, const vec2 &end);

    virtual
    ~bezier();

    virtual
    bool
    is_flat(void) const;

    virtual
    void
    compute(float in_t, vec2 *outp, vec2 *outp_t, vec2 *outp_tt) const;

    virtual
    void
    tessellate(tessellated_region *in_region,
               tessellated_region **out_regionA, tessellated_region **out_regionB,
               float *out_t, vec2 *out_p, vec2 *out_p_t, vec2 *out_p_tt,
               float *out_effective_curve_distance) const;

    virtual
    void
    approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const;

    virtual
    interpolator_base*
    deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const;

  private:
    bezier(const bezier &q,
           const reference_counted_ptr<const interpolator_base> &prev);

    void *m_d;
  };

  /*!
    An arc is for connecting one point to the next via an
    arc of a circle.
   */
  class arc:public interpolator_base
  {
  public:
    /*!
      Ctor.
      \param start start of curve
      \param angle The angle of the arc in radians, the value must not
                   be a multiple of 2*M_PI. Assuming a coordinate system
                   where y-increases vertically and x-increases to the right,
                   a positive value indicates to have the arc go counter-clockwise,
                   a negative angle for the arc to go clockwise.
      \param end end of curve
     */
    arc(const reference_counted_ptr<const interpolator_base> &start,
        float angle, const vec2 &end);

    ~arc();

    virtual
    bool
    is_flat(void) const;

    virtual
    void
    approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const;

    virtual
    interpolator_base*
    deep_copy(const reference_counted_ptr<const interpolator_base> &prev) const;

    virtual
    unsigned int
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         c_array<TessellatedPath::point> out_data,
                         float *out_effective_curve_distance,
                         float *out_effective_curvature) const;

  private:
    arc(const arc &q, const reference_counted_ptr<const interpolator_base> &prev);

    void *m_d;
  };

  /*!
    Ctor.
   */
  explicit
  PathContour(void);

  ~PathContour();

  /*!
    Start the PathContour, may only be called once in the lifetime
    of a PathContour() and must be called before adding points
    (to_point()), adding control points (add_control_point()),
    adding arcs (to_arc()), adding generic interpolator (
    to_generic()) or ending the contour (end(), end_generic()).
   */
  void
  start(const vec2 &pt);

  /*!
    End the current edge.
    \param pt point location of end of edge (and thus start of new edge)
   */
  void
  to_point(const vec2 &pt);

  /*!
    Add a control point. Will fail if end() was called
    \param pt location of new control point
   */
  void
  add_control_point(const vec2 &pt);

  /*!
    Will fail if end() was called of if add_control_point() has been
    called more recently than to_point() or if interpolator_base::prev_interpolator()
    of the passed interpolator_base is not the same as prev_interpolator().
    \param p interpolator describing edge
   */
  void
  to_generic(const reference_counted_ptr<const interpolator_base> &p);

  /*!
    Will fail if end() was called of if add_control_point() has been
    called more recently than to_point().
    \param angle angle of arc in radians
    \param pt point where arc ends (and next edge starts)
   */
  void
  to_arc(float angle, const vec2 &pt);

  /*!
    Ends with the passed interpolator_base. The interpolator
    must interpolate to the start point of the PathContour
    \param h interpolator describing the closing edge
   */
  void
  end_generic(reference_counted_ptr<const interpolator_base> h);

  /*!
    Ends with the Bezier curve defined by the current
    control points added by add_control_point().
   */
  void
  end(void);

  /*!
    Ends with an arc.
    \param angle angle of arc in radians
   */
  void
  end_arc(float angle);

  /*!
    Returns the last interpolator added to this PathContour.
    You MUST use this interpolator in the ctor of
    interpolator_base for interpolator passed to
    to_generic() and end_generic().
   */
  const reference_counted_ptr<const interpolator_base>&
  prev_interpolator(void);

  /*!
    Returns true if the PathContour has ended
   */
  bool
  ended(void) const;

  /*!
    Return the I'th point of this PathContour.
    For I = 0, returns the value passed to start().
    \param I index of point.
   */
  const vec2&
  point(unsigned int I) const;

  /*!
    Returns the number of points of this PathContour.
   */
  unsigned int
  number_points(void) const;

  /*!
    Returns the interpolator of this PathContour
    that interpolates from the I'th point to the
    (I+1)'th point. If I == number_points() - 1,
    then returns the interpolator from the last
    point to the first point.
   */
  const reference_counted_ptr<const interpolator_base>&
  interpolator(unsigned int I) const;

  /*!
    Returns an approximation of the bounding box for
    this PathContour WITHOUT relying on tessellating
    the \ref interpolator_base objects of this \ref
    PathContour IF this PathCountour is ended().
    Returns true if this is ended() and writes the
    values, otherwise returns false and does not write
    any value.
    \param out_min_bb (output) location to which to write the min-x and min-y
                               coordinates of the approximate bounding box.
    \param out_max_bb (output) location to which to write the max-x and max-y
                               coordinates of the approximate bounding box.
   */
  bool
  approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const;

  /*!
    Returns true if each interpolator of the PathContour is
    flat.
   */
  bool
  is_flat(void) const;

  /*!
    Create a deep copy of this PathContour.
   */
  PathContour*
  deep_copy(void);

private:
  void *m_d;
};

/*!
  A Path represents a collection of PathContour
  objects. To end a contour in a Path, see
  \ref end_contour_quadratic(), \ref
  end_contour_arc(), \ref end_contour_cubic(),
  \ref end_contour_custom(), \ref contour_end_arc
  and \ref contour_end, means to specify
  the edge from the last point of the current
  contour to the first point of it.
 */
class Path
{
public:
  /*!
    Class that wraps a vec2 to mark a point
    as a control point for a Bezier curve
   */
  class control_point
  {
  public:
    /*!
      Ctor
      \param pt value to which to set m_location
     */
    explicit
    control_point(const vec2 &pt):
      m_location(pt)
    {}

    /*!
      Ctor
      \param x value to which to set m_location.x()
      \param y value to which to set m_location.y()
     */
    control_point(float x, float y):
      m_location(x,y)
    {}

    /*!
      Position of control point
     */
    vec2 m_location;
  };

  /*!
    Wraps the data to specify an arc
   */
  class arc
  {
  public:
    /*!
      Ctor
      \param angle angle of arc in radians
      \param pt point to which to arc
     */
    arc(float angle, const vec2 &pt):
      m_angle(angle), m_pt(pt)
    {}

    /*!
      Angle of arc in radians
     */
    float m_angle;

    /*!
      End point of arc
     */
    vec2 m_pt;
  };

  /*!
    Tag class to mark the end of an contour
   */
  class contour_end
  {};

  /*!
    Tag class to mark the end of an contour with an arc
   */
  class contour_end_arc
  {
  public:
    /*!
      Ctor
      \param angle angle of arc in radians
     */
    explicit
    contour_end_arc(float angle):
      m_angle(angle)
    {}

    /*!
      Angle of arc in radians
     */
    float m_angle;
  };

  /*!
    Ctor.
   */
  explicit
  Path(void);

  /*!
    Copy ctor.
    \param obj Path from which to copy path data
   */
  Path(const Path &obj);

  ~Path();

  /*!
    Assignment operator
    \param rhs value from which to assign.
   */
  const Path&
  operator=(const Path &rhs);

  /*!
    Clear the path, i.e. remove all PathContour's from the
    path
   */
  void
  clear(void);

  /*!
    Swap contents of Path with another Path
    \param obj Path with which to swap
   */
  void
  swap(Path &obj);

  /*!
    Create an arc but specify the angle in degrees.
    \param angle angle of arc in degrees
    \param pt point to which to arc
   */
  static
  arc
  arc_degrees(float angle, const vec2 &pt)
  {
    return arc(angle*float(M_PI)/180.0f, pt);
  }

  /*!
    Create an contour_end_arc but specify the angle in degrees.
    \param angle angle or arc in degrees
   */
  static
  contour_end_arc
  contour_end_arc_degrees(float angle)
  {
    return contour_end_arc(angle*float(M_PI)/180.0f);
  }

  /*!
    Operator overload to add a point of the current
    contour in the Path.
    \param pt point to add
   */
  Path&
  operator<<(const vec2 &pt);

  /*!
    Operator overload to add a control point of the current
    contour in the Path.
    \param pt control point to add
   */
  Path&
  operator<<(const control_point &pt);

  /*!
    Operator overload to add an arc to the current contour
    in the Path.
    \param a arc to add
   */
  Path&
  operator<<(const arc &a);

  /*!
    Operator overload to end the current contour
   */
  Path&
  operator<<(contour_end);

  /*!
    Operator overload to end the current contour
    \param a specifies the angle of the arc for closing
             the current contour
   */
  Path&
  operator<<(contour_end_arc a);

  /*!
    Append a line to the current contour.
    \param pt point to which the line goes
   */
  Path&
  line_to(const vec2 &pt);

  /*!
    Append a quadratic Bezier curve to the current contour.
    \param ct control point of the quadratic Bezier curve
    \param pt point to which the quadratic Bezier curve goes
   */
  Path&
  quadratic_to(const vec2 &ct, const vec2 &pt);

  /*!
    Append a cubic Bezier curve to the current contour.
    \param ct1 first control point of the cubic Bezier curve
    \param ct2 second control point of the cubic Bezier curve
    \param pt point to which the cubic Bezier curve goes
   */
  Path&
  cubic_to(const vec2 &ct1, const vec2 &ct2, const vec2 &pt);

  /*!
    Append an arc curve to the current contour.
    \param angle gives the angle of the arc in radians. For a coordinate system
                 where y increases upwards and x increases to the right, a positive
                 value indicates counter-clockwise and a negative value indicates
                 clockwise
    \param pt point to which the arc curve goes
   */
  Path&
  arc_to(float angle, const vec2 &pt);

  /*!
    Returns the last interpolator added to this the current
    contour of this Path. When creating custom
    interpolator to be added with custom_to(),
    You MUST use this interpolator in the ctor of
    interpolator_base.
   */
  const reference_counted_ptr<const PathContour::interpolator_base>&
  prev_interpolator(void);

  /*!
    Add a custom interpolator. Use prev_interpolator()
    to get the last interpolator of the current contour.
   */
  Path&
  custom_to(const reference_counted_ptr<const PathContour::interpolator_base> &p);

  /*!
    Begin a new contour
    \param pt point at which the contour begins
   */
  Path&
  move(const vec2 &pt);

  /*!
    End the current contour in an arc and begin a new contour
    \param angle gives the angle of the arc in radians. For a coordinate system
                 where y increases upwards and x increases to the right, a positive
                 value indicates counter-clockwise and a negative value indicates
                 clockwise
    \param pt point at which the contour begins
   */
  Path&
  arc_move(float angle, const vec2 &pt);

  /*!
    End the current contour in an arc
    \param angle gives the angle of the arc in radians. For a coordinate system
                 where y increases upwards and x increases to the right, a positive
                 value indicates counter-clockwise and a negative value indicates
                 clockwise
   */
  Path&
  end_contour_arc(float angle);

  /*!
    End the current contour in a quadratic Bezier curve and begin a new contour
    \param ct control point of the quadratic Bezier curve
    \param pt point at which the contour begins
   */
  Path&
  quadratic_move(const vec2 &ct, const vec2 &pt);

  /*!
    End the current contour in a quadratic Bezier curve
    \param ct control point of the quadratic Bezier curve
   */
  Path&
  end_contour_quadratic(const vec2 &ct);

  /*!
    End the current contour in a cubic Bezier curve and begin a new contour
    \param ct1 first control point of the cubic Bezier curve
    \param ct2 second control point of the cubic Bezier curve
    \param pt point at which the contour begins
   */
  Path&
  cubic_move(const vec2 &ct1, const vec2 &ct2, const vec2 &pt);

  /*!
    End the current contour in a cubic Bezier curve
    \param ct1 first control point of the cubic Bezier curve
    \param ct2 second control point of the cubic Bezier curve
   */
  Path&
  end_contour_cubic(const vec2 &ct1, const vec2 &ct2);

  /*!
    Use a custom interpolator to end the current contour and begin a new contour
    Use prev_interpolator() to get the last interpolator
    of the current contour.
    \param p custom interpolator
    \param pt point at which the contour begins
   */
  Path&
  custom_move(const reference_counted_ptr<const PathContour::interpolator_base> &p, const vec2 &pt);

  /*!
    Use a custom interpolator to end the current contour
    Use prev_interpolator() to get the last interpolator
    of the current contour.
    \param p custom interpolator
   */
  Path&
  end_contour_custom(const reference_counted_ptr<const PathContour::interpolator_base> &p);

  /*!
    Adds a PathContour to this Path. PathContour is only added
    if the contour is ended (i.e. PathContour::ended() returns
    true). A fixed, ended PathContour can be used by multiple
    Path's at the same time.
    \param contour PathContour to add to the Path
   */
  Path&
  add_contour(const reference_counted_ptr<const PathContour> &contour);

  /*!
    Add all the ended PathContour objects of a Path into this Path.
    \param path Path to add
   */
  Path&
  add_contours(const Path &path);

  /*!
    Returns the number of contours of the Path.
   */
  unsigned int
  number_contours(void) const;

  /*!
    Returns the named contour
    \param i index of contour to fetch (0 <= i < number_contours())
   */
  reference_counted_ptr<const PathContour>
  contour(unsigned int i) const;

  /*!
    Returns true if each PathContour of the Path is flat.
   */
  bool
  is_flat(void) const;

  /*!
    Returns an approximation of the bounding box for
    the PathContour::ended() PathContour obejcts of
    this Path WITHOUT relying on tessellating
    (or for that matter creating a TessellatedPath)
    this Path. Returns true if atleast one PathContour
    of this Path is PathContour::ended() and writes the
    values, otherwise returns false and does not write
    any value.
    \param out_min_bb (output) location to which to write the min-x and min-y
                               coordinates of the approximate bounding box.
    \param out_max_bb (output) location to which to write the max-x and max-y
                               coordinates of the approximate bounding box.
   */
  bool
  approximate_bounding_box(vec2 *out_min_bb, vec2 *out_max_bb) const;

  /*!
    Return the tessellation of this Path at a specific
    level of detail. The TessellatedPath is constructed
    lazily. Additionally, if this Path changes its geometry,
    then a new TessellatedPath will be contructed on the
    next call to tessellation().
    \param thresh the returned tessellated path will be so that
                  TessellatedPath::effective_curve_distance_threshhold()
                  is no more than thresh. A non-positive value
                  will return the starting point tessellation that
                  is created with default values of
                  TessellatedPath::TessellationParams.
   */
  const reference_counted_ptr<const TessellatedPath>&
  tessellation(float thresh) const;

  /*!
    Return the tessellation of this Path tessellated with the
    default values of TessellatedPath::TessellationParams.
    The TessellatedPath is constructed lazily. Additionally,
    If this Path changes its geometry, then a new TessellatedPath
    will be contructed on the next call to tessellation().
   */
  const reference_counted_ptr<const TessellatedPath>&
  tessellation(void) const;

private:
  void *m_d;
};

/*! @} */

}
