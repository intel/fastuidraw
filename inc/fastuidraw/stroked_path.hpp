/*!
 * \file stroked_path.hpp
 * \brief file stroked_path.hpp
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

namespace fastuidraw  {

///@cond
class PainterAttributeData;
class TessellatedPath;
class Path;
///@endcond

/*!\addtogroup Core
  @{
 */

/*!
  A StrokedPath represents the data needed to draw a path stroked.
  It contains -all- the data needed to stroke a path regardless of
  stroking style. in particular, for a given TessellatedPath,
  one only needs to construct a StrokedPath <i>once</i> regardless
  of how one strokes the original path for drawing.
 */
class StrokedPath:
    public reference_counted<StrokedPath>::non_concurrent
{
public:
  /*!
    Enumeration for specifing a point type.
   */
  enum point_type_t
    {
      /*!
        The point is for an edge of the path
       */
      edge_point,

      /*!
        The point is for a rounded join of the path
       */
      rounded_join_point,

      /*!
        The point is for a miter join of the path
       */
      miter_join_point,

      /*!
        The point is for a rounded cap of the path
       */
      rounded_cap_point,

      /*!
        The point is for a square cap of the path
       */
      square_cap_point
    };

  /*!
    Enumeration encoding how bits of point::m_tag are used.
   */
  enum tag_bit_layout_t
    {
      point_type_num_bits = 3,
      point_type_bit0 = 0,

      normal0_y_sign_bit = point_type_bit0 + point_type_num_bits,
      normal1_y_sign_bit = normal0_y_sign_bit + 1,
      sin_sign_bit = normal1_y_sign_bit + 1,

      point_type_mask = FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(point_type_num_bits) << point_type_bit0,
      normal0_y_sign_mask = 1 << normal0_y_sign_bit,
      normal1_y_sign_mask = 1 << normal1_y_sign_bit,
      sin_sign_mask = 1 << sin_sign_bit,
    };

  /*!
    A point holds the data for a point of stroking.
    The data is so that changing the stroking width
    or miter limit does not change the stroking data.
   */
  class point
  {
  public:
    /*!
      The base position of a point, taken directly from
      the TessellatedPath from which the
      StrokedPath is constructed.
     */
    vec2 m_position;

    /*!
      Gives the offset vector used to help compute
      the point offset vector.
    */
    vec2 m_pre_offset;

    /*!
      Provides an auxilary offset data, used ONLY for
      StrokedPath::miter_join_point points.
     */
    vec2 m_auxilary_offset;

    /*!
      Gives the distance of the point from the start
      of the -edge- on which the point resides.
     */
    float m_distance_from_edge_start;

    /*!
      Gives the distance of the point from the start
      of the -contour- on which the point resides.
     */
    float m_distance_from_contour_start;

    /*!
      Has value -1, 0 or +1. If the value is 0,
      then the point is on the path. If the value has
      absolute value 1, then indicates a point that
      is on the boundary of the stroked path. The triangles
      produced from stroking are so that when
      m_on_boundary is interpolated across the triangle
      the center of stroking the value is 0 and the
      value has absolute value +1 on the boundary.
     */
    int m_on_boundary;

    /*!
      When stroking the data, the depth test to only
      pass when the depth value is -strictly- larger
      so that a fixed pixel is not stroked twice by
      a single path. The value m_depth stores
      a relative z-value for a vertex. The points
      drawn first have the largest z-values.
     */
    unsigned int m_depth;

    /*!
      Tag is a bit field where
       - m_tag & point_type_mask gives the value to point_type()
       - m_tag & normal0_y_sign_mask up if the y-component of n0 vector is negative (rounded join points only)
       - m_tag & normal1_y_sign_mask up if the y-component of n1 vector is negative (rounded join points only)
       - m_tag & sin_sign_mask  up if the y-component of sin value is negative (rounded join points only)
     */
    uint32_t m_tag;

    /*!
      Provides the point type for the point. The value is one of the
      enumerations of StrokedPath::point_type_t. NOTE: if a point comes
      from the geometry of an edge, it is always the value
      StrokedPath::edge_point; it takes on other values for those points
      of those joins/caps that are not the geometry of the edge. In
      particular, since bevel joins do not add any points, there is no
      enumeration with for bevel joins.
     */
    enum point_type_t
    point_type(void) const
    {
      return static_cast<enum point_type_t>(m_tag & point_type_mask);
    }

    /*!
      Computes the offset vector for a point. The final position
      of a point when stroking with a width of W is given by
      \code
      m_position + 0.5f * W * offset_vector().
      \endcode
      The computation for offset_vector() is as follows.
      - For those with point_type() being StrokedPath::edge_point,
        the offset is given by
        \code
        m_pre_offset
        \endcode
      - For those with point_type() being StrokedPath::square_cap_points,
        the value is given by
        \code
        m_pre_offset + 0.5 * m_auxilary_offset
        \endcode
      - For those with point_type() being StrokedPath::miter_join_point,
        the value is given by the following code
        \code
        vec2 n = m_pre_offset, v = vec2(-n.y(), n.x());
        float r, lambda;
        lambda = -sign(dot(v, m_auxilary_offset));
        r = (dot(m_pre_offset, m_auxilary_offset) - 1.0) / dot(v, m_auxilary_offset);
        //result:
        offset = lambda * (n - r * v);
        \endcode
        To enfore a miter limit M, clamp the value r to [-M,M].
      - For those with point_type() being StrokedPath::rounded_cap_point,
        the value is given by the following code
        \code
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        offset = m_auxilary_offset.x() * v + m_auxilary_offset.y() * n;
        \endcode
      - For those with point_type() being StrokedPath::rounded_join_point,
        the value is given by the following code
        \code
        vec2 cs;

        cs.x() = m_auxilary_offset.y();
        cs.y() = sqrt(1.0 - cs.x() * cs.x());

        if(m_tag & sin_sign_mask)
          cs.y() = -cs.y();

        offset = cs
        \endcode
        In addition, the source data for join is encoded as follows:
        \code
        float t;
        vec2 n0, n1;

        t = m_auxilary_offset.x();
        n0.x() = m_offset.x();
        n1.x() = m_offset.y();

        n0.y() = sqrt(1.0 - n0.x() * n0.x());
        n1.y() = sqrt(1.0 - n1.x() * n1.x());

        if(m_tag & normal0_y_sign_mask)
          n0.y() = -n0.y();

        if(m_tag & normal1_y_sign_mask)
          n1.y() = -n1.y();
        \endcode
        The vector n0 represents the normal of the path going into the join,
        the vector n1 represents the normal of the path going out of the join
        and t represents how much to interpolate from n0 to n1.
     */
    vec2
    offset_vector(void);

    /*!
      When point_type() is miter_join_point, returns the distance
      to the miter point. For other point types, returns 0.0.
     */
    float
    miter_distance(void) const;
  };

  /*!
    Ctor. Construct a StrokedPath from the data
    of a TessellatedPath.
    \param P source TessellatedPath
   */
  explicit
  StrokedPath(const TessellatedPath &P);

  ~StrokedPath();

  /*!
    Returns the geometric data for stroking the edges.
    \param including_closing_edge if true, include the geometric data for
                                  the closing edge.
   */
  const_c_array<point>
  edge_points(bool including_closing_edge) const;

  /*!
    Returns the index data into edge_points() for stroking edges.
    \param including_closing_edge if true, include the index data for
                                  the closing edge.
   */
  const_c_array<unsigned int>
  edge_indices(bool including_closing_edge) const;

  /*!
    The vertices returned by edge_points() have
    that point::m_depth are in the half-open range
    [0, edge_number_depth())
    \param including_closing_edge if true, include the index data for
                                  the closing edge.
   */
  unsigned int
  edge_number_depth(bool including_closing_edge) const;

  /*!
    Returns the geometric data for stroking the rounded joins.
    \param including_closing_edge if true, include the geometric data for
                                  the joins of the closing edge.
   */
  const_c_array<point>
  rounded_joins_points(bool including_closing_edge) const;

  /*!
    Returns the index data into rounded_joins_points() for stroking edges.
    \param including_closing_edge if true, include the index data for
                                  the joins of the closing edge.
   */
  const_c_array<unsigned int>
  rounded_joins_indices(bool including_closing_edge) const;

  /*!
    The vertices returned by rounded_joins_indices()
    have that point::m_depth are in the half-open range
    [0, rounded_join_number_depth())
    \param including_closing_edge if true, include the index data for
                                  the joins of the closing edge.
   */
  unsigned int
  rounded_join_number_depth(bool including_closing_edge) const;

  /*!
    Returns the geometric data for stroking the bevel joins.
    \param including_closing_edge if true, include the geometric data for
                                  the joins of the closing edge.
   */
  const_c_array<point>
  bevel_joins_points(bool including_closing_edge) const;

  /*!
    Returns the index data into bevel_joins_points() for stroking edges.
    \param including_closing_edge if true, include the index data for
                                  the joins of the closing edge.
   */
  const_c_array<unsigned int>
  bevel_joins_indices(bool including_closing_edge) const;

  /*!
    The vertices returned by bevel_joins_indices()
    have that point::m_depth are in the half-open range
    [0, bevel_join_number_depth())
    \param including_closing_edge if true, include the index data for
                                  the joins of the closing edge.
   */
  unsigned int
  bevel_join_number_depth(bool including_closing_edge) const;

  /*!
    Returns the geometric data for stroking the miter joins.
    Danger: For Miter joins,
    \param including_closing_edge if true, include the geometric data for
                                  the joins of the closing edge.
   */
  const_c_array<point>
  miter_joins_points(bool including_closing_edge) const;

  /*!
    Returns the index data into miter_joins_points() for stroking edges.
    \param including_closing_edge if true, include the index data for
                                  the joins of the closing edge.
   */
  const_c_array<unsigned int>
  miter_joins_indices(bool including_closing_edge) const;

  /*!
    The vertices returned by miter_joins_indices()
    have that point::m_depth are in the half-open range
    [0, miter_join_number_depth())
    \param including_closing_edge if true, include the index data for
                                  the joins of the closing edge.
   */
  unsigned int
  miter_join_number_depth(bool including_closing_edge) const;

  /*!
    Returns the geometric data for stroking the rounded caps.
   */
  const_c_array<point>
  rounded_cap_points(void) const;

  /*!
    Returns the index data into rounded_cap_points() for the caps.
   */
  const_c_array<unsigned int>
  rounded_cap_indices(void) const;

  /*!
    Returns the geometric data for stroking the square caps.
   */
  const_c_array<point>
  square_cap_points(void) const;

  /*!
    Returns the index data into square_cap_points() for the caps.
   */
  const_c_array<unsigned int>
  square_cap_indices(void) const;

  /*!
    The vertices returned by rounded_cap_indices(),
    and square_cap_points() have that point::m_depth
    are in the half-open range [0, cap_number_depth())
   */
  unsigned int
  cap_number_depth(void) const;

  /*!
    Returns data that can be passed to a PainterPacker
    to stroke a path.
   */
  const PainterAttributeData&
  painter_data(void) const;

private:
  void *m_d;
};

/*! @} */

}
