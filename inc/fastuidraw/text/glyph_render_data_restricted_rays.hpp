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

#include <fastuidraw/util/rect.hpp>
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
   * as follows.
   *  - We break the glyph's box into a hierarchy of boxes where
   *    each leaf node has a list of what curves are in the box
   *    together with a single sample point inside the box giving
   *    the winding number at the sample point.
   *  - To compute the winding number, one runs the technique
   *    on the ray connecting the fragment position to the winding
   *    sample position and increment the value by the winding value
   *    of the sample. Here the main caveat is that one needs to
   *    ignore any intersection that are not between the fragment
   *    position and the sample position.
   *  - The shader (which can be fetched with the function
   *    fastuidraw::glsl::restricted_rays_compute_coverage())
   *    tracks the closest curve (in a local L1-metric scaled to
   *    window coordinates) to the fragment position that increments
   *    the winding value and also tracks the closest curve that
   *    decrements the winding value. Using those two values together
   *    with the winding value allows the shader to compute a coverage
   *    value to perform anti-aliasing.
   */
  class GlyphRenderDataRestrictedRays:public GlyphRenderData
  {
  public:
    /*!
     * Enumeration to describe the hierarchy of bounding
     * boxes as packed into the data. A node in the
     * hierarchy is a single 32-bit value. A leaf in the
     * hierarchy is a single 32-bit value followed by a
     * single sample point which has a winding value
     * and offset position packed as according to \ref
     * winding_sample_packing_t.
     */
    enum hierarchy_packing_t
      {

        /*!
         * This is the number of bits used to store the
         * offsets to a child node.
         */
        hierarchy_child_offset_numbits = 15u,

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
         * is the number of bits used to encode the size
         * of the list of curves for the box is located.
         * The list of curves is packed as according to
         * \ref curve_list_packing_t.
         */
        hierarchy_leaf_curve_list_size_numbits = 15u,

        /*!
         * If this bit is up, indicates that the 32-bit value
         * is holding node data. If the bit is down indicates
         * that the element is a leaf and the value holds the
         * properties of the curve list in the box and the next
         * value holds the winding sample information for the
         * box and are packed as according to \ref
         * winding_sample_packing_t.
         */
        hierarchy_is_node_bit = 0u,

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
        hierarchy_splitting_coordinate_bit = hierarchy_is_node_bit + 1u,

        /*!
         * For case where the element is a node, i.e. the
         * bit \ref hierarchy_is_node_bit is up. This is
         * the first bit holding the offset from the start
         * of the geomertic data of the glyph for the child
         * node which comes before the split, i.e. the child
         * on the left or bottom side.
         */
        hierarchy_child0_offset_bit0 = hierarchy_splitting_coordinate_bit + 1u,

        /*!
         * For case where the element is a node, i.e. the
         * bit \ref hierarchy_is_node_bit is up. This is
         * the first bit holding the offset from the start
         * of the geomertic data of the glyph for the child
         * node which comes after the split, i.e. the child
         * on the right or top side.
         */
        hierarchy_child1_offset_bit0 = hierarchy_child0_offset_bit0 + hierarchy_child_offset_numbits,

        /*!
         * For case where the element is leaf, i.e. the
         * bit \ref hierarchy_is_node_bit is down. This
         * is the first bit used to encode the offset
         * to where the list of curves for the box is
         * located. The list of curves is packed as
         * according to \ref curve_list_packing_t.
         */
        hierarchy_leaf_curve_list_bit0 = hierarchy_is_node_bit + 1u,

        /*!
         * For case where the element is leaf, i.e. the
         * bit \ref hierarchy_is_node_bit is down. This
         * is the first bit used to encode the size
         * of the list of curves for the box is located.
         * The list of curves is packed as according to
         * \ref curve_list_packing_t.
         */
        hierarchy_leaf_curve_list_size_bit0 = hierarchy_leaf_curve_list_bit0 + hierarchy_leaf_curve_list_numbits,
      };

    /*!
     * Enumeration to describe how the winding samples
     * of a leaf-box of the hierarchy are packed. The
     * position of the sample is the bottom left corner
     * of the node offset by a delta:
     * $ Delta = RelativeDelta * BoxDimensions / DeltaFactor $
     * where PackedDelta is extracted from the 32-bit
     * value as a pair of 8-bit values located at bits
     * \ref delta_x_bit0 and \ref delta_y_bit0; DeltaFactor
     * is given by \ref delta_div_factor and BoxDimensions
     * is the width and height of the box of the leaf.
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
         * The amount by which to divide the delta
         */
        delta_div_factor = 256,

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
     * Points are packed as (fp16, fp16) pairs.
     */
    enum point_packing_t
      {
      };

    enum
      {
        /* The glyph coordinate value in each coordiante varies
         * from -\ref glyph_coord_value to +\ref glyph_coord_value,
         * i.e. the glyph is drawn as rect with min-corner
         * (-\ref glyph_coord_value, -\ref glyph_coord_value)
         * and max-corner (+\ref glyph_coord_value, +\ref glyph_coord_value)
         */
        glyph_coord_value = 2048,
      };

    /*!
     * This enumeration describes the meaning of the
     * attributes. The glyph shader is to assume that
     * the glyph-coordinates at the min-corner is
     * (-\ref glyph_coord_value, -\ref glyph_coord_value)
     * and the glyph coordiantes at the max-corner is
     * (+\ref glyph_coord_value, +\ref glyph_coord_value)
     */
    enum attribute_values_t
      {
        /*!
         * Value is 0 if on min-x side of glyph, value is
         * 1 if on max-x side of glyph; packed as uint.
         */
        glyph_normalized_x,

        /*!
         * Value is 0 if on min-y side of glyph, value is
         * 1 if on max-y side of glyph; packed as uint.
         */
        glyph_normalized_y,

        /*!
         * the index into GlyphAttribute::m_data storing
         * the fill rule and the offset into the store for
         * the glyph data. The offset is encoded as follows
         *  - bits0-bits29 encode the offset
         *  - bit30 indicates to complement fill
         *  - bit31 up indicates odd-even fill rule and
         *          down indicates non-zero fill rule.
         */
        glyph_offset,

        /*!
         * Number attribute values needed.
         */
        glyph_num_attributes
      };

    /*!
     * A query_info holds data about a \ref GlyphRenderDataRestrictedRays
     * value (after its finalized method).
     */
    class query_info
    {
    public:
      /*!
       * Default ctor, initializing the value as empty
       */
      query_info(void)
      {}

      /*!
       * Set the value of a \ref vecN of \ref GlyphAttribute values
       * derived from this query_info object
       * \param out_attribs location to which to write GlyphAttribute values
       * \param fill_rule fill rule with which to fill the glyphs
       * \param offset location of glyph data
       */
      void
      set_glyph_attributes(vecN<GlyphAttribute, glyph_num_attributes> *out_attribs,
                           enum PainterEnums::fill_rule_t fill_rule,
                           uint32_t offset);

      /*!
       * The GPU data of the queried \ref GlyphRenderDataRestrictedRays
       * object; the data pointed to by the array is backed
       * internally by the queried \ref GlyphRenderDataRestrictedRays;
       * thus, if the point becomes invalid once the queried
       * \ref GlyphRenderDataRestrictedRays goes out of scope.
       */
      c_array<const generic_data> m_gpu_data;
    };

    /*!
     * Ctor.
     */
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
     * Finalize the input data after which no more contours or curves may be added;
     * all added contours must be closed before calling finalize(). Once finalize()
     * is called, no further data can be added. How the data is broken into bounding
     * boxes is specified by
     * - units_per_EM argument (see below)
     * - GlyphGenerateParams::restricted_rays_minimum_render_size()
     * - GlyphGenerateParams::restricted_rays_split_thresh()
     * - GlyphGenerateParams::restricted_rays_max_recursion()
     * All contours added must be closed as well.
     * \param f fill rule to use for rendering
     * \param glyph_rect the rect of the glyph
     * \param units_per_EM the units per EM for the glyph; this value together with
     *                     GlyphGenerateParams::restricted_rays_minimum_render_size()
     *                     is used to decide how close a curve may be to a bounding
     *                     box to decide if it is included.
     */
    void
    finalize(enum PainterEnums::fill_rule_t f, const RectT<int> &glyph_rect,
             float units_per_EM);

    /*!
     * Finalize the input data after which no more contours or curves may be added;
     * all added contours must be closed before calling finale(). Once finalize()
     * is called, no further data can be added. Instead of using methods from
     * \ref GlyphGenerateParams, directly specify how the data is broken into
     * boxes.
     * \param f fill rule to use for rendering
     * \param glyph_rect the rect of the glyph
     * \param split_thresh if the number of curves within a box is greater than
     *                     this value, the box is split
     * \param max_recursion the maximum level of recursion allowed in splitting
     *                      the data into boxes
     * \param near_thresh horizontal and vertical threshhold to decide if a curve
     *                    outside of a box should be added to a box
     */
    void
    finalize(enum PainterEnums::fill_rule_t f, const RectT<int> &glyph_rect,
             int split_thresh, int max_recursion, vec2 near_thresh);

    /*!
     * Query the data; may only be called after finalize(). Returns
     * \ref routine_fail if finalize() has not yet been called.
     * \param out_info location to which to write information about
     *                 this object.
     */
    enum return_code
    query(query_info *out_info) const;

    virtual
    c_array<const c_string>
    render_info_labels(void) const;

    virtual
    enum fastuidraw::return_code
    upload_to_atlas(GlyphAtlasProxy &atlas_proxy,
                    GlyphAttribute::Array &attributes,
                    c_array<float> render_costs) const;

  private:
    void *m_d;
  };
/*! @} */
}
