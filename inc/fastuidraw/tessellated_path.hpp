/*!
 * \file tessellated_path.hpp
 * \brief file tessellated_path.hpp
 *
 * Copyright 2018 by Intel.
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
#include <fastuidraw/painter/stroked_caps_joins.hpp>

namespace fastuidraw  {

///@cond
class Path;
class ArcStrokedPath;
class StrokedPath;
class FilledPath;
///@endcond

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * An TessellatedPath represents the tessellation of a Path
 * into line segments and arcs.
 *
 * A single contour of a TessellatedPath is constructed from
 * a single \ref PathContour of the source \ref Path. Each
 * edge of a contour of a TessellatedPath is contructed from
 * a single \ref PathContour::interpolator_base of the source \ref
 * PathContour. The ordering of the contours of a
 * TessellatedPath is the same ordering as the source
 * \ref PathContour objects of the source \ref Path. Also,
 * the ordering of edges within a contour is the same ordering
 * as the \ref PathContour::interpolator_base objects of
 * the source \ref PathContour. In particular, for each contour
 * of a TessellatedPath, the closing edge is the last edge.
 */
class TessellatedPath:
    public reference_counted<TessellatedPath>::non_concurrent
{
public:
  /*!
   * \brief
   * Enumeration to identify the type of a \ref segment
   */
  enum segment_type_t
    {
      /*!
       * Indicates that the segment is an arc segment,
       * i.e. it connects two point via an arc of a
       * circle
       */
      arc_segment,

      /*!
       * Indicates that the segment is a line segment
       * i.e. it connects two point via a line.
       */
      line_segment,
    };

  /*!
   * \brief
   * A TessellationParams stores how finely to tessellate
   * the curves of a path.
   */
  class TessellationParams
  {
  public:
    /*!
     * Ctor, initializes values.
     */
    TessellationParams(void):
      m_threshhold(1.0f),
      m_max_recursion(5),
      m_allow_arcs(true)
    {}

    /*!
     * Provided as a conveniance. Equivalent to
     * \code
     * m_threshhold = tp;
     * \endcode
     * \param p value to which to assign to \ref m_threshhold
     */
    TessellationParams&
    threshhold(float p)
    {
      m_threshhold = p;
      return *this;
    }

    /*!
     * Set the value of \ref m_max_segments.
     * \param v value to which to assign to \ref m_max_segments
     */
    TessellationParams&
    max_recursion(unsigned int v)
    {
      m_max_recursion = v;
      return *this;
    }

    /*!
     * Provided as a conveniance. Equivalent to
     * \code
     * m_allow_arcs = p;
     * \endcode
     * \param p value to which to assign to \ref m_allow_arcs
     */
    TessellationParams&
    allow_arcs(bool p)
    {
      m_allow_arcs = p;
      return *this;
    }

    /*!
     * Default value is 1.0.
     */
    float m_threshhold;

    /*!
     * Maximum number of times to cut a single edge in
     * half. Default value is 5.
     */
    unsigned int m_max_recursion;

    /*!
     * If true, the produces tessellation will have \ref arc_segment
     * typed \ref segment objects. If false, all segment objects
     * will be of type \ref line_segment. Default value is true.
     */
    bool m_allow_arcs;
  };

  /*!
   * \brief
   * Represents segment of an arc-tessellated path.
   */
  class segment
  {
  public:
    /*!
     * Specifies the segment type which in turn determines the
     * meaning of \ref m_p, \ref m_data and \ref m_radius
     */
    enum segment_type_t m_type;

    /*!
     * Gives the start point on the path of the segment.
     */
    vec2 m_start_pt;

    /*!
     * Gives the end point on the path of the segment.
     */
    vec2 m_end_pt;

    /*!
     * Only valid if \ref m_type is \ref arc_segment; gives
     * the center of the arc.
     */
    vec2 m_center;

    /*!
     * Only valid if \ref m_type is \ref arc_segment; gives
     * the angle range of the arc.
     */
    range_type<float> m_arc_angle;

    /*!
     * Only valid if \ref m_type is \ref arc_segment; gives
     * the radius of the arc.
     */
    float m_radius;

    /*!
     * Gives the length of the segment.
     */
    float m_length;

    /*!
     * Gives the distance of the start of the segment from
     * the start of the edge (i.e PathContour::interpolator_base).
     */
    float m_distance_from_edge_start;

    /*!
     * Gives the distance of the start of segment to the
     * start of the -contour-.
     */
    float m_distance_from_contour_start;

    /*!
     * Gives the length of the edge (i.e.
     * PathContour::interpolator_base) on which the
     * segment lies. This value is the same for all
     * segments along a fixed edge.
     */
    float m_edge_length;

    /*!
     * Gives the length of the contour open on which
     * the segment lies. This value is the same for all
     * segments along a fixed contour.
     */
    float m_open_contour_length;

    /*!
     * Gives the length of the contour closed on which
     * the segment lies. This value is the same for all
     * segments along a fixed contour.
     */
    float m_closed_contour_length;

    /*!
     * Gives the unit-vector of the path entering the segment.
     */
    vec2 m_enter_segment_unit_vector;

    /*!
     * Gives the unit-vector of the path leaving the segment.
     */
    vec2 m_leaving_segment_unit_vector;

    /*!
     * If true, indicates that the arc is tangent with its
     * predecessor. This happens when TessellatedPath breaks
     * a \ref segment into smaller pieces to make its angle
     * smaller or to make it monotonic.
     */
    bool m_tangent_with_predecessor;
  };

  /*!
   * \brief
   * A wrapper over a dynamic array of \ref segment objects;
   * segment values added to SegmentStorage must be added
   * in order of time along the domain of a \ref
   * PathContour::interpolate_base
   */
  class SegmentStorage:fastuidraw::noncopyable
  {
  public:
    /*!
     * Add a \ref segment to the SegmentStorage that is a
     * line segment between two points.
     * \param start the starting point of the segment
     * \param end the ending point of the segment
     */
    void
    add_line_segment(vec2 start, vec2 end);

    /*!
     * Add a \ref segment to the SegmentStorage that is an
     * arc segment. If necessary, An arc-segment will be broken
     * into multiple segments to that each segment has an arc-angle
     * of no more than M_PI / 4.0 radians (45 degrees).
     * \param start gives the start point of the arc on the path
     * \param end gives the end point of the arc on the path
     * \param center is the center of the circle that defines the arc
     * \param radius is the radius of the circle that defines the arc
     * \param arc_angle is the arc-angle range that defines the arc
     */
    void
    add_arc_segment(vec2 start, vec2 end,
		    vec2 center, float radius,
                    range_type<float> arc_angle);

  private:
    SegmentStorage(void) {}
    ~SegmentStorage() {}

    friend class TessellatedPath;
    void *m_d;
  };

  /*!
   * Ctor. Construct a TessellatedPath from a Path
   * \param input source path to tessellate
   * \param P parameters on how to tessellate the source Path
   */
  TessellatedPath(const Path &input, TessellationParams P);

  ~TessellatedPath();

  /*!
   * Returns the tessellation parameters used to construct
   * this TessellatedPath.
   */
  const TessellationParams&
  tessellation_parameters(void) const;

  /*!
   * Returns true if and only if there is a \ref segment
   * in segment_data() for which segment::m_type is
   * \ref arc_segment.
   */
  bool
  has_arcs(void) const;

  /*!
   * Returns the tessellation threshold achieved
   */
  float
  effective_threshhold(void) const;

  /*!
   * Returns the maximum number of segments any edge needed
   */
  unsigned int
  max_segments(void) const;

  /*!
   * Returns the maximum recursion employed by any edge
   */
  unsigned int
  max_recursion(void) const;

  /*!
   * Returns all the segment data
   */
  c_array<const segment>
  segment_data(void) const;

  /*!
   * Returns the number of contours
   */
  unsigned int
  number_contours(void) const;

  /*!
   * Returns the range into segment_data() for the named
   * contour.
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  range_type<unsigned int>
  contour_range(unsigned int contour) const;

  /*!
   * Returns the range into segment_data() for the named
   * contour lacking the closing edge.
   * replicated (because the derivatives are different).
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  range_type<unsigned int>
  unclosed_contour_range(unsigned int contour) const;

  /*!
   * Returns the segment data of the named contour including
   * the closing edge. Provided as a conveniance equivalent to
   * \code
   * segment_data().sub_array(contour_range(contour))
   * \endcode
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  c_array<const segment>
  contour_segment_data(unsigned int contour) const;

  /*!
   * Returns the segment data of the named contour
   * lacking the segment data of the closing edge.
   * Provided as a conveniance, equivalent to
   * \code
   * segment_data().sub_array(unclosed_contour_range(contour))
   * \endcode
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  c_array<const segment>
  unclosed_contour_segment_data(unsigned int contour) const;

  /*!
   * Returns the number of edges for the named contour
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  unsigned int
  number_edges(unsigned int contour) const;

  /*!
   * Returns the range into segment_data(void)
   * for the named edge of the named contour.
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   * \param edge which edge of the contour to query, must
   *             have that 0 <= edge < number_edges(contour)
   */
  range_type<unsigned int>
  edge_range(unsigned int contour, unsigned int edge) const;

  /*!
   * Returns the segment data of the named edge of the
   * named contour, provided as a conveniance, equivalent
   * to
   * \code
   * segment_data().sub_array(edge_range(contour, edge))
   * \endcode
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   * \param edge which edge of the contour to query, must
   *             have that 0 <= edge < number_edges(contour)
   */
  c_array<const segment>
  edge_segment_data(unsigned int contour, unsigned int edge) const;

  /*!
   * Returns the minimum point of the bounding box of
   * the tessellation.
   */
  vec2
  bounding_box_min(void) const;

  /*!
   * Returns the maximum point of the bounding box of
   * the tessellation.
   */
  vec2
  bounding_box_max(void) const;

  /*!
   * Returns the dimensions of the bounding box
   * of the tessellated path.
   */
  vec2
  bounding_box_size(void) const;

  /*!
   * Returns this TessellatedPath arc-stroked. The ArcStrokedPath
   * object is constructed lazily.
   */
  const reference_counted_ptr<const ArcStrokedPath>&
  arc_stroked(void) const;

  /*!
   * Returns this TessellatedPath linearly-stroked. The StrokedPath
   * object is constructed lazily. NOTE: will return a null-reference
   * if \ref has_arcs() returns true.
   */
  const reference_counted_ptr<const StrokedPath>&
  stroked(void) const;

  /*!
   * Returns this TessellatedPath linearly-filled. The FilledPath
   * object is constructed lazily. NOTE: will return a null-reference
   * if \ref has_arcs() returns true.
   */
  const reference_counted_ptr<const FilledPath>&
  filled(void) const;

private:
  void *m_d;
};

/*! @} */

}
