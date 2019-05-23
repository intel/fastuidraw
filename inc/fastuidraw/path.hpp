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
#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/path_enums.hpp>
#include <fastuidraw/tessellated_path.hpp>
#include <fastuidraw/painter/shader_filled_path.hpp>

namespace fastuidraw  {

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * An PathContour represents a single contour within
 * a Path.
 *
 * Closeing a contour (see \ref close(), \ref
 * close_generic() and close_arc()) means to specify
 * the edge from the last point of the PathContour
 * to the first point.
 */
class PathContour:
    public reference_counted<PathContour>::non_concurrent
{
public:
  /*!
   * \brief
   * Provides an interface to resume from a previous tessellation
   * of a \ref interpolator_base derived object.
   */
  class tessellation_state:
    public reference_counted<tessellation_state>::non_concurrent
  {
  public:
    /*!
     * To be implemented by a derived class to return the depth
     * of recursion at this objects stage of tessellation.
     */
    virtual
    unsigned int
    recursion_depth(void) const = 0;

    /*!
     * To be implemented by a derived class to resume tessellation
     * and to (try to) achieve the required threshhold within the
     * recursion limits of a \ref TessellatedPath::TessellationParams
     * value.
     * \param tess_params tessellation parameters
     * \param out_data location to which to write the tessellations
     * \param out_max_distance location to which to write an upperbound for the
     *                         distance between the curve and the tesseallation
     *                         approximation.
     */
    virtual
    void
    resume_tessellation(const TessellatedPath::TessellationParams &tess_params,
                        TessellatedPath::SegmentStorage *out_data,
                        float *out_max_distance) = 0;
  };

  /*!
   * \brief
   * Base class to describe how to interpolate from one
   * point of a PathContour to the next, i.e. describes
   * the shape of an edge.
   */
  class interpolator_base:
    public reference_counted<interpolator_base>::non_concurrent
  {
  public:
    /*!
     * Ctor.
     * \param contour \ref PathContour to which to add the interpolator.
     *                The interpolator is added to the contour at the
     *                interpolator's construction. The start point is
     *                computed from the current state of the \ref
     *                PathContour.
     * \param end end point of the edge of this interpolator
     * \param tp nature the edge represented by this interpolator_base
     */
    interpolator_base(PathContour &contour,
                      const vec2 &end, enum PathEnums::edge_type_t tp);

    virtual
    ~interpolator_base();

    /*!
     * Returns the starting point of this interpolator.
     */
    const vec2&
    start_pt(void) const;

    /*!
     * Returns the ending point of this interpolator
     */
    const vec2&
    end_pt(void) const;

    /*!
     * Returns the edge type.
     */
    enum PathEnums::edge_type_t
    edge_type(void) const;

    /*!
     * To be implemented by a derived class to return true if
     * the interpolator is flat, i.e. is just a line segment
     * connecting start_pt() to end_pt().
     */
    virtual
    bool
    is_flat(void) const = 0;

    /*!
     * To be implemented by a derived class to produce the arc-tessellation
     * from start_pt() to end_pt(). In addition, for recursive tessellation,
     * returns the tessellation state to be queried for recursion depth and
     * reused to refine the tessellation. If the tessellation routine is not
     * recursive in nature, return nullptr.
     *
     * \param tess_params tessellation parameters
     * \param out_data location to which to write the tessellations
     * \param out_max_distance location to which to write an upperbound for the
     *                       distance between the curve and the tesseallation
     *                       approximation.
     */
    virtual
    reference_counted_ptr<tessellation_state>
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         TessellatedPath::SegmentStorage *out_data,
                         float *out_max_distance) const = 0;

    /*!
     * To be implemented by a derived class to return a fast (and approximate)
     * bounding box for the interpolator.
     * \param out_bb (output) location to which to write the bounding box value
     */
    virtual
    void
    approximate_bounding_box(Rect *out_bb) const = 0;

    /*!
     * To be implemented by a derived class to create and
     * return a deep copy of the interpolator object.
     */
    virtual
    reference_counted_ptr<interpolator_base>
    deep_copy(PathContour &contour) const = 0;

    /*!
     * To be optionally implemented by a derived class to add this
     * interpolator to a \ref ShaderFilledPath::Builder. A return
     * code of \ref routine_fail means that the interpolator cannot
     * be realized in such a way to be added and a \ref Path that
     * includes such an interpolator in a closed contour will
     * be unable to realized a \ref ShaderFilledPath value and
     * \ref Path::shader_filled_path() will return a null handle.
     * Default implementation is to return routine_fail.
     * \param builder object to which to add interpolator.
     * \param tol error goal between the interpolator and how it
     *            is realized on the ShaderFilledPath::Builder
     */
    virtual
    enum return_code
    add_to_builder(ShaderFilledPath::Builder *builder, float tol) const;

  private:
    friend class PathContour;
    void *m_d;
  };

  /*!
   * \brief
   * A flat interpolator represents a flat edge.
   */
  class flat:public interpolator_base
  {
  public:
    /*!
     * Ctor.
     * \param contour \ref PathContour to which to add the interpolator.
     *                The interpolator is added to the contour at the
     *                interpolator's construction. The start point is
     *                computed from the current state of the \ref
     *                PathContour
     * \param end end point of the edge of this interpolator
     * \param tp nature the edge represented by this interpolator_base
     */
    flat(PathContour &contour,
         const vec2 &end, enum PathEnums::edge_type_t tp):
      interpolator_base(contour, end, tp)
    {}

    virtual
    bool
    is_flat(void) const;

    virtual
    reference_counted_ptr<tessellation_state>
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         TessellatedPath::SegmentStorage *out_data,
                         float *out_max_distance) const;
    virtual
    void
    approximate_bounding_box(Rect *out_bb) const;

    virtual
    reference_counted_ptr<interpolator_base>
    deep_copy(PathContour &contour) const;

    virtual
    enum return_code
    add_to_builder(ShaderFilledPath::Builder *builder, float tol) const;
  };

  /*!
   * \brief
   * Interpolator generic implements tessellation by recursion
   * and relying on analytic derivative provided by derived
   * class.
   */
  class interpolator_generic:public interpolator_base
  {
  public:
    /*!
     * A tessellated_region is a base class for a cookie
     * used and generated by tessellate().
     */
    class tessellated_region:
      public reference_counted<tessellated_region>::non_concurrent
    {
    public:
      /*!
       * To be implemented by a derived class to compute an upper-bound
       * of the distance from the curve restricted to the region to the
       * line segment connecting the end points of the region.
       */
      virtual
      float
      distance_to_line_segment(void) const = 0;

      /*!
       * To be implemented by a derived class to compute an approximate
       * upper-bound for the distance from the curve restricted to the
       * region to a given arc.
       * \param arc_radius radius of the arc
       * \param center center of the circle of the arc
       * \param unit_vector_arc_middle unit vector from center to the midpoint of the arc
       * \param cos_half_arc_angle the cosine of half of the arc-angle
       */
      virtual
      float
      distance_to_arc(float arc_radius, vec2 center,
                      vec2 unit_vector_arc_middle,
                      float cos_half_arc_angle) const = 0;
    };

    /*!
     * Ctor.
     * \param contour \ref PathContour to which the interpolator is added,
     *                the start point of the interpolator will be ending
     *                point of \ref PathContour::prev_interpolator().
     * \param end end point of the edge of this interpolator
     * \param tp nature the edge represented by this interpolator_base
     */
    interpolator_generic(PathContour &contour,
                         const vec2 &end, enum PathEnums::edge_type_t tp):
      interpolator_base(contour, end, tp)
    {}

    virtual
    reference_counted_ptr<tessellation_state>
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         TessellatedPath::SegmentStorage *out_data,
                         float *out_max_distance) const;

    /*!
     * To be implemented by a derived to assist in recursive tessellation.
     * \param in_region region to divide in half, a nullptr value indicates
     *                  that the region is the entire interpolator.
     * \param out_regionA location to which to write the first half
     * \param out_regionB location to which to write the second half
     * \param out_p location to which to write the position of the point
     *              on the curve in the middle (with repsect to time) of
     *              in_region
     */
    virtual
    void
    tessellate(reference_counted_ptr<tessellated_region> in_region,
               reference_counted_ptr<tessellated_region> *out_regionA,
               reference_counted_ptr<tessellated_region> *out_regionB,
               vec2 *out_p) const = 0;

    /*!
     * To be implemented by a derived class to return a reasonable
     * lower bound on the needed number of times the edge should be
     * cut in half in order to capture its shape.
     */
    virtual
    unsigned int
    minimum_tessellation_recursion(void) const = 0;
  };

  /*!
   * \brief
   * Derived class of interpolator_base to indicate a Bezier curve.
   * Supports Bezier curves of _any_ degree.
   */
  class bezier:public interpolator_generic
  {
  public:
    /*!
     * Ctor. One control point, thus interpolation is a quadratic curve.
     * \param contour \ref PathContour to which to add the interpolator.
     *                The interpolator is added to the contour at the
     *                interpolator's construction. The start point is
     *                computed from the current state of the \ref
     *                PathContour
     * \param ct control point
     * \param end end of curve
     * \param tp nature the edge represented by this interpolator_base
     */
    bezier(PathContour &contour,
           const vec2 &ct, const vec2 &end, enum PathEnums::edge_type_t tp);

    /*!
     * Ctor. Two control points, thus interpolation is a cubic curve.
     * \param contour \ref PathContour to which to add the interpolator.
     *                The interpolator is added to the contour at the
     *                interpolator's construction. The start point is
     *                computed from the current state of the \ref
     *                PathContour
     * \param ct1 1st control point
     * \param ct2 2nd control point
     * \param end end point of curve
     * \param tp nature the edge represented by this interpolator_base
     */
    bezier(PathContour &contour,
           const vec2 &ct1, const vec2 &ct2, const vec2 &end,
           enum PathEnums::edge_type_t tp);

    /*!
     * Ctor. Iterator range defines the control points of the bezier curve.
     * \param contour \ref PathContour to which to add the interpolator.
     *                The interpolator is added to the contour at the
     *                interpolator's construction. The start point is
     *                computed from the current state of the \ref
     *                PathContour
     * \param control_pts control points of the bezier curve created,
     *                    can be any size allowing bezier curves of
     *                    arbitrary degree
     * \param end end point of curve
     * \param tp nature the edge represented by this interpolator_base
     */
    bezier(PathContour &contour,
           c_array<const vec2> control_pts, const vec2 &end,
           enum PathEnums::edge_type_t tp);

    virtual
    ~bezier();

    /*!
     * Returns the control points of the Bezier curve with
     * c_array<const vec2>::front() having the same value as
     * start_pt() and c_array<const vec2>::back() having the
     * same value as end_pt().
     */
    c_array<const vec2>
    pts(void) const;

    virtual
    bool
    is_flat(void) const;

    virtual
    void
    tessellate(reference_counted_ptr<tessellated_region> in_region,
               reference_counted_ptr<tessellated_region> *out_regionA,
               reference_counted_ptr<tessellated_region> *out_regionB,
               vec2 *out_p) const;
    virtual
    void
    approximate_bounding_box(Rect *out_bb) const;

    virtual
    reference_counted_ptr<interpolator_base>
    deep_copy(PathContour &contour) const;

    virtual
    unsigned int
    minimum_tessellation_recursion(void) const;

    virtual
    enum return_code
    add_to_builder(ShaderFilledPath::Builder *builder, float tol) const;

  private:
    void *m_d;
  };

  /*!
   * \brief
   * An arc is for connecting one point to the next via an
   * arc of a circle.
   */
  class arc:public interpolator_base
  {
  public:
    /*!
     * Ctor.
     * \param contour \ref PathContour to which to add the interpolator.
     *                The interpolator is added to the contour at the
     *                interpolator's construction. The start point is
     *                computed from the current state of the \ref
     *                PathContour
     * \param angle The angle of the arc in radians, the value must not
     *              be a multiple of 2*FASTUIDRAW_PI. Assuming a coordinate system
     *              where y-increases vertically and x-increases to the right,
     *              a positive value indicates to have the arc go counter-clockwise,
     *              a negative angle for the arc to go clockwise.
     * \param end end of curve
     * \param tp nature the edge represented by this interpolator_base
     */
    arc(PathContour &contour,
        float angle, const vec2 &end, enum PathEnums::edge_type_t tp);

    ~arc();

    /*!
     * Returns the center of the arc.
     */
    vec2
    center(void) const;

    /*!
     * Returns the starting and ending angle of the arc
     * each in radians.
     */
    range_type<float>
    angle(void) const;

    virtual
    bool
    is_flat(void) const;

    virtual
    void
    approximate_bounding_box(Rect *out_bb) const;

    virtual
    reference_counted_ptr<interpolator_base>
    deep_copy(PathContour &contour) const;

    virtual
    reference_counted_ptr<tessellation_state>
    produce_tessellation(const TessellatedPath::TessellationParams &tess_params,
                         TessellatedPath::SegmentStorage *out_data,
                         float *out_max_distance) const;

    virtual
    enum return_code
    add_to_builder(ShaderFilledPath::Builder *builder, float tol) const;

  private:
    arc(const arc &q, PathContour &contour);

    void *m_d;
  };

  /*!
   * Ctor.
   */
  explicit
  PathContour(void);

  ~PathContour();

  /*!
   * Start the PathContour, may only be called once in the lifetime
   * of a PathContour() and must be called before adding points
   * (to_point()), adding control points (add_control_point()),
   * adding arcs (to_arc()), creating any \ref interpolator_base
   * objects using this \ref PathContour or closing the contour.
   */
  void
  start(const vec2 &pt);

  /*!
   * Close the current edge.
   * \param pt point location of end of edge (and thus start of new edge)
   * \param etp the edge type of the new edge made; if this is the first
   *            edge of the contour, the value of etp is ignored and the
   *            value \ref PathEnums::starts_new_edge is used.
   */
  void
  to_point(const vec2 &pt, enum PathEnums::edge_type_t etp);

  /*!
   * Add a control point. Will fail if close() was called
   * \param pt location of new control point
   */
  void
  add_control_point(const vec2 &pt);

  /*!
   * Clear any current control points.
   */
  void
  clear_control_points(void);

  /*!
   * Will fail if close() was called of if add_control_point() has been
   * called more recently than to_point().
   * \param angle angle of arc in radians
   * \param pt point where arc ends (and next edge starts)
   * \param etp the edge type of the new edge made; if this is the first
   *            edge of the contour, the value of etp is ignored and the
   *            value \ref PathEnums::starts_new_edge is used.
   */
  void
  to_arc(float angle, const vec2 &pt, enum PathEnums::edge_type_t etp);

  /*!
   * End the PathContour without adding a closing edge.
   */
  void
  end(void);

  /*!
   * Closes the \ref PathContour using the last \ref interpolator_base
   * derived object on the \ref PathContour. That interpolator must
   * interpolate to the start point of the \ref PathContour
   */
  void
  close_generic(void);

  /*!
   * Closes with the Bezier curve defined by the current
   * control points added by add_control_point().
   * \param etp the edge type of the new edge made.
   */
  void
  close(enum PathEnums::edge_type_t etp);

  /*!
   * Closes with an arc.
   * \param angle angle of arc in radians
   * \param etp the edge type of the new edge made.
   */
  void
  close_arc(float angle, enum PathEnums::edge_type_t etp);

  /*!
   * Returns the last interpolator added to this \ref PathContour.
   * If no contours have been added, returns a null reference.
   */
  const reference_counted_ptr<const interpolator_base>&
  prev_interpolator(void);

  /*!
   * Returns true if the PathContour is closed.
   */
  bool
  closed(void) const;

  /*!
   * Returns true if the PathContour is ended, and thus
   * no additional interpolator may be added.
   */
  bool
  ended(void) const;

  /*!
   * Return the I'th point of this PathContour.
   * For I = 0, returns the value passed to start().
   * \param I index of point.
   */
  const vec2&
  point(unsigned int I) const;

  /*!
   * Returns the number of points of this PathContour.
   */
  unsigned int
  number_points(void) const;

  /*!
   * Returns the number of interpolators of this
   * PathContour. This is equal to number_points()
   * if closed() is true; otherwise it is equal to
   * number_points() - 1.
   */
  unsigned int
  number_interpolators(void) const;

  /*!
   * Returns the interpolator of this PathContour
   * that interpolates from the I'th point to the
   * (I + 1)'th point. When the closed() is true,
   * if I is number_points() - 1, then returns the
   * interpolator from the last point to the first
   * point. When closed() is false, if I has value
   * number_points() - 1, then returns a null reference.
   */
  const reference_counted_ptr<const interpolator_base>&
  interpolator(unsigned int I) const;

  /*!
   * Returns an approximation of the bounding box for
   * this PathContour WITHOUT relying on tessellating
   * the \ref interpolator_base objects of this \ref
   * PathContour. Returns false if the box is empty.
   * \param out_bb (output) location to which to write
   *                        the bounding box value
   */
  bool
  approximate_bounding_box(Rect *out_bb) const;

  /*!
   * Returns true if each interpolator of the PathContour is
   * flat.
   */
  bool
  is_flat(void) const;

  /*!
   * Create a deep copy of this PathContour.
   */
  reference_counted_ptr<PathContour>
  deep_copy(void) const;

private:
  void *m_d;
};

/*!
 * \brief
 * A Path represents a collection of PathContour
 * objects.
 */
class Path:noncopyable
{
public:
  /*!
   * \brief
   * Class that wraps a vec2 to mark a point
   * as a control point for a Bezier curve
   */
  class control_point
  {
  public:
    /*!
     * Ctor
     * \param pt value to which to set \ref m_location
     */
    explicit
    control_point(const vec2 &pt):
      m_location(pt)
    {}

    /*!
     * Ctor
     * \param x value to which to set m_location.x()
     * \param y value to which to set m_location.y()
     */
    control_point(float x, float y):
      m_location(x,y)
    {}

    /*!
     * Position of control point
     */
    vec2 m_location;
  };

  /*!
   * \brief
   * Wraps the data to specify an arc
   */
  class arc
  {
  public:
    /*!
     * Ctor
     * \param angle angle of arc in radians
     * \param pt point to which to arc
     */
    arc(float angle, const vec2 &pt):
      m_angle(angle), m_pt(pt)
    {}

    /*!
     * Angle of arc in radians
     */
    float m_angle;

    /*!
     * End point of arc
     */
    vec2 m_pt;
  };

  /*!
   * \brief
   * Tag class to mark the close of a contour
   */
  class contour_close
  {};

  /*!
   * \brief
   * Tag class to mark the end of a contour without
   * adding a closing edge of the contour and start
   * a new contour
   */
  class contour_end
  {};

  /*!
   * \brief
   * Indicates to end the existing contour with adding
   * a closing edge of the contour and start a new contour
   */
  class contour_start
  {
  public:
    /*!
     * Ctor to indicate to start a new contour
     * without closing the previous contour.
     */
    explicit
    contour_start(const vec2 &pt):
      m_pt(pt)
    {}

    /*!
     * Ctor to indicate to start a new contour
     * without closing the previous contour.
     */
    explicit
    contour_start(float x, float y):
      m_pt(x, y)
    {}

    /*!
     * Location of start of new contour.
     */
    vec2 m_pt;
  };

  /*!
   * \brief
   * Tag class to mark the close of an contour with an arc
   */
  class contour_close_arc
  {
  public:
    /*!
     * Ctor
     * \param angle angle of arc in radians
     */
    explicit
    contour_close_arc(float angle):
      m_angle(angle)
    {}

    /*!
     * Angle of arc in radians
     */
    float m_angle;
  };

  /*!
   * Ctor.
   */
  explicit
  Path(void);

  ~Path();

  /*!
   * Clear the path, i.e. remove all PathContour's from the
   * path
   */
  void
  clear(void);

  /*!
   * Swap contents of Path with another Path
   * \param obj Path with which to swap
   */
  void
  swap(Path &obj);

  /*!
   * Create an arc but specify the angle in degrees.
   * \param angle angle of arc in degrees
   * \param pt point to which to arc
   */
  static
  arc
  arc_degrees(float angle, const vec2 &pt)
  {
    return arc(angle*float(FASTUIDRAW_PI)/180.0f, pt);
  }

  /*!
   * Create an contour_close_arc but specify the angle in degrees.
   * \param angle angle or arc in degrees
   */
  static
  contour_close_arc
  contour_close_arc_degrees(float angle)
  {
    return contour_close_arc(angle*float(FASTUIDRAW_PI)/180.0f);
  }

  /*!
   * Operator overload to add a point of the current
   * contour in the Path.
   * \param pt point to add
   */
  Path&
  operator<<(const vec2 &pt);

  /*!
   * Operator overload to add a control point of the current
   * contour in the Path.
   * \param pt control point to add
   */
  Path&
  operator<<(const control_point &pt);

  /*!
   * Operator overload to add an arc to the current contour
   * in the Path.
   * \param a arc to add
   */
  Path&
  operator<<(const arc &a);

  /*!
   * Operator overload to close the current contour
   */
  Path&
  operator<<(contour_close);

  /*!
   * Operator overload to end the current contour
   */
  Path&
  operator<<(contour_end);

  /*!
   * Operator overload to close the current contour
   * \param a specifies the angle of the arc for closing
   *          the current contour
   */
  Path&
  operator<<(contour_close_arc a);

  /*!
   * Operator overload to start a new contour without closing
   * the current contour.
   * \param st specifies the starting point of the new contour
   */
  Path&
  operator<<(const contour_start &st)
  {
    move(st.m_pt);
    return *this;
  }

  /*!
   * Operator overload to control PathEnums::edge_type_t
   * of the next edge made via operator overloads.
   * If no edge is yet present on the current contour, then
   * the value is ignored. The tag is reset back to \ref
   * PathEnums::starts_new_edge after an edge is added.
   * \param etp edge type
   */
  Path&
  operator<<(enum PathEnums::edge_type_t etp);

  /*!
   * Append a line to the current contour.
   * \param pt point to which the line goes
   * \param etp the edge type of the new line made; if this is the first
   *            edge of the current contour, the value of etp is ignored
   *            and the value \ref PathEnums::starts_new_edge is used.
   */
  Path&
  line_to(const vec2 &pt,
          enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * Append a quadratic Bezier curve to the current contour.
   * \param ct control point of the quadratic Bezier curve
   * \param pt point to which the quadratic Bezier curve goes
   * \param etp the edge type of the new quadratic made; if this is the first
   *            edge of the current contour, the value of etp is ignored
   *            and the value \ref PathEnums::starts_new_edge is used.
   */
  Path&
  quadratic_to(const vec2 &ct, const vec2 &pt,
               enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * Append a cubic Bezier curve to the current contour.
   * \param ct1 first control point of the cubic Bezier curve
   * \param ct2 second control point of the cubic Bezier curve
   * \param pt point to which the cubic Bezier curve goes
   * \param etp the edge type of the new cubic made; if this is the first
   *            edge of the current contour, the value of etp is ignored
   *            and the value \ref PathEnums::starts_new_edge is used.
   */
  Path&
  cubic_to(const vec2 &ct1, const vec2 &ct2, const vec2 &pt,
           enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * Append an arc curve to the current contour.
   * \param angle gives the angle of the arc in radians. For a coordinate system
   *              where y increases upwards and x increases to the right, a positive
   *              value indicates counter-clockwise and a negative value indicates
   *              clockwise
   * \param pt point to which the arc curve goes
   * \param etp the edge type of the new arc made; if this is the first
   *            edge of the current contour, the value of etp is ignored
   *            and the value \ref PathEnums::starts_new_edge is used.
   */
  Path&
  arc_to(float angle, const vec2 &pt,
         enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * Begin a new contour.
   * \param pt point at which the contour begins
   */
  Path&
  move(const vec2 &pt);

  /*!
   * End the current contour without adding a closing edge.
   */
  Path&
  end_contour(void);

  /*!
   * Close the current contour with a line segment.
   * \param etp the edge type of the closing edge made.
   */
  Path&
  close_contour(enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * Close the current contour in an arc
   * \param angle gives the angle of the arc in radians. For a coordinate system
   *              where y increases upwards and x increases to the right, a positive
   *              value indicates counter-clockwise and a negative value indicates
   *              clockwise
   * \param etp the edge type of the closing edge made.
   */
  Path&
  close_contour_arc(float angle,
                    enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * Close the current contour in a quadratic Bezier curve
   * \param ct control point of the quadratic Bezier curve
   * \param etp the edge type of the closing edge made.
   */
  Path&
  close_contour_quadratic(const vec2 &ct,
                          enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * Close the current contour in a cubic Bezier curve
   * \param ct1 first control point of the cubic Bezier curve
   * \param ct2 second control point of the cubic Bezier curve
   * \param etp the edge type of the closing edge made.
   */
  Path&
  close_contour_cubic(const vec2 &ct1, const vec2 &ct2,
                      enum PathEnums::edge_type_t etp = PathEnums::starts_new_edge);

  /*!
   * The current contour of this \ref Path. Use thie value when creating
   * \ref PathContour::interpolator_base objects.
   */
  PathContour&
  current_contour(void);

  /*!
   * Adds a PathContour to this Path. The current contour remains
   * as the current contour though.
   * \param contour PathContour to add to the Path
   */
  Path&
  add_contour(const reference_counted_ptr<const PathContour> &contour);

  /*!
   * Add all the \ref PathContour objects of a Path into this Path.
   * \param path Path to add
   */
  Path&
  add_contours(const Path &path);

  /*!
   * Returns the number of contours of the Path.
   */
  unsigned int
  number_contours(void) const;

  /*!
   * Returns the named contour
   * \param i index of contour to fetch (0 <= i < number_contours())
   */
  reference_counted_ptr<const PathContour>
  contour(unsigned int i) const;

  /*!
   * Returns true if each PathContour of the Path is flat.
   */
  bool
  is_flat(void) const;

  /*!
   * Returns an approximation of the bounding box for
   * this Path. Returns false if the Path is empty.
   * \param out_bb (output) location to which to write
   *                        the bounding box value
   */
  bool
  approximate_bounding_box(Rect *out_bb) const;

  /*!
   * Return the tessellation of this Path at a specific
   * level of detail. The TessellatedPath is constructed
   * lazily. Additionally, if this Path changes its geometry,
   * then a new TessellatedPath will be contructed on the
   * next call to tessellation().
   * \param thresh the returned tessellated path will be so that
   *               TessellatedPath::max_distance() is no more than
   *               thresh. A non-positive value will return the
   *               lowest level of detail tessellation.
   */
  const TessellatedPath&
  tessellation(float thresh) const;

  /*!
   * Provided as a conveniance, returns the starting point tessellation.
   * Equivalent to
   * \code
   * tessellation(-1.0f)
   * \endcode
   */
  const TessellatedPath&
  tessellation(void) const;

  /*!
   * Returns the \ref ShaderFilledPath coming from this
   * Path. The returned reference will be null if the
   * Path contains anything besides line segments,
   * quadratic Bezier curves or cubic Bezier curves.
   */
  const ShaderFilledPath&
  shader_filled_path(void) const;

private:
  void *m_d;
};

/*! @} */

}
