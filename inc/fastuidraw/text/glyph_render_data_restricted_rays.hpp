/*!
 * \file glyph_render_data_restricted_rays.hpp
 * \brief file glyph_render_data_restricted_rays.hpp
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

#include <fastuidraw/text/glyph_render_data.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * A GlyphRenderDataRestrictedRays represents the data needed to
   * build a glyph to render it with a modification to the technique
   * of "GPU-Centered Font Rendering Directly from Glyph Outlines"
   * by Eric Lengyel. The modifications we have to the technique are
   *  - We break the glyph's box into a hierarchy of boxes where
   *    each leaf node has a list of what curves are in the box
   *    together with 4 sample points inside the box (near the
   *    corners of the box) giving the winding number at each box.
   *  - To compute the winding number, one runs the technique
   *    on the ray connecting the fragment position to one of the
   *    winding sample position and increment the value by the
   *    winding value of the sample. Here the main caveat is that
   *    one needs to ignore any intersection that are not between
   *    the fragment position and the sample position.
   *  - The shader should use the two sample points furthest from
   *    the fragment position to get better anti-alias results.
   */
  class GlyphRenderDataRestrictedRays:public GlyphRenderData
  {
  public:
    /*!
     * Enumeration to describe the hierarchy of bounding
     * boxes as packed into the geometry data. A node
     * in the hierarchy is a single 32-bit value. A leaf
     * in the hierarchy is a single 32-bit value followed
     * by 4 sample winding positions each packed as according
     * to \ref winding_sample_packing_t. Each sample point
     * is associated to a corner of the box. The order of
     * winding sample points is given by \ref
     * GlyphAttribute::corner_t.
     */
    enum hierarchy_packing_t
      {
        /*!
         * If this bit is up, indicates that the 32-bit value
         * is holding node data. If the bit is down indicates
         * that the element is a leaf and the value holds the
         * properties of the curve list in the box and the next
         * four values hold the windind sample informatin for
         * box and are packed as according to \ref
         * winding_sample_packing_t.
         */
        hierarchy_is_node_bit = 31u,

        /*!
         * For case where the element is a node, i.e. the
         * bit \ref hierarchy_is_node_bit is up. This bit
         * indicates if the split of the node is horizontal
         * of verical. A value of 0 indicates that the split
         * happens in the x-coordinate (i.e. the child nodes
         * have the same values for min-y and max-y) and a
         * value of 1 indicates the split happens in the
         * y-coordinate.
         */
        hierarchy_splitting_coordinate_bit = 30u,

        /*!
         * For case where the element is a node, i.e. the
         * bit \ref hierarchy_is_node_bit is up. This bit
         * indicates if the split of the node is horizontal
         * of verical. This is the first bit holding the
         * offset from the  start of the geomertic data of
         * the glyph for the  a child node which comes
         * before the split, i.e. the child on the left
         * or bottom side.
         */
        hierarchy_child0_offset_bit0 = 0u,

        /*!
         * For case where the element is a node, i.e. the
         * bit \ref hierarchy_is_node_bit is up. This bit
         * indicates if the split of the node is horizontal
         * of verical. This is the first bit holding the
         * offset from the  start of the geomertic data of
         * the glyph for the  a child node which comes
         * before the split, i.e. the child on the right
         * or top side.
         */
        hierarchy_child1_offset_bit0 = 15u,

        /*!
         * This is the number of bits used to store the
         * offsets to a child node.
         */
        hierarchy_child_offset_numbits = 15u,

        /*!
         * For case where the element is leaf, i.e. the
         * bit \ref hierarchy_is_node_bit is down. This
         * is the first bit used to encode the offset
         * to where the list of curves for the box is
         * located. The list of curves is packed as
         * according to \ref curve_list_packing_t.
         */
        hierarchy_leaf_curve_list_bit0 = 0u,

        /*!
         * For case where the element is leaf, i.e. the
         * bit \ref hierarchy_is_node_bit is down. This
         * is the number bit used to encode the offset
         * to where the list of curves for the box is
         * located. The list of curves is packed as
         * according to \ref curve_list_packing_t.
         */
        hierarchy_leaf_curve_list_numbits = 16u,

        /*!
         * For case where the element is leaf, i.e. the
         * bit \ref hierarchy_is_node_bit is down. This
         * is the first bit used to encode the size
         * of the list of curves for the box is located.
         * The list of curves is packed as according to
         * \ref curve_list_packing_t.
         */
        hierarchy_leaf_curve_list_size_bit0 = 16u,

        /*!
         * For case where the element is leaf, i.e. the
         * bit \ref hierarchy_is_node_bit is down. This
         * is the number of bits used to encode the size
         * of the list of curves for the box is located.
         * The list of curves is packed as according to
         * \ref curve_list_packing_t.
         */
        hierarchy_leaf_curve_list_size_numbits = 15u,
      };

    /*!
     * Enumeration to describe how the winding samples
     * of a leaf-box of the hierarchy are packed. Each
     * winding sample value is 32-bits and they come in
     * the order specified by \ref GlyphAttribute::corner_t.
     * The sample point is located at the named corner
     * offset by a delta. The sample point is offset from
     * the corner to which it is associated. The offset is
     * stored with a bias and fractionally. The value of
     * the offset from the corner in each coordinate is
     * given by
     * $ float(PackedValue - \ref delta_bias) / float(\ref delta_div_factor) $
     * where PackedValue is extracted from the 32-bit value.
     */
    enum winding_sample_packing_t
      {
        /*!
         * Winding values are stored biased (in order
         * to be able to store negative winding values)
         * This is the value to add to the unpacked
         * winding number found at bit \ref
         * winding_value_bit0
         */
        winding_bias = 32768u,

        /*!
         * The first bit used to encode the winding value
         * (which is stored biased by \ref winding_bias).
         */
        winding_value_bit0 = 0u,

        /*!
         * The number of bits used to encode the winding value
         * (which is stored biased by \ref winding_bias).
         */
        winding_value_numbits = 16u,

        /*!
         * The amount by which to bias the x and y-coordinate
         * of the delta before applying \ref delta_div_factor.
         */
        delta_bias = 127,

        /*!
         * The amount by which to divide the x and y-coordinate
         * of the delta after applying the bias \ref delta_bias.
         */
        delta_div_factor = 7,

        /*!
         * The first bit used to store the delta x-coordinate
         */
        delta_x_bit0 = 16u,

        /*!
         * The first bit used to store the delta y-coordinate
         */
        delta_y_bit0 = 24u,

        /*!
         * The number of bits used to store the delta x-coordinate
         * and delta y-coordinate values.
         */
        delta_numbits = 8u,
      };

    /*!
     * Enumeration to describe how a list of curves is packed.
     * Each 32-bit value holds the data for two curves.
     * A curve entry is 16-bit value whose highest bit gives the
     * degree of the curve and the remaining 15-bits give the
     * offset to the location of the curve's control points.
     */
    enum curve_list_packing_t
      {
        /*!
         * The number of bits to store a single curve entry.
         */
        curve_numbits = 16u,

        /*!
         * The first bit used for the first curve of the entry.
         */
        curve_entry0_bit0 = 0u,

        /*!
         * The first bit used for the second curve of the entry.
         */
        curve_entry1_bit0 = 16u,

        /*!
         * Given an unpacked curve entry (which is 16-bits wide),
         * if this bit of the value is up, then the curve referenced
         * is a quadratic Bezier curve having control points. Otherwise,
         * it is a line segment connecting its two points.
         */
        curve_is_quadratic_bit = 15u,

        /*!
         * Given an unpacked curve entry (which is 16-bits wide),
         * this is the first bit used to store the offset to the
         * location of the points of the curve (packed as according
         * to \ref point_packing_t).
         */
        curve_location_bit0 = 0u,

        /*!
         * Given an unpacked curve entry (which is 16-bits wide),
         * this is the number of bits used to store the offset to the
         * location of the points of the curve (packed as according
         * to \ref point_packing_t).
         */
        curve_location_numbits = 15u,
      };

    /*!
     * This enumeration specifies how the points of a curve are packed.
     * Each point is realized as a single 32-bit value. Both the
     * x and y-coordinates are integer values coming from the outline
     * of the glyph.
     */
    enum point_packing_t
      {
        /*!
         * The point value is stored biased by this value.
         * Subtract this value from the unpacked value to
         * get the coordiante value.
         */
        point_coordinate_bias = 32768,

        /*!
         * The number of bits to store a coordinate value
         */
        point_coordinate_numbits = 16u,

        /*!
         * The first bit used to store the x-coordinate of the point
         */
        point_x_coordinate_bit0 = 0u,

        /*!
         * The first bit used to store the y-coordinate of the point
         */
        point_y_coordinate_bit0 = 16u,
      };

    /*!
     * This enumeration describes the meaning of the
     * attributes. The data of the glyph is offset so
     * that a shader can assume that the bottom left
     * corner has glyph-coordinate (0, 0) and the top
     * right corner has glyph-coordinate (width, height)
     * where width and height are the width and height
     * of the glyph is glyph coordinates.
     */
    enum attribute_values_t
      {
        /*!
         * the index into GlyphAttribute::m_data storing
         * the x-value for the glyph coordinate of the vertex
         * of a quad to draw a glyph.
         */
        glyph_coordinate_x = 0,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the y-value for the glyph coordinate of the vertex
         * of a quad to draw a glyph.
         */
        glyph_coordinate_y = 1,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the width of the glyph in glyph coordinates.
         */
        glyph_width = 2,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the height of the glyph in glyph coordinates.
         */
        glyph_height = 3,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the offset into the geometry store for the glyph
         * data.
         */
        glyph_geometry_offset = 4,

        /*!
         * Number attribute values needed.
         */
        glyph_num_attributes = 5
      };

    /*!
     * Ctor.
     */
    explicit
    GlyphRenderDataRestrictedRays(void);

    ~GlyphRenderDataRestrictedRays();

    /*!
     * Start a contour. Before starting a new contour
     * the previous contour must be closed by calling
     * line_to() or quadratic_to() connecting to the
     * start point of the previous contour.
     * \param pt start point of the new contour
     */
    void
    move_to(ivec2 pt);

    /*!
     * Add a line segment connecting the end point of the
     * last curve or line segment of the current contour to
     * a given point.
     * \param pt end point of the new line segment
     */
    void
    line_to(ivec2 pt);

    /*!
     * Add a quadratic curveconnecting the end point of the
     * last curve or line segment of the current contour
     * \param ct control point of the quadratic curve
     * \param pt end point of the quadratic curve
     */
    void
    quadratic_to(ivec2 ct, ivec2 pt);

    /*!
     * Finalize the input data after which no more contours
     * or curves may be added. All contours added must be
     * closed as well.
     * \param f fill rule to use for rendering
     * \param min_pt minimum point of the bounding box of the
     *               contours added
     * \param max_pt maximum point of the bounding box of the
     *               contours added
     */
    void
    finalize(enum PainterEnums::fill_rule_t f,
             ivec2 min_pt, ivec2 max_pt);

    virtual
    enum fastuidraw::return_code
    upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                    GlyphAttribute::Array &attributes) const;

  private:
    void *m_d;
  };
/*! @} */
}
