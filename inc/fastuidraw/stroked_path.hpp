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
    Enumeration for specifing how to compute
    StrokedPath::point::offset_vector()).
   */
  enum offset_type_t
    {
      /*!
        The point is for an edge of the path,
        point signifies the start of a sub-edge
        (quad) of drawing an edge.
       */
      offset_edge,

      /*!
        The point is for an edge of the path,
        point signifies the end of a sub-edge
        (quad) of drawing an edge.
       */
      offset_next_edge,

      /*!
        The point is at a position that has the
        same value as point on an edge
       */
      offset_shared_with_edge,

      /*!
        The point is for a boundary point of a rounded join of the path
       */
      offset_rounded_join,

      /*!
        The point is for a boundary point of a miter join of the path
       */
      offset_miter_join,

      /*!
        The point is for a boundary point of a rounded cap of the path
       */
      offset_rounded_cap,

      /*!
        The point is for a boundary point of a square cap of the path
       */
      offset_square_cap,

      /*!
        The point is for a boundary point of a sqaure-cap join point.
        These points are for dashed stroking when the point of the join
        is NOT covered by the dash pattern. Their layout of data is the
        same as \ref offset_miter_join. The purpose of this point type is
        to make sure caps of dashed stroking is drawn at the join location.
        When placing such points, it is placed the same as \ref
        offset_miter_join except that the miter limit is 0.5.
       */
      offset_cap_join,

      /*!
        Number different point types with respect to rendering
       */
      number_offset_types
    };

  /*!
    Enumeration to select what points of stroking to select.
   */
  enum point_set_t
    {
      /*!
        Select the set of points for edges
       */
      edge_point_set,

      /*!
        Select the set of points for bevel joins
       */
      bevel_join_point_set,

      /*!
        Select the set of points for rounded joins
       */
      rounded_join_point_set,

      /*!
        Select the set of points for miter joins
       */
      miter_join_point_set,

      /*!
        Select the set of points for square caps
       */
      square_cap_point_set,

      /*!
        Select the set of points for rouded caps
       */
      rounded_cap_point_set,

      /*!
        Select the set of points for cap joins
       */
      cap_join_point_set,

      /*!
        Number point set types
       */
      number_point_set_types
    };

  /*!
    Enumeration encoding how bits of point::m_packed_data are used.
   */
  enum packed_data_bit_layout_t
    {
      /*!
        Bit0 for holding the offset_type() value
        of the point.
       */
      offset_type_bit0 = 0,

      /*!
        number of bits needed to hold the
        offset_type() value of the point.
       */
      offset_type_num_bits = 4,

      /*!
        Bit for holding the the sign of
        the y-coordinate of the normal 0
        for the offset_type() \ref
        offset_rounded_join.
       */
      normal0_y_sign_bit = offset_type_bit0 + offset_type_num_bits,

      /*!
        Bit for holding the the sign of
        the y-coordinate of the normal 1
        for the offset_type() \ref
        offset_rounded_join.
       */
      normal1_y_sign_bit = normal0_y_sign_bit + 1,

      /*!
        Bit for holding the the sign of
        sin() value for the offset_type()
        \ref offset_rounded_join.
       */
      sin_sign_bit = normal1_y_sign_bit + 1,

      /*!
        Bit for holding boundary() value
        of the point
       */
      boundary_bit = sin_sign_bit + 1,

      /*!
        Bit to indicate point is from a join set.
       */
      join_bit = boundary_bit + 1,

      /*!
        Bit0 for holding the depth() value
        of the point
       */
      depth_bit0 = join_bit + 1,

      /*!
        number of bits needed to hold the
        depth() value of the point.
       */
      depth_num_bits = 20,

      /*!
        If this bit is up, indicates that when dashed stroking,
        the triangle generated by such vertices is drawn regardless
        of the dash pattern.
       */
      skip_dash_computation_bit = depth_bit0 + depth_num_bits,

      /*!
        Mask generated for \ref offset_type_bit0 and
        \ref offset_type_num_bits
       */
      offset_type_mask = FASTUIDRAW_MASK(offset_type_bit0, offset_type_num_bits),

      /*!
        Mask generated for \ref normal0_y_sign_bit
       */
      normal0_y_sign_mask = FASTUIDRAW_MASK(normal0_y_sign_bit, 1),

      /*!
        Mask generated for \ref normal1_y_sign_bit
       */
      normal1_y_sign_mask = FASTUIDRAW_MASK(normal1_y_sign_bit, 1),

      /*!
        Mask generated for \ref sin_sign_bit sin_sign_bit
       */
      sin_sign_mask = FASTUIDRAW_MASK(sin_sign_bit, 1),

      /*!
        Mask generated for \ref boundary_bit
       */
      boundary_mask = FASTUIDRAW_MASK(boundary_bit, 1),

      /*!
        Mask generated for \ref join_bit
       */
      join_mask = FASTUIDRAW_MASK(join_bit, 1),

      /*!
        Mask generated for \ref depth_bit0 and \ref depth_num_bits
       */
      depth_mask = FASTUIDRAW_MASK(depth_bit0, depth_num_bits),

      /*!
        Mask generated for \ref skip_dash_computation_bit
       */
      skip_dash_computation_mask = FASTUIDRAW_MASK(skip_dash_computation_bit, 1)
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
      Provides an auxilary offset data
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
      Gives the length of the edge on which the
      point lies. This value is the same for all
      points along a fixed edge.
     */
    float m_edge_length;

    /*!
      Gives the length of the contour open on which
      the point lies. This value is the same for all
      points along a fixed contour.
     */
    float m_open_contour_length;

    /*!
      Gives the length of the contour closed on which
      the point lies. This value is the same for all
      points along a fixed contour.
     */
    float m_closed_contour_length;

    /*!
      Bit field with data packed as according to \ref packed_data_bit_layout_t.
     */
    uint32_t m_packed_data;

    /*!
      Provides the point type for the point. The value is one of the
      enumerations of StrokedPath::offset_type_t.
     */
    enum offset_type_t
    offset_type(void) const
    {
      uint32_t v;
      v = unpack_bits(offset_type_bit0, offset_type_num_bits, m_packed_data);
      return static_cast<enum offset_type_t>(v);
    }

    /*!
      When stroking the data, the depth test to only
      pass when the depth value is -strictly- larger
      so that a fixed pixel is not stroked twice by
      a single path. The value returned by depth() is
      a relative z-value for a vertex. The points
      drawn first have the largest z-values.
     */
    uint32_t
    depth(void) const
    {
      return unpack_bits(depth_bit0, depth_num_bits, m_packed_data);
    }

    /*!
      Has value 0 or +1. If the value is 0, then
      the point is on the path. If the value has
      absolute value 1, then indicates a point that
      is on the boundary of the stroked path. The triangles
      produced from stroking are so that when
      m_on_boundary is interpolated across the triangle
      the center of stroking the value is 0 and the
      value has absolute value +1 on the boundary.
     */
    int
    on_boundary(void) const
    {
      return unpack_bits(boundary_bit, 1u, m_packed_data);
    }

    /*!
      When performing dashed stroking, some stroke data sent
      to the shader is so that the triangle it generates
      is that it is covered regardless of dash pattern.
      This returns true (by checking if the bit \ref
      skip_dash_computation_bit is up) if the point is
      such a point (if a triangle has a point with this
      true, all the points have it true).
     */
    bool
    skip_dash_computation(void) const
    {
      return m_packed_data & skip_dash_computation_mask;
    }

    /*!
      Computes the offset vector for a point. The final position
      of a point when stroking with a width of W is given by
      \code
      m_position + 0.5f * W * offset_vector().
      \endcode
      The computation for offset_vector() is as follows.
      - For those with offset_type() being StrokedPath::offset_edge,
        StrokedPath::offset_next_edge and StrokedPath::offset_shared_with_edge,
        the offset is given by
        \code
        m_pre_offset
        \endcode
        In addition, for types StrokedPath::offset_edge and
        StrokedPath::offset_next_edge, \ref m_auxilary_offset
        holds the the delta vector to the point on edge with
        which the point makes a quad.
      - For those with offset_type() being StrokedPath::offset_square_cap,
        the value is given by
        \code
        m_pre_offset + 0.5 * m_auxilary_offset
        \endcode
        In addition, \ref m_auxilary_offset holds the vector leaving
        from the contour where the cap is located.
      - For those with offset_type() being StrokedPath::offset_miter_join
        or StrokedPath::cap_join_point, the value is given by the following
        code
        \code
        vec2 n = m_pre_offset, v = vec2(-n.y(), n.x());
        float r, lambda;
        lambda = -sign(dot(v, m_auxilary_offset));
        r = (dot(m_pre_offset, m_auxilary_offset) - 1.0) / dot(v, m_auxilary_offset);
        //result:
        offset = lambda * (n - r * v);
        \endcode
        To enfore a miter limit M, clamp the value r to [-M,M].
      - For those with offset_type() being StrokedPath::offset_rounded_cap,
        the value is given by the following code
        \code
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        offset = m_auxilary_offset.x() * v + m_auxilary_offset.y() * n;
        \endcode
      - For those with offset_type() being StrokedPath::offset_rounded_join,
        the value is given by the following code
        \code
        vec2 cs;

        cs.x() = m_auxilary_offset.y();
        cs.y() = sqrt(1.0 - cs.x() * cs.x());

        if(m_packed_data & sin_sign_mask)
          cs.y() = -cs.y();

        offset = cs
        \endcode
        In addition, the source data for join is encoded as follows:
        \code
        float t;
        vec2 n0, n1;

        t = m_auxilary_offset.x();
        n0.x() = m_pre_offset.x();
        n1.x() = m_pre_offset.y();

        n0.y() = sqrt(1.0 - n0.x() * n0.x());
        n1.y() = sqrt(1.0 - n1.x() * n1.x());

        if(m_packed_data & normal0_y_sign_mask)
          n0.y() = -n0.y();

        if(m_packed_data & normal1_y_sign_mask)
          n1.y() = -n1.y();
        \endcode
        The vector n0 represents the normal of the path going into the join,
        the vector n1 represents the normal of the path going out of the join
        and t represents how much to interpolate from n0 to n1.
     */
    vec2
    offset_vector(void);

    /*!
      When offset_type() is offset_miter_join, returns the distance
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
    Returns the geomtric data for stroking the path. The backing data
    store for with and without closing edge data is shared so that
    \code
    points(tp, false) == points(tp, true).sub_array(0, points(tp,false).size())
    \endcode
    i.e., the geometric data for the closing edge comes at the end.
    \param tp what data to fetch, i.e. edge data, join data (which join data), etc.
    \param including_closing_edge if true, include the geometric data for of the
                                  closing edge. Asking for caps ignores the value
                                  for closing edge.
   */
  const_c_array<point>
  points(enum point_set_t tp, bool including_closing_edge) const;

  /*!
    Return the index data into as returned by points() for stroking
    the path. The backing data store for with and without closing edge
    data is shared so that
    \code
    unsigned int size_with, size_without;
    size_with = indices(tp, true);
    size_without  = indices(tp, false);
    assert(size_with >= size_without);
    assert(indices(tp, true).sub_array(size_with - size_without) == indices(tp, false))
    \endcode
    i.e., the index data for the closing edge is at the start of the
    index array.
    \param tp what data to fetch, i.e. edge data, join data (which join data), etc.
    \param including_closing_edge if true, include the index data for of the
                                  closing edge. Asking for caps ignores the value
                                  for closing edge.
   */
  const_c_array<unsigned int>
  indices(enum point_set_t tp, bool including_closing_edge) const;

  /*!
    Points returned by points(tp, including_closing_edge) have that
    their value for point::m_depth are in the half-open range
    [0, number_depth(tp, including_closing_edge))
    \param tp what data to fetch, i.e. edge data, join data (which join
              data), etc.
    \param including_closing_edge if true, include the index data for
                                  the closing edge.
   */
  unsigned int
  number_depth(enum point_set_t tp, bool including_closing_edge) const;

  /*!
    Returns the number of contours of the generating path.
   */
  unsigned int
  number_contours(void) const;

  /*!
    Returns the number of joins for the named contour
    of the generating path. Joint numbering is so that
    join A is the join that connects edge A to A + 1.
    In particular the joins of a closing edge of contour
    C are then at number_joins(C) - 2 and number_joins(C) - 1.
    \param contour which contour, with contour < number_contours().
   */
  unsigned int
  number_joins(unsigned int contour) const;

  /*!
    Returns the range into points(tp, true) for the
    indices of the named join or cap of the named contour.
    \param tp what join type to query. If tp is not a type for a
              join or cap, returns an empty range.
    \param contour which contour, with contour < number_contours().
    \param J if tp is a joint type, gives which join with J < number_joins(contour).
             if tp is a cap type, gives which cap with J = 0 meaning the
             cap at the start of the contour and J = 1 the cap at the
             end of the contour
   */
  range_type<unsigned int>
  points_range(enum point_set_t tp, unsigned int contour, unsigned int J) const;

  /*!
    Returns the range into indices(tp, true) for the
    indices of the named join or cap of the named contour.
    \param tp what join type to query. If tp is not a type for a
              join or cap, returns an empty range.
    \param contour which contour, with contour < number_contours().
    \param J if tp is a joint type, gives which join with J < number_joins(contour).
             if tp is a cap type, gives which cap with J = 0 meaning the
             cap at the start of the contour and J = 1 the cap at the
             end of the contour
   */
  range_type<unsigned int>
  indices_range(enum point_set_t tp, unsigned int contour, unsigned int J) const;

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
