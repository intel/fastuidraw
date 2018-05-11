/*!
 * \file stroked_point.hpp
 * \brief file stroked_point.hpp
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

#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/painter/painter_attribute.hpp>

namespace fastuidraw  {

/*!\addtogroup Paths
 * @{
 */

/*!
 * \brief
 * A StrokedPoint holds the data for a point of stroking
 * where all segments are line segments (coming from
 * curve tessellation). The upshot is that the fragment
 * shader does NOT perform any coverage computation for
 * non-dashed stroking. In addition, the data is so that
 * changing the stroking width or miter limit does not
 * change the stroking data.
 */
class StrokedPoint
{
public:
  /*!
   * \brief
   * Enumeration for specifing the point type which in turn
   * determines the meaning of the fields \ref m_pre_offset
   * and \ref m_auxiliary_offset
   */
  enum offset_type_t
    {
      /*!
       * The point is for an edge of the path, point signifies the start
       * of a sub-edge (quad) of drawing an edge. The meanings of \ref
       * m_pre_offset and \ref m_auxiliary_offset are:
       *  - \ref m_pre_offset the normal vector to the edge in which
       *                      move the point by when stroking
       *  - \ref m_auxiliary_offset when added to \ref m_position, gives
       *                            the position of the point on the other
       *                            side of the edge.
       */
      offset_sub_edge,

      /*!
       * The point is at a position that has the same value as point on
       * an edge but the point itself is part of a cap or join. The
       * meanings of the members \ref m_pre_offset and \ref
       * m_auxiliary_offset are:
       *  - \ref m_pre_offset the normal vector to the edge in which move
       *                      the point by when stroking; this vector can
       *                      be (0, 0).
       *  - \ref m_auxiliary_offset unused (set to (0, 0))
       */
      offset_shared_with_edge,

      /*!
       * The point is for a boundary point of a rounded join of the path.
       * The meanings of the members  \ref m_pre_offset and and \ref
       * m_auxiliary_offset are:
       *  - \ref m_pre_offset the .x() component holds the unit normal vector
       *                      between the join point and the edge going into
       *                      the join. The .y() component holds the unit
       *                      normal vector between the join point and the edge
       *                      leaving the join. The packing is that the
       *                      x-coordinate value is given and the y-coordinate
       *                      magnitude is \f$ sqrt(1 - x^2) \f$. If the bit
       *                      \ref normal0_y_sign_bit is up, then the y-coordinate
       *                      for the normal vector of going into the join,
       *                      then the y-value is negative. If the bit \ref
       *                      normal1_y_sign_bit is up, then the y-coordinate
       *                      for the normal vector of leaving the join, then
       *                      the y-value is negative.
       *  - \ref m_auxiliary_offset the .x() component gives an interpolation
       *                            in the range [0, 1] to interpolate between
       *                            the normal vectors packed in \ref m_pre_offset.
       *                            The .y() value gives the normal vector directly
       *                            but packed (as in \ref m_pre_offset) where the
       *                            y-coordinate sign is negative if the bit \ref
       *                            sin_sign_mask is up.
       */
      offset_rounded_join,

      /*!
       * Point type for miter-clip join point whose position depends on the
       * stroking radius and the miter-limit. The meanings of \ref m_pre_offset
       * and \ref m_auxiliary_offset are:
       * - \ref m_pre_offset gives the unit normal vector of teh edge going
       *                     into the join
       * - \ref m_auxiliary_offset gives the unit normal vector of teh edge
       *                           leaving the join
       */
      offset_miter_clip_join,

      /*!
       * Point type for miter-bevel join point whose position depends on the
       * stroking radius and the miter-limit. The meanings of \ref m_pre_offset
       * and \ref m_auxiliary_offset are:
       * - \ref m_pre_offset gives the unit normal vector of teh edge going
       *                     into the join
       * - \ref m_auxiliary_offset gives the unit normal vector of teh edge
       *                           leaving the join
       */
      offset_miter_bevel_join,

      /*!
       * Point type for miter join whose position position depends on the
       * stroking radius and the miter-limit. The meanings of \ref m_pre_offset
       * and \ref m_auxiliary_offset are:
       * - \ref m_pre_offset gives the unit normal vector of teh edge going
       *                     into the join
       * - \ref m_auxiliary_offset gives the unit normal vector of teh edge
       *                           leaving the join
       */
      offset_miter_join,

      /*!
       * The point is for a boundary point of a rounded cap of the path.
       * The meanings of \ref m_pre_offset and \ref m_auxiliary_offset
       * are:
       * - \ref m_pre_offset is the normal vector to the path to start
       *                     drawing the rounded cap
       * - \ref m_auxiliary_offset gives the the unit vector (cos, sin)
       *                           of the angle to make with the vector
       *                           given by \ref m_pre_offset
       */
      offset_rounded_cap,

      /*!
       * The point is for a boundary point of a square cap of the path,
       * the meanings of \ref m_pre_offset and \ref m_auxiliary_offset are:
       * - \ref m_pre_offset is the normal vector to the path by which
       *                     to move the point
       * - \ref m_auxiliary_offset is the tangent vector to the path
       *                           by which to move the point
       */
      offset_square_cap,

      /*!
       * The point is a point of an adjustable cap. These points are for
       * dashed stroking with caps; they contain data to allow one from a
       * vertex shader to extend or shrink the cap area correctly to implement
       * dashed stroking. The meanings of \ref m_pre_offset and \ref
       * m_auxiliary_offset are:
       * - \ref m_pre_offset is the normal vector to the path by which
       *                     to move the point; this value can be (0, 0)
       *                     to indicate to not move perpindicular to the
       *                     path
       * - \ref m_auxiliary_offset is the tangent vector to the path
       *                           by which to move the point; this value
       *                           can be (0, 0) to indicate to not move
       *                           parallel to the path
       */
      offset_adjustable_cap,

      /*!
       * Number different point types with respect to rendering
       */
      number_offset_types
    };

  /*!
   * \brief
   * Enumeration encoding of bits of \ref m_packed_data
   * common to all offset types.
   */
  enum packed_data_bit_layout_common_t
    {
      /*!
       * Bit0 for holding the offset_type() value
       * of the point.
       */
      offset_type_bit0 = 0,

      /*!
       * number of bits needed to hold the
       * offset_type() value of the point.
       */
      offset_type_num_bits = 4,

      /*!
       * Bit for holding boundary() value
       * of the point
       */
      boundary_bit = offset_type_bit0 + offset_type_num_bits,

      /*!
       * Bit0 for holding the depth() value
       * of the point
       */
      depth_bit0,

      /*!
       * number of bits needed to hold the
       * depth() value of the point.
       */
      depth_num_bits = 20,

      /*!
       * Bit to indicate point is from a join. For these
       * points, during dashed stroking. For joins, one is
       * guaranteed that the distance values for all points
       * of a fixed join are the same.
       */
      join_bit = depth_bit0 + depth_num_bits,

      /*!
       * Number of bits used on common packed data
       */
      number_common_bits,
    };

  /*!\brief
   * Enumeration encoding of bits of \ref m_packed_data
   * for those with offset type \ref offset_sub_edge
   */
  enum packed_data_sub_edge_t
    {
      /*!
       * If this bit is down indicates the point is the
       * start of a sub-edge; if the bit is up, indicates
       * that the point is the end of a subedge.
       */
      end_sub_edge_bit = number_common_bits,

      /*!
       * The bit is up if the point is for the
       * geometry of a bevel between two sub-edges.
       */
      bevel_edge_bit
    };

  /*!
   * \brief
   * Enumeration encoding of bits of \ref m_packed_data
   * for those with offset type \ref offset_rounded_join
   */
  enum packed_data_bit_layout_rounded_join_t
    {
      /*!
       * Bit for holding the the sign of
       * the y-coordinate of the normal 0
       * for the offset_type() \ref
       * offset_rounded_join.
       */
      normal0_y_sign_bit = number_common_bits,

      /*!
       * Bit for holding the the sign of
       * the y-coordinate of the normal 1
       * for the offset_type() \ref
       * offset_rounded_join.
       */
      normal1_y_sign_bit,

      /*!
       * Bit for holding the the sign of
       * sin() value for the offset_type()
       * \ref offset_rounded_join.
       */
      sin_sign_bit,
    };

  /*!
   * \brief
   * Enumeration encoding of bits of \ref m_packed_data
   * for those with offset type \ref offset_miter_clip_join.
   */
  enum packed_data_bit_layout_miter_join_t
    {
      /*!
       * Indicates that the lambda of the miter-join
       * computation should be negated.
       */
      lambda_negated_bit = number_common_bits,
    };

  /*!
   * \brief
   * Enumeration encoding of bits of \ref m_packed_data for
   * those with offset type \ref offset_adjustable_cap
   */
  enum packed_data_bit_adjustable_cap_t
    {
      /*!
       * The bit is up if the point is for end of point
       * of a cap (i.e. the side to be extended to make
       * sure the entire cap near the end of edge is drawn).
       */
      adjustable_cap_ending_bit = number_common_bits,

      /*!
       * The bit is up if the point is for cap at the
       * end of the contour.
       */
      adjustable_cap_is_end_contour_bit
    };

  /*!
   * \brief
   * Enumeration holding bit masks.
   */
  enum packed_data_bit_masks_t
    {
      /*!
       * Mask generated for \ref offset_type_bit0 and
       * \ref offset_type_num_bits
       */
      offset_type_mask = FASTUIDRAW_MASK(offset_type_bit0, offset_type_num_bits),

      /*!
       * Mask generated for \ref boundary_bit
       */
      boundary_mask = FASTUIDRAW_MASK(boundary_bit, 1),

      /*!
       * Mask generated for \ref depth_bit0 and \ref depth_num_bits
       */
      depth_mask = FASTUIDRAW_MASK(depth_bit0, depth_num_bits),

      /*!
       * Mask generated for \ref end_sub_edge_bit
       */
      end_sub_edge_mask = FASTUIDRAW_MASK(end_sub_edge_bit, 1),

      /*!
       * Mask generated for \ref bevel_edge_bit
       */
      bevel_edge_mask = FASTUIDRAW_MASK(bevel_edge_bit, 1),

      /*!
       * Mask generated for \ref normal0_y_sign_bit
       */
      normal0_y_sign_mask = FASTUIDRAW_MASK(normal0_y_sign_bit, 1),

      /*!
       * Mask generated for \ref normal1_y_sign_bit
       */
      normal1_y_sign_mask = FASTUIDRAW_MASK(normal1_y_sign_bit, 1),

      /*!
       * Mask generated for \ref sin_sign_bit sin_sign_bit
       */
      sin_sign_mask = FASTUIDRAW_MASK(sin_sign_bit, 1),

      /*!
       * Mask generated for \ref lambda_negated_mask
       */
      lambda_negated_mask = FASTUIDRAW_MASK(lambda_negated_bit, 1),

      /*!
       * Mask generated for \ref join_bit
       */
      join_mask = FASTUIDRAW_MASK(join_bit, 1),

      /*!
       * Mask generated for \ref adjustable_cap_ending_bit
       */
      adjustable_cap_ending_mask = FASTUIDRAW_MASK(adjustable_cap_ending_bit, 1),

      /*!
       * Mask generated for \ref adjustable_cap_is_end_contour_bit
       */
      adjustable_cap_is_end_contour_mask = FASTUIDRAW_MASK(adjustable_cap_is_end_contour_bit, 1),
    };

  /*!
   * The base position of a point before applying the stroking width
   * to the position.
   */
  vec2 m_position;

  /*!
   * Gives values to help compute the location of the point after
   * applying the stroking width. See the descriptions to the
   * elements of \ref offset_type_t for its meaning for different
   * offset types.
   */
  vec2 m_pre_offset;

  /*!
   * Gives values to help compute the location of the point after
   * applying the stroking width. See the descriptions to the
   * elements of \ref offset_type_t for its meaning for different
   * offset types.
   */
  vec2 m_auxiliary_offset;

  /*!
   * Gives the distance of the point from the start
   * of the -edge- on which the point resides.
   */
  float m_distance_from_edge_start;

  /*!
   * Gives the distance of the point from the start
   * of the -contour- on which the point resides.
   */
  float m_distance_from_contour_start;

  /*!
   * Gives the length of the edge on which the
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

  /*!
   * Bit field with data packed as according to
   * \ref packed_data_bit_layout_common_t, \ref
   * packed_data_bit_layout_rounded_join_t, \ref
   * packed_data_bit_adjustable_cap_t and \ref
   * packed_data_bit_sub_edge_t. See also,
   * \ref packed_data_bit_masks_t for bit masks
   * generated.
   */
  uint32_t m_packed_data;

  /*!
   * Provides the point type from a value of \ref m_packed_data.
   * The return value is one of the enumerations of
   * \ref offset_type_t.
   * \param packed_data_value value suitable for \ref m_packed_data
   */
  static
  enum offset_type_t
  offset_type(uint32_t packed_data_value)
  {
    uint32_t v;
    v = unpack_bits(offset_type_bit0, offset_type_num_bits, packed_data_value);
    return static_cast<enum offset_type_t>(v);
  }

  /*!
   * Provides the point type for the point. The return value
   * is one of the enumerations of \ref offset_type_t.
   */
  enum offset_type_t
  offset_type(void) const
  {
    return offset_type(m_packed_data);
  }

  /*!
   * When stroking the data, the depth test to only
   * pass when the depth value is -strictly- larger
   * so that a fixed pixel is not stroked twice by
   * a single path. The value returned by depth() is
   * a relative z-value for a vertex. The points
   * drawn first have the largest z-values.
   */
  uint32_t
  depth(void) const
  {
    return unpack_bits(depth_bit0, depth_num_bits, m_packed_data);
  }

  /*!
   * Has value 0 or 1. If the value is 0, then
   * the point is on the path. If the value has
   * absolute value 1, then indicates a point that
   * is on the boundary of the stroked path. The triangles
   * produced from stroking are so that when
   * on_boundary() is interpolated across the triangle
   * the center of stroking the value is 0 and the
   * value has value 1 on the boundary.
   */
  int
  on_boundary(void) const
  {
    return unpack_bits(boundary_bit, 1u, m_packed_data);
  }

  /*!
   * When offset_type() is one of \ref offset_miter_clip_join,
   * \ref offset_miter_bevel_join or \ref offset_miter_join,
   * returns the distance to the miter point. For other point
   * types, returns 0.0.
   */
  float
  miter_distance(void) const;

  /*!
   * Pack the data of this \ref StrokedPoint into a \ref
   * PainterAttribute. The packing is as follows:
   * - PainterAttribute::m_attrib0 .xy -> \ref m_position (float)
   * - PainterAttribute::m_attrib0 .zw -> \ref m_pre_offset (float)
   * - PainterAttribute::m_attrib1 .x  -> \ref m_distance_from_edge_start (float)
   * - PainterAttribute::m_attrib1 .y  -> \ref m_distance_from_contour_start (float)
   * - PainterAttribute::m_attrib1 .zw -> \ref m_auxiliary_offset (float)
   * - PainterAttribute::m_attrib2 .x  -> \ref m_packed_data (uint)
   * - PainterAttribute::m_attrib2 .y  -> \ref m_edge_length (float)
   * - PainterAttribute::m_attrib2 .z  -> \ref m_open_contour_length (float)
   * - PainterAttribute::m_attrib2 .w  -> \ref m_closed_contour_length (float)
   *
   * \param dst PainterAttribute to which to pack
   */
  void
  pack_point(PainterAttribute *dst) const;

  /*!
   * Unpack a \ref StrokedPoint from a \ref PainterAttribute.
   * \param dst point to which to unpack data
   * \param src PainterAttribute from which to unpack data
   */
  static
  void
  unpack_point(StrokedPoint *dst, const PainterAttribute &src);
};


/*! @} */

}
