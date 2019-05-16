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
#include <fastuidraw/util/rect.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/path_enums.hpp>

namespace fastuidraw  {

///@cond
class Path;
class StrokedPath;
class FilledPath;
class PartitionedTessellatedPath;
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
 * of a TessellatedPath, if an edge is closed, the closing edge
 * is the last edge.
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
   * Enumeration to describe if a segment is split
   */
  enum split_t
    {
      /*!
       * Indicates that entire segment is before
       * the split value
       */
      segment_completey_before_split,

      /*!
       * Indicates that entire segment is after
       * the split value
       */
      segment_completey_after_split,

      /*!
       * indicates that the \ref segment was split
       * with the segment starting before the split
       * point.
       */
      segment_split_start_before,

      /*!
       * indicates that the \ref segment was split
       * with the segment starting after the split
       * point.
       */
      segment_split_start_after,
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
      m_max_distance(-1.0f),
      m_max_recursion(5)
    {}

    /*!
     * Provided as a conveniance. Equivalent to
     * \code
     * m_max_distance = tp;
     * \endcode
     * \param p value to which to assign to \ref m_max_distance
     */
    TessellationParams&
    max_distance(float p)
    {
      m_max_distance = p;
      return *this;
    }

    /*!
     * Set the value of \ref m_max_recursion.
     * \param v value to which to assign to \ref m_max_recursion
     */
    TessellationParams&
    max_recursion(unsigned int v)
    {
      m_max_recursion = v;
      return *this;
    }

    /*!
     * Maximum distance to attempt between the actual curve and the
     * tessellation. A value less than or equal to zero indicates to
     * accept any distance value between the tessellation and the
     * curve. Default value is -1.0 (i.e. accept any distance value).
     */
    float m_max_distance;

    /*!
     * Maximum number of times to perform recursion to tessellate an edge.
     * Default value is 5.
     */
    unsigned int m_max_recursion;
  };

  /*!
   * \brief
   * Represents segment of a tessellated or arc-tessellated path.
   */
  class segment
  {
  public:
    /*!
     * Specifies the segment type.
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
     * Gives the length of the contour on which this
     * segment lies. This value is the same for all
     * segments along a fixed contour.
     */
    float m_contour_length;

    /*!
     * Gives the unit-vector of the path entering the segment.
     */
    vec2 m_enter_segment_unit_vector;

    /*!
     * Gives the unit-vector of the path leaving the segment.
     */
    vec2 m_leaving_segment_unit_vector;

    /*!
     * If true, indicates that the arc is a continuation of
     * its predecessor. This happens when TessellatedPath
     * breaks a \ref segment into smaller pieces to make its
     * angle smaller, to make it monotonic or if it is the
     * second portion of a split segment as calculated from
     * \ref compute_split_x() or \ref compute_split_y().
     */
    bool m_continuation_with_predecessor;

    /*!
     * The contour from which the \ref segment originates,
     * i.e. the \ref segment originates from the \ref
     * PathContour::interpolator_base \ref
     * PathContour::interpolator(\ref m_edge_id) of the
     * \ref PathContour of \ref Path::contour(\ref m_contour_id).
     */
    unsigned int m_contour_id;

    /*!
     * The edge from which the \ref segment originates,
     * i.e. the \ref segment originates from the \ref
     * PathContour::interpolator_base \ref
     * PathContour::interpolator(\ref m_edge_id) of the
     * \ref PathContour of \ref Path::contour(\ref m_contour_id).
     */
    unsigned int m_edge_id;

    /*!
     * Indicates the this segment is the first segment of
     * an edge
     */
    bool m_first_segment_of_edge;

    /*!
     * Indicates the this segment is the last segment of
     * an edge
     */
    bool m_last_segment_of_edge;

    /*!
     * Compute the splitting splitting of this \ref segment
     * against a vertical line with the given x-coordinate
     * \param x_split x-coordinate of vertical splitting line
     * \param dst_before_split location to which to write
     *                         the portion of the segment that
     *                         comes before the splitting line
     * \param dst_after_split location to which to write
     *                         the portion of the segment that
     *                         comes after the splitting line
     * \returns how the segment was split. Note that if the return
     *          value is \ref segment_completey_before_split or
     *          \ref segment_completey_after_split then neither
     *          of dst_before_split and dst_after_split are
     *          written to.
     */
    enum split_t
    compute_split_x(float x_split,
                    segment *dst_before_split,
                    segment *dst_after_split) const;

    /*!
     * Compute the splitting splitting of this \ref segment
     * against a horizontal line with the given y-coordinate
     * \param y_split y-coordinate of horizontal splitting line
     * \param dst_before_split location to which to write
     *                         the portion of the segment that
     *                         comes before the splitting line
     * \param dst_after_split location to which to write
     *                         the portion of the segment that
     *                         comes after the splitting line
     * \returns how the segment was split. Note that if the return
     *          value is \ref segment_completey_before_split or
     *          \ref segment_completey_after_split then neither
     *          of dst_before_split and dst_after_split are
     *          written to.
     */
    enum split_t
    compute_split_y(float y_split,
                    segment *dst_before_split,
                    segment *dst_after_split) const;

    /*!
     * Compute the splitting splitting of this \ref segment
     * against a horizontal or vertical line with the given
     * coordinate. Provided as a conveniance, equivalent to
     * \code
     * if (splitting_coordinate == 0)
     *  {
     *     compute_split_x(split, dst_before_split, dst_after_split);
     *  }
     * else
     *  {
     *     compute_split_y(split, dst_before_split, dst_after_split);
     *  }
     * \endcode
     * \param split x-coordinate or y-coordinate of splitting line
     * \param dst_before_split location to which to write
     *                         the portion of the segment that
     *                         comes before the splitting line
     * \param dst_after_split location to which to write
     *                         the portion of the segment that
     *                         comes after the splitting line
     * \param splitting_coordinate determines if to split by a vertical
     *                             line or a horizontal line.
     * \returns how the segment was split. Note that if the return
     *          value is \ref segment_completey_before_split or
     *          \ref segment_completey_after_split then neither
     *          of dst_before_split and dst_after_split are
     *          written to.
     */
    enum split_t
    compute_split(float split,
                  segment *dst_before_split,
                  segment *dst_after_split,
                  int splitting_coordinate) const;
  };

  /*!
   * \brief
   * Represents the geometric data for a join
   */
  class join
  {
  public:
    /*!
     * Gives the position of the join
     */
    vec2 m_position;

    /*!
     * Gives the unit-vector of the path entering the join.
     */
    vec2 m_enter_join_unit_vector;

    /*!
     * Gives the unit-vector of the path leaving the join.
     */
    vec2 m_leaving_join_unit_vector;

    /*!
     * Gives the distance of the join from the previous join.
     */
    float m_distance_from_previous_join;

    /*!
     * Gives the distance of the join from the start
     * of the -contour- on which the point resides.
     */
    float m_distance_from_contour_start;

    /*!
     * Length of the contour on which the join resides.
     */
    float m_contour_length;

    /*!
     * Gives the contour from which the join originates,
     * following the same convention as \ref
     * segment::m_contour_id.
     */
    unsigned int m_contour_id;

    /*!
     * Gives the interpolator that goes into the join,
     * following the same convention as \ref
     * segment::m_edge_id.
     */
    unsigned int m_edge_into_join_id;

    /*!
     * Gives the interpolator that leaves the join,
     * following the same convention as \ref
     * segment::m_edge_id.
     */
    unsigned int m_edge_leaving_join_id;

    /*!
     * When stroking a join, one needs to know what side of the
     * edge gets the join. For example a bevel join is formed by
     * the triangle formed from the three points: the outer edge
     * at the join of the segment going into the join, the outer
     * edge of the segment leaving the join and the point where
     * the segments meet. The value of lambda() gives the sign to
     * apply to \ref enter_join_normal() and \ref leaving_join_normal()
     * to get the unit vector from where the segments meet to the
     * outer edge.
     */
    float
    lambda(void) const;

    /*!
     * If this join is realized as a miter-join, returns the distance
     * from the point of the join (i.e. where the segments intersect)
     * to the tip of the miter join. If the path enter and leaving
     * the join are parallel or anti-parallel, then return -1.0.
     */
    float
    miter_distance(void) const;

    /*!
     * Gives the normal vector to going into the join
     */
    vec2
    enter_join_normal(void) const
    {
      return vec2(-m_enter_join_unit_vector.y(), m_enter_join_unit_vector.x());
    }

    /*!
     * Gives the normal vector to leaving from the join
     */
    vec2
    leaving_join_normal(void) const
    {
      return vec2(-m_leaving_join_unit_vector.y(), m_leaving_join_unit_vector.x());
    }
  };

  /*!
   * \brief
   * Represents the geometric data for a cap
   */
  class cap
  {
  public:
    /*!
     * Gives the position of the cap
     */
    vec2 m_position;

    /*!
     * Gives the unit-vector into the cap
     */
    vec2 m_unit_vector;

    /*!
     * Length of the contour on which the cap resides.
     */
    float m_contour_length;

    /*!
     * Length of the edge on which the cap resides.
     */
    float m_edge_length;

    /*!
     * True if the cap is from the start of a contour
     */
    bool m_is_starting_cap;

    /*!
     * Gives the contour from which the join originates,
     * following the same convention as \ref
     * segment::m_contour_id.
     */
    unsigned int m_contour_id;
  };

  /*!
   * \brief
   * A wrapper over a dynamic array of \ref segment objects;
   * segment values added to SegmentStorage must be added
   * in order of time along the domain of a \ref
   * PathContour::interpolator_base
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
     * into multiple segments to that each segment is monotonic
     * in the x and y coordiantes and each segment's arc-angle
     * is no more than FASTUIDRAW_PI / 4.0 radians (45 degrees).
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
   * A Refiner is stateful object that creates new TessellatedPath
   * objects from a starting TessellatedPath where the tessellation
   * is made finer.
   */
  class Refiner:
    public reference_counted<Refiner>::non_concurrent
  {
  public:
    virtual
    ~Refiner();

    /*!
     * Update the TessellatedPath returned by tessellated_path() by
     * refining the current value returned by tessellated_path().
     * \param max_distance new maximum distance to aim for
     * \param additional_recursion amount by which to additionally recurse
     *                             when tessellating.
     */
    void
    refine_tessellation(float max_distance,
                        unsigned int additional_recursion);

    /*!
     * Returns the current TessellatedPath of this Refiner.
     */
    reference_counted_ptr<TessellatedPath>
    tessellated_path(void) const;

  private:
    friend class TessellatedPath;
    Refiner(TessellatedPath *p, const Path &path);

    void *m_d;
  };

  /*!
   * Ctor. Construct a TessellatedPath from a Path
   * \param input source path to tessellate
   * \param P parameters on how to tessellate the source Path
   * \param ref if non-NULL, construct a Refiner object and return
   *            the value via upading the value of ref.
   */
  TessellatedPath(const Path &input, TessellationParams P,
                  reference_counted_ptr<Refiner> *ref = nullptr);

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
   * Returns the maximum across all edges of all contours
   * of the distance between the tessellation and the actual
   * path.
   */
  float
  max_distance(void) const;

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
   * Returns all the join data
   */
  c_array<const join>
  join_data(void) const;

  /*!
   * Returns all the cap data
   */
  c_array<const cap>
  cap_data(void) const;

  /*!
   * Returns the number of contours
   */
  unsigned int
  number_contours(void) const;

  /*!
   * Returns true if the named contour was closed
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  bool
  contour_closed(unsigned int contour) const;

  /*!
   * Returns the range into segment_data() for the named
   * contour.
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  range_type<unsigned int>
  contour_range(unsigned int contour) const;

  /*!
   * Returns the segment data of the named contour.
   * Provided as a conveniance equivalent to
   * \code
   * segment_data().sub_array(contour_range(contour))
   * \endcode
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  c_array<const segment>
  contour_segment_data(unsigned int contour) const;

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
   * Returns the edge type of the named edge of the named contour
   * of the source Path.
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   * \param edge which edge of the contour to query, must
   *             have that 0 <= edge < number_edges(contour)
   */
  enum PathEnums::edge_type_t
  edge_type(unsigned int contour, unsigned int edge) const;

  /*!
   * Returns the bounding box of the tessellation.
   */
  const Rect&
  bounding_box(void) const;

  /*!
   * Returns this \ref TessellatedPath where any arcs are
   * realized as segments. If this \ref TessellatedPath has
   * no arcs, returns this object. If a non-positive value
   * is passed, returns a linearization where arc-segments
   * are tessellated into very few line segments.
   * \param thresh threshhold at which to linearize
   *               arc-segments.
   */
  const TessellatedPath&
  linearization(float thresh) const;

  /*!
   * Provided as a conveniance, returns the starting point tessellation.
   * Equivalent to
   * \code
   * linearization(-1.0f)
   * \endcode
   */
  const TessellatedPath&
  linearization(void) const;

  /*!
   * Returns this \ref TessellatedPath stroked. The \ref
   * StrokedPath object is constructed lazily.
   */
  const StrokedPath&
  stroked(void) const;

  /*!
   * Returns this \ref TessellatedPath filled. If this
   * \ref TessellatedPath has arcs will return
   * the fill associated to the linearization() of
   * this \ref TessellatedPath. If a non-positive value
   * is passed, returns the fill of the linearization
   * where arc-segments are tessellated into very few
   * line segments.
   * \param thresh threshhold at which to linearize
   *               arc-segments.
   */
  const FilledPath&
  filled(float thresh) const;

  /*!
   * Provided as a conveniance, returns the starting point tessellation.
   * Equivalent to
   * \code
   * filled(-1.0f)
   * \endcode
   */
  const FilledPath&
  filled(void) const;

  /*!
   * Returns the partitioning of this \ref TessellatedPath.
   */
  const PartitionedTessellatedPath&
  partitioned(void) const;

private:
  TessellatedPath(Refiner *p, float threshhold,
                  unsigned int additional_recursion_count);

  TessellatedPath(const TessellatedPath &with_arcs,
                  float thresh);

  void *m_d;
};

/*! @} */

}
