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
        The point is for a bevel join of the path
       */
      bevel_join_point,

      /*!
        The point is for a rounded cap of the path
       */
      rounded_cap_point,

      /*!
        The point is for a squre cap of the path
       */
      square_cap_point
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
     For non-miter points, the position of the attribute data is
     given by m_position + stroking_width * pre_m_offset.
     For points on the boundary of stroking, m_pre_offset is
     of unit length. For points in the interior of stroking,
     m_pre_offset is zero.

     For miter-points, the computation is dependent on
     the miter-limit and m_miter_distance. The methods
     offset_vector(void) const and offset_vector(float) const
     provide the offset vector needed. For reference, when
     the miter limit is greater in magnitude that
     m_miter_distance, formula is given by:
     \code
        m_pre_offset + m_miter_distance * vec2(-m_pre_offset.y(), m_pre_offset.x())
     \endcode
     When the miter limit is smaller in magnitude than
     m_miter_distance it is then given by
     \code
        m_pre_offset + sign(m_miter_distance) * miter_limit * vec2(-m_pre_offset.y(), m_pre_offset.x())
     \endcode
     */
    vec2 m_pre_offset;

    /*!
      Gives the distance of the point from the start
      of the -edge- on which the point resides.
     */
    float m_distance_from_edge_start;

    /*!
      Gives the distance of the point from the start
      of the -outline- on which the point resides.
     */
    float m_distance_from_outline_start;

    /*!
      For a miter point, the he absolute values gives the
      distance of miter point to the edge of the stroking
      in units of stroking width. If the point is not a
      miter point, value is zero.
     */
    float m_miter_distance;

    /*!
      Has value -1.0, 0.0 or +1.0. If the value is 0.0,
      then the point is on the path. If the value has
      absolute value 1.0, then indicates a point that
      is on the boundary of the stroked path. The triangles
      produced from stroking are so that when
      m_on_boundary is interpolated across the triangle
      the center of stroking the value is 0 and the
      value has absolute value +1.0 on the boundary.
     */
    float m_on_boundary;

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
      Provides the point type for the point.
      The value is one of the enumerations of
      point_type_t.
     */
    enum point_type_t m_point_type;

    /*!
      Returns true if the return value to offset_vector(float)
      depends on the miter limit
     */
    bool
    depends_on_miter_limit(void) const;

    /*!
      Given a miter limit, returns the offset vector.
      Return the offset vector so that miter point
      is clamed to be within the miter limit. The
      final position of the point is given by
      m_position + stroking_width * offset_vector().
     */
    vec2
    offset_vector(float miter_limit) const;

    /*!
      Returns the offset vector ignoring any miter limit.
     */
    vec2
    offset_vector(void) const
    {
      vec2 miter_offset(-m_pre_offset.y(), m_pre_offset.x());
      return m_pre_offset + m_miter_distance * miter_offset;
    }
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
