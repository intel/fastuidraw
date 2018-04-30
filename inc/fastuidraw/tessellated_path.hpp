/*!
 * \file tessellated_path.hpp
 * \brief file tessellated_path.hpp
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
#include <fastuidraw/painter/stroked_caps_joins.hpp>

namespace fastuidraw  {

///@cond
class Path;
class StrokedPath;
class FilledPath;
///@endcond

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * A TessellatedPath represents the tessellation of a Path.
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
   * Enumeration to specify the type of threshhold
   * a \ref TessellationParams is.
   */
  enum threshhold_type_t
    {
      /*!
       *  Threshhold specifies how much distance is between
       *  line segment between from successive points of a
       *  tessellated edge and the sub-curve of the starting
       *  curve between those two points.
       */
      threshhold_curve_distance,

      number_threshholds,
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
      m_threshhold_type(threshhold_curve_distance),
      m_threshhold(1.0f),
      m_max_segments(32)
    {}

    /*!
     * Non-equal comparison operator.
     * \param rhs value to which to compare against
     */
    bool
    operator!=(const TessellationParams &rhs) const
    {
      return m_threshhold_type != rhs.m_threshhold_type
        || m_threshhold != rhs.m_threshhold
        || m_max_segments != rhs.m_max_segments;
    }

    /*!
     * Provided as a conveniance. Equivalent to
     * \code
     * m_threshhold_type = tp;
     * \endcode
     * \param tp value to which to assign to \ref m_threshhold_type
     */
    TessellationParams&
    threshhold_type(enum threshhold_type_t tp)
    {
      m_threshhold_type = tp;
      return *this;
    }

    /*!
     * Provided as a conveniance. Equivalent to
     * \code
     * m_threshhold_type = tp;
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
     * Provided as a conveniance. Equivalent to
     * \code
     * m_threshhold_type = threshhold_curve_distance;
     * m_threshhold = p;
     * \endcode
     * \param p value to which to assign to \ref m_threshhold
     */
    TessellationParams&
    curve_distance_tessellate(float p)
    {
      m_threshhold_type = threshhold_curve_distance;
      m_threshhold = p;
      return *this;
    }

    /*!
     * Set the value of \ref m_max_segments.
     * \param v value to which to assign to \ref m_max_segments
     */
    TessellationParams&
    max_segments(unsigned int v)
    {
      m_max_segments = v;
      return *this;
    }

    /*!
     * Specifies the meaning of \ref m_threshhold.
     * Default value is \ref threshhold_curve_distance.
     */
    enum threshhold_type_t m_threshhold_type;

    /*!
     * Meaning depends on \ref m_threshhold_type.
     * Default value is M_PI / 30.0.
     */
    float m_threshhold;

    /*!
     * Maximum number of segments to tessellate each
     * PathContour::interpolator_base from each
     * PathContour of a Path. Default value is 32.
     */
    unsigned int m_max_segments;
  };

  /*!
   * \brief
   * Represents point of a tessellated path.
   */
  class point
  {
  public:
    /*!
     * The position of the point.
     */
    vec2 m_p;

    /*!
     * Gives the distance of the point from the start
     * of the edge (i.e PathContour::interpolator_base)
     * on which the point resides.
     */
    float m_distance_from_edge_start;

    /*!
     * Gives the distance of the point from the start
     * of the -contour- on which the point resides.
     */
    float m_distance_from_contour_start;

    /*!
     * Gives the length of the edge (i.e.
     * PathContour::interpolator_base) on which the
     * point lies. This value is the same for all
     * points along a fixed edge.
     */
    float m_edge_length;

    /*!
     * Gives the length of the contour open on which
     * the point lies. This value is the same for all
     * points along a fixed contour.
     */
    float m_open_contour_length;

    /*!
     * Gives the length of the contour closed on which
     * the point lies. This value is the same for all
     * points along a fixed contour.
     */
    float m_closed_contour_length;
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
   * Returns the tessellation threshold achieved for
   * the named threshhold type.
   */
  float
  effective_threshhold(enum threshhold_type_t tp) const;

  /*!
   * Returns the maximum number of segments any edge needed
   */
  unsigned int
  max_segments(void) const;

  /*!
   * Returns all the point data
   */
  c_array<const point>
  point_data(void) const;

  /*!
   * Returns the number of contours
   */
  unsigned int
  number_contours(void) const;

  /*!
   * Returns the range into point_data()
   * for the named contour. The contour data is a
   * sequence of points specifing a line strip.
   * Points that are shared between edges (an edge
   * corresponds to a PathContour::interpolator_base) are
   * replicated (because the derivatives are different).
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  range_type<unsigned int>
  contour_range(unsigned int contour) const;

  /*!
   * Returns the range into point_data()
   * for the named contour lacking the closing
   * edge. The contour data is a
   * sequence of points specifing a line strip.
   * Points that are shared between edges (an edge
   * corresponds to a PathContour::interpolator_base) are
   * replicated (because the derivatives are different).
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  range_type<unsigned int>
  unclosed_contour_range(unsigned int contour) const;

  /*!
   * Returns the point data of the named contour
   * including the closing edge. The contour data is a
   * sequence of points specifing a line strip.
   * Points that are shared between edges (an edge
   * corresponds to a PathContour::interpolator_base) are
   * replicated (because the derivatives are different).
   * Provided as a conveniance, equivalent to
   * \code
   * point_data().sub_array(contour_range(contour))
   * \endcode
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  c_array<const point>
  contour_point_data(unsigned int contour) const;

  /*!
   * Returns the point data of the named contour
   * lacking the point data of the closing edge.
   * The contour data is a sequence of points
   * specifing a line strip. Points that are
   * shared between edges (an edge corresponds to
   * a PathContour::interpolator_base) are replicated
   * (because the derivatives are different).
   * Provided as a conveniance, equivalent to
   * \code
   * point_data().sub_array(unclosed_contour_range(contour))
   * \endcode
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  c_array<const point>
  unclosed_contour_point_data(unsigned int contour) const;

  /*!
   * Returns the number of edges for the named contour
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   */
  unsigned int
  number_edges(unsigned int contour) const;

  /*!
   * Returns the range into point_data(void)
   * for the named edge of the named contour.
   * The returned range does include the end
   * point of the edge. The edge corresponds
   * to the PathContour::interpolator_base
   * Path::contour(contour)->interpolator(edge).
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   * \param edge which edge of the contour to query, must
   *             have that 0 <= edge < number_edges(contour)
   */
  range_type<unsigned int>
  edge_range(unsigned int contour, unsigned int edge) const;

  /*!
   * Returns the point data of the named edge of the
   * named contour, provided as a conveniance, equivalent
   * to
   * \code
   * point_data().sub_array(edge_range(contour, edge))
   * \endcode
   * \param contour which path contour to query, must have
   *                that 0 <= contour < number_contours()
   * \param edge which edge of the contour to query, must
   *             have that 0 <= edge < number_edges(contour)
   */
  c_array<const point>
  edge_point_data(unsigned int contour, unsigned int edge) const;

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
   * Returns this TessellatedPath stroked. The StrokedPath object
   * is constructed lazily.
   */
  const reference_counted_ptr<const StrokedPath>&
  stroked(void) const;

  /*!
   * Returns this TessellatedPath filled. The FilledPath object
   * is constructed lazily.
   */
  const reference_counted_ptr<const FilledPath>&
  filled(void) const;

private:
  void *m_d;
};

/*! @} */

}
